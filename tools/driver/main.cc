#include "cps/Lexer.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"
#ifdef CPSC_RUN_EMBEDDED
#include "EmbeddedRunner.h"
#endif
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#ifdef CPSC_RUN_PATH
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Regex.h"
#endif
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <string>

namespace {

int compilationTerminated() {
    llvm::errs() << "compilation terminated.\n";
    return 1;
}

#ifdef CPSC_RUN_PATH
bool checkLliVersion(llvm::StringRef LliPath) {
    llvm::SmallString<256> VersionPath;
    if (auto EC = llvm::sys::fs::createTemporaryFile("cpsc-lli-version", "txt", VersionPath)) {
        llvm::errs() << "cpsc: fatal error: cannot check lli version: " << EC.message() << "\n";
        return false;
    }
    llvm::FileRemover TemporaryVersion(VersionPath);
    std::string ExecutionError;
    int Result = llvm::sys::ExecuteAndWait(LliPath, {LliPath, "--version"},
        std::nullopt, {llvm::StringRef(""), llvm::StringRef(VersionPath), llvm::StringRef("")},
        5, 0, &ExecutionError);
    if (Result != 0) {
        llvm::errs() << "cpsc: fatal error: cannot check lli version at '" << LliPath
                     << "' (status " << Result << ")";
        if (!ExecutionError.empty()) llvm::errs() << ": " << ExecutionError;
        llvm::errs() << "\n";
        return false;
    }
    auto Version = llvm::MemoryBuffer::getFile(VersionPath);
    if (!Version) {
        llvm::errs() << "cpsc: fatal error: cannot read lli version: "
                     << Version.getError().message() << "\n";
        return false;
    }
    llvm::SmallVector<llvm::StringRef, 2> Matches;
    if (!llvm::Regex("LLVM version ([0-9]+)\\.").match((*Version)->getBuffer(), &Matches)) {
        llvm::errs() << "cpsc: fatal error: cannot identify LLVM version of lli at '"
                     << LliPath << "'; LLVM 23 is required\n";
        return false;
    }
    if (Matches[1] != "23") {
        llvm::errs() << "cpsc: fatal error: -r requires lli from LLVM 23; found LLVM "
                     << Matches[1] << " at '" << LliPath << "'\n";
        return false;
    }
    return true;
}
#endif

} // namespace

int main(int argc, char **argv) {
    for (int I = 1; I < argc; ++I) {
        if (std::string(argv[I]) == "-h") {
            llvm::outs() << "CIE Pseudocode Compiler is licensed under AGPL 3.0 or later. This is free software; see the source for copying conditions. There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
            llvm::outs() << "The source is available here: github.com/ovfor4/cie-pseudocode-compiler\n";
            llvm::outs() << "Please leave a star on GitHub if you find it helpful, so that the project can be more popular\n";
            llvm::outs() << "\n";
            llvm::outs() << "\n";

#ifdef _WIN32
            llvm::outs() << "See \'Start Menu > cpsc > cpsc Help\' for HTML help.\n";
#else
            llvm::outs() << "See \'man 1 cpsc\' for help.\n";
#endif
            return 0;
        }
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
                llvm::errs() << "cpsc: error: -o may only be specified once\n";
                return compilationTerminated();
            }
            if (I + 1 == argc || argv[I + 1][0] == '-' || argv[I + 1][0] == '\0') {
                llvm::errs() << "cpsc: error: -o requires an output path\n";
                return compilationTerminated();
            }
            OutputPath = argv[++I];
        } else if (Arg.empty() || Arg[0] == '-') {
            llvm::errs() << "cpsc: error: unknown argument '" << Arg << "'\n";
            return compilationTerminated();
        } else if (!InputPath.empty()) {
            llvm::errs() << "cpsc: error: only one source file is supported\n";
            return compilationTerminated();
        } else {
            InputPath = Arg;
        }
    }

    if (InputPath.empty()) {
        llvm::errs() << "cpsc: fatal error: no input files\n";
        return compilationTerminated();
    }
#ifdef CPSC_RUN_PATH
    std::string LliPath;
#endif
    if (Run) {
#ifdef CPSC_RUN_PATH
        auto Lli = llvm::sys::findProgramByName("lli");
        if (!Lli) {
            llvm::errs() << "cpsc: fatal error: -r requires lli from LLVM 23 in PATH: " << Lli.getError().message() << "\n";
            return compilationTerminated();
        }
        LliPath = *Lli;
        if (!checkLliVersion(LliPath)) return compilationTerminated();
#elif defined(CPSC_RUN_NONE)
        llvm::errs() << "cpsc: fatal error: -r is disabled in this build (CPSC_RUN_MODE=NONE)\n";
        return compilationTerminated();
#endif
    }
    if (OutputPath.empty() && !Run) {
        llvm::SmallString<256> DefaultPath(InputPath);
        llvm::sys::path::replace_extension(DefaultPath, ".ir");
        OutputPath = std::string(DefaultPath);
    }

    std::ifstream Input(InputPath, std::ios::binary);
    if (!Input) {
        llvm::errs() << "cpsc: fatal error: cannot open source file '" << InputPath << "'\n";
        return compilationTerminated();
    }
    if (llvm::sys::fs::equivalent(InputPath, OutputPath)) {
        llvm::errs() << "cpsc: error: output file must not be the source file\n";
        return compilationTerminated();
    }

    cps::Lexer Lex(Input);

    cps::Parser Parser(Lex);
    auto Statements = Parser.Parse();
    if (Input.bad()) {
        llvm::errs() << "cpsc: fatal error: cannot read source file '" << InputPath << "'\n";
        return compilationTerminated();
    }
    if (Lex.hadError() || Parser.hadError()) return compilationTerminated();

    cps::CodeGen CG;
    CG.compile(Statements);

    if (CG.hadError()) return compilationTerminated();

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
        llvm::errs() << "cpsc: fatal error: cannot open output file '" << OutputPath
                     << "': " << EC.message() << "\n";
        return compilationTerminated();
    }
    llvm::raw_fd_ostream Output(OutputFD, true);
    CG.print(Output);
    Output.close();
    if (Output.has_error()) {
        llvm::errs() << "cpsc: fatal error: cannot write output file '" << OutputPath
                     << "': " << Output.error().message() << "\n";
        Output.clear_error();
        return compilationTerminated();
    }
    if (!Run) return 0;

#ifdef CPSC_RUN_PATH
    llvm::SmallString<256> RunPath(OutputPath);
    if (auto PathError = llvm::sys::fs::make_absolute(RunPath)) {
        llvm::errs() << "cpsc: error: cannot resolve IR path: " << PathError.message() << "\n";
        return 1;
    }
    std::string ExecutionError;
    int Result = llvm::sys::ExecuteAndWait(LliPath, {LliPath, RunPath},
                                         std::nullopt, {}, 0, 0, &ExecutionError);
    if (Result < 0) {
        llvm::errs() << "cpsc: error: lli execution failed (status " << Result << "): "
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
