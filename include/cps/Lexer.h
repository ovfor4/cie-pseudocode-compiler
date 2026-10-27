#pragma once
#include <istream>
#include <string>
#include <cstdint>
#include <deque>

namespace cps {

enum Token {
    tok_eof = -1,

    tok_declare = -2,
    tok_integer_kw = -3,
    tok_input = -4,
    tok_output = -5,
    
    tok_constant = -43,
    tok_case = -40,
    tok_endcase = -41,
    tok_otherwise = -42,

    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_endif = -9,
    
    tok_while = -10,
    tok_endwhile = -12,
    
    tok_repeat = -13,
    tok_until = -14,
    
    tok_for = -15,
    tok_to = -16,
    tok_step = -17,
    tok_next = -18,

    tok_array = -19,
    tok_of = -20,

    tok_real_kw = -21,
    tok_boolean_kw = -22,
    tok_true = -23,
    tok_false = -24,
    
    tok_and = -25,
    tok_or = -26,
    tok_not = -27,

    tok_string_kw = -30,
    tok_string_literal = -36,
    tok_c_string_literal = -37,
    tok_char_kw = -38,
    tok_char_literal = -39,

    tok_identifier = -100,
    tok_number_int = -101,
    tok_number_real = -102,
    
    tok_assign = -200, // <-
    tok_eq = -201, // =
    tok_ne = -202, // <>
    tok_le = -203, // <=
    tok_ge = -204, // >=

    tok_div = -205,
    tok_mod = -206,

    tok_colon = -300, // :

    tok_operator = -398,
    tok_overload = -399,
    tok_function = -400, 
    tok_endfunction = -401, 
    tok_return = -402, 
    tok_returns = -403,
    tok_procedure = -404, 
    tok_endprocedure = -405, 
    tok_call = -406,
    tok_byref = -407,
    tok_byval = -408,

    tok_openfile = -500,
    tok_readfile = -501,
    tok_writefile = -502,
    tok_closefile = -503,
    tok_read = -504,
    tok_write = -505,
    tok_append = -506,
    tok_random = -507,

    tok_type = -600,
    tok_endtype = -601,
    tok_set = -602,    // reserved: SET types are recognised but not implemented
    tok_define = -603, // reserved: DEFINE (set literals) likewise
    tok_class = -604,
    tok_endclass = -605,
    tok_public = -606,
    tok_private = -607,
    tok_c_type = -608,
    tok_c_int = -609,
    tok_c_char = -610,
    tok_c_string = -611,
    tok_c_double = -612,
    tok_new = -613, tok_delete = -614,
    tok_template = -615,

    tok_include = -650, tok_global = -651, tok_endglobal = -652,
    tok_include_obj = -653, tok_extern = -654,
    tok_extern_function = -655, tok_extern_procedure = -656,
    tok_ellipsis = -657, // ...

    tok_try = -700,
    tok_catch = -701,
    tok_endtry = -702,
    tok_throw = -703,

    tok_pp_define = -710,
    tok_pp_undef = -711,
    tok_pp_ifdef = -712,
    tok_pp_ifndef = -713,
    tok_pp_endif = -714
};

class Lexer {
    std::istream &Input;
    int LastChar = ' ';
    int CurrentLine = 1;
    int TokenLine = 1;
    bool HadError = false;
    bool LineStart = true;
    struct TokenData {
        int Kind, Line;
        std::string Identifier, String;
        uint64_t Integer;
        double Real;
        char Character;
    };
    std::deque<TokenData> Lookahead;
    // Reports a source error at Line (printf-style) and marks the lexer as failed.
    __attribute__((format(printf, 3, 4)))
    void reportError(int Line, const char *Format, ...);
    std::string readEscape(bool IsChar);
    int readToken();
    TokenData captureToken(int Kind) const;
    void restoreToken(const TokenData &Token);
public:
    explicit Lexer(std::istream &Input) : Input(Input) {}

    std::string IdentifierStr;
    std::string StringVal;
    uint64_t NumVal = 0;
    double RealVal = 0;
    char CharVal = '\0';

    int gettok();
    int peekToken(unsigned Offset = 0);
    int peekLine(unsigned Offset = 0);
    int getLine() const { return TokenLine; }
    bool hadError() const { return HadError; }
    // Human-readable spelling of Tok for diagnostics; Lex supplies identifier text.
    static std::string tokenSpelling(int Tok, const Lexer &Lex);
};

}
