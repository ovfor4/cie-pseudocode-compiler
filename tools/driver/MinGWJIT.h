#ifndef CPSC_DRIVER_MINGWJIT_H
#define CPSC_DRIVER_MINGWJIT_H

#if defined(_WIN32) && defined(__x86_64__)
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/Support/MemoryBufferRef.h"
#include <memory>

namespace llvm::jitlink {
class JITLinkMemoryManager;
}
namespace llvm::orc {
class ObjectLinkingLayer;
}

std::unique_ptr<llvm::orc::ObjectLinkingLayer>
createMinGWObjectLinkingLayer(llvm::orc::ExecutionSession &ES,
                             llvm::jitlink::JITLinkMemoryManager &MM);

llvm::Expected<llvm::orc::MaterializationUnit::Interface>
getMinGWObjectFileInterface(llvm::orc::ExecutionSession &ES,
                           llvm::MemoryBufferRef Ref);
#endif

#endif
