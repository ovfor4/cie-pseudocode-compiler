#include "cps/Lexer.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"
#ifdef CPSC_RUN_EMBEDDED
#include "EmbeddedRunner.h"
#endif
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
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
    bool Run = false;
    for (int I = 1; I < argc; ++I) {
        std::string Arg = argv[I];
        if (Arg == "-r") {
            Run = true;
        } else if (Arg == "-o") {
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
#ifdef CPSC_RUN_PATH
    std::string LliPath;
#endif
    if (Run) {
#ifdef CPSC_RUN_PATH
        auto Lli = llvm::sys::findProgramByName("lli");
        if (!Lli) {
            llvm::errs() << "Error: -r requires lli in PATH: " << Lli.getError().message() << "\n";
            return 1;
        }
        LliPath = *Lli;
#elif defined(CPSC_RUN_NONE)
        llvm::errs() << "Error: -r is disabled in this build (CPSC_RUN_MODE=NONE)\n";
        return 1;
#endif
    }
    if (OutputPath.empty() && !Run) {
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

#ifdef CPSC_RUN_EMBEDDED
    if (Run && OutputPath.empty()) return runEmbedded(CG);
#endif
    std::error_code EC;
    int OutputFD;
    llvm::FileRemover TemporaryOutput;
    if (OutputPath.empty()) {
        llvm::SmallString<256> TemporaryPath;
        EC = llvm::sys::fs::createTemporaryFile("cpsc", "ir", OutputFD, TemporaryPath);
        if (!EC) {
            OutputPath = std::string(TemporaryPath);
            TemporaryOutput.setFile(OutputPath);
        }
    } else {
        EC = llvm::sys::fs::openFileForWrite(OutputPath, OutputFD);
    }
    if (EC) {
        llvm::errs() << "Error: cannot open output file '" << OutputPath
                     << "': " << EC.message() << "\n";
        return 1;
    }
    llvm::raw_fd_ostream Output(OutputFD, true);
    CG.print(Output);
    Output.close();
    if (Output.has_error()) {
        llvm::errs() << "Error: cannot write output file '" << OutputPath
                     << "': " << Output.error().message() << "\n";
        Output.clear_error();
        return 1;
    }
    if (!Run) return 0;

#ifdef CPSC_RUN_PATH
    llvm::SmallString<256> RunPath(OutputPath);
    if (auto PathError = llvm::sys::fs::make_absolute(RunPath)) {
        llvm::errs() << "Error: cannot resolve IR path: " << PathError.message() << "\n";
        return 1;
    }
    std::string ExecutionError;
    int Result = llvm::sys::ExecuteAndWait(LliPath, {LliPath, RunPath},
                                         std::nullopt, {}, 0, 0, &ExecutionError);
    if (Result < 0) {
        llvm::errs() << "Error: lli execution failed (status " << Result << "): "
                     << ExecutionError << "\n";
        return 1;
    }
    return Result;
#elif defined(CPSC_RUN_EMBEDDED)
    return runEmbedded(CG);
#else
    return 0;
#endif
}
