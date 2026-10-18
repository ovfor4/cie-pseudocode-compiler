#include "cps/CodeGen.h"
#include "cps/Operator.h"
#include "cps/TypeAST.h"

using namespace cps;
using namespace llvm;

// Probing candidates must not evaluate operands or diagnose absent overloads:
// both static typing and emission use this same selection.
CodeGen::OperatorResolution CodeGen::resolveOperator(
    int Op, const TypeInfo *Left, const TypeInfo *Right) {
    OperatorResolution Result;
    if (!Left) return Result;
    auto Candidates = FuncGen->getOverloads(operatorName(Op));
    if (Left->IsClass) {
        auto Members = FuncGen->getOverloads(Left->Name + "." + operatorName(Op));
        Candidates.insert(Candidates.end(), Members.begin(), Members.end());
    }
    for (const FuncSig *Sig : Candidates) {
        bool Member = !Sig->Prototype->getOwnerClass().empty();
        if (Sig->Prototype->getOperator() != Op || Sig->Params.size() != (Right ? 2u : 1u) - Member) continue;
        if (!Member && !Left->sameType(resolveType(Sig->Params[0].TypeName))) continue;
        if (Right && !Right->sameType(resolveType(Sig->Params[Member ? 0 : 1].TypeName))) continue;
        if (Result.Signature) Result.Ambiguous = true;
        else Result.Signature = Sig;
    }
    return Result;
}

bool CodeGen::checkOperatorAccess(const FuncSig &Signature) {
    const PrototypeAST *Proto = Signature.Prototype;
    if (!Proto->getOwnerClass().empty() && !Proto->isPublic() &&
        !sameIdentifier(CurrentClass, Proto->getOwnerClass())) {
        reportError("Cannot access private method %s of type %s",
                    Proto->getName().c_str(), Proto->getOwnerClass().c_str());
        return false;
    }
    return true;
}

Value *CodeGen::materializeOperatorReceiver(Value *Val, const TypeInfo *Type) {
    if (!Val || !Type) return nullptr;
    auto *Storage = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                          Type->LLVMType, "operator_receiver");
    size_t Object = registerStackObject(Storage, Type->Name, DiagnosticLine, false, true);
    Builder->CreateStore(Val, Storage);
    activateStackObject(Object);
    return Storage;
}

Value *CodeGen::emitOperatorCall(const FuncSig &Signature, ExprAST *Left, ExprAST *Right) {
    if (!checkOperatorAccess(Signature)) return nullptr;
    std::vector<Value*> Args;
    std::vector<ExprAST*> Explicit;
    if (Signature.Prototype->getOwnerClass().empty()) {
        Explicit.push_back(Left);
    } else {
        Value *Receiver = nullptr;
        if (dynamic_cast<VariableExprAST*>(Left) || dynamic_cast<ArrayAccessExprAST*>(Left) ||
            dynamic_cast<DesignatorExprAST*>(Left)) {
            LValueInfo LV;
            if (!emitLValue(Left, LV)) return nullptr;
            Receiver = LV.Addr;
        } else {
            Receiver = materializeOperatorReceiver(emitExpr(Left), getExprTypeInfo(Left));
        }
        if (!Receiver) return nullptr;
        Args.push_back(Receiver);
    }
    if (Right) Explicit.push_back(Right);
    if (!marshalCallArgs(Signature, Explicit, DiagnosticLine, Args)) return nullptr;
    return FuncGen->emitCall(Signature, Args);
}

// CASE has already evaluated and captured its selector and label values.
Value *CodeGen::emitOperatorValues(const FuncSig &Signature, Value *Left,
                                   const TypeInfo *LeftType, Value *Right) {
    if (!checkOperatorAccess(Signature)) return nullptr;
    if (!Signature.Prototype->getOwnerClass().empty())
        Left = materializeOperatorReceiver(Left, LeftType);
    if (!Left) return nullptr;
    return FuncGen->emitCall(Signature, {Left, Right});
}
