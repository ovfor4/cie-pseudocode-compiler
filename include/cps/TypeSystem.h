#pragma once
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <map>
#include <string>

namespace cps {

enum class TypeKind {
    Integer,
    Real,
    Boolean,
    String,
    Char,
    Void,
    Custom
};

struct TypeInfo {
    std::string Name;
    llvm::Type *LLVMType = nullptr;
    uint64_t ElementSize = 0;
    TypeKind Kind = TypeKind::Custom;
    bool Printable = false;
    bool Builtin = false;
    bool ReferenceLike = false;

    bool isNumeric() const;
    bool isIntegral() const;
    bool isReal() const;
    bool isBoolean() const;
    bool isString() const;
    bool isChar() const;
    bool isVoid() const;
};

struct SymbolInfo {
    llvm::Value *Storage = nullptr;
    std::string TypeName;
    bool IsArray = false;
};

class TypeSystem {
    llvm::LLVMContext &Context;
    std::map<std::string, TypeInfo> Types;

    void registerBuiltins();

public:
    explicit TypeSystem(llvm::LLVMContext &Ctx);

    const TypeInfo *resolve(const std::string &TypeName) const;
    bool hasType(const std::string &TypeName) const;

    const TypeInfo *registerType(const std::string &TypeName,
                                 llvm::Type *LLVMType,
                                 uint64_t ElementSize,
                                 TypeKind Kind = TypeKind::Custom,
                                 bool Printable = false,
                                 bool ReferenceLike = false,
                                 bool Builtin = false);

    llvm::Type *getLLVMType(const std::string &TypeName) const;
    uint64_t getElementSize(const std::string &TypeName) const;
    llvm::Constant *getZeroValue(const std::string &TypeName) const;

    // Target-independent sizeof: a null-GEP+ptrtoint i64 constant the backend
    // folds with the *real* target's DataLayout. The module carries no
    // DataLayout string, so sizes must never be computed host-side.
    llvm::Constant *getSizeOfConstant(llvm::Type *Ty) const;
};

} // namespace cps
