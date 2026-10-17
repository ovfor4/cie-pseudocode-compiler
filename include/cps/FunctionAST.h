#pragma once
#include "cps/AST.h"
#include "cps/Identifier.h"
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
    int Line = 0;
};

class PrototypeAST {
    std::string Name;
    std::vector<ParamDecl> Params;
    std::string ReturnType;
    bool IsExternal;
    bool IsOverload;
    std::string OwnerClass;
    bool IsPublic;

public:
    PrototypeAST(const std::string &Name,
                 std::vector<ParamDecl> Params,
                 const std::string &ReturnType,
                 bool IsExternal = false, std::string OwnerClass = {}, bool IsPublic = true, bool IsOverload = false)
        : Name(Name), Params(std::move(Params)), ReturnType(ReturnType), IsExternal(IsExternal), IsOverload(IsOverload),
          OwnerClass(std::move(OwnerClass)), IsPublic(IsPublic) {}

    const std::string &getName() const { return Name; }
    const std::vector<ParamDecl> &getParams() const { return Params; }
    const std::string &getReturnType() const { return ReturnType; }
    bool isOverload() const { return IsOverload; }
    bool isExternal() const { return IsExternal; }
    const std::string &getOwnerClass() const { return OwnerClass; }
    bool isPublic() const { return IsPublic; }
    std::string getLinkName() const {
        if (!OwnerClass.empty()) return OwnerClass + "." + Name;
        return sameIdentifier(Name, "MAIN") ? "__cps_main" : Name;
    }
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

public:
    CallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : ExprAST(Line), Callee(Callee), Args(std::move(Args)) {}

    const std::string &getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
};

class CallStmtAST : public StmtAST {
    std::string Callee;
    std::vector<std::unique_ptr<ExprAST>> Args;

public:
    CallStmtAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : StmtAST(Line), Callee(Callee), Args(std::move(Args)) {}

    const std::string &getCallee() const { return Callee; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
};


// A member call keeps the receiver separate from explicit arguments.
class MemberCallExprAST : public ExprAST {
    std::unique_ptr<ExprAST> Receiver;
    std::string Method;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    MemberCallExprAST(std::unique_ptr<ExprAST> Receiver, std::string Method,
                      std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : ExprAST(Line), Receiver(std::move(Receiver)), Method(std::move(Method)), Args(std::move(Args)) {}
    ExprAST *getReceiver() const { return Receiver.get(); }
    const std::string &getMethod() const { return Method; }
    const std::vector<std::unique_ptr<ExprAST>> &getArgs() const { return Args; }
};

class MemberCallStmtAST : public StmtAST {
    std::unique_ptr<MemberCallExprAST> Call;
public:
    explicit MemberCallStmtAST(std::unique_ptr<MemberCallExprAST> Call)
        : StmtAST(Call->getLine()), Call(std::move(Call)) {}
    MemberCallExprAST *getCall() const { return Call.get(); }
};

class ReturnStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> RetVal;

public:
    ReturnStmtAST(std::unique_ptr<ExprAST> RetVal = nullptr) 
        : RetVal(std::move(RetVal)) {}
        
    ExprAST *getRetVal() const { return RetVal.get(); }
};

} // namespace cps
