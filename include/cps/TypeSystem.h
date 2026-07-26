#pragma once
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace cps {

// Enum ordinals are 0-based: DECLARE zero-init and array memset(0) then
// produce the first enumerator, a valid value.
inline constexpr int64_t kEnumOrdinalBase = 0;

// LLVM struct name prefix for record types: %record.NAME
inline constexpr const char *kRecordStructPrefix = "record.";

enum class TypeKind {
    Integer,
    Real,
    Boolean,
    String,
    Char,
    Void,
    Enum,
    Record,
    Pointer
};

// ---- Per-kind payloads (filled only for the matching TypeKind) ----

struct RecordFieldInfo {
    std::string TypeName;
    unsigned Index = 0; // struct field index
};

struct RecordPayload {
    std::vector<std::pair<std::string, RecordFieldInfo>> Fields; // declaration order
    const RecordFieldInfo *findField(const std::string &Name) const;
};

struct EnumPayload {
    std::vector<std::string> ValueNames; // ordinal = position + kEnumOrdinalBase
};

struct PointerPayload {
    std::string PointeeTypeName;
};

// A bare enum value name, resolvable in expression position.
struct EnumConstant {
    std::string TypeName;
    int64_t Ordinal = 0;
};

struct TypeInfo {
    std::string Name;
    llvm::Type *LLVMType = nullptr;
    TypeKind Kind = TypeKind::Void;

    RecordPayload Record;   // Kind == Record only
    EnumPayload Enum;       // Kind == Enum only
    PointerPayload Pointer; // Kind == Pointer only

    bool isNumeric() const;
    bool isIntegral() const;
    bool isReal() const;
    bool isBoolean() const;
    bool isString() const;
    bool isChar() const;
    bool isVoid() const;
    bool isEnum() const;
    bool isRecord() const;
    bool isPointer() const;
    // Enum/Record/Pointer: type identity is the NAME; representation-level
    // coercion must never be reached without a prior name-level check.
    bool isUserKind() const;
    // OUTPUT admissibility, derived from Kind (enums print their ordinal).
    bool isOutputtable() const;

    // Checked payload accessors: nullptr unless Kind matches.
    const RecordPayload *asRecord() const;
    const EnumPayload *asEnum() const;
    const PointerPayload *asPointer() const;
};

struct SymbolInfo {
    llvm::Value *Storage = nullptr;
    std::string TypeName;
    bool IsArray = false;
};

class TypeSystem {
    llvm::LLVMContext &Context;
    std::map<std::string, TypeInfo> Types;
    std::map<std::string, EnumConstant> EnumConstants;

    void registerBuiltins();
    const TypeInfo *registerType(const std::string &TypeName,
                                 llvm::Type *LLVMType,
                                 TypeKind Kind);

public:
    explicit TypeSystem(llvm::LLVMContext &Ctx);

    const TypeInfo *resolve(const std::string &TypeName) const;

    // Two-pass user-type registration (pass A then pass B, both in source
    // order). Pass A reserves every name so pointer pointees may forward-
    // reference; pass B fills definitions. On failure Err carries the
    // diagnostic and nothing is (further) registered.
    bool declareUserType(const std::string &Name, TypeKind Kind, std::string &Err);
    bool defineEnum(const std::string &Name,
                    const std::vector<std::string> &Values,
                    std::string &Err);
    bool defineRecord(const std::string &Name,
                      const std::vector<std::pair<std::string, std::string>> &Fields,
                      std::string &Err);
    bool definePointer(const std::string &Name,
                       const std::string &PointeeName,
                       std::string &Err);

    const EnumConstant *lookupEnumConstant(const std::string &Name) const;

    llvm::Type *getLLVMType(const std::string &TypeName) const;
    llvm::Constant *getZeroValue(const std::string &TypeName) const;

    // Target-independent sizeof: a null-GEP+ptrtoint i64 constant the backend
    // folds with the *real* target's DataLayout. The module carries no
    // DataLayout string, so sizes must never be computed host-side.
    llvm::Constant *getSizeOfConstant(llvm::Type *Ty) const;
};

} // namespace cps
