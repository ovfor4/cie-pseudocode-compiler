#include "cps/ArrayHandler.h"
#include "cps/CodeGen.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

void ArrayHandler::bindArrayParameter(const std::string &Name,
                                      const std::string &ElemTypeName,
                                      Value *DataPtr,
                                      const std::vector<Value*> &LBs,
                                      const std::vector<Value*> &UBs,
                                      bool MakeCopy,
                                      CodeGen &CG) {
    const TypeInfo *ElemInfo = Types.resolve(ElemTypeName);
    if (!ElemInfo || !ElemInfo->LLVMType || ElemInfo->isVoid()) {
        CG.reportError("Unknown array element type %s", ElemTypeName.c_str());
        return;
    }

    std::vector<Value*> Multipliers;
    Value *TotalElements = nullptr;
    computeDimsAndMultipliers(Name, LBs, UBs, Multipliers, TotalElements);

    ArrayMetadata Meta;
    Meta.Rank = static_cast<int>(LBs.size());
    Meta.ElementTypeName = ElemInfo->Name;
    Meta.ElementType = ElemInfo->LLVMType;
    Meta.ElementSizeC = Types.getSizeOfConstant(ElemInfo->LLVMType);
    Meta.LowerBounds = LBs;
    Meta.UpperBounds = UBs;
    Meta.Multipliers = std::move(Multipliers);
    ArrayTable[Name] = Meta;

    Value *Ptr = DataPtr;
    if (MakeCopy) {
        // BYVAL: whole-buffer copy so callee writes stay invisible to the
        // caller. The copy is malloc'd and never freed, consistent with the
        // project-wide string policy.
        Value *TotalBytes = Builder->CreateMul(TotalElements, Meta.ElementSizeC,
                                               Name + "_copy_bytes");
        Value *Copy = Builder->CreateCall(MallocFunc, TotalBytes, Name + "_copy");
        FunctionType *MemcpyType = FunctionType::get(
            PointerType::getUnqual(*TheContext),
            {PointerType::getUnqual(*TheContext), PointerType::getUnqual(*TheContext),
             Type::getInt64Ty(*TheContext)},
            false);
        FunctionCallee MemcpyFunc = TheModule->getOrInsertFunction("memcpy", MemcpyType);
        Builder->CreateCall(MemcpyFunc, {Copy, DataPtr, TotalBytes});
        Ptr = Copy;
    }

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    AllocaInst *Alloca = CG.CreateEntryBlockAlloca(TheFunction, PointerType::getUnqual(*TheContext), Name);
    Builder->CreateStore(Ptr, Alloca);

    CG.registerSymbol(Name, Alloca, ElemInfo->Name, true);
}

bool ArrayHandler::emitArrayArgument(ExprAST *ArgExpr,
                                     const std::string &ElemTypeName,
                                     int Rank,
                                     const std::vector<std::pair<int64_t, int64_t>> &DeclaredBounds,
                                     const std::string &Callee,
                                     int Line,
                                     CodeGen &CG,
                                     std::vector<Value*> &Out) {
    auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr);
    const ArrayMetadata *Meta = Var ? getMetadata(Var->getName()) : nullptr;
    if (!Meta) {
        CG.reportError("Argument to %s must be a whole array variable (ARRAY OF %s)",
                       Callee.c_str(), ElemTypeName.c_str());
        return false;
    }
    if (!CG.resolveType(Meta->ElementTypeName)->sameType(CG.resolveType(ElemTypeName))) {
        CG.reportError("Array argument %s to %s must have element type %s (got %s)",
                       Var->getName().c_str(), Callee.c_str(),
                       ElemTypeName.c_str(), Meta->ElementTypeName.c_str());
        return false;
    }
    if (Meta->Rank != Rank) {
        CG.reportError("Array argument %s to %s must have %d dimension%s (got %d)",
                       Var->getName().c_str(), Callee.c_str(),
                       Rank, Rank == 1 ? "" : "s", Meta->Rank);
        return false;
    }

    // Bounded parameters are a contract: verified at compile time when the
    // caller's bounds are constants (the common case), at run time otherwise.
    for (size_t D = 0; D < DeclaredBounds.size(); ++D) {
        int64_t DeclLo = DeclaredBounds[D].first;
        int64_t DeclHi = DeclaredBounds[D].second;
        auto *ActLo = dyn_cast<ConstantInt>(Meta->LowerBounds[D]);
        auto *ActHi = dyn_cast<ConstantInt>(Meta->UpperBounds[D]);
        if (ActLo && ActHi) {
            if (ActLo->getSExtValue() != DeclLo || ActHi->getSExtValue() != DeclHi) {
                CG.reportError("Array argument %s to %s has bounds [%lld:%lld] but the parameter declares [%lld:%lld]",
                               Var->getName().c_str(), Callee.c_str(),
                               static_cast<long long>(ActLo->getSExtValue()),
                               static_cast<long long>(ActHi->getSExtValue()),
                               static_cast<long long>(DeclLo),
                               static_cast<long long>(DeclHi));
                return false;
            }
        } else {
            RuntimeChecker.emitArrayArgBoundsCheck(Meta->LowerBounds[D], Meta->UpperBounds[D],
                                                   DeclLo, DeclHi, Line);
        }
    }

    Value *DataPtr = getArrayBasePointer(Var->getName());
    if (!DataPtr) return false;
    Out.push_back(DataPtr);
    for (int D = 0; D < Rank; ++D) {
        Out.push_back(Meta->LowerBounds[D]);
        Out.push_back(Meta->UpperBounds[D]);
    }
    return true;
}
