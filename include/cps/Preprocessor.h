#pragma once
#include <istream>
#include <string>
#include <vector>

namespace cps {

struct PreprocessedSource {
    std::string Text;
    // One entry per original source line, before processing that line's directive.
    std::vector<bool> LibraryLines;
    bool HadError = false;
};

// Switches and conditional nesting belong to this input only. Masked lines retain
// their byte lengths and line endings so the lexer sees original source locations.
PreprocessedSource preprocess(std::istream &Input);

}
