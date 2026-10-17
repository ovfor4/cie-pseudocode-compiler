#include "cps/CodeGen.h"
#include "cps/Lexer.h"
#include "cps/TypeAST.h"
#include "Builtins.h"

using namespace llvm;
using namespace cps;

using namespace cps::detail;

const TypeInfo *CodeGen::getExprTypeInfo(ExprAST *Expr) const {
    if (!Expr) return nullptr;

    if (dynamic_cast<IntegerExprAST*>(Expr)) return resolveType("INTEGER");
    if (dynamic_cast<RealExprAST*>(Expr)) return resolveType("REAL");
    if (dynamic_cast<BooleanExprAST*>(Expr)) return resolveType("BOOLEAN");
    if (dynamic_cast<CharExprAST*>(Expr)) return resolveType("CHAR");
    if (dynamic_cast<StringExprAST*>(Expr)) return resolveType("STRING");
    if (auto *Cast = dynamic_cast<CastExprAST*>(Expr))
        return resolveType(Cast->getTypeName());

    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(Var->getName());
        if (Info) return resolveType(Info->TypeName);
        if (const EnumConstant *EC = Types->lookupEnumConstant(Var->getName())) {
            return resolveType(EC->TypeName);
        }
        return nullptr;
    }

    if (auto *Arr = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(Arr->getName());
        return Info ? resolveType(Info->TypeName) : nullptr;
    }

    if (auto *D = dynamic_cast<DesignatorExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(D->getBaseName());
        if (!Info) return nullptr;
        std::string TypeName = Info->TypeName;
        size_t Start = 0;
        if (Info->IsArray) {
            // SymbolInfo already holds the ELEMENT type name for arrays.
            if (D->getAccesses().empty() || D->getAccesses()[0].Kind != AccessKind::Index)
                return nullptr;
            Start = 1;
        }
        for (size_t I = Start; I < D->getAccesses().size(); ++I) {
            const TypeInfo *Cur = resolveType(TypeName);
            if (!Cur) return nullptr;
            const DesignatorAccess &A = D->getAccesses()[I];
            if (A.Kind == AccessKind::Field) {
                const RecordPayload *RP = Cur->asRecord();
                const RecordFieldInfo *FI = RP ? RP->findField(A.FieldName) : nullptr;
                if (!FI) return nullptr;
                if (!FI->IsPublic && !sameIdentifier(CurrentClass, TypeName)) return nullptr;
                TypeName = FI->TypeName;
            } else if (A.Kind == AccessKind::Deref) {
                const PointerPayload *PP = Cur->asPointer();
                if (!PP) return nullptr;
                TypeName = PP->PointeeTypeName;
            } else {
                return nullptr; // indexing a non-array
            }
        }
        return resolveType(TypeName);
    }

    // ^x has no pseudocode type of its own; only the pointer admission in
    // emitCoercedExpr may consume it.
    if (dynamic_cast<AddrOfExprAST*>(Expr)) return nullptr;

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        if (Unary->getOp() == tok_not) {
            return resolveType("BOOLEAN");
        }
        const TypeInfo *Operand = getExprTypeInfo(Unary->getOperand());
        if (Operand && Operand->isC()) return Types->promoteCScalar(Operand);
        if (Operand && !Operand->isC() && !Operand->isUserKind())
            return Operand->isReal() ? resolveType("REAL") : resolveType("INTEGER");
        return nullptr;
    }

    if (auto *Bin = dynamic_cast<BinaryExprAST*>(Expr)) {
        switch (Bin->getOp()) {
            case tok_eq:
            case tok_ne:
            case '<':
            case '>':
            case tok_le:
            case tok_ge:
            case tok_and:
            case tok_or:
                return resolveType("BOOLEAN");
            case '&':
                return resolveType("STRING");
            case '/':
                return resolveType("REAL");
            default:
                break;
        }

        const TypeInfo *L = getExprTypeInfo(Bin->getLHS());
        const TypeInfo *R = getExprTypeInfo(Bin->getRHS());
        if ((L && L->isC()) || (R && R->isC())) return Types->commonCScalar(L, R);
        // Enum arithmetic: E +/- INTEGER (enum on the left) yields the enum;
        // every other user-kind combination has no type (emission rejects it).
        if ((L && L->isUserKind()) || (R && R->isUserKind())) {
            if (L && L->isEnum() && R && R->Kind == TypeKind::Integer &&
                (Bin->getOp() == '+' || Bin->getOp() == '-')) {
                return L;
            }
            return nullptr;
        }
        if ((L && L->Kind == TypeKind::Real) || (R && R->Kind == TypeKind::Real)) {
            return resolveType("REAL");
        }
        return resolveType("INTEGER");
    }

    if (auto *Call = dynamic_cast<MemberCallExprAST*>(Expr)) {
        const TypeInfo *Receiver = getExprTypeInfo(Call->getReceiver());
        const PrototypeAST *Method = Receiver ? findMethod(Receiver->Name, Call->getMethod()) : nullptr;
        if (!Method || (!Method->isPublic() && !sameIdentifier(CurrentClass, Receiver->Name))) return nullptr;
        return resolveType(Method->getReturnType());
    }

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr)) {
        const std::string &Name = Call->getCallee();
        if (const BuiltinInfo *B = findBuiltin(Name)) {
            return resolveType(B->ReturnTypeName);
        }
        if (const FuncSig *Sig = FuncGen->getSignature(Name)) {
            return resolveType(Sig->ReturnTypeName);
        }

        Function *CalleeF = TheModule->getFunction(Name);
        if (!CalleeF) return resolveType("INTEGER");

        Type *RetTy = CalleeF->getReturnType();
        if (RetTy->isVoidTy()) return resolveType("VOID");
        if (RetTy->isDoubleTy()) return resolveType("REAL");
        if (RetTy->isIntegerTy(1)) return resolveType("BOOLEAN");
        if (RetTy->isIntegerTy(8)) return resolveType("CHAR");
        if (RetTy->isPointerTy()) return resolveType("STRING");
        return resolveType("INTEGER");
    }

    return nullptr;
}

Value *CodeGen::coerceValueToType(Value *Val, const TypeInfo *TargetInfo) {
    if (!Val || !TargetInfo || !TargetInfo->LLVMType) return nullptr;

    if (Val->getType() == TargetInfo->LLVMType) {
        return Val;
    }

    switch (TargetInfo->Kind) {
        case TypeKind::Real:
            if (Val->getType()->isDoubleTy()) return Val;
            if (Val->getType()->isIntegerTy(1))
                return Builder->CreateUIToFP(Val, Type::getDoubleTy(*TheContext), "bool_to_real");
            if (Val->getType()->isIntegerTy(8))
                return Builder->CreateUIToFP(Val, Type::getDoubleTy(*TheContext), "char_to_real");
            if (Val->getType()->isIntegerTy())
                return Builder->CreateSIToFP(Val, Type::getDoubleTy(*TheContext), "int_to_real");
            break;

        case TypeKind::Integer:
            if (Val->getType()->isIntegerTy(64)) return Val;
            if (Val->getType()->isDoubleTy())
                return Builder->CreateFPToSI(Val, Type::getInt64Ty(*TheContext), "real_to_int");
            if (Val->getType()->isIntegerTy(1))
                return Builder->CreateZExt(Val, Type::getInt64Ty(*TheContext), "bool_to_int");
            if (Val->getType()->isIntegerTy(8))
                return Builder->CreateZExt(Val, Type::getInt64Ty(*TheContext), "char_to_int");
            if (Val->getType()->isIntegerTy())
                return Builder->CreateSExtOrTrunc(Val, Type::getInt64Ty(*TheContext), "int_cast");
            break;

        case TypeKind::Boolean:
            if (Val->getType()->isIntegerTy(1)) return Val;
            if (Val->getType()->isIntegerTy())
                return Builder->CreateICmpNE(Val, Constant::getNullValue(Val->getType()), "to_bool");
            if (Val->getType()->isDoubleTy())
                return Builder->CreateFCmpONE(Val, ConstantFP::get(*TheContext, APFloat(0.0)), "to_bool");
            if (Val->getType()->isPointerTy())
                return Builder->CreateICmpNE(Val,
                                             ConstantPointerNull::get(cast<PointerType>(Val->getType())),
                                             "ptr_to_bool");
            break;

        case TypeKind::Char:
            if (Val->getType()->isIntegerTy(8)) return Val;
            if (Val->getType()->isIntegerTy(1))
                return Builder->CreateZExt(Val, Type::getInt8Ty(*TheContext), "bool_to_char");
            if (Val->getType()->isDoubleTy())
                return Builder->CreateFPToUI(Val, Type::getInt8Ty(*TheContext), "real_to_char");
            if (Val->getType()->isIntegerTy())
                return Builder->CreateTruncOrBitCast(Val, Type::getInt8Ty(*TheContext), "int_to_char");
            break;

        case TypeKind::String:
            if (Val->getType()->isPointerTy()) return Val;
            if (Val->getType()->isIntegerTy(1)) {
                return Builder->CreateSelect(Val, TrueStr, FalseStr, "bool_to_str");
            }
            if (Val->getType()->isDoubleTy()) {
                return StrConvHandler->emitNumToStr(Val, true);
            }
            if (Val->getType()->isIntegerTy(8)) {
                Value *Mem = Builder->CreateCall(MallocFunc,
                                                 ConstantInt::get(*TheContext, APInt(64, 2)),
                                                 "char_str");
                Builder->CreateStore(Val, Mem);
                Value *NullPtr = Builder->CreateInBoundsGEP(Type::getInt8Ty(*TheContext),
                                                            Mem,
                                                            ConstantInt::get(*TheContext, APInt(64, 1)));
                Builder->CreateStore(ConstantInt::get(Type::getInt8Ty(*TheContext), 0), NullPtr);
                return Mem;
            }
            if (Val->getType()->isIntegerTy()) {
                Value *AsInt = coerceValueToType(Val, resolveType("INTEGER"));
                return AsInt ? StrConvHandler->emitNumToStr(AsInt, false) : nullptr;
            }
            break;

        case TypeKind::Void:
            return nullptr;

        // Enum/Record/Pointer identity lives in the type NAME; at this level
        // an enum is a plain i64 and a pointer a plain ptr, so nothing can be
        // checked here. emitCoercedExpr is the only legitimate door.
        case TypeKind::Enum:
        case TypeKind::Record:
        case TypeKind::Pointer:
        case TypeKind::CScalar:
        case TypeKind::CPointer:
            reportError("Internal: a %s value reached representation coercion without a name-level check",
                        TargetInfo->Name.c_str());
            return nullptr;
    }

    reportError("Cannot coerce value of LLVM type to target type %s",
            TargetInfo->Name.c_str());
    return nullptr;
}

void CodeGen::emitDeclareStmt(DeclareStmtAST *Stmt) {
    const TypeInfo *Info = resolveType(Stmt->getType());
    if (!Info || !Info->LLVMType || Info->isVoid()) {
        reportError("Unknown type %s", Stmt->getType().c_str());
        return;
    }
    if (Types->lookupEnumConstant(Stmt->getName())) {
        reportError("'%s' is an enum value and cannot be declared as a variable",
                    Stmt->getName().c_str());
        return;
    }
    if (Types->resolve(Stmt->getName())) {
        reportError("'%s' is a type name and cannot be declared as a variable",
                    Stmt->getName().c_str());
        return;
    }

    if (!prepareSymbolDeclaration(Stmt->getName())) return;

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, Info->LLVMType, Stmt->getName());

    Value *InitVal = nullptr;
    if (Info->isString()) {
        InitVal = EmptyStringStr;
    } else {
        InitVal = Types->getZeroValue(Info->Name);
    }

    if (InitVal) {
        Builder->CreateStore(InitVal, Alloca);
    }

    Value *Storage = Alloca;
    if (Info->IsClass) {
        Storage = CreateEntryBlockAlloca(TheFunction, PointerType::getUnqual(*TheContext),
                                         Stmt->getName() + ".ref");
        Builder->CreateStore(Alloca, Storage);
    }
    registerSymbol(Stmt->getName(), Storage, Info->Name, false);
    Symbols.find(Stmt->getName())->second.IsReference = Info->IsClass;
    if (Stmt->hasParens() && !Info->IsClass) {
        reportError("Constructor arguments require a class type");
        return;
    }
    emitConstruct(Info->Name, Alloca, Stmt->getArgs(), Stmt->getLine());
}
