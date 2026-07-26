#include "cps/RuntimeCheck.h"
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
