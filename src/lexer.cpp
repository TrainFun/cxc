#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
        tok_eof = -1,

        // primary
        tok_identifier = -2,
        tok_number = -3,

        // control
        tok_if = -4,
        tok_else = -5,
        tok_while = -6,
        tok_write = -7,
        tok_read = -8,

        // operators
        tok_le = -9,    // <=
        tok_ge = -10,   // >=
        tok_eq = -11,   // ==
        tok_ne = -12,   // !=
        tok_lor = -13,  // ||
        tok_land = -14, // &&

        // var definition
        tok_int = -15,
        tok_bool = -16,
};

static std::string IdentifierStr;
static double NumVal;

/// gettok - Return the next token from standard input.
static int gettok() {
        static int LastChar = ' ';

        // Skip whitespaces.
        while (isspace(LastChar))
                LastChar = getchar();

        // Identifiers.
        if (isalpha(LastChar)) {
                IdentifierStr = LastChar;
                while (isalnum((LastChar = getchar())))
                        IdentifierStr += LastChar;

                if (IdentifierStr == "int")
                        return tok_int;
                if (IdentifierStr == "bool")
                        return tok_bool;
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

        if (LastChar == '/' && (LastChar = getchar()) == '*') {
        }

        if (LastChar == EOF)
                return tok_eof;

        int ThisChar = LastChar;
        LastChar = getchar();

        // Multi-char operators
        if (ThisChar == '<' && LastChar == '=')
                return tok_le;
        if (ThisChar == '>' && LastChar == '=')
                return tok_ge;
        if (ThisChar == '=' && LastChar == '=')
                return tok_eq;
        if (ThisChar == '!' && LastChar == '=')
                return tok_ne;
        if (ThisChar == '|' && LastChar == '|')
                return tok_lor;
        if (ThisChar == '&' && LastChar == '&')
                return tok_land;

        // Comments.
        if (ThisChar == '/' && LastChar == '*') {
                do {
                        ThisChar = LastChar;
                        LastChar = getchar();
                } while (LastChar != EOF && !(ThisChar == '*' && LastChar == '/'));

                if (LastChar != EOF)
                        return gettok();
        }

        return ThisChar;
}
