#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace cps {

class StringConversionHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;

    llvm::FunctionCallee SprintfFunc;
    llvm::FunctionCallee StrtollFunc;
    llvm::FunctionCallee StrtodFunc;
    llvm::FunctionCallee MallocFunc;

public:
    StringConversionHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, llvm::Module &M);
    
    void setupExternalFunctions();
    
    // NUM TO STR(x) RETURNS STRING
    llvm::Value *emitNumToStr(llvm::Value *Num, bool IsReal);
    
    // STR TO NUM(x) RETURNS NUMBER
    llvm::Value *emitStrToNum(llvm::Value *Str, bool AsReal);
    
    // IS NUM(ThisString) RETURNS BOOLEAN
    llvm::Value *emitIsNum(llvm::Value *Str);
};

} // namespace cps
