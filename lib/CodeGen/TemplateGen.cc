#include "cps/CodeGen.h"
#include "cps/TemplateAST.h"
#include "TemplateClone.h"
#include "llvm/ADT/ScopeExit.h"

using namespace cps;
using namespace cps::detail;
using namespace llvm;

bool CodeGen::hasFunctionTemplate(const std::string &Name) const {
    auto It = Templates.find(Name);
    if (It == Templates.end()) return false;
    for (const auto &Definition : It->second)
        if (Definition.Decl->getFunction() && isVisible(Definition.OwnerUnit)) return true;
    return false;
}

bool CodeGen::hasCallable(const std::string &Name) const {
    return FuncGen->hasFunction(Name) || hasFunctionTemplate(Name);
}

void CodeGen::validateTemplates() {
    for (const auto &[Name, Definitions] : Templates) {
        auto Ordinary = FuncGen->getOverloads(Name, true);
        size_t Count = Ordinary.size();
        for (const auto &Definition : Definitions) Count += Definition.Decl->getFunction() != nullptr;
        for (const auto &Definition : Definitions) {
            selectUnit(Definition.OwnerUnit);
            SaveAndRestore<int> Location(DiagnosticLine, Definition.Decl->getLine());
            auto *Function = Definition.Decl->getFunction();
            if (!Function) continue;
            if (sameIdentifier(Name, "MAIN")) reportError("MAIN cannot be a template");
            if (FunctionGen::isReservedRuntimeName(Name))
                reportError("'%s' is a reserved runtime symbol and cannot be a FUNCTION/PROCEDURE name", Name.c_str());
            if (isBuiltinName(Name)) reportError("'%s' is a built-in function and cannot be redefined", Name.c_str());
            if (Types->lookupEnumConstant(Name) || Types->resolve(Name))
                reportError("Template %s conflicts with an enum value or type name", Name.c_str());
            bool Consistent = Function->getProto()->isOverload();
            for (const auto &Other : Definitions)
                if (Other.Decl->getFunction()) Consistent &= Other.Decl->getFunction()->getProto()->isOverload();
            for (const auto *Other : Ordinary) Consistent &= Other->IsOverload;
            if (Count > 1 && !Consistent)
                reportError("Function %s has multiple definitions; every overload must be declared OVERLOAD", Name.c_str());
        }
    }
}

void CodeGen::captureTemplateSymbols() {
    for (size_t I = 0; I < Units->size(); ++I) {
        selectUnit(I);
        importSymbols();
        for (const auto &Stmt : (*Units)[I].Statements) {
            if (auto *Constant = dynamic_cast<ConstantStmtAST *>(Stmt.get())) {
                auto It = Exports[I].find(Constant->getName());
                if (It != Exports[I].end()) Symbols[It->first] = It->second;
            } else if (auto *Template = dynamic_cast<TemplateDeclAST *>(Stmt.get())) {
                TemplateSymbols.emplace(Template, Symbols);
            }
        }
    }
}

const FuncSig *CodeGen::instantiateFunction(const TemplateDefinition &Definition,
    const TemplateBindings &Bindings) {
    std::string Key;
    for (const auto &Param : Definition.Decl->getParams()) {
        auto It = Bindings.find(Param.Name);
        if (It == Bindings.end()) return nullptr;
        for (const auto &Type : It->second) {
            if (!Key.empty()) Key += ',';
            Key += Type;
        }
    }
    for (const auto &Instance : TemplateInstances)
        if (Instance.Decl == Definition.Decl && sameIdentifier(Instance.Arguments, Key)) return Instance.Signature;
    if (TemplateDepth >= 64) {
        reportError("Template instantiation depth exceeds 64");
        return nullptr;
    }
    size_t Requester = VisibilityUnit.value_or(CurrentUnit);
    size_t SavedUnit = CurrentUnit;
    scope_exit RestoreUnit([&] { selectUnit(SavedUnit); });
    SaveAndRestore<std::optional<size_t>> Visibility(VisibilityUnit, Requester);
    selectUnit(Definition.OwnerUnit);
    SaveAndRestore<int> Location(DiagnosticLine, Definition.Decl->getLine());
    TemplateClone Clone(Bindings, [this](int Line, const std::string &Message) {
        SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
        reportError("%s", Message.c_str());
    });
    std::string Name = Definition.Decl->getName() + "<" + Key + ">";
    auto Function = Clone.function(Definition.Decl->getFunction(), Name);
    if (!Function || !FuncGen->emitPrototype(Function->getProto())) return nullptr;
    auto Signatures = FuncGen->getOverloads(Name);
    const FuncSig *Signature = nullptr;
    for (const auto *Sig : Signatures)
        if (Sig->Prototype == Function->getProto()) Signature = Sig;
    TemplateInstances.push_back({Definition.Decl, Key, std::move(Function), Signature,
        Definition.OwnerUnit, Requester, TemplateDepth + 1});
    return Signature;
}

void CodeGen::drainTemplateBodies() {
    size_t SavedUnit = CurrentUnit;
    scope_exit RestoreUnit([&] { selectUnit(SavedUnit); });
    while (NextTemplateBody < TemplateInstances.size()) {
        // Emission may append instances, so keep no vector element references.
        const auto &Instance = TemplateInstances[NextTemplateBody++];
        auto *Function = Instance.Function.get();
        auto *Declaration = Instance.Decl;
        size_t Owner = Instance.OwnerUnit;
        size_t Requester = Instance.RequesterUnit;
        unsigned Depth = Instance.Depth;
        SaveAndRestore<std::optional<size_t>> Visibility(VisibilityUnit, Requester);
        SaveAndRestore<unsigned> InstantiationDepth(TemplateDepth, Depth);
        SaveAndRestore<IdentifierMap<SymbolInfo>> SymbolContext(Symbols, TemplateSymbols.at(Declaration));
        selectUnit(Owner);
        emitStmt(Function);
    }
}
