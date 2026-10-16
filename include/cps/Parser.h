#pragma once
#include "cps/Lexer.h"
#include "cps/AST.h"
#include "cps/TypeAST.h"
#include "cps/FunctionAST.h"
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <set>

namespace cps {

class Parser {
    Lexer &Lex;
    int CurTok = tok_eof;
    bool StartsLine = true;
    bool ParsingCaseLabel = false;
    std::vector<unsigned> CaseBodyDepths;
    unsigned NumErrors = 0;
    unsigned BlockDepth = 0; // ParseBlock nesting; 1 = top level

    std::map<int, int> BinopPrecedence;
    std::set<std::string> TypeNames = {
        "INTEGER", "REAL", "BOOLEAN", "STRING", "CHAR", "VOID",
        "C_INT", "C_CHAR", "C_STRING", "C_DOUBLE"
    };

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
    std::vector<std::unique_ptr<StmtAST>> ParseBlock(std::initializer_list<int> Terminators, bool CaseBody = false);
    bool isCaseLabel();
    bool atCaseBoundary();
    std::unique_ptr<StmtAST> ParseCaseStmt();
    std::optional<std::vector<std::unique_ptr<ExprAST>>> ParseExprList(char CloseDelim, bool AllowEmpty);
    void syncToStatementStart();
    std::unique_ptr<StmtAST> ParseIfStmt();
    std::unique_ptr<StmtAST> ParseTryStmt();
    std::unique_ptr<StmtAST> ParseWhileStmt();
    std::unique_ptr<StmtAST> ParseRepeatStmt();
    std::unique_ptr<StmtAST> ParseForStmt();
    
    std::unique_ptr<StmtAST> ParseDeclare();
    std::unique_ptr<StmtAST> ParseConstant();
    std::unique_ptr<StmtAST> ParseAssignStmt();
    bool ParseDesignatorSuffix(std::vector<DesignatorAccess> &Out);

    std::unique_ptr<StmtAST> ParseTypeDecl();
    std::optional<std::vector<RecordFieldDecl>> ParseRecordBody(
        const std::string &Name, int Line, int EndToken);
    bool ParseRecordField(std::vector<RecordFieldDecl> &Out, bool IsPublic);

    std::unique_ptr<StmtAST> ParseFunction();
    std::unique_ptr<StmtAST> ParseProcedure();
    std::unique_ptr<StmtAST> ParseCallStmt();
    std::unique_ptr<StmtAST> ParseReturnStmt();
    std::optional<std::vector<ParamDecl>> ParsePrototypeArgs(bool AllowByRef);
    bool ParseSignedIntBound(int64_t &Out);

    std::unique_ptr<StmtAST> ParseOpenFile();
    std::unique_ptr<StmtAST> ParseReadFile();
    std::unique_ptr<StmtAST> ParseWriteFile();
    std::unique_ptr<StmtAST> ParseCloseFile();

public:
    Parser(Lexer &L);
    std::vector<std::unique_ptr<StmtAST>> Parse();
    bool hadError() const { return NumErrors != 0; }
};

} // namespace cps
