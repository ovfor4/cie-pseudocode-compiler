#include "cps/CodeGen.h"
#include "cps/ExceptionHandler.h"
#include "cps/TypeAST.h"
#include <climits>
#include <cstddef>

using namespace llvm;
using namespace cps;

bool CodeGen::isObjectLValue(ExprAST *Expr) const {
    return dynamic_cast<VariableExprAST *>(Expr) || dynamic_cast<ArrayAccessExprAST *>(Expr) ||
           dynamic_cast<DesignatorExprAST *>(Expr);
}

void CodeGen::transferObjects(const std::vector<size_t> &Objects) {
    for (size_t Index : Objects)
        if (Index != size_t(-1)) Builder->CreateStore(Builder->getFalse(), StackObjects[Index].Alive);
}

Value *CodeGen::emitOwnedValue(ExprAST *Expr, const TypeInfo *Type,
                               std::vector<size_t> &Transfers) {
    expectResult(Expr, Type);
    if (!Type || (!Type->IsClass && !containsString(Type))) return emitCoercedExpr(Expr, Type);
    const TypeInfo *Source = getExprTypeInfo(Expr);
    // A CHAR argument becomes a fresh STRING temporary, owned like any other.
    bool FromChar = Source && Type->isString() && Source->Kind == TypeKind::Char;
    if (!Source || (!Source->sameType(Type) && !FromChar)) return emitCoercedExpr(Expr, Type);
    Value *Value = nullptr;
    if (!FromChar && isObjectLValue(Expr)) {
        LValueInfo LV;
        const auto *Var = dynamic_cast<VariableExprAST *>(Expr);
        const auto *Symbol = Var ? getSymbolInfo(Var->getName()) : nullptr;
        if (Symbol && Symbol->isConstant() && Symbol->Storage) {
            LV = {symbolAddress(*Symbol), Symbol->TypeName};
        } else if (!emitLValue(Expr, LV)) return nullptr;
        return copyClassValue(Type, LV.Addr, Transfers);
    }
    Value = emitCoercedExpr(Expr, Type);
    if (!Value) return nullptr;
    // A pending argument owns its result until all remaining arguments succeed.
    auto *Storage = materializeOperatorReceiver(Value, Type);
    if (typeNeedsDestruction(Type->Name)) Transfers.push_back(StackObjects.size() - 1);
    return Builder->CreateLoad(Type->LLVMType, Storage);
}

Value *CodeGen::copyClassValue(const TypeInfo *Type, Value *Address,
                                std::vector<size_t> &Transfers) {
    const FuncSig *Copy = nullptr;
    for (const FuncSig *Sig : FuncGen->getOverloads(Type->Name + ".NEW"))
        if (Sig->IsOverload && Sig->Params.size() == 1 && Sig->Params[0].IsByRef &&
            !Sig->Params[0].IsArray && Type->sameType(resolveType(Sig->Params[0].TypeName)))
            Copy = Sig;
    if (!Copy) {
        if (!containsString(Type) || Type->isString())
            return Builder->CreateLoad(Type->LLVMType, Address);
        Value *Result = UndefValue::get(Type->LLVMType);
        std::vector<size_t> Fields;
        for (const auto &[Name, Field] : Type->Record.Fields) {
            const auto *FieldType = resolveType(Field.TypeName);
            Value *Source = Builder->CreateStructGEP(Type->LLVMType, Address, Field.Index);
            Value *Value = containsString(FieldType)
                ? copyClassValue(FieldType, Source, Fields)
                : Builder->CreateLoad(FieldType->LLVMType, Source);
            if (!Value) return nullptr;
            Result = Builder->CreateInsertValue(Result, Value, {Field.Index});
        }
        auto *Storage = materializeOperatorReceiver(Result, Type);
        transferObjects(Fields);
        if (typeNeedsDestruction(Type->Name)) Transfers.push_back(StackObjects.size() - 1);
        return Builder->CreateLoad(Type->LLVMType, Storage);
    }
    if (!checkOperatorAccess(*Copy)) return nullptr;
    auto *Storage = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                           Type->LLVMType, "copy_object");
    size_t Lifetime = registerStackObject(Storage, Type->Name, DiagnosticLine);
    if (Lifetime != size_t(-1)) StackObjects[Lifetime].Temporary = true;
    Builder->CreateStore(Types->getZeroValue(Type->Name), Storage);
    if (!emitFieldDefaults(Type, Storage, DiagnosticLine)) return nullptr;
    if (!FuncGen->emitCall(*Copy, {Storage, Address})) return nullptr;
    activateStackObject(Lifetime);
    Transfers.push_back(Lifetime);
    return Builder->CreateLoad(Type->LLVMType, Storage, "copied_value");
}

void CodeGen::cleanupTemporaries(size_t Start) {
    if (!Builder->GetInsertBlock() || Builder->GetInsertBlock()->hasTerminator()) return;
    // Destructor emission can itself register temporaries. Copy each record before emitting.
    for (size_t I = StackObjects.size(); I > Start; --I) {
        auto Object = StackObjects[I - 1];
        if (Object.Temporary) emitStackObjectCleanup(Object);
    }
}

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
    if (Info->asRecord() && !emitFieldDefaults(Info, Addr, Line)) return false;
    if (findMethod(TypeName, "NEW"))
        return emitMethodCall(TypeName, Addr, "NEW", Args, Line) != nullptr;
    if (!Args.empty()) {
        reportError("Type %s has no constructor NEW but arguments were supplied", TypeName.c_str());
        return false;
    }
    return true;
}

// Guide 10.2 creation pattern: a plain DECLARE only runs NEW when the class has
// a parameterless one; otherwise the object stays zeroed (fields constructed)
// and a later `x <- NEW C(...)` binds it.
bool CodeGen::emitDefaultConstruct(const std::string &TypeName, Value *Addr, int Line) {
    const TypeInfo *Info = resolveType(TypeName);
    if (!Info) return false;
    for (const FuncSig *Sig : FuncGen->getOverloads(TypeName + ".NEW"))
        if (Sig->Params.empty()) return emitConstruct(TypeName, Addr, {}, Line);
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    return !Info->asRecord() || emitFieldDefaults(Info, Addr, Line);
}

// Default-constructs, in declaration order, each field of the record at Addr
// that needs construction.
bool CodeGen::emitFieldDefaults(const TypeInfo *Type, Value *Addr, int Line) {
    for (const auto &[Name, Field] : Type->Record.Fields) {
        if (!typeNeedsConstruction(Field.TypeName)) continue;
        Value *FieldAddr = Builder->CreateStructGEP(Type->LLVMType, Addr, Field.Index);
        if (!emitDefaultConstruct(Field.TypeName, FieldAddr, Line)) return false;
    }
    return true;
}

Value *CodeGen::symbolAddress(const SymbolInfo &Info) {
    if (!Info.IsReference) return Info.Storage;
    return Builder->CreateLoad(PointerType::getUnqual(*TheContext), Info.Storage, "object_address");
}

void CodeGen::emitAssignment(const LValueInfo &Target, ExprAST *Expr, const SymbolInfo *Symbol) {
    const TypeInfo *Info = resolveType(Target.TypeName);
    expectResult(Expr, Info);
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
        Value *Object = emitMemoryCall("_memory_impl_malloc", Size, New->getLine());
        if (!Object) return;
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
    const TypeInfo *ExprType = getExprTypeInfo(Expr);
    // A CHAR assigned to a STRING is converted first, not bound to OPERATOR<-.
    bool CharToString = Info && Info->isString() && ExprType && ExprType->Kind == TypeKind::Char;
    if (Info && Info->IsClass && !CharToString) {
        auto Resolution = resolveOperator(tok_assign, Info, ExprType);
        if (Resolution.Ambiguous) {
            reportError("Ambiguous assignment operator");
            return;
        }
        if (const FuncSig *Sig = Resolution.Signature) {
            if (!checkOperatorAccess(*Sig)) return;
            std::vector<Value *> Args = {Target.Addr};
            if (marshalCallArgs(*Sig, std::vector<ExprAST *>{Expr}, DiagnosticLine, Args))
                FuncGen->emitCall(*Sig, Args);
            return;
        }
    }
    if (Info && containsString(Info)) {
        Value *Val = emitCoercedExpr(Expr, Info);
        if (!Val) return;
        std::vector<size_t> Transfers;
        // Only a value of the same type is an existing object to copy; a
        // converted one is already a fresh temporary.
        bool CopySource = isObjectLValue(Expr) && ExprType && ExprType->sameType(Info);
        if (!CopySource) {
            materializeOperatorReceiver(Val, Info);
            if (typeNeedsDestruction(Info->Name)) Transfers.push_back(StackObjects.size() - 1);
        }
        // Default aggregate assignment replaces fields without reconstructing
        // the enclosing class or calling its DELETE method.
        std::function<Value *(const TypeInfo *, Value *)> Clone =
            [&](const TypeInfo *Type, Value *Value) -> llvm::Value * {
                if (Type->isString()) {
                    auto *Source = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                                          Type->LLVMType, "assignment_source");
                    Builder->CreateStore(Value, Source);
                    return copyClassValue(Type, Source, Transfers);
                }
                for (const auto &[Name, Field] : Type->Record.Fields) {
                    const auto *FieldType = resolveType(Field.TypeName);
                    if (!containsString(FieldType)) continue;
                    auto *Copy = Clone(FieldType, Builder->CreateExtractValue(Value, {Field.Index}));
                    if (!Copy) return nullptr;
                    Value = Builder->CreateInsertValue(Value, Copy, {Field.Index});
                }
                return Value;
            };
        if (CopySource) Val = Clone(Info, Val);
        if (!Val) return;
        std::function<bool(const TypeInfo *, Value *)> Release =
            [&](const TypeInfo *Type, Value *Address) {
                if (Type->isString()) return emitDestroy(Type->Name, Address, DiagnosticLine);
                for (const auto &[Name, Field] : Type->Record.Fields) {
                    const auto *FieldType = resolveType(Field.TypeName);
                    if (containsString(FieldType) && !Release(FieldType,
                        Builder->CreateStructGEP(Type->LLVMType, Address, Field.Index))) return false;
                }
                return true;
            };
        if (!Release(Info, Target.Addr)) return;
        Builder->CreateStore(Val, Target.Addr);
        transferObjects(Transfers);
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
                                     bool IsArray) {
    // Arrays always register: their buffer is freed when the scope ends.
    if (!IsArray && !typeNeedsDestruction(TypeName)) return size_t(-1);
    auto *F = Builder->GetInsertBlock()->getParent();
    IRBuilder<> Entry(&F->getEntryBlock(), F->getEntryBlock().begin());
    auto *Alive = Entry.CreateAlloca(Entry.getInt1Ty(), nullptr, "object_alive");
    Entry.CreateStore(Entry.getFalse(), Alive);
    AllocaInst *Count = nullptr;
    if (IsArray) {
        Count = Entry.CreateAlloca(Entry.getInt64Ty(), nullptr, "object_count");
        Entry.CreateStore(Entry.getInt64(0), Count);
    }
    StackObjects.push_back({Addr, TypeName, Alive, Count, Line, CurrentUnit});
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
    size_t SavedUnit = CurrentUnit;
    selectUnit(Object.OwnerUnit);
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
        auto *Free = BasicBlock::Create(*TheContext, "free_array", F);
        if (typeNeedsDestruction(Object.TypeName)) {
            Value *Count = Builder->CreateLoad(Builder->getInt64Ty(), Object.Count);
            auto *Preheader = Builder->GetInsertBlock();
            auto *Loop = BasicBlock::Create(*TheContext, "destroy_elements", F);
            auto *Body = BasicBlock::Create(*TheContext, "destroy_element", F);
            Builder->CreateBr(Loop);
            Builder->SetInsertPoint(Loop);
            auto *Index = Builder->CreatePHI(Builder->getInt64Ty(), 2);
            Index->addIncoming(Count, Preheader);
            Builder->CreateCondBr(Builder->CreateICmpNE(Index, Builder->getInt64(0)), Body, Free);
            Builder->SetInsertPoint(Body);
            Value *Previous = Builder->CreateSub(Index, Builder->getInt64(1));
            Value *Element = Builder->CreateGEP(resolveType(Object.TypeName)->LLVMType, Base, Previous);
            emitDestroy(Object.TypeName, Element, Object.Line);
            Index->addIncoming(Previous, Builder->GetInsertBlock());
            Builder->CreateBr(Loop);
        } else {
            Builder->CreateBr(Free);
        }
        // Release the buffer; the slot is nulled so a later cleanup of the
        // same (inactive) object can never free it twice.
        Builder->SetInsertPoint(Free);
        FunctionCallee FreeFunc = TheModule->getOrInsertFunction(
            "free", FunctionType::get(Builder->getVoidTy(), {PointerType::getUnqual(*TheContext)}, false));
        Builder->CreateCall(FreeFunc, {Base});
        Builder->CreateStore(ConstantPointerNull::get(PointerType::getUnqual(*TheContext)), Object.Addr);
    }
    Builder->CreateBr(Done);
    Builder->SetInsertPoint(Done);
    selectUnit(SavedUnit);
}

void CodeGen::finishStackObjects(Function *F) {
    Exceptions->fillCleanups(F, [this](bool TemporariesOnly) {
        for (auto It = StackObjects.rbegin(); It != StackObjects.rend(); ++It)
            if (!TemporariesOnly || It->Temporary) emitStackObjectCleanup(*It);
    });
}
