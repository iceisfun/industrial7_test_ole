/* dtd_dcf_*: DispCallFunc (oleaut32, x64 DispCallFuncAmd64).
 * Decomp-verified rules: vtReturn&0xfff > 0x17 -> E_INVALIDARG (VT_VOID, VT_HRESULT, VT_RECORD ... all rejected, result
 * untouched); target address NULL (oVft with no instance, or NULL vtable slot) -> E_POINTER; cc is never examined on x64;
 * result->vt = vtReturn (DECIMAL restored after the call); arguments are copied raw (no coercion): <=8 byte types go in
 * the 64-bit arg slot, 16/24 byte types (DECIMAL, VARIANT) are copied and passed by pointer (Win64 ABI); the first four
 * argument slots are loaded into both integer and XMM registers. */
#include "dtd.h"

#define PTR(x) ((VARIANTARG *)&(x))

static LONG WINAPI nf_sum4(LONG a, LONG b, LONG c, LONG d) { return a + 10 * b + 100 * c + 1000 * d; }
static LONG WINAPI nf_sum10(LONG a0, LONG a1, LONG a2, LONG a3, LONG a4, LONG a5, LONG a6, LONG a7, LONG a8, LONG a9)
{ return a0 * 1 + a1 * 2 + a2 * 3 + a3 * 4 + a4 * 5 + a5 * 6 + a6 * 7 + a7 * 8 + a8 * 9 + a9 * 10; }
static double WINAPI nf_dmix(LONG a, double b, LONG c, double d) { return a + b * 10 + c * 100 + d * 1000; }
static double WINAPI nf_d6(double a, double b, double c, double d, double e, double f)
{ return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f; }
static double WINAPI nf_mix8(LONG a, double b, LONG c, double d, LONG e, double f, LONG g, double h)
{ return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * f + 7 * g + 8 * h; }
static float WINAPI nf_f2(float a, float b) { return a * 10.0f + b; }
static LONGLONG WINAPI nf_i64(LONGLONG a, LONGLONG b) { return a * 3 + b; }
static ULONGLONG WINAPI nf_u64big(void) { return 0xF000000000000001ULL; }
static BSTR WINAPI nf_bstrdup(BSTR s) { return bs_allocn(s, bs_len(s)); }
static void WINAPI nf_setp(LONG *p, LONG v) { *p = v; }
static LONG WINAPI nf_varvt(VARIANT v) { return (LONG)V_VT(&v); }
static LONG WINAPI nf_varerr(VARIANT v) { return V_VT(&v) == VT_ERROR ? (LONG)V_ERROR(&v) : -1; }
static VARIANT WINAPI nf_varret(LONG x) { VARIANT v; memset(&v, 0, sizeof v); V_VT(&v) = VT_I4; V_I4(&v) = x * 2; return v; }
static LONG WINAPI nf_dec(DECIMAL d) { return (LONG)d.Lo32 + (LONG)d.Hi32 * 100000 + d.scale * 10 + d.sign; }
static DECIMAL WINAPI nf_decret(LONG x) { DECIMAL d; memset(&d, 0, sizeof d); d.scale = 2; d.sign = 0; d.Hi32 = 7; d.Lo64 = (ULONGLONG)x; return d; }
static LONG WINAPI nf_cy(CY c) { return (LONG)(c.int64 / 10000); }
static SHORT WINAPI nf_ret_i2(void) { return -5; }
static BYTE WINAPI nf_ret_ui1(void) { return 200; }
static VARIANT_BOOL WINAPI nf_ret_true(void) { return VARIANT_TRUE; }
static HRESULT WINAPI nf_ret_false(void) { return S_FALSE; }
static LONG g_target = 1234;
static LONG *WINAPI nf_ret_ptr(void) { return &g_target; }
static float WINAPI nf_fret(float a) { return a * 1.5f; }
static LONG WINAPI nf_42(void) { return 42; }
static LONG WINAPI nf_bytes(BYTE a, SHORT b, BYTE c) { return a * 1 + b * 1000 + c * 1000000; }
static LONG WINAPI nf_sub(LONG a, LONG b) { return a - b; }

static VARIANT *vi4(VARIANT *v, LONG x) { v_i4(v, x); return v; }

TEST(dtd_dcf_ints_order)
{
    VARIANT a[4], r; VARIANTARG *pa[4]; VARTYPE vt[4] = { VT_I4, VT_I4, VT_I4, VT_I4 };
    HRESULT hr; int i;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 4; i++) { v_i4(&a[i], i + 1); pa[i] = &a[i]; }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sum4, CC_STDCALL, VT_I4, 4, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4);
    EXPECT_EQ(V_I4(&r), 4321);
}

TEST(dtd_dcf_ints_negative_and_extremes)
{
    VARIANT a[4], r; VARIANTARG *pa[4]; VARTYPE vt[4] = { VT_I4, VT_I4, VT_I4, VT_I4 };
    LONG in[4] = { -1, 0, 1, 0 };
    int i; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 4; i++) { v_i4(&a[i], in[i]); pa[i] = &a[i]; }
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sum4, CC_STDCALL, VT_I4, 4, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 99);
    v_i4(&a[0], 0x7fffffff); v_i4(&a[1], 0); v_i4(&a[2], 0); v_i4(&a[3], 0);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sum4, CC_STDCALL, VT_I4, 4, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 0x7fffffff);
}

TEST(dtd_dcf_ten_int_args_stack)
{
    VARIANT a[10], r; VARIANTARG *pa[10]; VARTYPE vt[10]; int i; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 10; i++) { v_i4(&a[i], i + 1); pa[i] = &a[i]; vt[i] = VT_I4; }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sum10, CC_STDCALL, VT_I4, 10, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4);
    EXPECT_EQ(V_I4(&r), 385);   /* sum (i+1)^2, i=0..9 */
}

TEST(dtd_dcf_void_return_no_args)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_42, CC_STDCALL, VT_EMPTY, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
}

TEST(dtd_dcf_no_args_i4_return)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_42, CC_STDCALL, VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 42);
}

TEST(dtd_dcf_int_and_double_slots)
{
    /* x64: slot i is passed in RCX/RDX/R8/R9 *and* XMM0-3; doubles in slots 1 and 3 land in XMM1/XMM3. */
    VARIANT a[4], r; VARIANTARG *pa[4]; VARTYPE vt[4] = { VT_I4, VT_R8, VT_I4, VT_R8 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a[0], 1); v_r8(&a[1], 2.5); v_i4(&a[2], 3); v_r8(&a[3], 4.25);
    pa[0] = &a[0]; pa[1] = &a[1]; pa[2] = &a[2]; pa[3] = &a[3];
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_dmix, CC_STDCALL, VT_R8, 4, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_R8);
    CHECK(V_R8(&r) == 1 + 25.0 + 300 + 4250.0);
}

TEST(dtd_dcf_six_doubles)
{
    VARIANT a[6], r; VARIANTARG *pa[6]; VARTYPE vt[6]; int i; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 6; i++) { v_r8(&a[i], i + 1.5); pa[i] = &a[i]; vt[i] = VT_R8; }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_d6, CC_STDCALL, VT_R8, 6, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_R8);
    CHECK(V_R8(&r) == 1.5 + 2 * 2.5 + 3 * 3.5 + 4 * 4.5 + 5 * 5.5 + 6 * 6.5);
}

TEST(dtd_dcf_eight_mixed_args_stack_doubles)
{
    VARIANT a[8], r; VARIANTARG *pa[8]; VARTYPE vt[8]; int i; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 8; i++) {
        if (i & 1) { v_r8(&a[i], i + 0.5); vt[i] = VT_R8; } else { v_i4(&a[i], i); vt[i] = VT_I4; }
        pa[i] = &a[i];
    }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_mix8, CC_STDCALL, VT_R8, 8, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    CHECK(V_R8(&r) == 0 + 2 * 1.5 + 3 * 2 + 4 * 3.5 + 5 * 4 + 6 * 5.5 + 7 * 6 + 8 * 7.5);
}

TEST(dtd_dcf_float_args_and_return)
{
    VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_R4, VT_R4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a[0]); V_VT(&a[0]) = VT_R4; V_R4(&a[0]) = 2.0f;
    vinit(&a[1]); V_VT(&a[1]) = VT_R4; V_R4(&a[1]) = 0.5f;
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_f2, CC_STDCALL, VT_R4, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_R4);
    CHECK(V_R4(&r) == 20.5f);
}

TEST(dtd_dcf_float_return_single_arg)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_R4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a); V_VT(&a) = VT_R4; V_R4(&a) = -3.0f;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_fret, CC_STDCALL, VT_R4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_R4); CHECK(V_R4(&r) == -4.5f);
}

TEST(dtd_dcf_int64_args_and_return)
{
    VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I8, VT_I8 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a[0]); V_VT(&a[0]) = VT_I8; V_I8(&a[0]) = 0x100000001LL;
    vinit(&a[1]); V_VT(&a[1]) = VT_I8; V_I8(&a[1]) = -7;
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_i64, CC_STDCALL, VT_I8, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I8);
    CHECK(V_I8(&r) == 0x300000003LL - 7);
}

TEST(dtd_dcf_uint64_return)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_u64big, CC_STDCALL, VT_UI8, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_UI8);
    CHECK(V_UI8(&r) == 0xF000000000000001ULL);
}

TEST(dtd_dcf_bstr_arg_and_return)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_BSTR }; HRESULT hr;
    BSTR in;
    REQUIRE(OA(DispCallFunc));
    in = bs_alloc(L"dispcallfunc");
    vinit(&a); V_VT(&a) = VT_BSTR; V_BSTR(&a) = in;
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_bstrdup, CC_STDCALL, VT_BSTR, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_BSTR);
    CHECK(V_BSTR(&r) != NULL);
    if (V_BSTR(&r)) { CHECK(V_BSTR(&r) != in); CHECK(bs_eq(V_BSTR(&r), L"dispcallfunc")); }
    bs_free(V_BSTR(&r)); bs_free(in);
}

TEST(dtd_dcf_byref_pointer_arg_written_through)
{
    VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_BYREF | VT_I4, VT_I4 };
    LONG slot = 5; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a[0]); V_VT(&a[0]) = VT_BYREF | VT_I4; V_I4REF(&a[0]) = &slot;
    v_i4(&a[1], 999);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_setp, CC_STDCALL, VT_EMPTY, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(slot, 999);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
}

TEST(dtd_dcf_small_int_types_in_slots)
{
    VARIANT a[3], r; VARIANTARG *pa[3] = { &a[0], &a[1], &a[2] }; VARTYPE vt[3] = { VT_UI1, VT_I2, VT_UI1 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a[0]); V_VT(&a[0]) = VT_UI1; V_UI1(&a[0]) = 0xF0;
    vinit(&a[1]); V_VT(&a[1]) = VT_I2; V_I2(&a[1]) = -2;
    vinit(&a[2]); V_VT(&a[2]) = VT_UI1; V_UI1(&a[2]) = 3;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_bytes, CC_STDCALL, VT_I4, 3, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 240 - 2000 + 3000000);
}

TEST(dtd_dcf_variant_arg_passed_by_pointer_copy)
{
    /* VT_VARIANT: the whole 24-byte VARIANT is copied and passed by hidden pointer (Win64 by-value struct). */
    VARIANT in, r; VARIANTARG *pa[1] = { &in }; VARTYPE vt[1] = { VT_VARIANT }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&in); V_VT(&in) = VT_I4; V_I4(&in) = 5;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varvt, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), VT_I4);
    vinit(&in); V_VT(&in) = VT_R8; V_R8(&in) = 1.0;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varvt, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), VT_R8);
    vinit(&in); V_VT(&in) = VT_NULL;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varvt, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), VT_NULL);
    vinit(&in); V_VT(&in) = VT_BYREF | VT_BSTR;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varvt, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), VT_BYREF | VT_BSTR);
}

TEST(dtd_dcf_variant_arg_error_scode_preserved)
{
    VARIANT in, r; VARIANTARG *pa[1] = { &in }; VARTYPE vt[1] = { VT_VARIANT }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&in); V_VT(&in) = VT_ERROR; V_ERROR(&in) = DISP_E_PARAMNOTFOUND;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varerr, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ((ULONG)V_I4(&r), 0x80020004);
}

TEST(dtd_dcf_variant_return_hidden_pointer)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_I4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a, 21);
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_varret, CC_STDCALL, VT_VARIANT, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4);   /* callee wrote the VARIANT in place */
    EXPECT_EQ(V_I4(&r), 42);
}

TEST(dtd_dcf_decimal_arg_by_pointer_copy)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_DECIMAL }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&a, 0, sizeof a);
    V_DECIMAL(&a).Hi32 = 3; V_DECIMAL(&a).Lo64 = 77; V_DECIMAL(&a).scale = 4; V_DECIMAL(&a).sign = 0;
    V_VT(&a) = VT_DECIMAL;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_dec, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 77 + 3 * 100000 + 4 * 10);
}

TEST(dtd_dcf_decimal_return_vt_restored)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_I4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a, 1234);
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_decret, CC_STDCALL, VT_DECIMAL, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_VT(&r), VT_DECIMAL);   /* the DECIMAL overlays vt; DispCallFunc restores it after the call */
    EXPECT_EQ(V_DECIMAL(&r).Hi32, 7);
    CHECK(V_DECIMAL(&r).Lo64 == 1234);
    EXPECT_EQ(V_DECIMAL(&r).scale, 2);
}

TEST(dtd_dcf_currency_arg)
{
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_CY }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    vinit(&a); V_VT(&a) = VT_CY; V_CY(&a).int64 = (LONGLONG)10000 * 12345;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_cy, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), 12345);
}

TEST(dtd_dcf_return_small_types_vt_tag)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_i2, CC_STDCALL, VT_I2, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I2); EXPECT_EQ(V_I2(&r), -5);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_ui1, CC_STDCALL, VT_UI1, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_UI1); EXPECT_EQ(V_UI1(&r), 200);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_true, CC_STDCALL, VT_BOOL, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_BOOL); EXPECT_EQ(V_BOOL(&r), VARIANT_TRUE);
}

TEST(dtd_dcf_return_vt_error_carries_scode)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_false, CC_STDCALL, VT_ERROR, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_ERROR); EXPECT_EQ(V_ERROR(&r), S_FALSE);
}

TEST(dtd_dcf_return_byref_and_array_flags_are_pointer_sized)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_ptr, CC_STDCALL, VT_BYREF | VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_BYREF | VT_I4); EXPECT_EQ(V_I4REF(&r), &g_target);
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_ptr, CC_STDCALL, VT_ARRAY | VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_ARRAY | VT_I4); CHECK((void *)V_ARRAY(&r) == (void *)&g_target);
}

TEST(dtd_dcf_return_vt_tag_for_all_valid_types)
{
    /* every vtReturn in 0..0x17 is accepted and tagged in the result; the callee returns 42 / ignores hidden pointers. */
    VARTYPE t; int bad = 0;
    REQUIRE(OA(DispCallFunc));
    for (t = 0; t <= 0x17; t++) {
        VARIANT r; HRESULT hr;
        memset(&r, 0xcc, sizeof r);
        hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_42, CC_STDCALL, t, 0, NULL, NULL, &r);
        CHECKF(hr == S_OK, "vt=%u hr=%08lx", (unsigned)t, (unsigned long)hr);
        CHECKF(V_VT(&r) == t, "vt=%u tag=%u", (unsigned)t, (unsigned)V_VT(&r));
        (void)bad;
    }
}

TEST(dtd_dcf_return_i4_value_for_integer_vts)
{
    static const VARTYPE ints[] = { VT_I4, VT_UI4, VT_INT, VT_UINT, VT_ERROR };
    unsigned i;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < sizeof ints / sizeof ints[0]; i++) {
        VARIANT r; HRESULT hr;
        memset(&r, 0, sizeof r);
        hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_42, CC_STDCALL, ints[i], 0, NULL, NULL, &r);
        CHECKF(hr == S_OK && V_VT(&r) == ints[i] && V_I4(&r) == 42, "vt=%u", (unsigned)ints[i]);
    }
}

TEST(dtd_dcf_invalid_return_vt_is_einvalidarg_and_result_untouched)
{
    static const VARTYPE bad[] = { VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, VT_LPSTR,
                                   VT_LPWSTR, VT_RECORD, VT_INT_PTR, VT_UINT_PTR, VT_FILETIME, VT_BLOB, VT_STREAM,
                                   VT_STORAGE, VT_STREAMED_OBJECT, VT_STORED_OBJECT, VT_BLOB_OBJECT, VT_CF, VT_CLSID,
                                   0x0fff, 0xffff, VT_BYREF | VT_VOID, VT_ARRAY | VT_RECORD, VT_BYREF | VT_HRESULT };
    unsigned i;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        VARIANT r; HRESULT hr;
        memset(&r, 0xcc, sizeof r);
        hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_42, CC_STDCALL, bad[i], 0, NULL, NULL, &r);
        CHECKF(hr == E_INVALIDARG, "vtReturn=0x%x hr=%08lx", (unsigned)bad[i], (unsigned long)hr);
        CHECKF(V_VT(&r) == 0xcccc, "vtReturn=0x%x result touched vt=0x%x", (unsigned)bad[i], (unsigned)V_VT(&r));
    }
}

TEST(dtd_dcf_null_target_is_epointer)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    if (t_is_wine()) { t_skip("emulator calls address 0"); return; }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(NULL, 0, CC_STDCALL, VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, HR_E_POINTER);
    EXPECT_EQ(V_VT(&r), 0xcccc);
}

TEST(dtd_dcf_instance_null_vtable_slot_is_epointer)
{
    const void *const vt0[4] = { NULL, NULL, NULL, NULL };
    const void *const *inst = vt0;
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    if (t_is_wine()) { t_skip("emulator calls address 0"); return; }
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(&inst, 3 * sizeof(void *), CC_STDCALL, VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, HR_E_POINTER);
    EXPECT_EQ(V_VT(&r), 0xcccc);
}

TEST(dtd_dcf_bad_return_vt_checked_before_target)
{
    VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    if (t_is_wine()) { t_skip("emulator calls address 0"); return; }
    hr = OA(DispCallFunc)(NULL, 0, CC_STDCALL, VT_VOID, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, E_INVALIDARG);   /* vtReturn is validated first, then the target address */
}

TEST(dtd_dcf_cc_is_ignored_on_x64)
{
    static const CALLCONV ccs[] = { CC_CDECL, CC_PASCAL, CC_MACPASCAL, CC_STDCALL, CC_FPFASTCALL, CC_SYSCALL, CC_MPWCDECL,
                                    CC_MPWPASCAL, CC_MAX, (CALLCONV)0x1234, (CALLCONV)-1 };
    unsigned i;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < sizeof ccs / sizeof ccs[0]; i++) {
        VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I4, VT_I4 }; HRESULT hr;
        v_i4(&a[0], 50); v_i4(&a[1], 8);
        memset(&r, 0, sizeof r);
        hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sub, ccs[i], VT_I4, 2, vt, pa, &r);
        CHECKF(hr == S_OK && V_I4(&r) == 42, "cc=%d hr=%08lx", (int)ccs[i], (unsigned long)hr);
    }
}

TEST(dtd_dcf_instance_method_calls)
{
    Demo d; VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I4, VT_I4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d);
    v_i4(&a[0], 30); v_i4(&a[1], 12);
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(&d, DV_ADD * sizeof(void *), CC_STDCALL, VT_I4, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 42);
    hr = OA(DispCallFunc)(&d, DV_SUB * sizeof(void *), CC_STDCALL, VT_I4, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), 18);
    EXPECT_EQ(d.calls, 2);
}

TEST(dtd_dcf_instance_vtable_offset_is_in_bytes)
{
    Demo d; VARIANT r; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d); d.value = 31337;
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(&d, DV_GETVALUE * 8, CC_STDCALL, VT_I4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), 31337);
    /* AddRef / Release through slot 1/2 */
    hr = OA(DispCallFunc)(&d, 1 * 8, CC_STDCALL, VT_UI4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_UI4(&r), 2);
    hr = OA(DispCallFunc)(&d, 2 * 8, CC_STDCALL, VT_UI4, 0, NULL, NULL, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_UI4(&r), 1);
}

TEST(dtd_dcf_instance_double_and_float_returns)
{
    Demo d; VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_R8, VT_I4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d);
    v_r8(&a[0], 1.25); v_i4(&a[1], 8);
    hr = OA(DispCallFunc)(&d, DV_SCALE * 8, CC_STDCALL, VT_R8, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_R8); CHECK(V_R8(&r) == 10.0);
    vt[0] = VT_R4; vinit(&a[0]); V_VT(&a[0]) = VT_R4; V_R4(&a[0]) = 9.0f;
    hr = OA(DispCallFunc)(&d, DV_HALF * 8, CC_STDCALL, VT_R4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_R4); CHECK(V_R4(&r) == 4.5f);
}

TEST(dtd_dcf_instance_bstr_method)
{
    Demo d; VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_BSTR, VT_BSTR }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d);
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"foo");
    vinit(&a[1]); V_VT(&a[1]) = VT_BSTR; V_BSTR(&a[1]) = bs_alloc(L"bar!");
    memset(&r, 0, sizeof r);
    hr = OA(DispCallFunc)(&d, DV_CONCAT * 8, CC_STDCALL, VT_BSTR, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_BSTR);
    CHECK(bs_eq(V_BSTR(&r), L"foobar!"));
    EXPECT_EQ(bs_len(V_BSTR(&r)), 7);
    bs_free(V_BSTR(&r)); bs_free(V_BSTR(&a[0])); bs_free(V_BSTR(&a[1]));
}

TEST(dtd_dcf_instance_variant_arg_method)
{
    Demo d; VARIANT in, r; VARIANTARG *pa[1] = { &in }; VARTYPE vt[1] = { VT_VARIANT }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d);
    v_i4(&in, 7);
    hr = OA(DispCallFunc)(&d, DV_VARVT * 8, CC_STDCALL, VT_I4, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_I4(&r), VT_I4 | (7 << 16));
}

TEST(dtd_dcf_instance_int64_method)
{
    Demo d; VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I8, VT_I8 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    Demo_init(&d);
    vinit(&a[0]); V_VT(&a[0]) = VT_I8; V_I8(&a[0]) = 3000000000LL;
    vinit(&a[1]); V_VT(&a[1]) = VT_I8; V_I8(&a[1]) = 3;
    hr = OA(DispCallFunc)(&d, DV_MUL64 * 8, CC_STDCALL, VT_I8, 2, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_I8); CHECK(V_I8(&r) == 9000000000LL);
}

static void WINAPI inst_varret(void *self, VARIANT *ret, LONG a)
{
    /* Win64 method returning a VARIANT: (this, hidden ret, args) */
    memset(ret, 0, sizeof *ret); V_VT(ret) = VT_UI4; V_UI4(ret) = 4000 + a;
}

TEST(dtd_dcf_instance_variant_return_hidden_pointer_after_this)
{
    const void *const vtab[4] = { NULL, NULL, NULL, inst_varret };
    const void *const *inst = vtab;
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_I4 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a, 5);
    memset(&r, 0xcc, sizeof r);
    hr = OA(DispCallFunc)(&inst, 3 * 8, CC_STDCALL, VT_VARIANT, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_UI4); EXPECT_EQ(V_UI4(&r), 4005);
}

TEST(dtd_dcf_args_not_modified)
{
    VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I4, VT_I4 }; VARIANT c0, c1;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a[0], 9); v_i4(&a[1], 4);
    c0 = a[0]; c1 = a[1];
    OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sub, CC_STDCALL, VT_I4, 2, vt, pa, &r);
    CHECK(!memcmp(&c0, &a[0], sizeof c0));
    CHECK(!memcmp(&c1, &a[1], sizeof c1));
}

TEST(dtd_dcf_repeat_calls_stable)
{
    VARIANT a[2], r; VARIANTARG *pa[2] = { &a[0], &a[1] }; VARTYPE vt[2] = { VT_I4, VT_I4 }; int i, ok = 0;
    REQUIRE(OA(DispCallFunc));
    for (i = 0; i < 2000; i++) {
        v_i4(&a[0], i); v_i4(&a[1], 1);
        if (OA(DispCallFunc)(NULL, (ULONG_PTR)nf_sub, CC_STDCALL, VT_I4, 2, vt, pa, &r) == S_OK && V_I4(&r) == i - 1) ok++;
    }
    EXPECT_EQ(ok, 2000);
}

TEST(dtd_dcf_arg_vt_selects_size_not_variant_vt)
{
    /* prgvt (not the VARIANT's own vt) decides how the slot is read: pass a VT_I4 variant described as VT_I2. */
    VARIANT a, r; VARIANTARG *pa[3] = { &a, &a, &a }; VARTYPE vt[3] = { VT_UI1, VT_I2, VT_UI1 }; HRESULT hr;
    REQUIRE(OA(DispCallFunc));
    v_i4(&a, 0x00010002);
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_bytes, CC_STDCALL, VT_I4, 3, vt, pa, &r);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(V_I4(&r), 2 + 2 * 1000 + 2 * 1000000);
}

TEST(dtd_dcf_extra_variant_types_as_pointers)
{
    /* BSTR/DISPATCH/UNKNOWN/BYREF args are pointer-sized raw values */
    VARIANT a, r; VARIANTARG *pa[1] = { &a }; VARTYPE vt[1] = { VT_UNKNOWN }; HRESULT hr;
    LONG dummy;
    REQUIRE(OA(DispCallFunc));
    vinit(&a); V_VT(&a) = VT_UNKNOWN; V_UNKNOWN(&a) = (IUnknown *)(void *)&dummy;
    hr = OA(DispCallFunc)(NULL, (ULONG_PTR)nf_ret_ptr, CC_STDCALL, VT_UNKNOWN, 1, vt, pa, &r);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_UNKNOWN); CHECK((void *)V_UNKNOWN(&r) == (void *)&g_target);
}
