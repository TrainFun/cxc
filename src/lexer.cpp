#include "lexer.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

std::string IdentifierStr;
unsigned NumVal;

/// gettok - Return the next token from standard input.
int gettok() {
  static int LastChar = ' ';

  // Skip whitespaces.
  while (isspace(LastChar))
    LastChar = getchar();

  // Identifiers.
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "true")
      return tok_true;
    if (IdentifierStr == "false")
      return tok_false;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "else")
      return tok_else;
    if (IdentifierStr == "while")
      return tok_while;
    if (IdentifierStr == "write")
      return tok_write;
    if (IdentifierStr == "read")
      return tok_read;
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "int")
      return tok_int;
    if (IdentifierStr == "bool")
      return tok_bool;
    return tok_identifier;
  }

  // Numbers.
  if (isdigit(LastChar)) {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar));

    NumVal = atoi(NumStr.c_str());
    return tok_number;
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
