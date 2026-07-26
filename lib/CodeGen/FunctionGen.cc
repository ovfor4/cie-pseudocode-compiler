#include "cps/FunctionGen.h"
#include "llvm/IR/Verifier.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

Type *FunctionGen::getLLVMType(const std::string &TypeName) {
    Type *Resolved = Types.getLLVMType(TypeName);
    if (!Resolved) {
        fprintf(stderr, "Error: Unknown type %s\n", TypeName.c_str());
        HadError = true;
        return Type::getInt64Ty(Context);
    }
    return Resolved;
}

void FunctionGen::createArgumentAllocas(Function *F, const std::vector<std::tuple<std::string, std::string, bool>> &Args) {
    Function::arg_iterator AI = F->arg_begin();
    for (unsigned Idx = 0, E = Args.size(); Idx != E; ++Idx, ++AI) {
        std::string ArgName = std::get<0>(Args[Idx]);
        std::string ArgTypeStr = std::get<1>(Args[Idx]);
        bool IsRef = std::get<2>(Args[Idx]);

        Value *ArgVal = &(*AI);
        ArgVal->setName(ArgName);

        if (IsRef) {
            Symbols[ArgName] = {ArgVal, ArgTypeStr, false};
            continue;
        }

        Type *ArgType = getLLVMType(ArgTypeStr);
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(ArgType, nullptr, ArgName);

        Builder.CreateStore(ArgVal, Alloca);
        Symbols[ArgName] = {Alloca, ArgTypeStr, false};
    }
}

const FuncSig *FunctionGen::getSignature(const std::string &Name) const {
    auto It = Signatures.find(Name);
    return It == Signatures.end() ? nullptr : &It->second;
}

Function *FunctionGen::emitPrototype(PrototypeAST *Proto) {
    FuncSig Sig;
    Sig.ReturnTypeName = Proto->getReturnType();

    std::vector<Type*> ArgTypes;
    for (const auto &Arg : Proto->getArgs()) {
        Type *T = getLLVMType(std::get<1>(Arg));
        if (std::get<2>(Arg)) {
            T = T->getPointerTo();
        }
        ArgTypes.push_back(T);
        Sig.Params.emplace_back(std::get<1>(Arg), std::get<2>(Arg));
    }
    Signatures[Proto->getName()] = std::move(Sig);

    Type *RetType = getLLVMType(Proto->getReturnType());
    FunctionType *FT = FunctionType::get(RetType, ArgTypes, false);
    Function *F = Module.getFunction(Proto->getName());
    if (!F) {
        F = Function::Create(FT, Function::ExternalLinkage, Proto->getName(), &Module);
    }

    unsigned Idx = 0;
    for (auto &Arg : F->args()) {
        if (Idx < Proto->getArgs().size()) {
            Arg.setName(std::get<0>(Proto->getArgs()[Idx++]));
        }
    }
    return F;
}

Function *FunctionGen::emitFunctionDef(FunctionDefAST *FuncAST,
                                       const std::function<void(StmtAST*)> &StmtEmitter) {
    PrototypeAST *Proto = FuncAST->getProto();
    Function *TheFunction = Module.getFunction(Proto->getName());

    if (!TheFunction) {
        TheFunction = emitPrototype(Proto);
    }

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

    createArgumentAllocas(TheFunction, Proto->getArgs());

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
