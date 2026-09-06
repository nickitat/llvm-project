//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ImplicitConditionalOperandCopyCheck.h"
#include "../utils/TypeTraits.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"

using namespace clang::ast_matchers;

namespace clang::tidy::performance {

namespace {

/// Strips the nodes clang inserts between a conditional operator and the
/// expression that produces the value of one of its branches.
const Expr *ignoreTemporaryBinding(const Expr *E) {
  while (true) {
    if (const auto *Paren = dyn_cast<ParenExpr>(E)) {
      E = Paren->getSubExpr();
      continue;
    }

    // The only implicit conversion expected on the way to the copy is the one
    // that adds `const`, which clang inserts when the composite type of the
    // conditional operator is `const T`. Every other conversion is a real one,
    // and looking through it would attribute its operand's copy to the wrong
    // expression, so stop there instead.
    if (const auto *Cast = dyn_cast<ImplicitCastExpr>(E)) {
      if (Cast->getCastKind() != CK_NoOp)
        return E;
      E = Cast->getSubExpr();
      continue;
    }

    if (const auto *Bind = dyn_cast<CXXBindTemporaryExpr>(E)) {
      E = Bind->getSubExpr();
      continue;
    }

    // Before C++17, a prvalue that initializes an object is copied (or moved)
    // into it by an elidable constructor call that does not exist in the C++17
    // and later AST. Look through it, the way `ignoringElidableConstructorCall`
    // does, so that the check behaves the same in every language mode.
    if (const auto *Construct = dyn_cast<CXXConstructExpr>(E)) {
      if (Construct->isElidable() && Construct->getNumArgs() == 1) {
        if (const auto *Materialized =
                dyn_cast<MaterializeTemporaryExpr>(Construct->getArg(0))) {
          E = Materialized->getSubExpr();
          continue;
        }
      }
    }

    return E;
  }
}

/// Returns true if the materialized temporary only exists to be copied or moved
/// into an object, which is how initializing an object from a prvalue is
/// spelled before C++17. Such a copy is not avoidable.
/// Returns true if `Candidate` could be called with the operand itself in place
/// of a copy of it. The declaration has to settle that on its own, so anything
/// whose viability depends on deduction or constraint satisfaction is refused:
/// those need overload resolution, which is unavailable once `Sema` is gone.
bool acceptsLValueOfType(const FunctionDecl *Candidate, const Expr *Object,
                         unsigned ParamIndex, unsigned NumArgs,
                         QualType OperandType) {
  if (Candidate->isDeleted() || Candidate->getAccess() != AS_public ||
      Candidate->getDescribedFunctionTemplate() != nullptr ||
      Candidate->getTrailingRequiresClause())
    return false;

  if (const auto *Method = dyn_cast<CXXMethodDecl>(Candidate)) {
    // An explicit object parameter describes the object in a way this does not
    // account for.
    if (Method->isExplicitObjectMemberFunction())
      return false;
    // The implicit object parameter is `cv T &` or `cv T &&`, and the rewritten
    // call names the object exactly as the original does, so the candidate has
    // to accept it in both respects. Its cv-qualifiers must cover the object's.
    if (Object != nullptr) {
      const QualType ObjectType = Object->getType();
      if ((ObjectType.isConstQualified() && !Method->isConst()) ||
          (ObjectType.isVolatileQualified() && !Method->isVolatile()))
        return false;
    }
    switch (Method->getRefQualifier()) {
    case RQ_None:
      break;
    case RQ_LValue:
      if (Object == nullptr || !Object->isLValue())
        return false;
      break;
    case RQ_RValue:
      if (Object == nullptr || Object->isLValue())
        return false;
      break;
    }
  }
  // A converting constructor has to be usable in copy-initialization.
  if (const auto *Constructor = dyn_cast<CXXConstructorDecl>(Candidate))
    if (Constructor->isExplicit())
      return false;

  // The rewritten expression passes the same arguments, so an overload needing
  // more of them, or taking fewer, is not one it could resolve to.
  if (NumArgs < Candidate->getMinRequiredArguments() ||
      (NumArgs > Candidate->getNumParams() && !Candidate->isVariadic()) ||
      ParamIndex >= Candidate->getNumParams())
    return false;

  const QualType ParamType = Candidate->getParamDecl(ParamIndex)->getType();
  // An rvalue reference cannot bind the operand, and a non-const lvalue
  // reference would let the callee modify it rather than a copy of it.
  if (ParamType->isRValueReferenceType())
    return false;
  if (ParamType->isLValueReferenceType() &&
      !ParamType.getNonReferenceType().isConstQualified())
    return false;

  return ParamType.getNonReferenceType()
             .getCanonicalType()
             .getUnqualifiedType() ==
         OperandType.getCanonicalType().getUnqualifiedType();
}

/// Returns true if `Record` can take the operand itself, so that replacing
/// `Target = Cond ? A : make()` with `Target = A`, or `return Cond ? A :
/// make()` with `return A`, still compiles and yields the same value. Only
/// members of this one class are considered, which is what keeps the question
/// answerable without performing overload resolution.
bool targetAcceptsLValue(const CXXRecordDecl *Record, const Expr *Object,
                         bool Assignment, unsigned ParamIndex, unsigned NumArgs,
                         QualType OperandType) {
  if (Record == nullptr || !Record->hasDefinition())
    return false;

  if (Assignment) {
    for (const CXXMethodDecl *Method : Record->methods())
      if (Method->getOverloadedOperator() == OO_Equal &&
          acceptsLValueOfType(Method, Object, ParamIndex, NumArgs, OperandType))
        return true;
    return false;
  }

  for (const CXXConstructorDecl *Constructor : Record->ctors())
    if (acceptsLValueOfType(Constructor, /*Object=*/nullptr, ParamIndex,
                            NumArgs, OperandType))
      return true;
  return false;
}

/// Returns true if the materialized temporary is only ever read through, so
/// that reusing the lvalue operand in its place cannot change what the program
/// does. These are exactly the two contexts a `const T &` can serve: a binding
/// to a reference to const, and the object of a `const` member call.
///
/// Anything else -- an rvalue reference, a mutating member call, an argument
/// passed on to another function, the target of an assignment -- would need
/// overload resolution to decide, and that cannot be done here: the check runs
/// on a finished AST, after Sema is gone. Such copies are left alone rather
/// than reported on a guess.
bool isUsedAsConstObject(const MaterializeTemporaryExpr *Materialized,
                         ASTContext &Context) {
  // Before C++17 a prvalue that initializes an object is copied into it by an
  // elidable constructor call that does not exist in the C++17 and later AST.
  // That call materializes the temporary, and binds it as a `const T &` lvalue
  // when the type has no move constructor, but the copy is required all the
  // same.
  for (const DynTypedNode &Parent : Context.getParents(*Materialized))
    if (const auto *Construct = Parent.get<CXXConstructExpr>())
      if (Construct->isElidable())
        return false;

  // An lvalue materialization is a binding to a reference to const; a
  // non-const reference cannot bind a temporary.
  if (Materialized->isLValue())
    return true;

  const Expr *Current = Materialized;
  while (true) {
    const DynTypedNodeList Parents = Context.getParents(*Current);
    if (Parents.size() != 1)
      return false;

    if (const auto *Member = Parents[0].get<MemberExpr>()) {
      const auto *Method = dyn_cast<CXXMethodDecl>(Member->getMemberDecl());
      // An `&&`-qualified member needs an rvalue object, which neither rewrite
      // provides: the operand is an lvalue, and so is a `const T &` cast of it.
      if (Method == nullptr || !Method->isConst() ||
          Method->getRefQualifier() == RQ_RValue)
        return false;

      // Being `const` does not stop a member from handing out mutable access to
      // `mutable` state. Writing through what it returns reaches the temporary
      // today, but would reach the operand itself once the copy is gone.
      const QualType ReturnType = Method->getReturnType();
      if ((ReturnType->isReferenceType() || ReturnType->isPointerType()) &&
          !ReturnType->getPointeeType().isConstQualified())
        return false;

      return true;
    }

    // `Target = Cond ? A : make()` hands the value straight on to `Target`.
    // Assigning `A` to it directly gives the same result without the
    // intermediate copy, and leaves `A` untouched either way, as long as the
    // type has an assignment operator that takes an lvalue.
    if (const auto *Assignment = Parents[0].get<CXXOperatorCallExpr>()) {
      if (Assignment->getOperator() != OO_Equal ||
          Assignment->getNumArgs() < 2 || Assignment->getArg(0) == Current)
        return false;
      const auto *Method =
          dyn_cast_or_null<CXXMethodDecl>(Assignment->getDirectCallee());
      return Method != nullptr &&
             targetAcceptsLValue(Method->getParent(), Assignment->getArg(0),
                                 /*Assignment=*/true, /*ParamIndex=*/0,
                                 /*NumArgs=*/1, Materialized->getType());
    }

    // The temporary is converted into some other object, as in
    // `return Cond ? A : make();` where the return type wraps the operand.
    // Constructing that object from `A` directly is equivalent when a
    // constructor takes an lvalue.
    if (const auto *Construct = Parents[0].get<CXXConstructExpr>()) {
      unsigned ParamIndex = Construct->getNumArgs();
      for (unsigned I = 0, E = Construct->getNumArgs(); I != E; ++I)
        if (Construct->getArg(I) == Current)
          ParamIndex = I;
      if (ParamIndex == Construct->getNumArgs())
        return false;
      return targetAcceptsLValue(Construct->getConstructor()->getParent(),
                                 /*Object=*/nullptr, /*Assignment=*/false,
                                 ParamIndex, Construct->getNumArgs(),
                                 Materialized->getType());
    }

    // Clang inserts an implicit `NoOp` cast to `const T` between the
    // materialized temporary and a `const` member access, so the member
    // expression is not reached in a single step.
    const auto *Parent = Parents[0].get<Expr>();
    if (Parent == nullptr ||
        !isa<ImplicitCastExpr, ParenExpr, ExprWithCleanups>(Parent))
      return false;
    Current = Parent;
  }
}

const CXXConstructExpr *getImplicitLValueCopy(const Expr *Branch) {
  const auto *Construct =
      dyn_cast<CXXConstructExpr>(ignoreTemporaryBinding(Branch));
  if (Construct == nullptr)
    return nullptr;

  // A copy the user spelled out, as in `Cond ? Expensive(E) : makeExpensive()`,
  // is intentional.
  if (isa<CXXTemporaryObjectExpr>(Construct))
    return nullptr;

  if (!Construct->getConstructor()->isCopyConstructor())
    return nullptr;

  // A copy constructor is always called with the object it copies as its first
  // argument.
  assert(Construct->getNumArgs() >= 1 &&
         "copy construction without a source argument");

  // Selecting the copy constructor for an xvalue means the type has no move
  // constructor; the operand was already given up by the author either way.
  if (!Construct->getArg(0)->isLValue())
    return nullptr;

  return Construct;
}

} // namespace

void ImplicitConditionalOperandCopyCheck::registerMatchers(
    MatchFinder *Finder) {
  // A `MaterializeTemporaryExpr` above the conditional operator is what makes
  // the copy avoidable: the prvalue result is only ever used through a
  // reference, so an `if`-`else` (or a cast of both operands to a reference
  // type) would bind to the original object instead of a copy of it.
  Finder->addMatcher(
      materializeTemporaryExpr(
          has(ignoringParenImpCasts(
              conditionalOperator(hasType(hasCanonicalType(recordType())))
                  .bind("conditional"))))
          .bind("materialized"),
      this);
}

void ImplicitConditionalOperandCopyCheck::check(
    const MatchFinder::MatchResult &Result) {
  const auto *Conditional =
      Result.Nodes.getNodeAs<ConditionalOperator>("conditional");
  const auto *Materialized =
      Result.Nodes.getNodeAs<MaterializeTemporaryExpr>("materialized");

  // Only report a copy that could actually be removed without changing what
  // the program does.
  if (!isUsedAsConstObject(Materialized, *Result.Context))
    return;

  for (const Expr *Branch :
       {Conditional->getTrueExpr(), Conditional->getFalseExpr()}) {
    const CXXConstructExpr *Copy = getImplicitLValueCopy(Branch);
    if (!Copy)
      continue;

    const QualType CopiedType = Copy->getType().getUnqualifiedType();
    if (!utils::type_traits::isExpensiveToCopy(CopiedType, *Result.Context)
             .value_or(false))
      continue;

    // Both reported contexts accept a `const T` lvalue, so casting the operands
    // is always an option alongside the `if`-`else` rewrite.
    const QualType ConstRefType =
        Result.Context->getLValueReferenceType(CopiedType.withConst());
    diag(Copy->getExprLoc(),
         "operand of type %0 is implicitly copied to form the result of the "
         "conditional operator; consider rewriting as an 'if'-'else' "
         "statement, or casting both operands to %1")
        << CopiedType << ConstRefType;
  }
}

} // namespace clang::tidy::performance
