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

int Parser::GetTokPrecedence() {
    if (BinopPrecedence.find(CurTok) == BinopPrecedence.end()) return -1;
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

int Parser::getNextToken() {
    return CurTok = Lex.gettok();
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    if (CurTok == tok_number_real) {
        auto Result = std::make_unique<RealExprAST>(Lex.RealVal);
        getNextToken();
        return Result;
    }
    auto Result = std::make_unique<IntegerExprAST>(Lex.NumVal);
    getNextToken();
    return Result;
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    std::string IdName = Lex.IdentifierStr;
    int Line = Lex.getLine();
    getNextToken();

    if (CurTok == '(') {
        getNextToken();
        std::vector<std::unique_ptr<ExprAST>> Args;
        if (CurTok != ')') {
            while (true) {
                if (auto Arg = ParseExpression())
                    Args.push_back(std::move(Arg));
                else
                    return nullptr;

                if (CurTok == ')') break;

                if (CurTok != ',') {
                    fprintf(stderr, "Error: Expected ')' or ',' in function call.\n");
                    return nullptr;
                }
                getNextToken();
            }
        }
        getNextToken();
        return std::make_unique<CallExprAST>(IdName, std::move(Args));
    }
    
    if (CurTok == '[') {
        getNextToken();
        std::vector<std::unique_ptr<ExprAST>> Indices;
        while (true) {
            auto Exp = ParseExpression();
            if (!Exp) return nullptr;
            Indices.push_back(std::move(Exp));
            if (CurTok == ']') break;
            if (CurTok == ',') { getNextToken(); continue; }
            fprintf(stderr, "Error: Expected ',' or ']'\n");
            return nullptr;
        }
        getNextToken(); 
        return std::make_unique<ArrayAccessExprAST>(IdName, std::move(Indices), Line);
    }
    
    return std::make_unique<VariableExprAST>(IdName);
}

std::string Parser::ParseTypeName(bool AllowVoid) {
    if (CurTok == tok_integer_kw) {
        getNextToken();
        return "INTEGER";
    }
    if (CurTok == tok_real_kw) {
        getNextToken();
        return "REAL";
    }
    if (CurTok == tok_boolean_kw) {
        getNextToken();
        return "BOOLEAN";
    }
    if (CurTok == tok_string_kw) {
        getNextToken();
        return "STRING";
    }
    if (CurTok == tok_char_kw) {
        getNextToken();
        return "CHAR";
    }
    if (CurTok == tok_identifier) {
        std::string TypeName = Lex.IdentifierStr;
        if (!AllowVoid && TypeName == "VOID") {
            fprintf(stderr, "Error: VOID is not allowed here\n");
            return "";
        }
        getNextToken();
        return TypeName;
    }

    fprintf(stderr, "Error: Unknown type name\n");
    return "";
}

std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken();
    auto V = ParseExpression();
    if (!V) return nullptr;
    if (CurTok != ')') {
        fprintf(stderr, "Error: expected ')'\n");
        return nullptr;
    }
    getNextToken();
    return V;
}

std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    switch (CurTok) {
    case tok_identifier: return ParseIdentifierExpr();
    case tok_number_int: return ParseNumberExpr();
    case tok_number_real: return ParseNumberExpr();
    case tok_string_literal: {
        auto Res = std::make_unique<StringExprAST>(Lex.StringVal);
        getNextToken();
        return Res;
    }
    case tok_char_literal: {
        auto Res = std::make_unique<CharExprAST>(Lex.CharVal);
        getNextToken();
        return Res;
    }
    case tok_true: {
        getNextToken();
        return std::make_unique<BooleanExprAST>(true);
    }
    case tok_false: {
        getNextToken();
        return std::make_unique<BooleanExprAST>(false);
    }
    case '(':            return ParseParenExpr();
    default:
        fprintf(stderr, "Error: unknown token '%c' (%d) at line %d when expecting an expression\n", 
                (char)CurTok, CurTok, Lex.getLine());
        return nullptr;
    }
}

std::unique_ptr<ExprAST> Parser::ParseUnary() {
    if (CurTok == tok_not) {
        getNextToken();
        auto Operand = ParseUnary();
        if (!Operand) return nullptr;
        return std::make_unique<UnaryExprAST>(tok_not, std::move(Operand));
    }

    if (CurTok == '-') {
        int Line = Lex.getLine();
        getNextToken();
        auto Operand = ParseUnary();
        if (!Operand) return nullptr;
        auto Zero = std::make_unique<IntegerExprAST>(0);
        return std::make_unique<BinaryExprAST>('-', std::move(Zero), std::move(Operand), Line);
    }

    return ParsePrimary();
}

std::unique_ptr<ExprAST> Parser::ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS) {
    while (true) {
        int TokPrec = GetTokPrecedence();
        if (TokPrec < ExprPrec) return LHS;

        int BinOp = CurTok;
        int Line = Lex.getLine();
        getNextToken();

        auto RHS = ParseUnary();
        if (!RHS) return nullptr;

        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
            if (!RHS) return nullptr;
        }

        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS), Line);
    }
}

std::unique_ptr<ExprAST> Parser::ParseExpression() {
    auto LHS = ParseUnary();
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

std::vector<std::unique_ptr<StmtAST>> Parser::Parse() {
    std::vector<std::unique_ptr<StmtAST>> Statements;
    while (CurTok != tok_eof) {
        if (auto Stmt = ParseStatement()) {
            Statements.push_back(std::move(Stmt));
        } else {
            if (CurTok != tok_eof) getNextToken();
        }
    }
    return Statements;
}

std::unique_ptr<StmtAST> Parser::ParseDeclare() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected identifier after DECLARE at line %d\n", Lex.getLine());
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_colon) {
        fprintf(stderr, "Error: Expected ':' after variable name '%s' at line %d\n", Name.c_str(), Lex.getLine());
        return nullptr;
    }
    getNextToken();
    
    if (CurTok == tok_array) {
        getNextToken(); 
        if (CurTok != '[') {
            fprintf(stderr, "Error: Expected '[' after ARRAY\n");
            return nullptr;
        }
        getNextToken();
        
        std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Bounds;
        
        while (true) {
            auto Lower = ParseExpression();
            if (!Lower) return nullptr;
            
            if (CurTok != tok_colon) {
                fprintf(stderr, "Error: Expected ':' in array range (e.g. 1:10)\n");
                return nullptr;
            }
            getNextToken();
            
            auto Upper = ParseExpression();
            if (!Upper) return nullptr;
            
            Bounds.push_back({std::move(Lower), std::move(Upper)});
            
            if (CurTok == ']') break;
            if (CurTok == ',') {
                getNextToken();
                continue;
            }
            fprintf(stderr, "Error: Expected ',' or ']' in array dims\n");
            return nullptr;
        }
        getNextToken();
        
        if (CurTok != tok_of) {
            fprintf(stderr, "Error: Expected OF after array dims\n");
            return nullptr;
        }
        getNextToken();

        std::string TypeStr = ParseTypeName(false);
        if (TypeStr.empty()) {
            return nullptr;
        }
        
        return std::make_unique<ArrayDeclareStmtAST>(Name, std::move(Bounds), TypeStr);
    } 
    else {
        std::string TypeStr = ParseTypeName(false);
        if (TypeStr.empty()) {
            return nullptr;
        }
        return std::make_unique<DeclareStmtAST>(Name, TypeStr);
    }
}

std::unique_ptr<StmtAST> Parser::ParseIfStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok != tok_then) {
        fprintf(stderr, "Error: expected THEN\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> ThenStmts;
    while (CurTok != tok_else && CurTok != tok_endif && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) ThenStmts.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_else && CurTok != tok_endif) getNextToken();
    }

    std::vector<std::unique_ptr<StmtAST>> ElseStmts;
    if (CurTok == tok_else) {
        getNextToken();
        while (CurTok != tok_endif && CurTok != tok_eof) {
            auto Stmt = ParseStatement();
            if (Stmt) ElseStmts.push_back(std::move(Stmt));
            else if (CurTok != tok_eof && CurTok != tok_endif) getNextToken();
        }
    }

    if (CurTok != tok_endif) {
        fprintf(stderr, "Error: expected ENDIF\n");
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
        fprintf(stderr, "Error: expected DO after WHILE condition\n");
        return nullptr;
    }
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_endwhile && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_endwhile) getNextToken();
    }

    if (CurTok != tok_endwhile) {
        fprintf(stderr, "Error: expected ENDWHILE\n");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseRepeatStmt() {
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_until && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_until) getNextToken();
    }

    if (CurTok != tok_until) {
        fprintf(stderr, "Error: expected UNTIL\n");
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
        fprintf(stderr, "Error: expected identifier after FOR\n");
        return nullptr;
    }
    std::string VarName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_assign) {
        fprintf(stderr, "Error: expected '<-' in FOR loop\n");
        return nullptr;
    }
    getNextToken();

    auto Start = ParseExpression();
    if (!Start) return nullptr;

    if (CurTok != tok_to) {
        fprintf(stderr, "Error: expected TO in FOR loop\n");
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

    std::vector<std::unique_ptr<StmtAST>> Body;
    while (CurTok != tok_next && CurTok != tok_eof) {
        auto Stmt = ParseStatement();
        if (Stmt) Body.push_back(std::move(Stmt));
        else if (CurTok != tok_eof && CurTok != tok_next) getNextToken();
    }

    if (CurTok != tok_next) {
        fprintf(stderr, "Error: expected NEXT\n");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: expected identifier after NEXT (e.g., NEXT %s)\n", VarName.c_str());
        return nullptr;
    }

    if (Lex.IdentifierStr != VarName) {
        fprintf(stderr, "Error: NEXT identifier '%s' does not match FOR variable '%s'\n", 
                Lex.IdentifierStr.c_str(), VarName.c_str());
        return nullptr;
    }
    getNextToken();

    return std::make_unique<ForStmtAST>(VarName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
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
        std::string Name = Lex.IdentifierStr;
        int Line = Lex.getLine();
        getNextToken();
        
        if (CurTok == '[') {
            getNextToken();
            std::vector<std::unique_ptr<ExprAST>> Indices;
            while (true) {
                auto Exp = ParseExpression();
                if (!Exp) return nullptr;
                Indices.push_back(std::move(Exp));
                if (CurTok == ']') break;
                if (CurTok == ',') getNextToken();
            }
            getNextToken();
            
            if (CurTok != tok_assign) {
                fprintf(stderr, "Error: Expected '<-' after array access in assignment\n");
                return nullptr;
            }
            getNextToken();
            auto Expr = ParseExpression();
            if (!Expr) return nullptr;
            return std::make_unique<ArrayAssignStmtAST>(Name, std::move(Indices), std::move(Expr), Line);
        }
        
        if (CurTok != tok_assign) {
            fprintf(stderr, "Error: Expected '<-' after identifier '%s' at line %d\n", Name.c_str(), Line);
            return nullptr;
        }
        getNextToken();
        auto Expr = ParseExpression();
        if (!Expr) return nullptr;
        return std::make_unique<AssignStmtAST>(Name, std::move(Expr));
    }
    else if (CurTok == tok_input) {
        getNextToken();
        if (CurTok != tok_identifier) {
            fprintf(stderr, "Error: Expected variable name after INPUT at line %d\n", Lex.getLine());
            return nullptr;
        }
        std::string Name = Lex.IdentifierStr;
        getNextToken();
        return std::make_unique<InputStmtAST>(Name);
    }
    else if (CurTok == tok_output) {
        getNextToken();
        auto Expr = ParseExpression();
        if (!Expr) return nullptr;
        return std::make_unique<OutputStmtAST>(std::move(Expr));
    }
    else if (CurTok == tok_if) {
        return ParseIfStmt();
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

    fprintf(stderr, "Error: Unexpected token at line %d when expecting a statement\n", Lex.getLine());
    return nullptr;
}
