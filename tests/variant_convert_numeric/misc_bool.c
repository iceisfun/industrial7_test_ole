/* VARIANT_BOOL results and VarBoolFromStr */
#include "vcn.h"

typedef HRESULT (WINAPI *bsfn)(const WCHAR *, LCID, ULONG, VARIANT_BOOL *);
#define LCID_EN 0x409
#define LCID_FR 0x40c
#define LCID_DE 0x407
#define LCID_PL 0x415

/* adapted from Wine dlls/oleaut32/tests/vartype.c test_VarBoolFromStr (LGPL-2.1+) */
TEST(vcn_boolfromstr_literals)
{
    REQUIRE(U(VarBoolFromStr));
    static const struct { const WCHAR *s; LCID lcid; ULONG fl; HRESULT hr; VARIANT_BOOL v; } t[] = {
        { L"#FALSE#", LCID_EN, 0, S_OK, VARIANT_FALSE }, { L"#TRUE#", LCID_EN, 0, S_OK, VARIANT_TRUE },
        { L"#FALSE#", LCID_FR, 0, S_OK, VARIANT_FALSE }, { L"#TRUE#", LCID_FR, VARIANT_LOCALBOOL, S_OK, VARIANT_TRUE },
        { L"#False#", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"#true#", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"#TRUE", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"TRUE#", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"true", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"True", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"TRUE", LCID_EN, 0, S_OK, VARIANT_TRUE },
        { L"tRuE", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"false", LCID_EN, 0, S_OK, VARIANT_FALSE }, { L"False", LCID_EN, 0, S_OK, VARIANT_FALSE },
        { L"FALSE", LCID_EN, 0, S_OK, VARIANT_FALSE },
        { L"true", LCID_FR, 0, S_OK, VARIANT_TRUE }, { L"false", LCID_FR, 0, S_OK, VARIANT_FALSE },      /* English accepted everywhere */
        { L"true", LCID_DE, 0, S_OK, VARIANT_TRUE }, { L"False", LCID_PL, 0, S_OK, VARIANT_FALSE },
        { L" true", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"true ", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"truee", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"tru", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"On", LCID_EN, 0xffffffff, DISP_E_TYPEMISMATCH, 0 }, { L"Yes", LCID_EN, 0xffffffff, DISP_E_TYPEMISMATCH, 0 },
        { L"No", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"Off", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"T", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L"F", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 }, { L" ", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        /* numeric strings are read through VarR8FromStr: zero is False, anything else True (-1) */
        { L"0", LCID_EN, 0, S_OK, VARIANT_FALSE }, { L"-1", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"+1", LCID_EN, 0, S_OK, VARIANT_TRUE },
        { L"1", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"2", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"-2", LCID_EN, 0, S_OK, VARIANT_TRUE },
        { L"0.0", LCID_EN, 0, S_OK, VARIANT_FALSE }, { L"0.001", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"-0", LCID_EN, 0, S_OK, VARIANT_FALSE },
        { L"1e-5", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"0e5", LCID_EN, 0, S_OK, VARIANT_FALSE }, { L" 5 ", LCID_EN, 0, S_OK, VARIANT_TRUE },
        { L"65536", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"4294967296", LCID_EN, 0, S_OK, VARIANT_TRUE }, { L"abc", LCID_EN, 0, DISP_E_TYPEMISMATCH, 0 },
        { L"0,5", LCID_DE, 0, S_OK, VARIANT_TRUE }, { L"0,0", LCID_DE, 0, S_OK, VARIANT_FALSE }, { L"1.000", LCID_DE, 0, S_OK, VARIANT_TRUE },
    };
    for (unsigned i = 0; i < VCN_ROWS(t); i++) {
        VARIANT_BOOL v = 0x5a5a;
        HRESULT hr = U(VarBoolFromStr)(t[i].s, t[i].lcid, t[i].fl, &v);
        CHECKF(hr == t[i].hr, "row %u '%ls' lcid=%x fl=%lx: hr=0x%08lx want 0x%08lx", i, t[i].s, t[i].lcid, (unsigned long)t[i].fl, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK && t[i].hr == S_OK) CHECKF(v == t[i].v, "row %u '%ls': v=%d want %d", i, t[i].s, v, t[i].v);
        if (t[i].hr != S_OK && hr == t[i].hr) CHECKF(v == 0x5a5a, "row %u '%ls': output written on failure", i, t[i].s);
    }
}

TEST(vcn_boolfromstr_null_pointers)
{
    REQUIRE(U(VarBoolFromStr));
    VARIANT_BOOL v = 0x5a5a;
    /* decomp: explicit NULL checks on both pointers -> E_INVALIDARG (Wine's test accepts TYPEMISMATCH for NULL string too) */
    EXPECT_EQ(U(VarBoolFromStr)(NULL, LCID_EN, 0, &v), E_INVALIDARG);
    EXPECT_EQ(v, 0x5a5a);
    EXPECT_EQ(U(VarBoolFromStr)(L"1", LCID_EN, 0, NULL), E_INVALIDARG);
    EXPECT_EQ(U(VarBoolFromStr)(L"#TRUE#", LCID_EN, 0, NULL), E_INVALIDARG);
    EXPECT_EQ(U(VarBoolFromStr)(NULL, LCID_EN, 0, NULL), E_INVALIDARG);
}

TEST(vcn_boolfromstr_localbool)
{
    REQUIRE(U(VarBoolFromStr));
    VARIANT_BOOL v;
    /* VARIANT_LOCALBOOL (0x10) additionally accepts the localized words of the LCID (primary language lookup).
     * Without the flag only English / #TRUE# / numbers are accepted. */
    v = 0x5a5a; EXPECT_EQ(U(VarBoolFromStr)(L"faux", LCID_FR, 0, &v), DISP_E_TYPEMISMATCH);
    v = 0x5a5a; HRESULT hr = U(VarBoolFromStr)(L"faux", LCID_FR, VARIANT_LOCALBOOL, &v);
    if (hr == S_OK) { EXPECT_EQ(v, VARIANT_FALSE); } else t_note("French 'faux' with LOCALBOOL -> 0x%08lx", (unsigned long)hr);
    v = 0x5a5a; hr = U(VarBoolFromStr)(L"Faux", LCID_FR, VARIANT_LOCALBOOL, &v);
    if (hr == S_OK) EXPECT_EQ(v, VARIANT_FALSE); else t_note("French 'Faux' with LOCALBOOL -> 0x%08lx", (unsigned long)hr);
    v = 0x5a5a; hr = U(VarBoolFromStr)(L"vrai", LCID_FR, VARIANT_LOCALBOOL, &v);
    if (hr == S_OK) EXPECT_EQ(v, VARIANT_TRUE); else t_note("French 'vrai' with LOCALBOOL -> 0x%08lx", (unsigned long)hr);
    v = 0x5a5a; hr = U(VarBoolFromStr)(L"falsch", LCID_DE, VARIANT_LOCALBOOL, &v);
    if (hr == S_OK) EXPECT_EQ(v, VARIANT_FALSE); else t_note("German 'falsch' with LOCALBOOL -> 0x%08lx", (unsigned long)hr);
    v = 0x5a5a; hr = U(VarBoolFromStr)(L"wahr", LCID_DE, VARIANT_LOCALBOOL, &v);
    if (hr == S_OK) EXPECT_EQ(v, VARIANT_TRUE); else t_note("German 'wahr' with LOCALBOOL -> 0x%08lx", (unsigned long)hr);
    /* a different locale does not accept French words */
    v = 0x5a5a; hr = U(VarBoolFromStr)(L"faux", LCID_PL, VARIANT_LOCALBOOL, &v);
    t_note("Polish locale, 'faux' with LOCALBOOL -> 0x%08lx (expected TYPEMISMATCH)", (unsigned long)hr);
    /* English stays accepted under LOCALBOOL */
    v = 0x5a5a; EXPECT_EQ(U(VarBoolFromStr)(L"true", LCID_FR, VARIANT_LOCALBOOL, &v), S_OK); EXPECT_EQ(v, VARIANT_TRUE);
    v = 0x5a5a; EXPECT_EQ(U(VarBoolFromStr)(L"FALSE", LCID_DE, VARIANT_LOCALBOOL, &v), S_OK); EXPECT_EQ(v, VARIANT_FALSE);
}

TEST(vcn_bool_results_are_canonical)
{
    /* every conversion TO VARIANT_BOOL yields exactly 0 or -1 (VARIANT_TRUE), never 1 */
    REQUIRE(U(VarBoolFromI4)); REQUIRE(U(VarBoolFromR8)); REQUIRE(U(VarBoolFromUI8)); REQUIRE(U(VarBoolFromCy)); REQUIRE(U(VarBoolFromI1));
    VARIANT_BOOL b;
    static const LONG li[] = { 1, 2, -1, 0x7fffffff, (LONG)0x80000000UL, 0x10000, 256 };
    for (unsigned i = 0; i < VCN_ROWS(li); i++) { b = 0x5a5a; EXPECT_EQ(U(VarBoolFromI4)(li[i], &b), S_OK); EXPECT_EQ(b, -1); }
    b = 0x5a5a; EXPECT_EQ(U(VarBoolFromI4)(0, &b), S_OK); EXPECT_EQ(b, 0);
    /* values that vanish in a narrowing cast are still TRUE */
    b = 0; EXPECT_EQ(U(VarBoolFromI4)(0x10000, &b), S_OK); EXPECT_EQ(b, -1);
    b = 0; EXPECT_EQ(U(VarBoolFromUI8)(0x100000000ULL, &b), S_OK); EXPECT_EQ(b, -1);
    b = 0; EXPECT_EQ(U(VarBoolFromI1)(-128, &b), S_OK); EXPECT_EQ(b, -1);
    CY c; c.int64 = 1; b = 0; EXPECT_EQ(U(VarBoolFromCy)(c, &b), S_OK); EXPECT_EQ(b, -1);
    c.int64 = 0; b = 5; EXPECT_EQ(U(VarBoolFromCy)(c, &b), S_OK); EXPECT_EQ(b, 0);
    b = 0; EXPECT_EQ(U(VarBoolFromR8)(5e-324, &b), S_OK); EXPECT_EQ(b, -1);
    b = 5; EXPECT_EQ(U(VarBoolFromR8)(-0.0, &b), S_OK); EXPECT_EQ(b, 0);
    b = 0; EXPECT_EQ(U(VarBoolFromR8)(NAN, &b), S_OK); EXPECT_EQ(b, -1);      /* NaN != 0 */
    b = 0; EXPECT_EQ(U(VarBoolFromR8)(INFINITY, &b), S_OK); EXPECT_EQ(b, -1);
}

TEST(vcn_nan_inf_special_destinations)
{
    REQUIRE(U(VarR4FromR8)); REQUIRE(U(VarCyFromR8)); REQUIRE(U(VarDateFromR8)); REQUIRE(U(VarDecFromR8)); REQUIRE(U(VarR8FromR4));
    float f = 1; CY c; DATE d; DECIMAL dec;
    /* range checks are written as in-range tests, so NaN lands on the overflow side */
    EXPECT_EQ(U(VarR4FromR8)(NAN, &f), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarR4FromR8)(INFINITY, &f), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarR4FromR8)(-INFINITY, &f), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarCyFromR8)(NAN, &c), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarDateFromR8)(NAN, &d), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarDateFromR8)(INFINITY, &d), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarDecFromR8)(NAN, &dec), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarDecFromR8)(-INFINITY, &dec), DISP_E_OVERFLOW);
    double r = 0;
    EXPECT_EQ(U(VarR8FromR4)(INFINITY, &r), S_OK); CHECK(isinf(r) && r > 0);
    EXPECT_EQ(U(VarR8FromR4)(-INFINITY, &r), S_OK); CHECK(isinf(r) && r < 0);
    EXPECT_EQ(U(VarR8FromR4)(NAN, &r), S_OK); CHECK(isnan(r));
    /* R4 limits: FLT_MAX converts, tiny values underflow silently to denormal/zero */
    EXPECT_EQ(U(VarR4FromR8)(3.4028234663852886e38, &f), S_OK); CHECK(f == 3.4028234663852886e38f);
    EXPECT_EQ(U(VarR4FromR8)(-3.4028234663852886e38, &f), S_OK); CHECK(f == -3.4028234663852886e38f);
    EXPECT_EQ(U(VarR4FromR8)(3.5e38, &f), DISP_E_OVERFLOW);
    EXPECT_EQ(U(VarR4FromR8)(1e300, &f), DISP_E_OVERFLOW);
    f = 1; EXPECT_EQ(U(VarR4FromR8)(1e-50, &f), S_OK); CHECK(f == 0.0f);
    f = 1; EXPECT_EQ(U(VarR4FromR8)(1e-40, &f), S_OK); CHECK(f > 0.0f && f < 1.2e-38f);
    EXPECT_EQ(U(VarR4FromR8)(-0.0, &f), S_OK); CHECK(signbit(f) && f == 0.0f);
}
