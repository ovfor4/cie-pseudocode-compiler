#include "cps/TypeSystem.h"
#include "cps/TypeAST.h"
#include "llvm/IR/Constants.h"
#include <utility>
#include <climits>
#include <limits>
#include <type_traits>
#include <sstream>
#include <cctype>

using namespace llvm;
using namespace cps;

namespace {

std::string cTypeName(const std::string &Identity) {
    return "C_TYPE(\"" + Identity + "\")";
}

// sizeof alone cannot distinguish padded x87 from IEEE quad or double-double.
template<typename T> Type *nativeFloatType(LLVMContext &C) {
    constexpr int Digits = std::numeric_limits<T>::digits;
    constexpr int Exponent = std::numeric_limits<T>::max_exponent;
    static_assert(std::numeric_limits<T>::radix == 2, "Unsupported native C floating-point radix");
    static_assert((Digits == 24 && Exponent == 128) ||
                  (Digits == 53 && Exponent == 1024) ||
                  (Digits == 64 && Exponent == 16384) ||
                  (Digits == 113 && Exponent == 16384) ||
                  (Digits == 106 && Exponent == 1024), "Unsupported native C floating-point format");
    if constexpr (Digits == 24) return Type::getFloatTy(C);
    if constexpr (Digits == 53) return Type::getDoubleTy(C);
    if constexpr (Digits == 64) return Type::getX86_FP80Ty(C);
    if constexpr (Digits == 113) return Type::getFP128Ty(C);
    return Type::getPPC_FP128Ty(C);
}

} // namespace

bool TypeInfo::isC() const { return !CIdentity.empty(); }

bool TypeInfo::sameType(const TypeInfo *Other) const {
    if (!Other) return false;
    if (isC() || Other->isC())
        return isC() && Other->isC() && CIdentity == Other->CIdentity;
    return Name == Other->Name;
}

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
    return Kind == TypeKind::Void || (isC() && CIdentity == "void");
}

bool TypeInfo::isEnum() const {
    return Kind == TypeKind::Enum;
}

bool TypeInfo::isRecord() const {
    return Kind == TypeKind::Record;
}

bool TypeInfo::isPointer() const {
    return Kind == TypeKind::Pointer || Kind == TypeKind::CPointer;
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
    case TypeKind::CScalar:
    case TypeKind::CPointer:
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
    return isPointer() ? &Pointer : nullptr;
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

    // The compiler's native C/C++ ABI determines representation, not a table
    // of assumed LP64/LLP64 widths. LLVM's addressable bytes are eight bits.
    static_assert(CHAR_BIT == 8, "LLVM requires eight-bit addressable bytes");
    auto Integral = [&](auto Sample, const char *Name, unsigned Rank,
                        const char *InputFormat, const char *OutputFormat) {
        using T = decltype(Sample);
        TypeInfo Info;
        Info.Name = cTypeName(Name);
        Info.CIdentity = Name;
        Info.Kind = TypeKind::CScalar;
        Info.LLVMType = Type::getIntNTy(Context, sizeof(T) * CHAR_BIT);
        Info.CRank = Rank;
        Info.CSigned = std::is_signed<T>::value;
        Info.CInputFormat = InputFormat;
        Info.COutputFormat = OutputFormat;
        Types.emplace(Info.Name, std::move(Info));
    };
    Integral(char{}, "char", 2, " %c", "%c");
    Integral(static_cast<signed char>(0), "signed char", 2, "%hhd", "%hhd");
    Integral(static_cast<unsigned char>(0), "unsigned char", 2, "%hhu", "%hhu");
    Integral(short{}, "short", 3, "%hd", "%hd");
    Integral(static_cast<unsigned short>(0), "unsigned short", 3, "%hu", "%hu");
    Integral(int{}, "int", 4, "%d", "%d");
    Integral(static_cast<unsigned int>(0), "unsigned int", 4, "%u", "%u");
    Integral(long{}, "long", 5, "%ld", "%ld");
    Integral(static_cast<unsigned long>(0), "unsigned long", 5, "%lu", "%lu");
    Integral(static_cast<long long>(0), "long long", 6, "%lld", "%lld");
    Integral(static_cast<unsigned long long>(0), "unsigned long long", 6, "%llu", "%llu");

    auto Floating = [&](auto Sample, const char *Name, unsigned Rank, const char *Format) {
        TypeInfo Info;
        Info.Name = cTypeName(Name);
        Info.CIdentity = Name;
        Info.Kind = TypeKind::CScalar;
        Info.LLVMType = nativeFloatType<decltype(Sample)>(Context);
        Info.CRank = Rank;
        Info.CInputFormat = Format;
        Info.COutputFormat = Rank == 9 ? "%Lf" : "%f";
        Types.emplace(Info.Name, std::move(Info));
    };
    Floating(float{}, "float", 7, "%f");
    Floating(double{}, "double", 8, "%lf");
    Floating(static_cast<long double>(0), "long double", 9, "%Lf");

    TypeInfo Bool;
    Bool.Name = cTypeName("_Bool");
    Bool.CIdentity = "_Bool";
    Bool.Kind = TypeKind::CScalar;
    Bool.LLVMType = Type::getInt1Ty(Context);
    Bool.CRank = 1;
    Types.emplace(Bool.Name, std::move(Bool));
    TypeInfo Void;
    Void.Name = cTypeName("void");
    Void.CIdentity = "void";
    Void.Kind = TypeKind::CScalar;
    Void.LLVMType = Type::getVoidTy(Context);
    Types.emplace(Void.Name, std::move(Void));

    std::string Err;
    defineCAlias("C_INT", "int", Err);
    defineCAlias("C_CHAR", "char", Err);
    defineCAlias("C_STRING", "char*", Err);
    defineCAlias("C_DOUBLE", "double", Err);
}

bool TypeSystem::sameType(const std::string &Left, const std::string &Right) const {
    const TypeInfo *L = resolve(Left);
    return L && L->sameType(resolve(Right));
}

const TypeInfo *TypeSystem::importCType(const std::string &Expression, std::string &Err) {
    // Whitespace is immaterial around stars; C's multiword base type is
    // validated separately. Stars must all precede or all follow the base.
    std::istringstream Input(Expression);
    std::string Word, Text;
    while (Input >> Word) {
        if (!Text.empty()) Text += ' ';
        Text += Word;
    }
    unsigned Depth = 0;
    bool Prefix = !Text.empty() && Text.front() == '*';
    if (Prefix) {
        while (!Text.empty() && (Text.front() == '*' || Text.front() == ' ')) {
            if (Text.front() == '*') ++Depth;
            Text.erase(Text.begin());
        }
    } else {
        while (!Text.empty() && (Text.back() == '*' || Text.back() == ' ')) {
            if (Text.back() == '*') ++Depth;
            Text.pop_back();
        }
    }
    std::istringstream BaseWords(Text);
    std::map<std::string, unsigned> Counts;
    bool Valid = !Text.empty();
    while (BaseWords >> Word) {
        if (Word != "signed" && Word != "unsigned" && Word != "short" &&
            Word != "long" && Word != "int" && Word != "char" &&
            Word != "float" && Word != "double" && Word != "_Bool" && Word != "void")
            Valid = false;
        ++Counts[Word];
    }
    for (const auto &Entry : Counts)
        if (Entry.second > (Entry.first == "long" ? 2u : 1u)) Valid = false;
    auto Has = [&](const char *Name) { return Counts.count(Name) != 0; };
    if (Has("signed") && Has("unsigned")) Valid = false;
    std::string Base;
    if (Has("void") || Has("_Bool") || Has("float")) {
        if (Counts.size() != 1) Valid = false;
        Base = Has("void") ? "void" : Has("_Bool") ? "_Bool" : "float";
    } else if (Has("double")) {
        if (Counts.size() != (Has("long") ? 2u : 1u) || (Has("long") && Counts["long"] > 1))
            Valid = false;
        Base = Has("long") ? "long double" : "double";
    } else if (Has("char")) {
        if (Has("int") || Has("short") || Has("long")) Valid = false;
        Base = Has("unsigned") ? "unsigned char" : Has("signed") ? "signed char" : "char";
    } else {
        if (Has("short") && Has("long")) Valid = false;
        Base = Has("short") ? "short" : Has("long") ?
            (Counts["long"] == 2 ? "long long" : "long") : "int";
        if (Has("unsigned")) Base = "unsigned " + Base;
    }
    if (!Valid) {
        Err = "Invalid or unsupported C type expression '" + Expression + "'";
        return nullptr;
    }
    const TypeInfo *Info = resolve(cTypeName(Base));
    for (unsigned I = 0; I < Depth; ++I) {
        std::string Identity = Info->CIdentity + '*';
        const TypeInfo *Existing = resolve(cTypeName(Identity));
        if (Existing) {
            Info = Existing;
            continue;
        }
        TypeInfo Pointer;
        Pointer.Name = cTypeName(Identity);
        Pointer.CIdentity = Identity;
        Pointer.Kind = TypeKind::CPointer;
        Pointer.LLVMType = PointerType::getUnqual(Context);
        Pointer.Pointer.PointeeTypeName = Info->Name;
        auto Result = Types.emplace(Pointer.Name, std::move(Pointer));
        Info = &Result.first->second;
    }
    return Info;
}

bool TypeSystem::defineCAlias(const std::string &Name, const std::string &Expression,
                             std::string &Err) {
    if (Types.count(Name) || EnumConstants.count(Name)) {
        Err = "Type name '" + Name + "' is already defined or reserved";
        return false;
    }
    const TypeInfo *C = importCType(Expression, Err);
    if (!C) return false;
    TypeInfo Alias = *C;
    Alias.Name = Name;
    Types.emplace(Name, std::move(Alias));
    return true;
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
                              const std::vector<RecordFieldDecl> &Fields,
                              std::string &Err) {
    auto It = Types.find(Name);
    if (It == Types.end() || It->second.Kind != TypeKind::Record) {
        Err = "Internal: defineRecord on undeclared record " + Name;
        return false;
    }

    std::vector<Type*> FieldTypes;
    RecordPayload Payload;
    for (const RecordFieldDecl &Field : Fields) {
        const std::string &FieldName = Field.Name;
        const std::string &FieldTypeName = Field.TypeName;
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
        FI.IsPublic = Field.IsPublic;
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
        case TypeKind::CScalar:
        case TypeKind::CPointer:
            return Constant::getNullValue(Info->LLVMType);
    }

    return Constant::getNullValue(Info->LLVMType);
}
