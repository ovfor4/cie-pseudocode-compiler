#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "cps/FunctionAST.h"
#include "cps/TypeSystem.h"
#include <cstdint>
#include <map>
#include <utility>
#include <vector>
#include <functional>
#include <string>

namespace cps {

// Array-parameter ABI: each rank-R array parameter is flattened into the
// data pointer followed by kBoundArgsPerDim i64 bounds per dimension.
inline constexpr unsigned kBoundArgsPerDim = 2; // lower, upper
inline constexpr const char *kLowerBoundSuffix = ".lb";
inline constexpr const char *kUpperBoundSuffix = ".ub";

// Pseudocode-level signature of one FUNCTION/PROCEDURE parameter; for arrays
// TypeName is the ELEMENT type and DeclaredBounds mirrors ParamDecl.
struct ParamSig {
    std::string TypeName;
    bool IsByRef = false;
    bool IsArray = false;
    int Rank = 0;
    std::vector<std::pair<int64_t, int64_t>> DeclaredBounds;
};

// Pseudocode-level signature of a FUNCTION/PROCEDURE, kept so call sites can
// dispatch on declared parameter types and modes instead of inferring them
// back from LLVM types.
struct FuncSig {
    std::string ReturnTypeName;
    std::vector<ParamSig> Params;
};

class FunctionGen {
public:
    // Injected by CodeGen (StmtEmitter inversion precedent) so array
    // parameters can be bound into ArrayHandler without FunctionGen ever
    // seeing it: (param, incoming data ptr, per-dim lower/upper bounds).
    using ArrayParamBinder =
        std::function<void(const ParamDecl &, llvm::Value *,
                           const std::vector<llvm::Value*> &,
                           const std::vector<llvm::Value*> &)>;

private:
    llvm::LLVMContext &Context;
    llvm::Module &Module;
    llvm::IRBuilder<> &Builder;
    TypeSystem &Types;

    std::map<std::string, SymbolInfo> &Symbols;
    bool &HadError;

    std::map<std::string, FuncSig> Signatures;
    ArrayParamBinder BindArrayParam;

    void createArgumentAllocas(llvm::Function *F, const std::vector<ParamDecl> &Params);

public:
    FunctionGen(llvm::LLVMContext &C,
                llvm::Module &M,
                llvm::IRBuilder<> &B,
                TypeSystem &TS,
                std::map<std::string, SymbolInfo> &Sym,
                bool &HadErrorFlag)
        : Context(C), Module(M), Builder(B), Types(TS), Symbols(Sym), HadError(HadErrorFlag) {}

    llvm::Type *getLLVMType(const std::string &TypeName);

    void setArrayParamBinder(ArrayParamBinder Binder) { BindArrayParam = std::move(Binder); }

    const FuncSig *getSignature(const std::string &Name) const;

    llvm::Function *emitPrototype(PrototypeAST *Proto);

    llvm::Function *emitFunctionDef(FunctionDefAST *FuncAST, const std::function<void(StmtAST*)> &StmtEmitter);

    llvm::Value *emitCallExpr(CallExprAST *Call, const std::vector<llvm::Value*> &Args);
    
    void emitCallStmt(CallStmtAST *Call, const std::vector<llvm::Value*> &Args);

    void emitReturn(ReturnStmtAST *Ret, llvm::Value *RetVal);
};

}
