/* bstr_core: additional VarBstrFrom* integer and ordering tests. Windows-side expectations from the documented API. */
#include "bc.h"

TEST(bstr_extra_from_ui1_table)
{
    static const struct { BYTE v; const WCHAR *s; } c[] = { {0, L"0"}, {1, L"1"}, {9, L"9"}, {10, L"10"}, {127, L"127"}, {255, L"255"} };
    size_t i;
    REQUIRE(U(VarBstrFromUI1));
    for (i = 0; i < sizeof c / sizeof c[0]; i++) {
        BSTR b = NULL;
        HRESULT hr = U(VarBstrFromUI1)(c[i].v, LOCALE_USER_DEFAULT, LOCALE_NOUSEROVERRIDE, &b);
        EXPECT_EQ(hr, S_OK);
        CHECK(b && bc_eq_w(b, c[i].s));
        BC_FREE(b);
    }
}

TEST(bstr_extra_from_i1_ui2_ui4_table)
{
    BSTR b = NULL;
    REQUIRE(U(VarBstrFromI1)); REQUIRE(U(VarBstrFromUI2)); REQUIRE(U(VarBstrFromUI4));
    EXPECT_EQ(U(VarBstrFromI1)(-128, 0x409, LOCALE_NOUSEROVERRIDE, &b), S_OK);
    CHECK(b && bc_eq_w(b, L"-128")); BC_FREE(b); b = NULL;
    EXPECT_EQ(U(VarBstrFromUI2)(65535, 0x409, LOCALE_NOUSEROVERRIDE, &b), S_OK);
    CHECK(b && bc_eq_w(b, L"65535")); BC_FREE(b); b = NULL;
    EXPECT_EQ(U(VarBstrFromUI4)(0xFFFFFFFFu, 0x409, LOCALE_NOUSEROVERRIDE, &b), S_OK);
    CHECK(b && bc_eq_w(b, L"4294967295")); BC_FREE(b);
}

TEST(bstr_extra_cmp_ordering_ascii)
{
    static const WCHAR *w[] = { L"", L"a", L"ab", L"abc", L"b" };
    int i, j;
    REQUIRE(U(VarBstrCmp)); REQUIRE(U(SysAllocString));
    for (i = 0; i < 5; i++) for (j = 0; j < 5; j++) {
        BSTR x = U(SysAllocString)(w[i]), y = U(SysAllocString)(w[j]);
        HRESULT want = i < j ? VCMP_LT : i == j ? VCMP_EQ : VCMP_GT;
        HRESULT hr = U(VarBstrCmp)(x, y, 0x409, 0);
        CHECKF(hr == want, "i=%d j=%d hr=%ld", i, j, (long)hr);
        BC_FREE(x); BC_FREE(y);
    }
}

TEST(bstr_extra_cmp_antisymmetric_ignorecase)
{
    BSTR x, y;
    REQUIRE(U(VarBstrCmp)); REQUIRE(U(SysAllocString));
    x = U(SysAllocString)(L"Hello"); y = U(SysAllocString)(L"hELLO");
    EXPECT_EQ(U(VarBstrCmp)(x, y, 0x409, NORM_IGNORECASE), VCMP_EQ);
    EXPECT_EQ(U(VarBstrCmp)(y, x, 0x409, NORM_IGNORECASE), VCMP_EQ);
    CHECK(U(VarBstrCmp)(x, y, 0x409, 0) != VCMP_EQ);
    BC_FREE(x); BC_FREE(y);
}

TEST(bstr_extra_cat_result_independent_of_inputs)
{
    BSTR a, b, r = NULL;
    REQUIRE(U(VarBstrCat)); REQUIRE(U(SysAllocString));
    a = U(SysAllocString)(L"foo"); b = U(SysAllocString)(L"bar");
    EXPECT_EQ(U(VarBstrCat)(a, b, &r), S_OK);
    CHECK(r != a && r != b);
    CHECK(r && bc_eq_w(r, L"foobar"));
    BC_FREE(a); BC_FREE(b);
    CHECK(r && bc_eq_w(r, L"foobar"));
    BC_FREE(r);
}

TEST(bstr_extra_bytelen_ansi_prefix)
{
    BSTR b;
    REQUIRE(U(SysAllocStringByteLen)); REQUIRE(U(SysStringByteLen)); REQUIRE(U(SysStringLen));
    b = U(SysAllocStringByteLen)("abcde", 5);
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(U(SysStringByteLen)(b), 5);
        EXPECT_EQ(U(SysStringLen)(b), 2);
        CHECK(!memcmp(b, "abcde", 5));
        EXPECT_EQ(((BYTE *)b)[5], 0);
        EXPECT_EQ(((BYTE *)b)[6], 0);
        BC_FREE(b);
    }
}
