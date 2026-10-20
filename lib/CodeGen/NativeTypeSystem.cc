#include "cps/TypeSystem.h"
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


void TypeSystem::registerBuiltins() {
    registerType("INTEGER", Type::getInt64Ty(Context), TypeKind::Integer);
    registerType("REAL", Type::getDoubleTy(Context), TypeKind::Real);
    registerType("BOOLEAN", Type::getInt1Ty(Context), TypeKind::Boolean);
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
    importCType("void*", Err);
}


const TypeInfo *TypeSystem::promoteCScalar(const TypeInfo *Info) const {
    if (!Info || Info->Kind != TypeKind::CScalar || Info->isVoid()) return nullptr;
    if (Info->CRank >= 4) return Info;
    const TypeInfo *Int = resolve(cTypeName("int"));
    unsigned Width = Info->LLVMType->getIntegerBitWidth();
    unsigned IntWidth = Int->LLVMType->getIntegerBitWidth();
    return Width < IntWidth || (Width == IntWidth && Info->CSigned)
        ? Int : resolve(cTypeName("unsigned int"));
}

const TypeInfo *TypeSystem::commonCScalar(const TypeInfo *Left, const TypeInfo *Right) const {
    const TypeInfo *L = promoteCScalar(Left);
    const TypeInfo *R = promoteCScalar(Right);
    if (!L || !R) return nullptr;
    if (L->isReal() || R->isReal() || L->CSigned == R->CSigned)
        return L->CRank >= R->CRank ? L : R;
    const TypeInfo *S = L->CSigned ? L : R;
    const TypeInfo *U = L->CSigned ? R : L;
    if (U->CRank >= S->CRank) return U;
    if (S->LLVMType->getIntegerBitWidth() > U->LLVMType->getIntegerBitWidth()) return S;
    return resolve(cTypeName("unsigned " + S->CIdentity));
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
