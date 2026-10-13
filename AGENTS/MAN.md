# Man documentation

Read this file before editing the manual sources or their build, packaging and installation paths. All project text is English.

## Sources and naming

- `doc/cpsc.1.md`: command-line manual, section 1.
- `doc/language/cpsc-language-overview.7.md`: syntax overview and index, section 7.
- `doc/language/keyword/cpsc-keyword-NAME.7.md`: keyword manuals.
- `doc/language/function/cpsc-function-NAME.7.md`: built-in function manuals.

Use lowercase filenames and preserve underscores: `NUM_TO_STR` becomes `cpsc-function-num_to_str.7.md`. Keep closing and auxiliary keywords with the main construct; for example, IF includes THEN, ELSE and ENDIF.

## Writing

Write each prose paragraph on one source line. Never manually wrap prose to a column limit or split it into lines just because it looks long. man chooses line wrapping from the terminal width. Separate paragraphs with blank lines; preserve structural lines for headings, lists and definition lists, and meaningful line breaks/indentation inside syntax and example code blocks. Apply this rule to both `doc/cpsc.1.md` and every language manual.

Keep pages short: NAME, SYNOPSIS, DESCRIPTION, a small EXAMPLE and SEE ALSO. Describe actual compiler support and necessary limitations. Check `reference/697401-2026-pseudocode-guide-for-teachers.pdf` for official syntax; briefly note relevant differences instead of promising unsupported features.

Use relative Markdown links between source pages, including `../` when crossing categories. Keep the page name and section in the label, such as `cpsc-keyword-if(7)`. `cmake/ManualLinks.lua` renders these as man references or local HTML links.

## Build and verification

`doc`, `install` and `release` generate all manuals with Pandoc; ordinary builds do not need it. CMake discovers language sources recursively and preserves categories in generated files and release packages. Windows installs HTML with the same category layout. Unix installs all section 7 pages together in `PREFIX/share/cpsc/man/man7`, alongside the command page in `man1`; manpath registration is handled by the installer.

Before committing, run `cmake --build build --target doc`, check the affected links and render a changed page with `man -l build/doc/language/keyword/cpsc-keyword-if.7` (use the relevant path). For formatting-only edits, a comparison of Pandoc's man output before and after is sufficient. For path changes, also check HTML links, package contents and a DESTDIR installation. Keep verification simple; do not add a test framework for manual edits.
