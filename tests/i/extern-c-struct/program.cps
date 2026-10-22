// Records whose fields are C types, and pointer types to C types, pass to C functions.
INCLUDE_OBJ "lib.o"
TYPE Sample
    DECLARE Tag : C_CHAR
    DECLARE Value : C_DOUBLE
    DECLARE Count : C_INT
ENDTYPE
TYPE PSample = ^Sample
TYPE PPSample = ^PSample
TYPE PInt = ^C_INT
TYPE Counter
    DECLARE Target : PInt
ENDTYPE
TYPE PCounter = ^Counter
EXTERN "C" EXTERN_PROCEDURE scale_sample(PSample, C_INT)
EXTERN "C" EXTERN_FUNCTION shared_sample() RETURNS PSample
EXTERN "C" EXTERN_FUNCTION shared_count() RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION sum_counts(C_INT, ...) RETURNS C_INT
EXTERN "C" EXTERN_PROCEDURE bump(PInt)
EXTERN "C" EXTERN_PROCEDURE bump_counter(PCounter)
EXTERN "C" EXTERN_PROCEDURE get_shared(PPSample)
DECLARE S : Sample
DECLARE P : PSample
DECLARE Q : PSample
DECLARE R : PSample
DECLARE N : C_INT
DECLARE C : Counter
S.Tag <- (C_CHAR)'S'
S.Value <- (C_DOUBLE)1.5
S.Count <- (C_INT)5
CALL scale_sample(^S, (C_INT)3)
OUTPUT S.Tag, " ", S.Value, " ", S.Count
P <- shared_sample()
OUTPUT P^.Tag, " ", P^.Value, " ", P^.Count
P^.Count <- (C_INT)9
OUTPUT shared_count()
Q <- ^S
OUTPUT sum_counts((C_INT)2, P, Q)
N <- (C_INT)1
CALL bump(^N)
C.Target <- ^N
CALL bump_counter(^C)
OUTPUT N
CALL get_shared(^R)
OUTPUT R^.Tag, " ", R^.Count
OUTPUT sum_counts((C_INT)2, ^S, P)
