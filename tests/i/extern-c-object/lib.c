#include <stdio.h>
#include <string.h>

static int calls;

int add3(int a, int b, int c) { return a + b + c; }
double scale(double x, int k) { return x * k; }
int name_length(const char *s) { return (int)strlen(s); }
int next_call(void) { return ++calls; }
void say(const char *s) { puts(s); }
