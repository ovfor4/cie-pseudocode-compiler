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

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endfunction});

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

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endprocedure});

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
        auto Parsed = ParseExprList(')', true);
        if (!Parsed) return nullptr;
        Args = std::move(*Parsed);
    }

    return std::make_unique<CallStmtAST>(Callee, std::move(Args));
}

std::unique_ptr<StmtAST> Parser::ParseReturnStmt() {
    getNextToken();
    std::unique_ptr<ExprAST> Expr = nullptr;
    // A bare RETURN is recognized right before any block terminator.
    if (CurTok != tok_endif && CurTok != tok_else && CurTok != tok_endfunction &&
        CurTok != tok_endprocedure && CurTok != tok_endwhile && CurTok != tok_until &&
        CurTok != tok_next && CurTok != tok_eof) {
        Expr = ParseExpression();
        if (!Expr) return nullptr;
    }
    return std::make_unique<ReturnStmtAST>(std::move(Expr));
}
