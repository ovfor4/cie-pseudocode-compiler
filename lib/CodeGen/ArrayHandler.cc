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

    std::vector<Value*> Lows;
    std::vector<Value*> Highs;
    std::vector<Value*> Dims;
    Value *TotalElements = ConstantInt::get(*TheContext, APInt(64, 1));

    for (const auto &Pair : Stmt->getBounds()) {
        Value *L = CG.emitExpr(Pair.first.get());
        Value *R = CG.emitExpr(Pair.second.get());
        L = CG.coerceValueToType(L, CG.resolveType("INTEGER"));
        R = CG.coerceValueToType(R, CG.resolveType("INTEGER"));

        if (!L || !R) return;

        Lows.push_back(L);
        Highs.push_back(R);

        Value *Diff = Builder->CreateSub(R, L, Name + "_dim_diff");
        Value *DimSize = Builder->CreateAdd(Diff, ConstantInt::get(*TheContext, APInt(64, 1)), Name + "_dim_size");
        Dims.push_back(DimSize);
        TotalElements = Builder->CreateMul(TotalElements, DimSize, Name + "_total_elems");
    }

    std::vector<Value*> Multipliers(Rank);
    Multipliers[Rank - 1] = ConstantInt::get(*TheContext, APInt(64, 1));
    for (int i = Rank - 2; i >= 0; --i) {
        Multipliers[i] = Builder->CreateMul(Multipliers[i + 1], Dims[i + 1], Name + "_mult");
    }

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
        Value *Idx = CG.emitExpr(IndexExprs[i].get());
        Idx = CG.coerceValueToType(Idx, CG.resolveType("INTEGER"));
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

bool ArrayHandler::tryEmitArrayOutput(ExprAST *Expr, CodeGen &CG) {
    std::string Name;
    const std::vector<std::unique_ptr<ExprAST>> *IndexExprs = nullptr;
    int Line = 0;

    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        Name = Var->getName();
        if (!getMetadata(Name)) return false;
    } else if (auto *Acc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        Name = Acc->getName();
        const ArrayMetadata *Meta = getMetadata(Name);
        if (!Meta) return false;
        if (Acc->getIndices().size() == static_cast<size_t>(Meta->Rank)) {
            return false; // full element access: handled by the normal expression path
        }
        IndexExprs = &Acc->getIndices();
        Line = Acc->getLine();
    } else {
        return false;
    }

    const TypeInfo *ElemInfo = Types.resolve(getMetadata(Name)->ElementTypeName);
    if (ElemInfo && !ElemInfo->isOutputtable()) {
        CG.reportError("Cannot OUTPUT array %s of type %s; output individual fields",
                       Name.c_str(), ElemInfo->Name.c_str());
        return true; // diagnosed
    }

    std::vector<Value*> Indices;
    if (IndexExprs && !emitCheckedIndices(Name, *IndexExprs, Line, false, CG, Indices)) {
        return true; // diagnosed
    }

    emitPrintLoop(Name, static_cast<int>(Indices.size()), Indices, CG);
    return true;
}

void ArrayHandler::emitPrintLoop(const std::string &Name,
                                 int CurrentDim,
                                 std::vector<Value*> CurrentIndices,
                                 CodeGen &CG) {
    const ArrayMetadata *Meta = getMetadata(Name);
    if (!Meta) return;

    if (CurrentDim == Meta->Rank) {
        Value *Offset = computeFlatIndex(Name, CurrentIndices);
        Value *Val = loadElement(Name, Offset, CG);
        if (!Val) return;

        CG.emitOutputValue(Val, Types.resolve(Meta->ElementTypeName), true);
        return;
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "arr_loop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "arr_after", TheFunction);

    AllocaInst *LoopVar = CG.CreateEntryBlockAlloca(TheFunction, Type::getInt64Ty(*TheContext), "idx");
    Value *Start = Meta->LowerBounds[CurrentDim];
    Value *End = Meta->UpperBounds[CurrentDim];

    Builder->CreateStore(Start, LoopVar);
    Builder->CreateBr(LoopBB);

    Builder->SetInsertPoint(LoopBB);
    Value *CurVal = Builder->CreateLoad(Type::getInt64Ty(*TheContext), LoopVar);

    std::vector<Value*> NextIndices = CurrentIndices;
    NextIndices.push_back(CurVal);
    emitPrintLoop(Name, CurrentDim + 1, NextIndices, CG);

    Value *NextVal = Builder->CreateAdd(CurVal, ConstantInt::get(*TheContext, APInt(64, 1)));
    Builder->CreateStore(NextVal, LoopVar);

    Value *Cond = Builder->CreateICmpSLE(NextVal, End);
    Builder->CreateCondBr(Cond, LoopBB, AfterBB);

    Builder->SetInsertPoint(AfterBB);
}
