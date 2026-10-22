#include <stdarg.h>

/* Each struct is passed or returned by value; the host C ABI decides how. */
struct Pair { int a, b; };
struct Vec2 { double x, y; };
struct Mixed { char tag; double value; int count; };
struct Vec3f { float x, y, z; };
struct Wide { long long lo, hi; };

int pair_sum(struct Pair p) { return p.a + p.b; }
double vec2_dot(struct Vec2 u, struct Vec2 v) { return u.x * v.x + u.y * v.y; }
double mixed_score(short bias, struct Mixed m, int k) {
    return bias + m.value * k + m.count + (m.tag == 'M');
}
double vec3f_sum(struct Vec3f v) { return v.x + v.y + v.z; }
long long wide_after_five(int a, int b, int c, int d, int e, struct Wide w) {
    return a + b + c + d + e + w.lo + w.hi;
}

struct Pair make_pair(int a, int b) { struct Pair p = {a, b}; return p; }
struct Vec2 make_vec2(double x, double y) { struct Vec2 v = {x, y}; return v; }
struct Mixed make_mixed(char tag, double value, int count) {
    struct Mixed m = {tag, value, count};
    return m;
}
struct Vec3f make_vec3f(float x, float y, float z) { struct Vec3f v = {x, y, z}; return v; }

/* Each argument after n is a struct read back with va_arg. */
double sum_vec2(int n, ...) {
    va_list ap;
    double total = 0;
    va_start(ap, n);
    for (int i = 0; i < n; ++i) {
        struct Vec2 v = va_arg(ap, struct Vec2);
        total += v.x + v.y;
    }
    va_end(ap);
    return total;
}

int mixed_counts(int n, ...) {
    va_list ap;
    int total = 0;
    va_start(ap, n);
    for (int i = 0; i < n; ++i) total += va_arg(ap, struct Mixed).count;
    va_end(ap);
    return total;
}
