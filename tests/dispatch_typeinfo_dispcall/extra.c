/* dtd_x_*: additional DispCallFunc / DispGetParam / hash edge cases (reviewer additions). */
#include "dtd.h"

static LONG WINAPI xf_sub(LONG a, LONG b) { return a - b; }
static LONG g_called;
static void WINAPI xf_void(void) { g_called++; }
static LONG WINAPI xf_ret7(void) { return 7; }

TEST(dtd_x_dcf_vt_empty_return_calls_void_function)
{
    VARIANT r; HRESULT hr;
    REQUIRE(UIN("oleaut32.dll", DispCallFunc));
    g_called = 0;
    memset(&r, 0xcc, sizeof r);
    hr = UIN("oleaut32.dll", DispCallFunc)(NULL, (ULONG_PTR)xf_void, CC_STDCALL, VT_EMPTY, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(g_called, 1);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
}

TEST(dtd_x_dcf_zero_args_ignores_null_arrays)
{
    VARIANT r; HRESULT hr;
    REQUIRE(UIN("oleaut32.dll", DispCallFunc));
    memset(&r, 0, sizeof r);
    hr = UIN("oleaut32.dll", DispCallFunc)(NULL, (ULONG_PTR)xf_ret7, CC_STDCALL, VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4);
    EXPECT_EQ(V_I4(&r), 7);
}

TEST(dtd_x_dcf_arg_order_first_is_first_parameter)
{
    VARIANT a, b, r; VARIANTARG *pa[2]; VARTYPE vt[2] = { VT_I4, VT_I4 }; HRESULT hr;
    REQUIRE(UIN("oleaut32.dll", DispCallFunc));
    memset(&a, 0, sizeof a); V_VT(&a) = VT_I4; V_I4(&a) = 100;
    memset(&b, 0, sizeof b); V_VT(&b) = VT_I4; V_I4(&b) = 1;
    pa[0] = &a; pa[1] = &b;
    memset(&r, 0, sizeof r);
    hr = UIN("oleaut32.dll", DispCallFunc)(NULL, (ULONG_PTR)xf_sub, CC_STDCALL, VT_I4, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 99);
}

TEST(dtd_x_hash_syskind_mac_differs_only_in_low_bit_class)
{
    unsigned long w, m;
    REQUIRE(UIN("oleaut32.dll", LHashValOfNameSysA));
    w = UIN("oleaut32.dll", LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello");
    m = UIN("oleaut32.dll", LHashValOfNameSysA)(SYS_MAC, 0x409, "Hello");
    EXPECT_EQ(m ^ w, 0x10000);
    t_note("win32 hash %08lx mac hash %08lx", w, m);
}
