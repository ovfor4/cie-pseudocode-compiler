# Trace notes

Review status: pending human review.

A C++ string starts with 4096 x bytes and appends the three digits 123, giving 4099. The second EXTERN function catches invalid_argument, prints its message followed by a newline to stderr using fprintf, and rethrows it; pseudocode catches it once. This also exercises MinGW stdio locking during exception handling. These are C++ object and exception extensions.
