#include "cps/Diagnostics.h"
#include "cps/Lexer.h"
#include <algorithm>
#include <charconv>
#include <limits>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

using namespace cps;

namespace {
const std::unordered_map<std::string, int> Keywords = {
    {"PP_DEFINE", tok_pp_define},
    {"PP_UNDEF", tok_pp_undef},
    {"PP_IFDEF", tok_pp_ifdef},
    {"PP_IFNDEF", tok_pp_ifndef},
    {"PP_ENDIF", tok_pp_endif},
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
    {"DEFAULT", tok_default},
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
}

std::string Lexer::tokenSpelling(int Tok, const Lexer &Lex) {
    switch (Tok) {
    case tok_eof:          return "end of file";
    case tok_identifier:   return "identifier '" + Lex.IdentifierStr + "'";
    case tok_number_int:
    case tok_number_real:  return "number";
    case tok_string_literal:
    case tok_c_string_literal: return "string literal";
    case tok_char_literal: return "character literal";
    case tok_assign:       return "'<-'";
    case tok_eq:           return "'='";
    case tok_ne:           return "'<>'";
    case tok_le:           return "'<='";
    case tok_ge:           return "'>='";
    case tok_colon:        return "':'";
    case tok_ellipsis:     return "'...'";
    default: break;
    }
    for (const auto &[Name, Kind] : Keywords)
        if (Kind == Tok) return Name;
    if (Tok > 0) return std::string("'") + static_cast<char>(Tok) + "'";
    return "unknown token";
}


void Lexer::reportError(int Line, const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    cps::vsourceDiagnostic(cps::Severity::Error, Line, Format, Args);
    va_end(Args);
    HadError = true;
}

std::string Lexer::readEscape(bool IsChar) {
    auto Error = [&](const char *Reason) {
        reportError(TokenLine, "%s", Reason);
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
        do {
            LastChar = Input.get();
        } while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');
        reportError(TokenLine, "Unknown compiler directive");
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
            reportError(TokenLine, "Unterminated %s literal", IsChar ? "char" : "string");
        }
        if (IsChar) {
            if (StringVal.size() != 1) {
                reportError(TokenLine, "CHAR literal must contain exactly one character");
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
                reportError(TokenLine, "Reserved runtime identifier");
            }
        }
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
            reportError(TokenLine, "Integer literal is outside the i64 range");
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
        reportError(CurrentLine, "Expected '...'");
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
