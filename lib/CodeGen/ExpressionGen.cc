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
        const ResultContext *Context = resultContext(Expr);
        const TypeInfo *OperandType = getExprTypeInfo(Unary->getOperand());
        auto Overload = resolveOperator(Unary->getOp(), OperandType, nullptr, Context);
        if (Overload.Ambiguous) {
            reportError("Ambiguous overload for %s", operatorName(Unary->getOp()).c_str());
            return nullptr;
        }
        if (Overload.WrongResult) {
            reportError("No matching overload for %s(%s) returning %s",
                        operatorName(Unary->getOp()).c_str(),
                        OperandType ? OperandType->Name.c_str() : "?", Context->Type->Name.c_str());
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
            if (Info->Kind == TypeKind::Boolean || Info->Kind == TypeKind::Char) {
                reportError("Unary '-' cannot be applied to %s", Info->Name.c_str());
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
        const ResultContext *Context = resultContext(Expr);
        if (LT && RT) {
            auto Overload = resolveOperator(Bin->getOp(), LT, RT, Context);
            if (Overload.Ambiguous) {
                reportError("Ambiguous overload for %s(%s, %s)", operatorName(Bin->getOp()).c_str(),
                            LT->Name.c_str(), RT->Name.c_str());
                return nullptr;
            }
            if (Overload.WrongResult) {
                reportError("No matching overload for %s(%s, %s) returning %s",
                            operatorName(Bin->getOp()).c_str(), LT->Name.c_str(),
                            RT->Name.c_str(), Context->Type->Name.c_str());
                return nullptr;
            }
            if (Overload.Signature)
                return emitOperatorCall(*Overload.Signature, Bin->getLHS(), Bin->getRHS());
        }
        if ((Bin->getOp() == tok_and || Bin->getOp() == tok_or) && LT && RT &&
            Builder->GetInsertBlock() && isLogicalOperand(LT) && isLogicalOperand(RT))
            return emitShortCircuit(Bin, LT, RT);
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        return emitBinaryOp(Bin->getOp(), L, LT, R, RT, Bin->getLine(), Context);
    }

    if (auto *Call = dynamic_cast<MemberCallExprAST*>(Expr))
        return emitMemberCall(Call, resultContext(Expr));

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr))
        return emitCall(Call->getCallee(), Call->getArgs(), Call->getLine(),
                        &Call->getTemplateArgs(), resultContext(Expr));

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

// BOOLEAN, or a C scalar tested against zero; everything else keeps the
// ordinary operator path (and its diagnostics).
bool CodeGen::isLogicalOperand(const TypeInfo *T) {
    return T->Kind == TypeKind::Boolean || (T->Kind == TypeKind::CScalar && !T->isVoid());
}

// AND / OR evaluate the right operand only when the left one does not
// already decide the result.
Value *CodeGen::emitShortCircuit(BinaryExprAST *Bin, const TypeInfo *LT, const TypeInfo *RT) {
    SaveAndRestore<int> Location(DiagnosticLine, Bin->getLine() ? Bin->getLine() : DiagnosticLine);
    const TypeInfo *Bool = resolveType("BOOLEAN");
    bool IsAnd = Bin->getOp() == tok_and;
    auto EmitOperand = [&](ExprAST *E, const TypeInfo *T) -> Value * {
        Value *V = emitExpr(E);
        if (!V) return nullptr;
        return T->Kind == TypeKind::Boolean ? V : convertNumericValue(V, T, Bool);
    };

    Value *L = EmitOperand(Bin->getLHS(), LT);
    if (!L) return nullptr;
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *LeftBB = Builder->GetInsertBlock();
    BasicBlock *RightBB = BasicBlock::Create(*TheContext, IsAnd ? "and_rhs" : "or_rhs", TheFunction);
    BasicBlock *DoneBB = BasicBlock::Create(*TheContext, IsAnd ? "and_done" : "or_done", TheFunction);
    if (IsAnd) Builder->CreateCondBr(L, RightBB, DoneBB);
    else       Builder->CreateCondBr(L, DoneBB, RightBB);

    Builder->SetInsertPoint(RightBB);
    Value *R = EmitOperand(Bin->getRHS(), RT);
    if (!R) return nullptr;
    BasicBlock *RightEndBB = Builder->GetInsertBlock();
    Builder->CreateBr(DoneBB);

    Builder->SetInsertPoint(DoneBB);
    PHINode *Result = Builder->CreatePHI(Type::getInt1Ty(*TheContext), 2, IsAnd ? "andtmp" : "ortmp");
    Result->addIncoming(ConstantInt::get(Type::getInt1Ty(*TheContext), IsAnd ? 0 : 1), LeftBB);
    Result->addIncoming(R, RightEndBB);
    return Result;
}

// Ordinary binary expressions and CASE use the same typed operations.
Value *CodeGen::emitBinaryOp(int Op, Value *L, const TypeInfo *LT,
                             Value *R, const TypeInfo *RT, int Line,
                             const ResultContext *Context) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    if (!L || !R) return nullptr;
    if (!LT || !RT) {
        reportError("Cannot determine operand types for %s", operatorName(Op).c_str());
        return nullptr;
    }
    auto Overload = resolveOperator(Op, LT, RT, Context);
    if (Overload.Ambiguous) {
        reportError("Ambiguous overload for %s(%s, %s)", operatorName(Op).c_str(), LT->Name.c_str(), RT->Name.c_str());
        return nullptr;
    }
    if (Overload.WrongResult) {
        reportError("No matching overload for %s(%s, %s) returning %s", operatorName(Op).c_str(),
                    LT->Name.c_str(), RT->Name.c_str(), Context->Type->Name.c_str());
        return nullptr;
    }
    if (Overload.Signature) return emitOperatorValues(*Overload.Signature, L, LT, R);
    if ((LT && LT->isC()) || (RT && RT->isC()))
        return emitCBinaryOp(Op, L, LT, R, RT, Line);
    if ((LT && LT->isUserKind()) || (RT && RT->isUserKind()))
        return emitUserKindBinaryOp(Op, L, LT, R, RT, Line);

    // Plain scalars only: BOOLEAN and CHAR are not numbers (use ASC / CHR),
    // AND / OR take BOOLEAN, comparisons need equal types or INTEGER with REAL.
    auto Numeric = [](const TypeInfo *T) {
        return T->Kind == TypeKind::Integer || T->Kind == TypeKind::Real;
    };
    bool Ok = true;
    switch (Op) {
    case '+': case '-': case '*': case '/': case tok_div: case tok_mod:
        Ok = Numeric(LT) && Numeric(RT);
        break;
    case tok_and: case tok_or:
        Ok = LT->Kind == TypeKind::Boolean && RT->Kind == TypeKind::Boolean;
        break;
    case tok_eq: case tok_ne: case '<': case '>': case tok_le: case tok_ge:
        Ok = LT->Kind == RT->Kind || (Numeric(LT) && Numeric(RT));
        break;
    }
    if (!Ok) {
        reportError("Operator %s cannot be applied to %s and %s",
                    operatorSpelling(Op).c_str(), LT->Name.c_str(), RT->Name.c_str());
        return nullptr;
    }
    return ArithHandler->emitBinaryOp(Op, L, R, Line);
}

Value *CodeGen::emitCall(const std::string &Callee,
                         const std::vector<std::unique_ptr<ExprAST>> &ArgExprs, int Line,
                         const std::vector<std::string> *Explicit,
                         const ResultContext *Context) {
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
    if (Name == "INT") return emitIntBuiltin(ArgExprs[0].get(), Line);
    if (Name == "EOF") {
        Value *FileName = emitFileNameOperand(ArgExprs[0].get(), "EOF");
        if (!FileName) return nullptr;
        return Files->emitEof(FileName, Line);
    }

    std::vector<Value*> Args;
    std::vector<const TypeInfo *> Variadic;
    const FuncSig *Sig = resolveCall(Name, ArgExprs, Explicit, Context);
    if (!Sig || !marshalCallArgs(*Sig, ArgExprs, Line, Args, &Variadic)) return nullptr;
    return FuncGen->emitCall(*Sig, Args, Variadic);
}

// INT(x): the integer part of a REAL, truncated toward zero. NaN and values
// outside the INTEGER range are a runtime error.
Value *CodeGen::emitIntBuiltin(ExprAST *Arg, int Line) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
    const TypeInfo *Info = getExprTypeInfo(Arg);
    if (isWholeArrayVar(Arg) || !Info ||
        (Info->Kind != TypeKind::Integer && Info->Kind != TypeKind::Real)) {
        reportError("INT expects an INTEGER or REAL argument");
        return nullptr;
    }
    Value *V = emitExpr(Arg);
    if (!V || Info->Kind == TypeKind::Integer) return V;

    Type *DoubleTy = Type::getDoubleTy(*TheContext);
    Value *AtLeastMin = Builder->CreateFCmpOGE(V, ConstantFP::get(DoubleTy, -9223372036854775808.0), "int_ge_min");
    Value *BelowMax = Builder->CreateFCmpOLT(V, ConstantFP::get(DoubleTy, 9223372036854775808.0), "int_lt_max");
    Value *InRange = Builder->CreateAnd(AtLeastMin, BelowMax, "int_in_range");
    Value *OutOfRange = Builder->CreateNot(InRange, "int_out_of_range");
    Value *Msg = Builder->CreateGlobalString("[Fatal] line %d: INT argument out of INTEGER range\n",
                                             "err_int_range", 0, TheModule.get());
    RuntimeChecker->emitCheck(OutOfRange, Msg, {RuntimeChecker->lineValue(Line)});
    return Builder->CreateFPToSI(V, Type::getInt64Ty(*TheContext), "int_value");
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
