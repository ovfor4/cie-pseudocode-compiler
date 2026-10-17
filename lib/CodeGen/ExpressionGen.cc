#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileHandler.h"
#include "cps/MemoryHandler.h"
#include "cps/Lexer.h"
#include "cps/TypeAST.h"
#include "Builtins.h"
#include <climits>
#include <cstddef>

using namespace llvm;
using namespace cps;

using namespace cps::detail;

Value *CodeGen::emitExpr(ExprAST *Expr) {
    SaveAndRestore<int> Location(DiagnosticLine, Expr && Expr->getLine() ? Expr->getLine() : DiagnosticLine);
    if (auto *Cast = dynamic_cast<CastExprAST*>(Expr)) return emitCast(Cast);
    if (!Expr) return nullptr;

    if (auto *Num = dynamic_cast<IntegerExprAST*>(Expr)) {
        return ConstantInt::get(*TheContext, APInt(64, Num->getVal(), true));
    }

    if (auto *Real = dynamic_cast<RealExprAST*>(Expr)) {
        return ConstantFP::get(*TheContext, APFloat(Real->getVal()));
    }

    if (auto *Bool = dynamic_cast<BooleanExprAST*>(Expr)) {
        return ConstantInt::get(*TheContext, APInt(1, Bool->getVal() ? 1 : 0));
    }

    if (auto *Chr = dynamic_cast<CharExprAST*>(Expr)) {
        return ConstantInt::get(Type::getInt8Ty(*TheContext), static_cast<unsigned char>(Chr->getVal()));
    }

    if (auto *Str = dynamic_cast<StringExprAST*>(Expr)) {
        return StrHandler->createLiteral(Str->getVal());
    }

    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(Var->getName());
        if (!Info) {
            // Enum values are bare identifiers; collisions with variables are
            // rejected at declaration time, so the order here cannot shadow.
            if (const EnumConstant *EC = Types->lookupEnumConstant(Var->getName())) {
                return ConstantInt::get(*TheContext, APInt(64, static_cast<uint64_t>(EC->Ordinal), true));
            }
            reportError("Unknown variable name %s", Var->getName().c_str());
            return nullptr;
        }
        if (Info->ConstantValue) return Info->ConstantValue;
        if (Info->IsArray) {
            return symbolAddress(*Info);
        }

        const TypeInfo *TypeInfo = resolveType(Info->TypeName);
        if (!TypeInfo || !TypeInfo->LLVMType) {
            reportError("Unknown type for variable %s", Var->getName().c_str());
            return nullptr;
        }

        Value *Loaded = Builder->CreateLoad(TypeInfo->LLVMType, symbolAddress(*Info), Var->getName().c_str());
        if (TypeInfo->isString()) {
            Loaded = emitStringNullGuard(Loaded);
        }
        return Loaded;
    }

    if (auto *ArrAcc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        return Arrays->emitArrayAccess(ArrAcc, *this);
    }

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        if (Unary->getOp() == tok_not) {
            const TypeInfo *Info = getExprTypeInfo(Unary->getOperand());
            Value *Operand = Info && Info->isC()
                ? convertNumericValue(emitExpr(Unary->getOperand()), Info, resolveType("BOOLEAN"))
                : emitCoercedExpr(Unary->getOperand(), resolveType("BOOLEAN"));
            if (Info && Info->Kind == TypeKind::CPointer) {
                reportError("NOT does not support C pointer operands");
                return nullptr;
            }
            if (!Operand) return nullptr;
            return Builder->CreateNot(Operand, "nottmp");
        }
        if (Unary->getOp() == '-') {
            const TypeInfo *Info = getExprTypeInfo(Unary->getOperand());
            if (Info && Info->Kind == TypeKind::CScalar && !Info->isVoid()) {
                const TypeInfo *Promoted = Types->promoteCScalar(Info);
                Value *Operand = convertNumericValue(emitExpr(Unary->getOperand()), Info, Promoted);
                if (!Operand) return nullptr;
                return Promoted->isReal() ? Builder->CreateFNeg(Operand, "c_neg")
                                          : Builder->CreateNeg(Operand, "c_neg");
            }
            if (!Info || Info->isC() || Info->isUserKind() || Info->isString() || Info->isVoid()) {
                reportError("Unary '-' requires a supported numeric operand");
                return nullptr;
            }
            Value *Operand = emitExpr(Unary->getOperand());
            if (!Operand) return nullptr;
            return ArithHandler->emitBinaryOp('-', ConstantInt::get(Type::getInt64Ty(*TheContext), 0),
                                             Operand, 0);
        }
        return nullptr;
    }

    if (auto *Bin = dynamic_cast<BinaryExprAST*>(Expr)) {
        // Whole arrays evaluate to raw data pointers and ^x to raw addresses;
        // neither is a value any operator may consume.
        if (isWholeArrayVar(Bin->getLHS()) || isWholeArrayVar(Bin->getRHS())) {
            reportError("A whole array cannot be used as an operand; index it");
            return nullptr;
        }
        if (dynamic_cast<AddrOfExprAST*>(Bin->getLHS()) ||
            dynamic_cast<AddrOfExprAST*>(Bin->getRHS())) {
            reportError("'^' (address-of) can only be used where a pointer type is expected");
            return nullptr;
        }

        // Enum/Record/Pointer operands never reach ArithmeticHandler or the
        // '&' path: their admissible operations are decided by name here.
        const TypeInfo *LT = getExprTypeInfo(Bin->getLHS());
        const TypeInfo *RT = getExprTypeInfo(Bin->getRHS());
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        return emitBinaryOp(Bin->getOp(), L, LT, R, RT, Bin->getLine());
    }

    if (auto *Call = dynamic_cast<MemberCallExprAST*>(Expr))
        return emitMemberCall(Call);

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr))
        return emitCall(Call->getCallee(), Call->getArgs(), Call->getLine());

    if (auto *D = dynamic_cast<DesignatorExprAST*>(Expr)) {
        LValueInfo LV;
        if (!emitLValue(D, LV)) return nullptr;
        return loadFromLValue(LV);
    }

    // ^x produces a raw address; it is only admissible where a pointer type
    // is expected, which emitCoercedExpr enforces by pointee name.
    if (auto *Addr = dynamic_cast<AddrOfExprAST*>(Expr)) {
        LValueInfo LV;
        if (!emitLValue(Addr->getTarget(), LV)) return nullptr;
        return LV.Addr;
    }

    return nullptr;
}

// Ordinary binary expressions and CASE use the same typed operations.
Value *CodeGen::emitBinaryOp(int Op, Value *L, const TypeInfo *LT,
                             Value *R, const TypeInfo *RT, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    if (!L || !R) return nullptr;
    if ((LT && LT->isC()) || (RT && RT->isC()))
        return emitCBinaryOp(Op, L, LT, R, RT, Line);
    if ((LT && LT->isUserKind()) || (RT && RT->isUserKind()))
        return emitUserKindBinaryOp(Op, L, LT, R, RT, Line);
    if (Op == '&') {
        L = coerceValueToType(L, resolveType("STRING"));
        R = coerceValueToType(R, resolveType("STRING"));
        return L && R ? StrHandler->emitConcat(L, R) : nullptr;
    }
    switch (Op) {
    case tok_eq: case tok_ne: case '<': case '>': case tok_le: case tok_ge:
        if ((LT && LT->isString()) || (RT && RT->isString())) {
            if (!(LT && LT->isString() && RT && RT->isString())) {
                reportError("STRING comparison requires two STRING operands");
                return nullptr;
            }
            return StrHandler->emitCompare(Op, L, R);
        }
        break;
    default: break;
    }
    return ArithHandler->emitBinaryOp(Op, L, R, Line);
}

Value *CodeGen::emitCall(const std::string &Callee,
                         const std::vector<std::unique_ptr<ExprAST>> &ArgExprs, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    std::string Name = Callee;
    if (const BuiltinInfo *B = findBuiltin(Name)) {
        Name = B->Name;
        if (ArgExprs.size() != B->Arity) {
            reportError("%s expects %u arg%s", B->Name, B->Arity, B->Arity == 1 ? "" : "s");
            return nullptr;
        }
    }
    if (Name == "MALLOC") {
        Value *Size = emitAllocationSize(ArgExprs[0].get());
        if (!Size) return nullptr;
        return Memory->emitAllocate(Size, Line);
    }
    if (Name == "REALLOC") {
        Value *Address = emitCoercedExpr(ArgExprs[0].get(), resolveType("C_TYPE(\"void*\")"));
        if (!Address) return nullptr;
        Value *Size = emitAllocationSize(ArgExprs[1].get());
        if (!Size) return nullptr;
        return Memory->emitResize(Address, Size, Line);
    }
    if (Name == "FREE") {
        Value *Address = emitCoercedExpr(ArgExprs[0].get(), resolveType("C_TYPE(\"void*\")"));
        if (!Address) return nullptr;
        return Memory->emitRelease(Address, Line);
    }
    if (Name == "LENGTH") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!Str) return nullptr;
        return StrHandler->emitLength(Str);
    }
    if (Name == "MID") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        Value *Start = emitCoercedExpr(ArgExprs[1].get(), resolveType("INTEGER"));
        Value *Len = emitCoercedExpr(ArgExprs[2].get(), resolveType("INTEGER"));
        if (!Str || !Start || !Len) return nullptr;
        return StrHandler->emitMid(Str, Start, Len);
    }
    if (Name == "RIGHT") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        Value *Len = emitCoercedExpr(ArgExprs[1].get(), resolveType("INTEGER"));
        if (!Str || !Len) return nullptr;
        return StrHandler->emitRight(Str, Len);
    }
    if (Name == "LEFT") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        Value *Len = emitCoercedExpr(ArgExprs[1].get(), resolveType("INTEGER"));
        if (!Str || !Len) return nullptr;
        return StrHandler->emitLeft(Str, Len);
    }
    if (Name == "LCASE") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!Str) return nullptr;
        return StrHandler->emitLCase(Str);
    }
    if (Name == "UCASE") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!Str) return nullptr;
        return StrHandler->emitUCase(Str);
    }
    if (Name == "ASC") {
        const TypeInfo *ArgType = getExprTypeInfo(ArgExprs[0].get());
        if (ArgType && (ArgType->isUserKind() || ArgType->isC())) {
            reportError("ASC expects a CHAR or STRING (got %s)", ArgType->Name.c_str());
            return nullptr;
        }
        Value *ArgVal = emitExpr(ArgExprs[0].get());
        if (!ArgVal) return nullptr;
        Value *CharVal = nullptr;
        if (ArgType && ArgType->isChar()) {
            CharVal = coerceValueToType(ArgVal, resolveType("CHAR"));
        } else {
            CharVal = Builder->CreateLoad(Type::getInt8Ty(*TheContext), ArgVal, "char_load");
        }
        Value *AscVal = Builder->CreateZExt(CharVal, Type::getInt32Ty(*TheContext), "asc_val");
        return coerceValueToType(AscVal, resolveType("INTEGER"));
    }
    if (Name == "CHR") {
        Value *IntVal = emitCoercedExpr(ArgExprs[0].get(), resolveType("INTEGER"));
        if (!IntVal) return nullptr;
        Value *CharVal = Builder->CreateTrunc(IntVal, Type::getInt8Ty(*TheContext), "chr_val");
        return coerceValueToType(CharVal, resolveType("STRING"));
    }
    if (Name == "IS_NUM") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!Str) return nullptr;
        return StrConvHandler->emitIsNum(Str);
    }
    if (Name == "NUM_TO_STR") {
        const TypeInfo *ArgType = getExprTypeInfo(ArgExprs[0].get());
        if (ArgType && (ArgType->isUserKind() || ArgType->isC())) {
            reportError("NUM_TO_STR expects a number (got %s)", ArgType->Name.c_str());
            return nullptr;
        }
        Value *NumV = emitExpr(ArgExprs[0].get());
        if (!NumV) return nullptr;
        bool IsReal = NumV->getType()->isDoubleTy();
        if (NumV->getType()->isIntegerTy(8)) {
            NumV = coerceValueToType(NumV, resolveType("INTEGER"));
        }
        return StrConvHandler->emitNumToStr(NumV, IsReal);
    }
    if (Name == "STR_TO_NUM") {
        Value *Str = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!Str) return nullptr;
        return StrConvHandler->emitStrToNum(Str, true);
    }
    if (Name == "EOF") {
        if (isWholeArrayVar(ArgExprs[0].get())) {
            reportError("Cannot use a whole array as the EOF filename");
            return nullptr;
        }
        Value *FileName = emitCoercedExpr(ArgExprs[0].get(), resolveType("STRING"));
        if (!FileName) return nullptr;
        return Files->emitEof(FileName, Line);
    }

    std::vector<Value*> Args;
    if (!marshalCallArgs(Name, ArgExprs, Line, Args)) return nullptr;
    return FuncGen->emitCall(Name, Args);
}

Value *CodeGen::emitAllocationSize(ExprAST *Expr) {
    SaveAndRestore<int> Location(DiagnosticLine, Expr->getLine() ? Expr->getLine() : DiagnosticLine);
    const TypeInfo *Info = getExprTypeInfo(Expr);
    if (isWholeArrayVar(Expr) || !Info ||
        !(Info->Kind == TypeKind::Integer ||
          (Info->Kind == TypeKind::CScalar && Info->isIntegral()))) {
        reportError("Allocation size must be INTEGER or a C integer");
        return nullptr;
    }
    Value *Size = emitExpr(Expr);
    if (!Size) return nullptr;
    auto *SizeTy = Type::getIntNTy(*TheContext, sizeof(std::size_t) * CHAR_BIT);
    return Builder->CreateIntCast(Size, SizeTy,
                                  Info->Kind == TypeKind::Integer || Info->CSigned, "allocation_size");
}
