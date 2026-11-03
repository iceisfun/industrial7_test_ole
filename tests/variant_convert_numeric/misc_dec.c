/* DECIMAL specifics: VarDecFromR8 (15 significant digits, trailing zeros stripped, scale/sign bytes),
 * VarDecFromR4 (7 digits), invalid-DECIMAL rejection, native quirks.  Rules from the Win10 decompilation. */
#include "vcn.h"

static int deq(const DECIMAL *d, int sc, int sg, ULONG hi, ULONG mid, ULONG lo)
{
    return d->scale == sc && d->sign == sg && d->Hi32 == hi && d->Mid32 == mid && d->Lo32 == lo;
}
#define DEC_CHECK(call_hr, d, sc, sg, hi, mid, lo, what) \
    CHECKF((call_hr) == S_OK && deq(&(d), sc, sg, hi, mid, lo), "%s: hr=0x%lx got sc=%u sg=%02x %08lx:%08lx:%08lx", what, \
           (unsigned long)(call_hr), (d).scale, (d).sign, (unsigned long)(d).Hi32, (unsigned long)(d).Mid32, (unsigned long)(d).Lo32)

/* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarDecFromR8 (LGPL-2.1+); the long values come from the decomp */
TEST(vcn_decfromr8_digit_rules)
{
    REQUIRE(U(VarDecFromR8));
    static const struct { double x; int sc, sg; ULONG hi, mid, lo; } t[] = {
        { 0.5, 1, 0, 0, 0, 5 }, { -0.5, 1, 0x80, 0, 0, 5 }, { 1.0, 0, 0, 0, 0, 1 }, { -1.0, 0, 0x80, 0, 0, 1 },
        { 100.0, 0, 0, 0, 0, 100 }, { 0.1, 1, 0, 0, 0, 1 }, { -0.1, 1, 0x80, 0, 0, 1 }, { 123.456, 3, 0, 0, 0, 123456 },
        { 1.0 / 3.0, 15, 0, 0, 77610, 921490773 },            /* 15 significant digits: 0.333333333333333 */
        { 2.0 / 3.0, 15, 0, 0, 155220, 1842981547 },          /* 0.666666666666667 */
        { -2.5, 1, 0x80, 0, 0, 25 }, { 4.35, 2, 0, 0, 0, 435 }, { 0.30000000000000004, 1, 0, 0, 0, 3 },
        { 0.9999999999999999, 1, 0, 0, 0, 10 },                /* rounds up to 1.00000000000000, zeros stripped only while scale>0 */
        { 99999.99999999999, 0, 0, 0, 0, 100000 },
        { 1e14, 0, 0, 0, 23283, 276447232 }, { 1e15, 0, 0, 0, 232830, 2764472320UL },
        { 1e16, 0, 0, 0, 2328306, 1874919424 }, { 1e20, 0, 0, 5, 1808227885UL, 1661992960UL },
        { 1e28, 0, 0, 542101086, 1042612833UL, 268435456 },
        { 1e-28, 28, 0, 0, 0, 1 }, { 12345678.9012345678, 7, 0, 0, 28744, 2249056122UL },
        { 123456789012345.0, 0, 0, 0, 28744, 2249056121UL },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        DECIMAL d; memset(&d, 0xCC, sizeof d);
        HRESULT hr = U(VarDecFromR8)(t[i].x, &d);
        char what[64]; snprintf(what, sizeof what, "VarDecFromR8(%.17g)", t[i].x);
        DEC_CHECK(hr, d, t[i].sc, t[i].sg, t[i].hi, t[i].mid, t[i].lo, what);
    }
}

TEST(vcn_decfromr8_zero_underflow_overflow)
{
    REQUIRE(U(VarDecFromR8));
    DECIMAL d; HRESULT hr;
    /* zero (either sign) and anything below 2^-95 gives a plain +0, the sign is dropped */
    static const double zeros[] = { 0.0, -0.0, 1e-300, -1e-300, 5e-324, -5e-324, 2.4e-29, -2.4e-29, 3e-29, -3e-29, 5e-29, -5e-29 };
    for (unsigned i = 0; i < VCN_ROWS(zeros); i++) {
        memset(&d, 0xCC, sizeof d);
        hr = U(VarDecFromR8)(zeros[i], &d);
        char what[64]; snprintf(what, sizeof what, "VarDecFromR8(%g)", zeros[i]);
        DEC_CHECK(hr, d, 0, 0, 0, 0, 0, what);
    }
    /* 1e-28 is the smallest non-zero magnitude; 1.5e-28 is a binary near-tie that rounds to 2 */
    hr = U(VarDecFromR8)(1.5e-28, &d); DEC_CHECK(hr, d, 28, 0, 0, 0, 2, "1.5e-28");
    hr = U(VarDecFromR8)(-1e-28, &d);  DEC_CHECK(hr, d, 28, 0x80, 0, 0, 1, "-1e-28");
    /* overflow from 2^96 up (biased exponent > 0x45e) */
    static const double big[] = { 7.9228162514264337593543950336e28, -7.9228162514264337593543950336e28, 1e29, -1e29, 1e300, 1.7976931348623157e308, INFINITY, -INFINITY, NAN };
    for (unsigned i = 0; i < VCN_ROWS(big); i++) {
        memset(&d, 0xCC, sizeof d);
        hr = U(VarDecFromR8)(big[i], &d);
        CHECKF(hr == DISP_E_OVERFLOW, "VarDecFromR8(%g) hr=0x%lx", big[i], (unsigned long)hr);
    }
    /* just below 2^96 still converts: 15 significant digits, then scaled back up (scale stays 0) */
    hr = U(VarDecFromR8)(7.9228162514264e28, &d);
    CHECKF(hr == S_OK && d.scale == 0 && d.sign == 0, "7.9228162514264e28: hr=0x%lx sc=%u", (unsigned long)hr, d.scale);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarDecFromR4 (LGPL-2.1+), digit counts from the decomp */
TEST(vcn_decfromr4_seven_digits)
{
    REQUIRE(U(VarDecFromR4));
    static const struct { float x; int sc, sg; ULONG hi, mid, lo; } t[] = {
        { 0.5f, 1, 0, 0, 0, 5 }, { -0.5f, 1, 0x80, 0, 0, 5 }, { 0.4f, 1, 0, 0, 0, 4 }, { 0.6f, 1, 0, 0, 0, 6 },
        { 0.1f, 1, 0, 0, 0, 1 },       /* (double)0.1f = 0.100000001490116... is rounded to 7 digits: exactly 0.1 */
        { 0.3f, 1, 0, 0, 0, 3 }, { 1.0f, 0, 0, 0, 0, 1 }, { 100.0f, 0, 0, 0, 0, 100 }, { 1234567.0f, 0, 0, 0, 0, 1234567 },
        { 12345678.0f, 0, 0, 0, 0, 12345680 },                  /* 8th digit is lost */
        { 16777216.0f, 0, 0, 0, 0, 16777220 }, { 0.1234567f, 7, 0, 0, 0, 1234567 }, { 3.14159274f, 6, 0, 0, 0, 3141593 },
        { -2.5f, 1, 0x80, 0, 0, 25 }, { 8.5f, 1, 0, 0, 0, 85 }, { 0.25f, 2, 0, 0, 0, 25 }, { 123456.7f, 1, 0, 0, 0, 1234567 },
        { 1.5e-20f, 21, 0, 0, 0, 15 },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        DECIMAL d; memset(&d, 0xCC, sizeof d);
        HRESULT hr = U(VarDecFromR4)(t[i].x, &d);
        char what[64]; snprintf(what, sizeof what, "VarDecFromR4(%.9g)", t[i].x);
        DEC_CHECK(hr, d, t[i].sc, t[i].sg, t[i].hi, t[i].mid, t[i].lo, what);
    }
    DECIMAL d;
    static const float ovf[] = { INFINITY, -INFINITY, NAN, 1e30f, -1e30f, 3.4e38f };
    for (unsigned i = 0; i < VCN_ROWS(ovf); i++)
        CHECKF(U(VarDecFromR4)(ovf[i], &d) == DISP_E_OVERFLOW, "VarDecFromR4(%g) must overflow", ovf[i]);
    HRESULT hr = U(VarDecFromR4)(-0.0f, &d); DEC_CHECK(hr, d, 0, 0, 0, 0, 0, "-0.0f");
    hr = U(VarDecFromR4)(1e-30f, &d);        DEC_CHECK(hr, d, 0, 0, 0, 0, 0, "1e-30f -> zero (below 2^-95)");
}

TEST(vcn_dec_invalid_rejected_by_all_consumers)
{
    /* scale > 28 or any sign bit other than 0x80 is E_INVALIDARG (before any range test), output untouched
     * adapted from Wine dlls/oleaut32/tests/vartype.c CONVERT_BADDEC (LGPL-2.1+) */
    static const struct { const char *fn; unsigned size; int writes_on_fail; } f[] = {
        { "VarI1FromDec", 1, 0 }, { "VarUI1FromDec", 1, 0 }, { "VarI2FromDec", 2, 0 }, { "VarUI2FromDec", 2, 0 },
        { "VarI4FromDec", 4, 0 }, { "VarUI4FromDec", 4, 0 }, { "VarI8FromDec", 8, 0 }, { "VarUI8FromDec", 8, 0 },
        { "VarR8FromDec", 8, 0 }, { "VarCyFromDec", 8, 0 }, { "VarBoolFromDec", 2, 0 },
        { "VarR4FromDec", 4, 1 }, { "VarDateFromDec", 8, 1 },     /* these two ignore the inner failure when storing */
    };
    static const struct { int sc, sg; } bad[] = { {29,0},{30,0},{255,0},{0,1},{0,0x40},{0,0x7f},{0,0x81},{0,0xff},{4,0x20},{28,0x10},{29,0x80} };
    for (unsigned i = 0; i < VCN_ROWS(f); i++) {
        typedef HRESULT (WINAPI *fn_t)(DECIMAL *, void *);
        fn_t fn = (fn_t)t_proc(f[i].fn);
        if (!fn) { t_skip("missing export %s", f[i].fn); return; }
        for (unsigned j = 0; j < VCN_ROWS(bad); j++) {
            DECIMAL d; memset(&d, 0, sizeof d);
            d.scale = bad[j].sc; d.sign = bad[j].sg; d.Lo32 = 1;
            unsigned char out[16]; memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(&d, out);
            CHECKF(hr == E_INVALIDARG, "%s scale=%d sign=0x%x: hr=0x%08lx", f[i].fn, bad[j].sc, bad[j].sg, (unsigned long)hr);
            if (!f[i].writes_on_fail) CHECKF(vcn_untouched(out, f[i].size), "%s scale=%d sign=0x%x: output written", f[i].fn, bad[j].sc, bad[j].sg);
        }
        /* edge of validity: scale 28 and sign 0x80 are accepted */
        DECIMAL ok; memset(&ok, 0, sizeof ok); ok.scale = 28; ok.sign = 0x80;
        unsigned char out[16];
        CHECKF(fn(&ok, out) != E_INVALIDARG, "%s must accept scale 28 / sign 0x80 (zero value)", f[i].fn);
    }
}

TEST(vcn_dec_wreserved_is_ignored)
{
    REQUIRE(U(VarI4FromDec)); REQUIRE(U(VarR8FromDec)); REQUIRE(U(VarBoolFromDec));
    DECIMAL d; memset(&d, 0, sizeof d);
    d.wReserved = 0xFFFF; d.scale = 2; d.sign = 0x80; d.Lo32 = 12345;
    LONG l = 0; double r = 0; VARIANT_BOOL b = 0;
    EXPECT_EQ(U(VarI4FromDec)(&d, &l), S_OK); EXPECT_EQ(l, -123);
    EXPECT_EQ(U(VarR8FromDec)(&d, &r), S_OK); CHECK(r == -123.45);
    EXPECT_EQ(U(VarBoolFromDec)(&d, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
}

TEST(vcn_dec_negative_zero_and_zero_scale)
{
    /* -0 (sign set, value 0) is accepted by every integer consumer and yields 0; only non-zero magnitudes are rejected by unsigned ones */
    REQUIRE(U(VarUI1FromDec)); REQUIRE(U(VarUI2FromDec)); REQUIRE(U(VarUI4FromDec)); REQUIRE(U(VarUI8FromDec));
    REQUIRE(U(VarI1FromDec)); REQUIRE(U(VarI8FromDec)); REQUIRE(U(VarBoolFromDec)); REQUIRE(U(VarR8FromDec));
    DECIMAL d; memset(&d, 0, sizeof d); d.sign = 0x80;
    BYTE b = 9; USHORT w = 9; ULONG l = 9; ULONG64 q = 9; CHAR c = 9; LONG64 s = 9; VARIANT_BOOL vb = 9; double r = 9;
    EXPECT_EQ(U(VarUI1FromDec)(&d, &b), S_OK);  EXPECT_EQ(b, 0);
    EXPECT_EQ(U(VarUI2FromDec)(&d, &w), S_OK);  EXPECT_EQ(w, 0);
    EXPECT_EQ(U(VarUI4FromDec)(&d, &l), S_OK);  EXPECT_EQ(l, 0);
    EXPECT_EQ(U(VarUI8FromDec)(&d, &q), S_OK);  EXPECT_EQ(q, 0);
    EXPECT_EQ(U(VarI1FromDec)(&d, &c), S_OK);   EXPECT_EQ(c, 0);
    EXPECT_EQ(U(VarI8FromDec)(&d, &s), S_OK);   EXPECT_EQ(s, 0);
    EXPECT_EQ(U(VarBoolFromDec)(&d, &vb), S_OK); EXPECT_EQ(vb, 0);
    EXPECT_EQ(U(VarR8FromDec)(&d, &r), S_OK);   CHECK(r == 0.0);
    CHECK(signbit(r));      /* decomp: the sign is applied by flipping the sign bit, so -0.0 results */
}

TEST(vcn_decfromint_layouts)
{
    REQUIRE(U(VarDecFromI1)); REQUIRE(U(VarDecFromI2)); REQUIRE(U(VarDecFromI4)); REQUIRE(U(VarDecFromI8));
    REQUIRE(U(VarDecFromUI1)); REQUIRE(U(VarDecFromUI2)); REQUIRE(U(VarDecFromUI4)); REQUIRE(U(VarDecFromUI8)); REQUIRE(U(VarDecFromBool));
    DECIMAL d; HRESULT hr;
    hr = U(VarDecFromI1)(-128, &d);  DEC_CHECK(hr, d, 0, 0x80, 0, 0, 128, "I1 -128");
    hr = U(VarDecFromI1)(127, &d);   DEC_CHECK(hr, d, 0, 0, 0, 0, 127, "I1 127");
    hr = U(VarDecFromI2)(-32768, &d); DEC_CHECK(hr, d, 0, 0x80, 0, 0, 32768, "I2 min");
    hr = U(VarDecFromI2)(32767, &d); DEC_CHECK(hr, d, 0, 0, 0, 0, 32767, "I2 max");
    hr = U(VarDecFromI4)(-2147483647L - 1, &d); DEC_CHECK(hr, d, 0, 0x80, 0, 0, 0x80000000UL, "I4 min");
    hr = U(VarDecFromI4)(-1, &d);    DEC_CHECK(hr, d, 0, 0x80, 0, 0, 1, "I4 -1");
    hr = U(VarDecFromI8)((-0x7fffffffffffffffLL) - 1, &d); DEC_CHECK(hr, d, 0, 0x80, 0, 0x80000000UL, 0, "I8 min");
    hr = U(VarDecFromI8)(0x7fffffffffffffffLL, &d); DEC_CHECK(hr, d, 0, 0, 0, 0x7fffffffUL, 0xffffffffUL, "I8 max");
    hr = U(VarDecFromI8)(-4294967296LL, &d); DEC_CHECK(hr, d, 0, 0x80, 0, 1, 0, "I8 -2^32");
    hr = U(VarDecFromUI1)(255, &d);  DEC_CHECK(hr, d, 0, 0, 0, 0, 255, "UI1");
    hr = U(VarDecFromUI2)(65535, &d); DEC_CHECK(hr, d, 0, 0, 0, 0, 65535, "UI2");
    hr = U(VarDecFromUI4)(0xffffffffUL, &d); DEC_CHECK(hr, d, 0, 0, 0, 0, 0xffffffffUL, "UI4");
    hr = U(VarDecFromUI8)(0xffffffffffffffffULL, &d); DEC_CHECK(hr, d, 0, 0, 0, 0xffffffffUL, 0xffffffffUL, "UI8");
    /* every non-zero VARIANT_BOOL becomes -1 (sign set, magnitude 1) */
    hr = U(VarDecFromBool)(VARIANT_TRUE, &d);  DEC_CHECK(hr, d, 0, 0x80, 0, 0, 1, "TRUE");
    hr = U(VarDecFromBool)(VARIANT_FALSE, &d); DEC_CHECK(hr, d, 0, 0, 0, 0, 0, "FALSE");
    hr = U(VarDecFromBool)(1, &d);             DEC_CHECK(hr, d, 0, 0x80, 0, 0, 1, "1");
    hr = U(VarDecFromBool)(-32768, &d);        DEC_CHECK(hr, d, 0, 0x80, 0, 0, 1, "-32768");
}

TEST(vcn_ui8_from_dec_native_negative_quirk)
{
    REQUIRE(U(VarUI8FromDec));
    /* Native bug (kept on Win10, see decomp: sign rejected only if BOTH Lo32 and Mid32 are non-zero; Wine's
     * test comment: "This returns 1 under native"): a negative scale-0 DECIMAL with Mid32==0 or Lo32==0 is accepted. */
    ULONG64 q;
    DECIMAL d; memset(&d, 0, sizeof d); d.sign = 0x80; d.Lo32 = 1;
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), S_OK); EXPECT_EQ(q, 1);
    d.Lo32 = 0xffffffffUL;
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), S_OK); EXPECT_EQ(q, 0xffffffffULL);
    d.Lo32 = 0; d.Mid32 = 7;
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), S_OK); EXPECT_EQ(q, 7ULL << 32);
    d.Lo32 = 1; d.Mid32 = 7;        /* both words non-zero: finally rejected */
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), DISP_E_OVERFLOW); EXPECT_EQ(q, 0x55);
    d.Hi32 = 1; d.Lo32 = 0; d.Mid32 = 0; d.sign = 0;
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), DISP_E_OVERFLOW);
    /* with a scale the value goes through double and the negative is rejected properly */
    memset(&d, 0, sizeof d); d.scale = 2; d.sign = 0x80; d.Lo32 = 100;
    q = 0x55; EXPECT_EQ(U(VarUI8FromDec)(&d, &q), DISP_E_OVERFLOW);
    /* the unsigned 32/16/8-bit consumers are strict */
    BYTE b; USHORT w; ULONG l;
    memset(&d, 0, sizeof d); d.sign = 0x80; d.Lo32 = 1;
    EXPECT_EQ(U(VarUI1FromDec)(&d, &b), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI2FromDec)(&d, &w), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI4FromDec)(&d, &l), DISP_E_OVERFLOW);
}

TEST(vcn_signed_dec_extremes)
{
    REQUIRE(U(VarI1FromDec)); REQUIRE(U(VarI2FromDec)); REQUIRE(U(VarI4FromDec)); REQUIRE(U(VarI8FromDec));
    DECIMAL d; memset(&d, 0, sizeof d);
    CHAR c; SHORT s; LONG l; LONG64 q;
    d.Lo32 = 128; d.sign = 0x80;  EXPECT_EQ(U(VarI1FromDec)(&d, &c), S_OK); EXPECT_EQ(c, -128);
    d.sign = 0;                   EXPECT_EQ(U(VarI1FromDec)(&d, &c), DISP_E_OVERFLOW);
    d.Lo32 = 32768; d.sign = 0x80; EXPECT_EQ(U(VarI2FromDec)(&d, &s), S_OK); EXPECT_EQ(s, -32768);
    d.sign = 0;                   EXPECT_EQ(U(VarI2FromDec)(&d, &s), DISP_E_OVERFLOW);
    d.Lo32 = 0x80000000UL; d.sign = 0x80; EXPECT_EQ(U(VarI4FromDec)(&d, &l), S_OK); EXPECT_EQ(l, (LONG)0x80000000UL);
    d.sign = 0;                   EXPECT_EQ(U(VarI4FromDec)(&d, &l), DISP_E_OVERFLOW);
    d.Lo32 = 0; d.Mid32 = 0x80000000UL; d.sign = 0x80; EXPECT_EQ(U(VarI8FromDec)(&d, &q), S_OK); EXPECT_EQ(q, (-0x7fffffffffffffffLL) - 1);
    d.sign = 0;                   EXPECT_EQ(U(VarI8FromDec)(&d, &q), DISP_E_OVERFLOW);
    d.Lo32 = 1; d.Mid32 = 0; d.Hi32 = 1; d.sign = 0x80; EXPECT_EQ(U(VarI8FromDec)(&d, &q), DISP_E_OVERFLOW);
}
