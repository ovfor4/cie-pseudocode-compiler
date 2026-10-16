#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <cstdio>

using namespace cps;

Parser::Parser(Lexer &L) : Lex(L) {
    getNextToken();

    BinopPrecedence[tok_or] = 3;
    BinopPrecedence[tok_and] = 5;

    BinopPrecedence[tok_eq] = 10;
    BinopPrecedence[tok_ne] = 10;
    BinopPrecedence[tok_le] = 10;
    BinopPrecedence[tok_ge] = 10;

    BinopPrecedence['<'] = 10;
    BinopPrecedence['>'] = 10;

    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['&'] = 20;
    
    BinopPrecedence['*'] = 40;
    BinopPrecedence['/'] = 40;
    BinopPrecedence[tok_div] = 40;
    BinopPrecedence[tok_mod] = 40;
}

int Parser::getNextToken() {
    return CurTok = Lex.gettok();
}

std::vector<std::unique_ptr<StmtAST>> Parser::Parse() {
    return ParseBlock({});
}

// After a statement fails to parse, skip ahead to the next token that can
// start a statement or end a block, so one bad statement cannot swallow the
// start of the next good one.
void Parser::syncToStatementStart() {
    while (CurTok != tok_eof) {
        switch (CurTok) {
        case tok_declare: case tok_identifier: case tok_input: case tok_output:
        case tok_if: case tok_while: case tok_repeat: case tok_for:
        case tok_function: case tok_procedure: case tok_call: case tok_return:
        case tok_else: case tok_endif: case tok_endwhile: case tok_until:
        case tok_next: case tok_endfunction: case tok_endprocedure:
        case tok_openfile: case tok_readfile: case tok_writefile: case tok_closefile:
        case tok_type: case tok_define: case tok_endtype:
        case tok_class: case tok_endclass:
        case tok_try: case tok_catch: case tok_endtry: case tok_throw:
            return;
        default:
            getNextToken();
        }
    }
}

std::vector<std::unique_ptr<StmtAST>> Parser::ParseBlock(std::initializer_list<int> Terminators) {
    auto AtTerminator = [&] {
        for (int T : Terminators) {
            if (CurTok == T) return true;
        }
        return false;
    };

    ++BlockDepth; // 1 = top level (Parse() calls ParseBlock({}))
    std::vector<std::unique_ptr<StmtAST>> Body;
    while (!AtTerminator() && CurTok != tok_eof) {
        if (auto Stmt = ParseStatement()) {
            Body.push_back(std::move(Stmt));
        } else {
            syncToStatementStart();
        }
    }
    --BlockDepth;
    return Body;
}

std::unique_ptr<StmtAST> Parser::ParseStatement() {
    auto Stmt = ParseStatementImpl();
    if (!Stmt) ++NumErrors;
    return Stmt;
}

std::unique_ptr<StmtAST> Parser::ParseStatementImpl() {
    if (CurTok == tok_declare) {
        return ParseDeclare();
    }
    else if (CurTok == tok_identifier) {
        return ParseAssignStmt();
    }
    else if (CurTok == tok_input) {
        getNextToken();
        if (CurTok != tok_identifier) {
            fprintf(stderr, "error: Expected variable name after INPUT at line %d\n", Lex.getLine());
            return nullptr;
        }
        std::string Name = Lex.IdentifierStr;
        int Line = Lex.getLine();
        getNextToken();
        if (CurTok == '[' || CurTok == '.' || CurTok == '^') {
            // Consume the whole designator so it cannot replay as statements.
            std::vector<DesignatorAccess> Dummy;
            ParseDesignatorSuffix(Dummy);
            fprintf(stderr, "error: INPUT target must be a plain variable at line %d\n", Line);
            return nullptr;
        }
        return std::make_unique<InputStmtAST>(Name);
    }
    else if (CurTok == tok_output) {
        getNextToken();
        std::vector<std::unique_ptr<ExprAST>> Exprs;
        while (true) {
            auto Expr = ParseExpression();
            if (!Expr) return nullptr;
            Exprs.push_back(std::move(Expr));
            if (CurTok != ',') break;
            getNextToken();
        }
        return std::make_unique<OutputStmtAST>(std::move(Exprs));
    }
    else if (CurTok == tok_if) {
        return ParseIfStmt();
    }
    else if (CurTok == tok_try) {
        return ParseTryStmt();
    }
    else if (CurTok == tok_throw) {
        getNextToken();
        return std::make_unique<ThrowStmtAST>();
    }
    else if (CurTok == tok_catch || CurTok == tok_endtry) {
        fprintf(stderr, "error: Unexpected %s without matching TRY at line %d\n",
                CurTok == tok_catch ? "CATCH" : "ENDTRY", Lex.getLine());
        getNextToken();
        return nullptr;
    }
    else if (CurTok == tok_while) {
        return ParseWhileStmt();
    }
    else if (CurTok == tok_repeat) {
        return ParseRepeatStmt();
    }
    else if (CurTok == tok_for) {
        return ParseForStmt();
    }
    else if (CurTok == tok_function) {
        return ParseFunction();
    }
    else if (CurTok == tok_procedure) {
        return ParseProcedure();
    }
    else if (CurTok == tok_call) {
        return ParseCallStmt();
    }
    else if (CurTok == tok_return) {
        return ParseReturnStmt();
    }
    else if (CurTok == tok_openfile) {
        return ParseOpenFile();
    }
    else if (CurTok == tok_readfile) {
        return ParseReadFile();
    }
    else if (CurTok == tok_writefile) {
        return ParseWriteFile();
    }
    else if (CurTok == tok_closefile) {
        return ParseCloseFile();
    }
    else if (CurTok == tok_type || CurTok == tok_class) {
        return ParseTypeDecl();
    }
    else if (CurTok == tok_define) {
        fprintf(stderr, "error: DEFINE (SET literals) is not implemented at line %d\n", Lex.getLine());
        getNextToken();
        return nullptr;
    }
    else if (CurTok == tok_endtype) {
        fprintf(stderr, "error: ENDTYPE without matching TYPE at line %d\n", Lex.getLine());
        getNextToken();
        return nullptr;
    }
    else if (CurTok == tok_endclass) {
        fprintf(stderr, "error: ENDCLASS without matching CLASS at line %d\n", Lex.getLine());
        getNextToken();
        return nullptr;
    }

    fprintf(stderr, "error: Unexpected token at line %d when expecting a statement\n", Lex.getLine());
    getNextToken(); // guarantee progress so error recovery cannot loop forever
    return nullptr;
}

// <designator> <- <expr>
// Bare-name and single-index targets degrade to their legacy nodes so the
// existing assignment code paths stay untouched.
std::unique_ptr<StmtAST> Parser::ParseAssignStmt() {
    std::string Name = Lex.IdentifierStr;
    int Line = Lex.getLine();
    getNextToken();

    std::vector<DesignatorAccess> Accesses;
    if (!ParseDesignatorSuffix(Accesses)) return nullptr;

    if (CurTok != tok_assign) {
        fprintf(stderr, "error: Expected '<-' after identifier '%s' at line %d\n", Name.c_str(), Line);
        return nullptr;
    }
    getNextToken();
    auto Expr = ParseExpression();
    if (!Expr) return nullptr;

    if (Accesses.empty())
        return std::make_unique<AssignStmtAST>(Name, std::move(Expr));
    if (Accesses.size() == 1 && Accesses[0].Kind == AccessKind::Index)
        return std::make_unique<ArrayAssignStmtAST>(Name, std::move(Accesses[0].Indices), std::move(Expr), Line);

    auto Target = std::make_unique<DesignatorExprAST>(Name, std::move(Accesses), Line);
    return std::make_unique<DesignatorAssignStmtAST>(std::move(Target), std::move(Expr));
}
