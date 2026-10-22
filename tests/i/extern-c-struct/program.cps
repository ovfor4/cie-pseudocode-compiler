// Records whose fields are C types pass to C functions through pointer types.
INCLUDE_OBJ "lib.o"
TYPE Sample
    DECLARE Tag : C_CHAR
    DECLARE Value : C_DOUBLE
    DECLARE Count : C_INT
ENDTYPE
TYPE PSample = ^Sample
EXTERN "C" EXTERN_PROCEDURE scale_sample(PSample, C_INT)
EXTERN "C" EXTERN_FUNCTION shared_sample() RETURNS PSample
EXTERN "C" EXTERN_FUNCTION shared_count() RETURNS C_INT
DECLARE S : Sample
DECLARE P : PSample
S.Tag <- (C_CHAR)'S'
S.Value <- (C_DOUBLE)1.5
S.Count <- (C_INT)5
CALL scale_sample(^S, (C_INT)3)
OUTPUT S.Tag, " ", S.Value, " ", S.Count
P <- shared_sample()
OUTPUT P^.Tag, " ", P^.Value, " ", P^.Count
P^.Count <- (C_INT)9
OUTPUT shared_count()
