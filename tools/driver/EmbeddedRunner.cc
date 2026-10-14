#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <cxxabi.h>
#include <typeinfo>
#include <unwind.h>

// Static C++ runtime symbols are not available through dlsym. Register their
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

static void registerExceptionSymbols() {
    using llvm::sys::DynamicLibrary;
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
    registerExceptionSymbols();
    // Keep CodeGen's module and context alive while the engine owns its clone.
    auto Module = llvm::CloneModule(CG.getModule());
    Module->setTargetTriple(llvm::Triple(llvm::sys::getProcessTriple()));
    llvm::EngineBuilder Builder(std::move(Module));
    Builder.setEngineKind(llvm::EngineKind::JIT);
    Builder.setErrorStr(&Error);
    Builder.setMCJITMemoryManager(std::make_unique<llvm::SectionMemoryManager>());
    std::unique_ptr<llvm::ExecutionEngine> Engine(Builder.create());
    if (!Engine) {
        llvm::errs() << "error: cannot create the embedded LLVM engine: " << Error << "\n";
        return 1;
    }
    Engine->finalizeObject();
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
