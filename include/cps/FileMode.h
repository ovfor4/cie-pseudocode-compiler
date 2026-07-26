#pragma once
#include <cstdint>

namespace cps {

// Shared between FileAST (parser side) and FileHandler (codegen side).
// The numeric values are the mode byte stored in the runtime file table;
// 0 is reserved to mean "free slot".
enum class FileMode : uint8_t { Read = 1, Write = 2, Append = 3 };

} // namespace cps
