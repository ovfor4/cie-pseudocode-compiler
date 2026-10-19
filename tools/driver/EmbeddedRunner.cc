#include "EmbeddedRunner.h"
#include "NativeCompiler.h"
#include "llvm/Object/ObjectFile.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectFileInterface.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cxxabi.h>
#include <typeinfo>
#include <unwind.h>
#if defined(_WIN32) && defined(__x86_64__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <limits>
#include <mutex>
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/x86_64.h"
#include "llvm/ExecutionEngine/Orc/LinkGraphLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/WrapperFunctionUtils.h"
#include "llvm/Support/Endian.h"
#include "MinGWCOFF.h"
// Windows IMAGE_* macros conflict with LLVM COFF enums and inline helpers.
// Parse all LLVM COFF declarations and helpers before introducing those macros.
#include <windows.h>
#endif
#ifdef __MINGW32__
#include <io.h>
#endif

namespace {

#if defined(_WIN32) && defined(__x86_64__)
struct JITBlockAddressRange {
    uint64_t Begin, End;
    std::string Section;
};

struct JITSymbolAddress {
    uint64_t Address;
    std::string Name;
};

struct JITGraphAddresses {
    std::string Name;
    std::vector<JITBlockAddressRange> Blocks;
    std::vector<JITSymbolAddress> Symbols;
};

static std::mutex JITAddressMutex;
static std::vector<JITGraphAddresses> JITAddresses;
static volatile LONG JITCrashReported = 0;

static llvm::Error recordJITAddresses(llvm::jitlink::LinkGraph &G) {
    JITGraphAddresses Graph;
    Graph.Name = G.getName();
    for (auto *B : G.blocks()) {
        uint64_t Begin = B->getAddress().getValue();
        Graph.Blocks.push_back({Begin, Begin + B->getSize(),
                                B->getSection().getName().str()});
    }
    for (auto *S : G.defined_symbols()) {
        if (S->hasName())
            Graph.Symbols.push_back({S->getAddress().getValue(),
                                     (*S->getName()).str()});
    }
    std::lock_guard<std::mutex> Lock(JITAddressMutex);
    JITAddresses.push_back(std::move(Graph));
    return llvm::Error::success();
}

static void clearJITAddresses() {
    std::lock_guard<std::mutex> Lock(JITAddressMutex);
    JITAddresses.clear();
}

static void printWindowsJITFrame(unsigned Frame, DWORD64 PC, DWORD64 SP,
                                 bool HasUnwind) {
    std::fprintf(stderr, "  #%02u pc=0x%016llx sp=0x%016llx [unwind=%s] ",
                 Frame, static_cast<unsigned long long>(PC),
                 static_cast<unsigned long long>(SP), HasUnwind ? "yes" : "no");
    // A fault during lazy linking must not wait on a lock held by that thread.
    std::unique_lock<std::mutex> Lock(JITAddressMutex, std::try_to_lock);
    if (Lock.owns_lock()) {
        for (const auto &Graph : JITAddresses) {
            for (const auto &Block : Graph.Blocks) {
                if (PC < Block.Begin || PC >= Block.End)
                    continue;
                const JITSymbolAddress *Nearest = nullptr;
                for (const auto &Symbol : Graph.Symbols) {
                    // Anonymous stubs must not inherit an unrelated symbol
                    // from a different block in the same graph.
                    if (Symbol.Address >= Block.Begin && Symbol.Address <= PC &&
                        (!Nearest || Symbol.Address > Nearest->Address))
                        Nearest = &Symbol;
                }
                std::fprintf(stderr, "JIT %s!%s+0x%llx\n", Graph.Name.c_str(),
                             Nearest ? Nearest->Name.c_str() : Block.Section.c_str(),
                             static_cast<unsigned long long>(
                                 PC - (Nearest ? Nearest->Address : Block.Begin)));
                return;
            }
        }
        Lock.unlock();
    }

    HMODULE Module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(static_cast<uintptr_t>(PC)),
                          &Module)) {
        wchar_t Path[MAX_PATH];
        DWORD Length = GetModuleFileNameW(Module, Path, MAX_PATH);
        if (Length && Length < MAX_PATH) {
            const wchar_t *Name = std::wcsrchr(Path, L'\\');
            Name = Name ? Name + 1 : Path;
            char UTF8Name[3 * MAX_PATH];
            if (WideCharToMultiByte(CP_UTF8, 0, Name, -1, UTF8Name,
                                    sizeof(UTF8Name), nullptr, nullptr)) {
                std::fprintf(stderr, "%s+0x%llx\n", UTF8Name,
                             static_cast<unsigned long long>(
                                 PC - reinterpret_cast<uintptr_t>(Module)));
                return;
            }
        }
    }
    std::fprintf(stderr, "<unknown>\n");
}

// Keep the context copy and module-name buffers off the stack-overflow path.
LLVM_ATTRIBUTE_NOINLINE static void printWindowsJITStack(const CONTEXT &FaultContext) {
    CONTEXT Context = FaultContext;
    for (unsigned Frame = 0; Frame < 32 && Context.Rip; ++Frame) {
        DWORD64 PC = Context.Rip;
        DWORD64 SP = Context.Rsp;
        DWORD64 ImageBase = 0;
        auto *Entry = RtlLookupFunctionEntry(PC, &ImageBase, nullptr);
        printWindowsJITFrame(Frame, PC, SP, Entry != nullptr);
        // Preserve each frame even if corrupt unwind metadata faults below.
        std::fflush(stderr);
        if (Entry) {
            PVOID HandlerData = nullptr;
            DWORD64 EstablisherFrame = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, ImageBase, PC, Entry, &Context,
                             &HandlerData, &EstablisherFrame, nullptr);
        } else {
            // Leaf functions have no entry. Read safely in case RSP is corrupt.
            DWORD64 ReturnPC = 0;
            SIZE_T Read = 0;
            if (!ReadProcessMemory(GetCurrentProcess(),
                                   reinterpret_cast<const void *>(SP),
                                   &ReturnPC, sizeof(ReturnPC), &Read) ||
                Read != sizeof(ReturnPC))
                break;
            Context.Rip = ReturnPC;
            Context.Rsp += sizeof(ReturnPC);
        }
        if (Context.Rsp <= SP)
            break;
    }
}

static LONG WINAPI reportWindowsJITCrash(EXCEPTION_POINTERS *Info) {
    const auto &Exception = *Info->ExceptionRecord;
    switch (Exception.ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_STACK_CHECK:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_INVALID_DISPOSITION:
    case EXCEPTION_STACK_OVERFLOW:
        break;
    default:
        // In particular, GCC C++ exceptions (0x20474343) are normal control flow.
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (InterlockedCompareExchange(&JITCrashReported, 1, 0))
        return EXCEPTION_CONTINUE_SEARCH;

    std::fprintf(stderr, "cpsc: Windows JIT crash: exception=0x%08lx address=%p\n",
                 static_cast<unsigned long>(Exception.ExceptionCode),
                 Exception.ExceptionAddress);
    std::fflush(stderr);
    // Preserve buffered program output even if damaged unwind data faults again.
    std::fflush(stdout);
    if (Exception.ExceptionCode != EXCEPTION_STACK_OVERFLOW) {
        if ((Exception.ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
             Exception.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) &&
            Exception.NumberParameters >= 2) {
            auto Access = Exception.ExceptionInformation[0];
            std::fprintf(stderr, "  access=%s (%llu) target=0x%016llx\n",
                         Access == 0 ? "read" : Access == 1 ? "write" :
                         Access == 8 ? "execute" : "unknown",
                         static_cast<unsigned long long>(Access),
                         static_cast<unsigned long long>(Exception.ExceptionInformation[1]));
            std::fflush(stderr);
        }
        printWindowsJITStack(*Info->ContextRecord);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

// In-process COFF/SEH support for the MinGW x86_64 JIT.
//
// The full upstream COFFPlatform starts the VC runtime loading machinery,
// which does not apply to the current MinGW build. This plugin provides only
// the linking and registration pieces needed for in-process execution:
// a per-graph __ImageBase shared by relocations and SEH registration,
// JITLink stubs for external direct branches and GNU personalities in .xdata,
// and RtlAddFunctionTable/RtlDeleteFunctionTable via JITLink allocation
// actions. Exception propagation still uses the existing C++ ABI, LLVM
// exception instructions and platform unwinder.

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
    if (Size == 0 || ImageBase.isNull())
        return make_error<StringError>("invalid embedded SEH function table",
                                       inconvertibleErrorCode());
    if (Size % sizeof(RUNTIME_FUNCTION))
        return make_error<StringError>("invalid embedded SEH function table",
                                       inconvertibleErrorCode());
    uint64_t Count64 = Size / sizeof(RUNTIME_FUNCTION);
    if (Count64 == 0 || Count64 > static_cast<uint64_t>(std::numeric_limits<DWORD>::max()))
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
                                       llvm::orc::ExecutorAddr ImageBase) {
    (void)ImageBase;
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
    void modifyPassConfig(llvm::orc::MaterializationResponsibility &MR,
                           llvm::jitlink::LinkGraph &G,
                           llvm::jitlink::PassConfiguration &Config) override {
        using namespace llvm;
        using namespace llvm::orc;
        using namespace llvm::jitlink;
        const Triple &TT = G.getTargetTriple();
        if (!TT.isOSBinFormatCOFF() || TT.getArch() != Triple::x86_64)
            return;

        // SEH table liveness relies on the COFF backend's default
        // SEHFrameKeepAlivePass(".pdata"), which keeps .pdata alive from its
        // referenced function/unwind blocks. Normalized .pdata$/.xdata$
        // sections are kept alive by their associative COMDAT code edges.
        // COFF unwind tables address the personality by 32-bit RVA, so it
        // must live near JIT code. Retarget .xdata personality references
        // to this graph's anonymous pointer jump stubs; the stub pointer
        // itself uses a 64-bit relocation to reach the far host runtime.
        // External direct calls/jumps from user objects also need nearby
        // stubs: their PCRel32 displacement cannot reach a distant CRT DLL.
        // The same target reuses the same stub, including personality uses.
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
        Config.PostFixupPasses.push_back(recordJITAddresses);
        (void)MR;
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
#endif

// Embedded JITLink object layer. Created before LLJIT takes ownership so
// runEmbedded keeps only the execution flow. COFF flags stay conditional on
// the object format; other formats use LLVM defaults.
static std::unique_ptr<llvm::orc::ObjectLinkingLayer>
createEmbeddedObjectLayer(llvm::orc::ExecutionSession &ES,
                          llvm::jitlink::JITLinkMemoryManager &MM,
                          const llvm::Triple &TT) {
    std::unique_ptr<llvm::orc::ObjectLinkingLayer> Layer;
#if defined(_WIN32) && defined(__x86_64__)
    if (TT.isOSBinFormatCOFF() && TT.getArch() == llvm::Triple::x86_64)
        Layer = std::make_unique<MinGWObjectLinkingLayer>(ES, MM);
    else
#endif
        Layer = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, MM);
    if (TT.isOSBinFormatCOFF()) {
        Layer->setOverrideObjectFlagsWithResponsibilityFlags(true);
        Layer->setAutoClaimResponsibilityForObjectSymbols(true);
    }
#if defined(_WIN32) && defined(__x86_64__)
    if (TT.isOSBinFormatCOFF() && TT.getArch() == llvm::Triple::x86_64)
        Layer->addPlugin(std::make_shared<MinGWSEHRegistrationPlugin>());
#endif
    return Layer;
}

} // namespace

// Static runtime symbols are not available through dlsym. Provide their
// actual addresses so the JIT and its unwind tables use the host's runtime.
extern "C" int
#if defined(__SEH__)
__gxx_personality_seh0
#elif defined(__USING_SJLJ_EXCEPTIONS__)
__gxx_personality_sj0
#else
__gxx_personality_v0
#endif
(int, _Unwind_Action, _Unwind_Exception_Class, _Unwind_Exception *, _Unwind_Context *);

#ifdef _WIN32
extern "C" void __cps_jit_noop__main() {}
#endif

// MinGW x86_64 emits ___chkstk_ms stack probes for large frames. Bind the
// toolchain-provided symbol directly (as LLVM lli does); it is supplied by
// compiler-rt or libgcc. The address is only published to the JIT, never
// called by the compiler itself.
#if defined(__MINGW32__) && defined(__x86_64__)
extern "C" void ___chkstk_ms(void);
#endif
#ifdef __MINGW32__
// Use the initialized guard and failure handler from the statically linked
// host runtime rather than loading a second guard and its CRT initializer.
extern "C" uintptr_t __stack_chk_guard;
extern "C" void __stack_chk_fail(void);
#endif

static llvm::Error defineRuntimeSymbols(llvm::orc::LLJIT &J) {
    llvm::orc::SymbolMap Symbols;
    auto Intern = [&](llvm::StringRef Name) { return J.mangleAndIntern(Name); };
    using llvm::orc::ExecutorSymbolDef;
    Symbols[Intern("__cxa_allocate_exception")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_allocate_exception);
    Symbols[Intern("__cxa_current_exception_type")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_current_exception_type);
    Symbols[Intern("__cxa_demangle")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_demangle);
    Symbols[Intern("__cxa_free_exception")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_free_exception);
    Symbols[Intern("__cxa_rethrow")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_rethrow);
    Symbols[Intern("__cxa_get_exception_ptr")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_get_exception_ptr);
    Symbols[Intern("__cxa_guard_acquire")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_guard_acquire);
    Symbols[Intern("__cxa_guard_release")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_guard_release);
    Symbols[Intern("__cxa_guard_abort")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_guard_abort);
    Symbols[Intern("__cxa_throw")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_throw);
    Symbols[Intern("__cxa_pure_virtual")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_pure_virtual);
    Symbols[Intern("__cxa_begin_catch")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_begin_catch);
    Symbols[Intern("__cxa_end_catch")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_end_catch);
    Symbols[Intern("_ZTIi")] = ExecutorSymbolDef::fromPtr(
        const_cast<std::type_info *>(&typeid(int)));
#if defined(__SEH__)
    Symbols[Intern("__gxx_personality_seh0")] =
        ExecutorSymbolDef::fromPtr(&__gxx_personality_seh0);
#elif defined(__USING_SJLJ_EXCEPTIONS__)
    Symbols[Intern("__gxx_personality_sj0")] =
        ExecutorSymbolDef::fromPtr(&__gxx_personality_sj0);
    Symbols[Intern("_Unwind_SjLj_Resume")] =
        ExecutorSymbolDef::fromPtr(&_Unwind_SjLj_Resume);
#else
    Symbols[Intern("__gxx_personality_v0")] =
        ExecutorSymbolDef::fromPtr(&__gxx_personality_v0);
#endif
    Symbols[Intern("_Unwind_Resume")] =
        ExecutorSymbolDef::fromPtr(&_Unwind_Resume);
#ifdef __MINGW32__
    // Use the same CRT aliases as native IR linking, including its stdio formatting.
    Symbols[Intern("snprintf")] =
        ExecutorSymbolDef::fromPtr(&__ms_snprintf);
    Symbols[Intern("strtoll")] =
        ExecutorSymbolDef::fromPtr(&std::strtoll);
    // msvcrt.dll exports _write; MinGW archive members call its POSIX alias.
    Symbols[Intern("write")] = ExecutorSymbolDef::fromPtr(&_write);
    Symbols[Intern("__stack_chk_guard")] =
        ExecutorSymbolDef::fromPtr(&__stack_chk_guard);
    Symbols[Intern("__stack_chk_fail")] =
        ExecutorSymbolDef::fromPtr(&__stack_chk_fail);
#endif
#ifdef _WIN32
    // Generated diagnostics write to stderr through __acrt_iob_func(2). The
    // msvcrt DLL only exports __iob_func, so bind the host's shim directly.
    Symbols[Intern("__acrt_iob_func")] =
        ExecutorSymbolDef::fromPtr(&__acrt_iob_func);
#endif
#ifdef _WIN32
    // The JIT-generated main references __main on MinGW; provide a no-op so
    // the host CRT is not initialized a second time.
    Symbols[Intern("__main")] =
        ExecutorSymbolDef::fromPtr(&__cps_jit_noop__main);
#endif
#if defined(__MINGW32__) && defined(__x86_64__)
    Symbols[Intern("___chkstk_ms")] =
        ExecutorSymbolDef::fromPtr(&___chkstk_ms);
#endif
    return J.getMainJITDylib().define(
        llvm::orc::absoluteSymbols(std::move(Symbols)));
}

static llvm::Error loadCxxRuntime(llvm::orc::LLJIT &J) {
    std::vector<std::unique_ptr<llvm::MemoryBuffer>> Archives;
    if (!unpackCxxRuntime(Archives))
        return llvm::make_error<llvm::StringError>("cannot unpack the C++ runtime", llvm::inconvertibleErrorCode());
    for (auto &Archive : Archives) {
        llvm::orc::StaticLibraryDefinitionGenerator::GetObjectFileInterface GetInterface;
#if defined(_WIN32) && defined(__x86_64__)
        // Match the normalized object that MinGWObjectLinkingLayer::emit links.
        GetInterface = [](llvm::orc::ExecutionSession &ES, llvm::MemoryBufferRef Ref)
            -> llvm::Expected<llvm::orc::MaterializationUnit::Interface> {
            auto Normalized = normalizeMinGWObject(
                llvm::MemoryBuffer::getMemBuffer(Ref, false));
            if (!Normalized)
                return Normalized.takeError();
            return llvm::orc::getObjectFileInterface(ES, (*Normalized)->getMemBufferRef());
        };
#endif
        auto Generator = llvm::orc::StaticLibraryDefinitionGenerator::Create(
            J.getObjLinkingLayer(), std::move(Archive),
            [](llvm::object::Archive &, llvm::MemoryBufferRef Buffer, size_t) -> llvm::Expected<bool> {
                auto Object = llvm::object::ObjectFile::createObjectFile(Buffer);
                if (!Object) { llvm::consumeError(Object.takeError()); return false; }
                // Keep one exception ABI and unwinder. ABI archive members must
                // not replace the host addresses registered above.
                for (const auto &Symbol : (*Object)->symbols()) {
                    auto Flags = Symbol.getFlags();
                    if (!Flags) return Flags.takeError();
                    if (*Flags & llvm::object::SymbolRef::SF_Undefined) continue;
                    auto Name = Symbol.getName();
                    if (!Name) return Name.takeError();
                    llvm::StringRef N = *Name;
                    if ((*Object)->isMachO() && N.starts_with("_")) N = N.drop_front();
                    if (N.starts_with("__cxa_") || N.starts_with("__gxx_personality_") ||
                        N.starts_with("_Unwind_") || N == "_ZTIi") return false;
                }
                return true;
            }, std::move(GetInterface));
        if (!Generator) return Generator.takeError();
        J.getMainJITDylib().addGenerator(std::move(*Generator));
    }
    return llvm::Error::success();
}

int runEmbedded(const cps::CodeGen &CG, const std::vector<std::string> &Objects) {
#if defined(_WIN32) && defined(__x86_64__)
    // Destroy the address copies after LLJIT has stopped all linking work.
    llvm::scope_exit ClearJITAddresses(clearJITAddresses);
#endif
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) {
        llvm::errs() << "error: cannot initialize the embedded LLVM target\n";
        return 1;
    }

    auto JTMBOrErr = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!JTMBOrErr) {
        llvm::errs() << "error: cannot detect the embedded JIT target: "
                     << llvm::toString(JTMBOrErr.takeError()) << "\n";
        return 1;
    }
    JTMBOrErr->setCodeModel(llvm::CodeModel::Large);
    JTMBOrErr->setRelocationModel(llvm::Reloc::PIC_);

    llvm::orc::LLJITBuilder Builder;
    const llvm::Triple LayerTriple = JTMBOrErr->getTargetTriple();
    Builder.setJITTargetMachineBuilder(std::move(*JTMBOrErr));
    // Use JITLink explicitly so Windows COFF does not fall back to RuntimeDyld.
    // The factory configures COFF flags and the SEH plugin before LLJIT owns
    // the layer.
    Builder.setObjectLinkingLayerCreator(
        [LayerTriple](llvm::orc::ExecutionSession &ES,
                      llvm::jitlink::JITLinkMemoryManager &MM)
            -> llvm::Expected<std::unique_ptr<llvm::orc::ObjectLayer>> {
            return std::unique_ptr<llvm::orc::ObjectLayer>(
                createEmbeddedObjectLayer(ES, MM, LayerTriple));
        });

    auto JITOrErr = Builder.create();
    if (!JITOrErr) {
        llvm::errs() << "error: cannot create the embedded LLVM engine: "
                     << llvm::toString(JITOrErr.takeError()) << "\n";
        return 1;
    }
    std::unique_ptr<llvm::orc::LLJIT> J = std::move(*JITOrErr);

    if (auto Err = defineRuntimeSymbols(*J)) {
        llvm::errs() << "error: cannot define embedded runtime symbols: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    if (auto Err = loadCxxRuntime(*J)) {
        llvm::errs() << "error: cannot load embedded C++ runtime: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
#ifdef _WIN32
    // Process-wide lookup may find C functions in ucrtbase.dll, loaded by
    // system DLLs, whose stdio buffers differ from the host's msvcrt. Prefer
    // the host CRT so EXTERN calls such as puts share stdout with OUTPUT.
    auto CRT = llvm::orc::DynamicLibrarySearchGenerator::Load(
        "msvcrt.dll", J->getDataLayout().getGlobalPrefix());
    if (!CRT) {
        llvm::errs() << "error: cannot load the embedded C runtime: "
                     << llvm::toString(CRT.takeError()) << "\n";
        return 1;
    }
    J->getMainJITDylib().addGenerator(std::move(*CRT));
#endif

    // Keep CodeGen's module and context alive; the JIT owns an independent copy.
    llvm::orc::ThreadSafeContext TSCtx(std::make_unique<llvm::LLVMContext>());
    llvm::orc::ThreadSafeModule TSM =
        llvm::orc::cloneExternalModuleToContext(CG.getModule(), TSCtx);
    if (!TSM) {
        llvm::errs() << "error: cannot clone the embedded module for JIT execution\n";
        return 1;
    }
    TSM.withModuleDo([&](llvm::Module &M) {
        M.setTargetTriple(J->getTargetTriple());
        M.setDataLayout(J->getDataLayout());
    });

    if (auto Err = J->addIRModule(J->getMainJITDylib(), std::move(TSM))) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    for (const auto &Path : Objects) {
        auto Buffer = llvm::MemoryBuffer::getFile(Path);
        if (!Buffer) {
            llvm::errs() << "error: cannot read object file '" << Path << "': "
                         << Buffer.getError().message() << "\n";
            return 1;
        }
        if (auto Err = J->addObjectFile(std::move(*Buffer))) {
            llvm::errs() << "error: cannot load object file '" << Path << "': "
                         << llvm::toString(std::move(Err)) << "\n";
            return 1;
        }
    }
    if (auto Err = J->initialize(J->getMainJITDylib())) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }

    auto MainSymOrErr = J->lookup("main");
    if (!MainSymOrErr) {
        llvm::errs() << "error: embedded LLVM could not resolve main: "
                     << llvm::toString(MainSymOrErr.takeError()) << "\n";
        return 1;
    }
    auto Main = MainSymOrErr->toPtr<int (*)()>();
#if defined(_WIN32) && defined(__x86_64__)
    InterlockedExchange(&JITCrashReported, 0);
    PVOID CrashHandler = AddVectoredExceptionHandler(1, reportWindowsJITCrash);
    if (!CrashHandler) {
        llvm::errs() << "error: cannot install the Windows JIT crash reporter: "
                     << GetLastError() << "\n";
        return 1;
    }
#endif
    int ExitCode = Main();
#if defined(_WIN32) && defined(__x86_64__)
    RemoveVectoredExceptionHandler(CrashHandler);
#endif

    // Run platform deinitializers while JIT code is still mapped; frame
    // deregistration runs when the JIT releases its memory below.
    if (auto Err = J->deinitialize(J->getMainJITDylib())) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    return ExitCode;
}
