% CPSC-KEYWORD-PP_IFDEF(7)

# NAME

cpsc-keyword-pp_ifdef, cpsc-keyword-pp_ifndef, cpsc-keyword-pp_endif - conditionally compile source regions

# SYNOPSIS

```text
PP_IFDEF name
    source lines
PP_ENDIF

PP_IFNDEF name
    source lines
PP_ENDIF
```

# DESCRIPTION

PP_IFDEF, PP_IFNDEF and PP_ENDIF are compiler extensions processed before lexing and parsing. PP_IFDEF includes its region when the named switch is defined; PP_IFNDEF includes it when the switch is undefined. Both require exactly one switch name. Each must have a matching PP_ENDIF, which takes no arguments. Conditions may nest, and a region is included only when all enclosing conditions are enabled. There is no PP_ELSE or condition-expression syntax.

Directives occupy their own lines, optionally preceded by whitespace and followed by // comments. Keywords and switch names are case-sensitive. Switches belong to each file independently and change in source order through [cpsc-keyword-pp_define(7)](cpsc-keyword-pp_define.7.md).

Disabled ordinary source is not lexed, parsed or compiled, and its PP_DEFINE and PP_UNDEF directives have no effect. Conditional nesting is still checked, including inside disabled regions. Directive lines and disabled source preserve original line numbering for diagnostics. Preprocessor conditions do not introduce ordinary scopes or add indentation in the VS Code plugin or Bundle.

# EXAMPLE

```text
PP_DEFINE Debug
PP_IFDEF Debug
OUTPUT "Debug enabled"
PP_IFNDEF Quiet
OUTPUT "Verbose debug output"
PP_ENDIF
PP_ENDIF
PP_UNDEF Debug
PP_IFNDEF Debug
OUTPUT "Debug disabled"
PP_ENDIF
```

# SEE ALSO

[cpsc-keyword-pp_define(7)](cpsc-keyword-pp_define.7.md), [cpsc-language-overview(7)](../cpsc-language-overview.7.md).
