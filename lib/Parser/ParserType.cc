#include "cps/Parser.h"
#include "cps/TypeAST.h"
#include <cstdio>

using namespace cps;

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

    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: Expected type name after %s at line %d\n", Keyword, Lex.getLine());
        if (IsClass) ParseRecordBody("<unnamed>", Line, tok_endclass);
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    std::unique_ptr<StmtAST> Decl;

    if (!IsClass && CurTok == tok_eq) {
        getNextToken();

        if (CurTok == tok_c_type) {
            getNextToken();
            if (CurTok != '(') {
                fprintf(stderr, "error: Expected '(' after C_TYPE at line %d\n", Lex.getLine());
                return nullptr;
            }
            getNextToken();
            if (CurTok != tok_string_literal) {
                fprintf(stderr, "error: C_TYPE requires a string literal at line %d\n", Lex.getLine());
                return nullptr;
            }
            std::string Expression = Lex.StringVal;
            getNextToken();
            if (CurTok != ')') {
                fprintf(stderr, "error: Expected ')' after C_TYPE string at line %d\n", Lex.getLine());
                return nullptr;
            }
            getNextToken();
            Decl = std::make_unique<CTypeDeclAST>(Name, Expression);
        } else if (CurTok == '(') {
            getNextToken();
            std::vector<std::string> Values;
            while (true) {
                if (CurTok != tok_identifier) {
                    fprintf(stderr, "error: Expected an enum value name in TYPE %s at line %d\n",
                            Name.c_str(), Lex.getLine());
                    return nullptr;
                }
                Values.push_back(Lex.IdentifierStr);
                getNextToken();
                if (CurTok == ')') break;
                if (CurTok != ',') {
                    fprintf(stderr, "error: Expected ',' or ')' in TYPE %s at line %d\n",
                            Name.c_str(), Lex.getLine());
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
            fprintf(stderr, "error: SET OF types are not implemented at line %d\n", Lex.getLine());
            getNextToken();
            return nullptr;
        } else {
            fprintf(stderr, "error: Expected '(', '^' or C_TYPE after '=' in TYPE %s at line %d\n",
                    Name.c_str(), Lex.getLine());
            return nullptr;
        }
    } else {
        auto Fields = ParseRecordBody(Name, Line, IsClass ? tok_endclass : tok_endtype);
        if (!Fields) return nullptr;
        if (IsClass)
            Decl = std::make_unique<ClassTypeDeclAST>(Name, std::move(*Fields));
        else
            Decl = std::make_unique<RecordTypeDeclAST>(Name, std::move(*Fields));
    }

    if (!AtTopLevel) {
        fprintf(stderr, "error: %s declarations are only allowed at top level at line %d\n", Keyword, Line);
        return nullptr;
    }
    TypeNames.insert(Name);
    return Decl;
}

// Shared record body and field parsing. Fields are data, never statements.
std::optional<std::vector<RecordFieldDecl>> Parser::ParseRecordBody(
    const std::string &Name, int Line, int EndToken) {
    bool IsClass = EndToken == tok_endclass;
    const char *Keyword = IsClass ? "CLASS" : "TYPE";
    const char *EndKeyword = IsClass ? "ENDCLASS" : "ENDTYPE";
    auto AtField = [&] {
        return IsClass ? (CurTok == tok_public || CurTok == tok_private)
                       : CurTok == tok_declare;
    };
    std::vector<RecordFieldDecl> Fields;
    bool HadFieldError = false;
    while (CurTok != EndToken && CurTok != tok_eof) {
        if (AtField()) {
            // Record fields are always public; class fields have no default.
            bool IsPublic = !IsClass || CurTok == tok_public;
            if (ParseRecordField(Fields, IsPublic)) continue;
        } else {
            fprintf(stderr, "error: Expected %s or %s in %s %s at line %d\n",
                    IsClass ? "PUBLIC or PRIVATE" : "DECLARE", EndKeyword,
                    Keyword, Name.c_str(), Lex.getLine());
        }
        HadFieldError = true;
        // Consume invalid bodies rather than replaying their contents as
        // statements. Nested type declarations are consumed in full too.
        while (!AtField() && CurTok != EndToken && CurTok != tok_eof) {
            if (CurTok == tok_class || CurTok == tok_type)
                ParseTypeDecl();
            else
                getNextToken();
        }
    }
    if (CurTok != EndToken) {
        fprintf(stderr, "error: Expected %s in %s %s at line %d\n",
                EndKeyword, Keyword, Name.c_str(), Lex.getLine());
        return std::nullopt;
    }
    getNextToken(); // eat body terminator
    if (Fields.empty() && !HadFieldError) {
        fprintf(stderr, "error: %s %s has no fields at line %d\n", Keyword, Name.c_str(), Line);
        return std::nullopt;
    }
    if (HadFieldError) return std::nullopt;
    return Fields;
}

// <field introducer> <field> : <type>. Array fields are deferred.
bool Parser::ParseRecordField(std::vector<RecordFieldDecl> &Out, bool IsPublic) {
    const char *Introducer = CurTok == tok_declare ? "DECLARE"
                           : CurTok == tok_public ? "PUBLIC" : "PRIVATE";
    getNextToken(); // eat field introducer
    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: Expected field name after %s at line %d\n", Introducer, Lex.getLine());
        return false;
    }
    std::string FieldName = Lex.IdentifierStr;
    getNextToken();

    if (CurTok != tok_colon) {
        fprintf(stderr, "error: Expected ':' after field name %s at line %d\n",
                FieldName.c_str(), Lex.getLine());
        return false;
    }
    getNextToken();

    if (CurTok == tok_array) {
        fprintf(stderr, "error: Array fields inside records are not implemented at line %d\n",
                Lex.getLine());
        return false;
    }

    std::string TypeName = ParseTypeName(false);
    if (TypeName.empty()) return false;

    Out.push_back({FieldName, TypeName, IsPublic});
    return true;
}
