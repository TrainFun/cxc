#include "parser.h"
#include "AST.h"
#include "ir.h"
#include "lexer.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <pthread.h>
#include <string>

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
  fprintf(stderr, "Error: %s\n", Str);
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

std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
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

std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken();

  if (CurTok != '(')
    return LogError("Expected '(' in if");
  getNextToken();

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != ')')
    return LogError("Expected ')' in if");
  getNextToken();

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("Expected else");
  getNextToken();

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}

enum CXType ParseType();

std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken(); // eat "for"

  if (CurTok != '(')
    return LogError("Expect '(' after for");
  getNextToken();

  enum CXType IdType = ParseType();
  if (IdType == typ_err)
    return nullptr;

  if (CurTok != tok_identifier)
    return LogError("Expect identifier in for");

  std::string IdName = IdentifierStr;
  getNextToken();

  if (CurTok != '=')
    return LogError("Expected '=' in for");
  getNextToken();

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ';')
    return LogError("Expected ';' after loop variable definition");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;
  if (CurTok != ';')
    return LogError("Expected ';' after loop condition");
  getNextToken();

  auto Step = ParseExpression();
  if (!Step)
    return nullptr;

  if (CurTok != ')')
    return LogError("Expect ')' in for");
  getNextToken();

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(IdType, IdName, std::move(Start),
                                      std::move(End), std::move(Step),
                                      std::move(Body));
}

std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  default:
    return LogError("unknown token when expecting an expression");
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case tok_true:
    return ParseBooleanExpr();
  case tok_false:
    return ParseBooleanExpr();
  case '(':
    return ParseParenExpr();
  case tok_if:
    return ParseIfExpr();
  case tok_for:
    return ParseForExpr();
  }
}

std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                       std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    auto RHS = ParsePrimary();
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
  auto LHS = ParsePrimary();
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
  }
  getNextToken();
  return Type;
}

std::unique_ptr<DeclAST> ParseDeclaration() {
  enum CXType Type = ParseType();
  if (!Type)
    return nullptr;

  if (CurTok != tok_identifier)
    return LogErrorD("Expected variable name in declaration");
  std::string VarName = IdentifierStr;
  getNextToken();

  return std::make_unique<DeclAST>(Type, VarName);
}

std::unique_ptr<PrototypeAST> ParsePrototype() {
  enum CXType RetTyp = ParseType();
  if (!RetTyp)
    return nullptr;

  if (CurTok != tok_identifier)
    return LogErrorP("Expected function name in prototype");
  std::string FnName = IdentifierStr;
  getNextToken();

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");
  getNextToken();
  std::vector<std::pair<enum CXType, std::string>> Params;

  if (CurTok != ')') {
    while (true) {
      enum CXType ParamType = ParseType();
      if (!ParamType)
        return nullptr;

      if (CurTok != tok_identifier)
        return LogErrorP("Expected variable name in prototype");
      std::string ParamName = IdentifierStr;
      getNextToken();

      Params.push_back(std::make_pair(ParamType, ParamName));

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogErrorP("Expected ')' or ',' in prototype");
      getNextToken();
    }
  }

  getNextToken();

  return std::make_unique<PrototypeAST>(RetTyp, FnName, std::move(Params));
}

// 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
  getNextToken();
  auto Proto = ParsePrototype();
  if (!Proto)
    return nullptr;

  if (auto E = ParseExpression())
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  return nullptr;
}

std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
  if (auto E = ParseExpression()) {
    auto Proto = std::make_unique<PrototypeAST>(
        typ_int, "", std::vector<std::pair<enum CXType, std::string>>());
    return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
  }
  return nullptr;
}

void InitializeModule() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("my cool jit", *TheContext);

  Builder = std::make_unique<IRBuilder<>>(*TheContext);
}

void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read function definition:");
      FnIR->print(errs());
      fprintf(stderr, "\n");
    }
  } else {
    getNextToken();
  }
}

void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Read top-level expression:");
      FnIR->print(errs());
      fprintf(stderr, "\n");

      FnIR->eraseFromParent();
    }
  } else {
    getNextToken();
  }
}

void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    // case ';':
    case '$':
      getNextToken();
      break;
    case tok_def:
      HandleDefinition();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}
