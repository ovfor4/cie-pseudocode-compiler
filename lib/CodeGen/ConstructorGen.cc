#include "cps/CodeGen.h"
#include "cps/MemoryHandler.h"
#include "cps/TypeAST.h"
#include <climits>
#include <cstddef>

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

Value *CodeGen::symbolAddress(const SymbolInfo &Info) {
    if (!Info.IsReference) return Info.Storage;
    return Builder->CreateLoad(PointerType::getUnqual(*TheContext), Info.Storage, "object_address");
}

void CodeGen::emitAssignment(const LValueInfo &Target, ExprAST *Expr, const SymbolInfo *Symbol) {
    const TypeInfo *Info = resolveType(Target.TypeName);
    if (auto *New = dynamic_cast<NewExprAST*>(Expr)) {
        SaveAndRestore<int> Location(DiagnosticLine, New->getLine());
        const TypeInfo *Class = resolveType(New->getClassName());
        if (!Class || !Class->IsClass) {
            reportError("NEW requires a class type: %s", New->getClassName().c_str());
            return;
        }
        if (!Info || !Info->sameType(Class)) {
            reportError("NEW target must have type %s", Class->Name.c_str());
            return;
        }
        Value *Size = Builder->CreateIntCast(Types->getSizeOfConstant(Class->LLVMType),
                         Type::getIntNTy(*TheContext, sizeof(std::size_t) * CHAR_BIT), false);
        // Method-only classes have zero-sized LLVM structs but still need
        // a non-null object address (MALLOC(0) deliberately returns null).
        Size = Builder->CreateSelect(Builder->CreateICmpEQ(Size, ConstantInt::get(Size->getType(), 0)),
                                      ConstantInt::get(Size->getType(), 1), Size);
        Value *Object = Memory->emitAllocate(Size, New->getLine());
        Builder->CreateStore(Types->getZeroValue(Class->Name), Object);
        if (!emitConstruct(Class->Name, Object, New->getArgs(), New->getLine())) return;
        if (Symbol && Symbol->IsReference) {
            Builder->CreateStore(Object, Symbol->Storage);
        } else {
            Value *Val = Builder->CreateLoad(Class->LLVMType, Object);
            Builder->CreateStore(Val, Target.Addr);
        }
        return;
    }
    Value *Val = emitCoercedExpr(Expr, Info);
    if (Val) Builder->CreateStore(Val, Target.Addr);
}
