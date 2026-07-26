#pragma once
#include "cps/AST.h"
#include <vector>
#include <string>
#include <memory>
#include <tuple>

namespace cps {

class PrototypeAST {
    std::string Name;
    std::vector<std::tuple<std::string, std::string, bool>> Args;
    std::string ReturnType;
    bool IsExternal;

public:
    PrototypeAST(const std::string &Name, 
                 std::vector<std::tuple<std::string, std::string, bool>> Args, 
                 const std::string &ReturnType,
                 bool IsExternal = false)
        : Name(Name), Args(std::move(Args)), ReturnType(ReturnType), IsExternal(IsExternal) {}

    const std::string &getName() const { return Name; }
    const std::vector<std::tuple<std::string, std::string, bool>> &getArgs() const { return Args; }
    const std::string &getReturnType() const { return ReturnType; }
    bool isExternal() const { return IsExternal; }
};

class FunctionDefAST : public StmtAST {
    std::unique_ptr<PrototypeAST> Proto;
    std::vector<std::unique_ptr<StmtAST>> Body;

public:
    FunctionDefAST(std::unique_ptr<PrototypeAST> Proto, 
                   std::vector<std::unique_ptr<StmtAST>> Body)
        : Proto(std::move(Proto)), Body(std::move(Body)) {}

    PrototypeAST *getProto() const { return Proto.get(); }
    const std::vector<std::unique_ptr<StmtAST>> &getBody() const { return Body; }
};

class CallExprAST : public ExprAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;
    int Line;

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : Callee(Callee), Args(std::move(Args)), Line(Line) {}

    const std::string &getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
    int getLine() const { return Line; }
};

class CallStmtAST : public StmtAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallStmtAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
        : Callee(Callee), Args(std::move(Args)) {}

    const std::string &getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
};


class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> RetVal;

public:
    ReturnStmtAST(std::unique_ptr<ExprAST> RetVal = nullptr) 
        : RetVal(std::move(RetVal)) {}
        
    ExprAST *getRetVal() const { return RetVal.get(); }
};

} // namespace cps
