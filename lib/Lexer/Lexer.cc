#include "cps/Lexer.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cps;

int Lexer::gettok() {
    static int LastChar = ' ';

    while (isspace(LastChar)) {
        if (LastChar == '\n') CurrentLine++;
        LastChar = getchar();
    }

    if (LastChar == '"') {
        StringVal.clear();
        LastChar = getchar();

        while (LastChar != '"' && LastChar != EOF && LastChar != '\n') {
            StringVal += static_cast<char>(LastChar);
            LastChar = getchar();
        }

        if (LastChar == '"') {
            LastChar = getchar();
        } else {
            fprintf(stderr, "Error: Unterminated string literal at line %d\n", CurrentLine);
            HadError = true;
        }

        return tok_string_literal;
    }

    if (LastChar == '\'') {
        int CharCode = getchar();
        if (CharCode == EOF || CharCode == '\n') {
            fprintf(stderr, "Error: Unterminated char literal at line %d\n", CurrentLine);
            HadError = true;
            CharVal = '?';
            StringVal.assign(1, CharVal);
            // Leave the newline/EOF for the main loop so line counting stays right.
            LastChar = CharCode;
            return tok_char_literal;
        }

        int ClosingQuote = getchar();
        if (ClosingQuote != '\'') {
            fprintf(stderr, "Error: CHAR literal must contain exactly one character (line %d)\n", CurrentLine);
            HadError = true;
            while (ClosingQuote != EOF && ClosingQuote != '\n' && ClosingQuote != '\'') {
                ClosingQuote = getchar();
            }
            CharVal = static_cast<char>(CharCode);
            StringVal.assign(1, CharVal);
            LastChar = (ClosingQuote == '\'') ? getchar() : ClosingQuote;
            return tok_char_literal;
        }

        CharVal = static_cast<char>(CharCode);
        StringVal.assign(1, CharVal);
        LastChar = getchar();
        return tok_char_literal;
    }

    if (isalpha(LastChar)) {
        IdentifierStr = static_cast<char>(LastChar);
        while (isalnum((LastChar = getchar())) || LastChar == '_')
            IdentifierStr += static_cast<char>(LastChar);

        if (IdentifierStr == "DECLARE") return tok_declare;
        if (IdentifierStr == "INTEGER") return tok_integer_kw;
        if (IdentifierStr == "BOOLEAN") return tok_boolean_kw;
        if (IdentifierStr == "REAL")    return tok_real_kw;
        if (IdentifierStr == "STRING")  return tok_string_kw;
        if (IdentifierStr == "CHAR")    return tok_char_kw;

        if (IdentifierStr == "TRUE") return tok_true;
        if (IdentifierStr == "FALSE") return tok_false;

        if (IdentifierStr == "INPUT") return tok_input;
        if (IdentifierStr == "OUTPUT") return tok_output;

        if (IdentifierStr == "IF") return tok_if;
        if (IdentifierStr == "THEN") return tok_then;
        if (IdentifierStr == "ELSE") return tok_else;
        if (IdentifierStr == "ENDIF") return tok_endif;

        if (IdentifierStr == "WHILE") return tok_while;
        if (IdentifierStr == "DO") return tok_do;
        if (IdentifierStr == "ENDWHILE") return tok_endwhile;

        if (IdentifierStr == "REPEAT") return tok_repeat;
        if (IdentifierStr == "UNTIL") return tok_until;

        if (IdentifierStr == "FOR") return tok_for;
        if (IdentifierStr == "TO") return tok_to;
        if (IdentifierStr == "STEP") return tok_step;
        if (IdentifierStr == "NEXT") return tok_next;

        if (IdentifierStr == "ARRAY") return tok_array;
        if (IdentifierStr == "OF") return tok_of;

        if (IdentifierStr == "DIV") return tok_div;
        if (IdentifierStr == "MOD") return tok_mod;

        if (IdentifierStr == "AND") return tok_and;
        if (IdentifierStr == "OR") return tok_or;
        if (IdentifierStr == "NOT") return tok_not;

        if (IdentifierStr == "FUNCTION") return tok_function;
        if (IdentifierStr == "ENDFUNCTION") return tok_endfunction;
        if (IdentifierStr == "PROCEDURE") return tok_procedure;
        if (IdentifierStr == "ENDPROCEDURE") return tok_endprocedure;
        if (IdentifierStr == "RETURN") return tok_return;
        if (IdentifierStr == "RETURNS") return tok_returns;
        if (IdentifierStr == "CALL") return tok_call;

        if (IdentifierStr == "BYREF") return tok_byref;
        if (IdentifierStr == "BYVAL") return tok_byval;

        return tok_identifier;
    }

    if (isdigit(LastChar)) {
        std::string NumStr;
        bool isReal = false;
        do {
            NumStr += static_cast<char>(LastChar);
            LastChar = getchar();
            if (LastChar == '.' && !isReal) {
                isReal = true;
                NumStr += '.';
                LastChar = getchar();
            }
        } while (isdigit(LastChar));

        if (isReal) {
            RealVal = strtod(NumStr.c_str(), nullptr);
            return tok_number_real;
        }

        NumVal = strtoll(NumStr.c_str(), nullptr, 10);
        return tok_number_int;
    }

    if (LastChar == '<') {
        LastChar = getchar();
        if (LastChar == '-') {
            LastChar = getchar();
            return tok_assign;
        }
        if (LastChar == '=') {
            LastChar = getchar();
            return tok_le;
        }
        if (LastChar == '>') {
            LastChar = getchar();
            return tok_ne;
        }
        return '<';
    }

    if (LastChar == '>') {
        LastChar = getchar();
        if (LastChar == '=') {
            LastChar = getchar();
            return tok_ge;
        }
        return '>';
    }

    if (LastChar == '=') {
        LastChar = getchar();
        return tok_eq;
    }

    if (LastChar == ':') {
        LastChar = getchar();
        return tok_colon;
    }

    if (LastChar == '(') { LastChar = getchar(); return '('; }
    if (LastChar == ')') { LastChar = getchar(); return ')'; }
    if (LastChar == '[') { LastChar = getchar(); return '['; }
    if (LastChar == ']') { LastChar = getchar(); return ']'; }
    if (LastChar == ',') { LastChar = getchar(); return ','; }
    if (LastChar == '+') { LastChar = getchar(); return '+'; }
    if (LastChar == '-') { LastChar = getchar(); return '-'; }
    if (LastChar == '*') { LastChar = getchar(); return '*'; }

    if (LastChar == '/') {
        int NextChar = getchar();
        if (NextChar == '/') {
            do {
                LastChar = getchar();
            } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

            if (LastChar != EOF)
                return gettok();
        } else {
            LastChar = NextChar;
            return '/';
        }
    }

    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = getchar();
    return ThisChar;
}
