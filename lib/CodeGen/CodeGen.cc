#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileHandler.h"
#include "cps/ExceptionHandler.h"
#include "Builtins.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include <cstdarg>
#include <cstdio>

using namespace llvm;
using namespace cps;

using namespace cps::detail;

CodeGen::~CodeGen() = default;

bool CodeGen::isBuiltinName(const std::string &Name) {
    return findBuiltin(Name) != nullptr;
}

void CodeGen::reportError(const char *Fmt, ...) {
    va_list Args;
    va_start(Args, Fmt);
    fprintf(stderr, "error: ");
    vfprintf(stderr, Fmt, Args);
    if (DiagnosticLine) fprintf(stderr, " at line %d", DiagnosticLine);
    fprintf(stderr, "\n");
    va_end(Args);
    HadError = true;
}

CodeGen::CodeGen() {
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("cps_module", *TheContext);
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    Types = std::make_unique<TypeSystem>(*TheContext);
    Exceptions = std::make_unique<ExceptionHandler>(*TheContext, *Builder, *TheModule);
    RuntimeChecker = std::make_unique<RuntimeCheck>(*TheModule, *TheContext, *Builder, *Exceptions);

    Arrays = std::make_unique<ArrayHandler>(*TheContext,
                                            *Builder,
                                            *TheModule,
                                            Symbols,
                                            *Types,
                                            *RuntimeChecker);

    FuncGen = std::make_unique<FunctionGen>(*TheContext,
                                            *TheModule,
                                            *Builder,
                                            *Types,
                                            *Exceptions,
                                            Symbols,
                                            HadError, DiagnosticLine);

    ArithHandler = std::make_unique<ArithmeticHandler>(*TheContext, *Builder, HadError,
                                                     *RuntimeChecker);
    StrHandler = std::make_unique<StringHandler>(*TheContext, *Builder, *TheModule);
    StrConvHandler = std::make_unique<StringConversionHandler>(*TheContext, *Builder, *TheModule);
    Files = std::make_unique<FileHandler>(*TheContext, *Builder, *TheModule, *RuntimeChecker,
                                         *Exceptions);

    // Array parameters are bound into ArrayHandler through this inversion so
    // FunctionGen never sees the array subsystem.
    FuncGen->setArrayParamBinder(
        [this](const ParamDecl &P, Value *DataPtr,
               const std::vector<Value*> &LBs, const std::vector<Value*> &UBs) {
            Arrays->bindArrayParameter(P.Name, P.TypeName, DataPtr, LBs, UBs,
                                       /*MakeCopy=*/P.Mode == PassMode::ByVal, *this);
        });

    SetupExternalFunctions();
}

void CodeGen::SetupExternalFunctions() {
    std::vector<Type*> PrintfArgs;
    PrintfArgs.push_back(PointerType::getUnqual(*TheContext));
    FunctionType *PrintfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    PrintfFunc = TheModule->getOrInsertFunction("printf", PrintfType);

    FunctionType *ScanfType = FunctionType::get(Type::getInt32Ty(*TheContext), PrintfArgs, true);
    ScanfFunc = TheModule->getOrInsertFunction("scanf", ScanfType);

    FunctionType *MallocType = FunctionType::get(PointerType::getUnqual(*TheContext),
                                                 {Type::getInt64Ty(*TheContext)},
                                                 false);
    MallocFunc = TheModule->getOrInsertFunction("malloc", MallocType);

    PrintfFormatStr = Builder->CreateGlobalString("%lld\n", "fmt_nl", 0, TheModule.get());
    PrintfFloatFormatStr = Builder->CreateGlobalString("%f\n", "fmt_flt", 0, TheModule.get());
    PrintfStringFormatStr = Builder->CreateGlobalString("%s\n", "fmt_str", 0, TheModule.get());
    PrintfCharFormatStr = Builder->CreateGlobalString("%c\n", "fmt_chr", 0, TheModule.get());

    ScanfFormatStr = Builder->CreateGlobalString("%lld", "fmt_in", 0, TheModule.get());
    ScanfFloatFormatStr = Builder->CreateGlobalString("%lf", "fmt_in_flt", 0, TheModule.get());
    ScanfStringFormatStr = Builder->CreateGlobalString("%s", "fmt_in_str", 0, TheModule.get());

    TrueStr = Builder->CreateGlobalString("TRUE", "str_true", 0, TheModule.get());
    FalseStr = Builder->CreateGlobalString("FALSE", "str_false", 0, TheModule.get());
    EmptyStringStr = Builder->CreateGlobalString("", "str_empty", 0, TheModule.get());

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
    Symbols[Name] = {Storage, TypeName, IsArray, nullptr, ConstantScopes.back().ID};
}

const SymbolInfo *CodeGen::getSymbolInfo(const std::string &Name) const {
    auto It = Symbols.find(Name);
    if (It == Symbols.end()) {
        return nullptr;
    }
    return &It->second;
}

// A whole-array variable reference evaluates to the raw buffer pointer, which
// the pointer->STRING coercion would accept unchanged; file-statement operands
// must reject it explicitly or garbage gets treated as a C string.
bool CodeGen::isWholeArrayVar(ExprAST *Expr) const {
    auto *Var = dynamic_cast<VariableExprAST*>(Expr);
    if (!Var) return false;
    const SymbolInfo *Info = getSymbolInfo(Var->getName());
    return Info && Info->IsArray;
}

const TypeInfo *CodeGen::resolveType(const std::string &TypeName) const {
    return Types->resolve(TypeName);
}

void CodeGen::compile(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(*TheContext), false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);

    Exceptions->prepareFunction(F);
    BasicBlock *UnhandledBB = BasicBlock::Create(*TheContext, "unhandled_exception", F);
    Exceptions->pushHandler(UnhandledBB);

    // User types first: prototypes may name them in parameters/returns.
    runTypePrePass(Statements);

    // Pre-register every top-level FUNCTION/PROCEDURE prototype so calls may
    // precede definitions.
    for (const auto &Stmt : Statements) {
        SaveAndRestore<int> Location(DiagnosticLine, Stmt->getLine());
        if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt.get())) {
            FuncGen->emitPrototype(FuncDef->getProto());
        }
    }

    for (const auto &Stmt : Statements) {
        SaveAndRestore<int> Location(DiagnosticLine, Stmt->getLine());
        emitStmt(Stmt.get());
    }

    if (!Builder->GetInsertBlock()->hasTerminator()) {
        Builder->CreateRet(ConstantInt::get(*TheContext, APInt(32, 0)));
    }

    auto *Exception = Exceptions->popHandler();
    Builder->SetInsertPoint(UnhandledBB);
    Exceptions->beginCatch(Exception);
    Exceptions->endCatch();
    Value *Msg = Builder->CreateGlobalString("Unhandled exception. Program terminated.\n",
                                               "err_unhandled", 0, TheModule.get());
    Builder->CreateCall(PrintfFunc, {Msg});
    Builder->CreateRet(ConstantInt::get(Type::getInt32Ty(*TheContext), 1));

    if (verifyModule(*TheModule, &errs())) {
        errs() << "fatal error: LLVM module verification failed\n";
        HadError = true;
    }
}

void CodeGen::print(raw_ostream &Output) {
    TheModule->print(Output, nullptr);
}

// Collects the arguments for a call to a user FUNCTION/PROCEDURE, dispatching
// on the *declared* signature: BYREF parameters take the variable's alloca,
// BYVAL parameters are evaluated and coerced to the declared parameter type.
bool CodeGen::marshalCallArgs(const std::string &Callee,
                              const std::vector<std::unique_ptr<ExprAST>> &ArgExprs,
                              int Line,
                              std::vector<Value*> &Out) {
    SaveAndRestore<int> Location(DiagnosticLine, Line ? Line : DiagnosticLine);
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
        const ParamSig &P = Sig->Params[i];
        const std::string &TypeName = P.TypeName;
        bool IsByRef = P.IsByRef;
        ExprAST *ArgExpr = ArgExprs[i].get();

        if (P.IsArray) {
            if (!Arrays->emitArrayArgument(ArgExpr, P.TypeName, P.Rank, P.DeclaredBounds,
                                           Callee, Line, *this, Out)) {
                return false;
            }
            continue;
        }

        // A whole-array argument against a scalar parameter is always a user
        // error (and would smash the data pointer if let through).
        if (auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr)) {
            const SymbolInfo *Info = getSymbolInfo(Var->getName());
            if (Info && Info->IsArray) {
                reportError("Cannot pass whole array %s to %s; the parameter is not an array (declare it ARRAY OF %s)",
                            Var->getName().c_str(), Callee.c_str(), TypeName.c_str());
                return false;
            }
        }

        if (IsByRef) {
            auto *Var = dynamic_cast<VariableExprAST*>(ArgExpr);
            if (!Var) {
                reportError("BYREF argument to %s must be a variable", Callee.c_str());
                return false;
            }
            const SymbolInfo *Info = getSymbolInfo(Var->getName());
            if (!Info) {
                reportError("Unknown variable %s in BYREF call", Var->getName().c_str());
                return false;
            }
            if (!requireWritable(*Info, Var->getName())) return false;
            // BYREF aliases the storage, so the types must match exactly.
            if (!Types->sameType(Info->TypeName, TypeName)) {
                reportError("BYREF argument %s to %s must be of type %s (got %s)",
                            Var->getName().c_str(), Callee.c_str(),
                            TypeName.c_str(), Info->TypeName.c_str());
                return false;
            }
            Out.push_back(Info->Storage);
        } else {
            Value *V = emitCoercedExpr(ArgExpr, resolveType(TypeName));
            if (!V) return false;
            Out.push_back(V);
        }
    }
    return true;
}
