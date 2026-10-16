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
        bool Ok = true;
        if (auto *C = dynamic_cast<CTypeDeclAST*>(S.get()))
            Ok = Types->defineCAlias(C->getName(), C->getExpression(), Err);
        else if (auto *E = dynamic_cast<EnumTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(E->getName(), TypeKind::Enum, Err);
        else if (auto *P = dynamic_cast<PointerTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(P->getName(), TypeKind::Pointer, Err);
        else if (auto *R = dynamic_cast<RecordTypeDeclAST*>(S.get()))
            Ok = Types->declareUserType(R->getName(), TypeKind::Record, Err);
        if (!Ok) reportError("%s", Err.c_str());
    }

    for (const auto &S : Statements) {
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
}

// Resolves a designator (or legacy variable/array-access node) to the address
// of its storage plus the type name stored there. Array indexing goes through
// ArrayHandler's checked pipeline; dereferences emit the null-pointer check.
bool CodeGen::emitLValue(ExprAST *E, LValueInfo &Out) {
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
        Out.Addr = Info->Storage;
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
    Value *Addr = Info->Storage;
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
            if (!FI->IsPublic) {
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
