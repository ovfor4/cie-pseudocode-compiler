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

    Value *AllocSize = Builder.CreateAdd(ActualLen, One);
    Value *NewStrMem = Builder.CreateCall(MallocFunc, AllocSize, "mid_str_mem");

    Value *SrcPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), Str, StartZeroBased);

    std::vector<Value*> Args = {NewStrMem, SrcPtr, ActualLen};
    Builder.CreateCall(MemCpyFunc, Args);

    Value *NullTermPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStrMem, ActualLen);
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), NullTermPtr);
    
    return NewStrMem;
}

Value *StringHandler::emitRight(Value *Str, Value *Len) {
    Value *FullLen = emitLength(Str);

    Value *StartIdx = Builder.CreateSub(FullLen, Len, "right_start");

    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    Value *IsNeg = Builder.CreateICmpSLT(StartIdx, Zero);
    StartIdx = Builder.CreateSelect(IsNeg, Zero, StartIdx);

    Value *ActualLen = Builder.CreateSub(FullLen, StartIdx);

    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(ActualLen, One);
    Value *NewStrMem = Builder.CreateCall(MallocFunc, AllocSize, "right_str_mem");

    Value *SrcPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), Str, StartIdx);

    std::vector<Value*> Args = {NewStrMem, SrcPtr, ActualLen};
    Builder.CreateCall(MemCpyFunc, Args);

    Value *NullTermPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStrMem, ActualLen);
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), NullTermPtr);
    
    return NewStrMem;
}

Value *StringHandler::emitLeft(Value *Str, Value *Len) {
    Value *FullLen = emitLength(Str);

    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    Value *IsNeg = Builder.CreateICmpSLT(Len, Zero);
    Value *SafeLen = Builder.CreateSelect(IsNeg, Zero, Len);

    Value *IsTooBig = Builder.CreateICmpSGT(SafeLen, FullLen);
    Value *ActualLen = Builder.CreateSelect(IsTooBig, FullLen, SafeLen);

    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(ActualLen, One);
    Value *NewStrMem = Builder.CreateCall(MallocFunc, AllocSize, "left_str_mem");

    std::vector<Value*> Args = {NewStrMem, Str, ActualLen};
    Builder.CreateCall(MemCpyFunc, Args);

    Value *NullTermPtr = Builder.CreateInBoundsGEP(Type::getInt8Ty(Context), NewStrMem, ActualLen);
    Builder.CreateStore(ConstantInt::get(Type::getInt8Ty(Context), 0), NullTermPtr);

    return NewStrMem;
}

Value *StringHandler::emitLCase(Value *Str) {
    Value *Len = emitLength(Str);
    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(Len, One);
    Value *NewStr = Builder.CreateCall(MallocFunc, AllocSize, "lcase_str");

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
    Value *LowerChar = Builder.CreateCall(ToLowerFunc, ExtChar);
    Value *TruncChar = Builder.CreateTrunc(LowerChar, Type::getInt8Ty(Context));

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

Value *StringHandler::emitUCase(Value *Str) {
    Value *Len = emitLength(Str);
    Value *One = ConstantInt::get(Context, APInt(64, 1));
    Value *AllocSize = Builder.CreateAdd(Len, One);
    Value *NewStr = Builder.CreateCall(MallocFunc, AllocSize, "ucase_str");
    
    // Loop
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(Context, "afterloop", TheFunction);
    
    // Init i = 0
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
    Value *UpperChar = Builder.CreateCall(ToUpperFunc, ExtChar);
    Value *TruncChar = Builder.CreateTrunc(UpperChar, Type::getInt8Ty(Context));

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
