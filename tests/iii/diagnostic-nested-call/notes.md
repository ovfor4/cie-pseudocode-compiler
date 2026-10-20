# diagnostic-nested-call

The compiler must reject this program: the standard-library MID function expects 3 arguments. The diagnostic must report the incorrect argument count on line 2; compilation must preserve existing output.
