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

Value *CodeGen::emitCBinaryOp(int Op, Value *L, const TypeInfo *Left,
                               Value *R, const TypeInfo *Right, int Line) {
    if (!Left || !Right || !Left->isC() || !Right->isC()) {
        reportError("C and pseudocode operands require an explicit cast (line %d)", Line);
        return nullptr;
    }
    if (Left->isPointer() || Right->isPointer()) {
        if (Left->Kind != TypeKind::CPointer || Right->Kind != TypeKind::CPointer ||
            !Left->sameType(Right) || (Op != tok_eq && Op != tok_ne)) {
            reportError("C pointers support only = and <> between the same C pointer type");
            return nullptr;
        }
        return Op == tok_eq ? Builder->CreateICmpEQ(L, R, "c_ptr_eq")
                            : Builder->CreateICmpNE(L, R, "c_ptr_ne");
    }
    const TypeInfo *Common = Types->commonCScalar(Left, Right);
    if (!Common) {
        reportError("C arithmetic requires scalar operands");
        return nullptr;
    }
    if (Op == '&' || ((Op == tok_div || Op == tok_mod) && !Common->isIntegral())) {
        reportError("Operator is not supported for these C operand types (line %d)", Line);
        return nullptr;
    }
    if (Op == tok_and || Op == tok_or) {
        L = convertNumericValue(L, Left, resolveType("BOOLEAN"));
        R = convertNumericValue(R, Right, resolveType("BOOLEAN"));
        return Op == tok_and ? Builder->CreateAnd(L, R, "c_and")
                             : Builder->CreateOr(L, R, "c_or");
    }
    // '/' is a pseudocode operator even with C operands: real division,
    // yielding the existing REAL type rather than a native integer quotient.
    if (Op == '/') {
        L = convertNumericValue(L, Left, resolveType("REAL"));
        R = convertNumericValue(R, Right, resolveType("REAL"));
        RuntimeChecker->emitDivZeroCheck(R, Line);
        return Builder->CreateFDiv(L, R, "c_real_div");
    }
    L = convertNumericValue(L, Left, Common);
    R = convertNumericValue(R, Right, Common);
    if (Common->isReal()) {
        switch (Op) {
        case '+': return Builder->CreateFAdd(L, R, "c_add");
        case '-': return Builder->CreateFSub(L, R, "c_sub");
        case '*': return Builder->CreateFMul(L, R, "c_mul");
        case tok_eq: return Builder->CreateFCmpOEQ(L, R, "c_eq");
        case tok_ne: return Builder->CreateFCmpUNE(L, R, "c_ne");
        case '<': return Builder->CreateFCmpOLT(L, R, "c_lt");
        case '>': return Builder->CreateFCmpOGT(L, R, "c_gt");
        case tok_le: return Builder->CreateFCmpOLE(L, R, "c_le");
        case tok_ge: return Builder->CreateFCmpOGE(L, R, "c_ge");
        }
    } else {
        switch (Op) {
        case '+': return Builder->CreateAdd(L, R, "c_add");
        case '-': return Builder->CreateSub(L, R, "c_sub");
        case '*': return Builder->CreateMul(L, R, "c_mul");
        case tok_div: case tok_mod:
            RuntimeChecker->emitDivZeroCheck(R, Line);
            if (Op == tok_div)
                return Common->CSigned ? Builder->CreateSDiv(L, R, "c_div")
                                       : Builder->CreateUDiv(L, R, "c_div");
            return Common->CSigned ? Builder->CreateSRem(L, R, "c_mod")
                                   : Builder->CreateURem(L, R, "c_mod");
        case tok_eq: return Builder->CreateICmpEQ(L, R, "c_eq");
        case tok_ne: return Builder->CreateICmpNE(L, R, "c_ne");
        case '<': return Common->CSigned ? Builder->CreateICmpSLT(L, R, "c_lt")
                                        : Builder->CreateICmpULT(L, R, "c_lt");
        case '>': return Common->CSigned ? Builder->CreateICmpSGT(L, R, "c_gt")
                                        : Builder->CreateICmpUGT(L, R, "c_gt");
        case tok_le: return Common->CSigned ? Builder->CreateICmpSLE(L, R, "c_le")
                                          : Builder->CreateICmpULE(L, R, "c_le");
        case tok_ge: return Common->CSigned ? Builder->CreateICmpSGE(L, R, "c_ge")
                                          : Builder->CreateICmpUGE(L, R, "c_ge");
        }
    }
    reportError("Unsupported operator for C operands (line %d)", Line);
    return nullptr;
}

void CodeGen::emitCOutput(Value *Val, const TypeInfo *Info, bool AppendNewline) {
    if (Info->isVoid()) {
        reportError("Cannot OUTPUT a void value");
        return;
    }
    std::string Format = Info->COutputFormat;
    if (Info->isBoolean()) {
        Format = "%s";
        Val = Builder->CreateSelect(Val, TrueStr, FalseStr);
    } else if (Info->CIdentity == "float") {
        Val = convertNumericValue(Val, Info, resolveType("C_DOUBLE"));
    } else if (Info->isIntegral()) {
        Val = convertNumericValue(Val, Info, Types->promoteCScalar(Info));
    }
    if (AppendNewline) Format += '\n';
    Value *Fmt = Builder->CreateGlobalString(Format, "fmt_c_output", 0, TheModule.get());
    Builder->CreateCall(PrintfFunc, {Fmt, Val});
}

void CodeGen::emitCInput(const SymbolInfo *Symbol, const TypeInfo *Info) {
    if (Info->Kind != TypeKind::CScalar || Info->isVoid()) {
        reportError("INPUT is not supported for C pointer or void type %s", Info->Name.c_str());
        return;
    }
    if (Info->isBoolean()) {
        const TypeInfo *NativeInt = resolveType("C_INT");
        AllocaInst *Temp = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(),
                                                 NativeInt->LLVMType, "c_bool_input");
        Builder->CreateStore(Types->getZeroValue("C_INT"), Temp);
        Value *Fmt = Builder->CreateGlobalString("%d", "fmt_c_input_bool", 0, TheModule.get());
        Builder->CreateCall(ScanfFunc, {Fmt, Temp});
        Value *Input = Builder->CreateLoad(NativeInt->LLVMType, Temp);
        Value *Bool = convertNumericValue(Input, NativeInt, Info);
        Builder->CreateStore(Bool, Symbol->Storage);
        return;
    }
    Value *Fmt = Builder->CreateGlobalString(Info->CInputFormat, "fmt_c_input", 0, TheModule.get());
    Builder->CreateCall(ScanfFunc, {Fmt, Symbol->Storage});
}
