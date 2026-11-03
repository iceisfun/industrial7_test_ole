/* Typelib marshaling: interface pointers, callbacks, records, enums and error info (IOletCustom). */
#include "mar.h"
#define M(c, name, ...) ((c)->lpVtbl->name((c), __VA_ARGS__))
#define M0(c, name) ((c)->lpVtbl->name((c)))

/* ------------------------------------------------------------------ interface pointers */
static void cl_getsink(void *p, void *ctx)
{
    CLI(c, p); IOletSink *s = NULL; HRESULT hr; LONG t = -1; BSTR str;
    hr = M(c, GetSink, &s);
    EQ(hr, S_OK);
    CHECK(s != NULL);
    if (!s) return;
    EQ(s->lpVtbl->Total(s, &t), S_OK); EQ(t, 100);
    str = SysAllocString(L"hello");
    memset(&g_last, 0, sizeof g_last);
    EQ(s->lpVtbl->Notify(s, 5, str), S_OK);
    SysFreeString(str);
    EQ(g_last.nsink, 1);
    CHECK(!lstrcmpW(g_last.str, L"hello"));
    EQ(g_last.strlen_, 5);
    EQ(s->lpVtbl->Total(s, &t), S_OK); EQ(t, 105);
    /* the returned interface is a separate object per call */
    {
        IOletSink *s2 = NULL; LONG t2 = -1;
        EQ(M(c, GetSink, &s2), S_OK);
        CHECK(s2 != s);
        if (s2) { EQ(s2->lpVtbl->Total(s2, &t2), S_OK); EQ(t2, 100); s2->lpVtbl->Release(s2); }
    }
    s->lpVtbl->Release(s);
}
TMTEST(dxtm_mar_interface_out_param) { RUN_CUSTOM(cl_getsink, NULL); }

static void cl_sink_qi(void *p, void *ctx)
{
    CLI(c, p); IOletSink *s = NULL; void *x = NULL; IUnknown *u1 = NULL, *u2 = NULL; HRESULT hr;
    EQ(M(c, GetSink, &s), S_OK);
    if (!s) return;
    EQ(s->lpVtbl->QueryInterface(s, &TM_IID_IOletSink, &x), S_OK);
    CHECK(x == s); if (x) ((IOletSink *)x)->lpVtbl->Release((IOletSink *)x);
    hr = s->lpVtbl->QueryInterface(s, &IID_IUnknown, (void **)&u1);
    EQ(hr, S_OK);
    hr = s->lpVtbl->QueryInterface(s, &IID_IUnknown, (void **)&u2);
    EQ(hr, S_OK);
    CHECK(u1 == u2);        /* IUnknown identity of one proxy */
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    x = (void *)(ULONG_PTR)1;
    hr = s->lpVtbl->QueryInterface(s, &TM_IID_IOletCustom, &x);
    CHECK(FAILED(hr));
    EQ(hr, E_NOINTERFACE);
    CHECK(x == NULL);
    s->lpVtbl->Release(s);
}
TMTEST(dxtm_mar_proxy_queryinterface) { RUN_CUSTOM(cl_sink_qi, NULL); }

static void cl_custom_qi(void *p, void *ctx)
{
    CLI(c, p); void *x = (void *)(ULONG_PTR)1; HRESULT hr; IOletCustom *c2 = NULL; ISupportErrorInfo *sei = NULL;
    hr = IUnknown_QueryInterface((IUnknown *)c, &TM_IID_IOletDual, &x);
    EQ(hr, E_NOINTERFACE); CHECK(x == NULL);
    hr = IUnknown_QueryInterface((IUnknown *)c, &TM_IID_IOletCustom, (void **)&c2);
    EQ(hr, S_OK); CHECK(c2 == c); if (c2) M0(c2, Release);
    /* ISupportErrorInfo of the object is reachable through the proxy (the system registers a proxy for it) */
    hr = IUnknown_QueryInterface((IUnknown *)c, &TM_IID_ISupportErrorInfo, (void **)&sei);
    EQ(hr, S_OK);
    if (sei) {
        EQ(ISupportErrorInfo_InterfaceSupportsErrorInfo(sei, &TM_IID_IOletCustom), S_OK);
        EQ(ISupportErrorInfo_InterfaceSupportsErrorInfo(sei, &TM_IID_IOletSink), S_FALSE);
        ISupportErrorInfo_Release(sei);
    }
    hr = IUnknown_QueryInterface((IUnknown *)c, &IID_IMarshal, &x);
    t_note("QI(IMarshal) on a typelib proxy: hr=0x%lx", (long)hr);
    if (SUCCEEDED(hr) && x) IUnknown_Release((IUnknown *)x);
}
TMTEST(dxtm_mar_proxy_object_interfaces) { RUN_CUSTOM(cl_custom_qi, NULL); }

static void cl_refcount(void *p, void *ctx)
{
    CLI(c, p); ULONG r1, r2;
    r1 = M0(c, AddRef); r2 = M0(c, Release);
    CHECKF(r1 >= 2, "AddRef on a proxy returned %lu", (unsigned long)r1);
    CHECKF(r2 == r1 - 1 || r2 >= 1, "Release on a proxy returned %lu", (unsigned long)r2);
}
TMTEST(dxtm_mar_proxy_refcounting) { RUN_CUSTOM(cl_refcount, NULL); }

static void cl_putsink(void *p, void *ctx)
{
    CLI(c, p); IOletSink *mine = tm_new_sink(); LONG total = -1;
    BSTR s;
    EQ(M(c, PutSink, mine), S_OK);       /* the server keeps a proxy to our sink */
    EQ(M(c, Callback, mine, 7, &total), S_OK);
    EQ(total, 7);
    EQ(M(c, Callback, mine, 5, &total), S_OK);
    EQ(total, 12);
    EQ(tm_sink_total(mine), 12);
    s = SysAllocString(L"cb");
    CHECK(!lstrcmpW(g_last.str, s)); SysFreeString(s);
    EQ(M(c, PutSink, NULL), S_OK);       /* NULL releases the proxy */
    mine->lpVtbl->Release(mine);
}
TMTEST(dxtm_mar_interface_in_param_and_callback) { RUN_CUSTOM(cl_putsink, NULL); }

static void cl_callback_sink_remote(void *p, void *ctx)
{
    /* the client hands the server a sink that itself lives behind a proxy (GetSink result) */
    CLI(c, p); IOletSink *s = NULL; LONG total = -1;
    EQ(M(c, GetSink, &s), S_OK);
    if (!s) return;
    EQ(M(c, Callback, s, 11, &total), S_OK);       /* server calls back into its own object through our proxy and back */
    EQ(total, 111);
    s->lpVtbl->Release(s);
}
TMTEST(dxtm_mar_proxy_passed_back_to_its_server) { RUN_CUSTOM(cl_callback_sink_remote, NULL); }

static void cl_callback_null(void *p, void *ctx)
{
    CLI(c, p); LONG total = 77; HRESULT hr;
    /* the server dereferences the sink; a NULL [in] interface pointer reaches it as NULL (do not call it with NULL here) */
    hr = M(c, PutSink, NULL);
    EQ(hr, S_OK);
    (void)total;
}
TMTEST(dxtm_mar_null_interface_pointer_in) { RUN_CUSTOM(cl_callback_null, NULL); }

static void cl_echounk(void *p, void *ctx)
{
    CLI(c, p); IOletSink *mine = tm_new_sink(); IUnknown *out = NULL, *mine_unk = NULL; HRESULT hr;
    IUnknown_QueryInterface((IUnknown *)mine, &IID_IUnknown, (void **)&mine_unk);
    hr = M(c, EchoUnk, (IUnknown *)mine, &out);
    EQ(hr, S_OK);
    CHECK(out != NULL);
    if (out) {
        IUnknown *ou = NULL;
        IUnknown_QueryInterface(out, &IID_IUnknown, (void **)&ou);
        /* a client object that made the round trip through the server comes back as the original pointer */
        CHECK(ou == mine_unk);
        if (ou) IUnknown_Release(ou);
        IUnknown_Release(out);
    }
    out = (IUnknown *)(ULONG_PTR)1;
    hr = M(c, EchoUnk, NULL, &out);
    EQ(hr, S_OK);
    CHECK(out == NULL);
    IUnknown_Release(mine_unk);
    mine->lpVtbl->Release(mine);
}
TMTEST(dxtm_mar_iunknown_roundtrip_identity) { RUN_CUSTOM(cl_echounk, NULL); }

static void cl_echounk_wrong_iface(void *p, void *ctx)
{
    /* IUnknown* parameters accept any object: here an object the typelib marshaler has no registration for goes through the standard marshaler */
    CLI(c, p); IOletMini *m = tm_new_mini(&TM_IID_Mini8); IUnknown *out = NULL; HRESULT hr;
    hr = M(c, EchoUnk, (IUnknown *)m, &out);
    EQ(hr, S_OK);
    if (out) {
        void *x = NULL;
        EQ(IUnknown_QueryInterface(out, &TM_IID_Mini8, &x), S_OK);   /* same-apartment round trip: the raw object */
        if (x) IUnknown_Release((IUnknown *)x);
        IUnknown_Release(out);
    }
    m->lpVtbl->Release(m);
}
TMTEST(dxtm_mar_iunknown_param_with_unregistered_iid) { RUN_CUSTOM(cl_echounk_wrong_iface, NULL); }

static void cl_lifetime(void *p, void *ctx)
{
    CLI(c, p); IOletSink *s = NULL; LONG t;
    EQ(M(c, GetSink, &s), S_OK);
    CHECK(s != NULL);
    if (!s) return;
    CHECK(tm_live_objects() >= 2);       /* the custom object and its sink */
    s->lpVtbl->Release(s);               /* last proxy reference releases the server object */
    {
        int i;
        for (i = 0; i < 100 && tm_live_objects() != 1; i++) Sleep(20);
        EQ(tm_live_objects(), 1);
    }
    (void)t;
}
TMTEST(dxtm_mar_releasing_proxy_releases_server_object) { RUN_CUSTOM(cl_lifetime, NULL); }

/* ------------------------------------------------------------------ records and enums */
static void cl_point(void *p, void *ctx)
{
    CLI(c, p); OletPoint in = { 3, -4 }, out = { 0, 0 }, io = { 10, 20 }; HRESULT hr;
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, EchoPoint, in, &out);
    EQ(hr, S_OK);
    EQ(g_last.point_x, 3); EQ(g_last.point_y, -4);
    EQ(out.x, 6); EQ(out.y, -8);
    hr = M(c, SwapPoint, &io);
    EQ(hr, S_OK);
    EQ(io.x, 20); EQ(io.y, 10);
}
TMTEST(dxtm_mar_record_by_value_and_pointer)
{
    if (t_is_wine()) { t_skip("Wine's ICreateTypeInfo::LayOut leaves record instance size 0 and its marshaler then faults"); return; }
    RUN_CUSTOM(cl_point, NULL);
}

static void cl_mix(void *p, void *ctx)
{
    CLI(c, p); OletMix in, out; HRESULT hr;
    memset(&in, 0, sizeof in); memset(&out, 0, sizeof out);
    in.name = SysAllocString(L"record"); in.d = 1.25; in.id = 41; in.pt.x = 1; in.pt.y = 2;
    hr = M(c, EchoMix, &in, &out);
    EQ(hr, S_OK);
    CHECK(out.name != NULL);
    if (out.name) EXPECT_WSTR(out.name, L"record!");
    CHECK(out.d == 2.5);
    EQ(out.id, 42);
    EQ(out.pt.x, 2); EQ(out.pt.y, 1);
    SysFreeString(out.name);
    SysFreeString(in.name);
    in.name = NULL; memset(&out, 0, sizeof out);
    hr = M(c, EchoMix, &in, &out);
    EQ(hr, S_OK);
    CHECK(out.name == NULL);
    EQ(out.id, 1);
}
TMTEST(dxtm_mar_record_with_bstr_field)
{
    if (t_is_wine()) { t_skip("Wine's ICreateTypeInfo::LayOut leaves record instance size 0"); return; }
    RUN_CUSTOM(cl_mix, NULL);
}

static void cl_enum(void *p, void *ctx)
{
    CLI(c, p); static const OletColor vals[] = { OLET_RED, OLET_GREEN, OLET_BLUE, 0, 3, 0x40000000 }; unsigned i;
    for (i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        OletColor out = -1; HRESULT hr = M(c, EchoColor, vals[i], &out);
        EQ(hr, S_OK);
        CHECKF(out == vals[i] * 2, "EchoColor(%ld)=%ld", (long)vals[i], (long)out);
    }
}
TMTEST(dxtm_mar_enum_parameter) { RUN_CUSTOM(cl_enum, NULL); }

/* ------------------------------------------------------------------ error info */
static void cl_errinfo(void *p, void *ctx)
{
    CLI(c, p); BSTR d = SysAllocString(L"something went wrong"); HRESULT hr; IErrorInfo *ei = NULL; BSTR s = NULL, desc = NULL, hf = NULL; GUID g; DWORD hc = 0;
    SetErrorInfo(0, NULL);
    hr = M(c, RaiseError, d, E_FAIL);
    EQ(hr, E_FAIL);
    hr = GetErrorInfo(0, &ei);
    EQ(hr, S_OK);
    CHECK(ei != NULL);
    if (ei) {
        EQ(IErrorInfo_GetDescription(ei, &desc), S_OK);
        CHECK(desc && !lstrcmpW(desc, L"something went wrong"));
        EQ(IErrorInfo_GetSource(ei, &s), S_OK);
        CHECK(s && !lstrcmpW(s, L"Olet.Server"));
        EQ(IErrorInfo_GetGUID(ei, &g), S_OK);
        CHECK(IsEqualGUID(&g, &TM_IID_IOletCustom));
        EQ(IErrorInfo_GetHelpFile(ei, &hf), S_OK);
        CHECK(hf && !lstrcmpW(hf, L"olet.hlp"));
        EQ(IErrorInfo_GetHelpContext(ei, &hc), S_OK);
        EQ(hc, 4242);
        SysFreeString(desc); SysFreeString(s); SysFreeString(hf);
        IErrorInfo_Release(ei);
    }
    /* GetErrorInfo clears the per-thread error object */
    ei = NULL;
    hr = GetErrorInfo(0, &ei);
    EQ(hr, S_FALSE);
    CHECK(ei == NULL);
    SysFreeString(d);
}
TMTEST(dxtm_mar_errorinfo_propagates_through_proxy) { RUN_CUSTOM(cl_errinfo, NULL); }

static void cl_errinfo_hrs(void *p, void *ctx)
{
    CLI(c, p); static const LONG v[] = { E_FAIL, E_INVALIDARG, (LONG)0x8abc0001, E_NOTIMPL }; unsigned i;
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        BSTR d = SysAllocString(L"d"); IErrorInfo *ei = NULL; HRESULT hr;
        hr = M(c, RaiseError, d, v[i]);
        EQ(hr, v[i]);
        hr = GetErrorInfo(0, &ei);
        CHECKF(hr == S_OK && ei, "error info for hr 0x%lx", (long)v[i]);
        if (ei) IErrorInfo_Release(ei);
        SysFreeString(d);
    }
}
TMTEST(dxtm_mar_errorinfo_with_various_hresults) { RUN_CUSTOM(cl_errinfo_hrs, NULL); }

static void cl_errinfo_none(void *p, void *ctx)
{
    CLI(c, p); LONG r; IErrorInfo *ei = (IErrorInfo *)(ULONG_PTR)1; HRESULT hr;
    SetErrorInfo(0, NULL);
    EQ(M(c, AddLong, 1, 1, &r), S_OK);
    hr = GetErrorInfo(0, &ei);
    EQ(hr, S_FALSE);
    CHECK(ei == NULL);
    /* a failing method that sets no error info leaves none */
    EQ(M(c, ReturnHr, E_FAIL), E_FAIL);
    ei = (IErrorInfo *)(ULONG_PTR)1;
    hr = GetErrorInfo(0, &ei);
    EQ(hr, S_FALSE);
    CHECK(ei == NULL);
}
TMTEST(dxtm_mar_no_errorinfo_without_server_error) { RUN_CUSTOM(cl_errinfo_none, NULL); }

static void cl_errinfo_long(void *p, void *ctx)
{
    CLI(c, p); BSTR d = SysAllocStringLen(NULL, 70000); IErrorInfo *ei = NULL; BSTR got = NULL; UINT i;
    for (i = 0; i < 70000; i++) d[i] = (WCHAR)(L'a' + i % 26);
    EQ(M(c, RaiseError, d, E_FAIL), E_FAIL);
    EQ(GetErrorInfo(0, &ei), S_OK);
    if (ei) {
        EQ(IErrorInfo_GetDescription(ei, &got), S_OK);
        CHECK(got && SysStringLen(got) == 70000 && !memcmp(got, d, 70000 * 2));
        SysFreeString(got); IErrorInfo_Release(ei);
    }
    SysFreeString(d);
}
TMTEST(dxtm_mar_errorinfo_description_over_64k) { RUN_CUSTOM(cl_errinfo_long, NULL); }

static void cl_errinfo_unsupported_iface(void *p, void *ctx)
{
    /* IOletSink::Notify does not set error info; the sink object does not implement ISupportErrorInfo, so nothing travels */
    CLI(c, p); IOletSink *s = NULL; IErrorInfo *ei = NULL;
    EQ(M(c, GetSink, &s), S_OK);
    if (!s) return;
    SetErrorInfo(0, NULL);
    EQ(s->lpVtbl->Notify(s, 1, NULL), S_OK);
    EQ(GetErrorInfo(0, &ei), S_FALSE);
    s->lpVtbl->Release(s);
}
TMTEST(dxtm_mar_second_interface_no_errorinfo) { RUN_CUSTOM(cl_errinfo_unsupported_iface, NULL); }
