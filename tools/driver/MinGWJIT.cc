#include "MinGWJIT.h"

#if defined(_WIN32) && defined(__x86_64__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/x86_64.h"
#include "llvm/ExecutionEngine/Orc/ObjectFileInterface.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/WrapperFunctionUtils.h"
#include "llvm/Object/COFF.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/MemoryBuffer.h"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <vector>

namespace {

bool isCOFFUnwindSection(llvm::StringRef Name, llvm::StringRef Base) {
    return Name == Base || (Name.starts_with(Base) &&
                           Name.drop_front(Base.size()).starts_with("$"));
}

// MinGW emits .xdata$foo/.pdata$foo as SELECT_ANY without an external
// COMDAT symbol. JITLink waits for that symbol and never creates the section
// symbol used by relocations. Express their implicit association with .text$foo
// as ordinary associative COMDATs in a private copy of the object instead.
llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
normalizeMinGWObject(std::unique_ptr<llvm::MemoryBuffer> Buffer) {
    using namespace llvm;
    auto Object = object::ObjectFile::createObjectFile(Buffer->getMemBufferRef());
    if (!Object)
        return Object.takeError();
    auto *COFF = dyn_cast<object::COFFObjectFile>(Object->get());
    if (!COFF || COFF->getMachine() != COFF::IMAGE_FILE_MACHINE_AMD64)
        return std::move(Buffer);

    StringMap<int32_t> CodeSections;
    DenseSet<int32_t> ExternalSections;
    for (const auto &Section : COFF->sections()) {
        auto Name = Section.getName();
        if (!Name)
            return Name.takeError();
        if (Name->starts_with(".text$") &&
            (COFF->getCOFFSection(Section)->Characteristics &
             COFF::IMAGE_SCN_LNK_COMDAT))
            CodeSections[*Name] = COFF->getSectionID(Section);
    }
    for (const auto &Symbol : COFF->symbols()) {
        auto Sym = COFF->getCOFFSymbol(Symbol);
        if (Sym.isExternal() && Sym.getSectionNumber() > 0)
            ExternalSections.insert(Sym.getSectionNumber());
    }

    struct Association { size_t Offset; int32_t Parent; };
    SmallVector<Association, 16> Associations;
    SmallVector<size_t, 2> PureVirtualReferences;
    SmallVector<size_t, 2> PureVirtualFallbacks;
    for (const auto &Symbol : COFF->symbols()) {
        auto Sym = COFF->getCOFFSymbol(Symbol);
        if (auto *Weak = Sym.getWeakExternal()) {
            auto Name = Symbol.getName();
            if (!Name)
                return Name.takeError();
            if (*Name == "__cxa_pure_virtual" &&
                Weak->Characteristics == COFF::IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY) {
                auto Fallback = COFF->getSymbol(Weak->TagIndex);
                if (!Fallback)
                    return Fallback.takeError();
                if (Fallback->isAbsolute() && Fallback->getValue() == 0) {
                    size_t Offset = reinterpret_cast<const char *>(Sym.getRawPtr()) -
                                    Buffer->getBufferStart();
                    Offset += Sym.isBigObj()
                                  ? offsetof(object::coff_symbol32, StorageClass)
                                  : offsetof(object::coff_symbol16, StorageClass);
                    PureVirtualReferences.push_back(Offset);
                    size_t FallbackOffset =
                        reinterpret_cast<const char *>(Fallback->getRawPtr()) -
                        Buffer->getBufferStart();
                    FallbackOffset += Fallback->isBigObj()
                                          ? offsetof(object::coff_symbol32, StorageClass)
                                          : offsetof(object::coff_symbol16, StorageClass);
                    PureVirtualFallbacks.push_back(FallbackOffset);
                }
            }
        }
        auto *Def = Sym.getSectionDefinition();
        if (!Def || Def->Selection != COFF::IMAGE_COMDAT_SELECT_ANY ||
            Sym.getSectionNumber() <= 0 ||
            ExternalSections.contains(Sym.getSectionNumber()))
            continue;
        auto Section = COFF->getSection(Sym.getSectionNumber());
        if (!Section)
            return Section.takeError();
        if (!((*Section)->Characteristics & COFF::IMAGE_SCN_LNK_COMDAT))
            continue;
        auto Name = COFF->getSectionName(*Section);
        if (!Name)
            return Name.takeError();
        if (!Name->starts_with(".xdata$") && !Name->starts_with(".pdata$"))
            continue;
        std::string CodeName = (".text$" + Name->drop_front(7)).str();
        auto Parent = CodeSections.find(CodeName);
        if (Parent == CodeSections.end())
            return make_error<StringError>(
                "missing MinGW unwind code section '" + CodeName + "' in '" +
                    Buffer->getBufferIdentifier().str() + "'",
                inconvertibleErrorCode());
        Associations.push_back({static_cast<size_t>(
                                    reinterpret_cast<const char *>(Def) -
                                    Buffer->getBufferStart()),
                                Parent->second});
    }
    if (Associations.empty() && PureVirtualReferences.empty())
        return std::move(Buffer);

    auto Copy = WritableMemoryBuffer::getNewUninitMemBuffer(
        Buffer->getBufferSize(), Buffer->getBufferIdentifier());
    std::memcpy(Copy->getBufferStart(), Buffer->getBufferStart(),
                Buffer->getBufferSize());
    for (const auto &A : Associations) {
        auto *Def = reinterpret_cast<object::coff_aux_section_definition *>(
            Copy->getBufferStart() + A.Offset);
        Def->Selection = COFF::IMAGE_COMDAT_SELECT_ASSOCIATIVE;
        Def->NumberLowPart = A.Parent & 0xffff;
        Def->NumberHighPart = static_cast<uint32_t>(A.Parent) >> 16;
    }
    // JITLink cannot alias a weak external to an absolute-zero fallback.
    // The runner always supplies the host's __cxa_pure_virtual, so this known
    // optional ABI reference can be an ordinary external. Keep its auxiliary
    // entry count intact so the remaining symbol indices do not move.
    for (size_t Offset : PureVirtualReferences)
        Copy->getBufferStart()[Offset] = COFF::IMAGE_SYM_CLASS_EXTERNAL;
    // Only the now-unused weak auxiliary record references the fallback.
    // Keep it local so archive interfaces do not advertise a global definition.
    for (size_t Offset : PureVirtualFallbacks)
        Copy->getBufferStart()[Offset] = COFF::IMAGE_SYM_CLASS_STATIC;
    return std::unique_ptr<MemoryBuffer>(std::move(Copy));
}

} // namespace

// Parse LLVM COFF helpers and normalization before Windows IMAGE_* macros.
#include <windows.h>

namespace {

// Keep MinGW's GNU C++ ABI while registering COFF unwind tables with Windows.
// Full ORC COFFPlatform setup loads the VC runtime, which this build does not use.
static constexpr llvm::StringLiteral COFFSEHTableSection = "$__cpsc_seh";

static llvm::Error addCOFFJumpStubs(llvm::jitlink::LinkGraph &G) {
    using namespace llvm;
    using namespace llvm::jitlink;
    using namespace llvm::jitlink::x86_64;
    GOTTableManager GOT(G);
    PLTTableManager PLT(G, GOT);
    // Creating a stub may add sections/blocks. Visit only the original blocks.
    std::vector<Block *> Blocks(G.blocks().begin(), G.blocks().end());
    for (auto *B : Blocks) {
        bool Executable = (B->getSection().getMemProt() & orc::MemProt::Exec) !=
                          orc::MemProt::None;
        for (auto &E : B->edges()) {
            Symbol &Target = E.getTarget();
            if (!Target.isExternal())
                continue;
            bool Personality = false;
            if (isCOFFUnwindSection(B->getSection().getName(), ".xdata") &&
                Target.hasName()) {
                StringRef Name = *Target.getName();
                Personality = Name == "__gxx_personality_seh0" ||
                              Name == "__gxx_personality_sj0" ||
                              Name == "__gxx_personality_v0";
            }
            // COFF's private PCRel32 kind is lowered only in PreFixup.
            // Use its name rather than confusing it with x86_64::PCRel32.
            // Only E8/E9 direct branches with a plain symbol target need a
            // jump stub; RIP-relative data and indirect calls must stay intact.
            bool DirectBranch = false;
            if (Executable && !B->isZeroFill() && E.getAddend() == 0 &&
                StringRef(G.getEdgeKindName(E.getKind())) == "PCRel32") {
                auto Content = B->getContent();
                size_t Offset = E.getOffset();
                if (Offset > 0 && Offset <= Content.size() &&
                    Content.size() - Offset >= 4) {
                    unsigned char Opcode = Content[Offset - 1];
                    DirectBranch = Opcode == 0xe8 || Opcode == 0xe9;
                }
            }
            if (Personality || DirectBranch)
                E.setTarget(PLT.getEntryForTarget(G, Target));
        }
    }
    return Error::success();
}

static llvm::Error registerSEHTable(llvm::orc::ExecutorAddrRange TableRange,
                                     llvm::orc::ExecutorAddr ImageBase) {
    using llvm::make_error;
    using llvm::StringError;
    using llvm::inconvertibleErrorCode;
    size_t Size = TableRange.size();
    if (Size == 0 || ImageBase.isNull() || Size % sizeof(RUNTIME_FUNCTION))
        return make_error<StringError>("invalid embedded SEH function table",
                                       inconvertibleErrorCode());
    uint64_t Count64 = Size / sizeof(RUNTIME_FUNCTION);
    if (Count64 > static_cast<uint64_t>(std::numeric_limits<DWORD>::max()))
        return make_error<StringError>("invalid embedded SEH function table",
                                       inconvertibleErrorCode());
    auto *Table = TableRange.Start.toPtr<const RUNTIME_FUNCTION *>();
    DWORD PrevBegin = 0;
    for (uint64_t I = 0; I < Count64; ++I) {
        DWORD Begin = llvm::support::endian::read32le(
            reinterpret_cast<const char *>(Table + I) + 0);
        DWORD End = llvm::support::endian::read32le(
            reinterpret_cast<const char *>(Table + I) + 4);
        if (!(Begin < End))
            return make_error<StringError>("invalid embedded SEH function table",
                                           inconvertibleErrorCode());
        if (I > 0 && !(PrevBegin < Begin))
            return make_error<StringError>("invalid embedded SEH function table",
                                           inconvertibleErrorCode());
        PrevBegin = Begin;
    }
    auto *MutableTable = const_cast<PRUNTIME_FUNCTION>(Table);
    DWORD Count = static_cast<DWORD>(Count64);
    DWORD64 Base = static_cast<DWORD64>(ImageBase.getValue());
    if (!RtlAddFunctionTable(MutableTable, Count, Base))
        return make_error<StringError>("cannot register embedded SEH function table",
                                       inconvertibleErrorCode());
    return llvm::Error::success();
}

static llvm::Error deregisterSEHTable(llvm::orc::ExecutorAddrRange TableRange,
                                       llvm::orc::ExecutorAddr) {
    auto *Table = TableRange.Start.toPtr<PRUNTIME_FUNCTION>();
    if (TableRange.size() == 0 || !Table)
        return llvm::Error::success();
    RtlDeleteFunctionTable(Table);
    return llvm::Error::success();
}

extern "C" llvm::orc::shared::CWrapperFunctionBuffer
cps_register_seh_table_alloc_action(const char *ArgData, size_t ArgSize) {
    using namespace llvm::orc::shared;
    return WrapperFunction<SPSError(SPSExecutorAddrRange, SPSExecutorAddr)>::handle(
               ArgData, ArgSize, registerSEHTable)
        .release();
}

extern "C" llvm::orc::shared::CWrapperFunctionBuffer
cps_deregister_seh_table_alloc_action(const char *ArgData, size_t ArgSize) {
    using namespace llvm::orc::shared;
    return WrapperFunction<SPSError(SPSExecutorAddrRange, SPSExecutorAddr)>::handle(
               ArgData, ArgSize, deregisterSEHTable)
        .release();
}

class MinGWSEHRegistrationPlugin final
    : public llvm::orc::LinkGraphLinkingLayer::Plugin {
public:
    void modifyPassConfig(llvm::orc::MaterializationResponsibility &,
                           llvm::jitlink::LinkGraph &G,
                           llvm::jitlink::PassConfiguration &Config) override {
        using namespace llvm;
        using namespace llvm::orc;
        using namespace llvm::jitlink;
        const Triple &TT = G.getTargetTriple();
        if (!TT.isOSBinFormatCOFF() || TT.getArch() != Triple::x86_64)
            return;

        // LLVM's SEHFrameKeepAlivePass retains .pdata; associative COMDATs
        // retain the normalized suffixed sections. Nearby jump stubs let
        // 32-bit personality RVAs and direct branches reach the host runtime.
        Config.PostPrunePasses.push_back(addCOFFJumpStubs);

        // Tables sharing an image base have overlapping lookup ranges in the
        // Windows unwinder. Reserve one table for all retained .pdata sections
        // in this graph, then fill and sort its entries after relocations.
        Config.PostPrunePasses.push_back([](LinkGraph &G) -> Error {
            size_t Size = 0;
            for (auto &Sec : G.sections()) {
                if (!isCOFFUnwindSection(Sec.getName(), ".pdata"))
                    continue;
                for (auto *B : Sec.blocks()) {
                    if (B->isZeroFill() || B->getSize() % sizeof(RUNTIME_FUNCTION))
                        return make_error<JITLinkError>(
                            "invalid embedded SEH function table");
                    Size += B->getSize();
                }
            }
            if (Size) {
                auto &Sec = G.createSection(COFFSEHTableSection, MemProt::Read);
                G.createMutableContentBlock(Sec, Size, ExecutorAddr(),
                                            alignof(RUNTIME_FUNCTION), 0);
            }
            return Error::success();
        });

        // COFF unwind tables use 32-bit image-relative addresses, so the
        // image base must sit at or below the lowest JIT address. Alias the
        // external __ImageBase to the lowest defined block once addresses
        // are final; a trailing zero block would push RVAs negative.
        Config.PostAllocationPasses.push_back([](LinkGraph &G) -> Error {
            // A graph-local alias never collides at materialization time,
            // no matter how many graphs define the same base name.
            if (G.findDefinedSymbolByName(G.intern("__ImageBase")))
                return Error::success();
            auto *ImageBaseExt =
                G.findExternalSymbolByName(G.intern("__ImageBase"));
            if (!ImageBaseExt)
                return Error::success();
            jitlink::Block *Low = nullptr;
            for (auto &Sec : G.sections()) {
                for (auto *B : Sec.blocks()) {
                    if (!Low || B->getAddress() < Low->getAddress())
                        Low = B;
                }
            }
            if (!Low)
                return make_error<JITLinkError>(
                    "missing embedded __ImageBase content");
            G.makeDefined(*ImageBaseExt, *Low, 0, 8, Linkage::Strong,
                          Scope::Local, true);
            return Error::success();
        });

        // Copy fixed-up entries into one sorted, allocation-owned table.
        // Register it once per graph, with deregistration on link failure or
        // teardown before the table and its code are released.
        Config.PostFixupPasses.push_back(
            [](LinkGraph &G) -> Error {
                using namespace llvm::orc::shared;
                auto *TableSec = G.findSectionByName(COFFSEHTableSection);
                if (!TableSec)
                    return Error::success();
                auto *Table = *TableSec->blocks().begin();
                std::vector<RUNTIME_FUNCTION> Entries;
                Entries.reserve(Table->getSize() / sizeof(RUNTIME_FUNCTION));
                for (auto &Sec : G.sections()) {
                    if (!isCOFFUnwindSection(Sec.getName(), ".pdata"))
                        continue;
                    for (auto *B : Sec.blocks()) {
                        size_t Start = Entries.size();
                        Entries.resize(Start + B->getSize() / sizeof(RUNTIME_FUNCTION));
                        if (B->getSize())
                            std::memcpy(Entries.data() + Start, B->getContent().data(),
                                        B->getSize());
                    }
                }
                std::sort(Entries.begin(), Entries.end(),
                          [](const RUNTIME_FUNCTION &L, const RUNTIME_FUNCTION &R) {
                              return L.BeginAddress < R.BeginAddress;
                          });
                std::memcpy(Table->getAlreadyMutableContent().data(), Entries.data(),
                            Table->getSize());
                ExecutorAddrRange TableRange(Table->getAddress(), Table->getSize());
                auto *ImageBaseSym =
                    G.findDefinedSymbolByName(G.intern("__ImageBase"));
                if (!ImageBaseSym)
                    return make_error<JITLinkError>(
                        "missing embedded __ImageBase for SEH registration");
                ExecutorAddr ImageBase = ImageBaseSym->getAddress();
                if (ImageBase.isNull())
                    return make_error<JITLinkError>("invalid embedded SEH image base");
                auto RegisterAction =
                    WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddrRange,
                                                           SPSExecutorAddr>>(
                        ExecutorAddr::fromPtr(&cps_register_seh_table_alloc_action),
                        TableRange, ImageBase);
                if (!RegisterAction)
                    return RegisterAction.takeError();
                auto DeregisterAction =
                    WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddrRange,
                                                           SPSExecutorAddr>>(
                        ExecutorAddr::fromPtr(&cps_deregister_seh_table_alloc_action),
                        TableRange, ImageBase);
                if (!DeregisterAction)
                    return DeregisterAction.takeError();
                G.allocActions().push_back(
                    {std::move(*RegisterAction), std::move(*DeregisterAction)});
                return Error::success();
            });
    }

    llvm::Error notifyFailed(llvm::orc::MaterializationResponsibility &) override {
        return llvm::Error::success();
    }

    llvm::Error notifyRemovingResources(llvm::orc::JITDylib &,
                                        llvm::orc::ResourceKey) override {
        return llvm::Error::success();
    }

    void notifyTransferringResources(llvm::orc::JITDylib &,
                                     llvm::orc::ResourceKey,
                                     llvm::orc::ResourceKey) override {}
};

// Both user objects and lazily extracted runtime archive members pass through
// this layer, before JITLink builds their graphs or runs plugin passes.
class MinGWObjectLinkingLayer final : public llvm::orc::ObjectLinkingLayer {
public:
    using ObjectLinkingLayer::ObjectLinkingLayer;

    void emit(std::unique_ptr<llvm::orc::MaterializationResponsibility> R,
              std::unique_ptr<llvm::MemoryBuffer> O) override {
        auto Normalized = normalizeMinGWObject(std::move(O));
        if (!Normalized) {
            getExecutionSession().reportError(Normalized.takeError());
            R->failMaterialization();
            return;
        }
        ObjectLinkingLayer::emit(std::move(R), std::move(*Normalized));
    }
};

} // namespace

std::unique_ptr<llvm::orc::ObjectLinkingLayer>
createMinGWObjectLinkingLayer(llvm::orc::ExecutionSession &ES,
                             llvm::jitlink::JITLinkMemoryManager &MM) {
    auto Layer = std::make_unique<MinGWObjectLinkingLayer>(ES, MM);
    Layer->setOverrideObjectFlagsWithResponsibilityFlags(true);
    Layer->setAutoClaimResponsibilityForObjectSymbols(true);
    Layer->addPlugin(std::make_shared<MinGWSEHRegistrationPlugin>());
    return Layer;
}

llvm::Expected<llvm::orc::MaterializationUnit::Interface>
getMinGWObjectFileInterface(llvm::orc::ExecutionSession &ES,
                           llvm::MemoryBufferRef Ref) {
    // Archive interfaces must describe the same normalized object emit links.
    auto Normalized = normalizeMinGWObject(
        llvm::MemoryBuffer::getMemBuffer(Ref, false));
    if (!Normalized)
        return Normalized.takeError();
    return llvm::orc::getObjectFileInterface(ES, (*Normalized)->getMemBufferRef());
}
#endif
