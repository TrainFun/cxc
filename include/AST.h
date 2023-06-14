#pragma once

#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

// namespace {

enum CXType { typ_err = 0, typ_int, typ_bool };

class ExprAST {
  enum CXType ExprType;

protected:
  ExprAST() {}
  // ExprAST(enum CXType ExprType) : ExprType(ExprType) {}
  void setCXType(enum CXType ExprType) { this->ExprType = ExprType; }

public:
  enum CXType getCXType() { return ExprType; }
  virtual ~ExprAST() = default;
  virtual Value *codegen() = 0;
};

class NumberExprAST : public ExprAST {
  unsigned Val;

public:
  NumberExprAST(unsigned Val) : Val(Val) {}
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
  const std::string getName() const { return Name; }
  Value *codegen() override;
};

class BinaryExprAST : public ExprAST {
  int Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value *codegen() override;
};

class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}
  Value *codegen() override;
};

class DeclAST {
  enum CXType Type;
  std::string Name;

public:
  DeclAST(const enum CXType Type, const std::string &Name)
      : Type(Type), Name(Name) {}
};

class PrototypeAST {
  enum CXType RetTyp;
  std::string Name;
  std::vector<std::pair<CXType, std::string>> Args;

public:
  PrototypeAST(const enum CXType RetTyp, const std::string &Name,
               std::vector<std::pair<enum CXType, std::string>> Args)
      : RetTyp(RetTyp), Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
  enum CXType getRetType() const { return RetTyp; }
  Function *codegen();
};

class FunctionAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  Function *codegen();
};

class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  Value *codegen() override;
};

class ForExprAST : public ExprAST {
  enum CXType VarType;
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const enum CXType VarType, const std::string &VarName,
             std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
             std::unique_ptr<ExprAST> Step, std::unique_ptr<ExprAST> Body)
      : VarType(VarType), VarName(VarName), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

  Value *codegen() override;
};

class UnaryExprAST : public ExprAST {
  int Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(int Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}
  Value *codegen() override;
};

// } // namespace
