#pragma once
#include "cps/AST.h"
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <utility>

namespace cps {

enum class PassMode : uint8_t { ByVal, ByRef };

// One declared FUNCTION/PROCEDURE parameter. For array parameters TypeName
// is the ELEMENT type; DeclaredBounds holds literal per-dimension bounds for
// the bounded form (empty for `ARRAY OF T`, which is rank 1 with the bounds
// travelling alongside the argument).
struct ParamDecl {
    std::string Name;
    std::string TypeName;
    PassMode Mode = PassMode::ByVal;
    bool IsArray = false;
    int Rank = 0; // 0 = scalar
    std::vector<std::pair<int64_t, int64_t>> DeclaredBounds;
};

class PrototypeAST {
    std::string Name;
    std::vector<ParamDecl> Params;
    std::string ReturnType;
    bool IsExternal;

public:
    PrototypeAST(const std::string &Name,
                 std::vector<ParamDecl> Params,
                 const std::string &ReturnType,
                 bool IsExternal = false)
        : Name(Name), Params(std::move(Params)), ReturnType(ReturnType), IsExternal(IsExternal) {}

    const std::string &getName() const { return Name; }
    const std::vector<ParamDecl> &getParams() const { return Params; }
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
    int Line;

public:
    CallStmtAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : Callee(Callee), Args(std::move(Args)), Line(Line) {}

    const std::string &getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
    int getLine() const { return Line; }
};


class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> RetVal;

public:
    ReturnStmtAST(std::unique_ptr<ExprAST> RetVal = nullptr) 
        : RetVal(std::move(RetVal)) {}
        
    ExprAST *getRetVal() const { return RetVal.get(); }
};

} // namespace cps
