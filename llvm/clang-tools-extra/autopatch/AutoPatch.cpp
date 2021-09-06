#include <iostream>
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Driver/Options.h"
#include "clang/Frontend/ASTConsumers.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/Lex/HeaderSearch.h"
#include "clang/Lex/HeaderSearchOptions.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Lex/PreprocessorOptions.h"
#include "clang/Basic/Builtins.h"
#include "clang/Sema/Sema.h"
#include "clang/AST/DeclBase.h"
#include "clang/AST/Type.h"
#include "clang/AST/Decl.h"
#include "clang/Sema/Lookup.h"
#include "clang/Sema/Ownership.h"
#include "clang/Parse/ParseAST.h"

using namespace clang::tooling;
using namespace clang;
using namespace llvm;

static unsigned int _linenum;
static bool _isloop;


class FindNamedCallVisitor : public clang::RecursiveASTVisitor<FindNamedCallVisitor> {
  public:
    explicit FindNamedCallVisitor(ASTContext *Context, unsigned int linenum, bool isloop)
      : Context(Context), linenum(linenum), isloop(isloop) {}

    bool VisitIfStmt(IfStmt *stmt) {
      FullSourceLoc FullLocation =
        Context->getFullLoc(stmt->getBeginLoc());
      if (FullLocation.isValid() && FullLocation.getLineNumber() == linenum) {
        llvm::outs() << "XXXXXXXXXX Found if at "
          << FullLocation.getSpellingLineNumber() << ":"
          << FullLocation.getSpellingColumnNumber() << "\n";
        bool hasloopparent = false;
        const Stmt *pstmt = dynamic_cast<const Stmt*>(stmt);
        while (true) {
          //get parents
          const auto& parents = Context->getParents(*pstmt);
          if ( parents.empty() ) {
            llvm::errs() << "Can not find parent\n";
            return false;
          }
          llvm::errs() << "find parent size=" << parents.size() << "\n";
          pstmt = parents[0].get<Stmt>();
          if (!pstmt)
            break;
          //pstmt->dump();
          FullSourceLoc loc0 = Context->getFullLoc(pstmt->getBeginLoc());
          FullSourceLoc loc1 = Context->getFullLoc(pstmt->getEndLoc());
          if (isa<ForStmt>(pstmt)) {
            hasloopparent = true;
            llvm::outs() << "XXXXXXXXXX For Parent at "
              << loc0.getSpellingLineNumber() << ":"
              << loc1.getSpellingLineNumber() << "\n";
            break;
          }
          if (isa<WhileStmt>(pstmt)) {
            hasloopparent = true;
            llvm::outs() << "XXXXXXXXXX While Parent at "
              << loc0.getSpellingLineNumber() << ":"
              << loc1.getSpellingLineNumber() << "\n";
            break;
          }
          //if (isa<CompoundStmt>(pstmt))
          //  break;

        }
        for (Stmt::child_iterator i = stmt->child_begin(), e = stmt->child_end(); i != e; ++i) {
          Stmt *cstmt = *i;
          //cstmt->dump();
          FullSourceLoc loc0 = Context->getFullLoc(cstmt->getBeginLoc());
          FullSourceLoc loc1 = Context->getFullLoc(cstmt->getEndLoc());
          if (isa<BreakStmt>(cstmt)) {
            hasloopparent = true;
            llvm::outs() << "XXXXXXXXXX Break Child at "
              << loc0.getSpellingLineNumber() << ":"
              << loc1.getSpellingLineNumber() << "\n";
            break;
          }
          if (isa<ContinueStmt>(cstmt)) {
            hasloopparent = true;
            llvm::outs() << "XXXXXXXXXX Continue Child at "
              << loc0.getSpellingLineNumber() << ":"
              << loc1.getSpellingLineNumber() << "\n";
            break;
          }
          if (isa<ReturnStmt>(cstmt)) {
            hasloopparent = true;
            llvm::outs() << "XXXXXXXXXX Return Child at "
              << loc0.getSpellingLineNumber() << ":"
              << loc1.getSpellingLineNumber() << "\n";
            break;
          }
        }

#if 0
        const Stmt *pstmt = dynamic_cast<const Stmt*>(stmt);
        while (true) {
          const auto& parents = Context->getParents(*pstmt);
          if ( parents.empty() ) {
            llvm::errs() << "Can not find parent\n";
            return false;
          }
          llvm::errs() << "find parent size=" << parents.size() << "\n";
          pstmt = parents[0].get<Stmt>();
          if (!pstmt)
            return false;
          pstmt->dump();
          if (isa<ForStmt>(pstmt))
            break;
          if (isa<WhileStmt>(pstmt))
            break;
        }
#endif
      }
      return true;

    }

    bool VisitForStmt(ForStmt *stmt) {
      FullSourceLoc FullLocation =
        Context->getFullLoc(stmt->getBeginLoc());
      if (FullLocation.isValid() && FullLocation.getLineNumber() == linenum) {
        llvm::outs() << "XXXXXXXXXX Found for at "
          << FullLocation.getSpellingLineNumber() << ":"
          << FullLocation.getSpellingColumnNumber() << "\n";
        const Stmt* body = stmt->getBody();
        if(body) {
          FullSourceLoc body0 = Context->getFullLoc(body->getBeginLoc());
          FullSourceLoc body1 = Context->getFullLoc(body->getEndLoc());
          llvm::outs() << "XXXXXXXXXX Body at "
            << body0.getSpellingLineNumber() << ":"
            << body1.getSpellingLineNumber() << "\n";
        }
      }
      return true;

    }

    bool VisitWhileStmt(WhileStmt *stmt) {
      FullSourceLoc FullLocation =
        Context->getFullLoc(stmt->getBeginLoc());
      if (FullLocation.isValid() && FullLocation.getLineNumber() == linenum) {
        llvm::outs() << "XXXXXXXXXX Found while at "
          << FullLocation.getSpellingLineNumber() << ":"
          << FullLocation.getSpellingColumnNumber() << "\n";
        const Stmt* body = stmt->getBody();
        if(body) {
          FullSourceLoc body0 = Context->getFullLoc(body->getBeginLoc());
          FullSourceLoc body1 = Context->getFullLoc(body->getEndLoc());
          llvm::outs() << "XXXXXXXXXX Body at "
            << body0.getSpellingLineNumber() << ":"
            << body1.getSpellingLineNumber() << "\n";
        }
      }
      return true;
    }

#if 0
    bool VisitCallExpr(CallExpr *CallExpression) {
      QualType q = CallExpression->getType();
      const clang::Type *t = q.getTypePtrOrNull();

      if (t != NULL) {
        FunctionDecl *func = CallExpression->getDirectCallee();
        //if(func) {
        //  const std::string funcName = func->getNameInfo().getAsString();
        //  if (fName == funcName) {
            FullSourceLoc FullLocation =
              Context->getFullLoc(CallExpression->getBeginLoc());
            if (FullLocation.isValid())
              llvm::outs() << "Found call at "
                << FullLocation.getSpellingLineNumber() << ":"
                << FullLocation.getSpellingColumnNumber() << "\n";
         // }
        //}
      }

      return true;
    }
#endif

  private:
    ASTContext *Context;
    unsigned int linenum;
    bool isloop;
};

class FindNamedCallConsumer : public clang::ASTConsumer {
  public:
    explicit FindNamedCallConsumer(ASTContext *Context, unsigned int linenum, bool isloop)
      : Visitor(Context, linenum, isloop) {}

    virtual void HandleTranslationUnit(clang::ASTContext &Context) {
      Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    }

  private:
    FindNamedCallVisitor Visitor;
};

class FindNamedCallAction : public clang::ASTFrontendAction {
  public:
    FindNamedCallAction() {}
    virtual std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
        clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
      return std::unique_ptr<clang::ASTConsumer>(
          new FindNamedCallConsumer(&Compiler.getASTContext(), _linenum, _isloop));
    }
};


static llvm::cl::OptionCategory MyToolCategory("my-tool options");
// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...\n");

int main(int argc, const char **argv) {
  _linenum = atoi(argv[1]);
  _isloop = true;
  argv[1] = argv[0];
  llvm::outs() << "LINENUM: " << _linenum << "\n";
  int argc2= argc-1;
  CommonOptionsParser OptionsParser(argc2, &argv[1], MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(),
      OptionsParser.getSourcePathList());

  // run the Clang Tool, creating a new FrontendAction (explained below)
  int result = Tool.run(newFrontendActionFactory<FindNamedCallAction>().get());

  std::cout << "OK\n";
}
