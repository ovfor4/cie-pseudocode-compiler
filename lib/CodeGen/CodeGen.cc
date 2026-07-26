#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/Lexer.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include <cstdarg>
#include <cstdio>

using namespace llvm;
using namespace cps;

namespace {

// Single source of truth for the built-in functions: name, arity and return
// type. emitExpr checks arity against this table before dispatching, and
// getExprTypeInfo reads the return type from it.
struct BuiltinInfo {
    const char *Name;
    unsigned Arity;
    const char *ReturnTypeName;
};

constexpr BuiltinInfo Builtins[] = {
    {"LENGTH",     1, "INTEGER"},
    {"MID",        3, "STRING"},
    {"RIGHT",      2, "STRING"},
    {"LEFT",       2, "STRING"},
    {"LCASE",      1, "STRING"},
    {"UCASE",      1, "STRING"},
    {"ASC",        1, "INTEGER"},
    {"CHR",        1, "STRING"},
    {"IS_NUM",     1, "BOOLEAN"},
    {"NUM_TO_STR", 1, "STRING"},
    {"STR_TO_NUM", 1, "REAL"},
};

const BuiltinInfo *findBuiltin(const std::string &Name) {
    for (const auto &B : Builtins) {
        if (Name == B.Name) return &B;
    }
    return nullptr;
}

} // namespace

CodeGen::~CodeGen() = default;

void CodeGen::reportError(const char *Fmt, ...) {
    va_list Args;
    va_start(Args, Fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, Fmt, Args);
    fprintf(stderr, "\n");
    va_end(Args);
    HadError = true;
}

CodeGen::CodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("cps_module", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    Types = std::make_unique<TypeSystem>(*TheContext);
    RuntimeChecker = std::make_unique<RuntimeCheck>(*TheModule, *TheContext, *Builder);

    Arrays = std::make_unique<ArrayHandler>(*TheContext,
                                            *Builder,
                                            *TheModule,
                                            NamedValues,
                                            Symbols,
                                            *Types,
                                            *RuntimeChecker);

    FuncGen = std::make_unique<FunctionGen>(*TheContext,
                                            *TheModule,
                                            *Builder,
                                            *Types,
                                            NamedValues,
                                            Symbols,
                                            HadError);

    IntHandler = std::make_unique<IntegerHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    RealHelper = std::make_unique<RealHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    BoolHandler = std::make_unique<BooleanHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    ArithHandler = std::make_unique<ArithmeticHandler>(*TheContext, *Builder, HadError);
    StrHandler = std::make_unique<StringHandler>(*TheContext, *Builder, *TheModule, NamedValues);
    ChrHandler = std::make_unique<CharHandler>(*TheContext, *Builder, *TheModule);
    StrConvHandler = std::make_unique<StringConversionHandler>(*TheContext, *Builder, *TheModule);

    SetupExternalFunctions();
}

void CodeGen::SetupExternalFunctions() {
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    FunctionType *ScanfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    ScanfFunc = TheModule->getOrInsertFunction("scanf", ScanfType);

    PrintfFormatStr = Builder->CreateGlobalStringPtr("%lld\n", "fmt_nl", 0, TheModule.get());
    PrintfFloatFormatStr = Builder->CreateGlobalStringPtr("%f\n", "fmt_flt", 0, TheModule.get());
    PrintfStringFormatStr = Builder->CreateGlobalStringPtr("%s\n", "fmt_str", 0, TheModule.get());
    PrintfCharFormatStr = Builder->CreateGlobalStringPtr("%c\n", "fmt_chr", 0, TheModule.get());

    ScanfFormatStr = Builder->CreateGlobalStringPtr("%lld", "fmt_in", 0, TheModule.get());
    ScanfFloatFormatStr = Builder->CreateGlobalStringPtr("%lf", "fmt_in_flt", 0, TheModule.get());
    ScanfStringFormatStr = Builder->CreateGlobalStringPtr("%s", "fmt_in_str", 0, TheModule.get());

    TrueStr = Builder->CreateGlobalStringPtr("TRUE", "str_true", 0, TheModule.get());
    FalseStr = Builder->CreateGlobalStringPtr("FALSE", "str_false", 0, TheModule.get());
    EmptyStringStr = Builder->CreateGlobalStringPtr("", "str_empty", 0, TheModule.get());

    Arrays->setupExternalFunctions();
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction, const std::string &VarName) {
    return CreateEntryBlockAlloca(TheFunction, Type::getInt64Ty(*TheContext), VarName);
}

AllocaInst *CodeGen::CreateEntryBlockAlloca(Function *TheFunction,
                                            Type *AllocType,
                                            const std::string &VarName) {
    IRBuilder<> TmpB(&TheFunction->getEntryBlock(), TheFunction->getEntryBlock().begin());
    return TmpB.CreateAlloca(AllocType, nullptr, VarName);
}

void CodeGen::registerSymbol(const std::string &Name,
                             Value *Storage,
                             const std::string &TypeName,
                             bool IsArray) {
    NamedValues[Name] = Storage;
    Symbols[Name] = {Storage, TypeName, IsArray};
}

const SymbolInfo *CodeGen::getSymbolInfo(const std::string &Name) const {
    auto It = Symbols.find(Name);
    if (It == Symbols.end()) {
        return nullptr;
    }
    return &It->second;
}

const TypeInfo *CodeGen::resolveType(const std::string &TypeName) const {
    return Types->resolve(TypeName);
}

Type *CodeGen::getLLVMType(const std::string &TypeName) const {
    return Types->getLLVMType(TypeName);
}

Value *CodeGen::getNamedValue(const std::string &Name) const {
    auto It = NamedValues.find(Name);
    return It == NamedValues.end() ? nullptr : It->second;
}

const TypeInfo *CodeGen::getExprTypeInfo(ExprAST *Expr) const {
    if (!Expr) return nullptr;

    if (dynamic_cast<IntegerExprAST*>(Expr)) return resolveType("INTEGER");
    if (dynamic_cast<RealExprAST*>(Expr)) return resolveType("REAL");
    if (dynamic_cast<BooleanExprAST*>(Expr)) return resolveType("BOOLEAN");
    if (dynamic_cast<CharExprAST*>(Expr)) return resolveType("CHAR");
    if (dynamic_cast<StringExprAST*>(Expr)) return resolveType("STRING");

    if (auto *Var = dynamic_cast<VariableExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(Var->getName());
        return Info ? resolveType(Info->TypeName) : nullptr;
    }

    if (auto *Arr = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        const SymbolInfo *Info = getSymbolInfo(Arr->getName());
        return Info ? resolveType(Info->TypeName) : nullptr;
    }

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        if (Unary->getOp() == tok_not) {
            return resolveType("BOOLEAN");
        }
        return getExprTypeInfo(Unary->getOperand());
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
            default:
                break;
        }

        const TypeInfo *L = getExprTypeInfo(Bin->getLHS());
        const TypeInfo *R = getExprTypeInfo(Bin->getRHS());
        if ((L && L->Kind == TypeKind::Real) || (R && R->Kind == TypeKind::Real)) {
            return resolveType("REAL");
        }
        return resolveType("INTEGER");
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
                Function *MallocF = TheModule->getFunction("malloc");
                if (!MallocF) {
                    FunctionType *MallocTy = FunctionType::get(PointerType::getUnqual(*TheContext),
                                                               {Type::getInt64Ty(*TheContext)},
                                                               false);
                    MallocF = Function::Create(MallocTy,
                                               Function::ExternalLinkage,
                                               "malloc",
                                               TheModule.get());
                }
                Value *Mem = Builder->CreateCall(MallocF,
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

        case TypeKind::Custom:
            if (Val->getType() == TargetInfo->LLVMType) return Val;
            if (Val->getType()->isPointerTy() && TargetInfo->LLVMType->isPointerTy()) {
                return Builder->CreateBitCast(Val, TargetInfo->LLVMType, "custom_ptr_cast");
            }
            break;
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

    registerSymbol(Stmt->getName(), Alloca, Info->Name, false);
}

void CodeGen::emitOutputValue(Value *Val, const TypeInfo *Info, bool AppendNewline) {
    if (!Val || !Info) return;
    if (!Info->Printable && Info->Kind == TypeKind::Custom) {
        reportError("OUTPUT for custom type %s is not implemented yet", Info->Name.c_str());
        return;
    }

    std::vector<Value*> Args;
    Value *Fmt = nullptr;

    if (Info->isReal()) {
        Fmt = AppendNewline
            ? PrintfFloatFormatStr
            : Builder->CreateGlobalStringPtr("%f", "fmt_flt_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Val);
    } else if (Info->isBoolean()) {
        Value *BoolVal = coerceValueToType(Val, resolveType("BOOLEAN"));
        Fmt = AppendNewline
            ? PrintfStringFormatStr
            : Builder->CreateGlobalStringPtr("%s", "fmt_str_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Builder->CreateSelect(BoolVal, TrueStr, FalseStr));
    } else if (Info->isString()) {
        Value *StrVal = Val;
        if (StrVal->getType()->isPointerTy()) {
            Value *IsNull = Builder->CreateICmpEQ(StrVal,
                                                  ConstantPointerNull::get(cast<PointerType>(StrVal->getType())),
                                                  "str_is_null");
            StrVal = Builder->CreateSelect(IsNull, EmptyStringStr, StrVal, "safe_str");
        }
        Fmt = AppendNewline
            ? PrintfStringFormatStr
            : Builder->CreateGlobalStringPtr("%s", "fmt_str_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(StrVal);
    } else if (Info->isChar()) {
        Value *CharVal = coerceValueToType(Val, resolveType("CHAR"));
        Value *Promoted = Builder->CreateZExt(CharVal, Type::getInt32Ty(*TheContext), "char_for_printf");
        Fmt = AppendNewline
            ? PrintfCharFormatStr
            : Builder->CreateGlobalStringPtr("%c", "fmt_chr_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(Promoted);
    } else {
        Value *IntVal = coerceValueToType(Val, resolveType("INTEGER"));
        Fmt = AppendNewline
            ? PrintfFormatStr
            : Builder->CreateGlobalStringPtr("%lld", "fmt_int_inline", 0, TheModule.get());
        Args.push_back(Fmt);
        Args.push_back(IntVal);
    }

    Builder->CreateCall(PrintfFunc, Args);
}

void CodeGen::compile(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(*TheContext), false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);

    // Pre-register every top-level FUNCTION/PROCEDURE prototype so calls may
    // precede definitions.
    for (const auto &Stmt : Statements) {
        if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt.get())) {
            FuncGen->emitPrototype(FuncDef->getProto());
        }
    }

    for (const auto &Stmt : Statements) {
        emitStmt(Stmt.get());
    }

    if (!Builder->GetInsertBlock()->getTerminator()) {
        Builder->CreateRet(ConstantInt::get(*TheContext, APInt(32, 0)));
    }

    if (verifyModule(*TheModule, &errs())) {
        HadError = true;
    }
}

void CodeGen::print() {
    TheModule->print(errs(), nullptr);
}

Value *CodeGen::emitExpr(ExprAST *Expr) {
    if (!Expr) return nullptr;

    if (auto *Num = dynamic_cast<IntegerExprAST*>(Expr)) {
        return IntHandler->createLiteral(Num->getVal());
    }

    if (auto *Real = dynamic_cast<RealExprAST*>(Expr)) {
        return RealHelper->createLiteral(Real->getVal());
    }

    if (auto *Bool = dynamic_cast<BooleanExprAST*>(Expr)) {
        return BoolHandler->createLiteral(Bool->getVal());
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
            reportError("Unknown variable name %s", Var->getName().c_str());
            return nullptr;
        }
        if (Info->IsArray) {
            return Info->Storage;
        }

        const TypeInfo *TypeInfo = resolveType(Info->TypeName);
        if (!TypeInfo || !TypeInfo->LLVMType) {
            reportError("Unknown type for variable %s", Var->getName().c_str());
            return nullptr;
        }

        Value *Loaded = Builder->CreateLoad(TypeInfo->LLVMType, Info->Storage, Var->getName().c_str());
        if (TypeInfo->isString()) {
            Value *IsNull = Builder->CreateICmpEQ(Loaded,
                                                  ConstantPointerNull::get(cast<PointerType>(Loaded->getType())),
                                                  "str_is_null");
            Loaded = Builder->CreateSelect(IsNull, EmptyStringStr, Loaded, "safe_var_str");
        }
        return Loaded;
    }

    if (auto *ArrAcc = dynamic_cast<ArrayAccessExprAST*>(Expr)) {
        return Arrays->emitArrayAccess(ArrAcc, *this);
    }

    if (auto *Unary = dynamic_cast<UnaryExprAST*>(Expr)) {
        Value *Operand = emitExpr(Unary->getOperand());
        if (!Operand) return nullptr;

        if (Unary->getOp() == tok_not) {
            Operand = coerceValueToType(Operand, resolveType("BOOLEAN"));
            return Builder->CreateNot(Operand, "nottmp");
        }
    }

    if (auto *Bin = dynamic_cast<BinaryExprAST*>(Expr)) {
        Value *L = emitExpr(Bin->getLHS());
        Value *R = emitExpr(Bin->getRHS());
        if (!L || !R) return nullptr;
        return ArithHandler->emitBinaryOp(Bin->getOp(), L, R, Bin->getLine());
    }

    if (auto *Call = dynamic_cast<CallExprAST*>(Expr)) {
        std::string Name = Call->getCallee();
        if (const BuiltinInfo *B = findBuiltin(Name)) {
            if (Call->getArgs().size() != B->Arity) {
                reportError("%s expects %u arg%s", B->Name, B->Arity, B->Arity == 1 ? "" : "s");
                return nullptr;
            }
        }
        if (Name == "LENGTH") {
            return StrHandler->emitLength(emitExpr(Call->getArgs()[0].get()));
        }
        if (Name == "MID") {
            Value *Str = emitExpr(Call->getArgs()[0].get());
            Value *Start = coerceValueToType(emitExpr(Call->getArgs()[1].get()), resolveType("INTEGER"));
            Value *Len = coerceValueToType(emitExpr(Call->getArgs()[2].get()), resolveType("INTEGER"));
            if (!Str || !Start || !Len) return nullptr;
            return StrHandler->emitMid(Str, Start, Len);
        }
        if (Name == "RIGHT") {
            Value *Str = emitExpr(Call->getArgs()[0].get());
            Value *Len = coerceValueToType(emitExpr(Call->getArgs()[1].get()), resolveType("INTEGER"));
            if (!Str || !Len) return nullptr;
            return StrHandler->emitRight(Str, Len);
        }
        if (Name == "LEFT") {
            Value *Str = emitExpr(Call->getArgs()[0].get());
            Value *Len = coerceValueToType(emitExpr(Call->getArgs()[1].get()), resolveType("INTEGER"));
            if (!Str || !Len) return nullptr;
            return StrHandler->emitLeft(Str, Len);
        }
        if (Name == "LCASE") {
            return StrHandler->emitLCase(emitExpr(Call->getArgs()[0].get()));
        }
        if (Name == "UCASE") {
            return StrHandler->emitUCase(emitExpr(Call->getArgs()[0].get()));
        }
        if (Name == "ASC") {
            Value *ArgVal = emitExpr(Call->getArgs()[0].get());
            const TypeInfo *ArgType = getExprTypeInfo(Call->getArgs()[0].get());
            Value *CharVal = nullptr;
            if (ArgType && ArgType->isChar()) {
                CharVal = coerceValueToType(ArgVal, resolveType("CHAR"));
            } else {
                CharVal = Builder->CreateLoad(Type::getInt8Ty(*TheContext), ArgVal, "char_load");
            }
            Value *AscVal = ChrHandler->emitAsc(CharVal);
            return coerceValueToType(AscVal, resolveType("INTEGER"));
        }
        if (Name == "CHR") {
            Value *IntVal = coerceValueToType(emitExpr(Call->getArgs()[0].get()), resolveType("INTEGER"));
            Value *CharVal = ChrHandler->emitChr(IntVal);
            Function *MallocF = TheModule->getFunction("malloc");
            Value *Mem = Builder->CreateCall(MallocF, ConstantInt::get(*TheContext, APInt(64, 2)));
            Builder->CreateStore(CharVal, Mem);
            Value *NullPtr = Builder->CreateInBoundsGEP(Type::getInt8Ty(*TheContext),
                                                        Mem,
                                                        ConstantInt::get(*TheContext, APInt(64, 1)));
            Builder->CreateStore(ConstantInt::get(Type::getInt8Ty(*TheContext), 0), NullPtr);
            return Mem;
        }
        if (Name == "IS_NUM") {
            return StrConvHandler->emitIsNum(emitExpr(Call->getArgs()[0].get()));
        }
        if (Name == "NUM_TO_STR") {
            Value *NumV = emitExpr(Call->getArgs()[0].get());
            bool IsReal = NumV->getType()->isDoubleTy();
            if (NumV->getType()->isIntegerTy(8)) {
                NumV = coerceValueToType(NumV, resolveType("INTEGER"));
            }
            return StrConvHandler->emitNumToStr(NumV, IsReal);
        }
        if (Name == "STR_TO_NUM") {
            return StrConvHandler->emitStrToNum(emitExpr(Call->getArgs()[0].get()), true);
        }

        std::vector<Value*> Args;
        if (!marshalCallArgs(Name, Call->getArgs(), Args)) return nullptr;
        return FuncGen->emitCallExpr(Call, Args);
    }

    return nullptr;
}

// Collects the arguments for a call to a user FUNCTION/PROCEDURE, dispatching
// on the *declared* signature: BYREF parameters take the variable's alloca,
// BYVAL parameters are evaluated and coerced to the declared parameter type.
bool CodeGen::marshalCallArgs(const std::string &Callee,
                              const std::vector<std::unique_ptr<ExprAST>> &ArgExprs,
                              std::vector<Value*> &Out) {
    const FuncSig *Sig = FuncGen->getSignature(Callee);
    if (!Sig) {
        reportError("Call to undefined function %s", Callee.c_str());
        return false;
    }
    if (ArgExprs.size() != Sig->Params.size()) {
        reportError("Incorrect # arguments passed to %s", Callee.c_str());
        return false;
    }

    for (size_t i = 0; i < ArgExprs.size(); ++i) {
        const std::string &TypeName = Sig->Params[i].first;
        bool IsByRef = Sig->Params[i].second;
        ExprAST *ArgExpr = ArgExprs[i].get();

        if (IsByRef) {
            auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr);
            if (!Var) {
                reportError("BYREF argument to %s must be a variable", Callee.c_str());
                return false;
            }
            Value *Ptr = getNamedValue(Var->getName());
            if (!Ptr) {
                reportError("Unknown variable %s in BYREF call", Var->getName().c_str());
                return false;
            }
            Out.push_back(Ptr);
        } else {
            Value *V = coerceValueToType(emitExpr(ArgExpr), resolveType(TypeName));
            if (!V) return false;
            Out.push_back(V);
        }
    }
    return true;
}

void CodeGen::emitIfStmt(IfStmtAST *Stmt) {
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;

    CondV = coerceValueToType(CondV, resolveType("BOOLEAN"));

    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    BasicBlock *ThenBB = BasicBlock::Create(*TheContext, "then", TheFunction);
    BasicBlock *ElseBB = BasicBlock::Create(*TheContext, "else", TheFunction);
    BasicBlock *MergeBB = BasicBlock::Create(*TheContext, "ifcont", TheFunction);

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    Builder->SetInsertPoint(ThenBB);
    for (const auto &S : Stmt->getThenStmts()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(MergeBB);

    Builder->SetInsertPoint(ElseBB);
    for (const auto &S : Stmt->getElseStmts()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(MergeBB);

    Builder->SetInsertPoint(MergeBB);
}

void CodeGen::emitWhileStmt(WhileStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "whilecond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "whileloop", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "whilecont", TheFunction);

    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;
    CondV = coerceValueToType(CondV, resolveType("BOOLEAN"));

    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());

    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitRepeatStmt(RepeatStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();

    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "repeatloop", TheFunction);
    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "repeatcond", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "repeatcont", TheFunction);

    Builder->CreateBr(LoopBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(CondBB);
    Value *CondV = emitExpr(Stmt->getCond());
    if (!CondV) return;
    CondV = coerceValueToType(CondV, resolveType("BOOLEAN"));

    Builder->CreateCondBr(CondV, AfterBB, LoopBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitForStmt(ForStmtAST *Stmt) {
    Function *TheFunction = Builder->GetInsertBlock()->getParent();
    std::string VarName = Stmt->getVarName();

    const SymbolInfo *Symbol = getSymbolInfo(VarName);
    if (!Symbol) {
        reportError("Unknown variable in FOR loop %s", VarName.c_str());
        return;
    }
    if (Symbol->TypeName != "INTEGER") {
        reportError("FOR loop variable %s must be INTEGER", VarName.c_str());
        return;
    }

    Value *StartVal = coerceValueToType(emitExpr(Stmt->getStart()), resolveType("INTEGER"));
    if (!StartVal) return;

    Value *Alloca = Symbol->Storage;
    Builder->CreateStore(StartVal, Alloca);

    BasicBlock *CondBB = BasicBlock::Create(*TheContext, "forcond", TheFunction);
    BasicBlock *LoopBB = BasicBlock::Create(*TheContext, "forloop", TheFunction);
    BasicBlock *IncBB = BasicBlock::Create(*TheContext, "forinc", TheFunction);
    BasicBlock *AfterBB = BasicBlock::Create(*TheContext, "forcont", TheFunction);

    Builder->CreateBr(CondBB);
    Builder->SetInsertPoint(CondBB);

    Value *CurVar = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *EndVal = coerceValueToType(emitExpr(Stmt->getEnd()), resolveType("INTEGER"));
    if (!EndVal) return;

    Value *StepVal = nullptr;
    if (Stmt->getStep()) {
        StepVal = coerceValueToType(emitExpr(Stmt->getStep()), resolveType("INTEGER"));
        if (!StepVal) return;
    } else {
        StepVal = ConstantInt::get(*TheContext, APInt(64, 1));
    }

    // The loop direction depends on the STEP value at run time.
    Value *StepNonNeg = Builder->CreateICmpSGE(StepVal,
                                               ConstantInt::get(*TheContext, APInt(64, 0)),
                                               "step_nonneg");
    Value *CondUp = Builder->CreateICmpSLE(CurVar, EndVal, "forcond_le");
    Value *CondDown = Builder->CreateICmpSGE(CurVar, EndVal, "forcond_ge");
    Value *CondV = Builder->CreateSelect(StepNonNeg, CondUp, CondDown, "forcond");

    Builder->CreateCondBr(CondV, LoopBB, AfterBB);

    Builder->SetInsertPoint(LoopBB);
    for (const auto &S : Stmt->getBody()) emitStmt(S.get());
    if (!Builder->GetInsertBlock()->getTerminator())
        Builder->CreateBr(IncBB);

    Builder->SetInsertPoint(IncBB);
    Value *CurValForInc = Builder->CreateLoad(Type::getInt64Ty(*TheContext), Alloca, VarName.c_str());
    Value *NextVal = Builder->CreateAdd(CurValForInc, StepVal, "nextval");
    Builder->CreateStore(NextVal, Alloca);
    Builder->CreateBr(CondBB);

    Builder->SetInsertPoint(AfterBB);
}

void CodeGen::emitStmt(StmtAST *Stmt) {
    if (!Stmt) return;

    if (auto *ArrDecl = dynamic_cast<ArrayDeclareStmtAST*>(Stmt)) {
        Arrays->emitArrayDeclare(ArrDecl, *this);
        return;
    }

    if (auto *ArrAssign = dynamic_cast<ArrayAssignStmtAST*>(Stmt)) {
        Arrays->emitArrayAssign(ArrAssign, *this);
        return;
    }

    if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt)) {
        BasicBlock *SavedBlock = Builder->GetInsertBlock();

        FuncGen->emitFunctionDef(FuncDef, [this](StmtAST *S) {
            this->emitStmt(S);
        });

        if (SavedBlock) Builder->SetInsertPoint(SavedBlock);
        return;
    }

    if (auto *Call = dynamic_cast<CallStmtAST*>(Stmt)) {
        std::vector<Value*> Args;
        if (!marshalCallArgs(Call->getCallee(), Call->getArgs(), Args)) return;
        FuncGen->emitCallStmt(Call, Args);
        return;
    }

    if (auto *Ret = dynamic_cast<ReturnStmtAST*>(Stmt)) {
        Value *RetVal = nullptr;
        if (Ret->getRetVal()) {
            RetVal = emitExpr(Ret->getRetVal());
        }
        FuncGen->emitReturn(Ret, RetVal);
        return;
    }

    if (auto *Decl = dynamic_cast<DeclareStmtAST*>(Stmt)) {
        emitDeclareStmt(Decl);
        return;
    }

    if (auto *Assign = dynamic_cast<AssignStmtAST*>(Stmt)) {
        const SymbolInfo *Info = getSymbolInfo(Assign->getName());
        if (!Info) {
            reportError("Unknown variable name %s", Assign->getName().c_str());
            return;
        }
        if (Info->IsArray) {
            reportError("Cannot assign array %s without indices", Assign->getName().c_str());
            return;
        }

        Value *Val = emitExpr(Assign->getExpr());
        const TypeInfo *TargetType = resolveType(Info->TypeName);
        Val = coerceValueToType(Val, TargetType);
        if (!Val) return;

        Builder->CreateStore(Val, Info->Storage);
        return;
    }

    if (auto *In = dynamic_cast<InputStmtAST*>(Stmt)) {
        const SymbolInfo *Info = getSymbolInfo(In->getName());
        if (!Info) {
            reportError("Unknown variable name %s", In->getName().c_str());
            return;
        }
        if (Info->IsArray) {
            reportError("INPUT for entire array %s is not supported", In->getName().c_str());
            return;
        }

        const TypeInfo *TypeInfo = resolveType(Info->TypeName);
        if (!TypeInfo) {
            reportError("Unknown type for INPUT %s", In->getName().c_str());
            return;
        }

        std::vector<Value*> Args;
        if (TypeInfo->isReal()) {
            Args.push_back(ScanfFloatFormatStr);
            Args.push_back(Info->Storage);
            Builder->CreateCall(ScanfFunc, Args);
        } else if (TypeInfo->isBoolean()) {
            AllocaInst *TempInt = CreateEntryBlockAlloca(Builder->GetInsertBlock()->getParent(), "tmp_bool_input");
            Args.push_back(ScanfFormatStr);
            Args.push_back(TempInt);
            Builder->CreateCall(ScanfFunc, Args);

            Value *Val = Builder->CreateLoad(Type::getInt64Ty(*TheContext), TempInt);
            Value *BoolVal = Builder->CreateICmpNE(Val, ConstantInt::get(*TheContext, APInt(64, 0)), "bool_cast");
            Builder->CreateStore(BoolVal, Info->Storage);
        } else if (TypeInfo->isString()) {
            Function *MallocF = TheModule->getFunction("malloc");
            Value *Mem = Builder->CreateCall(MallocF, ConstantInt::get(*TheContext, APInt(64, 1024)), "input_str");
            Args.push_back(ScanfStringFormatStr);
            Args.push_back(Mem);
            Builder->CreateCall(ScanfFunc, Args);
            Builder->CreateStore(Mem, Info->Storage);
        } else if (TypeInfo->isChar()) {
            Value *CharFmt = Builder->CreateGlobalStringPtr(" %c", "fmt_in_char", 0, TheModule.get());
            Args.push_back(CharFmt);
            Args.push_back(Info->Storage);
            Builder->CreateCall(ScanfFunc, Args);
        } else {
            Args.push_back(ScanfFormatStr);
            Args.push_back(Info->Storage);
            Builder->CreateCall(ScanfFunc, Args);
        }
        return;
    }

    if (auto *Out = dynamic_cast<OutputStmtAST*>(Stmt)) {
        bool Handled = Arrays->tryEmitArrayOutput(Out->getExpr(), *this);
        if (Handled) return;

        Value *Val = emitExpr(Out->getExpr());
        const TypeInfo *TypeInfo = getExprTypeInfo(Out->getExpr());
        if (!TypeInfo) {
            reportError("Cannot infer the type of the OUTPUT expression");
            return;
        }
        emitOutputValue(Val, TypeInfo, true);
        return;
    }

    if (auto *IfStmt = dynamic_cast<IfStmtAST*>(Stmt)) {
        emitIfStmt(IfStmt);
        return;
    }
    if (auto *WhileStmt = dynamic_cast<WhileStmtAST*>(Stmt)) {
        emitWhileStmt(WhileStmt);
        return;
    }
    if (auto *RepeatStmt = dynamic_cast<RepeatStmtAST*>(Stmt)) {
        emitRepeatStmt(RepeatStmt);
        return;
    }
    if (auto *ForStmt = dynamic_cast<ForStmtAST*>(Stmt)) {
        emitForStmt(ForStmt);
        return;
    }
}
