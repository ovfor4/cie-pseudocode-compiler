#pragma once
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include <map>
#include <functional>
#include <vector>

namespace cps {

// C++ ABI exception emission. State is keyed by function because file helper
// bodies and user functions are emitted using the same builder as their caller.
class ExceptionHandler {
    struct Handler {
        llvm::BasicBlock *Target;
        llvm::AllocaInst *Exception;
        unsigned CatchDepth;
    };
    struct FunctionState {
        std::vector<Handler> Handlers;
        unsigned CatchDepth = 0;
        bool HasCleanups = false;
    };

    struct CleanupPoint {
        llvm::Instruction *Before;
        FunctionState State;
        bool Unwinding;
        bool TemporariesOnly = false;
    };
    std::map<llvm::Function *, std::vector<CleanupPoint>> CleanupPoints;
    bool FillingUnwind = false;
    llvm::LLVMContext &Context;
    llvm::IRBuilder<> &Builder;
    llvm::Module &Module;
    std::map<llvm::Function *, FunctionState> States;
    llvm::FunctionCallee Allocate, Throw, BeginCatch, EndCatch, Personality;
    llvm::GlobalVariable *IntTypeInfo = nullptr;

    void ensureRuntime();
    llvm::BasicBlock *emitUnwindDestination();

public:
    ExceptionHandler(llvm::LLVMContext &C, llvm::IRBuilder<> &B, llvm::Module &M)
        : Context(C), Builder(B), Module(M) {}

    void prepareFunction(llvm::Function *F);
    void pushHandler(llvm::BasicBlock *Target);
    llvm::AllocaInst *popHandler();
    void beginCatch(llvm::AllocaInst *Exception);
    void endCatch();
    llvm::Instruction *emitReturnCleanup();
    void enableCleanups(llvm::Function *F);
    void recordExit(llvm::Instruction *Before, bool Unwinding = false);
    void fillCleanups(llvm::Function *F, const std::function<void(bool)> &Destroy);

    llvm::Value *emitCall(llvm::FunctionCallee Callee, llvm::ArrayRef<llvm::Value *> Args,
                          const llvm::Twine &Name = "");
    void emitThrow();
};

} // namespace cps
