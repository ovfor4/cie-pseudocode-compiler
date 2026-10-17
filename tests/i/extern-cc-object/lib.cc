// No C++ runtime library: only C++ language features, so unmangled names are the point.
namespace {
template <typename T>
struct Accumulator {
    T total;
    constexpr Accumulator() : total(100) {}
    T add(T v) { total += v; return total; }
};
Accumulator<int> acc;

unsigned length_of(const char *s) {
    unsigned n = 0;
    while (s[n]) ++n;
    return n;
}
}

extern "C" int puts(const char *);

extern "C" {
int cc_sum(void) { return acc.total; }
int cc_add(int v) { return acc.add(v); }
int cc_text_length(const char *s) { return (int)length_of(s) + 2; }
void cc_say(const char *s) { puts(s); }
}
