#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "cps/Lexer.h"

namespace cps {

class RuntimeCheck;

class ArithmeticHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    bool &HadError;
    RuntimeCheck &RuntimeChecker;

public:
    ArithmeticHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, bool &HadErrorFlag,
                      RuntimeCheck &RC)
        : Context(Ctx), Builder(B), HadError(HadErrorFlag), RuntimeChecker(RC) {}

    llvm::Value *emitBinaryOp(int Op, llvm::Value *LHS, llvm::Value *RHS, int Line);
};

}
