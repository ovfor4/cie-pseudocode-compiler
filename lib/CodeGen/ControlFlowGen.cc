#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/ExceptionHandler.h"

using namespace llvm;
using namespace cps;

void CodeGen::emitTryStmt(TryStmtAST *Stmt) {
    auto *F = Builder->GetInsertBlock()->getParent();
    auto *CatchBB = BasicBlock::Create(*TheContext, "catch", F);
    auto *AfterBB = BasicBlock::Create(*TheContext, "try_cont", F);
    auto SavedArrays = Arrays->snapshotTable();

    Exceptions->pushHandler(CatchBB);
    emitScopedBlock(Stmt->getBody());
    auto *Exception = Exceptions->popHandler();
    if (!Builder->GetInsertBlock()->hasTerminator()) Builder->CreateBr(AfterBB);
    Arrays->exchangeTable(SavedArrays);

    Builder->SetInsertPoint(CatchBB);
    Exceptions->beginCatch(Exception);
    emitScopedBlock(Stmt->getCatchBody());
    Exceptions->endCatch();
    if (!Builder->GetInsertBlock()->hasTerminator()) Builder->CreateBr(AfterBB);
    Arrays->exchangeTable(std::move(SavedArrays));
    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitIfStmt(IfStmtAST *Stmt) {
    Value *CondV = emitCoercedExpr(Stmt->getCond(), resolveType("BOOLEAN"));
    if (!CondV) return;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else", TheFunction);
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont", TheFunction);

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // Arrays are block-scoped: metadata SSA values and the buffer setup live
    // inside the branch, so a declaration must not survive it.
    auto SavedArrays = Arrays->snapshotTable();
    Builder->SetInsertPoint(ThenBB);
    emitScopedBlock(Stmt->getThenStmts());
    if (!Builder->GetInsertBlock()->hasTerminator()) Builder->CreateBr(MergeBB);
    Arrays->exchangeTable(SavedArrays);

    Builder->SetInsertPoint(ElseBB);
    emitScopedBlock(Stmt->getElseStmts());
    if (!Builder->GetInsertBlock()->hasTerminator()) Builder->CreateBr(MergeBB);
    Arrays->exchangeTable(std::move(SavedArrays));

    Builder->SetInsertPoint(MergeBB);
}

void CodeGen::emitWhileStmt(WhileStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "whilecond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "whileloop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "whilecont", TheFunction);

    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitCoercedExpr(Stmt->getCond(), resolveType("BOOLEAN"));
    if (!CondV) return;

    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    // Arrays declared in the body are block-scoped (see emitIfStmt).
    auto SavedArrays = Arrays->snapshotTable();
    Builder->SetInsertPoint(LoopBB);
    emitScopedBlock(Stmt->getBody());
    Arrays->exchangeTable(std::move(SavedArrays));

    if (!Builder->GetInsertBlock()->hasTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitRepeatStmt(RepeatStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "repeatloop", TheFunction);
    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "repeatcond", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "repeatcont", TheFunction);

    Builder->CreateBr(LoopBB);

    // Arrays declared in the body are block-scoped (see emitIfStmt).
    auto SavedArrays = Arrays->snapshotTable();
    Builder->SetInsertPoint(LoopBB);
    emitScopedBlock(Stmt->getBody());
    Arrays->exchangeTable(std::move(SavedArrays));
    if (!Builder->GetInsertBlock()->hasTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitCoercedExpr(Stmt->getCond(), resolveType("BOOLEAN"));
    if (!CondV) return;

    Builder->CreateCondBr(CondV, AfterBB, LoopBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitForStmt(ForStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    std::string VarName = Stmt->getVarName();

    const SymbolInfo *Symbol = getSymbolInfo(VarName);
    if (!Symbol) {
        reportError("Unknown variable in FOR loop %s", VarName.c_str());
        return;
    }
    if (!requireWritable(*Symbol, VarName)) return;
    if (Symbol->TypeName != "INTEGER") {
        reportError("FOR loop variable %s must be INTEGER", VarName.c_str());
        return;
    }

    Value *StartVal = emitCoercedExpr(Stmt->getStart(), resolveType("INTEGER"));
    if (!StartVal) return;

    Value *Alloca = Symbol->Storage;
    Builder->CreateStore(StartVal, Alloca);

    // STEP is evaluated exactly once, before the loop.
    Value *StepVal = nullptr;
    if (Stmt->getStep()) {
        StepVal = emitCoercedExpr(Stmt->getStep(), resolveType("INTEGER"));
        if (!StepVal) return;
    } else {
        StepVal = ConstantInt::get(*TheContext, APInt(64, 1));
    }

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "forcond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "forloop", TheFunction);
    BasicBlock *IncBB = BasicBlock::Create(*TheContext, "forinc", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "forcont", TheFunction);

    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    Value *CurVar = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *EndVal = emitCoercedExpr(Stmt->getEnd(), resolveType("INTEGER"));
    if (!EndVal) return;

    // The loop direction depends on the STEP value at run time.
    Value *StepNonNeg = Builder->CreateICmpSGE(StepVal,
                                               ConstantInt::get(*TheContext, APInt(64, 0)),
                                               "step_nonneg");
    Value *CondUp = Builder->CreateICmpSLE(CurVar, EndVal, "forcond_le");
    Value *CondDown = Builder->CreateICmpSGE(CurVar, EndVal, "forcond_ge");
    Value *CondV = Builder->CreateSelect(StepNonNeg, CondUp, CondDown, "forcond");

    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    // Arrays declared in the body are block-scoped (see emitIfStmt).
    auto SavedArrays = Arrays->snapshotTable();
    Builder->SetInsertPoint(LoopBB);
    emitScopedBlock(Stmt->getBody());
    Arrays->exchangeTable(std::move(SavedArrays));
    if (!Builder->GetInsertBlock()->hasTerminator())
        Builder->CreateBr(IncBB);

    Builder->SetInsertPoint(IncBB);
    Value *CurValForInc = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *NextVal = Builder->CreateAdd(CurValForInc, StepVal, "nextval");
    Builder->CreateStore(NextVal, Alloca);
    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitCaseStmt(CaseStmtAST *Stmt) {
    VariableExprAST Selector(Stmt->getSelector());
    if (isWholeArrayVar(&Selector)) {
        reportError("CASE selector cannot be a whole array");
        return;
    }
    const TypeInfo *SelectorType = getExprTypeInfo(&Selector);
    Value *Selected = emitExpr(&Selector);
    if (!Selected || !SelectorType) return;

    auto *F = Builder->GetInsertBlock()->getParent();
    F->addFnAttr("no-jump-tables", "true");
    auto *After = BasicBlock::Create(*TheContext, "case_cont", F);
    auto SavedArrays = Arrays->snapshotTable();
    for (const auto &Branch : Stmt->getBranches()) {
        auto *Body = BasicBlock::Create(*TheContext, "case_body", F);
        auto *Next = BasicBlock::Create(*TheContext, "case_next", F);
        auto Compare = [&](int Op, ExprAST *Label) -> Value * {
            if (isWholeArrayVar(Label)) {
                reportError("CASE label cannot be a whole array");
                return nullptr;
            }
            const TypeInfo *LabelType = getExprTypeInfo(Label);
            if (!LabelType) {
                reportError("Cannot determine CASE label type at line %d", Branch.Line);
                return nullptr;
            }
            Value *LabelValue = emitExpr(Label);
            return emitBinaryOp(Op, Selected, SelectorType, LabelValue, LabelType, Branch.Line);
        };
        if (!Branch.Lower) {
            Builder->CreateBr(Body);
        } else {
            Value *Match = Compare(Branch.Upper ? tok_ge : tok_eq, Branch.Lower.get());
            if (!Match) {
                // Finish the CFG even after a diagnostic; the driver discards it.
                Builder->CreateBr(Next);
            } else if (Branch.Upper) {
                auto *Upper = BasicBlock::Create(*TheContext, "case_upper", F);
                Builder->CreateCondBr(Match, Upper, Next);
                Builder->SetInsertPoint(Upper);
                Match = Compare(tok_le, Branch.Upper.get());
                if (Match) Builder->CreateCondBr(Match, Body, Next);
                else Builder->CreateBr(Next);
            } else {
                Builder->CreateCondBr(Match, Body, Next);
            }
        }
        Builder->SetInsertPoint(Body);
        emitScopedBlock(Branch.Body);
        if (!Builder->GetInsertBlock()->hasTerminator()) Builder->CreateBr(After);
        Arrays->exchangeTable(SavedArrays);
        Builder->SetInsertPoint(Next);
    }
    Builder->CreateBr(After);
    Builder->SetInsertPoint(After);
}
