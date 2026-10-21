#pragma once
#include "cps/FunctionAST.h"
#include "cps/TypeAST.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cps {

struct TemplateParam {
    std::string Name;
    bool IsPack = false;
    int Line = 0;
};

class TemplateDeclAST : public StmtAST {
    std::vector<TemplateParam> Params;
    std::unique_ptr<FunctionDefAST> Function;
    std::unique_ptr<ClassTypeDeclAST> Class;
public:
    TemplateDeclAST(std::vector<TemplateParam> Params, std::unique_ptr<FunctionDefAST> Function)
        : Params(std::move(Params)), Function(std::move(Function)) {}
    TemplateDeclAST(std::vector<TemplateParam> Params, std::unique_ptr<ClassTypeDeclAST> Class)
        : Params(std::move(Params)), Class(std::move(Class)) {}
    const auto &getParams() const { return Params; }
    const std::string &getName() const {
        return Function ? Function->getProto()->getName() : Class->getName();
    }
    bool isVariadic() const { return !Params.empty() && Params.back().IsPack; }
    FunctionDefAST *getFunction() const { return Function.get(); }
    ClassTypeDeclAST *getClass() const { return Class.get(); }
};

class PackExpansionExprAST : public ExprAST {
    std::string Name;
public:
    PackExpansionExprAST(std::string Name, int Line)
        : ExprAST(Line), Name(std::move(Name)) {}
    const std::string &getName() const { return Name; }
};

} // namespace cps
