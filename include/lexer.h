#pragma once

#include <string>

enum Token {
  tok_eof = -1,

  // commands
  // tok_def = -2,

  // primary
  tok_identifier = -3,
  tok_intliteral = -4,
  tok_doubleliteral = -5,
  tok_true = -6,
  tok_false = -7,

  // control
  tok_if = -8,
  tok_else = -9,
  tok_switch = -10,
  tok_default = -11,
  tok_case = -12,
  tok_while = -13,
  tok_do = -14,
  tok_for = -15,
  tok_repeat = -16,
  tok_until = -17,
  tok_write = -18,
  tok_read = -19,
  tok_continue = -20,
  tok_break = -21,
  tok_return = -22,
  tok_exit = -23,

  // operators
  tok_le = -24,   // <=
  tok_ge = -25,   // >=
  tok_eq = -26,   // ==
  tok_ne = -27,   // !=
  tok_lor = -28,  // ||
  tok_land = -29, // &&
  tok_increment = -30,
  tok_decrement = -31,
  tok_ODD = -32,

  // var definition
  tok_int = -33,
  tok_bool = -34,
  tok_double = -35,
  tok_const = -36,
};

extern std::string IdentifierStr;
extern double NumVal;

int gettok();
