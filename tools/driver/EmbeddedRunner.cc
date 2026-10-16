#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Utils/Cloning.h"
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

// RuntimeDyld relocates COFF unwind entries relative to the lowest loaded
// section. Keep sections together and give Windows that same image base.
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

// Static runtime symbols are not available through dlsym. Register their
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

static void registerRuntimeSymbols() {
    using llvm::sys::DynamicLibrary;
#ifdef __MINGW32__
    // Use the same CRT aliases as native IR linking, including its stdio formatting.
    DynamicLibrary::AddSymbol("snprintf", reinterpret_cast<void *>(&__ms_snprintf));
    DynamicLibrary::AddSymbol("strtoll", reinterpret_cast<void *>(&std::strtoll));
#endif
    DynamicLibrary::AddSymbol("__cxa_allocate_exception", reinterpret_cast<void *>(&__cxxabiv1::__cxa_allocate_exception));
    DynamicLibrary::AddSymbol("__cxa_throw", reinterpret_cast<void *>(&__cxxabiv1::__cxa_throw));
    DynamicLibrary::AddSymbol("__cxa_begin_catch", reinterpret_cast<void *>(&__cxxabiv1::__cxa_begin_catch));
    DynamicLibrary::AddSymbol("__cxa_end_catch", reinterpret_cast<void *>(&__cxxabiv1::__cxa_end_catch));
    DynamicLibrary::AddSymbol("_ZTIi", const_cast<std::type_info *>(&typeid(int)));
#if defined(__SEH__)
    DynamicLibrary::AddSymbol("__gxx_personality_seh0", reinterpret_cast<void *>(&__gxx_personality_seh0));
#elif defined(__USING_SJLJ_EXCEPTIONS__)
    DynamicLibrary::AddSymbol("__gxx_personality_sj0", reinterpret_cast<void *>(&__gxx_personality_sj0));
    DynamicLibrary::AddSymbol("_Unwind_SjLj_Resume", reinterpret_cast<void *>(&_Unwind_SjLj_Resume));
#else
    DynamicLibrary::AddSymbol("__gxx_personality_v0", reinterpret_cast<void *>(&__gxx_personality_v0));
#endif
    DynamicLibrary::AddSymbol("_Unwind_Resume", reinterpret_cast<void *>(&_Unwind_Resume));
}

int runEmbedded(const cps::CodeGen &CG) {
    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter()) {
        llvm::errs() << "error: cannot initialize the embedded LLVM target\n";
        return 1;
    }
    std::string Error;
    if (llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr, &Error)) {
        llvm::errs() << "error: cannot load process symbols: " << Error << "\n";
        return 1;
    }
    registerRuntimeSymbols();
    // Keep CodeGen's module and context alive while the engine owns its clone.
    auto Module = llvm::CloneModule(CG.getModule());
    Module->setTargetTriple(llvm::Triple(llvm::sys::getProcessTriple()));
    llvm::EngineBuilder Builder(std::move(Module));
    Builder.setEngineKind(llvm::EngineKind::JIT);
    Builder.setErrorStr(&Error);
#ifdef __APPLE__
    // MCJIT registers DWARF frames, not Darwin's compact unwind records.
    llvm::TargetOptions Options;
    Options.MCOptions.EmitDwarfUnwind = llvm::EmitDwarfUnwindType::Always;
    Builder.setTargetOptions(Options);
#endif
#if defined(_WIN32) && defined(__x86_64__)
    auto MemoryManager = std::make_unique<WindowsJITMemoryManager>();
    auto *WindowsMemory = MemoryManager.get();
    Builder.setMCJITMemoryManager(std::move(MemoryManager));
#else
    Builder.setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
#endif
    std::unique_ptr<llvm::ExecutionEngine> Engine(Builder.create());
    if (!Engine) {
        llvm::errs() << "error: cannot create the embedded LLVM engine: " << Error << "\n";
        return 1;
    }
    Engine->finalizeObject();
#if defined(_WIN32) && defined(__x86_64__)
    // MCJIT does not propagate memory-manager registration failures.
    if (!WindowsMemory->getError().empty()) {
        llvm::errs() << "error: embedded LLVM execution failed: "
                     << WindowsMemory->getError() << "\n";
        return 1;
    }
#endif
    if (Engine->hasError()) {
        llvm::errs() << "error: embedded LLVM execution failed: " << Engine->getErrorMessage() << "\n";
        return 1;
    }
    auto MainAddress = Engine->getFunctionAddress("main");
    if (!MainAddress) {
        llvm::errs() << "error: embedded LLVM could not resolve main\n";
        return 1;
    }
    auto Main = reinterpret_cast<int (*)()>(MainAddress);
    return Main();
}
