#pragma once

#include <llvm/IR/Value.h>
#include <memory>
#include <string>

using namespace llvm;

// namespace {

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  unsigned Val;

public:
  NumberExprAST(double Val) : Val(Val) {}
  Value *codegen() override;
};

class BooleanExprAST : public ExprAST {
  bool Val;
  Value *codegen() override;

public:
  BooleanExprAST(bool Val) : Val(Val) {}
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  VariableExprAST(const std::string &Name) : Name(Name) {}
  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value *codegen() override;
};

// } // namespace
