#include "EmbeddedRunner.h"
#include "cps/CodeGen.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/Utils/Cloning.h"

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
    // Keep CodeGen's module and context alive while the engine owns its clone.
    auto Module = llvm::CloneModule(CG.getModule());
    Module->setTargetTriple(llvm::sys::getProcessTriple());
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
