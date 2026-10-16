#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <cstdio>

using namespace cps;

std::unique_ptr<StmtAST> Parser::ParseTryStmt() {
    getNextToken();
    auto Body = ParseBlock({tok_catch, tok_endtry});
    if (CurTok != tok_catch) {
        fprintf(stderr, "error: Expected CATCH in TRY at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken();
    auto CatchBody = ParseBlock({tok_endtry, tok_catch});
    if (CurTok != tok_endtry) {
        fprintf(stderr, "error: Expected ENDTRY after CATCH at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken();
    return std::make_unique<TryStmtAST>(std::move(Body), std::move(CatchBody));
}

std::unique_ptr<StmtAST> Parser::ParseIfStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != tok_then) {
        fprintf(stderr, "error: expected THEN\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> ThenStmts = ParseBlock({tok_else, tok_endif});

    std::vector<std::unique_ptr<StmtAST>> ElseStmts;
    if (CurTok == tok_else) {
        getNextToken();
        ElseStmts = ParseBlock({tok_endif});
    }

    if (CurTok != tok_endif) {
        fprintf(stderr, "error: expected ENDIF\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<IfStmtAST>(std::move(Cond), std::move(ThenStmts), std::move(ElseStmts));
}

std::unique_ptr<StmtAST> Parser::ParseWhileStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != tok_do) {
        fprintf(stderr, "error: expected DO after WHILE condition\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endwhile});

    if (CurTok != tok_endwhile) {
        fprintf(stderr, "error: expected ENDWHILE\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseRepeatStmt() {
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_until});

    if (CurTok != tok_until) {
        fprintf(stderr, "error: expected UNTIL\n");
        return nullptr;
    }
    getNextToken();

    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    return std::make_unique<RepeatStmtAST>(std::move(Body), std::move(Cond));
}

std::unique_ptr<StmtAST> Parser::ParseForStmt() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: expected identifier after FOR\n");
        return nullptr;
    }
    std::string VarName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_assign) {
        fprintf(stderr, "error: expected '<-' in FOR loop\n");
        return nullptr;
    }
    getNextToken();

    auto Start = ParseExpression();
    if (!Start) return nullptr;

    if (CurTok != tok_to) {
        fprintf(stderr, "error: expected TO in FOR loop\n");
        return nullptr;
    }
    getNextToken();

    auto End = ParseExpression();
    if (!End) return nullptr;

    std::unique_ptr<ExprAST> Step = nullptr;
    if (CurTok == tok_step) {
        getNextToken();
        Step = ParseExpression();
        if (!Step) return nullptr;
    }

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_next});

    if (CurTok != tok_next) {
        fprintf(stderr, "error: expected NEXT\n");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: expected identifier after NEXT (e.g., NEXT %s)\n", VarName.c_str());
        return nullptr;
    }

    if (Lex.IdentifierStr != VarName) {
        fprintf(stderr, "error: NEXT identifier '%s' does not match FOR variable '%s'\n",
                Lex.IdentifierStr.c_str(), VarName.c_str());
        return nullptr;
    }
    getNextToken();

    return std::make_unique<ForStmtAST>(VarName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}
