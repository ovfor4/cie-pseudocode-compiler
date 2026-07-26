#include "cps/StringHandler.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace cps;

StringHandler::StringHandler(LLVMContext &Ctx, IRBuilder<> &B, llvm::Module &M)
    : Context(Ctx), Builder(B), Module(M) {
    setupExternalFunctions();
}

void StringHandler::setupExternalFunctions() {
    FunctionType *MallocType = FunctionType::get(PointerType::getUnqual(Context), {Type::getInt64Ty(Context)}, false);
    MallocFunc = Module.getOrInsertFunction("malloc", MallocType);

    FunctionType *StrLenType = FunctionType::get(Type::getInt64Ty(Context), {PointerType::getUnqual(Context)}, false);
    StrLenFunc = Module.getOrInsertFunction("strlen", StrLenType);

    std::vector<Type*> MemCpyArgs = {PointerType::getUnqual(Context), PointerType::getUnqual(Context), Type::getInt64Ty(Context)};
    FunctionType *MemCpyType = FunctionType::get(PointerType::getUnqual(Context), MemCpyArgs, false);
    MemCpyFunc = Module.getOrInsertFunction("memcpy", MemCpyType);

    FunctionType *StrCpyType = FunctionType::get(PointerType::getUnqual(Context),
                                                 {PointerType::getUnqual(Context), PointerType::getUnqual(Context)},
                                                 false);
    StrCpyFunc = Module.getOrInsertFunction("strcpy", StrCpyType);
    StrCatFunc = Module.getOrInsertFunction("strcat", StrCpyType);

    FunctionType *CharCaseType = FunctionType::get(Type::getInt32Ty(Context), {Type::getInt32Ty(Context)}, false);
    ToUpperFunc = Module.getOrInsertFunction("toupper", CharCaseType);
    ToLowerFunc = Module.getOrInsertFunction("tolower", CharCaseType);
}

Value *StringHandler::createLiteral(const std::string &Val) {
    return Builder.CreateGlobalStringPtr(Val);
}

Value *StringHandler::emitLength(Value *Str) {
    if (!Str) return nullptr;
    return Builder.CreateCall(StrLenFunc, Str, "len");
}

Value *StringHandler::emitConcat(Value *LHS, Value *RHS) {
    if (!LHS || !RHS) return nullptr;

    Value *LLen = Builder.CreateCall(StrLenFunc, {LHS}, "llen");
    Value *RLen = Builder.CreateCall(StrLenFunc, {RHS}, "rlen");
    Value *TotalLen = Builder.CreateAdd(LLen, RLen, "totallen");
    Value *AllocSize = Builder.CreateAdd(TotalLen, ConstantInt::get(Type::getInt64Ty(Context), 1), "allocsize");

    Value *NewStr = Builder.CreateCall(MallocFunc, {AllocSize}, "concat_str");

    Builder.CreateCall(StrCpyFunc, {NewStr, LHS});
    Builder.CreateCall(StrCatFunc, {NewStr, RHS});

    return NewStr;
}

// malloc(Len + 1), copy Len bytes from Src + StartIdx, NUL-terminate.
// Clamping is each caller's business — MID/RIGHT/LEFT clamp different things.
Value *StringHandler::allocCopySubstring(Value *Src, Value *StartIdx, Value *Len) {
    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(Len, One);
    Value *NewStr = Builder.CreateCall(MallocFunc, AllocSize, "substr_mem");

    Value *SrcPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), Src, StartIdx);
    Builder.CreateCall(MemCpyFunc, {NewStr, SrcPtr, Len});

    Value *NullTermPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStr, Len);
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), NullTermPtr);

    return NewStr;
}

Value *StringHandler::emitMid(Value *Str, Value *Start, Value *Len) {
    Value *FullLen = emitLength(Str);

    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    Value *StartZeroBased = Builder.CreateSub(Start, One, "start_idx");

    Value *IsNeg = Builder.CreateICmpSLT(StartZeroBased, Zero);
    StartZeroBased = Builder.CreateSelect(IsNeg, Zero, StartZeroBased);

    Value *IsTooBig = Builder.CreateICmpSGT(StartZeroBased, FullLen);
    StartZeroBased = Builder.CreateSelect(IsTooBig, FullLen, StartZeroBased);

    Value *RemLen = Builder.CreateSub(FullLen, StartZeroBased);

    Value *IsLenTooBig = Builder.CreateICmpSGT(Len, RemLen);
    Value *ActualLen = Builder.CreateSelect(IsLenTooBig, RemLen, Len);

    Value *IsLenNeg = Builder.CreateICmpSLT(ActualLen, Zero);
    ActualLen = Builder.CreateSelect(IsLenNeg, Zero, ActualLen);

    return allocCopySubstring(Str, StartZeroBased, ActualLen);
}

Value *StringHandler::emitRight(Value *Str, Value *Len) {
    Value *FullLen = emitLength(Str);

    Value *StartIdx = Builder.CreateSub(FullLen, Len, "right_start");

    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    Value *IsNeg = Builder.CreateICmpSLT(StartIdx, Zero);
    StartIdx = Builder.CreateSelect(IsNeg, Zero, StartIdx);

    Value *ActualLen = Builder.CreateSub(FullLen, StartIdx);

    return allocCopySubstring(Str, StartIdx, ActualLen);
}

Value *StringHandler::emitLeft(Value *Str, Value *Len) {
    Value *FullLen = emitLength(Str);

    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    Value *IsNeg = Builder.CreateICmpSLT(Len, Zero);
    Value *SafeLen = Builder.CreateSelect(IsNeg, Zero, Len);

    Value *IsTooBig = Builder.CreateICmpSGT(SafeLen, FullLen);
    Value *ActualLen = Builder.CreateSelect(IsTooBig, FullLen, SafeLen);

    return allocCopySubstring(Str, Zero, ActualLen);
}

// Shared loop for LCASE/UCASE: malloc a copy of Str with every byte run
// through CaseFn (tolower/toupper).
Value *StringHandler::emitCaseConvert(Value *Str, FunctionCallee CaseFn) {
    Value *Len = emitLength(Str);
    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(Len, One);
    Value *NewStr = Builder.CreateCall(MallocFunc, AllocSize, "case_str");

    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(Context, "afterloop", TheFunction);

    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    AllocaInst *IdxVar = TmpB.CreateAlloca(Type::getInt64Ty(Context), nullptr, "idx");
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, 0)), IdxVar);

    Builder.CreateBr(LoopBB);
    Builder.SetInsertPoint(LoopBB);

    Value *CurIdx = Builder.CreateLoad(Type::getInt64Ty(Context), IdxVar);
    Value *Cond = Builder.CreateICmpSLT(CurIdx, Len);

    BasicBlock *BodyBB = BasicBlock::Create(Context, "body", TheFunction);
    Builder.CreateCondBr(Cond, BodyBB, AfterBB);

    Builder.SetInsertPoint(BodyBB);

    Value *SrcPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), Str, CurIdx);
    Value *CharVal = Builder.CreateLoad(Type::getInt8Ty(Context), SrcPtr);

    Value *ExtChar = Builder.CreateSExt(CharVal, Type::getInt32Ty(Context));
    Value *ConvChar = Builder.CreateCall(CaseFn, ExtChar);
    Value *TruncChar = Builder.CreateTrunc(ConvChar, Type::getInt8Ty(Context));

    Value *DestPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStr, CurIdx);
    Builder.CreateStore(TruncChar, DestPtr);

    Value *NextIdx = Builder.CreateAdd(CurIdx, One);
    Builder.CreateStore(NextIdx, IdxVar);
    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(AfterBB);

    Value *NullPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStr, Len);
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), NullPtr);

    return NewStr;
}

Value *StringHandler::emitLCase(Value *Str) {
    return emitCaseConvert(Str, ToLowerFunc);
}

Value *StringHandler::emitUCase(Value *Str) {
    return emitCaseConvert(Str, ToUpperFunc);
}
