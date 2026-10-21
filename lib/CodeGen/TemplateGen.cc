#include "cps/CodeGen.h"
#include "cps/TemplateAST.h"
#include "TemplateClone.h"
#include "llvm/ADT/ScopeExit.h"

using namespace cps;
using namespace cps::detail;
using namespace llvm;

namespace {
// Source template lists are nonempty; substitution may produce Empty<>.
bool splitTemplateType(const std::string &Name, std::string &Base,
                       std::vector<std::string> &Arguments) {
    size_t Open = Name.find('<');
    Base = Name.substr(0, Open);
    if (Open == std::string::npos) return true;
    if (Name.empty() || Name.back() != '>') return false;
    size_t Start = Open + 1;
    unsigned Depth = 0;
    for (size_t I = Start; I + 1 < Name.size(); ++I) {
        if (Name[I] == '<') ++Depth;
        if (Name[I] == '>') {
            if (!Depth) return false;
            --Depth;
        }
        if (!Depth && Name[I] == ',') {
            if (I == Start) return false;
            Arguments.push_back(Name.substr(Start, I - Start));
            Start = I + 1;
        }
    }
    if (Depth) return false;
    if (Start + 1 < Name.size()) Arguments.push_back(Name.substr(Start, Name.size() - Start - 1));
    else if (Start != Open + 1) return false;
    return true;
}
}

std::optional<std::string> CodeGen::canonicalTemplateArgument(const std::string &Name) {
    SaveAndRestore<bool> Speculation(SpeculativeTemplateResolution, true);
    if (const TypeInfo *Type = resolveType(Name))
        return Type->isVoid() ? std::nullopt : std::optional<std::string>(Type->Name);
    if (FailedTemplateTypes.count(Name)) return std::nullopt;
    std::string Base;
    std::vector<std::string> Arguments;
    if (!splitTemplateType(Name, Base, Arguments) || Name.find('<') == std::string::npos)
        return std::nullopt;
    auto Definitions = Templates.find(Base);
    if (Definitions == Templates.end()) return std::nullopt;
    for (const auto &Definition : Definitions->second) {
        if (!Definition.Decl->getClass() || !isVisible(Definition.OwnerUnit)) continue;
        size_t Fixed = Definition.Decl->getParams().size() - Definition.Decl->isVariadic();
        if (Arguments.size() < Fixed || (!Definition.Decl->isVariadic() && Arguments.size() != Fixed))
            return std::nullopt;
        std::string Canonical = Definition.Decl->getName() + "<";
        for (size_t I = 0; I < Arguments.size(); ++I) {
            auto Argument = canonicalTemplateArgument(Arguments[I]);
            if (!Argument) return std::nullopt;
            if (I) Canonical += ',';
            Canonical += *Argument;
        }
        return Canonical + ">";
    }
    return std::nullopt;
}

const TypeInfo *CodeGen::instantiateClass(const std::string &Name) {
    std::string Base;
    std::vector<std::string> Arguments;
    bool Valid = splitTemplateType(Name, Base, Arguments);
    auto Definitions = Templates.find(Base);
    if (Definitions == Templates.end()) return nullptr;
    const TemplateDefinition *Definition = nullptr;
    for (const auto &Candidate : Definitions->second)
        if (Candidate.Decl->getClass() && isVisible(Candidate.OwnerUnit)) {
            Definition = &Candidate;
            break;
        }
    if (!Definition || FailedTemplateTypes.count(Name)) return nullptr;
    auto Fail = [&](const std::string &Message) -> const TypeInfo * {
        if (!SpeculativeTemplateResolution && FailedTemplateTypes.insert(Name).second)
            reportError("%s", Message.c_str());
        return nullptr;
    };
    if (Name.find('<') == std::string::npos)
        return Fail("Class template " + Base + " requires template arguments");
    if (!Valid) return Fail("Invalid class template type " + Name);
    const auto &Params = Definition->Decl->getParams();
    size_t Fixed = Params.size() - Definition->Decl->isVariadic();
    if (Arguments.size() < Fixed || (!Definition->Decl->isVariadic() && Arguments.size() != Fixed))
        return Fail("Wrong number of template arguments for " + Base);
    TemplateBindings Bindings;
    for (const auto &Param : Params) Bindings.emplace(Param.Name, std::vector<std::string>{});
    std::string Key;
    for (size_t I = 0; I < Arguments.size(); ++I) {
        const TypeInfo *Argument = resolveType(Arguments[I]);
        if (!Argument || Argument->isVoid()) return Fail("Unknown or invalid template argument " + Arguments[I] + " for " + Base);
        if (!Key.empty()) Key += ',';
        Key += Argument->Name;
        Bindings.at(Params[std::min(I, Fixed)].Name).push_back(Argument->Name);
    }
    std::string InstanceName = Definition->Decl->getName() + "<" + Key + ">";
    if (FailedTemplateTypes.count(InstanceName)) return nullptr;
    for (const auto &Instance : TemplateInstances)
        if (Instance.Class && Instance.Decl == Definition->Decl && sameIdentifier(Instance.Arguments, Key))
            return Types->resolve(InstanceName);
    // Candidate matching must neither emit diagnostics nor reserve failed types.
    // Its actual class type is already available, so an exact match reuses it above.
    if (SpeculativeTemplateResolution) return nullptr;
    if (TemplateDepth >= 64) return Fail("Template instantiation depth exceeds 64");

    size_t Requester = VisibilityUnit.value_or(CurrentUnit);
    size_t SavedUnit = CurrentUnit;
    scope_exit RestoreUnit([&] { selectUnit(SavedUnit); });
    SaveAndRestore<std::optional<size_t>> Visibility(VisibilityUnit, Requester);
    SaveAndRestore<unsigned> Depth(TemplateDepth, TemplateDepth + 1);
    selectUnit(Definition->OwnerUnit);
    SaveAndRestore<int> Location(DiagnosticLine, Definition->Decl->getLine());
    TemplateClone Clone(Bindings, [this](int Line, const std::string &Message) {
        SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
        reportError("%s", Message.c_str());
    });
    auto Class = Clone.klass(Definition->Decl->getClass(), InstanceName);
    if (!Class) {
        FailedTemplateTypes.insert(Name);
        return nullptr;
    }
    std::string Error;
    if (!Types->declareUserType(InstanceName, TypeKind::Record, Error, true)) return Fail(Error);
    TypeOwners[InstanceName] = Definition->OwnerUnit;
    // Reserve and cache before fields resolve: recursion observes an opaque record
    // and defineRecord diagnoses recursive by-value containment.
    auto *ClonedClass = Class.get();
    size_t Index = TemplateInstances.size();
    TemplateInstances.push_back({Definition->Decl, Key, nullptr, std::move(Class), nullptr,
        Definition->OwnerUnit, Requester, TemplateDepth});
    if (!Types->defineRecord(InstanceName, ClonedClass->getFields(), Error)) {
        Fail(Error);
        FailedTemplateTypes.insert(InstanceName);
        return nullptr;
    }
    registerClassMethods(ClonedClass);
    if (TypePrePass) DeferredClassPrototypes.push_back(Index);
    else emitClassPrototypes(ClonedClass);
    return Types->resolve(InstanceName);
}

void CodeGen::emitDeferredClassPrototypes() {
    size_t SavedUnit = CurrentUnit;
    scope_exit RestoreUnit([&] { selectUnit(SavedUnit); });
    for (size_t Index : DeferredClassPrototypes) {
        const auto &Instance = TemplateInstances[Index];
        auto *Class = Instance.Class.get();
        size_t Requester = Instance.RequesterUnit;
        size_t Owner = Instance.OwnerUnit;
        unsigned Depth = Instance.Depth;
        SaveAndRestore<std::optional<size_t>> Visibility(VisibilityUnit, Requester);
        SaveAndRestore<unsigned> InstantiationDepth(TemplateDepth, Depth);
        selectUnit(Owner);
        emitClassPrototypes(Class);
    }
    DeferredClassPrototypes.clear();
}

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
    TemplateInstances.push_back({Definition.Decl, Key, std::move(Function), nullptr, Signature,
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
        auto *Class = Instance.Class.get();
        auto *Declaration = Instance.Decl;
        size_t Owner = Instance.OwnerUnit;
        size_t Requester = Instance.RequesterUnit;
        unsigned Depth = Instance.Depth;
        SaveAndRestore<std::optional<size_t>> Visibility(VisibilityUnit, Requester);
        SaveAndRestore<unsigned> InstantiationDepth(TemplateDepth, Depth);
        SaveAndRestore<IdentifierMap<SymbolInfo>> SymbolContext(Symbols, TemplateSymbols.at(Declaration));
        selectUnit(Owner);
        if (Class && !FailedTemplateTypes.count(Class->getName())) emitStmt(Class);
        if (Function) emitStmt(Function);
    }
}
