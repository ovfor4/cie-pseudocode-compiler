#include "cps/CodeGen.h"

using namespace llvm;
using namespace cps;

bool CodeGen::typeNeedsConstruction(const std::string &TypeName) const {
    if (findMethod(TypeName, "NEW")) return true;
    const TypeInfo *Info = resolveType(TypeName);
    if (Info && Info->asRecord()) {
        for (const auto &[Name, Field] : Info->Record.Fields)
            if (typeNeedsConstruction(Field.TypeName)) return true;
    }
    return false;
}

bool CodeGen::emitConstruct(const std::string &TypeName, Value *Addr,
                            const std::vector<std::unique_ptr<ExprAST>> &Args, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    const TypeInfo *Info = resolveType(TypeName);
    if (!Info) return false;
    if (Info->asRecord()) {
        for (const auto &[Name, Field] : Info->Record.Fields) {
            if (!typeNeedsConstruction(Field.TypeName)) continue;
            Value *FieldAddr = Builder->CreateStructGEP(Info->LLVMType, Addr, Field.Index);
            if (!emitDefaultConstruct(Field.TypeName, FieldAddr, Line)) return false;
        }
    }
    if (findMethod(TypeName, "NEW"))
        return emitMethodCall(TypeName, Addr, "NEW", Args, Line) != nullptr;
    if (!Args.empty()) {
        reportError("Type %s has no constructor NEW but arguments were supplied", TypeName.c_str());
        return false;
    }
    return true;
}

bool CodeGen::emitDefaultConstruct(const std::string &TypeName, Value *Addr, int Line) {
    return emitConstruct(TypeName, Addr, {}, Line);
}
