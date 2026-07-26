#pragma once
#include "cps/AST.h"
#include "cps/FileMode.h"
#include <memory>
#include <string>

namespace cps {

class OpenFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    FileMode Mode;
    int Line;

public:
    OpenFileStmtAST(std::unique_ptr<ExprAST> FileName, FileMode Mode, int Line)
        : FileName(std::move(FileName)), Mode(Mode), Line(Line) {}

    ExprAST *getFileName() const { return FileName.get(); }
    FileMode getMode() const { return Mode; }
    int getLine() const { return Line; }
};

class ReadFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    std::string VarName;
    int Line;

public:
    ReadFileStmtAST(std::unique_ptr<ExprAST> FileName, const std::string &VarName, int Line)
        : FileName(std::move(FileName)), VarName(VarName), Line(Line) {}

    ExprAST *getFileName() const { return FileName.get(); }
    const std::string &getVarName() const { return VarName; }
    int getLine() const { return Line; }
};

class WriteFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    std::unique_ptr<ExprAST> Data;
    int Line;

public:
    WriteFileStmtAST(std::unique_ptr<ExprAST> FileName, std::unique_ptr<ExprAST> Data, int Line)
        : FileName(std::move(FileName)), Data(std::move(Data)), Line(Line) {}

    ExprAST *getFileName() const { return FileName.get(); }
    ExprAST *getData() const { return Data.get(); }
    int getLine() const { return Line; }
};

class CloseFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    int Line;

public:
    CloseFileStmtAST(std::unique_ptr<ExprAST> FileName, int Line)
        : FileName(std::move(FileName)), Line(Line) {}

    ExprAST *getFileName() const { return FileName.get(); }
    int getLine() const { return Line; }
};

} // namespace cps
