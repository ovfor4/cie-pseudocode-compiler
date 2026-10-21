#include "StandardLibrary.h"
#include "cps/Diagnostics.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace {
struct LibraryFile {
    const char *Name;
    const char *Data;
    size_t Size;
};
#include "StandardLibrary.inc"
}

StandardLibrary::~StandardLibrary() {
    if (!Directory.empty()) llvm::sys::fs::remove_directories(Directory);
}

bool StandardLibrary::load(std::vector<std::string> &Inputs) {
    auto Error = llvm::sys::fs::createUniqueDirectory("cpsc-std", Directory);
    if (Error) {
        llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal)
                     << "cannot extract standard library: " << Error.message() << "\n";
        return false;
    }
    for (const auto &File : LibraryFiles) {
        llvm::SmallString<256> Path(Directory);
        llvm::sys::path::append(Path, File.Name);
        llvm::raw_fd_ostream Output(Path, Error);
        if (!Error) {
            Output.write(File.Data, File.Size);
            Output.close();
            Error = Output.error();
        }
        if (Error) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal)
                         << "cannot extract standard library file '" << File.Name
                         << "': " << Error.message() << "\n";
            Output.clear_error();
            return false;
        }
        if (llvm::sys::path::extension(Path) == ".cps") Inputs.emplace_back(Path);
    }
    return true;
}
