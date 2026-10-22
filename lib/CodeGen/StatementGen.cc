#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileAST.h"
#include "cps/FileHandler.h"
#include "cps/MemoryHandler.h"
#include "cps/ExceptionHandler.h"
#include "cps/TypeAST.h"
#include "cps/TemplateAST.h"

using namespace llvm;
using namespace cps;

void CodeGen::emitStmt(StmtAST *Stmt) {
    size_t Start = StackObjects.size();
    emitStmtImpl(Stmt);
    cleanupTemporaries(Start);
}

void CodeGen::emitStmtImpl(StmtAST *Stmt) {
    SaveAndRestore<int> Location(DiagnosticLine, Stmt && Stmt->getLine() ? Stmt->getLine() : DiagnosticLine);
    if (!Stmt) return;
    if (dynamic_cast<TemplateDeclAST*>(Stmt)) return;

    if (dynamic_cast<IncludeStmtAST*>(Stmt)) return;
    if (dynamic_cast<GlobalBlockAST*>(Stmt)) return;
    if (dynamic_cast<IncludeObjStmtAST*>(Stmt) || dynamic_cast<ExternDeclAST*>(Stmt)) return;

    if (auto *Class = dynamic_cast<ClassTypeDeclAST*>(Stmt)) {
        for (const auto &Method : Class->getMethods()) {
            // Invalid or duplicate members have already been diagnosed.
            if (isRegisteredMethod(Class->getName(), Method->getProto()))
                emitStmt(Method.get());
        }
        return;
    }

    if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt)) {
        emitFunctionDefStmt(FuncDef);
        return;
    }

    // Keep diagnosing statements after RETURN/THROW without appending code
    // to a terminated block or making those statements reachable.
    if (Builder->GetInsertBlock()->hasTerminator()) {
        auto *F = Builder->GetInsertBlock()->getParent();
        Builder->SetInsertPoint(BasicBlock::Create(*TheContext, "unreachable_code", F));
    }

    if (auto *Delete = dynamic_cast<DeleteStmtAST*>(Stmt)) {
        LValueInfo Object;
        if (!emitLValue(Delete->getTarget(), Object)) return;
        const TypeInfo *Info = resolveType(Object.TypeName);
        if (!Info || !Info->IsClass) {
            reportError("DELETE requires a class object");
            return;
        }
        Memory->emitValidate(Object.Addr, Stmt->getLine());
        if (emitDestroy(Object.TypeName, Object.Addr, Stmt->getLine()))
            Memory->emitRelease(Object.Addr, Stmt->getLine());
        return;
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

    if (auto *Call = dynamic_cast<MemberCallStmtAST*>(Stmt)) {
        Value *Result = emitMemberCall(Call->getCall());
        const TypeInfo *Type = getExprTypeInfo(Call->getCall());
        if (Result && Type && (Type->IsClass || containsString(Type))) materializeOperatorReceiver(Result, Type);
        return;
    }

    if (auto *Call = dynamic_cast<CallStmtAST*>(Stmt)) {
        Value *Result = emitCall(Call->getCallee(), Call->getArgs(), Call->getLine(), &Call->getTemplateArgs());
        const FuncSig *Sig = hasCallable(Call->getCallee())
            ? resolveCall(Call->getCallee(), Call->getArgs(), &Call->getTemplateArgs()) : nullptr;
        const TypeInfo *Type = Sig ? resolveType(Sig->ReturnTypeName) : nullptr;
        if (Result && Type && (Type->IsClass || containsString(Type))) materializeOperatorReceiver(Result, Type);
        return;
    }

    if (auto *Ret = dynamic_cast<ReturnStmtAST*>(Stmt)) {
        Function *F = Builder->GetInsertBlock()->getParent();
        const FuncSig *Sig = F->getName() == "main" ? nullptr
            : FuncGen->getSignatureByLink(F->getName().str());
        // The generated wrapper has no signature; user MAIN uses __cps_main.
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
            std::vector<size_t> Transfers;
            RetVal = emitOwnedValue(Ret->getRetVal(), resolveType(Sig->ReturnTypeName), Transfers);
            transferObjects(Transfers);
            if (!RetVal) return;
        }
        cleanupTemporaries();
        FuncGen->emitReturn(RetVal);
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
        const SymbolInfo *Info = lookupWritableSymbol(Assign->getName());
        if (!Info) return;
        if (Info->IsArray) {
            reportError("Cannot assign array %s without indices", Assign->getName().c_str());
            return;
        }

        emitAssignment({symbolAddress(*Info), Info->TypeName}, Assign->getExpr(), Info);
        return;
    }

    if (auto *In = dynamic_cast<InputStmtAST*>(Stmt)) {
        emitInputStmt(In);
        return;
    }

    if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt)) {
        emitOutputStmt(Out);
        return;
    }

    if (auto *Open = dynamic_cast<OpenFileStmtAST*>(Stmt)) {
        Value *Name = emitFileNameOperand(Open->getFileName(), "OPENFILE");
        if (!Name) return;
        Files->emitOpen(Name, Open->getMode(), Open->getLine());
        return;
    }

    if (auto *Read = dynamic_cast<ReadFileStmtAST*>(Stmt)) {
        emitReadFileStmt(Read);
        return;
    }

    if (auto *Write = dynamic_cast<WriteFileStmtAST*>(Stmt)) {
        if (isWholeArrayVar(Write->getFileName()) || isWholeArrayVar(Write->getData())) {
            reportError("Cannot use a whole array in WRITEFILE");
            return;
        }
        Value *Name = emitCStringExpr(Write->getFileName());
        Value *Data = emitCStringExpr(Write->getData());
        if (!Name || !Data) return;
        Files->emitWrite(Name, Data, Write->getLine());
        return;
    }

    if (auto *Close = dynamic_cast<CloseFileStmtAST*>(Stmt)) {
        Value *Name = emitFileNameOperand(Close->getFileName(), "CLOSEFILE");
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
        emitAssignment(LV, DA->getExpr());
        return;
    }
}

void CodeGen::emitFunctionDefStmt(FunctionDefAST *FuncDef) {
    SaveAndRestore<std::string> ClassContext(CurrentClass, FuncDef->getProto()->getOwnerClass());
    for (const auto &Param : FuncDef->getProto()->getParams())
        warnMemberShadow(Param.Name, Param.Line ? Param.Line : DiagnosticLine);
    BasicBlock *SavedBlock = Builder->GetInsertBlock();
    // Function bodies get a fresh array scope (metadata SSA values belong
    // to the emitting function), mirroring FunctionGen's Symbols handling.
    auto SavedArrays = Arrays->exchangeTable({});

    auto SavedObjects = std::move(StackObjects);
    StackObjects.clear();
    auto SavedScopes = std::move(ConstantScopes);
    ConstantScopes = {{NextScopeID++, {}}};
    Function *Emitted = FuncGen->emitFunctionDef(FuncDef, [this](StmtAST *S) {
        this->emitStmt(S);
    }, ConstantScopes.back().ID);
    if (Emitted) finishStackObjects(Emitted);
    StackObjects = std::move(SavedObjects);
    ConstantScopes = std::move(SavedScopes);

    Arrays->exchangeTable(std::move(SavedArrays));
    if (SavedBlock) Builder->SetInsertPoint(SavedBlock);
}

void CodeGen::emitInputStmt(InputStmtAST *In) {
    LValueInfo Target;
    if (!emitLValue(In->getTarget(), Target)) return;
    const TypeInfo *TypeInfo = resolveType(Target.TypeName);
    if (!TypeInfo) {
        reportError("Unknown type for INPUT %s", Target.TypeName.c_str());
        return;
    }

    if (TypeInfo->isC()) {
        emitCInput(Target.Addr, TypeInfo, In->getLine());
    } else if (TypeInfo->isString()) {
        emitMethodCall(TypeInfo->Name, Target.Addr, "READ_INPUT", {}, In->getLine());
    } else if (TypeInfo->Kind == TypeKind::Integer || TypeInfo->isReal() || TypeInfo->isBoolean() ||
               TypeInfo->isChar()) {
        emitScalarInput(Target.Addr, TypeInfo, In->getLine());
    } else {
        // Enum/Record/Pointer and any future kind: reading raw bytes
        // into typed storage would corrupt it — reject explicitly.
        reportError("INPUT is not supported for a value of type %s", TypeInfo->Name.c_str());
    }
}

void CodeGen::emitOutputStmt(OutputStmtAST *Out) {
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
        if (TypeInfo->IsClass && !isObjectLValue(Expr))
            materializeOperatorReceiver(Val, TypeInfo);
        emitOutputValue(Val, TypeInfo, I + 1 == Exprs.size());
    }
}

void CodeGen::emitReadFileStmt(ReadFileStmtAST *Read) {
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
    if (!sameIdentifier(Info->TypeName, "STRING")) {
        reportError("READFILE variable %s must be of type STRING (got %s)",
                    Read->getVarName().c_str(), Info->TypeName.c_str());
        return;
    }
    Value *Name = emitFileNameOperand(Read->getFileName(), "READFILE");
    if (!Name) return;
    Value *LineStr = Files->emitRead(Name, Read->getLine());
    if (!LineStr) return;
    const auto *Type = resolveType(Info->TypeName);
    for (const FuncSig *Sig : FuncGen->getOverloads(Type->Name + ".OPERATOR <-")) {
        if (Sig->Params.size() == 1 && !Sig->Params[0].IsByRef &&
            resolveType("C_STRING")->sameType(resolveType(Sig->Params[0].TypeName)) &&
            checkOperatorAccess(*Sig)) {
            FuncGen->emitCall(*Sig, {symbolAddress(*Info), LineStr});
            auto Free = TheModule->getOrInsertFunction("free", FunctionType::get(
                Type::getVoidTy(*TheContext), {PointerType::getUnqual(*TheContext)}, false));
            Builder->CreateCall(Free, {LineStr});
            return;
        }
    }
    reportError("READFILE requires STRING.OPERATOR<-(Text : C_STRING)");
}

// File keywords take their filename as a STRING-valued expression; a whole
// array is rejected before evaluation.
Value *CodeGen::emitFileNameOperand(ExprAST *E, const char *Keyword) {
    if (isWholeArrayVar(E)) {
        reportError("Cannot use a whole array as the %s filename", Keyword);
        return nullptr;
    }
    return emitCStringExpr(E);
}
