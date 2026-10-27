#include "cps/CodeGen.h"

using namespace llvm;
using namespace cps;

// Keyword NEW/DELETE use the same visible C helpers as memory.cps. Ordinary
// MALLOC/REALLOC/FREE calls are resolved entirely through the library.
Value *CodeGen::emitMemoryCall(const char *Name, Value *Argument, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    auto Declarations = FuncGen->getOverloads(Name);
    if (Declarations.empty()) {
        reportError("NEW/DELETE requires memory.cps; include the memory library");
        return nullptr;
    }
    const FuncSig &Sig = *Declarations.front();
    bool Allocate = sameIdentifier(Name, "_memory_impl_malloc");
    bool Validate = sameIdentifier(Name, "_memory_impl_validate");
    const TypeInfo *Parameter = resolveType(Allocate ? "C_TYPE(\"unsigned long long\")"
                                                    : "C_TYPE(\"void*\")");
    const TypeInfo *Result = resolveType(Validate ? "VOID" : "C_TYPE(\"void*\")");
    if (!Sig.IsExternal || Sig.IsVariadic || Sig.Params.size() != 1 ||
        Sig.Params[0].IsArray || Sig.Params[0].IsByRef ||
        !Types->sameType(Sig.Params[0].TypeName, Parameter->Name) ||
        !Types->sameType(Sig.ReturnTypeName, Result->Name)) {
        reportError("Invalid memory runtime declaration for %s", Name);
        return nullptr;
    }
    if (Allocate) Argument = Builder->CreateIntCast(Argument, Parameter->LLVMType, false);
    return FuncGen->emitCall(Sig, {Argument});
}
