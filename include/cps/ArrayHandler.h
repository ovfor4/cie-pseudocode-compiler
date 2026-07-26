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
    llvm::Constant *ElementSizeC = nullptr; // i64 sizeof constant (target-folded)
    std::vector<llvm::Value*> LowerBounds;
    std::vector<llvm::Value*> UpperBounds;
    std::vector<llvm::Value*> Multipliers;
};

class CodeGen;

class ArrayHandler {
    llvm::LLVMContext *TheContext;
    llvm::IRBuilder<> *Builder;
    llvm::Module *TheModule;
    std::map<std::string, SymbolInfo> *Symbols;
    TypeSystem &Types;

    std::map<std::string, ArrayMetadata> ArrayTable;
    RuntimeCheck &RuntimeChecker;

    llvm::FunctionCallee MallocFunc;

    llvm::Value *computeFlatIndex(const std::string &Name, const std::vector<llvm::Value*> &Indices);
    llvm::Value *getArrayBasePointer(const std::string &Name);
    llvm::Value *getElementPointer(const std::string &Name, llvm::Value *Offset);
    const ArrayMetadata *getMetadata(const std::string &Name) const;
    // Shared bounds math: DimSize_i = (Upper_i - Lower_i) + 1, row-major
    // multipliers back-to-front, TotalElements = product of the sizes.
    void computeDimsAndMultipliers(const std::string &Name,
                                   const std::vector<llvm::Value*> &Lows,
                                   const std::vector<llvm::Value*> &Highs,
                                   std::vector<llvm::Value*> &Multipliers,
                                   llvm::Value *&TotalElements);
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
                 std::map<std::string, SymbolInfo> &Sym,
                 TypeSystem &TS,
                 RuntimeCheck &RC);

    void setupExternalFunctions();

    void emitArrayDeclare(ArrayDeclareStmtAST *Stmt, CodeGen &CG);
    void emitArrayAssign(ArrayAssignStmtAST *Stmt, CodeGen &CG);
    llvm::Value *emitArrayAccess(ArrayAccessExprAST *Expr, CodeGen &CG);

    bool tryEmitArrayOutput(ExprAST *Expr, CodeGen &CG);

    // Full-rank checked element address (bounds checks included); reports the
    // element type name through ElemTypeNameOut. The lvalue layer builds
    // field/deref accesses on top of this.
    llvm::Value *emitElementAddress(const std::string &Name,
                                    const std::vector<std::unique_ptr<ExprAST>> &IndexExprs,
                                    int Line,
                                    CodeGen &CG,
                                    std::string &ElemTypeNameOut);
    llvm::Value *emitElementAddress(ArrayAccessExprAST *Expr,
                                    CodeGen &CG,
                                    std::string &ElemTypeNameOut);

    // Swap in a fresh table (function bodies get their own scope, mirroring
    // the Symbols save/clear/restore discipline); returns the previous table.
    std::map<std::string, ArrayMetadata> exchangeTable(std::map<std::string, ArrayMetadata> NewTable);

    // Callee side of an array parameter: rebuild metadata from the incoming
    // data pointer + per-dim bounds. MakeCopy (BYVAL) mallocs and memcpys the
    // whole buffer so callee writes stay invisible to the caller.
    void bindArrayParameter(const std::string &Name,
                            const std::string &ElemTypeName,
                            llvm::Value *DataPtr,
                            const std::vector<llvm::Value*> &LBs,
                            const std::vector<llvm::Value*> &UBs,
                            bool MakeCopy,
                            CodeGen &CG);

    // Call side: validate a whole-array argument against the declared
    // parameter (element type and rank exactly; bounded params check their
    // bounds at compile time when constant, else at run time) and append the
    // data pointer plus per-dim bounds to Out.
    bool emitArrayArgument(ExprAST *ArgExpr,
                           const std::string &ElemTypeName,
                           int Rank,
                           const std::vector<std::pair<int64_t, int64_t>> &DeclaredBounds,
                           const std::string &Callee,
                           int Line,
                           CodeGen &CG,
                           std::vector<llvm::Value*> &Out);
};

} // namespace cps
