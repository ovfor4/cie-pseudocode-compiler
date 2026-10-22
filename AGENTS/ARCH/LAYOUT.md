# Source layout

| Path | Contents |
|---|---|
| `include/cps/` | All headers, included as `"cps/Foo.h"` |
| `lib/Lexer/Lexer.cc` | Token scanning and payload-preserving lookahead; `gettok()` consumes and `peekToken`/`peekLine` inspect buffered tokens |
| `lib/Parser/Parser.cc` | Entry point, statement dispatch, assignment parsing and error recovery |
| `lib/Parser/ParserExpression.cc`, `ParserControlFlow.cc` | Expressions/designators and control-flow statement parsing (including ordered CASE branches) |
| `lib/Parser/ParserFunction.cc` | FUNCTION/PROCEDURE/CALL/RETURN + parameter-list parsing |
| `lib/Parser/ParserFile.cc` | OPENFILE/READFILE/WRITEFILE/CLOSEFILE parsing |
| `lib/Parser/ParserType.cc` | Type names, variable/array/constant declarations, TYPE/ENDTYPE and CLASS/ENDCLASS declarations, shared field and method parsing, SET/DEFINE reserved stubs |
| `include/cps/TemplateAST.h`, `lib/Parser/ParserTemplate.cc` | TEMPLATE declarations, type parameter packs and canonical template ids; call and pack parsing also uses ParserExpression.cc/ParserFunction.cc |
| `lib/CodeGen/CodeGen.cc` | Initialization, symbols and main() compilation |
| `lib/CodeGen/CallGen.cc` | Call-argument marshalling (`marshalCallArgs`, variadic arguments) and call resolution (`resolveCall`) |
| `lib/CodeGen/ExpressionGen.cc`, `StatementGen.cc`, `ControlFlowGen.cc` | Expression/statement dispatch and control-flow emission |
| `lib/CodeGen/OperatorGen.cc`, `include/cps/Operator.h` | Exact global/member operator selection, calls and temporary receivers; shared operator spellings |
| `lib/CodeGen/TemplateGen.cc`, `TemplateClone.h`, `TemplateClone.cc` | Template registry validation, cached function/class instantiation, structural type substitution, pack expansion and deferred bodies; call deduction/selection lives in CallGen.cc |
| `lib/CodeGen/ConstructorGen.cc` | Construction, copying, assignment, destruction and deferred object cleanup |
| `lib/CodeGen/GlobalGen.cc` | GLOBAL declarations and scalar/owning STRING initializers |
| `lib/CodeGen/ConstantGen.cc` | Literal constants, declaration conflicts and lexical binding restoration |
| `lib/CodeGen/ValueTypeGen.cc`, `OutputGen.cc`, `Builtins.h` | Expression typing, coercion/declarations, scalar output and the shared builtin table |
| `lib/CodeGen/Designator.cc`, `TypeCheckGen.cc` | TYPE pre-pass, member lookup/calls and lvalue access; type admission and user-kind binary operators |
| `lib/CodeGen/CTypeGen.cc` | CodeGen methods for native C scalar conversions, casts, arithmetic and input/output |
| `lib/CodeGen/NativeTypeSystem.cc` | Builtin registration, native C type imports/aliases and scalar promotion |
| `lib/CodeGen/MemoryHandler.cc` | Explicit raw memory allocation, resizing and release with a live-address tracking list |
| `lib/CodeGen/FileHandler.cc`, `FileRuntimeIO.cc` | File runtime setup/lookup/open and call entry points; read/write/close/EOF helper bodies |
| `lib/CodeGen/ArrayHandler.cc`, `ArrayParameters.cc`, `ArrayOutput.cc` | Array allocation/indexing, parameter binding/arguments and whole/subarray output |
| `lib/CodeGen/*Handler.cc`, `FunctionGen.cc`, `RuntimeCheck.cc`, `TypeSystem.cc` | Helpers owned by CodeGen (including exception emission) |
| `CMakeLists.txt` | 3 static libs (CPSLexer, CPSParser, CPSCodeGen) + `cpsc`. **New .cc files must be added to the source lists here** |
| `tools/driver/NativeCompiler.cc`, `cmake/NativeCompiler.cmake` | Host object generation, embedded Clang/LLD linking and compressed link resources |
| `tools/driver/EmbeddedRunner.cc`, `tools/driver/MinGWJIT.cc`, `cmake/EmbeddedLLVM.cmake` | EMBEDDED-only LLJIT/JITLink runner and MinGW COFF/SEH support; static support-library selection for all modes |
| `tools/driver/PathRunner.cc` | PATH-only `-r` runner: lli lookup, LLVM 23 version check and lli execution |
| `cmake/LocalDependencies.cmake`, `cmake/*Packages.cmake` | Local package extraction and platform relocation |
| `cmake/DependencyPaths.cmake`, `cmake/FindDependency.cmake` | Shared local paths and local-first/system-fallback lookup |
| `cmake/BuildTools.cmake`, `cmake/LLVMDependencies.cmake` | Compiler/build tool selection and LLVM dependency discovery |
| `cmake/Manual.cmake`, `cmake/GenerateManual.cmake.in` | Optional manual generation with local-first Pandoc discovery |
| `install.sh`, `install.ps1`, `cmake/Install.cmake.in`, `cmake/Release.cmake` | Artifact installation and versioned binary release packaging |

AST nodes live in `include/cps/AST.h` (+ `FunctionAST.h`, `FileAST.h`, `TypeAST.h`, `TemplateAST.h`);
they are pure data with virtual destructors only — no codegen/visitor methods.
