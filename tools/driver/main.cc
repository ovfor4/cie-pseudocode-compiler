#include "cps/Lexer.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <string>

int main(int argc, char **argv) {
    if (argc == 1) {
        // TODO: Print help here when help text is available.
        return 0;
    }

    std::string InputPath;
    std::string OutputPath;
    for (int I = 1; I < argc; ++I) {
        std::string Arg = argv[I];
        if (Arg == "-o") {
            if (!OutputPath.empty()) {
                llvm::errs() << "Error: -o may only be specified once\n";
                return 1;
            }
            if (I + 1 == argc || argv[I + 1][0] == '-' || argv[I + 1][0] == '\0') {
                llvm::errs() << "Error: -o requires an output path\n";
                return 1;
            }
            OutputPath = argv[++I];
        } else if (Arg.empty() || Arg[0] == '-') {
            llvm::errs() << "Error: unknown argument '" << Arg << "'\n";
            return 1;
        } else if (!InputPath.empty()) {
            llvm::errs() << "Error: only one source file is supported\n";
            return 1;
        } else {
            InputPath = Arg;
        }
    }

    if (InputPath.empty()) {
        llvm::errs() << "Error: no source file specified\n";
        return 1;
    }
    if (OutputPath.empty()) {
        llvm::SmallString<256> DefaultPath(InputPath);
        llvm::sys::path::replace_extension(DefaultPath, ".ir");
        OutputPath = std::string(DefaultPath);
    }

    std::ifstream Input(InputPath, std::ios::binary);
    if (!Input) {
        llvm::errs() << "Error: cannot open source file '" << InputPath << "'\n";
        return 1;
    }
    if (llvm::sys::fs::equivalent(InputPath, OutputPath)) {
        llvm::errs() << "Error: output file must not be the source file\n";
        return 1;
    }

    cps::Lexer Lex(Input);

    cps::Parser Parser(Lex);
    auto Statements = Parser.Parse();
    if (Input.bad()) {
        llvm::errs() << "Error: cannot read source file '" << InputPath << "'\n";
        return 1;
    }
    if (Lex.hadError() || Parser.hadError()) return 1;

    cps::CodeGen CG;
    CG.compile(Statements);

    if (CG.hadError()) return 1;

    std::error_code EC;
    llvm::raw_fd_ostream Output(OutputPath, EC);
    if (EC) {
        llvm::errs() << "Error: cannot open output file '" << OutputPath
                     << "': " << EC.message() << "\n";
        return 1;
    }
    CG.print(Output);
    Output.close();
    if (Output.has_error()) {
        llvm::errs() << "Error: cannot write output file '" << OutputPath
                     << "': " << Output.error().message() << "\n";
        Output.clear_error();
        return 1;
    }
    return 0;
}
