#pragma once
#include "cps/TemplateAST.h"
#include "cps/Identifier.h"
#include <functional>

namespace cps::detail {

using TemplateBindings = IdentifierMap<std::vector<std::string>>;

// Clones own all children; source ASTs remain reusable for later instances.
class TemplateClone {
    const TemplateBindings &Bindings;
    IdentifierMap<std::vector<std::string>> PackNames;
    std::function<void(int, const std::string &)> Error;
    bool Failed = false;
    void fail(int Line, const std::string &Message);
    std::vector<std::unique_ptr<ExprAST>> arguments(const std::vector<std::unique_ptr<ExprAST>> &Args);
    std::vector<std::unique_ptr<StmtAST>> body(const std::vector<std::unique_ptr<StmtAST>> &Body);
    std::unique_ptr<ExprAST> expression(ExprAST *Expr);
    std::unique_ptr<StmtAST> statement(StmtAST *Stmt);
public:
    TemplateClone(const TemplateBindings &Bindings,
                  std::function<void(int, const std::string &)> Error)
        : Bindings(Bindings), Error(std::move(Error)) {}
    std::string type(const std::string &Name);
    std::unique_ptr<FunctionDefAST> function(const FunctionDefAST *Source,
        const std::string &Name, const std::string &OwnerClass = {});
    std::unique_ptr<ClassTypeDeclAST> klass(const ClassTypeDeclAST *Source,
                                           const std::string &Name);
};

} // namespace cps::detail
