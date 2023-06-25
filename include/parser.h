#pragma once

#include "AST.h"
#include <map>
#include <memory>

extern std::map<int, int> BinopPrecedence;

int getNextToken();
int MainLoop();
std::unique_ptr<ExprAST> LogError(const char *Str);
void InitializeModule();
