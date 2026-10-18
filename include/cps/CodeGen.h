#pragma once
#include "llvm/Support/SaveAndRestore.h"
#include "cps/Identifier.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/SourceUnit.h"
#include "cps/RuntimeCheck.h"
#include "cps/FunctionGen.h"
#include "cps/TypeSystem.h"

#include "cps/ArithmeticHandler.h"
#include "cps/StringHandler.h"
#include "cps/StringConversionHandler.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

namespace cps {

class ArrayHandler;
class FileHandler;
class MemoryHandler;
class ExceptionHandler;
class CastExprAST;
class NewExprAST;

class CodeGen {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    int DiagnosticLine = 0;
    const std::vector<SourceUnit> *Units = nullptr;
    size_t CurrentUnit = 0;
    size_t MainUnit = 0;
    IdentifierMap<size_t> TypeOwners;
    std::vector<IdentifierMap<SymbolInfo>> Exports;
    bool isVisible(size_t Owner) const;
    void selectUnit(size_t Unit);
    void importSymbols();
    void runGlobalPrePass(const SourceUnit &Unit, int Phase);
    IdentifierMap<SymbolInfo> Symbols;
    IdentifierMap<IdentifierMap<std::vector<FunctionDefAST*>>> ClassMethods;
    std::string CurrentClass;
    const std::vector<FunctionDefAST *> *findMethod(const std::string &Class, const std::string &Name) const;
    bool isRegisteredMethod(const std::string &Class, const PrototypeAST *Method) const;
    const FuncSig *resolveMethod(const std::string &Class, const std::string &Name,
                                 const std::vector<std::unique_ptr<ExprAST>> &Args);
    llvm::Value *emitMemberCall(MemberCallExprAST *Call);
    llvm::Value *emitMethodCall(const std::string &Class, llvm::Value *Receiver,
                                const std::string &Name,
                                const std::vector<std::unique_ptr<ExprAST>> &Args, int Line);
    bool typeNeedsDestruction(const std::string &TypeName) const;
    bool emitDestroy(const std::string &TypeName, llvm::Value *Addr, int Line);
    bool typeNeedsConstruction(const std::string &TypeName) const;
    bool emitConstruct(const std::string &TypeName, llvm::Value *Addr,
                       const std::vector<std::unique_ptr<ExprAST>> &Args, int Line);
    bool emitDefaultConstruct(const std::string &TypeName, llvm::Value *Addr, int Line);
    struct StackObject {
        llvm::Value *Addr;
        std::string TypeName;
        llvm::AllocaInst *Alive;
        llvm::AllocaInst *Count; // Non-null for an array; Addr then holds its buffer pointer.
        int Line;
        bool IsCopy;
    };
    std::vector<StackObject> StackObjects;
    size_t registerStackObject(llvm::Value *Addr, const std::string &TypeName, int Line,
                               bool IsArray = false, bool IsCopy = false);
    void activateStackObject(size_t Index, llvm::Value *Count = nullptr);
    void emitStackObjectCleanup(const StackObject &Object);
    void finishStackObjects(llvm::Function *F);
    struct ConstantScope {
        unsigned ID;
        IdentifierMap<std::optional<SymbolInfo>> Saved;
    };
    std::vector<ConstantScope> ConstantScopes = {{0, {}}};
    unsigned NextScopeID = 1;
    void emitScopedBlock(const std::vector<std::unique_ptr<StmtAST>> &Body);
    void warnMemberShadow(const std::string &Name, int Line) const;
    bool prepareSymbolDeclaration(const std::string &Name, bool IsConstant = false);
    bool requireWritable(const SymbolInfo &Info, const std::string &Name);
    void emitConstantStmt(ConstantStmtAST *Stmt);
    std::unique_ptr<TypeSystem> Types;

    std::unique_ptr<ArrayHandler> Arrays;
    std::unique_ptr<ExceptionHandler> Exceptions;
    std::unique_ptr<RuntimeCheck> RuntimeChecker;
    std::unique_ptr<FunctionGen> FuncGen;

    std::unique_ptr<ArithmeticHandler> ArithHandler;
    std::unique_ptr<StringHandler> StrHandler;
    std::unique_ptr<StringConversionHandler> StrConvHandler;
    std::unique_ptr<FileHandler> Files;
    std::unique_ptr<MemoryHandler> Memory;

    llvm::FunctionCallee PrintfFunc;
    llvm::FunctionCallee ScanfFunc;
    llvm::FunctionCallee MallocFunc;
    
    llvm::Value *PrintfFormatStr;       // %lld\n
    llvm::Value *PrintfFloatFormatStr;  // %f\n
    llvm::Value *PrintfStringFormatStr; // %s\n
    llvm::Value *PrintfCharFormatStr;   // %c\n
    
    llvm::Value *ScanfFormatStr;        // %lld
    llvm::Value *ScanfFloatFormatStr;   // %lf
    llvm::Value *ScanfStringFormatStr;  // %s
    
    llvm::Value *TrueStr;               // "TRUE"
    llvm::Value *FalseStr;              // "FALSE"
    llvm::Value *EmptyStringStr;        // ""

    bool HadError = false;

    void SetupExternalFunctions();
    llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, const std::string &VarName);
    llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction, llvm::Type *AllocType, const std::string &VarName);

    void registerSymbol(const std::string &Name, llvm::Value *Storage, const std::string &TypeName, bool IsArray = false);
    const SymbolInfo *getSymbolInfo(const std::string &Name) const;
    bool isWholeArrayVar(ExprAST *Expr) const;
    const TypeInfo *getExprTypeInfo(ExprAST *Expr);
    llvm::Value *coerceValueToType(llvm::Value *Val, const TypeInfo *TargetInfo);
    llvm::Value *convertNumericValue(llvm::Value *Val, const TypeInfo *Source,
                                     const TypeInfo *Target);
    llvm::Value *emitCast(CastExprAST *Cast);
    llvm::Value *emitBinaryOp(int Op, llvm::Value *L, const TypeInfo *LT,
                              llvm::Value *R, const TypeInfo *RT, int Line);
    llvm::Value *emitCBinaryOp(int Op, llvm::Value *L, const TypeInfo *Left,
                               llvm::Value *R, const TypeInfo *Right, int Line);
    void emitCOutput(llvm::Value *Val, const TypeInfo *Info, bool AppendNewline);
    void emitCInput(llvm::Value *Addr, const TypeInfo *Info);

    // TYPE-system registration (Designator.cc) and operators (TypeCheckGen.cc).
    void runTypePrePass(const std::vector<std::unique_ptr<StmtAST>> &Statements, int Phase);
    llvm::Value *emitUserKindBinaryOp(int Op, llvm::Value *L, const TypeInfo *LT,
                                      llvm::Value *R, const TypeInfo *RT, int Line);

    // An addressable location and the pseudocode type stored there.
    struct LValueInfo {
        llvm::Value *Addr = nullptr;
        std::string TypeName;
    };
    // Resolves a variable / array element / field chain / deref to an address
    // (emitting index and null-pointer checks along the way).
    bool emitLValue(ExprAST *E, LValueInfo &Out);
    // The rvalue read: load + the STRING null->"" guard.
    llvm::Value *loadFromLValue(const LValueInfo &LV);
    llvm::Value *emitAllocationSize(ExprAST *Expr);
    llvm::Value *emitCall(const std::string &Callee,
                          const std::vector<std::unique_ptr<ExprAST>> &ArgExprs, int Line);
    const FuncSig *resolveCall(const std::string &Name,
                               const std::vector<std::unique_ptr<ExprAST>> &Args);
    struct OperatorResolution {
        const FuncSig *Signature = nullptr;
        bool Ambiguous = false;
    };
    OperatorResolution resolveOperator(int Op, const TypeInfo *Left, const TypeInfo *Right = nullptr);
    llvm::Value *emitVariadicArgument(ExprAST *Expr, const std::string &Callee, size_t Position);
    bool marshalCallArgs(const FuncSig &Signature,
                         const std::vector<std::unique_ptr<ExprAST>> &ArgExprs,
                         int Line,
                         std::vector<llvm::Value*> &Out);
    llvm::Value *symbolAddress(const SymbolInfo &Info);
    void emitAssignment(const LValueInfo &Target, ExprAST *Expr,
                        const SymbolInfo *Symbol = nullptr);
    void emitDeclareStmt(DeclareStmtAST *Stmt);
    void emitOutputValue(llvm::Value *Val, const TypeInfo *TypeInfo, bool AppendNewline = true);

    void emitCaseStmt(CaseStmtAST *Stmt);
    void emitIfStmt(IfStmtAST *Stmt);
    void emitTryStmt(TryStmtAST *Stmt);
    void emitWhileStmt(WhileStmtAST *Stmt);
    void emitRepeatStmt(RepeatStmtAST *Stmt);
    void emitForStmt(ForStmtAST *Stmt);

public:
    CodeGen();
    ~CodeGen();
    void compile(const std::vector<std::unique_ptr<StmtAST>> &Statements);
    void compile(const std::vector<SourceUnit> &Sources);
    size_t getMainUnit() const { return MainUnit; }
    void print(llvm::raw_ostream &Output);
    const llvm::Module &getModule() const { return *TheModule; }
    bool hadError() const { return HadError; }
    void reportError(const char *Fmt, ...);
    static bool isBuiltinName(const std::string &Name);
    
    llvm::Value *emitExpr(ExprAST *Expr);
    void emitStmt(StmtAST *Stmt);

    // STRING slots zero-init to null; normalize null to "" at load/print sites.
    llvm::Value *emitStringNullGuard(llvm::Value *StrVal);

    // Name-level admission gate for Enum/Record/Pointer plus the existing
    // representation coercion for builtin targets. The ONLY door through
    // which assignments, BYVAL args, RETURN values and file/builtin operands
    // may produce a typed value. (TypeCheckGen.cc)
    llvm::Value *emitCoercedExpr(ExprAST *E, const TypeInfo *TargetInfo);

    const TypeInfo *resolveType(const std::string &TypeName) const;

    friend class ArrayHandler;
};

}
