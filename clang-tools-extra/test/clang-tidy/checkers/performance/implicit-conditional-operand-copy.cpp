// RUN: %check_clang_tidy %s performance-implicit-conditional-operand-copy %t

struct Expensive {
  Expensive();
  Expensive(const Expensive &);
  Expensive(Expensive &&);
  Expensive &operator=(const Expensive &);
  Expensive &operator=(Expensive &&);
  ~Expensive();
  void constMethod() const;
  void mutatingMethod();
  void lvalueQualifiedConstMethod() const &;
  void rvalueQualifiedConstMethod() const &&;
  mutable int Cached;
  int &mutableAccess() const;
  const int &readOnlyAccess() const;
};

struct NoMove {
  NoMove();
  NoMove(const NoMove &);
  ~NoMove();
};

struct Derived : Expensive {};

struct Cheap {
  int A;
  int B;
};

struct Wrapper {
  Wrapper(const Expensive &);
};

// Constructible from an lvalue, so `return E;` works in place of the copy.
struct Converting {
  Converting(const Expensive &);
  Converting(Expensive &&);
};

// No assignment operator takes an lvalue, so `S = E;` would not compile.
struct RValueOnlyAssign {
  RValueOnlyAssign &operator=(Expensive &&);
};

// A user-declared move assignment operator deletes the implicit copy
// assignment operator, so `D = E;` does not compile either.
struct DeletedCopyAssign {
  DeletedCopyAssign();
  DeletedCopyAssign(const DeletedCopyAssign &);
  DeletedCopyAssign &operator=(DeletedCopyAssign &&);
  ~DeletedCopyAssign();
};
DeletedCopyAssign makeDeletedCopyAssign();

// The overload taking an lvalue is private.
struct PrivateLValueAssign {
  PrivateLValueAssign &operator=(Expensive &&);

private:
  PrivateLValueAssign &operator=(const Expensive &);
};

// The overload taking an lvalue needs an rvalue object.
struct RefQualifiedAssign {
  RefQualifiedAssign &operator=(Expensive &&);
  RefQualifiedAssign &operator=(const Expensive &) &&;
};

// Mirror image: the overload taking an lvalue needs an lvalue object, so it is
// unavailable when the assignment target is named as an rvalue.
struct LValueQualifiedAssign {
  LValueQualifiedAssign &operator=(Expensive &&);
  LValueQualifiedAssign &operator=(const Expensive &) &;
};

// Only the rvalue-taking overload is callable on a `const` object.
struct ConstAssignable {
  ConstAssignable &operator=(Expensive &&) const;
  ConstAssignable &operator=(const Expensive &);
};

struct StringLike {
  StringLike(const char *);
  StringLike(const StringLike &);
  ~StringLike();
};

struct Container {
  Container();
  Container(const Container &);
  ~Container();
  const int *begin() const;
  const int *end() const;
};

Expensive makeExpensive();
Container makeContainer();
NoMove makeNoMove();
Cheap makeCheap();
const Expensive &getExpensiveRef();
bool cond();

void takesConstRef(const Expensive &);
void takesValue(Expensive);
void takesRRef(Expensive &&);
void takesCheapConstRef(const Cheap &);
void takesStringLike(const StringLike &);

void boundToConstReference() {
  Expensive E;

  takesConstRef(cond() ? E : makeExpensive());
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  takesConstRef(cond() ? makeExpensive() : E);
  // CHECK-MESSAGES: :[[@LINE-1]]:44: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  const Expensive &R = cond() ? E : makeExpensive();
  // CHECK-MESSAGES: :[[@LINE-1]]:33: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
  (void)R;

  // The operand does not have to be a variable; any lvalue is copied.
  takesConstRef(cond() ? getExpensiveRef() : makeExpensive());
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  // Slicing a derived lvalue into the base still copies.
  Derived D;
  takesConstRef(cond() ? D : makeExpensive());
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  // The operands do not have to have the same type; here the string literal is
  // converted, and the lvalue operand is the one that gets copied.
  StringLike S("x");
  takesStringLike(cond() ? S : "hello world");
  // CHECK-MESSAGES: :[[@LINE-1]]:28: warning: operand of type 'StringLike' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const StringLike &' [performance-implicit-conditional-operand-copy]

  // A constructor parameter is no different from any other.
  Wrapper W(cond() ? E : makeExpensive());
  // CHECK-MESSAGES: :[[@LINE-1]]:22: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
  (void)W;
}

void objectOfConstMemberCall() {
  Expensive E;

  // The temporary is only used as a `const` object, so casting both operands to
  // `const Expensive &` keeps this call well-formed.
  (cond() ? E : makeExpensive()).constMethod();
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  // An `&`-qualified member accepts the `const Expensive &` the cast produces.
  (cond() ? E : makeExpensive()).lvalueQualifiedConstMethod();
  // CHECK-MESSAGES: :[[@LINE-1]]:13: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  // An `&&`-qualified member needs an rvalue object. Neither the `if`-`else`
  // rewrite nor the cast provides one, so the copy stays.
  (cond() ? E : makeExpensive()).rvalueQualifiedConstMethod();

  // Returning a reference to const cannot be used to modify the operand.
  (void)(cond() ? E : makeExpensive()).readOnlyAccess();
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]

  // This `const` member hands out mutable access, so writing through it would
  // reach `E` itself once the copy is gone.
  (cond() ? E : makeExpensive()).mutableAccess() = 5;
}

void assignedOnward() {
  Expensive E, X;

  // `X` takes the value either way and `E` is left untouched, so assigning `E`
  // to `X` directly removes the copy without changing behaviour.
  X = cond() ? E : makeExpensive();
  // CHECK-MESSAGES: :[[@LINE-1]]:16: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
}

Converting convertedOnReturn() {
  Expensive E;

  // `Converting` can be built from an lvalue, so `return E;` is equivalent.
  return cond() ? E : makeExpensive();
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
}

void assignmentTargetsThatCannotTakeAnLValue() {
  Expensive E;
  RValueOnlyAssign S;
  DeletedCopyAssign A, D;
  PrivateLValueAssign P;
  RefQualifiedAssign Q;

  // In each of these the rewritten assignment would not compile, so the copy
  // that produces the rvalue is what makes the original work.
  S = cond() ? E : makeExpensive();
  D = cond() ? A : makeDeletedCopyAssign();
  P = cond() ? E : makeExpensive();
  Q = cond() ? E : makeExpensive();

  // The `&`-qualified overload cannot be used on an rvalue target.
  LValueQualifiedAssign L;
  static_cast<LValueQualifiedAssign &&>(L) = cond() ? E : makeExpensive();

  // The non-const overload cannot be used on a `const` target.
  const ConstAssignable CA;
  CA = cond() ? E : makeExpensive();

  // Named as an lvalue, the same overload is available.
  L = cond() ? E : makeExpensive();
  // CHECK-MESSAGES: :[[@LINE-1]]:16: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
}

void copyIsNotAvoidable() {
  Expensive E, F;

  // A copy has to happen to produce the argument.
  takesValue(cond() ? E : makeExpensive());

  // A copy has to happen to initialize the variable.
  Expensive X = cond() ? E : makeExpensive();
  (void)X;

  // Both operands are lvalues of the same type, so the conditional operator is
  // an lvalue and nothing is copied.
  takesConstRef(cond() ? E : F);
  takesConstRef(cond() ? E : getExpensiveRef());

  // A derived lvalue converts to an lvalue reference to the base, so this
  // conditional operator is an lvalue too.
  Derived D;
  takesConstRef(cond() ? D : E);

  // An explicitly written copy is intentional.
  takesConstRef(cond() ? Expensive(E) : makeExpensive());

  // Moving is not copying.
  takesConstRef(cond() ? static_cast<Expensive &&>(E) : makeExpensive());

  // No conditional operator at all.
  takesConstRef(E);
  takesConstRef(makeExpensive());
}

void usesThatAreNotProvablyReadOnly() {
  Expensive E;

  // Whether any of these could drop the copy depends on overload resolution,
  // which cannot be performed here, so none of them is reported.

  // Binding to an rvalue reference hands the temporary to code that may consume
  // it.
  takesRRef(cond() ? E : makeExpensive());

  Expensive &&R = cond() ? E : makeExpensive();
  (void)R;

  // The call mutates the temporary rather than reading it.
  (cond() ? E : makeExpensive()).mutatingMethod();

  // The range of a range-based `for` loop is bound to `auto &&`, so the loop
  // could mutate it.
  Container C;
  for (int I : cond() ? C : makeContainer())
    (void)I;
}

void copyConstructorSelectedForXValue() {
  NoMove N;

  // `NoMove` has no move constructor, so the cast selects the copy
  // constructor. The author already asked for the operand to be given up.
  const NoMove &R = cond() ? static_cast<NoMove &&>(N) : makeNoMove();
  (void)R;
}

void cheapTypesAreIgnored() {
  Cheap C;
  takesCheapConstRef(cond() ? C : makeCheap());

  int I = 0;
  const int &R = cond() ? I : 1;
  (void)R;
}

template <typename T>
void neverInstantiated() {
  T V;
  takesConstRef(cond() ? V : makeExpensive());
}

template <typename T>
void instantiated() {
  T V;
  takesConstRef(cond() ? V : makeExpensive());
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
}

void useInstantiated() { instantiated<Expensive>(); }

#define COPY_IN_MACRO(X) takesConstRef(cond() ? (X) : makeExpensive())

void inMacro() {
  Expensive E;
  COPY_IN_MACRO(E);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: operand of type 'Expensive' is implicitly copied to form the result of the conditional operator; consider rewriting as an 'if'-'else' statement, or casting both operands to 'const Expensive &' [performance-implicit-conditional-operand-copy]
}
