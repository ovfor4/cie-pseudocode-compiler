#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileAST.h"
#include "cps/FileHandler.h"
#include "cps/ExceptionHandler.h"
#include "cps/TypeAST.h"

using namespace llvm;
using namespace cps;

void CodeGen::emitStmt(StmtAST *Stmt) {
    SaveAndRestore<int> Location(DiagnosticLine, Stmt && Stmt->getLine() ? Stmt->getLine() : DiagnosticLine);
    if (!Stmt) return;

    if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt)) {
        BasicBlock *SavedBlock = Builder->GetInsertBlock();
        // Function bodies get a fresh array scope (metadata SSA values belong
        // to the emitting function), mirroring FunctionGen's Symbols handling.
        auto SavedArrays = Arrays->exchangeTable({});

        auto SavedScopes = std::move(ConstantScopes);
        ConstantScopes = {{NextScopeID++, {}}};
        FuncGen->emitFunctionDef(FuncDef, [this](StmtAST *S) {
            this->emitStmt(S);
        }, ConstantScopes.back().ID);
        ConstantScopes = std::move(SavedScopes);

        Arrays->exchangeTable(std::move(SavedArrays));
        if (SavedBlock) Builder->SetInsertPoint(SavedBlock);
        return;
    }

    // Keep diagnosing statements after RETURN/THROW without appending code
    // to a terminated block or making those statements reachable.
    if (Builder->GetInsertBlock()->hasTerminator()) {
        auto *F = Builder->GetInsertBlock()->getParent();
        Builder->SetInsertPoint(BasicBlock::Create(*TheContext, "unreachable_code", F));
    }

    if (auto *Try = dynamic_cast<TryStmtAST*>(Stmt)) {
        emitTryStmt(Try);
        return;
    }
    if (dynamic_cast<ThrowStmtAST*>(Stmt)) {
        Exceptions->emitThrow();
        return;
    }
    if (auto *ArrDecl = dynamic_cast<ArrayDeclareStmtAST*>(Stmt)) {
        Arrays->emitArrayDeclare(ArrDecl, *this);
        return;
    }
    if (auto *ArrAssign = dynamic_cast<ArrayAssignStmtAST*>(Stmt)) {
        Arrays->emitArrayAssign(ArrAssign, *this);
        return;
    }

    if (auto *Call = dynamic_cast<CallStmtAST*>(Stmt)) {
        std::vector<Value*> Args;
        if (!marshalCallArgs(Call->getCallee(), Call->getArgs(), Call->getLine(), Args)) return;
        FuncGen->emitCallStmt(Call, Args);
        return;
    }

    if (auto *Ret = dynamic_cast<ReturnStmtAST*>(Stmt)) {
        Function *F = Builder->GetInsertBlock()->getParent();
        const FuncSig *Sig = FuncGen->getSignature(F->getName().str());
        // Pseudocode cannot name a function "main" (reserved runtime symbol),
        // so no signature means we are emitting the top level.
        if (!Sig) {
            reportError("RETURN is only allowed inside a FUNCTION or PROCEDURE");
            return;
        }
        if (F->getReturnType()->isVoidTy() && Ret->getRetVal()) {
            reportError("RETURN with a value is only allowed in a FUNCTION");
            return;
        }
        if (!F->getReturnType()->isVoidTy() && !Ret->getRetVal()) {
            reportError("RETURN in a FUNCTION requires a value");
            return;
        }
        Value *RetVal = nullptr;
        if (Ret->getRetVal()) {
            RetVal = emitCoercedExpr(Ret->getRetVal(), resolveType(Sig->ReturnTypeName));
            if (!RetVal) return;
        }
        FuncGen->emitReturn(Ret, RetVal);
        return;
    }

    if (auto *Const = dynamic_cast<ConstantStmtAST*>(Stmt)) {
        emitConstantStmt(Const);
        return;
    }

    if (auto *Decl = dynamic_cast<DeclareStmtAST*>(Stmt)) {
        emitDeclareStmt(Decl);
        return;
    }

    if (auto *Assign = dynamic_cast<AssignStmtAST*>(Stmt)) {
        const SymbolInfo *Info = getSymbolInfo(Assign->getName());
        if (!Info) {
            if (Types->lookupEnumConstant(Assign->getName())) {
                reportError("Enum value %s is not an assignable location", Assign->getName().c_str());
            } else {
                reportError("Unknown variable name %s", Assign->getName().c_str());
            }
            return;
        }
        if (!requireWritable(*Info, Assign->getName())) return;
        if (Info->IsArray) {
            reportError("Cannot assign array %s without indices", Assign->getName().c_str());
            return;
        }

        Value *Val = emitCoercedExpr(Assign->getExpr(), resolveType(Info->TypeName));
        if (!Val) return;

        Builder->CreateStore(Val, Info->Storage);
        return;
    }

    if (auto *In = dynamic_cast<InputStmtAST*>(Stmt)) {
        LValueInfo Target;
        if (!emitLValue(In->getTarget(), Target)) return;
        const TypeInfo *TypeInfo = resolveType(Target.TypeName);
        if (!TypeInfo) {
            reportError("Unknown type for INPUT %s", Target.TypeName.c_str());
            return;
        }

        std::vector<Value*> Args;
        if (TypeInfo->isC()) {
            emitCInput(Target.Addr, TypeInfo);
        } else if (TypeInfo->isReal()) {
            Args.push_back(ScanfFloatFormatStr);
            Args.push_back(Target.Addr);
            Builder->CreateCall(ScanfFunc, Args);
        } else if (TypeInfo->isBoolean()) {
            AllocaInst *TempInt = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(), "tmp_bool_input");
            Args.push_back(ScanfFormatStr);
            Args.push_back(TempInt);
            Builder->CreateCall(ScanfFunc, Args);

            Value *Val = Builder->CreateLoad(Type::getInt64Ty(*TheContext), TempInt);
            Value *BoolVal = Builder->CreateICmpNE(Val, ConstantInt::get(*TheContext, APInt(64, 0)), "bool_cast");
            Builder->CreateStore(BoolVal, Target.Addr);
        } else if (TypeInfo->isString()) {
            Value *Mem = Builder->CreateCall(MallocFunc, ConstantInt::get(*TheContext, APInt(64, 1024)), "input_str");
            Args.push_back(ScanfStringFormatStr);
            Args.push_back(Mem);
            Builder->CreateCall(ScanfFunc, Args);
            Builder->CreateStore(Mem, Target.Addr);
        } else if (TypeInfo->isChar()) {
            Value *CharFmt = Builder->CreateGlobalString(" %c", "fmt_in_char", 0, TheModule.get());
            Args.push_back(CharFmt);
            Args.push_back(Target.Addr);
            Builder->CreateCall(ScanfFunc, Args);
        } else if (TypeInfo->Kind == TypeKind::Integer) {
            Args.push_back(ScanfFormatStr);
            Args.push_back(Target.Addr);
            Builder->CreateCall(ScanfFunc, Args);
        } else {
            // Enum/Record/Pointer and any future kind: scanf'ing raw bytes
            // into typed storage would corrupt it — reject explicitly.
            reportError("INPUT is not supported for a value of type %s", TypeInfo->Name.c_str());
        }
        return;
    }

    if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt)) {
        const auto &Exprs = Out->getExprs();
        for (size_t I = 0; I < Exprs.size(); ++I) {
            ExprAST *Expr = Exprs[I].get();
            if (Arrays->tryEmitArrayOutput(Expr, *this)) continue;

            Value *Val = emitExpr(Expr);
            const TypeInfo *TypeInfo = getExprTypeInfo(Expr);
            if (!TypeInfo) {
                reportError("Cannot infer the type of the OUTPUT expression");
                continue;
            }
            if (TypeInfo->isVoid()) {
                reportError("Cannot OUTPUT the result of a PROCEDURE call");
                continue;
            }
            emitOutputValue(Val, TypeInfo, I + 1 == Exprs.size());
        }
        return;
    }

    if (auto *Open = dynamic_cast<OpenFileStmtAST*>(Stmt)) {
        if (isWholeArrayVar(Open->getFileName())) {
            reportError("Cannot use a whole array as the OPENFILE filename");
            return;
        }
        Value *Name = emitCoercedExpr(Open->getFileName(), resolveType("STRING"));
        if (!Name) return;
        Files->emitOpen(Name, Open->getMode(), Open->getLine());
        return;
    }

    if (auto *Read = dynamic_cast<ReadFileStmtAST*>(Stmt)) {
        const SymbolInfo *Info = getSymbolInfo(Read->getVarName());
        if (!Info) {
            reportError("Unknown variable name %s", Read->getVarName().c_str());
            return;
        }
        if (!requireWritable(*Info, Read->getVarName())) return;
        if (Info->IsArray) {
            reportError("READFILE into entire array %s is not supported", Read->getVarName().c_str());
            return;
        }
        if (Info->TypeName != "STRING") {
            reportError("READFILE variable %s must be of type STRING (got %s)",
                        Read->getVarName().c_str(), Info->TypeName.c_str());
            return;
        }
        if (isWholeArrayVar(Read->getFileName())) {
            reportError("Cannot use a whole array as the READFILE filename");
            return;
        }
        Value *Name = emitCoercedExpr(Read->getFileName(), resolveType("STRING"));
        if (!Name) return;
        Value *LineStr = Files->emitRead(Name, Read->getLine());
        if (!LineStr) return;
        Builder->CreateStore(LineStr, Info->Storage);
        return;
    }

    if (auto *Write = dynamic_cast<WriteFileStmtAST*>(Stmt)) {
        if (isWholeArrayVar(Write->getFileName()) || isWholeArrayVar(Write->getData())) {
            reportError("Cannot use a whole array in WRITEFILE");
            return;
        }
        Value *Name = emitCoercedExpr(Write->getFileName(), resolveType("STRING"));
        Value *Data = emitCoercedExpr(Write->getData(), resolveType("STRING"));
        if (!Name || !Data) return;
        Files->emitWrite(Name, Data, Write->getLine());
        return;
    }

    if (auto *Close = dynamic_cast<CloseFileStmtAST*>(Stmt)) {
        if (isWholeArrayVar(Close->getFileName())) {
            reportError("Cannot use a whole array as the CLOSEFILE filename");
            return;
        }
        Value *Name = emitCoercedExpr(Close->getFileName(), resolveType("STRING"));
        if (!Name) return;
        Files->emitClose(Name, Close->getLine());
        return;
    }

    if (auto *Case = dynamic_cast<CaseStmtAST*>(Stmt)) {
        emitCaseStmt(Case);
        return;
    }
    if (auto *IfStmt = dynamic_cast<IfStmtAST*>(Stmt)) {
        emitIfStmt(IfStmt);
        return;
    }
    if (auto *WhileStmt = dynamic_cast<WhileStmtAST*>(Stmt)) {
        emitWhileStmt(WhileStmt);
        return;
    }
    if (auto *RepeatStmt = dynamic_cast<RepeatStmtAST*>(Stmt)) {
        emitRepeatStmt(RepeatStmt);
        return;
    }
    if (auto *ForStmt = dynamic_cast<ForStmtAST*>(Stmt)) {
        emitForStmt(ForStmt);
        return;
    }

    // TYPE declarations are registered by compile()'s pre-pass (the parser
    // guarantees they are top-level); nothing is emitted here.
    if (dynamic_cast<EnumTypeDeclAST*>(Stmt) || dynamic_cast<PointerTypeDeclAST*>(Stmt) ||
        dynamic_cast<RecordTypeDeclAST*>(Stmt) || dynamic_cast<CTypeDeclAST*>(Stmt)) {
        return;
    }
    if (auto *DA = dynamic_cast<DesignatorAssignStmtAST*>(Stmt)) {
        LValueInfo LV;
        if (!emitLValue(DA->getTarget(), LV)) return;
        Value *Val = emitCoercedExpr(DA->getExpr(), resolveType(LV.TypeName));
        if (!Val) return;
        Builder->CreateStore(Val, LV.Addr);
        return;
    }
}
