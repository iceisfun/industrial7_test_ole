/* dtd_err_*: CreateErrorInfo / ICreateErrorInfo / IErrorInfo / SetErrorInfo / GetErrorInfo.
 * oleaut32 exports are thin jumps into combase (CoCreateErrorInfo / CoSetErrorInfo / CoGetErrorInfo). Decomp-verified:
 *  - CreateErrorInfo(NULL) -> E_INVALIDARG; the object (CErrorObject) answers IUnknown, ICreateErrorInfo, IErrorInfo, IMarshal.
 *  - IErrorInfo getters: NULL out -> E_INVALIDARG; unset string -> *out = NULL, S_OK; set -> fresh BSTR copy each call.
 *  - SetErrorInfo(reserved!=0) -> E_INVALIDARG (nothing changed); otherwise the thread slot is replaced: new object AddRef'd,
 *    old one Released; SetErrorInfo(0, NULL) clears.
 *  - GetErrorInfo: pperrinfo NULL -> E_INVALIDARG; *pperrinfo = NULL on entry; reserved!=0 -> E_INVALIDARG; empty slot ->
 *    S_FALSE; else QI(IID_IErrorInfo) on the stored object, on success the slot is cleared (ownership moves to the caller),
 *    on QI failure the object stays stored and the QI HRESULT is returned. */
#include "dtd.h"

#define CE(x)  OA(x)

static const GUID kGuid = { 0x9474ba1a, 0x258b, 0x490b, { 0xbc, 0x13, 0x51, 0x6e, 0x92, 0x39, 0xac, 0xe0 } };

static void drain(void)
{
    IErrorInfo *e = NULL;
    HRESULT hr = CE(GetErrorInfo)(0, &e);
    if (hr == S_OK && e) IErrorInfo_Release(e);
}

static ICreateErrorInfo *mk(void)
{
    ICreateErrorInfo *c = NULL;
    HRESULT hr = CE(CreateErrorInfo)(&c);
    CHECKF(hr == S_OK && c, "CreateErrorInfo hr=%08lx", (unsigned long)hr);
    return hr == S_OK ? c : NULL;
}

/* ---------------- object model ---------------- */
TEST(dtd_err_create_null_out)
{
    REQUIRE(CE(CreateErrorInfo));
    EXPECT_EQ(CE(CreateErrorInfo)(NULL), E_INVALIDARG);
}

TEST(dtd_err_create_returns_object_refcount_one)
{
    ICreateErrorInfo *c; ULONG r;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    r = ICreateErrorInfo_AddRef(c);   EXPECT_EQ(r, 2);
    r = ICreateErrorInfo_Release(c);  EXPECT_EQ(r, 1);
    r = ICreateErrorInfo_Release(c);  EXPECT_EQ(r, 0);
}

TEST(dtd_err_out_param_reset_on_create)
{
    ICreateErrorInfo *c = (ICreateErrorInfo *)(ULONG_PTR)0x1234;
    HRESULT hr;
    REQUIRE(CE(CreateErrorInfo));
    hr = CE(CreateErrorInfo)(&c);
    EXPECT_EQ(hr, S_OK);
    CHECK(c != NULL && c != (ICreateErrorInfo *)(ULONG_PTR)0x1234);
    if (c) ICreateErrorInfo_Release(c);
}

TEST(dtd_err_qi_supported_interfaces)
{
    ICreateErrorInfo *c; IUnknown *u = NULL; IErrorInfo *e = NULL; ICreateErrorInfo *c2 = NULL; void *m = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    EXPECT_EQ(ICreateErrorInfo_QueryInterface(c, &DI_IUnknown, (void **)&u), S_OK);
    EXPECT_EQ(ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e), S_OK);
    EXPECT_EQ(ICreateErrorInfo_QueryInterface(c, &DI_ICreateErrorInfo, (void **)&c2), S_OK);
    EXPECT_EQ(ICreateErrorInfo_QueryInterface(c, &DI_IMarshal, &m), S_OK);
    CHECK(u && e && c2 && m);
    CHECK((void *)e != (void *)c);   /* distinct interface identity */
    if (u) IUnknown_Release(u);
    if (e) IErrorInfo_Release(e);
    if (c2) ICreateErrorInfo_Release(c2);
    if (m) ((IUnknown *)m)->lpVtbl->Release((IUnknown *)m);
    EXPECT_EQ(ICreateErrorInfo_Release(c), 0);
}

TEST(dtd_err_qi_unsupported_interface)
{
    ICreateErrorInfo *c; void *p = (void *)1; HRESULT hr;
    static const GUID *bad[] = { &DI_IDispatch, &DI_ITypeInfo, &DI_ISupportErrorInfo, &DI_IClassFactory, &DI_NULL };
    unsigned i;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        p = (void *)1;
        hr = ICreateErrorInfo_QueryInterface(c, bad[i], &p);
        CHECKF(hr == E_NOINTERFACE && p == NULL, "i=%u hr=%08lx p=%p", i, (unsigned long)hr, p);
    }
    ICreateErrorInfo_Release(c);
}

TEST(dtd_err_qi_null_out_is_einvalidarg)
{
    ICreateErrorInfo *c;
    REQUIRE(CE(CreateErrorInfo));
    if (t_is_wine()) { t_skip("emulator faults on NULL ppv"); return; }
    c = mk(); if (!c) return;
    EXPECT_EQ(ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, NULL), E_INVALIDARG);
    ICreateErrorInfo_Release(c);
}

TEST(dtd_err_iunknown_identity_is_stable)
{
    ICreateErrorInfo *c; IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL; IErrorInfo *e = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IUnknown, (void **)&u1);
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    IErrorInfo_QueryInterface(e, &DI_IUnknown, (void **)&u2);
    ICreateErrorInfo_QueryInterface(c, &DI_IUnknown, (void **)&u3);
    CHECK(u1 && u1 == u2 && u2 == u3);
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (u3) IUnknown_Release(u3);
    if (e) IErrorInfo_Release(e);
    ICreateErrorInfo_Release(c);
}

TEST(dtd_err_refcount_shared_across_interfaces)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);   /* ref 2 */
    EXPECT_EQ(IErrorInfo_AddRef(e), 3);
    EXPECT_EQ(ICreateErrorInfo_AddRef(c), 4);
    EXPECT_EQ(ICreateErrorInfo_Release(c), 3);
    EXPECT_EQ(IErrorInfo_Release(e), 2);
    EXPECT_EQ(IErrorInfo_Release(e), 1);
    EXPECT_EQ(ICreateErrorInfo_Release(c), 0);
}

TEST(dtd_err_marshal_unmarshal_class_is_error_object)
{
    static const GUID clsid_errobj = { 0x0000031b, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
    ICreateErrorInfo *c; IMarshal *m = NULL; CLSID cl; HRESULT hr;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IMarshal, (void **)&m);
    if (!m) { CHECK(m != NULL); ICreateErrorInfo_Release(c); return; }
    memset(&cl, 0xcc, sizeof cl);
    hr = IMarshal_GetUnmarshalClass(m, &DI_IErrorInfo, NULL, MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL, &cl);
    EXPECT_EQ(hr, S_OK);
    CHECK(IsEqualGUID(&cl, &clsid_errobj));
    IMarshal_Release(m);
    ICreateErrorInfo_Release(c);
}

/* ---------------- defaults / setters / getters ---------------- */
TEST(dtd_err_defaults_after_create)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; GUID g; BSTR b = (BSTR)1; DWORD ctx = 0xdead; HRESULT hr;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    memset(&g, 0xcc, sizeof g);
    EXPECT_EQ(IErrorInfo_GetGUID(e, &g), S_OK);
    CHECK(IsEqualGUID(&g, &DI_NULL));
    b = (BSTR)1; hr = IErrorInfo_GetSource(e, &b);       EXPECT_EQ(hr, S_OK); CHECK(b == NULL);
    b = (BSTR)1; hr = IErrorInfo_GetDescription(e, &b);  EXPECT_EQ(hr, S_OK); CHECK(b == NULL);
    b = (BSTR)1; hr = IErrorInfo_GetHelpFile(e, &b);     EXPECT_EQ(hr, S_OK); CHECK(b == NULL);
    EXPECT_EQ(IErrorInfo_GetHelpContext(e, &ctx), S_OK);
    EXPECT_EQ(ctx, 0);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_getters_null_out_is_einvalidarg)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    EXPECT_EQ(IErrorInfo_GetGUID(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetSource(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetDescription(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetHelpFile(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetHelpContext(e, NULL), E_INVALIDARG);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_getters_null_out_even_when_strings_set)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_SetSource(c, (LPOLESTR)L"s"); ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"d"); ICreateErrorInfo_SetHelpFile(c, (LPOLESTR)L"h");
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    EXPECT_EQ(IErrorInfo_GetSource(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetDescription(e, NULL), E_INVALIDARG);
    EXPECT_EQ(IErrorInfo_GetHelpFile(e, NULL), E_INVALIDARG);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_set_get_all_fields_roundtrip)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; GUID g; BSTR s = NULL, d = NULL, h = NULL; DWORD ctx = 0;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    EXPECT_EQ(ICreateErrorInfo_SetGUID(c, &kGuid), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetSource(c, (LPOLESTR)L"sprocket"), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"Failed Sprocket"), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetHelpFile(c, (LPOLESTR)L"sprocket.hlp"), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetHelpContext(c, 0xdeadbeef), S_OK);
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    EXPECT_EQ(IErrorInfo_GetGUID(e, &g), S_OK);          CHECK(IsEqualGUID(&g, &kGuid));
    EXPECT_EQ(IErrorInfo_GetSource(e, &s), S_OK);        CHECK(bs_eq(s, L"sprocket"));
    EXPECT_EQ(IErrorInfo_GetDescription(e, &d), S_OK);   CHECK(bs_eq(d, L"Failed Sprocket"));
    EXPECT_EQ(IErrorInfo_GetHelpFile(e, &h), S_OK);      CHECK(bs_eq(h, L"sprocket.hlp"));
    EXPECT_EQ(IErrorInfo_GetHelpContext(e, &ctx), S_OK); EXPECT_EQ(ctx, 0xdeadbeef);
    EXPECT_EQ(bs_len(s), 8); EXPECT_EQ(bs_len(d), 15); EXPECT_EQ(bs_len(h), 12);
    bs_free(s); bs_free(d); bs_free(h);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_fields_are_independent)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR s = NULL, d = NULL, h = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"only-desc");
    IErrorInfo_GetSource(e, &s); IErrorInfo_GetDescription(e, &d); IErrorInfo_GetHelpFile(e, &h);
    CHECK(s == NULL); CHECK(bs_eq(d, L"only-desc")); CHECK(h == NULL);
    bs_free(d); d = NULL;
    ICreateErrorInfo_SetHelpFile(c, (LPOLESTR)L"only-help");
    IErrorInfo_GetSource(e, &s); IErrorInfo_GetDescription(e, &d); IErrorInfo_GetHelpFile(e, &h);
    CHECK(s == NULL); CHECK(bs_eq(d, L"only-desc")); CHECK(bs_eq(h, L"only-help"));
    bs_free(d); bs_free(h); d = h = NULL;
    ICreateErrorInfo_SetSource(c, (LPOLESTR)L"only-src");
    IErrorInfo_GetSource(e, &s); IErrorInfo_GetDescription(e, &d); IErrorInfo_GetHelpFile(e, &h);
    CHECK(bs_eq(s, L"only-src")); CHECK(bs_eq(d, L"only-desc")); CHECK(bs_eq(h, L"only-help"));
    bs_free(s); bs_free(d); bs_free(h);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_getters_return_fresh_copies)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR a = NULL, b = NULL; const WCHAR *src = L"copy-me";
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)src);
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    IErrorInfo_GetDescription(e, &a); IErrorInfo_GetDescription(e, &b);
    CHECK(a && b && a != b);
    CHECK(a != src && b != src);
    bs_free(a);
    CHECK(bs_eq(b, L"copy-me"));   /* freeing one copy does not disturb the other */
    bs_free(b);
    IErrorInfo_GetDescription(e, &a);
    CHECK(bs_eq(a, L"copy-me")); bs_free(a);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_setter_copies_caller_string)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; WCHAR buf[16] = L"original"; BSTR b = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_SetSource(c, (LPOLESTR)buf);
    memcpy(buf, L"clobber", sizeof(L"clobber"));
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    IErrorInfo_GetSource(e, &b);
    CHECK(bs_eq(b, L"original")); bs_free(b);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_set_null_string_clears)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR b = (BSTR)1;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    ICreateErrorInfo_SetSource(c, (LPOLESTR)L"x"); ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"y"); ICreateErrorInfo_SetHelpFile(c, (LPOLESTR)L"z");
    EXPECT_EQ(ICreateErrorInfo_SetSource(c, NULL), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetDescription(c, NULL), S_OK);
    EXPECT_EQ(ICreateErrorInfo_SetHelpFile(c, NULL), S_OK);
    b = (BSTR)1; EXPECT_EQ(IErrorInfo_GetSource(e, &b), S_OK); CHECK(b == NULL);
    b = (BSTR)1; EXPECT_EQ(IErrorInfo_GetDescription(e, &b), S_OK); CHECK(b == NULL);
    b = (BSTR)1; EXPECT_EQ(IErrorInfo_GetHelpFile(e, &b), S_OK); CHECK(b == NULL);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_set_empty_string_is_distinct_from_null)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR b = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    EXPECT_EQ(ICreateErrorInfo_SetDescription(c, (LPOLESTR)L""), S_OK);
    EXPECT_EQ(IErrorInfo_GetDescription(e, &b), S_OK);
    CHECK(b != NULL);
    if (b) { EXPECT_EQ(bs_len(b), 0); bs_free(b); }
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_set_overwrites_previous)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR b = NULL; int i;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    for (i = 0; i < 50; i++) {
        WCHAR w[16]; wsprintfW(w, L"msg-%d", i);
        ICreateErrorInfo_SetDescription(c, (LPOLESTR)w);
    }
    IErrorInfo_GetDescription(e, &b);
    CHECK(bs_eq(b, L"msg-49")); bs_free(b);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_long_and_unicode_strings)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR b = NULL; static WCHAR big[5001]; WCHAR uni[8] = { 0x3b1, 0x3b2, 0x20ac, 0xd83d, 0xde00, 0xe9, 0 };
    int i;
    REQUIRE(CE(CreateErrorInfo));
    for (i = 0; i < 5000; i++) big[i] = (WCHAR)('a' + (i % 26));
    big[5000] = 0;
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)big);
    IErrorInfo_GetDescription(e, &b);
    EXPECT_EQ(bs_len(b), 5000); CHECK(b && !memcmp(b, big, sizeof big)); bs_free(b); b = NULL;
    ICreateErrorInfo_SetSource(c, (LPOLESTR)uni);
    IErrorInfo_GetSource(e, &b);
    CHECK(b && !memcmp(b, uni, sizeof uni)); EXPECT_EQ(bs_len(b), 6); bs_free(b);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_embedded_state_help_context_values)
{
    static const DWORD vals[] = { 0, 1, 0x7fffffff, 0x80000000, 0xffffffff, 0x12345678 };
    ICreateErrorInfo *c; IErrorInfo *e = NULL; unsigned i;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        DWORD got = 0xa5a5a5a5;
        EXPECT_EQ(ICreateErrorInfo_SetHelpContext(c, vals[i]), S_OK);
        EXPECT_EQ(IErrorInfo_GetHelpContext(e, &got), S_OK);
        CHECKF(got == vals[i], "set %08lx got %08lx", (unsigned long)vals[i], (unsigned long)got);
    }
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_guid_values_roundtrip)
{
    static const GUID vals[] = {
        { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } },
        { 0xffffffff, 0xffff, 0xffff, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } },
        { 0x00020400, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } },
        { 0x01234567, 0x89ab, 0xcdef, { 1, 2, 3, 4, 5, 6, 7, 8 } },
    };
    ICreateErrorInfo *c; IErrorInfo *e = NULL; unsigned i;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        GUID g; memset(&g, 0x5a, sizeof g);
        EXPECT_EQ(ICreateErrorInfo_SetGUID(c, &vals[i]), S_OK);
        EXPECT_EQ(IErrorInfo_GetGUID(e, &g), S_OK);
        CHECKF(IsEqualGUID(&g, &vals[i]), "i=%u", i);
    }
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
}

TEST(dtd_err_object_survives_creator_release)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL; BSTR b = NULL;
    REQUIRE(CE(CreateErrorInfo));
    c = mk(); if (!c) return;
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"persist");
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    EXPECT_EQ(ICreateErrorInfo_Release(c), 1);
    IErrorInfo_GetDescription(e, &b);
    CHECK(bs_eq(b, L"persist")); bs_free(b);
    EXPECT_EQ(IErrorInfo_Release(e), 0);
}

/* ---------------- thread-local SetErrorInfo / GetErrorInfo ---------------- */
static IErrorInfo *mk_info(const WCHAR *desc, ICreateErrorInfo **keep)
{
    ICreateErrorInfo *c = mk(); IErrorInfo *e = NULL;
    if (!c) return NULL;
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)desc);
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    if (keep) *keep = c; else ICreateErrorInfo_Release(c);
    return e;
}

TEST(dtd_err_thread_set_then_get_same_object)
{
    IErrorInfo *e, *got = (IErrorInfo *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"one", NULL); if (!e) return;
    EXPECT_EQ(CE(SetErrorInfo)(0, e), S_OK);
    hr = CE(GetErrorInfo)(0, &got);
    EXPECT_EQ(hr, S_OK);
    CHECK(got == e);
    IErrorInfo_Release(e);
    if (got) { BSTR b = NULL; IErrorInfo_GetDescription(got, &b); CHECK(bs_eq(b, L"one")); bs_free(b); IErrorInfo_Release(got); }
    drain();
}

TEST(dtd_err_thread_get_clears_slot)
{
    IErrorInfo *e, *got = NULL; HRESULT hr;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"clear-on-read", NULL); if (!e) return;
    CE(SetErrorInfo)(0, e);
    hr = CE(GetErrorInfo)(0, &got); EXPECT_EQ(hr, S_OK); if (got) IErrorInfo_Release(got);
    got = (IErrorInfo *)(ULONG_PTR)1;
    hr = CE(GetErrorInfo)(0, &got);
    EXPECT_EQ(hr, S_FALSE);
    CHECK(got == NULL);
    IErrorInfo_Release(e);
}

TEST(dtd_err_thread_get_when_empty_is_sfalse_with_null)
{
    IErrorInfo *got = (IErrorInfo *)(ULONG_PTR)0x77;
    REQUIRE(CE(GetErrorInfo));
    drain();
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_FALSE);
    CHECK(got == NULL);
}

TEST(dtd_err_get_null_pointer_is_einvalidarg)
{
    REQUIRE(CE(GetErrorInfo));
    EXPECT_EQ(CE(GetErrorInfo)(0, NULL), E_INVALIDARG);
}

TEST(dtd_err_get_reserved_nonzero_is_einvalidarg_and_nulls_out)
{
    IErrorInfo *e, *got = (IErrorInfo *)(ULONG_PTR)0x77; HRESULT hr;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"keep", NULL); if (!e) return;
    CE(SetErrorInfo)(0, e);
    hr = CE(GetErrorInfo)(0xdeadbeef, &got);
    EXPECT_EQ(hr, E_INVALIDARG);
    CHECK(got == NULL);
    got = NULL;
    hr = CE(GetErrorInfo)(0, &got);     /* the stored error was not consumed by the bad call */
    EXPECT_EQ(hr, S_OK); CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    IErrorInfo_Release(e);
    drain();
}

TEST(dtd_err_get_reserved_values_table)
{
    static const ULONG vals[] = { 1, 2, 0x80000000, 0xffffffff, 0x100 };
    unsigned i;
    REQUIRE(CE(GetErrorInfo)); REQUIRE(CE(SetErrorInfo));
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        IErrorInfo *got = (IErrorInfo *)(ULONG_PTR)1;
        HRESULT hr = CE(GetErrorInfo)(vals[i], &got);
        CHECKF(hr == E_INVALIDARG && got == NULL, "reserved=%lx hr=%08lx", (unsigned long)vals[i], (unsigned long)hr);
        hr = CE(SetErrorInfo)(vals[i], NULL);
        CHECKF(hr == E_INVALIDARG, "set reserved=%lx hr=%08lx", (unsigned long)vals[i], (unsigned long)hr);
    }
}

TEST(dtd_err_set_reserved_nonzero_leaves_current_error)
{
    IErrorInfo *e, *other, *got = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"current", NULL); other = mk_info(L"other", NULL); if (!e || !other) return;
    CE(SetErrorInfo)(0, e);
    EXPECT_EQ(CE(SetErrorInfo)(1, other), E_INVALIDARG);
    EXPECT_EQ(CE(SetErrorInfo)(0xdeadbeef, NULL), E_INVALIDARG);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    IErrorInfo_Release(e); IErrorInfo_Release(other);
    drain();
}

TEST(dtd_err_set_null_clears_current)
{
    IErrorInfo *e, *got = (IErrorInfo *)(ULONG_PTR)1;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"to-clear", NULL); if (!e) return;
    CE(SetErrorInfo)(0, e);
    EXPECT_EQ(CE(SetErrorInfo)(0, NULL), S_OK);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_FALSE);
    CHECK(got == NULL);
    IErrorInfo_Release(e);
}

TEST(dtd_err_set_null_when_already_empty_is_ok)
{
    REQUIRE(CE(SetErrorInfo));
    drain();
    EXPECT_EQ(CE(SetErrorInfo)(0, NULL), S_OK);
    EXPECT_EQ(CE(SetErrorInfo)(0, NULL), S_OK);
}

TEST(dtd_err_set_replaces_previous)
{
    IErrorInfo *a, *b, *got = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    a = mk_info(L"first", NULL); b = mk_info(L"second", NULL); if (!a || !b) return;
    CE(SetErrorInfo)(0, a);
    CE(SetErrorInfo)(0, b);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == b);
    if (got) IErrorInfo_Release(got);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_FALSE);
    IErrorInfo_Release(a); IErrorInfo_Release(b);
}

TEST(dtd_err_set_refcount_ownership)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL, *got = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);   /* 2 */
    ICreateErrorInfo_Release(c);                                         /* 1 */
    CE(SetErrorInfo)(0, e);                                              /* 2: thread slot holds a ref */
    EXPECT_EQ(IErrorInfo_AddRef(e), 3); IErrorInfo_Release(e);
    CE(GetErrorInfo)(0, &got);                                           /* ownership moves: still 2 */
    CHECK(got == e);
    EXPECT_EQ(IErrorInfo_AddRef(e), 3); IErrorInfo_Release(e);
    EXPECT_EQ(IErrorInfo_Release(e), 1);
    EXPECT_EQ(IErrorInfo_Release(got), 0);
}

TEST(dtd_err_set_null_releases_slot_reference)
{
    ICreateErrorInfo *c; IErrorInfo *e = NULL;
    REQUIRE(CE(SetErrorInfo));
    drain();
    c = mk(); if (!c) return;
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    ICreateErrorInfo_Release(c);
    CE(SetErrorInfo)(0, e);
    EXPECT_EQ(IErrorInfo_AddRef(e), 3); IErrorInfo_Release(e);
    CE(SetErrorInfo)(0, NULL);
    EXPECT_EQ(IErrorInfo_AddRef(e), 2); IErrorInfo_Release(e);
    EXPECT_EQ(IErrorInfo_Release(e), 0);
}

TEST(dtd_err_set_replace_releases_old_adds_new)
{
    IErrorInfo *a, *b;
    REQUIRE(CE(SetErrorInfo));
    drain();
    a = mk_info(L"a", NULL); b = mk_info(L"b", NULL); if (!a || !b) return;
    CE(SetErrorInfo)(0, a);
    EXPECT_EQ(IErrorInfo_AddRef(a), 3); IErrorInfo_Release(a);
    CE(SetErrorInfo)(0, b);
    EXPECT_EQ(IErrorInfo_AddRef(a), 2); IErrorInfo_Release(a);   /* old one released */
    EXPECT_EQ(IErrorInfo_AddRef(b), 3); IErrorInfo_Release(b);   /* new one held */
    CE(SetErrorInfo)(0, NULL);
    IErrorInfo_Release(a); IErrorInfo_Release(b);
}

TEST(dtd_err_set_same_object_twice_keeps_one_slot_ref)
{
    IErrorInfo *a;
    REQUIRE(CE(SetErrorInfo));
    drain();
    a = mk_info(L"same", NULL); if (!a) return;
    CE(SetErrorInfo)(0, a);
    CE(SetErrorInfo)(0, a);
    EXPECT_EQ(IErrorInfo_AddRef(a), 3); IErrorInfo_Release(a);
    CE(SetErrorInfo)(0, NULL);
    EXPECT_EQ(IErrorInfo_Release(a), 0);
}

/* custom IErrorInfo objects */
typedef struct CustErr {
    const void *const *vtbl;
    LONG ref;
    int qi_mode;           /* 0: succeed with self; 1: fail with HR_E_FAIL; 2: return alias */
    struct CustErr *alias;
    LONG qi_calls;
} CustErr;
static HRESULT WINAPI ce_QI(CustErr *o, REFIID riid, void **ppv)
{
    o->qi_calls++;
    if (o->qi_mode == 1) { *ppv = NULL; return HR_E_FAIL; }
    if (IsEqualIID(riid, &DI_IErrorInfo) || IsEqualIID(riid, &DI_IUnknown)) {
        CustErr *t = (o->qi_mode == 2 && o->alias) ? o->alias : o;
        *ppv = t; InterlockedIncrement(&t->ref); return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ce_AddRef(CustErr *o) { return InterlockedIncrement(&o->ref); }
static ULONG WINAPI ce_Release(CustErr *o) { return InterlockedDecrement(&o->ref); }
static HRESULT WINAPI ce_GetGUID(CustErr *o, GUID *g) { *g = kGuid; return S_OK; }
static HRESULT WINAPI ce_GetStr(CustErr *o, BSTR *b) { *b = NULL; return S_OK; }
static HRESULT WINAPI ce_GetCtx(CustErr *o, DWORD *c) { *c = 5; return S_OK; }
static const void *const ce_vtbl[8] = { ce_QI, ce_AddRef, ce_Release, ce_GetGUID, ce_GetStr, ce_GetStr, ce_GetStr, ce_GetCtx };
static void ce_init(CustErr *o, int mode) { memset(o, 0, sizeof *o); o->vtbl = ce_vtbl; o->ref = 1; o->qi_mode = mode; }

TEST(dtd_err_custom_object_accepted_without_validation)
{
    CustErr o; IErrorInfo *got = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    ce_init(&o, 0);
    EXPECT_EQ(CE(SetErrorInfo)(0, (IErrorInfo *)&o), S_OK);
    EXPECT_EQ(o.ref, 2);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == (IErrorInfo *)&o);
    EXPECT_EQ(o.qi_calls, 1);
    EXPECT_EQ(o.ref, 2);   /* QI AddRef'd (3) then the slot ref was released (2) */
    if (got) IErrorInfo_Release(got);
    EXPECT_EQ(o.ref, 1);
}

TEST(dtd_err_get_qi_failure_keeps_error_and_propagates_hr)
{
    CustErr o; IErrorInfo *got = (IErrorInfo *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    ce_init(&o, 1);
    CE(SetErrorInfo)(0, (IErrorInfo *)&o);
    EXPECT_EQ(o.ref, 2);
    hr = CE(GetErrorInfo)(0, &got);
    EXPECT_EQ(hr, HR_E_FAIL);
    EXPECT_EQ(o.qi_calls, 1);
    EXPECT_EQ(o.ref, 2);   /* still owned by the thread slot */
    hr = CE(GetErrorInfo)(0, &got);
    EXPECT_EQ(hr, HR_E_FAIL);   /* error was not consumed */
    EXPECT_EQ(o.qi_calls, 2);
    EXPECT_EQ(CE(SetErrorInfo)(0, NULL), S_OK);
    EXPECT_EQ(o.ref, 1);
}

TEST(dtd_err_get_returns_qi_result_pointer)
{
    CustErr outer, inner; IErrorInfo *got = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    ce_init(&inner, 0); ce_init(&outer, 2); outer.alias = &inner;
    CE(SetErrorInfo)(0, (IErrorInfo *)&outer);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == (IErrorInfo *)&inner);        /* the QI(IID_IErrorInfo) pointer, not the stored one */
    EXPECT_EQ(outer.ref, 1);                   /* slot ref released */
    EXPECT_EQ(inner.ref, 2);
    if (got) IErrorInfo_Release(got);
}

static DWORD WINAPI err_worker_empty(LPVOID p)
{
    IErrorInfo *got = (IErrorInfo *)(ULONG_PTR)1;
    HRESULT hr = OA(GetErrorInfo)(0, &got);
    return (hr == S_FALSE && got == NULL) ? 1 : 0;
}

TEST(dtd_err_slot_is_per_thread)
{
    IErrorInfo *e, *got = NULL; HANDLE h; DWORD code = 0;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"main-thread", NULL); if (!e) return;
    CE(SetErrorInfo)(0, e);
    h = CreateThread(NULL, 0, err_worker_empty, NULL, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0); GetExitCodeThread(h, &code); CloseHandle(h); }
    EXPECT_EQ(code, 1);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);   /* still there for the owning thread */
    CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    IErrorInfo_Release(e);
}

static DWORD WINAPI err_worker_set(LPVOID p)
{
    ICreateErrorInfo *c = NULL; IErrorInfo *e = NULL; DWORD r = 0;
    if (OA(CreateErrorInfo)(&c) != S_OK) return 0;
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"worker-error");
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    ICreateErrorInfo_Release(c);
    if (OA(SetErrorInfo)(0, e) == S_OK) r = 1;
    IErrorInfo_Release(e);
    return r;   /* thread exits with the error still stored */
}

TEST(dtd_err_worker_set_not_visible_to_main)
{
    IErrorInfo *got = (IErrorInfo *)(ULONG_PTR)1; HANDLE h; DWORD code = 0;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    h = CreateThread(NULL, 0, err_worker_set, NULL, 0, NULL);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0); GetExitCodeThread(h, &code); CloseHandle(h); }
    EXPECT_EQ(code, 1);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_FALSE);
    CHECK(got == NULL);
}

TEST(dtd_err_com_init_does_not_disturb_slot_across_calls)
{
    IErrorInfo *e, *got = NULL; HRESULT hr;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo)); REQUIRE(OL(CoInitializeEx)); REQUIRE(OL(CoUninitialize));
    drain();
    e = mk_info(L"across-init", NULL); if (!e) return;
    hr = OL(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    CHECK(hr == S_OK || hr == S_FALSE);
    CE(SetErrorInfo)(0, e);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    OL(CoUninitialize)();
    IErrorInfo_Release(e);
}

/* ---------------- ISupportErrorInfo style plumbing ---------------- */
typedef struct Thrower { const void *const *vtbl; LONG ref; } Thrower;
static HRESULT WINAPI th_QI(Thrower *t, REFIID r, void **p)
{
    if (IsEqualIID(r, &DI_IUnknown) || IsEqualIID(r, &DI_ISupportErrorInfo)) { *p = t; t->ref++; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI th_AddRef(Thrower *t) { return ++t->ref; }
static ULONG WINAPI th_Release(Thrower *t) { return --t->ref; }
static HRESULT WINAPI th_Supports(Thrower *t, REFIID r) { return IsEqualIID(r, &DI_IDispatch) ? S_OK : S_FALSE; }
static const void *const th_vtbl[4] = { th_QI, th_AddRef, th_Release, th_Supports };

static HRESULT thrower_fail(void)
{
    ICreateErrorInfo *c = NULL; IErrorInfo *e = NULL;
    if (OA(CreateErrorInfo)(&c) != S_OK) return E_FAIL;
    ICreateErrorInfo_SetGUID(c, &DI_IDispatch);
    ICreateErrorInfo_SetSource(c, (LPOLESTR)L"Thrower.Object");
    ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"thrower failed on purpose");
    ICreateErrorInfo_SetHelpFile(c, (LPOLESTR)L"thrower.hlp");
    ICreateErrorInfo_SetHelpContext(c, 77);
    ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e);
    OA(SetErrorInfo)(0, e);
    IErrorInfo_Release(e); ICreateErrorInfo_Release(c);
    return DISP_E_EXCEPTION;
}

TEST(dtd_err_full_plumbing_scenario)
{
    Thrower t; ISupportErrorInfo *sei = NULL; IErrorInfo *e = NULL; HRESULT hr;
    GUID g; BSTR s = NULL, d = NULL, h = NULL; DWORD ctx = 0;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    t.vtbl = th_vtbl; t.ref = 1;
    hr = thrower_fail();
    EXPECT_EQ(hr, DISP_E_EXCEPTION);
    hr = th_QI(&t, &DI_ISupportErrorInfo, (void **)&sei);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(ISupportErrorInfo_InterfaceSupportsErrorInfo(sei, &DI_IDispatch), S_OK);
    EXPECT_EQ(ISupportErrorInfo_InterfaceSupportsErrorInfo(sei, &DI_ITypeInfo), S_FALSE);
    ISupportErrorInfo_Release(sei);
    EXPECT_EQ(CE(GetErrorInfo)(0, &e), S_OK);
    if (e) {
        EXPECT_EQ(IErrorInfo_GetGUID(e, &g), S_OK); CHECK(IsEqualGUID(&g, &DI_IDispatch));
        IErrorInfo_GetSource(e, &s); IErrorInfo_GetDescription(e, &d); IErrorInfo_GetHelpFile(e, &h); IErrorInfo_GetHelpContext(e, &ctx);
        CHECK(bs_eq(s, L"Thrower.Object")); CHECK(bs_eq(d, L"thrower failed on purpose")); CHECK(bs_eq(h, L"thrower.hlp"));
        EXPECT_EQ(ctx, 77);
        bs_free(s); bs_free(d); bs_free(h);
        EXPECT_EQ(IErrorInfo_Release(e), 0);
    }
    EXPECT_EQ(CE(GetErrorInfo)(0, &e), S_FALSE);
}

TEST(dtd_err_second_failure_overwrites_first)
{
    IErrorInfo *e = NULL; BSTR d = NULL;
    REQUIRE(CE(SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    thrower_fail();
    { ICreateErrorInfo *c = mk(); IErrorInfo *e2 = NULL;
      if (!c) return;
      ICreateErrorInfo_SetDescription(c, (LPOLESTR)L"second");
      ICreateErrorInfo_QueryInterface(c, &DI_IErrorInfo, (void **)&e2);
      CE(SetErrorInfo)(0, e2);
      IErrorInfo_Release(e2); ICreateErrorInfo_Release(c); }
    EXPECT_EQ(CE(GetErrorInfo)(0, &e), S_OK);
    if (e) { IErrorInfo_GetDescription(e, &d); CHECK(bs_eq(d, L"second")); bs_free(d); IErrorInfo_Release(e); }
    EXPECT_EQ(CE(GetErrorInfo)(0, &e), S_FALSE);
}

/* ---------------- export parity oleaut32 vs combase ---------------- */
TEST(dtd_err_combase_exports_share_slot_with_oleaut32)
{
    IErrorInfo *e, *got = NULL;
    REQUIRE(UIN("combase.dll", GetErrorInfo)); REQUIRE(UIN("combase.dll", SetErrorInfo)); REQUIRE(CE(GetErrorInfo));
    drain();
    e = mk_info(L"via-combase", NULL); if (!e) return;
    EXPECT_EQ(UIN("combase.dll", SetErrorInfo)(0, e), S_OK);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    EXPECT_EQ(UIN("combase.dll", GetErrorInfo)(0, &got), S_FALSE);
    IErrorInfo_Release(e);
}

TEST(dtd_err_combase_create_error_info_null_out)
{
    REQUIRE(UIN("combase.dll", CreateErrorInfo));
    EXPECT_EQ(UIN("combase.dll", CreateErrorInfo)(NULL), E_INVALIDARG);
}

TEST(dtd_err_ole32_reexports_when_present)
{
    IErrorInfo *e, *got = NULL;
    if (!UIN("ole32.dll", GetErrorInfo) || !UIN("ole32.dll", SetErrorInfo)) { t_skip("ole32 does not export Get/SetErrorInfo"); return; }
    drain();
    e = mk_info(L"via-ole32", NULL); if (!e) return;
    EXPECT_EQ(UIN("ole32.dll", SetErrorInfo)(0, e), S_OK);
    EXPECT_EQ(CE(GetErrorInfo)(0, &got), S_OK);
    CHECK(got == e);
    if (got) IErrorInfo_Release(got);
    IErrorInfo_Release(e);
}
