#include "cps/Diagnostics.h"
#include "cps/CodeGen.h"
#include "cps/ArrayHandler.h"
#include "cps/FileHandler.h"
#include "cps/MemoryHandler.h"
#include "cps/ExceptionHandler.h"
#include "Builtins.h"
#include "TemplateClone.h"
#include "cps/TypeAST.h"
#include "cps/TemplateAST.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Verifier.h"
#include <algorithm>
#include <cstdarg>

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
    cps::vsourceDiagnostic(cps::Severity::Error, DiagnosticLine, Fmt, Args);
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
    Files = std::make_unique<FileHandler>(*TheContext, *Builder, *TheModule, *RuntimeChecker,
                                         *Exceptions);

    Memory = std::make_unique<MemoryHandler>(*TheContext, *Builder, *TheModule, *Types,
                                             *RuntimeChecker, *Exceptions);

    // Array parameters are bound into ArrayHandler through this inversion so
    // FunctionGen never sees the array subsystem.
    FuncGen->setArrayParamBinder(
        [this](const ParamDecl &P, Value *DataPtr,
               const std::vector<Value*> &LBs, const std::vector<Value*> &UBs) {
            Arrays->bindArrayParameter(P.Name, P.TypeName, DataPtr, LBs, UBs,
                                       /*MakeCopy=*/P.Mode == PassMode::ByVal, *this);
        });

    FuncGen->setCopyRegistrar([this](Value *Addr, const std::string &TypeName, int Line) {
        activateStackObject(registerStackObject(Addr, TypeName, Line));
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

    PrintfFormatStr = Builder->CreateGlobalString("%lld\n", "fmt_nl", 0, TheModule.get());
    PrintfFloatFormatStr = Builder->CreateGlobalString("%f\n", "fmt_flt", 0, TheModule.get());
    PrintfStringFormatStr = Builder->CreateGlobalString("%s\n", "fmt_str", 0, TheModule.get());
    PrintfCharFormatStr = Builder->CreateGlobalString("%c\n", "fmt_chr", 0, TheModule.get());

    ScanfFormatStr = Builder->CreateGlobalString("%lld", "fmt_in", 0, TheModule.get());
    ScanfFloatFormatStr = Builder->CreateGlobalString("%lf", "fmt_in_flt", 0, TheModule.get());

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

bool CodeGen::isVisible(size_t Owner) const {
    return !Units || (*Units)[VisibilityUnit.value_or(CurrentUnit)].Visible[Owner];
}

void CodeGen::selectUnit(size_t Unit) {
    CurrentUnit = Unit;
    DiagnosticSource = (*Units)[Unit].Path;
    FuncGen->setCurrentUnit(Unit, (*Units)[Unit].Path);
    RuntimeChecker->setLibraryMode((*Units)[Unit].IsLibrary);
    FuncGen->setCallHook([this](const FuncSig &Sig) {
        if (!(*Units)[CurrentUnit].IsLibrary && (*Units)[Sig.OwnerUnit].IsLibrary)
            RuntimeChecker->setUserLine(DiagnosticLine);
    });
}

void CodeGen::importSymbols() {
    Symbols.clear();
    for (size_t I = 0; I < Exports.size(); ++I) {
        if (!isVisible(I)) continue;
        for (const auto &[Name, Info] : Exports[I])
            if (I != CurrentUnit || Info.IsGlobal) Symbols.emplace(Name, Info);
    }
}

void CodeGen::compile(const std::vector<std::unique_ptr<StmtAST>> &Statements) {
    compile(std::vector<SourceUnit>{{"", Statements, {true}}});
}

void CodeGen::compile(const std::vector<SourceUnit> &Sources) {
    SaveAndRestore<const std::vector<SourceUnit> *> SourceContext(Units, &Sources);
    SaveAndRestore<std::string> SourceName(DiagnosticSource);
    if (Sources.empty()) { reportError("no main program"); return; }
    Exports.resize(Sources.size());
    Types->setVisibility([this](const std::string &Name) {
        if (FailedTemplateTypes.count(Name)) return false;
        auto It = TypeOwners.find(Name);
        return It == TypeOwners.end() || isVisible(It->second);
    });
    Types->setResolveMiss([this](const std::string &Name) { return instantiateClass(Name); });
    FuncGen->setVisibility([this](size_t Owner) { return isVisible(Owner); });

    struct Definition { size_t Unit; bool IsFunction; bool IsOverload; bool IsExtern; bool IsTemplate; int Line; };
    IdentifierMap<Definition> Definitions;
    std::optional<size_t> CodeUnit;
    FunctionDefAST *Entry = nullptr;
    size_t EntryUnit = 0;
    auto Reserve = [&](const std::string &Name, bool IsFunction = false, bool IsOverload = false,
                       bool IsExtern = false, bool IsTemplate = false) {
        auto [It, Added] = Definitions.emplace(Name, Definition{CurrentUnit, IsFunction, IsOverload, IsExtern, IsTemplate, DiagnosticLine});
        bool Compatible = (IsFunction && IsOverload && It->second.IsFunction && It->second.IsOverload) ||
                          (IsExtern && It->second.IsExtern);
        if (!Added && !Compatible && (It->second.Unit != CurrentUnit ||
            ((IsTemplate || It->second.IsTemplate) && !(IsFunction && It->second.IsFunction)))) {
            // Default library units follow user inputs; point at the user's conflicting definition.
            bool UserConflict = Sources[CurrentUnit].IsLibrary && !Sources[It->second.Unit].IsLibrary;
            SaveAndRestore<int> Location(DiagnosticLine, UserConflict ? It->second.Line : DiagnosticLine);
            SaveAndRestore<std::string> Source(DiagnosticSource,
                UserConflict ? Sources[It->second.Unit].Path : DiagnosticSource);
            reportError("Duplicate definition %s in %s and %s", Name.c_str(),
                        Sources[It->second.Unit].Path.c_str(), Sources[CurrentUnit].Path.c_str());
        }
    };
    for (size_t I = 0; I < Sources.size(); ++I) {
        selectUnit(I);
        for (const auto &S : Sources[I].Statements) {
            SaveAndRestore<int> Location(DiagnosticLine, S->getLine());
            std::string TypeName;
            if (auto *T = dynamic_cast<TemplateDeclAST*>(S.get())) {
                auto *Function = T->getFunction();
                Reserve(T->getName(), Function != nullptr, Function && Function->getProto()->isOverload(), false, true);
                Templates[T->getName()].push_back({T, I});
            } else if (auto *F = dynamic_cast<FunctionDefAST*>(S.get())) {
                Reserve(F->getProto()->getName(), true, F->getProto()->isOverload());
                if (sameIdentifier(F->getProto()->getName(), "MAIN")) {
                    if (Entry) reportError("Multiple MAIN definitions in %s and %s",
                                           Sources[EntryUnit].Path.c_str(), Sources[I].Path.c_str());
                    Entry = F;
                    EntryUnit = I;
                    if (!F->getProto()->getParams().empty() ||
                        !sameIdentifier(F->getProto()->getReturnType(), "INTEGER"))
                        reportError("MAIN must be a FUNCTION with no parameters returning INTEGER");
                }
            } else if (auto *X = dynamic_cast<ExternDeclAST*>(S.get())) {
                Reserve(X->getProto()->getName(), true, false, true);
            } else if (dynamic_cast<IncludeObjStmtAST*>(S.get())) {
            } else if (auto *E = dynamic_cast<EnumTypeDeclAST*>(S.get())) {
                TypeName = E->getName();
                for (const auto &V : E->getValueNames()) Reserve(V);
            } else if (auto *R = dynamic_cast<RecordTypeDeclAST*>(S.get())) {
                TypeName = R->getName();
            } else if (auto *P = dynamic_cast<PointerTypeDeclAST*>(S.get())) {
                TypeName = P->getName();
            } else if (auto *C = dynamic_cast<CTypeDeclAST*>(S.get())) {
                TypeName = C->getName();
            } else if (auto *C = dynamic_cast<ConstantStmtAST*>(S.get())) {
                Reserve(C->getName());
            } else if (auto *G = dynamic_cast<GlobalBlockAST*>(S.get())) {
                for (const auto &Body : G->getBody())
                    if (auto *D = dynamic_cast<DeclareStmtAST*>(Body.get())) Reserve(D->getName());
            } else if (!dynamic_cast<IncludeStmtAST*>(S.get())) {
                if (CodeUnit && *CodeUnit != I) {
                    reportError("Top-level code in both %s and %s",
                                Sources[*CodeUnit].Path.c_str(), Sources[I].Path.c_str());
                    return;
                }
                CodeUnit = I;
            }
            if (!TypeName.empty()) {
                Reserve(TypeName);
                TypeOwners.emplace(TypeName, I);
            }
        }
    }
    if (HadError) return;
    if (Entry && CodeUnit) {
        selectUnit(EntryUnit);
        SaveAndRestore<int> Location(DiagnosticLine, Entry->getLine());
        reportError("FUNCTION MAIN in %s cannot coexist with top-level code in %s",
                    Sources[EntryUnit].Path.c_str(), Sources[*CodeUnit].Path.c_str());
        return;
    }
    MainUnit = Entry ? EntryUnit : CodeUnit.value_or(0);
    FunctionType *FT = FunctionType::get(Type::getInt32Ty(*TheContext), false);
    Function *F = Function::Create(FT, Function::ExternalLinkage, "main", TheModule.get());
    BasicBlock *BB = BasicBlock::Create(*TheContext, "entry", F);
    Builder->SetInsertPoint(BB);

    Exceptions->prepareFunction(F);
    BasicBlock *UnhandledBB = BasicBlock::Create(*TheContext, "unhandled_exception", F);
    Exceptions->pushHandler(UnhandledBB);

    TypePrePass = true;
    // Reserve names across every unit before resolving any type payload.
    for (size_t I = 0; I < Sources.size(); ++I) {
        selectUnit(I);
        runTypePrePass(Sources[I].Statements, 0);
    }
    // Complete included units first. The visited set makes include cycles harmless.
    std::vector<bool> Visited(Sources.size(), false);
    std::function<void(size_t)> DefineTypes = [&](size_t I) {
        if (Visited[I]) return;
        Visited[I] = true;
        for (size_t J = 0; J < Sources.size(); ++J)
            if (J != I && Sources[I].Visible[J]) DefineTypes(J);
        selectUnit(I);
        runTypePrePass(Sources[I].Statements, 1);
        runTypePrePass(Sources[I].Statements, 2);
    };
    for (size_t I = 0; I < Sources.size(); ++I) DefineTypes(I);
    TypePrePass = false;
    emitDeferredClassPrototypes();

    for (size_t I = 0; I < Sources.size(); ++I) {
        selectUnit(I);
        for (const auto &Stmt : Sources[I].Statements) {
            SaveAndRestore<int> Location(DiagnosticLine, Stmt->getLine());
            if (auto *FuncDef = dynamic_cast<FunctionDefAST*>(Stmt.get()))
                FuncGen->emitPrototype(FuncDef->getProto());
            if (auto *Extern = dynamic_cast<ExternDeclAST*>(Stmt.get()))
                FuncGen->emitExternPrototype(Extern->getProto());
            if (auto *Class = dynamic_cast<ClassTypeDeclAST*>(Stmt.get())) {
                emitClassPrototypes(Class);
            }
        }
    }

    validateTemplates();

    // Constants are literal exports. Own constants are still installed only
    // when their declaration is encountered during body emission.
    for (size_t I = 0; I < Sources.size(); ++I) {
        selectUnit(I);
        Symbols.clear();
        for (const auto &Stmt : Sources[I].Statements) {
            if (auto *Constant = dynamic_cast<ConstantStmtAST*>(Stmt.get())) {
                SaveAndRestore<int> Location(DiagnosticLine, Stmt->getLine());
                emitConstantStmt(Constant);
            }
        }
        Exports[I] = Symbols;
    }
    for (int Phase = 0; Phase < 2; ++Phase) {
        for (size_t I = 0; I < Sources.size(); ++I) {
            selectUnit(I);
            importSymbols();
            runGlobalPrePass(Sources[I], Phase);
        }
    }
    captureTemplateSymbols();
    for (size_t I = 0; I < Sources.size(); ++I) {
        selectUnit(I);
        importSymbols();
        for (const auto &Stmt : Sources[I].Statements) emitStmt(Stmt.get());
    }
    drainTemplateBodies();
    selectUnit(MainUnit);
    if (!CodeUnit && !Entry && !HadError) {
        SaveAndRestore<int> Location(DiagnosticLine, Sources.size() > 1 ? 1 : 0);
        reportError("no main program");
    }

    if (!Builder->GetInsertBlock()->hasTerminator()) {
        Value *Status = ConstantInt::get(*TheContext, APInt(32, 0));
        if (Entry && !HadError) {
            Value *Result = Exceptions->emitCall(TheModule->getFunction("__cps_main"), {}, "main_result");
            Status = Builder->CreateTrunc(Result, Type::getInt32Ty(*TheContext));
        }
        Exceptions->recordExit(Builder->CreateRet(Status));
    }

    finishStackObjects(F);
    auto *Exception = Exceptions->popHandler();
    Builder->SetInsertPoint(UnhandledBB);
    Exceptions->beginCatch(Exception);
    Exceptions->endCatch();
    Value *Msg = Builder->CreateGlobalString("Unhandled exception. Program terminated.\n",
                                               "err_unhandled", 0, TheModule.get());
    RuntimeChecker->emitStderr(Msg);
    Builder->CreateRet(ConstantInt::get(Type::getInt32Ty(*TheContext), 1));

    if (verifyModule(*TheModule, &errs())) {
        errs() << cps::diagnosticPrefix(cps::Severity::Fatal, "") << "LLVM module verification failed\n";
        HadError = true;
    }
}

void CodeGen::print(raw_ostream &Output) {
    TheModule->print(Output, nullptr);
}
