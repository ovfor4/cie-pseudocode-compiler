# extern-cc-object

Human review: pending. Expected output was independently traced by AI, not captured from cpsc.

Basis: compiler extension (EXTERN "C", INCLUDE_OBJ of a user object); `lib.cc` is built with the host C++ compiler, exports unmangled `extern "C"` symbols and avoids the C++ runtime library (JIT does not load libstdc++; native would).

Trace: a C++ template object starts at 100. cc_sum()=100; cc_add(5)=105; cc_add(7)=112; cc_sum()=112. cc_text_length("abc") counts 3 characters plus 2, giving 5. cc_say calls the C library puts, linked through INCLUDE_OBJ "__libc".

