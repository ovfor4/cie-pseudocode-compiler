#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/FunctionAST.h"
#include <cstdio>
#include <limits>

using namespace cps;

int Parser::GetTokPrecedence() {
    if (atCaseBoundary()) return -1;
    if (BinopPrecedence.find(CurTok) == BinopPrecedence.end()) return -1;
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}


bool Parser::ParseIntegerValue(bool Negative, int64_t &Out) {
    const uint64_t Max = uint64_t(std::numeric_limits<int64_t>::max());
    if (Lex.NumVal > Max + (Negative ? 1u : 0u)) {
        cps::sourceDiagnostic(Lex.getLine(), "error: Integer literal is outside the i64 range at line %d\n", Lex.getLine());
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

std::unique_ptr<ExprAST> Parser::ParseIdentifierExpr() {
    int Line = Lex.getLine();
    std::string IdName;
    if (CurTok == tok_operator) {
        int Op;
        IdName = ParseOperatorName(Op);
        if (IdName.empty()) return nullptr;
    } else {
        IdName = Lex.IdentifierStr;
        getNextToken();
    }

    if (CurTok == '(' && !atCaseBoundary()) {
        getNextToken();
        auto Args = ParseExprList(')', true);
        if (!Args) return nullptr;
        if (CurTok == '[' || CurTok == '^') {
            cps::sourceDiagnostic(Lex.getLine(), "error: record/array/pointer access on a function result is not supported at line %d\n",
                    Lex.getLine());
            return nullptr;
        }
        return ParseResultMethods(std::make_unique<CallExprAST>(IdName, std::move(*Args), Line), Line);
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
        cps::sourceDiagnostic(Lex.getLine(), "error: Expected a method name after '.' at line %d\n", Line);
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
        cps::sourceDiagnostic(Lex.getLine(), "error: record/array/pointer access on a function result is not supported at line %d\n",
                Lex.getLine());
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
                cps::sourceDiagnostic(Lex.getLine(), "error: Expected field name after '.' at line %d\n", Lex.getLine());
                return false;
            }
            DesignatorAccess A;
            A.Kind = AccessKind::Field;
            if (CurTok == tok_operator) {
                int Op;
                A.FieldName = ParseOperatorName(Op);
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
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected ')' after cast type at line %d\n", Lex.getLine());
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
        cps::sourceDiagnostic(Lex.getLine(), "error: expected ')' at line %d\n", Lex.getLine());
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
            int Op;
            Name = ParseOperatorName(Op);
            if (Name.empty()) return nullptr;
        } else if (CurTok == tok_identifier || CurTok == tok_delete) {
            Name = CurTok == tok_delete ? "DELETE" : Lex.IdentifierStr;
            getNextToken();
        } else {
            sourceDiagnostic(Lex.getLine(), "error: Expected method name at line %d\n", Lex.getLine());
            return nullptr;
        }
        if (CurTok != '(') {
            sourceDiagnostic(Lex.getLine(), "error: Expected '(' after method name at line %d\n", Lex.getLine());
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
        cps::sourceDiagnostic(Lex.getLine(), "error: NEW is only allowed on the right of an assignment at line %d\n", Lex.getLine());
        return nullptr;
    case tok_identifier: case tok_operator: return ParseIdentifierExpr();
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
        cps::sourceDiagnostic(Lex.getLine(), "error: unknown token '%c' (%d) at line %d when expecting an expression\n",
                (char)CurTok, CurTok, Lex.getLine());
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
            cps::sourceDiagnostic(Lex.getLine(), "error: '^' (address-of) requires a variable, array element or record field at line %d\n",
                    Line);
            return nullptr;
        }
        std::string BaseName = Lex.IdentifierStr;
        getNextToken();
        if (CurTok == '(') {
            cps::sourceDiagnostic(Lex.getLine(), "error: cannot take the address of a function result at line %d\n", Line);
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
            cps::sourceDiagnostic(Lex.getLine(), "error: Expected ',' or '%c' at line %d\n", CloseDelim, Lex.getLine());
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
