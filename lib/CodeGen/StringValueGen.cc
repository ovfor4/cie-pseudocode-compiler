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

// CHAR -> STRING: STRING.NEW(1, ch).
Value *CodeGen::emitStringFromChar(Value *Ch) {
    const TypeInfo *Type = requireStringType();
    if (!Type || !Ch) return nullptr;
    const TypeInfo *CChar = resolveType("C_CHAR");
    const FuncSig *Constructor = nullptr;
    for (const FuncSig *Sig : FuncGen->getOverloads(Type->Name + ".NEW"))
        if (Sig->Params.size() == 2 && !Sig->Params[1].IsArray && !Sig->Params[1].IsByRef &&
            CChar->sameType(resolveType(Sig->Params[1].TypeName)))
            Constructor = Sig;
    if (!Constructor) {
        reportError("STRING requires a constructor NEW(Count : INTEGER, Ch : C_CHAR)");
        return nullptr;
    }
    if (!checkOperatorAccess(*Constructor)) return nullptr;
    auto *Storage = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                          Type->LLVMType, "string_value");
    Builder->CreateStore(Types->getZeroValue(Type->Name), Storage);
    if (!emitFieldDefaults(Type, Storage, DiagnosticLine)) return nullptr;
    Value *Count = ConstantInt::get(resolveType("INTEGER")->LLVMType, 1);
    if (!FuncGen->emitCall(*Constructor, {Storage, Count, Ch})) return nullptr;
    // The consumer takes ownership, just as with an ordinary class result.
    return Builder->CreateLoad(Type->LLVMType, Storage, "string");
}

// STRING -> CHAR: the string must hold exactly one character.
Value *CodeGen::emitCharFromString(ExprAST *Expr, int Line) {
    const TypeInfo *Type = requireStringType();
    if (!Type) return nullptr;
    Value *Text = emitExpr(Expr);
    if (!Text) return nullptr;
    if (!isObjectLValue(Expr)) materializeOperatorReceiver(Text, Type);
    auto *Receiver = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                           Type->LLVMType, "string_view");
    Builder->CreateStore(Text, Receiver);
    const FuncSig *Method = resolveMethod(Type->Name, "_LENGTH", {});
    if (!Method || !resolveType("INTEGER")->sameType(resolveType(Method->ReturnTypeName))) {
        reportError("STRING requires a method _LENGTH() RETURNS INTEGER");
        return nullptr;
    }
    Value *Length = emitMethodCall(Type->Name, Receiver, "_LENGTH", {}, Line);
    if (!Length) return nullptr;
    Value *Wrong = Builder->CreateICmpNE(Length, ConstantInt::get(Length->getType(), 1), "char_len_bad");
    Value *Msg = Builder->CreateGlobalString("[Fatal] line %d: STRING of length %lld cannot be converted to CHAR\n",
                                             "err_string_to_char", 0, TheModule.get());
    RuntimeChecker->emitCheck(Wrong, Msg, {RuntimeChecker->lineValue(Line), Length});
    Value *Ptr = emitMethodCall(Type->Name, Receiver, "C_STR", {}, Line);
    if (!Ptr) return nullptr;
    return Builder->CreateLoad(resolveType("CHAR")->LLVMType, Ptr, "char_value");
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
