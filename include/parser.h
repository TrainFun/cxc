#pragma once

#include <map>
#include <memory>
#include <string>

namespace {

class ExprAST {
public:
  virtual ~ExprAST() = default;
};

class NumberExprAST : public ExprAST {
  unsigned Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
};

class BooleanExprAST : public ExprAST {
  bool Val;

public:
  BooleanExprAST(bool Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
};

} // namespace

extern std::map<int, int> BinopPrecedence;

int getNextToken();
void MainLoop();
