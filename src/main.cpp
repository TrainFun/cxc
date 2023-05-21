#include "lexer.h"
#include <cstdio>

int main() {
  int tok;
  do {
    tok = gettok();
    printf("%d\n", tok);
  } while (tok != -1);
}
