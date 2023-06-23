#pragma once

#include "AST.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <map>

using namespace llvm;

extern std::unique_ptr<LLVMContext> TheContext;
extern std::unique_ptr<Module> TheModule;
extern std::unique_ptr<IRBuilder<>> Builder;
extern std::map<std::string, std::pair<bool, AllocaInst *>> NamedValues;
extern std::map<std::string, std::unique_ptr<PrototypeAST>> NamedFns;
