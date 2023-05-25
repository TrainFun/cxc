#include "ir.h"
#include "AST.h"
#include "parser.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::map<std::string, Value *> NamedValues;

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Value *NumberExprAST::codegen() {
  return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *BooleanExprAST::codegen() {
  return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
  Value *V = NamedValues[Name];
  if (!V)
    LogErrorV("Unknown variable name");
  return V;
}

Value *BinaryExprAST::codegen() {
  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  switch (Op) {
  case '+':
    return Builder->CreateAdd(L, R, "addtmp");
  case '-':
    return Builder->CreateSub(L, R, "subtmp");
  case '*':
    return Builder->CreateMul(L, R, "multmp");
  case '<':
    return Builder->CreateICmpULT(L, R, "cmptmp");
  default:
    return LogErrorV("invalid binary operator");
  }
}
