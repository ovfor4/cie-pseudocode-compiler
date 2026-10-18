#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include "cps/Operator.h"
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
        cps::sourceDiagnostic(Lex.getLine(), "error: Array parameter bounds must be integer literals at line %d\n",
                Lex.getLine());
        return false;
    }
    return ParseIntegerValue(Negative, Out);
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
                cps::sourceDiagnostic(Lex.getLine(), "error: BYREF parameters are not allowed in a FUNCTION (procedures only) at line %d\n",
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
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected argument name at line %d\n", Lex.getLine());
            return std::nullopt;
        }
        ParamDecl P;
        P.Line = Lex.getLine();
        P.Name = Lex.IdentifierStr;
        P.Mode = Current;
        getNextToken();

        if (CurTok != tok_colon) {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected ':' after argument name at line %d\n", Lex.getLine());
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
                        cps::sourceDiagnostic(Lex.getLine(), "error: Expected ':' between array parameter bounds at line %d\n",
                                Lex.getLine());
                        return std::nullopt;
                    }
                    getNextToken();
                    if (!ParseSignedIntBound(Hi)) return std::nullopt;
                    P.DeclaredBounds.emplace_back(Lo, Hi);
                    if (CurTok == ']') break;
                    if (CurTok != ',') {
                        cps::sourceDiagnostic(Lex.getLine(), "error: Expected ',' or ']' in array parameter bounds at line %d\n",
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
                cps::sourceDiagnostic(Lex.getLine(), "error: Expected OF after ARRAY in parameter '%s' at line %d\n",
                        P.Name.c_str(), Lex.getLine());
                return std::nullopt;
            }
            getNextToken();
        }

        P.TypeName = ParseTypeName(false);
        if (P.TypeName.empty()) {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected argument type for '%s'\n", P.Name.c_str());
            return std::nullopt;
        }

        for (const ParamDecl &Prev : Params) {
            if (sameIdentifier(Prev.Name, P.Name)) {
                cps::sourceDiagnostic(Lex.getLine(), "error: Duplicate parameter name '%s' at line %d\n",
                        P.Name.c_str(), Lex.getLine());
                return std::nullopt;
            }
        }
        Params.push_back(std::move(P));

        if (CurTok == ')') break;
        if (CurTok != ',') {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected ',' or ')'\n");
            return std::nullopt;
        }
        getNextToken();
        if (CurTok == ')') {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected another parameter after ',' at line %d\n", Lex.getLine());
            return std::nullopt;
        }
    }
    getNextToken();
    return Params;
}

std::string Parser::ParseOperatorName(int &Op) {
    getNextToken(); // OPERATOR
    Op = CurTok;
    if (operatorSpelling(Op).empty()) {
        sourceDiagnostic(Lex.getLine(), "error: Expected a supported operator after OPERATOR at line %d\n", Lex.getLine());
        getNextToken();
        return {};
    }
    getNextToken();
    return operatorName(Op);
}

std::unique_ptr<FunctionDefAST> Parser::ParseFunction(const std::string &OwnerClass, bool IsPublic, bool IsOverload) {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok == tok_delete || (CurTok == tok_identifier && sameIdentifier(Lex.IdentifierStr, "DELETE"))) {
        cps::sourceDiagnostic(Lex.getLine(), "error: destructor DELETE must be a PROCEDURE at line %d\n", Line);
        return nullptr;
    }
    if (CurTok == tok_new || (CurTok == tok_identifier && sameIdentifier(Lex.IdentifierStr, "NEW"))) {
        cps::sourceDiagnostic(Lex.getLine(), "error: constructor NEW must be a PROCEDURE at line %d\n", Line);
        return nullptr;
    }
    if (CurTok != tok_identifier && CurTok != tok_operator) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected function name\n");
        return nullptr;
    }

    int Operator = 0;
    std::string Name;
    if (CurTok == tok_operator) {
        Name = ParseOperatorName(Operator);
        if (Name.empty()) return nullptr;
        if (!IsOverload) {
            sourceDiagnostic(Line, "error: An operator function must be declared OVERLOAD at line %d\n", Line);
            return nullptr;
        }
    } else {
        Name = Lex.IdentifierStr;
        getNextToken();
    }

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/false);
    if (!Args) return nullptr;

    std::string RetType = "INTEGER";
    if (CurTok == tok_returns) {
        getNextToken();
        if (CurTok == tok_array) {
            cps::sourceDiagnostic(Lex.getLine(), "error: A FUNCTION cannot RETURN an array; use a BYREF PROCEDURE parameter instead at line %d\n",
                    Lex.getLine());
            return nullptr;
        }
        RetType = ParseTypeName(true);
        if (RetType.empty()) {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected return type for function '%s'\n", Name.c_str());
            return nullptr;
        }
    }

    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), RetType, false, OwnerClass, IsPublic, IsOverload);
    Proto->setOperator(Operator);

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endfunction});

    if (CurTok != tok_endfunction) {
        cps::sourceDiagnostic(Lex.getLine(), "error: expected ENDFUNCTION\n");
        return nullptr;
    }
    getNextToken();

    auto Def = std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
    Def->setLine(Line);
    return Def;
}

std::unique_ptr<FunctionDefAST> Parser::ParseProcedure(const std::string &OwnerClass, bool IsPublic, bool IsOverload) {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok != tok_identifier && CurTok != tok_new && CurTok != tok_delete) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected procedure name\n");
        return nullptr;
    }

    std::string Name = CurTok == tok_new ? "NEW" : CurTok == tok_delete ? "DELETE" : Lex.IdentifierStr;
    if (OwnerClass.empty() && sameIdentifier(Name, "DELETE")) {
        cps::sourceDiagnostic(Lex.getLine(), "error: destructor DELETE is only allowed in a CLASS at line %d\n", Line);
        return nullptr;
    }
    if (OwnerClass.empty() && sameIdentifier(Name, "NEW")) {
        cps::sourceDiagnostic(Lex.getLine(), "error: constructor NEW is only allowed in a CLASS at line %d\n", Line);
        return nullptr;
    }
    getNextToken();

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/true);
    if (!Args) return nullptr;
    if (sameIdentifier(Name, "DELETE") && !Args->empty()) {
        cps::sourceDiagnostic(Lex.getLine(), "error: destructor DELETE must have no parameters at line %d\n", Line);
        return nullptr;
    }
    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), "VOID", false, OwnerClass, IsPublic, IsOverload);

    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({tok_endprocedure});

    if (CurTok != tok_endprocedure) {
        cps::sourceDiagnostic(Lex.getLine(), "error: expected ENDPROCEDURE\n");
        return nullptr;
    }
    getNextToken();

    auto Def = std::make_unique<FunctionDefAST>(std::move(Proto), std::move(Body));
    Def->setLine(Line);
    return Def;
}

// EXTERN "C" EXTERN_FUNCTION <name>(<type>, ... [, ...]) RETURNS <type>
// EXTERN "C" EXTERN_PROCEDURE <name>(<type>, ... [, ...])
// Parameters are unnamed; a trailing ... makes the prototype variadic.
std::unique_ptr<StmtAST> Parser::ParseExternDecl() {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok != tok_string_literal || Lex.StringVal != "C") {
        cps::sourceDiagnostic(Lex.getLine(), "error: EXTERN must be followed by \"C\" at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken();
    if (CurTok != tok_extern_function && CurTok != tok_extern_procedure) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected EXTERN_FUNCTION or EXTERN_PROCEDURE after EXTERN \"C\" at line %d\n",
                Lex.getLine());
        return nullptr;
    }
    bool IsFunction = CurTok == tok_extern_function;
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected C function name at line %d\n", Lex.getLine());
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    std::vector<ParamDecl> Params;
    bool IsVariadic = false;
    if (CurTok != '(') {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected '(' after C function name '%s' at line %d\n",
                Name.c_str(), Lex.getLine());
        return nullptr;
    }
    getNextToken();
    while (CurTok != ')') {
        if (CurTok == tok_ellipsis) {
            IsVariadic = true;
            getNextToken();
            if (CurTok != ')') {
                cps::sourceDiagnostic(Lex.getLine(), "error: '...' must be the last parameter at line %d\n", Lex.getLine());
                return nullptr;
            }
            break;
        }
        if (CurTok == tok_byref || CurTok == tok_byval) {
            cps::sourceDiagnostic(Lex.getLine(), "error: EXTERN parameters cannot be BYREF or BYVAL; use a C pointer type at line %d\n",
                    Lex.getLine());
            return nullptr;
        }
        ParamDecl P;
        P.Line = Lex.getLine();
        P.TypeName = ParseTypeName(false);
        if (P.TypeName.empty()) return nullptr;
        P.Name = "arg" + std::to_string(Params.size() + 1);
        Params.push_back(std::move(P));
        if (CurTok == ')') break;
        if (CurTok != ',') {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected ',' or ')' at line %d\n", Lex.getLine());
            return nullptr;
        }
        getNextToken();
        if (CurTok == ')') {
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected another parameter after ',' at line %d\n", Lex.getLine());
            return nullptr;
        }
    }
    getNextToken();

    std::string RetType = "VOID";
    if (CurTok == tok_returns) {
        if (!IsFunction) {
            cps::sourceDiagnostic(Lex.getLine(), "error: EXTERN_PROCEDURE cannot have RETURNS at line %d\n", Lex.getLine());
            return nullptr;
        }
        getNextToken();
        RetType = ParseTypeName(false);
        if (RetType.empty()) return nullptr;
    } else if (IsFunction) {
        cps::sourceDiagnostic(Lex.getLine(), "error: EXTERN_FUNCTION '%s' requires RETURNS at line %d\n",
                Name.c_str(), Line);
        return nullptr;
    }

    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(Params), RetType, /*IsExternal=*/true);
    Proto->setVariadic(IsVariadic);
    auto Decl = std::make_unique<ExternDeclAST>(std::move(Proto));
    Decl->setLine(Line);
    return Decl;
}

std::unique_ptr<StmtAST> Parser::ParseCallStmt() {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected callee name after CALL\n");
        return nullptr;
    }

    std::string Callee = Lex.IdentifierStr;
    getNextToken();

    std::vector<DesignatorAccess> Accesses;
    if (!ParseDesignatorSuffix(Accesses)) return nullptr;
    if (!Accesses.empty()) {
        auto Call = ParseMemberCall(Callee, std::move(Accesses), Line);
        if (!Call) return nullptr;
        return std::make_unique<MemberCallStmtAST>(std::move(Call));
    }

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
    if (!atCaseBoundary() && CurTok != tok_endcase && CurTok != tok_otherwise &&
        CurTok != tok_endif && CurTok != tok_else && CurTok != tok_endfunction &&
        CurTok != tok_endprocedure && CurTok != tok_endwhile && CurTok != tok_until &&
        CurTok != tok_next && CurTok != tok_catch && CurTok != tok_endtry &&
        CurTok != tok_eof) {
        Expr = ParseExpression();
        if (!Expr) return nullptr;
    }
    return std::make_unique<ReturnStmtAST>(std::move(Expr));
}
