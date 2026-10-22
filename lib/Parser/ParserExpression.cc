#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <limits>
#include <functional>

using namespace cps;

int Parser::GetTokPrecedence() {
    if (atCaseBoundary()) return -1;
    auto It = BinopPrecedence.find(CurTok);
    return It == BinopPrecedence.end() ? -1 : It->second;
}


bool Parser::ParseIntegerValue(bool Negative, int64_t &Out) {
    const uint64_t Max = uint64_t(std::numeric_limits<int64_t>::max());
    if (Lex.NumVal > Max + (Negative ? 1u : 0u)) {
        cps::sourceError(Lex.getLine(), "Integer literal is outside the i64 range");
        getNextToken();
        return false;
    }
    Out = Negative && Lex.NumVal == Max + 1
        ? std::numeric_limits<int64_t>::min()
        : Negative ? -static_cast<int64_t>(Lex.NumVal) : static_cast<int64_t>(Lex.NumVal);
    getNextToken();
    return true;
}

std::unique_ptr<ExprAST> Parser::ParseNumberExpr() {
    if (CurTok == tok_number_real) {
        auto Result = std::make_unique<RealExprAST>(Lex.RealVal);
        getNextToken();
        return Result;
    }
    int64_t Value;
    if (!ParseIntegerValue(false, Value)) return nullptr;
    return std::make_unique<IntegerExprAST>(Value);
}

// Grammar lookahead distinguishes a template call from a comparison without
// consuming tokens. Like C#, A < B, C > (D) selects the template-call grammar.
bool Parser::isTemplateCall() {
    if (CurTok != '<') return false;
    unsigned Offset = 0;
    auto IsTypeToken = [](int Tok) {
        return Tok == tok_identifier || Tok == tok_integer_kw || Tok == tok_real_kw ||
            Tok == tok_boolean_kw || Tok == tok_string_kw || Tok == tok_char_kw ||
            Tok == tok_c_int || Tok == tok_c_char || Tok == tok_c_string || Tok == tok_c_double;
    };
    std::function<bool()> Type = [&] {
        if (!IsTypeToken(Lex.peekToken(Offset))) return false;
        ++Offset;
        if (Lex.peekToken(Offset) == '<') {
            ++Offset;
            if (!Type()) return false;
            while (Lex.peekToken(Offset) == ',') {
                ++Offset;
                if (!Type()) return false;
            }
            if (Lex.peekToken(Offset++) != '>') return false;
        }
        if (Lex.peekToken(Offset) == tok_ellipsis) ++Offset;
        return true;
    };
    if (!Type()) return false;
    while (Lex.peekToken(Offset) == ',') {
        ++Offset;
        if (!Type()) return false;
    }
    return Lex.peekToken(Offset) == '>' && Lex.peekToken(Offset + 1) == '(';
}

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    int Line = Lex.getLine();
    std::string IdName;
    if (!ParseCalleeName(IdName)) return nullptr;

    if (PackNames.count(IdName)) {
        if (CurTok != tok_ellipsis) {
            sourceError(Line, "A parameter pack must be expanded with '...'");
            return nullptr;
        }
        getNextToken();
        return std::make_unique<PackExpansionExprAST>(IdName, Line);
    }
    if (CurTok == tok_ellipsis) {
        sourceError(Line, "Only a parameter pack may be expanded");
        getNextToken();
        return nullptr;
    }
    std::vector<std::string> TemplateArgs;
    if (isTemplateCall()) {
        auto Parsed = ParseTemplateArgs();
        if (!Parsed) return nullptr;
        TemplateArgs = std::move(*Parsed);
    }
    if (CurTok == '(' && !atCaseBoundary()) {
        getNextToken();
        auto Args = ParseExprList(')', true);
        if (!Args) return nullptr;
        if (CurTok == '[' || CurTok == '^') {
            cps::sourceError(Lex.getLine(), "record/array/pointer access on a function result is not supported");
            return nullptr;
        }
        return ParseResultMethods(std::make_unique<CallExprAST>(IdName, std::move(*Args), Line, std::move(TemplateArgs)), Line);
    }

    std::vector<DesignatorAccess> Accesses;
    if (!ParseDesignatorSuffix(Accesses)) return nullptr;

    if (CurTok == '(' && !atCaseBoundary())
        return ParseMemberCall(IdName, std::move(Accesses), Line);

    // Degrade rule: shapes that existed before designators keep their nodes.
    if (Accesses.empty())
        return std::make_unique<VariableExprAST>(IdName);
    if (Accesses.size() == 1 && Accesses[0].Kind == AccessKind::Index)
        return std::make_unique<ArrayAccessExprAST>(IdName, std::move(Accesses[0].Indices), Line);
    return std::make_unique<DesignatorExprAST>(IdName, std::move(Accesses), Line);
}

// The final field suffix names the method; preceding suffixes form its receiver.
std::unique_ptr<MemberCallExprAST> Parser::ParseMemberCall(
    const std::string &Name, std::vector<DesignatorAccess> Accesses, int Line) {
    if (Accesses.empty() || Accesses.back().Kind != AccessKind::Field) {
        cps::sourceError(Line, "Expected a method name after '.'");
        return nullptr;
    }
    std::string Method = Accesses.back().FieldName;
    Accesses.pop_back();
    auto Receiver = std::make_unique<DesignatorExprAST>(Name, std::move(Accesses), Line);
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok == '(') {
        getNextToken();
        auto Parsed = ParseExprList(')', true);
        if (!Parsed) return nullptr;
        Args = std::move(*Parsed);
    }
    if (CurTok == '[' || CurTok == '^') {
        cps::sourceError(Lex.getLine(), "record/array/pointer access on a function result is not supported");
        return nullptr;
    }
    auto Result = ParseResultMethods(std::make_unique<MemberCallExprAST>(std::move(Receiver), Method, std::move(Args), Line), Line);
    return std::unique_ptr<MemberCallExprAST>(static_cast<MemberCallExprAST *>(Result.release()));
}

// Postfix accessor chain after an identifier: '[' exprs ']' | '.' IDENT | '^'
bool Parser::ParseDesignatorSuffix(std::vector<DesignatorAccess> &Out) {
    while (true) {
        if (atCaseBoundary()) return true;
        if (CurTok == '[') {
            getNextToken();
            auto Indices = ParseExprList(']', false);
            if (!Indices) return false;
            DesignatorAccess A;
            A.Kind = AccessKind::Index;
            A.Indices = std::move(*Indices);
            Out.push_back(std::move(A));
        } else if (CurTok == '.') {
            getNextToken();
            if (CurTok != tok_identifier && CurTok != tok_new && CurTok != tok_delete && CurTok != tok_operator) {
                cps::sourceError(Lex.getLine(), "Expected field name after '.'");
                return false;
            }
            DesignatorAccess A;
            A.Kind = AccessKind::Field;
            if (CurTok == tok_operator) {
                A.FieldName = ParseOperatorName();
                if (A.FieldName.empty()) return false;
            } else {
                A.FieldName = CurTok == tok_new ? "NEW" : CurTok == tok_delete ? "DELETE" : Lex.IdentifierStr;
                getNextToken();
            }
            Out.push_back(std::move(A));
        } else if (CurTok == '^') {
            getNextToken();
            DesignatorAccess A;
            A.Kind = AccessKind::Deref;
            Out.push_back(std::move(A));
        } else {
            return true;
        }
    }
}


std::unique_ptr<ExprAST> Parser::ParseParenExpr() {
    getNextToken();
    bool IsType = CurTok == tok_integer_kw || CurTok == tok_real_kw ||
        CurTok == tok_boolean_kw || CurTok == tok_string_kw || CurTok == tok_char_kw ||
        CurTok == tok_c_int || CurTok == tok_c_char || CurTok == tok_c_string ||
        CurTok == tok_c_double ||
        (CurTok == tok_identifier && TypeNames.count(Lex.IdentifierStr));
    if (IsType) {
        std::string TypeName = ParseTypeName();
        if (TypeName.empty()) return nullptr;
        if (CurTok != ')') {
            cps::sourceError(Lex.getLine(), "Expected ')' after cast type");
            return nullptr;
        }
        getNextToken();
        auto Operand = ParseUnary();
        if (!Operand) return nullptr;
        return std::make_unique<CastExprAST>(TypeName, std::move(Operand));
    }
    auto V = ParseExpression();
    if (!V) return nullptr;
    if (CurTok != ')') {
        cps::sourceError(Lex.getLine(), "expected ')'");
        return nullptr;
    }
    getNextToken();
    return V;
}

std::unique_ptr<ExprAST> Parser::ParseResultMethods(std::unique_ptr<ExprAST> Expr, int Line) {
    while (CurTok == '.' && !atCaseBoundary()) {
        getNextToken();
        std::string Name;
        if (CurTok == tok_operator) {
            Name = ParseOperatorName();
            if (Name.empty()) return nullptr;
        } else if (CurTok == tok_identifier || CurTok == tok_delete) {
            Name = CurTok == tok_delete ? "DELETE" : Lex.IdentifierStr;
            getNextToken();
        } else {
            sourceError(Lex.getLine(), "Expected method name");
            return nullptr;
        }
        if (CurTok != '(') {
            sourceError(Lex.getLine(), "Expected '(' after method name");
            return nullptr;
        }
        getNextToken();
        auto Args = ParseExprList(')', true);
        if (!Args) return nullptr;
        Expr = std::make_unique<MemberCallExprAST>(std::move(Expr), Name, std::move(*Args), Line);
    }
    return Expr;
}

std::unique_ptr<ExprAST> Parser::ParsePrimary() {
    int Line = Lex.getLine();
    auto Expr = ParsePrimaryImpl();
    if (Expr && !Expr->getLine()) Expr->setLine(Line);
    return Expr ? ParseResultMethods(std::move(Expr), Line) : nullptr;
}

std::unique_ptr<ExprAST> Parser::ParsePrimaryImpl() {
    switch (CurTok) {
    case tok_new:
        cps::sourceError(Lex.getLine(), "NEW is only allowed on the right of an assignment");
        return nullptr;
    case tok_identifier: case tok_operator: return ParseIdentifierExpr();
    case tok_number_int: return ParseNumberExpr();
    case tok_number_real: return ParseNumberExpr();
    case tok_string_literal: case tok_c_string_literal: {
        auto Res = std::make_unique<StringExprAST>(Lex.StringVal, CurTok == tok_c_string_literal);
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
        cps::sourceError(Lex.getLine(), "unknown token '%c' (%d) when expecting an expression",
                (char)CurTok, CurTok);
        return nullptr;
    }
}

std::unique_ptr<ExprAST> Parser::ParseUnary() {
    int Line = Lex.getLine();
    auto Expr = ParseUnaryImpl();
    if (Expr && !Expr->getLine()) Expr->setLine(Line);
    return Expr;
}

std::unique_ptr<ExprAST> Parser::ParseUnaryImpl() {
    if (CurTok == tok_not) {
        getNextToken();
        auto Operand = ParseUnary();
        if (!Operand) return nullptr;
        return std::make_unique<UnaryExprAST>(tok_not, std::move(Operand));
    }

    // ^<designator> — address-of; the operand must be an lvalue shape.
    if (CurTok == '^') {
        int Line = Lex.getLine();
        getNextToken();
        if (CurTok != tok_identifier) {
            cps::sourceError(Line, "'^' (address-of) requires a variable, array element or record field");
            return nullptr;
        }
        std::string BaseName = Lex.IdentifierStr;
        getNextToken();
        if (CurTok == '(') {
            cps::sourceError(Line, "cannot take the address of a function result");
            return nullptr;
        }
        std::vector<DesignatorAccess> Accesses;
        if (!ParseDesignatorSuffix(Accesses)) return nullptr;
        auto Target = std::make_unique<DesignatorExprAST>(BaseName, std::move(Accesses), Line);
        return std::make_unique<AddrOfExprAST>(std::move(Target));
    }

    if (CurTok == '-') {
        getNextToken();
        if (CurTok == tok_number_int) {
            int64_t Value;
            if (!ParseIntegerValue(true, Value)) return nullptr;
            return std::make_unique<IntegerExprAST>(Value);
        }
        auto Operand = ParseUnary();
        if (!Operand) return nullptr;
        return std::make_unique<UnaryExprAST>('-', std::move(Operand));
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


std::optional<std::vector<std::unique_ptr<ExprAST>>> Parser::ParseExprList(char CloseDelim, bool AllowEmpty) {
    std::vector<std::unique_ptr<ExprAST>> Elems;
    if (AllowEmpty && CurTok == CloseDelim) {
        getNextToken();
        return Elems;
    }

    while (true) {
        auto E = ParseExpression();
        if (!E) return std::nullopt;
        Elems.push_back(std::move(E));

        if (CurTok == CloseDelim) break;
        if (CurTok != ',') {
            cps::sourceError(Lex.getLine(), "Expected ',' or '%c'", CloseDelim);
            return std::nullopt;
        }
        getNextToken();
    }
    getNextToken();
    return Elems;
}

std::unique_ptr<ExprAST> Parser::ParseAssignmentRHS() {
    if (CurTok != tok_new) return ParseExpression();
    int Line = Lex.getLine();
    getNextToken();
    std::string ClassName = ParseTypeName(false);
    if (ClassName.empty()) return nullptr;
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok == '(') {
        getNextToken();
        auto Parsed = ParseExprList(')', true);
        if (!Parsed) return nullptr;
        Args = std::move(*Parsed);
    }
    return std::make_unique<NewExprAST>(ClassName, std::move(Args), Line);
}
