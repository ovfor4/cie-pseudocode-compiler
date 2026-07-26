#include "cps/FunctionGen.h"
#include "cps/CodeGen.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

// Symbols the compiler itself declares (libc + runtime); a user
// FUNCTION/PROCEDURE with one of these names would hijack the runtime.
static bool isReservedRuntimeName(const std::string &Name) {
    static const char *Reserved[] = {
        "main", "printf", "scanf", "malloc", "free", "strlen", "memcpy",
        "strcpy", "strcat", "toupper", "tolower", "sprintf", "strtol",
        "strtod", "memset", "exit",
        "fopen", "fclose", "fgetc", "ungetc", "fputs", "fputc",
        "realloc", "strcmp",
    };
    for (const char *R : Reserved) {
        if (Name == R) return true;
    }
    return false;
}

Type *FunctionGen::getLLVMType(const std::string &TypeName) {
    Type *Resolved = Types.getLLVMType(TypeName);
    if (!Resolved) {
        fprintf(stderr, "Error: Unknown type %s\n", TypeName.c_str());
        HadError = true;
        return Type::getInt64Ty(Context);
    }
    return Resolved;
}

void FunctionGen::createArgumentAllocas(Function *F, const std::vector<ParamDecl> &Params) {
    Function::arg_iterator AI = F->arg_begin();
    for (const ParamDecl &P : Params) {
        if (Types.lookupEnumConstant(P.Name)) {
            fprintf(stderr, "Error: parameter '%s' collides with an enum value\n", P.Name.c_str());
            HadError = true;
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
            if (BindArrayParam) BindArrayParam(P, ArgVal, LBs, UBs);
            continue;
        }

        if (P.Mode == PassMode::ByRef) {
            Symbols[P.Name] = {ArgVal, P.TypeName, false};
            continue;
        }

        Type *ArgType = getLLVMType(P.TypeName);
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(ArgType, nullptr, P.Name);

        Builder.CreateStore(ArgVal, Alloca);
        Symbols[P.Name] = {Alloca, P.TypeName, false};
    }
}

const FuncSig *FunctionGen::getSignature(const std::string &Name) const {
    auto It = Signatures.find(Name);
    return It == Signatures.end() ? nullptr : &It->second;
}

Function *FunctionGen::emitPrototype(PrototypeAST *Proto) {
    if (isReservedRuntimeName(Proto->getName())) {
        fprintf(stderr, "Error: '%s' is a reserved runtime symbol and cannot be a FUNCTION/PROCEDURE name\n",
                Proto->getName().c_str());
        HadError = true;
        return nullptr;
    }
    if (CodeGen::isBuiltinName(Proto->getName())) {
        fprintf(stderr, "Error: '%s' is a built-in function and cannot be redefined\n",
                Proto->getName().c_str());
        HadError = true;
        return nullptr;
    }
    if (Types.lookupEnumConstant(Proto->getName())) {
        fprintf(stderr, "Error: '%s' is an enum value and cannot be a FUNCTION/PROCEDURE name\n",
                Proto->getName().c_str());
        HadError = true;
        return nullptr;
    }

    FuncSig Sig;
    Sig.ReturnTypeName = Proto->getReturnType();

    std::vector<Type*> ArgTypes;
    for (const ParamDecl &P : Proto->getParams()) {
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
    Function *F = Module.getFunction(Proto->getName());
    if (F && F->getFunctionType() != FT) {
        fprintf(stderr, "Error: Function %s conflicts with an existing declaration of a different type\n",
                Proto->getName().c_str());
        HadError = true;
        return nullptr;
    }
    if (!F) {
        F = Function::Create(FT, Function::ExternalLinkage, Proto->getName(), &Module);
    }

    // Keep the first registration: on a duplicate definition the first one
    // wins and the second is rejected by emitFunctionDef.
    Signatures.emplace(Proto->getName(), std::move(Sig));

    Function::arg_iterator AI = F->arg_begin();
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
                                       const std::function<void(StmtAST*)> &StmtEmitter) {
    PrototypeAST *Proto = FuncAST->getProto();
    Function *TheFunction = emitPrototype(Proto);

    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) {
        fprintf(stderr, "Error: Function %s cannot be redefined.\n", Proto->getName().c_str());
        HadError = true;
        return nullptr;
    }

    BasicBlock *BB = BasicBlock::Create(Context, "entry", TheFunction);
    Builder.SetInsertPoint(BB);

    std::map<std::string, SymbolInfo> OldSymbols = Symbols;
    Symbols.clear();

    createArgumentAllocas(TheFunction, Proto->getParams());

    for (const auto &Stmt : FuncAST->getBody()) {
        StmtEmitter(Stmt.get());
    }

    if (!Builder.GetInsertBlock()->getTerminator()) {
        if (Proto->getReturnType() == "VOID") {
            Builder.CreateRetVoid();
        } else {
            Builder.CreateRet(Constant::getNullValue(TheFunction->getReturnType()));
        }
    }

    Symbols = OldSymbols;
    return TheFunction;
}

static Value *GenerateCall(llvm::Module &Module,
                           llvm::IRBuilder<> &Builder,
                           const std::string &CalleeName,
                           const std::vector<llvm::Value*> &Args,
                           bool &HadError) {
    Function *CalleeF = Module.getFunction(CalleeName);
    if (!CalleeF) {
        fprintf(stderr, "Error: Call to undefined function %s\n", CalleeName.c_str());
        HadError = true;
        return nullptr;
    }

    if (CalleeF->getReturnType()->isVoidTy()) {
        return Builder.CreateCall(CalleeF, Args);
    }

    return Builder.CreateCall(CalleeF, Args, "calltmp");
}

llvm::Value *FunctionGen::emitCallExpr(CallExprAST *Call, const std::vector<llvm::Value*> &Args) {
    return GenerateCall(Module, Builder, Call->getCallee(), Args, HadError);
}

void FunctionGen::emitCallStmt(CallStmtAST *Call, const std::vector<llvm::Value*> &Args) {
    GenerateCall(Module, Builder, Call->getCallee(), Args, HadError);
}

void FunctionGen::emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal) {
    if (RetVal) {
        Builder.CreateRet(RetVal);
    } else {
        Builder.CreateRetVoid();
    }
}
