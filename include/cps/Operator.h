#pragma once
#include "cps/Lexer.h"
#include <string>

namespace cps {

inline std::string operatorSpelling(int Op) {
    switch (Op) {
    case '+': case '-': case '*': case '/': case '&': case '<': case '>':
        return std::string(1, static_cast<char>(Op));
    case tok_div: return "DIV";
    case tok_mod: return "MOD";
    case tok_eq: return "=";
    case tok_ne: return "<>";
    case tok_le: return "<=";
    case tok_ge: return ">=";
    case tok_and: return "AND";
    case tok_or: return "OR";
    case tok_not: return "NOT";
    default: return {};
    }
}

inline std::string operatorName(int Op) {
    return "OPERATOR" + operatorSpelling(Op);
}

} // namespace cps
