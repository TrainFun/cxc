#pragma once

#include <algorithm>
#include <llvm/ADT/StringExtras.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

// namespace {

enum CXType { typ_err = 0, typ_int, typ_bool, typ_double };

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

class BlockElemAST {};

class StmtAST : public BlockElemAST {
public:
  virtual ~StmtAST() = default;
  virtual Value *codegen() = 0;
};

class ExprStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Expr;

public:
  ExprStmtAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
  Value *codegen() override;
};

class IntExprAST : public ExprAST {
  unsigned Val;

public:
  IntExprAST(unsigned Val) : Val(Val) {}
  Value *codegen() override;
};

class DoubleExprAST : public ExprAST {
  double Val;

public:
  DoubleExprAST(double Val) : Val(Val) {}
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

class BlockStmtAST : public StmtAST {
  std::vector<std::unique_ptr<BlockElemAST>> Elems;

public:
  BlockStmtAST(std::vector<std::unique_ptr<BlockElemAST>> Elems)
      : Elems(std::move(Elems)) {}
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
public:
  ~DeclAST() = default;
  virtual Function *codegen() = 0;
};

class VarDeclAST : public BlockElemAST, public DeclAST {
  bool isConst;
  enum CXType Type;
  std::string Name;
  std::unique_ptr<ExprAST> Val;

public:
  VarDeclAST(bool isConst, const enum CXType Type, const std::string &Name,
             std::unique_ptr<ExprAST> Val)
      : isConst(isConst), Type(Type), Name(Name), Val(std::move(Val)) {}
  Function *codegen() override;
  bool isConstVar() { return isConst; }
  enum CXType getType() { return Type; }
  const std::string &getName() const { return Name; }
};

class GlobVarDeclAST : public VarDeclAST {
  using VarDeclAST::VarDeclAST;

public:
  Function *codegen() override;
};

class PrototypeAST : public DeclAST {
  enum CXType RetTyp;
  std::string Name;
  std::vector<std::unique_ptr<VarDeclAST>> Args;

public:
  PrototypeAST(const enum CXType RetTyp, const std::string &Name,
               std::vector<std::unique_ptr<VarDeclAST>> Args)
      : RetTyp(RetTyp), Name(Name), Args(std::move(Args)) {}

  const std::string &getName() const { return Name; }
  enum CXType getRetType() const { return RetTyp; }
  const std::vector<std::unique_ptr<VarDeclAST>> &getArgs() { return Args; }
  Function *codegen() override;
};

class FunctionAST : public DeclAST {
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<BlockStmtAST> Body;

public:
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<BlockStmtAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  Function *codegen() override;
};

class IfStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Cond;
  std::unique_ptr<StmtAST> Then, Else;

public:
  IfStmtAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<StmtAST> Then,
            std::unique_ptr<StmtAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  Value *codegen() override;
};

class ForStmtAST : public StmtAST {
  enum CXType VarType;
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End;
  std::unique_ptr<StmtAST> Step, Body;

public:
  ForStmtAST(const enum CXType VarType, const std::string &VarName,
             std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
             std::unique_ptr<StmtAST> Step, std::unique_ptr<StmtAST> Body)
      : VarType(VarType), VarName(VarName), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

  Value *codegen() override;
};

class SwitchStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Expr;
  std::vector<std::pair<std::unique_ptr<ExprAST>,
                        std::vector<std::unique_ptr<StmtAST>>>>
      BasicBlocks;

public:
  SwitchStmtAST(std::unique_ptr<ExprAST> Expr,
                std::vector<std::pair<std::unique_ptr<ExprAST>,
                                      std::vector<std::unique_ptr<StmtAST>>>>
                    BasicBlocks)
      : Expr(std::move(Expr)), BasicBlocks(std::move(BasicBlocks)) {}
  Value *codegen() override;
};

class WhileStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Cond;
  std::unique_ptr<StmtAST> Stmt;

public:
  WhileStmtAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<StmtAST> Stmt)
      : Cond(std::move(Cond)), Stmt(std::move(Stmt)) {}
  Value *codegen() override;
};

class DoStmtAST : public StmtAST {
  std::unique_ptr<StmtAST> Stmt;
  std::unique_ptr<ExprAST> Cond;

public:
  DoStmtAST(std::unique_ptr<StmtAST> Stmt, std::unique_ptr<ExprAST> Cond)
      : Stmt(std::move(Stmt)), Cond(std::move(Cond)) {}
  Value *codegen() override;
};

class UntilStmtAST : public StmtAST {
  std::unique_ptr<StmtAST> Stmt;
  std::unique_ptr<ExprAST> Cond;

public:
  UntilStmtAST(std::unique_ptr<StmtAST> Stmt, std::unique_ptr<ExprAST> Cond)
      : Stmt(std::move(Stmt)), Cond(std::move(Cond)) {}
  Value *codegen() override;
};

class ReadStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Var;

public:
  ReadStmtAST(std::unique_ptr<ExprAST> Var) : Var(std::move(Var)) {}
  Value *codegen() override;
};

class WriteStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Val;

public:
  WriteStmtAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
  Value *codegen() override;
};

class ContStmtAST : public StmtAST {
public:
  Value *codegen() override;
};

class BrkStmtAST : public StmtAST {
public:
  Value *codegen() override;
};

class RetStmtAST : public StmtAST {
  std::unique_ptr<ExprAST> Val;

public:
  RetStmtAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
  Value *codegen() override;
};

// } // namespace
