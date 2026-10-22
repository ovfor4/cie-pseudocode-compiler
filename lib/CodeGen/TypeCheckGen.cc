// Type admission and binary operators for user-defined types.
#include "cps/CodeGen.h"
#include "cps/TypeAST.h"
#include "cps/Lexer.h"
#include "cps/Operator.h"

using namespace llvm;
using namespace cps;

// The single name-level admission gate. Targets of a user kind demand exact
// type-name equality (their representations are indistinguishable at the
// LLVM level); builtin targets additionally reject user-kind sources, then
// keep the existing representation coercions unchanged.
Value *CodeGen::emitCoercedExpr(ExprAST *E, const TypeInfo *Target) {
    SaveAndRestore<int> Location(DiagnosticLine, E && E->getLine() ? E->getLine() : DiagnosticLine);
    if (!E || !Target) return nullptr;

    // A whole-array reference evaluates to its raw data pointer, and
    // getExprTypeInfo reports the ELEMENT type for it — both would slip
    // through the name check and smash the pointer into the target.
    if (isWholeArrayVar(E)) {
        reportError("A whole array cannot be used as a value of type %s; index it",
                    Target->Name.c_str());
        return nullptr;
    }

    const TypeInfo *Src = getExprTypeInfo(E);
    if (Src && Src->isString() && Target->Kind == TypeKind::Boolean) {
        reportError("Cannot implicitly convert STRING to BOOLEAN");
        return nullptr;
    }

    // CHAR and STRING convert into one another; a STRING that is not exactly
    // one character long is a runtime error.
    if (Src && Src->Kind == TypeKind::Char && Target->isString())
        return emitStringFromChar(emitExpr(E));
    if (Src && Src->isString() && Target->Kind == TypeKind::Char)
        return emitCharFromString(E, E->getLine() ? E->getLine() : DiagnosticLine);

    if (Target->isC() || (Src && Src->isC())) {
        if (auto *Addr = dynamic_cast<AddrOfExprAST*>(E)) {
            const PointerPayload *PP = Target->asPointer();
            LValueInfo LV;
            if (PP && emitLValue(Addr->getTarget(), LV)) {
                if (Types->sameType(LV.TypeName, PP->PointeeTypeName)) return LV.Addr;
            }
            reportError("Address does not match C pointer type %s", Target->Name.c_str());
            return nullptr;
        }
        if (Src && Src->sameType(Target) && !Target->isVoid()) return emitExpr(E);
        if (Src && Src->Kind == TypeKind::CScalar && Target->Kind == TypeKind::CScalar &&
            !Src->isVoid() && !Target->isVoid())
            return convertNumericValue(emitExpr(E), Src, Target);
        reportError("Cannot implicitly convert %s to %s; use an explicit cast",
                    Src ? Src->Name.c_str() : "expression", Target->Name.c_str());
        return nullptr;
    }

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
            if (!Types->sameType(LV.TypeName, PP->PointeeTypeName)) {
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
        if (!Src->sameType(Target)) {
            reportError("Cannot use a value of type %s where %s is expected",
                        Src->Name.c_str(), Target->Name.c_str());
            return nullptr;
        }
        return emitExpr(E); // same nominal type: no conversion needed
    }

    // ^x is a raw address: admissible ONLY where a pointer type is expected.
    // Without this check the ptr would coerce into STRING/BOOLEAN silently.
    if (dynamic_cast<AddrOfExprAST*>(E)) {
        reportError("'^' (address-of) can only be used where a pointer type is expected (got %s)",
                    Target->Name.c_str());
        return nullptr;
    }

    if (Src && Src->isUserKind()) {
        reportError("Cannot use a value of type %s where %s is expected",
                    Src->Name.c_str(), Target->Name.c_str());
        return nullptr;
    }
    // Builtin scalars never convert into one another implicitly, except
    // INTEGER -> REAL: BOOLEAN and CHAR are not numbers (use ASC / CHR).
    auto IsPlain = [](const TypeInfo *T) {
        return T->Kind == TypeKind::Integer || T->Kind == TypeKind::Real ||
               T->Kind == TypeKind::Boolean || T->Kind == TypeKind::Char;
    };
    if (Src && IsPlain(Target) && IsPlain(Src)) {
        bool Allowed = Src->Kind == Target->Kind ||
                       (Src->Kind == TypeKind::Integer && Target->Kind == TypeKind::Real);
        if (!Allowed) {
            reportError("Cannot use a value of type %s where %s is expected%s",
                        Src->Name.c_str(), Target->Name.c_str(),
                        Src->Kind == TypeKind::Real && Target->Kind == TypeKind::Integer
                            ? " (use INT(...) to convert)" : "");
            return nullptr;
        }
    }
    return coerceValueToType(emitExpr(E), Target);
}

// Binary operators with at least one Enum/Record/Pointer operand. Decides
// admissibility by type name and emits directly; ArithmeticHandler stays
// ignorant of user kinds.
Value *CodeGen::emitUserKindBinaryOp(int Op, Value *L, const TypeInfo *LT,
                                      Value *R, const TypeInfo *RT, int Line) {
    const char *LName = LT ? LT->Name.c_str() : "?";
    const char *RName = RT ? RT->Name.c_str() : "?";

    if ((LT && LT->isRecord()) || (RT && RT->isRecord())) {
        reportError("Operator %s cannot be applied to %s and %s",
                    operatorSpelling(Op).c_str(), LName, RName);
        return nullptr;
    }

    if ((LT && LT->isPointer()) || (RT && RT->isPointer())) {
        bool SameName = LT && RT && LT->isPointer() && RT->isPointer() && LT->sameType(RT);
        if (SameName && (Op == tok_eq || Op == tok_ne)) {
            return Op == tok_eq ? Builder->CreateICmpEQ(L, R, "ptr_eq")
                                : Builder->CreateICmpNE(L, R, "ptr_ne");
        }
        reportError("Pointer values support only = and <> between the same pointer type (got %s %s %s)",
                    LName, operatorSpelling(Op).c_str(), RName);
        return nullptr;
    }

    // At least one enum operand from here on.
    bool BothSameEnum = LT && RT && LT->isEnum() && RT->isEnum() && LT->sameType(RT);

    switch (Op) {
    case tok_eq: case tok_ne: case '<': case '>': case tok_le: case tok_ge: {
        if (!BothSameEnum) break;
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
        Value *Res = Op == '+' ? Builder->CreateAdd(L, R, "enum_step")
                               : Builder->CreateSub(L, R, "enum_step");
        RuntimeChecker->emitEnumRangeCheck(Res, LT->asEnum()->ValueNames.size(),
                                           LT->Name, Line);
        return Res;
    }

    default:
        break;
    }

    reportError("Operator %s cannot be applied to %s and %s "
                "(enums allow comparison with the same enum type, and enum + or - INTEGER)",
                operatorSpelling(Op).c_str(), LName, RName);
    return nullptr;
}
