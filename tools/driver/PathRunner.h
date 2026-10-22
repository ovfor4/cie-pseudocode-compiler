#pragma once

#include <string>
#include <vector>

// Finds PATH's lli for -r and checks that it is from LLVM 23. Reports a fatal
// error and returns false otherwise.
bool findLli(std::string &LliPath);

// Executes the IR at IRPath with lli and returns the program's status.
// Objects named by INCLUDE_OBJ are passed to lli as extra objects.
int runLli(const std::string &LliPath, const std::string &IRPath,
           const std::vector<std::string> &Objects);
