#include "cps/StringConversionHandler.h"
#include <limits>

using namespace llvm;
using namespace cps;

StringConversionHandler::StringConversionHandler(LLVMContext &Ctx, IRBuilder<> &B, llvm::Module &M)
    : Context(Ctx), Builder(B), Module(M) {
    setupExternalFunctions();
}

void StringConversionHandler::setupExternalFunctions() {
    FunctionType *SnprintfType = FunctionType::get(Type::getInt32Ty(Context), {PointerType::getUnqual(Context), Type::getInt64Ty(Context), PointerType::getUnqual(Context)}, true);
    SnprintfFunc = Module.getOrInsertFunction("snprintf", SnprintfType);

    FunctionType *StrtollType = FunctionType::get(Type::getInt64Ty(Context), {PointerType::getUnqual(Context), PointerType::getUnqual(Context), Type::getInt32Ty(Context)}, false);
    StrtollFunc = Module.getOrInsertFunction("strtoll", StrtollType);

    FunctionType *StrtodType = FunctionType::get(Type::getDoubleTy(Context), {PointerType::getUnqual(Context), PointerType::getUnqual(Context)}, false);
    StrtodFunc = Module.getOrInsertFunction("strtod", StrtodType);

    FunctionType *MallocType = FunctionType::get(PointerType::getUnqual(Context), {Type::getInt64Ty(Context)}, false);
    MallocFunc = Module.getOrInsertFunction("malloc", MallocType);
}

Value *StringConversionHandler::emitNumToStr(Value *Num, bool IsReal) {
    // Allow an extra integer digit, sign, decimal point, six fractional digits and NUL.
    Value *AllocSize = ConstantInt::get(Type::getInt64Ty(Context),
                                       IsReal ? std::numeric_limits<double>::max_exponent10 + 10 : 64);
    Value *Buffer = Builder.CreateCall(MallocFunc, AllocSize, "num_str_buf");

    Value *FormatStr;
    if (IsReal) {
        FormatStr = Builder.CreateGlobalString("%f");
    } else {
        FormatStr = Builder.CreateGlobalString("%lld");
    }

    Builder.CreateCall(SnprintfFunc, {Buffer, AllocSize, FormatStr, Num});
    return Buffer;
}

Value *StringConversionHandler::emitStrToNum(Value *Str, bool AsReal) {
    Value *NullPtr = ConstantPointerNull::get(PointerType::getUnqual(Context));
    if (AsReal) {
        return Builder.CreateCall(StrtodFunc, {Str, NullPtr}, "str_to_real");
    } else {
        Value *Base = ConstantInt::get(Type::getInt32Ty(Context), 10);
        return Builder.CreateCall(StrtollFunc, {Str, NullPtr, Base}, "str_to_int");
    }
}

Value *StringConversionHandler::emitIsNum(Value *Str) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *EndPtrAlloc = TmpB.CreateAlloca(PointerType::getUnqual(Context), nullptr, "endptr_alloc");
    
    Builder.CreateCall(StrtodFunc, {Str, EndPtrAlloc});
    Value *EndPtrVal = Builder.CreateLoad(PointerType::getUnqual(Context), EndPtrAlloc, "endptr_val");
    
    FunctionType *StrLenType = FunctionType::get(Type::getInt64Ty(Context), {PointerType::getUnqual(Context)}, false);
    FunctionCallee StrLenFunc = Module.getOrInsertFunction("strlen", StrLenType);
    Value *Len = Builder.CreateCall(StrLenFunc, Str, "str_len");
    
    Value *ExpectedEndPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), Str, Len);
    
    Value *IsMatch = Builder.CreateICmpEQ(EndPtrVal, ExpectedEndPtr, "is_full_match");
    
    Value *Zero = ConstantInt::get(Type::getInt64Ty(Context), 0);
    Value *IsNotEmpty = Builder.CreateICmpSGT(Len, Zero, "is_not_empty");
    
    return Builder.CreateAnd(IsMatch, IsNotEmpty, "is_num_bool");
}
