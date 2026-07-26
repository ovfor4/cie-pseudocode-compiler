#include "cps/Parser.h"
#include "cps/FileAST.h"
#include <cstdio>

using namespace cps;

// OPENFILE <expr> FOR READ|WRITE|APPEND
std::unique_ptr<StmtAST> Parser::ParseOpenFile() {
    int Line = Lex.getLine();
    getNextToken(); // eat OPENFILE

    auto FileName = ParseExpression();
    if (!FileName) return nullptr;

    if (CurTok != tok_for) {
        fprintf(stderr, "Error: Expected FOR after OPENFILE filename at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken(); // eat FOR

    FileMode Mode;
    switch (CurTok) {
    case tok_read:   Mode = FileMode::Read;   break;
    case tok_write:  Mode = FileMode::Write;  break;
    case tok_append: Mode = FileMode::Append; break;
    case tok_random:
        fprintf(stderr, "Error: File mode RANDOM is not implemented (text files only) at line %d\n",
                Lex.getLine());
        return nullptr;
    default:
        fprintf(stderr, "Error: Expected file mode READ, WRITE or APPEND at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken(); // eat the mode keyword

    return std::make_unique<OpenFileStmtAST>(std::move(FileName), Mode, Line);
}

// READFILE <expr>, <identifier>
std::unique_ptr<StmtAST> Parser::ParseReadFile() {
    int Line = Lex.getLine();
    getNextToken(); // eat READFILE

    auto FileName = ParseExpression();
    if (!FileName) return nullptr;

    if (CurTok != ',') {
        fprintf(stderr, "Error: Expected ',' after READFILE filename at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken(); // eat ','

    if (CurTok != tok_identifier) {
        fprintf(stderr, "Error: Expected variable name after ',' in READFILE at line %d\n", Lex.getLine());
        return nullptr;
    }
    std::string VarName = Lex.IdentifierStr;
    getNextToken(); // eat the variable name

    if (CurTok == '[' || CurTok == '.' || CurTok == '^') {
        // Consume the whole designator so it cannot replay as statements.
        std::vector<DesignatorAccess> Dummy;
        ParseDesignatorSuffix(Dummy);
        fprintf(stderr, "Error: READFILE target must be a plain STRING variable at line %d\n", Line);
        return nullptr;
    }

    return std::make_unique<ReadFileStmtAST>(std::move(FileName), VarName, Line);
}

// WRITEFILE <expr>, <expr>
std::unique_ptr<StmtAST> Parser::ParseWriteFile() {
    int Line = Lex.getLine();
    getNextToken(); // eat WRITEFILE

    auto FileName = ParseExpression();
    if (!FileName) return nullptr;

    if (CurTok != ',') {
        fprintf(stderr, "Error: Expected ',' after WRITEFILE filename at line %d\n", Lex.getLine());
        return nullptr;
    }
    getNextToken(); // eat ','

    auto Data = ParseExpression();
    if (!Data) return nullptr;

    return std::make_unique<WriteFileStmtAST>(std::move(FileName), std::move(Data), Line);
}

// CLOSEFILE <expr>
std::unique_ptr<StmtAST> Parser::ParseCloseFile() {
    int Line = Lex.getLine();
    getNextToken(); // eat CLOSEFILE

    auto FileName = ParseExpression();
    if (!FileName) return nullptr;

    return std::make_unique<CloseFileStmtAST>(std::move(FileName), Line);
}
