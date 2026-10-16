#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/Orc/AbsoluteSymbols.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdio>
#include <cstdlib>
#include <cxxabi.h>
#include <typeinfo>
#include <unwind.h>
#if defined(_WIN32) && defined(__x86_64__)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <limits>
#include <vector>

namespace {

// Kept for the follow-up Windows JITLink SEH work; unused by the LLJIT path.
class WindowsJITMemoryManager final : public llvm::SectionMemoryManager {
    uintptr_t ImageBase = std::numeric_limits<uintptr_t>::max();
    std::vector<PRUNTIME_FUNCTION> FunctionTables;
    std::string Error;

    uint8_t *recordSection(uint8_t *Address, uintptr_t Size) {
        auto Base = reinterpret_cast<uintptr_t>(Address);
        if (Address && Size && Base < ImageBase) ImageBase = Base;
        return Address;
    }

public:
    WindowsJITMemoryManager() : SectionMemoryManager(nullptr, true) {}
    ~WindowsJITMemoryManager() override { deregisterEHFrames(); }

    uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID, llvm::StringRef Name) override {
        return recordSection(SectionMemoryManager::allocateCodeSection(
            Size, Alignment, SectionID, Name), Size);
    }

    uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID, llvm::StringRef Name,
                                 bool IsReadOnly) override {
        return recordSection(SectionMemoryManager::allocateDataSection(
            Size, Alignment, SectionID, Name, IsReadOnly), Size);
    }

    void registerEHFrames(uint8_t *Address, uint64_t, size_t Size) override {
        if (!Size || !Error.empty()) return;
        if (Size % sizeof(RUNTIME_FUNCTION) ||
            Size / sizeof(RUNTIME_FUNCTION) > std::numeric_limits<DWORD>::max() ||
            ImageBase == std::numeric_limits<uintptr_t>::max()) {
            Error = "invalid embedded SEH function table";
            return;
        }
        auto *Table = reinterpret_cast<PRUNTIME_FUNCTION>(Address);
        if (!RtlAddFunctionTable(Table, static_cast<DWORD>(Size / sizeof(RUNTIME_FUNCTION)),
                                 static_cast<DWORD64>(ImageBase))) {
            Error = "cannot register embedded SEH function table";
            return;
        }
        FunctionTables.push_back(Table);
    }

    void deregisterEHFrames() override {
        for (auto *Table : FunctionTables) RtlDeleteFunctionTable(Table);
        FunctionTables.clear();
    }

    const std::string &getError() const { return Error; }
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
    JTMBOrErr->setCodeModel(llvm::CodeModel::Small);
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
