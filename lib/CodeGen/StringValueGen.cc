#include "cps/CodeGen.h"
#include "cps/Operator.h"

using namespace cps;
using namespace llvm;

const TypeInfo *CodeGen::requireStringType() {
    const TypeInfo *Type = resolveType("STRING");
    if (!Type || !Type->IsClass) {
        reportError("String literals require a visible STRING class; include the string library or omit -no-std");
        return nullptr;
    }
    return Type;
}

Value *CodeGen::emitStringFromCString(Value *Text) {
    const TypeInfo *Type = requireStringType();
    if (!Type || !Text) return nullptr;
    const FuncSig *Constructor = nullptr;
    for (const FuncSig *Sig : FuncGen->getOverloads(Type->Name + ".NEW"))
        if (Sig->Params.size() == 1 && !Sig->Params[0].IsArray && !Sig->Params[0].IsByRef &&
            resolveType("C_STRING")->sameType(resolveType(Sig->Params[0].TypeName)))
            Constructor = Sig;
    if (!Constructor) {
        reportError("STRING requires a constructor NEW(Text : C_STRING)");
        return nullptr;
    }
    if (!checkOperatorAccess(*Constructor)) return nullptr;
    auto *Storage = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                          Type->LLVMType, "string_value");
    Builder->CreateStore(Types->getZeroValue(Type->Name), Storage);
    if (!emitFieldDefaults(Type, Storage, DiagnosticLine)) return nullptr;
    if (!FuncGen->emitCall(*Constructor, {Storage, Text})) return nullptr;
    // The consumer takes ownership, just as with an ordinary class result.
    return Builder->CreateLoad(Type->LLVMType, Storage, "string");
}

Value *CodeGen::emitStringCString(Value *Value) {
    const TypeInfo *Type = requireStringType();
    if (!Type || !Value) return nullptr;
    const FuncSig *Method = resolveMethod(Type->Name, "C_STR", {});
    if (!Method || !resolveType("C_STRING")->sameType(resolveType(Method->ReturnTypeName))) {
        reportError("STRING requires a method C_STR() RETURNS C_STRING");
        return nullptr;
    }
    auto *Receiver = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                           Type->LLVMType, "string_view");
    Builder->CreateStore(Value, Receiver);
    return emitMethodCall(Type->Name, Receiver, "C_STR", {}, DiagnosticLine);
}

Value *CodeGen::emitCStringExpr(ExprAST *Expr) {
    const TypeInfo *Type = getExprTypeInfo(Expr);
    if (!Type || !Type->isString() || isWholeArrayVar(Expr)) {
        reportError("Expected a STRING value");
        return nullptr;
    }
    Value *Value = emitExpr(Expr);
    if (!Value) return nullptr;
    if (!isObjectLValue(Expr)) materializeOperatorReceiver(Value, Type);
    return emitStringCString(Value);
}

bool CodeGen::containsString(const TypeInfo *Type) const {
    if (!Type) return false;
    if (Type->isString()) return true;
    if (Type->asRecord())
        for (const auto &[Name, Field] : Type->Record.Fields)
            if (containsString(resolveType(Field.TypeName))) return true;
    return false;
}
