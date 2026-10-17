#include "cps/Diagnostics.h"
#include "cps/FunctionGen.h"
#include "cps/CodeGen.h"
#include "cps/ExceptionHandler.h"
#include <cstdio>
#include <cstdarg>

using namespace llvm;
using namespace cps;

// Symbols the compiler itself declares (libc + runtime); a user
// FUNCTION/PROCEDURE with one of these names would hijack the runtime.
static bool isReservedRuntimeName(const std::string &Name) {
    static const char *Reserved[] = {
        "__cps_main", "printf", "scanf", "malloc", "free", "strlen", "memcpy",
        "strcpy", "strcat", "toupper", "tolower", "sprintf", "snprintf", "strtol", "strtoll",
        "strtod", "memset", "exit", "write",
        "fopen", "fclose", "fgetc", "ungetc", "fputs", "fputc",
        "realloc", "strcmp", "__cxa_allocate_exception", "__cxa_throw",
        "__cxa_begin_catch", "__cxa_end_catch", "__gxx_personality_v0",
        "__gxx_personality_seh0", "__gxx_personality_sj0", "_Unwind_Resume", "_ZTIi",
    };
    for (const char *R : Reserved) {
        if (sameIdentifier(Name, R)) return true;
    }
    return false;
}

void FunctionGen::reportError(const char *Fmt, ...) {
    va_list Args;
    va_start(Args, Fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, Fmt, Args);
    if (DiagnosticLine) fprintf(stderr, "%s", lineSuffix(DiagnosticLine).c_str());
    fprintf(stderr, "\n");
    va_end(Args);
    HadError = true;
}

Type *FunctionGen::getLLVMType(const std::string &TypeName) {
    Type *Resolved = Types.getLLVMType(TypeName);
    if (!Resolved) {
        reportError("Unknown type %s", TypeName.c_str());
        return Type::getInt64Ty(Context);
    }
    return Resolved;
}

void FunctionGen::createArgumentAllocas(Function *F, const std::vector<ParamDecl> &Params, bool IsMethod) {
    Function::arg_iterator AI = F->arg_begin();
    if (IsMethod) ++AI;
    for (const ParamDecl &P : Params) {
        bool InvalidThis = IsMethod && sameIdentifier(P.Name, "this");
        if (InvalidThis) reportError("A method parameter cannot be named this");
        if (Types.lookupEnumConstant(P.Name)) {
            reportError("parameter '%s' collides with an enum value", P.Name.c_str());
        }
        if (Types.resolve(P.Name)) {
            reportError("parameter '%s' collides with a type name", P.Name.c_str());
        }
        Value *ArgVal = &(*AI);
        ++AI;
        ArgVal->setName(P.Name);

        if (P.IsArray) {
            // Data pointer + kBoundArgsPerDim bounds per dimension; binding
            // into the array subsystem is delegated to the injected binder.
            std::vector<Value*> LBs, UBs;
            for (int D = 1; D <= P.Rank; ++D) {
                Value *LB = &(*AI);
                ++AI;
                LB->setName(P.Name + kLowerBoundSuffix + std::to_string(D));
                Value *UB = &(*AI);
                ++AI;
                UB->setName(P.Name + kUpperBoundSuffix + std::to_string(D));
                LBs.push_back(LB);
                UBs.push_back(UB);
            }
            if (!InvalidThis && BindArrayParam) BindArrayParam(P, ArgVal, LBs, UBs);
            continue;
        }

        if (InvalidThis) continue;
        if (P.Mode == PassMode::ByRef) {
            Symbols[P.Name] = {ArgVal, P.TypeName, false};
            continue;
        }

        Type *ArgType = getLLVMType(P.TypeName);
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(ArgType, nullptr, P.Name);

        // Shallow BYVAL copy. The common lifetime hook is the future copy-constructor point.
        Builder.CreateStore(ArgVal, Alloca);
        const TypeInfo *Info = Types.resolve(P.TypeName);
        if (Info && Info->IsClass) {
            AllocaInst *Slot = TmpB.CreateAlloca(PointerType::getUnqual(Context), nullptr, P.Name + ".ref");
            Builder.CreateStore(Alloca, Slot);
            Symbols[P.Name] = {Slot, P.TypeName, false, nullptr, 0, true};
        } else {
            Symbols[P.Name] = {Alloca, P.TypeName, false};
        }
        if (RegisterCopy) RegisterCopy(Alloca, P.TypeName, P.Line);
    }
}

const FuncSig *FunctionGen::getSignature(const std::string &Name) const {
    auto It = Signatures.find(sameIdentifier(Name, "MAIN") ? "__cps_main" : Name);
    return It == Signatures.end() || (Visible && !Visible(It->second.OwnerUnit))
               ? nullptr : &It->second;
}

Function *FunctionGen::emitPrototype(PrototypeAST *Proto) {
    if (Proto->getOwnerClass().empty()) {
        if (isReservedRuntimeName(Proto->getName())) {
            reportError("'%s' is a reserved runtime symbol and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return nullptr;
        }
        if (CodeGen::isBuiltinName(Proto->getName())) {
            reportError("'%s' is a built-in function and cannot be redefined",
                    Proto->getName().c_str());
            return nullptr;
        }
        if (Types.lookupEnumConstant(Proto->getName())) {
            reportError("'%s' is an enum value and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return nullptr;
        }
        if (Types.resolve(Proto->getName())) {
            reportError("'%s' is a type name and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return nullptr;
        }
    }

    FuncSig Sig;
    Sig.OwnerUnit = CurrentUnit;
    Sig.Name = Proto->getLinkName();
    Sig.ReturnTypeName = Proto->getReturnType();

    std::vector<Type*> ArgTypes;
    if (!Proto->getOwnerClass().empty()) ArgTypes.push_back(PointerType::getUnqual(Context));
    for (const ParamDecl &P : Proto->getParams()) {
        const TypeInfo *Info = Types.resolve(P.TypeName);
        if (Info && Info->isVoid()) {
            reportError("parameter %s cannot have void type", P.Name.c_str());
            return nullptr;
        }
        ParamSig PS;
        PS.TypeName = P.TypeName;
        PS.IsByRef = P.Mode == PassMode::ByRef;
        PS.IsArray = P.IsArray;
        PS.Rank = P.Rank;
        PS.DeclaredBounds = P.DeclaredBounds;

        if (P.IsArray) {
            getLLVMType(P.TypeName); // diagnose an unknown element type
            ArgTypes.push_back(PointerType::getUnqual(Context)); // data pointer
            for (unsigned D = 0; D < static_cast<unsigned>(P.Rank) * kBoundArgsPerDim; ++D) {
                ArgTypes.push_back(Type::getInt64Ty(Context));
            }
        } else {
            Type *T = getLLVMType(P.TypeName);
            if (PS.IsByRef) {
                T = T->getPointerTo();
            }
            ArgTypes.push_back(T);
        }
        Sig.Params.push_back(std::move(PS));
    }

    Type *RetType = getLLVMType(Proto->getReturnType());
    FunctionType *FT = FunctionType::get(RetType, ArgTypes, false);
    const FuncSig *Existing = getSignature(Proto->getLinkName());
    Function *F = Module.getFunction(Existing ? Existing->Name : Proto->getLinkName());
    if (F && F->getFunctionType() != FT) {
        reportError("Function %s conflicts with an existing declaration of a different type",
                Proto->getName().c_str());
        return nullptr;
    }
    if (!F) {
        F = Function::Create(FT,
            Proto->getOwnerClass().empty() && sameIdentifier(Proto->getName(), "MAIN")
                ? Function::InternalLinkage : Function::ExternalLinkage,
            Proto->getLinkName(), &Module);
    }
    Exceptions.prepareFunction(F);

    // Keep the first registration: on a duplicate definition the first one
    // wins and the second is rejected by emitFunctionDef.
    Signatures.emplace(Proto->getLinkName(), std::move(Sig));

    Function::arg_iterator AI = F->arg_begin();
    if (!Proto->getOwnerClass().empty()) (AI++)->setName("this");
    for (const ParamDecl &P : Proto->getParams()) {
        if (AI == F->arg_end()) break;
        (AI++)->setName(P.Name);
        for (int D = 1; P.IsArray && D <= P.Rank && AI != F->arg_end(); ++D) {
            (AI++)->setName(P.Name + kLowerBoundSuffix + std::to_string(D));
            if (AI != F->arg_end())
                (AI++)->setName(P.Name + kUpperBoundSuffix + std::to_string(D));
        }
    }
    return F;
}

Function *FunctionGen::emitFunctionDef(FunctionDefAST *FuncAST,
                                       const std::function<void(StmtAST*)> &StmtEmitter, unsigned ScopeID) {
    PrototypeAST *Proto = FuncAST->getProto();
    Function *TheFunction = emitPrototype(Proto);

    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) {
        reportError("Function %s cannot be redefined.", Proto->getName().c_str());
        return nullptr;
    }

    BasicBlock *BB = BasicBlock::Create(Context, "entry", TheFunction);
    Builder.SetInsertPoint(BB);
    Exceptions.enableCleanups(TheFunction);

    IdentifierMap<SymbolInfo> OldSymbols = Symbols;
    Symbols.clear();
    for (const auto &[Name, Info] : OldSymbols) {
        if (Info.IsGlobal || (Info.ConstantValue && Info.ScopeID == 0)) Symbols.emplace(Name, Info);
    }

    bool IsMethod = !Proto->getOwnerClass().empty();
    if (IsMethod) {
        const TypeInfo *Class = Types.resolve(Proto->getOwnerClass());
        Value *Self = TheFunction->getArg(0);
        if (Class && Class->asRecord()) {
            for (const auto &[Name, Field] : Class->asRecord()->Fields) {
                Value *Addr = Builder.CreateStructGEP(Class->LLVMType, Self, Field.Index, Name);
                Symbols[Name] = {Addr, Field.TypeName, false};
            }
        }
        Symbols["this"] = {Self, Proto->getOwnerClass(), false, nullptr, ScopeID};
    }
    createArgumentAllocas(TheFunction, Proto->getParams(), IsMethod);
    for (const auto &P : Proto->getParams()) {
        auto It = Symbols.find(P.Name);
        if (It != Symbols.end()) It->second.ScopeID = ScopeID;
    }

    for (const auto &Stmt : FuncAST->getBody()) {
        StmtEmitter(Stmt.get());
    }

    if (!Builder.GetInsertBlock()->hasTerminator()) {
        if (sameIdentifier(Proto->getReturnType(), "VOID")) {
            Exceptions.recordExit(Builder.CreateRetVoid());
        } else {
            Exceptions.recordExit(Builder.CreateRet(Constant::getNullValue(TheFunction->getReturnType())));
        }
    }

    Symbols = OldSymbols;
    return TheFunction;
}

static Value *GenerateCall(llvm::Module &Module,
                           ExceptionHandler &Exceptions,
                           const std::string &CalleeName,
                           const std::vector<llvm::Value*> &Args,
                           bool &HadError, int DiagnosticLine) {
    Function *CalleeF = Module.getFunction(CalleeName);
    if (!CalleeF) {
        sourceDiagnostic(DiagnosticLine, "error: Call to undefined function %s\n", CalleeName.c_str());
        HadError = true;
        return nullptr;
    }

    if (CalleeF->getReturnType()->isVoidTy()) {
        return Exceptions.emitCall(CalleeF, Args);
    }

    return Exceptions.emitCall(CalleeF, Args, "calltmp");
}

llvm::Value *FunctionGen::emitCall(const std::string &Callee, const std::vector<llvm::Value*> &Args) {
    const FuncSig *Sig = getSignature(Callee);
    return GenerateCall(Module, Exceptions, Sig ? Sig->Name : Callee, Args, HadError, DiagnosticLine);
}

void FunctionGen::emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal) {
    Instruction *CatchCleanup = Exceptions.emitReturnCleanup();
    Instruction *RetInst = RetVal ? Builder.CreateRet(RetVal) : Builder.CreateRetVoid();
    Exceptions.recordExit(CatchCleanup ? CatchCleanup : RetInst);
}
