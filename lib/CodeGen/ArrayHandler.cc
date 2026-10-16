#include "cps/ArrayHandler.h"
#include "cps/CodeGen.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

ArrayHandler::ArrayHandler(LLVMContext &C,
                           IRBuilder<> &B,
                           Module &M,
                           std::map<std::string, SymbolInfo> &Sym,
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
    int Rank = static_cast<int>(Stmt->getBounds().size());

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

    std::vector<Value*> Lows;
    std::vector<Value*> Highs;

    for (const auto &Pair : Stmt->getBounds()) {
        Value *L = CG.emitCoercedExpr(Pair.first.get(), CG.resolveType("INTEGER"));
        Value *R = CG.emitCoercedExpr(Pair.second.get(), CG.resolveType("INTEGER"));

        if (!L || !R) return;

        Lows.push_back(L);
        Highs.push_back(R);
    }

    std::vector<Value*> Multipliers;
    Value *TotalElements = nullptr;
    computeDimsAndMultipliers(Name, Lows, Highs, Multipliers, TotalElements);

    ArrayMetadata Meta;
    Meta.Rank = Rank;
    Meta.ElementTypeName = ElemInfo->Name;
    Meta.ElementType = ElemInfo->LLVMType;
    Meta.ElementSizeC = Types.getSizeOfConstant(ElemInfo->LLVMType);
    Meta.LowerBounds = std::move(Lows);
    Meta.UpperBounds = std::move(Highs);
    Meta.Multipliers = std::move(Multipliers);
    ArrayTable[Name] = Meta;

    Value *TotalBytes = Builder->CreateMul(TotalElements, Meta.ElementSizeC, Name + "_total_bytes");
    CallInst *Ptr = Builder->CreateCall(MallocFunc, TotalBytes, Name + "_malloc");

    FunctionType *MemsetType = FunctionType::get(PointerType::getUnqual(*TheContext),
                                                 {PointerType::getUnqual(*TheContext), Type::getInt32Ty(*TheContext), Type::getInt64Ty(*TheContext)},
                                                 false);
    FunctionCallee MemsetFunc = TheModule->getOrInsertFunction("memset", MemsetType);
    Builder->CreateCall(MemsetFunc,
                        {Ptr, ConstantInt::get(Type::getInt32Ty(*TheContext), 0), TotalBytes});

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    AllocaInst *Alloca = CG.CreateEntryBlockAlloca(TheFunction, PointerType::getUnqual(*TheContext), Name);
    Builder->CreateStore(Ptr, Alloca);

    CG.registerSymbol(Name, Alloca, ElemInfo->Name, true);
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

Value *ArrayHandler::loadElement(const std::string &Name, Value *Offset, CodeGen &CG) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta) return nullptr;

    Value *ElemPtr = getElementPointer(Name, Offset);
    if (!ElemPtr) return nullptr;

    Value *Val = Builder->CreateLoad(Meta->ElementType, ElemPtr, Name + "_elem");
    if (Meta->ElementTypeName == "STRING") {
        Val = CG.emitStringNullGuard(Val);
    }
    return Val;
}

Value *ArrayHandler::emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG) {
    std::string Name = Expr->getName();
    std::vector<Value*> Indices;
    if (!emitCheckedIndices(Name, Expr->getIndices(), Expr->getLine(), true, CG, Indices))
        return nullptr;

    Value *Offset = computeFlatIndex(Name, Indices);
    return loadElement(Name, Offset, CG);
}

void ArrayHandler::emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG) {
    std::string ElemTypeName;
    Value *ElemPtr = emitElementAddress(Stmt->getName(), Stmt->getIndices(),
                                        Stmt->getLine(), CG, ElemTypeName);
    if (!ElemPtr) return;

    Value *Val = CG.emitCoercedExpr(Stmt->getExpr(), Types.resolve(ElemTypeName));
    if (!Val) return;

    Builder->CreateStore(Val, ElemPtr);
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

std::map<std::string, ArrayMetadata> ArrayHandler::exchangeTable(std::map<std::string, ArrayMetadata> NewTable) {
    std::swap(ArrayTable, NewTable);
    return NewTable;
}
