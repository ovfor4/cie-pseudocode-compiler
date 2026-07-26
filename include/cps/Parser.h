#pragma once
#include "cps/Lexer.h"
#include "cps/AST.h"
#include <vector>
#include <initializer_list>
#include <memory>
#include <map>
#include <optional>
#include <tuple>
#include <string>

namespace cps {

class Parser {
    Lexer &Lex;
    int CurTok;
    unsigned NumErrors = 0;

    std::map<int, int> BinopPrecedence;

    int getNextToken();
    int GetTokPrecedence();

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<ExprAST> ParseUnary(); 
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS); 

    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::unique_ptr<ExprAST> ParseParenExpr();
    std::string ParseTypeName(bool AllowVoid = false);

    std::unique_ptr<StmtAST> ParseStatement();
    std::unique_ptr<StmtAST> ParseStatementImpl();
    std::vector<std::unique_ptr<StmtAST>> ParseBlock(std::initializer_list<int> Terminators);
    std::optional<std::vector<std::unique_ptr<ExprAST>>> ParseExprList(char CloseDelim, bool AllowEmpty);
    void syncToStatementStart();
    std::unique_ptr<StmtAST> ParseIfStmt();
    std::unique_ptr<StmtAST> ParseWhileStmt();
    std::unique_ptr<StmtAST> ParseRepeatStmt();
    std::unique_ptr<StmtAST> ParseForStmt();
    
    std::unique_ptr<StmtAST> ParseDeclare();

    std::unique_ptr<StmtAST> ParseFunction();
    std::unique_ptr<StmtAST> ParseProcedure();
    std::unique_ptr<StmtAST> ParseCallStmt();
    std::unique_ptr<StmtAST> ParseReturnStmt();
    std::optional<std::vector<std::tuple<std::string, std::string, bool>>> ParsePrototypeArgs();

public:
    Parser(Lexer &L);
    std::vector<std::unique_ptr<StmtAST>> Parse();
    bool hadError() const { return NumErrors != 0; }
};

} // namespace cps
