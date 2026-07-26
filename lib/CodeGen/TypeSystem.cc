#include "cps/TypeSystem.h"
#include "llvm/IR/Constants.h"
#include <utility>

using namespace llvm;
using namespace cps;

bool TypeInfo::isNumeric() const {
    return Kind == TypeKind::Integer || Kind == TypeKind::Real;
}

bool TypeInfo::isIntegral() const {
    return Kind == TypeKind::Integer || Kind == TypeKind::Char;
}

bool TypeInfo::isReal() const {
    return Kind == TypeKind::Real;
}

bool TypeInfo::isBoolean() const {
    return Kind == TypeKind::Boolean;
}

bool TypeInfo::isString() const {
    return Kind == TypeKind::String;
}

bool TypeInfo::isChar() const {
    return Kind == TypeKind::Char;
}

bool TypeInfo::isVoid() const {
    return Kind == TypeKind::Void;
}

TypeSystem::TypeSystem(LLVMContext &Ctx) : Context(Ctx) {
    registerBuiltins();
}

void TypeSystem::registerBuiltins() {
    registerType("INTEGER",
                 Type::getInt64Ty(Context),
                 8,
                 TypeKind::Integer,
                 true,
                 false,
                 true);

    registerType("REAL",
                 Type::getDoubleTy(Context),
                 8,
                 TypeKind::Real,
                 true,
                 false,
                 true);

    registerType("BOOLEAN",
                 Type::getInt1Ty(Context),
                 1,
                 TypeKind::Boolean,
                 true,
                 false,
                 true);

    registerType("STRING",
                 PointerType::getUnqual(Context),
                 8,
                 TypeKind::String,
                 true,
                 true,
                 true);

    registerType("CHAR",
                 Type::getInt8Ty(Context),
                 1,
                 TypeKind::Char,
                 true,
                 false,
                 true);

    registerType("VOID",
                 Type::getVoidTy(Context),
                 0,
                 TypeKind::Void,
                 false,
                 false,
                 true);
}

const TypeInfo *TypeSystem::resolve(const std::string &TypeName) const {
    auto It = Types.find(TypeName);
    if (It == Types.end()) {
        return nullptr;
    }
    return &It->second;
}

bool TypeSystem::hasType(const std::string &TypeName) const {
    return resolve(TypeName) != nullptr;
}

const TypeInfo *TypeSystem::registerType(const std::string &TypeName,
                                         Type *LLVMType,
                                         uint64_t ElementSize,
                                         TypeKind Kind,
                                         bool Printable,
                                         bool ReferenceLike,
                                         bool Builtin) {
    TypeInfo Info;
    Info.Name = TypeName;
    Info.LLVMType = LLVMType;
    Info.ElementSize = ElementSize;
    Info.Kind = Kind;
    Info.Printable = Printable;
    Info.Builtin = Builtin;
    Info.ReferenceLike = ReferenceLike;

    auto Result = Types.insert_or_assign(TypeName, std::move(Info));
    return &Result.first->second;
}

Type *TypeSystem::getLLVMType(const std::string &TypeName) const {
    const TypeInfo *Info = resolve(TypeName);
    return Info ? Info->LLVMType : nullptr;
}

uint64_t TypeSystem::getElementSize(const std::string &TypeName) const {
    const TypeInfo *Info = resolve(TypeName);
    return Info ? Info->ElementSize : 0;
}

Constant *TypeSystem::getSizeOfConstant(Type *Ty) const {
    return ConstantExpr::getSizeOf(Ty);
}

Constant *TypeSystem::getZeroValue(const std::string &TypeName) const {
    const TypeInfo *Info = resolve(TypeName);
    if (!Info || !Info->LLVMType || Info->LLVMType->isVoidTy()) {
        return nullptr;
    }

    switch (Info->Kind) {
        case TypeKind::Integer:
            return ConstantInt::get(Context, APInt(64, 0));
        case TypeKind::Real:
            return ConstantFP::get(Context, APFloat(0.0));
        case TypeKind::Boolean:
            return ConstantInt::get(Context, APInt(1, 0));
        case TypeKind::Char:
            return ConstantInt::get(Context, APInt(8, 0));
        case TypeKind::String:
            return ConstantPointerNull::get(cast<PointerType>(Info->LLVMType));
        case TypeKind::Void:
            return nullptr;
        case TypeKind::Custom:
            return Constant::getNullValue(Info->LLVMType);
    }

    return Constant::getNullValue(Info->LLVMType);
}
