#include "parser.h"
#include "AST.h"
#include "ir.h"
#include "lexer.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>
#include <utility>

int CurTok;
std::map<int, int> BinopPrecedence;

int getNextToken() { return CurTok = gettok(); }

int GetTokPrecedence() {
  // if (!isascii(CurTok))
  //   return -1;

  int TokPrec = BinopPrecedence[CurTok];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "line %u Error: %s\n", NR, Str);
  return nullptr;
}

std::unique_ptr<StmtAST> LogErrorS(const char *Str) {
  LogError(Str);
  return nullptr;
}

std::unique_ptr<DeclAST> LogErrorD(const char *Str) {
  LogError(Str);
  return nullptr;
}

std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}

std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<ExprAST> ParseIntExpr() {
  auto Result = std::make_unique<IntExprAST>((unsigned)NumVal);
  getNextToken();
  return Result;
}

std::unique_ptr<ExprAST> ParseDoubleExpr() {
  auto Result = std::make_unique<DoubleExprAST>(NumVal);
  getNextToken();
  return Result;
}

std::unique_ptr<ExprAST> ParseBooleanExpr() {
  auto Result = std::make_unique<BooleanExprAST>(CurTok == tok_true);
  getNextToken();
  return Result;
}

std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // ')'
  return V;
}

std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;

  getNextToken();

  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(IdName);

  // Call.
  getNextToken();
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  getNextToken();

  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}

std::unique_ptr<StmtAST> ParseStatement();

std::unique_ptr<StmtAST> ParseIfStmt() {
  getNextToken(); // eat "if"

  if (CurTok != '(')
    return LogErrorS("Expected '(' in if");
  getNextToken();

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != ')')
    return LogErrorS("Expected ')' in if");
  getNextToken();

  auto Then = ParseStatement();
  if (!Then)
    return nullptr;

  std::unique_ptr<StmtAST> Else = nullptr;
  if (CurTok == tok_else) {
    getNextToken();
    Else = ParseStatement();
    if (!Else)
      return nullptr;
  }

  return std::make_unique<IfStmtAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}

enum CXType ParseType();

std::unique_ptr<StmtAST> ParseSwitchStmt() {
  getNextToken(); // eat "swtich"

  if (CurTok != '(')
    return LogErrorS("Expect '(' in switch");
  getNextToken();

  auto Expr = ParseExpression();
  if (!Expr)
    return nullptr;

  if (CurTok != ')')
    return LogErrorS("Expect ')' in switch");
  getNextToken();

  if (CurTok != '{')
    return LogErrorS("Expect '{' in switch");
  getNextToken();

  std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                        std::vector<std::unique_ptr<StmtAST>>>>
      CaseList;

  if (CurTok == '}')
    return std::make_unique<SwitchStmtAST>(std::move(Expr),
                                           std::move(CaseList));

  if (CurTok != tok_case && CurTok != tok_default) {
    return LogErrorS("Expect switch starting with 'case' or 'default'");
  }

  bool hasDefault = false;

  do {
    std::vector<std::unique_ptr<ExprAST>> Matches;
    do {
      std::unique_ptr<ExprAST> Match = nullptr;
      if (CurTok == tok_case) {
        getNextToken();
        Match = ParseExpression();
        if (!Match)
          return nullptr;
      } else {
        if (hasDefault)
          return LogErrorS("Switch has more than one 'default'");
        hasDefault = true;
        getNextToken();
      }
      if (CurTok != ':')
        return LogErrorS("Expect ':' after 'case' or 'default'");
      getNextToken();

      Matches.push_back(std::move(Match));
    } while (CurTok == tok_case || CurTok == tok_default);

    std::vector<std::unique_ptr<StmtAST>> Stmts;
    while (CurTok != tok_case && CurTok != tok_default && CurTok != '}') {
      auto Stmt = ParseStatement();
      if (!Stmt)
        return nullptr;
      Stmts.push_back(std::move(Stmt));
    }

    CaseList.push_back(std::make_pair(std::move(Matches), std::move(Stmts)));
  } while (CurTok == tok_case || CurTok == tok_default);

  getNextToken(); // eat '}'

  return std::make_unique<SwitchStmtAST>(std::move(Expr), std::move(CaseList));
}

std::unique_ptr<StmtAST> ParseWhileStmt() {
  getNextToken(); // eat "while"

  if (CurTok != '(')
    return LogErrorS("Expect '(' in while");
  getNextToken();

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != ')')
    return LogErrorS("Expect ')' in while");
  getNextToken();

  auto Stmt = ParseStatement();
  if (!Stmt)
    return nullptr;

  return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Stmt));
}

std::unique_ptr<StmtAST> ParseDoStmt() {
  getNextToken(); // eat "do"

  auto Stmt = ParseStatement();
  if (!Stmt)
    return nullptr;

  if (CurTok != tok_while)
    return LogErrorS("Expect 'while' in 'do-while'");
  getNextToken();

  if (CurTok != '(')
    return LogErrorS("Expect '(' in do-while");
  getNextToken();

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != ')')
    return LogErrorS("Expect ')' in do-while");
  getNextToken();

  if (CurTok != ';')
    return LogErrorS("Expect ';' after do-while");

  return std::make_unique<DoStmtAST>(std::move(Stmt), std::move(Cond));
}

std::unique_ptr<StmtAST> ParseForStmt() {
  getNextToken(); // eat "for"

  if (CurTok != '(')
    return LogErrorS("Expect '(' after for");
  getNextToken();

  enum CXType IdType = typ_err;
  std::string IdName = "";
  std::unique_ptr<ExprAST> Start = nullptr;
  if (CurTok != ';') {
    IdType = ParseType();
    if (IdType == typ_err)
      return nullptr;

    if (CurTok != tok_identifier)
      return LogErrorS("Expect identifier in for");

    IdName = IdentifierStr;
    getNextToken();

    if (CurTok == '=') {
      getNextToken();

      Start = ParseExpression();
      if (!Start)
        return nullptr;
    }
    if (CurTok != ';')
      return LogErrorS("Expected ';' after loop variable definition");
  }
  getNextToken(); // eat ';'

  std::unique_ptr<ExprAST> End = nullptr;
  if (CurTok != ';') {
    End = ParseExpression();
    if (!End)
      return nullptr;
    if (CurTok != ';')
      return LogErrorS("Expected ';' after loop condition");
  }
  getNextToken(); // eat ';'

  std::unique_ptr<ExprAST> Step = nullptr;
  if (CurTok != ')') {
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != ')')
    return LogErrorS("Expect ')' in for");
  getNextToken();

  auto Body = ParseStatement();
  if (!Body)
    return nullptr;

  return std::make_unique<ForStmtAST>(IdType, IdName, std::move(Start),
                                      std::move(End), std::move(Step),
                                      std::move(Body));
}

std::unique_ptr<StmtAST> ParseUntilStmt() {
  getNextToken(); // eat "repeat"

  auto Stmt = ParseStatement();
  if (!Stmt)
    return nullptr;

  if (CurTok != tok_while)
    return LogErrorS("Expect 'while' in 'repeat-until'");
  getNextToken();

  if (CurTok != '(')
    return LogErrorS("Expect '(' in repeat-until");
  getNextToken();

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != ')')
    return LogErrorS("Expect ')' in repeat-until");
  getNextToken();

  if (CurTok != ';')
    return LogErrorS("Expect ';' after repeat-until");

  return std::make_unique<UntilStmtAST>(std::move(Stmt), std::move(Cond));
}

std::unique_ptr<StmtAST> ParseReadStmt() {
  getNextToken(); // eat "read"

  auto Var = ParseExpression();
  if (!Var)
    return nullptr;

  if (CurTok != ';')
    return LogErrorS("Expect ';' after read");
  getNextToken();

  return std::make_unique<ReadStmtAST>(std::move(Var));
}

std::unique_ptr<StmtAST> ParseWriteStmt() {
  getNextToken(); // eat "write"

  auto Val = ParseExpression();
  if (!Val)
    return nullptr;

  if (CurTok != ';')
    return LogErrorS("Expect ';' after read");
  getNextToken();

  return std::make_unique<WriteStmtAST>(std::move(Val));
}

std::unique_ptr<DeclAST> ParseDeclaration();

std::unique_ptr<StmtAST> ParseBlockStmt() {
  getNextToken(); // eat '{'

  std::vector<std::unique_ptr<BlockElemAST>> Elems;
  while (CurTok != '}') {
    if (CurTok == tok_const || CurTok == tok_int || CurTok == tok_bool ||
        CurTok == tok_double) {
      auto Decl = ParseDeclaration();
      if (!Decl)
        return nullptr;
      auto VarDecl = std::unique_ptr<VarDeclAST>{
          static_cast<VarDeclAST *>(Decl.release())};
      Elems.push_back(std::move(VarDecl));
    } else {
      auto Stmt = ParseStatement();
      if (!Stmt)
        return nullptr;
      Elems.push_back(std::move(Stmt));
    }
  }

  // if (CurTok != '}')
  //   return LogErrorS("Expect '}' after block");
  getNextToken();

  return std::make_unique<BlockStmtAST>(std::move(Elems));
}

std::unique_ptr<StmtAST> ParseRetStmt() {
  getNextToken(); // eat "return"

  auto Val = ParseExpression();
  if (!Val)
    return nullptr;

  if (CurTok != ';')
    return LogErrorS("Expect ';' after return");
  getNextToken();

  return std::make_unique<RetStmtAST>(std::move(Val));
}

std::unique_ptr<StmtAST> ParseExprStmt() {
  std::unique_ptr<ExprAST> Expr = nullptr;
  if (CurTok != ';') {
    Expr = ParseExpression();
    if (!Expr)
      return nullptr;
  }
  getNextToken(); // eat ';'

  return std::make_unique<ExprStmtAST>(std::move(Expr));
}

std::unique_ptr<StmtAST> ParseContStmt() {
  getNextToken();
  if (CurTok != ';')
    return LogErrorS("Expect ';' after continue");
  return std::make_unique<ContStmtAST>();
}

std::unique_ptr<StmtAST> ParseBrkStmt() {
  getNextToken();
  if (CurTok != ';')
    return LogErrorS("Expect ';' after break");
  return std::make_unique<BrkStmtAST>();
}

std::unique_ptr<StmtAST> ParseExitStmt() {
  getNextToken();
  auto ExitCode = ParseExpression();
  if (!ExitCode)
    return nullptr;
  if (CurTok != ';')
    return LogErrorS("Expect ';' after exit");
  return std::make_unique<ExitStmtAST>(std::move(ExitCode));
}

std::unique_ptr<StmtAST> ParseStatement() {
  switch (CurTok) {
  default:
    return LogErrorS("unknown token when expecting a statement");
  case ';':
  case '!':
  case '(':
  case tok_increment:
  case tok_decrement:
  case tok_ODD:
  case tok_intliteral:
  case tok_doubleliteral:
  case tok_true:
  case tok_false:
  case tok_identifier:
  case tok_cast:
    return ParseExprStmt();
  case '{':
    return ParseBlockStmt();
  case tok_if:
    return ParseIfStmt();
  case tok_switch:
    return ParseSwitchStmt();
  case tok_while:
    return ParseWhileStmt();
  case tok_do:
    return ParseDoStmt();
  case tok_for:
    return ParseForStmt();
  case tok_repeat:
    return ParseUntilStmt();
  case tok_read:
    return ParseReadStmt();
  case tok_write:
    return ParseWriteStmt();
  case tok_continue:
    return ParseContStmt();
  case tok_break:
    return ParseBrkStmt();
  case tok_return:
    return ParseRetStmt();
  case tok_exit:
    return ParseExitStmt();
  }
}

std::unique_ptr<ExprAST> ParseCastExpr() {
  getNextToken(); // eat "cast"

  if (CurTok != '<')
    return LogError("Expected '<' in cast");
  getNextToken();

  auto Type = ParseType();
  if (Type == typ_err)
    return nullptr;

  if (CurTok != '>')
    return LogError("Expected '>' in cast");
  getNextToken();

  if (CurTok != '(')
    return LogError("Expected '(' in cast");

  auto From = ParseParenExpr();
  if (!From)
    return nullptr;

  return std::make_unique<CastExprAST>(Type, std::move(From));
}

std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_intliteral:
    return ParseIntExpr();
  case tok_doubleliteral:
    return ParseDoubleExpr();
  case tok_true:
    return ParseBooleanExpr();
  case tok_false:
    return ParseBooleanExpr();
  case '(':
    return ParseParenExpr();
  case tok_cast:
    return ParseCastExpr();
  }
}

std::unique_ptr<ExprAST> ParseUnary() {
  if (CurTok == tok_identifier || CurTok == tok_intliteral ||
      CurTok == tok_doubleliteral || CurTok == tok_true ||
      CurTok == tok_false || CurTok == '(' || CurTok == tok_cast)
    return ParsePrimary();

  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}

std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                       std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    auto RHS = ParseUnary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}

enum CXType ParseType() {
  enum CXType Type;
  switch (CurTok) {
  default:
    LogError("Expected type");
    return typ_err;
  case tok_int:
    Type = typ_int;
    break;
  case tok_bool:
    Type = typ_bool;
    break;
  case tok_double:
    Type = typ_double;
    break;
  }
  getNextToken();
  return Type;
}

std::unique_ptr<DeclAST> ParseDeclaration() {
  bool isConst = false;
  if (CurTok == tok_const) {
    isConst = true;
    getNextToken();
  }

  enum CXType Type = ParseType();
  if (!Type)
    return nullptr;

  if (CurTok != tok_identifier)
    return LogErrorD("Expected variable name in declaration");
  std::string VarName = IdentifierStr;
  getNextToken();

  std::unique_ptr<ExprAST> Val = nullptr;
  if (CurTok == '=') {
    getNextToken();
    Val = ParseExpression();
    if (!Val)
      return nullptr;
  }
  if (CurTok != ';')
    return LogErrorD("Expect ';' after declaration");
  getNextToken();
  return std::make_unique<VarDeclAST>(isConst, Type, VarName, std::move(Val));
}

std::unique_ptr<DeclAST> ParseTopLevelDeclaration() {
  bool isConst = false;
  if (CurTok == tok_const) {
    isConst = true;
    getNextToken();
  }

  enum CXType Type = ParseType();
  if (!Type)
    return nullptr;

  if (CurTok != tok_identifier)
    return LogErrorD("Expected variable name in declaration");
  std::string VarName = IdentifierStr;
  getNextToken();

  if (CurTok != '(') {
    std::unique_ptr<ExprAST> Val = nullptr;
    if (CurTok == '=') {
      getNextToken();
      Val = ParseExpression();
      if (!Val)
        return nullptr;
    }
    if (CurTok != ';')
      return LogErrorD("Expect ';' after declaration");
    getNextToken();
    return std::make_unique<GlobVarDeclAST>(isConst, Type, VarName,
                                            std::move(Val));
  }

  // Prototype.
  if (isConst)
    return LogErrorD("Const functions are not supported");

  getNextToken(); // eat '('

  std::vector<std::unique_ptr<VarDeclAST>> Params;

  if (CurTok != ')') {
    while (true) {
      bool isConstParam = false;
      if (CurTok == tok_const) {
        isConstParam = true;
        getNextToken();
      }
      enum CXType ParamType = ParseType();
      if (!ParamType)
        return nullptr;

      if (CurTok != tok_identifier)
        return LogErrorP("Expected variable name in prototype");
      std::string ParamName = IdentifierStr;
      getNextToken();

      Params.push_back(std::make_unique<VarDeclAST>(isConstParam, ParamType,
                                                    ParamName, nullptr));

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogErrorP("Expected ')' or ',' in prototype");
      getNextToken();
    }
  }

  getNextToken(); // eat ')'

  if (CurTok == ';') {
    getNextToken();
    return std::make_unique<PrototypeAST>(Type, VarName, std::move(Params));
  }

  if (CurTok != '{')
    return LogErrorP("Expected function body");

  auto Proto = std::make_unique<PrototypeAST>(Type, VarName, std::move(Params));

  if (auto Body = ParseBlockStmt()) {
    auto BlockBody = std::unique_ptr<BlockStmtAST>{
        static_cast<BlockStmtAST *>(Body.release())};
    return std::make_unique<FunctionAST>(std::move(Proto),
                                         std::move(BlockBody));
  }
  return nullptr;
}

// std::unique_ptr<PrototypeAST> ParsePrototype() {
//   enum CXType RetTyp = ParseType();
//   if (!RetTyp)
//     return nullptr;
//
//   if (CurTok != tok_identifier)
//     return LogErrorP("Expected function name in prototype");
//   std::string FnName = IdentifierStr;
//   getNextToken();
//
//   if (CurTok != '(')
//     return LogErrorP("Expected '(' in prototype");
//   getNextToken();
//   std::vector<std::pair<enum CXType, std::string>> Params;
//
//   if (CurTok != ')') {
//     while (true) {
//       enum CXType ParamType = ParseType();
//       if (!ParamType)
//         return nullptr;
//
//       if (CurTok != tok_identifier)
//         return LogErrorP("Expected variable name in prototype");
//       std::string ParamName = IdentifierStr;
//       getNextToken();
//
//       Params.push_back(std::make_pair(ParamType, ParamName));
//
//       if (CurTok == ')')
//         break;
//
//       if (CurTok != ',')
//         return LogErrorP("Expected ')' or ',' in prototype");
//       getNextToken();
//     }
//   }
//
//   getNextToken();
//
//   return std::make_unique<PrototypeAST>(RetTyp, FnName, std::move(Params));
// }

// 'def' prototype expression
// std::unique_ptr<FunctionAST> ParseDefinition() {
//   getNextToken();
//   auto Proto = ParsePrototype();
//   if (!Proto)
//     return nullptr;
//
//   if (auto E = ParseExpression())
//     return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
//   return nullptr;
// }
//
// std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
//   if (auto E = ParseExpression()) {
//     auto Proto = std::make_unique<PrototypeAST>(
//         typ_int, "", std::vector<std::pair<enum CXType, std::string>>());
//     return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
//   }
//   return nullptr;
// }

// void HandleDefinition() {
//   if (auto FnAST = ParseDefinition()) {
//     if (auto *FnIR = FnAST->codegen()) {
//       fprintf(stderr, "Read function definition:");
//       FnIR->print(errs());
//       fprintf(stderr, "\n");
//     }
//   } else {
//     getNextToken();
//   }
// }
//
// void HandleTopLevelExpression() {
//   if (auto FnAST = ParseTopLevelExpr()) {
//     if (auto *FnIR = FnAST->codegen()) {
//       fprintf(stderr, "Read top-level expression:");
//       FnIR->print(errs());
//       fprintf(stderr, "\n");
//
//       FnIR->eraseFromParent();
//     }
//   } else {
//     getNextToken();
//   }
// }

void HandleTopLevelDeclaration() {
  if (auto DeclAST = ParseTopLevelDeclaration()) {
    if (DeclAST->isVarDecl()) {
      auto D = std::unique_ptr<GlobVarDeclAST>(
          static_cast<GlobVarDeclAST *>(DeclAST.release())); // WHY?
      D->codegen();
      return;
    }

    // Function.
    if (auto DeclIR = DeclAST->codegen()) {
      DeclIR->print(errs());
      fprintf(stderr, "\n");

      auto P = std::unique_ptr<PrototypeAST>(
          dynamic_cast<PrototypeAST *>(DeclAST.release()));
      if (P)
        NamedFns[P->getName()] = std::move(P);
    }
    return;
  }

  // Parsing fails.
  while (true) {
    auto LastTok = CurTok;
    getNextToken();
    if (LastTok == ';' || LastTok == '}')
      break;
  }
}

void MainLoop() {
  while (true) {
    // fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break;
    default:
      HandleTopLevelDeclaration();
      break;
    }
  }
}
