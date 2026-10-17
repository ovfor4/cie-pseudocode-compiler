#pragma once
#include "cps/AST.h"
#include <string>
#include <vector>

namespace cps {
// The caller owns the parsed statements for the duration of compilation.
struct SourceUnit {
    std::string Path;
    const std::vector<std::unique_ptr<StmtAST>> &Statements;
    std::vector<bool> Visible;
};
}
