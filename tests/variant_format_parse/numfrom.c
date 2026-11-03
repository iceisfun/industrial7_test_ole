/* variant_format_parse: VarNumFromParseNum (oleaut32.dll): turn a NUMPARSE + digit array into a typed VARIANT.
 * dwVtBits (VTBIT_*) lists the acceptable output types; the smallest type that holds the value is chosen, signed before
 * unsigned for the same size, then R4, R8, CY, DECIMAL.  Decomp-verified: with nBaseShift != 0 more than 66 bits of digits
 * (cDig*nBaseShift > 0x42) fails with DISP_E_OVERFLOW (0x8002000A); no NULL checks are made (never pass NULL). */
#include "vfp.h"

static HRESULT numfrom(int cdig, ULONG out, int base, int pwr, ULONG vtbits, const BYTE *digs, VARIANT *v)
{
    NUMPARSE np;
    BYTE rgb[128];
    memset(rgb, 0xff, sizeof rgb);
    if (digs) memcpy(rgb, digs, cdig);
    memset(&np, 0, sizeof np);
    np.cDig = cdig; np.dwInFlags = 0; np.dwOutFlags = out; np.cchUsed = cdig; np.nBaseShift = base; np.nPwr10 = pwr;
    memset(v, 0, sizeof *v);
    return U(VarNumFromParseNum)(&np, rgb, vtbits, v);
}
#define INTBITS (VTBIT_I1 | VTBIT_UI1 | VTBIT_I2 | VTBIT_UI2 | VTBIT_I4 | VTBIT_UI4 | VTBIT_I8 | VTBIT_UI8)

typedef struct {
    const char *name;
    const char *digs;   /* hex chars */
    int base, pwr;
    ULONG out, bits;
    int vt;             /* expected VT, or -1 for overflow */
    long long iv;       /* integer expected */
    double dv;          /* floating expected */
} NRowP;

static void run_nrows_p(const NRowP *rows, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++) {
        BYTE d[128];
        int cd = (int)strlen(rows[i].digs), k;
        VARIANT v;
        HRESULT hr;
        for (k = 0; k < cd; k++) { char c = rows[i].digs[k]; d[k] = c <= '9' ? c - '0' : c - 'a' + 10; }
        hr = numfrom(cd, rows[i].out, rows[i].base, rows[i].pwr, rows[i].bits, d, &v);
        if (rows[i].vt < 0) { CHECKF(hr == VFP_E_OVERFLOW, "%s: want overflow got hr=0x%lx vt=%d", rows[i].name, (unsigned long)hr, V_VT(&v)); continue; }
        CHECKF(hr == S_OK, "%s: hr=0x%lx", rows[i].name, (unsigned long)hr);
        if (hr != S_OK) continue;
        CHECKF(V_VT(&v) == rows[i].vt, "%s: vt %d want %d", rows[i].name, V_VT(&v), rows[i].vt);
        if (V_VT(&v) != rows[i].vt) continue;
        switch (rows[i].vt) {
        case VT_I1:  CHECKF(V_I1(&v) == (signed char)rows[i].iv, "%s: %d", rows[i].name, V_I1(&v)); break;
        case VT_UI1: CHECKF(V_UI1(&v) == (BYTE)rows[i].iv, "%s: %u", rows[i].name, V_UI1(&v)); break;
        case VT_I2:  CHECKF(V_I2(&v) == (SHORT)rows[i].iv, "%s: %d", rows[i].name, V_I2(&v)); break;
        case VT_UI2: CHECKF(V_UI2(&v) == (USHORT)rows[i].iv, "%s: %u", rows[i].name, V_UI2(&v)); break;
        case VT_I4:  CHECKF(V_I4(&v) == (LONG)rows[i].iv, "%s: %ld", rows[i].name, (long)V_I4(&v)); break;
        case VT_UI4: CHECKF(V_UI4(&v) == (ULONG)rows[i].iv, "%s: %lu", rows[i].name, (unsigned long)V_UI4(&v)); break;
        case VT_I8:  CHECKF(V_I8(&v) == rows[i].iv, "%s: %lld", rows[i].name, (long long)V_I8(&v)); break;
        case VT_UI8: CHECKF(V_UI8(&v) == (ULONGLONG)rows[i].iv, "%s: %llu", rows[i].name, (unsigned long long)V_UI8(&v)); break;
        case VT_R4:  CHECKF(V_R4(&v) == (float)rows[i].dv, "%s: %g", rows[i].name, V_R4(&v)); break;
        case VT_R8:  CHECKF(V_R8(&v) == rows[i].dv, "%s: %g", rows[i].name, V_R8(&v)); break;
        case VT_CY:  CHECKF(V_CY(&v).int64 == (LONG64)rows[i].iv, "%s: %lld", rows[i].name, (long long)V_CY(&v).int64); break;
        case VT_DECIMAL: CHECKF(V_DECIMAL(&v).Lo32 == (ULONG)rows[i].iv && V_DECIMAL(&v).Mid32 == 0 && V_DECIMAL(&v).Hi32 == 0, "%s: lo=%lu", rows[i].name, (unsigned long)V_DECIMAL(&v).Lo32); break;
        }
    }
}
#define IR(nm, d, base, pwr, out, bits, vt, iv) { nm, d, base, pwr, out, bits, vt, iv, 0 }
#define FR(nm, d, base, pwr, out, bits, vt, dv) { nm, d, base, pwr, out, bits, vt, 0, dv }
#define OV(nm, d, base, pwr, out, bits) { nm, d, base, pwr, out, bits, -1, 0, 0 }

/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_numfrom_decimal_integer_selection)
{
    static const NRowP rows[] = {
        IR("1/I1", "1", 0, 0, 0, VTBIT_I1, VT_I1, 1), IR("1/UI1", "1", 0, 0, 0, VTBIT_UI1, VT_UI1, 1), IR("1/I1|UI1", "1", 0, 0, 0, VTBIT_I1 | VTBIT_UI1, VT_I1, 1),
        IR("1/I2|UI1", "1", 0, 0, 0, VTBIT_I2 | VTBIT_UI1, VT_UI1, 1), IR("1/all", "1", 0, 0, 0, INTBITS, VT_I1, 1), IR("127", "127", 0, 0, 0, INTBITS, VT_I1, 127),
        IR("128", "128", 0, 0, 0, INTBITS, VT_UI1, 128), IR("255", "255", 0, 0, 0, INTBITS, VT_UI1, 255), IR("256", "256", 0, 0, 0, INTBITS, VT_I2, 256),
        IR("32767", "32767", 0, 0, 0, INTBITS, VT_I2, 32767), IR("32768", "32768", 0, 0, 0, INTBITS, VT_UI2, 32768), IR("65535", "65535", 0, 0, 0, INTBITS, VT_UI2, 65535),
        IR("65536", "65536", 0, 0, 0, INTBITS, VT_I4, 65536), IR("2147483647", "2147483647", 0, 0, 0, INTBITS, VT_I4, 2147483647LL),
        IR("2147483648", "2147483648", 0, 0, 0, INTBITS, VT_UI4, 2147483648LL), IR("4294967295", "4294967295", 0, 0, 0, INTBITS, VT_UI4, 4294967295LL),
        IR("4294967296", "4294967296", 0, 0, 0, INTBITS, VT_I8, 4294967296LL), IR("0", "0", 0, 0, 0, INTBITS, VT_I1, 0),
        IR("-128", "128", 0, 0, NUMPRS_NEG, INTBITS, VT_I1, -128), IR("-129", "129", 0, 0, NUMPRS_NEG, INTBITS, VT_I2, -129), IR("-32768", "32768", 0, 0, NUMPRS_NEG, INTBITS, VT_I2, -32768),
        IR("-32769", "32769", 0, 0, NUMPRS_NEG, INTBITS, VT_I4, -32769), IR("-1", "1", 0, 0, NUMPRS_NEG, INTBITS, VT_I1, -1), IR("-2147483648", "2147483648", 0, 0, NUMPRS_NEG, INTBITS, VT_I4, -2147483648LL),
        IR("-2147483649", "2147483649", 0, 0, NUMPRS_NEG, INTBITS, VT_I8, -2147483649LL),
        IR("1e2 as I1", "1", 0, 2, 0, INTBITS, VT_I1, 100), IR("1e3 as I2", "1", 0, 3, 0, INTBITS, VT_I2, 1000), IR("12e1", "12", 0, 1, 0, INTBITS, VT_I1, 120), IR("13e1", "13", 0, 1, 0, INTBITS, VT_UI1, 130),
        IR("9223372036854775807", "9223372036854775807", 0, 0, 0, INTBITS, VT_I8, 9223372036854775807LL), IR("9223372036854775808 UI8", "9223372036854775808", 0, 0, 0, INTBITS, VT_UI8, (long long)9223372036854775808ULL),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_numfrom_hex_octal)
{
    static const NRowP rows[] = {
        IR("hex 1", "1", 4, 0, 0, INTBITS, VT_I1, 1), IR("hex 7f", "7f", 4, 0, 0, INTBITS, VT_I1, 0x7f), IR("hex 7f dec", "7f", 4, 0, 0, VTBIT_DECIMAL, VT_DECIMAL, 0x7f),
        IR("hex 7fff", "7fff", 4, 0, 0, INTBITS, VT_I2, 0x7fff), IR("hex 7fffffff", "7fffffff", 4, 0, 0, INTBITS, VT_I4, 0x7fffffffLL),
        IR("hex 7fffffffffffffff I8", "7fffffffffffffff", 4, 0, 0, VTBIT_I8, VT_I8, 0x7fffffffffffffffLL),
        /* the top bit is preserved as the sign bit of the smallest requested type (hex literals wrap) */
        IR("hex 82 I1", "82", 4, 0, 0, INTBITS, VT_I1, (signed char)0x82), IR("hex 8002 I2", "8002", 4, 0, 0, INTBITS, VT_I2, (short)0x8002),
        IR("hex 80000002 I4", "80000002", 4, 0, 0, INTBITS, VT_I4, (int)0x80000002u), IR("hex 8000000000000002 I8", "8000000000000002", 4, 0, 0, VTBIT_I8, VT_I8, (long long)0x8000000000000002ULL),
        IR("hex 82 strict I1", "82", 4, 0, 0, VTBIT_I1, VT_I1, (signed char)0x82), IR("hex 8002 strict I2", "8002", 4, 0, 0, VTBIT_I2, VT_I2, (short)0x8002),
        IR("hex 80000002 strict I4", "80000002", 4, 0, 0, VTBIT_I4, VT_I4, (int)0x80000002u),
        IR("oct 17", "17", 3, 0, 0, INTBITS, VT_I1, 15), IR("oct 377 (top bit kept: I1)", "377", 3, 0, 0, INTBITS, VT_I1, (signed char)255), IR("oct 777", "777", 3, 0, 0, INTBITS, VT_I2, 511),
        IR("hex ff UI1", "ff", 4, 0, 0, VTBIT_UI1, VT_UI1, 255), IR("hex 10", "10", 4, 0, 0, INTBITS, VT_I1, 16), IR("hex 0", "0", 4, 0, 0, INTBITS, VT_I1, 0),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
TEST(vfp_numfrom_hex_overflow_digit_limit)
{
    /* decomp: cDig * nBaseShift > 0x42 (66 bits) -> DISP_E_OVERFLOW, and exactly 66 bits only when the first digit is <= 1 */
    static const NRowP rows[] = {
        OV("17 hex digits (68 bits)", "10000000000000000", 4, 0, 0, VTBIT_I8),
        OV("17 hex digits all f", "fffffffffffffffff", 4, 0, 0, INTBITS),
        OV("23 octal digits (69 bits)", "10000000000000000000000", 3, 0, 0, INTBITS),
        OV("22 octal digits first digit 2 (66 bits)", "2000000000000000000000", 3, 0, 0, INTBITS),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_numfrom_negative_unsigned_overflow)
{
    static const NRowP rows[] = {
        OV("-1 UI1", "1", 0, 0, NUMPRS_NEG, VTBIT_UI1), OV("-1 UI2", "1", 0, 0, NUMPRS_NEG, VTBIT_UI2), OV("-1 UI4", "1", 0, 0, NUMPRS_NEG, VTBIT_UI4),
        OV("-1 UI8", "1", 0, 0, NUMPRS_NEG, VTBIT_UI8), OV("-0.6 UI1", "6", 0, -1, NUMPRS_NEG, VTBIT_UI1),
        IR("-0.5 UI1 rounds to 0", "5", 0, -1, NUMPRS_NEG, VTBIT_UI1, VT_UI1, 0), IR("-0.4 UI1", "4", 0, -1, NUMPRS_NEG, VTBIT_UI1, VT_UI1, 0),
        IR("0 neg UI1", "0", 0, 0, NUMPRS_NEG, VTBIT_UI1, VT_UI1, 0),
        OV("256 UI1", "256", 0, 0, 0, VTBIT_UI1), OV("128 I1", "128", 0, 0, 0, VTBIT_I1), OV("32768 I2", "32768", 0, 0, 0, VTBIT_I2), OV("65536 UI2", "65536", 0, 0, 0, VTBIT_UI2),
        OV("2147483648 I4", "2147483648", 0, 0, 0, VTBIT_I4), OV("4294967296 UI4", "4294967296", 0, 0, 0, VTBIT_UI4), OV("9223372036854775808 I8", "9223372036854775808", 0, 0, 0, VTBIT_I8),
        OV("18446744073709551616 UI8", "18446744073709551616", 0, 0, 0, VTBIT_UI8),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_numfrom_float_types)
{
    static const NRowP rows[] = {
        FR("1 R4", "1", 0, 0, 0, VTBIT_R4, VT_R4, 1.0), FR("1 R8", "1", 0, 0, 0, VTBIT_R8, VT_R8, 1.0), IR("1 CY", "1", 0, 0, 0, VTBIT_CY, VT_CY, 10000),
        FR("1 R4|R8 prefers R4", "1", 0, 0, 0, VTBIT_R4 | VTBIT_R8, VT_R4, 1.0), FR("1 R8|CY prefers R8", "1", 0, 0, 0, VTBIT_R8 | VTBIT_CY, VT_R8, 1.0),
        IR("1 CY|DEC prefers CY", "1", 0, 0, 0, VTBIT_CY | VTBIT_DECIMAL, VT_CY, 10000),
        FR("0.00000000E0 R8", "0", 0, -8, 0, VTBIT_R8, VT_R8, 0.0), FR("0.5 R8", "5", 0, -1, 0, VTBIT_R8, VT_R8, 0.5), FR("0.25 R8", "25", 0, -2, 0, VTBIT_R8, VT_R8, 0.25),
        FR("-0.5 R8", "5", 0, -1, NUMPRS_NEG, VTBIT_R8, VT_R8, -0.5), FR("1234.5 R8", "12345", 0, -1, 0, VTBIT_R8, VT_R8, 1234.5), FR("1e10 R8", "1", 0, 10, 0, VTBIT_R8, VT_R8, 1e10),
        FR("1.5 R4", "15", 0, -1, 0, VTBIT_R4, VT_R4, 1.5), FR("123456789 R8", "123456789", 0, 0, 0, VTBIT_R8, VT_R8, 123456789.0),
        FR("underflow R4", "1", 0, -94938484, NUMPRS_EXPONENT, VTBIT_R4, VT_R4, 0.0), FR("underflow R8", "1", 0, -94938484, NUMPRS_EXPONENT, VTBIT_R8, VT_R8, 0.0),
        IR("underflow CY", "1", 0, -94938484, NUMPRS_EXPONENT, VTBIT_CY, VT_CY, 0),
        IR("1.5 CY", "15", 0, -1, 0, VTBIT_CY, VT_CY, 15000), IR("-1.5 CY", "15", 0, -1, NUMPRS_NEG, VTBIT_CY, VT_CY, -15000), IR("0.0001 CY", "1", 0, -4, 0, VTBIT_CY, VT_CY, 1),
        IR("123.4567 CY", "1234567", 0, -4, 0, VTBIT_CY, VT_CY, 1234567),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
TEST(vfp_numfrom_fraction_with_integer_types)
{
    /* integer destinations round (banker's rounding to nearest is the OLE convention); fractional input is accepted */
    static const NRowP rows[] = {
        IR("1.4 -> 1", "14", 0, -1, 0, INTBITS, VT_I1, 1), IR("1.6 -> 2", "16", 0, -1, 0, INTBITS, VT_I1, 2), IR("-1.6 -> -2", "16", 0, -1, NUMPRS_NEG, INTBITS, VT_I1, -2),
        IR("2.5 -> 2 (even)", "25", 0, -1, 0, INTBITS, VT_I1, 2), IR("3.5 -> 4 (even)", "35", 0, -1, 0, INTBITS, VT_I1, 4), IR("0.5 -> 0 (even)", "5", 0, -1, 0, INTBITS, VT_I1, 0),
        IR("1.5 -> 2 (even)", "15", 0, -1, 0, INTBITS, VT_I1, 2), IR("100.4 I2", "1004", 0, -1, 0, INTBITS, VT_I1, 100),
    };
    REQUIRE(U(VarNumFromParseNum));
    run_nrows_p(rows, sizeof rows / sizeof *rows);
}
TEST(vfp_numfrom_decimal_destination)
{
    VARIANT v;
    BYTE d[3] = { 1, 2, 3 };
    HRESULT hr;
    REQUIRE(U(VarNumFromParseNum));
    hr = numfrom(3, 0, 0, 0, VTBIT_DECIMAL, d, &v);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { EXPECT_EQ(V_VT(&v), VT_DECIMAL); EXPECT_EQ(V_DECIMAL(&v).Lo32, 123); EXPECT_EQ(V_DECIMAL(&v).Mid32, 0); EXPECT_EQ(V_DECIMAL(&v).Hi32, 0); EXPECT_EQ(V_DECIMAL(&v).sign, 0); }
    hr = numfrom(3, NUMPRS_NEG, 0, -2, VTBIT_DECIMAL, d, &v);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) { EXPECT_EQ(V_VT(&v), VT_DECIMAL); EXPECT_EQ(V_DECIMAL(&v).Lo32, 123); EXPECT_EQ(V_DECIMAL(&v).scale, 2); CHECK(V_DECIMAL(&v).sign == DECIMAL_NEG); }
}
TEST(vfp_numfrom_no_matching_type_is_error)
{
    VARIANT v;
    BYTE d[1] = { 1 };
    HRESULT hr;
    REQUIRE(U(VarNumFromParseNum));
    hr = numfrom(1, 0, 0, 0, 0, d, &v);
    t_note("dwVtBits=0: hr=0x%lx vt=%d", (unsigned long)hr, V_VT(&v));
    CHECK(hr != S_OK || V_VT(&v) != VT_EMPTY);
}
