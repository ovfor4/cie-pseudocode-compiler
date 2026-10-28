#pragma once
#include "cps/CABI.h"
#include "cps/Identifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "cps/FunctionAST.h"
#include "cps/TypeSystem.h"
#include <cstdint>
#include <deque>
#include <map>
#include <utility>
#include <vector>
#include <functional>
#include <string>

namespace cps {

class ExceptionHandler;

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
    std::string Name;
    std::string LinkName;
    std::string ParamKey;
    std::string ReturnKey;
    bool IsOverload = false;
    bool IsDefaultReturn = false;
    const PrototypeAST *Prototype = nullptr;
    std::string ReturnTypeName;
    std::vector<ParamSig> Params;
    size_t OwnerUnit = 0;
    // EXTERN "C": called through its declared C type, which may differ from a
    // runtime declaration of the same libc symbol. ExternABI is clang's
    // arrangement of the fixed parameters; ExternType and ExternAttrs lower it.
    bool IsExternal = false;
    bool IsVariadic = false;
    CABISignature ExternABI;
    llvm::FunctionType *ExternType = nullptr;
    llvm::AttributeList ExternAttrs;
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
    ExceptionHandler &Exceptions;

    IdentifierMap<SymbolInfo> &Symbols;
    bool &HadError;
    int &DiagnosticLine;
    __attribute__((format(printf, 2, 3))) void reportError(const char *Fmt, ...);

    IdentifierMap<std::deque<FuncSig>> Signatures;
    IdentifierMap<std::pair<std::string, size_t>> LinkIndex;
    std::map<const PrototypeAST *, std::string> PrototypeLinks;
    std::map<size_t, std::string> UnitPaths;
    size_t CurrentUnit = 0;
    std::function<bool(size_t)> Visible;
    std::function<void(const FuncSig &)> BeforeCall;
    ArrayParamBinder BindArrayParam;
    std::function<void(llvm::Value *, const std::string &, int)> RegisterCopy;
    std::function<void(int, const std::string &)> MissingReturn;
    ClangABI ABI{Context, Types}; // clang context created on the first EXTERN

    void createArgumentAllocas(llvm::Function *F, const std::vector<ParamDecl> &Params, bool IsMethod);
    llvm::Type *getLLVMType(const std::string &TypeName);
    bool validatePrototype(PrototypeAST *Proto);
    llvm::Value *emitCall(const std::string &Callee, const std::vector<llvm::Value*> &Args);

public:
    FunctionGen(llvm::LLVMContext &C,
                llvm::Module &M,
                llvm::IRBuilder<> &B,
                TypeSystem &TS,
                ExceptionHandler &EH,
                IdentifierMap<SymbolInfo> &Sym,
                bool &HadErrorFlag, int &Line)
        : Context(C), Module(M), Builder(B), Types(TS), Exceptions(EH), Symbols(Sym), HadError(HadErrorFlag), DiagnosticLine(Line) {}

    void setArrayParamBinder(ArrayParamBinder Binder) { BindArrayParam = std::move(Binder); }

    // Called (with the FUNCTION header line and name) where a non-VOID FUNCTION
    // body can fall off its end; emits the runtime error and throw.
    void setMissingReturnHandler(std::function<void(int, const std::string &)> Handler) {
        MissingReturn = std::move(Handler);
    }

    void setCopyRegistrar(std::function<void(llvm::Value *, const std::string &, int)> Registrar) {
        RegisterCopy = std::move(Registrar);
    }

    void setCurrentUnit(size_t Unit, const std::string &Path) { CurrentUnit = Unit; UnitPaths[Unit] = Path; }
    void setCallHook(std::function<void(const FuncSig &)> Hook) { BeforeCall = std::move(Hook); }
    void setVisibility(std::function<bool(size_t)> Predicate) { Visible = std::move(Predicate); }
    std::vector<const FuncSig *> getOverloads(const std::string &Name, bool AllUnits = false) const;
    static bool isReservedRuntimeName(const std::string &Name);
    static const FuncSig *selectDefaultReturn(const std::vector<const FuncSig *> &Candidates);
    bool hasFunction(const std::string &Name) const { return !getOverloads(Name).empty(); }
    const FuncSig *getSignatureByLink(const std::string &Name) const;

    llvm::Function *emitPrototype(PrototypeAST *Proto);
    bool emitExternPrototype(PrototypeAST *Proto);

    llvm::Function *emitFunctionDef(FunctionDefAST *FuncAST, const std::function<void(StmtAST*)> &StmtEmitter, unsigned ScopeID);

    // Variadic holds the promoted types of an EXTERN call's variadic arguments.
    llvm::Value *emitCall(const FuncSig &Sig, const std::vector<llvm::Value*> &Args,
                          const std::vector<const TypeInfo *> &Variadic = {});

    void emitReturn(llvm::Value *RetVal);
};

}
