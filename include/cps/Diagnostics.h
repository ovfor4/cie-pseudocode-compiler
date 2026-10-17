#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace cps {
// Empty for single-source builds to preserve existing diagnostic text.
inline std::string DiagnosticSource;
inline std::string lineSuffix(int Line) {
    return " at line " + std::to_string(Line) +
           (DiagnosticSource.empty() ? "" : " in " + DiagnosticSource);
}
inline void sourceDiagnostic(int Line, const char *Format, ...) {
    va_list Args, Copy;
    va_start(Args, Format);
    va_copy(Copy, Args);
    int Size = std::vsnprintf(nullptr, 0, Format, Copy);
    va_end(Copy);
    if (Size < 0) { va_end(Args); return; }
    std::vector<char> Buffer(Size + 1);
    std::vsnprintf(Buffer.data(), Buffer.size(), Format, Args);
    va_end(Args);
    std::string Message(Buffer.data(), Size);
    if (!DiagnosticSource.empty()) {
        auto At = Message.find("at line ");
        if (At != std::string::npos) {
            auto End = Message.find_first_not_of("0123456789", At + 8);
            Message.insert(End == std::string::npos ? Message.size() : End,
                           " in " + DiagnosticSource);
        } else {
            auto Old = Message.find("(line ");
            if (Old != std::string::npos) {
                auto End = Message.find(')', Old);
                Message.replace(Old, End - Old + 1, lineSuffix(Line).substr(1));
            } else {
                auto End = Message.find_last_not_of("\n");
                Message.insert(End == std::string::npos ? 0 : End + 1, lineSuffix(Line));
            }
        }
    }
    std::fputs(Message.c_str(), stderr);
}
}
