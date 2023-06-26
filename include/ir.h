#pragma once

#include "AST.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <map>

using namespace llvm;

/**
 * @brief The LLVM Context.
 */
extern std::unique_ptr<LLVMContext> TheContext;
/**
 * @brief The LLVM Module.
 */
extern std::unique_ptr<Module> TheModule;
/**
 * @brief An LLVM IR Builder.
 */
extern std::unique_ptr<IRBuilder<>> Builder;
/**
 * Local variable names, if they are constant and their addresses.
 */
extern std::map<std::string, std::pair<bool, AllocaInst *>> NamedValues;
/**
 * Function names and their prototypes.
 */
extern std::map<std::string, std::unique_ptr<PrototypeAST>> NamedFns;

/**
 * Initialize the LLVM Module, including creating essential instances and
 * declaring `scanf`, `printf` and `exit` functions, as well as creating
 * variables that will be used as parameters in those functions.
 */
void InitializeModule();
