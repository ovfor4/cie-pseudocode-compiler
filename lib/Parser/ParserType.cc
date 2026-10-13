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
// TYPE is a top-level-only statement; a nested declaration is still consumed
// in full so its body cannot be replayed as statements.
std::unique_ptr<StmtAST> Parser::ParseTypeDecl() {
    bool AtTopLevel = BlockDepth <= 1;
    int Line = Lex.getLine();
    getNextToken(); // eat TYPE

    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: Expected type name after TYPE at line %d\n", Lex.getLine());
        return nullptr;
    }
    std::string Name = Lex.IdentifierStr;
    getNextToken();

    std::unique_ptr<StmtAST> Decl;

    if (CurTok == tok_eq) {
        getNextToken();

        if (CurTok == '(') {
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
            fprintf(stderr, "error: Expected '(' or '^' after '=' in TYPE %s at line %d\n",
                    Name.c_str(), Lex.getLine());
            return nullptr;
        }
    } else {
        auto Fields = ParseRecordBody(Name, Line, tok_endtype);
        if (!Fields) return nullptr;
        Decl = std::make_unique<RecordTypeDeclAST>(Name, std::move(*Fields));
    }

    if (!AtTopLevel) {
        fprintf(stderr, "error: TYPE declarations are only allowed at top level at line %d\n", Line);
        return nullptr;
    }
    return Decl;
}

// Shared record body and field parsing. Fields are data, never statements.
std::optional<std::vector<RecordFieldDecl>> Parser::ParseRecordBody(
    const std::string &Name, int Line, int EndToken) {
    std::vector<RecordFieldDecl> Fields;
    bool HadFieldError = false;
    while (CurTok == tok_declare) {
        if (!ParseRecordField(Fields, true)) {
            HadFieldError = true;
            while (CurTok != tok_declare && CurTok != EndToken && CurTok != tok_eof)
                getNextToken();
        }
    }
    if (CurTok != EndToken) {
        fprintf(stderr, "error: Expected DECLARE or ENDTYPE in TYPE %s at line %d\n",
                Name.c_str(), Lex.getLine());
        return std::nullopt;
    }
    getNextToken(); // eat body terminator
    if (Fields.empty()) {
        fprintf(stderr, "error: Record TYPE %s has no fields at line %d\n", Name.c_str(), Line);
        return std::nullopt;
    }
    if (HadFieldError) return std::nullopt;
    return Fields;
}

// <field introducer> <field> : <type>. Array fields are deferred.
bool Parser::ParseRecordField(std::vector<RecordFieldDecl> &Out, bool IsPublic) {
    getNextToken(); // eat field introducer
    if (CurTok != tok_identifier) {
        fprintf(stderr, "error: Expected field name after DECLARE at line %d\n", Lex.getLine());
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
