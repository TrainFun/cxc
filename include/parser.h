#pragma once

#include "AST.h"
#include <map>
#include <memory>

/**
 * Precedence of binary operators. Every key in this map is recognized as a
 * binary operator.
 */
extern std::map<int, int> BinopPrecedence;

/**
 * Get a new token from input stream and replace current token with it.
 * @return The new token it gets.
 */
int getNextToken();
/**
 * Keep getting new tokens and do parsing procedures accordingly, until EOF.
 * @return 0 if nothing goes wrong; 1 otherwise.
 */
int MainLoop();
/**
 * Print errors occurring when parsing expressions.
 * @return nullptr
 */
std::unique_ptr<ExprAST> LogError(const char *Str);
