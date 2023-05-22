#pragma once

#include <string>

enum Token {
  tok_eof = -1,

  // primary
  tok_identifier = -2,
  tok_number = -3,
  tok_true = -4,
  tok_false = -5,

  // control
  tok_if = -6,
  tok_else = -7,
  tok_while = -8,
  tok_write = -9,
  tok_read = -10,

  // operators
  tok_le = -11,   // <=
  tok_ge = -12,   // >=
  tok_eq = -13,   // ==
  tok_ne = -14,   // !=
  tok_lor = -15,  // ||
  tok_land = -16, // &&

  // var definition
  tok_int = -17,
  tok_bool = -18,
};

extern std::string IdentifierStr;
extern unsigned NumVal;

int gettok();
