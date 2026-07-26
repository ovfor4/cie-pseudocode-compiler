#include "cps/TypeSystem.h"
#include "llvm/IR/Constants.h"
#include <utility>

using namespace llvm;
using namespace cps;

const RecordFieldInfo *RecordPayload::findField(const std::string &Name) const {
    for (const auto &F : Fields) {
        if (F.first == Name) return &F.second;
    }
    return nullptr;
}

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

bool TypeInfo::isEnum() const {
    return Kind == TypeKind::Enum;
}

bool TypeInfo::isRecord() const {
    return Kind == TypeKind::Record;
}

bool TypeInfo::isPointer() const {
    return Kind == TypeKind::Pointer;
}

bool TypeInfo::isUserKind() const {
    return Kind == TypeKind::Enum || Kind == TypeKind::Record || Kind == TypeKind::Pointer;
}

bool TypeInfo::isOutputtable() const {
    switch (Kind) {
    case TypeKind::Integer:
    case TypeKind::Real:
    case TypeKind::Boolean:
    case TypeKind::String:
    case TypeKind::Char:
    case TypeKind::Enum: // prints its ordinal
        return true;
    case TypeKind::Void:
    case TypeKind::Record:
    case TypeKind::Pointer:
        return false;
    }
    return false;
}

const RecordPayload *TypeInfo::asRecord() const {
    return Kind == TypeKind::Record ? &Record : nullptr;
}

const EnumPayload *TypeInfo::asEnum() const {
    return Kind == TypeKind::Enum ? &Enum : nullptr;
}

const PointerPayload *TypeInfo::asPointer() const {
    return Kind == TypeKind::Pointer ? &Pointer : nullptr;
}

TypeSystem::TypeSystem(LLVMContext &Ctx) : Context(Ctx) {
    registerBuiltins();
}

void TypeSystem::registerBuiltins() {
    registerType("INTEGER", Type::getInt64Ty(Context), TypeKind::Integer);
    registerType("REAL", Type::getDoubleTy(Context), TypeKind::Real);
    registerType("BOOLEAN", Type::getInt1Ty(Context), TypeKind::Boolean);
    registerType("STRING", PointerType::getUnqual(Context), TypeKind::String);
    registerType("CHAR", Type::getInt8Ty(Context), TypeKind::Char);
    registerType("VOID", Type::getVoidTy(Context), TypeKind::Void);
}

const TypeInfo *TypeSystem::resolve(const std::string &TypeName) const {
    auto It = Types.find(TypeName);
    if (It == Types.end()) {
        return nullptr;
    }
    return &It->second;
}

const TypeInfo *TypeSystem::registerType(const std::string &TypeName,
                                         Type *LLVMType,
                                         TypeKind Kind) {
    TypeInfo Info;
    Info.Name = TypeName;
    Info.LLVMType = LLVMType;
    Info.Kind = Kind;

    auto Result = Types.insert_or_assign(TypeName, std::move(Info));
    return &Result.first->second;
}

bool TypeSystem::declareUserType(const std::string &Name, TypeKind Kind, std::string &Err) {
    if (Types.count(Name)) {
        Err = "Type " + Name + " is already defined";
        return false;
    }
    if (EnumConstants.count(Name)) {
        Err = "'" + Name + "' is an enum value and cannot name a type";
        return false;
    }

    Type *LLVMType = nullptr;
    switch (Kind) {
    case TypeKind::Enum:
        LLVMType = Type::getInt64Ty(Context); // INTEGER representation; identity is the name
        break;
    case TypeKind::Pointer:
        LLVMType = PointerType::getUnqual(Context);
        break;
    case TypeKind::Record:
        // Opaque named struct; the body is set in pass B.
        LLVMType = StructType::create(Context, std::string(kRecordStructPrefix) + Name);
        break;
    default:
        Err = "Internal: declareUserType called with a builtin kind";
        return false;
    }

    registerType(Name, LLVMType, Kind);
    return true;
}

bool TypeSystem::defineEnum(const std::string &Name,
                            const std::vector<std::string> &Values,
                            std::string &Err) {
    auto It = Types.find(Name);
    if (It == Types.end() || It->second.Kind != TypeKind::Enum) {
        Err = "Internal: defineEnum on undeclared enum " + Name;
        return false;
    }

    for (size_t I = 0; I < Values.size(); ++I) {
        const std::string &V = Values[I];
        for (size_t J = 0; J < I; ++J) {
            if (Values[J] == V) {
                Err = "Duplicate value '" + V + "' in enum " + Name;
                return false;
            }
        }
        if (Types.count(V)) {
            Err = "Enum value '" + V + "' in " + Name + " collides with a type name";
            return false;
        }
        if (auto CIt = EnumConstants.find(V); CIt != EnumConstants.end()) {
            Err = "Enum value '" + V + "' in " + Name + " is already a value of enum " +
                  CIt->second.TypeName;
            return false;
        }
    }

    It->second.Enum.ValueNames = Values;
    for (size_t I = 0; I < Values.size(); ++I) {
        EnumConstants[Values[I]] = {Name, static_cast<int64_t>(I) + kEnumOrdinalBase};
    }
    return true;
}

bool TypeSystem::defineRecord(const std::string &Name,
                              const std::vector<std::pair<std::string, std::string>> &Fields,
                              std::string &Err) {
    auto It = Types.find(Name);
    if (It == Types.end() || It->second.Kind != TypeKind::Record) {
        Err = "Internal: defineRecord on undeclared record " + Name;
        return false;
    }

    std::vector<Type*> FieldTypes;
    RecordPayload Payload;
    for (const auto &[FieldName, FieldTypeName] : Fields) {
        if (Payload.findField(FieldName)) {
            Err = "Duplicate field '" + FieldName + "' in record " + Name;
            return false;
        }
        const TypeInfo *FieldInfo = resolve(FieldTypeName);
        if (!FieldInfo) {
            Err = "Unknown type " + FieldTypeName + " for field " + FieldName +
                  " of record " + Name;
            return false;
        }
        if (FieldInfo->isVoid()) {
            Err = "Field " + FieldName + " of record " + Name + " cannot be VOID";
            return false;
        }
        // By-value record fields require a *completed* record (source order);
        // this also rules out self/mutual inclusion. Inclusion via a pointer
        // type is fine (opaque ptr, always complete).
        if (FieldInfo->isRecord() && llvm::cast<StructType>(FieldInfo->LLVMType)->isOpaque()) {
            Err = "Record type " + FieldTypeName + " must be fully defined before field " +
                  FieldName + " of record " + Name + " (records cannot contain themselves)";
            return false;
        }

        RecordFieldInfo FI;
        FI.TypeName = FieldInfo->Name;
        FI.Index = static_cast<unsigned>(FieldTypes.size());
        Payload.Fields.emplace_back(FieldName, FI);
        FieldTypes.push_back(FieldInfo->LLVMType);
    }

    llvm::cast<StructType>(It->second.LLVMType)->setBody(FieldTypes);
    It->second.Record = std::move(Payload);
    return true;
}

bool TypeSystem::definePointer(const std::string &Name,
                               const std::string &PointeeName,
                               std::string &Err) {
    auto It = Types.find(Name);
    if (It == Types.end() || It->second.Kind != TypeKind::Pointer) {
        Err = "Internal: definePointer on undeclared pointer type " + Name;
        return false;
    }

    const TypeInfo *Pointee = resolve(PointeeName);
    if (!Pointee) {
        Err = "Unknown pointee type " + PointeeName + " in pointer type " + Name;
        return false;
    }
    if (Pointee->isVoid()) {
        Err = "Pointer type " + Name + " cannot point to VOID";
        return false;
    }

    It->second.Pointer.PointeeTypeName = Pointee->Name;
    return true;
}

const EnumConstant *TypeSystem::lookupEnumConstant(const std::string &Name) const {
    auto It = EnumConstants.find(Name);
    return It == EnumConstants.end() ? nullptr : &It->second;
}

Type *TypeSystem::getLLVMType(const std::string &TypeName) const {
    const TypeInfo *Info = resolve(TypeName);
    return Info ? Info->LLVMType : nullptr;
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
        case TypeKind::Enum:
            return ConstantInt::get(Context, APInt(64, kEnumOrdinalBase));
        case TypeKind::Record:
            return Constant::getNullValue(Info->LLVMType); // zeroinitializer
        case TypeKind::Pointer:
            return ConstantPointerNull::get(cast<PointerType>(Info->LLVMType));
    }

    return Constant::getNullValue(Info->LLVMType);
}
