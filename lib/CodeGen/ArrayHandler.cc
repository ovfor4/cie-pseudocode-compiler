#include "cps/ArrayHandler.h"
#include "llvm/IR/Intrinsics.h"
#include "cps/CodeGen.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace cps;

ArrayHandler::ArrayHandler(LLVMContext &C,
                           IRBuilder<> &B,
                           Module &M,
                           IdentifierMap<SymbolInfo> &Sym,
                           TypeSystem &TS,
                           RuntimeCheck &RC)
    : TheContext(&C),
      Builder(&B),
      TheModule(&M),
      Symbols(&Sym),
      Types(TS),
      RuntimeChecker(RC) {
    setupExternalFunctions();
}

void ArrayHandler::setupExternalFunctions() {
    std::vector<Type*> MallocArgs;
    MallocArgs.push_back(Type::getInt64Ty(*TheContext));

    FunctionType *MallocType = FunctionType::get(PointerType::getUnqual(*TheContext), MallocArgs, false);
    MallocFunc = TheModule->getOrInsertFunction("malloc", MallocType);
}

const ArrayMetadata *ArrayHandler::getMetadata(const std::string &Name) const {
    auto Symbol = Symbols->find(Name);
    if (Symbol == Symbols->end() || !Symbol->second.IsArray) return nullptr;
    auto It = ArrayTable.find(Name);
    if (It == ArrayTable.end()) {
        return nullptr;
    }
    return &It->second;
}

Value *ArrayHandler::computeFlatIndex(const std::string &Name, const std::vector<Value*> &Indices) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta) return nullptr;

    Value *Offset = ConstantInt::get(*TheContext, APInt(64, 0));
    for (size_t i = 0; i < Indices.size(); ++i) {
        Value *Idx = Indices[i];
        Value *Lower = Meta->LowerBounds[i];
        Value *Diff = Builder->CreateSub(Idx, Lower);
        Value *Term = Builder->CreateMul(Diff, Meta->Multipliers[i]);
        Offset = Builder->CreateAdd(Offset, Term);
    }
    return Offset;
}

Value *ArrayHandler::getArrayBasePointer(const std::string &Name) {
    auto It = Symbols->find(Name);
    if (It == Symbols->end() || !It->second.Storage) {
        return nullptr;
    }
    return Builder->CreateLoad(PointerType::getUnqual(*TheContext), It->second.Storage, (Name + "_raw").c_str());
}

Value *ArrayHandler::getElementPointer(const std::string &Name, Value *Offset) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta || !Offset) return nullptr;

    Value *RawPtr = getArrayBasePointer(Name);
    if (!RawPtr) return nullptr;

    PointerType *TypedPtrTy = PointerType::getUnqual(Meta->ElementType);
    Value *TypedPtr = Builder->CreateBitCast(RawPtr, TypedPtrTy, Name + "_typed_ptr");
    return Builder->CreateGEP(Meta->ElementType, TypedPtr, Offset, Name + "_elem_ptr");
}

void ArrayHandler::emitArrayDeclare(ArrayDeclareStmtAST *Stmt, CodeGen &CG) {
    std::string Name = Stmt->getName();

    const TypeInfo *ElemInfo = Types.resolve(Stmt->getType());
    if (!ElemInfo || !ElemInfo->LLVMType || ElemInfo->isVoid()) {
        CG.reportError("Unknown array element type %s", Stmt->getType().c_str());
        return;
    }
    if (Types.lookupEnumConstant(Name)) {
        CG.reportError("'%s' is an enum value and cannot be declared as an array", Name.c_str());
        return;
    }
    if (Types.resolve(Name)) {
        CG.reportError("'%s' is a type name and cannot be declared as an array", Name.c_str());
        return;
    }

    if (!CG.prepareSymbolDeclaration(Name)) return;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    AllocaInst *Alloca = CG.CreateEntryBlockAlloca(TheFunction, PointerType::getUnqual(*TheContext), Name);
    size_t Lifetime = CG.registerStackObject(Alloca, ElemInfo->Name, Stmt->getLine(), true);

    std::vector<Value*> Lows;
    std::vector<Value*> Highs;

    for (const auto &Pair : Stmt->getBounds()) {
        Value *L = CG.emitCoercedExpr(Pair.first.get(), CG.resolveType("INTEGER"));
        Value *R = CG.emitCoercedExpr(Pair.second.get(), CG.resolveType("INTEGER"));

        if (!L || !R) return;

        auto *ConstL = dyn_cast<ConstantInt>(L);
        auto *ConstR = dyn_cast<ConstantInt>(R);
        if (ConstL && ConstR && ConstR->getSExtValue() < ConstL->getSExtValue()) {
            CG.reportError("Array %s upper bound %lld is less than lower bound %lld", Name.c_str(),
                           static_cast<long long>(ConstR->getSExtValue()),
                           static_cast<long long>(ConstL->getSExtValue()));
            return;
        }

        Lows.push_back(L);
        Highs.push_back(R);
    }

    Value *TotalElements = nullptr;
    const ArrayMetadata &Meta =
        defineMetadata(Name, ElemInfo, std::move(Lows), std::move(Highs), TotalElements);

    Value *TotalBytes = emitCheckedByteSize(Name, Meta.LowerBounds, Meta.UpperBounds,
                                            Meta.ElementSizeC, Stmt->getLine());
    CallInst *Ptr = Builder->CreateCall(MallocFunc, TotalBytes, Name + "_malloc");
    emitAllocCheck(Ptr, Name, Stmt->getLine());

    FunctionType *MemsetType = FunctionType::get(PointerType::getUnqual(*TheContext),
                                                 {PointerType::getUnqual(*TheContext), Type::getInt32Ty(*TheContext), Type::getInt64Ty(*TheContext)},
                                                 false);
    FunctionCallee MemsetFunc = TheModule->getOrInsertFunction("memset", MemsetType);
    Builder->CreateCall(MemsetFunc,
                        {Ptr, ConstantInt::get(Type::getInt32Ty(*TheContext), 0), TotalBytes});

    Builder->CreateStore(Ptr, Alloca);

    CG.registerSymbol(Name, Alloca, ElemInfo->Name, true);
    if (CG.typeNeedsConstruction(ElemInfo->Name)) {
        BasicBlock *Preheader = Builder->GetInsertBlock();
        BasicBlock *Loop = BasicBlock::Create(*TheContext, "construct_elements", TheFunction);
        BasicBlock *Body = BasicBlock::Create(*TheContext, "construct_element", TheFunction);
        BasicBlock *Done = BasicBlock::Create(*TheContext, "constructed_array", TheFunction);
        Builder->CreateBr(Loop);
        Builder->SetInsertPoint(Loop);
        auto *Index = Builder->CreatePHI(Type::getInt64Ty(*TheContext), 2, "construct_index");
        Index->addIncoming(ConstantInt::get(Type::getInt64Ty(*TheContext), 0), Preheader);
        Builder->CreateCondBr(Builder->CreateICmpULT(Index, TotalElements), Body, Done);
        Builder->SetInsertPoint(Body);
        Value *Element = Builder->CreateGEP(ElemInfo->LLVMType, Ptr, Index);
        CG.emitDefaultConstruct(ElemInfo->Name, Element, Stmt->getLine());
        Value *Next = Builder->CreateAdd(Index, ConstantInt::get(Type::getInt64Ty(*TheContext), 1));
        Index->addIncoming(Next, Builder->GetInsertBlock());
        Builder->CreateBr(Loop);
        Builder->SetInsertPoint(Done);
    }
    CG.activateStackObject(Lifetime, TotalElements);
}

Value *ArrayHandler::emitCheckedByteSize(const std::string &Name,
                                         const std::vector<Value*> &Lows,
                                         const std::vector<Value*> &Highs,
                                         Constant *ElementSize, int Line) {
    Type *I64 = Type::getInt64Ty(*TheContext);
    Value *NameStr = Builder->CreateGlobalString(Name, "array_name", 0, TheModule);
    Value *InvalidMsg = Builder->CreateGlobalString(
        "[Fatal] line %d: Invalid bounds [%lld:%lld] for array %s\n", "err_array_bounds", 0, TheModule);
    Value *TooLargeMsg = Builder->CreateGlobalString(
        "[Fatal] line %d: Array %s is too large\n", "err_array_large", 0, TheModule);

    // Pass 1: upper >= lower in every dimension (this also keeps the
    // differences below non-negative, so only the subtraction can overflow).
    for (size_t I = 0; I < Lows.size(); ++I) {
        if (isa<ConstantInt>(Lows[I]) && isa<ConstantInt>(Highs[I])) continue; // checked at compile time
        Value *Inverted = Builder->CreateICmpSLT(Highs[I], Lows[I], Name + "_inverted");
        RuntimeChecker.emitCheck(Inverted, InvalidMsg, {RuntimeChecker.lineValue(Line), Lows[I], Highs[I], NameStr});
    }

    // Pass 2: sizes with overflow detection.
    auto Overflowing = [&](Intrinsic::ID Id, Value *A, Value *B, Value *&Overflow) {
        Value *Pair = Builder->CreateBinaryIntrinsic(Id, A, B);
        Value *Flag = Builder->CreateExtractValue(Pair, 1);
        Overflow = Overflow ? Builder->CreateOr(Overflow, Flag) : Flag;
        return Builder->CreateExtractValue(Pair, 0);
    };
    Value *Overflow = nullptr;
    Value *Total = ConstantInt::get(I64, 1);
    for (size_t I = 0; I < Lows.size(); ++I) {
        Value *Diff = Overflowing(Intrinsic::ssub_with_overflow, Highs[I], Lows[I], Overflow);
        Value *Dim = Builder->CreateAdd(Diff, ConstantInt::get(I64, 1), Name + "_dim_size");
        Total = Overflowing(Intrinsic::umul_with_overflow, Total, Dim, Overflow);
    }
    Value *Bytes = Overflowing(Intrinsic::umul_with_overflow, Total, ElementSize, Overflow);
    RuntimeChecker.emitCheck(Overflow, TooLargeMsg, {RuntimeChecker.lineValue(Line), NameStr});
    return Bytes;
}

void ArrayHandler::emitAllocCheck(Value *Ptr, const std::string &Name, int Line) {
    Value *NameStr = Builder->CreateGlobalString(Name, "array_name", 0, TheModule);
    Value *TooLargeMsg = Builder->CreateGlobalString(
        "[Fatal] line %d: Array %s is too large\n", "err_array_large", 0, TheModule);
    Value *IsNull = Builder->CreateICmpEQ(
        Ptr, ConstantPointerNull::get(PointerType::getUnqual(*TheContext)), Name + "_alloc_failed");
    RuntimeChecker.emitCheck(IsNull, TooLargeMsg, {RuntimeChecker.lineValue(Line), NameStr});
}

const ArrayMetadata &ArrayHandler::defineMetadata(const std::string &Name, const TypeInfo *ElemInfo,
                                                 std::vector<Value*> Lows,
                                                 std::vector<Value*> Highs,
                                                 Value *&TotalElements) {
    std::vector<Value*> Multipliers;
    computeDimsAndMultipliers(Name, Lows, Highs, Multipliers, TotalElements);

    ArrayMetadata Meta;
    Meta.Rank = static_cast<int>(Lows.size());
    Meta.ElementTypeName = ElemInfo->Name;
    Meta.ElementType = ElemInfo->LLVMType;
    Meta.ElementSizeC = Types.getSizeOfConstant(ElemInfo->LLVMType);
    Meta.LowerBounds = std::move(Lows);
    Meta.UpperBounds = std::move(Highs);
    Meta.Multipliers = std::move(Multipliers);
    return ArrayTable[Name] = std::move(Meta);
}

void ArrayHandler::computeDimsAndMultipliers(const std::string &Name,
                                             const std::vector<Value*> &Lows,
                                             const std::vector<Value*> &Highs,
                                             std::vector<Value*> &Multipliers,
                                             Value *&TotalElements) {
    int Rank = static_cast<int>(Lows.size());
    std::vector<Value*> Dims;
    TotalElements = ConstantInt::get(*TheContext, APInt(64, 1));

    for (int I = 0; I < Rank; ++I) {
        Value *Diff = Builder->CreateSub(Highs[I], Lows[I], Name + "_dim_diff");
        Value *DimSize = Builder->CreateAdd(Diff, ConstantInt::get(*TheContext, APInt(64, 1)),
                                            Name + "_dim_size");
        Dims.push_back(DimSize);
        TotalElements = Builder->CreateMul(TotalElements, DimSize, Name + "_total_elems");
    }

    Multipliers.assign(Rank, nullptr);
    Multipliers[Rank - 1] = ConstantInt::get(*TheContext, APInt(64, 1));
    for (int I = Rank - 2; I >= 0; --I) {
        Multipliers[I] = Builder->CreateMul(Multipliers[I + 1], Dims[I + 1], Name + "_mult");
    }
}

bool ArrayHandler::emitCheckedIndices(const std::string &Name,
                                      const std::vector<std::unique_ptr<ExprAST>> &IndexExprs,
                                      int Line,
                                      bool RequireExact,
                                      CodeGen &CG,
                                      std::vector<Value*> &Out) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta) {
        CG.reportError("Undeclared array %s", Name.c_str());
        return false;
    }

    size_t Rank = static_cast<size_t>(Meta->Rank);
    if (IndexExprs.size() > Rank || (RequireExact && IndexExprs.size() != Rank)) {
        CG.reportError("Incorrect number of indices for %s", Name.c_str());
        return false;
    }

    for (size_t i = 0; i < IndexExprs.size(); ++i) {
        Value *Idx = CG.emitCoercedExpr(IndexExprs[i].get(), CG.resolveType("INTEGER"));
        if (!Idx) return false;

        Out.push_back(Idx);
        RuntimeChecker.emitIndexCheck(Idx, Meta->LowerBounds[i], Meta->UpperBounds[i], Line);
    }
    return true;
}

Value *ArrayHandler::loadElement(const std::string &Name, Value *Offset) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta) return nullptr;

    Value *ElemPtr = getElementPointer(Name, Offset);
    if (!ElemPtr) return nullptr;

    Value *Val = Builder->CreateLoad(Meta->ElementType, ElemPtr, Name + "_elem");
    return Val;
}

Value *ArrayHandler::emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG) {
    std::string Name = Expr->getName();
    std::vector<Value*> Indices;
    if (!emitCheckedIndices(Name, Expr->getIndices(), Expr->getLine(), true, CG, Indices))
        return nullptr;

    Value *Offset = computeFlatIndex(Name, Indices);
    return loadElement(Name, Offset);
}

void ArrayHandler::emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG) {
    std::string ElemTypeName;
    Value *ElemPtr = emitElementAddress(Stmt->getName(), Stmt->getIndices(),
                                        Stmt->getLine(), CG, ElemTypeName);
    if (!ElemPtr) return;

    CG.emitAssignment({ElemPtr, ElemTypeName}, Stmt->getExpr());
}

Value *ArrayHandler::emitElementAddress(const std::string &Name,
                                        const std::vector<std::unique_ptr<ExprAST>> &IndexExprs,
                                        int Line,
                                        CodeGen &CG,
                                        std::string &ElemTypeNameOut) {
    std::vector<Value*> Indices;
    if (!emitCheckedIndices(Name, IndexExprs, Line, true, CG, Indices))
        return nullptr;

    Value *Offset = computeFlatIndex(Name, Indices);
    Value *ElemPtr = getElementPointer(Name, Offset);
    if (!ElemPtr) return nullptr;

    ElemTypeNameOut = getMetadata(Name)->ElementTypeName;
    return ElemPtr;
}

Value *ArrayHandler::emitElementAddress(ArrayAccessExprAST *Expr,
                                        CodeGen &CG,
                                        std::string &ElemTypeNameOut) {
    return emitElementAddress(Expr->getName(), Expr->getIndices(), Expr->getLine(),
                              CG, ElemTypeNameOut);
}

IdentifierMap<ArrayMetadata> ArrayHandler::exchangeTable(IdentifierMap<ArrayMetadata> NewTable) {
    std::swap(ArrayTable, NewTable);
    return NewTable;
}
