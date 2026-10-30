#include "cps/CodeGen.h"

using namespace llvm;
using namespace cps;

namespace {
// Defines (once per module) `void __cps_output_real(double, i1 Newline)`, which prints a REAL in
// the shortest round-trip form of Python's repr: fixed notation (at least one decimal) for
// 1e-4 <= |x| < 1e16, otherwise scientific with a two-digit exponent. The text is
// NUM_TO_STR-identical to _string_impl_from_real in pseudocode-lib/string.cc; keep both in sync.
FunctionCallee getOutputRealHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *VoidTy = Type::getVoidTy(Ctx), *I32 = Type::getInt32Ty(Ctx), *I64 = Type::getInt64Ty(Ctx);
    Type *DoubleTy = Type::getDoubleTy(Ctx), *I1 = Type::getInt1Ty(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(VoidTy, {DoubleTy, I1}, false);
    if (Function *Existing = M.getFunction("__cps_output_real")) return {HelperTy, Existing};

    FunctionCallee Printf = M.getOrInsertFunction("printf", FunctionType::get(I32, {Ptr}, true));
    FunctionCallee Snprintf = M.getOrInsertFunction("snprintf", FunctionType::get(I32, {Ptr, I64, Ptr}, true));
    FunctionCallee Strtod = M.getOrInsertFunction("strtod", FunctionType::get(DoubleTy, {Ptr, Ptr}, false));

    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_output_real", &M);
    Value *X = F->getArg(0), *Newline = F->getArg(1);
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
    BasicBlock *NanBB = BasicBlock::Create(Ctx, "nan", F), *NotNan = BasicBlock::Create(Ctx, "notnan", F);
    BasicBlock *InfBB = BasicBlock::Create(Ctx, "inf", F), *Finite = BasicBlock::Create(Ctx, "finite", F);
    BasicBlock *FixedHead = BasicBlock::Create(Ctx, "fixed.head", F), *FixedBody = BasicBlock::Create(Ctx, "fixed.body", F);
    BasicBlock *SciHead = BasicBlock::Create(Ctx, "sci.head", F), *SciBody = BasicBlock::Create(Ctx, "sci.body", F);
    BasicBlock *Print = BasicBlock::Create(Ctx, "print", F);

    IRBuilder<> B(Entry);
    constexpr uint64_t BufSize = 64;
    Value *Buf = B.CreateAlloca(ArrayType::get(Type::getInt8Ty(Ctx), BufSize), nullptr, "buf");
    auto Str = [&](const char *Text, const char *Name) { return B.CreateGlobalString(Text, Name, 0, &M); };
    Value *SizeV = ConstantInt::get(I64, BufSize);
    Value *NanCheck = B.CreateFCmpUNO(X, X);
    B.CreateCondBr(NanCheck, NanBB, NotNan);

    B.SetInsertPoint(NanBB);
    B.CreateCall(Snprintf, {Buf, SizeV, Str("%s", "fmt_real_s"), Str("nan", "real_nan")});
    B.CreateBr(Print);

    B.SetInsertPoint(NotNan);
    Value *IsNeg = B.CreateFCmpOLT(X, ConstantFP::get(DoubleTy, 0.0));
    Value *Abs = B.CreateSelect(IsNeg, B.CreateFNeg(X), X, "abs");
    B.CreateCondBr(B.CreateFCmpOEQ(Abs, ConstantFP::getInfinity(DoubleTy)), InfBB, Finite);

    B.SetInsertPoint(InfBB);
    B.CreateCall(Snprintf, {Buf, SizeV, Str("%s", "fmt_real_s"),
                            B.CreateSelect(IsNeg, Str("-inf", "real_ninf"), Str("inf", "real_inf"))});
    B.CreateBr(Print);

    B.SetInsertPoint(Finite);
    Value *IsFixed = B.CreateOr(
        B.CreateAnd(B.CreateFCmpOGE(Abs, ConstantFP::get(DoubleTy, 1e-4)),
                    B.CreateFCmpOLT(Abs, ConstantFP::get(DoubleTy, 1e16))),
        B.CreateFCmpOEQ(Abs, ConstantFP::get(DoubleTy, 0.0)));
    B.CreateCondBr(IsFixed, FixedHead, SciHead);

    // Fixed notation: the fewest decimals (at least one) that round-trip.
    B.SetInsertPoint(FixedHead);
    PHINode *Decimals = B.CreatePHI(I32, 2, "decimals");
    Value *FixedFmt = Str("%.*f", "fmt_real_f");
    B.CreateCall(Snprintf, {Buf, SizeV, FixedFmt, Decimals, X});
    Value *FixedBack = B.CreateCall(Strtod, {Buf, ConstantPointerNull::get(Ptr)});
    Value *FixedDone = B.CreateOr(B.CreateFCmpOEQ(FixedBack, X),
                                  B.CreateICmpUGE(Decimals, ConstantInt::get(I32, 40)));
    B.CreateCondBr(FixedDone, Print, FixedBody);
    B.SetInsertPoint(FixedBody);
    Value *NextDecimals = B.CreateAdd(Decimals, ConstantInt::get(I32, 1));
    B.CreateBr(FixedHead);
    Decimals->addIncoming(ConstantInt::get(I32, 1), Finite);
    Decimals->addIncoming(NextDecimals, FixedBody);

    // Scientific notation: the fewest significant digits (1..17) that round-trip.
    B.SetInsertPoint(SciHead);
    PHINode *Precision = B.CreatePHI(I32, 2, "precision");
    Value *SciFmt = Str("%.*e", "fmt_real_e");
    B.CreateCall(Snprintf, {Buf, SizeV, SciFmt, Precision, X});
    Value *SciBack = B.CreateCall(Strtod, {Buf, ConstantPointerNull::get(Ptr)});
    Value *SciDone = B.CreateOr(B.CreateFCmpOEQ(SciBack, X),
                                B.CreateICmpUGE(Precision, ConstantInt::get(I32, 16)));
    B.CreateCondBr(SciDone, Print, SciBody);
    B.SetInsertPoint(SciBody);
    Value *NextPrecision = B.CreateAdd(Precision, ConstantInt::get(I32, 1));
    B.CreateBr(SciHead);
    Precision->addIncoming(ConstantInt::get(I32, 0), Finite);
    Precision->addIncoming(NextPrecision, SciBody);

    B.SetInsertPoint(Print);
    B.CreateCall(Printf, {B.CreateSelect(Newline, Str("%s\n", "fmt_real_out_nl"), Str("%s", "fmt_real_out")), Buf});
    B.CreateRetVoid();
    return {HelperTy, F};
}
}

Value *CodeGen::emitStringNullGuard(Value *StrVal) {
    if (!StrVal || !StrVal->getType()->isPointerTy()) return StrVal;
    Value *IsNull = Builder->CreateICmpEQ(StrVal,
                                          ConstantPointerNull::get(cast<PointerType>(StrVal->getType())),
                                          "str_is_null");
    return Builder->CreateSelect(IsNull, EmptyStringStr, StrVal, "safe_str");
}

void CodeGen::emitOutputValue(Value *Val, const TypeInfo *Info, bool AppendNewline) {
    if (!Val || !Info) return;
    if (Info->CIdentity == "char*") {
        const TypeInfo *String = requireStringType();
        const FuncSig *Constructor = findImplicitConstructor(Info, String);
        if (!Constructor) {
            if (String) reportError("STRING requires an IMPLICIT constructor NEW(Text : C_STRING)");
            return;
        }
        Value *Converted = emitImplicitConstruct(*Constructor, String, Val);
        if (!Converted) return;
        materializeOperatorReceiver(Converted, String);
        emitOutputValue(Converted, String, AppendNewline);
        return;
    }
    if (Info->isString()) {
        Value *Text = emitStringCString(Val);
        if (Text) Builder->CreateCall(PrintfFunc, {
            AppendNewline ? PrintfStringFormatStr
                : Builder->CreateGlobalString("%s", "fmt_str_inline", 0, TheModule.get()), Text});
        return;
    }

    std::vector<Value*> Args;
    Value *Fmt = nullptr;

    // Exhaustive over TypeKind (no default): the compiler warns when a new
    // kind is added without deciding its OUTPUT story.
    switch (Info->Kind) {
    case TypeKind::Real:
        Builder->CreateCall(getOutputRealHelper(*TheModule),
                            {coerceValueToType(Val, resolveType("REAL")), Builder->getInt1(AppendNewline)});
        return;

    case TypeKind::Boolean: {
        Value *BoolVal = coerceValueToType(Val, resolveType("BOOLEAN"));
        Fmt = AppendNewline
            ? PrintfStringFormatStr
            : Builder->CreateGlobalString("%s", "fmt_str_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Builder->CreateSelect(BoolVal, TrueStr, FalseStr));
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
