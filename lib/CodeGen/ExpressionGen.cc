#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileHandler.h"
#include "cps/MemoryHandler.h"
#include "cps/Lexer.h"
#include "cps/TypeAST.h"
#include "cps/Operator.h"
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
        Value *Text = Builder->CreateGlobalString(Str->getVal());
        return Str->isCString() ? Text : emitStringFromCString(Text);
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
        if (Info->ConstantValue && !Info->Storage) return Info->ConstantValue;
        if (Info->IsArray) {
            return symbolAddress(*Info);
        }

        const TypeInfo *TypeInfo = resolveType(Info->TypeName);
        if (!TypeInfo || !TypeInfo->LLVMType) {
            reportError("Unknown type for variable %s", Var->getName().c_str());
            return nullptr;
        }

        Value *Loaded = Builder->CreateLoad(TypeInfo->LLVMType, symbolAddress(*Info), Var->getName().c_str());
        return Loaded;
    }

    if (auto *ArrAcc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        return Arrays->emitArrayAccess(ArrAcc, *this);
    }

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        if (isWholeArrayVar(Unary->getOperand())) {
            reportError("A whole array cannot be used as an operand; index it");
            return nullptr;
        }
        auto Overload = resolveOperator(Unary->getOp(), getExprTypeInfo(Unary->getOperand()));
        if (Overload.Ambiguous) {
            reportError("Ambiguous overload for %s", operatorName(Unary->getOp()).c_str());
            return nullptr;
        }
        if (Overload.Signature) return emitOperatorCall(*Overload.Signature, Unary->getOperand());
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
        if (LT && RT) {
            auto Overload = resolveOperator(Bin->getOp(), LT, RT);
            if (Overload.Ambiguous) {
                reportError("Ambiguous overload for %s(%s, %s)", operatorName(Bin->getOp()).c_str(),
                            LT->Name.c_str(), RT->Name.c_str());
                return nullptr;
            }
            if (Overload.Signature)
                return emitOperatorCall(*Overload.Signature, Bin->getLHS(), Bin->getRHS());
        }
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        return emitBinaryOp(Bin->getOp(), L, LT, R, RT, Bin->getLine());
    }

    if (auto *Call = dynamic_cast<MemberCallExprAST*>(Expr))
        return emitMemberCall(Call);

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr))
        return emitCall(Call->getCallee(), Call->getArgs(), Call->getLine(), &Call->getTemplateArgs());

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
    if (!LT || !RT) {
        reportError("Cannot determine operand types for %s", operatorName(Op).c_str());
        return nullptr;
    }
    auto Overload = resolveOperator(Op, LT, RT);
    if (Overload.Ambiguous) {
        reportError("Ambiguous overload for %s(%s, %s)", operatorName(Op).c_str(), LT->Name.c_str(), RT->Name.c_str());
        return nullptr;
    }
    if (Overload.Signature) return emitOperatorValues(*Overload.Signature, L, LT, R);
    if ((LT && LT->isC()) || (RT && RT->isC()))
        return emitCBinaryOp(Op, L, LT, R, RT, Line);
    if ((LT && LT->isUserKind()) || (RT && RT->isUserKind()))
        return emitUserKindBinaryOp(Op, L, LT, R, RT, Line);
    return ArithHandler->emitBinaryOp(Op, L, R, Line);
}

Value *CodeGen::emitCall(const std::string &Callee,
                         const std::vector<std::unique_ptr<ExprAST>> &ArgExprs, int Line,
                         const std::vector<std::string> *Explicit) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    std::string Name = Callee;
    if (const BuiltinInfo *B = findBuiltin(Name)) {
        if (Explicit && !Explicit->empty()) {
            reportError("Explicit template arguments are not allowed on builtin %s", B->Name);
            return nullptr;
        }
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
    if (Name == "EOF") {
        if (isWholeArrayVar(ArgExprs[0].get())) {
            reportError("Cannot use a whole array as the EOF filename");
            return nullptr;
        }
        Value *FileName = emitCStringExpr(ArgExprs[0].get());
        if (!FileName) return nullptr;
        return Files->emitEof(FileName, Line);
    }

    std::vector<Value*> Args;
    const FuncSig *Sig = resolveCall(Name, ArgExprs, Explicit);
    if (!Sig || !marshalCallArgs(*Sig, ArgExprs, Line, Args)) return nullptr;
    return FuncGen->emitCall(*Sig, Args);
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
