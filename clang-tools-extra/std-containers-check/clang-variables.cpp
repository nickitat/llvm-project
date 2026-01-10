// Clang includes
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// LLVM includes
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

// Standard includes
#include <memory>

namespace ClangVariables {

constexpr llvm::StringRef Marker{"clang"};

/// Callback class for clang-variable matches.
class MatchHandler : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;

  /// Handles the matched variable.
  ///
  /// Checks if the name of the matched variable is either empty or prefixed
  /// with `clang_` else emits a diagnostic and FixItHint.
  void run(const MatchResult &Result) override {
    // for (const auto &[ID, Node] : Result.Nodes.getMap()) {
    //   llvm::errs() << "Bound ID: " << ID << "\n";
    //   Node.dump(llvm::errs(), *Result.Context);
    //   llvm::errs() << "\n---\n";
    // }

    llvm::StringRef Name;
    const clang::NamedDecl *Value = nullptr;
    if ((Value = Result.Nodes.getNodeAs<clang::VarDecl>(Marker))) {
      Name = Value->getName();
    } else if ((Value = Result.Nodes.getNodeAs<clang::FieldDecl>(Marker))) {
      Name = Value->getName();
    } else if ((Value =
                    Result.Nodes.getNodeAs<clang::TypedefNameDecl>(Marker))) {
      Name = Value->getName();
    } else if ((Value = Result.Nodes.getNodeAs<clang::TypeAliasTemplateDecl>(
                    Marker))) {
      Name = Value->getName();
    } else if ((Value = Result.Nodes.getNodeAs<clang::ParmVarDecl>(Marker))) {
      Name = Value->getName();
      // } else if ((Value =
      // Result.Nodes.getNodeAs<clang::NonTypeTemplateParmDecl>(
      //                 Marker))) {
      //   Name = Value->getName();
      // } else if ((Value =
      // Result.Nodes.getNodeAs<clang::TemplateTypeParmDecl>(
      //                 Marker))) {
      //   Name = Value->getName();
    } else {
      llvm::report_fatal_error(
          "Error: Matched node is neither VarDecl nor FieldDecl");
    }

    if (Name.empty() || Name.starts_with("clang_"))
      return;

    clang::DiagnosticsEngine &Engine = Result.Context->getDiagnostics();
    const unsigned ID =
        Engine.getCustomDiagID(clang::DiagnosticsEngine::Warning,
                               "clang variable must have 'clang_' prefix");

    /// Hint to the user to prefix the variable with 'clang_'.
    const clang::FixItHint FixIt =
        clang::FixItHint::CreateInsertion(Value->getLocation(), "clang_");

    Engine.Report(Value->getLocation(), ID).AddFixItHint(FixIt);
  }
}; // namespace ClangVariables

/// Dispatches the ASTMatcher.
class Consumer : public clang::ASTConsumer {
public:
  /// Creates the matcher for clang variables and dispatches it on the TU.
  void HandleTranslationUnit(clang::ASTContext &Context) override {
    using namespace clang::ast_matchers; // NOLINT(build/namespaces)

    // clang-format off
    const auto ContainerNameMatcher =
      hasAnyName(
        "::std::vector"
      );

    const auto ContainerTemplateMatcher =
      classTemplateSpecializationDecl(
        ContainerNameMatcher
      );

    const auto ContainerTypeMatcher =
      hasUnqualifiedDesugaredType(
        recordType(
          hasDeclaration(
            ContainerTemplateMatcher
          )
        )
      );

    const auto VarMatcher =
      varDecl(
        isExpansionInMainFile(),
        hasType(
          ContainerTemplateMatcher
        )
      );

    const auto FieldMatcher =
      fieldDecl(
        isExpansionInMainFile(),
        hasType(
          ContainerTemplateMatcher
        )
      );

    const auto TypeAliasMatcher =
      typedefNameDecl(
        isExpansionInMainFile(),
        hasType(
          ContainerTypeMatcher
        )
      );

    const auto TemplateAliasMatcher =
      typeAliasTemplateDecl(
        isExpansionInMainFile(),
        has(
          typeAliasDecl(
            hasType(
              templateSpecializationType(
                hasDeclaration(
                  classTemplateDecl(
                    ContainerNameMatcher
                  )
                )
              )
            )
          )
        )
      );

    const auto ParamMatcher =
      parmVarDecl(
        isExpansionInMainFile(),
          anyOf(
           hasType(ContainerTypeMatcher),
           hasType(pointerType(pointee(ContainerTypeMatcher))),
           hasType(referenceType(pointee(ContainerTypeMatcher)))
        )
      );

    const auto CombinedMatcher =
      namedDecl(
        anyOf(
          VarMatcher,
          FieldMatcher,
          TypeAliasMatcher,
          TemplateAliasMatcher,
          ParamMatcher
        )
      ).bind(Marker);
    // clang-format on

    MatchHandler Handler;
    MatchFinder MatchFinder;
    MatchFinder.addMatcher(CombinedMatcher, &Handler);
    MatchFinder.matchAST(Context);
  }
};

/// Creates an `ASTConsumer` and logs begin and end of file processing.
class Action : public clang::ASTFrontendAction {
public:
  using ASTConsumerPointer = std::unique_ptr<clang::ASTConsumer>;

  ASTConsumerPointer CreateASTConsumer(clang::CompilerInstance &Compiler,
                                       llvm::StringRef Filename) override {
    return std::make_unique<Consumer>();
  }

  bool BeginSourceFileAction(clang::CompilerInstance &Compiler) override {
    return true;
  }

  void EndSourceFileAction() override {}
};
} // namespace ClangVariables

namespace {
llvm::cl::OptionCategory ToolCategory("clang-variables options");

llvm::cl::extrahelp MoreHelp(R"(
  Finds all Const Lambdas, that take an Auto parameter, are declared Noexcept
  and have a Goto statement inside, e.g.:

  const auto lambda = [] (auto) noexcept {
    bool done = true;
    flip: done = !done;
    if (!done) goto flip;
  };
)");

llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
} // namespace

auto main(int argc, const char *argv[]) -> int {
  using namespace clang::tooling;

  auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  const auto Action = newFrontendActionFactory<ClangVariables::Action>();
  return Tool.run(Action.get());
}
