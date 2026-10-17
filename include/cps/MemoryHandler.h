#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"

namespace cps {

class TypeSystem;
class RuntimeCheck;
class ExceptionHandler;

// Explicit memory builtins and keyword NEW own entries in this allocation list.
// Internal STRING/ARRAY storage retains its existing lifetime rules.
class MemoryHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;
    TypeSystem &Types;
    RuntimeCheck &Checker;
    ExceptionHandler &Exceptions;
    llvm::IntegerType *SizeTy = nullptr;
    llvm::StructType *NodeTy = nullptr;
    llvm::GlobalVariable *Head = nullptr;
    llvm::FunctionCallee Malloc, Free, Realloc;
    llvm::Function *AllocateFn = nullptr, *ReleaseFn = nullptr, *FindFn = nullptr,
                   *ResizeFn = nullptr;
    llvm::Value *AllocationError = nullptr, *TrackingError = nullptr, *InvalidAddress = nullptr;

    void ensureRuntime();
    llvm::Function *createHelper(const char *Name, llvm::ArrayRef<llvm::Type *> Args);
    void buildFind();
    void buildAllocate();
    void buildRelease();
    void buildResize();

public:
    MemoryHandler(llvm::LLVMContext &C, llvm::IRBuilder<> &B, llvm::Module &M,
                  TypeSystem &TS, RuntimeCheck &RC, ExceptionHandler &EH)
        : Context(C), Builder(B), Module(M), Types(TS), Checker(RC), Exceptions(EH) {}

    llvm::Value *emitAllocate(llvm::Value *Size, int Line);
    void emitValidate(llvm::Value *Address, int Line);
    llvm::Value *emitRelease(llvm::Value *Address, int Line);
    llvm::Value *emitResize(llvm::Value *Address, llvm::Value *Size, int Line);
};

} // namespace cps
