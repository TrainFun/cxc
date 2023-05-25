#pragma once

#include <string>

enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,

  // primary
  tok_identifier = -3,
  tok_number = -4,
  tok_true = -5,
  tok_false = -6,

  // control
  tok_if = -7,
  tok_else = -8,
  tok_while = -9,
  tok_write = -10,
  tok_read = -11,

  // operators
  tok_le = -12,   // <=
  tok_ge = -13,   // >=
  tok_eq = -14,   // ==
  tok_ne = -15,   // !=
  tok_lor = -16,  // ||
  tok_land = -17, // &&

  // var definition
  tok_int = -18,
  tok_bool = -19,
};

extern std::string IdentifierStr;
extern unsigned NumVal;

int gettok();
