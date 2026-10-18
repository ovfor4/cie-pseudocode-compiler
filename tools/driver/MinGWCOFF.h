#ifndef CPSC_DRIVER_MINGWCOFF_H
#define CPSC_DRIVER_MINGWCOFF_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Object/COFF.h"
#include "llvm/Support/MemoryBuffer.h"
#include <cstddef>
#include <cstring>

namespace {

inline bool isCOFFUnwindSection(llvm::StringRef Name, llvm::StringRef Base) {
    return Name == Base || (Name.starts_with(Base) &&
                           Name.drop_front(Base.size()).starts_with("$"));
}

// MinGW emits .xdata$foo/.pdata$foo as SELECT_ANY without an external
// COMDAT symbol. JITLink waits for that symbol and never creates the section
// symbol used by relocations. Express their implicit association with .text$foo
// as ordinary associative COMDATs in a private copy of the object instead.
inline llvm::Expected<std::unique_ptr<llvm::MemoryBuffer>>
normalizeMinGWObject(std::unique_ptr<llvm::MemoryBuffer> Buffer) {
    using namespace llvm;
    auto Object = object::ObjectFile::createObjectFile(Buffer->getMemBufferRef());
    if (!Object)
        return Object.takeError();
    auto *COFF = dyn_cast<object::COFFObjectFile>(Object->get());
    if (!COFF || COFF->getMachine() != COFF::IMAGE_FILE_MACHINE_AMD64)
        return std::move(Buffer);

    StringMap<int32_t> CodeSections;
    DenseSet<int32_t> ExternalSections;
    for (const auto &Section : COFF->sections()) {
        auto Name = Section.getName();
        if (!Name)
            return Name.takeError();
        if (Name->starts_with(".text$") &&
            (COFF->getCOFFSection(Section)->Characteristics &
             COFF::IMAGE_SCN_LNK_COMDAT))
            CodeSections[*Name] = COFF->getSectionID(Section);
    }
    for (const auto &Symbol : COFF->symbols()) {
        auto Sym = COFF->getCOFFSymbol(Symbol);
        if (Sym.isExternal() && Sym.getSectionNumber() > 0)
            ExternalSections.insert(Sym.getSectionNumber());
    }

    struct Association { size_t Offset; int32_t Parent; };
    SmallVector<Association, 16> Associations;
    SmallVector<size_t, 2> PureVirtualReferences;
    for (const auto &Symbol : COFF->symbols()) {
        auto Sym = COFF->getCOFFSymbol(Symbol);
        if (auto *Weak = Sym.getWeakExternal()) {
            auto Name = Symbol.getName();
            if (!Name)
                return Name.takeError();
            if (*Name == "__cxa_pure_virtual" &&
                Weak->Characteristics == COFF::IMAGE_WEAK_EXTERN_SEARCH_NOLIBRARY) {
                auto Fallback = COFF->getSymbol(Weak->TagIndex);
                if (!Fallback)
                    return Fallback.takeError();
                if (Fallback->isAbsolute() && Fallback->getValue() == 0) {
                    size_t Offset = reinterpret_cast<const char *>(Sym.getRawPtr()) -
                                    Buffer->getBufferStart();
                    Offset += Sym.isBigObj()
                                  ? offsetof(object::coff_symbol32, StorageClass)
                                  : offsetof(object::coff_symbol16, StorageClass);
                    PureVirtualReferences.push_back(Offset);
                }
            }
        }
        auto *Def = Sym.getSectionDefinition();
        if (!Def || Def->Selection != COFF::IMAGE_COMDAT_SELECT_ANY ||
            Sym.getSectionNumber() <= 0 ||
            ExternalSections.contains(Sym.getSectionNumber()))
            continue;
        auto Section = COFF->getSection(Sym.getSectionNumber());
        if (!Section)
            return Section.takeError();
        if (!((*Section)->Characteristics & COFF::IMAGE_SCN_LNK_COMDAT))
            continue;
        auto Name = COFF->getSectionName(*Section);
        if (!Name)
            return Name.takeError();
        if (!Name->starts_with(".xdata$") && !Name->starts_with(".pdata$"))
            continue;
        std::string CodeName = (".text$" + Name->drop_front(7)).str();
        auto Parent = CodeSections.find(CodeName);
        if (Parent == CodeSections.end())
            return make_error<StringError>(
                "missing MinGW unwind code section '" + CodeName + "' in '" +
                    Buffer->getBufferIdentifier().str() + "'",
                inconvertibleErrorCode());
        Associations.push_back({static_cast<size_t>(
                                    reinterpret_cast<const char *>(Def) -
                                    Buffer->getBufferStart()),
                                Parent->second});
    }
    if (Associations.empty() && PureVirtualReferences.empty())
        return std::move(Buffer);

    auto Copy = WritableMemoryBuffer::getNewUninitMemBuffer(
        Buffer->getBufferSize(), Buffer->getBufferIdentifier());
    std::memcpy(Copy->getBufferStart(), Buffer->getBufferStart(),
                Buffer->getBufferSize());
    for (const auto &A : Associations) {
        auto *Def = reinterpret_cast<object::coff_aux_section_definition *>(
            Copy->getBufferStart() + A.Offset);
        Def->Selection = COFF::IMAGE_COMDAT_SELECT_ASSOCIATIVE;
        Def->NumberLowPart = A.Parent & 0xffff;
        Def->NumberHighPart = static_cast<uint32_t>(A.Parent) >> 16;
    }
    // JITLink cannot alias a weak external to an absolute-zero fallback.
    // The runner always supplies the host's __cxa_pure_virtual, so this known
    // optional ABI reference can be an ordinary external. Keep its auxiliary
    // entry count intact so the remaining symbol indices do not move.
    for (size_t Offset : PureVirtualReferences)
        Copy->getBufferStart()[Offset] = COFF::IMAGE_SYM_CLASS_EXTERNAL;
    return std::unique_ptr<MemoryBuffer>(std::move(Copy));
}

} // namespace

#endif
