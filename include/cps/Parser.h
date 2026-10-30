#pragma once
#include "cps/Identifier.h"
#include "cps/Lexer.h"
#include "cps/AST.h"
#include "cps/TypeAST.h"
#include "cps/FunctionAST.h"
#include "cps/TemplateAST.h"
#include <cstdint>
#include <vector>
#include <initializer_list>
#include <memory>
#include <map>
#include <optional>
#include <string>

namespace cps {

class Parser {
    Lexer &Lex;
    int CurTok = tok_eof;
    bool StartsLine = true;
    int PrevTokLine = 0; // line of the token before CurTok
    bool ParsingCaseLabel = false;
    std::vector<unsigned> CaseBodyDepths;
    unsigned NumErrors = 0;
    unsigned BlockDepth = 0; // ParseBlock nesting; 1 = top level
    std::vector<TemplateParam> CurrentTemplate;
    IdentifierSet PackNames;

    std::map<int, int> BinopPrecedence;
    IdentifierSet TypeNames = {
        "INTEGER", "REAL", "BOOLEAN", "STRING", "CHAR", "VOID",
        "C_INT", "C_CHAR", "C_STRING", "C_DOUBLE"
    };

    void getNextToken();
    // Line for a diagnostic about a token missing before CurTok: the previous
    // line when CurTok starts a new one.
    int missingLine() const { return StartsLine ? PrevTokLine : Lex.getLine(); }
    int GetTokPrecedence();

    std::unique_ptr<ExprAST> ParseExpression();
    std::unique_ptr<ExprAST> ParseAssignmentRHS();
    std::unique_ptr<ExprAST> ParseUnaryImpl();
    std::unique_ptr<ExprAST> ParseUnary(); 
    std::unique_ptr<ExprAST> ParsePrimaryImpl();
    std::unique_ptr<ExprAST> ParseResultMethods(std::unique_ptr<ExprAST> Expr, int Line);
    std::unique_ptr<ExprAST> ParsePrimary();
    std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS); 

    std::unique_ptr<ExprAST> ParseNumberExpr();
    std::unique_ptr<ExprAST> ParseIdentifierExpr();
    std::string ParseOperatorName(int *Op = nullptr);
    bool ParseCalleeName(std::string &Name);
    std::unique_ptr<ExprAST> ParseParenExpr();
    bool ParseOptionalCallArgs(std::vector<std::unique_ptr<ExprAST>> &Args);
    bool diagnoseResultAccess();
    static bool isBuiltinTypeToken(int Tok);
    std::unique_ptr<MemberCallExprAST> ParseMemberCall(
        const std::string &Name, std::vector<DesignatorAccess> Accesses, int Line);
    std::string ParseTypeName(bool AllowVoid = false);
    std::optional<std::vector<std::string>> ParseTemplateArgs();
    bool isTemplateCall();
    bool isTemplatePack(const std::string &Name) const;

    std::unique_ptr<StmtAST> ParseStatement();
    std::unique_ptr<StmtAST> ParseStatementImpl();
    std::unique_ptr<StmtAST> ParseIncludeOrGlobal();
    std::unique_ptr<StmtAST> ParseDeleteStmt();
    std::unique_ptr<StmtAST> ParseInputStmt();
    std::unique_ptr<StmtAST> ParseOutputStmt();
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
    static std::unique_ptr<ExprAST> makeDesignator(
        const std::string &Name, std::vector<DesignatorAccess> Accesses, int Line);

    std::unique_ptr<StmtAST> ParseTypeDecl();
    std::unique_ptr<StmtAST> ParseTemplateDecl();
    std::optional<std::vector<RecordFieldDecl>> ParseRecordBody(
        const std::string &Name, int Line, int EndToken,
        std::vector<std::unique_ptr<FunctionDefAST>> *Methods = nullptr);
    bool ParseRecordField(std::vector<RecordFieldDecl> &Out, bool IsPublic);

    std::unique_ptr<FunctionDefAST> ParseFunction(const std::string &OwnerClass = {}, bool IsPublic = true, bool IsOverload = false);
    std::unique_ptr<FunctionDefAST> ParseProcedure(const std::string &OwnerClass = {}, bool IsPublic = true, bool IsOverload = false, bool IsImplicit = false);
    std::unique_ptr<FunctionDefAST> ParseFunctionBody(
        std::unique_ptr<PrototypeAST> Proto, int EndToken, const char *EndMessage, int Line);
    std::unique_ptr<StmtAST> ParseExternDecl();
    std::unique_ptr<StmtAST> ParseCallStmt();
    std::unique_ptr<StmtAST> ParseReturnStmt();
    std::optional<std::vector<ParamDecl>> ParsePrototypeArgs(bool AllowByRef);
    bool ParseIntegerValue(bool Negative, int64_t &Out);
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
