/* dtd_get_*: DispGetParam;  dtd_hash_*: LHashValOfNameSys / LHashValOfNameSysA;  dtd_oabv_*: OaBuildVersion.
 * DispGetParam (decomp): IndexOfParam(pdispparams, position) then VariantChangeTypeEx(result, &rgvarg[idx], lcid 0x400, 0, vt).
 *   IndexOfParam: if position equals rgdispidNamedArgs[i] -> idx = i; else idx = cArgs - position - 1 and idx < cNamedArgs
 *   (slot reserved for a named arg) or idx < 0 -> DISP_E_PARAMNOTFOUND. *puArgErr = idx only when the coercion fails.
 * LHashValOfNameSysA (decomp + table dump from oleaut32): low word = ((h = 0xdeadbee; h = h*37 + tab[c]) mod 65599) & 0xffff,
 *   high word from the LCID class (| 0x1 for SYS_MAC); NULL -> 0; LHashValOfNameSys converts with CP_ACP into a 256-byte buffer. */
#include "dtd.h"
#include "hashtab.inc"

static DISPPARAMS dp3(VARIANT *args, DISPID *named, UINT cargs, UINT cnamed)
{ return dp_make(args, cargs, named, cnamed); }

static void three_args(VARIANT v[3])
{   /* rgvarg[0] is the LAST-declared... no: position p maps to rgvarg[cArgs-1-p], so position 2 = rgvarg[0] = I2 42,
       position 1 = rgvarg[1] = I4 1234567890, position 0 = rgvarg[2] = BSTR "Sunshine" */
    vinit(&v[0]); V_VT(&v[0]) = VT_I2;  V_I2(&v[0]) = 42;
    vinit(&v[1]); V_VT(&v[1]) = VT_I4;  V_I4(&v[1]) = 1234567890;
    vinit(&v[2]); V_VT(&v[2]) = VT_BSTR; V_BSTR(&v[2]) = bs_alloc(L"Sunshine");
}
static void three_clear(VARIANT v[3]) { vclear(&v[0]); vclear(&v[1]); vclear(&v[2]); }

/* adapted from Wine dlls/oleaut32/tests/dispatch.c (LGPL-2.1+) */
TEST(dtd_get_zero_params_paramnotfound)
{
    DISPPARAMS dp = dp3(NULL, NULL, 0, 0); VARIANT r; UINT err; unsigned pos;
    REQUIRE(OA(DispGetParam));
    for (pos = 0; pos < 4; pos++) {
        vinit(&r); err = 0xdeadbeef;
        EXPECT_EQ(OA(DispGetParam)(&dp, pos, VT_I2, &r, &err), DISP_E_PARAMNOTFOUND);
        EXPECT_EQ(V_VT(&r), VT_EMPTY);
        EXPECT_EQ(err, 0xdeadbeef);
    }
}

TEST(dtd_get_zero_params_null_result_and_null_err)
{
    DISPPARAMS dp = dp3(NULL, NULL, 0, 0); VARIANT r; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, NULL, &err), DISP_E_PARAMNOTFOUND);
    EXPECT_EQ(err, 0xdeadbeef);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, &r, NULL), DISP_E_PARAMNOTFOUND);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
}

TEST(dtd_get_null_rgvarg_with_nonzero_cargs_is_einvalidarg)
{
    DISPPARAMS dp = dp3(NULL, NULL, 1, 0); VARIANT r; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, &r, &err), E_INVALIDARG);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    EXPECT_EQ(err, 0);
}

TEST(dtd_get_each_position_native_type)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r); err = 0xdeadbeef;
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I2, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I2); EXPECT_EQ(V_I2(&r), 42); EXPECT_EQ(err, 0xdeadbeef);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 1, VT_I4, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 1234567890);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BSTR, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_BSTR); CHECK(bs_eq(V_BSTR(&r), L"Sunshine"));
    CHECK(V_BSTR(&r) != V_BSTR(&a[2]));   /* same-type "conversion" still produces a copy */
    vclear(&r);
    three_clear(a);
}

TEST(dtd_get_position_is_end_based)
{
    /* position 0 is the LAST element of rgvarg[] (arguments are stored in reverse order) */
    VARIANT a[3], r; DISPPARAMS dp = dp3(a, NULL, 3, 0); int i;
    REQUIRE(OA(DispGetParam));
    for (i = 0; i < 3; i++) v_i4(&a[i], 100 + i);
    for (i = 0; i < 3; i++) {
        vinit(&r);
        EXPECT_EQ(OA(DispGetParam)(&dp, i, VT_I4, &r, NULL), S_OK);
        CHECKF(V_I4(&r) == 100 + (2 - i), "pos %d got %ld", i, (long)V_I4(&r));
    }
}

TEST(dtd_get_position_out_of_range)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 3, VT_I2, &r, &err), DISP_E_PARAMNOTFOUND);
    EXPECT_EQ(V_VT(&r), VT_EMPTY); EXPECT_EQ(err, 0xdeadbeef);
    EXPECT_EQ(OA(DispGetParam)(&dp, 100, VT_I2, &r, &err), DISP_E_PARAMNOTFOUND);
    three_clear(a);
}

TEST(dtd_get_null_result_is_einvalidarg_with_argerr)
{
    VARIANT a[3]; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I2, NULL, &err), E_INVALIDARG);
    EXPECT_EQ(err, 0);   /* index of the failing rgvarg element */
    err = 0xdeadbeef;
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BSTR, NULL, &err), E_INVALIDARG);
    EXPECT_EQ(err, 2);
    three_clear(a);
}

TEST(dtd_get_null_argerr_pointer_ok)
{
    VARIANT a[3], r; DISPPARAMS dp;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I2, &r, NULL), S_OK);
    EXPECT_EQ(V_I2(&r), 42);
    vinit(&r);
    if (!t_is_wine())   /* an emulator dereferences puArgErr on failure; Windows checks it for NULL */
        EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, &r, NULL), DISP_E_TYPEMISMATCH);   /* failure with NULL puArgErr */
    three_clear(a);
}

TEST(dtd_get_coerce_i2_to_i4_and_bstr)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I4, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 42); EXPECT_EQ(err, 0xdeadbeef);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_BSTR, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_BSTR); CHECK(bs_eq(V_BSTR(&r), L"42"));
    vclear(&r);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_R8, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_R8); CHECK(V_R8(&r) == 42.0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_BOOL, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_BOOL); EXPECT_EQ(V_BOOL(&r), VARIANT_TRUE);
    three_clear(a);
}

TEST(dtd_get_overflow_sets_argerr_to_rgvarg_index)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 1, VT_I2, &r, &err), DISP_E_OVERFLOW);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    EXPECT_EQ(err, 1);
    three_clear(a);
}

TEST(dtd_get_typemismatch_sets_argerr)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    EXPECT_EQ(err, 2);
    three_clear(a);
}

TEST(dtd_get_badvartype_sets_argerr)
{
    VARIANT a[3], r; DISPPARAMS dp; UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_ILLEGAL, &r, &err), DISP_E_BADVARTYPE);
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
    EXPECT_EQ(err, 0);
    three_clear(a);
}

TEST(dtd_get_empty_arg_coerces_to_bstr_and_zero)
{
    VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0); UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    vinit(&a);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BSTR, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_BSTR); EXPECT_EQ(err, 0xdeadbeef);
    vclear(&r);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, &err), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 0);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BOOL, &r, &err), S_OK);
    EXPECT_EQ(V_BOOL(&r), VARIANT_FALSE);
}

TEST(dtd_get_null_arg_cannot_coerce_to_number)
{
    VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0); UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    vinit(&a); V_VT(&a) = VT_NULL;
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, &err), DISP_E_TYPEMISMATCH);
    EXPECT_EQ(err, 0);
}

static void conv_row(VARIANT *src, VARTYPE target, HRESULT want_hr)
{
    VARIANT r; DISPPARAMS dp = dp3(src, NULL, 1, 0); UINT err = 0xdeadbeef; HRESULT hr;
    vinit(&r);
    hr = OA(DispGetParam)(&dp, 0, target, &r, &err);
    CHECKF(hr == want_hr, "src vt=%u target=%u hr=%08lx want %08lx", (unsigned)V_VT(src), (unsigned)target,
           (unsigned long)hr, (unsigned long)want_hr);
    if (hr == S_OK) { CHECK(V_VT(&r) == target); vclear(&r); }
}

TEST(dtd_get_coercion_table_numeric)
{
    VARIANT s;
    REQUIRE(OA(DispGetParam));
    v_i4(&s, 70000);   conv_row(&s, VT_I2, DISP_E_OVERFLOW);
    v_i4(&s, -32768);  conv_row(&s, VT_I2, S_OK);
    v_i4(&s, 32768);   conv_row(&s, VT_I2, DISP_E_OVERFLOW);
    v_i4(&s, 255);     conv_row(&s, VT_UI1, S_OK);
    v_i4(&s, 256);     conv_row(&s, VT_UI1, DISP_E_OVERFLOW);
    v_i4(&s, -1);      conv_row(&s, VT_UI1, DISP_E_OVERFLOW);
    v_r8(&s, 1e300);   conv_row(&s, VT_I4, DISP_E_OVERFLOW);
    v_r8(&s, 3.5);     conv_row(&s, VT_I4, S_OK);
    v_r8(&s, 3.5);     conv_row(&s, VT_BSTR, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_R4, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_CY, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_DATE, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_I8, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_UI4, S_OK);
    v_i4(&s, -1);      conv_row(&s, VT_UI4, DISP_E_OVERFLOW);
    v_i4(&s, 1);       conv_row(&s, VT_INT, S_OK);
    v_i4(&s, 1);       conv_row(&s, VT_DECIMAL, S_OK);
}

TEST(dtd_get_rounding_is_bankers)
{
    static const struct { double in; LONG out; } t[] = { { 0.5, 0 }, { 1.5, 2 }, { 2.5, 2 }, { 3.5, 4 }, { -0.5, 0 }, { -1.5, -2 }, { -2.5, -2 }, { 2.4, 2 }, { 2.6, 3 } };
    unsigned i;
    REQUIRE(OA(DispGetParam));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0);
        v_r8(&a, t[i].in); vinit(&r);
        CHECKF(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL) == S_OK && V_I4(&r) == t[i].out, "in=%g got %ld want %ld", t[i].in, (long)V_I4(&r), (long)t[i].out);
    }
}

TEST(dtd_get_string_to_number_coercions)
{
    static const struct { const WCHAR *s; HRESULT hr; LONG v; } t[] = {
        { L"17", S_OK, 17 }, { L"-5", S_OK, -5 }, { L"  12  ", S_OK, 12 }, { L"0", S_OK, 0 }, { L"1e2", S_OK, 100 },
        { L"abc", DISP_E_TYPEMISMATCH, 0 }, { L"", DISP_E_TYPEMISMATCH, 0 }, { L"12abc", DISP_E_TYPEMISMATCH, 0 },
        { L"99999999999", DISP_E_OVERFLOW, 0 },
    };
    unsigned i;
    REQUIRE(OA(DispGetParam));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0); UINT err = 0xdeadbeef; HRESULT hr;
        vinit(&a); V_VT(&a) = VT_BSTR; V_BSTR(&a) = bs_alloc(t[i].s); vinit(&r);
        hr = OA(DispGetParam)(&dp, 0, VT_I4, &r, &err);
        CHECKF(hr == t[i].hr, "i=%u hr=%08lx want %08lx", i, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK) CHECKF(V_I4(&r) == t[i].v, "i=%u v=%ld", i, (long)V_I4(&r));
        else CHECKF(err == 0, "i=%u err=%u", i, err);
        bs_free(V_BSTR(&a));
    }
}

TEST(dtd_get_byref_source_is_dereferenced)
{
    VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0); LONG val = 777; BSTR bs = bs_alloc(L"byref-bstr"); VARIANT inner;
    REQUIRE(OA(DispGetParam));
    vinit(&a); V_VT(&a) = VT_BYREF | VT_I4; V_I4REF(&a) = &val;
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I4); EXPECT_EQ(V_I4(&r), 777);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BSTR, &r, NULL), S_OK);
    CHECK(bs_eq(V_BSTR(&r), L"777")); vclear(&r);
    vinit(&a); V_VT(&a) = VT_BYREF | VT_BSTR; V_BSTRREF(&a) = &bs;
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_BSTR, &r, NULL), S_OK);
    CHECK(bs_eq(V_BSTR(&r), L"byref-bstr")); CHECK(V_BSTR(&r) != bs); vclear(&r);
    v_i4(&inner, 31);
    vinit(&a); V_VT(&a) = VT_BYREF | VT_VARIANT; V_VARIANTREF(&a) = &inner;
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I2, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_I2); EXPECT_EQ(V_I2(&r), 31);
    bs_free(bs);
}

TEST(dtd_get_dispatch_arg_addref_balanced)
{
    Mock m; VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0);
    REQUIRE(OA(DispGetParam));
    Mock_init(&m);
    vinit(&a); V_VT(&a) = VT_DISPATCH; V_DISPATCH(&a) = (IDispatch *)&m;
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_DISPATCH, &r, NULL), S_OK);
    EXPECT_EQ(V_VT(&r), VT_DISPATCH); CHECK(V_DISPATCH(&r) == (IDispatch *)&m);
    EXPECT_EQ(m.ref, 2);
    vclear(&r);
    EXPECT_EQ(m.ref, 1);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_UNKNOWN, &r, NULL), S_OK);   /* IDispatch -> IUnknown via QI */
    EXPECT_EQ(V_VT(&r), VT_UNKNOWN);
    vclear(&r);
    EXPECT_EQ(m.ref, 1);
}

/* ---- named arguments ---- */
TEST(dtd_get_named_arg_lookup_by_dispid)
{
    /* f(a, b, c) called as f(1, 2, c:=9): rgvarg = { 9 (named c), 2 (b), 1 (a) }, rgdispidNamedArgs = { 2 } */
    VARIANT a[3], r; DISPID named[1] = { 2 }; DISPPARAMS dp = dp3(a, named, 3, 1); int pos;
    static const LONG want[3] = { 1, 2, 9 };
    REQUIRE(OA(DispGetParam));
    v_i4(&a[0], 9); v_i4(&a[1], 2); v_i4(&a[2], 1);
    for (pos = 0; pos < 3; pos++) {
        vinit(&r);
        CHECKF(OA(DispGetParam)(&dp, pos, VT_I4, &r, NULL) == S_OK && V_I4(&r) == want[pos], "pos %d got %ld", pos, (long)V_I4(&r));
    }
}

TEST(dtd_get_named_arg_two_named_reordered)
{
    /* f(a, b, c) called as f(10, c:=30, b:=20)?  rgvarg = { 30 (named c), 20 (named b), 10 (positional a) }, named = { 2, 1 } */
    VARIANT a[3], r; DISPID named[2] = { 2, 1 }; DISPPARAMS dp = dp3(a, named, 3, 2);
    REQUIRE(OA(DispGetParam));
    v_i4(&a[0], 30); v_i4(&a[1], 20); v_i4(&a[2], 10);
    vinit(&r); EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 10);
    vinit(&r); EXPECT_EQ(OA(DispGetParam)(&dp, 1, VT_I4, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 20);
    vinit(&r); EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I4, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 30);
}

TEST(dtd_get_positional_slot_overlapping_named_region_is_paramnotfound)
{
    /* cArgs=3, two named: positions whose end-based index falls inside the named block are not positional. */
    VARIANT a[3], r; DISPID named[2] = { 1, 0 }; DISPPARAMS dp = dp3(a, named, 3, 2); UINT err = 0xdeadbeef;
    REQUIRE(OA(DispGetParam));
    v_i4(&a[0], 20); v_i4(&a[1], 10); v_i4(&a[2], 30);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 2, VT_I4, &r, &err), DISP_E_PARAMNOTFOUND);   /* not named; idx 0 < cNamedArgs */
    EXPECT_EQ(V_VT(&r), VT_EMPTY); EXPECT_EQ(err, 0xdeadbeef);
}

TEST(dtd_get_property_put_named_dispid)
{
    /* property put: cArgs=1, cNamedArgs=1, rgdispidNamedArgs={DISPID_PROPERTYPUT(-3)} */
    VARIANT a, r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp3(&a, named, 1, 1);
    REQUIRE(OA(DispGetParam));
    v_i4(&a, 55);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, (UINT)DISPID_PROPERTYPUT, VT_I4, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 55);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL), DISP_E_PARAMNOTFOUND);   /* slot 0 belongs to the named arg */
    EXPECT_EQ(V_VT(&r), VT_EMPTY);
}

TEST(dtd_get_property_put_with_index_args)
{
    /* Item(i) = v :  rgvarg = { v (named PROPERTYPUT), i } */
    VARIANT a[2], r; DISPID named[1] = { DISPID_PROPERTYPUT }; DISPPARAMS dp = dp3(a, named, 2, 1);
    REQUIRE(OA(DispGetParam));
    v_i4(&a[0], 900); v_i4(&a[1], 3);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 3);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, (UINT)DISPID_PROPERTYPUT, VT_I4, &r, NULL), S_OK); EXPECT_EQ(V_I4(&r), 900);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 1, VT_I4, &r, NULL), DISP_E_PARAMNOTFOUND);
}

TEST(dtd_get_named_dispid_match_has_priority_over_positional)
{
    /* position 0 is both a named dispid (-> rgvarg[0]) and, positionally, rgvarg[cArgs-1]: named wins */
    VARIANT a[2], r; DISPID named[1] = { 0 }; DISPPARAMS dp = dp3(a, named, 2, 1);
    REQUIRE(OA(DispGetParam));
    v_i4(&a[0], 111); v_i4(&a[1], 222);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 0, VT_I4, &r, NULL), S_OK);
    EXPECT_EQ(V_I4(&r), 111);
    vinit(&r);
    EXPECT_EQ(OA(DispGetParam)(&dp, 1, VT_I4, &r, NULL), DISP_E_PARAMNOTFOUND);   /* idx = 2-1-1 = 0 < cNamed and 1 is not named */
}

TEST(dtd_get_variant_error_notfound_arg)
{
    /* an omitted optional argument is passed as VT_ERROR / DISP_E_PARAMNOTFOUND; it does not coerce to a number */
    VARIANT a, r; DISPPARAMS dp = dp3(&a, NULL, 1, 0); UINT err = 0xdeadbeef; HRESULT hr;
    REQUIRE(OA(DispGetParam));
    vinit(&a); V_VT(&a) = VT_ERROR; V_ERROR(&a) = DISP_E_PARAMNOTFOUND;
    vinit(&r);
    hr = OA(DispGetParam)(&dp, 0, VT_ERROR, &r, &err);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(V_VT(&r), VT_ERROR); EXPECT_EQ((ULONG)V_ERROR(&r), 0x80020004);
    vinit(&r);
    hr = OA(DispGetParam)(&dp, 0, VT_I4, &r, &err);
    CHECKF(hr == DISP_E_TYPEMISMATCH, "hr=%08lx", (unsigned long)hr);
    EXPECT_EQ(err, 0);
}

TEST(dtd_get_does_not_modify_source_args)
{
    VARIANT a[3], c[3], r; DISPPARAMS dp; int i;
    REQUIRE(OA(DispGetParam));
    three_args(a); dp = dp3(a, NULL, 3, 0);
    memcpy(c, a, sizeof a);
    vinit(&r); OA(DispGetParam)(&dp, 0, VT_BSTR, &r, NULL); vclear(&r);
    vinit(&r); OA(DispGetParam)(&dp, 1, VT_R8, &r, NULL);
    vinit(&r); OA(DispGetParam)(&dp, 2, VT_BSTR, &r, NULL); vclear(&r);
    for (i = 0; i < 3; i++) CHECKF(!memcmp(&c[i], &a[i], sizeof(VARIANT)), "arg %d modified", i);
    three_clear(a);
}

TEST(dtd_get_big_arglist_all_positions)
{
    VARIANT a[64], r; DISPPARAMS dp = dp3(a, NULL, 64, 0); int i, ok = 0;
    REQUIRE(OA(DispGetParam));
    for (i = 0; i < 64; i++) v_i4(&a[i], i);
    for (i = 0; i < 64; i++) { vinit(&r); if (OA(DispGetParam)(&dp, i, VT_I4, &r, NULL) == S_OK && V_I4(&r) == 63 - i) ok++; }
    EXPECT_EQ(ok, 64);
}

/* ===================== LHashValOfNameSys ===================== */
static ULONG model_hash(const unsigned char *tab, ULONG hi, const char *s)
{
    ULONG v = 0x0deadbee;
    while (*s) { v = v * 37 + tab[(unsigned char)*s]; s++; }
    return hi | ((v - (v / 0x1003f) * 0x3f) & 0xffff);
}

/* adapted from Wine dlls/oleaut32/hash.c behaviour (LGPL-2.1+), constants regenerated from the Win10 table dump */
TEST(dtd_hash_known_values_win32_neutral)
{
    static const struct { const char *s; ULONG h; } t[] = {
        { "", 0x107015 }, { "a", 0x101058 }, { "abc", 0x1072ac }, { "ABC", 0x1072ac }, { "AB", 0x105c7c },
        { "Hello", 0x10669a }, { "IUnknown", 0x10885b }, { "QueryInterface", 0x108ebe }, { "stdole", 0x106093 },
        { "A/B", 0x106921 },
    };
    unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ULONG h = OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, t[i].s);
        CHECKF(h == t[i].h, "\"%s\" got %06lx want %06lx", t[i].s, (unsigned long)h, (unsigned long)t[i].h);
    }
}

TEST(dtd_hash_known_values_mac)
{
    static const struct { const char *s; ULONG h; } t[] = {
        { "", 0x117015 }, { "abc", 0x1172ac }, { "Hello", 0x11669a }, { "IUnknown", 0x11885b }, { "stdole", 0x116093 },
    };
    unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ULONG h = OA(LHashValOfNameSysA)(SYS_MAC, 0x409, t[i].s);
        CHECKF(h == t[i].h, "\"%s\" got %06lx want %06lx", t[i].s, (unsigned long)h, (unsigned long)t[i].h);
    }
}

TEST(dtd_hash_null_string_is_zero)
{
    REQUIRE(OA(LHashValOfNameSysA)); REQUIRE(OA(LHashValOfNameSys));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, NULL), 0);
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_MAC, 0x409, NULL), 0);
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN16, 0, NULL), 0);
    EXPECT_EQ(OA(LHashValOfNameSys)(SYS_WIN32, 0x409, NULL), 0);
    EXPECT_EQ(OA(LHashValOfNameSys)(SYS_MAC, 0x407, NULL), 0);
}

TEST(dtd_hash_empty_string_is_not_zero)
{
    REQUIRE(OA(LHashValOfNameSysA)); REQUIRE(OA(LHashValOfNameSys));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, ""), 0x107015);
    EXPECT_EQ(OA(LHashValOfNameSys)(SYS_WIN32, 0x409, L""), 0x107015);
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "") & 0xffff, (0x0deadbee % 65599) & 0xffff);
}

TEST(dtd_hash_wide_matches_ansi_for_ascii)
{
    static const char *a[] = { "", "x", "Hello", "IUnknown", "SomeLongerIdentifier_42", "with space", "tab\there", "A/B/C", "~!@#$%^&*()" };
    static const WCHAR *w[] = { L"", L"x", L"Hello", L"IUnknown", L"SomeLongerIdentifier_42", L"with space", L"tab\there", L"A/B/C", L"~!@#$%^&*()" };
    unsigned i; int k;
    static const SYSKIND sk[] = { SYS_WIN16, SYS_WIN32, SYS_MAC };
    REQUIRE(OA(LHashValOfNameSysA)); REQUIRE(OA(LHashValOfNameSys));
    for (i = 0; i < sizeof a / sizeof a[0]; i++)
        for (k = 0; k < 3; k++) {
            ULONG ha = OA(LHashValOfNameSysA)(sk[k], 0x409, a[i]), hw = OA(LHashValOfNameSys)(sk[k], 0x409, w[i]);
            CHECKF(ha == hw, "i=%u k=%d A=%06lx W=%06lx", i, k, (unsigned long)ha, (unsigned long)hw);
        }
}

TEST(dtd_hash_case_insensitive_ascii)
{
    unsigned i; int bad = 0;
    static const char *names[] = { "IUnknown", "iunknown", "IUNKNOWN", "QueryInterface", "queryinterface", "QUERYINTERFACE", "zZzZ", "ZzZz" };
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[0]), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[1]));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[0]), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[2]));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[3]), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[4]));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[3]), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[5]));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[6]), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, names[7]));
    (void)i; (void)bad;
}

TEST(dtd_hash_every_ascii_letter_case_fold)
{
    char lo[2] = { 0, 0 }, up[2] = { 0, 0 }; int c;
    REQUIRE(OA(LHashValOfNameSysA));
    for (c = 'a'; c <= 'z'; c++) {
        lo[0] = (char)c; up[0] = (char)(c - 32);
        CHECKF(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, lo) == OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, up), "letter %c", c);
    }
}

TEST(dtd_hash_table_quirks_v_w_and_u_y_collide)
{
    /* the native collation table maps W->V and Y->U (and '/' -> 0) */
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "v"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "w"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "V"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "W"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "u"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Y"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "AwB"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "avb"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "/"), model_hash(kTabWin, 0x100000, "/"));
    CHECK(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "A/B") != OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "AB"));
    CHECK(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "x") != OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "y"));
    CHECK(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "v") != OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "x"));
}

TEST(dtd_hash_ansi_accent_folding_win32)
{
    /* Windows table (second half): accented Latin-1 letters hash like their base letter */
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "caf\xe9"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "cafe"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "\xc0\xc1\xc2\xc3\xc4\xc5"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "AAAAAA"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "\xe7"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "c"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "\xf1"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "N"));
}

TEST(dtd_hash_model_win32_matches_for_all_single_bytes)
{
    int c, bad = 0;
    REQUIRE(OA(LHashValOfNameSysA));
    for (c = 1; c < 256; c++) {
        char s[2] = { (char)c, 0 };
        ULONG got = OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, s), want = model_hash(kTabWin, 0x100000, s);
        if (got != want) { bad++; CHECKF(0, "byte 0x%02x got %06lx want %06lx", c, (unsigned long)got, (unsigned long)want); }
    }
    EXPECT_EQ(bad, 0);
}

TEST(dtd_hash_model_mac_matches_for_all_single_bytes)
{
    int c, bad = 0;
    REQUIRE(OA(LHashValOfNameSysA));
    for (c = 1; c < 256; c++) {
        char s[2] = { (char)c, 0 };
        ULONG got = OA(LHashValOfNameSysA)(SYS_MAC, 0x409, s), want = model_hash(kTabMac, 0x110000, s);
        if (got != want) { bad++; CHECKF(0, "byte 0x%02x got %06lx want %06lx", c, (unsigned long)got, (unsigned long)want); }
    }
    EXPECT_EQ(bad, 0);
}

TEST(dtd_hash_model_random_strings)
{
    unsigned seed = 12345, i; int bad = 0;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < 3000; i++) {
        char s[40]; unsigned n, k;
        seed = seed * 1103515245u + 12345u; n = 1 + ((seed >> 16) % 30);
        for (k = 0; k < n; k++) { seed = seed * 1103515245u + 12345u; s[k] = (char)(1 + ((seed >> 16) % 255)); }
        s[n] = 0;
        if (OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, s) != model_hash(kTabWin, 0x100000, s)) bad++;
        if (OA(LHashValOfNameSysA)(SYS_MAC, 0x409, s) != model_hash(kTabMac, 0x110000, s)) bad++;
    }
    EXPECT_EQ(bad, 0);
}

TEST(dtd_hash_long_string_wraps_prime_modulus)
{
    char big[4097]; unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < 4096; i++) big[i] = (char)('A' + (i % 26));
    big[4096] = 0;
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, big), model_hash(kTabWin, 0x100000, big));
}

TEST(dtd_hash_syskind_win16_equals_win32)
{
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN16, 0x409, "Hello"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN16, 0x409, "caf\xe9"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "caf\xe9"));
}

TEST(dtd_hash_syskind_other_values_behave_as_win32)
{
    /* only SYS_MAC (2) selects the Mac table; every other value uses the Windows one */
    static const int kinds[] = { 3, 4, 100, -1 };
    unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < sizeof kinds / sizeof kinds[0]; i++)
        CHECKF(OA(LHashValOfNameSysA)((SYSKIND)kinds[i], 0x409, "Hello") == OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello"), "kind %d", kinds[i]);
}

TEST(dtd_hash_mac_flag_in_high_word_only_for_sysmac)
{
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "xyz") >> 16, 0x11);
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "xyz") >> 16, 0x10);
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "xyz") & 0xffff, OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "xyz") & 0xffff);
}

TEST(dtd_hash_mac_high_chars_use_mac_table)
{
    /* on SYS_MAC the high-byte half of the table is the Mac set: 0x8e (e-acute in MacRoman) folds to 'E'. */
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "\x8e"), OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "E"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "\x80"), OA(LHashValOfNameSysA)(SYS_MAC, 0x409, "A"));
    CHECK(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "\x80") != OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "A"));   /* Windows table maps 0x80 -> 0x7f */
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "\xe9"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "E"));
}

TEST(dtd_hash_high_word_by_lcid)
{
    /* class numbers (x 0x10000) from the lcid switch in LHashValOfNameSysA */
    static const struct { LCID lcid; ULONG hi; } t[] = {
        { 0x0000, 0x10 }, { 0x0409, 0x10 }, { 0x0809, 0x10 }, { 0x0407, 0x10 }, { 0x040c, 0x10 }, { 0x0410, 0x10 },
        { 0x0416, 0x10 }, { 0x0c0a, 0x10 }, { 0x0813, 0x10 }, { 0x0420, 0x10 },
        { 0x0405, 0x20 }, { 0x0415, 0x20 }, { 0x040e, 0x20 }, { 0x041b, 0x20 },
        { 0x0419, 0x30 }, { 0x0408, 0x80 }, { 0x040f, 0x90 }, { 0x041f, 0xa0 }, { 0x0814, 0xb0 }, { 0x1809, 0xc0 },
        { 0x040d, 0xe0 }, { 0x0401, 0xd0 }, { 0x0801, 0xd0 }, { 0x0c01, 0xd0 }, { 0x0429, 0xd0 },
    };
    unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < sizeof t / sizeof t[0]; i++) {
        ULONG h = OA(LHashValOfNameSysA)(SYS_WIN32, t[i].lcid, "Test");
        CHECKF((h >> 16) == t[i].hi, "lcid %04lx got %02lx want %02lx", (unsigned long)t[i].lcid, (unsigned long)(h >> 16), (unsigned long)t[i].hi);
        h = OA(LHashValOfNameSysA)(SYS_MAC, t[i].lcid, "Test");
        CHECKF((h >> 16) == (t[i].hi | 1), "mac lcid %04lx got %02lx", (unsigned long)t[i].lcid, (unsigned long)(h >> 16));
    }
}

TEST(dtd_hash_dbcs_locales_noted)
{
    static const LCID l[] = { 0x0411, 0x0412, 0x0804, 0x0404, 0x0c04, 0x1004, 0x1404 };
    unsigned i;
    REQUIRE(OA(LHashValOfNameSysA));
    for (i = 0; i < sizeof l / sizeof l[0]; i++)
        t_note("lcid %04lx hash(\"Test\")=%06lx (DBCS class, via LHashValOfDBCSName)", (unsigned long)l[i], (unsigned long)OA(LHashValOfNameSysA)(SYS_WIN32, l[i], "Test"));
    CHECK(1);
}

TEST(dtd_hash_low_word_independent_of_non_dbcs_lcid_table)
{
    /* LCID classes that share table 0x10 give identical hashes; classes with other tables only change the high word for ASCII
       in the common half (tables share the first 128 entries? no: only verified for class 0x10 here) */
    REQUIRE(OA(LHashValOfNameSysA));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x407, "Hello"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello"), OA(LHashValOfNameSysA)(SYS_WIN32, 0, "Hello"));
    EXPECT_EQ(OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "Hello"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x40c, "Hello"));
}

TEST(dtd_hash_macro_lhashvalofname_is_win32)
{
    REQUIRE(OA(LHashValOfNameSys));
    EXPECT_EQ(OA(LHashValOfNameSys)(SYS_WIN32, 0x409, L"MacroCheck"), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "MacroCheck"));
}

TEST(dtd_hash_wide_long_name_within_buffer)
{
    WCHAR w[256]; char a[256]; int i;
    REQUIRE(OA(LHashValOfNameSys));
    for (i = 0; i < 250; i++) { w[i] = (WCHAR)('a' + i % 26); a[i] = (char)('a' + i % 26); }
    w[250] = 0; a[250] = 0;
    EXPECT_EQ(OA(LHashValOfNameSys)(SYS_WIN32, 0x409, w), OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, a));
}

TEST(dtd_hash_stability_and_range)
{
    ULONG h1, h2;
    REQUIRE(OA(LHashValOfNameSysA));
    h1 = OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "stable");
    h2 = OA(LHashValOfNameSysA)(SYS_WIN32, 0x409, "stable");
    EXPECT_EQ(h1, h2);
    CHECK((h1 & 0xffe00000) == 0);
    CHECK((h1 & 0xffff) < 65599);
}

/* ===================== OaBuildVersion ===================== */
TEST(dtd_oabv_high_word_and_stability)
{
    ULONG v1, v2;
    REQUIRE(OA(OaBuildVersion));
    v1 = OA(OaBuildVersion)(); v2 = OA(OaBuildVersion)();
    EXPECT_EQ(v1, v2);
    EXPECT_EQ(v1 >> 16, 0x32);
    t_note("OaBuildVersion = %08lx (19041.2965 build decodes to 0x00321396)", (unsigned long)v1);
}

TEST(dtd_oabv_low_word_is_nonzero)
{
    REQUIRE(OA(OaBuildVersion));
    CHECK((OA(OaBuildVersion)() & 0xffff) != 0);
}
