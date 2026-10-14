#include "cps/CodeGen.h"
#include "cps/TypeAST.h"

using namespace llvm;
using namespace cps;

namespace {

bool isScalar(const TypeInfo *Info) {
    if (!Info || Info->isVoid()) return false;
    return Info->Kind == TypeKind::CScalar || Info->Kind == TypeKind::Integer ||
        Info->Kind == TypeKind::Real || Info->Kind == TypeKind::Boolean ||
        Info->Kind == TypeKind::Char;
}

bool isSigned(const TypeInfo *Info) {
    return Info->isC() ? Info->CSigned : Info->Kind == TypeKind::Integer;
}

} // namespace

// Source type identity is essential: native signed char and pseudocode CHAR
// can share i8 while requiring different extensions and FP conversions.
Value *CodeGen::convertNumericValue(Value *Val, const TypeInfo *Source, const TypeInfo *Target) {
    if (!Val || !isScalar(Source) || !isScalar(Target)) return nullptr;
    Type *From = Val->getType();
    Type *To = Target->LLVMType;
    if (From == To) return Val;
    if (To->isIntegerTy(1)) {
        if (From->isIntegerTy())
            return Builder->CreateICmpNE(Val, Constant::getNullValue(From), "cast_bool");
        return Builder->CreateFCmpUNE(Val, ConstantFP::get(From, 0.0), "cast_bool");
    }
    if (From->isIntegerTy() && To->isIntegerTy())
        return Builder->CreateIntCast(Val, To, isSigned(Source), "cast_int");
    if (From->isIntegerTy() && To->isFloatingPointTy())
        return isSigned(Source) ? Builder->CreateSIToFP(Val, To, "cast_fp")
                                : Builder->CreateUIToFP(Val, To, "cast_fp");
    if (From->isFloatingPointTy() && To->isIntegerTy())
        return isSigned(Target) ? Builder->CreateFPToSI(Val, To, "cast_int")
                                : Builder->CreateFPToUI(Val, To, "cast_int");
    return Builder->CreateFPCast(Val, To, "cast_fp");
}

Value *CodeGen::emitCast(CastExprAST *Cast) {
    const TypeInfo *Target = resolveType(Cast->getTypeName());
    ExprAST *Operand = Cast->getOperand();
    if (!Target || Target->isVoid()) {
        reportError("Invalid cast target %s", Cast->getTypeName().c_str());
        return nullptr;
    }
    if (isWholeArrayVar(Operand)) {
        reportError("Cannot cast a whole array; index it");
        return nullptr;
    }
    if (auto *Address = dynamic_cast<AddrOfExprAST*>(Operand)) {
        LValueInfo LV;
        if (!emitLValue(Address->getTarget(), LV)) return nullptr;
        const TypeInfo *Pointee = resolveType(LV.TypeName);
        if (Target->Kind == TypeKind::CPointer && Pointee && Pointee->isC()) return LV.Addr;
        reportError("Address casts require a C pointee and C pointer target");
        return nullptr;
    }
    const TypeInfo *Source = getExprTypeInfo(Operand);
    if (!Source || Source->isVoid() || Source->isUserKind() || Target->isUserKind()) {
        reportError("Unsupported operand or target for cast to %s", Target->Name.c_str());
        return nullptr;
    }
    if (isScalar(Source) && isScalar(Target))
        return convertNumericValue(emitExpr(Operand), Source, Target);
    if (Source->Kind == TypeKind::CPointer && Target->Kind == TypeKind::CPointer)
        return emitExpr(Operand);
    if (Source->isString() && Target->CIdentity == "char*") return emitExpr(Operand);
    if (Source->CIdentity == "char*" && Target->isString())
        return emitStringNullGuard(emitExpr(Operand));
    if (Source->isString() && Target->isString()) return emitExpr(Operand);
    reportError("Cannot cast %s to %s", Source->Name.c_str(), Target->Name.c_str());
    return nullptr;
}
