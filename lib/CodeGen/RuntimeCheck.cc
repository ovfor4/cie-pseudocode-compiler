#include "cps/RuntimeCheck.h"
#include "cps/ExceptionHandler.h"
#include "cps/TypeSystem.h" // kEnumOrdinalBase
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace cps;

RuntimeCheck::RuntimeCheck(LLVMContext &C, IRBuilder<> &B, ExceptionHandler &EH)
    : TheContext(C), Builder(B), Exceptions(EH) {}

void RuntimeCheck::emitCheck(Value *Condition) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();
    BasicBlock *FailBB = BasicBlock::Create(TheContext, "check_fail", TheFunction);
    BasicBlock *ContBB = BasicBlock::Create(TheContext, "check_cont", TheFunction);
    Builder.CreateCondBr(Condition, FailBB, ContBB);
    Builder.SetInsertPoint(FailBB);
    Exceptions.emitThrow();
    Builder.SetInsertPoint(ContBB);
}

void RuntimeCheck::emitDivZeroCheck(Value *Divisor, int Line) {
    Value *IsZero;
    if (Divisor->getType()->isDoubleTy()) {
        IsZero = Builder.CreateFCmpOEQ(Divisor, ConstantFP::get(Divisor->getType(), 0.0), "is_zero");
    } else {
        IsZero = Builder.CreateICmpEQ(Divisor, ConstantInt::get(Divisor->getType(), 0), "is_zero");
    }
    emitCheck(IsZero);
}

void RuntimeCheck::emitIndexCheck(Value *Index, Value *Lower, Value *Upper, int Line) {
    Value *TooLow = Builder.CreateICmpSLT(Index, Lower, "too_low");
    Value *TooHigh = Builder.CreateICmpSGT(Index, Upper, "too_high");
    Value *OutOfBounds = Builder.CreateOr(TooLow, TooHigh, "out_of_bounds");

    emitCheck(OutOfBounds);
}

void RuntimeCheck::emitEnumRangeCheck(Value *Ordinal, uint64_t ValueCount,
                                      const std::string &TypeName, int Line) {
    Value *Lo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(kEnumOrdinalBase), true));
    Value *Hi = ConstantInt::get(TheContext,
                                 APInt(64, static_cast<uint64_t>(kEnumOrdinalBase) + ValueCount, true));
    Value *TooLow = Builder.CreateICmpSLT(Ordinal, Lo, "enum_too_low");
    Value *TooHigh = Builder.CreateICmpSGE(Ordinal, Hi, "enum_too_high");
    Value *OutOfRange = Builder.CreateOr(TooLow, TooHigh, "enum_out_of_range");

    emitCheck(OutOfRange);
}

void RuntimeCheck::emitNullDerefCheck(Value *Ptr, int Line) {
    Value *IsNull = Builder.CreateICmpEQ(Ptr,
                                         ConstantPointerNull::get(PointerType::getUnqual(TheContext)),
                                         "ptr_is_null");
    emitCheck(IsNull);
}

void RuntimeCheck::emitArrayArgBoundsCheck(Value *ActualLower, Value *ActualUpper,
                                           int64_t DeclaredLower, int64_t DeclaredUpper,
                                           int Line) {
    Value *DeclLo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredLower), true));
    Value *DeclHi = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredUpper), true));
    Value *LoDiff = Builder.CreateICmpNE(ActualLower, DeclLo, "arg_lo_ne");
    Value *HiDiff = Builder.CreateICmpNE(ActualUpper, DeclHi, "arg_hi_ne");
    Value *Mismatch = Builder.CreateOr(LoDiff, HiDiff, "arg_bounds_ne");

    emitCheck(Mismatch);
}
