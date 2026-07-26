#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <string>

namespace cps {

class StringHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;

    llvm::FunctionCallee MallocFunc;
    llvm::FunctionCallee StrLenFunc;
    llvm::FunctionCallee MemCpyFunc;
    llvm::FunctionCallee StrCpyFunc;
    llvm::FunctionCallee StrCatFunc;
    llvm::FunctionCallee ToUpperFunc;
    llvm::FunctionCallee ToLowerFunc;

    llvm::Value *emitCaseConvert(llvm::Value *Str, llvm::FunctionCallee CaseFn);
    llvm::Value *allocCopySubstring(llvm::Value *Src, llvm::Value *StartIdx, llvm::Value *Len);

public:
    StringHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, llvm::Module &M);

    void setupExternalFunctions();
    llvm::Value *createLiteral(const std::string &Val);
    
    llvm::Value *emitLength(llvm::Value *Str);
    llvm::Value *emitConcat(llvm::Value *LHS, llvm::Value *RHS);
    llvm::Value *emitMid(llvm::Value *Str, llvm::Value *Start, llvm::Value *Len);
    llvm::Value *emitRight(llvm::Value *Str, llvm::Value *Len);
    llvm::Value *emitLeft(llvm::Value *Str, llvm::Value *Len);
    llvm::Value *emitLCase(llvm::Value *Str);
    llvm::Value *emitUCase(llvm::Value *Str);
};

}
