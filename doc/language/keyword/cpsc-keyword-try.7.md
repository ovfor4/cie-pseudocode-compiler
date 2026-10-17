% CPSC-KEYWORD-TRY(7)

# NAME

cpsc-keyword-try, cpsc-keyword-catch, cpsc-keyword-endtry, cpsc-keyword-throw - catch exceptions with stack unwinding

# SYNOPSIS

```text
TRY
    statements
CATCH
    handler statements
ENDTRY

THROW
```

# DESCRIPTION

TRY, CATCH, ENDTRY, and THROW are compiler extensions to Cambridge pseudocode. Each TRY requires exactly one CATCH and an ENDTRY. CATCH takes no type or error variable; there is no error object or what() support.

Normal execution skips CATCH. An exception stops the current operation, unwinds function and procedure calls, and enters the nearest active CATCH. After the handler completes, execution continues after ENDTRY. Blocks may nest and may appear inside functions, procedures, and loops.

THROW takes no argument and always raises a new anonymous exception. An exception raised inside CATCH propagates to an enclosing TRY or a caller's handler; the same CATCH cannot catch it again. RETURN may leave either body, and leaving a handler finishes its catch state.

Runtime checks throw the same anonymous exceptions for division by zero, array index errors, null pointer dereferences, invalid enumeration values, array-parameter bounds mismatches, and file misuse. Before throwing, a runtime check prints the source line and error reason to standard error, even when CATCH handles the exception. Explicit THROW prints no diagnostic itself. Compiler errors cannot be caught. If no handler catches an exception, the program prints `Unhandled exception. Program terminated.` to standard error and returns status 1.

Arrays declared inside either body remain local to that block. Stack unwinding does not automatically free heap strings or arrays, close files, or run user-defined destructors. Exception execution is verified on Linux with LLVM 23; Windows and macOS remain unverified.

# EXAMPLE

```text
PROCEDURE Fail()
    THROW
ENDPROCEDURE

TRY
    CALL Fail()
CATCH
    OUTPUT "Caught an exception"
ENDTRY
OUTPUT "Continuing"
```

# SEE ALSO

[cpsc-language-overview(7)](../cpsc-language-overview.7.md), [cpsc-keyword-function(7)](cpsc-keyword-function.7.md), [cpsc-keyword-procedure(7)](cpsc-keyword-procedure.7.md), [cpsc(1)](../../cpsc.1.md).
