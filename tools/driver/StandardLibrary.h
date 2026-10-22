#pragma once
#include "llvm/ADT/SmallString.h"
#include <string>
#include <vector>

// Extracted files live through native linking and runner execution. They are cached per user
// (never deleted); only the fallback temporary directory is removed on destruction.
class StandardLibrary {
    llvm::SmallString<256> Directory;
    bool Temporary = false;
public:
    ~StandardLibrary();
    bool load(std::vector<std::string> &Inputs);
};
