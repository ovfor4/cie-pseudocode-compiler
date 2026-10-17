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
the STRING null-guard); the public `emitElementAddress` exposes the same checked
pipeline as an address for the lvalue layer (`Form[i].YearGroup`, `^arr[i]`) — never
hand-roll indexing or element loads. Array parameters: `bindArrayParameter` rebuilds
callee-side metadata from the flattened `ptr + 2R bounds` ABI (BYVAL = malloc+memcpy
whole copy), `emitArrayArgument` validates and pushes the caller side.
`OUTPUT arr` (and partially-indexed accesses) is intercepted by
`ArrayHandler::tryEmitArrayOutput` — the first branch for each expression in an OUTPUT
list, emitting nested print loops (element kinds that are not outputtable, i.e. records/pointers,
are a compile error there). Arrays keep their per-element newlines and dispatch
continues with the next expression; a final array requires no extra newline.

**Functions** (`FunctionGen`): plain LLVM functions, external linkage, exact pseudocode
name. Params are `ParamDecl{name, typeString, mode, array rank/bounds}`. BYVAL args are
copied into entry-block allocas (whole structs for records); BYREF args bind the
incoming pointer directly as the variable's storage; array params are handed to the
binder callback CodeGen injects (`setArrayParamBinder`) so FunctionGen never sees
ArrayHandler. Missing RETURNS defaults the return type to INTEGER; procedures use
"VOID". `emitPrototype` records each function's pseudocode signature in a `FuncSig`
map (`FunctionGen::getSignature`); `CodeGen::marshalCallArgs` dispatches call arguments
on those *declared* `ParamSig`s, and `getExprTypeInfo` reads return types from the same
map. Top-level prototypes are pre-registered at the start of `compile()` (after the
TYPE pre-pass), so calls may precede definitions; calling a name with no signature is a
compile error (no extern is fabricated). Functions inherit previously declared top-level CONSTANT values; local constants use the same Symbols table with lexical binding restoration. Body emission is inverted: CodeGen injects a
`StmtEmitter` callback, and must restore its own insert point (and the exchanged
ArrayTable) afterwards.

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
intercepts binary operators ahead of ArithmeticHandler (which, like StringHandler and
FileHandler, is deliberately ignorant of user kinds). The enum range check after
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
and access level; `getLinkName()` qualifies method symbols as `Class.Method`, while
global function names remain unmangled. CodeGen registers methods separately in
`ClassMethods` and pre-registers their prototypes after the type pre-pass.
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
construction, then rebinds reference slots or copies into other destinations. Ordinary assignment always copies. Heap objects require explicit keyword DELETE; INHERITS/SUPER remain unsupported.

`ConstructorGen.cc` also owns `typeNeedsDestruction` and recursive `emitDestroy`:
ordinary DELETE method first, then fields in reverse declaration order. Explicit
DELETE method calls remain ordinary calls without field recursion. Keyword DELETE
resolves a class lvalue, validates its allocation start through MemoryHandler before
any destruction, destroys it, then releases it through FREE; aliases remain dangling.
Stack lifetimes use a per-function `StackObjects` list saved/restored around function
emission. DECLARE and shallow BYVAL copies share registration (the copy point is
marked), including arrays and records with class fields. Each record retains the
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
reason to stdout, and calls `ExceptionHandler::emitThrow` on failure. File helper
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
EH-frame on macOS, COFF SEH registration on Windows x86_64). Linux embedded
execution is verified; Windows/macOS await CI verification.
This does not change heap-string/array lifetimes or automatically close files.

**Files** (`FileHandler`): text-file runtime for §9.1.
`FileHandler.cc` owns setup, lookup/open and emission entry points;
`FileRuntimeIO.cc` builds the read/write/close/EOF helpers. StringHandler-shaped leaf —
holds Context/Builder/Module/RuntimeCheck/ExceptionHandler refs only, never sees
CodeGen, AST nodes or the symbol table; `CodeGen::emitStmt`'s four file branches (and the EOF builtin
intercept) evaluate/coerce operands and hand over `llvm::Value*`s plus the statement
line. Calls into throwing helpers go through ExceptionHandler. On first file use,
`ensureRuntime()` synthesizes the whole runtime once — the
file module-level helpers: internal-linkage `__cps_file_find/
open/read/write/close/eof` plus a 16-slot open-file table (`__cps_file_names/handles/
modes` parallel globals; mode byte 0 = free). Files are keyed by filename *content*
(strcmp scan), consistent with STRING comparison; the name pointer is stored as-is
(strings are immutable and never freed). EOF is an fgetc/ungetc peek; READFILE grows a
malloc/realloc buffer per line and strips one trailing `\r`; fopen modes are text
`"r"/"w"/"a"` (Windows CRT translates CRLF). File-free programs get byte-identical IR —
nothing is declared or emitted eagerly. Helper bodies are built with the *shared*
Builder (RuntimeCheck emits through the same one), saving/restoring the insert block.

**Explicit memory** (`MemoryHandler`): lazily emits internal allocation/find/release/resize helpers and a linked list of live allocation start addresses. Explicit MALLOC/REALLOC and keyword NEW allocations enter the list; STRING/ARRAY internals retain their existing ownership. `emitValidate` always calls the find helper, including for null, so keyword DELETE validates before running destructors. FREE validates non-null addresses, unlinks and frees the tracking node, releases the block and returns the original address. Zero-byte allocation returns null. REALLOC validates non-null addresses before its zero-size release path; null behaves as MALLOC. Positive-size resizing updates the node only after success, leaving the old block and record intact on failure. Node allocation precedes the user allocation; if the latter fails, its node is freed before the diagnostic and anonymous throw. Helpers use RuntimeCheck and ExceptionHandler, including source-line arguments and per-function unwind preparation. Sizes accept INTEGER and C integers and convert to native size_t. Address reuse can make stale aliases indistinguishable from current allocations; dereferences are not checked against this list.
