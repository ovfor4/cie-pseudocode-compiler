#include "cps/ExceptionHandler.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include <cassert>

using namespace llvm;
using namespace cps;

void ExceptionHandler::ensureRuntime() {
    if (IntTypeInfo) return;
    auto *Ptr = PointerType::getUnqual(Context);
    auto *Void = Type::getVoidTy(Context);
    Allocate = Module.getOrInsertFunction("__cxa_allocate_exception",
        FunctionType::get(Ptr, {Type::getInt64Ty(Context)}, false));
    Throw = Module.getOrInsertFunction("__cxa_throw",
        FunctionType::get(Void, {Ptr, Ptr, Ptr}, false));
    BeginCatch = Module.getOrInsertFunction("__cxa_begin_catch",
        FunctionType::get(Ptr, {Ptr}, false));
    EndCatch = Module.getOrInsertFunction("__cxa_end_catch",
        FunctionType::get(Void, false));
#if defined(__SEH__)
    const char *PersonalityName = "__gxx_personality_seh0";
#elif defined(__USING_SJLJ_EXCEPTIONS__)
    const char *PersonalityName = "__gxx_personality_sj0";
#else
    const char *PersonalityName = "__gxx_personality_v0";
#endif
    Personality = Module.getOrInsertFunction(PersonalityName,
        FunctionType::get(Type::getInt32Ty(Context), true));
    IntTypeInfo = new GlobalVariable(Module, Ptr, true, GlobalValue::ExternalLinkage,
                                      nullptr, "_ZTIi");
    cast<Function>(Allocate.getCallee())->setDoesNotThrow();
    cast<Function>(BeginCatch.getCallee())->setDoesNotThrow();
    // Only anonymous integer exceptions are thrown; there is no throwing
    // exception-object destructor to invoke when a catch ends.
    cast<Function>(EndCatch.getCallee())->setDoesNotThrow();
    cast<Function>(Throw.getCallee())->setDoesNotReturn();
}

void ExceptionHandler::prepareFunction(Function *F) {
    F->setUWTableKind(UWTableKind::Default);
}

void ExceptionHandler::pushHandler(BasicBlock *Target) {
    auto *F = Builder.GetInsertBlock()->getParent();
    IRBuilder<> Entry(&F->getEntryBlock(), F->getEntryBlock().begin());
    auto *Exception = Entry.CreateAlloca(PointerType::getUnqual(Context), nullptr,
                                         "caught_exception");
    auto &State = States[F];
    State.Handlers.push_back({Target, Exception, State.CatchDepth});
}

AllocaInst *ExceptionHandler::popHandler() {
    auto &Handlers = States[Builder.GetInsertBlock()->getParent()].Handlers;
    assert(!Handlers.empty());
    auto *Exception = Handlers.back().Exception;
    Handlers.pop_back();
    return Exception;
}

void ExceptionHandler::beginCatch(AllocaInst *Exception) {
    ensureRuntime();
    auto *Ptr = Builder.CreateLoad(PointerType::getUnqual(Context), Exception);
    Builder.CreateCall(BeginCatch, {Ptr});
    ++States[Builder.GetInsertBlock()->getParent()].CatchDepth;
}

void ExceptionHandler::endCatch() {
    auto &Depth = States[Builder.GetInsertBlock()->getParent()].CatchDepth;
    assert(Depth != 0);
    --Depth;
    if (!Builder.GetInsertBlock()->hasTerminator()) Builder.CreateCall(EndCatch);
}

void ExceptionHandler::emitReturnCleanup() {
    auto &State = States[Builder.GetInsertBlock()->getParent()];
    for (unsigned I = 0; I < State.CatchDepth; ++I) Builder.CreateCall(EndCatch);
}

BasicBlock *ExceptionHandler::emitUnwindDestination() {
    auto *F = Builder.GetInsertBlock()->getParent();
    auto &State = States[F];
    if (State.Handlers.empty() && State.CatchDepth == 0) return nullptr;
    ensureRuntime();
    F->setPersonalityFn(cast<Constant>(Personality.getCallee()));
    auto Saved = Builder.saveIP();
    auto *Unwind = BasicBlock::Create(Context, "exception_unwind", F);
    Builder.SetInsertPoint(Unwind);
    auto *LPType = StructType::get(PointerType::getUnqual(Context), Type::getInt32Ty(Context));
    auto *LP = Builder.CreateLandingPad(LPType, 1, "exception");
    const Handler *Handler = State.Handlers.empty() ? nullptr : &State.Handlers.back();
    unsigned TargetDepth = Handler ? Handler->CatchDepth : 0;
    if (Handler) LP->addClause(ConstantPointerNull::get(PointerType::getUnqual(Context)));
    if (State.CatchDepth > TargetDepth) LP->setCleanup(true);
    // Finish catches we are leaving before entering the outer handler (or
    // resuming propagation through a function with no local handler).
    for (unsigned I = TargetDepth; I < State.CatchDepth; ++I) Builder.CreateCall(EndCatch);
    if (Handler) {
        Builder.CreateStore(Builder.CreateExtractValue(LP, 0), Handler->Exception);
        Builder.CreateBr(Handler->Target);
    } else {
        Builder.CreateResume(LP);
    }
    Builder.restoreIP(Saved);
    return Unwind;
}

Value *ExceptionHandler::emitCall(FunctionCallee Callee, ArrayRef<Value *> Args,
                                   const Twine &Name) {
    auto *Unwind = emitUnwindDestination();
    if (!Unwind) return Builder.CreateCall(Callee, Args, Name);
    auto *Cont = BasicBlock::Create(Context, "invoke_cont", Builder.GetInsertBlock()->getParent());
    auto *Call = Builder.CreateInvoke(Callee, Cont, Unwind, Args, Name);
    Builder.SetInsertPoint(Cont);
    return Call;
}

void ExceptionHandler::emitThrow() {
    ensureRuntime();
    // sizeof(int) is a target-folded null GEP, as for every other allocation.
    auto *I32 = Type::getInt32Ty(Context);
    auto *Null = ConstantPointerNull::get(PointerType::getUnqual(Context));
    auto *Size = ConstantExpr::getSizeOf(I32);
    auto *Object = Builder.CreateCall(Allocate, {Size}, "anonymous_exception");
    Builder.CreateStore(ConstantInt::get(I32, 0), Object);
    emitCall(Throw, {Object, IntTypeInfo, Null});
    Builder.CreateUnreachable();
}
