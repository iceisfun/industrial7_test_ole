/* bstr_core: BstrFromVector / VectorFromBstr / GetAltMonthNames.
 * Decomp-verified (Win10 x64 oleaut32):
 *  BstrFromVector(psa,&b): *b=NULL first; psa==NULL -> S_OK/NULL; cDims!=1 or cbElements!=1 -> DISP_E_TYPEMISMATCH
 *    (0x80020005); pvData==NULL or cElements==0 -> S_OK with NULL BSTR; else SysAllocStringByteLen(pvData,cElements).
 *  VectorFromBstr(b,&psa): *psa=NULL first; SafeArrayCreate(VT_UI1,1,{lbound 0, cElements=SysStringByteLen(b)}), data copied;
 *    NULL bstr gives a 0-element array. */
#include "bc.h"

static SAFEARRAY *mkvec(VARTYPE vt, const void *data, ULONG n)
{
    SAFEARRAY *psa = U(SafeArrayCreateVector)(vt, 0, n);
    if (psa && n && data) {
        void *p = NULL;
        if (SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p) { memcpy(p, data, n * (vt == VT_I2 ? 2 : vt == VT_I4 ? 4 : 1)); U(SafeArrayUnaccessData)(psa); }
    }
    return psa;
}

/* ------------------------ BstrFromVector ------------------------ */
TEST(bstr_fromvector_null_psa)
{
    BSTR b = (BSTR)(ULONG_PTR)0x10;
    HRESULT hr;
    REQUIRE(U(BstrFromVector));
    hr = U(BstrFromVector)(NULL, &b);
    EXPECT_EQ(hr, S_OK);
    CHECK(b == NULL);
}

TEST(bstr_fromvector_empty_vector_gives_null)
{
    SAFEARRAY *psa;
    BSTR b = (BSTR)(ULONG_PTR)0x10;
    HRESULT hr;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy));
    psa = U(SafeArrayCreateVector)(VT_UI1, 0, 0);
    CHECK(psa != NULL); if (!psa) return;
    hr = U(BstrFromVector)(psa, &b);
    EXPECT_EQ(hr, S_OK);
    CHECK(b == NULL);
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_bytes)
{
    unsigned char data[11] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    SAFEARRAY *psa;
    BSTR b = NULL;
    HRESULT hr;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    psa = mkvec(VT_UI1, data, 11);
    CHECK(psa != NULL); if (!psa) return;
    hr = U(BstrFromVector)(psa, &b);
    EXPECT_EQ(hr, S_OK);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(U(SysStringByteLen)(b), 11);
        EXPECT_EQ(U(SysStringLen)(b), 5);
        CHECK(!memcmp(b, data, 11));
        EXPECT_EQ(((unsigned char *)b)[11], 0);
        EXPECT_EQ(((unsigned char *)b)[12], 0);
        EXPECT_EQ(((unsigned char *)b)[13], 0);
        U(SysFreeString)(b);
    }
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_size_sweep)
{
    unsigned char data[70];
    UINT n;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    bc_fill(data, sizeof(data), 99);
    for (n = 1; n <= 64; n++) {
        SAFEARRAY *psa = mkvec(VT_UI1, data, n);
        BSTR b = NULL;
        CHECK(psa != NULL); if (!psa) continue;
        EXPECT_EQ(U(BstrFromVector)(psa, &b), S_OK);
        CHECKF(b != NULL && BLEN(b) == n && !memcmp(b, data, n) && ((unsigned char *)b)[n] == 0, "n=%u", n);
        U(SysFreeString)(b);
        U(SafeArrayDestroy)(psa);
    }
}

TEST(bstr_fromvector_vt_i1_accepted)
{
    /* only dims==1 and cbElements==1 are checked; the element vt is irrelevant */
    SAFEARRAY *psa;
    BSTR b = NULL;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    psa = mkvec(VT_I1, "abcd", 4);
    CHECK(psa != NULL); if (!psa) return;
    EXPECT_EQ(U(BstrFromVector)(psa, &b), S_OK);
    CHECK(b != NULL && BLEN(b) == 4 && !memcmp(b, "abcd", 4));
    U(SysFreeString)(b);
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_wrong_element_size)
{
    VARTYPE vts[] = { VT_I2, VT_I4, VT_R8, VT_BSTR };
    unsigned i;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy));
    for (i = 0; i < sizeof(vts) / sizeof(vts[0]); i++) {
        SAFEARRAY *psa = U(SafeArrayCreateVector)(vts[i], 0, 4);
        BSTR b = (BSTR)(ULONG_PTR)0x20;
        HRESULT hr;
        CHECK(psa != NULL); if (!psa) continue;
        hr = U(BstrFromVector)(psa, &b);
        CHECKF(hr == BC_E_TYPEMISMATCH, "vt=%u hr=0x%08lx", vts[i], (unsigned long)hr);
        CHECKF(b == NULL, "vt=%u output must be cleared", vts[i]);
        U(SafeArrayDestroy)(psa);
    }
}

TEST(bstr_fromvector_wrong_dims)
{
    SAFEARRAYBOUND sab[2] = { { 3, 0 }, { 3, 0 } };
    SAFEARRAY *psa;
    BSTR b = (BSTR)(ULONG_PTR)0x20;
    HRESULT hr;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayDestroy));
    psa = U(SafeArrayCreate)(VT_UI1, 2, sab);
    CHECK(psa != NULL); if (!psa) return;
    hr = U(BstrFromVector)(psa, &b);
    EXPECT_EQ(hr, BC_E_TYPEMISMATCH);
    CHECK(b == NULL);
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_nonzero_lbound)
{
    SAFEARRAYBOUND sab = { 4, 5 };
    SAFEARRAY *psa;
    BSTR b = NULL;
    void *p = NULL;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    psa = U(SafeArrayCreate)(VT_UI1, 1, &sab);
    CHECK(psa != NULL); if (!psa) return;
    CHECK(SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p);
    memcpy(p, "wxyz", 4);
    U(SafeArrayUnaccessData)(psa);
    EXPECT_EQ(U(BstrFromVector)(psa, &b), S_OK);
    CHECK(b != NULL && BLEN(b) == 4 && !memcmp(b, "wxyz", 4));
    U(SysFreeString)(b);
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_copy_is_independent)
{
    unsigned char data[6] = { 'a', 'b', 'c', 'd', 'e', 'f' };
    SAFEARRAY *psa;
    BSTR b = NULL;
    void *p = NULL;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    psa = mkvec(VT_UI1, data, 6);
    CHECK(psa != NULL); if (!psa) return;
    EXPECT_EQ(U(BstrFromVector)(psa, &b), S_OK);
    U(SafeArrayAccessData)(psa, &p);
    ((char *)p)[0] = 'Z';
    U(SafeArrayUnaccessData)(psa);
    CHECK(b != NULL && ((char *)b)[0] == 'a');
    U(SysFreeString)(b);
    U(SafeArrayDestroy)(psa);
}

TEST(bstr_fromvector_embedded_zero_bytes)
{
    unsigned char data[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    SAFEARRAY *psa;
    BSTR b = NULL;
    REQUIRE(U(BstrFromVector)); REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    psa = mkvec(VT_UI1, data, 8);
    CHECK(psa != NULL); if (!psa) return;
    EXPECT_EQ(U(BstrFromVector)(psa, &b), S_OK);
    CHECK(b != NULL && U(SysStringByteLen)(b) == 8 && U(SysStringLen)(b) == 4);
    U(SysFreeString)(b);
    U(SafeArrayDestroy)(psa);
}

/* ------------------------ VectorFromBstr ------------------------ */
static void check_vec_shape(SAFEARRAY *psa, LONG nbytes, const char *what)
{
    LONG lb = 99, ub = 99;
    CHECKF(psa != NULL, "%s", what); if (!psa) return;
    EXPECT_EQ(U(SafeArrayGetDim)(psa), 1);
    EXPECT_EQ(U(SafeArrayGetElemsize)(psa), 1);
    CHECKF(U(SafeArrayGetLBound)(psa, 1, &lb) == S_OK && lb == 0, "%s lbound %ld", what, lb);
    CHECKF(U(SafeArrayGetUBound)(psa, 1, &ub) == S_OK && ub == nbytes - 1, "%s ubound %ld want %ld", what, ub, nbytes - 1);
    {
        VARTYPE vt = 0;
        HRESULT hr = U(SafeArrayGetVartype)(psa, &vt);
        CHECKF(hr == S_OK && vt == VT_UI1, "%s vt=%u hr=0x%lx", what, vt, (unsigned long)hr);
    }
}

#define NEED_SA() do { REQUIRE(U(VectorFromBstr)); REQUIRE(U(SafeArrayGetDim)); REQUIRE(U(SafeArrayGetElemsize)); REQUIRE(U(SafeArrayGetLBound)); \
    REQUIRE(U(SafeArrayGetUBound)); REQUIRE(U(SafeArrayGetVartype)); REQUIRE(U(SafeArrayDestroy)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData)); } while (0)

TEST(bstr_fromvector_null_bstr_gives_empty_array)
{
    SAFEARRAY *psa = (SAFEARRAY *)(ULONG_PTR)0x10;
    NEED_SA();
    EXPECT_EQ(U(VectorFromBstr)(NULL, &psa), S_OK);
    check_vec_shape(psa, 0, "NULL bstr");
    if (psa) U(SafeArrayDestroy)(psa);
}

TEST(bstr_vectorfrombstr_empty_bstr)
{
    SAFEARRAY *psa = NULL;
    BSTR b;
    NEED_SA(); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(VectorFromBstr)(b, &psa), S_OK);
    check_vec_shape(psa, 0, "empty bstr");
    if (psa) U(SafeArrayDestroy)(psa);
    U(SysFreeString)(b);
}

TEST(bstr_vectorfrombstr_wide_string)
{
    SAFEARRAY *psa = NULL;
    BSTR b;
    void *p = NULL;
    NEED_SA(); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    b = U(SysAllocString)(L"abc");
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(VectorFromBstr)(b, &psa), S_OK);
    check_vec_shape(psa, 6, "abc");
    if (psa) {
        CHECK(SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p && !memcmp(p, L"abc", 6));
        U(SafeArrayUnaccessData)(psa);
        U(SafeArrayDestroy)(psa);
    }
    U(SysFreeString)(b);
}

TEST(bstr_vectorfrombstr_odd_bytelen)
{
    SAFEARRAY *psa = NULL;
    BSTR b;
    void *p = NULL;
    NEED_SA(); REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringByteLen)("\x01\x02\x03\x04\x05", 5);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(VectorFromBstr)(b, &psa), S_OK);
    check_vec_shape(psa, 5, "5 bytes");
    if (psa) {
        CHECK(SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p && !memcmp(p, "\x01\x02\x03\x04\x05", 5));
        U(SafeArrayUnaccessData)(psa);
        U(SafeArrayDestroy)(psa);
    }
    U(SysFreeString)(b);
}

TEST(bstr_vectorfrombstr_embedded_nul_chars)
{
    static const WCHAR w[] = { 0, 'a', 0, 0 };
    SAFEARRAY *psa = NULL;
    BSTR b;
    void *p = NULL;
    NEED_SA(); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysFreeString));
    b = U(SysAllocStringLen)(w, 4);
    CHECK(b != NULL); if (!b) return;
    EXPECT_EQ(U(VectorFromBstr)(b, &psa), S_OK);
    check_vec_shape(psa, 8, "4 wchars");
    if (psa) {
        CHECK(SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p && !memcmp(p, w, 8));
        U(SafeArrayUnaccessData)(psa);
        U(SafeArrayDestroy)(psa);
    }
    U(SysFreeString)(b);
}

TEST(bstr_vector_roundtrip_sweep)
{
    unsigned char data[300];
    UINT n;
    NEED_SA(); REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysFreeString)); REQUIRE(U(BstrFromVector));
    bc_fill(data, sizeof(data), 1234);
    for (n = 1; n <= 260; n += (n < 40 ? 1 : 11)) {
        BSTR b = U(SysAllocStringByteLen)((LPCSTR)data, n), b2 = NULL;
        SAFEARRAY *psa = NULL;
        void *p = NULL;
        CHECKF(b != NULL, "n=%u", n); if (!b) continue;
        EXPECT_EQ(U(VectorFromBstr)(b, &psa), S_OK);
        CHECKF(psa != NULL, "n=%u", n);
        if (psa) {
            CHECK(SUCCEEDED(U(SafeArrayAccessData)(psa, &p)) && p && !memcmp(p, data, n));
            U(SafeArrayUnaccessData)(psa);
            EXPECT_EQ(U(BstrFromVector)(psa, &b2), S_OK);
            CHECKF(b2 != NULL && b2 != b && BLEN(b2) == n && !memcmp(b2, data, n), "n=%u roundtrip", n);
            U(SysFreeString)(b2);
            U(SafeArrayDestroy)(psa);
        }
        U(SysFreeString)(b);
    }
}

/* ------------------------ GetAltMonthNames ------------------------ */
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(bstr_altmonth_none_for_most_locales)
{
    LCID lcids[] = { 0, 0x409, 0x407, 0x40C, 0x411, 0x804, 0x400, 0x800, 0x0C0A, 0xFFFF };
    unsigned i;
    REQUIRE(U(GetAltMonthNames));
    (void)0;
    for (i = 0; i < sizeof(lcids) / sizeof(lcids[0]); i++) {
        LPOLESTR *p = (LPOLESTR *)(ULONG_PTR)0xdeadbeef;
        HRESULT hr = U(GetAltMonthNames)(lcids[i], &p);
        CHECKF(hr == S_OK, "lcid=0x%lx hr=0x%lx", (unsigned long)lcids[i], (unsigned long)hr);
        CHECKF(p == NULL, "lcid=0x%lx got %p", (unsigned long)lcids[i], p);
    }
}

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(bstr_altmonth_arabic_russian_polish)
{
    struct { LCID lcid; const char *n; } t[] = {
        { 0x0401, "ar-SA" }, { 0x0801, "ar-IQ" }, { 0x0C01, "ar-EG" }, { 0x3801, "ar-AE" },
        { 0x0419, "ru-RU" }, { 0x0415, "pl-PL" },
        { 0x10401, "ar-SA alt sort" }, { 0x10419, "ru-RU alt sort" }, { 0x0815, "pl other sublang" },
    };
    LPOLESTR *first[9] = { 0 };
    unsigned i;
    REQUIRE(U(GetAltMonthNames));
    for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        LPOLESTR *p = NULL, *q = NULL;
        HRESULT hr = U(GetAltMonthNames)(t[i].lcid, &p);
        CHECKF(hr == S_OK && p != NULL, "%s hr=0x%lx p=%p", t[i].n, (unsigned long)hr, p);
        hr = U(GetAltMonthNames)(t[i].lcid, &q);
        CHECKF(hr == S_OK && q == p, "%s second call returns the same table", t[i].n);
        first[i] = p;
    }
    /* same primary language -> same table (only the low 10 bits are used) */
    CHECK(first[0] == first[1] && first[0] == first[2] && first[0] == first[3] && first[0] == first[6]);
    CHECK(first[4] == first[7]);
    CHECK(first[5] == first[8]);
    CHECK(first[0] != first[4] && first[0] != first[5] && first[4] != first[5]);
}

TEST(bstr_altmonth_table_contents_note)
{
    struct { LCID lcid; const char *n; } t[] = { { 0x0401, "Hijri" }, { 0x0419, "Russian" }, { 0x0415, "Polish" } };
    unsigned i, m;
    REQUIRE(U(GetAltMonthNames));
    for (i = 0; i < 3; i++) {
        LPOLESTR *p = NULL;
        U(GetAltMonthNames)(t[i].lcid, &p);
        if (!p) continue;
        for (m = 0; m < 12; m++) {
            char buf[128] = "";
            if (p[m]) WideCharToMultiByte(CP_UTF8, 0, p[m], -1, buf, sizeof(buf), NULL, NULL);
            t_note("%s month[%u] = %s", t[i].n, m, p[m] ? buf : "(null)");
        }
    }
}
