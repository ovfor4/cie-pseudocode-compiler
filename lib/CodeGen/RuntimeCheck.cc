#include "cps/RuntimeCheck.h"
#include "cps/ExceptionHandler.h"
#include "cps/TypeSystem.h" // kEnumOrdinalBase
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"

using namespace llvm;
using namespace cps;

RuntimeCheck::RuntimeCheck(Module &M, LLVMContext &C, IRBuilder<> &B, ExceptionHandler &EH)
    : TheModule(M), TheContext(C), Builder(B), Exceptions(EH) {
    UserLine = new GlobalVariable(TheModule, Builder.getInt32Ty(), false,
                                  GlobalValue::ExternalLinkage, Builder.getInt32(0), "__cps_user_line");
    setupExternalFunctions();
}

Value *RuntimeCheck::lineValue(int Line) {
    return LibraryLookup && LibraryLookup(Line)
        ? Builder.CreateLoad(Builder.getInt32Ty(), UserLine, "user_line")
        : static_cast<Value *>(Builder.getInt32(Line));
}

void RuntimeCheck::setUserLine(int Line) {
    Builder.CreateStore(Builder.getInt32(Line), UserLine);
}

void RuntimeCheck::setupExternalFunctions() {
    Type *PtrTy = PointerType::getUnqual(TheContext);
    Type *I32 = Type::getInt32Ty(TheContext);
    FprintfFunc = TheModule.getOrInsertFunction(
        "fprintf", FunctionType::get(I32, {PtrTy, PtrTy}, true));
    // The stderr FILE* is a platform-specific symbol; host and target match.
#if defined(_WIN32)
    StderrFunc = TheModule.getOrInsertFunction(
        "__acrt_iob_func", FunctionType::get(PtrTy, {I32}, false));
#else
#if defined(__APPLE__)
    const char *StderrName = "__stderrp";
#else
    const char *StderrName = "stderr";
#endif
    StderrVar = new GlobalVariable(TheModule, PtrTy, false, GlobalValue::ExternalLinkage,
                                   nullptr, StderrName);
#endif

    DivZeroMsg = Builder.CreateGlobalString("[Fatal] line %d: Division by zero\n", "err_div_zero", 0, &TheModule);
    OutOfBoundsMsg = Builder.CreateGlobalString("[Fatal] line %d: Array index out of bounds\n", "err_bounds", 0, &TheModule);
    EnumRangeMsg = Builder.CreateGlobalString("[Fatal] line %d: Value out of range for enum %s\n", "err_enum_range", 0, &TheModule);
    NullDerefMsg = Builder.CreateGlobalString("[Fatal] line %d: Dereference of an unset pointer\n", "err_null_deref", 0, &TheModule);
    ArrayArgBoundsMsg = Builder.CreateGlobalString(
        "[Fatal] line %d: Array argument bounds [%lld:%lld] do not match the declared parameter bounds [%lld:%lld]\n",
        "err_arr_arg_bounds", 0, &TheModule);
}

void RuntimeCheck::emitStderr(Value *Msg, ArrayRef<Value *> FmtArgs) {
#if defined(_WIN32)
    Value *Stream = Builder.CreateCall(StderrFunc, {ConstantInt::get(Type::getInt32Ty(TheContext), 2)});
#else
    Value *Stream = Builder.CreateLoad(PointerType::getUnqual(TheContext), StderrVar);
#endif
    std::vector<Value *> Args = {Stream, Msg};
    Args.insert(Args.end(), FmtArgs.begin(), FmtArgs.end());
    Builder.CreateCall(FprintfFunc, Args);
}

void RuntimeCheck::emitCheck(Value *Condition, Value *Msg, int Line) {
    emitCheck(Condition, Msg, {lineValue(Line)});
}

void RuntimeCheck::emitCheck(Value *Condition, Value *Msg, ArrayRef<Value *> FmtArgs) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    BasicBlock *FailBB = BasicBlock::Create(TheContext, "check_fail", TheFunction);
    BasicBlock *ContBB = BasicBlock::Create(TheContext, "check_cont", TheFunction);

    Builder.CreateCondBr(Condition, FailBB, ContBB);

    Builder.SetInsertPoint(FailBB);
    emitStderr(Msg, FmtArgs);

    Exceptions.emitThrow();

    Builder.SetInsertPoint(ContBB);
}

void RuntimeCheck::emitDivZeroCheck(Value *Divisor, int Line) {
    Value *IsZero;
    if (Divisor->getType()->isFloatingPointTy()) {
        IsZero = Builder.CreateFCmpOEQ(Divisor, ConstantFP::get(Divisor->getType(), 0.0), "is_zero");
    } else {
        IsZero = Builder.CreateICmpEQ(Divisor, ConstantInt::get(Divisor->getType(), 0), "is_zero");
    }
    emitCheck(IsZero, DivZeroMsg, Line);
}

void RuntimeCheck::emitIndexCheck(Value *Index, Value *Lower, Value *Upper, int Line) {
    Value *TooLow = Builder.CreateICmpSLT(Index, Lower, "too_low");
    Value *TooHigh = Builder.CreateICmpSGT(Index, Upper, "too_high");
    Value *OutOfBounds = Builder.CreateOr(TooLow, TooHigh, "out_of_bounds");

    emitCheck(OutOfBounds, OutOfBoundsMsg, Line);
}

void RuntimeCheck::emitEnumRangeCheck(Value *Ordinal, uint64_t ValueCount,
                                      const std::string &TypeName, int Line) {
    Value *Lo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(kEnumOrdinalBase), true));
    Value *Hi = ConstantInt::get(TheContext,
                                 APInt(64, static_cast<uint64_t>(kEnumOrdinalBase) + ValueCount, true));
    Value *TooLow = Builder.CreateICmpSLT(Ordinal, Lo, "enum_too_low");
    Value *TooHigh = Builder.CreateICmpSGE(Ordinal, Hi, "enum_too_high");
    Value *OutOfRange = Builder.CreateOr(TooLow, TooHigh, "enum_out_of_range");

    Value *NameStr = Builder.CreateGlobalString(TypeName, "enum_name", 0, &TheModule);
    emitCheck(OutOfRange, EnumRangeMsg,
              {lineValue(Line), NameStr});
}

void RuntimeCheck::emitNullDerefCheck(Value *Ptr, int Line) {
    Value *IsNull = Builder.CreateICmpEQ(Ptr,
                                         ConstantPointerNull::get(PointerType::getUnqual(TheContext)),
                                         "ptr_is_null");
    emitCheck(IsNull, NullDerefMsg, Line);
}

void RuntimeCheck::emitArrayArgBoundsCheck(Value *ActualLower, Value *ActualUpper,
                                           int64_t DeclaredLower, int64_t DeclaredUpper,
                                           int Line) {
    Value *DeclLo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredLower), true));
    Value *DeclHi = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredUpper), true));
    Value *LoDiff = Builder.CreateICmpNE(ActualLower, DeclLo, "arg_lo_ne");
    Value *HiDiff = Builder.CreateICmpNE(ActualUpper, DeclHi, "arg_hi_ne");
    Value *Mismatch = Builder.CreateOr(LoDiff, HiDiff, "arg_bounds_ne");

    emitCheck(Mismatch, ArrayArgBoundsMsg,
              {lineValue(Line),
               ActualLower, ActualUpper, DeclLo, DeclHi});
}
