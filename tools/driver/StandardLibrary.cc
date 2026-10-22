#include "StandardLibrary.h"
#include "cps/Diagnostics.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/xxhash.h"

namespace {
struct LibraryFile {
    const char *Name;
    const char *Data;
    size_t Size;
};
#include "StandardLibrary.inc"
}

StandardLibrary::~StandardLibrary() {
    if (!Directory.empty() && Temporary) llvm::sys::fs::remove_directories(Directory);
}

namespace {
std::string contentKey() {
    std::string Bytes;
    for (const auto &File : LibraryFiles) {
        Bytes.append(File.Name);
        Bytes.push_back('\0');
        Bytes.append(File.Data, File.Size);
    }
    return llvm::utohexstr(llvm::xxh3_64bits(llvm::ArrayRef<uint8_t>(
        reinterpret_cast<const uint8_t *>(Bytes.data()), Bytes.size())), true);
}

bool sameContents(const llvm::Twine &Path, const LibraryFile &File) {
    auto Existing = llvm::MemoryBuffer::getFile(Path);
    return Existing && (*Existing)->getBuffer() == llvm::StringRef(File.Data, File.Size);
}

// Atomically publishes the bytes at Path; concurrent runs never see partial files.
std::error_code publish(llvm::StringRef Directory, llvm::StringRef Path, const LibraryFile &File) {
    llvm::SmallString<256> Pattern(Directory);
    llvm::sys::path::append(Pattern, "tmp-%%%%%%%%");
    auto Temp = llvm::sys::fs::TempFile::create(Pattern);
    if (!Temp) return llvm::errorToErrorCode(Temp.takeError());
    llvm::raw_fd_ostream Output(Temp->FD, false);
    Output.write(File.Data, File.Size);
    Output.flush();
    if (Output.has_error()) {
        auto EC = Output.error();
        Output.clear_error();
        llvm::consumeError(Temp->discard());
        return EC;
    }
    if (auto Error = Temp->keep(Path)) return llvm::errorToErrorCode(std::move(Error));
    return {};
}
}

bool StandardLibrary::load(std::vector<std::string> &Inputs) {
    // The extracted files are cached per user and keyed by content, so killed runs leave nothing behind.
    llvm::SmallString<256> Cache;
    if (llvm::sys::path::cache_directory(Cache)) {
        llvm::sys::path::append(Cache, "cpsc", "std-" + contentKey());
        if (!llvm::sys::fs::create_directories(Cache)) Directory = Cache;
    }
    if (Directory.empty()) {
        Temporary = true;
        if (auto Error = llvm::sys::fs::createUniqueDirectory("cpsc-std", Directory)) {
            llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal)
                         << "cannot extract standard library: " << Error.message() << "\n";
            return false;
        }
    }
    for (const auto &File : LibraryFiles) {
        llvm::SmallString<256> Path(Directory);
        llvm::sys::path::append(Path, File.Name);
        if (!(!Temporary && sameContents(Path, File))) {
            std::error_code Error;
            if (Temporary) {
                llvm::raw_fd_ostream Output(Path, Error);
                if (!Error) {
                    Output.write(File.Data, File.Size);
                    Output.close();
                    Error = Output.error();
                    Output.clear_error();
                }
            } else {
                Error = publish(Directory, Path, File);
            }
            if (Error) {
                llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal)
                             << "cannot extract standard library file '" << File.Name
                             << "': " << Error.message() << "\n";
                return false;
            }
        }
        if (llvm::sys::path::extension(Path) == ".cps") Inputs.emplace_back(Path);
    }
    return true;
}
