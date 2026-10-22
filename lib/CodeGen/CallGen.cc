#include "cps/Diagnostics.h"
#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileHandler.h"
#include "cps/MemoryHandler.h"
#include "cps/ExceptionHandler.h"
#include "Builtins.h"
#include "TemplateClone.h"
#include "cps/TypeAST.h"
#include "cps/TemplateAST.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cstdarg>

using namespace llvm;
using namespace cps;


// Collects the arguments for a call to a user FUNCTION/PROCEDURE, dispatching
// on the *declared* signature: BYREF parameters take the variable's alloca,
// BYVAL parameters are evaluated and coerced to the declared parameter type.
bool CodeGen::marshalCallArgs(const FuncSig &Signature,
                              const std::vector<std::unique_ptr<ExprAST>> &ArgExprs,
                              int Line,
                              std::vector<Value*> &Out,
                              std::vector<const TypeInfo *> *Variadic) {
    std::vector<ExprAST*> Args;
    for (const auto &Arg : ArgExprs) Args.push_back(Arg.get());
    return marshalCallArgs(Signature, Args, Line, Out, Variadic);
}

bool CodeGen::marshalCallArgs(const FuncSig &Signature,
                              const std::vector<ExprAST*> &ArgExprs,
                              int Line, std::vector<Value*> &Out,
                              std::vector<const TypeInfo *> *Variadic) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    const FuncSig *Sig = &Signature;
    const std::string &Callee = Sig->Name;
    if (Sig->IsVariadic ? ArgExprs.size() < Sig->Params.size() : ArgExprs.size() != Sig->Params.size()) {
        reportError("Incorrect # arguments passed to %s", Callee.c_str());
        return false;
    }

    std::vector<size_t> Transfers;
    for (size_t i = 0; i < ArgExprs.size(); ++i) {
        if (i >= Sig->Params.size()) {
            const TypeInfo *Promoted = nullptr;
            Value *V = emitVariadicArgument(ArgExprs[i], Callee, i + 1, Promoted);
            if (!V) return false;
            Out.push_back(V);
            if (Variadic) Variadic->push_back(Promoted);
            continue;
        }
        const ParamSig &P = Sig->Params[i];
        const std::string &TypeName = P.TypeName;
        bool IsByRef = P.IsByRef;
        ExprAST *ArgExpr = ArgExprs[i];

        if (P.IsArray) {
            if (!Arrays->emitArrayArgument(ArgExpr, P.TypeName, P.Rank, P.DeclaredBounds,
                                           Callee, Line, *this, Out)) {
                return false;
            }
            continue;
        }

        // A whole-array argument against a scalar parameter is always a user
        // error (and would smash the data pointer if let through).
        if (auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr)) {
            const SymbolInfo *Info = getSymbolInfo(Var->getName());
            if (Info && Info->IsArray) {
                reportError("Cannot pass whole array %s to %s; the parameter is not an array (declare it ARRAY OF %s)",
                            Var->getName().c_str(), Callee.c_str(), TypeName.c_str());
                return false;
            }
        }

        if (IsByRef) {
            auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr);
            bool InternalBinding = Signature.Prototype &&
                (sameIdentifier(Signature.Prototype->getName(), "NEW") ||
                 Signature.Prototype->getOperator() == tok_assign);
            if (!Var && InternalBinding) {
                const TypeInfo *Actual = getExprTypeInfo(ArgExpr);
                if (!Actual || !Actual->sameType(resolveType(TypeName))) {
                    reportError("BYREF argument to %s has an incompatible type", Callee.c_str());
                    return false;
                }
                if (isObjectLValue(ArgExpr)) {
                    LValueInfo LV;
                    if (!emitLValue(ArgExpr, LV)) return false;
                    Out.push_back(LV.Addr);
                } else {
                    Value *Storage = materializeOperatorReceiver(emitExpr(ArgExpr), Actual);
                    if (!Storage) return false;
                    Out.push_back(Storage);
                }
                continue;
            }
            if (!Var) {
                reportError("BYREF argument to %s must be a variable", Callee.c_str());
                return false;
            }
            const SymbolInfo *Info = getSymbolInfo(Var->getName());
            if (!Info) {
                reportError("Unknown variable %s in BYREF call", Var->getName().c_str());
                return false;
            }
            const TypeInfo *ParameterType = resolveType(TypeName);
            if (!(InternalBinding && ParameterType && ParameterType->isString()) &&
                !requireWritable(*Info, Var->getName())) return false;
            // BYREF aliases the storage, so the types must match exactly.
            if (!Types->sameType(Info->TypeName, TypeName)) {
                reportError("BYREF argument %s to %s must be of type %s (got %s)",
                            Var->getName().c_str(), Callee.c_str(),
                            TypeName.c_str(), Info->TypeName.c_str());
                return false;
            }
            Out.push_back(symbolAddress(*Info));
        } else {
            Value *V = emitOwnedValue(ArgExpr, resolveType(TypeName), Transfers);
            if (!V) return false;
            Out.push_back(V);
        }
    }
    transferObjects(Transfers);
    return true;
}

// C default argument promotions for the variadic part of an EXTERN call; only
// C values are accepted, matching the explicit-cast rule for C/pseudocode.
// C structs and pointers to them pass unchanged (C11 6.5.2.2p6-7, 7.16.1.1p2).
Value *CodeGen::emitVariadicArgument(ExprAST *Expr, const std::string &Callee, size_t Position,
                                     const TypeInfo *&Promoted) {
    ExprAST *Target = Expr;
    auto *Addr = dynamic_cast<AddrOfExprAST *>(Expr);
    if (Addr) Target = Addr->getTarget();
    const TypeInfo *Info = isWholeArrayVar(Target) ? nullptr : getExprTypeInfo(Target);
    const TypeInfo *Record = Types->pointerBase(Info);
    std::string Why;
    if (Record && Record->isRecord() && !Record->IsClass && !Types->isCStruct(Record, &Why)) {
        reportError("Variadic argument %zu to %s uses record %s, which is not a C struct: %s",
                    Position, Callee.c_str(), Record->Name.c_str(), Why.c_str());
        return nullptr;
    }
    if (!Info || !Types->isExternType(Info)) {
        reportError("Variadic argument %zu to %s must be a C value; cast it to a C type", Position, Callee.c_str());
        return nullptr;
    }
    // ^x of a C value or C struct is a plain pointer, like C's f(n, &x).
    if (Addr) {
        LValueInfo LV;
        if (!emitLValue(Target, LV)) return nullptr;
        Promoted = Types->resolve("C_TYPE(\"void*\")");
        return LV.Addr;
    }
    Promoted = Info;
    Value *V = emitExpr(Expr);
    if (!V || Info->Kind != TypeKind::CScalar) return V;
    if (Info->isReal()) {
        const TypeInfo *Double = resolveType("C_DOUBLE");
        if (Info->CRank >= Double->CRank) return V;
        Promoted = Double;
        return convertNumericValue(V, Info, Double);
    }
    Promoted = Types->promoteCScalar(Info);
    return convertNumericValue(V, Info, Promoted);
}

// Resolve using static source types only; argument evaluation belongs to marshalling.
const FuncSig *CodeGen::resolveCall(const std::string &Name,
                                  const std::vector<std::unique_ptr<ExprAST>> &Args,
                                  const std::vector<std::string> *Explicit) {
    auto Overloads = FuncGen->getOverloads(Name);
    bool HasTemplates = hasFunctionTemplate(Name);
    bool HasExplicit = Explicit && !Explicit->empty();
    if (HasExplicit && !HasTemplates) {
        reportError("Explicit template arguments require a function template: %s", Name.c_str());
        return nullptr;
    }
    if (Overloads.empty() && !HasTemplates) {
        reportError("Call to undefined function %s", Name.c_str());
        return nullptr;
    }
    if (!HasTemplates && Overloads.size() == 1 && !Overloads.front()->IsOverload) return Overloads.front();
    // Visible EXTERN redeclarations are identical by construction.
    if (!HasTemplates && Overloads.front()->IsExternal) return Overloads.front();
    struct ArgumentType { const TypeInfo *Type; int Rank; };
    std::vector<ArgumentType> Actual;
    std::string Key;
    for (const auto &Arg : Args) {
        const ArrayMetadata *Array = nullptr;
        if (auto *Var = dynamic_cast<VariableExprAST *>(Arg.get())) {
            const SymbolInfo *Symbol = getSymbolInfo(Var->getName());
            if (Symbol && Symbol->IsArray) Array = Arrays->getMetadata(Var->getName());
        }
        const TypeInfo *Type = Array ? Types->resolve(Array->ElementTypeName) : getExprTypeInfo(Arg.get());
        int Rank = Array ? Array->Rank : 0;
        Actual.push_back({Type, Rank});
        if (!Key.empty()) Key += ", ";
        if (Rank) Key += "ARRAY" + std::to_string(Rank) + " OF ";
        Key += Type ? Type->Name : "?";
    }
    const FuncSig *Ordinary = nullptr;
    for (const FuncSig *Sig : Overloads) {
        if (HasExplicit) break;
        if (Sig->Params.size() != Actual.size()) continue;
        bool Match = true;
        for (size_t I = 0; I < Actual.size(); ++I) {
            const ParamSig &P = Sig->Params[I];
            if (Actual[I].Rank != (P.IsArray ? P.Rank : 0) || !Actual[I].Type ||
                !Actual[I].Type->sameType(Types->resolve(P.TypeName))) {
                Match = false;
                break;
            }
        }
        if (Match) {
            if (Ordinary && HasTemplates) {
                reportError("Ambiguous call to %s(%s)", Name.c_str(), Key.c_str());
                return nullptr;
            }
            Ordinary = Sig;
            if (!HasTemplates) return Sig;
        }
    }
    if (Ordinary) return Ordinary;
    // Failing an exact match, CHAR and STRING arguments convert into one
    // another; the overload must be unique.
    if (!HasTemplates && !HasExplicit) {
        auto Scalar = [](const TypeInfo *T) { return T && (T->Kind == TypeKind::Char || T->isString()); };
        const FuncSig *Converted = nullptr;
        for (const FuncSig *Sig : Overloads) {
            if (Sig->Params.size() != Actual.size()) continue;
            bool Match = true;
            for (size_t I = 0; I < Actual.size() && Match; ++I) {
                const ParamSig &P = Sig->Params[I];
                const TypeInfo *Formal = Types->resolve(P.TypeName);
                Match = Actual[I].Rank == (P.IsArray ? P.Rank : 0) && Actual[I].Type &&
                        (Actual[I].Type->sameType(Formal) ||
                         (!P.IsByRef && !P.IsArray && Scalar(Actual[I].Type) && Scalar(Formal)));
            }
            if (!Match) continue;
            if (Converted) {
                reportError("Ambiguous call to %s(%s)", Name.c_str(), Key.c_str());
                return nullptr;
            }
            Converted = Sig;
        }
        if (Converted) return Converted;
    }
    if (HasTemplates) {
        // A matching non-variadic template wins before any variadic candidate.
        for (bool Variadic : {false, true}) {
            const TemplateDefinition *Selected = nullptr;
            detail::TemplateBindings SelectedBindings;
            for (const auto &Definition : Templates.at(Name)) {
                auto *Function = Definition.Decl->getFunction();
                if (!Function || Definition.Decl->isVariadic() != Variadic || !isVisible(Definition.OwnerUnit)) continue;
                const auto &Params = Function->getProto()->getParams();
                const auto &TemplateParams = Definition.Decl->getParams();
                bool HasPackParam = !Params.empty() && Params.back().IsPack;
                size_t FixedCount = Params.size() - HasPackParam;
                if (HasPackParam ? Actual.size() < FixedCount : Actual.size() != FixedCount) continue;
                detail::TemplateBindings Bindings;
                for (const auto &Param : TemplateParams) Bindings.emplace(Param.Name, std::vector<std::string>{});
                SaveAndRestore<bool> Speculation(SpeculativeTemplateResolution, true);
                bool Match = true;
                if (HasExplicit) {
                    size_t FixedTypes = TemplateParams.size() - Variadic;
                    if (!Variadic && Explicit->size() > FixedTypes) continue;
                    for (size_t I = 0; I < Explicit->size(); ++I) {
                        auto Type = canonicalTemplateArgument((*Explicit)[I]);
                        if (!Type) { Match = false; break; }
                        Bindings.at(TemplateParams[std::min(I, FixedTypes)].Name).push_back(*Type);
                    }
                }
                if (!Match) continue;
                for (size_t I = 0; I < FixedCount; ++I) {
                    const auto &P = Params[I];
                    const TypeInfo *Type = Actual[I].Type;
                    if (!Type || Type->isVoid() || Actual[I].Rank != (P.IsArray ? P.Rank : 0)) { Match = false; break; }
                    auto Binding = Bindings.find(P.TypeName);
                    if (Binding != Bindings.end()) {
                        if (Binding->second.empty()) Binding->second.push_back(Type->Name);
                        else if (!Type->sameType(resolveType(Binding->second.front()))) { Match = false; break; }
                    }
                }
                if (HasPackParam) {
                    auto &Pack = Bindings.at(Params.back().TypeName);
                    // Explicit pack types constrain a prefix; deduction fills the rest.
                    if (Pack.size() > Actual.size() - FixedCount) Match = false;
                    for (size_t I = FixedCount; Match && I < Actual.size(); ++I) {
                        const TypeInfo *Type = Actual[I].Type;
                        size_t Index = I - FixedCount;
                        if (!Type || Type->isVoid() || Actual[I].Rank) { Match = false; break; }
                        if (Index < Pack.size()) {
                            if (!Type->sameType(resolveType(Pack[Index]))) Match = false;
                        } else Pack.push_back(Type->Name);
                    }
                }
                for (const auto &Param : TemplateParams)
                    if (!Param.IsPack && Bindings.at(Param.Name).empty()) Match = false;
                if (!Match) continue;
                // Composite parameter types can use bindings deduced at later positions,
                // but do not themselves contribute deductions.
                detail::TemplateClone Substitute(Bindings, [&](int, const std::string &) { Match = false; });
                for (size_t I = 0; I < FixedCount; ++I) {
                    const auto &TypeName = Params[I].TypeName;
                    auto Binding = Bindings.find(TypeName);
                    std::string Expected = Binding != Bindings.end() ? Binding->second.front()
                        : TypeName.find('<') != std::string::npos ? Substitute.type(TypeName) : TypeName;
                    if (!Match || !Actual[I].Type->sameType(resolveType(Expected))) { Match = false; break; }
                }
                if (!Match) continue;
                if (Selected) {
                    reportError("Ambiguous call to %s(%s)", Name.c_str(), Key.c_str());
                    return nullptr;
                }
                Selected = &Definition;
                SelectedBindings = std::move(Bindings);
            }
            if (Selected) return instantiateFunction(*Selected, SelectedBindings);
        }
    }
    reportError("No matching overload for %s(%s)", Name.c_str(), Key.c_str());
    return nullptr;
}
