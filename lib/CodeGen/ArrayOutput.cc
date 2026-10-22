#include "cps/ArrayHandler.h"
#include "cps/CodeGen.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;
using namespace cps;

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
        Value *Val = loadElement(Name, Offset);
        if (!Val) return;

        CG.emitOutputValue(Val, Types.resolve(Meta->ElementTypeName), true);
        return;
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    // Do-while shape: safe because every array has at least one element per
    // dimension (inverted bounds are rejected at declaration).
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
