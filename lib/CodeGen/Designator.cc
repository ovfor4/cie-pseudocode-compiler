// Type-name-level emission helpers for the user-defined TYPE system:
// the type pre-pass, the admission gate for assignments/arguments, and the
// binary-operator interception for Enum/Record/Pointer operands.
// These are CodeGen methods; the file is split out to keep the hub
// dispatch-only.
#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/TypeAST.h"
#include "cps/Lexer.h"

using namespace llvm;
using namespace cps;

namespace {

// Operator spelling for diagnostics (raw ASCII or a keyword token).
std::string opToText(int Op) {
    switch (Op) {
    case tok_eq:  return "=";
    case tok_ne:  return "<>";
    case tok_le:  return "<=";
    case tok_ge:  return ">=";
    case tok_and: return "AND";
    case tok_or:  return "OR";
    case tok_div: return "DIV";
    case tok_mod: return "MOD";
    default:
        if (Op >= 0) return std::string(1, static_cast<char>(Op));
        return "?";
    }
}

} // namespace

// Registers every top-level TYPE declaration before prototypes are emitted.
// Two passes so a pointer TYPE may reference a type declared later
// (TYPE NodePtr = ^Node before TYPE Node — required for linked structures);
// by-value record fields still require their record type to be complete,
// which pass B's source order enforces.
void CodeGen::runTypePrePass(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    std::string Err;

    for (const auto &S : Statements) {
        bool Ok = true;
        if (auto *E = dynamic_cast<EnumTypeDeclAST*>(S.get()))
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
            std::vector<std::pair<std::string, std::string>> Fields;
            for (const RecordFieldDecl &F : R->getFields())
                Fields.emplace_back(F.Name, F.TypeName);
            Ok = Types->defineRecord(R->getName(), Fields, Err);
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

// The single name-level admission gate. Targets of a user kind demand exact
// type-name equality (their representations are indistinguishable at the
// LLVM level); builtin targets additionally reject user-kind sources, then
// keep the existing representation coercions unchanged.
Value *CodeGen::emitCoercedExpr(ExprAST *E, const TypeInfo *Target) {
    if (!E || !Target) return nullptr;

    const TypeInfo *Src = getExprTypeInfo(E);

    if (Target->isUserKind()) {
        // ^x into a pointer target is structural: the pointee type name must
        // match the pointer type's declared pointee exactly (strict — the
        // guide's own ^INTEGER-at-a-Season example is rejected by design).
        if (auto *Addr = dynamic_cast<AddrOfExprAST*>(E)) {
            const PointerPayload *PP = Target->asPointer();
            if (!PP) {
                reportError("'^' (address-of) can only be used where a pointer type is expected (got %s)",
                            Target->Name.c_str());
                return nullptr;
            }
            LValueInfo LV;
            if (!emitLValue(Addr->getTarget(), LV)) return nullptr;
            if (LV.TypeName != PP->PointeeTypeName) {
                reportError("%s is a pointer to %s and cannot point at a value of type %s",
                            Target->Name.c_str(), PP->PointeeTypeName.c_str(), LV.TypeName.c_str());
                return nullptr;
            }
            return LV.Addr;
        }
        if (!Src) {
            // Emit anyway: an inadmissible sub-expression (e.g. enum * 2)
            // then reports its own, more specific diagnostic.
            if (emitExpr(E)) {
                reportError("Cannot determine the type of the value used where %s is expected",
                            Target->Name.c_str());
            }
            return nullptr;
        }
        if (Src->Name != Target->Name) {
            reportError("Cannot use a value of type %s where %s is expected",
                        Src->Name.c_str(), Target->Name.c_str());
            return nullptr;
        }
        return emitExpr(E); // same nominal type: no conversion needed
    }

    if (Src && Src->isUserKind()) {
        reportError("Cannot use a value of type %s where %s is expected",
                    Src->Name.c_str(), Target->Name.c_str());
        return nullptr;
    }
    return coerceValueToType(emitExpr(E), Target);
}

// Binary operators with at least one Enum/Record/Pointer operand. Decides
// admissibility by type name and emits directly; ArithmeticHandler stays
// ignorant of user kinds.
Value *CodeGen::emitUserKindBinaryOp(BinaryExprAST *Bin, const TypeInfo *LT, const TypeInfo *RT) {
    int Op = Bin->getOp();
    const char *LName = LT ? LT->Name.c_str() : "?";
    const char *RName = RT ? RT->Name.c_str() : "?";

    if ((LT && LT->isRecord()) || (RT && RT->isRecord())) {
        reportError("Operator %s cannot be applied to a record value (type %s)",
                    opToText(Op).c_str(), (LT && LT->isRecord()) ? LName : RName);
        return nullptr;
    }

    if ((LT && LT->isPointer()) || (RT && RT->isPointer())) {
        bool SameName = LT && RT && LT->isPointer() && RT->isPointer() && LT->Name == RT->Name;
        if (SameName && (Op == tok_eq || Op == tok_ne)) {
            Value *L = emitExpr(Bin->getLHS());
            Value *R = emitExpr(Bin->getRHS());
            if (!L || !R) return nullptr;
            return Op == tok_eq ? Builder->CreateICmpEQ(L, R, "ptr_eq")
                                : Builder->CreateICmpNE(L, R, "ptr_ne");
        }
        reportError("Pointer values support only = and <> between the same pointer type (got %s %s %s)",
                    LName, opToText(Op).c_str(), RName);
        return nullptr;
    }

    // At least one enum operand from here on.
    bool BothSameEnum = LT && RT && LT->isEnum() && RT->isEnum() && LT->Name == RT->Name;

    switch (Op) {
    case tok_eq: case tok_ne: case '<': case '>': case tok_le: case tok_ge: {
        if (!BothSameEnum) break;
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        if (!L || !R) return nullptr;
        switch (Op) {
        case tok_eq: return Builder->CreateICmpEQ(L, R, "enum_eq");
        case tok_ne: return Builder->CreateICmpNE(L, R, "enum_ne");
        case '<':    return Builder->CreateICmpSLT(L, R, "enum_lt");
        case '>':    return Builder->CreateICmpSGT(L, R, "enum_gt");
        case tok_le: return Builder->CreateICmpSLE(L, R, "enum_le");
        case tok_ge: return Builder->CreateICmpSGE(L, R, "enum_ge");
        }
        return nullptr;
    }

    case '+': case '-': {
        // The one legal arithmetic shape (guide §4.2): enum on the left,
        // INTEGER on the right, result checked against the enum's range.
        if (!(LT && LT->isEnum() && RT && RT->Kind == TypeKind::Integer)) break;
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        if (!L || !R) return nullptr;
        Value *Res = Op == '+' ? Builder->CreateAdd(L, R, "enum_step")
                               : Builder->CreateSub(L, R, "enum_step");
        RuntimeChecker->emitEnumRangeCheck(Res, LT->asEnum()->ValueNames.size(),
                                           LT->Name, Bin->getLine());
        return Res;
    }

    default:
        break;
    }

    reportError("Operator %s cannot be applied to %s and %s "
                "(enums allow comparison with the same enum type, and enum + or - INTEGER)",
                opToText(Op).c_str(), LName, RName);
    return nullptr;
}
