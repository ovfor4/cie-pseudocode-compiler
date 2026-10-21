# Overview and value representation

`cpsc` compiles CIE A-Level 9618 pseudocode to native executables or LLVM IR. Classic three-stage
Kaleidoscope-style design, C++17, built with LLVM 23 and Clang 23. CMake rejects
other major versions and non-Clang compilers. Execution is selected at
build time: external lli, an embedded static LLVM engine, or disabled.

```
files ──> Lexer ──> Parser ──> vector<unique_ptr<StmtAST>> ──> CodeGen ──> LLVM IR ──> NativeCompiler (object emission + embedded LLD)
        (istream)  (recursive       (flat program list;         (one llvm::Module "cps_module",
                    descent)         FunctionDefAST mixed in)    entry code inside i32 main())
```

Fixed value representation (ABI — much code infers types backwards from these):
INTEGER = `i64`, REAL = `double`, BOOLEAN = `i1`, CHAR = `i8`. STRING is the standard-library class `%record.STRING`, owning a std::string handle; C_STRING is a raw NUL-terminated pointer. User-defined types (§4): enum = `i64` (0-based ordinal),
pointer = opaque `ptr`, record = named struct `%record.NAME`. Enums and pointers are LLVM-identical to INTEGER/C_STRING **by design** — their identity lives in the type
*name*, enforced upstream (see CONVENTIONS.md). Allocation byte sizes are never computed
host-side: the module has no DataLayout string, so every allocation size is a null-GEP
`sizeof` constant (`TypeSystem::getSizeOfConstant`) folded by the backend.

C_INT/C_CHAR/C_STRING/C_DOUBLE alias native int/char/char*/double. C_TYPE string
imports cover standard C integer/real types, _Bool, void and object pointers.
CScalar/CPointer identities are canonical C types, not pseudocode alias names;
native widths and signedness come from sizeof/type traits, while floating formats
come from numeric_limits (long double size alone is insufficient). Programs use
the compiler's build ABI; cross-target emission is not implemented. EXTERN "C"
declarations call C functions from INCLUDE_OBJ objects (see SUBSYSTEMS.md).
CTypeGen handles typed numeric conversions, casts, native arithmetic and I/O.
Crossing C/pseudocode domains requires a cast, including literals. C arithmetic
uses C++ promotions, except that / always returns pseudocode REAL.

## Language manuals

See [MAN.md](../MAN.md) before editing the command or syntax manuals. It defines
source layout, naming, prose formatting and the small checks needed for changes.
