// Clang includes
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"

// LLVM includes
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

// Standard includes
#include <memory>

using namespace clang;

using MatchResult = clang::ast_matchers::MatchFinder::MatchResult;

namespace StdContainersCheck {

constexpr llvm::StringRef Marker{"std-containers-check-marker"};

constexpr llvm::StringRef AllowPhrase{"allow-std-containers"};

static std::vector<std::pair<SourceLocation, StringRef>>
getCommentsInRange(ASTContext *Ctx, CharSourceRange Range) {
  std::vector<std::pair<SourceLocation, StringRef>> Comments;
  auto &SM = Ctx->getSourceManager();
  const std::pair<FileID, unsigned> BeginLoc =
                                        SM.getDecomposedLoc(Range.getBegin()),
                                    EndLoc =
                                        SM.getDecomposedLoc(Range.getEnd());

  llvm::dbgs() << "Searching comments in range: "
               << Range.getBegin().printToString(SM) << " - "
               << Range.getEnd().printToString(SM) << "\n";
  llvm::dbgs() << "Decomposed begin: " << BeginLoc.second << ":"
               << EndLoc.second << "\n";

  if (BeginLoc.first != EndLoc.first)
    return Comments;

  bool Invalid = false;
  const StringRef Buffer = SM.getBufferData(BeginLoc.first, &Invalid);
  if (Invalid)
    llvm::report_fatal_error("Error: Unable to retrieve source buffer");

  const char *StrData = Buffer.data() + BeginLoc.second;
  Lexer TheLexer(SM.getLocForStartOfFile(BeginLoc.first), Ctx->getLangOpts(),
                 Buffer.begin(), StrData, Buffer.end());
  TheLexer.SetCommentRetentionState(true);

  while (true) {
    Token Tok;

    if (TheLexer.LexFromRawLexer(Tok))
      break;

    // Check if we've gone past the end of our range
    if (SM.getDecomposedLoc(Tok.getLocation()).second >= EndLoc.second ||
        Tok.is(tok::eof))
      break;

    if (Tok.is(tok::comment)) {
      const auto CommentLoc = SM.getDecomposedLoc(Tok.getLocation());
      assert(CommentLoc.first == BeginLoc.first);
      const auto CommentText =
          StringRef(Buffer.begin() + CommentLoc.second, Tok.getLength());
      Comments.emplace_back(Tok.getLocation(), CommentText);
      llvm::dbgs() << "  Found comment: " << CommentText << "; at "
                   << SM.getDecomposedLoc(Tok.getLocation()).second << "\n";
    }
  }

  return Comments;
}

static CharSourceRange getSourceRange(const MatchResult &Result,
                                      const clang::NamedDecl &Value) {
  clang::SourceManager &SM = *Result.SourceManager;
  const auto MatchBeginLoc = Value.getBeginLoc();
  const auto MatchBeginLine = SM.getPresumedLoc(MatchBeginLoc).getLine();
  const auto FileID = SM.getFileID(MatchBeginLoc);
  const auto SearchBeginLoc = SM.translateLineCol(FileID, MatchBeginLine, 1);
  const auto MatchEndLine = SM.getPresumedLoc(Value.getEndLoc()).getLine();
  auto SearchEndLoc = SM.translateLineCol(FileID, MatchEndLine + 1, 1);
  if (SearchEndLoc.isValid()) {
    SearchEndLoc = SearchEndLoc.getLocWithOffset(-1);
  }
  if (!SearchBeginLoc.isValid() || !SearchEndLoc.isValid()) {
    llvm::report_fatal_error("Error: Unable to compute search range");
  }
  return CharSourceRange::getCharRange(SearchBeginLoc, SearchEndLoc);
}

class MatchHandler : public clang::ast_matchers::MatchFinder::MatchCallback {
public:
  void run(const MatchResult &Result) override {
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
    } else {
      llvm::report_fatal_error("Error: Matched node is of an unexpected type");
    }

    const auto SourceRange = getSourceRange(Result, *Value);
    const auto Comments = getCommentsInRange(Result.Context, SourceRange);
    // If any of the surrounding comments contains the allow phrase, don't
    // produce a warning.
    for (const auto &[_, CommentText] : Comments) {
      if (CommentText.contains(AllowPhrase)) {
        return;
      }
    }

    auto &Engine = Result.Context->getDiagnostics();
    const unsigned ID = Engine.getCustomDiagID(
        clang::DiagnosticsEngine::Warning,
        "An alternative specialisation from "
        "`src/Common/ContainersWithMemoryTracking.h` should be used instead of "
        "default standard containers. Please, either replace accordingly or "
        "annotate this usage with a comment containing "
        "\"allow-std-containers\" string");
    Engine.Report(Value->getLocation(), ID);
  }

private:
  void printMatches(const MatchResult &Result) {
    for (const auto &[ID, Node] : Result.Nodes.getMap()) {
      llvm::dbgs() << "Bound ID: " << ID << "\n";
      Node.dump(llvm::errs(), *Result.Context);
      llvm::dbgs() << "\n---\n";
    }
  }
}; // namespace StdContainersCheck

/// Dispatches the ASTMatcher.
class Consumer : public clang::ASTConsumer {
public:
  /// Creates the matcher and dispatches it on the TU.
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
};
} // namespace StdContainersCheck

namespace {
llvm::cl::OptionCategory ToolCategory("std-containers-check options");

llvm::cl::extrahelp MoreHelp(R"(
  Looks for usages of standard containers in variables, member fields, typedef-s, arguments and so on.
  Unless they are annotated with the special tag 'allow-std-containers', all such usages should be replaced
  with the corresponding container from `src/Common/ContainersWithMemoryTracking.h`.
)");

llvm::cl::extrahelp
    CommonHelp(clang::tooling::CommonOptionsParser::HelpMessage);
} // namespace

int main(int argc, const char *argv[]) {
  using namespace clang::tooling;

  auto ExpectedParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!ExpectedParser) {
    llvm::errs() << ExpectedParser.takeError();
    return 1;
  }
  CommonOptionsParser &OptionsParser = ExpectedParser.get();
  ClangTool Tool(OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  const auto Action = newFrontendActionFactory<StdContainersCheck::Action>();
  return Tool.run(Action.get());
}
