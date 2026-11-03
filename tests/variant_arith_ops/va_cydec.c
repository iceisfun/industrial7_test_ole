/* VarCy* / VarDec* arithmetic on raw CY (int64 scaled by 10000) and DECIMAL operands. */
#include "va.h"

/* ---- CY ---- */
static CY cy(long long raw) { CY c; c.int64 = raw; return c; }
#define CYMIN I8_MIN
#define CYMAX I8_MAX

typedef HRESULT (WINAPI *cy2_t)(CY, CY, CY *);
typedef HRESULT (WINAPI *cy1_t)(CY, CY *);

static void cy2(int line, const char *name, cy2_t f, long long a, long long b, HRESULT hr, long long want)
{
    CY out; HRESULT got;
    out.int64 = 0x1234;
    got = f(cy(a), cy(b), &out);
    CHECKF(got == hr, "[line %d] %s(%lld,%lld) hr 0x%lx want 0x%lx", line, name, a, b, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK) CHECKF(out.int64 == want, "[line %d] %s(%lld,%lld) = %lld want %lld", line, name, a, b, out.int64, want);
}
static void cy1(int line, const char *name, cy1_t f, long long a, HRESULT hr, long long want)
{
    CY out; HRESULT got;
    out.int64 = 0x1234;
    got = f(cy(a), &out);
    CHECKF(got == hr, "[line %d] %s(%lld) hr 0x%lx want 0x%lx", line, name, a, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK) CHECKF(out.int64 == want, "[line %d] %s(%lld) = %lld want %lld", line, name, a, out.int64, want);
}
#define CY2(f, a, b, hr, want) cy2(__LINE__, #f, U(f), a, b, hr, want)
#define CY1(f, a, hr, want) cy1(__LINE__, #f, U(f), a, hr, want)

TEST(variant_arith_ops_cy_add_sub)
{
    REQUIRE(U(VarCyAdd)); REQUIRE(U(VarCySub));
    CY2(VarCyAdd, 5000, 5000, S_OK, 10000);
    CY2(VarCyAdd, 5000, -4000, S_OK, 1000);
    CY2(VarCyAdd, -5000, -5000, S_OK, -10000);
    CY2(VarCyAdd, 0, 0, S_OK, 0);
    CY2(VarCyAdd, 1, -1, S_OK, 0);
    CY2(VarCyAdd, CYMAX, 1, DISP_E_OVERFLOW, 0);
    CY2(VarCyAdd, 1, CYMAX, DISP_E_OVERFLOW, 0);
    CY2(VarCyAdd, CYMAX, CYMAX, DISP_E_OVERFLOW, 0);
    CY2(VarCyAdd, CYMIN, -1, DISP_E_OVERFLOW, 0);
    CY2(VarCyAdd, CYMIN, CYMIN, DISP_E_OVERFLOW, 0);
    CY2(VarCyAdd, 4611686018427387904LL, 4611686018427387904LL, DISP_E_OVERFLOW, 0);
    CY2(VarCySub, 5000, 5000, S_OK, 0);
    CY2(VarCySub, 5000, -4000, S_OK, 9000);
    CY2(VarCySub, 5000, -6000, S_OK, 11000);
    CY2(VarCySub, -5000, -5000, S_OK, 0);
    CY2(VarCySub, 0, 5000, S_OK, -5000);
    CY2(VarCySub, CYMAX, -1, DISP_E_OVERFLOW, 0);
    CY2(VarCySub, CYMIN, 1, DISP_E_OVERFLOW, 0);
    CY2(VarCySub, 0, CYMIN, DISP_E_OVERFLOW, 0);
    CY2(VarCySub, CYMIN, CYMAX, DISP_E_OVERFLOW, 0);
    CY2(VarCySub, CYMAX, CYMIN, DISP_E_OVERFLOW, 0);
}

TEST(variant_arith_ops_cy_mul)
{
    REQUIRE(U(VarCyMul)); REQUIRE(U(VarCyMulI4)); REQUIRE(U(VarCyMulI8));
    CY2(VarCyMul, 10000, 10000, S_OK, 10000);          /* 1.0 * 1.0 */
    CY2(VarCyMul, 10000, 12345, S_OK, 12345);
    CY2(VarCyMul, 12345, 10000, S_OK, 12345);
    CY2(VarCyMul, 20000, 12345, S_OK, 24690);
    CY2(VarCyMul, -20000, 12345, S_OK, -24690);
    CY2(VarCyMul, -20000, -12345, S_OK, 24690);
    CY2(VarCyMul, 5000, 5000, S_OK, 2500);
    CY2(VarCyMul, 5000, -4000, S_OK, -2000);
    CY2(VarCyMul, 5000, -6000, S_OK, -3000);
    CY2(VarCyMul, 5340000000LL, 0, S_OK, 0);
    CY2(VarCyMul, 0, 5340000000LL, S_OK, 0);
    CY2(VarCyMul, 1, 1, S_OK, 0);                       /* 0.0001 * 0.0001 underflows to 0 */
    CY2(VarCyMul, 100000000000000LL, 10000, S_OK, 100000000000000LL);
    CY2(VarCyMul, CYMAX, 20000, DISP_E_OVERFLOW, 0);
    CY2(VarCyMul, CYMIN, 20000, DISP_E_OVERFLOW, 0);
    CY2(VarCyMul, CYMIN, -10000, DISP_E_OVERFLOW, 0);
    CY2(VarCyMul, 9223372036854760000LL, 20000, DISP_E_OVERFLOW, 0);
    CY2(VarCyMul, 5000000000000000LL, 5000000000000000LL, DISP_E_OVERFLOW, 0);
}

static void mulint(int line, int is8, long long a, long long m, HRESULT hr, long long want)
{
    CY out; HRESULT got;
    out.int64 = 0x1234;
    got = is8 ? U(VarCyMulI8)(cy(a), m, &out) : U(VarCyMulI4)(cy(a), (LONG)m, &out);
    CHECKF(got == hr, "[line %d] VarCyMulI%d(%lld,%lld) hr 0x%lx want 0x%lx", line, is8 ? 8 : 4, a, m, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK) CHECKF(out.int64 == want, "[line %d] VarCyMulI%d(%lld,%lld) = %lld want %lld", line, is8 ? 8 : 4, a, m, out.int64, want);
}
#define MI(is8, a, m, hr, want) mulint(__LINE__, is8, a, m, hr, want)

TEST(variant_arith_ops_cy_muli)
{
    int k;
    REQUIRE(U(VarCyMulI4)); REQUIRE(U(VarCyMulI8));
    for (k = 0; k < 2; k++) {
        MI(k, 5340000000LL, 0, S_OK, 0);
        MI(k, 5000, 1, S_OK, 5000);
        MI(k, 5000, 2, S_OK, 10000);
        MI(k, 5000, -2, S_OK, -10000);
        MI(k, -5000, -2, S_OK, 10000);
        MI(k, 0, 12345, S_OK, 0);
        MI(k, 1, 1000000, S_OK, 1000000);
        MI(k, CYMAX, 2, DISP_E_OVERFLOW, 0);
        MI(k, CYMIN, 2, DISP_E_OVERFLOW, 0);
        MI(k, CYMIN, -1, DISP_E_OVERFLOW, 0);
        MI(k, 9223372036854760000LL, 2, DISP_E_OVERFLOW, 0);
        MI(k, 4611686018427387904LL, 2, DISP_E_OVERFLOW, 0);
    }
    /* 64-bit multiplier */
    MI(1, 1, 0x100000000LL, S_OK, 0x100000000LL);
    MI(1, 1000000, 10000000000000LL, DISP_E_OVERFLOW, 0);
}

TEST(variant_arith_ops_cy_unary)
{
    REQUIRE(U(VarCyAbs)); REQUIRE(U(VarCyNeg)); REQUIRE(U(VarCyFix)); REQUIRE(U(VarCyInt));
    CY1(VarCyAbs, 5000, S_OK, 5000);
    CY1(VarCyAbs, -5000, S_OK, 5000);
    CY1(VarCyAbs, 0, S_OK, 0);
    CY1(VarCyAbs, -1, S_OK, 1);
    CY1(VarCyNeg, 5000, S_OK, -5000);
    CY1(VarCyNeg, -5000, S_OK, 5000);
    CY1(VarCyNeg, 0, S_OK, 0);
    CY1(VarCyFix, 0, S_OK, 0);
    CY1(VarCyFix, 9999, S_OK, 0);
    CY1(VarCyFix, 10000, S_OK, 10000);
    CY1(VarCyFix, 19999, S_OK, 10000);
    CY1(VarCyFix, -1, S_OK, 0);
    CY1(VarCyFix, -9999, S_OK, 0);
    CY1(VarCyFix, -10000, S_OK, -10000);
    CY1(VarCyFix, -15000, S_OK, -10000);
    CY1(VarCyFix, -19999, S_OK, -10000);
    CY1(VarCyFix, 123456789, S_OK, 123450000);
    CY1(VarCyFix, -123456789, S_OK, -123450000);
    CY1(VarCyInt, 0, S_OK, 0);
    CY1(VarCyInt, 1, S_OK, 0);
    CY1(VarCyInt, 9999, S_OK, 0);
    CY1(VarCyInt, 10000, S_OK, 10000);
    CY1(VarCyInt, 19999, S_OK, 10000);
    CY1(VarCyInt, -1, S_OK, -10000);
    CY1(VarCyInt, -9999, S_OK, -10000);
    CY1(VarCyInt, -10000, S_OK, -10000);
    CY1(VarCyInt, -10001, S_OK, -20000);
    CY1(VarCyInt, -15000, S_OK, -20000);
    CY1(VarCyInt, 123456789, S_OK, 123450000);
    CY1(VarCyInt, -123456789, S_OK, -123460000);
}

static void cyround(int line, long long a, int nd, HRESULT hr, long long want)
{
    CY out; HRESULT got;
    out.int64 = 0x1234;
    got = U(VarCyRound)(cy(a), nd, &out);
    CHECKF(got == hr, "[line %d] VarCyRound(%lld,%d) hr 0x%lx want 0x%lx", line, a, nd, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK) CHECKF(out.int64 == want, "[line %d] VarCyRound(%lld,%d) = %lld want %lld", line, a, nd, out.int64, want);
}
#define CYR(a, nd, hr, want) cyround(__LINE__, a, nd, hr, want)

TEST(variant_arith_ops_cy_round)
{
    REQUIRE(U(VarCyRound));
    CYR(5432, 5, S_OK, 5432);
    CYR(5432, 4, S_OK, 5432);
    CYR(5432, 3, S_OK, 5430);
    CYR(5432, 2, S_OK, 5400);
    CYR(5432, 1, S_OK, 5000);
    CYR(5532, 0, S_OK, 10000);
    CYR(5568, 3, S_OK, 5570);
    CYR(5568, 2, S_OK, 5600);
    CYR(5568, 1, S_OK, 6000);
    CYR(5568, 0, S_OK, 10000);
    CYR(4999, 0, S_OK, 0);
    CYR(5001, 0, S_OK, 10000);
    CYR(14999, 0, S_OK, 10000);
    CYR(15001, 0, S_OK, 20000);
    CYR(5000, 0, S_OK, 0);                              /* ties round to even */
    CYR(15000, 0, S_OK, 20000);
    CYR(-5432, 3, S_OK, -5430);
    CYR(-5568, 1, S_OK, -6000);
    CYR(-5532, 0, S_OK, -10000);
    CYR(-4999, 0, S_OK, 0);
    CYR(0, 0, S_OK, 0);
    CYR(0, 4, S_OK, 0);
    CYR(123456789, 100, S_OK, 123456789);
    CYR(5432, -1, E_INVALIDARG, 0);
    CYR(5432, -2, E_INVALIDARG, 0);
    CYR(0, -1, E_INVALIDARG, 0);
    CYR(5432, I4_MIN, E_INVALIDARG, 0);
}

static HRESULT cycmp(long long a, long long b) { return U(VarCyCmp)(cy(a), cy(b)); }
TEST(variant_arith_ops_cy_cmp)
{
    static const long long v[] = { -1000000000000LL, -999999999999LL, -10000, -1, 0, 1, 10000, 999999999999LL, 1000000000000LL };
    size_t i, j;
    REQUIRE(U(VarCyCmp)); REQUIRE(U(VarCyCmpR8)); REQUIRE(U(VarCyFromR8));
    for (i = 0; i < sizeof(v)/sizeof(v[0]); i++)
        for (j = 0; j < sizeof(v)/sizeof(v[0]); j++) {
            HRESULT want = i < j ? VARCMP_LT : i == j ? VARCMP_EQ : VARCMP_GT;
            HRESULT got = cycmp(v[i], v[j]);
            CHECKF(got == want, "VarCyCmp(%lld,%lld) = %ld want %ld", v[i], v[j], (long)got, (long)want);
        }
    {
        static const struct { long long a; double b; HRESULT want; } r[] = {
            { 10000, 1.0, VARCMP_EQ }, { 10000, 0.0, VARCMP_GT }, { 10000, 2.0, VARCMP_LT }, { -10000, -1.0, VARCMP_EQ },
            { -10000, 0.0, VARCMP_LT }, { 0, 0.0, VARCMP_EQ }, { 0, -1.0, VARCMP_GT }, { 15000, 1.5, VARCMP_EQ },
            { 15000, 1.25, VARCMP_GT }, { 15000, 1.75, VARCMP_LT }, { 1, 0.0, VARCMP_GT },
        };
        for (i = 0; i < sizeof(r)/sizeof(r[0]); i++) {
            HRESULT got = U(VarCyCmpR8)(cy(r[i].a), r[i].b);
            CHECKF(got == r[i].want, "VarCyCmpR8(%lld,%g) = %ld want %ld", r[i].a, r[i].b, (long)got, (long)r[i].want);
        }
    }
}

/* ---- DECIMAL ---- */
static DECIMAL mkdec(int scale, int sign, ULONG hi, ULONGLONG lo)
{
    DECIMAL d; memset(&d, 0, sizeof(d));
    d.scale = (BYTE)scale; d.sign = (BYTE)sign; d.Hi32 = hi; d.Lo64 = lo;
    return d;
}
/* exact field comparison, the sign of a zero is not significant */
static int deceq(const DECIMAL *a, const DECIMAL *b)
{
    return a->scale == b->scale && a->Hi32 == b->Hi32 && a->Lo64 == b->Lo64 && (a->sign == b->sign || (a->Hi32 == 0 && a->Lo64 == 0));
}
/* equal in value (any scale / zero sign) */
static int decval(const DECIMAL *a, const DECIMAL *b) { return U(VarDecCmp)((DECIMAL *)a, (DECIMAL *)b) == VARCMP_EQ; }

typedef HRESULT (WINAPI *dec2_t)(LPDECIMAL, LPDECIMAL, LPDECIMAL);
typedef HRESULT (WINAPI *dec1_t)(LPDECIMAL, LPDECIMAL);

static void dec2(int line, const char *name, dec2_t f, DECIMAL a, DECIMAL b, HRESULT hr, DECIMAL want, int exact)
{
    DECIMAL out; HRESULT got;
    memset(&out, 0xcc, sizeof(out));
    got = f(&a, &b, &out);
    CHECKF(got == hr, "[line %d] %s hr 0x%lx want 0x%lx", line, name, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK)
        CHECKF(exact ? deceq(&out, &want) : decval(&out, &want), "[line %d] %s got (scale %d sign 0x%x hi %lu lo %llu) want (scale %d sign 0x%x hi %lu lo %llu)", line, name,
               out.scale, out.sign, out.Hi32, out.Lo64, want.scale, want.sign, want.Hi32, want.Lo64);
}
static void dec1(int line, const char *name, dec1_t f, DECIMAL a, HRESULT hr, DECIMAL want, int exact)
{
    DECIMAL out; HRESULT got;
    memset(&out, 0xcc, sizeof(out));
    got = f(&a, &out);
    CHECKF(got == hr, "[line %d] %s hr 0x%lx want 0x%lx", line, name, (long)got, (long)hr);
    if (hr == S_OK && got == S_OK)
        CHECKF(exact ? deceq(&out, &want) : decval(&out, &want), "[line %d] %s got (scale %d sign 0x%x hi %lu lo %llu) want (scale %d sign 0x%x hi %lu lo %llu)", line, name,
               out.scale, out.sign, out.Hi32, out.Lo64, want.scale, want.sign, want.Hi32, want.Lo64);
}
#define D2(f, a, b, hr, want, ex) dec2(__LINE__, #f, U(f), a, b, hr, want, ex)
#define D1(f, a, hr, want, ex) dec1(__LINE__, #f, U(f), a, hr, want, ex)
#define DEC mkdec
#define NEG 0x80

TEST(variant_arith_ops_dec_add_sub)
{
    REQUIRE(U(VarDecAdd)); REQUIRE(U(VarDecSub)); REQUIRE(U(VarDecCmp));
    /* results carry the larger scale of the operands */
    D2(VarDecAdd, DEC(1, 0, 0, 15), DEC(2, 0, 0, 225), S_OK, DEC(2, 0, 0, 375), 1);      /* 1.5 + 2.25 = 3.75 */
    D2(VarDecAdd, DEC(1, 0, 0, 1), DEC(1, 0, 0, 2), S_OK, DEC(1, 0, 0, 3), 1);          /* 0.1 + 0.2 */
    D2(VarDecAdd, DEC(0, 0, 0, 7), DEC(0, 0, 0, 5), S_OK, DEC(0, 0, 0, 12), 1);
    D2(VarDecAdd, DEC(0, NEG, 0, 7), DEC(0, 0, 0, 5), S_OK, DEC(0, NEG, 0, 2), 1);
    D2(VarDecAdd, DEC(0, 0, 0, 7), DEC(0, NEG, 0, 5), S_OK, DEC(0, 0, 0, 2), 1);
    D2(VarDecAdd, DEC(0, NEG, 0, 7), DEC(0, NEG, 0, 5), S_OK, DEC(0, NEG, 0, 12), 1);
    D2(VarDecAdd, DEC(3, 0, 0, 1500), DEC(0, 0, 0, 1), S_OK, DEC(3, 0, 0, 2500), 1);    /* 1.500 + 1 */
    D2(VarDecAdd, DEC(0, 0, 0, 0xffffffffULL), DEC(0, 0, 0, 1), S_OK, DEC(0, 0, 0, 0x100000000ULL), 1);
    D2(VarDecAdd, DEC(0, 0, 0, 0xffffffffffffffffULL), DEC(0, 0, 0, 1), S_OK, DEC(0, 0, 1, 0), 1);
    D2(VarDecAdd, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, 0, 0, 1), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecAdd, DEC(0, NEG, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, NEG, 0, 1), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecAdd, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, NEG, 0, 1), S_OK, DEC(0, 0, 0xffffffffu, 0xfffffffffffffffeULL), 1);
    D2(VarDecAdd, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, NEG, 0xffffffffu, 0xffffffffffffffffULL), S_OK, DEC(0, 0, 0, 0), 0);
    D2(VarDecSub, DEC(0, 0, 0, 7), DEC(0, 0, 0, 5), S_OK, DEC(0, 0, 0, 2), 1);
    D2(VarDecSub, DEC(0, 0, 0, 5), DEC(0, 0, 0, 7), S_OK, DEC(0, NEG, 0, 2), 1);
    D2(VarDecSub, DEC(0, 0, 0, 5), DEC(0, NEG, 0, 7), S_OK, DEC(0, 0, 0, 12), 1);
    D2(VarDecSub, DEC(0, NEG, 0, 5), DEC(0, 0, 0, 7), S_OK, DEC(0, NEG, 0, 12), 1);
    D2(VarDecSub, DEC(2, 0, 0, 150), DEC(2, 0, 0, 25), S_OK, DEC(2, 0, 0, 125), 1);      /* 1.50 - 0.25 */
    D2(VarDecSub, DEC(1, 0, 0, 15), DEC(0, 0, 0, 2), S_OK, DEC(1, NEG, 0, 5), 1);        /* 1.5 - 2 = -0.5 */
    D2(VarDecSub, DEC(0, 0, 0, 9), DEC(0, 0, 0, 9), S_OK, DEC(0, 0, 0, 0), 0);
    D2(VarDecSub, DEC(0, NEG, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, 0, 0, 1), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecSub, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, NEG, 0, 1), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
}

TEST(variant_arith_ops_dec_mul_div)
{
    REQUIRE(U(VarDecMul)); REQUIRE(U(VarDecDiv)); REQUIRE(U(VarDecCmp));
    D2(VarDecMul, DEC(0, 0, 0, 6), DEC(0, 0, 0, 7), S_OK, DEC(0, 0, 0, 42), 1);
    D2(VarDecMul, DEC(0, NEG, 0, 6), DEC(0, 0, 0, 7), S_OK, DEC(0, NEG, 0, 42), 1);
    D2(VarDecMul, DEC(0, 0, 0, 6), DEC(0, NEG, 0, 7), S_OK, DEC(0, NEG, 0, 42), 1);
    D2(VarDecMul, DEC(0, NEG, 0, 6), DEC(0, NEG, 0, 7), S_OK, DEC(0, 0, 0, 42), 1);
    D2(VarDecMul, DEC(1, 0, 0, 15), DEC(0, 0, 0, 2), S_OK, DEC(1, 0, 0, 30), 1);         /* 1.5 * 2 = 3.0 */
    D2(VarDecMul, DEC(1, 0, 0, 15), DEC(1, 0, 0, 15), S_OK, DEC(2, 0, 0, 225), 1);       /* scales add */
    D2(VarDecMul, DEC(2, 0, 0, 5), DEC(3, 0, 0, 2), S_OK, DEC(5, 0, 0, 10), 1);
    D2(VarDecMul, DEC(0, 0, 0, 0), DEC(0, 0, 0, 12345), S_OK, DEC(0, 0, 0, 0), 0);
    D2(VarDecMul, DEC(0, 0, 0, 0xffffffffffffffffULL), DEC(0, 0, 0, 2), S_OK, DEC(0, 0, 1, 0xfffffffffffffffeULL), 1);
    D2(VarDecMul, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, 0, 0, 2), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecMul, DEC(0, 0, 0x80000000u, 0), DEC(0, 0, 0, 2), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecMul, DEC(0, NEG, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, 0, 0, 2), DISP_E_OVERFLOW, DEC(0, 0, 0, 0), 1);
    D2(VarDecMul, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), DEC(0, 0, 0, 1), S_OK, DEC(0, 0, 0xffffffffu, 0xffffffffffffffffULL), 1);

    /* exact divisions */
    D2(VarDecDiv, DEC(0, 0, 0, 45), DEC(0, 0, 0, 9), S_OK, DEC(0, 0, 0, 5), 1);
    D2(VarDecDiv, DEC(0, NEG, 0, 45), DEC(0, 0, 0, 9), S_OK, DEC(0, NEG, 0, 5), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 45), DEC(0, NEG, 0, 9), S_OK, DEC(0, NEG, 0, 5), 1);
    D2(VarDecDiv, DEC(0, NEG, 0, 45), DEC(0, NEG, 0, 9), S_OK, DEC(0, 0, 0, 5), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 0), DEC(0, 0, 0, 1), S_OK, DEC(0, 0, 0, 0), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 1), DEC(0, 0, 0, 1), S_OK, DEC(0, 0, 0, 1), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 100), DEC(0, 0, 0, 4), S_OK, DEC(0, 0, 0, 25), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 7), DEC(0, 0, 0, 2), S_OK, DEC(1, 0, 0, 35), 0);          /* 7 / 2 = 3.5 (value) */
    D2(VarDecDiv, DEC(0, 0, 0, 1), DEC(0, 0, 0, 8), S_OK, DEC(3, 0, 0, 125), 0);         /* 0.125 */
    D2(VarDecDiv, DEC(1, 0, 0, 15), DEC(0, 0, 0, 3), S_OK, DEC(1, 0, 0, 5), 0);          /* 1.5 / 3 = 0.5 */
    /* inexact quotients are rounded to 28 digits of scale */
    D2(VarDecDiv, DEC(0, 0, 0, 1), DEC(0, 0, 0, 3), S_OK, DEC(28, 0, 180700362, 0x14b700cb05555555ULL), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 0), DEC(0, 0, 0, 0), DISP_E_DIVBYZERO, DEC(0, 0, 0, 0), 1);
    D2(VarDecDiv, DEC(0, 0, 0, 1), DEC(0, 0, 0, 0), DISP_E_DIVBYZERO, DEC(0, 0, 0, 0), 1);
    D2(VarDecDiv, DEC(2, 0, 0, 100), DEC(5, 0, 0, 0), DISP_E_DIVBYZERO, DEC(0, 0, 0, 0), 1);
    D2(VarDecDiv, DEC(0, NEG, 0, 1), DEC(0, 0, 0, 0), DISP_E_DIVBYZERO, DEC(0, 0, 0, 0), 1);
}

TEST(variant_arith_ops_dec_unary)
{
    REQUIRE(U(VarDecAbs)); REQUIRE(U(VarDecNeg)); REQUIRE(U(VarDecFix)); REQUIRE(U(VarDecInt)); REQUIRE(U(VarDecCmp));
    D1(VarDecAbs, DEC(0, NEG, 0, 5), S_OK, DEC(0, 0, 0, 5), 1);
    D1(VarDecAbs, DEC(3, NEG, 7, 12345), S_OK, DEC(3, 0, 7, 12345), 1);
    D1(VarDecAbs, DEC(3, 0, 7, 12345), S_OK, DEC(3, 0, 7, 12345), 1);
    D1(VarDecNeg, DEC(0, 0, 0, 5), S_OK, DEC(0, NEG, 0, 5), 1);
    D1(VarDecNeg, DEC(0, NEG, 0, 5), S_OK, DEC(0, 0, 0, 5), 1);
    D1(VarDecNeg, DEC(5, 0, 0xffffffffu, 0xffffffffffffffffULL), S_OK, DEC(5, NEG, 0xffffffffu, 0xffffffffffffffffULL), 1);
    D1(VarDecNeg, DEC(0, 0, 0, 0), S_OK, DEC(0, NEG, 0, 0), 1);
    /* Fix truncates toward zero, Int floors */
    D1(VarDecFix, DEC(1, 0, 0, 15), S_OK, DEC(0, 0, 0, 1), 0);
    D1(VarDecFix, DEC(1, NEG, 0, 15), S_OK, DEC(0, NEG, 0, 1), 0);
    D1(VarDecFix, DEC(2, 0, 0, 99), S_OK, DEC(0, 0, 0, 0), 0);
    D1(VarDecFix, DEC(2, NEG, 0, 99), S_OK, DEC(0, 0, 0, 0), 0);
    D1(VarDecFix, DEC(0, NEG, 0, 7), S_OK, DEC(0, NEG, 0, 7), 0);
    D1(VarDecFix, DEC(3, 0, 0, 12999), S_OK, DEC(0, 0, 0, 12), 0);
    D1(VarDecInt, DEC(1, 0, 0, 15), S_OK, DEC(0, 0, 0, 1), 0);
    D1(VarDecInt, DEC(1, NEG, 0, 15), S_OK, DEC(0, NEG, 0, 2), 0);
    D1(VarDecInt, DEC(2, NEG, 0, 1), S_OK, DEC(0, NEG, 0, 1), 0);
    D1(VarDecInt, DEC(2, 0, 0, 99), S_OK, DEC(0, 0, 0, 0), 0);
    D1(VarDecInt, DEC(0, NEG, 0, 7), S_OK, DEC(0, NEG, 0, 7), 0);
    D1(VarDecInt, DEC(1, NEG, 0, 10), S_OK, DEC(0, NEG, 0, 1), 0);
    D1(VarDecInt, DEC(3, NEG, 0, 12001), S_OK, DEC(0, NEG, 0, 13), 0);
}

TEST(variant_arith_ops_dec_cmp)
{
    DECIMAL a, b;
    REQUIRE(U(VarDecCmp)); REQUIRE(U(VarDecCmpR8));
    /* scale-independent ordering */
    a = mkdec(2, 0, 0, 150); b = mkdec(1, 0, 0, 15);  CHECK(U(VarDecCmp)(&a, &b) == VARCMP_EQ);
    a = mkdec(2, 0, 0, 151); b = mkdec(1, 0, 0, 15);  CHECK(U(VarDecCmp)(&a, &b) == VARCMP_GT);
    a = mkdec(2, 0, 0, 149); b = mkdec(1, 0, 0, 15);  CHECK(U(VarDecCmp)(&a, &b) == VARCMP_LT);
    a = mkdec(28, 0, 0, 1); b = mkdec(0, 0, 0, 0);    CHECK(U(VarDecCmp)(&a, &b) == VARCMP_GT);
    a = mkdec(28, NEG, 0, 1); b = mkdec(0, 0, 0, 0);  CHECK(U(VarDecCmp)(&a, &b) == VARCMP_LT);
    a = mkdec(28, NEG, 0, 1); b = mkdec(28, 0, 0, 1); CHECK(U(VarDecCmp)(&a, &b) == VARCMP_LT);
    a = mkdec(1, NEG, 0, 15); b = mkdec(1, NEG, 0, 14); CHECK(U(VarDecCmp)(&a, &b) == VARCMP_LT);
    a = mkdec(0, 0, 0, 0xffffffffffffffffULL); b = mkdec(0, 0, 1, 0); CHECK(U(VarDecCmp)(&a, &b) == VARCMP_LT);
    a = mkdec(0, NEG, 0, 0); b = mkdec(0, 0, 0, 0);   CHECK(U(VarDecCmp)(&a, &b) == VARCMP_EQ);
    CHECK(U(VarDecCmpR8)(&a, 0.0) == VARCMP_EQ);
    a = mkdec(1, 0, 0, 15);
    CHECK(U(VarDecCmpR8)(&a, 1.5) == VARCMP_EQ);
    CHECK(U(VarDecCmpR8)(&a, 1.0) == VARCMP_GT);
    CHECK(U(VarDecCmpR8)(&a, 2.0) == VARCMP_LT);
    CHECK(U(VarDecCmpR8)(&a, -2.0) == VARCMP_GT);
    a = mkdec(1, NEG, 0, 15);
    CHECK(U(VarDecCmpR8)(&a, -1.5) == VARCMP_EQ);
    CHECK(U(VarDecCmpR8)(&a, -1.0) == VARCMP_LT);
    CHECK(U(VarDecCmpR8)(&a, -2.0) == VARCMP_GT);
    CHECK(U(VarDecCmpR8)(&a, 0.0) == VARCMP_LT);
}

typedef HRESULT (WINAPI *fromr8_t)(double, LPDECIMAL);
typedef HRESULT (WINAPI *fromr4_t)(float, LPDECIMAL);
TEST(variant_arith_ops_dec_from_real)
{
    static const struct { double v; int scale, sign; ULONG hi; ULONGLONG lo; } t[] = {
        { 0.0, 0, 0, 0, 0 }, { 1.0, 0, 0, 0, 1 }, { -1.0, 0, 0x80, 0, 1 }, { 0.5, 1, 0, 0, 5 }, { -0.5, 1, 0x80, 0, 5 },
        { 0.4, 1, 0, 0, 4 }, { -0.6, 1, 0x80, 0, 6 }, { 0.6, 1, 0, 0, 6 }, { 0.1, 1, 0, 0, 1 }, { 1.5, 1, 0, 0, 15 },
        { 123.456, 3, 0, 0, 123456 }, { -2.25, 2, 0x80, 0, 225 }, { 100.0, 0, 0, 0, 100 }, { 1e10, 0, 0, 0, 10000000000ULL },
        { 0.001, 3, 0, 0, 1 }, { 0.0001, 4, 0, 0, 1 },
    };
    size_t i;
    REQUIRE(U(VarDecFromR8)); REQUIRE(U(VarDecFromR4));
    for (i = 0; i < sizeof(t)/sizeof(t[0]); i++) {
        DECIMAL out, want = mkdec(t[i].scale, t[i].sign, t[i].hi, t[i].lo);
        HRESULT hr;
        memset(&out, 0xcc, sizeof(out));
        hr = U(VarDecFromR8)(t[i].v, &out);
        CHECKF(hr == S_OK, "VarDecFromR8(%g) hr 0x%lx", t[i].v, (long)hr);
        if (hr == S_OK) CHECKF(deceq(&out, &want), "VarDecFromR8(%.17g) = (scale %d sign 0x%x hi %lu lo %llu) want (scale %d sign 0x%x lo %llu)", t[i].v, out.scale, out.sign, out.Hi32, out.Lo64, want.scale, want.sign, want.Lo64);
        if (fabs(t[i].v) < 1000 && (double)(float)t[i].v == t[i].v) {
            memset(&out, 0xcc, sizeof(out));
            hr = U(VarDecFromR4)((float)t[i].v, &out);
            CHECKF(hr == S_OK, "VarDecFromR4(%g) hr 0x%lx", t[i].v, (long)hr);
            if (hr == S_OK) CHECKF(deceq(&out, &want), "VarDecFromR4(%.9g) = (scale %d sign 0x%x hi %lu lo %llu) want (scale %d sign 0x%x lo %llu)", (float)t[i].v, out.scale, out.sign, out.Hi32, out.Lo64, want.scale, want.sign, want.Lo64);
        }
    }
    /* out of range magnitudes overflow */
    { DECIMAL out; HRESULT hr;
      hr = U(VarDecFromR8)(1e30, &out); CHECKF(hr == DISP_E_OVERFLOW, "VarDecFromR8(1e30) hr 0x%lx want OVERFLOW", (long)hr);
      hr = U(VarDecFromR8)(-1e30, &out); CHECKF(hr == DISP_E_OVERFLOW, "VarDecFromR8(-1e30) hr 0x%lx want OVERFLOW", (long)hr);
      hr = U(VarDecFromR8)(R8_MAX, &out); CHECKF(hr == DISP_E_OVERFLOW, "VarDecFromR8(R8_MAX) hr 0x%lx want OVERFLOW", (long)hr);
      hr = U(VarDecFromR4)(R4_MAX, &out); CHECKF(hr == DISP_E_OVERFLOW, "VarDecFromR4(R4_MAX) hr 0x%lx want OVERFLOW", (long)hr); }
}
