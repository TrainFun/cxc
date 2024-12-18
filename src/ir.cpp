#include "ir.h"
#include "AST.h"
#include "lexer.h"
#include "parser.h"
#include <llvm/ADT/APFloat.h>
#include <llvm/ADT/APInt.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <memory>
#include <utility>

std::unique_ptr<LLVMContext> TheContext;
std::unique_ptr<Module> TheModule;
std::unique_ptr<IRBuilder<>> Builder;
std::map<std::string, std::pair<bool, AllocaInst *>> NamedValues;
std::map<std::string, std::unique_ptr<PrototypeAST>> NamedFns;
std::set<std::string> TakenNames;
BasicBlock *ContDest = nullptr;
BasicBlock *BrkDest = nullptr;

void InitializeModule() {
  TheContext = std::make_unique<LLVMContext>();
  TheModule = std::make_unique<Module>("CXC", *TheContext);

  Builder = std::make_unique<IRBuilder<>>(*TheContext);

  Builder->CreateGlobalString("%u\n", "outfmt_int", 0, TheModule.get());
  Builder->CreateGlobalString("%f\n", "outfmt_double", 0, TheModule.get());
  Builder->CreateGlobalString("%u", "infmt_int", 0, TheModule.get());
  Builder->CreateGlobalString("%f", "infmt_double", 0, TheModule.get());

  Function::Create(
      FunctionType::get(Builder->getInt32Ty(), Builder->getInt8PtrTy(), true),
      Function::ExternalLinkage, "printf", *TheModule);
  Function::Create(
      FunctionType::get(Builder->getInt32Ty(), Builder->getInt8PtrTy(), true),
      Function::ExternalLinkage, "scanf", *TheModule);
  Function::Create(
      FunctionType::get(Builder->getInt32Ty(), Builder->getInt32Ty(), false),
      Function::ExternalLinkage, "exit", *TheModule);
}

Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}

Type *llvmTypeFromCXType(const enum CXType T) {
  switch (T) {
  case typ_err:
    return nullptr;
  case typ_int:
    return Type::getInt32Ty(*TheContext);
  case typ_bool:
    return Type::getInt1Ty(*TheContext);
  case typ_double:
    return Type::getDoubleTy(*TheContext);
  }
  return nullptr; // Unreachable.
}

Value *IntExprAST::codegen() {
  setCXType(typ_int);
  return ConstantInt::get(*TheContext, APInt(32, Val));
}

Value *DoubleExprAST::codegen() {
  setCXType(typ_double);
  return ConstantFP::get(*TheContext, APFloat(Val));
}

Value *BooleanExprAST::codegen() {
  setCXType(typ_bool);
  return ConstantInt::get(*TheContext, APInt(1, Val));
}

Value *VariableExprAST::codegen() {
  AllocaInst *A = NamedValues[Name].second;
  if (!A) {
    auto *G = TheModule->getNamedGlobal(Name);
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
    VariableExprAST *LHSE = dynamic_cast<VariableExprAST *>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");

    AllocaInst *Variable = NamedValues[LHSE->getName()].second;
    if (!Variable) {
      auto *G = TheModule->getNamedGlobal(LHSE->getName());
      if (!G)
        return LogErrorV("Unknown variable name");

      if (G->isConstant())
        return LogErrorV("Can't assign to const variables");

      Value *Val = RHS->codegen();
      if (!Val)
        return nullptr;

      if (G->getValueType() == Type::getInt32Ty(*TheContext))
        setCXType(typ_int);
      if (G->getValueType() == Type::getInt1Ty(*TheContext))
        setCXType(typ_bool);
      if (G->getValueType() == Type::getDoubleTy(*TheContext))
        setCXType(typ_double);

      Builder->CreateStore(Val, G);
      return Val;
    }

    if (NamedValues[LHSE->getName()].first)
      return LogErrorV("Can't assign to const variables");

    Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;

    if (Variable->getAllocatedType() == Type::getInt32Ty(*TheContext))
      setCXType(typ_int);
    if (Variable->getAllocatedType() == Type::getInt1Ty(*TheContext))
      setCXType(typ_bool);
    if (Variable->getAllocatedType() == Type::getDoubleTy(*TheContext))
      setCXType(typ_double);

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
      return Builder->CreateUDiv(L, R, "divtmp");
    case typ_bool:
      return LogErrorV("operator '/' not defined for bool");
    case typ_double:
      return Builder->CreateFDiv(L, R, "divtmp");
    }
  case '%':
    setCXType(LHS->getCXType());
    switch (LHS->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_int:
      return Builder->CreateURem(L, R, "modtmp");
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
  auto Var = TheModule->getNamedGlobal(Name);
  if (Var)
    return (Function *)LogErrorV("Redeclaration of identifier");

  Function *TheFunction = TheModule->getFunction(Name);

  if (TheFunction)
    return (Function *)LogErrorV("Redeclaration of identifier");

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
  auto Var = TheModule->getNamedGlobal(Proto->getName());
  if (Var)
    return (Function *)LogErrorV("Redefinition of identifier");

  Function *TheFunction = TheModule->getFunction(Proto->getName());

  if (!TheFunction) {
    TheFunction = Proto->codegen();
  } else if (!(*Proto == *NamedFns[Proto->getName()])) {
    return (Function *)LogErrorV("Function has conflicting signatures");
  }

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

    NamedFns[Proto->getName()] = std::move(Proto);

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

  bool OldIsConst;
  AllocaInst *OldAlloca = nullptr;

  if (VarType != typ_err) {
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarType, VarName);

    Value *StartVal = nullptr;

    if (Start) {
      StartVal = Start->codegen();
      if (!StartVal)
        return nullptr;
      if (Start->getCXType() != VarType)
        return LogErrorV(
            "The loop variable was assigned a value of other type");

      Builder->CreateStore(StartVal, Alloca);

      OldIsConst = NamedValues[VarName].first;
      OldAlloca = NamedValues[VarName].second;
      NamedValues[VarName].first = false;
      NamedValues[VarName].second = Alloca;
    }
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
  BasicBlock *StepBB = BasicBlock::Create(*TheContext, "step");
  BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "afterloop");

  auto OldContDest = ContDest;
  auto OldBrkDest = BrkDest;
  ContDest = StepBB;
  BrkDest = AfterBB;

  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);

  if (!Body->codegen())
    return nullptr;

  Builder->CreateBr(StepBB);

  TheFunction->insert(TheFunction->end(), StepBB);
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

  ContDest = OldContDest;
  BrkDest = OldBrkDest;

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
    return Builder->CreateNot(V, "nottmp");
  }

  case tok_ODD: {
    if (Operand->getCXType() != typ_int)
      return LogErrorV("Expected int expression after 'ODD'");
    setCXType(typ_bool);
    auto AndTmp = Builder->CreateAnd(
        V,
        Constant::getIntegerValue(Type::getInt32Ty(*TheContext), APInt(32, 1)),
        "andtmp");
    return Builder->CreateICmpEQ(
        AndTmp,
        Constant::getIntegerValue(Type::getInt32Ty(*TheContext), APInt(32, 1)),
        "andtmp");
  }

  case tok_increment: {
    auto OpVar = std::unique_ptr<VariableExprAST>(
        dynamic_cast<VariableExprAST *>(Operand.release()));
    if (!OpVar)
      return LogErrorV("Operand of '++' must be a variable");

    Value *Dest = NamedValues[OpVar->getName()].second;
    if (!Dest) {
      auto G = TheModule->getNamedGlobal(OpVar->getName());
      if (!G)
        return LogErrorV("Unknown variable");
      Dest = G;
    }

    if (NamedValues[OpVar->getName()].first)
      return LogErrorV("Const variables cannot perform self increment");

    switch (OpVar->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_bool:
      return LogErrorV("operator ++ is not defined for bool");
    case typ_int: {
      setCXType(typ_int);
      auto AddTmp =
          Builder->CreateAdd(V,
                             Constant::getIntegerValue(
                                 Type::getInt32Ty(*TheContext), APInt(32, 1)),
                             "addtmp");
      Builder->CreateStore(AddTmp, Dest);
      return AddTmp;
    }

    case typ_double: {
      setCXType(typ_double);
      auto AddTmp = Builder->CreateFAdd(
          V, ConstantFP::get(*TheContext, APFloat(1.0)), "addtmp");
      Builder->CreateStore(AddTmp, Dest);
      return AddTmp;
    }
    }
  }

  case tok_decrement: {
    auto OpVar = std::unique_ptr<VariableExprAST>(
        dynamic_cast<VariableExprAST *>(Operand.release()));
    if (!OpVar)
      return LogErrorV("Operand of '--' must be a variable");

    Value *Dest = NamedValues[OpVar->getName()].second;
    if (!Dest) {
      auto G = TheModule->getNamedGlobal(OpVar->getName());
      if (!G)
        return LogErrorV("Unknown variable");
      Dest = G;
    }

    if (NamedValues[OpVar->getName()].first)
      return LogErrorV("Const variables cannot perform self increment");

    switch (OpVar->getCXType()) {
    default:
      return LogErrorV("Unreachable!");
    case typ_bool:
      return LogErrorV("operator -- is not defined for bool");
    case typ_int: {
      setCXType(typ_int);
      auto SubTmp =
          Builder->CreateSub(V,
                             Constant::getIntegerValue(
                                 Type::getInt32Ty(*TheContext), APInt(32, 1)),
                             "subtmp");
      Builder->CreateStore(SubTmp, Dest);
      return SubTmp;
    }
    case typ_double: {
      setCXType(typ_double);
      auto SubTmp = Builder->CreateFSub(
          V, ConstantFP::get(*TheContext, APFloat(1.0)), "subtmp");
      Builder->CreateStore(SubTmp, Dest);
      return SubTmp;
    }
    }
  }
  }
}

Value *ExprStmtAST::codegen() {
  if (Expr && !Expr->codegen())
    return nullptr;
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *BlockStmtAST::codegen() {
  // Backup.
  auto OldNamedValues = NamedValues;
  auto OldTakenNames = TakenNames;
  TakenNames.clear();

  for (auto &Elem : Elems) {
    auto VarDecl = dynamic_cast<VarDeclAST *>(Elem.get());
    if (VarDecl && !VarDecl->codegen())
      return nullptr;

    auto Stmt = dynamic_cast<StmtAST *>(Elem.get());
    if (Stmt && !Stmt->codegen())
      return nullptr;
  }

  // Recover.
  TakenNames = OldTakenNames;
  NamedValues = OldNamedValues;
  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Function *VarDeclAST::codegen() {
  if (TakenNames.find(Name) != TakenNames.end())
    return (Function *)LogErrorV("The name has been taken in the same scope");

  auto TheFunction = Builder->GetInsertBlock()->getParent();
  auto Alloca = CreateEntryBlockAlloca(TheFunction, Type, Name);

  if (Val) {
    auto V = Val->codegen();
    if (!V)
      return nullptr;
    if (Val->getCXType() != Type)
      return (Function *)LogErrorV("Incompatible types.");
    Builder->CreateStore(V, Alloca);
  }

  TakenNames.insert(Name);
  NamedValues[Name] = std::make_pair(isConst, Alloca);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Function *GlobVarDeclAST::codegen() {
  auto Var = TheModule->getNamedGlobal(Name);
  if (Var)
    return (Function *)LogErrorV("Redefinition of identifier");

  auto Fn = TheModule->getFunction(Name);
  if (Fn)
    return (Function *)LogErrorV("Redefinition of identifier");

  Value *V = nullptr;
  if (Val) {
    switch (Type) {
    case typ_err:
      return (Function *)LogErrorV("Unreachable!");
    case typ_int: {
      if (!dynamic_cast<IntExprAST *>(Val.get()))
        return (Function *)LogErrorV(
            "Expected initial value to be int constant");
      break;
    }
    case typ_bool: {
      if (!dynamic_cast<BooleanExprAST *>(Val.get()))
        return (Function *)LogErrorV(
            "Expected initial value to be bool constant");
      break;
    }
    case typ_double: {
      if (!dynamic_cast<DoubleExprAST *>(Val.get()))
        return (Function *)LogErrorV(
            "Expected initial value to be double constant");
      break;
    }
    }
    V = Val->codegen();
    if (!V)
      return (Function *)LogErrorV("Expected initial value");
  }

  Var = new GlobalVariable(*TheModule, llvmTypeFromCXType(Type), isConst,
                           GlobalValue::ExternalLinkage,
                           Constant::getNullValue(llvmTypeFromCXType(Type)),
                           Name);
  if (V)
    Var->setInitializer((Constant *)V);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *SwitchStmtAST::codegen() {
  Function *TheFunction = Builder->GetInsertBlock()->getParent();

  auto *V = Expr->codegen();
  if (!V)
    return nullptr;

  auto ExprType = Expr->getCXType();
  if (ExprType == typ_double)
    return LogErrorV("Expected integer types in switch");

  auto CondBB = BasicBlock::Create(*TheContext, "cond", TheFunction);
  auto HandleBB = BasicBlock::Create(*TheContext, "handle");
  auto AfterBB = BasicBlock::Create(*TheContext, "afterswitch", TheFunction);
  Builder->CreateBr(CondBB);

  auto OldBrkDest = BrkDest;
  BrkDest = AfterBB;

  BasicBlock *DefaultBB = nullptr;
  for (auto &Item : BasicBlocks) {
    auto &CondList = Item.first;
    auto &Handle = Item.second;

    for (auto &Cond : CondList) {
      if (!Cond) { // the "default" case
        DefaultBB = HandleBB;
        continue;
      }

      TheFunction->insert(TheFunction->end(), CondBB);
      Builder->SetInsertPoint(CondBB);

      auto CondV = Cond->codegen();
      if (!CondV)
        return nullptr;

      if (Cond->getCXType() != ExprType)
        return LogErrorV("Expected same type in switch-case");

      switch (ExprType) {
      case typ_err:
      case typ_double:
        return LogErrorV("Unreachable!");
      case typ_int:
      case typ_bool:
        CondV = Builder->CreateICmpEQ(V, CondV, "cmptmp");
      }

      CondBB = BasicBlock::Create(*TheContext, "cond");
      Builder->CreateCondBr(CondV, HandleBB, CondBB);
    }

    TheFunction->insert(TheFunction->end(), HandleBB);
    Builder->SetInsertPoint(HandleBB);
    for (auto &Stmt : Handle) {
      auto *S = Stmt->codegen();
      if (!S)
        return nullptr;
    }
    HandleBB = BasicBlock::Create(*TheContext, "handle");
    Builder->CreateBr(HandleBB);
  }

  TheFunction->insert(TheFunction->end(), CondBB);
  Builder->SetInsertPoint(CondBB);

  if (DefaultBB)
    Builder->CreateBr(DefaultBB);
  else
    Builder->CreateBr(AfterBB);

  TheFunction->insert(TheFunction->end(), HandleBB);
  Builder->SetInsertPoint(HandleBB);
  Builder->CreateBr(AfterBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  BrkDest = OldBrkDest;

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *WhileStmtAST::codegen() {
  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto CondBB = BasicBlock::Create(*TheContext, "cond", TheFunction);
  Builder->CreateBr(CondBB);
  Builder->SetInsertPoint(CondBB);

  auto CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  auto LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  auto AfterBB = BasicBlock::Create(*TheContext, "afterloop");
  Builder->CreateCondBr(CondV, LoopBB, AfterBB);
  Builder->SetInsertPoint(LoopBB);

  auto OldContDest = ContDest;
  auto OldBrkDest = BrkDest;
  ContDest = CondBB;
  BrkDest = AfterBB;

  if (!Body->codegen())
    return nullptr;

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), AfterBB);
  Builder->SetInsertPoint(AfterBB);

  ContDest = OldContDest;
  BrkDest = OldBrkDest;

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *DoStmtAST::codegen() {
  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  auto CondBB = BasicBlock::Create(*TheContext, "cond");
  auto AfterBB = BasicBlock::Create(*TheContext, "afterloop");

  auto OldContDest = ContDest;
  auto OldBrkDest = BrkDest;
  ContDest = CondBB;
  BrkDest = AfterBB;

  if (!Body->codegen())
    return nullptr;

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), CondBB);
  Builder->SetInsertPoint(CondBB);

  auto CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  Builder->CreateCondBr(CondV, LoopBB, AfterBB);

  TheFunction->insert(TheFunction->end(), CondBB);
  Builder->SetInsertPoint(AfterBB);

  ContDest = OldContDest;
  BrkDest = OldBrkDest;

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *UntilStmtAST::codegen() {
  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto LoopBB = BasicBlock::Create(*TheContext, "loop", TheFunction);
  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  auto CondBB = BasicBlock::Create(*TheContext, "cond");
  auto AfterBB = BasicBlock::Create(*TheContext, "afterloop");

  auto OldContDest = ContDest;
  auto OldBrkDest = BrkDest;
  ContDest = CondBB;
  BrkDest = AfterBB;

  if (!Body->codegen())
    return nullptr;

  Builder->CreateBr(CondBB);

  TheFunction->insert(TheFunction->end(), CondBB);
  Builder->SetInsertPoint(CondBB);

  auto CondV = Cond->codegen();
  if (!CondV)
    return nullptr;
  CondV = Builder->CreateNot(CondV, "nottmp");

  Builder->CreateCondBr(CondV, LoopBB, AfterBB);

  TheFunction->insert(TheFunction->end(), CondBB);
  Builder->SetInsertPoint(AfterBB);

  ContDest = OldContDest;
  BrkDest = OldBrkDest;

  return Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *ReadStmtAST::codegen() {
  auto Var = dynamic_cast<VariableExprAST *>(this->Var.get());
  if (!Var)
    return LogErrorV("Can only read to a variable");

  AllocaInst *A = NamedValues[Var->getName()].second;
  if (!A) {
    auto *G = TheModule->getNamedGlobal(Var->getName());
    if (!G)
      return LogErrorV("Unknown variable name");

    if (G->isConstant())
      return LogErrorV("Can't assign to const variables");

    auto CalleeF = TheModule->getFunction("scanf");
    Value *fmt = nullptr;

    if (G->getValueType() == Type::getInt32Ty(*TheContext)) {
      fmt = TheModule->getNamedGlobal("infmt_int");
    }
    if (G->getValueType() == Type::getInt1Ty(*TheContext)) {
      fmt = TheModule->getNamedGlobal("infmt_int");
    }
    if (G->getValueType() == Type::getDoubleTy(*TheContext)) {
      fmt = TheModule->getNamedGlobal("infmt_double");
    }

    Value *Args[] = {fmt, G};
    Builder->CreateCall(CalleeF, Args, "calltmp");
    return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
  }

  if (NamedValues[Var->getName()].first)
    LogErrorV("Cannot read to a const variable");

  auto CalleeF = TheModule->getFunction("scanf");
  Value *fmt = nullptr;

  if (A->getAllocatedType() == Type::getInt32Ty(*TheContext)) {
    fmt = TheModule->getNamedGlobal("infmt_int");
  }
  if (A->getAllocatedType() == Type::getInt1Ty(*TheContext)) {
    fmt = TheModule->getNamedGlobal("infmt_int");
  }
  if (A->getAllocatedType() == Type::getDoubleTy(*TheContext)) {
    fmt = TheModule->getNamedGlobal("infmt_double");
  }

  Value *Args[] = {fmt, A};
  Builder->CreateCall(CalleeF, Args, "calltmp");
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *WriteStmtAST::codegen() {
  auto *V = Val->codegen();
  if (!V)
    return nullptr;

  auto CalleeF = TheModule->getFunction("printf");
  Value *fmt = nullptr;

  switch (Val->getCXType()) {
  case typ_err:
    return LogErrorV("Unreachable!");
  case typ_int:
  case typ_bool:
    fmt = TheModule->getNamedGlobal("outfmt_int");
    break;
  case typ_double:
    fmt = TheModule->getNamedGlobal("outfmt_double");
    break;
  }

  Value *Args[] = {fmt, V};
  Builder->CreateCall(CalleeF, Args, "calltmp");
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *ContStmtAST::codegen() {
  if (!ContDest)
    return LogErrorV("Cannot use 'continue' here");
  Builder->CreateBr(ContDest);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *BrkStmtAST::codegen() {
  if (!BrkDest)
    return LogErrorV("Cannot use 'break' here");
  Builder->CreateBr(BrkDest);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *RetStmtAST::codegen() {
  auto TheFunction = Builder->GetInsertBlock()->getParent();

  auto V = Val->codegen();
  if (!V)
    return nullptr;

  if (TheFunction->getReturnType() != V->getType())
    return LogErrorV("Incompatable return type");

  Builder->CreateRet(V);
  return (Function *)Constant::getNullValue(Type::getVoidTy(*TheContext));
}

Value *CastExprAST::codegen() {
  auto FromV = From->codegen();
  if (!FromV)
    return nullptr;

  Value *V = nullptr;

  switch (Type) {
  case typ_err:
    return LogErrorV("Unreachable!");

  case typ_int: {
    switch (From->getCXType()) {
    case typ_err:
      return LogErrorV("Unreachable!");
    case typ_int: {
      V = FromV;
      break;
    }
    case typ_bool: {
      V = Builder->CreateIntCast(FromV, Type::getInt32Ty(*TheContext), false,
                                 "casttmp");
      break;
    }
    case typ_double: {
    }
      V = Builder->CreateFPToUI(FromV, Type::getInt32Ty(*TheContext),
                                "casttmp");
      break;
    }
    break;
  }

  case typ_bool: {
    switch (From->getCXType()) {
    case typ_err:
      return LogErrorV("Unreachable!");
    case typ_int: {
      V = Builder->CreateIsNotNull(FromV, "notnulltmp");
      break;
    }
    case typ_double: {
      V = Builder->CreateFCmpONE(
          FromV, ConstantFP::getNullValue(Type::getDoubleTy(*TheContext)),
          "cmptmp");
      break;
    }
    case typ_bool: {
      V = FromV;
      break;
    }
    }
    break;
  }

  case typ_double: {
    switch (From->getCXType()) {
    case typ_err:
      return LogErrorV("Unreachable!");
    case typ_bool:
    case typ_int: {

      V = Builder->CreateUIToFP(FromV, Type::getDoubleTy(*TheContext),
                                "casttmp");
      break;
    }
    case typ_double: {
      V = FromV;
      break;
    }
    }
  } break;
  }

  if (V)
    setCXType(Type);
  return V;
}

Value *ExitStmtAST::codegen() {
  auto V = ExitCode->codegen();
  if (!V)
    return nullptr;

  auto CalleeF = TheModule->getFunction("exit");
  Value *Args[] = {V};
  Builder->CreateCall(CalleeF, Args, "calltmp");

  return Constant::getNullValue(Builder->getVoidTy());
}
