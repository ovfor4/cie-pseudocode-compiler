#include "cps/CodeGen.h"
#include "cps/Operator.h"
#include "cps/TypeAST.h"

using namespace cps;
using namespace llvm;

// Probing candidates must not evaluate operands or diagnose absent overloads:
// both static typing and emission use this same selection.
CodeGen::OperatorResolution CodeGen::resolveOperator(
    int Op, const TypeInfo *Left, const TypeInfo *Right, const ResultContext *Context) {
    OperatorResolution Result;
    if (!Left) return Result;
    auto Candidates = FuncGen->getOverloads(operatorName(Op));
    if (Left->IsClass) {
        auto Members = FuncGen->getOverloads(Left->Name + "." + operatorName(Op));
        Candidates.insert(Candidates.end(), Members.begin(), Members.end());
    }
    // An exact match wins; failing that, a CHAR operand may stand in for a
    // STRING parameter (it is converted when the arguments are marshalled).
    auto Accepts = [&](const TypeInfo *Actual, const std::string &Param, bool Lenient) {
        const TypeInfo *Formal = resolveType(Param);
        if (Actual->sameType(Formal)) return true;
        return Lenient && Actual->Kind == TypeKind::Char && Formal && Formal->isString();
    };
    // Two CHARs compare as CHARs; only '&' always builds a STRING.
    bool CanConvert = Right && (Op == '&' || Left->isString() || Right->isString());
    const bool PreferResult = Context && Context->Type;
    auto ReturnsWanted = [&](const FuncSig *Sig) {
        const TypeInfo *Ret = resolveType(Sig->ReturnTypeName);
        return Ret && Ret->sameType(Context->Type);
    };
    bool ParamMatched = false;
    auto RunLayers = [&](bool FilterReturn) {
        Result.Signature = nullptr;
        Result.Ambiguous = false;
        for (bool Lenient : {false, true}) {
            if (Lenient && !CanConvert) break;
            const FuncSig *Selected = nullptr;
            int Count = 0;
            std::vector<const FuncSig *> Matches;
            for (const FuncSig *Sig : Candidates) {
                bool Member = !Sig->Prototype->getOwnerClass().empty();
                if (Sig->Prototype->getOperator() != Op ||
                    Sig->Params.size() != (Right ? 2u : 1u) - Member) continue;
                if (!Member && !Accepts(Left, Sig->Params[0].TypeName, Lenient)) continue;
                if (Right && !Accepts(Right, Sig->Params[Member ? 0 : 1].TypeName, Lenient)) continue;
                ParamMatched = true;
                if (FilterReturn && !ReturnsWanted(Sig)) continue;
                ++Count;
                Matches.push_back(Sig);
                if (!Selected) Selected = Sig;
            }
            if (!FilterReturn && Count > 1) {
                if (const FuncSig *Default = FunctionGen::selectDefaultReturn(Matches)) {
                    Selected = Default;
                    Count = 1;
                }
            }
            if (Selected) {
                Result.Signature = Selected;
                Result.Ambiguous = Count > 1;
                break;
            }
        }
    };

    if (PreferResult) {
        RunLayers(true);
        if (Result.Signature || Result.Ambiguous) return Result;
    }
    if (!Context || !Context->Exact) {
        RunLayers(false);
    } else if (ParamMatched) {
        Result.WrongResult = true;
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
    size_t Object = registerStackObject(Storage, Type->Name, DiagnosticLine);
    Builder->CreateStore(Val, Storage);
    activateStackObject(Object);
    if (Object != size_t(-1)) StackObjects[Object].Temporary = true;
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
    bool Member = !Signature.Prototype->getOwnerClass().empty();
    std::vector<Value *> Args;
    std::vector<size_t> Transfers;
    auto Capture = [&](Value *Val, const TypeInfo *Type) -> Value * {
        auto *Slot = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                           Type->LLVMType, "case_operand");
        Builder->CreateStore(Val, Slot);
        return Slot;
    };
    if (Member) Args.push_back(Capture(Left, LeftType));
    else {
        if (LeftType->IsClass || containsString(LeftType))
            Left = copyClassValue(LeftType, Capture(Left, LeftType), Transfers);
        if (!Left) return nullptr;
        Args.push_back(Left);
    }
    const TypeInfo *RightType = resolveType(Signature.Params[Member ? 0 : 1].TypeName);
    if (RightType->IsClass || containsString(RightType))
        Right = copyClassValue(RightType, Capture(Right, RightType), Transfers);
    if (!Right) return nullptr;
    Args.push_back(Right);
    transferObjects(Transfers);
    return FuncGen->emitCall(Signature, Args);
}
