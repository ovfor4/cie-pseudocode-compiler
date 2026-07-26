#include "cps/RuntimeCheck.h"
#include "cps/TypeSystem.h" // kEnumOrdinalBase
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
using namespace cps;

RuntimeCheck::RuntimeCheck(Module &M, LLVMContext &C, IRBuilder<> &B)
    : TheModule(M), TheContext(C), Builder(B) {
    setupExternalFunctions();
}

void RuntimeCheck::setupExternalFunctions() {
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(TheContext), PrintfArgs, true);
    PrintfFunc = TheModule.getOrInsertFunction("printf", PrintfType);

    std::vector<Type*> ExitArgs;
    ExitArgs.push_back(Type::getInt32Ty(TheContext));
    FunctionType *ExitType = FunctionType::get(Type::getVoidTy(TheContext), ExitArgs, false);
    ExitFunc = TheModule.getOrInsertFunction("exit", ExitType);

    DivZeroMsg = Builder.CreateGlobalStringPtr("[Fatal] line %d: Division by zero\n", "err_div_zero", 0, &TheModule);
    OutOfBoundsMsg = Builder.CreateGlobalStringPtr("[Fatal] line %d: Array index out of bounds\n", "err_bounds", 0, &TheModule);
    EnumRangeMsg = Builder.CreateGlobalStringPtr("[Fatal] line %d: Value out of range for enum %s\n", "err_enum_range", 0, &TheModule);
    NullDerefMsg = Builder.CreateGlobalStringPtr("[Fatal] line %d: Dereference of an unset pointer\n", "err_null_deref", 0, &TheModule);
    ArrayArgBoundsMsg = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: Array argument bounds [%lld:%lld] do not match the declared parameter bounds [%lld:%lld]\n",
        "err_arr_arg_bounds", 0, &TheModule);
}

void RuntimeCheck::emitErrorAndExit(Value *Condition, Value *Msg, int Line) {
    emitErrorAndExit(Condition, Msg, {ConstantInt::get(TheContext, APInt(32, Line))});
}

void RuntimeCheck::emitErrorAndExit(Value *Condition, Value *Msg, ArrayRef<Value *> FmtArgs) {
    Function *TheFunction = Builder.GetInsertBlock()->getParent();

    BasicBlock *FailBB = BasicBlock::Create(TheContext, "check_fail", TheFunction);
    BasicBlock *ContBB = BasicBlock::Create(TheContext, "check_cont", TheFunction);

    Builder.CreateCondBr(Condition, FailBB, ContBB);

    Builder.SetInsertPoint(FailBB);
    std::vector<Value*> PrintArgs;
    PrintArgs.push_back(Msg);
    PrintArgs.insert(PrintArgs.end(), FmtArgs.begin(), FmtArgs.end());
    Builder.CreateCall(PrintfFunc, PrintArgs);

    Builder.CreateCall(ExitFunc, ConstantInt::get(TheContext, APInt(32, 1)));
    Builder.CreateUnreachable();

    Builder.SetInsertPoint(ContBB);
}

void RuntimeCheck::emitDivZeroCheck(Value *Divisor, int Line) {
    Value *IsZero = Builder.CreateICmpEQ(Divisor, ConstantInt::get(TheContext, APInt(64, 0)), "is_zero");
    emitErrorAndExit(IsZero, DivZeroMsg, Line);
}

void RuntimeCheck::emitIndexCheck(Value *Index, Value *Lower, Value *Upper, int Line) {
    Value *TooLow = Builder.CreateICmpSLT(Index, Lower, "too_low");
    Value *TooHigh = Builder.CreateICmpSGT(Index, Upper, "too_high");
    Value *OutOfBounds = Builder.CreateOr(TooLow, TooHigh, "out_of_bounds");

    emitErrorAndExit(OutOfBounds, OutOfBoundsMsg, Line);
}

void RuntimeCheck::emitEnumRangeCheck(Value *Ordinal, uint64_t ValueCount,
                                      const std::string &TypeName, int Line) {
    Value *Lo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(kEnumOrdinalBase), true));
    Value *Hi = ConstantInt::get(TheContext,
                                 APInt(64, static_cast<uint64_t>(kEnumOrdinalBase) + ValueCount, true));
    Value *TooLow = Builder.CreateICmpSLT(Ordinal, Lo, "enum_too_low");
    Value *TooHigh = Builder.CreateICmpSGE(Ordinal, Hi, "enum_too_high");
    Value *OutOfRange = Builder.CreateOr(TooLow, TooHigh, "enum_out_of_range");

    Value *NameStr = Builder.CreateGlobalStringPtr(TypeName, "enum_name", 0, &TheModule);
    emitErrorAndExit(OutOfRange, EnumRangeMsg,
                     {ConstantInt::get(TheContext, APInt(32, Line)), NameStr});
}

void RuntimeCheck::emitNullDerefCheck(Value *Ptr, int Line) {
    Value *IsNull = Builder.CreateICmpEQ(Ptr,
                                         ConstantPointerNull::get(PointerType::getUnqual(TheContext)),
                                         "ptr_is_null");
    emitErrorAndExit(IsNull, NullDerefMsg, Line);
}

void RuntimeCheck::emitArrayArgBoundsCheck(Value *ActualLower, Value *ActualUpper,
                                           int64_t DeclaredLower, int64_t DeclaredUpper,
                                           int Line) {
    Value *DeclLo = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredLower), true));
    Value *DeclHi = ConstantInt::get(TheContext, APInt(64, static_cast<uint64_t>(DeclaredUpper), true));
    Value *LoDiff = Builder.CreateICmpNE(ActualLower, DeclLo, "arg_lo_ne");
    Value *HiDiff = Builder.CreateICmpNE(ActualUpper, DeclHi, "arg_hi_ne");
    Value *Mismatch = Builder.CreateOr(LoDiff, HiDiff, "arg_bounds_ne");

    emitErrorAndExit(Mismatch, ArrayArgBoundsMsg,
                     {ConstantInt::get(TheContext, APInt(32, Line)),
                      ActualLower, ActualUpper, DeclLo, DeclHi});
}
