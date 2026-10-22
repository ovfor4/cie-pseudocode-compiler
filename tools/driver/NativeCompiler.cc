#include "NativeCompiler.h"
#include "cps/CodeGen.h"
#include "cps/Diagnostics.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Driver/Job.h"
#include "lld/Common/Driver.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/InitializePasses.h"
#include "llvm/PassRegistry.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/TapiFile.h"
#include "llvm/Object/TapiUniversal.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Compression.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FileUtilities.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include <cstdlib>

#ifdef _WIN32
LLD_HAS_DRIVER(mingw)
LLD_HAS_DRIVER(coff)
#elif defined(__APPLE__)
LLD_HAS_DRIVER(macho)
#else
LLD_HAS_DRIVER(elf)
#endif

namespace {

#include "NativeRuntime.inc"

struct TemporaryDirectory {
    llvm::SmallString<256> Path;
    ~TemporaryDirectory() {
        if (!Path.empty()) llvm::sys::fs::remove_directories(Path);
    }
};

bool fail(const llvm::Twine &Message) {
    llvm::errs() << cps::diagnosticPrefix(cps::Severity::Fatal) << Message << "\n";
    return false;
}

class DriverDiagnostics : public clang::DiagnosticConsumer {
    void HandleDiagnostic(clang::DiagnosticsEngine::Level Level,
                          const clang::Diagnostic &Info) override {
        clang::DiagnosticConsumer::HandleDiagnostic(Level, Info);
        llvm::SmallString<256> Message;
        Info.FormatDiagnostic(Message);
        llvm::errs() << cps::diagnosticPrefix(Level >= clang::DiagnosticsEngine::Error
                                                  ? cps::Severity::Error
                                                  : cps::Severity::Warning)
                     << Message << "\n";
    }
};

// Mach-O and TAPI symbols carry the C global prefix '_'.
void addSymbols(const llvm::object::SymbolicFile &File, bool Prefixed, llvm::StringSet<> &Out) {
    auto Add = [&](const llvm::object::BasicSymbolRef &Symbol) {
        auto Flags = Symbol.getFlags();
        if (!Flags) {
            llvm::consumeError(Flags.takeError());
            return;
        }
        using llvm::object::BasicSymbolRef;
        if (!(*Flags & BasicSymbolRef::SF_Global) || (*Flags & BasicSymbolRef::SF_Undefined)) return;
        std::string Name;
        llvm::raw_string_ostream Stream(Name);
        if (auto Error = Symbol.printName(Stream)) {
            llvm::consumeError(std::move(Error));
            return;
        }
        llvm::StringRef Text(Name);
        if (Prefixed && !Text.consume_front("_")) return;
        Out.insert(Text);
    };
    for (const auto &Symbol : File.symbols()) Add(Symbol);
    if (const auto *ELF = llvm::dyn_cast<llvm::object::ELFObjectFileBase>(&File))
        for (const auto &Symbol : ELF->getDynamicSymbolIterators()) Add(Symbol);
}

// Objects, archives (recursively) and TAPI stubs; other inputs such as GNU
// linker scripts contribute nothing.
void addBinarySymbols(llvm::MemoryBufferRef Buffer, llvm::StringSet<> &Out) {
    // Embedded resources and archive members need not be aligned for parsing.
    auto Copy = llvm::MemoryBuffer::getMemBufferCopy(Buffer.getBuffer(), Buffer.getBufferIdentifier());
    auto Binary = llvm::object::createBinary(Copy->getMemBufferRef());
    if (!Binary) {
        llvm::consumeError(Binary.takeError());
        return;
    }
    if (auto *Archive = llvm::dyn_cast<llvm::object::Archive>(Binary->get())) {
        llvm::Error Error = llvm::Error::success();
        for (const auto &Child : Archive->children(Error)) {
            auto Member = Child.getMemoryBufferRef();
            if (Member) addBinarySymbols(*Member, Out);
            else llvm::consumeError(Member.takeError());
        }
        llvm::consumeError(std::move(Error));
    } else if (auto *Tapi = llvm::dyn_cast<llvm::object::TapiUniversal>(Binary->get())) {
        for (const auto &Object : Tapi->objects()) {
            auto File = Object.getAsObjectFile();
            if (File) addSymbols(**File, true, Out);
            else llvm::consumeError(File.takeError());
        }
    } else if (auto *File = llvm::dyn_cast<llvm::object::SymbolicFile>(Binary->get())) {
        addSymbols(*File, File->isMachO(), Out);
    }
}

bool isLibcInput(llvm::StringRef Path) {
    llvm::StringRef Name = llvm::sys::path::filename(Path);
#ifdef _WIN32
    return Name.starts_with("libmsvcrt") || Name.starts_with("libucrt") ||
           Name == "libmingwex.a" || Name == "libm.a";
#elif defined(__APPLE__)
    return Name == "libSystem.B.tbd" || Name == "libsystem_c.tbd" || Name == "libsystem_m.tbd";
#else
    return Name.starts_with("libc.so") || Name == "libc_nonshared.a" ||
           Name.starts_with("libm.so") || Name.starts_with("libmvec.so");
#endif
}

// The embedded link resources: their manifest and decompressed file contents.
// Metadata points into Manifest, so the object is neither copied nor moved.
struct NativeRuntime {
    std::optional<llvm::json::Value> Manifest;
    llvm::json::Object *Metadata = nullptr;
    llvm::SmallVector<uint8_t, 0> Data;

    NativeRuntime() = default;
    NativeRuntime(const NativeRuntime &) = delete;
    NativeRuntime &operator=(const NativeRuntime &) = delete;

    bool load() {
        auto Parsed = llvm::json::parse(NativeRuntimeManifest);
        if (!Parsed) return fail(llvm::toString(Parsed.takeError()));
        Manifest = std::move(*Parsed);
        Metadata = Manifest->getAsObject();
        if (auto Error = llvm::compression::zlib::decompress(
                llvm::ArrayRef(reinterpret_cast<const uint8_t *>(NativeRuntimeData),
                               sizeof(NativeRuntimeData) - 1), Data, NativeRuntimeSize))
            return fail("cannot unpack native link resources: " + llvm::toString(std::move(Error)));
        return true;
    }

    const llvm::json::Array &files() const { return *Metadata->getArray("files"); }

    llvm::StringRef contents(const llvm::json::Object &File) const {
        return llvm::StringRef(reinterpret_cast<const char *>(Data.data()) +
                                   *File.getInteger("offset"), *File.getInteger("size"));
    }
};

} // namespace

bool collectObjectSymbols(llvm::StringRef Path, llvm::StringSet<> &Out, std::string &Error) {
    auto Buffer = llvm::MemoryBuffer::getFile(Path);
    if (!Buffer) {
        Error = Buffer.getError().message();
        return false;
    }
    auto Binary = llvm::object::createBinary((*Buffer)->getMemBufferRef());
    if (!Binary) {
        Error = llvm::toString(Binary.takeError());
        return false;
    }
    auto *Object = llvm::dyn_cast<llvm::object::ObjectFile>(Binary->get());
    if (!Object) {
        Error = "not an object file";
        return false;
    }
    addSymbols(*Object, Object->isMachO(), Out);
    return true;
}

bool collectLibcSymbols(llvm::StringSet<> &Out) {
    NativeRuntime Resources;
    if (!Resources.load()) return false;
    for (const auto &Entry : Resources.files()) {
        const auto &File = *Entry.getAsObject();
        if (*File.getBoolean("script") || !isLibcInput(*File.getString("path"))) continue;
        addBinarySymbols(llvm::MemoryBufferRef(Resources.contents(File), *File.getString("path")), Out);
    }
    return true;
}

bool unpackCxxRuntime(std::vector<std::unique_ptr<llvm::MemoryBuffer>> &Archives) {
    NativeRuntime Resources;
    if (!Resources.load()) return false;
    for (const auto &Entry : Resources.files()) {
        const auto &File = *Entry.getAsObject();
        llvm::StringRef Path = *File.getString("path");
        llvm::StringRef Name = llvm::sys::path::filename(Path);
        // The unwind and C libraries continue to come from the host process.
        if (*File.getBoolean("script") ||
            !(Name == "libstdc++.a" || Name == "libc++.a" || Name == "libc++abi.a" ||
              Name == "libgcc.a" || Name == "libmingwex.a" || Name == "libm.a")) continue;
        Archives.push_back(llvm::MemoryBuffer::getMemBufferCopy(Resources.contents(File), Path));
    }
    return true;
}

bool compileNative(const cps::CodeGen &CG, llvm::StringRef OutputPath, const char *Argv0,
                   const std::vector<std::string> &Objects) {
    TemporaryDirectory Runtime;
    if (auto EC = llvm::sys::fs::createUniqueDirectory("cpsc-native", Runtime.Path))
        return fail("cannot create native compilation directory: " + EC.message());

    NativeRuntime Resources;
    if (!Resources.load()) return false;
    auto &Metadata = *Resources.Metadata;

    auto relocate = [&](llvm::StringRef Text) {
        std::string Result = Text.str();
        const std::string Marker = "@CPSC_RUNTIME@";
        size_t Position = 0;
        while ((Position = Result.find(Marker, Position)) != std::string::npos) {
            Result.replace(Position, Marker.size(), Runtime.Path.str());
            Position += Runtime.Path.size();
        }
        return Result;
    };
    for (const auto &Entry : Resources.files()) {
        const auto &File = *Entry.getAsObject();
        llvm::SmallString<256> Path(Runtime.Path);
        llvm::sys::path::append(Path, *File.getString("path"));
        if (auto EC = llvm::sys::fs::create_directories(llvm::sys::path::parent_path(Path)))
            return fail("cannot prepare native link resources: " + EC.message());
        std::error_code EC;
        llvm::raw_fd_ostream Output(Path, EC);
        if (EC) return fail("cannot open native link resource: " + EC.message());
        llvm::StringRef Contents = Resources.contents(File);
        if (*File.getBoolean("script")) Output << relocate(Contents);
        else Output << Contents;
        Output.close();
        if (Output.has_error()) {
            auto Error = Output.error();
            Output.clear_error();
            return fail("cannot write native link resource: " + Error.message());
        }
    }

    if (llvm::InitializeNativeTarget() || llvm::InitializeNativeTargetAsmPrinter())
        return fail("cannot initialize the native LLVM target");
    auto &Registry = *llvm::PassRegistry::getPassRegistry();
    llvm::initializeCore(Registry);
    llvm::initializeCodeGen(Registry);
    llvm::initializeScalarOpts(Registry);
    llvm::initializeVectorization(Registry);
    llvm::initializeTransformUtils(Registry);
    llvm::initializeInstCombine(Registry);
    llvm::initializeTarget(Registry);
    llvm::Triple Triple(*Metadata.getString("target"));
    std::string Error;
    const auto *Target = llvm::TargetRegistry::lookupTarget(Triple, Error);
    if (!Target) return fail(Error);
    std::unique_ptr<llvm::TargetMachine> Machine(Target->createTargetMachine(
        Triple, "generic", "", llvm::TargetOptions(), llvm::Reloc::PIC_));
    if (!Machine) return fail("cannot create the native LLVM target machine");
    auto Module = llvm::CloneModule(CG.getModule());
    Module->setTargetTriple(Triple);
    Module->setDataLayout(Machine->createDataLayout());
    llvm::SmallString<256> ObjectPath(Runtime.Path);
    llvm::sys::path::append(ObjectPath, "program.o");
    std::error_code EC;
    llvm::raw_fd_ostream ObjectOutput(ObjectPath, EC);
    if (EC) return fail("cannot open native object file: " + EC.message());
    llvm::legacy::PassManager Passes;
    if (Machine->addPassesToEmitFile(Passes, ObjectOutput, nullptr,
                                    llvm::CodeGenFileType::ObjectFile))
        return fail("native target cannot emit object files");
    Passes.run(*Module);
    ObjectOutput.close();
    if (ObjectOutput.has_error()) {
        auto Error = ObjectOutput.error();
        ObjectOutput.clear_error();
        return fail("cannot write native object file: " + Error.message());
    }

    // Link beside the destination so the final rename stays on one filesystem.
    llvm::SmallString<256> StagedPath;
    int FD;
    if (auto EC = llvm::sys::fs::createUniqueFile(OutputPath + ".tmp-%%%%%%", FD, StagedPath))
        return fail("cannot open native output file: " + EC.message());
    llvm::FileRemover StagedOutput(StagedPath);
    llvm::raw_fd_ostream CloseFD(FD, true);
    CloseFD.close();

    // All link inputs are explicit. Clang's driver provides job validation and
    // platform command construction; no compiler subprocess is executed.
    clang::DiagnosticOptions Options;
    DriverDiagnostics Printer;
    clang::DiagnosticsEngine Diagnostics(new clang::DiagnosticIDs, Options, &Printer, false);
    clang::driver::Driver Driver("cpsc", Triple.str(), Diagnostics);
    // Clang validates the linker executable even for an unlaunched job. Use
    // cpsc's own path; only the job's arguments are passed to embedded LLD.
    std::vector<std::string> Args = {"cpsc", "--driver-mode=g++", "-nostdlib", "-nostdlib++",
        "-fuse-ld=lld", "-o", std::string(StagedPath), std::string(ObjectPath)};
    Args.insert(Args.end(), Objects.begin(), Objects.end());
    Args.push_back("--ld-path=" + llvm::sys::fs::getMainExecutable(
        Argv0, reinterpret_cast<void *>(&compileNative)));
    for (const auto &Arg : *Metadata.getArray("arguments")) {
        Args.push_back("-Xlinker");
        Args.push_back(relocate(*Arg.getAsString()));
    }
    std::vector<const char *> Argv;
    for (const auto &Arg : Args) Argv.push_back(Arg.c_str());
    Driver.setCheckInputsExist(false);
    std::unique_ptr<clang::driver::Compilation> Compilation(Driver.BuildCompilation(Argv));
    if (!Compilation || Diagnostics.hasErrorOccurred()) return false;
    if (Compilation->getJobs().size() != 1)
        return fail("expected one embedded linker job");
    const auto &Job = *Compilation->getJobs().begin();
    llvm::SmallVector<const char *, 64> LinkArgs;
#ifdef _WIN32
    LinkArgs.push_back("ld.lld");
    const lld::DriverDef Linkers[] = {{lld::MinGW, &lld::mingw::link}, {lld::WinLink, &lld::coff::link}};
#elif defined(__APPLE__)
    LinkArgs.push_back("ld64.lld");
    const lld::DriverDef Linkers[] = {{lld::Darwin, &lld::macho::link}};
#else
    LinkArgs.push_back("ld.lld");
    const lld::DriverDef Linkers[] = {{lld::Gnu, &lld::elf::link}};
#endif
    LinkArgs.append(Job.getArguments().begin(), Job.getArguments().end());
    auto Result = lld::lldMain(LinkArgs, llvm::outs(), llvm::errs(), Linkers);
    if (!Result.canRunAgain) std::exit(1);
    if (Result.retCode != 0) return fail("native linking failed");
    if (auto EC = llvm::sys::fs::setPermissions(StagedPath,
            llvm::sys::fs::all_read | llvm::sys::fs::owner_write | llvm::sys::fs::all_exe))
        return fail("cannot set executable permissions: " + EC.message());
    if (auto EC = llvm::sys::fs::rename(StagedPath, OutputPath))
        return fail("cannot save native output: " + EC.message());
    return true;
}
