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
    const TypeInfo *getExprTypeInfo(ExprAST *Expr) const;
    llvm::Value *coerceValueToType(llvm::Value *Val, const TypeInfo *TargetInfo);
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
    
    llvm::Value *emitExpr(ExprAST *Expr);
    void emitStmt(StmtAST *Stmt);

    const TypeInfo *resolveType(const std::string &TypeName) const;
    llvm::Type *getLLVMType(const std::string &TypeName) const;
    llvm::Value *getNamedValue(const std::string &Name) const;
    
    friend class ArrayHandler;
};

}
