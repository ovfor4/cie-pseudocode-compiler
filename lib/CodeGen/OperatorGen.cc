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
    auto Accepts = [&](const TypeInfo *Actual, const ParamSig &Param, bool Lenient) {
        const TypeInfo *Formal = resolveType(Param.TypeName);
        if (Actual->sameType(Formal)) return true;
        // Keep builtin CHAR comparisons ahead of STRING construction.
        if (Right && Left->Kind == TypeKind::Char && Right->Kind == TypeKind::Char &&
            Op != '&' && Formal && Formal->isString()) return false;
        return Lenient && !Param.IsByRef && !Param.IsArray && implicitConversionRank(Actual, Formal) == 1;
    };
    // Builtin operations on exact operand types precede converted overloads.
    // Raw C text is the exception: visible STRING operators supply text semantics.
    auto Numeric = [](const TypeInfo *T) {
        return T && (T->Kind == TypeKind::Integer || T->Kind == TypeKind::Real);
    };
    bool Comparison = Op == tok_eq || Op == tok_ne || Op == '<' || Op == '>' ||
                      Op == tok_le || Op == tok_ge;
    bool Builtin = false;
    if (!Right) {
        Builtin = (Op == '-' && Numeric(Left)) ||
                  (Op == tok_not && Left->Kind == TypeKind::Boolean) ||
                  Left->Kind == TypeKind::CScalar;
    } else {
        Builtin = (Numeric(Left) && Numeric(Right) &&
                   (Comparison || Op == '+' || Op == '-' || Op == '*' || Op == '/' ||
                    Op == tok_div || Op == tok_mod)) ||
                  (Left->sameType(Right) &&
                   ((Comparison && (Left->Kind == TypeKind::Char ||
                                    Left->Kind == TypeKind::Boolean || Left->isEnum())) ||
                    ((Op == tok_and || Op == tok_or) && Left->Kind == TypeKind::Boolean) ||
                    ((Op == tok_eq || Op == tok_ne) && Left->isPointer() &&
                     Left->CIdentity != "char*"))) ||
                  (Left->Kind == TypeKind::CScalar && Right->Kind == TypeKind::CScalar) ||
                  (Left->isEnum() && Right->Kind == TypeKind::Integer && (Op == '+' || Op == '-'));
    }
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
            if (Lenient && Builtin) break;
            const FuncSig *Selected = nullptr;
            int Count = 0;
            std::vector<const FuncSig *> Matches;
            for (const FuncSig *Sig : Candidates) {
                bool Member = !Sig->Prototype->getOwnerClass().empty();
                if (Sig->Prototype->getOperator() != Op ||
                    Sig->Params.size() != (Right ? 2u : 1u) - Member) continue;
                if (!Member && !Accepts(Left, Sig->Params[0], Lenient)) continue;
                if (Right && !Accepts(Right, Sig->Params[Member ? 0 : 1], Lenient)) continue;
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
                                   const TypeInfo *LeftType, Value *Right, const TypeInfo *ActualRightType) {
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
    auto Prepare = [&](Value *Val, const TypeInfo *Actual, const TypeInfo *Formal) -> Value * {
        if (!Val || !Actual || !Formal) return nullptr;
        if (!Actual->sameType(Formal)) {
            const FuncSig *Constructor = findImplicitConstructor(Actual, Formal);
            if (!Constructor) return nullptr;
            std::vector<size_t> SourceTransfers;
            if (Actual->IsClass || containsString(Actual))
                Val = copyClassValue(Actual, Capture(Val, Actual), SourceTransfers);
            Val = emitImplicitConstruct(*Constructor, Formal, Val, SourceTransfers);
            if (!Val) return nullptr;
            materializeOperatorReceiver(Val, Formal);
            if (typeNeedsDestruction(Formal->Name)) Transfers.push_back(StackObjects.size() - 1);
        } else if (Formal->IsClass || containsString(Formal)) {
            Val = copyClassValue(Formal, Capture(Val, Formal), Transfers);
        }
        return Val;
    };
    if (Member) Args.push_back(Capture(Left, LeftType));
    else {
        Left = Prepare(Left, LeftType, resolveType(Signature.Params[0].TypeName));
        if (!Left) return nullptr;
        Args.push_back(Left);
    }
    const TypeInfo *RightType = resolveType(Signature.Params[Member ? 0 : 1].TypeName);
    Right = Prepare(Right, ActualRightType, RightType);
    if (!Right) return nullptr;
    Args.push_back(Right);
    transferObjects(Transfers);
    return FuncGen->emitCall(Signature, Args);
}
