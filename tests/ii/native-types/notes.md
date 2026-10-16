# native-types

Project C_TYPE contract only. C++ is built with the same CMake toolchain and ABI as cpsc: native long width, plain-char signedness, integer promotions and unsigned wrap follow that platform. Signed out-of-range casts are compared to that native implementation; no signed arithmetic overflow is performed. No pseudocode INTEGER overflow or REAL output is involved.

Each record produces exactly one output line. Random records are preceded by fixed boundary records; every arithmetic intermediate is bounded.
