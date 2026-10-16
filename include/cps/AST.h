#pragma once
#include <string>
#include <memory>
#include <vector>
#include <cstdint>

namespace cps {

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class IntegerExprAST : public ExprAST {
    int64_t Val;
public:
    IntegerExprAST(int64_t Val) : Val(Val) {}
    int64_t getVal() const { return Val; }
};

class RealExprAST : public ExprAST {
    double Val;
public:
    RealExprAST(double Val) : Val(Val) {}
    double getVal() const { return Val; }
};

class BooleanExprAST : public ExprAST {
    bool Val;
public:
    BooleanExprAST(bool Val) : Val(Val) {}
    bool getVal() const { return Val; }
};

class CharExprAST : public ExprAST {
    char Val;
public:
    CharExprAST(char Val) : Val(Val) {}
    char getVal() const { return Val; }
};

class StringExprAST : public ExprAST {
    std::string Val;
public:
    StringExprAST(const std::string &Val) : Val(Val) {}
    const std::string &getVal() const { return Val; }
};

class VariableExprAST : public ExprAST {
    std::string Name;
public:
    VariableExprAST(const std::string &Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
};

class ArrayAccessExprAST : public ExprAST {
    std::string Name;
    std::vector<std::unique_ptr<ExprAST>> Indices;
    int Line;
public:
    ArrayAccessExprAST(const std::string &Name, std::vector<std::unique_ptr<ExprAST>> Indices, int Line)
        : Name(Name), Indices(std::move(Indices)), Line(Line) {}
    const std::string &getName() const { return Name; }
    const std::vector<std::unique_ptr<ExprAST>> &getIndices() const { return Indices; }
    int getLine() const { return Line; }
};

class UnaryExprAST : public ExprAST {
    int Op;
    std::unique_ptr<ExprAST> Operand;
public:
    UnaryExprAST(int Op, std::unique_ptr<ExprAST> Operand)
        : Op(Op), Operand(std::move(Operand)) {}
    int getOp() const { return Op; }
    ExprAST *getOperand() const { return Operand.get(); }
};

class BinaryExprAST : public ExprAST {
    int Op;
    std::unique_ptr<ExprAST> LHS, RHS;
    int Line;
public:
    BinaryExprAST(int Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS, int Line)
        : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)), Line(Line) {}
    int getOp() const { return Op; }
    ExprAST *getLHS() const { return LHS.get(); }
    ExprAST *getRHS() const { return RHS.get(); }
    int getLine() const { return Line; }
};

class StmtAST {
public:
    virtual ~StmtAST() = default;
};

class TryStmtAST : public StmtAST {
    std::vector<std::unique_ptr<StmtAST>> Body, CatchBody;
public:
    TryStmtAST(std::vector<std::unique_ptr<StmtAST>> Body,
               std::vector<std::unique_ptr<StmtAST>> CatchBody)
        : Body(std::move(Body)), CatchBody(std::move(CatchBody)) {}
    const std::vector<std::unique_ptr<StmtAST>> &getBody() const { return Body; }
    const std::vector<std::unique_ptr<StmtAST>> &getCatchBody() const { return CatchBody; }
};

class ThrowStmtAST : public StmtAST {};

class DeclareStmtAST : public StmtAST {
    std::string Name;
    std::string Type;
public:
    DeclareStmtAST(const std::string &Name, const std::string &Type = "INTEGER") 
        : Name(Name), Type(Type) {}
    const std::string &getName() const { return Name; }
    const std::string &getType() const { return Type; }
};

class ArrayDeclareStmtAST : public StmtAST {
    std::string Name;
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Bounds; 
    std::string Type;
public:
    ArrayDeclareStmtAST(const std::string &Name, 
        std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Bounds,
        const std::string &Type)
        : Name(Name), Bounds(std::move(Bounds)), Type(Type) {}
    
    const std::string &getName() const { return Name; }
    const std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> &getBounds() const { return Bounds; }
    const std::string &getType() const { return Type; }
};

class AssignStmtAST : public StmtAST {
    std::string Name;
    std::unique_ptr<ExprAST> Expr;
public:
    AssignStmtAST(const std::string &Name, std::unique_ptr<ExprAST> Expr)
        : Name(Name), Expr(std::move(Expr)) {}
    const std::string &getName() const { return Name; }
    ExprAST *getExpr() const { return Expr.get(); }
};

class ArrayAssignStmtAST : public StmtAST {
    std::string Name;
    std::vector<std::unique_ptr<ExprAST>> Indices;
    std::unique_ptr<ExprAST> Expr;
    int Line;
public:
    ArrayAssignStmtAST(const std::string &Name, std::vector<std::unique_ptr<ExprAST>> Indices, std::unique_ptr<ExprAST> Expr, int Line)
        : Name(Name), Indices(std::move(Indices)), Expr(std::move(Expr)), Line(Line) {}
    const std::string &getName() const { return Name; }
    const std::vector<std::unique_ptr<ExprAST>> &getIndices() const { return Indices; }
    ExprAST *getExpr() const { return Expr.get(); }
    int getLine() const { return Line; }
};

class InputStmtAST : public StmtAST {
    std::string Name;
public:
    InputStmtAST(const std::string &Name) : Name(Name) {}
    const std::string &getName() const { return Name; }
};

class OutputStmtAST : public StmtAST {
    std::vector<std::unique_ptr<ExprAST>> Exprs;
public:
    OutputStmtAST(std::vector<std::unique_ptr<ExprAST>> Exprs) : Exprs(std::move(Exprs)) {}
    const std::vector<std::unique_ptr<ExprAST>> &getExprs() const { return Exprs; }
};

struct CaseBranch {
    std::unique_ptr<ExprAST> Lower, Upper; // Null Lower denotes OTHERWISE.
    std::vector<std::unique_ptr<StmtAST>> Body;
    int Line;
};

class CaseStmtAST : public StmtAST {
    std::string Selector;
    std::vector<CaseBranch> Branches;
public:
    CaseStmtAST(std::string Selector, std::vector<CaseBranch> Branches)
        : Selector(std::move(Selector)), Branches(std::move(Branches)) {}
    const std::string &getSelector() const { return Selector; }
    const std::vector<CaseBranch> &getBranches() const { return Branches; }
};

class IfStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<StmtAST>> ThenStmts;
    std::vector<std::unique_ptr<StmtAST>> ElseStmts;
public:
    IfStmtAST(std::unique_ptr<ExprAST> Cond,
              std::vector<std::unique_ptr<StmtAST>> ThenStmts,
              std::vector<std::unique_ptr<StmtAST>> ElseStmts)
        : Cond(std::move(Cond)), ThenStmts(std::move(ThenStmts)), ElseStmts(std::move(ElseStmts)) {}

    ExprAST *getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getThenStmts() const { return ThenStmts; }
    const std::vector<std::unique_ptr<StmtAST>> &getElseStmts() const { return ElseStmts; }
};

class WhileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> Cond;
    std::vector<std::unique_ptr<StmtAST>> Body;
public:
    WhileStmtAST(std::unique_ptr<ExprAST> Cond, std::vector<std::unique_ptr<StmtAST>> Body)
        : Cond(std::move(Cond)), Body(std::move(Body)) {}
    ExprAST *getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getBody() const { return Body; }
};

class RepeatStmtAST : public StmtAST {
    std::vector<std::unique_ptr<StmtAST>> Body;
    std::unique_ptr<ExprAST> Cond;
public:
    RepeatStmtAST(std::vector<std::unique_ptr<StmtAST>> Body, std::unique_ptr<ExprAST> Cond)
        : Body(std::move(Body)), Cond(std::move(Cond)) {}
    ExprAST *getCond() const { return Cond.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getBody() const { return Body; }
};

class ForStmtAST : public StmtAST {
    std::string VarName;
    std::unique_ptr<ExprAST> Start;
    std::unique_ptr<ExprAST> End;
    std::unique_ptr<ExprAST> Step;
    std::vector<std::unique_ptr<StmtAST>> Body;
public:
    ForStmtAST(const std::string &VarName, std::unique_ptr<ExprAST> Start, 
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::vector<std::unique_ptr<StmtAST>> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)), 
          Step(std::move(Step)), Body(std::move(Body)) {}
    const std::string &getVarName() const { return VarName; }
    ExprAST *getStart() const { return Start.get(); }
    ExprAST *getEnd() const { return End.get(); }
    ExprAST *getStep() const { return Step.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getBody() const { return Body; }
};

} // namespace cps
