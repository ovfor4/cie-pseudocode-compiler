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
    {"EOF",        1, "BOOLEAN"},
    {"MALLOC",     1, "C_TYPE(\"void*\")"},
    {"REALLOC",    2, "C_TYPE(\"void*\")"},
    {"FREE",       1, "C_TYPE(\"void*\")"},
    {"INT",        1, "INTEGER"},
};

inline const BuiltinInfo *findBuiltin(const std::string &Name) {
    for (const auto &B : Builtins) {
        if (sameIdentifier(Name, B.Name)) return &B;
    }
    return nullptr;
}

} // namespace cps::detail
