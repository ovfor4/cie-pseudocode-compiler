#include "cps/Lexer.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"

int main() {
    cps::Lexer Lex;

    cps::Parser Parser(Lex);
    auto Statements = Parser.Parse();

    cps::CodeGen CG;
    CG.compile(Statements);

    CG.print();

    return (Lex.hadError() || Parser.hadError() || CG.hadError()) ? 1 : 0;
}
