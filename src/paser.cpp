#include "lexer.h"
#include "parser.h"
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

std::unique_ptr<ExprAST> ParseExpression();

std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken();
  return std::move(Result);
}

std::unique_ptr<ExprAST> ParseBooleanExpr() {
  auto Result = std::make_unique<BooleanExprAST>(CurTok == tok_true);
  getNextToken();
  return std::move(Result);
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
  return nullptr;
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
  case '(':
    return ParseParenExpr();
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

void HandleTopLevelExpression() {
  if (ParseExpression()) {
    fprintf(stderr, "Parsed a top-level expr\n");
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
    case ';':
      getNextToken();
      break;
    default:
      HandleTopLevelExpression();
      break;
    }
  }
}
