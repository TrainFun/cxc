#include "lexer.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

std::string IdentifierStr;
double NumVal;
unsigned NR = 1;

/// gettok - Return the next token from standard input.
int gettok() {
  static int LastChar = ' ';

  // Skip whitespaces.
  while (isspace(LastChar)) {
    if (LastChar == '\n')
      ++NR;
    LastChar = getchar();
  }

  // Identifiers.
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    // if (IdentifierStr == "def")
    //   return tok_def;
    if (IdentifierStr == "true")
      return tok_true;
    if (IdentifierStr == "false")
      return tok_false;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "switch")
      return tok_switch;
    if (IdentifierStr == "default")
      return tok_default;
    if (IdentifierStr == "case")
      return tok_case;
    if (IdentifierStr == "while")
      return tok_while;
    if (IdentifierStr == "do")
      return tok_do;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "repeat")
      return tok_repeat;
    if (IdentifierStr == "until")
      return tok_until;
    if (IdentifierStr == "write")
      return tok_write;
    if (IdentifierStr == "read")
      return tok_read;
    if (IdentifierStr == "continue")
      return tok_continue;
    if (IdentifierStr == "break")
      return tok_break;
    if (IdentifierStr == "return")
      return tok_return;
    if (IdentifierStr == "exit")
      return tok_exit;
    if (IdentifierStr == "int")
      return tok_int;
    if (IdentifierStr == "bool")
      return tok_bool;
    if (IdentifierStr == "double")
      return tok_double;
    if (IdentifierStr == "const")
      return tok_const;
    if (IdentifierStr == "ODD")
      return tok_ODD;
    return tok_identifier;
  }

  // Numbers.
  if (isdigit(LastChar) || LastChar == '.') {
    bool isDouble = false;
    std::string NumStr;
    do {
      if (LastChar == '.') {
        if (isDouble)
          return tok_doubleliteral;
        isDouble = true;
      }
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), 0);

    if (isDouble)
      return tok_doubleliteral;
    return tok_intliteral;
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();

  // Multi-char operators
  if (ThisChar == '<' && LastChar == '=') {
    LastChar = getchar();
    return tok_le;
  }
  if (ThisChar == '>' && LastChar == '=') {
    LastChar = getchar();
    return tok_ge;
  }
  if (ThisChar == '=' && LastChar == '=') {
    LastChar = getchar();
    return tok_eq;
  }
  if (ThisChar == '!' && LastChar == '=') {
    LastChar = getchar();
    return tok_ne;
  }
  if (ThisChar == '|' && LastChar == '|') {
    LastChar = getchar();
    return tok_lor;
  }
  if (ThisChar == '&' && LastChar == '&') {
    LastChar = getchar();
    return tok_land;
  }
  if (ThisChar == '+' && LastChar == '+') {
    LastChar = getchar();
    return tok_increment;
  }
  if (ThisChar == '-' && LastChar == '-') {
    LastChar = getchar();
    return tok_decrement;
  }

  // Comments.
  if (ThisChar == '/' && LastChar == '*') {
    do {
      ThisChar = LastChar;
      LastChar = getchar();
    } while (LastChar != EOF && !(ThisChar == '*' && LastChar == '/'));

    if (LastChar != EOF) {
      LastChar = getchar();
      return gettok();
    }
  }

  return ThisChar;
}
