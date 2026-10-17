% CPSC-KEYWORD-INCLUDE_OBJ(7)

# NAME

cpsc-keyword-include_obj - link a C object file or the C library

# SYNOPSIS

```text
INCLUDE_OBJ "path.o"
INCLUDE_OBJ "__libc"
```

# DESCRIPTION

INCLUDE_OBJ is a compiler extension, allowed only at top level. It names an object file whose functions EXTERN declarations may call. Resolve the path relative to the including source's directory; it must name a `.o` object file built for the same platform, for example with `cc -c lib.c`. The special name `"__libc"` selects the C standard library, including the math library; it must be included explicitly before declaring C library functions.

INCLUDE_OBJ applies to the whole program: objects from every input file are linked together, and repeated objects are linked once. Each EXTERN symbol must be defined by one of them. Native output links the objects, and `-r` loads them for execution. `-S` writes IR only; link any objects yourself when building the IR manually.

# EXAMPLE

`lib.c`, compiled with `cc -c lib.c`:

```text
int add3(int a, int b, int c) { return a + b + c; }
```

`me.cps`:

```text
INCLUDE_OBJ "lib.o"
EXTERN "C" EXTERN_FUNCTION add3(C_INT, C_INT, C_INT) RETURNS C_INT
OUTPUT add3((C_INT)1, (C_INT)2, (C_INT)3)
```

Run with `cpsc me.cps -r` to print `6`.

# SEE ALSO

[cpsc-keyword-extern(7)](cpsc-keyword-extern.7.md), [cpsc-keyword-include(7)](cpsc-keyword-include.7.md), [cpsc(1)](../../cpsc.1.md).
