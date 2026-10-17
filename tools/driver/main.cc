#include "cps/Lexer.h"
#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"
#include "NativeCompiler.h"
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

    std::vector<std::string> InputPaths;
    std::string OutputPath;
    bool Run = false;
    bool EmitIR = false;
    for (int I = 1; I < argc; ++I) {
        std::string Arg = argv[I];
        if (Arg == "-r") {
            Run = true;
        } else if (Arg == "-S") {
            EmitIR = true;
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
        } else {
            InputPaths.push_back(Arg);
        }
    }

    if (InputPaths.empty()) {
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
    const bool Save = !Run || EmitIR || !OutputPath.empty();
    std::vector<std::vector<std::unique_ptr<cps::StmtAST>>> Programs(InputPaths.size());
    std::vector<cps::SourceUnit> Units;
    for (size_t I = 0; I < InputPaths.size(); ++I) {
        const auto &InputPath = InputPaths[I];
        for (size_t J = 0; J < I; ++J) {
            if (llvm::sys::fs::equivalent(InputPath, InputPaths[J])) {
                llvm::errs() << "cpsc: error: duplicate input file '" << InputPath << "'\n";
                return compilationTerminated();
            }
        }
        std::ifstream Input(InputPath, std::ios::binary);
        if (!Input) {
            llvm::errs() << "cpsc: fatal error: cannot open source file '" << InputPath << "'\n";
            return compilationTerminated();
        }
        cps::DiagnosticSource = InputPaths.size() > 1 ? InputPath : "";
        cps::Lexer Lex(Input);
        cps::Parser Parser(Lex);
        Programs[I] = Parser.Parse();
        if (Input.bad()) {
            llvm::errs() << "cpsc: fatal error: cannot read source file '" << InputPath << "'\n";
            return compilationTerminated();
        }
        if (Lex.hadError() || Parser.hadError()) return compilationTerminated();
        Units.push_back({InputPath, Programs[I], std::vector<bool>(InputPaths.size(), false)});
        Units.back().Visible[I] = true;
    }
    for (size_t I = 0; I < Units.size(); ++I) {
        cps::DiagnosticSource = Units.size() > 1 ? Units[I].Path : "";
        for (const auto &Stmt : Units[I].Statements) {
            auto *Include = dynamic_cast<cps::IncludeStmtAST*>(Stmt.get());
            if (!Include) continue;
            llvm::SmallString<256> Path(Include->getPath());
            if (!llvm::sys::path::is_absolute(Path)) {
                Path = llvm::sys::path::parent_path(Units[I].Path);
                llvm::sys::path::append(Path, Include->getPath());
            }
            bool Found = false;
            for (size_t J = 0; J < Units.size(); ++J) {
                if (llvm::sys::fs::equivalent(Path, Units[J].Path)) {
                    Units[I].Visible[J] = true;
                    Found = true;
                }
            }
            if (!Found) {
                cps::sourceDiagnostic(Stmt->getLine(),
                    "error: cannot INCLUDE '%s': not given on the command line at line %d\n",
                    Include->getPath().c_str(), Stmt->getLine());
                return compilationTerminated();
            }
        }
    }
    for (size_t K = 0; K < Units.size(); ++K)
        for (size_t I = 0; I < Units.size(); ++I)
            for (size_t J = 0; J < Units.size(); ++J)
                Units[I].Visible[J] = Units[I].Visible[J] ||
                    (Units[I].Visible[K] && Units[K].Visible[J]);

    cps::CodeGen CG;
    CG.compile(Units);
    if (CG.hadError()) return compilationTerminated();

    if (OutputPath.empty() && Save) {
        llvm::SmallString<256> DefaultPath(InputPaths[CG.getMainUnit()]);
        llvm::sys::path::replace_extension(DefaultPath, EmitIR ? ".ll" : ".out");
        OutputPath = std::string(DefaultPath);
    }
    for (const auto &InputPath : InputPaths) {
        if (Save && llvm::sys::fs::equivalent(InputPath, OutputPath)) {
            llvm::errs() << "cpsc: error: output file must not be the source file\n";
            return compilationTerminated();
        }
    }

    if (Save && !EmitIR) {
        if (!compileNative(CG, OutputPath, argv[0])) return compilationTerminated();
        if (!Run) return 0;
    }
    std::string IRPath = Save && EmitIR ? OutputPath : std::string();
#ifdef CPSC_RUN_EMBEDDED
    if (Run && IRPath.empty()) return runEmbedded(CG);
#endif
    std::error_code EC;
    int OutputFD;
    llvm::FileRemover TemporaryOutput;
    if (IRPath.empty()) {
        llvm::SmallString<256> TemporaryPath;
        EC = llvm::sys::fs::createTemporaryFile("cpsc", "ll", OutputFD, TemporaryPath);
        if (!EC) {
            IRPath = std::string(TemporaryPath);
            TemporaryOutput.setFile(IRPath);
        }
    } else {
        EC = llvm::sys::fs::openFileForWrite(IRPath, OutputFD);
    }
    if (EC) {
        llvm::errs() << "cpsc: fatal error: cannot open output file '" << IRPath
                     << "': " << EC.message() << "\n";
        return compilationTerminated();
    }
    llvm::raw_fd_ostream Output(OutputFD, true);
    CG.print(Output);
    Output.close();
    if (Output.has_error()) {
        llvm::errs() << "cpsc: fatal error: cannot write output file '" << IRPath
                     << "': " << Output.error().message() << "\n";
        Output.clear_error();
        return compilationTerminated();
    }
    if (!Run) return 0;

#ifdef CPSC_RUN_PATH
    llvm::SmallString<256> RunPath(IRPath);
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
