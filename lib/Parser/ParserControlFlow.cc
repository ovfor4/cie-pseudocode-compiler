#include "cps/Diagnostics.h"
#include "cps/Parser.h"

using namespace cps;

std::unique_ptr<StmtAST> Parser::ParseTryStmt() {
    getNextToken();
    auto Body = ParseBlock({tok_catch, tok_endtry});
    if (CurTok != tok_catch) {
        cps::sourceError(Lex.getLine(), "Expected CATCH in TRY");
        return nullptr;
    }
    getNextToken();
    auto CatchBody = ParseBlock({tok_endtry, tok_catch});
    if (CurTok != tok_endtry) {
        cps::sourceError(Lex.getLine(), "Expected ENDTRY after CATCH");
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
        cps::sourceError(Lex.getLine(), "expected THEN");
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
        cps::sourceError(Lex.getLine(), "expected ENDIF");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<IfStmtAST>(std::move(Cond), std::move(ThenStmts), std::move(ElseStmts));
}

std::unique_ptr<StmtAST> Parser::ParseWhileStmt() {
    getNextToken();
    auto Cond = ParseExpression();
    if (!Cond) return nullptr;

    if (CurTok == tok_identifier && Lex.IdentifierStr == "DO") getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endwhile});

    if (CurTok != tok_endwhile) {
        cps::sourceError(Lex.getLine(), "expected ENDWHILE");
        return nullptr;
    }
    getNextToken();

    return std::make_unique<WhileStmtAST>(std::move(Cond), std::move(Body));
}

std::unique_ptr<StmtAST> Parser::ParseRepeatStmt() {
    getNextToken();

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_until});

    if (CurTok != tok_until) {
        cps::sourceError(Lex.getLine(), "expected UNTIL");
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
        cps::sourceError(Lex.getLine(), "expected identifier after FOR");
        return nullptr;
    }
    std::string VarName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_assign) {
        cps::sourceError(Lex.getLine(), "expected '<-' in FOR loop");
        return nullptr;
    }
    getNextToken();

    auto Start = ParseExpression();
    if (!Start) return nullptr;

    if (CurTok != tok_to) {
        cps::sourceError(Lex.getLine(), "expected TO in FOR loop");
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
        cps::sourceError(Lex.getLine(), "expected NEXT");
        return nullptr;
    }
    getNextToken();
    
    if (CurTok != tok_identifier) {
        cps::sourceError(Lex.getLine(), "expected identifier after NEXT (e.g., NEXT %s)", VarName.c_str());
        return nullptr;
    }

    if (!sameIdentifier(Lex.IdentifierStr, VarName)) {
        cps::sourceError(Lex.getLine(), "NEXT identifier '%s' does not match FOR variable '%s'",
                Lex.IdentifierStr.c_str(), VarName.c_str());
        return nullptr;
    }
    getNextToken();

    return std::make_unique<ForStmtAST>(VarName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

// A label begins a source line and ends in ':' on that line. Lookahead
// preserves token payloads; assignments and statement keywords are not labels.
bool Parser::isCaseLabel() {
    if (!StartsLine) return false;
    int Line = Lex.getLine();
    int Depth = 0;
    for (unsigned I = 0;; ++I) {
        int T = I == 0 ? CurTok : Lex.peekToken(I - 1);
        int TokenLine = I == 0 ? Line : Lex.peekLine(I - 1);
        if (TokenLine != Line) return false;
        if (T == tok_colon) return Depth == 0;
        switch (T) {
        case '(': case '[': ++Depth; break;
        case ')': case ']': if (--Depth < 0) return false; break;
        case tok_identifier: case tok_number_int: case tok_number_real:
        case tok_string_literal: case tok_c_string_literal: case tok_char_literal: case tok_true: case tok_false:
        case tok_integer_kw: case tok_real_kw: case tok_boolean_kw:
        case tok_string_kw: case tok_char_kw: case tok_c_int: case tok_c_char:
        case tok_c_string: case tok_c_double:
        case tok_to: case tok_otherwise: case tok_and: case tok_or: case tok_not:
        case tok_eq: case tok_ne: case tok_le: case tok_ge: case tok_div: case tok_mod:
        case '+': case '-': case '*': case '/': case '&': case '^':
        case '<': case '>': case '.': case ',': break;
        default: return false;
        }
    }
}

bool Parser::atCaseBoundary() {
    return !ParsingCaseLabel && !CaseBodyDepths.empty() &&
           CaseBodyDepths.back() == BlockDepth && isCaseLabel();
}

std::unique_ptr<StmtAST> Parser::ParseCaseStmt() {
    getNextToken();
    if (CurTok != tok_of) {
        cps::sourceError(Lex.getLine(), "Expected OF after CASE");
        return nullptr;
    }
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceError(Lex.getLine(), "Expected identifier after CASE OF");
        return nullptr;
    }
    std::string Selector = Lex.IdentifierStr;
    getNextToken();
    std::vector<CaseBranch> Branches;
    bool SeenOtherwise = false;
    while (CurTok != tok_endcase && CurTok != tok_eof) {
        if (SeenOtherwise || !isCaseLabel()) {
            cps::sourceError(Lex.getLine(), "Expected CASE label on a new line (OTHERWISE must be last)");
            return nullptr;
        }
        CaseBranch Branch;
        Branch.Line = Lex.getLine();
        if (CurTok == tok_otherwise) {
            SeenOtherwise = true;
            getNextToken();
        } else {
            bool Saved = ParsingCaseLabel;
            ParsingCaseLabel = true;
            Branch.Lower = ParseExpression();
            if (Branch.Lower && CurTok == tok_to) {
                getNextToken();
                Branch.Upper = ParseExpression();
                if (!Branch.Upper) Branch.Lower.reset();
            }
            ParsingCaseLabel = Saved;
            if (!Branch.Lower) return nullptr;
        }
        if (CurTok != tok_colon) {
            cps::sourceError(Lex.getLine(), "Expected ':' after CASE label");
            return nullptr;
        }
        getNextToken();
        Branch.Body = ParseBlock({tok_endcase}, true);
        Branches.push_back(std::move(Branch));
    }
    if (CurTok != tok_endcase || Branches.empty()) {
        cps::sourceError(Lex.getLine(), "Expected nonempty CASE and ENDCASE");
        return nullptr;
    }
    getNextToken();
    return std::make_unique<CaseStmtAST>(std::move(Selector), std::move(Branches));
}
