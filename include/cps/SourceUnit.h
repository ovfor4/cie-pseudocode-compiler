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
    std::vector<bool> LibraryLines;
    bool isLibrary(int Line) const {
        return Line > 0 && static_cast<size_t>(Line) <= LibraryLines.size() && LibraryLines[Line - 1];
    }
};
}
