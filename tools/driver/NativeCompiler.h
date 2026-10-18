#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/MemoryBuffer.h"
#include <memory>
#include <string>
#include <vector>

namespace cps { class CodeGen; }

// Generate a host executable without an external compiler, linker or SDK.
// Failure leaves any existing output intact and reports a diagnostic.
// Objects named by INCLUDE_OBJ are linked after the program object.
bool compileNative(const cps::CodeGen &CG, llvm::StringRef OutputPath, const char *Argv0,
                   const std::vector<std::string> &Objects);

// Defined global C symbols of an INCLUDE_OBJ object file. Returns false with a
// message when the file cannot be read or is not an object file.
bool collectObjectSymbols(llvm::StringRef Path, llvm::StringSet<> &Out, std::string &Error);

// Defined global symbols of the embedded C library (INCLUDE_OBJ "__libc").
bool collectLibcSymbols(llvm::StringSet<> &Out);

// Owning copies of the embedded C++ and compiler-support static archives.
bool unpackCxxRuntime(std::vector<std::unique_ptr<llvm::MemoryBuffer>> &Archives);
