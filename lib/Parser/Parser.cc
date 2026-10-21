#include "cps/Diagnostics.h"
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
    int PreviousLine = CurTok == tok_eof ? 0 : Lex.getLine();
    CurTok = Lex.gettok();
    StartsLine = Lex.getLine() != PreviousLine;
    return CurTok;
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
        case tok_include: case tok_global: case tok_endglobal:
        case tok_include_obj: case tok_extern:
        case tok_delete: case tok_constant: case tok_declare: case tok_identifier: case tok_operator: case tok_input: case tok_output:
        case tok_case: case tok_endcase: case tok_otherwise:
        case tok_if: case tok_while: case tok_repeat: case tok_for:
        case tok_template: case tok_overload: case tok_function: case tok_procedure: case tok_call: case tok_return:
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

std::vector<std::unique_ptr<StmtAST>> Parser::ParseBlock(std::initializer_list<int> Terminators, bool CaseBody) {
    auto AtTerminator = [&] {
        for (int T : Terminators) {
            if (CurTok == T) return true;
        }
        return false;
    };

    ++BlockDepth; // 1 = top level (Parse() calls ParseBlock({}))
    if (CaseBody) CaseBodyDepths.push_back(BlockDepth);
    std::vector<std::unique_ptr<StmtAST>> Body;
    while (!AtTerminator() && CurTok != tok_eof && !(CaseBody && isCaseLabel())) {
        if (auto Stmt = ParseStatement()) {
            Body.push_back(std::move(Stmt));
        } else {
            syncToStatementStart();
        }
    }
    if (CaseBody) CaseBodyDepths.pop_back();
    --BlockDepth;
    return Body;
}

std::unique_ptr<StmtAST> Parser::ParseStatement() {
    int Line = Lex.getLine();
    auto Stmt = ParseStatementImpl();
    if (Stmt && !Stmt->getLine()) Stmt->setLine(Line);
    if (!Stmt) ++NumErrors;
    return Stmt;
}

std::unique_ptr<StmtAST> Parser::ParseStatementImpl() {
    if (CurTok == tok_template) return ParseTemplateDecl();
    if (CurTok == tok_extern) {
        if (BlockDepth != 1) {
            sourceError(Lex.getLine(), "EXTERN is only allowed at top level");
            getNextToken();
            return nullptr;
        }
        return ParseExternDecl();
    }
    if (CurTok == tok_include || CurTok == tok_include_obj || CurTok == tok_global) {
        const char *Keyword = CurTok == tok_include ? "INCLUDE" : CurTok == tok_include_obj ? "INCLUDE_OBJ" : "GLOBAL";
        bool Object = CurTok == tok_include_obj;
        bool Include = Object || CurTok == tok_include;
        int Line = Lex.getLine();
        getNextToken();
        if (BlockDepth != 1) {
            sourceError(Line, "%s is only allowed at top level", Keyword);
            return nullptr;
        }
        if (Include) {
            if (CurTok != tok_string_literal) {
                sourceError(Line, "%s requires a string literal", Keyword);
                return nullptr;
            }
            std::unique_ptr<StmtAST> Stmt;
            if (Object) Stmt = std::make_unique<IncludeObjStmtAST>(Lex.StringVal);
            else Stmt = std::make_unique<IncludeStmtAST>(Lex.StringVal);
            getNextToken();
            return Stmt;
        }
        auto Body = ParseBlock({tok_endglobal});
        if (CurTok != tok_endglobal) {
            sourceError(Lex.getLine(), "Expected ENDGLOBAL");
            return nullptr;
        }
        getNextToken();
        return std::make_unique<GlobalBlockAST>(std::move(Body));
    }
    if (CurTok == tok_endglobal) {
        sourceError(Lex.getLine(), "ENDGLOBAL without matching GLOBAL");
        getNextToken();
        return nullptr;
    }
    if (CurTok == tok_delete) {
        int Line = Lex.getLine();
        getNextToken();
        if (CurTok != tok_identifier) {
            cps::sourceError(Line, "DELETE requires an object designator");
            return nullptr;
        }
        std::string Name = Lex.IdentifierStr;
        getNextToken();
        std::vector<DesignatorAccess> Accesses;
        if (!ParseDesignatorSuffix(Accesses)) return nullptr;
        auto Target = std::make_unique<DesignatorExprAST>(Name, std::move(Accesses), Line);
        return std::make_unique<DeleteStmtAST>(std::move(Target));
    }
    if (CurTok == tok_case) return ParseCaseStmt();
    if (CurTok == tok_endcase || CurTok == tok_otherwise) {
        cps::sourceError(Lex.getLine(), "Unexpected CASE terminator");
        getNextToken();
        return nullptr;
    }
    if (CurTok == tok_constant) return ParseConstant();
    if (CurTok == tok_declare) {
        return ParseDeclare();
    }
    else if (CurTok == tok_identifier || CurTok == tok_operator) {
        return ParseAssignStmt();
    }
    else if (CurTok == tok_input) {
        getNextToken();
        if (CurTok != tok_identifier) {
            cps::sourceError(Lex.getLine(), "Expected variable name after INPUT");
            return nullptr;
        }
        std::string Name = Lex.IdentifierStr;
        int Line = Lex.getLine();
        getNextToken();
        std::vector<DesignatorAccess> Accesses;
        if (!ParseDesignatorSuffix(Accesses)) return nullptr;
        std::unique_ptr<ExprAST> Target;
        if (Accesses.empty())
            Target = std::make_unique<VariableExprAST>(Name);
        else if (Accesses.size() == 1 && Accesses[0].Kind == AccessKind::Index)
            Target = std::make_unique<ArrayAccessExprAST>(Name, std::move(Accesses[0].Indices), Line);
        else
            Target = std::make_unique<DesignatorExprAST>(Name, std::move(Accesses), Line);
        Target->setLine(Line);
        return std::make_unique<InputStmtAST>(std::move(Target));
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
        cps::sourceError(Lex.getLine(), "Unexpected %s without matching TRY",
                CurTok == tok_catch ? "CATCH" : "ENDTRY");
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
    else if (CurTok == tok_overload) {
        int Line = Lex.getLine();
        getNextToken();
        if (CurTok == tok_function) return ParseFunction({}, true, true);
        if (CurTok == tok_procedure) return ParseProcedure({}, true, true);
        cps::sourceError(Line, "OVERLOAD must be followed by FUNCTION or PROCEDURE");
        return nullptr;
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
        cps::sourceError(Lex.getLine(), "DEFINE (SET literals) is not implemented");
        getNextToken();
        return nullptr;
    }
    else if (CurTok == tok_endtype) {
        cps::sourceError(Lex.getLine(), "ENDTYPE without matching TYPE");
        getNextToken();
        return nullptr;
    }
    else if (CurTok == tok_endclass) {
        cps::sourceError(Lex.getLine(), "ENDCLASS without matching CLASS");
        getNextToken();
        return nullptr;
    }

    cps::sourceError(Lex.getLine(), "Unexpected token when expecting a statement");
    getNextToken(); // guarantee progress so error recovery cannot loop forever
    return nullptr;
}

// <designator> <- <expr>
// Bare-name and single-index targets degrade to their legacy nodes so the
// existing assignment code paths stay untouched.
std::unique_ptr<StmtAST> Parser::ParseAssignStmt() {
    int Line = Lex.getLine();
    std::string Name;
    if (CurTok == tok_operator) {
        int Op;
        Name = ParseOperatorName(Op);
        if (Name.empty()) return nullptr;
    } else {
        Name = Lex.IdentifierStr;
        getNextToken();
    }

    std::vector<std::string> TemplateArgs;
    if (CurTok == '<') {
        auto Parsed = ParseTemplateArgs();
        if (!Parsed) return nullptr;
        TemplateArgs = std::move(*Parsed);
        if (CurTok != '(') {
            sourceError(Line, "Expected '(' after explicit template arguments");
            return nullptr;
        }
    }
    if (CurTok == '(') {
        getNextToken();
        auto Args = ParseExprList(')', true);
        if (!Args) return nullptr;
        if (CurTok == '.') {
            auto Result = ParseResultMethods(std::make_unique<CallExprAST>(Name, std::move(*Args), Line, std::move(TemplateArgs)), Line);
            if (!Result) return nullptr;
            return std::make_unique<MemberCallStmtAST>(
                std::unique_ptr<MemberCallExprAST>(static_cast<MemberCallExprAST *>(Result.release())));
        }
        return std::make_unique<CallStmtAST>(Name, std::move(*Args), Line, std::move(TemplateArgs));
    }

    std::vector<DesignatorAccess> Accesses;
    if (!ParseDesignatorSuffix(Accesses)) return nullptr;

    if (CurTok == '(') {
        auto Call = ParseMemberCall(Name, std::move(Accesses), Line);
        if (!Call) return nullptr;
        return std::make_unique<MemberCallStmtAST>(std::move(Call));
    }

    if (CurTok != tok_assign) {
        cps::sourceError(Line, "Expected '<-' after identifier '%s'", Name.c_str());
        return nullptr;
    }
    getNextToken();
    auto Expr = ParseAssignmentRHS();
    if (!Expr) return nullptr;

    if (Accesses.empty())
        return std::make_unique<AssignStmtAST>(Name, std::move(Expr));
    if (Accesses.size() == 1 && Accesses[0].Kind == AccessKind::Index)
        return std::make_unique<ArrayAssignStmtAST>(Name, std::move(Accesses[0].Indices), std::move(Expr), Line);

    auto Target = std::make_unique<DesignatorExprAST>(Name, std::move(Accesses), Line);
    return std::make_unique<DesignatorAssignStmtAST>(std::move(Target), std::move(Expr));
}
