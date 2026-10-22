#include "cps/CodeGen.h"
#include "cps/RuntimeCheck.h"
#include "llvm/IR/Intrinsics.h"

using namespace llvm;
using namespace cps;

namespace {
// Defines (once per module) `ptr __cps_input_token()`: reads one whitespace-delimited token from
// stdin into a malloc'd NUL-terminated buffer, or returns null at end of input. After the token it
// skips spaces, tabs and carriage returns and consumes one newline if it follows, so the next
// INPUT of a STRING starts on the following line. The caller frees the buffer.
FunctionCallee getInputTokenHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *I8 = Type::getInt8Ty(Ctx), *I32 = Type::getInt32Ty(Ctx), *I64 = Type::getInt64Ty(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(Ptr, {}, false);
    if (Function *Existing = M.getFunction("__cps_input_token")) return {HelperTy, Existing};

    FunctionCallee Getchar = M.getOrInsertFunction("getchar", FunctionType::get(I32, {}, false));
    FunctionCallee Ungetc = M.getOrInsertFunction("ungetc", FunctionType::get(I32, {I32, Ptr}, false));
    FunctionCallee Isspace = M.getOrInsertFunction("isspace", FunctionType::get(I32, {I32}, false));
    FunctionCallee Realloc = M.getOrInsertFunction("realloc", FunctionType::get(Ptr, {Ptr, I64}, false));

    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_input_token", &M);
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
    BasicBlock *SkipHead = BasicBlock::Create(Ctx, "skip.head", F);
    BasicBlock *SkipTest = BasicBlock::Create(Ctx, "skip.test", F);
    BasicBlock *Append = BasicBlock::Create(Ctx, "append", F);
    BasicBlock *Grow = BasicBlock::Create(Ctx, "grow", F);
    BasicBlock *Store = BasicBlock::Create(Ctx, "store", F);
    BasicBlock *ReadTest = BasicBlock::Create(Ctx, "read.test", F);
    BasicBlock *Finish = BasicBlock::Create(Ctx, "finish", F);
    BasicBlock *TrailHead = BasicBlock::Create(Ctx, "trail.head", F);
    BasicBlock *TrailSkip = BasicBlock::Create(Ctx, "trail.skip", F);
    BasicBlock *TrailEnd = BasicBlock::Create(Ctx, "trail.end", F);
    BasicBlock *PushBack = BasicBlock::Create(Ctx, "pushback", F);
    BasicBlock *Done = BasicBlock::Create(Ctx, "done", F);
    BasicBlock *Eof = BasicBlock::Create(Ctx, "eof", F);

    IRBuilder<> B(Entry);
    Value *Buf = B.CreateAlloca(Ptr, nullptr, "buf");
    Value *Len = B.CreateAlloca(I64, nullptr, "len");
    Value *Cap = B.CreateAlloca(I64, nullptr, "cap");
    Value *Cur = B.CreateAlloca(I32, nullptr, "cur");
    B.CreateStore(ConstantPointerNull::get(Ptr), Buf);
    B.CreateStore(ConstantInt::get(I64, 0), Len);
    B.CreateStore(ConstantInt::get(I64, 0), Cap);
    B.CreateBr(SkipHead);

    // Leading whitespace.
    B.SetInsertPoint(SkipHead);
    Value *First = B.CreateCall(Getchar);
    B.CreateStore(First, Cur);
    B.CreateCondBr(B.CreateICmpEQ(First, ConstantInt::get(I32, -1)), Eof, SkipTest);
    B.SetInsertPoint(SkipTest);
    Value *IsSpace = B.CreateICmpNE(B.CreateCall(Isspace, {First}), ConstantInt::get(I32, 0));
    B.CreateCondBr(IsSpace, SkipHead, Append);

    // Append Cur to the buffer, keeping room for the byte and the terminator.
    B.SetInsertPoint(Append);
    Value *L = B.CreateLoad(I64, Len), *C = B.CreateLoad(I64, Cap);
    B.CreateCondBr(B.CreateICmpUGE(B.CreateAdd(L, ConstantInt::get(I64, 2)), C), Grow, Store);
    B.SetInsertPoint(Grow);
    Value *NewCap = B.CreateSelect(B.CreateICmpEQ(C, ConstantInt::get(I64, 0)),
                                   ConstantInt::get(I64, 32), B.CreateMul(C, ConstantInt::get(I64, 2)));
    B.CreateStore(B.CreateCall(Realloc, {B.CreateLoad(Ptr, Buf), NewCap}), Buf);
    B.CreateStore(NewCap, Cap);
    B.CreateBr(Store);
    B.SetInsertPoint(Store);
    Value *StoreLen = B.CreateLoad(I64, Len);
    Value *Slot = B.CreateGEP(I8, B.CreateLoad(Ptr, Buf), {StoreLen});
    B.CreateStore(B.CreateTrunc(B.CreateLoad(I32, Cur), I8), Slot);
    B.CreateStore(B.CreateAdd(StoreLen, ConstantInt::get(I64, 1)), Len);
    Value *Next = B.CreateCall(Getchar);
    B.CreateStore(Next, Cur);
    B.CreateCondBr(B.CreateICmpEQ(Next, ConstantInt::get(I32, -1)), Finish, ReadTest);
    B.SetInsertPoint(ReadTest);
    Value *NextSpace = B.CreateICmpNE(B.CreateCall(Isspace, {Next}), ConstantInt::get(I32, 0));
    B.CreateCondBr(NextSpace, Finish, Append);

    // Terminate, then skip trailing spaces/tabs/CRs and one newline.
    B.SetInsertPoint(Finish);
    Value *EndLen = B.CreateLoad(I64, Len);
    B.CreateStore(ConstantInt::get(I8, 0), B.CreateGEP(I8, B.CreateLoad(Ptr, Buf), {EndLen}));
    B.CreateBr(TrailHead);
    B.SetInsertPoint(TrailHead);
    Value *Ch = B.CreateLoad(I32, Cur);
    Value *Blank = B.CreateOr(B.CreateOr(B.CreateICmpEQ(Ch, ConstantInt::get(I32, ' ')),
                                         B.CreateICmpEQ(Ch, ConstantInt::get(I32, '\t'))),
                              B.CreateICmpEQ(Ch, ConstantInt::get(I32, '\r')));
    B.CreateCondBr(Blank, TrailSkip, TrailEnd);
    B.SetInsertPoint(TrailSkip);
    B.CreateStore(B.CreateCall(Getchar), Cur);
    B.CreateBr(TrailHead);
    B.SetInsertPoint(TrailEnd);
    Value *End = B.CreateLoad(I32, Cur);
    Value *KeepsNothing = B.CreateOr(B.CreateICmpEQ(End, ConstantInt::get(I32, '\n')),
                                     B.CreateICmpEQ(End, ConstantInt::get(I32, -1)));
    B.CreateCondBr(KeepsNothing, Done, PushBack);
    B.SetInsertPoint(PushBack);
    // The stdin FILE* is a platform-specific symbol; host and target match.
#if defined(_WIN32)
    FunctionCallee IobFunc = M.getOrInsertFunction("__acrt_iob_func", FunctionType::get(Ptr, {I32}, false));
    Value *Stdin = B.CreateCall(IobFunc, {ConstantInt::get(I32, 0)});
#else
#if defined(__APPLE__)
    const char *StdinName = "__stdinp";
#else
    const char *StdinName = "stdin";
#endif
    GlobalVariable *StdinVar = M.getGlobalVariable(StdinName);
    if (!StdinVar)
        StdinVar = new GlobalVariable(M, Ptr, false, GlobalValue::ExternalLinkage, nullptr, StdinName);
    Value *Stdin = B.CreateLoad(Ptr, StdinVar);
#endif
    B.CreateCall(Ungetc, {End, Stdin});
    B.CreateBr(Done);
    B.SetInsertPoint(Done);
    B.CreateRet(B.CreateLoad(Ptr, Buf));
    B.SetInsertPoint(Eof);
    B.CreateRet(ConstantPointerNull::get(Ptr));
    return {HelperTy, F};
}

// Defines `i1 __cps_parse_int(ptr token, ptr out)`: an optionally signed decimal integer that fills
// the whole token and fits in INTEGER (what strtoll accepts in base 10, without errno).
FunctionCallee getParseIntHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *I1 = Type::getInt1Ty(Ctx), *I8 = Type::getInt8Ty(Ctx), *I64 = Type::getInt64Ty(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(I1, {Ptr, Ptr}, false);
    if (Function *Existing = M.getFunction("__cps_parse_int")) return {HelperTy, Existing};

    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_parse_int", &M);
    Value *Token = F->getArg(0), *Out = F->getArg(1);
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
    BasicBlock *Head = BasicBlock::Create(Ctx, "head", F), *Body = BasicBlock::Create(Ctx, "body", F);
    BasicBlock *Tail = BasicBlock::Create(Ctx, "tail", F), *Negate = BasicBlock::Create(Ctx, "negate", F);
    BasicBlock *Finish = BasicBlock::Create(Ctx, "finish", F), *Fail = BasicBlock::Create(Ctx, "fail", F);

    IRBuilder<> B(Entry);
    Value *Acc = B.CreateAlloca(I64, nullptr, "acc");
    Value *Pos = B.CreateAlloca(Ptr, nullptr, "pos");
    Value *Sign = B.CreateLoad(I8, Token);
    Value *IsNeg = B.CreateICmpEQ(Sign, ConstantInt::get(I8, '-'));
    Value *HasSign = B.CreateOr(IsNeg, B.CreateICmpEQ(Sign, ConstantInt::get(I8, '+')));
    Value *Start = B.CreateGEP(I8, Token, {B.CreateZExt(HasSign, I64)});
    B.CreateStore(ConstantInt::get(I64, 0), Acc);
    B.CreateStore(Start, Pos);
    B.CreateBr(Head);

    // Accumulate negatively so INTEGER's minimum is representable.
    B.SetInsertPoint(Head);
    Value *P = B.CreateLoad(Ptr, Pos);
    Value *Digit = B.CreateSub(B.CreateLoad(I8, P), ConstantInt::get(I8, '0'));
    B.CreateCondBr(B.CreateICmpULT(Digit, ConstantInt::get(I8, 10)), Body, Tail);
    B.SetInsertPoint(Body);
    Value *Old = B.CreateLoad(I64, Acc);
    Value *Mul = B.CreateBinaryIntrinsic(Intrinsic::smul_with_overflow, Old, ConstantInt::get(I64, 10));
    Value *Sub = B.CreateBinaryIntrinsic(Intrinsic::ssub_with_overflow, B.CreateExtractValue(Mul, 0),
                                         B.CreateZExt(Digit, I64));
    Value *Overflow = B.CreateOr(B.CreateExtractValue(Mul, 1), B.CreateExtractValue(Sub, 1));
    B.CreateStore(B.CreateExtractValue(Sub, 0), Acc);
    B.CreateStore(B.CreateGEP(I8, P, {ConstantInt::get(I64, 1)}), Pos);
    BasicBlock *Next = BasicBlock::Create(Ctx, "next", F);
    B.CreateCondBr(Overflow, Fail, Next);
    B.SetInsertPoint(Next);
    B.CreateBr(Head);

    B.SetInsertPoint(Tail);
    Value *AtEnd = B.CreateICmpEQ(B.CreateLoad(I8, P), ConstantInt::get(I8, 0));
    Value *HasDigits = B.CreateICmpNE(P, Start);
    BasicBlock *Sign2 = BasicBlock::Create(Ctx, "sign", F);
    B.CreateCondBr(B.CreateAnd(AtEnd, HasDigits), Sign2, Fail);
    B.SetInsertPoint(Sign2);
    Value *Magnitude = B.CreateLoad(I64, Acc);
    B.CreateCondBr(IsNeg, Finish, Negate);
    B.SetInsertPoint(Negate);
    // +x needs -acc, which overflows only for INTEGER's minimum.
    BasicBlock *Positive = BasicBlock::Create(Ctx, "positive", F);
    B.CreateCondBr(B.CreateICmpEQ(Magnitude, ConstantInt::get(I64, APInt::getSignedMinValue(64))), Fail, Positive);
    B.SetInsertPoint(Positive);
    B.CreateStore(B.CreateNeg(Magnitude), Out);
    B.CreateRet(ConstantInt::getTrue(Ctx));
    B.SetInsertPoint(Finish);
    B.CreateStore(Magnitude, Out);
    B.CreateRet(ConstantInt::getTrue(Ctx));
    B.SetInsertPoint(Fail);
    B.CreateRet(ConstantInt::getFalse(Ctx));
    return {HelperTy, F};
}

// Defines `i1 __cps_parse_uint(ptr token, ptr out)`: an optionally '+'-signed decimal that fills the
// whole token and fits in 64 unsigned bits. Negative numbers are rejected.
FunctionCallee getParseUIntHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *I1 = Type::getInt1Ty(Ctx), *I8 = Type::getInt8Ty(Ctx), *I64 = Type::getInt64Ty(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(I1, {Ptr, Ptr}, false);
    if (Function *Existing = M.getFunction("__cps_parse_uint")) return {HelperTy, Existing};

    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_parse_uint", &M);
    Value *Token = F->getArg(0), *Out = F->getArg(1);
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
    BasicBlock *Head = BasicBlock::Create(Ctx, "head", F), *Body = BasicBlock::Create(Ctx, "body", F);
    BasicBlock *Next = BasicBlock::Create(Ctx, "next", F), *Tail = BasicBlock::Create(Ctx, "tail", F);
    BasicBlock *Ok = BasicBlock::Create(Ctx, "ok", F), *Fail = BasicBlock::Create(Ctx, "fail", F);

    IRBuilder<> B(Entry);
    Value *Acc = B.CreateAlloca(I64, nullptr, "acc");
    Value *Pos = B.CreateAlloca(Ptr, nullptr, "pos");
    Value *HasSign = B.CreateICmpEQ(B.CreateLoad(I8, Token), ConstantInt::get(I8, '+'));
    Value *Start = B.CreateGEP(I8, Token, {B.CreateZExt(HasSign, I64)});
    B.CreateStore(ConstantInt::get(I64, 0), Acc);
    B.CreateStore(Start, Pos);
    B.CreateBr(Head);

    B.SetInsertPoint(Head);
    Value *P = B.CreateLoad(Ptr, Pos);
    Value *Digit = B.CreateSub(B.CreateLoad(I8, P), ConstantInt::get(I8, '0'));
    B.CreateCondBr(B.CreateICmpULT(Digit, ConstantInt::get(I8, 10)), Body, Tail);
    B.SetInsertPoint(Body);
    Value *Mul = B.CreateBinaryIntrinsic(Intrinsic::umul_with_overflow, B.CreateLoad(I64, Acc),
                                         ConstantInt::get(I64, 10));
    Value *Add = B.CreateBinaryIntrinsic(Intrinsic::uadd_with_overflow, B.CreateExtractValue(Mul, 0),
                                         B.CreateZExt(Digit, I64));
    Value *Overflow = B.CreateOr(B.CreateExtractValue(Mul, 1), B.CreateExtractValue(Add, 1));
    B.CreateStore(B.CreateExtractValue(Add, 0), Acc);
    B.CreateStore(B.CreateGEP(I8, P, {ConstantInt::get(I64, 1)}), Pos);
    B.CreateCondBr(Overflow, Fail, Next);
    B.SetInsertPoint(Next);
    B.CreateBr(Head);

    B.SetInsertPoint(Tail);
    Value *AtEnd = B.CreateICmpEQ(B.CreateLoad(I8, P), ConstantInt::get(I8, 0));
    B.CreateCondBr(B.CreateAnd(AtEnd, B.CreateICmpNE(P, Start)), Ok, Fail);
    B.SetInsertPoint(Ok);
    B.CreateStore(B.CreateLoad(I64, Acc), Out);
    B.CreateRet(ConstantInt::getTrue(Ctx));
    B.SetInsertPoint(Fail);
    B.CreateRet(ConstantInt::getFalse(Ctx));
    return {HelperTy, F};
}

// Defines `i1 __cps_parse_real(ptr token, ptr out)`: the rule of parseNumber in pseudocode-lib/
// string.cc (shared by IS_NUM and STR_TO_NUM): strtod must consume the whole token and must not
// overflow to infinity. A finite overflow is told from an "inf" literal by the missing letter i.
FunctionCallee getParseRealHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *I1 = Type::getInt1Ty(Ctx), *I8 = Type::getInt8Ty(Ctx), *I32 = Type::getInt32Ty(Ctx);
    Type *DoubleTy = Type::getDoubleTy(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(I1, {Ptr, Ptr}, false);
    if (Function *Existing = M.getFunction("__cps_parse_real")) return {HelperTy, Existing};

    FunctionCallee Strtod = M.getOrInsertFunction("strtod", FunctionType::get(DoubleTy, {Ptr, Ptr}, false));
    FunctionCallee Strchr = M.getOrInsertFunction("strchr", FunctionType::get(Ptr, {Ptr, I32}, false));
    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_parse_real", &M);
    Value *Token = F->getArg(0), *Out = F->getArg(1);
    IRBuilder<> B(BasicBlock::Create(Ctx, "entry", F));
    Value *EndSlot = B.CreateAlloca(Ptr, nullptr, "end");
    Value *Number = B.CreateCall(Strtod, {Token, EndSlot});
    Value *Consumed = B.CreateICmpEQ(B.CreateLoad(I8, B.CreateLoad(Ptr, EndSlot)), ConstantInt::get(I8, 0));
    Value *Progress = B.CreateICmpNE(B.CreateLoad(Ptr, EndSlot), Token);
    Value *IsInf = B.CreateFCmpOEQ(B.CreateUnaryIntrinsic(Intrinsic::fabs, Number),
                                   ConstantFP::getInfinity(DoubleTy));
    Value *NoLowerI = B.CreateIsNull(B.CreateCall(Strchr, {Token, ConstantInt::get(I32, 'i')}));
    Value *NoUpperI = B.CreateIsNull(B.CreateCall(Strchr, {Token, ConstantInt::get(I32, 'I')}));
    Value *Overflowed = B.CreateAnd(IsInf, B.CreateAnd(NoLowerI, NoUpperI));
    B.CreateStore(Number, Out);
    B.CreateRet(B.CreateAnd(B.CreateAnd(Consumed, Progress), B.CreateNot(Overflowed)));
    return {HelperTy, F};
}

// Defines `i32 __cps_parse_bool(ptr token)`: 1 for TRUE, 0 for FALSE in any case, -1 otherwise.
FunctionCallee getParseBoolHelper(Module &M) {
    LLVMContext &Ctx = M.getContext();
    Type *I8 = Type::getInt8Ty(Ctx), *I32 = Type::getInt32Ty(Ctx), *I64 = Type::getInt64Ty(Ctx);
    PointerType *Ptr = PointerType::getUnqual(Ctx);
    FunctionType *HelperTy = FunctionType::get(I32, {Ptr}, false);
    if (Function *Existing = M.getFunction("__cps_parse_bool")) return {HelperTy, Existing};

    FunctionCallee Strlen = M.getOrInsertFunction("strlen", FunctionType::get(I64, {Ptr}, false));
    Function *F = Function::Create(HelperTy, Function::InternalLinkage, "__cps_parse_bool", &M);
    Value *Token = F->getArg(0);
    BasicBlock *Entry = BasicBlock::Create(Ctx, "entry", F);
    BasicBlock *IsTrue = BasicBlock::Create(Ctx, "true", F), *IsFalse = BasicBlock::Create(Ctx, "false", F);
    BasicBlock *Other = BasicBlock::Create(Ctx, "other", F), *Not4 = BasicBlock::Create(Ctx, "not4", F);
    IRBuilder<> B(Entry);
    Value *Length = B.CreateCall(Strlen, {Token});
    // ORing 0x20 folds ASCII upper case to lower case; no other byte folds onto a letter.
    auto Matches = [&](const char *Word) {
        Value *All = ConstantInt::getTrue(Ctx);
        for (unsigned I = 0; Word[I]; ++I) {
            Value *Byte = B.CreateOr(B.CreateLoad(I8, B.CreateGEP(I8, Token, {ConstantInt::get(I64, I)})),
                                     ConstantInt::get(I8, 0x20));
            All = B.CreateAnd(All, B.CreateICmpEQ(Byte, ConstantInt::get(I8, Word[I])));
        }
        return All;
    };
    BasicBlock *Check4 = BasicBlock::Create(Ctx, "check4", F), *Check5 = BasicBlock::Create(Ctx, "check5", F);
    B.CreateCondBr(B.CreateICmpEQ(Length, ConstantInt::get(I64, 4)), Check4, Not4);
    B.SetInsertPoint(Check4);
    B.CreateCondBr(Matches("true"), IsTrue, Other);
    B.SetInsertPoint(Not4);
    B.CreateCondBr(B.CreateICmpEQ(Length, ConstantInt::get(I64, 5)), Check5, Other);
    B.SetInsertPoint(Check5);
    B.CreateCondBr(Matches("false"), IsFalse, Other);
    B.SetInsertPoint(IsTrue);
    B.CreateRet(ConstantInt::get(I32, 1));
    B.SetInsertPoint(IsFalse);
    B.CreateRet(ConstantInt::get(I32, 0));
    B.SetInsertPoint(Other);
    B.CreateRet(ConstantInt::get(I32, -1));
    return {HelperTy, F};
}
} // namespace

// Reads one token from stdin and stores it, validated, at Addr. A malformed token or the end of
// input is a runtime error. C scalars follow the same token model; integers must fit the C type.
void CodeGen::emitScalarInput(Value *Addr, const TypeInfo *Info, int Line) {
    Module &M = *TheModule;
    Type *I32 = Type::getInt32Ty(*TheContext), *I64 = Type::getInt64Ty(*TheContext);
    Type *DoubleTy = Type::getDoubleTy(*TheContext);
    PointerType *Ptr = PointerType::getUnqual(*TheContext);
    FunctionCallee Free = M.getOrInsertFunction("free", FunctionType::get(Type::getVoidTy(*TheContext), {Ptr}, false));
    FunctionCallee Strlen = M.getOrInsertFunction("strlen", FunctionType::get(I64, {Ptr}, false));

    Value *Token = Builder->CreateCall(getInputTokenHelper(M), {}, "input_token");
    RuntimeChecker->emitCheck(Builder->CreateIsNull(Token),
        Builder->CreateGlobalString("[Fatal] line %d: INPUT reached the end of input\n", "err_input_eof", 0, &M),
        {RuntimeChecker->lineValue(Line)});
    Value *TypeName = Builder->CreateGlobalString(Info->Name, "input_type_name", 0, &M);
    Value *InvalidMsg = Builder->CreateGlobalString("[Fatal] line %d: Invalid %s input '%s'\n",
                                                    "err_input_invalid", 0, &M);
    auto Require = [&](Value *Valid) {
        RuntimeChecker->emitCheck(Builder->CreateNot(Valid), InvalidMsg,
                                  {RuntimeChecker->lineValue(Line), TypeName, Token});
    };
    Function *Fn = Builder->GetInsertBlock()->getParent();

    if (Info->isChar()) {
        Require(Builder->CreateICmpEQ(Builder->CreateCall(Strlen, {Token}), ConstantInt::get(I64, 1)));
        Builder->CreateStore(Builder->CreateLoad(Info->LLVMType, Token), Addr);
    } else if (Info->isBoolean() && !Info->isC()) {
        Value *Parsed = Builder->CreateCall(getParseBoolHelper(M), {Token});
        Require(Builder->CreateICmpSGE(Parsed, ConstantInt::get(I32, 0)));
        Builder->CreateStore(Builder->CreateICmpNE(Parsed, ConstantInt::get(I32, 0)), Addr);
    } else if (Info->isReal()) {
        AllocaInst *Slot = CreateEntryBlockAlloca(Fn, DoubleTy, "input_real");
        Require(Builder->CreateCall(getParseRealHelper(M), {Token, Slot}));
        Value *Number = Builder->CreateLoad(DoubleTy, Slot);
        Type *Target = Info->LLVMType;
        if (Target->isFloatTy()) Number = Builder->CreateFPTrunc(Number, Target);
        else if (Target != DoubleTy) Number = Builder->CreateFPExt(Number, Target);
        Builder->CreateStore(Number, Addr);
    } else {
        AllocaInst *Slot = CreateEntryBlockAlloca(Fn, I64, "input_int");
        bool Unsigned = Info->isC() && !Info->isBoolean() && !Info->CSigned;
        Require(Builder->CreateCall(Unsigned ? getParseUIntHelper(M) : getParseIntHelper(M), {Token, Slot}));
        Value *Number = Builder->CreateLoad(I64, Slot);
        if (Info->isBoolean()) {
            Builder->CreateStore(Builder->CreateICmpNE(Number, ConstantInt::get(I64, 0)), Addr);
        } else {
            unsigned Bits = Info->LLVMType->getIntegerBitWidth();
            Value *Stored = Number;
            if (Bits < 64) {
                Stored = Builder->CreateTrunc(Number, Info->LLVMType);
                Value *Back = Unsigned ? Builder->CreateZExt(Stored, I64) : Builder->CreateSExt(Stored, I64);
                Require(Builder->CreateICmpEQ(Back, Number));
            }
            Builder->CreateStore(Stored, Addr);
        }
    }
    Builder->CreateCall(Free, {Token});
}
