#include "cps/Diagnostics.h"
#include "cps/FunctionGen.h"
#include "cps/CodeGen.h"
#include "cps/ExceptionHandler.h"
#include "cps/Operator.h"
#include <cstdarg>

using namespace llvm;
using namespace cps;

// Symbols the compiler itself declares (libc + runtime); a user
// FUNCTION/PROCEDURE with one of these names would hijack the runtime.
bool FunctionGen::isReservedRuntimeName(const std::string &Name) {
    static const char *Reserved[] = {
        "__cps_main", "__cps_user_line", "printf", "scanf", "malloc", "free", "strlen", "memcpy",
        "strcpy", "strcat", "toupper", "tolower", "sprintf", "snprintf", "strtol", "strtoll",
        "strtod", "memset", "exit", "fprintf",
        "fopen", "fclose", "fgetc", "ungetc", "fputs", "fputc",
        "realloc", "strcmp", "__cxa_allocate_exception", "__cxa_throw",
        "__cxa_begin_catch", "__cxa_end_catch", "__gxx_personality_v0",
        "__gxx_personality_seh0", "__gxx_personality_sj0", "_Unwind_Resume", "_ZTIi",
    };
    for (const char *R : Reserved) {
        if (sameIdentifier(Name, R)) return true;
    }
    return false;
}

void FunctionGen::reportError(const char *Fmt, ...) {
    va_list Args;
    va_start(Args, Fmt);
    cps::vsourceDiagnostic(cps::Severity::Error, DiagnosticLine, Fmt, Args);
    va_end(Args);
    HadError = true;
}

Type *FunctionGen::getLLVMType(const std::string &TypeName) {
    Type *Resolved = Types.getLLVMType(TypeName);
    if (!Resolved) {
        reportError("Unknown type %s", TypeName.c_str());
        return Type::getInt64Ty(Context);
    }
    return Resolved;
}

void FunctionGen::createArgumentAllocas(Function *F, const std::vector<ParamDecl> &Params, bool IsMethod) {
    Function::arg_iterator AI = F->arg_begin();
    if (IsMethod) ++AI;
    for (const ParamDecl &P : Params) {
        bool InvalidThis = IsMethod && sameIdentifier(P.Name, "this");
        if (InvalidThis) reportError("A method parameter cannot be named this");
        if (Types.lookupEnumConstant(P.Name)) {
            reportError("parameter '%s' collides with an enum value", P.Name.c_str());
        }
        if (Types.resolve(P.Name)) {
            reportError("parameter '%s' collides with a type name", P.Name.c_str());
        }
        Value *ArgVal = &(*AI);
        ++AI;

        if (P.IsArray) {
            // Data pointer + kBoundArgsPerDim bounds per dimension; binding
            // into the array subsystem is delegated to the injected binder.
            std::vector<Value*> LBs, UBs;
            for (int D = 1; D <= P.Rank; ++D) {
                Value *LB = &(*AI);
                ++AI;
                Value *UB = &(*AI);
                ++AI;
                LBs.push_back(LB);
                UBs.push_back(UB);
            }
            if (!InvalidThis && BindArrayParam) BindArrayParam(P, ArgVal, LBs, UBs);
            continue;
        }

        if (InvalidThis) continue;
        if (P.Mode == PassMode::ByRef) {
            Symbols[P.Name] = {ArgVal, P.TypeName, false};
            continue;
        }

        Type *ArgType = getLLVMType(P.TypeName);
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(ArgType, nullptr, P.Name);

        // The caller constructs class arguments; this slot takes ownership of the value.
        Builder.CreateStore(ArgVal, Alloca);
        const TypeInfo *Info = Types.resolve(P.TypeName);
        if (Info && Info->IsClass) {
            AllocaInst *Slot = TmpB.CreateAlloca(PointerType::getUnqual(Context), nullptr, P.Name + ".ref");
            Builder.CreateStore(Alloca, Slot);
            Symbols[P.Name] = {Slot, P.TypeName, false, nullptr, 0, true};
        } else {
            Symbols[P.Name] = {Alloca, P.TypeName, false};
        }
        if (RegisterCopy) RegisterCopy(Alloca, P.TypeName, P.Line);
    }
}

std::vector<const FuncSig *> FunctionGen::getOverloads(const std::string &Name, bool AllUnits) const {
    std::vector<const FuncSig *> Result;
    auto It = Signatures.find(sameIdentifier(Name, "MAIN") ? "__cps_main" : Name);
    if (It != Signatures.end())
        for (const FuncSig &Sig : It->second)
            if (AllUnits || !Visible || Visible(Sig.OwnerUnit)) Result.push_back(&Sig);
    return Result;
}

const FuncSig *FunctionGen::getSignatureByLink(const std::string &Name) const {
    auto It = LinkIndex.find(Name);
    if (It == LinkIndex.end()) return nullptr;
    return &Signatures.at(It->second.first)[It->second.second];
}

// Copy-constructor, operator, MAIN and name-collision rules for a
// FUNCTION/PROCEDURE prototype; reports the first violation.
bool FunctionGen::validatePrototype(PrototypeAST *Proto) {
    if (!Proto->getOwnerClass().empty() && sameIdentifier(Proto->getName(), "NEW") &&
        Proto->getParams().size() == 1) {
        const auto &P = Proto->getParams()[0];
        if (!P.IsArray && sameIdentifier(P.TypeName, Proto->getOwnerClass()) &&
            P.Mode != PassMode::ByRef) {
            reportError("A copy constructor must take BYREF to its own class");
            return false;
        }
    }
    if (Proto->getOperator() == tok_assign &&
        (Proto->getOwnerClass().empty() || !Proto->isOverload() ||
         !sameIdentifier(Proto->getReturnType(), "VOID"))) {
        reportError("Assignment operator must be an OVERLOAD member PROCEDURE");
        return false;
    }
    if (int Op = Proto->getOperator()) {
        size_t Count = Proto->getParams().size() + !Proto->getOwnerClass().empty();
        bool ValidCount = Op == '-' ? Count == 1 || Count == 2
                         : Op == tok_not ? Count == 1 : Count == 2;
        if (!ValidCount) {
            reportError("Invalid operand count for %s", Proto->getName().c_str());
            return false;
        }
        bool HasUserOperand = !Proto->getOwnerClass().empty();
        for (const auto &P : Proto->getParams()) {
            if (P.IsArray) {
                reportError("An operator parameter cannot be a whole array");
                return false;
            }
            const TypeInfo *T = Types.resolve(P.TypeName);
            HasUserOperand |= T && (T->isRecord() || T->isEnum());
        }
        if (!HasUserOperand) {
            reportError("%s requires a record, class or enum operand", Proto->getName().c_str());
            return false;
        }
    }
    if (Proto->isOverload() && Proto->getOwnerClass().empty() && sameIdentifier(Proto->getName(), "MAIN")) {
        reportError("MAIN cannot be declared OVERLOAD");
        return false;
    }
    if (Proto->getOwnerClass().empty()) {
        if (!Proto->isExternal() && isReservedRuntimeName(Proto->getName())) {
            reportError("'%s' is a reserved runtime symbol and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return false;
        }
        if (CodeGen::isBuiltinName(Proto->getName())) {
            reportError("'%s' is a built-in function and cannot be redefined",
                    Proto->getName().c_str());
            return false;
        }
        if (Types.lookupEnumConstant(Proto->getName())) {
            reportError("'%s' is an enum value and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return false;
        }
        if (Types.resolve(Proto->getName())) {
            reportError("'%s' is a type name and cannot be a FUNCTION/PROCEDURE name",
                    Proto->getName().c_str());
            return false;
        }
    }
    return true;
}

Function *FunctionGen::emitPrototype(PrototypeAST *Proto) {
    auto Registered = PrototypeLinks.find(Proto);
    if (Registered != PrototypeLinks.end()) return Module.getFunction(Registered->second);
    if (!validatePrototype(Proto)) return nullptr;

    FuncSig Sig;
    Sig.OwnerUnit = CurrentUnit;
    Sig.Name = Proto->getLinkName();
    Sig.IsOverload = Proto->isOverload();
    Sig.Prototype = Proto;
    Sig.ReturnTypeName = Proto->getReturnType();

    std::vector<Type*> ArgTypes;
    if (!Proto->getOwnerClass().empty()) ArgTypes.push_back(PointerType::getUnqual(Context));
    for (const ParamDecl &P : Proto->getParams()) {
        const TypeInfo *Info = Types.resolve(P.TypeName);
        if (Info && Info->isVoid()) {
            reportError("parameter %s cannot have void type", P.Name.c_str());
            return nullptr;
        }
        ParamSig PS;
        PS.TypeName = P.TypeName;
        PS.IsByRef = P.Mode == PassMode::ByRef;
        PS.IsArray = P.IsArray;
        PS.Rank = P.Rank;
        PS.DeclaredBounds = P.DeclaredBounds;

        if (P.IsArray) {
            getLLVMType(P.TypeName); // diagnose an unknown element type
            ArgTypes.push_back(PointerType::getUnqual(Context)); // data pointer
            for (unsigned D = 0; D < static_cast<unsigned>(P.Rank) * kBoundArgsPerDim; ++D) {
                ArgTypes.push_back(Type::getInt64Ty(Context));
            }
        } else {
            Type *T = getLLVMType(P.TypeName);
            if (PS.IsByRef) {
                T = T->getPointerTo();
            }
            ArgTypes.push_back(T);
        }
        if (!Sig.ParamKey.empty()) Sig.ParamKey += ",";
        if (P.IsArray) Sig.ParamKey += "ARRAY" + std::to_string(P.Rank) + " OF ";
        Sig.ParamKey += Info ? (Info->isC() ? "C:" + Info->CIdentity : Info->Name) : P.TypeName;
        Sig.Params.push_back(std::move(PS));
    }

    Type *RetType = getLLVMType(Proto->getReturnType());
    FunctionType *FT = FunctionType::get(RetType, ArgTypes, false);
    auto &Overloads = Signatures[Sig.Name];
    for (const FuncSig &Existing : Overloads) {
        if (Existing.IsExternal) {
            reportError("Function %s is already declared EXTERN", Proto->getName().c_str());
            return nullptr;
        }
        if (!Sig.IsOverload || !Existing.IsOverload) {
            reportError("Function %s cannot be redefined. Multiple definition of %s; every overload must be declared OVERLOAD",
                        Proto->getName().c_str(), Proto->getName().c_str());
            return nullptr;
        }
        if (sameIdentifier(Existing.ParamKey, Sig.ParamKey)) {
            if (Existing.OwnerUnit != CurrentUnit)
                reportError("Duplicate definition %s in %s and %s", Sig.Name.c_str(),
                            UnitPaths[Existing.OwnerUnit].c_str(), UnitPaths[CurrentUnit].c_str());
            else reportError("Function %s cannot be redefined", Proto->getName().c_str());
            return nullptr;
        }
    }
    Sig.LinkName = Sig.Name + (Sig.IsOverload ? "(" + Sig.ParamKey + ")" : "");
    Function *F = Function::Create(FT,
        Proto->getOwnerClass().empty() && sameIdentifier(Proto->getName(), "MAIN")
            ? Function::InternalLinkage : Function::ExternalLinkage,
        Sig.LinkName, &Module);
    Exceptions.prepareFunction(F);
    PrototypeLinks.emplace(Proto, Sig.LinkName);
    LinkIndex.emplace(Sig.LinkName, std::make_pair(Sig.Name, Overloads.size()));
    Overloads.push_back(std::move(Sig));

    Function::arg_iterator AI = F->arg_begin();
    if (!Proto->getOwnerClass().empty()) (AI++)->setName("this");
    for (const ParamDecl &P : Proto->getParams()) {
        if (AI == F->arg_end()) break;
        (AI++)->setName(P.Name);
        for (int D = 1; P.IsArray && D <= P.Rank && AI != F->arg_end(); ++D) {
            (AI++)->setName(P.Name + kLowerBoundSuffix + std::to_string(D));
            if (AI != F->arg_end())
                (AI++)->setName(P.Name + kUpperBoundSuffix + std::to_string(D));
        }
    }
    return F;
}

// The prototype names the exact C symbol; parameters and the result must be C
// types. clang arranges the signature for the host C ABI.
bool FunctionGen::emitExternPrototype(PrototypeAST *Proto) {
    if (PrototypeLinks.count(Proto)) return true;
    const std::string &Name = Proto->getName();
    if (sameIdentifier(Name, "MAIN")) {
        reportError("MAIN cannot be declared EXTERN");
        return false;
    }
    if (CodeGen::isBuiltinName(Name)) {
        reportError("'%s' is a built-in function and cannot be redefined", Name.c_str());
        return false;
    }
    if (Types.lookupEnumConstant(Name) || Types.resolve(Name)) {
        reportError("'%s' is already a type or enum value name", Name.c_str());
        return false;
    }

    // Records cross the C boundary only as C structs, and are not yet
    // returned by value. What names the parameter or return type in diagnostics.
    auto CheckRecord = [&](const TypeInfo *Info, const std::string &What, bool IsResult) {
        const TypeInfo *Record = Info->Kind == TypeKind::Pointer
            ? Types.resolve(Info->Pointer.PointeeTypeName) : Info;
        if (!Record || !Record->isRecord() || Record->IsClass) return true;
        std::string Why;
        if (!Types.isCStruct(Record, &Why)) {
            reportError("%s uses record %s, which is not a C struct: %s", What.c_str(),
                        Record->Name.c_str(), Why.c_str());
            return false;
        }
        if (IsResult && Record == Info) {
            reportError("%s cannot be record %s by value; use a pointer type to it", What.c_str(),
                        Record->Name.c_str());
            return false;
        }
        return true;
    };

    FuncSig Sig;
    Sig.OwnerUnit = CurrentUnit;
    Sig.Name = Name;
    Sig.LinkName = Name;
    Sig.Prototype = Proto;
    Sig.ReturnTypeName = Proto->getReturnType();
    Sig.IsExternal = true;
    Sig.IsVariadic = Proto->isVariadic();

    std::vector<const TypeInfo *> ArgTypes;
    for (size_t I = 0; I < Proto->getParams().size(); ++I) {
        const ParamDecl &P = Proto->getParams()[I];
        const TypeInfo *Info = Types.resolve(P.TypeName);
        if (!Info) {
            reportError("Unknown type %s", P.TypeName.c_str());
            return false;
        }
        if (!CheckRecord(Info, "EXTERN parameter " + std::to_string(I + 1) + " of " + Name, false))
            return false;
        if (!Types.isExternType(Info)) {
            reportError("EXTERN parameter %zu of %s must be a non-void C type (got %s)",
                        I + 1, Name.c_str(), P.TypeName.c_str());
            return false;
        }
        ArgTypes.push_back(Info);
        ParamSig PS;
        PS.TypeName = P.TypeName;
        Sig.Params.push_back(std::move(PS));
    }
    const TypeInfo *RetType = nullptr;
    if (!sameIdentifier(Proto->getReturnType(), "VOID")) {
        const TypeInfo *Info = Types.resolve(Proto->getReturnType());
        if (!Info) {
            reportError("Unknown type %s", Proto->getReturnType().c_str());
            return false;
        }
        if (!CheckRecord(Info, "The return type of EXTERN_FUNCTION " + Name, true)) return false;
        if (!Types.isExternType(Info)) {
            reportError("EXTERN_FUNCTION %s must return a non-void C type (got %s)",
                        Name.c_str(), Proto->getReturnType().c_str());
            return false;
        }
        RetType = Info;
    }

    // Repeated identical EXTERN declarations, as in C, share one symbol; each
    // keeps its own unit so visibility follows the declaring file.
    auto &Overloads = Signatures[Sig.Name];
    if (!Overloads.empty() && Overloads.front().IsExternal) {
        const FuncSig &First = Overloads.front();
        auto Same = [&](const std::string &A, const std::string &B) {
            if (sameIdentifier(A, "VOID") || sameIdentifier(B, "VOID"))
                return sameIdentifier(A, B);
            const TypeInfo *L = Types.resolve(A);
            return L && L->sameType(Types.resolve(B));
        };
        bool Matches = First.IsVariadic == Sig.IsVariadic && First.Params.size() == Sig.Params.size() &&
                       Same(First.ReturnTypeName, Sig.ReturnTypeName);
        for (size_t I = 0; Matches && I < Sig.Params.size(); ++I)
            Matches = Same(First.Params[I].TypeName, Sig.Params[I].TypeName);
        if (!Matches) {
            reportError("Conflicting EXTERN declarations of %s", Name.c_str());
            return false;
        }
        Sig.ExternABI = First.ExternABI;
        Sig.ExternType = First.ExternType;
        Sig.ExternAttrs = First.ExternAttrs;
        PrototypeLinks.emplace(Proto, Name);
        Overloads.push_back(std::move(Sig));
        return true;
    }
    if (!Overloads.empty()) {
        if (Overloads.front().OwnerUnit != CurrentUnit)
            reportError("Duplicate definition %s in %s and %s", Name.c_str(),
                        UnitPaths[Overloads.front().OwnerUnit].c_str(), UnitPaths[CurrentUnit].c_str());
        else reportError("EXTERN %s conflicts with FUNCTION/PROCEDURE %s", Name.c_str(), Name.c_str());
        return false;
    }
    std::string Err;
    if (!ABI.arrange(RetType, ArgTypes, ArgTypes.size(), Sig.IsVariadic, Sig.ExternABI, Err)) {
        reportError("Internal: cannot arrange EXTERN %s: %s", Name.c_str(), Err.c_str());
        return false;
    }
    Sig.ExternType = getCABIFunctionType(Sig.ExternABI);
    Sig.ExternAttrs = getCABIAttributes(Context, Sig.ExternABI);
    FunctionCallee Declared = Module.getOrInsertFunction(Name, Sig.ExternType);
    if (auto *F = dyn_cast<Function>(Declared.getCallee()))
        F->setCallingConv(Sig.ExternABI.CallingConv);
    PrototypeLinks.emplace(Proto, Name);
    Overloads.push_back(std::move(Sig));
    return true;
}

Function *FunctionGen::emitFunctionDef(FunctionDefAST *FuncAST,
                                       const std::function<void(StmtAST*)> &StmtEmitter, unsigned ScopeID) {
    PrototypeAST *Proto = FuncAST->getProto();
    Function *TheFunction = emitPrototype(Proto);

    if (!TheFunction) return nullptr;
    if (!TheFunction->empty()) {
        reportError("Function %s cannot be redefined.", Proto->getName().c_str());
        return nullptr;
    }

    BasicBlock *BB = BasicBlock::Create(Context, "entry", TheFunction);
    Builder.SetInsertPoint(BB);
    Exceptions.enableCleanups(TheFunction);

    IdentifierMap<SymbolInfo> OldSymbols = Symbols;
    Symbols.clear();
    for (const auto &[Name, Info] : OldSymbols) {
        if (Info.IsGlobal || (Info.isConstant() && Info.ScopeID == 0)) Symbols.emplace(Name, Info);
    }

    bool IsMethod = !Proto->getOwnerClass().empty();
    if (IsMethod) {
        const TypeInfo *Class = Types.resolve(Proto->getOwnerClass());
        Value *Self = TheFunction->getArg(0);
        if (Class && Class->asRecord()) {
            for (const auto &[Name, Field] : Class->asRecord()->Fields) {
                Value *Addr = Builder.CreateStructGEP(Class->LLVMType, Self, Field.Index, Name);
                Symbols[Name] = {Addr, Field.TypeName, false};
            }
        }
        Symbols["this"] = {Self, Proto->getOwnerClass(), false, nullptr, ScopeID};
    }
    createArgumentAllocas(TheFunction, Proto->getParams(), IsMethod);
    for (const auto &P : Proto->getParams()) {
        auto It = Symbols.find(P.Name);
        if (It != Symbols.end()) It->second.ScopeID = ScopeID;
    }

    for (const auto &Stmt : FuncAST->getBody()) {
        StmtEmitter(Stmt.get());
    }

    if (!Builder.GetInsertBlock()->hasTerminator()) {
        if (sameIdentifier(Proto->getReturnType(), "VOID")) {
            Exceptions.recordExit(Builder.CreateRetVoid());
        } else {
            Exceptions.recordExit(Builder.CreateRet(Constant::getNullValue(TheFunction->getReturnType())));
        }
    }

    Symbols = OldSymbols;
    return TheFunction;
}

llvm::Value *FunctionGen::emitCall(const std::string &Callee, const std::vector<llvm::Value*> &Args) {
    // A link name can be taken by another module global (e.g. FUNCTION stderr).
    Function *CalleeF = Module.getFunction(Callee);
    if (!CalleeF) {
        sourceError(DiagnosticLine, "Call to undefined function %s", Callee.c_str());
        HadError = true;
        return nullptr;
    }

    if (CalleeF->getReturnType()->isVoidTy()) {
        return Exceptions.emitCall(CalleeF, Args);
    }

    return Exceptions.emitCall(CalleeF, Args, "calltmp");
}

llvm::Value *FunctionGen::emitCall(const FuncSig &Sig, const std::vector<llvm::Value*> &Args) {
    if (BeforeCall) BeforeCall(Sig);
    if (!Sig.IsExternal) return emitCall(Sig.LinkName, Args);
    FunctionCallee Callee(Sig.ExternType, Module.getNamedValue(Sig.LinkName));
    // Variadic arguments arrive promoted and pass as they are.
    size_t Fixed = Sig.ExternABI.Fixed;
    Value *SRet = nullptr;
    std::vector<Value *> IRArgs = lowerCABIArguments(
        Builder, Sig.ExternABI, ArrayRef<Value *>(Args).take_front(Fixed), SRet);
    IRArgs.insert(IRArgs.end(), Args.begin() + Fixed, Args.end());
    Value *Call = Sig.ExternType->getReturnType()->isVoidTy()
        ? Exceptions.emitCall(Callee, IRArgs) : Exceptions.emitCall(Callee, IRArgs, "calltmp");
    cast<CallBase>(Call)->setAttributes(Sig.ExternAttrs);
    cast<CallBase>(Call)->setCallingConv(Sig.ExternABI.CallingConv);
    Value *Result = lowerCABIResult(Builder, Sig.ExternABI, Call, SRet);
    return Result ? Result : Call;
}

void FunctionGen::emitReturn(llvm::Value *RetVal) {
    Instruction *CatchCleanup = Exceptions.emitReturnCleanup();
    Instruction *RetInst = RetVal ? Builder.CreateRet(RetVal) : Builder.CreateRetVoid();
    Exceptions.recordExit(CatchCleanup ? CatchCleanup : RetInst);
}
