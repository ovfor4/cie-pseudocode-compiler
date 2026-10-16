#include "cps/CodeGen.h"

using namespace llvm;
using namespace cps;

Value *CodeGen::emitStringNullGuard(Value *StrVal) {
    if (!StrVal || !StrVal->getType()->isPointerTy()) return StrVal;
    Value *IsNull = Builder->CreateICmpEQ(StrVal,
                                          ConstantPointerNull::get(cast<PointerType>(StrVal->getType())),
                                          "str_is_null");
    return Builder->CreateSelect(IsNull, EmptyStringStr, StrVal, "safe_str");
}

void CodeGen::emitOutputValue(Value *Val, const TypeInfo *Info, bool AppendNewline) {
    if (!Val || !Info) return;

    std::vector<Value*> Args;
    Value *Fmt = nullptr;

    // Exhaustive over TypeKind (no default): the compiler warns when a new
    // kind is added without deciding its OUTPUT story.
    switch (Info->Kind) {
    case TypeKind::Real:
        Fmt = AppendNewline
            ? PrintfFloatFormatStr
            : Builder->CreateGlobalString("%f", "fmt_flt_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Val);
        break;

    case TypeKind::Boolean: {
        Value *BoolVal = coerceValueToType(Val, resolveType("BOOLEAN"));
        Fmt = AppendNewline
            ? PrintfStringFormatStr
            : Builder->CreateGlobalString("%s", "fmt_str_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Builder->CreateSelect(BoolVal, TrueStr, FalseStr));
        break;
    }

    case TypeKind::String: {
        Value *StrVal = emitStringNullGuard(Val);
        Fmt = AppendNewline
            ? PrintfStringFormatStr
            : Builder->CreateGlobalString("%s", "fmt_str_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(StrVal);
        break;
    }

    case TypeKind::Char: {
        Value *CharVal = coerceValueToType(Val, resolveType("CHAR"));
        Value *Promoted = Builder->CreateZExt(CharVal, Type::getInt32Ty(*TheContext), "char_for_printf");
        Fmt = AppendNewline
            ? PrintfCharFormatStr
            : Builder->CreateGlobalString("%c", "fmt_chr_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Promoted);
        break;
    }

    case TypeKind::Integer:
    case TypeKind::Enum: { // an enum prints its ordinal (user decision)
        Value *IntVal = Info->isEnum() ? Val : coerceValueToType(Val, resolveType("INTEGER"));
        Fmt = AppendNewline
            ? PrintfFormatStr
            : Builder->CreateGlobalString("%lld", "fmt_int_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(IntVal);
        break;
    }

    case TypeKind::Record:
        reportError("Cannot OUTPUT a value of record type %s; output its fields individually",
                    Info->Name.c_str());
        return;

    case TypeKind::Pointer:
        reportError("Cannot OUTPUT a value of pointer type %s", Info->Name.c_str());
        return;

    case TypeKind::CScalar:
        emitCOutput(Val, Info, AppendNewline);
        return;
    case TypeKind::CPointer:
        reportError("OUTPUT is not supported for type %s", Info->Name.c_str());
        return;

    case TypeKind::Void:
        reportError("Cannot OUTPUT a VOID value");
        return;
    }

    Builder->CreateCall(PrintfFunc, Args);
}
