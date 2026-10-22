// Records whose fields are C types pass to C functions by value.
INCLUDE_OBJ "lib.o"
TYPE CShort = C_TYPE("short")
TYPE CFloat = C_TYPE("float")
TYPE CLongLong = C_TYPE("long long")
TYPE Pair
    DECLARE A : C_INT
    DECLARE B : C_INT
ENDTYPE
TYPE Vec2
    DECLARE X : C_DOUBLE
    DECLARE Y : C_DOUBLE
ENDTYPE
TYPE Mixed
    DECLARE Tag : C_CHAR
    DECLARE Value : C_DOUBLE
    DECLARE Count : C_INT
ENDTYPE
TYPE Vec3f
    DECLARE X : CFloat
    DECLARE Y : CFloat
    DECLARE Z : CFloat
ENDTYPE
TYPE Wide
    DECLARE Lo : CLongLong
    DECLARE Hi : CLongLong
ENDTYPE
EXTERN "C" EXTERN_FUNCTION pair_sum(Pair) RETURNS C_INT
EXTERN "C" EXTERN_FUNCTION vec2_dot(Vec2, Vec2) RETURNS C_DOUBLE
EXTERN "C" EXTERN_FUNCTION mixed_score(CShort, Mixed, C_INT) RETURNS C_DOUBLE
EXTERN "C" EXTERN_FUNCTION vec3f_sum(Vec3f) RETURNS C_DOUBLE
EXTERN "C" EXTERN_FUNCTION wide_after_five(C_INT, C_INT, C_INT, C_INT, C_INT, Wide) RETURNS CLongLong
DECLARE P : Pair
DECLARE U : Vec2
DECLARE V : Vec2
DECLARE M : Mixed
DECLARE F : Vec3f
DECLARE W : Wide
P.A <- (C_INT)3
P.B <- (C_INT)4
OUTPUT pair_sum(P)
U.X <- (C_DOUBLE)1.5
U.Y <- (C_DOUBLE)2
V.X <- (C_DOUBLE)4
V.Y <- (C_DOUBLE)0.25
OUTPUT vec2_dot(U, V)
M.Tag <- (C_CHAR)'M'
M.Value <- (C_DOUBLE)2.5
M.Count <- (C_INT)3
OUTPUT mixed_score((CShort)-1, M, (C_INT)4)
F.X <- (CFloat)0.5
F.Y <- (CFloat)1.25
F.Z <- (CFloat)2
OUTPUT vec3f_sum(F)
W.Lo <- (CLongLong)100
W.Hi <- (CLongLong)1000
OUTPUT wide_after_five((C_INT)1, (C_INT)2, (C_INT)3, (C_INT)4, (C_INT)5, W)
