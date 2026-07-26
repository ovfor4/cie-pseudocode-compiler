#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <cstdio>

using namespace cps;

// Array-parameter bounds live in the signature, not the AST, so they must be
// integer literals (an optional leading '-' is allowed).
bool Parser::ParseSignedIntBound(int64_t &Out) {
    bool Negative = false;
    if (CurTok == '-') {
        Negative = true;
        getNextToken();
    }
    if (CurTok != tok_number_int) {
        fprintf(stderr, "Error: Array parameter bounds must be integer literals at line %d\n",
                Lex.getLine());
        return false;
    }
    Out = Negative ? -Lex.NumVal : Lex.NumVal;
    getNextToken();
    return true;
}

// ( [BYREF|BYVAL] <name> : <type>
//                | <name> : ARRAY OF <type>                 -- rank 1, bounds from the argument
//                | <name> : ARRAY[<int>:<int>,...] OF <type> , ... )
// The pass mode is sticky across parameters (guide §8.3: "the BYVAL or BYREF
// keyword need not be repeated"); unannotated parameters default to BYVAL
// (§8.1). BYREF is procedure-only (§8.3), gated by AllowByRef.
std::optional<std::vector<ParamDecl>> Parser::ParsePrototypeArgs(bool AllowByRef) {
    std::vector<ParamDecl> Params;
    if (CurTok != '(') return Params;
    getNextToken();

    PassMode Current = PassMode::ByVal;
    while (CurTok != ')') {
        if (CurTok == tok_byref) {
            if (!AllowByRef) {
                fprintf(stderr, "Error: BYREF parameters are not allowed in a FUNCTION (procedures only) at line %d\n",
                        Lex.getLine());
                return std::nullopt;
            }
            Current = PassMode::ByRef;
            getNextToken();
        } else if (CurTok == tok_byval) {
            Current = PassMode::ByVal;
            getNextToken();
        }

        if (CurTok != tok_identifier) {
            fprintf(stderr, "Error: Expected argument name at line %d\n", Lex.getLine());
            return std::nullopt;
        }
        ParamDecl P;
        P.Name = Lex.IdentifierStr;
        P.Mode = Current;
        getNextToken();

        if (CurTok != tok_colon) {
            fprintf(stderr, "Error: Expected ':' after argument name at line %d\n", Lex.getLine());
            return std::nullopt;
        }
        getNextToken();

        if (CurTok == tok_array) {
            getNextToken();
            P.IsArray = true;
            if (CurTok == '[') {
                getNextToken();
                while (true) {
                    int64_t Lo = 0, Hi = 0;
                    if (!ParseSignedIntBound(Lo)) return std::nullopt;
                    if (CurTok != tok_colon) {
                        fprintf(stderr, "Error: Expected ':' between array parameter bounds at line %d\n",
                                Lex.getLine());
                        return std::nullopt;
                    }
                    getNextToken();
                    if (!ParseSignedIntBound(Hi)) return std::nullopt;
                    P.DeclaredBounds.emplace_back(Lo, Hi);
                    if (CurTok == ']') break;
                    if (CurTok != ',') {
                        fprintf(stderr, "Error: Expected ',' or ']' in array parameter bounds at line %d\n",
                                Lex.getLine());
                        return std::nullopt;
                    }
                    getNextToken();
                }
                getNextToken(); // eat ']'
                P.Rank = static_cast<int>(P.DeclaredBounds.size());
            } else {
                P.Rank = 1; // ARRAY OF T: bounds travel with the argument
            }
            if (CurTok != tok_of) {
                fprintf(stderr, "Error: Expected OF after ARRAY in parameter '%s' at line %d\n",
                        P.Name.c_str(), Lex.getLine());
                return std::nullopt;
            }
            getNextToken();
        }

        P.TypeName = ParseTypeName(false);
        if (P.TypeName.empty()) {
            fprintf(stderr, "Error: Expected argument type for '%s'\n", P.Name.c_str());
            return std::nullopt;
        }

        Params.push_back(std::move(P));

        if (CurTok == ')') break;
        if (CurTok != ',') {
            fprintf(stderr, "Error: Expected ',' or ')'\n");
            return std::nullopt;
        }
        getNextToken();
    }
    getNextToken();
    return Params;
}

std::unique_ptr<StmtAST> Parser::ParseFunction() {
    getNextToken();
    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected function name\n");
        return nullptr;
    }

    std::string Name = Lex.IdentifierStr;
    getNextToken();

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/false);
    if (!Args) return nullptr;

    std::string RetType = "INTEGER";
    if (CurTok == tok_returns) {
        getNextToken();
        if (CurTok == tok_array) {
            fprintf(stderr, "Error: A FUNCTION cannot RETURN an array; use a BYREF PROCEDURE parameter instead at line %d\n",
                    Lex.getLine());
            return nullptr;
        }
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

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/true);
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
    int Line = Lex.getLine();
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

    return std::make_unique<CallStmtAST>(Callee, std::move(Args), Line);
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
