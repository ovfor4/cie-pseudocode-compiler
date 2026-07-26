#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <cstdio>
#include <tuple>

using namespace cps;

std::optional<std::vector<std::tuple<std::string, std::string, bool>>> Parser::ParsePrototypeArgs() {
    std::vector<std::tuple<std::string, std::string, bool>> Args;
    if (CurTok != '(') return Args;
    getNextToken();

    while (CurTok != ')') {
        if (CurTok == -1000 || CurTok == -1001) getNextToken();

        bool IsRef = false;
        if (CurTok == tok_byref) {
            IsRef = true;
            getNextToken();
        } else if (CurTok == tok_byval) {
            IsRef = false;
            getNextToken();
        }

        if (CurTok != tok_identifier) {
            fprintf(stderr, "Error: Expected argument name\n");
            return std::nullopt;
        }
        std::string Name = Lex.IdentifierStr;
        getNextToken();

        if (CurTok != tok_colon) {
            fprintf(stderr, "Error: Expected ':' after argument name\n");
            return std::nullopt;
        }
        getNextToken();

        std::string Type = ParseTypeName(false);
        if (Type.empty()) {
            fprintf(stderr, "Error: Expected argument type for '%s'\n", Name.c_str());
            return std::nullopt;
        }

        Args.emplace_back(Name, Type, IsRef);

        if (CurTok == ')') break;
        if (CurTok != ',') {
            fprintf(stderr, "Error: Expected ',' or ')'\n");
            return std::nullopt;
        }
        getNextToken();
    }
    getNextToken();
    return Args;
}

std::unique_ptr<StmtAST> Parser::ParseFunction() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected function name\n");
        return nullptr;
    }

    std::string Name = Lex.IdentifierStr;
    getNextToken();

    auto Args = ParsePrototypeArgs();
    if (!Args) return nullptr;

    std::string RetType = "INTEGER";
    if (CurTok == tok_returns) {
        getNextToken();
        RetType = ParseTypeName(true);
        if (RetType.empty()) {
            fprintf(stderr, "Error: Expected return type for function '%s'\n", Name.c_str());
            return nullptr;
        }
    }

    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), RetType);

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endfunction && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_endfunction) getNextToken();
    }

    if (CurTok != tok_endfunction) {
        fprintf(stderr, "Error: expected ENDFUNCTION\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseProcedure() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected procedure name\n");
        return nullptr;
    }

    std::string Name = Lex.IdentifierStr;
    getNextToken();

    auto Args = ParsePrototypeArgs();
    if (!Args) return nullptr;
    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), "VOID");

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endprocedure && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_endprocedure) getNextToken();
    }

    if (CurTok != tok_endprocedure) {
        fprintf(stderr, "Error: expected ENDPROCEDURE\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseCallStmt() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected callee name after CALL\n");
        return nullptr;
    }

    std::string Callee = Lex.IdentifierStr;
    getNextToken();

    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok == '(') {
        getNextToken();
        while (CurTok != ')') {
            auto Arg = ParseExpression();
            if (Arg) Args.push_back(std::move(Arg));
            else return nullptr;
            
            if (CurTok == ')') break;
            if (CurTok != ',') {
                fprintf(stderr, "Error: Expected ',' or ')' in call\n");
                return nullptr;
            }
            getNextToken();
        }
        getNextToken();
    }
    
    return std::make_unique<CallStmtAST>(Callee, std::move(Args));
}

std::unique_ptr<StmtAST> Parser::ParseReturnStmt() {
    getNextToken();
    std::unique_ptr<ExprAST> Expr = nullptr;
    if (CurTok != tok_endif && CurTok != tok_else && CurTok != tok_endfunction && CurTok != tok_endprocedure) {
        Expr = ParseExpression();
    }
    return std::make_unique<ReturnStmtAST>(std::move(Expr));
}
