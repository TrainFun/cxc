#pragma once

#include <algorithm>
#include <cwchar>
#include <llvm/ADT/StringExtras.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace llvm;

// namespace {

/**
 * @brief Types in CX language.
 */
enum CXType { typ_err = 0, typ_int, typ_bool, typ_double };

/**
 * @brief Convert CX types to LLVM types.
 * @return The corresponding LLVM type.
 */
Type *llvmTypeFromCXType(const enum CXType T);

/**
 * @brief Expression AST node.
 */
class ExprAST {
  enum CXType ExprType;

protected:
  /**
   * @brief Constructor.
   */
  ExprAST() {}
  // ExprAST(enum CXType ExprType) : ExprType(ExprType) {}

  /**
   * @brief Set the CX type of the expression.
   * @param ExprType The designate CX type.
   */
  void setCXType(enum CXType ExprType) { this->ExprType = ExprType; }

public:
  /**
   * @brief Get CX type of this expression.
   * @return CX type.
   */
  enum CXType getCXType() { return ExprType; }
  /**
   * @brief Destructor.
   */
  virtual ~ExprAST() = default;
  /**
   * @brief Generate IR.
   * @return LLVM Value of this AST.
   */
  virtual Value *codegen() = 0;
};

/**
 * Class for representing elements in a block statement. It can either be a
 * *statement* or a *local variable declaration*.
 */
class BlockElemAST {
  /**
   * @brief Is this a declaration or not?
   * @return True if yes and false if no.
   */
  virtual bool isVarDecl() = 0;
};

/**
 * @brief Statement AST node.
 */
class StmtAST : public BlockElemAST {
public:
  /**
   * @brief Destructor
   */
  virtual ~StmtAST() = default;
  /**
   * @brief Generate IR
   * @return LLVM Value of this AST. It is a null value of type void if nothing
   * goes wrong and is nullptr otherwise.
   */
  virtual Value *codegen() = 0;
  bool isVarDecl() override { return false; }
};

/**
 * Statements consisting of one or no expression and a `';'`
 * @brief Expression statment AST node
 */
class ExprStmtAST : public StmtAST {
  /**
   * @brief The expression in this statement. Can be nullptr.
   */
  std::unique_ptr<ExprAST> Expr;

public:
  /**
   * @brief Constructor.
   */
  ExprStmtAST(std::unique_ptr<ExprAST> Expr) : Expr(std::move(Expr)) {}
  Value *codegen() override;
};

/**
 * @brief Integer literal AST node.
 */
class IntExprAST : public ExprAST {
  /**
   * @brief The parsed integer value.
   */
  unsigned Val;

public:
  /**
   * @brief Constructor.
   */
  IntExprAST(unsigned Val) : Val(Val) {}
  Value *codegen() override;
};

/**
 * @brief Double precesion float point literal AST node.
 */
class DoubleExprAST : public ExprAST {
  /**
   * The parsed double value.
   */
  double Val;

public:
  /**
   * @brief Constructor.
   */
  DoubleExprAST(double Val) : Val(Val) {}
  Value *codegen() override;
};

/**
 * @brief Boolean literal AST node. True of false.
 */
class BooleanExprAST : public ExprAST {
  /**
   * The parsed boolean value.
   */
  bool Val;
  Value *codegen() override;

public:
  /**
   * @brief Constructor.
   */
  BooleanExprAST(bool Val) : Val(Val) {}
};

/**
 * AST node of an expression only consisting of a variable.
 * @brief Variable expression AST node.
 */
class VariableExprAST : public ExprAST {
  /**
   * @brief Variable name.
   */
  std::string Name;

public:
  /**
   * @brief Constructor.
   */
  VariableExprAST(const std::string &Name) : Name(Name) {}
  /**
   * @brief Get variable name.
   * @return Variable name.
   */
  const std::string getName() const { return Name; }
  Value *codegen() override;
};

/**
 * @brief Block statement AST node.
 */
class BlockStmtAST : public StmtAST {
  std::vector<std::unique_ptr<BlockElemAST>> Elems;

public:
  /**
   * @brief Constructor
   */
  BlockStmtAST(std::vector<std::unique_ptr<BlockElemAST>> Elems)
      : Elems(std::move(Elems)) {}
  Value *codegen() override;
};

/**
 * AST node of an expression consisting of a unary operator and a unary
 * expression.
 * @brief Unary expression AST node.
 */
class UnaryExprAST : public ExprAST {
  /**
   * @brief The unary operator.
   */
  int Opcode;
  /**
   * @brief Operand of the unary operation.
   */
  std::unique_ptr<ExprAST> Operand;

public:
  /**
   * @brief Constructor.
   */
  UnaryExprAST(int Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}
  Value *codegen() override;
};

/**
 * @brief Binary expression AST node.
 */
class BinaryExprAST : public ExprAST {
  /**
   * @brief Binary operator
   */
  int Op;
  /**
   * @brief Left hand side operand and right hand side operand.
   */
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  /**
   * @brief Constructor.
   */
  BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  Value *codegen() override;
};

/**
 * @brief Call expression AST node.
 */
class CallExprAST : public ExprAST {
  /**
   * @brief Callee's name.
   */
  std::string Callee;
  /**
   * @brief Arguments of function call.
   */
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  /**
   * @brief Constructor.
   */
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}
  Value *codegen() override;
};

/**
 * @brief Declaration AST node.
 */
class DeclAST {
public:
  /**
   * @brief Destructor.
   */
  ~DeclAST() = default;
  /**
   * @brief Generate IR.
   * @return Created LLVM function if this is a function declaration. A null
   * value of type void otherwise.
   */
  virtual Function *codegen() = 0;
  /**
   * @brief Is this a variable declaration?
   * @return True if yes and false if no.
   */
  virtual bool isVarDecl() = 0;
};

/**
 * @brief Variable declaration AST node.
 */
class VarDeclAST : public BlockElemAST, public DeclAST {
protected:
  /**
   * @brief Whether this is a constant variable.
   */
  bool isConst;
  /**
   * @brief CX type of this variable.
   */
  enum CXType Type;
  /**
   * @brief Variable name.
   */
  std::string Name;
  /**
   * @brief Initial value. Can be nullptr.
   */
  std::unique_ptr<ExprAST> Val;

public:
  /**
   * @brief Constructor.
   */
  VarDeclAST(bool isConst, const enum CXType Type, const std::string &Name,
             std::unique_ptr<ExprAST> Val)
      : isConst(isConst), Type(Type), Name(Name), Val(std::move(Val)) {}
  Function *codegen() override;
  /**
   * @brief See if this is a constant variable.
   * @return True if yes and false if no.
   */
  bool isConstVar() { return isConst; }
  /**
   * @brief Get the variable's CX type.
   * @return Its CX type.
   */
  enum CXType getType() { return Type; }
  /**
   * @brief Get the variable's name.
   * @return Its name.
   */
  const std::string &getName() const { return Name; }
  bool isVarDecl() override { return true; }

  /**
   * @brief Override operator `==`
   */
  bool operator==(const VarDeclAST &other) const {
    bool ret = true;
    ret = ret && isConst == other.isConst;
    ret = ret && Type == other.Type;
    ret = ret && Name == other.Name;
    return ret;
  }
};

/**
 * @brief Global variable declaration AST node.
 */
class GlobVarDeclAST : public VarDeclAST {
  using VarDeclAST::VarDeclAST;

public:
  Function *codegen() override;
};

/**
 * @brief Function prototype AST node.
 */
class PrototypeAST : public DeclAST {
  /**
   * @brief Function's return type.
   */
  enum CXType RetTyp;
  /**
   * @brief Function name.
   */
  std::string Name;
  /**
   * @brief Function parameters.
   */
  std::vector<std::unique_ptr<VarDeclAST>> Args;

public:
  /**
   * @brief Constructor.
   */
  PrototypeAST(const enum CXType RetTyp, const std::string &Name,
               std::vector<std::unique_ptr<VarDeclAST>> Args)
      : RetTyp(RetTyp), Name(Name), Args(std::move(Args)) {}

  /**
   * @brief Get function name.
   * @return Function name.
   */
  const std::string &getName() const { return Name; }
  /**
   * @brief Get return type.
   * @return Return type.
   */
  enum CXType getRetType() const { return RetTyp; }
  /**
   * @brief Get parameters.
   * @return Parameters.
   */
  const std::vector<std::unique_ptr<VarDeclAST>> &getArgs() { return Args; }
  Function *codegen() override;
  bool isVarDecl() override { return false; }

  /**
   * @brief Override operator `==`
   */
  bool operator==(const PrototypeAST &other) const {
    bool ret = true;
    ret = ret && RetTyp == other.RetTyp;
    ret = ret && Name == other.Name;
    ret = ret && Args.size() == other.Args.size();

    if (ret) {
      int ArgNum = Args.size();
      for (int i = 0; i < ArgNum; ++i)
        ret = ret && *Args[i] == *other.Args[i];
    }
    return ret;
  }
};

/**
 * Function definition AST.
 */
class FunctionAST : public DeclAST {
  /**
   * @brief Function prototype.
   */
  std::unique_ptr<PrototypeAST> Proto;
  /**
   * @brief Function body.
   */
  std::unique_ptr<BlockStmtAST> Body;

public:
  /**
   * @brief Constructor
   */
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<BlockStmtAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  Function *codegen() override;
  bool isVarDecl() override { return false; }
};

/**
 * @brief If statement AST node.
 */
class IfStmtAST : public StmtAST {
  /**
   * @brief Condition.
   */
  std::unique_ptr<ExprAST> Cond;
  /**
   * @brief Then statement and else statement, the latter can be nullptr.
   */
  std::unique_ptr<StmtAST> Then, Else;

public:
  /**
   * @brief Constructor.
   */
  IfStmtAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<StmtAST> Then,
            std::unique_ptr<StmtAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  Value *codegen() override;
};

/**
 * @brief For statement AST node.
 */
class ForStmtAST : public StmtAST {
  /**
   * @brief CX type of the loop variable. `typ_err` if has no loop variable.
   */
  enum CXType VarType;
  /**
   * @brief Loop variable name. `""` if has no loop variable.
   */
  std::string VarName;
  /**
   * @brief Initial value of loop variable, loop condition, and loop step
   * expression. All can be nullptr;
   */
  std::unique_ptr<ExprAST> Start, End, Step;
  /**
   * @brief Loop body.
   */
  std::unique_ptr<StmtAST> Body;

public:
  /**
   * @brief Constructor.
   */
  ForStmtAST(const enum CXType VarType, const std::string &VarName,
             std::unique_ptr<ExprAST> Start, std::unique_ptr<ExprAST> End,
             std::unique_ptr<ExprAST> Step, std::unique_ptr<StmtAST> Body)
      : VarType(VarType), VarName(VarName), Start(std::move(Start)),
        End(std::move(End)), Step(std::move(Step)), Body(std::move(Body)) {}

  Value *codegen() override;
};

/**
 * @brief Switch statement AST node.
 */
class SwitchStmtAST : public StmtAST {
  /**
   * @brief The expression to be switched.
   */
  std::unique_ptr<ExprAST> Expr;
  /**
   * @brief Labels and corresponding statements.
   */
  std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                        std::vector<std::unique_ptr<StmtAST>>>>
      BasicBlocks;

public:
  /**
   * @brief Constructor.
   */
  SwitchStmtAST(std::unique_ptr<ExprAST> Expr,
                std::vector<std::pair<std::vector<std::unique_ptr<ExprAST>>,
                                      std::vector<std::unique_ptr<StmtAST>>>>
                    BasicBlocks)
      : Expr(std::move(Expr)), BasicBlocks(std::move(BasicBlocks)) {}
  Value *codegen() override;
};

/**
 * @brief While statement AST node.
 */
class WhileStmtAST : public StmtAST {
  /**
   * @brief Loop condition.
   */
  std::unique_ptr<ExprAST> Cond;
  /**
   * @brief Loop body.
   */
  std::unique_ptr<StmtAST> Body;

public:
  /**
   * @brief Constructor.
   */
  WhileStmtAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<StmtAST> Body)
      : Cond(std::move(Cond)), Body(std::move(Body)) {}
  Value *codegen() override;
};

/**
 * @brief Do/while statement AST node.
 */
class DoStmtAST : public StmtAST {
  /**
   * @brief Loop body.
   */
  std::unique_ptr<StmtAST> Body;
  /**
   * @brief Loop condition.
   */
  std::unique_ptr<ExprAST> Cond;

public:
  /**
   * @brief Constructor.
   */
  DoStmtAST(std::unique_ptr<StmtAST> Body, std::unique_ptr<ExprAST> Cond)
      : Body(std::move(Body)), Cond(std::move(Cond)) {}
  Value *codegen() override;
};

/**
 * @brief Repeat/until statement AST node.
 */
class UntilStmtAST : public StmtAST {
  /**
   * @brief Loop body.
   */
  std::unique_ptr<StmtAST> Body;
  /**
   * @brief End condition.
   */
  std::unique_ptr<ExprAST> Cond;

public:
  /**
   * @brief Constructor.
   */
  UntilStmtAST(std::unique_ptr<StmtAST> Body, std::unique_ptr<ExprAST> Cond)
      : Body(std::move(Body)), Cond(std::move(Cond)) {}
  Value *codegen() override;
};

/**
 * @brief Read statement AST node.
 */
class ReadStmtAST : public StmtAST {
  /**
   * @brief Variable to be read to.
   */
  std::unique_ptr<ExprAST> Var;

public:
  /**
   * @brief Constructor.
   */
  ReadStmtAST(std::unique_ptr<ExprAST> Var) : Var(std::move(Var)) {}
  Value *codegen() override;
};

/**
 * @brief Write statement AST node.
 */
class WriteStmtAST : public StmtAST {
  /**
   * @brief Expression to be written.
   */
  std::unique_ptr<ExprAST> Val;

public:
  /**
   * @brief Constructor.
   */
  WriteStmtAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
  Value *codegen() override;
};

/**
 * @brief Continue statement AST node.
 */
class ContStmtAST : public StmtAST {
public:
  Value *codegen() override;
};

/**
 * @brief Break statement AST node.
 */
class BrkStmtAST : public StmtAST {
public:
  Value *codegen() override;
};

/**
 * @brief Return statement AST node.
 */
class RetStmtAST : public StmtAST {
  /**
   * @brief The expression to be returned.
   */
  std::unique_ptr<ExprAST> Val;

public:
  /**
   * @brief Constructor.
   */
  RetStmtAST(std::unique_ptr<ExprAST> Val) : Val(std::move(Val)) {}
  Value *codegen() override;
};

/**
 * AST node of a cast expression, who casts an expression of one type to an
 * expression of another type.
 * @brief Cast expression AST node.
 */
class CastExprAST : public ExprAST {
  /**
   * @brief Destination CX type.
   */
  enum CXType Type;
  /**
   * @brief Expression to be cast.
   */
  std::unique_ptr<ExprAST> From;

public:
  /**
   * @brief Constructor.
   */
  CastExprAST(const enum CXType Type, std::unique_ptr<ExprAST> From)
      : Type(Type), From(std::move(From)) {}
  Value *codegen() override;
};

/**
 * @brief Exit statement AST.
 */
class ExitStmtAST : public StmtAST {
  /**
   * @brief Exit code.
   */
  std::unique_ptr<ExprAST> ExitCode;

public:
  /**
   * @brief Constructor.
   */
  ExitStmtAST(std::unique_ptr<ExprAST> ExitCode)
      : ExitCode(std::move(ExitCode)) {}
  Value *codegen() override;
};

// } // namespace
