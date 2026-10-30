#include "TemplateClone.h"
#include "cps/FileAST.h"
#include <cctype>

using namespace cps;
using namespace cps::detail;

namespace {
template<class T, class U> std::unique_ptr<T> located(std::unique_ptr<T> Copy, const U *Source) {
    Copy->setLine(Source->getLine());
    return Copy;
}
}

void TemplateClone::fail(int Line, const std::string &Message) {
    Failed = true;
    Error(Line, Message);
}

std::string TemplateClone::type(const std::string &Name) {
    size_t Position = 0;
    auto Join = [](const std::vector<std::string> &Parts) {
        std::string Result;
        for (const auto &Part : Parts) {
            if (!Result.empty()) Result += ',';
            Result += Part;
        }
        return Result;
    };
    std::function<std::vector<std::string>()> Parse = [&]() -> std::vector<std::string> {
        size_t Start = Position;
        while (Position < Name.size() &&
               (std::isalnum(static_cast<unsigned char>(Name[Position])) || Name[Position] == '_')) ++Position;
        if (Position == Start) {
            fail(0, "Invalid canonical template type " + Name);
            Position = Name.size();
            return {};
        }
        std::string Base = Name.substr(Start, Position - Start);
        bool HasArguments = Position < Name.size() && Name[Position] == '<';
        if (HasArguments) {
            ++Position;
            std::vector<std::string> Arguments;
            while (Position < Name.size() && Name[Position] != '>') {
                auto Expanded = Parse();
                Arguments.insert(Arguments.end(), Expanded.begin(), Expanded.end());
                if (Position == Name.size() || Name[Position] != ',') break;
                ++Position;
            }
            if (Position == Name.size() || Name[Position] != '>') {
                fail(0, "Invalid canonical template type " + Name);
                return {};
            }
            ++Position;
            Base += "<" + Join(Arguments) + ">";
        }
        bool Expand = Name.compare(Position, 3, "...") == 0;
        if (Expand) Position += 3;
        auto Binding = Bindings.find(Base);
        if (Binding == Bindings.end()) return {Base};
        if (Expand) return Binding->second;
        if (Binding->second.size() != 1) {
            fail(0, "A type pack requires an expansion");
            return {};
        }
        return Binding->second;
    };
    auto Types = Parse();
    if (Position != Name.size()) fail(0, "Invalid canonical template type " + Name);
    return Join(Types);
}

std::vector<std::unique_ptr<ExprAST>> TemplateClone::arguments(
    const std::vector<std::unique_ptr<ExprAST>> &Args) {
    std::vector<std::unique_ptr<ExprAST>> Result;
    for (const auto &Arg : Args) {
        if (auto *Pack = dynamic_cast<PackExpansionExprAST *>(Arg.get())) {
            auto Names = PackNames.find(Pack->getName());
            if (Names == PackNames.end()) fail(Pack->getLine(), "Unknown parameter pack " + Pack->getName());
            else for (const auto &Name : Names->second)
                Result.push_back(located(std::make_unique<VariableExprAST>(Name), Pack));
        } else Result.push_back(expression(Arg.get()));
    }
    return Result;
}

std::vector<std::unique_ptr<StmtAST>> TemplateClone::body(
    const std::vector<std::unique_ptr<StmtAST>> &Body) {
    std::vector<std::unique_ptr<StmtAST>> Result;
    for (const auto &Stmt : Body) Result.push_back(statement(Stmt.get()));
    return Result;
}

std::unique_ptr<ExprAST> TemplateClone::expression(ExprAST *Expr) {
    if (!Expr) return nullptr;
    std::unique_ptr<ExprAST> Copy;
    if (auto *E = dynamic_cast<IntegerExprAST *>(Expr)) Copy = std::make_unique<IntegerExprAST>(E->getVal());
    else if (auto *E = dynamic_cast<RealExprAST *>(Expr)) Copy = std::make_unique<RealExprAST>(E->getVal());
    else if (auto *E = dynamic_cast<BooleanExprAST *>(Expr)) Copy = std::make_unique<BooleanExprAST>(E->getVal());
    else if (auto *E = dynamic_cast<CharExprAST *>(Expr)) Copy = std::make_unique<CharExprAST>(E->getVal());
    else if (auto *E = dynamic_cast<StringExprAST *>(Expr)) Copy = std::make_unique<StringExprAST>(E->getVal(), E->isCString());
    else if (auto *E = dynamic_cast<VariableExprAST *>(Expr)) Copy = std::make_unique<VariableExprAST>(E->getName());
    else if (auto *E = dynamic_cast<ArrayAccessExprAST *>(Expr)) {
        std::vector<std::unique_ptr<ExprAST>> Indices;
        for (const auto &Index : E->getIndices()) Indices.push_back(expression(Index.get()));
        Copy = std::make_unique<ArrayAccessExprAST>(E->getName(), std::move(Indices), E->getLine());
    } else if (auto *E = dynamic_cast<UnaryExprAST *>(Expr)) Copy = std::make_unique<UnaryExprAST>(E->getOp(), expression(E->getOperand()));
    else if (auto *E = dynamic_cast<BinaryExprAST *>(Expr)) Copy = std::make_unique<BinaryExprAST>(E->getOp(), expression(E->getLHS()), expression(E->getRHS()), E->getLine());
    else if (auto *E = dynamic_cast<CallExprAST *>(Expr)) {
        std::vector<std::string> Types;
        for (const auto &Arg : E->getTemplateArgs()) {
            std::string Substituted = type(Arg);
            // A template argument pack expands into separate argument slots.
            size_t Start = 0, Depth = 0;
            for (size_t I = 0; I <= Substituted.size(); ++I) {
                if (I < Substituted.size() && Substituted[I] == '<') ++Depth;
                if (I < Substituted.size() && Substituted[I] == '>') --Depth;
                if (I == Substituted.size() || (!Depth && Substituted[I] == ',')) {
                    if (I > Start) Types.push_back(Substituted.substr(Start, I - Start));
                    Start = I + 1;
                }
            }
        }
        Copy = std::make_unique<CallExprAST>(E->getCallee(), arguments(E->getArgs()), E->getLine(), std::move(Types));
    } else if (auto *E = dynamic_cast<MemberCallExprAST *>(Expr)) Copy = std::make_unique<MemberCallExprAST>(expression(E->getReceiver()), E->getMethod(), arguments(E->getArgs()), E->getLine());
    else if (auto *E = dynamic_cast<NewExprAST *>(Expr)) Copy = std::make_unique<NewExprAST>(type(E->getClassName()), arguments(E->getArgs()), E->getLine());
    else if (auto *E = dynamic_cast<CastExprAST *>(Expr)) Copy = std::make_unique<CastExprAST>(type(E->getTypeName()), expression(E->getOperand()));
    else if (auto *E = dynamic_cast<DesignatorExprAST *>(Expr)) {
        std::vector<DesignatorAccess> Accesses;
        for (const auto &A : E->getAccesses()) {
            DesignatorAccess CopyAccess{A.Kind, {}, A.FieldName};
            for (const auto &Index : A.Indices) CopyAccess.Indices.push_back(expression(Index.get()));
            Accesses.push_back(std::move(CopyAccess));
        }
        Copy = std::make_unique<DesignatorExprAST>(E->getBaseName(), std::move(Accesses), E->getLine());
    } else if (auto *E = dynamic_cast<AddrOfExprAST *>(Expr)) {
        auto Target = expression(E->getTarget());
        Copy = std::make_unique<AddrOfExprAST>(std::unique_ptr<DesignatorExprAST>(static_cast<DesignatorExprAST *>(Target.release())));
    } else if (dynamic_cast<PackExpansionExprAST *>(Expr)) fail(Expr->getLine(), "A parameter pack expansion is only allowed in an argument list");
    else fail(Expr->getLine(), "Unsupported expression in template body");
    if (Copy) Copy->setLine(Expr->getLine());
    return Copy;
}

std::unique_ptr<StmtAST> TemplateClone::statement(StmtAST *Stmt) {
    std::unique_ptr<StmtAST> Copy;
    if (auto *S = dynamic_cast<DeclareStmtAST *>(Stmt)) Copy = std::make_unique<DeclareStmtAST>(S->getName(), type(S->getType()), arguments(S->getArgs()), S->hasParens());
    else if (auto *S = dynamic_cast<ArrayDeclareStmtAST *>(Stmt)) {
        std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Bounds;
        for (const auto &B : S->getBounds()) Bounds.emplace_back(expression(B.first.get()), expression(B.second.get()));
        Copy = std::make_unique<ArrayDeclareStmtAST>(S->getName(), std::move(Bounds), type(S->getType()));
    } else if (auto *S = dynamic_cast<ConstantStmtAST *>(Stmt)) Copy = std::make_unique<ConstantStmtAST>(S->getName(), expression(S->getValue()));
    else if (auto *S = dynamic_cast<AssignStmtAST *>(Stmt)) Copy = std::make_unique<AssignStmtAST>(S->getName(), expression(S->getExpr()));
    else if (auto *S = dynamic_cast<ArrayAssignStmtAST *>(Stmt)) {
        std::vector<std::unique_ptr<ExprAST>> Indices;
        for (const auto &Index : S->getIndices()) Indices.push_back(expression(Index.get()));
        Copy = std::make_unique<ArrayAssignStmtAST>(S->getName(), std::move(Indices), expression(S->getExpr()), S->getLine());
    } else if (auto *S = dynamic_cast<DesignatorAssignStmtAST *>(Stmt)) {
        auto Target = expression(S->getTarget());
        Copy = std::make_unique<DesignatorAssignStmtAST>(std::unique_ptr<DesignatorExprAST>(static_cast<DesignatorExprAST *>(Target.release())), expression(S->getExpr()));
    } else if (auto *S = dynamic_cast<InputStmtAST *>(Stmt)) Copy = std::make_unique<InputStmtAST>(expression(S->getTarget()));
    else if (auto *S = dynamic_cast<DeleteStmtAST *>(Stmt)) Copy = std::make_unique<DeleteStmtAST>(expression(S->getTarget()));
    else if (auto *S = dynamic_cast<OutputStmtAST *>(Stmt)) {
        std::vector<std::unique_ptr<ExprAST>> Exprs;
        for (const auto &E : S->getExprs()) Exprs.push_back(expression(E.get()));
        Copy = std::make_unique<OutputStmtAST>(std::move(Exprs));
    } else if (auto *S = dynamic_cast<ReturnStmtAST *>(Stmt)) Copy = std::make_unique<ReturnStmtAST>(expression(S->getRetVal()));
    else if (auto *S = dynamic_cast<CallStmtAST *>(Stmt)) {
        CallExprAST Call(S->getCallee(), {}, S->getLine(), S->getTemplateArgs());
        auto Expr = expression(&Call);
        auto *Cloned = static_cast<CallExprAST *>(Expr.get());
        Copy = std::make_unique<CallStmtAST>(S->getCallee(), arguments(S->getArgs()), S->getLine(), Cloned->getTemplateArgs());
    } else if (auto *S = dynamic_cast<MemberCallStmtAST *>(Stmt)) {
        auto Call = expression(S->getCall());
        Copy = std::make_unique<MemberCallStmtAST>(std::unique_ptr<MemberCallExprAST>(static_cast<MemberCallExprAST *>(Call.release())));
    } else if (auto *S = dynamic_cast<IfStmtAST *>(Stmt)) Copy = std::make_unique<IfStmtAST>(expression(S->getCond()), body(S->getThenStmts()), body(S->getElseStmts()));
    else if (auto *S = dynamic_cast<WhileStmtAST *>(Stmt)) Copy = std::make_unique<WhileStmtAST>(expression(S->getCond()), body(S->getBody()));
    else if (auto *S = dynamic_cast<RepeatStmtAST *>(Stmt)) Copy = std::make_unique<RepeatStmtAST>(body(S->getBody()), expression(S->getCond()));
    else if (auto *S = dynamic_cast<ForStmtAST *>(Stmt)) Copy = std::make_unique<ForStmtAST>(S->getVarName(), expression(S->getStart()), expression(S->getEnd()), expression(S->getStep()), body(S->getBody()));
    else if (auto *S = dynamic_cast<CaseStmtAST *>(Stmt)) {
        std::vector<CaseBranch> Branches;
        for (const auto &B : S->getBranches()) Branches.push_back({expression(B.Lower.get()), expression(B.Upper.get()), body(B.Body), B.Line});
        Copy = std::make_unique<CaseStmtAST>(S->getSelector(), std::move(Branches));
    } else if (auto *S = dynamic_cast<TryStmtAST *>(Stmt)) Copy = std::make_unique<TryStmtAST>(body(S->getBody()), body(S->getCatchBody()));
    else if (dynamic_cast<ThrowStmtAST *>(Stmt)) Copy = std::make_unique<ThrowStmtAST>();
    else if (auto *S = dynamic_cast<OpenFileStmtAST *>(Stmt)) Copy = std::make_unique<OpenFileStmtAST>(expression(S->getFileName()), S->getMode(), S->getLine());
    else if (auto *S = dynamic_cast<ReadFileStmtAST *>(Stmt)) Copy = std::make_unique<ReadFileStmtAST>(expression(S->getFileName()), S->getVarName(), S->getLine());
    else if (auto *S = dynamic_cast<WriteFileStmtAST *>(Stmt)) Copy = std::make_unique<WriteFileStmtAST>(expression(S->getFileName()), expression(S->getData()), S->getLine());
    else if (auto *S = dynamic_cast<CloseFileStmtAST *>(Stmt)) Copy = std::make_unique<CloseFileStmtAST>(expression(S->getFileName()), S->getLine());
    else if (dynamic_cast<FunctionDefAST *>(Stmt)) fail(Stmt->getLine(), "Nested FUNCTION or PROCEDURE definitions are not allowed in template bodies");
    else fail(Stmt->getLine(), "Unsupported declaration in template body");
    if (Copy) Copy->setLine(Stmt->getLine());
    return Copy;
}

std::unique_ptr<FunctionDefAST> TemplateClone::function(const FunctionDefAST *Source,
    const std::string &Name, const std::string &OwnerClass) {
    auto SavedNames = PackNames;
    const auto *Proto = Source->getProto();
    std::vector<ParamDecl> Params;
    for (const auto &P : Proto->getParams()) {
        if (P.IsPack) {
            auto Binding = Bindings.find(P.TypeName);
            if (Binding == Bindings.end()) { fail(P.Line, "Unbound template parameter pack"); continue; }
            auto &Names = PackNames[P.Name];
            Names.clear();
            for (size_t I = 0; I < Binding->second.size(); ++I) {
                ParamDecl Copy = P;
                Copy.Name += "#" + std::to_string(I + 1);
                Copy.TypeName = Binding->second[I];
                Copy.IsPack = false;
                Names.push_back(Copy.Name);
                Params.push_back(std::move(Copy));
            }
        } else {
            Params.push_back(P);
            Params.back().TypeName = type(P.TypeName);
        }
    }
    auto CopyProto = std::make_unique<PrototypeAST>(Name, std::move(Params), type(Proto->getReturnType()),
        Proto->isExternal(), OwnerClass, Proto->isPublic(), Proto->isOverload());
    CopyProto->setOperator(Proto->getOperator());
    CopyProto->setDefaultReturn(Proto->isDefaultReturn());
    CopyProto->setImplicit(Proto->isImplicit());
    CopyProto->setVariadic(Proto->isVariadic());
    auto Copy = located(std::make_unique<FunctionDefAST>(std::move(CopyProto), body(Source->getBody())), Source);
    PackNames = std::move(SavedNames);
    return Failed ? nullptr : std::move(Copy);
}

std::unique_ptr<ClassTypeDeclAST> TemplateClone::klass(const ClassTypeDeclAST *Source,
    const std::string &Name) {
    std::vector<RecordFieldDecl> Fields = Source->getFields();
    for (auto &Field : Fields) Field.TypeName = type(Field.TypeName);
    std::vector<std::unique_ptr<FunctionDefAST>> Methods;
    for (const auto &Method : Source->getMethods())
        Methods.push_back(function(Method.get(), Method->getProto()->getName(), Name));
    auto Copy = located(std::make_unique<ClassTypeDeclAST>(Name, std::move(Fields), std::move(Methods)), Source);
    return Failed ? nullptr : std::move(Copy);
}
