// User-defined type registration and addressable designator access.
#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/TypeAST.h"

using namespace llvm;
using namespace cps;

// Registers every top-level TYPE declaration before prototypes are emitted.
// Two passes so a pointer TYPE may reference a type declared later
// (TYPE NodePtr = ^Node before TYPE Node — required for linked structures);
// by-value record fields still require their record type to be complete,
// which pass B's source order enforces.
void CodeGen::runTypePrePass(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    std::string Err;

    for (const auto &S : Statements) {
        SaveAndRestore<int> Location(DiagnosticLine, S->getLine());
        bool Ok = true;
        if (auto *C = dynamic_cast<CTypeDeclAST*>(S.get()))
            Ok = Types->defineCAlias(C->getName(), C->getExpression(), Err);
        else if (auto *E = dynamic_cast<EnumTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(E->getName(), TypeKind::Enum, Err);
        else if (auto *P = dynamic_cast<PointerTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(P->getName(), TypeKind::Pointer, Err);
        else if (auto *R = dynamic_cast<RecordTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(R->getName(), TypeKind::Record, Err,
                                        dynamic_cast<ClassTypeDeclAST*>(R) != nullptr);
        if (!Ok) reportError("%s", Err.c_str());
    }

    for (const auto &S : Statements) {
        SaveAndRestore<int> Location(DiagnosticLine, S->getLine());
        bool Ok = true;
        if (auto *E = dynamic_cast<EnumTypeDeclAST*>(S.get())) {
            Ok = Types->defineEnum(E->getName(), E->getValueNames(), Err);
        } else if (auto *P = dynamic_cast<PointerTypeDeclAST*>(S.get())) {
            Ok = Types->definePointer(P->getName(), P->getPointeeTypeName(), Err);
        } else if (auto *R = dynamic_cast<RecordTypeDeclAST*>(S.get())) {
            Ok = Types->defineRecord(R->getName(), R->getFields(), Err);
        }
        if (!Ok) reportError("%s", Err.c_str());
    }

    for (const auto &S : Statements) {
        auto *Class = dynamic_cast<ClassTypeDeclAST*>(S.get());
        if (!Class) continue;
        auto &Methods = ClassMethods[Class->getName()];
        IdentifierSet Names;
        for (const auto &Field : Class->getFields()) {
            Names.insert(Field.Name);
            if (sameIdentifier(Field.Name, "this")) {
                SaveAndRestore<int> Location(DiagnosticLine, Class->getLine());
                reportError("A class field cannot be named this");
            }
        }
        for (const auto &Method : Class->getMethods()) {
            SaveAndRestore<int> Location(DiagnosticLine, Method->getLine());
            const std::string &Name = Method->getProto()->getName();
            if (sameIdentifier(Name, "this")) {
                reportError("A method cannot be named this");
            } else if (!Names.insert(Name).second) {
                reportError("Duplicate member %s in class %s", Name.c_str(), Class->getName().c_str());
            } else {
                Methods.emplace(Name, Method.get());
            }
        }
    }
}

const PrototypeAST *CodeGen::findMethod(const std::string &Class, const std::string &Name) const {
    auto C = ClassMethods.find(Class);
    if (C == ClassMethods.end()) return nullptr;
    auto M = C->second.find(Name);
    return M == C->second.end() ? nullptr : M->second->getProto();
}

Value *CodeGen::emitMemberCall(MemberCallExprAST *Call) {
    SaveAndRestore<int> Location(DiagnosticLine, Call->getLine());
    LValueInfo Receiver;
    if (!emitLValue(Call->getReceiver(), Receiver)) return nullptr;
    return emitMethodCall(Receiver.TypeName, Receiver.Addr, Call->getMethod(),
                          Call->getArgs(), Call->getLine());
}

Value *CodeGen::emitMethodCall(const std::string &Class, Value *Receiver,
                                const std::string &Name,
                                const std::vector<std::unique_ptr<ExprAST>> &ArgExprs, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line);
    const PrototypeAST *Method = findMethod(Class, Name);
    if (!Method) {
        reportError("Type %s has no method %s", Class.c_str(), Name.c_str());
        return nullptr;
    }
    if (!Method->isPublic() && !sameIdentifier(CurrentClass, Class)) {
        reportError("Cannot access private method %s of type %s", Name.c_str(), Class.c_str());
        return nullptr;
    }
    std::vector<Value*> Args = {Receiver};
    if (!marshalCallArgs(Method->getLinkName(), ArgExprs, Line, Args)) return nullptr;
    return FuncGen->emitCall(Method->getLinkName(), Args);
}

// Resolves a designator (or legacy variable/array-access node) to the address
// of its storage plus the type name stored there. Array indexing goes through
// ArrayHandler's checked pipeline; dereferences emit the null-pointer check.
bool CodeGen::emitLValue(ExprAST *E, LValueInfo &Out) {
    SaveAndRestore<int> Location(DiagnosticLine, E && E->getLine() ? E->getLine() : DiagnosticLine);
    if (auto *Var = dynamic_cast<VariableExprAST*>(E)) {
        const SymbolInfo *Info = getSymbolInfo(Var->getName());
        if (!Info) {
            if (Types->lookupEnumConstant(Var->getName())) {
                reportError("Enum value %s is not an assignable location", Var->getName().c_str());
            } else {
                reportError("Unknown variable name %s", Var->getName().c_str());
            }
            return false;
        }
        if (!requireWritable(*Info, Var->getName())) return false;
        if (Info->IsArray) {
            reportError("Whole array %s is not an assignable location; index it", Var->getName().c_str());
            return false;
        }
        Out.Addr = symbolAddress(*Info);
        Out.TypeName = Info->TypeName;
        return true;
    }

    if (auto *Acc = dynamic_cast<ArrayAccessExprAST*>(E)) {
        Out.Addr = Arrays->emitElementAddress(Acc, *this, Out.TypeName);
        return Out.Addr != nullptr;
    }

    auto *D = dynamic_cast<DesignatorExprAST*>(E);
    if (!D) {
        reportError("Expression is not an assignable location");
        return false;
    }

    const SymbolInfo *Info = getSymbolInfo(D->getBaseName());
    if (!Info) {
        if (Types->lookupEnumConstant(D->getBaseName())) {
            reportError("Enum value %s is not an assignable location", D->getBaseName().c_str());
        } else {
            reportError("Unknown variable name %s", D->getBaseName().c_str());
        }
        return false;
    }

    if (!requireWritable(*Info, D->getBaseName())) return false;
    const auto &Accesses = D->getAccesses();
    Value *Addr = symbolAddress(*Info);
    std::string TypeName = Info->TypeName;
    size_t Start = 0;

    if (Info->IsArray) {
        if (Accesses.empty() || Accesses[0].Kind != AccessKind::Index) {
            reportError("Array %s must be indexed before '.' or '^' access", D->getBaseName().c_str());
            return false;
        }
        Addr = Arrays->emitElementAddress(D->getBaseName(), Accesses[0].Indices,
                                          D->getLine(), *this, TypeName);
        if (!Addr) return false;
        Start = 1;
    }

    for (size_t I = Start; I < Accesses.size(); ++I) {
        const DesignatorAccess &A = Accesses[I];
        const TypeInfo *Cur = resolveType(TypeName);
        if (!Cur) {
            reportError("Unknown type %s", TypeName.c_str());
            return false;
        }

        switch (A.Kind) {
        case AccessKind::Field: {
            const RecordPayload *RP = Cur->asRecord();
            if (!RP) {
                reportError("Type %s is not a record; '.%s' is invalid",
                            TypeName.c_str(), A.FieldName.c_str());
                return false;
            }
            const RecordFieldInfo *FI = RP->findField(A.FieldName);
            if (!FI) {
                reportError("Record type %s has no field %s", TypeName.c_str(), A.FieldName.c_str());
                return false;
            }
            if (!FI->IsPublic && !sameIdentifier(CurrentClass, TypeName)) {
                reportError("Cannot access private field %s of type %s",
                            A.FieldName.c_str(), TypeName.c_str());
                return false;
            }
            Addr = Builder->CreateStructGEP(Cur->LLVMType, Addr, FI->Index,
                                            TypeName + "." + A.FieldName);
            TypeName = FI->TypeName;
            break;
        }

        case AccessKind::Deref: {
            const PointerPayload *PP = Cur->asPointer();
            if (!PP) {
                reportError("Cannot dereference a value of type %s ('^' needs a pointer)",
                            TypeName.c_str());
                return false;
            }
            const TypeInfo *Pointee = resolveType(PP->PointeeTypeName);
            if (!Pointee || Pointee->isVoid()) {
                reportError("Cannot dereference a void pointer");
                return false;
            }
            Value *P = Builder->CreateLoad(PointerType::getUnqual(*TheContext), Addr, "deref_ptr");
            RuntimeChecker->emitNullDerefCheck(P, D->getLine());
            Addr = P;
            TypeName = PP->PointeeTypeName;
            break;
        }

        case AccessKind::Index:
            reportError("Cannot index a value of type %s (only declared arrays can be indexed)",
                        TypeName.c_str());
            return false;
        }
    }

    Out.Addr = Addr;
    Out.TypeName = TypeName;
    return true;
}

Value *CodeGen::loadFromLValue(const LValueInfo &LV) {
    const TypeInfo *Info = resolveType(LV.TypeName);
    if (!Info || !Info->LLVMType) {
        reportError("Unknown type %s", LV.TypeName.c_str());
        return nullptr;
    }
    Value *Val = Builder->CreateLoad(Info->LLVMType, LV.Addr);
    if (Info->isString()) {
        Val = emitStringNullGuard(Val);
    }
    return Val;
}
