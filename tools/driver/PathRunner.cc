#include "PathRunner.h"
#include "cps/Diagnostics.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Regex.h"
#include "llvm/Support/raw_ostream.h"

namespace {

bool checkLliVersion(llvm::StringRef LliPath) {
    llvm::SmallString<256> VersionPath;
    if (auto EC = llvm::sys::fs::createTemporaryFile("cpsc-lli-version", "txt", VersionPath)) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot check lli version: " << EC.message() << "\n";
        return false;
    }
    llvm::FileRemover TemporaryVersion(VersionPath);
    std::string ExecutionError;
    int Result = llvm::sys::ExecuteAndWait(LliPath, {LliPath, "--version"},
        std::nullopt, {llvm::StringRef(""), llvm::StringRef(VersionPath), llvm::StringRef("")},
        5, 0, &ExecutionError);
    if (Result != 0) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot check lli version at '" << LliPath
                     << "' (status " << Result << ")";
        if (!ExecutionError.empty()) llvm::errs() << ": " << ExecutionError;
        llvm::errs() << "\n";
        return false;
    }
    auto Version = llvm::MemoryBuffer::getFile(VersionPath);
    if (!Version) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot read lli version: "
                     << Version.getError().message() << "\n";
        return false;
    }
    llvm::SmallVector<llvm::StringRef, 2> Matches;
    if (!llvm::Regex("LLVM version ([0-9]+)\\.").match((*Version)->getBuffer(), &Matches)) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "cannot identify LLVM version of lli at '"
                     << LliPath << "'; LLVM 23 is required\n";
        return false;
    }
    if (Matches[1] != "23") {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "-r requires lli from LLVM 23; found LLVM "
                     << Matches[1] << " at '" << LliPath << "'\n";
        return false;
    }
    return true;
}

} // namespace

bool findLli(std::string &LliPath) {
    auto Lli = llvm::sys::findProgramByName("lli");
    if (!Lli) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << "-r requires lli from LLVM 23 in PATH: " << Lli.getError().message() << "\n";
        return false;
    }
    LliPath = *Lli;
    return checkLliVersion(LliPath);
}

int runLli(const std::string &LliPath, const std::string &IRPath,
           const std::vector<std::string> &Objects) {
    llvm::SmallString<256> RunPath(IRPath);
    if (auto PathError = llvm::sys::fs::make_absolute(RunPath)) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "cannot resolve IR path: " << PathError.message() << "\n";
        return 1;
    }
    std::string ExecutionError;
    std::vector<std::string> LliArgs = {LliPath};
    for (const auto &Object : Objects) LliArgs.push_back("-extra-object=" + Object);
    LliArgs.push_back(std::string(RunPath));
    std::vector<llvm::StringRef> LliArgv(LliArgs.begin(), LliArgs.end());
    int Result = llvm::sys::ExecuteAndWait(LliPath, LliArgv,
                                         std::nullopt, {}, 0, 0, &ExecutionError);
    if (Result < 0) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Error) << "lli execution failed (status " << Result << "): "
                     << ExecutionError << "\n";
        return 1;
    }
    return Result;
}
