#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "cps/FunctionAST.h"
#include "cps/TypeSystem.h"
#include <map>
#include <vector>
#include <functional>
#include <string>
#include <tuple>

namespace cps {

// Pseudocode-level signature of a FUNCTION/PROCEDURE, kept so call sites can
// dispatch on declared parameter types and BYREF flags instead of inferring
// them back from LLVM types.
struct FuncSig {
    std::string ReturnTypeName;
    std::vector<std::pair<std::string, bool>> Params; // (TypeName, IsByRef)
};

class FunctionGen {
    llvm::LLVMContext &Context;
    llvm::Module &Module;
    llvm::IRBuilder<> &Builder;
    TypeSystem &Types;

    std::map<std::string, llvm::Value*> &NamedValues;
    std::map<std::string, SymbolInfo> &Symbols;
    bool &HadError;

    std::map<std::string, FuncSig> Signatures;

    void createArgumentAllocas(llvm::Function *F, const std::vector<std::tuple<std::string, std::string, bool>> &Args);

public:
    FunctionGen(llvm::LLVMContext &C,
                llvm::Module &M,
                llvm::IRBuilder<> &B,
                TypeSystem &TS,
                std::map<std::string, llvm::Value*> &NV,
                std::map<std::string, SymbolInfo> &Sym,
                bool &HadErrorFlag)
        : Context(C), Module(M), Builder(B), Types(TS), NamedValues(NV), Symbols(Sym), HadError(HadErrorFlag) {}

    llvm::Type *getLLVMType(const std::string &TypeName);

    const FuncSig *getSignature(const std::string &Name) const;

    llvm::Function *emitPrototype(PrototypeAST *Proto);

    llvm::Function *emitFunctionDef(FunctionDefAST *FuncAST, const std::function<void(StmtAST*)> &StmtEmitter);

    llvm::Value *emitCallExpr(CallExprAST *Call, const std::vector<llvm::Value*> &Args);
    
    void emitCallStmt(CallStmtAST *Call, const std::vector<llvm::Value*> &Args);

    void emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal);
};

}
