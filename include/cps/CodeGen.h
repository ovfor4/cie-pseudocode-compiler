#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/RuntimeCheck.h"
#include "cps/FunctionGen.h"
#include "cps/TypeSystem.h"

#include "cps/ArithmeticHandler.h"
#include "cps/StringHandler.h"
#include "cps/StringConversionHandler.h"

#include <map>
#include <memory>
#include <string>

namespace cps {

class ArrayHandler;
class FileHandler;

class CodeGen {
    std::unique_ptr<llvm::LLVMContext> TheContext;
    std::unique_ptr<llvm::Module> TheModule;
    std::unique_ptr<llvm::IRBuilder<>> Builder;
    std::map<std::string, SymbolInfo> Symbols;
    std::unique_ptr<TypeSystem> Types;

    std::unique_ptr<ArrayHandler> Arrays;
    std::unique_ptr<RuntimeCheck> RuntimeChecker;
    std::unique_ptr<FunctionGen> FuncGen;

    std::unique_ptr<ArithmeticHandler> ArithHandler;
    std::unique_ptr<StringHandler> StrHandler;
    std::unique_ptr<StringConversionHandler> StrConvHandler;
    std::unique_ptr<FileHandler> Files;

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
    const TypeInfo *getExprTypeInfo(ExprAST *Expr) const;
    llvm::Value *coerceValueToType(llvm::Value *Val, const TypeInfo *TargetInfo);

    // TYPE-system helpers (Designator.cc).
    void runTypePrePass(const std::vector<std::unique_ptr<StmtAST>> &Statements);
    llvm::Value *emitUserKindBinaryOp(BinaryExprAST *Bin, const TypeInfo *LT, const TypeInfo *RT);

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
    bool marshalCallArgs(const std::string &Callee,
                         const std::vector<std::unique_ptr<ExprAST>> &ArgExprs,
                         std::vector<llvm::Value*> &Out);
    void emitDeclareStmt(DeclareStmtAST *Stmt);
    void emitOutputValue(llvm::Value *Val, const TypeInfo *TypeInfo, bool AppendNewline = true);

    void emitIfStmt(IfStmtAST *Stmt);
    void emitWhileStmt(WhileStmtAST *Stmt);
    void emitRepeatStmt(RepeatStmtAST *Stmt);
    void emitForStmt(ForStmtAST *Stmt);

public:
    CodeGen();
    ~CodeGen();
    void compile(const std::vector<std::unique_ptr<StmtAST>> &Statements);
    void print();
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
    // may produce a typed value. (Designator.cc)
    llvm::Value *emitCoercedExpr(ExprAST *E, const TypeInfo *TargetInfo);

    const TypeInfo *resolveType(const std::string &TypeName) const;

    friend class ArrayHandler;
};

}
