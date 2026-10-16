#pragma once

#include "llvm/ADT/StringRef.h"

namespace cps { class CodeGen; }

// Generate a host executable without an external compiler, linker or SDK.
// Failure leaves any existing output intact and reports a diagnostic.
bool compileNative(const cps::CodeGen &CG, llvm::StringRef OutputPath, const char *Argv0);
