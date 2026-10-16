#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Support/Error.h"
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

namespace {

// In-process COFF/SEH support for the MinGW x86_64 JIT.
//
// The full upstream COFFPlatform starts the VC runtime loading machinery,
// which does not apply to the current MinGW build. This plugin provides only
// the linking and registration pieces needed for in-process execution:
// a per-graph __ImageBase shared by relocations and SEH registration,
// preservation of .pdata/.xdata, GOT/stub construction for external calls
// via the x86_64 utilities, and RtlAddFunctionTable registration through
// JITLink allocation actions. Exception propagation still uses the existing
// C++ ABI, LLVM exception instructions and platform unwinder.

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

        // Keep SEH tables alive when their functions are alive. The default
        // COFF backend already keeps .pdata; also cover .xdata here.
        Config.PrePrunePasses.push_back([](LinkGraph &G) -> Error {
            for (const char *Name : {".pdata", ".xdata"}) {
                if (auto *Sec = G.findSectionByName(Name)) {
                    for (auto *Sym : Sec->symbols()) {
                        if (Sym->isDefined())
                            Sym->setLive(true);
                    }
                    for (auto *B : Sec->blocks()) {
                        (void)B;
                    }
                }
            }
            return Error::success();
        });

        // Build GOT/stub entries for externals. Host runtime calls reach
        // their far addresses through these under the large code model.
        Config.PostPrunePasses.push_back([](LinkGraph &G) -> Error {
            using namespace llvm::jitlink::x86_64;
            GOTTableManager GOT(G);
            PLTTableManager PLT(G, GOT);
            visitExistingEdges(G, GOT, PLT);
            return Error::success();
        });

        // COFF unwind tables use 32-bit image-relative addresses, so the
        // image base must sit at or below the lowest JIT address. Alias the
        // external __ImageBase to the lowest defined block once addresses
        // are final; a trailing zero block would push RVAs negative.
        Config.PostAllocationPasses.push_back([](LinkGraph &G) -> Error {
            // LLJIT platform-support graphs define their own __ImageBase;
            // only alias it when it is purely external (our module graph).
            if (G.findDefinedSymbolByName(G.intern("__ImageBase")))
                return Error::success();
            auto *ImageBaseExt =
                G.findExternalSymbolByName(G.intern("__ImageBase"));
            if (!ImageBaseExt)
                return Error::success();
            // Platform graphs carry LLJIT-internal symbols; leave their
            // own base alone or materialization sees it twice.
            if (G.findDefinedSymbolByName(
                    G.intern("__lljit.platform_support_instance")) ||
                G.findExternalSymbolByName(
                    G.intern("__lljit.platform_support_instance")))
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
                          Scope::Default, true);
            return Error::success();
        });

        // After relocations are applied, validate the function table and
        // register it with the system unwinder via allocation actions.
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
                size_t Size = TableRange.size();
                if (Size % sizeof(RUNTIME_FUNCTION) ||
                    Size / sizeof(RUNTIME_FUNCTION) == 0 ||
                    Size / sizeof(RUNTIME_FUNCTION) >
                        static_cast<size_t>(std::numeric_limits<DWORD>::max()))
                    return make_error<JITLinkError>(
                        "invalid embedded SEH function table");
                // The table must be sorted by BeginAddress with Begin < End.
                auto *Table = TableRange.Start.toPtr<const RUNTIME_FUNCTION *>();
                uint64_t Count = Size / sizeof(RUNTIME_FUNCTION);
                DWORD PrevBegin = 0;
                for (uint64_t I = 0; I < Count; ++I) {
                    DWORD Begin = support::endian::read32le(
                        reinterpret_cast<const char *>(Table + I) + 0);
                    DWORD End = support::endian::read32le(
                        reinterpret_cast<const char *>(Table + I) + 4);
                    if (!(Begin < End))
                        return make_error<JITLinkError>(
                            "invalid embedded SEH function table");
                    if (I > 0 && !(PrevBegin < Begin))
                        return make_error<JITLinkError>(
                            "invalid embedded SEH function table");
                    PrevBegin = Begin;
                }
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

    llvm::Error notifyFailed(llvm::orc::MaterializationResponsibility &MR) override {
        (void)MR;
        return llvm::Error::success();
    }

    llvm::Error notifyRemovingResources(llvm::orc::JITDylib &JD,
                                        llvm::orc::ResourceKey K) override {
        (void)JD;
        (void)K;
        return llvm::Error::success();
    }

    void notifyTransferringResources(llvm::orc::JITDylib &JD,
                                     llvm::orc::ResourceKey DstKey,
                                     llvm::orc::ResourceKey SrcKey) override {
        (void)JD;
        (void)DstKey;
        (void)SrcKey;
    }
};

} // namespace
#endif

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

// MinGW emits ___chkstk_ms stack probes for large frames with a custom
// convention (size in %eax, result in %rax, all else preserved). Never
// reference the host probe by name: a weak reference marks the host import
// weak on COFF and segfaults the compiler itself. Provide a JIT-only probe
// under a unique name and map the JIT symbol to it. Same code on all
// platforms; the stub is unused where the backend never emits the probe.
#if defined(__x86_64__)
extern "C" __attribute__((naked)) void __cps_jit_chkstk_ms() {
    __asm__ volatile(
        "pushq %rcx\n\t"
        "pushq %rax\n\t"
        "cmpq $0x1000, %rax\n\t"
        "leaq 0x18(%rsp), %rcx\n\t"
        "jb 1f\n\t"
        "0:\n\t"
        "subq $0x1000, %rcx\n\t"
        "orq $0, (%rcx)\n\t"
        "subq $0x1000, %rax\n\t"
        "cmpq $0x1000, %rax\n\t"
        "ja 0b\n\t"
        "1:\n\t"
        "subq %rax, %rcx\n\t"
        "orq $0, (%rcx)\n\t"
        "popq %rax\n\t"
        "popq %rcx\n\t"
        "ret\n\t");
}
#else
extern "C" void __cps_jit_chkstk_ms() {}
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
    // The JIT-generated main references __main on MinGW; provide a no-op so
    // the host CRT is not initialized a second time.
    Symbols[Intern("__main")] =
        ExecutorSymbolDef::fromPtr(&__cps_jit_noop__main);
#endif
    Symbols[Intern("___chkstk_ms")] =
        ExecutorSymbolDef::fromPtr(&__cps_jit_chkstk_ms);
    return J.getMainJITDylib().define(
        llvm::orc::absoluteSymbols(std::move(Symbols)));
}

int runEmbedded(const cps::CodeGen &CG) {
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
    Builder.setJITTargetMachineBuilder(std::move(*JTMBOrErr));
    // Use JITLink explicitly so Windows COFF does not fall back to RuntimeDyld.
    Builder.setObjectLinkingLayerCreator(
        [](llvm::orc::ExecutionSession &ES,
           llvm::jitlink::JITLinkMemoryManager &MM)
            -> llvm::Expected<std::unique_ptr<llvm::orc::ObjectLayer>> {
            return std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, MM);
        });

    auto JITOrErr = Builder.create();
    if (!JITOrErr) {
        llvm::errs() << "error: cannot create the embedded LLVM engine: "
                     << llvm::toString(JITOrErr.takeError()) << "\n";
        return 1;
    }
    std::unique_ptr<llvm::orc::LLJIT> J = std::move(*JITOrErr);

    // LLJIT's default layer sets these for COFF; our custom JITLink layer
    // must do the same or IR-added modules report Unexpected definitions
    // for backend-added symbols like __ImageBase.
    if (J->getTargetTriple().isOSBinFormatCOFF()) {
        if (auto *COFFObjLayer = llvm::dyn_cast<llvm::orc::ObjectLinkingLayer>(
                &J->getObjLinkingLayer())) {
            COFFObjLayer->setOverrideObjectFlagsWithResponsibilityFlags(true);
            COFFObjLayer->setAutoClaimResponsibilityForObjectSymbols(true);
        }
    }

#if defined(_WIN32) && defined(__x86_64__)
    // Register MinGW SEH tables through JITLink; failures surface as link
    // errors below and prevent entry to JIT code.
    if (auto *ObjLayer = llvm::dyn_cast<llvm::orc::ObjectLinkingLayer>(
            &J->getObjLinkingLayer()))
        ObjLayer->addPlugin(std::make_shared<MinGWSEHRegistrationPlugin>());
    else {
        llvm::errs() << "error: embedded LLVM engine does not use JITLink\n";
        return 1;
    }
#endif

    if (auto Err = defineRuntimeSymbols(*J)) {
        llvm::errs() << "error: cannot define embedded runtime symbols: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }

    // Keep CodeGen's module and context alive; the JIT owns an independent copy.
    llvm::orc::ThreadSafeContext TSCtx(std::make_unique<llvm::LLVMContext>());
    llvm::orc::ThreadSafeModule TSM =
        llvm::orc::cloneExternalModuleToContext(CG.getModule(), TSCtx);
    if (!TSM) {
        llvm::errs() << "error: cannot clone the embedded module for JIT execution\n";
        return 1;
    }
    if (auto Err = TSM.withModuleDo([&](llvm::Module &M) -> llvm::Error {
            M.setTargetTriple(J->getTargetTriple());
            M.setDataLayout(J->getDataLayout());
            // COFF unwind tables address the personality by 32-bit RVA, so
            // it must live near JIT code. Route through a JIT-local stub;
            // the stub's own call to the host runtime uses 64-bit. Same
            // code on all platforms; only COFF needs it.
            if (J->getTargetTriple().isOSBinFormatCOFF()) {
                for (const char *RealName :
                     {"__gxx_personality_seh0", "__gxx_personality_sj0",
                      "__gxx_personality_v0"}) {
                    llvm::Function *Real = M.getFunction(RealName);
                    if (!Real)
                        continue;
                    bool Used = false;
                    for (auto &F : M.functions()) {
                        if (F.hasPersonalityFn() &&
                            F.getPersonalityFn()->stripPointerCasts() == Real) {
                            Used = true;
                            break;
                        }
                    }
                    if (!Used)
                        continue;
                    std::string FwdName = std::string("__cps_jit_") + RealName;
                    if (M.getFunction(FwdName))
                        continue;
                    llvm::Function *Fwd = llvm::Function::Create(
                        Real->getFunctionType(),
                        llvm::GlobalValue::InternalLinkage, FwdName, M);
                    llvm::BasicBlock *BB = llvm::BasicBlock::Create(
                        M.getContext(), "", Fwd);
                    llvm::IRBuilder<> B(BB);
                    llvm::CallInst *C = B.CreateCall(Real, {});
                    C->setTailCall();
                    B.CreateRet(C);
                    for (auto &F : M.functions()) {
                        if (F.hasPersonalityFn() &&
                            F.getPersonalityFn()->stripPointerCasts() == Real)
                            F.setPersonalityFn(Fwd);
                    }
                }
            }
            return llvm::Error::success();
        })) {
        llvm::errs() << "error: cannot configure the embedded JIT module: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }

    if (auto Err = J->addIRModule(J->getMainJITDylib(), std::move(TSM))) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
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
