#pragma once

namespace cps { class CodeGen; }

#include <string>
#include <vector>

// Objects named by INCLUDE_OBJ are loaded into the JIT beside the program.
int runEmbedded(const cps::CodeGen &CG, const std::vector<std::string> &Objects);
