#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include "cps/Operator.h"

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
        cps::sourceError(Lex.getLine(), "Array parameter bounds must be integer literals");
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
                cps::sourceError(Lex.getLine(), "BYREF parameters are not allowed in a FUNCTION (procedures only)");
                return std::nullopt;
            }
            Current = PassMode::ByRef;
            getNextToken();
        } else if (CurTok == tok_byval) {
            Current = PassMode::ByVal;
            getNextToken();
        }

        if (CurTok != tok_identifier) {
            cps::sourceError(Lex.getLine(), "Expected argument name");
            return std::nullopt;
        }
        ParamDecl P;
        P.Line = Lex.getLine();
        P.Name = Lex.IdentifierStr;
        P.Mode = Current;
        getNextToken();

        if (CurTok != tok_colon) {
            cps::sourceError(Lex.getLine(), "Expected ':' after argument name");
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
                        cps::sourceError(Lex.getLine(), "Expected ':' between array parameter bounds");
                        return std::nullopt;
                    }
                    getNextToken();
                    if (!ParseSignedIntBound(Hi)) return std::nullopt;
                    P.DeclaredBounds.emplace_back(Lo, Hi);
                    if (CurTok == ']') break;
                    if (CurTok != ',') {
                        cps::sourceError(Lex.getLine(), "Expected ',' or ']' in array parameter bounds");
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
                cps::sourceError(Lex.getLine(), "Expected OF after ARRAY in parameter '%s'",
                        P.Name.c_str());
                return std::nullopt;
            }
            getNextToken();
        }

        P.TypeName = ParseTypeName(false);
        if (P.TypeName.empty()) {
            cps::sourceError(Lex.getLine(), "Expected argument type for '%s'", P.Name.c_str());
            return std::nullopt;
        }
        if (CurTok == tok_ellipsis) {
            if (P.IsArray || !isTemplatePack(P.TypeName)) {
                sourceError(P.Line, "A pack parameter must use the current template pack and cannot be ARRAY");
                return std::nullopt;
            }
            P.IsPack = true;
            getNextToken();
            if (CurTok != ')') {
                sourceError(P.Line, "A pack parameter must be last");
                return std::nullopt;
            }
        } else if (isTemplatePack(P.TypeName)) {
            sourceError(P.Line, "A template parameter pack requires '...'");
            return std::nullopt;
        }

        for (const ParamDecl &Prev : Params) {
            if (sameIdentifier(Prev.Name, P.Name)) {
                cps::sourceError(Lex.getLine(), "Duplicate parameter name '%s'",
                        P.Name.c_str());
                return std::nullopt;
            }
        }
        Params.push_back(std::move(P));

        if (CurTok == ')') break;
        if (CurTok != ',') {
            cps::sourceError(Lex.getLine(), "Expected ',' or ')'");
            return std::nullopt;
        }
        getNextToken();
        if (CurTok == ')') {
            cps::sourceError(Lex.getLine(), "Expected another parameter after ','");
            return std::nullopt;
        }
    }
    getNextToken();
    return Params;
}

std::string Parser::ParseOperatorName(int *OpOut) {
    getNextToken(); // OPERATOR
    int Op = CurTok;
    if (OpOut) *OpOut = Op;
    if (operatorSpelling(Op).empty()) {
        sourceError(Lex.getLine(), "Expected a supported operator after OPERATOR");
        getNextToken();
        return {};
    }
    getNextToken();
    if (CurTok != '(') {
        sourceError(Lex.getLine(), "Expected '(' after operator name");
        return {};
    }
    return operatorName(Op);
}

// A callee is an identifier or OPERATOR <op>; false means a diagnosed failure.
bool Parser::ParseCalleeName(std::string &Name) {
    if (CurTok == tok_operator) {
        Name = ParseOperatorName();
        return !Name.empty();
    }
    Name = Lex.IdentifierStr;
    getNextToken();
    return true;
}

std::unique_ptr<FunctionDefAST> Parser::ParseFunction(const std::string &OwnerClass, bool IsPublic, bool IsOverload) {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok == tok_delete || (CurTok == tok_identifier && sameIdentifier(Lex.IdentifierStr, "DELETE"))) {
        cps::sourceError(Line, "destructor DELETE must be a PROCEDURE");
        return nullptr;
    }
    if (CurTok == tok_new || (CurTok == tok_identifier && sameIdentifier(Lex.IdentifierStr, "NEW"))) {
        cps::sourceError(Line, "constructor NEW must be a PROCEDURE");
        return nullptr;
    }
    if (CurTok != tok_identifier && CurTok != tok_operator) {
        cps::sourceError(Lex.getLine(), "Expected function name");
        return nullptr;
    }

    int Operator = 0;
    std::string Name;
    if (CurTok == tok_operator) {
        Name = ParseOperatorName(&Operator);
        if (Name.empty()) return nullptr;
        if (!IsOverload) {
            sourceError(Line, "An operator function must be declared OVERLOAD");
            return nullptr;
        }
    } else {
        Name = Lex.IdentifierStr;
        getNextToken();
    }

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/false);
    if (!Args) return nullptr;

    std::string RetType = "INTEGER";
    bool IsDefaultReturn = false;
    if (CurTok == tok_returns) {
        getNextToken();
        if (CurTok == tok_default) {
            IsDefaultReturn = true;
            getNextToken();
        }
        if (CurTok == tok_array) {
            cps::sourceError(Lex.getLine(), "A FUNCTION cannot RETURN an array; use a BYREF PROCEDURE parameter instead");
            return nullptr;
        }
        RetType = ParseTypeName(true);
        if (RetType.empty()) {
            cps::sourceError(Lex.getLine(), "Expected return type for function '%s'", Name.c_str());
            return nullptr;
        }
    }

    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), RetType, false, OwnerClass, IsPublic, IsOverload);
    Proto->setOperator(Operator);
    Proto->setDefaultReturn(IsDefaultReturn);

    auto Def = ParseFunctionBody(std::move(Proto), tok_endfunction, "expected ENDFUNCTION", Line);
    if (IsDefaultReturn && !CurrentTemplate.empty()) {
        sourceError(Line, "DEFAULT return types are not supported in templates");
        return nullptr;
    }
    return Def;
}

std::unique_ptr<FunctionDefAST> Parser::ParseProcedure(const std::string &OwnerClass, bool IsPublic, bool IsOverload, bool IsImplicit) {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok != tok_identifier && CurTok != tok_new && CurTok != tok_delete && CurTok != tok_operator) {
        cps::sourceError(Lex.getLine(), "Expected procedure name");
        return nullptr;
    }

    int Operator = 0;
    bool IsOperator = CurTok == tok_operator;
    std::string OperatorName;
    if (IsOperator) {
        OperatorName = ParseOperatorName(&Operator);
        if (OperatorName.empty()) return nullptr;
        if (Operator != tok_assign || OwnerClass.empty() || !IsOverload) {
            sourceError(Line, "Assignment operator must be an OVERLOAD member PROCEDURE");
            return nullptr;
        }
    }
    std::string Name = IsOperator ? OperatorName : CurTok == tok_new ? "NEW" : CurTok == tok_delete ? "DELETE" : Lex.IdentifierStr;
    if (OwnerClass.empty() && sameIdentifier(Name, "DELETE")) {
        cps::sourceError(Line, "destructor DELETE is only allowed in a CLASS");
        return nullptr;
    }
    if (OwnerClass.empty() && sameIdentifier(Name, "NEW")) {
        cps::sourceError(Line, "constructor NEW is only allowed in a CLASS");
        return nullptr;
    }
    if (!IsOperator) getNextToken();

    auto Args = ParsePrototypeArgs(/*AllowByRef=*/true);
    if (!Args) return nullptr;
    if (IsImplicit) {
        for (const auto &P : *Args) if (P.IsPack) {
            cps::sourceError(Line, "IMPLICIT constructor parameters cannot be packs");
            return nullptr;
        }
    }
    if (sameIdentifier(Name, "DELETE") && !Args->empty()) {
        cps::sourceError(Line, "destructor DELETE must have no parameters");
        return nullptr;
    }
    auto Proto = std::make_unique<PrototypeAST>(Name, std::move(*Args), "VOID", false, OwnerClass, IsPublic, IsOverload);
    Proto->setOperator(Operator);
    Proto->setImplicit(IsImplicit);

    return ParseFunctionBody(std::move(Proto), tok_endprocedure, "expected ENDPROCEDURE", Line);
}

// Shared FUNCTION/PROCEDURE tail: the body (with the prototype's pack
// parameters in scope) up to EndToken, which must be present.
std::unique_ptr<FunctionDefAST> Parser::ParseFunctionBody(
    std::unique_ptr<PrototypeAST> Proto, int EndToken, const char *EndMessage, int Line) {
    auto SavedPacks = std::move(PackNames);
    PackNames.clear();
    for (const auto &P : Proto->getParams()) if (P.IsPack) PackNames.insert(P.Name);
    std::vector<std::unique_ptr<StmtAST>> Body = ParseBlock({EndToken});
    PackNames = std::move(SavedPacks);

    if (CurTok != EndToken) {
        cps::sourceError(Lex.getLine(), "%s", EndMessage);
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
        cps::sourceError(Lex.getLine(), "EXTERN must be followed by \"C\"");
        return nullptr;
    }
    getNextToken();
    if (CurTok != tok_extern_function && CurTok != tok_extern_procedure) {
        cps::sourceError(Lex.getLine(), "Expected EXTERN_FUNCTION or EXTERN_PROCEDURE after EXTERN \"C\"");
        return nullptr;
    }
    bool IsFunction = CurTok == tok_extern_function;
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceError(Lex.getLine(), "Expected C function name");
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    std::vector<ParamDecl> Params;
    bool IsVariadic = false;
    if (CurTok != '(') {
        cps::sourceError(Lex.getLine(), "Expected '(' after C function name '%s'",
                Name.c_str());
        return nullptr;
    }
    getNextToken();
    while (CurTok != ')') {
        if (CurTok == tok_ellipsis) {
            IsVariadic = true;
            getNextToken();
            if (CurTok != ')') {
                cps::sourceError(Lex.getLine(), "'...' must be the last parameter");
                return nullptr;
            }
            break;
        }
        if (CurTok == tok_byref || CurTok == tok_byval) {
            cps::sourceError(Lex.getLine(), "EXTERN parameters cannot be BYREF or BYVAL; use a C pointer type");
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
            cps::sourceError(Lex.getLine(), "Expected ',' or ')'");
            return nullptr;
        }
        getNextToken();
        if (CurTok == ')') {
            cps::sourceError(Lex.getLine(), "Expected another parameter after ','");
            return nullptr;
        }
    }
    getNextToken();

    std::string RetType = "VOID";
    if (CurTok == tok_returns) {
        if (!IsFunction) {
            cps::sourceError(Lex.getLine(), "EXTERN_PROCEDURE cannot have RETURNS");
            return nullptr;
        }
        getNextToken();
        RetType = ParseTypeName(false);
        if (RetType.empty()) return nullptr;
    } else if (IsFunction) {
        cps::sourceError(Line, "EXTERN_FUNCTION '%s' requires RETURNS",
                Name.c_str());
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
    if (CurTok != tok_identifier && CurTok != tok_operator) {
        cps::sourceError(Lex.getLine(), "Expected callee name after CALL");
        return nullptr;
    }

    std::string Callee;
    if (!ParseCalleeName(Callee)) return nullptr;

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

    if (CurTok == '.') {
        auto Result = ParseResultMethods(std::make_unique<CallExprAST>(Callee, std::move(Args), Line, std::move(TemplateArgs)), Line);
        if (!Result) return nullptr;
        return std::make_unique<MemberCallStmtAST>(
            std::unique_ptr<MemberCallExprAST>(static_cast<MemberCallExprAST *>(Result.release())));
    }
    return std::make_unique<CallStmtAST>(Callee, std::move(Args), Line, std::move(TemplateArgs));
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
