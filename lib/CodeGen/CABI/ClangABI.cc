// The only file that talks to clang. Keep it that way: clang::CodeGen clashes
// with cps::CodeGen, clang is built without RTTI (so never subclass its
// classes here), and the CodeGenerator must be destroyed before the ASTContext.
#include "cps/CABI.h"
#include "cps/TypeSystem.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/Builtins.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/IdentifierTable.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/CodeGen/CGFunctionInfo.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Driver/CreateInvocationFromArgs.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/TargetParser/Host.h"
#include <map>

using namespace cps;
using clang::CodeGen::ABIArgInfo;

// Members are destroyed in reverse order, so Gen goes before AST.
struct ClangABI::State {
    const TypeSystem *Types = nullptr;
    clang::DiagnosticOptions DiagOpts;
    llvm::IntrusiveRefCntPtr<clang::DiagnosticsEngine> Diags;
    std::shared_ptr<clang::CompilerInvocation> Invocation;
    llvm::IntrusiveRefCntPtr<clang::FileManager> Files;
    std::unique_ptr<clang::SourceManager> Sources;
    llvm::IntrusiveRefCntPtr<clang::TargetInfo> Target;
    std::unique_ptr<clang::IdentifierTable> Idents;
    clang::SelectorTable Selectors;
    clang::Builtin::Context Builtins;
    std::unique_ptr<clang::ASTContext> AST;
    std::unique_ptr<clang::CodeGenerator> Gen;
    std::map<const TypeInfo *, clang::CanQualType> Records;

    // Target and language options come from a driver invocation for the host
    // triple, as the C compiler would use them (e.g. Apple's darwinpcs and the
    // signedness of char).
    bool init(llvm::LLVMContext &Context, std::string &Err) {
        Diags = llvm::makeIntrusiveRefCnt<clang::DiagnosticsEngine>(
            clang::DiagnosticIDs::create(), DiagOpts, new clang::IgnoringDiagConsumer());
        std::string Triple = llvm::sys::getProcessTriple();
        std::string TargetArg = "--target=" + Triple;
        const char *Args[] = {"clang", TargetArg.c_str(), "-fsyntax-only", "-x", "c", "cps-abi.c"};
        clang::CreateInvocationOptions Opts;
        Opts.Diags = Diags;
        Invocation = clang::createInvocation(Args, Opts);
        if (Invocation) Target = clang::TargetInfo::CreateTargetInfo(*Diags, Invocation->getTargetOpts());
        if (!Target) {
            Err = "cannot create a clang C ABI context for " + Triple;
            return false;
        }
        clang::LangOptions &Lang = Invocation->getLangOpts();
        Target->adjust(*Diags, Lang, nullptr);
        auto VFS = llvm::vfs::getRealFileSystem();
        Files = llvm::makeIntrusiveRefCnt<clang::FileManager>(clang::FileSystemOptions(), VFS);
        Sources = std::make_unique<clang::SourceManager>(*Diags, *Files);
        Idents = std::make_unique<clang::IdentifierTable>(Lang);
        AST = std::make_unique<clang::ASTContext>(Lang, *Sources, *Idents, Selectors, Builtins,
                                                  clang::TU_Complete);
        AST->InitBuiltinTypes(*Target);
        Gen = clang::CreateLLVMCodeGen(*Diags, "cps-abi", VFS, Invocation->getHeaderSearchOpts(),
                                       Invocation->getPreprocessorOpts(),
                                       Invocation->getCodeGenOpts(), Context);
        Gen->Initialize(*AST);
        return true;
    }

    // C scalars map by name and every pointer to void *; records are built
    // once and checked against cpsc's layout.
    bool map(const TypeInfo *Info, clang::CanQualType &Out, std::string &Err) {
        using Member = clang::CanQualType clang::ASTContext::*;
        static const std::map<std::string, Member> Scalars = {
            {"_Bool", &clang::ASTContext::BoolTy},
            {"char", &clang::ASTContext::CharTy},
            {"signed char", &clang::ASTContext::SignedCharTy},
            {"unsigned char", &clang::ASTContext::UnsignedCharTy},
            {"short", &clang::ASTContext::ShortTy},
            {"unsigned short", &clang::ASTContext::UnsignedShortTy},
            {"int", &clang::ASTContext::IntTy},
            {"unsigned int", &clang::ASTContext::UnsignedIntTy},
            {"long", &clang::ASTContext::LongTy},
            {"unsigned long", &clang::ASTContext::UnsignedLongTy},
            {"long long", &clang::ASTContext::LongLongTy},
            {"unsigned long long", &clang::ASTContext::UnsignedLongLongTy},
            {"float", &clang::ASTContext::FloatTy},
            {"double", &clang::ASTContext::DoubleTy},
            {"long double", &clang::ASTContext::LongDoubleTy},
        };
        if (Info && (Info->Kind == TypeKind::CPointer || Info->Kind == TypeKind::Pointer)) {
            Out = AST->VoidPtrTy;
            return true;
        }
        if (Info && Info->Kind == TypeKind::CScalar) {
            auto It = Scalars.find(Info->CIdentity);
            if (It != Scalars.end()) {
                Out = (*AST).*(It->second);
                return true;
            }
        }
        if (!Info || !Info->isRecord()) {
            Err = "no C type for " + (Info ? Info->Name : std::string("an unknown type"));
            return false;
        }
        auto Cached = Records.find(Info);
        if (Cached != Records.end()) {
            Out = Cached->second;
            return true;
        }
        clang::TranslationUnitDecl *Unit = AST->getTranslationUnitDecl();
        auto *Record = clang::RecordDecl::Create(*AST, clang::TagTypeKind::Struct, Unit, {}, {},
                                                 &AST->Idents.get(Info->Name));
        Record->startDefinition();
        for (const auto &[Name, Field] : Info->Record.Fields) {
            clang::CanQualType FieldType;
            if (!map(Types->resolve(Field.TypeName), FieldType, Err)) return false;
            auto *Decl = clang::FieldDecl::Create(*AST, Record, {}, {}, &AST->Idents.get(Name),
                                                  FieldType, nullptr, nullptr, false,
                                                  clang::ICIS_NoInit);
            Decl->setAccess(clang::AS_public);
            Record->addDecl(Decl);
        }
        Record->completeDefinition();
        Unit->addDecl(Record);
        Out = AST->getCanonicalTagType(Record);
        uint64_t CSize = AST->getTypeSizeInChars(Out).getQuantity();
        uint64_t Size = Gen->GetModule()->getDataLayout().getTypeAllocSize(Info->LLVMType);
        if (CSize != Size) {
            Err = "record " + Info->Name + " has " + std::to_string(Size) + " bytes in cpsc but " +
                  std::to_string(CSize) + " in C";
            return false;
        }
        Records.emplace(Info, Out);
        return true;
    }
};

namespace {

bool translate(const ABIArgInfo &I, llvm::Type *ValueType, const std::string &What, CABIArg &Out,
               std::string &Err) {
    Out.ValueType = ValueType;
    const char *Unsupported = nullptr;
    switch (I.getKind()) {
    case ABIArgInfo::Direct:
    case ABIArgInfo::Extend:
        Out.K = I.isDirect() ? CABIArg::Kind::Direct : CABIArg::Kind::Extend;
        Out.CoerceType = I.getCoerceToType();
        Out.Offset = I.getDirectOffset();
        Out.DirectAlign = I.getDirectAlign();
        Out.Flatten = I.isDirect() && I.getCanBeFlattened() && llvm::isa<llvm::StructType>(Out.CoerceType);
        if (I.isExtend())
            Out.Extension = I.isSignExt() ? llvm::Attribute::SExt
                : I.isZeroExt() ? llvm::Attribute::ZExt : llvm::Attribute::NoExt;
        Out.InReg = I.getInReg();
        break;
    case ABIArgInfo::Indirect:
        Out.K = CABIArg::Kind::Indirect;
        Out.ByVal = I.getIndirectByVal();
        Out.Align = I.getIndirectAlign().getQuantity();
        Out.InReg = I.getInReg();
        break;
    case ABIArgInfo::Ignore:
        Out.K = CABIArg::Kind::Ignore;
        break;
    case ABIArgInfo::IndirectAliased: Unsupported = "IndirectAliased"; break;
    case ABIArgInfo::Expand: Unsupported = "Expand"; break;
    case ABIArgInfo::CoerceAndExpand: Unsupported = "CoerceAndExpand"; break;
    case ABIArgInfo::InAlloca: Unsupported = "InAlloca"; break;
    case ABIArgInfo::TargetSpecific: Unsupported = "TargetSpecific"; break;
    }
    if (!Unsupported && I.getPaddingType()) Unsupported = "with a padding type";
    if (Unsupported) {
        Err = "unsupported C ABI kind " + std::string(Unsupported) + " for " + What;
        return false;
    }
    return true;
}

} // namespace

ClangABI::ClangABI(llvm::LLVMContext &Context, const TypeSystem &Types)
    : Context(Context), Types(Types) {}

ClangABI::~ClangABI() = default;

bool ClangABI::arrange(const TypeInfo *Ret, const std::vector<const TypeInfo *> &Args, size_t Fixed,
                       bool Variadic, CABISignature &Sig, std::string &Err) {
    if (!S) {
        auto Fresh = std::make_unique<State>();
        Fresh->Types = &Types;
        if (!Fresh->init(Context, Err)) return false;
        S = std::move(Fresh);
    }
    clang::CanQualType RetType = S->AST->VoidTy;
    if (Ret && !S->map(Ret, RetType, Err)) return false;
    std::vector<clang::CanQualType> ArgTypes(Args.size());
    for (size_t I = 0; I < Args.size(); ++I)
        if (!S->map(Args[I], ArgTypes[I], Err)) return false;
    clang::CodeGen::RequiredArgs Required = Variadic
        ? clang::CodeGen::RequiredArgs(Fixed) : clang::CodeGen::RequiredArgs::All;
    const clang::CodeGen::CGFunctionInfo &Info = clang::CodeGen::arrangeFreeFunctionCall(
        S->Gen->CGM(), RetType, ArgTypes, clang::FunctionType::ExtInfo(), Required);

    Sig = CABISignature();
    Sig.Fixed = Fixed;
    Sig.Variadic = Variadic;
    Sig.CallingConv = Info.getEffectiveCallingConvention();
    if (!translate(Info.getReturnInfo(), Ret ? Ret->LLVMType : llvm::Type::getVoidTy(Context),
                   "the result", Sig.Ret, Err))
        return false;
    Sig.Args.resize(Args.size());
    for (size_t I = 0; I < Args.size(); ++I)
        if (!translate(Info.arguments()[I].info, Args[I]->LLVMType,
                       (I < Fixed ? "parameter " : "variadic argument ") + std::to_string(I + 1),
                       Sig.Args[I], Err))
            return false;
    return true;
}
