#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/TypeAST.h"
#include <cstdio>

using namespace cps;

bool Parser::isTemplatePack(const std::string &Name) const {
    for (const auto &P : CurrentTemplate)
        if (P.IsPack && sameIdentifier(P.Name, Name)) return true;
    return false;
}

// TEMPLATE <T, Rest...> [OVERLOAD] FUNCTION/PROCEDURE or CLASS.
// Invalid nesting still consumes the entire declaration before diagnosing it.
std::unique_ptr<StmtAST> Parser::ParseTemplateDecl() {
    int Line = Lex.getLine();
    bool AtTopLevel = BlockDepth == 1 && CurrentTemplate.empty();
    getNextToken();
    if (CurTok != '<') {
        sourceError(Line, "Expected '<' after TEMPLATE");
        return nullptr;
    }
    getNextToken();
    std::vector<TemplateParam> Params;
    IdentifierSet Names;
    bool Invalid = false;
    bool SeenPack = false;
    while (true) {
        if (CurTok != tok_identifier) {
            sourceError(Lex.getLine(), "Expected template parameter name");
            return nullptr;
        }
        TemplateParam P{Lex.IdentifierStr, false, Lex.getLine()};
        if (!Names.insert(P.Name).second) {
            sourceError(P.Line, "Duplicate template parameter '%s'", P.Name.c_str());
            Invalid = true;
        }
        getNextToken();
        if (CurTok == tok_ellipsis) {
            P.IsPack = true;
            if (SeenPack) {
                sourceError(P.Line, "A template may have only one parameter pack");
                Invalid = true;
            }
            SeenPack = true;
            getNextToken();
        }
        Params.push_back(std::move(P));
        if (CurTok == '>') break;
        if (CurTok != ',') {
            sourceError(Lex.getLine(), "Expected ',' or '>' in TEMPLATE");
            return nullptr;
        }
        if (Params.back().IsPack) {
            sourceError(Params.back().Line, "A template parameter pack must be last");
            Invalid = true;
        }
        getNextToken();
    }
    getNextToken();

    auto SavedTemplate = std::move(CurrentTemplate);
    auto SavedTypes = TypeNames;
    CurrentTemplate = Params;
    for (const auto &P : Params) TypeNames.insert(P.Name);
    bool IsOverload = CurTok == tok_overload;
    if (IsOverload) getNextToken();
    std::unique_ptr<StmtAST> Decl;
    if (CurTok == tok_function || CurTok == tok_procedure) {
        bool Operator = Lex.peekToken() == tok_operator;
        if (Operator) {
            sourceError(Line, "Operator templates are not supported");
            Invalid = true;
            int End = CurTok == tok_function ? tok_endfunction : tok_endprocedure;
            do { getNextToken(); } while (CurTok != End && CurTok != tok_eof);
            if (CurTok == End) getNextToken();
        } else {
            auto Function = CurTok == tok_function ? ParseFunction({}, true, IsOverload)
                                                  : ParseProcedure({}, true, IsOverload);
            if (Function) Decl = std::make_unique<TemplateDeclAST>(Params, std::move(Function));
        }
    } else if (CurTok == tok_class && !IsOverload) {
        int ClassLine = Lex.getLine();
        auto Class = ParseTypeDecl();
        if (Class) {
            Class->setLine(ClassLine);
            Decl = std::make_unique<TemplateDeclAST>(Params,
                std::unique_ptr<ClassTypeDeclAST>(static_cast<ClassTypeDeclAST *>(Class.release())));
        }
    } else if (CurTok == tok_type) {
        sourceError(Line, "TEMPLATE cannot prefix TYPE; use CLASS");
        ParseTypeDecl();
        Invalid = true;
    } else {
        sourceError(Line, "TEMPLATE must be followed by FUNCTION, PROCEDURE or CLASS");
        Invalid = true;
    }
    CurrentTemplate = std::move(SavedTemplate);
    TypeNames = std::move(SavedTypes);
    if (!AtTopLevel) {
        sourceError(Line, "TEMPLATE declarations are only allowed at top level");
        Invalid = true;
    }
    if (Invalid || !Decl) return nullptr;
    if (auto *T = dynamic_cast<TemplateDeclAST *>(Decl.get()); T->getClass()) TypeNames.insert(T->getName());
    Decl->setLine(Line);
    return Decl;
}

std::optional<std::vector<std::string>> Parser::ParseTemplateArgs() {
    getNextToken(); // '<'
    std::vector<std::string> Args;
    while (true) {
        int Line = Lex.getLine();
        std::string Type = ParseTypeName(false);
        if (Type.empty()) return std::nullopt;
        if (CurTok == tok_ellipsis) {
            if (!isTemplatePack(Type)) {
                sourceError(Line, "Only the current template parameter pack may use '...' in a type argument");
                return std::nullopt;
            }
            Type += "...";
            getNextToken();
        }
        Args.push_back(std::move(Type));
        if (CurTok == '>') break;
        if (CurTok != ',') {
            sourceError(Lex.getLine(), "Expected ',' or '>' in template arguments");
            return std::nullopt;
        }
        getNextToken();
    }
    getNextToken();
    return Args;
}

// TYPE <id> = (value1, value2, ...)     enumerated type
// TYPE <id> = ^<type>                   pointer type
// TYPE <id>                             record type
//    DECLARE <field> : <type>
//    ...
// ENDTYPE
// TYPE <id> = SET OF <type> is recognised and rejected (RANDOM precedent).
// CLASS <id> PUBLIC/PRIVATE <field> : <type> ... ENDCLASS
// TYPE and CLASS are top-level-only; a nested declaration is still consumed
// in full so its body cannot be replayed as statements.
std::unique_ptr<StmtAST> Parser::ParseTypeDecl() {
    bool AtTopLevel = BlockDepth <= 1;
    bool IsClass = CurTok == tok_class;
    const char *Keyword = IsClass ? "CLASS" : "TYPE";
    int Line = Lex.getLine();
    getNextToken(); // eat TYPE or CLASS

    if (CurTok != tok_identifier && !(IsClass && CurTok == tok_string_kw)) {
        cps::sourceError(Lex.getLine(), "Expected type name after %s", Keyword);
        if (IsClass) ParseRecordBody("<unnamed>", Line, tok_endclass);
        return nullptr;
    }
    std::string Name = CurTok == tok_string_kw ? "STRING" : Lex.IdentifierStr;
    getNextToken();
    if (IsClass && !CurrentTemplate.empty()) TypeNames.insert(Name);

    std::unique_ptr<StmtAST> Decl;

    if (!IsClass && CurTok == tok_eq) {
        getNextToken();

        if (CurTok == tok_c_type) {
            getNextToken();
            if (CurTok != '(') {
                cps::sourceError(Lex.getLine(), "Expected '(' after C_TYPE");
                return nullptr;
            }
            getNextToken();
            if (CurTok != tok_string_literal) {
                cps::sourceError(Lex.getLine(), "C_TYPE requires a string literal");
                return nullptr;
            }
            std::string Expression = Lex.StringVal;
            getNextToken();
            if (CurTok != ')') {
                cps::sourceError(Lex.getLine(), "Expected ')' after C_TYPE string");
                return nullptr;
            }
            getNextToken();
            Decl = std::make_unique<CTypeDeclAST>(Name, Expression);
        } else if (CurTok == '(') {
            getNextToken();
            std::vector<std::string> Values;
            while (true) {
                if (CurTok != tok_identifier) {
                    cps::sourceError(Lex.getLine(), "Expected an enum value name in TYPE %s",
                            Name.c_str());
                    return nullptr;
                }
                Values.push_back(Lex.IdentifierStr);
                getNextToken();
                if (CurTok == ')') break;
                if (CurTok != ',') {
                    cps::sourceError(Lex.getLine(), "Expected ',' or ')' in TYPE %s",
                            Name.c_str());
                    return nullptr;
                }
                getNextToken();
            }
            getNextToken(); // eat ')'
            Decl = std::make_unique<EnumTypeDeclAST>(Name, std::move(Values));
        } else if (CurTok == '^') {
            getNextToken();
            std::string Pointee = ParseTypeName(false);
            if (Pointee.empty()) return nullptr;
            Decl = std::make_unique<PointerTypeDeclAST>(Name, Pointee);
        } else if (CurTok == tok_set) {
            cps::sourceError(Lex.getLine(), "SET OF types are not implemented");
            getNextToken();
            return nullptr;
        } else {
            cps::sourceError(Lex.getLine(), "Expected '(', '^' or C_TYPE after '=' in TYPE %s",
                    Name.c_str());
            return nullptr;
        }
    } else {
        std::vector<std::unique_ptr<FunctionDefAST>> Methods;
        auto Fields = ParseRecordBody(Name, Line, IsClass ? tok_endclass : tok_endtype, &Methods);
        if (!Fields) return nullptr;
        if (IsClass)
            Decl = std::make_unique<ClassTypeDeclAST>(Name, std::move(*Fields), std::move(Methods));
        else
            Decl = std::make_unique<RecordTypeDeclAST>(Name, std::move(*Fields));
    }

    if (!AtTopLevel) {
        cps::sourceError(Line, "%s declarations are only allowed at top level", Keyword);
        return nullptr;
    }
    TypeNames.insert(Name);
    return Decl;
}

// Shared record body and field parsing. Fields are data, never statements.
std::optional<std::vector<RecordFieldDecl>> Parser::ParseRecordBody(
    const std::string &Name, int Line, int EndToken,
    std::vector<std::unique_ptr<FunctionDefAST>> *Methods) {
    bool IsClass = EndToken == tok_endclass;
    const char *Keyword = IsClass ? "CLASS" : "TYPE";
    const char *EndKeyword = IsClass ? "ENDCLASS" : "ENDTYPE";
    auto AtField = [&] {
        return IsClass ? (CurTok == tok_public || CurTok == tok_private || CurTok == tok_identifier ||
                            CurTok == tok_overload || CurTok == tok_function || CurTok == tok_procedure)
                       : CurTok == tok_declare;
    };
    std::vector<RecordFieldDecl> Fields;
    bool HadFieldError = false;
    while (CurTok != EndToken && CurTok != tok_eof) {
        if (CurTok == tok_template) {
            // A class body has no ParseBlock of its own.
            ++BlockDepth;
            ParseTemplateDecl();
            --BlockDepth;
            ++NumErrors;
            HadFieldError = true;
            continue;
        }
        if (AtField()) {
            // Class fields default to public, like record fields.
            bool IsPublic = CurTok != tok_private;
            if (IsClass && (CurTok == tok_public || CurTok == tok_private) &&
                (Lex.peekToken() == tok_overload || Lex.peekToken() == tok_function || Lex.peekToken() == tok_procedure))
                getNextToken();
            if (IsClass && (CurTok == tok_overload || CurTok == tok_function || CurTok == tok_procedure)) {
                bool IsOverload = CurTok == tok_overload;
                if (IsOverload) getNextToken();
                if (CurTok != tok_function && CurTok != tok_procedure) {
                    cps::sourceError(Lex.getLine(), "OVERLOAD must be followed by FUNCTION or PROCEDURE");
                    HadFieldError = true;
                    continue;
                }
                auto Method = CurTok == tok_function ? ParseFunction(Name, IsPublic, IsOverload)
                                                    : ParseProcedure(Name, IsPublic, IsOverload);
                if (Method && Methods) {
                    Methods->push_back(std::move(Method));
                    continue;
                }
            } else if (ParseRecordField(Fields, IsPublic)) continue;
        } else {
            cps::sourceError(Lex.getLine(), "Expected %s or %s in %s %s",
                    IsClass ? "field, method, PUBLIC or PRIVATE" : "DECLARE", EndKeyword,
                    Keyword, Name.c_str());
        }
        HadFieldError = true;
        // Consume invalid bodies rather than replaying their contents as
        // statements. Nested type declarations are consumed in full too.
        while (!AtField() && CurTok != EndToken && CurTok != tok_eof) {
            if (CurTok == tok_class || CurTok == tok_type)
                ParseTypeDecl();
            else if (CurTok == tok_template) {
                ++BlockDepth;
                ParseTemplateDecl();
                --BlockDepth;
                ++NumErrors;
            }
            else
                getNextToken();
        }
    }
    if (CurTok != EndToken) {
        cps::sourceError(Lex.getLine(), "Expected %s in %s %s",
                EndKeyword, Keyword, Name.c_str());
        return std::nullopt;
    }
    getNextToken(); // eat body terminator
    if (Fields.empty() && (!Methods || Methods->empty()) && !HadFieldError) {
        cps::sourceError(Line, "%s %s has no %s", Keyword, Name.c_str(),
                IsClass ? "members" : "fields");
        return std::nullopt;
    }
    if (HadFieldError) return std::nullopt;
    return Fields;
}

// <field introducer> <field> : <type>. Array fields are deferred.
bool Parser::ParseRecordField(std::vector<RecordFieldDecl> &Out, bool IsPublic) {
    int Line = Lex.getLine();
    const char *Introducer = CurTok == tok_declare ? "DECLARE"
                           : CurTok == tok_public ? "PUBLIC" : "PRIVATE";
    if (CurTok != tok_identifier) getNextToken(); // optional class access keyword
    if (CurTok != tok_identifier) {
        cps::sourceError(Lex.getLine(), "Expected field name after %s", Introducer);
        return false;
    }
    std::string FieldName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_colon) {
        cps::sourceError(Lex.getLine(), "Expected ':' after field name %s",
                FieldName.c_str());
        return false;
    }
    getNextToken();

    if (CurTok == tok_array) {
        cps::sourceError(Lex.getLine(), "Array fields inside records are not implemented");
        return false;
    }

    std::string TypeName = ParseTypeName(false);
    if (TypeName.empty()) return false;
    if (CurTok == tok_ellipsis || isTemplatePack(TypeName)) {
        sourceError(Line, "Pack fields are not supported");
        if (CurTok == tok_ellipsis) getNextToken();
        return false;
    }

    Out.push_back({FieldName, TypeName, IsPublic});
    return true;
}

std::string Parser::ParseTypeName(bool AllowVoid) {
    switch (CurTok) {
    case tok_c_int: getNextToken(); return "C_INT";
    case tok_c_char: getNextToken(); return "C_CHAR";
    case tok_c_string: getNextToken(); return "C_STRING";
    case tok_c_double: getNextToken(); return "C_DOUBLE";
    default: break;
    }
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
        if (!AllowVoid && sameIdentifier(TypeName, "VOID")) {
            cps::sourceError(Lex.getLine(), "VOID is not allowed here");
            return "";
        }
        getNextToken();
        if (CurTok == '<') {
            auto Args = ParseTemplateArgs();
            if (!Args) return {};
            TypeName += "<";
            for (size_t I = 0; I < Args->size(); ++I) {
                if (I) TypeName += ",";
                TypeName += (*Args)[I];
            }
            TypeName += ">";
        }
        return TypeName;
    }

    cps::sourceError(Lex.getLine(), "Unknown type name");
    return "";
}


std::unique_ptr<StmtAST> Parser::ParseDeclare() {
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceError(Lex.getLine(), "Expected identifier after DECLARE");
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_colon) {
        cps::sourceError(Lex.getLine(), "Expected ':' after variable name '%s'", Name.c_str());
        return nullptr;
    }
    getNextToken();
    
    if (CurTok == tok_array) {
        getNextToken(); 
        if (CurTok != '[') {
            cps::sourceError(Lex.getLine(), "Expected '[' after ARRAY");
            return nullptr;
        }
        getNextToken();
        
        std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<ExprAST>>> Bounds;
        
        while (true) {
            auto Lower = ParseExpression();
            if (!Lower) return nullptr;
            
            if (CurTok != tok_colon) {
                cps::sourceError(Lex.getLine(), "Expected ':' in array range (e.g. 1:10)");
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
            cps::sourceError(Lex.getLine(), "Expected ',' or ']' in array dims");
            return nullptr;
        }
        getNextToken();
        
        if (CurTok != tok_of) {
            cps::sourceError(Lex.getLine(), "Expected OF after array dims");
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
        bool HasParens = CurTok == '(';
        std::vector<std::unique_ptr<ExprAST>> Args;
        if (HasParens) {
            getNextToken();
            auto Parsed = ParseExprList(')', true);
            if (!Parsed) return nullptr;
            Args = std::move(*Parsed);
        }
        return std::make_unique<DeclareStmtAST>(Name, TypeStr, std::move(Args), HasParens);
    }
}

std::unique_ptr<StmtAST> Parser::ParseConstant() {
    int Line = Lex.getLine();
    getNextToken();
    if (CurTok != tok_identifier) {
        cps::sourceError(Line, "Expected identifier after CONSTANT");
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();
    if (CurTok != tok_eq) {
        cps::sourceError(Line, "Expected '=' in CONSTANT declaration");
        return nullptr;
    }
    getNextToken();
    bool Negative = CurTok == '-';
    if (Negative) getNextToken();
    bool Numeric = CurTok == tok_number_int || CurTok == tok_number_real;
    bool Literal = Numeric || CurTok == tok_true || CurTok == tok_false ||
                   CurTok == tok_string_literal || CurTok == tok_c_string_literal || CurTok == tok_char_literal;
    if (!Literal || (Negative && !Numeric)) {
        cps::sourceError(Line, "CONSTANT initializer must be a literal");
        return nullptr;
    }
    std::unique_ptr<ExprAST> Value;
    if (CurTok == tok_number_int) {
        int Line = Lex.getLine();
        int64_t Integer;
        if (!ParseIntegerValue(Negative, Integer)) return nullptr;
        Value = std::make_unique<IntegerExprAST>(Integer);
        Value->setLine(Line);
    } else {
        Value = ParsePrimary();
        if (!Value) return nullptr;
        if (Negative) Value = std::make_unique<UnaryExprAST>('-', std::move(Value));
    }
    if (GetTokPrecedence() >= 0) {
        cps::sourceError(Line, "CONSTANT initializer must be a literal, not an expression");
        return nullptr;
    }
    return std::make_unique<ConstantStmtAST>(std::move(Name), std::move(Value));
}
