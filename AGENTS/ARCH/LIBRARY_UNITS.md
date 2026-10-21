# Library units and standard library

**Library units.** Line-leading `#LIB` marks SourceUnit::IsLibrary. RuntimeCheck uses the externally visible i32 `__cps_user_line`; FunctionGen’s shared call hook updates it only on non-library to library calls. Runtime helper line arguments use RuntimeCheck::lineValue too. Compiler diagnostics retain the owning file location.

**Standard library.** Build-time embedding includes pseudocode-lib/std.cps, its imported string.cps and the matching string.o. StandardLibrary extracts these for parsing and linking; user units import std.cps by default. -no-std disables automatic loading/linking. STRING is an ordinary class; StringValueGen bridges literals, C_STRING conversions and I/O. String functions, concatenation and comparisons use normal function/operator resolution. Installed/released cpsc needs no separate library directory; manually linking saved IR still requires the matching string.o and C++ runtime.
