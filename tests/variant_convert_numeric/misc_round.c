/* Banker's rounding and exact overflow thresholds of the float->integer conversions.
 * Rules (decomp of VarI4FromR8/ConvI4FromR8/ConvUI4FromR8/ConvI8FromR8/ConvUI8FromR8):
 *   overflow if x < lo or x >= hi, otherwise round-half-to-even.  lo/hi are exact doubles:
 *   I1 [-128.5,127.5)  UI1 [-0.5,255.5)  I2 [-32768.5,32767.5)  UI2 [-0.5,65535.5)
 *   I4 [-2^31-0.5,2^31-0.5)  UI4 [-0.5,2^32-0.5)  I8 [-2^62,2^62) (Win10 constants 0xC3D0.. and 0x43D0.., not 2^63)  UI8 [-0.5,2^64)
 * (lower bound inclusive, upper bound exclusive.)  The same bounds apply to the R4 and Date sources. */
#include "vcn.h"

typedef HRESULT (WINAPI *r8fn)(double, void *);
typedef HRESULT (WINAPI *r4fn)(float, void *);

static const struct ifam {
    const char *name; unsigned size; int is_signed; double lo, hi; /* [lo, hi) */
} fam[] = {
    { "I1", 1, 1, -128.5, 127.5 }, { "UI1", 1, 0, -0.5, 255.5 }, { "I2", 2, 1, -32768.5, 32767.5 },
    { "UI2", 2, 0, -0.5, 65535.5 }, { "I4", 4, 1, -2147483648.5, 2147483647.5 }, { "UI4", 4, 0, -0.5, 4294967295.5 },
    { "I8", 8, 1, -4611686018427387904.0, 4611686018427387904.0 }, { "UI8", 8, 0, -0.5, 18446744073709551616.0 },
};

static long double as_ld(const struct ifam *f, const void *o)
{
    if (f->is_signed) switch (f->size) { case 1: return *(const signed char *)o; case 2: return *(const SHORT *)o;
                                         case 4: return *(const LONG *)o; default: return *(const LONG64 *)o; }
    switch (f->size) { case 1: return *(const BYTE *)o; case 2: return *(const USHORT *)o;
                       case 4: return *(const ULONG *)o; default: return *(const ULONG64 *)o; }
}

static r8fn get(const char *dst, const char *src)
{
    char n[64]; snprintf(n, sizeof n, "Var%sFrom%s", dst, src);
    return (r8fn)t_proc(n);
}

TEST(vcn_r8_to_int_thresholds_all_widths)
{
    for (unsigned i = 0; i < VCN_ROWS(fam); i++) {
        const struct ifam *f = &fam[i];
        r8fn fn = get(f->name, "R8");
        if (!fn) { t_skip("missing Var%sFromR8", f->name); return; }
        unsigned char out[16];
        double pts_ok[] = { f->lo, nextafter(f->lo, 0.0), 0.0, nextafter(f->hi, 0.0) };
        double pts_bad[] = { nextafter(f->lo, -1e300), f->hi, nextafter(f->hi, 1e300), -INFINITY, INFINITY };
        for (unsigned k = 0; k < 4; k++) {
            memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(pts_ok[k], out);
            CHECKF(hr == S_OK, "Var%sFromR8(%.17g) hr=0x%08lx", f->name, pts_ok[k], (unsigned long)hr);
            if (hr == S_OK) {
                long double got = as_ld(f, out), want = nearbyintl((long double)pts_ok[k]);
                CHECKF(got == want, "Var%sFromR8(%.17g) got %.0Lf want %.0Lf", f->name, pts_ok[k], got, want);
            }
        }
        for (unsigned k = 0; k < 5; k++) {
            memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(pts_bad[k], out);
            CHECKF(hr == DISP_E_OVERFLOW, "Var%sFromR8(%.17g) hr=0x%08lx want overflow", f->name, pts_bad[k], (unsigned long)hr);
            CHECKF(vcn_untouched(out, f->size), "Var%sFromR8(%.17g): output written on overflow", f->name, pts_bad[k]);
        }
    }
}

TEST(vcn_date_and_r4_use_the_same_thresholds)
{
    for (unsigned i = 0; i < VCN_ROWS(fam); i++) {
        const struct ifam *f = &fam[i];
        r8fn fd = get(f->name, "Date");
        r4fn f4 = (r4fn)get(f->name, "R4");
        if (!fd || !f4) { t_skip("missing Var%sFromDate/R4", f->name); return; }
        unsigned char o8[16], o4[16];
        double pts[] = { f->lo, nextafter(f->lo, 0.0), nextafter(f->lo, -1e300), nextafter(f->hi, 0.0), f->hi, 0.5, -0.5, 1.5, 2.5, -1.5 };
        for (unsigned k = 0; k < VCN_ROWS(pts); k++) {
            memset(o8, 0xCC, sizeof o8);
            HRESULT hr = fd(pts[k], o8);
            r8fn f8 = get(f->name, "R8"); unsigned char oo[16]; memset(oo, 0xCC, sizeof oo);
            HRESULT hr8 = f8(pts[k], oo);
            CHECKF(hr == hr8, "Var%sFromDate(%.17g) hr=0x%lx vs FromR8 0x%lx", f->name, pts[k], (unsigned long)hr, (unsigned long)hr8);
            if (hr == S_OK && hr8 == S_OK) CHECKF(as_ld(f, o8) == as_ld(f, oo), "Var%sFromDate(%.17g) differs from FromR8", f->name, pts[k]);
            /* R4: the float is widened first, so compare with R8 of the widened value */
            float fv = (float)pts[k];
            if ((double)fv == pts[k]) {
                memset(o4, 0xCC, sizeof o4);
                HRESULT h4 = f4(fv, o4);
                CHECKF(h4 == hr8, "Var%sFromR4(%.9g) hr=0x%lx vs FromR8 0x%lx", f->name, fv, (unsigned long)h4, (unsigned long)hr8);
                if (h4 == S_OK && hr8 == S_OK) CHECKF(as_ld(f, o4) == as_ld(f, oo), "Var%sFromR4(%.9g) differs", f->name, fv);
            }
        }
    }
}

TEST(vcn_banker_rounding_ties)
{
    /* every n+0.5 rounds to the even neighbour; shown for each integer width (signed ones also test negatives) */
    for (unsigned i = 0; i < VCN_ROWS(fam); i++) {
        const struct ifam *f = &fam[i];
        r8fn fn = get(f->name, "R8");
        if (!fn) { t_skip("missing Var%sFromR8", f->name); return; }
        for (int n = -6; n <= 20; n++) {
            double x = n + 0.5;
            unsigned char out[16]; memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(x, out);
            long long even = (n % 2 == 0) ? n : n + 1;
            if (n < 0) even = (-n % 2 == 0) ? n : n + 1;       /* -0.5 -> 0, -1.5 -> -2, -2.5 -> -2 */
            if (n == -1) even = 0;
            else if (n < 0) even = (n % 2 == 0) ? n : n + 1;
            if (!f->is_signed && x < -0.5) { CHECKF(hr == DISP_E_OVERFLOW, "Var%sFromR8(%g) hr=0x%lx", f->name, x, (unsigned long)hr); continue; }
            CHECKF(hr == S_OK, "Var%sFromR8(%g) hr=0x%lx", f->name, x, (unsigned long)hr);
            if (hr == S_OK) CHECKF(as_ld(f, out) == (long double)even, "Var%sFromR8(%g) got %.0Lf want %lld", f->name, x, as_ld(f, out), even);
        }
        /* neighbours of a tie are not ties */
        static const struct { double x; long long want; } nb[] = {
            { 0.49999999999999994, 0 }, { 0.5000000000000001, 1 }, { 1.4999999999999998, 1 }, { 1.5000000000000002, 2 },
            { 2.4999999999999996, 2 }, { 2.5000000000000004, 3 }, { 3.4999999999999996, 3 }, { 3.5000000000000004, 4 },
        };
        for (unsigned k = 0; k < VCN_ROWS(nb); k++) {
            unsigned char out[16]; memset(out, 0xCC, sizeof out);
            HRESULT hr = fn(nb[k].x, out);
            CHECKF(hr == S_OK && as_ld(f, out) == (long double)nb[k].want, "Var%sFromR8(%.17g) hr=0x%lx got %.0Lf want %lld", f->name, nb[k].x, (unsigned long)hr, as_ld(f, out), nb[k].want);
        }
    }
}

TEST(vcn_documented_examples)
{
    REQUIRE(U(VarUI1FromR8)); REQUIRE(U(VarI2FromR8)); REQUIRE(U(VarI4FromR8)); REQUIRE(U(VarI1FromR8));
    BYTE b; SHORT s; LONG l; CHAR c;
    /* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarUI1FromR8 / test_VarI2FromR8 (LGPL-2.1+) */
    EXPECT_EQ(U(VarUI1FromR8)(-0.5, &b), S_OK);          EXPECT_EQ(b, 0);
    EXPECT_EQ(U(VarUI1FromR8)(-0.51, &b), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarUI1FromR8)(0.5, &b), S_OK);           EXPECT_EQ(b, 0);
    EXPECT_EQ(U(VarUI1FromR8)(0.6, &b), S_OK);           EXPECT_EQ(b, 1);
    EXPECT_EQ(U(VarUI1FromR8)(1.5, &b), S_OK);           EXPECT_EQ(b, 2);
    EXPECT_EQ(U(VarUI1FromR8)(2.5, &b), S_OK);           EXPECT_EQ(b, 2);
    EXPECT_EQ(U(VarUI1FromR8)(254.5, &b), S_OK);         EXPECT_EQ(b, 254);
    EXPECT_EQ(U(VarUI1FromR8)(255.4, &b), S_OK);         EXPECT_EQ(b, 255);
    EXPECT_EQ(U(VarUI1FromR8)(255.5, &b), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI1FromR8)(-128.5, &c), S_OK);         EXPECT_EQ(c, -128);   /* tie goes to even -128 */
    EXPECT_EQ(U(VarI1FromR8)(-128.6, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI1FromR8)(127.5, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI1FromR8)(127.4, &c), S_OK);          EXPECT_EQ(c, 127);
    EXPECT_EQ(U(VarI2FromR8)(-32768.5, &s), S_OK);       EXPECT_EQ(s, -32768);
    EXPECT_EQ(U(VarI2FromR8)(32767.5, &s), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI4FromR8)(-2147483648.5, &l), S_OK);  EXPECT_EQ(l, (LONG)0x80000000UL);
    EXPECT_EQ(U(VarI4FromR8)(2147483647.5, &l), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarI4FromR8)(2147483646.5, &l), S_OK);   EXPECT_EQ(l, 2147483646);
}

TEST(vcn_nan_to_int_is_unspecified_note)
{
    /* NaN fails both comparisons, so no range check triggers and the (garbage) conversion result is stored with S_OK.
     * Not asserted (value is whatever cvttsd2si produces); recorded for implementors. */
    static const char *const names[] = { "I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8" };
    for (unsigned i = 0; i < VCN_ROWS(names); i++) {
        r8fn fn = get(names[i], "R8");
        if (!fn) continue;
        unsigned char out[16]; memset(out, 0xCC, sizeof out);
        HRESULT hr = fn(NAN, out);
        t_note("Var%sFromR8(NaN) -> 0x%08lx first out bytes %02x %02x", names[i], (unsigned long)hr, out[0], out[1]);
    }
    CHECK(1);
}
