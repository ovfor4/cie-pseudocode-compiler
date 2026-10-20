#pragma once
#include "llvm/ADT/SmallString.h"
#include <string>
#include <vector>

// Extracted files live through native linking and runner execution.
class StandardLibrary {
    llvm::SmallString<256> Directory;
public:
    ~StandardLibrary();
    bool load(std::vector<std::string> &Inputs);
};
