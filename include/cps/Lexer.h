#pragma once
#include <string>
#include <cstdint>

namespace cps {

enum Token {
    tok_eof = -1,

    tok_declare = -2,
    tok_integer_kw = -3,
    tok_input = -4,
    tok_output = -5,
    
    tok_if = -6,
    tok_then = -7,
    tok_else = -8,
    tok_endif = -9,
    
    tok_while = -10,
    tok_do = -11,
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
    tok_define = -603  // reserved: DEFINE (set literals) likewise
};

class Lexer {
    int CurrentLine = 1;
    bool HadError = false;
public:
    std::string IdentifierStr;
    std::string StringVal;
    int64_t NumVal;
    double RealVal;
    char CharVal = '\0';

    int gettok();
    int getLine() const { return CurrentLine; }
    bool hadError() const { return HadError; }
};

}
