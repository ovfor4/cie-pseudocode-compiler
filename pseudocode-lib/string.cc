#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

extern "C" int __cps_user_line;

namespace {
template <class Operation>
decltype(auto) checked(Operation &&Op) {
    try {
        return std::forward<Operation>(Op)();
    } catch (const std::exception &Error) {
        std::fprintf(stderr, "[Fatal] line %d: %s\n", __cps_user_line, Error.what());
        throw 0;
    }
}
std::string &text(void *Handle) { return *static_cast<std::string *>(Handle); }

// Python-repr-style shortest round-trip text for a REAL. Keep in sync with getOutputRealHelper in
// lib/CodeGen/OutputGen.cc, which OUTPUT uses.
std::string realText(double Number) {
    if (std::isnan(Number)) return "nan";
    if (std::isinf(Number)) return Number < 0 ? "-inf" : "inf";
    char Buffer[64];
    double Abs = std::fabs(Number);
    if (Abs == 0 || (Abs >= 1e-4 && Abs < 1e16)) {
        for (int Decimals = 1; Decimals < 40; ++Decimals) {
            std::snprintf(Buffer, sizeof Buffer, "%.*f", Decimals, Number);
            if (std::strtod(Buffer, nullptr) == Number) break;
        }
    } else {
        for (int Precision = 0; Precision < 17; ++Precision) {
            std::snprintf(Buffer, sizeof Buffer, "%.*e", Precision, Number);
            if (std::strtod(Buffer, nullptr) == Number) break;
        }
    }
    return Buffer;
}

enum class NumberStatus { Ok, Invalid, OutOfRange };

// Shared by IS_NUM and STR_TO_NUM so that they always agree.
NumberStatus parseNumber(const std::string &Source, double &Result) {
    if (Source.empty()) return NumberStatus::Invalid;
    char *End = nullptr;
    errno = 0;
    double Number = std::strtod(Source.c_str(), &End);
    if (End != Source.c_str() + Source.size()) return NumberStatus::Invalid;
    if (errno == ERANGE && std::isinf(Number)) return NumberStatus::OutOfRange;
    Result = Number;
    return NumberStatus::Ok;
}
}

extern "C" {
void *_string_impl_new() {
    return checked([] { return new std::string; });
}
void *_string_impl_copy(void *Other) {
    return checked([&] { return new std::string(text(Other)); });
}
void *_string_impl_from_cstr(const char *Text) {
    return checked([&] { return new std::string(Text); });
}
void *_string_impl_from_count_char(long long Count, char Ch) {
    return checked([&] {
        if (Count < 0) throw std::invalid_argument("negative string length");
        return new std::string(static_cast<std::size_t>(Count), Ch);
    });
}
void _string_impl_delete(void *Handle) {
    checked([&] { delete static_cast<std::string *>(Handle); });
}
void _string_impl_assign(void *Target, void *Other) {
    checked([&] { text(Target) = text(Other); });
}
void _string_impl_assign_cstr(void *Target, const char *Text) {
    checked([&] { text(Target) = Text; });
}
void _string_impl_concat(void *Target, void *A, void *B) {
    checked([&] { text(Target) = text(A) + text(B); });
}
void _string_impl_from_int(void *Target, long long Number) {
    checked([&] { text(Target) = std::to_string(Number); });
}
void _string_impl_from_real(void *Target, double Number) {
    checked([&] { text(Target) = realText(Number); });
}
double _string_impl_to_real(void *Handle) {
    return checked([&] {
        double Number = 0;
        switch (parseNumber(text(Handle), Number)) {
        case NumberStatus::Ok: return Number;
        case NumberStatus::OutOfRange:
            throw std::out_of_range("STR_TO_NUM: '" + text(Handle) + "' is out of the REAL range");
        case NumberStatus::Invalid: break;
        }
        throw std::invalid_argument("STR_TO_NUM: '" + text(Handle) + "' is not a number");
    });
}
long long _string_impl_length(void *Handle) {
    return checked([&] { return static_cast<long long>(text(Handle).size()); });
}
void _string_impl_left(void *Target, void *Handle, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size());
        text(Target) = Source.substr(0, Length);
    });
}
void _string_impl_right(void *Target, void *Handle, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size());
        text(Target) = Source.substr(Source.size() - Length, Length);
    });
}
void _string_impl_mid(void *Target, void *Handle, long long Start, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Position = Start <= 1 ? 0 : std::min(static_cast<std::size_t>(Start - 1), Source.size());
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size() - Position);
        text(Target) = Source.substr(Position, Length);
    });
}
void _string_impl_lcase(void *Target, void *Handle) {
    checked([&] {
        text(Target) = text(Handle);
        for (char &Ch : text(Target))
            Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
    });
}
char _string_impl_lcase_char(char Ch) {
    return checked([&] {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
    });
}
void _string_impl_ucase(void *Target, void *Handle) {
    checked([&] {
        text(Target) = text(Handle);
        for (char &Ch : text(Target))
            Ch = static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
    });
}
char _string_impl_ucase_char(char Ch) {
    return checked([&] {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(Ch)));
    });
}
long long _string_impl_asc(void *Handle) {
    return checked([&]() -> long long {
        const auto &Source = text(Handle);
        return Source.empty() ? 0 : static_cast<unsigned char>(Source.front());
    });
}
long long _string_impl_asc_char(char Ch) {
    return checked([&]() -> long long { return static_cast<unsigned char>(Ch); });
}
void _string_impl_chr(void *Target, long long Code) {
    checked([&] {
        text(Target).assign(1, static_cast<char>(static_cast<unsigned char>(Code)));
    });
}
int _string_impl_is_num(void *Handle) {
    return checked([&]() -> int {
        double Number = 0;
        return parseNumber(text(Handle), Number) == NumberStatus::Ok;
    });
}
const char *_string_impl_c_str(void *Handle) {
    return text(Handle).c_str();
}
int _string_impl_compare(void *A, void *B) {
    return checked([&] { return text(A).compare(text(B)); });
}
void _string_impl_input(void *Handle) {
    checked([&] {
        std::string Value;
        int Ch;
        do { Ch = std::getchar(); } while (Ch != EOF && std::isspace(static_cast<unsigned char>(Ch)));
        while (Ch != EOF && !std::isspace(static_cast<unsigned char>(Ch))) {
            Value.push_back(static_cast<char>(Ch));
            Ch = std::getchar();
        }
        if (Ch != EOF) std::ungetc(Ch, stdin);
        text(Handle) = std::move(Value);
    });
}
}
