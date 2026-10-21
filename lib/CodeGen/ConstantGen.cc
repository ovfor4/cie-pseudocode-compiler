#include "cps/Diagnostics.h"
#include "cps/CodeGen.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

// Only bindings introduced by constants or hiding constants are restored.
// Ordinary variables keep the compiler's existing function-wide visibility.
void CodeGen::emitScopedBlock(const std::vector<std::unique_ptr<StmtAST>> &Body) {
    ConstantScopes.push_back({NextScopeID++, {}});
    for (const auto &S : Body) emitStmt(S.get());
    for (const auto &[Name, Previous] : ConstantScopes.back().Saved) {
        if (Previous) Symbols[Name] = *Previous;
        else Symbols.erase(Name);
    }
    ConstantScopes.pop_back();
}

void CodeGen::warnMemberShadow(const std::string &Name, int Line) const {
    if (CurrentClass.empty()) return;
    const TypeInfo *Class = resolveType(CurrentClass);
    const RecordPayload *Record = Class ? Class->asRecord() : nullptr;
    if (Record && Record->findField(Name)) {
        cps::sourceDiagnostic(Line, "warning: Declaration of %s shadows a field of class %s at line %d\n",
                Name.c_str(), CurrentClass.c_str(), Line);
    }
}

bool CodeGen::prepareSymbolDeclaration(const std::string &Name, bool IsConstant) {
    if (!CurrentClass.empty() && sameIdentifier(Name, "this")) {
        reportError("Cannot redeclare this inside a method");
        return false;
    }
    auto &Scope = ConstantScopes.back();
    auto It = Symbols.find(Name);
    if (It != Symbols.end()) {
        if (It->second.ScopeID == Scope.ID && !It->second.IsGlobal) {
            reportError("Declaration of %s conflicts with an existing declaration in this scope", Name.c_str());
            return false;
        }
        if (IsConstant || It->second.isConstant() || It->second.IsGlobal) Scope.Saved.emplace(Name, It->second);
    } else if (IsConstant) {
        Scope.Saved.emplace(Name, std::nullopt);
    }
    warnMemberShadow(Name, DiagnosticLine);
    return true;
}

bool CodeGen::requireWritable(const SymbolInfo &Info, const std::string &Name) {
    if (!Info.isConstant()) return true;
    reportError("Constant %s is not a writable location", Name.c_str());
    return false;
}

void CodeGen::emitConstantStmt(ConstantStmtAST *Stmt) {
    const std::string &Name = Stmt->getName();
    if (Types->lookupEnumConstant(Name) || Types->resolve(Name)) {
        reportError("Constant %s conflicts with an enum value or type name", Name.c_str());
        return;
    }
    if (ConstantScopes.back().ID == 0 &&
        (hasCallable(Name) || isBuiltinName(Name))) {
        reportError("Constant %s conflicts with a function name", Name.c_str());
        return;
    }
    if (!prepareSymbolDeclaration(Name, true)) return;
    auto Existing = StringConstants.find(Stmt);
    if (Existing != StringConstants.end()) {
        Symbols[Name] = Existing->second;
        return;
    }
    const TypeInfo *Type = getExprTypeInfo(Stmt->getValue());
    Value *Val = emitExpr(Stmt->getValue());
    if (Type && Type->isString() && Val) {
        Value *Storage;
        bool TopLevel = ConstantScopes.back().ID == 0;
        if (TopLevel) {
            Storage = new GlobalVariable(*TheModule, Type->LLVMType, false,
                GlobalValue::InternalLinkage, Types->getZeroValue(Type->Name), "__cps_constant." + Name);
        } else {
            Storage = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(), Type->LLVMType, Name);
        }
        size_t Lifetime = registerStackObject(Storage, Type->Name, DiagnosticLine);
        Builder->CreateStore(Val, Storage);
        activateStackObject(Lifetime);
        SymbolInfo Info{Storage, Type->Name, false, nullptr, ConstantScopes.back().ID};
        Info.ReadOnly = true;
        Symbols[Name] = Info;
        if (TopLevel) StringConstants.emplace(Stmt, Info);
        return;
    }
    auto *Literal = dyn_cast_or_null<Constant>(Val);
    if (!Type || !Literal) {
        reportError("CONSTANT initializer must be a literal");
        return;
    }
    Symbols[Name] = {nullptr, Type->Name, false, Literal, ConstantScopes.back().ID};
}
