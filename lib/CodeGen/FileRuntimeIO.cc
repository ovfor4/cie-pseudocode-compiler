#include "cps/FileHandler.h"
#include "cps/RuntimeCheck.h"
#include "cps/ExceptionHandler.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace cps;

// ptr __cps_file_read(ptr name, i32 line): read one line into a fresh heap
// STRING ('\n' consumed and stripped, trailing '\r' dropped). Reading at EOF
// throws an exception — CIE programs are expected to guard with EOF().
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
    Checker.emitCheck(NotRead, ErrNotRead, {Line, Name});
    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");

    Value *MinusOne = ConstantInt::get(Context, APInt(32, -1, /*isSigned=*/true));
    Value *Peek = Builder.CreateCall(FgetcFunc, {Handle}, "peek");
    Value *AtEof = Builder.CreateICmpEQ(Peek, MinusOne, "at_eof");
    Checker.emitCheck(AtEof, ErrPastEof, {Line, Name});
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
    Checker.emitCheck(IsReadMode, ErrNotWrite, {Line, Name});

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
    Builder.CreateCall(FreeFunc, {Builder.CreateLoad(PtrTy, emitSlotGEP(FileNames, PtrTy, Idx))});
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
    Checker.emitCheck(NotRead, ErrNotRead, {Line, Name});

    Value *Handle = Builder.CreateLoad(PtrTy, emitSlotGEP(FileHandles, PtrTy, Idx), "handle");
    Value *Ch = Builder.CreateCall(FgetcFunc, {Handle}, "c");
    // ungetc(EOF, f) is a defined no-op, so no branch is needed.
    Builder.CreateCall(UngetcFunc, {Ch, Handle});
    Value *IsEof = Builder.CreateICmpEQ(Ch, ConstantInt::get(Context, APInt(32, -1, /*isSigned=*/true)), "is_eof");
    Builder.CreateRet(IsEof);
}
