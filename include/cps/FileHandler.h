#pragma once
#include "cps/FileMode.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

namespace cps {

class RuntimeCheck;

// Emits the text-file runtime: a fixed-size open-file table plus internal
// __cps_file_* helper functions, synthesized once on first use so programs
// without file statements keep their IR unchanged. The table is keyed by
// filename *content* (strcmp) — pseudocode STRING equality is pointer
// identity, so the same file may be named by many different buffers.
// Leaf module: methods only take and return llvm::Value*s; expression
// evaluation, type coercion and the symbol table stay in CodeGen.
class FileHandler {
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;
    RuntimeCheck &Checker;

    static constexpr uint64_t TableSize = 16;
    bool RuntimeEmitted = false;

    // libc — declared lazily on first file use
    llvm::FunctionCallee FopenFunc, FcloseFunc, FgetcFunc, UngetcFunc,
                         FputsFunc, FputcFunc, MallocFunc, ReallocFunc,
                         StrcmpFunc;

    // open-file table
    llvm::GlobalVariable *FileNames = nullptr;   // [TableSize x ptr] STRING passed to OPENFILE
    llvm::GlobalVariable *FileHandles = nullptr; // [TableSize x ptr] FILE*
    llvm::GlobalVariable *FileModes = nullptr;   // [TableSize x i8]  0 = free slot, else FileMode

    llvm::Value *ModeReadStr = nullptr, *ModeWriteStr = nullptr, *ModeAppendStr = nullptr;
    llvm::Value *ErrOpenFail = nullptr, *ErrAlreadyOpen = nullptr, *ErrTableFull = nullptr,
                *ErrNotOpen = nullptr, *ErrNotRead = nullptr, *ErrNotWrite = nullptr,
                *ErrPastEof = nullptr;

    // synthesized helpers (internal linkage; pseudocode identifiers cannot
    // start with '_', so these names are un-collidable)
    llvm::Function *FindFn = nullptr, *OpenFn = nullptr, *ReadFn = nullptr,
                   *WriteFn = nullptr, *CloseFn = nullptr, *EofFn = nullptr;

    void setupExternalFunctions();
    void setupGlobals();
    void ensureRuntime();

    llvm::Function *createHelper(const char *Name, llvm::FunctionType *FT);
    llvm::Value *emitSlotGEP(llvm::GlobalVariable *Table, llvm::Type *ElemTy, llvm::Value *Idx);
    llvm::Value *emitLookupOrFail(llvm::Value *Name, llvm::Value *Line);
    void buildFindFn();
    void buildOpenFn();
    void buildReadFn();
    void buildWriteFn();
    void buildCloseFn();
    void buildEofFn();

public:
    FileHandler(llvm::LLVMContext &Ctx, llvm::IRBuilder<> &B, llvm::Module &M,
                RuntimeCheck &RC);

    // NameStr/DataStr are already-coerced STRING ptrs; Line is the statement's
    // source line for runtime diagnostics. All methods null-check and bail;
    // the caller (CodeGen) does the diagnostics.
    void emitOpen(llvm::Value *NameStr, FileMode Mode, int Line);
    llvm::Value *emitRead(llvm::Value *NameStr, int Line); // returns STRING ptr
    void emitWrite(llvm::Value *NameStr, llvm::Value *DataStr, int Line);
    void emitClose(llvm::Value *NameStr, int Line);
    llvm::Value *emitEof(llvm::Value *NameStr, int Line); // returns i1
};

} // namespace cps
