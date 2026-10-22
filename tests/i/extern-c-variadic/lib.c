#include <stdarg.h>

/* Each argument after n arrives promoted, so va_arg reads int or double. */
int sum_ints(int n, ...) {
    va_list ap;
    int total = 0;
    va_start(ap, n);
    for (int i = 0; i < n; ++i) total += va_arg(ap, int);
    va_end(ap);
    return total;
}

double sum_doubles(int n, ...) {
    va_list ap;
    double total = 0;
    va_start(ap, n);
    for (int i = 0; i < n; ++i) total += va_arg(ap, double);
    va_end(ap);
    return total;
}
