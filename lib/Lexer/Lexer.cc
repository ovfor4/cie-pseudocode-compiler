#include "cps/Diagnostics.h"
#include "cps/Lexer.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace cps;

std::string Lexer::readEscape(bool IsChar) {
    auto Error = [&](const char *Reason) {
        cps::sourceDiagnostic(TokenLine, "error: %s at line %d\n", Reason, TokenLine);
        HadError = true;
        return std::string("?");
    };
    int Escape = Input.get();
    if (Escape == EOF || Escape == '\n' || Escape == '\r') {
        LastChar = Escape;
        return Error("Unterminated escape sequence");
    }
    LastChar = Input.get();
    switch (Escape) {
    case '\'': return "'";
    case '"': return "\"";
    case '?': return "?";
    case '\\': return "\\";
    case 'a': return "\a";
    case 'b': return "\b";
    case 'f': return "\f";
    case 'n': return "\n";
    case 'r': return "\r";
    case 't': return "\t";
    case 'v': return "\v";
    default: break;
    }
    auto Hex = [](int C) -> int {
        if (C >= '0' && C <= '9') return C - '0';
        if (C >= 'a' && C <= 'f') return C - 'a' + 10;
        if (C >= 'A' && C <= 'F') return C - 'A' + 10;
        return -1;
    };
    uint32_t Value = 0;
    if (Escape >= '0' && Escape <= '7') {
        Value = Escape - '0';
        for (int I = 1; I < 3 && LastChar >= '0' && LastChar <= '7'; ++I) {
            Value = Value * 8 + LastChar - '0';
            LastChar = Input.get();
        }
        if (Value > 255) return Error("Octal escape exceeds one byte");
        return std::string(1, static_cast<char>(Value));
    }
    if (Escape == 'x') {
        if (Hex(LastChar) < 0) return Error("Hex escape requires a digit");
        bool Overflow = false;
        while (Hex(LastChar) >= 0) {
            Value = Value * 16 + Hex(LastChar);
            if (Value > 255) { Overflow = true; Value = 0; }
            LastChar = Input.get();
        }
        if (Overflow) return Error("Hex escape exceeds one byte");
        return std::string(1, static_cast<char>(Value));
    }
    if (Escape != 'u' && Escape != 'U') return Error("Unknown escape sequence");
    for (int I = 0; I < (Escape == 'u' ? 4 : 8); ++I) {
        if (Hex(LastChar) < 0) return Error("Incomplete Unicode escape");
        Value = Value * 16 + Hex(LastChar);
        LastChar = Input.get();
    }
    if (Value > 0x10ffff || (Value >= 0xd800 && Value <= 0xdfff))
        return Error("Invalid Unicode code point");
    if (IsChar && Value > 127) return Error("Unicode CHAR escape must be ASCII");
    std::string Result;
    if (Value <= 0x7f) Result += static_cast<char>(Value);
    else {
        if (Value <= 0x7ff) Result += static_cast<char>(0xc0 | (Value >> 6));
        else {
            if (Value <= 0xffff) Result += static_cast<char>(0xe0 | (Value >> 12));
            else {
                Result += static_cast<char>(0xf0 | (Value >> 18));
                Result += static_cast<char>(0x80 | ((Value >> 12) & 63));
            }
            Result += static_cast<char>(0x80 | ((Value >> 6) & 63));
        }
        Result += static_cast<char>(0x80 | (Value & 63));
    }
    return Result;
}

int Lexer::readToken() {
    while (isspace(LastChar)) {
        if (LastChar == '\n') CurrentLine++;
        LastChar = Input.get();
    }

    TokenLine = CurrentLine;

    if (LastChar == '"' || LastChar == '\'') {
        const int Quote = LastChar;
        const bool IsChar = Quote == '\'';
        StringVal.clear();
        LastChar = Input.get();
        while (LastChar != Quote && LastChar != EOF && LastChar != '\n') {
            if (LastChar == '\\') StringVal += readEscape(IsChar);
            else {
                StringVal += static_cast<char>(LastChar);
                LastChar = Input.get();
            }
        }
        if (LastChar == Quote) LastChar = Input.get();
        else {
            cps::sourceDiagnostic(TokenLine, "error: Unterminated %s literal at line %d\n", IsChar ? "char" : "string", TokenLine);
            HadError = true;
        }
        if (IsChar) {
            if (StringVal.size() != 1) {
                cps::sourceDiagnostic(TokenLine, "error: CHAR literal must contain exactly one character at line %d\n", TokenLine);
                HadError = true;
            }
            CharVal = StringVal.size() == 1 ? StringVal[0] : '?';
        }
        return IsChar ? tok_char_literal : tok_string_literal;
    }

    if (isalpha(LastChar)) {
        IdentifierStr = static_cast<char>(LastChar);
        while (isalnum((LastChar = Input.get())) || LastChar == '_')
            IdentifierStr += static_cast<char>(LastChar);

        if (IdentifierStr == "CONSTANT") return tok_constant;
        if (IdentifierStr == "INCLUDE") return tok_include;
        if (IdentifierStr == "INCLUDE_OBJ") return tok_include_obj;
        if (IdentifierStr == "EXTERN") return tok_extern;
        if (IdentifierStr == "EXTERN_FUNCTION") return tok_extern_function;
        if (IdentifierStr == "EXTERN_PROCEDURE") return tok_extern_procedure;
        if (IdentifierStr == "GLOBAL") return tok_global;
        if (IdentifierStr == "ENDGLOBAL") return tok_endglobal;
        if (IdentifierStr == "DECLARE") return tok_declare;
        if (IdentifierStr == "INTEGER") return tok_integer_kw;
        if (IdentifierStr == "BOOLEAN") return tok_boolean_kw;
        if (IdentifierStr == "REAL")    return tok_real_kw;
        if (IdentifierStr == "STRING")  return tok_string_kw;
        if (IdentifierStr == "CHAR")    return tok_char_kw;
        if (IdentifierStr == "C_TYPE") return tok_c_type;
        if (IdentifierStr == "C_INT") return tok_c_int;
        if (IdentifierStr == "C_CHAR") return tok_c_char;
        if (IdentifierStr == "C_STRING") return tok_c_string;
        if (IdentifierStr == "C_DOUBLE") return tok_c_double;

        if (IdentifierStr == "TRUE") return tok_true;
        if (IdentifierStr == "FALSE") return tok_false;

        if (IdentifierStr == "INPUT") return tok_input;
        if (IdentifierStr == "OUTPUT") return tok_output;

        if (IdentifierStr == "CASE") return tok_case;
        if (IdentifierStr == "ENDCASE") return tok_endcase;
        if (IdentifierStr == "OTHERWISE") return tok_otherwise;

        if (IdentifierStr == "IF") return tok_if;
        if (IdentifierStr == "THEN") return tok_then;
        if (IdentifierStr == "ELSE") return tok_else;
        if (IdentifierStr == "ENDIF") return tok_endif;

        if (IdentifierStr == "TRY") return tok_try;
        if (IdentifierStr == "CATCH") return tok_catch;
        if (IdentifierStr == "ENDTRY") return tok_endtry;
        if (IdentifierStr == "THROW") return tok_throw;

        if (IdentifierStr == "WHILE") return tok_while;
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

        if (IdentifierStr == "OPERATOR") return tok_operator;
        if (IdentifierStr == "OVERLOAD") return tok_overload;
        if (IdentifierStr == "FUNCTION") return tok_function;
        if (IdentifierStr == "ENDFUNCTION") return tok_endfunction;
        if (IdentifierStr == "PROCEDURE") return tok_procedure;
        if (IdentifierStr == "ENDPROCEDURE") return tok_endprocedure;
        if (IdentifierStr == "RETURN") return tok_return;
        if (IdentifierStr == "RETURNS") return tok_returns;
        if (IdentifierStr == "CALL") return tok_call;

        if (IdentifierStr == "BYREF") return tok_byref;
        if (IdentifierStr == "BYVAL") return tok_byval;

        if (IdentifierStr == "OPENFILE") return tok_openfile;
        if (IdentifierStr == "READFILE") return tok_readfile;
        if (IdentifierStr == "WRITEFILE") return tok_writefile;
        if (IdentifierStr == "CLOSEFILE") return tok_closefile;
        if (IdentifierStr == "READ") return tok_read;
        if (IdentifierStr == "WRITE") return tok_write;
        if (IdentifierStr == "APPEND") return tok_append;
        if (IdentifierStr == "RANDOM") return tok_random;

        if (IdentifierStr == "TYPE") return tok_type;
        if (IdentifierStr == "ENDTYPE") return tok_endtype;
        if (IdentifierStr == "SET") return tok_set;
        if (IdentifierStr == "DEFINE") return tok_define;
        if (IdentifierStr == "NEW") return tok_new;
        if (IdentifierStr == "DELETE") return tok_delete;
        if (IdentifierStr == "CLASS") return tok_class;
        if (IdentifierStr == "ENDCLASS") return tok_endclass;
        if (IdentifierStr == "PUBLIC") return tok_public;
        if (IdentifierStr == "PRIVATE") return tok_private;

        return tok_identifier;
    }

    if (isdigit(LastChar)) {
        std::string NumStr;
        bool isReal = false;
        do {
            NumStr += static_cast<char>(LastChar);
            LastChar = Input.get();
            if (LastChar == '.' && !isReal) {
                isReal = true;
                NumStr += '.';
                LastChar = Input.get();
            }
        } while (isdigit(LastChar));

        if (isReal) {
            RealVal = strtod(NumStr.c_str(), nullptr);
            return tok_number_real;
        }

        auto Parsed = std::from_chars(NumStr.data(), NumStr.data() + NumStr.size(), NumVal);
        if (Parsed.ec != std::errc{} ||
            NumVal > uint64_t(std::numeric_limits<int64_t>::max()) + 1) {
            cps::sourceDiagnostic(CurrentLine, "error: Integer literal is outside the i64 range at line %d\n", TokenLine);
            HadError = true;
            NumVal = 0;
        }
        return tok_number_int;
    }

    if (LastChar == '<') {
        LastChar = Input.get();
        if (LastChar == '-') {
            LastChar = Input.get();
            return tok_assign;
        }
        if (LastChar == '=') {
            LastChar = Input.get();
            return tok_le;
        }
        if (LastChar == '>') {
            LastChar = Input.get();
            return tok_ne;
        }
        return '<';
    }

    if (LastChar == '>') {
        LastChar = Input.get();
        if (LastChar == '=') {
            LastChar = Input.get();
            return tok_ge;
        }
        return '>';
    }

    if (LastChar == '=') {
        LastChar = Input.get();
        return tok_eq;
    }

    if (LastChar == ':') {
        LastChar = Input.get();
        return tok_colon;
    }

    if (LastChar == '.' && Input.peek() == '.') {
        Input.get();
        if (Input.peek() == '.') {
            Input.get();
            LastChar = Input.get();
            return tok_ellipsis;
        }
        cps::sourceDiagnostic(CurrentLine, "error: Expected '...' at line %d\n", CurrentLine);
        HadError = true;
        LastChar = Input.get();
        return tok_ellipsis;
    }

    if (LastChar == '(') { LastChar = Input.get(); return '('; }
    if (LastChar == ')') { LastChar = Input.get(); return ')'; }
    if (LastChar == '[') { LastChar = Input.get(); return '['; }
    if (LastChar == ']') { LastChar = Input.get(); return ']'; }
    if (LastChar == ',') { LastChar = Input.get(); return ','; }
    if (LastChar == '+') { LastChar = Input.get(); return '+'; }
    if (LastChar == '-') { LastChar = Input.get(); return '-'; }
    if (LastChar == '*') { LastChar = Input.get(); return '*'; }

    if (LastChar == '/') {
        int NextChar = Input.get();
        if (NextChar == '/') {
            do {
                LastChar = Input.get();
            } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

            if (LastChar != EOF)
                return readToken();
        } else {
            LastChar = NextChar;
            return '/';
        }
    }

    if (LastChar == EOF)
        return tok_eof;

    int ThisChar = LastChar;
    LastChar = Input.get();
    return ThisChar;
}

Lexer::TokenData Lexer::captureToken(int Kind) const {
    return {Kind, TokenLine, IdentifierStr, StringVal, NumVal, RealVal, CharVal};
}

void Lexer::restoreToken(const TokenData &Token) {
    TokenLine = Token.Line;
    IdentifierStr = Token.Identifier;
    StringVal = Token.String;
    NumVal = Token.Integer;
    RealVal = Token.Real;
    CharVal = Token.Character;
}

int Lexer::gettok() {
    if (Lookahead.empty()) return readToken();
    auto Token = std::move(Lookahead.front());
    Lookahead.pop_front();
    restoreToken(Token);
    return Token.Kind;
}

int Lexer::peekToken(unsigned Offset) {
    auto Current = captureToken(0);
    while (Lookahead.size() <= Offset) {
        int Kind = readToken();
        Lookahead.push_back(captureToken(Kind));
    }
    restoreToken(Current);
    return Lookahead[Offset].Kind;
}

int Lexer::peekLine(unsigned Offset) {
    peekToken(Offset);
    return Lookahead[Offset].Line;
}
