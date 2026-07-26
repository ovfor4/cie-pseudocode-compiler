#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "cps/AST.h"
#include "cps/RuntimeCheck.h"
#include "cps/TypeSystem.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cps {

struct ArrayMetadata {
    int Rank = 0;
    std::string ElementTypeName;
    llvm::Type *ElementType = nullptr;
    uint64_t ElementSize = 0;
    std::vector<llvm::Value*> LowerBounds;
    std::vector<llvm::Value*> UpperBounds;
    std::vector<llvm::Value*> Multipliers;
};

class CodeGen;

class ArrayHandler {
    llvm::LLVMContext *TheContext;
    llvm::IRBuilder<> *Builder;
    llvm::Module *TheModule;
    std::map<std::string, llvm::Value*> *NamedValues;
    std::map<std::string, SymbolInfo> *Symbols;
    TypeSystem &Types;

    std::map<std::string, ArrayMetadata> ArrayTable;
    RuntimeCheck &RuntimeChecker;

    llvm::FunctionCallee MallocFunc;

    llvm::Value *computeFlatIndex(const std::string &Name, const std::vector<llvm::Value*> &Indices);
    llvm::Value *getArrayBasePointer(const std::string &Name);
    llvm::Value *getElementPointer(const std::string &Name, llvm::Value *Offset);
    const ArrayMetadata *getMetadata(const std::string &Name) const;
    bool emitCheckedIndices(const std::string &Name,
                            const std::vector<std::unique_ptr<ExprAST>> &IndexExprs,
                            int Line,
                            bool RequireExact,
                            CodeGen &CG,
                            std::vector<llvm::Value*> &Out);
    llvm::Value *loadElement(const std::string &Name, llvm::Value *Offset, CodeGen &CG);
    void emitPrintLoop(const std::string &Name,
                       int CurrentDim,
                       std::vector<llvm::Value*> CurrentIndices,
                       CodeGen &CG);

public:
    ArrayHandler(llvm::LLVMContext &C,
                 llvm::IRBuilder<> &B,
                 llvm::Module &M,
                 std::map<std::string, llvm::Value*> &NV,
                 std::map<std::string, SymbolInfo> &Sym,
                 TypeSystem &TS,
                 RuntimeCheck &RC);

    void setupExternalFunctions();

    void emitArrayDeclare(ArrayDeclareStmtAST *Stmt, CodeGen &CG);
    void emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG);
    llvm::Value *emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG);

    bool tryEmitArrayOutput(ExprAST *Expr, CodeGen &CG);
};

} // namespace cps
