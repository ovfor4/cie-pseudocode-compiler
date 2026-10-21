#pragma once
#include <cstdarg>
#include <cstdio>
#include <string>

namespace cps {
enum class Severity { Error, Warning, Note, Fatal };

// Path of the unit being diagnosed, as given on the command line.
inline std::string DiagnosticSource;

// gcc layout: "<locus>: <label>: "; an empty locus prints the label alone.
inline std::string diagnosticPrefix(Severity S, const std::string &Locus = "cpsc") {
    const char *Label = S == Severity::Error   ? "error: "
                      : S == Severity::Warning ? "warning: "
                      : S == Severity::Note    ? "note: "
                                               : "fatal error: ";
    return (Locus.empty() ? "" : Locus + ": ") + Label;
}

// Prints "<path>:<line>: <label>: <message>\n" (just "<path>:" when the line is
// unknown, "cpsc:" when no source is set).
inline void vsourceDiagnostic(Severity S, int Line, const char *Format, va_list Args) {
    std::string Locus = DiagnosticSource.empty() ? "cpsc" : DiagnosticSource;
    if (!DiagnosticSource.empty() && Line > 0) Locus += ":" + std::to_string(Line);
    std::fputs(diagnosticPrefix(S, Locus).c_str(), stderr);
    std::vfprintf(stderr, Format, Args);
    std::fputc('\n', stderr);
}

__attribute__((format(printf, 2, 3)))
inline void sourceError(int Line, const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    vsourceDiagnostic(Severity::Error, Line, Format, Args);
    va_end(Args);
}

__attribute__((format(printf, 2, 3)))
inline void sourceWarning(int Line, const char *Format, ...) {
    va_list Args;
    va_start(Args, Format);
    vsourceDiagnostic(Severity::Warning, Line, Format, Args);
    va_end(Args);
}
}
