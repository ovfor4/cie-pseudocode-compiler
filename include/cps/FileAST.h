#pragma once
#include "cps/AST.h"
#include "cps/FileMode.h"
#include <memory>
#include <string>

namespace cps {

class OpenFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    FileMode Mode;

public:
    OpenFileStmtAST(std::unique_ptr<ExprAST> FileName, FileMode Mode, int Line)
        : StmtAST(Line), FileName(std::move(FileName)), Mode(Mode) {}

    ExprAST *getFileName() const { return FileName.get(); }
    FileMode getMode() const { return Mode; }
};

class ReadFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    std::string VarName;

public:
    ReadFileStmtAST(std::unique_ptr<ExprAST> FileName, const std::string &VarName, int Line)
        : StmtAST(Line), FileName(std::move(FileName)), VarName(VarName) {}

    ExprAST *getFileName() const { return FileName.get(); }
    const std::string &getVarName() const { return VarName; }
};

class WriteFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;
    std::unique_ptr<ExprAST> Data;

public:
    WriteFileStmtAST(std::unique_ptr<ExprAST> FileName, std::unique_ptr<ExprAST> Data, int Line)
        : StmtAST(Line), FileName(std::move(FileName)), Data(std::move(Data)) {}

    ExprAST *getFileName() const { return FileName.get(); }
    ExprAST *getData() const { return Data.get(); }
};

class CloseFileStmtAST : public StmtAST {
    std::unique_ptr<ExprAST> FileName;

public:
    CloseFileStmtAST(std::unique_ptr<ExprAST> FileName, int Line)
        : StmtAST(Line), FileName(std::move(FileName)) {}

    ExprAST *getFileName() const { return FileName.get(); }
};

} // namespace cps
