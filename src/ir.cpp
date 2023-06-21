#include "ir.h"
#include "AST.h"
#include "lexer.h"
#include "parser.h"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <utility>

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::map<std::string, std::pair<bool, AllocaInst *>> NamedValues;

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Value *IntExprAST::codegen() {
  setCXType(typ_int);
  return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *DoubleExprAST::codegen() {
  setCXType(typ_int);
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *BooleanExprAST::codegen() {
  setCXType(typ_bool);
  return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
  AllocaInst *A = NamedValues[Name].second;
  if (!A) {
    auto *G = TheModule->getGlobalVariable(Name);
    if (!G)
      return LogErrorV("Unknown variable name");

    if (G->getValueType() == Type::getInt32Ty(*TheContext))
      setCXType(typ_int);
    if (G->getValueType() == Type::getInt1Ty(*TheContext))
      setCXType(typ_bool);
    if (G->getValueType() == Type::getDoubleTy(*TheContext))
      setCXType(typ_double);

    return Builder->CreateLoad(G->getValueType(), G, Name.c_str());
  }

  if (A->getAllocatedType() == Type::getInt32Ty(*TheContext))
    setCXType(typ_int);
  if (A->getAllocatedType() == Type::getInt1Ty(*TheContext))
    setCXType(typ_bool);
  if (A->getAllocatedType() == Type::getDoubleTy(*TheContext))
    setCXType(typ_double);

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

    AllocaInst *Variable = NamedValues[LHSE->getName()].second;
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
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateAdd(L, R, "addtmp");
    case typ_bool:
      return LogErrorV("operator '+' not defined for bool");
    case typ_double:
      return Builder->CreateFAdd(L, R, "addtmp");
    }
  case '-':
    setCXType(LHS->getCXType());
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateSub(L, R, "subtmp");
    case typ_bool:
      return LogErrorV("operator '-' not defined for bool");
    case typ_double:
      return Builder->CreateFSub(L, R, "subtmp");
    }
  case '*':
    setCXType(LHS->getCXType());
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateMul(L, R, "multmp");
    case typ_bool:
      return LogErrorV("operator '*' not defined for bool");
    case typ_double:
      return Builder->CreateFMul(L, R, "multmp");
    }
  case '/':
    setCXType(LHS->getCXType());
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateUDiv(L, R, "multmp");
    case typ_bool:
      return LogErrorV("operator '/' not defined for bool");
    case typ_double:
      return Builder->CreateFDiv(L, R, "multmp");
    }
  case '%':
    setCXType(LHS->getCXType());
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateURem(L, R, "multmp");
    case typ_bool:
      return LogErrorV("operator '%' not defined for bool");
    case typ_double:
      return LogErrorV("operator '%' not defined for double");
    }
  case '<':
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpULT(L, R, "cmptmp");
    case typ_bool:
      return LogErrorV("operator '<' not defined for bool");
    case typ_double:
      return Builder->CreateFCmpOLT(L, R, "cmptmp");
    }
  case '>':
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpUGT(L, R, "cmptmp");
    case typ_bool:
      return LogErrorV("operator '>' not defined for bool");
    case typ_double:
      return Builder->CreateFCmpOGT(L, R, "cmptmp");
    }
  case tok_eq:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpEQ(L, R, "cmptmp");
    case typ_bool:
      return Builder->CreateICmpEQ(L, R, "cmptmp");
    case typ_double:
      return Builder->CreateFCmpOEQ(L, R, "cmptmp");
    }
  case tok_ne:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpNE(L, R, "cmptmp");
    case typ_bool:
      return Builder->CreateICmpNE(L, R, "cmptmp");
    case typ_double:
      return Builder->CreateFCmpONE(L, R, "cmptmp");
    }
  case tok_le:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpULE(L, R, "cmptmp");
    case typ_bool:
      return LogErrorV("operator '<=' not defined for bool");
    case typ_double:
      return Builder->CreateFCmpOLE(L, R, "cmptmp");
    }
  case tok_ge:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateICmpUGE(L, R, "cmptmp");
    case typ_bool:
      return LogErrorV("operator '>=' not defined for bool");
    case typ_double:
      return Builder->CreateFCmpOGE(L, R, "cmptmp");
    }
  case tok_lor:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return LogErrorV("operator '||' not defined for int");
    case typ_bool:
      return Builder->CreateLogicalOr(L, R, "lortmp");
    case typ_double:
      return LogErrorV("operator '||' not defined for double");
    }
  case tok_land:
    setCXType(typ_bool);
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return LogErrorV("operator '&&' not defined for int");
    case typ_bool:
      return Builder->CreateLogicalAnd(L, R, "landtmp");
    case typ_double:
      return LogErrorV("operator '&&' not defined for double");
    }
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
  if (CalleeF->getReturnType() == Type::getDoubleTy(*TheContext))
    setCXType(typ_double);

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
  case typ_double:
    return TmpB.CreateAlloca(Type::getDoubleTy(*TheContext), nullptr, VarName);
  default:
    LogError("Unreachable!");
    return nullptr;
  }
}

Function *PrototypeAST::codegen() {
  std::vector<Type *> ArgsT;
  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    switch (Args[i]->getType()) {
    case typ_int:
      ArgsT.push_back(Type::getInt32Ty(*TheContext));
      break;
    case typ_bool:
      ArgsT.push_back(Type::getInt1Ty(*TheContext));
      break;
    case typ_double:
      ArgsT.push_back(Type::getDoubleTy(*TheContext));
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
  case typ_double:
    FT = FunctionType::get(Type::getDoubleTy(*TheContext), ArgsT, false);
    break;
  default:
    return (Function *)LogErrorV("invalid return type");
  }

  Function *F =
      Function::Create(FT, Function::ExternalLinkage, Name, TheModule.get());

  unsigned Idx = 0;
  for (auto &Arg : F->args())
    Arg.setName(Args[Idx++]->getName());

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
  unsigned Idx = 0;
  for (auto &Arg : TheFunction->args()) {
    AllocaInst *Alloca = nullptr;

    if (Arg.getType() == Type::getInt32Ty(*TheContext))
      Alloca =
          CreateEntryBlockAlloca(TheFunction, typ_int, Arg.getName().str());
    if (Arg.getType() == Type::getInt1Ty(*TheContext))
      Alloca =
          CreateEntryBlockAlloca(TheFunction, typ_bool, Arg.getName().str());
    if (Arg.getType() == Type::getDoubleTy(*TheContext))
      Alloca =
          CreateEntryBlockAlloca(TheFunction, typ_double, Arg.getName().str());

    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] =
        std::make_pair(Proto->getArgs()[Idx]->isConstVar(), Alloca);
  }

  if (Body->codegen()) {
    // if (Body->getCXType() != Proto->getRetType()) {
    //   TheFunction->eraseFromParent();
    //   switch (Proto->getRetType()) {
    //   case typ_int:
    //     return (Function *)LogErrorV("Expected return type \"int\"");
    //   case typ_bool:
    //     return (Function *)LogErrorV("Expected return type \"bool\"");
    //   default:
    //     return (Function *)LogErrorV("Unreachable!");
    //   }
    // }

    switch (Proto->getRetType()) {
    case typ_int:
      Builder->CreateRet(Constant::getNullValue(Type::getInt32Ty(*TheContext)));
      break;
    case typ_bool:
      Builder->CreateRet(Constant::getNullValue(Type::getInt1Ty(*TheContext)));
      break;
    case typ_double:
      Builder->CreateRet(
          Constant::getNullValue(Type::getDoubleTy(*TheContext)));
      break;
    default:
      return (Function *)LogErrorV("Unreachable!");
    }

    verifyFunction(*TheFunction);

    return TheFunction;
  }

  TheFunction->eraseFromParent();
  return nullptr;
}

Value *IfStmtAST::codegen() {
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

  if (Else) {
    Value *ElseV = Else->codegen();
    if (!ElseV)
      return nullptr;
  }

  // if (ThenV->getType() != ElseV->getType())
  //   return LogErrorV("Expected expressions of the same type in if-else");
  // setCXType(Then->getCXType());

  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);
  // PHINode *PN = nullptr;
  // switch (getCXType()) {
  // case typ_int:
  //   PN = Builder->CreatePHI(Type::getInt32Ty(*TheContext), 2, "iftmp");
  //   break;
  // case typ_bool:
  //   PN = Builder->CreatePHI(Type::getInt1Ty(*TheContext), 2, "iftmp");
  //   break;
  // default:
  //   LogErrorV("Unreachable!");
  // }
  //
  // PN->addIncoming(ThenV, ThenBB);
  // PN->addIncoming(ElseV, ElseBB);
  // return PN;

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *ForStmtAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarType, VarName);

  Value *StartVal = nullptr;
  bool OldIsConst;
  AllocaInst *OldAlloca;

  if (Start) {
    StartVal = Start->codegen();
    if (!StartVal)
      return nullptr;
    if (Start->getCXType() != VarType)
      return LogErrorV("The loop variable was assigned a value of other type");

    Builder->CreateStore(StartVal, Alloca);

    OldIsConst = NamedValues[VarName].first;
    OldAlloca = NamedValues[VarName].second;
    NamedValues[VarName].first = false;
    NamedValues[VarName].second = Alloca;
  }

  BasicBlock *CondBB = BasicBlock::Create(*TheContext, "cond", TheFunction);
  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);

  Value *EndCond = nullptr;
  if (End) {
    EndCond = End->codegen();
    if (!EndCond)
      return nullptr;
    if (End->getCXType() != typ_bool)
      return LogErrorV("Expected boolean expression in for");
  } else
    EndCond =
        Constant::getIntegerValue(Type::getInt1Ty(*TheContext), APInt(1, 1));

  BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop");

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);

  if (!Body->codegen())
    return nullptr;

  BasicBlock *StepBB = BasicBlock::Create(*TheContext, "step", TheFunction);
  Builder->CreateBr(StepBB);
  Builder->SetInsertPoint(StepBB);

  if (Step) {
    if (!Step->codegen())
      return nullptr;
  }

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  if (OldAlloca)
    NamedValues[VarName] = std::make_pair(OldIsConst, OldAlloca);
  else
    NamedValues.erase(VarName);

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *UnaryExprAST::codegen() {
  Value *V = Operand->codegen();
  if (!V)
    return nullptr;

  switch (Opcode) {

  default:
    return LogErrorV("Invalid unary operator");

  case '!': {
    if (Operand->getCXType() != typ_bool)
      return LogErrorV("Expected boolean expression after '!'");
    setCXType(typ_bool);
    return Builder->CreateNot(V);
  }

  case tok_ODD: {
    if (Operand->getCXType() != typ_int)
      return LogErrorV("Expected int expression after 'ODD'");
    setCXType(typ_bool);
    auto AndTmp = Builder->CreateAnd(
        V,
        Constant::getIntegerValue(Type::getInt32Ty(*TheContext), APInt(32, 1)));
    return Builder->CreateICmpEQ(
        AndTmp,
        Constant::getIntegerValue(Type::getInt32Ty(*TheContext), APInt(32, 1)));
  }

  case tok_increment: {
    switch (Operand->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_bool:
      return LogErrorV("operator ++ is not defined for bool");
    case typ_int:
      setCXType(typ_int);
      return Builder->CreateAdd(
          V, Constant::getIntegerValue(Type::getInt32Ty(*TheContext),
                                       APInt(32, 1)));
    case typ_double:
      setCXType(typ_double);
      return Builder->CreateFAdd(V, ConstantFP::get(*TheContext, APFloat(1.0)));
    }
  }

  case tok_decrement: {
    switch (Operand->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_bool:
      return LogErrorV("operator -- is not defined for bool");
    case typ_int:
      setCXType(typ_int);
      return Builder->CreateSub(
          V, Constant::getIntegerValue(Type::getInt32Ty(*TheContext),
                                       APInt(32, 1)));
    case typ_double:
      setCXType(typ_double);
      return Builder->CreateFSub(V, ConstantFP::get(*TheContext, APFloat(1.0)));
    }
  }
  }
}

Value *ExprStmtAST::codegen() {
  if (!Expr->codegen())
    return nullptr;
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *BlockStmtAST::codegen() {
  // Backup.
  auto OldNamedValues = NamedValues;

  for (auto &Elem : Elems) {
    auto VarDecl = dynamic_cast<VarDeclAST *>(Elem.get());
    if (VarDecl && !VarDecl->codegen())
      return nullptr;

    auto Stmt = dynamic_cast<StmtAST *>(Elem.get());
    if (Stmt && !Stmt->codegen())
      return nullptr;
  }

  // Recover.
  NamedValues = OldNamedValues;
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Function *VarDeclAST::codegen() {
  auto TheFunction = Builder->GetInsertBlock()->getParent();
  auto Alloca = CreateEntryBlockAlloca(TheFunction, Type, Name);

  if (Val) {
    auto V = Val->codegen();
    if (!V)
      return nullptr;
    Builder->CreateStore(V, Alloca);
  }

  NamedValues[Name] = std::make_pair(isConst, Alloca);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Function *GlobVarDeclAST::codegen() {}

Value *SwitchStmtAST::codegen() {}

Value *WhileStmtAST::codegen() {}
Value *DoStmtAST::codegen() {}
Value *UntilStmtAST::codegen() {}
Value *ReadStmtAST::codegen() {}
Value *WriteStmtAST::codegen() {}
Value *ContStmtAST::codegen() {}
Value *BrkStmtAST::codegen() {}
Value *RetStmtAST::codegen() {}
