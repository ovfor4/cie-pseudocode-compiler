#pragma once
#include <cstdarg>
#include <cstdio>
#include <set>
#include <string>

namespace cps {
enum class Severity { Error, Warning, Note, Fatal };

// Path of the unit being diagnosed, as given on the command line.
inline std::string DiagnosticSource;

// Set by the driver from -fdiagnostics-color.
inline bool DiagnosticColors = false;

// gcc layout: "<locus>: <label>: "; an empty locus prints the label alone.
// With colors, the bytes match gcc's default GCC_COLORS: bold locus
// (including its colon), then the label in bold red/magenta/cyan.
inline std::string diagnosticPrefix(Severity S, const std::string &Locus = "cpsc") {
    const char *Label = S == Severity::Error   ? "error: "
                      : S == Severity::Warning ? "warning: "
                      : S == Severity::Note    ? "note: "
                                               : "fatal error: ";
    if (!DiagnosticColors) return (Locus.empty() ? "" : Locus + ": ") + Label;
    const char *Color = S == Severity::Warning ? "01;35"
                      : S == Severity::Note    ? "01;36"
                                               : "01;31";
    auto Span = [](const char *SGR, const std::string &Text) {
        return std::string("\33[") + SGR + "m\33[K" + Text + "\33[m\33[K";
    };
    return (Locus.empty() ? "" : Span("01", Locus + ":") + " ") + Span(Color, Label);
}

// Every diagnostic line printed so far, to suppress exact repeats.
inline std::set<std::string> PrintedDiagnostics;

// Prints "<path>:<line>: <label>: <message>\n" (just "<path>:" when the line is
// unknown, "cpsc:" when no source is set).
inline void vsourceDiagnostic(Severity S, int Line, const char *Format, va_list Args) {
    std::string Locus = DiagnosticSource.empty() ? "cpsc" : DiagnosticSource;
    if (!DiagnosticSource.empty() && Line > 0) Locus += ":" + std::to_string(Line);
    va_list Copy;
    va_copy(Copy, Args);
    int Length = std::vsnprintf(nullptr, 0, Format, Copy);
    va_end(Copy);
    std::string Message(Length > 0 ? Length : 0, '\0');
    if (Length > 0) std::vsnprintf(Message.data(), Message.size() + 1, Format, Args);
    std::string Text = diagnosticPrefix(S, Locus) + Message + '\n';
    // The same node can be diagnosed more than once; print each line once.
    if (!PrintedDiagnostics.insert(Text).second) return;
    std::fputs(Text.c_str(), stderr);
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
