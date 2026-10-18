#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
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
#include <windows.h>
#include <limits>
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/x86_64.h"
#include "llvm/ExecutionEngine/Orc/LinkGraphLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/Shared/WrapperFunctionUtils.h"
#include "llvm/Support/Endian.h"
#endif

namespace {

#if defined(_WIN32) && defined(__x86_64__)
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
            if (B->getSection().getName() == ".xdata" && Target.hasName()) {
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
        // referenced function/unwind blocks. No custom PrePrune pass here.
        // COFF unwind tables address the personality by 32-bit RVA, so it
        // must live near JIT code. Retarget .xdata personality references
        // to this graph's anonymous pointer jump stubs; the stub pointer
        // itself uses a 64-bit relocation to reach the far host runtime.
        // External direct calls/jumps from user objects also need nearby
        // stubs: their PCRel32 displacement cannot reach a distant CRT DLL.
        // The same target reuses the same stub, including personality uses.
        Config.PostPrunePasses.push_back(addCOFFJumpStubs);

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

        // After relocations are applied, register the function table with
        // the system unwinder via allocation actions. Table contents are
        // validated in the registration callback; here only locate the
        // non-empty .pdata and the graph-local image base.
        Config.PostFixupPasses.push_back(
            [](LinkGraph &G) -> Error {
                using namespace llvm::orc::shared;
                auto *PDataSec = G.findSectionByName(".pdata");
                if (!PDataSec)
                    return Error::success();
                jitlink::SectionRange Range(*PDataSec);
                if (Range.empty())
                    return Error::success();
                auto TableRange = Range.getRange();
                // Resolve the graph-local image base established above.
                auto *ImageBaseSym =
                    G.findDefinedSymbolByName(G.intern("__ImageBase"));
                if (!ImageBaseSym)
                    return make_error<JITLinkError>(
                        "missing embedded __ImageBase for SEH registration");
                ExecutorAddr ImageBase = ImageBaseSym->getAddress();
                if (ImageBase.isNull())
                    return make_error<JITLinkError>(
                        "invalid embedded SEH image base");
                // Registration failure must prevent entry to JIT code; the
                // deregistration action also covers link-failure cleanup and
                // normal teardown before the memory is released.
                auto RegisterAction =
                    WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddrRange,
                                                           SPSExecutorAddr>>(
                        ExecutorAddr::fromPtr(
                            &cps_register_seh_table_alloc_action),
                        TableRange, ImageBase);
                if (!RegisterAction)
                    return RegisterAction.takeError();
                auto DeregisterAction =
                    WrapperFunctionCall::Create<SPSArgList<SPSExecutorAddrRange,
                                                           SPSExecutorAddr>>(
                        ExecutorAddr::fromPtr(
                            &cps_deregister_seh_table_alloc_action),
                        TableRange, ImageBase);
                if (!DeregisterAction)
                    return DeregisterAction.takeError();
                G.allocActions().push_back(
                    {std::move(*RegisterAction), std::move(*DeregisterAction)});
                return Error::success();
            });
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
#endif

// Embedded JITLink object layer. Created before LLJIT takes ownership so
// runEmbedded keeps only the execution flow. COFF flags stay conditional on
// the object format; other formats use LLVM defaults.
static std::unique_ptr<llvm::orc::ObjectLinkingLayer>
createEmbeddedObjectLayer(llvm::orc::ExecutionSession &ES,
                          llvm::jitlink::JITLinkMemoryManager &MM,
                          const llvm::Triple &TT) {
    auto Layer =
        std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, MM);
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

static llvm::Error defineRuntimeSymbols(llvm::orc::LLJIT &J) {
    llvm::orc::SymbolMap Symbols;
    auto Intern = [&](llvm::StringRef Name) { return J.mangleAndIntern(Name); };
    using llvm::orc::ExecutorSymbolDef;
    Symbols[Intern("__cxa_allocate_exception")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_allocate_exception);
    Symbols[Intern("__cxa_throw")] =
        ExecutorSymbolDef::fromPtr(&__cxxabiv1::__cxa_throw);
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

int runEmbedded(const cps::CodeGen &CG, const std::vector<std::string> &Objects) {
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
    int ExitCode = Main();

    // Run platform deinitializers while JIT code is still mapped; frame
    // deregistration runs when the JIT releases its memory below.
    if (auto Err = J->deinitialize(J->getMainJITDylib())) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    return ExitCode;
}
