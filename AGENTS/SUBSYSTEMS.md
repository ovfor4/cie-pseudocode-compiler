# Subsystem notes

Deep-dives on the CodeGen helper subsystems. (Invariants: `CONVENTIONS.md`;
known bugs: `PITFALLS.md`.)

**Arrays** (`ArrayHandler`): allocation/indexing live in `ArrayHandler.cc`,
parameter binding/arguments in `ArrayParameters.cc`, and output in `ArrayOutput.cc`.
A DECLAREd array is one malloc'd flat buffer (memset to
zero), row-major, any rank, any bounds. Bounds and multipliers are *SSA values* cached
in `ArrayMetadata`; `ArrayTable` is scoped per function (`exchangeTable`, driven from
CodeGen's `FunctionDefAST` branch, mirroring the Symbols discipline). Element sizes are
`sizeof` constants (`ElementSizeC`), never host-side numbers. Every access goes through
`emitCheckedIndices` (validates the index count for the caller's semantics, then
per-dimension coerce + `emitIndexCheck`) → `computeFlatIndex` → `loadElement` (load +
the typed value load); the public `emitElementAddress` exposes the same checked
pipeline as an address for the lvalue layer (`Form[i].YearGroup`, `^arr[i]`) — never
hand-roll indexing or element loads. Array parameters: `bindArrayParameter` rebuilds
callee-side metadata from the flattened `ptr + 2R bounds` ABI (BYVAL = allocated buffer copy with independent STRING-bearing elements), `emitArrayArgument` validates and pushes the caller side.
`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch for each expression in an OUTPUT
list, emitting nested print loops (element kinds that are not outputtable, i.e. records/pointers,
are a compile error there). Arrays keep their per-element newlines and dispatch
continues with the next expression; a final array requires no extra newline.

**Functions** (`FunctionGen`): plain LLVM functions, external linkage, exact pseudocode
name unless OVERLOAD appends a canonical parameter list (MAIN uses internal __cps_main). Params are `ParamDecl{name, typeString, mode, array rank/bounds}`. BYVAL args are
copied into entry-block allocas (whole structs for records); BYREF args bind the
incoming pointer directly as the variable's storage; array params are handed to the
binder callback CodeGen injects (`setArrayParamBinder`) so FunctionGen never sees
ArrayHandler. Missing RETURNS defaults the return type to INTEGER; procedures use
"VOID". `emitPrototype` records each function's pseudocode signature in a `FuncSig`
overload-set map (`FunctionGen::getOverloads`, filtered by unit visibility).
`resolveCall` selects exact static parameter types/ranks for OVERLOAD, while a single
ordinary definition keeps implicit conversions. Parameter keys exclude return types,
modes and bounds; equivalent C aliases use CIdentity. Every same-name definition must
be OVERLOAD; duplicate keys fail even across invisible units. FunctionGen tracks
prototype registration separately from source names and indexes signatures by LLVM
link name for RETURN (`getSignatureByLink`). `CodeGen::marshalCallArgs` consumes the
selected FuncSig, and `getExprTypeInfo` resolves the same calls without emitting IR. Top-level prototypes are pre-registered at the start of `compile()` (after the
TYPE pre-pass), so calls may precede definitions; calling a name with no signature is a
compile error (no extern is fabricated). Functions inherit visible GLOBAL storage and previously declared own top-level CONSTANT values, plus imported constants from the start; local constants use the same Symbols table with lexical binding restoration. Body emission is inverted: CodeGen injects a
`StmtEmitter` callback, and must restore its own insert point (and the exchanged
ArrayTable) afterwards.

**Operator overloads** (`OperatorGen.cc`): PrototypeAST retains the operator token;
FunctionGen validates operand count and requires a record/class/enum operand.
Global signatures and left-class methods share exact-type selection, used by
ValueTypeGen and ExpressionGen before builtin operations. Missing candidates leave
builtin rules intact; ties are errors. Member access is checked after selection.
The raw-expression argument-marshalling overload borrows AST pointers for operator
calls; ordinary calls adapt their unique_ptr lists into it. Member receivers use
lvalue storage or a materialized, Alive-tracked temporary. CASE calls the value
entry point with its already captured selector and requires BOOLEAN results.

**EXTERN "C"** (`ExternDeclAST`, `IncludeObjStmtAST`): the parser builds a body-less
`PrototypeAST` (IsExternal, IsVariadic, unnamed `argN` params). `FunctionGen::emitExternPrototype`
validates C types, records a `FuncSig` with `IsExternal`, the declared `FunctionType` and
sext/zext attributes for sub-32-bit integers, and `getOrInsertFunction`s the exact symbol.
Identical redeclarations append a FuncSig per declaring unit (visibility unchanged) and
`resolveCall` takes the first visible one; mismatched signatures are errors.
Calls go through `FunctionGen::emitCall(const FuncSig &, ...)` with that type, so a
runtime declaration of the same libc symbol with another type is harmless.
`CodeGen::emitVariadicArgument` applies C default promotions. The driver (`main.cc`)
resolves INCLUDE_OBJ paths, reads defined global symbols with `llvm::object`
(`collectObjectSymbols`; `"__libc"` uses `collectLibcSymbols` over the embedded runtime's
libc/libm inputs, copied to aligned buffers), rejects missing symbols, and passes objects
to `compileNative`, `runEmbedded` (`addObjectFile`) or lli (`-extra-object=`).
On Windows, `runEmbedded` adds a msvcrt.dll search generator ahead of process symbols.
Nightly CI runs extern-libc and the object-file fixtures in EMBEDDED mode on
Linux, Windows and macOS; PATH execution depends on the external lli runtime.

**User types** (`TypeSystem` + `Designator.cc` + `TypeCheckGen.cc`):
`Designator.cc` owns the type pre-pass and lvalue access; `TypeCheckGen.cc` owns
type admission and user-kind binary operators. TYPE declarations are top-level only
and registered by `CodeGen::runTypePrePass` in two passes — `declareUserType` reserves
every name (records become opaque named structs, so pointer pointees may
forward-reference: `TYPE NodePtr = ^Node` before `TYPE Node`), then
`defineEnum/defineRecord/definePointer` fill payloads in source order (a by-value
record field needs its record complete; inclusion via pointer is always fine). Enum
constants live in a TypeSystem-owned table, resolved in `emitExpr`'s
`VariableExprAST` branch after `Symbols` misses. The lvalue layer
(`emitLValue`/`loadFromLValue`) resolves designator chains to `{address, type name}`;
`emitCoercedExpr` is the single name-level admission gate; `emitUserKindBinaryOp`
intercepts built-in binary operators ahead of ArithmeticHandler (which, like FileHandler, is deliberately ignorant of user kinds). The enum range check after
`E ± INTEGER` is the only site that can produce an invalid ordinal — every other
producer (constants, same-name copies, zero-init, memset-0 arrays) is valid by
construction, which is why 0-based ordinals are load-bearing.

`ClassTypeDeclAST` inherits `RecordTypeDeclAST`: CLASS declarations use the same
two-pass registration, `TypeKind::Record`, `%record.NAME` layout and value semantics.
Shared body/field parsing lives in `ParserType.cc`. TYPE fields always have
`IsPublic = true`; CLASS fields default to public or accept an explicit
PUBLIC/PRIVATE keyword and carry that flag through `defineRecord` into
`RecordFieldInfo`. Both the static
designator type walk and `emitLValue` check private field access against the
current method's owning class, including address-of, array elements and pointer
dereferences. Other instances of the same class are accessible from its methods.

Class ASTs also own their `FunctionDefAST` methods. PrototypeAST retains the owner
and access level; `getLinkName()` supplies the source key `Class.Method`.
FunctionGen appends `(canonical parameter types)` only for OVERLOAD, leaving ordinary
symbols unchanged. CodeGen registers methods separately in
`ClassMethods` overload vectors and pre-registers their prototypes after the type pre-pass.
`resolveMethod` uses `resolveCall`; access is checked on the selected overload.
Constructor/default-construction paths select NEW through this same resolver.
`MemberCallExprAST` keeps an addressable receiver separate from explicit arguments;
`MemberCallStmtAST` discards its result. `emitMemberCall` checks access and prepends
the receiver address to arguments marshalled by the existing `marshalCallArgs`.
The hidden receiver is not part of FuncSig's explicit parameter list.
FunctionGen binds field aliases and `this` to the incoming receiver storage before
binding explicit parameters. CodeGen saves/restores `CurrentClass` around each
function body; unqualified calls still resolve globally. Local declarations and
parameters may shadow fields with a source-line warning; `this.Field` remains
explicit. `this` cannot be redeclared within methods. `TypeInfo::IsClass` distinguishes
classes from records without changing their shared LLVM representation.

`ConstructorGen.cc` emits recursive field construction in declaration order followed
by the ordinary NEW method, sharing `emitMethodCall` access checks, argument
marshalling and exception-aware calls. DECLARE zeroes its original storage on every
execution; arrays loop over all flat elements and default-construct each one.
Constructor argument lists are class-only, and field/array defaults require a
parameterless NEW. Explicit NEW method calls do not reconstruct fields.
`SymbolInfo::IsReference` marks a pointer slot for declared class variables and BYVAL
class parameters; `symbolAddress` loads their current object address for all value,
lvalue, address-of, receiver and BYREF paths. Field aliases, this and BYREF parameters
bind addresses directly. Assignment-only `NewExprAST` uses checked MemoryHandler
allocation (at least one byte for method-only classes), zero initialization and
construction, then rebinds reference slots or copies into other destinations. Ordinary assignment invokes an exact member OPERATOR<- when available, otherwise it copies. Heap objects require explicit keyword DELETE; INHERITS/SUPER remain unsupported.

`ConstructorGen.cc` also owns `typeNeedsDestruction` and recursive `emitDestroy`:
ordinary DELETE method first, then fields in reverse declaration order. Explicit
DELETE method calls remain ordinary calls without field recursion. Keyword DELETE
resolves a class lvalue, validates its allocation start through MemoryHandler before
any destruction, destroys it, then releases it through FREE; aliases remain dangling.
Stack lifetimes use a per-function `StackObjects` list saved/restored around function
emission. DECLARE and BYVAL copies share ownership registration, including arrays and records with class fields. Each record retains the
original address, an entry-initialized Alive flag and, for arrays, a runtime element
count in an entry slot. Array buffer addresses also live in entry slots so deferred
cleanup never references branch-local bound SSA values. Repeated DECLARE destroys
its previous live incarnation first. Alive becomes true only after construction or
copy completes and is cleared before destruction. Rebinding a reference slot with
NEW does not change the original address tracked for automatic destruction.

**Runtime checks** (`RuntimeCheck`): array bounds, file misuse, enum ranges, null
dereferences and array-parameter bounds contracts are checked at their emit sites.
`ArithmeticHandler` calls `emitDivZeroCheck` before `/`, DIV and MOD; the check accepts
integer and floating-point divisors, including both floating-point signed zeros.
`emitCheck(Cond, Msg, Args)` splits the block, prints the source line and error
reason to stderr, and calls `ExceptionHandler::emitThrow` on failure. File helper
checks receive the source line as a runtime argument.

**Exceptions** (`ExceptionHandler`): holds Context/Builder/Module refs and a map of
per-function handler stacks and active catch depths. TRY pushes a catch target and
an entry-block alloca for its exception pointer. A potentially throwing call with
a handler or active catch emits an `invoke` and a landing pad that finishes catches being left and transfers
to the nearest handler, or resumes unwinding when only cleanup is needed. CATCH
uses `__cxa_begin_catch` and removes its own TRY target before emitting its body;
normal exit and RETURN call `__cxa_end_catch`. User functions enable cleanup
support before their first call: loop back edges can make later DECLARE sites alive
at earlier invokes. Without a local handler, cleanup landing pads end active catches
and resume unwinding. Returns (before catch cleanup) and cleanup resumes record exit
points with their handler/catch state; CodeGen fills reverse, Alive-guarded destruction
only when the function is fully emitted. Normal-exit destructor calls use that saved
state and may create further cleanup pads. During unwind filling, a reentrancy guard
emits plain calls; a throwing destructor propagates and interrupts remaining cleanup
instead of terminating. Main records only its normal return, leaving its final
uncaught-exception handler free of automatic object destruction. Bare THROW always creates a new
anonymous integer exception, using `_ZTIi` internally without exposing a value.
Generated main owns the final catch target, prints the generic uncaught message,
and returns 1. Native binaries require the C++ runtime; EmbeddedRunner defines
its statically linked ABI symbols with ORC `absoluteSymbols`, while LLJIT with
JITLink manages unwind frames (generic EH-frame on Linux, compact-unwind or
EH-frame on macOS, COFF SEH registration on Windows x86_64). Nightly CI exercises
embedded exception execution on all three platforms.
Owned STRING values follow class cleanup. Raw array buffers are not freed and files are not automatically closed.

**Files** (`FileHandler`): text-file runtime for §9.1.
`FileHandler.cc` owns setup, lookup/open and emission entry points;
`FileRuntimeIO.cc` builds the read/write/close/EOF helpers. Leaf helper —
holds Context/Builder/Module/RuntimeCheck/ExceptionHandler refs only, never sees
CodeGen, AST nodes or the symbol table; `CodeGen::emitStmtImpl`'s four file branches (and the EOF builtin
intercept) evaluate/coerce operands and hand over `llvm::Value*`s plus the statement
line. Calls into throwing helpers go through ExceptionHandler. On first file use,
`ensureRuntime()` synthesizes the whole runtime once — the
file module-level helpers: internal-linkage `__cps_file_find/
open/read/write/close/eof` plus a 16-slot open-file table (`__cps_file_names/handles/
modes` parallel globals; mode byte 0 = free). Files are keyed by filename *content*
(strcmp scan), consistent with STRING comparison; the table owns a copied filename until CLOSEFILE, independently of temporary STRING lifetimes. EOF is an fgetc/ungetc peek; READFILE grows a
malloc/realloc buffer per line and strips one trailing `\r`; fopen modes are text
`"r"/"w"/"a"` (Windows CRT translates CRLF). File-free programs get byte-identical IR —
nothing is declared or emitted eagerly. Helper bodies are built with the *shared*
Builder (RuntimeCheck emits through the same one), saving/restoring the insert block.

**Explicit memory** (`MemoryHandler`): lazily emits internal allocation/find/release/resize helpers and a linked list of live allocation start addresses. Explicit MALLOC/REALLOC and keyword NEW allocations enter the list; STRING/ARRAY internals retain their existing ownership. `emitValidate` always calls the find helper, including for null, so keyword DELETE validates before running destructors. FREE validates non-null addresses, unlinks and frees the tracking node, releases the block and returns the original address. Zero-byte allocation returns null. REALLOC validates non-null addresses before its zero-size release path; null behaves as MALLOC. Positive-size resizing updates the node only after success, leaving the old block and record intact on failure. Node allocation precedes the user allocation; if the latter fails, its node is freed before the diagnostic and anonymous throw. Helpers use RuntimeCheck and ExceptionHandler, including source-line arguments and per-function unwind preparation. Sizes accept INTEGER and C integers and convert to native size_t. Address reuse can make stale aliases indistinguishable from current allocations; dereferences are not checked against this list.

**Class copies and temporaries.** Callers construct BYVAL class lvalue arguments with a same-class BYREF OVERLOAD NEW when defined, otherwise they copy bits; the callee's RegisterCopy hook takes ownership. emitOwnedValue constructs lvalue copies and keeps pending arguments Alive until marshalling succeeds. RETURN copies lvalues and transfers rvalues. Records and BYVAL arrays recursively clone STRING-bearing fields/elements. Other owning class fields retain the existing shallow-copy limits. StackObject::Temporary uses statement cleanup and deferred temporary-only cleanup before local handlers, while ordinary objects remain function-scoped. Function-result method receivers are materialized by the same path as operator receivers. Assignment first selects an exact member OPERATOR<-; without one, STRING-bearing aggregate assignment clones string fields from lvalues or transfers them from rvalues, releasing the target's old string fields without invoking the enclosing class destructor; other classes retain their existing bitwise assignment behavior.

**Standard library** (`pseudocode-lib/`): the default build compiles each .cc with the host compiler into build/pseudocode-lib/*.o, copies .cps units beside them, and embeds all these outputs in cpsc. StandardLibrary extracts the resources through linking/execution; the driver implicitly imports std.cps unless -no-std is set. std.cps imports string.cps. STRING is a #LIB class with a public internal void* handle owning std::string. The _string_impl_* C ABI reports __cps_user_line and throws 0 on std::exception. Functions/operators use ordinary resolution; StringValueGen bridges literals, C_STR, C conversions and file operands. C_STRING literals have an adjacent uppercase C suffix. INPUT delegates to READ_INPUT, OUTPUT uses C stdout buffers, and READFILE invokes assignment from C_STRING then frees its temporary buffer. Source/object resources are embedded in installed/released cpsc; saved IR requires the object when manually linked.
