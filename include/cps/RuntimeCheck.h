#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <string>

namespace cps {

class ExceptionHandler;

class RuntimeCheck {
    llvm::LLVMContext &TheContext;
    llvm::IRBuilder<> &Builder;
    ExceptionHandler &Exceptions;

public:
    RuntimeCheck(llvm::LLVMContext &C, llvm::IRBuilder<> &B, ExceptionHandler &EH);

    // Split the block, throwing an anonymous exception when Condition is true.
    void emitCheck(llvm::Value *Condition);

    void emitDivZeroCheck(llvm::Value *Divisor, int Line);
    void emitIndexCheck(llvm::Value *Index, llvm::Value *Lower, llvm::Value *Upper, int Line);

    // Enum +/- INTEGER results must stay inside the enum's value range.
    void emitEnumRangeCheck(llvm::Value *Ordinal, uint64_t ValueCount,
                            const std::string &TypeName, int Line);

    // Dereferencing an unset (null) pointer throws instead of segfaulting.
    void emitNullDerefCheck(llvm::Value *Ptr, int Line);

    // A bounded array parameter is a contract: the argument's runtime bounds
    // must equal the declared ones.
    void emitArrayArgBoundsCheck(llvm::Value *ActualLower, llvm::Value *ActualUpper,
                                 int64_t DeclaredLower, int64_t DeclaredUpper, int Line);

};

}
