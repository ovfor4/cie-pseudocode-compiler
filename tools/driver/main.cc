#include "cps/Lexer.h"
#include "cps/Preprocessor.h"
#include "cps/Diagnostics.h"
#include "cps/Parser.h"
#include "cps/CodeGen.h"
#include "NativeCompiler.h"
#include "StandardLibrary.h"
#ifdef CPSC_RUN_EMBEDDED
#include "EmbeddedRunner.h"
#endif
#ifdef CPSC_RUN_PATH
#include "PathRunner.h"
#endif
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <sstream>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#endif

namespace {

int compilationTerminated() {
    llvm::errs() << "compilation terminated.\n";
    return 1;
}

bool isColorOption(const std::string &Arg) {
    return Arg == "-fdiagnostics-color" || Arg == "-fno-diagnostics-color" ||
           Arg.rfind("-fdiagnostics-color=", 0) == 0;
}

// gcc's "auto": color only a terminal stderr. On Windows the console must
// also accept ANSI escape sequences.
bool colorTerminal() {
#ifdef _WIN32
    HANDLE Error = GetStdHandle(STD_ERROR_HANDLE);
    DWORD Mode;
    return GetConsoleMode(Error, &Mode) &&
           SetConsoleMode(Error, Mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    const char *Term = std::getenv("TERM");
    return Term && std::strcmp(Term, "dumb") != 0 && isatty(STDERR_FILENO);
#endif
}

// INCLUDE and INCLUDE_OBJ paths are absolute or relative to the including
// unit's directory.
llvm::SmallString<256> relativeToUnit(llvm::StringRef UnitPath, llvm::StringRef Path) {
    llvm::SmallString<256> Result(Path);
    if (!llvm::sys::path::is_absolute(Result)) {
        Result = llvm::sys::path::parent_path(UnitPath);
        llvm::sys::path::append(Result, Path);
    }
    return Result;
}

// -h anywhere prints a documentation pointer instead of compiling.
bool printHelp(int argc, char **argv) {
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
            return true;
        }
    }
    return false;
}

// Like gcc, the last valid -fdiagnostics-color option wins, and it is
// applied before any other diagnostic. Returns false after reporting an
// invalid color argument.
bool applyColorOptions(int argc, char **argv) {
    enum { ColorAuto, ColorAlways, ColorNever } ColorMode = ColorAuto;
    std::string BadColor;
    for (int I = 1; I < argc; ++I) {
        std::string Arg = argv[I];
        if (Arg == "-fdiagnostics-color" || Arg == "-fdiagnostics-color=always")
            ColorMode = ColorAlways;
        else if (Arg == "-fdiagnostics-color=auto")
            ColorMode = ColorAuto;
        else if (Arg == "-fdiagnostics-color=never" || Arg == "-fno-diagnostics-color")
            ColorMode = ColorNever;
        else if (isColorOption(Arg) && BadColor.empty())
            BadColor = Arg;
    }
    // colorTerminal() also enables escape sequences on a Windows console.
    const bool Terminal = ColorMode != ColorNever && colorTerminal();
    cps::DiagnosticColors = ColorMode == ColorAlways || Terminal;
    if (!BadColor.empty()) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error)
                     << "unrecognized argument in option '" << BadColor << "'\n"
                     << cps::diagnosticPrefix(cps::Severity::Note)
                     << "valid arguments to '-fdiagnostics-color=' are: always auto never\n";
        return false;
    }
    return true;
}

struct Options {
    std::vector<std::string> InputPaths;
    std::string OutputPath;
    bool Run = false;
    bool EmitIR = false;
    bool NoStd = false;
};

// Returns false after reporting an argument error or a missing input.
bool parseArguments(int argc, char **argv, Options &Opts) {
    for (int I = 1; I < argc; ++I) {
        std::string Arg = argv[I];
        if (Arg == "-r") {
            Opts.Run = true;
        } else if (Arg == "-S") {
            Opts.EmitIR = true;
        } else if (Arg == "-no-std") {
            Opts.NoStd = true;
        } else if (Arg == "-o") {
            if (!Opts.OutputPath.empty()) {
                llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "-o may only be specified once\n";
                return false;
            }
            if (I + 1 == argc || argv[I + 1][0] == '-' || argv[I + 1][0] == '\0') {
                llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "-o requires an output path\n";
                return false;
            }
            Opts.OutputPath = argv[++I];
        } else if (isColorOption(Arg)) {
            // Already applied above; not an unknown argument.
        } else if (Arg.empty() || Arg[0] == '-') {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "unknown argument '" << Arg << "'\n";
            return false;
        } else {
            Opts.InputPaths.push_back(Arg);
        }
    }

    if (Opts.InputPaths.empty()) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "no input files\n";
        return false;
    }
    return true;
}

// Parses each input independently into Programs[I] and its unit. Every unit
// sees itself; user inputs also see the embedded std.cps.
bool parseUnits(const std::vector<std::string> &InputPaths, size_t UserInputCount, bool NoStd,
                std::vector<std::vector<std::unique_ptr<cps::StmtAST>>> &Programs,
                std::vector<cps::SourceUnit> &Units) {
    for (size_t I = 0; I < InputPaths.size(); ++I) {
        const auto &InputPath = InputPaths[I];
        for (size_t J = 0; J < I; ++J) {
            if (llvm::sys::fs::equivalent(InputPath, InputPaths[J])) {
                llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "duplicate input file '" << InputPath << "'\n";
                return false;
            }
        }
        std::ifstream Input(InputPath, std::ios::binary);
        if (!Input) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot open source file '" << InputPath << "'\n";
            return false;
        }
        cps::DiagnosticSource = InputPath;
        auto Source = cps::preprocess(Input);
        if (Input.bad()) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot read source file '" << InputPath << "'\n";
            return false;
        }
        if (Source.HadError) return false;
        std::istringstream Processed(Source.Text);
        cps::Lexer Lex(Processed);
        cps::Parser Parser(Lex);
        Programs[I] = Parser.Parse();
        if (Lex.hadError() || Parser.hadError()) return false;
        Units.push_back({InputPath, Programs[I], std::vector<bool>(InputPaths.size(), false), std::move(Source.LibraryLines)});
        Units.back().Visible[I] = true;
        if (!NoStd && I < UserInputCount)
            for (size_t J = UserInputCount; J < InputPaths.size(); ++J)
                if (llvm::sys::path::filename(InputPaths[J]) == "std.cps")
                    Units.back().Visible[J] = true;
    }
    return true;
}

// Each INCLUDE makes the named command-line unit visible to the includer.
bool resolveIncludes(std::vector<cps::SourceUnit> &Units) {
    for (size_t I = 0; I < Units.size(); ++I) {
        cps::DiagnosticSource = Units[I].Path;
        for (const auto &Stmt : Units[I].Statements) {
            auto *Include = dynamic_cast<cps::IncludeStmtAST*>(Stmt.get());
            if (!Include) continue;
            llvm::SmallString<256> Path = relativeToUnit(Units[I].Path, Include->getPath());
            bool Found = false;
            for (size_t J = 0; J < Units.size(); ++J) {
                if (llvm::sys::fs::equivalent(Path, Units[J].Path)) {
                    Units[I].Visible[J] = true;
                    Found = true;
                }
            }
            if (!Found) {
                cps::sourceError(Stmt->getLine(),
                    "cannot INCLUDE '%s': not given on the command line",
                    Include->getPath().c_str());
                return false;
            }
        }
    }
    return true;
}

// Visibility is transitive.
void closeVisibility(std::vector<cps::SourceUnit> &Units) {
    for (size_t K = 0; K < Units.size(); ++K)
        for (size_t I = 0; I < Units.size(); ++I)
            for (size_t J = 0; J < Units.size(); ++J)
                Units[I].Visible[J] = Units[I].Visible[J] ||
                    (Units[I].Visible[K] && Units[K].Visible[J]);
}

// INCLUDE_OBJ is global: every input's objects join one link, and each
// EXTERN symbol must be defined by one of them ("__libc" is built in).
bool loadObjects(const std::vector<cps::SourceUnit> &Units, std::vector<std::string> &Objects,
                 llvm::StringSet<> &ExternSymbols) {
    bool LibcLoaded = false;
    for (size_t I = 0; I < Units.size(); ++I) {
        cps::DiagnosticSource = Units[I].Path;
        for (const auto &Stmt : Units[I].Statements) {
            auto *Object = dynamic_cast<cps::IncludeObjStmtAST*>(Stmt.get());
            if (!Object) continue;
            if (Object->getPath() == "__libc") {
                if (!LibcLoaded && !collectLibcSymbols(ExternSymbols)) return false;
                LibcLoaded = true;
                continue;
            }
            llvm::SmallString<256> Path = relativeToUnit(Units[I].Path, Object->getPath());
            if (llvm::sys::path::extension(Path) != ".o") {
                cps::sourceError(Stmt->getLine(),
                    "INCLUDE_OBJ '%s' must name a .o object file or \"__libc\"",
                    Object->getPath().c_str());
                return false;
            }
            bool Seen = false;
            for (const auto &Existing : Objects)
                Seen = Seen || llvm::sys::fs::equivalent(Path, Existing);
            if (Seen) continue;
            std::string Error;
            if (!collectObjectSymbols(Path, ExternSymbols, Error)) {
                cps::sourceError(Stmt->getLine(), "cannot INCLUDE_OBJ '%s': %s",
                    Object->getPath().c_str(), Error.c_str());
                return false;
            }
            Objects.push_back(std::string(Path));
        }
    }
    return true;
}

// Reports every EXTERN whose symbol no INCLUDE_OBJ defines.
bool checkExterns(const std::vector<cps::SourceUnit> &Units, const llvm::StringSet<> &ExternSymbols) {
    bool MissingSymbol = false;
    for (size_t I = 0; I < Units.size(); ++I) {
        cps::DiagnosticSource = Units[I].Path;
        for (const auto &Stmt : Units[I].Statements) {
            auto *Extern = dynamic_cast<cps::ExternDeclAST*>(Stmt.get());
            if (!Extern || ExternSymbols.contains(Extern->getProto()->getName())) continue;
            cps::sourceError(Stmt->getLine(), "C symbol '%s' not found in any INCLUDE_OBJ",
                Extern->getProto()->getName().c_str());
            MissingSymbol = true;
        }
    }
    return !MissingSymbol;
}

// Without -o, the entry source's extension becomes .ll or the executable's.
std::string defaultOutputPath(llvm::StringRef EntryPath, bool EmitIR) {
    llvm::SmallString<256> DefaultPath(EntryPath);
#ifdef _WIN32
    const char *ExecutableExtension = ".exe";
#else
    const char *ExecutableExtension = ".out";
#endif
    llvm::sys::path::replace_extension(DefaultPath, EmitIR ? ".ll" : ExecutableExtension);
    return std::string(DefaultPath);
}

// Writes textual IR to IRPath, or to a new temporary file owned by
// TemporaryOutput when IRPath is empty. Returns false after reporting a
// fatal error.
bool writeIR(cps::CodeGen &CG, std::string &IRPath, llvm::FileRemover &TemporaryOutput) {
    std::error_code EC;
    int OutputFD;
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
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot open output file '" << IRPath
                     << "': " << EC.message() << "\n";
        return false;
    }
    llvm::raw_fd_ostream Output(OutputFD, true);
    CG.print(Output);
    Output.close();
    if (Output.has_error()) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot write output file '" << IRPath
                     << "': " << Output.error().message() << "\n";
        Output.clear_error();
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    if (printHelp(argc, argv)) return 0;
    if (!applyColorOptions(argc, argv)) return compilationTerminated();

    Options Opts;
    if (!parseArguments(argc, argv, Opts)) return compilationTerminated();
    auto &InputPaths = Opts.InputPaths;
#ifdef CPSC_RUN_PATH
    std::string LliPath;
#endif
    if (Opts.Run) {
#ifdef CPSC_RUN_PATH
        if (!findLli(LliPath)) return compilationTerminated();
#elif defined(CPSC_RUN_NONE)
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "-r is disabled in this build (CPSC_RUN_MODE=NONE)\n";
        return compilationTerminated();
#endif
    }
    const bool Save = !Opts.Run || Opts.EmitIR || !Opts.OutputPath.empty();
    const size_t UserInputCount = InputPaths.size();
    StandardLibrary StdLibrary;
    if (!Opts.NoStd && !StdLibrary.load(InputPaths)) return compilationTerminated();
    std::vector<std::vector<std::unique_ptr<cps::StmtAST>>> Programs(InputPaths.size());
    std::vector<cps::SourceUnit> Units;
    if (!parseUnits(InputPaths, UserInputCount, Opts.NoStd, Programs, Units)) return compilationTerminated();
    if (!resolveIncludes(Units)) return compilationTerminated();
    closeVisibility(Units);

    std::vector<std::string> Objects;
    llvm::StringSet<> ExternSymbols;
    if (!loadObjects(Units, Objects, ExternSymbols)) return compilationTerminated();
    if (!checkExterns(Units, ExternSymbols)) return compilationTerminated();

    cps::CodeGen CG;
    CG.compile(Units);
    if (CG.hadError()) return compilationTerminated();

    if (Opts.OutputPath.empty() && Save)
        Opts.OutputPath = defaultOutputPath(InputPaths[CG.getMainUnit()], Opts.EmitIR);
    for (const auto &InputPath : InputPaths) {
        if (Save && llvm::sys::fs::equivalent(InputPath, Opts.OutputPath)) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "output file must not be the source file\n";
            return compilationTerminated();
        }
    }

    if (Save && !Opts.EmitIR) {
        if (!compileNative(CG, Opts.OutputPath, argv[0], Objects)) return compilationTerminated();
        if (!Opts.Run) return 0;
    }
    std::string IRPath = Save && Opts.EmitIR ? Opts.OutputPath : std::string();
#ifdef CPSC_RUN_EMBEDDED
    if (Opts.Run && IRPath.empty()) return runEmbedded(CG, Objects);
#endif
    llvm::FileRemover TemporaryOutput;
    if (!writeIR(CG, IRPath, TemporaryOutput)) return compilationTerminated();
    if (!Opts.Run) return 0;

#ifdef CPSC_RUN_PATH
    return runLli(LliPath, IRPath, Objects);
#elif defined(CPSC_RUN_EMBEDDED)
    return runEmbedded(CG, Objects);
#else
    return 0;
#endif
}
