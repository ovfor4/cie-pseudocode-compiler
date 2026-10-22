#include "cps/Diagnostics.h"
#include "cps/Lexer.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

using namespace cps;

std::string Lexer::readEscape(bool IsChar) {
    auto Error = [&](const char *Reason) {
        cps::sourceError(TokenLine, "%s", Reason);
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
        if (LastChar == '\n') { CurrentLine++; LineStart = true; }
        LastChar = Input.get();
    }

    TokenLine = CurrentLine;
    if (LastChar == '#' && LineStart) {
        std::string Directive;
        do {
            Directive += static_cast<char>(LastChar);
            LastChar = Input.get();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        while (!Directive.empty() && isspace(static_cast<unsigned char>(Directive.back())))
            Directive.pop_back();
        if (Directive == "#LIB") Library = true;
        else {
            cps::sourceError(TokenLine, "Unknown compiler directive");
            HadError = true;
        }
        return readToken();
    }
    LineStart = false;

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
            cps::sourceError(TokenLine, "Unterminated %s literal", IsChar ? "char" : "string");
            HadError = true;
        }
        if (IsChar) {
            if (StringVal.size() != 1) {
                cps::sourceError(TokenLine, "CHAR literal must contain exactly one character");
                HadError = true;
            }
            CharVal = StringVal.size() == 1 ? StringVal[0] : '?';
        }
        if (!IsChar && LastChar == 'C') {
            LastChar = Input.get();
            return tok_c_string_literal;
        }
        return IsChar ? tok_char_literal : tok_string_literal;
    }

    if (isalpha(LastChar) || LastChar == '_') {
        IdentifierStr = static_cast<char>(LastChar);
        while (isalnum((LastChar = Input.get())) || LastChar == '_')
            IdentifierStr += static_cast<char>(LastChar);

        if (IdentifierStr.size() >= 6) {
            std::string Prefix = IdentifierStr.substr(0, 6);
            std::transform(Prefix.begin(), Prefix.end(), Prefix.begin(),
                           [](unsigned char C) { return std::tolower(C); });
            if (Prefix == "__cps_") {
                cps::sourceError(TokenLine, "Reserved runtime identifier");
                HadError = true;
            }
        }
        static const std::unordered_map<std::string, int> Keywords = {
            {"CONSTANT", tok_constant},
            {"INCLUDE", tok_include},
            {"INCLUDE_OBJ", tok_include_obj},
            {"EXTERN", tok_extern},
            {"EXTERN_FUNCTION", tok_extern_function},
            {"EXTERN_PROCEDURE", tok_extern_procedure},
            {"GLOBAL", tok_global},
            {"ENDGLOBAL", tok_endglobal},
            {"DECLARE", tok_declare},
            {"INTEGER", tok_integer_kw},
            {"BOOLEAN", tok_boolean_kw},
            {"REAL", tok_real_kw},
            {"STRING", tok_string_kw},
            {"CHAR", tok_char_kw},
            {"C_TYPE", tok_c_type},
            {"C_INT", tok_c_int},
            {"C_CHAR", tok_c_char},
            {"C_STRING", tok_c_string},
            {"C_DOUBLE", tok_c_double},
            {"TRUE", tok_true},
            {"FALSE", tok_false},
            {"INPUT", tok_input},
            {"OUTPUT", tok_output},
            {"CASE", tok_case},
            {"ENDCASE", tok_endcase},
            {"OTHERWISE", tok_otherwise},
            {"IF", tok_if},
            {"THEN", tok_then},
            {"ELSE", tok_else},
            {"ENDIF", tok_endif},
            {"TRY", tok_try},
            {"CATCH", tok_catch},
            {"ENDTRY", tok_endtry},
            {"THROW", tok_throw},
            {"WHILE", tok_while},
            {"ENDWHILE", tok_endwhile},
            {"REPEAT", tok_repeat},
            {"UNTIL", tok_until},
            {"FOR", tok_for},
            {"TO", tok_to},
            {"STEP", tok_step},
            {"NEXT", tok_next},
            {"ARRAY", tok_array},
            {"OF", tok_of},
            {"DIV", tok_div},
            {"MOD", tok_mod},
            {"AND", tok_and},
            {"OR", tok_or},
            {"NOT", tok_not},
            {"OPERATOR", tok_operator},
            {"OVERLOAD", tok_overload},
            {"TEMPLATE", tok_template},
            {"FUNCTION", tok_function},
            {"ENDFUNCTION", tok_endfunction},
            {"PROCEDURE", tok_procedure},
            {"ENDPROCEDURE", tok_endprocedure},
            {"RETURN", tok_return},
            {"RETURNS", tok_returns},
            {"CALL", tok_call},
            {"BYREF", tok_byref},
            {"BYVAL", tok_byval},
            {"OPENFILE", tok_openfile},
            {"READFILE", tok_readfile},
            {"WRITEFILE", tok_writefile},
            {"CLOSEFILE", tok_closefile},
            {"READ", tok_read},
            {"WRITE", tok_write},
            {"APPEND", tok_append},
            {"RANDOM", tok_random},
            {"TYPE", tok_type},
            {"ENDTYPE", tok_endtype},
            {"SET", tok_set},
            {"DEFINE", tok_define},
            {"NEW", tok_new},
            {"DELETE", tok_delete},
            {"CLASS", tok_class},
            {"ENDCLASS", tok_endclass},
            {"PUBLIC", tok_public},
            {"PRIVATE", tok_private},
        };
        auto Keyword = Keywords.find(IdentifierStr);
        if (Keyword != Keywords.end()) return Keyword->second;

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
            cps::sourceError(TokenLine, "Integer literal is outside the i64 range");
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
        cps::sourceError(CurrentLine, "Expected '...'");
        HadError = true;
        LastChar = Input.get();
        return tok_ellipsis;
    }

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
