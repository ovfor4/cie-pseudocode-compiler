#include "cps/FileHandler.h"
#include "cps/RuntimeCheck.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace cps;

FileHandler::FileHandler(LLVMContext &Ctx, IRBuilder<> &B, llvm::Module &M,
                         RuntimeCheck &RC)
    : Context(Ctx), Builder(B), Module(M), Checker(RC) {
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
    ModeReadStr = Builder.CreateGlobalStringPtr("r", "file_mode_r", 0, &Module);
    ModeWriteStr = Builder.CreateGlobalStringPtr("w", "file_mode_w", 0, &Module);
    ModeAppendStr = Builder.CreateGlobalStringPtr("a", "file_mode_a", 0, &Module);

    ErrOpenFail = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: Cannot open file '%s'\n", "err_file_open", 0, &Module);
    ErrAlreadyOpen = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: File '%s' is already open\n", "err_file_already_open", 0, &Module);
    ErrTableFull = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: Too many open files\n", "err_file_table_full", 0, &Module);
    ErrNotOpen = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: File '%s' is not open\n", "err_file_not_open", 0, &Module);
    ErrNotRead = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: File '%s' is not open for READ\n", "err_file_not_read", 0, &Module);
    ErrNotWrite = Builder.CreateGlobalStringPtr(
        "[Fatal] line %d: File '%s' is not open for WRITE\n", "err_file_not_write", 0, &Module);
    ErrPastEof = Builder.CreateGlobalStringPtr(
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
    BasicBlock *Entry = BasicBlock::Create(Context, "entry", F);
    Builder.SetInsertPoint(Entry);
    return F;
}

Value *FileHandler::emitSlotGEP(GlobalVariable *Table, Type *ElemTy, Value *Idx) {
    Value *Zero = ConstantInt::get(Context, APInt(64, 0));
    return Builder.CreateInBoundsGEP(ArrayType::get(ElemTy, TableSize), Table, {Zero, Idx});
}

// Shared prologue of every helper that operates on an already-open file:
// look the name up, die with "not open" on a miss, return the slot index.
Value *FileHandler::emitLookupOrFail(Value *Name, Value *Line) {
    Value *Idx = Builder.CreateCall(FindFn, {Name}, "slot");
    Value *NotOpen = Builder.CreateICmpSLT(Idx, ConstantInt::get(Context, APInt(64, 0)), "not_open");
    Checker.emitErrorAndExit(NotOpen, ErrNotOpen, {Line, Name});
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
    Checker.emitErrorAndExit(AlreadyOpen, ErrAlreadyOpen, {Line, Name});

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
    Checker.emitErrorAndExit(NoFree, ErrTableFull, {Line});

    // Branch-free fopen-mode selection: 1 -> "r", 2 -> "w", 3 -> "a".
    Value *IsWrite = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, uint8_t(FileMode::Write)));
    Value *WriteOrAppend = Builder.CreateSelect(IsWrite, ModeWriteStr, ModeAppendStr);
    Value *IsRead = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, uint8_t(FileMode::Read)));
    Value *ModeStr = Builder.CreateSelect(IsRead, ModeReadStr, WriteOrAppend, "mode_str");

    Value *Handle = Builder.CreateCall(FopenFunc, {Name, ModeStr}, "handle");
    Value *OpenFailed = Builder.CreateICmpEQ(Handle, ConstantPointerNull::get(PtrTy), "open_failed");
    Checker.emitErrorAndExit(OpenFailed, ErrOpenFail, {Line, Name});

    Builder.CreateStore(Name, emitSlotGEP(FileNames, PtrTy, Slot));
    Builder.CreateStore(Handle, emitSlotGEP(FileHandles, PtrTy, Slot));
    Builder.CreateStore(Mode, emitSlotGEP(FileModes, I8, Slot));
    Builder.CreateRetVoid();
}

// ptr __cps_file_read(ptr name, i32 line): read one line into a fresh heap
// STRING ('\n' consumed and stripped, trailing '\r' dropped). Reading at EOF
// is a fatal error — CIE programs are expected to guard with EOF().
void FileHandler::buildReadFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I64 = Type::getInt64Ty(Context);
    Type *I32 = Type::getInt32Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);

    ReadFn = createHelper("__cps_file_read", FunctionType::get(PtrTy, {PtrTy, I32}, false));
    Value *Name = ReadFn->getArg(0);
    Value *Line = ReadFn->getArg(1);
    Name->setName("name");
    Line->setName("line");

    Value *Idx = emitLookupOrFail(Name, Line);
    Value *Mode = Builder.CreateLoad(I8, emitSlotGEP(FileModes, I8, Idx), "mode");
    Value *NotRead = Builder.CreateICmpNE(Mode, ConstantInt::get(I8, uint8_t(FileMode::Read)), "not_read");
    Checker.emitErrorAndExit(NotRead, ErrNotRead, {Line, Name});
    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");

    Value *MinusOne = ConstantInt::get(Context, APInt(32, -1, /*isSigned=*/true));
    Value *Peek = Builder.CreateCall(FgetcFunc, {Handle}, "peek");
    Value *AtEof = Builder.CreateICmpEQ(Peek, MinusOne, "at_eof");
    Checker.emitErrorAndExit(AtEof, ErrPastEof, {Line, Name});
    Builder.CreateCall(UngetcFunc, {Peek, Handle});

    // Growing buffer: cap doubles, always leaving room for byte + NUL.
    AllocaInst *BufVar = Builder.CreateAlloca(PtrTy, nullptr, "buf");
    AllocaInst *LenVar = Builder.CreateAlloca(I64, nullptr, "len");
    AllocaInst *CapVar = Builder.CreateAlloca(I64, nullptr, "cap");
    AllocaInst *ChVar = Builder.CreateAlloca(I32, nullptr, "ch");
    Value *InitCap = ConstantInt::get(Context, APInt(64, 64));
    Builder.CreateStore(Builder.CreateCall(MallocFunc, {InitCap}, "line_buf"), BufVar);
    Builder.CreateStore(ConstantInt::get(Context, APInt(64, 0)), LenVar);
    Builder.CreateStore(InitCap, CapVar);

    BasicBlock *LoopBB = BasicBlock::Create(Context, "read_loop", ReadFn);
    BasicBlock *EnsureBB = BasicBlock::Create(Context, "ensure_cap", ReadFn);
    BasicBlock *GrowBB = BasicBlock::Create(Context, "grow", ReadFn);
    BasicBlock *StoreBB = BasicBlock::Create(Context, "store_char", ReadFn);
    BasicBlock *StripBB = BasicBlock::Create(Context, "strip", ReadFn);
    BasicBlock *CheckCrBB = BasicBlock::Create(Context, "check_cr", ReadFn);
    BasicBlock *DropCrBB = BasicBlock::Create(Context, "drop_cr", ReadFn);
    BasicBlock *DoneBB = BasicBlock::Create(Context, "done", ReadFn);

    Builder.CreateBr(LoopBB);

    Builder.SetInsertPoint(LoopBB);
    Value *Ch = Builder.CreateCall(FgetcFunc, {Handle}, "c");
    Builder.CreateStore(Ch, ChVar);
    Value *IsEof = Builder.CreateICmpEQ(Ch, MinusOne);
    Value *IsNl = Builder.CreateICmpEQ(Ch, ConstantInt::get(Context, APInt(32, '\n')));
    Value *Stop = Builder.CreateOr(IsEof, IsNl, "stop");
    Builder.CreateCondBr(Stop, StripBB, EnsureBB);

    Builder.SetInsertPoint(EnsureBB);
    Value *Len = Builder.CreateLoad(I64, LenVar, "len");
    Value *Cap = Builder.CreateLoad(I64, CapVar, "cap");
    Value *Need = Builder.CreateAdd(Len, ConstantInt::get(Context, APInt(64, 2)), "need");
    Value *TooSmall = Builder.CreateICmpSGT(Need, Cap, "too_small");
    Builder.CreateCondBr(TooSmall, GrowBB, StoreBB);

    Builder.SetInsertPoint(GrowBB);
    Value *NewCap = Builder.CreateShl(Cap, ConstantInt::get(Context, APInt(64, 1)), "new_cap");
    Builder.CreateStore(NewCap, CapVar);
    Value *OldBuf = Builder.CreateLoad(PtrTy, BufVar);
    Builder.CreateStore(Builder.CreateCall(ReallocFunc, {OldBuf, NewCap}, "regrown"), BufVar);
    Builder.CreateBr(StoreBB);

    Builder.SetInsertPoint(StoreBB);
    Value *Buf = Builder.CreateLoad(PtrTy, BufVar);
    Value *CurLen = Builder.CreateLoad(I64, LenVar);
    Value *DestPtr = Builder.CreateInBoundsGEP(I8, Buf, CurLen);
    Value *Byte = Builder.CreateTrunc(Builder.CreateLoad(I32, ChVar), I8);
    Builder.CreateStore(Byte, DestPtr);
    Builder.CreateStore(Builder.CreateAdd(CurLen, ConstantInt::get(Context, APInt(64, 1))), LenVar);
    Builder.CreateBr(LoopBB);

    // Normalize CRLF: drop one trailing '\r' (covers reading Windows-authored
    // files on Linux; Windows text mode has already translated).
    Builder.SetInsertPoint(StripBB);
    Value *EndLen = Builder.CreateLoad(I64, LenVar, "end_len");
    Value *HasChars = Builder.CreateICmpSGT(EndLen, ConstantInt::get(Context, APInt(64, 0)));
    Builder.CreateCondBr(HasChars, CheckCrBB, DoneBB);

    Builder.SetInsertPoint(CheckCrBB);
    Value *EndBuf = Builder.CreateLoad(PtrTy, BufVar);
    Value *LastIdx = Builder.CreateSub(EndLen, ConstantInt::get(Context, APInt(64, 1)));
    Value *LastByte = Builder.CreateLoad(I8, Builder.CreateInBoundsGEP(I8, EndBuf, LastIdx), "last");
    Value *IsCr = Builder.CreateICmpEQ(LastByte, ConstantInt::get(I8, '\r'), "is_cr");
    Builder.CreateCondBr(IsCr, DropCrBB, DoneBB);

    Builder.SetInsertPoint(DropCrBB);
    Builder.CreateStore(LastIdx, LenVar);
    Builder.CreateBr(DoneBB);

    Builder.SetInsertPoint(DoneBB);
    Value *FinalBuf = Builder.CreateLoad(PtrTy, BufVar);
    Value *FinalLen = Builder.CreateLoad(I64, LenVar);
    Builder.CreateStore(ConstantInt::get(I8, 0), Builder.CreateInBoundsGEP(I8, FinalBuf, FinalLen));
    Builder.CreateRet(FinalBuf);
}

// void __cps_file_write(ptr name, ptr data, i32 line): one line, WRITE/APPEND only.
void FileHandler::buildWriteFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I32 = Type::getInt32Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);

    WriteFn = createHelper("__cps_file_write",
                           FunctionType::get(Type::getVoidTy(Context), {PtrTy, PtrTy, I32}, false));
    Value *Name = WriteFn->getArg(0);
    Value *Data = WriteFn->getArg(1);
    Value *Line = WriteFn->getArg(2);
    Name->setName("name");
    Data->setName("data");
    Line->setName("line");

    Value *Idx = emitLookupOrFail(Name, Line);
    Value *Mode = Builder.CreateLoad(I8, emitSlotGEP(FileModes, I8, Idx), "mode");
    Value *IsReadMode = Builder.CreateICmpEQ(Mode, ConstantInt::get(I8, uint8_t(FileMode::Read)), "read_only");
    Checker.emitErrorAndExit(IsReadMode, ErrNotWrite, {Line, Name});

    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");
    Builder.CreateCall(FputsFunc, {Data, Handle});
    Builder.CreateCall(FputcFunc, {ConstantInt::get(Context, APInt(32, '\n')), Handle});
    Builder.CreateRetVoid();
}

// void __cps_file_close(ptr name, i32 line)
void FileHandler::buildCloseFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I32 = Type::getInt32Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);

    CloseFn = createHelper("__cps_file_close",
                           FunctionType::get(Type::getVoidTy(Context), {PtrTy, I32}, false));
    Value *Name = CloseFn->getArg(0);
    Value *Line = CloseFn->getArg(1);
    Name->setName("name");
    Line->setName("line");

    Value *Idx = emitLookupOrFail(Name, Line);
    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");
    Builder.CreateCall(FcloseFunc, {Handle});
    Builder.CreateStore(ConstantInt::get(I8, 0), emitSlotGEP(FileModes, I8, Idx));
    Builder.CreateStore(ConstantPointerNull::get(PtrTy), emitSlotGEP(FileNames, PtrTy, Idx));
    Builder.CreateStore(ConstantPointerNull::get(PtrTy), emitSlotGEP(FileHandles, PtrTy, Idx));
    Builder.CreateRetVoid();
}

// i1 __cps_file_eof(ptr name, i32 line): TRUE iff no more lines can be read.
void FileHandler::buildEofFn() {
    PointerType *PtrTy = PointerType::getUnqual(Context);
    Type *I32 = Type::getInt32Ty(Context);
    Type *I8 = Type::getInt8Ty(Context);

    EofFn = createHelper("__cps_file_eof",
                         FunctionType::get(Type::getInt1Ty(Context), {PtrTy, I32}, false));
    Value *Name = EofFn->getArg(0);
    Value *Line = EofFn->getArg(1);
    Name->setName("name");
    Line->setName("line");

    Value *Idx = emitLookupOrFail(Name, Line);
    Value *Mode = Builder.CreateLoad(I8, emitSlotGEP(FileModes, I8, Idx), "mode");
    Value *NotRead = Builder.CreateICmpNE(Mode, ConstantInt::get(I8, uint8_t(FileMode::Read)), "not_read");
    Checker.emitErrorAndExit(NotRead, ErrNotRead, {Line, Name});

    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");
    Value *Ch = Builder.CreateCall(FgetcFunc, {Handle}, "c");
    // ungetc(EOF, f) is a defined no-op, so no branch is needed.
    Builder.CreateCall(UngetcFunc, {Ch, Handle});
    Value *IsEof = Builder.CreateICmpEQ(Ch, ConstantInt::get(Context, APInt(32, -1, /*isSigned=*/true)), "is_eof");
    Builder.CreateRet(IsEof);
}

void FileHandler::emitOpen(Value *NameStr, FileMode Mode, int Line) {
    if (!NameStr) return;
    ensureRuntime();
    Builder.CreateCall(OpenFn, {NameStr,
                                ConstantInt::get(Type::getInt8Ty(Context), uint8_t(Mode)),
                                ConstantInt::get(Context, APInt(32, Line))});
}

Value *FileHandler::emitRead(Value *NameStr, int Line) {
    if (!NameStr) return nullptr;
    ensureRuntime();
    return Builder.CreateCall(ReadFn, {NameStr, ConstantInt::get(Context, APInt(32, Line))},
                              "file_line");
}

void FileHandler::emitWrite(Value *NameStr, Value *DataStr, int Line) {
    if (!NameStr || !DataStr) return;
    ensureRuntime();
    Builder.CreateCall(WriteFn, {NameStr, DataStr, ConstantInt::get(Context, APInt(32, Line))});
}

void FileHandler::emitClose(Value *NameStr, int Line) {
    if (!NameStr) return;
    ensureRuntime();
    Builder.CreateCall(CloseFn, {NameStr, ConstantInt::get(Context, APInt(32, Line))});
}

Value *FileHandler::emitEof(Value *NameStr, int Line) {
    if (!NameStr) return nullptr;
    ensureRuntime();
    return Builder.CreateCall(EofFn, {NameStr, ConstantInt::get(Context, APInt(32, Line))},
                              "file_eof");
}
