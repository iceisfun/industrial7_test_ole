/* dtd_exc_*: EXCEPINFO plumbing around DISP_E_EXCEPTION.
 * Decomp-verified: CDispTypeInfo::Invoke never writes *pexcepinfo itself (only the caller-supplied pointer is handed to an
 * IDispatch it chains to); failures before the call (member lookup, flag check, argument coercion) leave EXCEPINFO and the
 * result VARIANT untouched.  Strings/pfnDeferredFillIn produced by the callee reach the caller unchanged and are the caller's
 * to free (SysFreeString) / to complete via pfnDeferredFillIn. */
#include "dtd.h"

#define SETUP(TI, D) ITypeInfo *TI; Demo D; REQUIRE(OA(CreateDispTypeInfo)); TI = demo_typeinfo(0x409); if (!TI) return; Demo_init(&D)

static void free_ex(EXCEPINFO *ex) { bs_free(ex->bstrSource); bs_free(ex->bstrDescription); bs_free(ex->bstrHelpFile); }

TEST(dtd_exc_chained_object_exception_reaches_caller)
{
    SETUP(ti, d); Mock m; VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); EXCEPINFO ex; UINT err = 0xdead; HRESULT hr;
    Mock_init(&m); m.exc_mode = 1; d.child = (IDispatch *)&m;
    v_i4(&a[0], 1);
    memset(&ex, 0xab, sizeof ex);
    hr = ITypeInfo_Invoke(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, &ex, &err);
    EXPECT_EQ(hr, DISP_E_EXCEPTION);
    if (hr != DISP_E_EXCEPTION) { ITypeInfo_Release(ti); return; }
    CHECK(m.last_ex == &ex);       /* the very same EXCEPINFO pointer is forwarded */
    EXPECT_EQ((ULONG)ex.scode, 0x80040201UL); EXPECT_EQ(ex.wCode, 0);
    CHECK(bs_eq(ex.bstrSource, L"Mock.Source")); CHECK(bs_eq(ex.bstrDescription, L"mock failure"));
    CHECK(ex.bstrHelpFile == NULL); EXPECT_EQ(ex.dwHelpContext, 77);
    CHECK(ex.pfnDeferredFillIn == Mock_DeferredFillIn);
    EXPECT_EQ(err, 0xdead);
    EXPECT_EQ(m.ref, 1);
    if (hr == DISP_E_EXCEPTION) free_ex(&ex);
    ITypeInfo_Release(ti);
}

TEST(dtd_exc_deferred_fill_in_is_callers_job)
{
    SETUP(ti, d); Mock m; VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); EXCEPINFO ex; HRESULT hr;
    Mock_init(&m); m.exc_mode = 1; d.child = (IDispatch *)&m;
    v_i4(&a[0], 1);
    hr = ITypeInfo_Invoke(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, &ex, NULL);
    EXPECT_EQ(hr, DISP_E_EXCEPTION);
    if (hr == DISP_E_EXCEPTION && ex.pfnDeferredFillIn) {
        CHECK(ex.bstrHelpFile == NULL);
        EXPECT_EQ(ex.pfnDeferredFillIn(&ex), S_OK);
        CHECK(bs_eq(ex.bstrHelpFile, L"deferred.hlp")); EXPECT_EQ(ex.dwHelpContext, 1234);
        CHECK(ex.pfnDeferredFillIn == NULL);
    }
    free_ex(&ex);
    ITypeInfo_Release(ti);
}

TEST(dtd_exc_chain_with_null_excepinfo_still_reports_exception)
{
    SETUP(ti, d); Mock m; VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0);
    Mock_init(&m); m.exc_mode = 1; d.child = (IDispatch *)&m;
    v_i4(&a[0], 1);
    EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, NULL, NULL), DISP_E_EXCEPTION);
    CHECK(m.last_ex == NULL);
    EXPECT_EQ(m.ref, 1);
    ITypeInfo_Release(ti);
}

TEST(dtd_exc_stddisp_forwards_exception_info)
{
    SETUP(ti, d); Mock m; IDispatch *disp = demo_stddisp(&d, ti, NULL); VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); EXCEPINFO ex; HRESULT hr;
    if (!disp) return;
    Mock_init(&m); m.exc_mode = 1; d.child = (IDispatch *)&m;
    v_i4(&a[0], 1);
    memset(&ex, 0, sizeof ex);
    hr = IDispatch_Invoke(disp, DD_CHILD, &DI_NULL, 0x409, DISPATCH_PROPERTYGET, &dp, &r, &ex, NULL);
    EXPECT_EQ(hr, DISP_E_EXCEPTION);
    if (hr != DISP_E_EXCEPTION) { IDispatch_Release(disp); ITypeInfo_Release(ti); return; }
    CHECK(bs_eq(ex.bstrDescription, L"mock failure")); EXPECT_EQ((ULONG)ex.scode, 0x80040201UL);
    if (hr == DISP_E_EXCEPTION) free_ex(&ex);
    IDispatch_Release(disp); ITypeInfo_Release(ti);
}

TEST(dtd_exc_excepinfo_strings_are_valid_bstrs)
{
    SETUP(ti, d); Mock m; VARIANT a[1], r; DISPPARAMS dp = dp_make(a, 1, NULL, 0); EXCEPINFO ex; HRESULT hr;
    REQUIRE(OA(SysStringByteLen));
    Mock_init(&m); m.exc_mode = 1; d.child = (IDispatch *)&m;
    v_i4(&a[0], 1);
    memset(&ex, 0, sizeof ex);
    hr = ITypeInfo_Invoke(ti, &d, DD_CHILD, DISPATCH_PROPERTYGET, &dp, &r, &ex, NULL);
    EXPECT_EQ(hr, DISP_E_EXCEPTION);
    if (hr == DISP_E_EXCEPTION) {
        EXPECT_EQ(bs_len(ex.bstrSource), 11); EXPECT_EQ(bs_len(ex.bstrDescription), 12);
        EXPECT_EQ(OA(SysStringByteLen)(ex.bstrSource), 22);
        free_ex(&ex);
    }
    ITypeInfo_Release(ti);
}

static void sentinel(EXCEPINFO *ex, VARIANT *r) { memset(ex, 0xab, sizeof *ex); memset(r, 0xcc, sizeof *r); }
static int untouched(EXCEPINFO *ex, VARIANT *r)
{
    EXCEPINFO e2; VARIANT r2; sentinel(&e2, &r2);
    return !memcmp(ex, &e2, sizeof e2) && !memcmp(r, &r2, sizeof r2);
}

TEST(dtd_exc_failures_before_call_leave_excepinfo_and_result_untouched)
{
    SETUP(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0), dp0 = dp_make(NULL, 0, NULL, 0); EXCEPINFO ex; UINT err = 0;
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    sentinel(&ex, &r); EXPECT_EQ(ITypeInfo_Invoke(ti, &d, 0x7000, DISPATCH_METHOD, &dp, &r, &ex, &err), DISP_E_MEMBERNOTFOUND); CHECK(untouched(&ex, &r));
    sentinel(&ex, &r); EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp0, &r, &ex, &err), DISP_E_BADPARAMCOUNT); CHECK(untouched(&ex, &r));
    sentinel(&ex, &r); EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, 0x40, &dp, &r, &ex, &err), E_INVALIDARG); CHECK(untouched(&ex, &r));
    vinit(&a[0]); V_VT(&a[0]) = VT_BSTR; V_BSTR(&a[0]) = bs_alloc(L"nope");
    sentinel(&ex, &r); EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &ex, &err), DISP_E_TYPEMISMATCH); CHECK(untouched(&ex, &r));
    vclear(&a[0]); v_r8(&a[0], 1e30);
    sentinel(&ex, &r); EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &ex, &err), DISP_E_OVERFLOW); CHECK(untouched(&ex, &r));
    EXPECT_EQ(d.calls, 0);
    ITypeInfo_Release(ti);
}

TEST(dtd_exc_successful_call_leaves_excepinfo_untouched)
{
    SETUP(ti, d); VARIANT a[2], r; DISPPARAMS dp = dp_make(a, 2, NULL, 0); EXCEPINFO ex; EXCEPINFO e2; UINT err = 0xdead;
    v_i4(&a[0], 1); v_i4(&a[1], 2);
    memset(&ex, 0xab, sizeof ex); memset(&e2, 0xab, sizeof e2);
    EXPECT_EQ(ITypeInfo_Invoke(ti, &d, DD_ADD, DISPATCH_METHOD, &dp, &r, &ex, &err), S_OK);
    CHECK(!memcmp(&ex, &e2, sizeof ex)); EXPECT_EQ(err, 0xdead);
    ITypeInfo_Release(ti);
}

TEST(dtd_exc_excepinfo_struct_layout_x64)
{
    /* the ABI the callee fills: sizes/offsets a native implementation must honour on x64 */
    EXPECT_EQ(sizeof(EXCEPINFO), 64);
    EXPECT_EQ(offsetof(EXCEPINFO, wCode), 0);
    EXPECT_EQ(offsetof(EXCEPINFO, bstrSource), 8);
    EXPECT_EQ(offsetof(EXCEPINFO, bstrDescription), 16);
    EXPECT_EQ(offsetof(EXCEPINFO, bstrHelpFile), 24);
    EXPECT_EQ(offsetof(EXCEPINFO, dwHelpContext), 32);
    EXPECT_EQ(offsetof(EXCEPINFO, pfnDeferredFillIn), 48);
    EXPECT_EQ(offsetof(EXCEPINFO, scode), 56);
    EXPECT_EQ(sizeof(DISPPARAMS), 24); EXPECT_EQ(sizeof(VARIANT), 24); EXPECT_EQ(sizeof(DECIMAL), 16);
    EXPECT_EQ(sizeof(METHODDATA), 40); EXPECT_EQ(sizeof(PARAMDATA), 16); EXPECT_EQ(sizeof(INTERFACEDATA), 16);
}
