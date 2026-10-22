#include <stdarg.h>

struct Sample {
    char tag;
    double value;
    int count;
};

static struct Sample shared = {'G', 0.5, 7};

void scale_sample(struct Sample *s, int k) { s->value *= k; }
struct Sample *shared_sample(void) { return &shared; }
int shared_count(void) { return shared.count; }

struct Counter {
    int *target;
};

void bump(int *p) { ++*p; }
void bump_counter(struct Counter *c) { ++*c->target; }
void get_shared(struct Sample **out) { *out = &shared; }

/* Each argument after n is a struct Sample *, read back with va_arg. */
int sum_counts(int n, ...) {
    va_list ap;
    int total = 0;
    va_start(ap, n);
    for (int i = 0; i < n; ++i) total += va_arg(ap, struct Sample *)->count;
    va_end(ap);
    return total;
}
