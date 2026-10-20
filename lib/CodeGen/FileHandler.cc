#include "cps/FileHandler.h"
#include "cps/RuntimeCheck.h"
#include "cps/ExceptionHandler.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace cps;

FileHandler::FileHandler(LLVMContext &Ctx, IRBuilder<> &B, llvm::Module &M,
                         RuntimeCheck &RC, ExceptionHandler &EH)
    : Context(Ctx), Builder(B), Module(M), Checker(RC), Exceptions(EH) {
    // Nothing is declared or emitted here: the whole runtime is synthesized
    // lazily by ensureRuntime() so file-free programs keep their IR unchanged.
}

void FileHandler::setupExternalFunctions() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I64 = Type::getInt64Ty(Context);
    Type *I32 = Type::getInt32Ty(Context);

    FopenFunc = Module.getOrInsertFunction(
        "fopen", FunctionType::get(PtrTy, {PtrTy, PtrTy}, false));
    FcloseFunc = Module.getOrInsertFunction(
        "fclose", FunctionType::get(I32, {PtrTy}, false));
    FgetcFunc = Module.getOrInsertFunction(
        "fgetc", FunctionType::get(I32, {PtrTy}, false));
    UngetcFunc = Module.getOrInsertFunction(
        "ungetc", FunctionType::get(I32, {I32, PtrTy}, false));
    FputsFunc = Module.getOrInsertFunction(
        "fputs", FunctionType::get(I32, {PtrTy, PtrTy}, false));
    FputcFunc = Module.getOrInsertFunction(
        "fputc", FunctionType::get(I32, {I32, PtrTy}, false));
    MallocFunc = Module.getOrInsertFunction(
        "malloc", FunctionType::get(PtrTy, {I64}, false));
    ReallocFunc = Module.getOrInsertFunction(
        "realloc", FunctionType::get(PtrTy, {PtrTy, I64}, false));
    StrcmpFunc = Module.getOrInsertFunction(
        "strcmp", FunctionType::get(I32, {PtrTy, PtrTy}, false));
    StrlenFunc = Module.getOrInsertFunction("strlen", FunctionType::get(I64, {PtrTy}, false));
    MemcpyFunc = Module.getOrInsertFunction("memcpy", FunctionType::get(PtrTy, {PtrTy, PtrTy, I64}, false));
    FreeFunc = Module.getOrInsertFunction("free", FunctionType::get(Type::getVoidTy(Context), {PtrTy}, false));
}

void FileHandler::setupGlobals() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    ArrayType *PtrArrTy = ArrayType::get(PtrTy, TableSize);
    ArrayType *ByteArrTy = ArrayType::get(Type::getInt8Ty(Context), TableSize);

    FileNames = new GlobalVariable(Module, PtrArrTy, false, GlobalValue::InternalLinkage,
                                   ConstantAggregateZero::get(PtrArrTy), "__cps_file_names");
    FileHandles = new GlobalVariable(Module, PtrArrTy, false, GlobalValue::InternalLinkage,
                                     ConstantAggregateZero::get(PtrArrTy), "__cps_file_handles");
    FileModes = new GlobalVariable(Module, ByteArrTy, false, GlobalValue::InternalLinkage,
                                   ConstantAggregateZero::get(ByteArrTy), "__cps_file_modes");

    // Text mode on purpose (no "b"): on Windows the CRT translates CRLF<->LF,
    // which is exactly what line-oriented READFILE/WRITEFILE want.
    ModeReadStr = Builder.CreateGlobalString("r", "file_mode_r", 0, &Module);
    ModeWriteStr = Builder.CreateGlobalString("w", "file_mode_w", 0, &Module);
    ModeAppendStr = Builder.CreateGlobalString("a", "file_mode_a", 0, &Module);

    ErrOpenFail = Builder.CreateGlobalString(
        "[Fatal] line %d: Cannot open file '%s'\n", "err_file_open", 0, &Module);
    ErrAlreadyOpen = Builder.CreateGlobalString(
        "[Fatal] line %d: File '%s' is already open\n", "err_file_already_open", 0, &Module);
    ErrTableFull = Builder.CreateGlobalString(
        "[Fatal] line %d: Too many open files\n", "err_file_table_full", 0, &Module);
    ErrNotOpen = Builder.CreateGlobalString(
        "[Fatal] line %d: File '%s' is not open\n", "err_file_not_open", 0, &Module);
    ErrNotRead = Builder.CreateGlobalString(
        "[Fatal] line %d: File '%s' is not open for READ\n", "err_file_not_read", 0, &Module);
    ErrNotWrite = Builder.CreateGlobalString(
        "[Fatal] line %d: File '%s' is not open for WRITE\n", "err_file_not_write", 0, &Module);
    ErrPastEof = Builder.CreateGlobalString(
        "[Fatal] line %d: Read past end of file '%s'\n", "err_file_past_eof", 0, &Module);
}

void FileHandler::ensureRuntime() {
    if (RuntimeEmitted) return;
    RuntimeEmitted = true;

    // Runs mid-statement-emission: the shared Builder sits at the end of an
    // unterminated block in main() or a user function. Save/restore it around
    // building the helper bodies (same discipline as FunctionDef emission).
    BasicBlock *Saved = Builder.GetInsertBlock();

    setupExternalFunctions();
    setupGlobals();
    buildFindFn();
    buildOpenFn();
    buildReadFn();
    buildWriteFn();
    buildCloseFn();
    buildEofFn();

    if (Saved) Builder.SetInsertPoint(Saved);
}

Function *FileHandler::createHelper(const char *Name, FunctionType *FT) {
    Function *F = Function::Create(FT, Function::InternalLinkage, Name, &Module);
    Exceptions.prepareFunction(F);
    BasicBlock *Entry = BasicBlock::Create(Context, "entry", F);
    Builder.SetInsertPoint(Entry);
    return F;
}

Value *FileHandler::emitSlotGEP(GlobalVariable *Table, Type *ElemTy, Value *Idx) {
    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    return Builder.CreateInBoundsGEP(ArrayType::get(ElemTy, TableSize), Table, {Zero, Idx});
}

// Shared prologue of every helper that operates on an already-open file:
// look the name up, report and throw on a miss, return the slot index.
Value *FileHandler::emitLookupOrFail(Value *Name, Value *Line) {
    Value *Idx = Builder.CreateCall(FindFn, {Name}, "slot");
    Value *NotOpen = Builder.CreateICmpSLT(Idx, ConstantInt::get(Context, APInt(64, 0)), "not_open");
    Checker.emitCheck(NotOpen, ErrNotOpen, {Line, Name});
    return Idx;
}

// i64 __cps_file_find(ptr name): strcmp scan over occupied slots; -1 on miss.
void FileHandler::buildFindFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I64 = Type::getInt64Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);

    FindFn = createHelper("__cps_file_find", FunctionType::get(I64, {PtrTy}, false));
    Value *Name = FindFn->getArg(0);
    Name->setName("name");

    AllocaInst *IdxVar = Builder.CreateAlloca(I64, nullptr, "idx");
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, 0)), IdxVar);

    BasicBlock *LoopBB = BasicBlock::Create(Context, "loop", FindFn);
    BasicBlock *CheckBB = BasicBlock::Create(Context, "check_slot", FindFn);
    BasicBlock *CmpBB = BasicBlock::Create(Context, "cmp_name", FindFn);
    BasicBlock *NextBB = BasicBlock::Create(Context, "next", FindFn);
    BasicBlock *FoundBB = BasicBlock::Create(Context, "found", FindFn);
    BasicBlock *MissBB = BasicBlock::Create(Context, "miss", FindFn);

    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(LoopBB);
    Value *Idx = Builder.CreateLoad(I64, IdxVar, "i");
    Value *AtEnd = Builder.CreateICmpSGE(Idx, ConstantInt::get(Context, APInt(64, TableSize)), "at_end");
    Builder.CreateCondBr(AtEnd, MissBB, CheckBB);

    Builder.SetInsertPoint(CheckBB);
    Value *Mode = Builder.CreateLoad(I8, emitSlotGEP(FileModes, I8, Idx), "mode");
    Value *IsFree = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, 0), "is_free");
    Builder.CreateCondBr(IsFree, NextBB, CmpBB);

    Builder.SetInsertPoint(CmpBB);
    Value *SlotName = Builder.CreateLoad(PtrTy, emitSlotGEP(FileNames, PtrTy, Idx), "slot_name");
    Value *Cmp = Builder.CreateCall(StrcmpFunc, {SlotName, Name}, "cmp");
    Value *IsEq = Builder.CreateICmpEQ(Cmp, ConstantInt::get(Context, APInt(32, 0)), "is_eq");
    Builder.CreateCondBr(IsEq, FoundBB, NextBB);

    Builder.SetInsertPoint(NextBB);
    Value *NextIdx = Builder.CreateAdd(Idx, ConstantInt::get(Context, APInt(64, 1)));
    Builder.CreateStore(NextIdx, IdxVar);
    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(FoundBB);
    Builder.CreateRet(Idx);

    Builder.SetInsertPoint(MissBB);
    Builder.CreateRet(ConstantInt::get(Context, APInt(64, -1, /*isSigned=*/true)));
}

// void __cps_file_open(ptr name, i8 mode, i32 line)
void FileHandler::buildOpenFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I64 = Type::getInt64Ty(Context);
    Type *I32 = Type::getInt32Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);
    Type *VoidTy = Type::getVoidTy(Context);

    OpenFn = createHelper("__cps_file_open", FunctionType::get(VoidTy, {PtrTy, I8, I32}, false));
    Value *Name = OpenFn->getArg(0);
    Value *Mode = OpenFn->getArg(1);
    Value *Line = OpenFn->getArg(2);
    Name->setName("name");
    Mode->setName("mode");
    Line->setName("line");

    Value *Existing = Builder.CreateCall(FindFn, {Name}, "existing");
    Value *AlreadyOpen = Builder.CreateICmpSGE(Existing, ConstantInt::get(Context, APInt(64, 0)), "already_open");
    Checker.emitCheck(AlreadyOpen, ErrAlreadyOpen, {Line, Name});

    // First-free-slot scan; SlotVar stays -1 when the table is full.
    AllocaInst *IdxVar = Builder.CreateAlloca(I64, nullptr, "idx");
    AllocaInst *SlotVar = Builder.CreateAlloca(I64, nullptr, "slot");
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, 0)), IdxVar);
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, -1, /*isSigned=*/true)), SlotVar);

    BasicBlock *ScanBB = BasicBlock::Create(Context, "scan", OpenFn);
    BasicBlock *CheckBB = BasicBlock::Create(Context, "check_free", OpenFn);
    BasicBlock *TakeBB = BasicBlock::Create(Context, "take_slot", OpenFn);
    BasicBlock *NextBB = BasicBlock::Create(Context, "next", OpenFn);
    BasicBlock *AfterBB = BasicBlock::Create(Context, "after_scan", OpenFn);

    Builder.CreateBr(ScanBB);

    Builder.SetInsertPoint(ScanBB);
    Value *Idx = Builder.CreateLoad(I64, IdxVar, "i");
    Value *AtEnd = Builder.CreateICmpSGE(Idx, ConstantInt::get(Context, APInt(64, TableSize)), "at_end");
    Builder.CreateCondBr(AtEnd, AfterBB, CheckBB);

    Builder.SetInsertPoint(CheckBB);
    Value *SlotMode = Builder.CreateLoad(I8, emitSlotGEP(FileModes, I8, Idx), "slot_mode");
    Value *IsFree = Builder.CreateICmpEQ(SlotMode, ConstantInt::get(I8, 0), "is_free");
    Builder.CreateCondBr(IsFree, TakeBB, NextBB);

    Builder.SetInsertPoint(TakeBB);
    Builder.CreateStore(Idx, SlotVar);
    Builder.CreateBr(AfterBB);

    Builder.SetInsertPoint(NextBB);
    Builder.CreateStore(Builder.CreateAdd(Idx, ConstantInt::get(Context, APInt(64, 1))), IdxVar);
    Builder.CreateBr(ScanBB);

    Builder.SetInsertPoint(AfterBB);
    Value *Slot = Builder.CreateLoad(I64, SlotVar, "free_slot");
    Value *NoFree = Builder.CreateICmpSLT(Slot, ConstantInt::get(Context, APInt(64, 0)), "table_full");
    Checker.emitCheck(NoFree, ErrTableFull, {Line});

    // Branch-free fopen-mode selection: 1 -> "r", 2 -> "w", 3 -> "a".
    Value *IsWrite = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, uint8_t(FileMode::Write)));
    Value *WriteOrAppend = Builder.CreateSelect(IsWrite, ModeWriteStr, ModeAppendStr);
    Value *IsRead = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, uint8_t(FileMode::Read)));
    Value *ModeStr = Builder.CreateSelect(IsRead, ModeReadStr, WriteOrAppend, "mode_str");

    Value *Size = Builder.CreateAdd(Builder.CreateCall(StrlenFunc, {Name}), Builder.getInt64(1));
    Value *NameCopy = Builder.CreateCall(MallocFunc, {Size}, "owned_filename");
    Value *AllocationError = Builder.CreateGlobalString("[Fatal] line %d: File name allocation failed\n");
    Checker.emitCheck(Builder.CreateICmpEQ(NameCopy, ConstantPointerNull::get(PtrTy)), AllocationError, {Line});
    Builder.CreateCall(MemcpyFunc, {NameCopy, Name, Size});
    Value *Handle = Builder.CreateCall(FopenFunc, {Name, ModeStr}, "handle");
    Value *OpenFailed = Builder.CreateICmpEQ(Handle, ConstantPointerNull::get(PtrTy), "open_failed");
    auto *Failed = BasicBlock::Create(Context, "open_failed", OpenFn);
    auto *Opened = BasicBlock::Create(Context, "opened", OpenFn);
    Builder.CreateCondBr(OpenFailed, Failed, Opened);
    Builder.SetInsertPoint(Failed);
    Builder.CreateCall(FreeFunc, {NameCopy});
    Checker.emitCheck(OpenFailed, ErrOpenFail, {Line, Name});
    Builder.CreateBr(Opened);
    Builder.SetInsertPoint(Opened);
    Builder.CreateStore(NameCopy, emitSlotGEP(FileNames, PtrTy, Slot));
    Builder.CreateStore(Handle, emitSlotGEP(FileHandles, PtrTy, Slot));
    Builder.CreateStore(Mode, emitSlotGEP(FileModes, I8, Slot));
    Builder.CreateRetVoid();
}

void FileHandler::emitOpen(Value *NameStr, FileMode Mode, int Line) {
    if (!NameStr) return;
    ensureRuntime();
    Exceptions.emitCall(OpenFn, {NameStr,
                                ConstantInt::get(Type::getInt8Ty(Context), uint8_t(Mode)),
                                Checker.lineValue(Line)});
}

Value *FileHandler::emitRead(Value *NameStr, int Line) {
    if (!NameStr) return nullptr;
    ensureRuntime();
    return Exceptions.emitCall(ReadFn, {NameStr, Checker.lineValue(Line)},
                              "file_line");
}

void FileHandler::emitWrite(Value *NameStr, Value *DataStr, int Line) {
    if (!NameStr || !DataStr) return;
    ensureRuntime();
    Exceptions.emitCall(WriteFn, {NameStr, DataStr, Checker.lineValue(Line)});
}

void FileHandler::emitClose(Value *NameStr, int Line) {
    if (!NameStr) return;
    ensureRuntime();
    Exceptions.emitCall(CloseFn, {NameStr, Checker.lineValue(Line)});
}

Value *FileHandler::emitEof(Value *NameStr, int Line) {
    if (!NameStr) return nullptr;
    ensureRuntime();
    return Exceptions.emitCall(EofFn, {NameStr, Checker.lineValue(Line)},
                              "file_eof");
}
