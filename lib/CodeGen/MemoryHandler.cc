#include "cps/MemoryHandler.h"
#include "cps/ExceptionHandler.h"
#include "cps/RuntimeCheck.h"
#include "cps/TypeSystem.h"
#include <climits>
#include <cstddef>

using namespace llvm;
using namespace cps;

Function *MemoryHandler::createHelper(const char *Name, ArrayRef<Type *> Args) {
    auto *F = Function::Create(FunctionType::get(PointerType::getUnqual(Context), Args, false),
                               Function::InternalLinkage, Name, Module);
    Exceptions.prepareFunction(F);
    Builder.SetInsertPoint(BasicBlock::Create(Context, "entry", F));
    return F;
}

void MemoryHandler::ensureRuntime() {
    if (AllocateFn) return;
    IRBuilderBase::InsertPointGuard Guard(Builder);
    auto *Ptr = PointerType::getUnqual(Context);
    auto *I32 = Type::getInt32Ty(Context);
    SizeTy = Type::getIntNTy(Context, sizeof(std::size_t) * CHAR_BIT);
    NodeTy = StructType::create(Context, {Ptr, Ptr}, "__cps_memory_node");
    Head = new GlobalVariable(Module, Ptr, false, GlobalValue::InternalLinkage,
                              ConstantPointerNull::get(Ptr), "__cps_memory_head");
    Malloc = Module.getOrInsertFunction("malloc", FunctionType::get(Ptr, {SizeTy}, false));
    Free = Module.getOrInsertFunction("free", FunctionType::get(Type::getVoidTy(Context), {Ptr}, false));
    Realloc = Module.getOrInsertFunction("realloc", FunctionType::get(Ptr, {Ptr, SizeTy}, false));
    AllocationError = Builder.CreateGlobalString("[Fatal] line %d: Memory allocation failed\n",
                                                  "err_allocation", 0, &Module);
    TrackingError = Builder.CreateGlobalString("[Fatal] line %d: Allocation tracking failed\n",
                                                "err_tracking", 0, &Module);
    InvalidAddress = Builder.CreateGlobalString("[Fatal] line %d: Invalid memory address (not an active allocation)\n",
                                                 "err_memory_address", 0, &Module);
    FindFn = createHelper("__cps_memory_find", {Ptr, I32});
    buildFind();
    AllocateFn = createHelper("__cps_memory_allocate", {SizeTy, I32});
    buildAllocate();
    ReleaseFn = createHelper("__cps_memory_release", {Ptr, I32});
    buildRelease();
    ResizeFn = createHelper("__cps_memory_resize", {Ptr, SizeTy, I32});
    buildResize();
}

// Return the address of the list link that owns the matching node. This also
// lets release unlink the head or an interior node through the same operation.
void MemoryHandler::buildFind() {
    auto *Ptr = PointerType::getUnqual(Context);
    Value *Address = FindFn->getArg(0), *Line = FindFn->getArg(1);
    auto *Entry = Builder.GetInsertBlock();
    auto *Loop = BasicBlock::Create(Context, "search", FindFn);
    auto *Found = BasicBlock::Create(Context, "found", FindFn);
    Builder.CreateBr(Loop);
    Builder.SetInsertPoint(Loop);
    auto *Link = Builder.CreatePHI(Ptr, 2, "link");
    Link->addIncoming(Head, Entry);
    Value *Node = Builder.CreateLoad(Ptr, Link, "node");
    Checker.emitCheck(Builder.CreateIsNull(Node), InvalidAddress, {Line});
    Value *Stored = Builder.CreateLoad(Ptr, Builder.CreateStructGEP(NodeTy, Node, 0));
    Value *NextLink = Builder.CreateStructGEP(NodeTy, Node, 1);
    Link->addIncoming(NextLink, Builder.GetInsertBlock());
    Builder.CreateCondBr(Builder.CreateICmpEQ(Stored, Address), Found, Loop);
    Builder.SetInsertPoint(Found);
    Builder.CreateRet(Link);
}

void MemoryHandler::buildAllocate() {
    auto *Ptr = PointerType::getUnqual(Context);
    Value *Size = AllocateFn->getArg(0), *Line = AllocateFn->getArg(1);
    auto *Zero = BasicBlock::Create(Context, "zero_size", AllocateFn);
    auto *Allocate = BasicBlock::Create(Context, "allocate", AllocateFn);
    Builder.CreateCondBr(Builder.CreateICmpEQ(Size, ConstantInt::get(SizeTy, 0)), Zero, Allocate);
    Builder.SetInsertPoint(Zero);
    Builder.CreateRet(ConstantPointerNull::get(Ptr));
    Builder.SetInsertPoint(Allocate);
    Value *NodeSize = Builder.CreateIntCast(Types.getSizeOfConstant(NodeTy), SizeTy, false);
    Value *Node = Builder.CreateCall(Malloc, {NodeSize}, "new_node");
    Checker.emitCheck(Builder.CreateIsNull(Node), TrackingError, {Line});
    Value *Address = Builder.CreateCall(Malloc, {Size}, "allocation");
    Value *Failed = Builder.CreateIsNull(Address);
    auto *Cleanup = BasicBlock::Create(Context, "cleanup_node", AllocateFn);
    auto *Checked = BasicBlock::Create(Context, "check_allocation", AllocateFn);
    Builder.CreateCondBr(Failed, Cleanup, Checked);
    Builder.SetInsertPoint(Cleanup);
    Builder.CreateCall(Free, {Node});
    Builder.CreateBr(Checked);
    Builder.SetInsertPoint(Checked);
    Checker.emitCheck(Failed, AllocationError, {Line});
    Builder.CreateStore(Address, Builder.CreateStructGEP(NodeTy, Node, 0));
    Builder.CreateStore(Builder.CreateLoad(Ptr, Head), Builder.CreateStructGEP(NodeTy, Node, 1));
    Builder.CreateStore(Node, Head);
    Builder.CreateRet(Address);
}

void MemoryHandler::buildRelease() {
    auto *Ptr = PointerType::getUnqual(Context);
    Value *Address = ReleaseFn->getArg(0), *Line = ReleaseFn->getArg(1);
    auto *Done = BasicBlock::Create(Context, "done", ReleaseFn);
    auto *Release = BasicBlock::Create(Context, "release", ReleaseFn);
    Builder.CreateCondBr(Builder.CreateIsNull(Address), Done, Release);
    Builder.SetInsertPoint(Release);
    Value *Link = Exceptions.emitCall(FindFn, {Address, Line}, "link");
    Value *Node = Builder.CreateLoad(Ptr, Link);
    Value *Next = Builder.CreateLoad(Ptr, Builder.CreateStructGEP(NodeTy, Node, 1));
    Builder.CreateStore(Next, Link);
    Builder.CreateCall(Free, {Address});
    Builder.CreateCall(Free, {Node});
    Builder.CreateBr(Done);
    Builder.SetInsertPoint(Done);
    Builder.CreateRet(Address);
}

Value *MemoryHandler::emitAllocate(Value *Size, int Line) {
    ensureRuntime();
    return Exceptions.emitCall(AllocateFn, {Size, Builder.getInt32(Line)}, "allocated");
}

Value *MemoryHandler::emitRelease(Value *Address, int Line) {
    ensureRuntime();
    return Exceptions.emitCall(ReleaseFn, {Address, Builder.getInt32(Line)}, "released");
}

void MemoryHandler::buildResize() {
    auto *Ptr = PointerType::getUnqual(Context);
    Value *Address = ResizeFn->getArg(0), *Size = ResizeFn->getArg(1), *Line = ResizeFn->getArg(2);
    auto *Allocate = BasicBlock::Create(Context, "allocate", ResizeFn);
    auto *Validate = BasicBlock::Create(Context, "validate", ResizeFn);
    auto *Release = BasicBlock::Create(Context, "release", ResizeFn);
    auto *Resize = BasicBlock::Create(Context, "resize", ResizeFn);
    Builder.CreateCondBr(Builder.CreateIsNull(Address), Allocate, Validate);
    Builder.SetInsertPoint(Allocate);
    Builder.CreateRet(Exceptions.emitCall(AllocateFn, {Size, Line}, "allocation"));
    Builder.SetInsertPoint(Validate);
    Value *Link = Exceptions.emitCall(FindFn, {Address, Line}, "link");
    Value *Node = Builder.CreateLoad(Ptr, Link);
    Builder.CreateCondBr(Builder.CreateICmpEQ(Size, ConstantInt::get(SizeTy, 0)), Release, Resize);
    Builder.SetInsertPoint(Release);
    Exceptions.emitCall(ReleaseFn, {Address, Line});
    Builder.CreateRet(ConstantPointerNull::get(Ptr));
    Builder.SetInsertPoint(Resize);
    Value *NewAddress = Builder.CreateCall(Realloc, {Address, Size}, "resized");
    // libc preserves the original block on nonzero-size failure. Leave the
    // tracking entry untouched until the check has succeeded as well.
    Checker.emitCheck(Builder.CreateIsNull(NewAddress), AllocationError, {Line});
    Builder.CreateStore(NewAddress, Builder.CreateStructGEP(NodeTy, Node, 0));
    Builder.CreateRet(NewAddress);
}

Value *MemoryHandler::emitResize(Value *Address, Value *Size, int Line) {
    ensureRuntime();
    return Exceptions.emitCall(ResizeFn, {Address, Size, Builder.getInt32(Line)}, "resized");
}

void MemoryHandler::emitValidate(Value *Address, int Line) {
    ensureRuntime();
    Exceptions.emitCall(FindFn, {Address, Builder.getInt32(Line)});
}
