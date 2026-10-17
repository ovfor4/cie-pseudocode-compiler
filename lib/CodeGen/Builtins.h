#pragma once
#include "cps/Identifier.h"
#include <string>

namespace cps::detail {

// Single source of truth for the built-in functions: name, arity and return
// type. emitExpr checks arity against this table before dispatching, and
// getExprTypeInfo reads the return type from it.
struct BuiltinInfo {
    const char *Name;
    unsigned Arity;
    const char *ReturnTypeName;
};

inline constexpr BuiltinInfo Builtins[] = {
    {"LENGTH",     1, "INTEGER"},
    {"MID",        3, "STRING"},
    {"RIGHT",      2, "STRING"},
    {"LEFT",       2, "STRING"},
    {"LCASE",      1, "STRING"},
    {"UCASE",      1, "STRING"},
    {"ASC",        1, "INTEGER"},
    {"CHR",        1, "STRING"},
    {"IS_NUM",     1, "BOOLEAN"},
    {"NUM_TO_STR", 1, "STRING"},
    {"STR_TO_NUM", 1, "REAL"},
    {"EOF",        1, "BOOLEAN"},
    {"MALLOC",     1, "C_TYPE(\"void*\")"},
    {"REALLOC",    2, "C_TYPE(\"void*\")"},
    {"FREE",       1, "C_TYPE(\"void*\")"},
};

inline const BuiltinInfo *findBuiltin(const std::string &Name) {
    for (const auto &B : Builtins) {
        if (sameIdentifier(Name, B.Name)) return &B;
    }
    return nullptr;
}

} // namespace cps::detail
