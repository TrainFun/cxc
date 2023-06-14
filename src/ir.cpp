#include "ir.h"
#include "AST.h"
#include "parser.h"
#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <memory>

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::map<std::string, AllocaInst *> NamedValues;

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Value *NumberExprAST::codegen() {
  setCXType(typ_int);
  return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *BooleanExprAST::codegen() {
  setCXType(typ_bool);
  return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
  AllocaInst *A = NamedValues[Name];
  if (!A)
    return LogErrorV("Unknown variable name");

  if (A->getAllocatedType() == Type::getInt32Ty(*TheContext))
    setCXType(typ_int);
  if (A->getAllocatedType() == Type::getInt1Ty(*TheContext))
    setCXType(typ_bool);

  return Builder->CreateLoad(A->getAllocatedType(), A, Name.c_str());
}

Value *BinaryExprAST::codegen() {
  if (Op == '=') {
    VariableExprAST *LHSE = static_cast<VariableExprAST *>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");

    Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;

    AllocaInst *Variable = NamedValues[LHSE->getName()];
    if (!Variable)
      return LogErrorV("Unknown variable name");

    if (Variable->getAllocatedType() == Type::getInt32Ty(*TheContext))
      setCXType(typ_int);
    if (Variable->getAllocatedType() == Type::getInt1Ty(*TheContext))
      setCXType(typ_bool);

    if (RHS->getCXType() != getCXType())
      return LogErrorV("Different types on each side of '='");

    Builder->CreateStore(Val, Variable);
    return Val;
  }

  Value *L = LHS->codegen();
  Value *R = RHS->codegen();
  if (!L || !R)
    return nullptr;

  if (LHS->getCXType() != RHS->getCXType())
    return LogErrorV("Binary operation on expressions of different types");

  switch (Op) {
  default:
    return LogErrorV("invalid binary operator");
  case '+':
    setCXType(LHS->getCXType());
    return Builder->CreateAdd(L, R, "addtmp");
  case '-':
    setCXType(LHS->getCXType());
    return Builder->CreateSub(L, R, "subtmp");
  case '*':
    setCXType(LHS->getCXType());
    return Builder->CreateMul(L, R, "multmp");
  case '<':
    setCXType(typ_bool);
    return Builder->CreateICmpULT(L, R, "cmptmp");
  }
}

Value *CallExprAST::codegen() {
  Function *CalleeF = TheModule->getFunction(Callee);
  if (!CalleeF)
    return LogErrorV("Unknown function referenced");

  if (CalleeF->arg_size() != Args.size())
    return LogErrorV("Incorrect # arguments passed");

  std::vector<Value *> ArgsV;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgsV.push_back(Args[i]->codegen());
    if (!ArgsV.back())
      return nullptr;
  }

  if (CalleeF->getReturnType() == Type::getInt32Ty(*TheContext))
    setCXType(typ_int);
  if (CalleeF->getReturnType() == Type::getInt1Ty(*TheContext))
    setCXType(typ_bool);

  return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
}

AllocaInst *CreateEntryBlockAlloca(Function *TheFunction,
                                   const enum CXType VarType,
                                   const std::string &VarName) {
  IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                   TheFunction->getEntryBlock().begin());

  switch (VarType) {
  case typ_int:
    return TmpB.CreateAlloca(Type::getInt32Ty(*TheContext), nullptr, VarName);
  case typ_bool:
    return TmpB.CreateAlloca(Type::getInt1Ty(*TheContext), nullptr, VarName);
  default:
    LogError("Unreachable!");
    return nullptr;
  }
}

Function *PrototypeAST::codegen() {
  std::vector<Type *> ArgsT;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    switch (Args[i].first) {
    case typ_int:
      ArgsT.push_back(Type::getInt32Ty(*TheContext));
      break;
    case typ_bool:
      ArgsT.push_back(Type::getInt1Ty(*TheContext));
      break;
    default:
      return (Function *)LogErrorV("invalid parameter type");
    }
  }

  FunctionType *FT;
  switch (RetTyp) {
  case typ_int:
    FT = FunctionType::get(Type::getInt32Ty(*TheContext), ArgsT, false);
    break;
  case typ_bool:
    FT = FunctionType::get(Type::getInt1Ty(*TheContext), ArgsT, false);
    break;
  default:
    return (Function *)LogErrorV("invalid return type");
  }

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++].second);

  return F;
}

Function *FunctionAST::codegen() {
  Function *TheFunction = TheModule->getFunction(Proto->getName());

  if (!TheFunction)
    TheFunction = Proto->codegen();

  if (!TheFunction)
    return nullptr;

  if (!TheFunction->empty())
    return (Function *)LogErrorV("Function cannot be redefined.");

  BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    AllocaInst *Alloca = nullptr;

    if (Arg.getType() == Type::getInt32Ty(*TheContext))
      Alloca =
          CreateEntryBlockAlloca(TheFunction, typ_int, Arg.getName().str());
    if (Arg.getType() == Type::getInt1Ty(*TheContext))
      Alloca =
          CreateEntryBlockAlloca(TheFunction, typ_bool, Arg.getName().str());

    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
  }

  if (Value *RetVal = Body->codegen()) {
    if (Body->getCXType() != Proto->getRetType()) {
      TheFunction->eraseFromParent();
      switch (Proto->getRetType()) {
      case typ_int:
        return (Function *)LogErrorV("Expected return type \"int\"");
      case typ_bool:
        return (Function *)LogErrorV("Expected return type \"bool\"");
      default:
        return (Function *)LogErrorV("Unreachable!");
      }
    }

    Builder->CreateRet(RetVal);

    verifyFunction(*TheFunction);

    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}

Value *IfExprAST::codegen() {
  Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  if (Cond->getCXType() != typ_bool)
    return LogErrorV("Expected boolean expression in if");

  // CondV = Builder->CreateICmpNE(
  //     CondV, ConstantInt::get(*TheContext, APInt(1, 0)), "ifcond");

  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else");
  BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  Builder->SetInsertPoint(ThenBB);

  Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;

  Builder->CreateBr(MergeBB);

  ThenBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);

  Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;

  if (ThenV->getType() != ElseV->getType())
    return LogErrorV("Expected expressions of the same type in if-else");
  setCXType(Then->getCXType());

  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  PHINode *PN = nullptr;
  switch (getCXType()) {
  case typ_int:
    PN = Builder->CreatePHI(Type::getInt32Ty(*TheContext), 2, "iftmp");
    break;
  case typ_bool:
    PN = Builder->CreatePHI(Type::getInt1Ty(*TheContext), 2, "iftmp");
    break;
  default:
    LogErrorV("Unreachable!");
  }

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

Value *ForExprAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarType, VarName);

  Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;
  if (Start->getCXType() != VarType)
    return LogErrorV("The loop variable was assigned a value of other type");

  Builder->CreateStore(StartVal, Alloca);

  AllocaInst *OldAlloca = NamedValues[VarName];
  NamedValues[VarName] = Alloca;

  BasicBlock *CondBB = BasicBlock::Create(*TheContext, "cond", TheFunction);
  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);

  Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;
  if (End->getCXType() != typ_bool)
    return LogErrorV("Expected boolean expression in for");

  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop");

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);

  if (!Body->codegen())
    return nullptr;

  BasicBlock *StepBB = BasicBlock::Create(*TheContext, "step", TheFunction);
  Builder->CreateBr(StepBB);
  Builder->SetInsertPoint(StepBB);

  if (!Step->codegen())
    return nullptr;

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  if (OldAlloca)
    NamedValues[VarName] = OldAlloca;
  else
    NamedValues.erase(VarName);

  setCXType(typ_int);

  return Constant::getNullValue(Type::getInt32Ty(*TheContext));
}

Value *UnaryExprAST::codegen() {
  Value *V = Operand->codegen();
  if (!V)
    return nullptr;

  switch (Opcode) {
  default:
    return LogErrorV("Invalid unary operator");
  case '!':
    if (Operand->getCXType() != typ_bool)
      return LogErrorV("Expected boolean expression after '!'");
    setCXType(typ_bool);
    return Builder->CreateNot(V);
  }
}
