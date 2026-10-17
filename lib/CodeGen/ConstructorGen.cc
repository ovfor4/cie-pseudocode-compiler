#include "cps/CodeGen.h"
#include "cps/MemoryHandler.h"
#include "cps/TypeAST.h"
#include <climits>
#include <cstddef>

using namespace llvm;
using namespace cps;

bool CodeGen::typeNeedsConstruction(const std::string &TypeName) const {
    if (findMethod(TypeName, "NEW")) return true;
    const TypeInfo *Info = resolveType(TypeName);
    if (Info && Info->asRecord()) {
        for (const auto &[Name, Field] : Info->Record.Fields)
            if (typeNeedsConstruction(Field.TypeName)) return true;
    }
    return false;
}

bool CodeGen::emitConstruct(const std::string &TypeName, Value *Addr,
                            const std::vector<std::unique_ptr<ExprAST>> &Args, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    const TypeInfo *Info = resolveType(TypeName);
    if (!Info) return false;
    if (Info->asRecord()) {
        for (const auto &[Name, Field] : Info->Record.Fields) {
            if (!typeNeedsConstruction(Field.TypeName)) continue;
            Value *FieldAddr = Builder->CreateStructGEP(Info->LLVMType, Addr, Field.Index);
            if (!emitDefaultConstruct(Field.TypeName, FieldAddr, Line)) return false;
        }
    }
    if (findMethod(TypeName, "NEW"))
        return emitMethodCall(TypeName, Addr, "NEW", Args, Line) != nullptr;
    if (!Args.empty()) {
        reportError("Type %s has no constructor NEW but arguments were supplied", TypeName.c_str());
        return false;
    }
    return true;
}

bool CodeGen::emitDefaultConstruct(const std::string &TypeName, Value *Addr, int Line) {
    return emitConstruct(TypeName, Addr, {}, Line);
}

Value *CodeGen::symbolAddress(const SymbolInfo &Info) {
    if (!Info.IsReference) return Info.Storage;
    return Builder->CreateLoad(PointerType::getUnqual(*TheContext), Info.Storage, "object_address");
}

void CodeGen::emitAssignment(const LValueInfo &Target, ExprAST *Expr, const SymbolInfo *Symbol) {
    const TypeInfo *Info = resolveType(Target.TypeName);
    if (auto *New = dynamic_cast<NewExprAST*>(Expr)) {
        SaveAndRestore<int> Location(DiagnosticLine, New->getLine());
        const TypeInfo *Class = resolveType(New->getClassName());
        if (!Class || !Class->IsClass) {
            reportError("NEW requires a class type: %s", New->getClassName().c_str());
            return;
        }
        if (!Info || !Info->sameType(Class)) {
            reportError("NEW target must have type %s", Class->Name.c_str());
            return;
        }
        Value *Size = Builder->CreateIntCast(Types->getSizeOfConstant(Class->LLVMType),
                         Type::getIntNTy(*TheContext, sizeof(std::size_t) * CHAR_BIT), false);
        // Method-only classes have zero-sized LLVM structs but still need
        // a non-null object address (MALLOC(0) deliberately returns null).
        Size = Builder->CreateSelect(Builder->CreateICmpEQ(Size, ConstantInt::get(Size->getType(), 0)),
                                      ConstantInt::get(Size->getType(), 1), Size);
        Value *Object = Memory->emitAllocate(Size, New->getLine());
        Builder->CreateStore(Types->getZeroValue(Class->Name), Object);
        if (!emitConstruct(Class->Name, Object, New->getArgs(), New->getLine())) return;
        if (Symbol && Symbol->IsReference) {
            Builder->CreateStore(Object, Symbol->Storage);
        } else {
            Value *Val = Builder->CreateLoad(Class->LLVMType, Object);
            Builder->CreateStore(Val, Target.Addr);
        }
        return;
    }
    Value *Val = emitCoercedExpr(Expr, Info);
    if (Val) Builder->CreateStore(Val, Target.Addr);
}

bool CodeGen::typeNeedsDestruction(const std::string &TypeName) const {
    if (findMethod(TypeName, "DELETE")) return true;
    const TypeInfo *Info = resolveType(TypeName);
    if (Info && Info->asRecord()) {
        for (const auto &[Name, Field] : Info->Record.Fields)
            if (typeNeedsDestruction(Field.TypeName)) return true;
    }
    return false;
}

bool CodeGen::emitDestroy(const std::string &TypeName, Value *Addr, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    const TypeInfo *Info = resolveType(TypeName);
    if (!Info) return false;
    if (findMethod(TypeName, "DELETE") &&
        !emitMethodCall(TypeName, Addr, "DELETE", {}, Line)) return false;
    if (Info->asRecord()) {
        for (auto It = Info->Record.Fields.rbegin(); It != Info->Record.Fields.rend(); ++It) {
            const auto &Field = It->second;
            if (!typeNeedsDestruction(Field.TypeName)) continue;
            Value *FieldAddr = Builder->CreateStructGEP(Info->LLVMType, Addr, Field.Index);
            if (!emitDestroy(Field.TypeName, FieldAddr, Line)) return false;
        }
    }
    return true;
}

size_t CodeGen::registerStackObject(Value *Addr, const std::string &TypeName, int Line,
                                     bool IsArray, bool IsCopy) {
    if (!typeNeedsDestruction(TypeName)) return size_t(-1);
    auto *F = Builder->GetInsertBlock()->getParent();
    IRBuilder<> Entry(&F->getEntryBlock(), F->getEntryBlock().begin());
    auto *Alive = Entry.CreateAlloca(Entry.getInt1Ty(), nullptr, "object_alive");
    Entry.CreateStore(Entry.getFalse(), Alive);
    AllocaInst *Count = nullptr;
    if (IsArray) {
        Count = Entry.CreateAlloca(Entry.getInt64Ty(), nullptr, "object_count");
        Entry.CreateStore(Entry.getInt64(0), Count);
    }
    StackObjects.push_back({Addr, TypeName, Alive, Count, Line, IsCopy});
    // This DECLARE can execute repeatedly, including after a backward loop edge.
    emitStackObjectCleanup(StackObjects.back());
    return StackObjects.size() - 1;
}

void CodeGen::activateStackObject(size_t Index, Value *Count) {
    if (Index == size_t(-1)) return;
    const auto &Object = StackObjects[Index];
    if (Object.Count) Builder->CreateStore(Count, Object.Count);
    Builder->CreateStore(Builder->getTrue(), Object.Alive);
}

void CodeGen::emitStackObjectCleanup(const StackObject &Object) {
    auto *F = Builder->GetInsertBlock()->getParent();
    auto *Destroy = BasicBlock::Create(*TheContext, "destroy_object", F);
    auto *Done = BasicBlock::Create(*TheContext, "destroyed_object", F);
    Builder->CreateCondBr(Builder->CreateLoad(Builder->getInt1Ty(), Object.Alive), Destroy, Done);
    Builder->SetInsertPoint(Destroy);
    // Clear before the user method: a throwing destructor must never run twice.
    Builder->CreateStore(Builder->getFalse(), Object.Alive);
    if (!Object.Count) {
        emitDestroy(Object.TypeName, Object.Addr, Object.Line);
    } else {
        Value *Base = Builder->CreateLoad(PointerType::getUnqual(*TheContext), Object.Addr);
        Value *Count = Builder->CreateLoad(Builder->getInt64Ty(), Object.Count);
        auto *Preheader = Builder->GetInsertBlock();
        auto *Loop = BasicBlock::Create(*TheContext, "destroy_elements", F);
        auto *Body = BasicBlock::Create(*TheContext, "destroy_element", F);
        Builder->CreateBr(Loop);
        Builder->SetInsertPoint(Loop);
        auto *Index = Builder->CreatePHI(Builder->getInt64Ty(), 2);
        Index->addIncoming(Count, Preheader);
        Builder->CreateCondBr(Builder->CreateICmpNE(Index, Builder->getInt64(0)), Body, Done);
        Builder->SetInsertPoint(Body);
        Value *Previous = Builder->CreateSub(Index, Builder->getInt64(1));
        Value *Element = Builder->CreateGEP(resolveType(Object.TypeName)->LLVMType, Base, Previous);
        emitDestroy(Object.TypeName, Element, Object.Line);
        Index->addIncoming(Previous, Builder->GetInsertBlock());
        Builder->CreateBr(Loop);
        Builder->SetInsertPoint(Done);
        return;
    }
    Builder->CreateBr(Done);
    Builder->SetInsertPoint(Done);
}

void CodeGen::finishStackObjects(Function *F) {
    IRBuilderBase::InsertPointGuard Guard(*Builder);
    std::vector<Instruction *> Exits;
    for (auto &BB : *F)
        if (BB.hasTerminator() && isa<ReturnInst>(BB.getTerminator()))
            Exits.push_back(BB.getTerminator());
    for (auto *Exit : Exits) {
        // Keep the original terminator in a continuation while cleanup builds branches.
        auto *BB = Exit->getParent();
        auto *Continuation = BB->splitBasicBlock(Exit->getIterator(), "after_destruction");
        BB->getTerminator()->eraseFromParent();
        Builder->SetInsertPoint(BB);
        for (auto It = StackObjects.rbegin(); It != StackObjects.rend(); ++It)
            emitStackObjectCleanup(*It);
        Builder->CreateBr(Continuation);
    }
}
