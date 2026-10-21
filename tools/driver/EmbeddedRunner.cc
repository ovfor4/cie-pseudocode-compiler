#include "EmbeddedRunner.h"
#include "MinGWJIT.h"
#include "NativeCompiler.h"
#include "llvm/Object/ObjectFile.h"
#include "cps/CodeGen.h"
#include "cps/Diagnostics.h"
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
#ifdef __MINGW32__
#include <io.h>
#endif

namespace {

// Embedded JITLink object layer. Created before LLJIT takes ownership so
// runEmbedded keeps only the execution flow. COFF flags stay conditional on
// the object format; other formats use LLVM defaults.
static std::unique_ptr<llvm::orc::ObjectLinkingLayer>
createEmbeddedObjectLayer(llvm::orc::ExecutionSession &ES,
                          llvm::jitlink::JITLinkMemoryManager &MM,
                          const llvm::Triple &TT) {
#if defined(_WIN32) && defined(__x86_64__)
    if (TT.isOSBinFormatCOFF() && TT.getArch() == llvm::Triple::x86_64)
        return createMinGWObjectLinkingLayer(ES, MM);
#endif
    auto Layer = std::make_unique<llvm::orc::ObjectLinkingLayer>(ES, MM);
    if (TT.isOSBinFormatCOFF()) {
        Layer->setOverrideObjectFlagsWithResponsibilityFlags(true);
        Layer->setAutoClaimResponsibilityForObjectSymbols(true);
    }
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
    // Microsoft msvcrt.dll does not export these MinGW compatibility functions.
    // Keep FILE locks in the host CRT instead of resolving another DLL's copies.
    Symbols[Intern("_lock_file")] = ExecutorSymbolDef::fromPtr(&_lock_file);
    Symbols[Intern("_unlock_file")] = ExecutorSymbolDef::fromPtr(&_unlock_file);
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
        GetInterface = getMinGWObjectFileInterface;
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
    // Pipes are fully buffered by default. Use the host CRT's stdout, also
    // preferred by the JIT on Windows, so OUTPUT reaches desktop clients now.
    if (std::setvbuf(stdout, nullptr, _IONBF, 0) != 0) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot configure embedded stdout\n";
        return 1;
    }
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot initialize the embedded LLVM target\n";
        return 1;
    }

    auto JTMBOrErr = llvm::orc::JITTargetMachineBuilder::detectHost();
    if (!JTMBOrErr) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot detect the embedded JIT target: "
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
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot create the embedded LLVM engine: "
                     << llvm::toString(JITOrErr.takeError()) << "\n";
        return 1;
    }
    std::unique_ptr<llvm::orc::LLJIT> J = std::move(*JITOrErr);

    if (auto Err = defineRuntimeSymbols(*J)) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot define embedded runtime symbols: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    if (auto Err = loadCxxRuntime(*J)) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot load embedded C++ runtime: "
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
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot load the embedded C runtime: "
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
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "cannot clone the embedded module for JIT execution\n";
        return 1;
    }
    TSM.withModuleDo([&](llvm::Module &M) {
        M.setTargetTriple(J->getTargetTriple());
        M.setDataLayout(J->getDataLayout());
    });

    if (auto Err = J->addIRModule(J->getMainJITDylib(), std::move(TSM))) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    for (const auto &Path : Objects) {
        auto Buffer = llvm::MemoryBuffer::getFile(Path);
        if (!Buffer) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                         << "cannot read object file '" << Path << "': "
                         << Buffer.getError().message() << "\n";
            return 1;
        }
        if (auto Err = J->addObjectFile(std::move(*Buffer))) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                         << "cannot load object file '" << Path << "': "
                         << llvm::toString(std::move(Err)) << "\n";
            return 1;
        }
    }
    if (auto Err = J->initialize(J->getMainJITDylib())) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }

    auto MainSymOrErr = J->lookup("main");
    if (!MainSymOrErr) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "embedded LLVM could not resolve main: "
                     << llvm::toString(MainSymOrErr.takeError()) << "\n";
        return 1;
    }
    auto Main = MainSymOrErr->toPtr<int (*)()>();
    int ExitCode = Main();

    // Run platform deinitializers while JIT code is still mapped; frame
    // deregistration runs when the JIT releases its memory below.
    if (auto Err = J->deinitialize(J->getMainJITDylib())) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error, "")
                     << "embedded LLVM execution failed: "
                     << llvm::toString(std::move(Err)) << "\n";
        return 1;
    }
    return ExitCode;
}
