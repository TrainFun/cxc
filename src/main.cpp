#include "lexer.h"
#include "parser.h"
#include <cstdio>

int main() {
  BinopPrecedence[tok_land] = 10;
  BinopPrecedence[tok_lor] = 10;
  BinopPrecedence['<'] = 20;
  BinopPrecedence['>'] = 20;
  BinopPrecedence[tok_eq] = 20;
  BinopPrecedence[tok_ne] = 20;
  BinopPrecedence[tok_le] = 20;
  BinopPrecedence[tok_ge] = 20;
  BinopPrecedence['+'] = 30;
  BinopPrecedence['-'] = 30;
  BinopPrecedence['*'] = 40;

  fprintf(stderr, "ready> ");
  getNextToken();

  MainLoop();
}
