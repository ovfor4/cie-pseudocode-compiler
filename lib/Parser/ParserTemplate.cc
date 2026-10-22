#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/TypeAST.h"

using namespace cps;
bool Parser::isTemplatePack(const std::string &Name) const {
    for (const auto &P : CurrentTemplate)
        if (P.IsPack && sameIdentifier(P.Name, Name)) return true;
    return false;
}

// TEMPLATE <T, Rest...> [OVERLOAD] FUNCTION/PROCEDURE or CLASS.
// Invalid nesting still consumes the entire declaration before diagnosing it.
std::unique_ptr<StmtAST> Parser::ParseTemplateDecl() {
    int Line = Lex.getLine();
    bool AtTopLevel = BlockDepth == 1 && CurrentTemplate.empty();
    getNextToken();
    if (CurTok != '<') {
        sourceError(Line, "Expected '<' after TEMPLATE");
        return nullptr;
    }
    getNextToken();
    std::vector<TemplateParam> Params;
    IdentifierSet Names;
    bool Invalid = false;
    bool SeenPack = false;
    while (true) {
        if (CurTok != tok_identifier) {
            sourceError(Lex.getLine(), "Expected template parameter name");
            return nullptr;
        }
        TemplateParam P{Lex.IdentifierStr, false, Lex.getLine()};
        if (!Names.insert(P.Name).second) {
            sourceError(P.Line, "Duplicate template parameter '%s'", P.Name.c_str());
            Invalid = true;
        }
        getNextToken();
        if (CurTok == tok_ellipsis) {
            P.IsPack = true;
            if (SeenPack) {
                sourceError(P.Line, "A template may have only one parameter pack");
                Invalid = true;
            }
            SeenPack = true;
            getNextToken();
        }
        Params.push_back(std::move(P));
        if (CurTok == '>') break;
        if (CurTok != ',') {
            sourceError(Lex.getLine(), "Expected ',' or '>' in TEMPLATE");
            return nullptr;
        }
        if (Params.back().IsPack) {
            sourceError(Params.back().Line, "A template parameter pack must be last");
            Invalid = true;
        }
        getNextToken();
    }
    getNextToken();

    auto SavedTemplate = std::move(CurrentTemplate);
    auto SavedTypes = TypeNames;
    CurrentTemplate = Params;
    for (const auto &P : Params) TypeNames.insert(P.Name);
    bool IsOverload = CurTok == tok_overload;
    if (IsOverload) getNextToken();
    std::unique_ptr<StmtAST> Decl;
    if (CurTok == tok_function || CurTok == tok_procedure) {
        bool Operator = Lex.peekToken() == tok_operator;
        if (Operator) {
            sourceError(Line, "Operator templates are not supported");
            Invalid = true;
            int End = CurTok == tok_function ? tok_endfunction : tok_endprocedure;
            do { getNextToken(); } while (CurTok != End && CurTok != tok_eof);
            if (CurTok == End) getNextToken();
        } else {
            auto Function = CurTok == tok_function ? ParseFunction({}, true, IsOverload)
                                                  : ParseProcedure({}, true, IsOverload);
            if (Function) Decl = std::make_unique<TemplateDeclAST>(Params, std::move(Function));
        }
    } else if (CurTok == tok_class && !IsOverload) {
        int ClassLine = Lex.getLine();
        auto Class = ParseTypeDecl();
        if (Class) {
            Class->setLine(ClassLine);
            Decl = std::make_unique<TemplateDeclAST>(Params,
                std::unique_ptr<ClassTypeDeclAST>(static_cast<ClassTypeDeclAST *>(Class.release())));
        }
    } else if (CurTok == tok_type) {
        sourceError(Line, "TEMPLATE cannot prefix TYPE; use CLASS");
        ParseTypeDecl();
        Invalid = true;
    } else {
        sourceError(Line, "TEMPLATE must be followed by FUNCTION, PROCEDURE or CLASS");
        Invalid = true;
    }
    CurrentTemplate = std::move(SavedTemplate);
    TypeNames = std::move(SavedTypes);
    if (!AtTopLevel) {
        sourceError(Line, "TEMPLATE declarations are only allowed at top level");
        Invalid = true;
    }
    if (Invalid || !Decl) return nullptr;
    if (auto *T = dynamic_cast<TemplateDeclAST *>(Decl.get()); T->getClass()) TypeNames.insert(T->getName());
    Decl->setLine(Line);
    return Decl;
}

std::optional<std::vector<std::string>> Parser::ParseTemplateArgs() {
    getNextToken(); // '<'
    std::vector<std::string> Args;
    while (true) {
        int Line = Lex.getLine();
        std::string Type = ParseTypeName(false);
        if (Type.empty()) return std::nullopt;
        if (CurTok == tok_ellipsis) {
            if (!isTemplatePack(Type)) {
                sourceError(Line, "Only the current template parameter pack may use '...' in a type argument");
                return std::nullopt;
            }
            Type += "...";
            getNextToken();
        }
        Args.push_back(std::move(Type));
        if (CurTok == '>') break;
        if (CurTok != ',') {
            sourceError(Lex.getLine(), "Expected ',' or '>' in template arguments");
            return std::nullopt;
        }
        getNextToken();
    }
    getNextToken();
    return Args;
}
