#include "ir.h"
#include "lexer.h"
#include "parser.h"
#include <cstdio>

int main(int argc, char **argv) {
  if (argc == 2) {
    freopen(argv[1], "r", stdin);
  }

  BinopPrecedence['='] = 2;
  BinopPrecedence[tok_land] = 20;
  BinopPrecedence[tok_lor] = 20;
  BinopPrecedence['<'] = 30;
  BinopPrecedence['>'] = 30;
  BinopPrecedence[tok_eq] = 30;
  BinopPrecedence[tok_ne] = 30;
  BinopPrecedence[tok_le] = 30;
  BinopPrecedence[tok_ge] = 30;
  BinopPrecedence['+'] = 40;
  BinopPrecedence['-'] = 40;
  BinopPrecedence['*'] = 50;
  BinopPrecedence['/'] = 50;
  BinopPrecedence['%'] = 50;

  // fprintf(stderr, "ready> ");
  getNextToken();

  InitializeModule();

  MainLoop();

  TheModule->print(outs(), nullptr);

  fclose(stdin);
}
