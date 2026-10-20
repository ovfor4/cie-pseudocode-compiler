#include <algorithm>
#include <cctype>
#include <cstdio>
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
}

extern "C" {
void *_exp_string_impl_new() {
    return checked([] { return new std::string; });
}
void *_exp_string_impl_copy(void *Other) {
    return checked([&] { return new std::string(text(Other)); });
}
void *_exp_string_impl_from_cstr(const char *Text) {
    return checked([&] { return new std::string(Text); });
}
void *_exp_string_impl_from_count_char(long long Count, char Ch) {
    return checked([&] {
        if (Count < 0) throw std::invalid_argument("negative string length");
        return new std::string(static_cast<std::size_t>(Count), Ch);
    });
}
void _exp_string_impl_delete(void *Handle) {
    checked([&] { delete static_cast<std::string *>(Handle); });
}
void _exp_string_impl_assign(void *Target, void *Other) {
    checked([&] { text(Target) = text(Other); });
}
void _exp_string_impl_assign_cstr(void *Target, const char *Text) {
    checked([&] { text(Target) = Text; });
}
void _exp_string_impl_concat(void *Target, void *A, void *B) {
    checked([&] { text(Target) = text(A) + text(B); });
}
void _exp_string_impl_from_int(void *Target, long long Number) {
    checked([&] { text(Target) = std::to_string(Number); });
}
void _exp_string_impl_from_real(void *Target, double Number) {
    checked([&] { text(Target) = std::to_string(Number); });
}
double _exp_string_impl_to_real(void *Handle) {
    return checked([&] {
        std::size_t Consumed = 0;
        double Number = std::stod(text(Handle), &Consumed);
        if (Consumed != text(Handle).size())
            throw std::invalid_argument("unconsumed characters in string conversion");
        return Number;
    });
}
long long _exp_string_impl_length(void *Handle) {
    return checked([&] { return static_cast<long long>(text(Handle).size()); });
}
void _exp_string_impl_left(void *Target, void *Handle, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size());
        text(Target) = Source.substr(0, Length);
    });
}
void _exp_string_impl_right(void *Target, void *Handle, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size());
        text(Target) = Source.substr(Source.size() - Length, Length);
    });
}
void _exp_string_impl_mid(void *Target, void *Handle, long long Start, long long Count) {
    checked([&] {
        const auto &Source = text(Handle);
        auto Position = Start <= 1 ? 0 : std::min(static_cast<std::size_t>(Start - 1), Source.size());
        auto Length = Count <= 0 ? 0 : std::min(static_cast<std::size_t>(Count), Source.size() - Position);
        text(Target) = Source.substr(Position, Length);
    });
}
void _exp_string_impl_lcase(void *Target, void *Handle) {
    checked([&] {
        text(Target) = text(Handle);
        for (char &Ch : text(Target))
            Ch = static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
    });
}
char _exp_string_impl_lcase_char(char Ch) {
    return checked([&] {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(Ch)));
    });
}
void _exp_string_impl_output(void *Handle) {
    checked([&] {
        const auto &Text = text(Handle);
        std::fwrite(Text.data(), 1, Text.size(), stdout);
        std::fputc('\n', stdout);
    });
}
}
