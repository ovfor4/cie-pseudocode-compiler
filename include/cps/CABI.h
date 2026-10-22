#pragma once
#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cps {

struct TypeInfo;
class TypeSystem;

// One EXTERN "C" parameter or result as clang's ABIArgInfo arranges it. Only
// the kinds our targets use exist here; ClangABI reports any other kind as an
// internal error.
struct CABIArg {
    enum class Kind { Direct, Extend, Indirect, Ignore };
    Kind K = Kind::Direct;
    llvm::Type *ValueType = nullptr;  // the pseudocode value; void for VOID
    llvm::Type *CoerceType = nullptr; // Direct, Extend: the IR type passed
    unsigned Offset = 0;              // Direct, Extend: byte offset of the coerced part
    bool Flatten = false;             // Direct: a struct CoerceType passed element by element
    unsigned DirectAlign = 0;         // Direct, Extend: stack alignment, if any
    llvm::Attribute::AttrKind Extension = llvm::Attribute::None; // Extend: SExt, ZExt or NoExt
    bool ByVal = false;               // Indirect: byval, otherwise a pointer to a copy
    uint64_t Align = 0;               // Indirect: alignment of the memory in bytes
    bool InReg = false;
};

// An arranged EXTERN signature. Args holds the Fixed prototype parameters and,
// for one variadic call, the promoted variadic arguments after them.
struct CABISignature {
    CABIArg Ret;
    std::vector<CABIArg> Args;
    size_t Fixed = 0;
    bool Variadic = false;
    unsigned CallingConv = 0;
};

// Arranges signatures for the host C ABI with clang's CodeGen. The clang
// context is created on the first call; ClangABI.cc is the only file that
// includes clang headers.
class ClangABI {
public:
    ClangABI(llvm::LLVMContext &Context, const TypeSystem &Types);
    ~ClangABI();
    // Ret is nullptr for VOID; the first Fixed of Args are the prototype's
    // parameters. On failure Err holds the reason for an internal error.
    bool arrange(const TypeInfo *Ret, const std::vector<const TypeInfo *> &Args, size_t Fixed,
                 bool Variadic, CABISignature &Sig, std::string &Err);

private:
    struct State;
    llvm::LLVMContext &Context;
    const TypeSystem &Types;
    std::unique_ptr<State> S;
};

// Generic lowering of an arranged signature (Lowering.cc), without
// per-architecture code.
// The declared function type, from the fixed parameters.
llvm::FunctionType *getCABIFunctionType(const CABISignature &Sig);
// The call attributes, for every argument in Sig.
llvm::AttributeList getCABIAttributes(llvm::LLVMContext &C, const CABISignature &Sig);
// The IR arguments for one pseudocode value per Sig.Args entry. SRet receives
// the result memory when the result is returned indirectly.
std::vector<llvm::Value *> lowerCABIArguments(llvm::IRBuilder<> &B, const CABISignature &Sig,
                                              llvm::ArrayRef<llvm::Value *> Args, llvm::Value *&SRet);
// The pseudocode result of Call; nullptr for VOID.
llvm::Value *lowerCABIResult(llvm::IRBuilder<> &B, const CABISignature &Sig, llvm::Value *Call,
                             llvm::Value *SRet);

} // namespace cps
