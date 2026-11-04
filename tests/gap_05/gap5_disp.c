/* VarBstrFromDisp: VariantChangeTypeEx(VT_DISPATCH -> VT_BSTR) on the object's default (DISPID_VALUE) property. */
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
static const GUID g5_IID_IUnknown = {0,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g5_IID_IDispatch = {0x20400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g5_IID_NULL = {0};
#define IID_IUnknown g5_IID_IUnknown
#define IID_IDispatch g5_IID_IDispatch
#define IID_NULL g5_IID_NULL
#define VariantInit(v) do { memset((v), 0, sizeof(VARIANT)); } while (0)
#define VariantClear(v) U(VariantClear)(v)
#define VariantCopy(a, b) U(VariantCopy)(a, b)

typedef struct MockDisp {
    IDispatch iface;
    LONG ref;
    VARIANT value;          /* returned from Invoke(DISPID_VALUE) */
    HRESULT invoke_hr;
    LONG invokes;
    DISPID last_id; WORD last_flags; LCID last_lcid; UINT last_argc; int last_iid_null;
} MockDisp;

static MockDisp *impl(IDispatch *p) { return (MockDisp *)p; }
static HRESULT WINAPI md_qi(IDispatch *p, REFIID riid, void **o)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IDispatch)) { *o = p; InterlockedIncrement(&impl(p)->ref); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI md_addref(IDispatch *p) { return InterlockedIncrement(&impl(p)->ref); }
static ULONG WINAPI md_release(IDispatch *p) { return InterlockedDecrement(&impl(p)->ref); }
static HRESULT WINAPI md_gtic(IDispatch *p, UINT *n) { *n = 0; return S_OK; }
static HRESULT WINAPI md_gti(IDispatch *p, UINT i, LCID l, ITypeInfo **t) { *t = NULL; return E_NOTIMPL; }
static HRESULT WINAPI md_gids(IDispatch *p, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return DISP_E_UNKNOWNNAME; }
static HRESULT WINAPI md_invoke(IDispatch *p, DISPID id, REFIID riid, LCID lcid, WORD fl, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, UINT *ae)
{
    MockDisp *m = impl(p);
    m->invokes++; m->last_id = id; m->last_flags = fl; m->last_lcid = lcid;
    m->last_argc = dp ? dp->cArgs : 99; m->last_iid_null = IsEqualGUID(riid, &IID_NULL);
    if (FAILED(m->invoke_hr)) return m->invoke_hr;
    if (res) { VariantInit(res); VariantCopy(res, &m->value); }
    return S_OK;
}
/* Windows 10 VarBstrFromDisp hands VariantChangeTypeEx an UNINITIALISED stack VARIANT as the destination, and
 * VariantChangeTypeEx first clears whatever the destination holds: stack garbage in its vt gives DISP_E_BADVARTYPE.
 * Zero the stack below the caller right before the call so the result is deterministic. */
static void __attribute__((noinline)) g5_scrub_stack(void)
{
    volatile unsigned char buf[2048];
    unsigned i;
    for (i = 0; i < sizeof buf; i++) buf[i] = 0;
}
static HRESULT __attribute__((noinline)) g5_vbfd(IDispatch *d, LCID l, ULONG f, BSTR *o)
{
    g5_scrub_stack();
    return U(VarBstrFromDisp)(d, l, f, o);
}

static const IDispatchVtbl md_vtbl = { md_qi, md_addref, md_release, md_gtic, md_gti, md_gids, md_invoke };

static void md_init(MockDisp *m) { memset(m, 0, sizeof *m); m->iface.lpVtbl = (IDispatchVtbl *)&md_vtbl; m->ref = 1; VariantInit(&m->value); m->invoke_hr = S_OK; }

TEST(gap5_bstrfromdisp_bstr_default_property)
{
    MockDisp m; BSTR out = NULL; HRESULT hr;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(SysAllocString)); REQUIRE(U(SysFreeString));
    md_init(&m);
    V_VT(&m.value) = VT_BSTR; V_BSTR(&m.value) = U(SysAllocString)(L"hello disp");
    hr = g5_vbfd(&m.iface, 0x409, 0, &out);
    EXPECT_EQ(hr, S_OK);
    CHECK(out && !lstrcmpW(out, L"hello disp"));
    CHECK(m.invokes == 1);
    EXPECT_EQ(m.last_id, DISPID_VALUE);
    CHECK((m.last_flags & DISPATCH_PROPERTYGET) != 0);
    CHECK(m.last_argc == 0);
    CHECK(m.last_iid_null);
    EXPECT_EQ(m.last_lcid, 0x409);
    CHECK(m.ref == 1);   /* no leaked reference */
    U(SysFreeString)(out);
    VariantClear(&m.value);
}

TEST(gap5_bstrfromdisp_lcid_passed_to_invoke)
{
    static const LCID l[] = { 0x409, 0x407, 0x411 };
    unsigned i;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(SysFreeString));
    for (i = 0; i < 3; i++) {
        MockDisp m; BSTR out = NULL;
        md_init(&m); V_VT(&m.value) = VT_I4; V_I4(&m.value) = 5;
        EXPECT_EQ(g5_vbfd(&m.iface, l[i], 0, &out), S_OK);
        EXPECT_EQ(m.last_lcid, l[i]);
        U(SysFreeString)(out);
    }
}

TEST(gap5_bstrfromdisp_numeric_defaults)
{
    typedef struct { const char *n; VARTYPE vt; LONGLONG v; const WCHAR *want; } c;
    static const c t[] = { { "I4 42", VT_I4, 42, L"42" }, { "I2 -7", VT_I2, -7, L"-7" }, { "UI1 200", VT_UI1, 200, L"200" },
                           { "I4 0", VT_I4, 0, L"0" }, { "I8 big", VT_I8, 123456789012345LL, L"123456789012345" } };
    unsigned i;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(SysFreeString));
    for (i = 0; i < 5; i++) {
        MockDisp m; BSTR out = NULL; HRESULT hr;
        md_init(&m); V_VT(&m.value) = t[i].vt;
        switch (t[i].vt) { case VT_I4: V_I4(&m.value) = (LONG)t[i].v; break; case VT_I2: V_I2(&m.value) = (SHORT)t[i].v; break;
                           case VT_UI1: V_UI1(&m.value) = (BYTE)t[i].v; break; default: V_I8(&m.value) = t[i].v; }
        hr = g5_vbfd(&m.iface, 0x409, 0, &out);
        t_check(hr == S_OK && out && !lstrcmpW(out, t[i].want), __FILE__, __LINE__, t[i].n, "hr 0x%lx", (unsigned long)hr);
        U(SysFreeString)(out);
    }
}

TEST(gap5_bstrfromdisp_bool_and_empty)
{
    MockDisp m; BSTR out = NULL; HRESULT hr;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(SysFreeString));
    md_init(&m); V_VT(&m.value) = VT_BOOL; V_BOOL(&m.value) = VARIANT_TRUE;
    hr = g5_vbfd(&m.iface, 0x409, 0, &out);
    EXPECT_EQ(hr, S_OK); CHECK(out && !lstrcmpW(out, L"-1")); U(SysFreeString)(out);
    out = NULL; V_BOOL(&m.value) = VARIANT_FALSE;
    hr = g5_vbfd(&m.iface, 0x409, 0, &out);
    EXPECT_EQ(hr, S_OK); CHECK(out && !lstrcmpW(out, L"0")); U(SysFreeString)(out);
    out = NULL; V_VT(&m.value) = VT_EMPTY;
    hr = g5_vbfd(&m.iface, 0x409, 0, &out);
    t_note("default property VT_EMPTY -> hr 0x%lx bstr %ls", (unsigned long)hr, out ? out : L"(null)");
    U(SysFreeString)(out);
}

TEST(gap5_bstrfromdisp_invoke_failure_propagates)
{
    static const HRESULT h[] = { DISP_E_MEMBERNOTFOUND, E_FAIL, E_NOTIMPL, DISP_E_EXCEPTION };
    unsigned i;
    REQUIRE(U(VarBstrFromDisp));
    for (i = 0; i < 4; i++) {
        MockDisp m; BSTR out = (BSTR)(ULONG_PTR)0x1234; HRESULT hr;
        md_init(&m); m.invoke_hr = h[i];
        hr = g5_vbfd(&m.iface, 0x409, 0, &out);
        t_note("Invoke returned 0x%lx -> VarBstrFromDisp 0x%lx", (unsigned long)h[i], (unsigned long)hr);
        CHECKF(FAILED(hr), "failure must propagate as failure, got 0x%lx", (unsigned long)hr);
        /* decomp: *pbstrOut is written only when VariantChangeTypeEx succeeds */
        CHECK(out == (BSTR)(ULONG_PTR)0x1234);
        CHECK(m.invokes >= 1);
    }
}

TEST(gap5_bstrfromdisp_null_dispatch)
{
    BSTR out = (BSTR)(ULONG_PTR)0x1234; HRESULT hr;
    REQUIRE(U(VarBstrFromDisp));
    hr = g5_vbfd(NULL, 0x409, 0, &out);
    t_note("VarBstrFromDisp(NULL) -> 0x%lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    CHECK(out == (BSTR)(ULONG_PTR)0x1234);
}

TEST(gap5_bstrfromdisp_flags_ignored)
{
    /* decomp passes wFlags = 0 to VariantChangeTypeEx regardless of dwFlags */
    static const ULONG f[] = { 0, 1, 2, 0x20, 0xffffffff };
    unsigned i;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(SysFreeString));
    for (i = 0; i < 5; i++) {
        MockDisp m; BSTR out = NULL; HRESULT hr;
        md_init(&m); V_VT(&m.value) = VT_I4; V_I4(&m.value) = 9;
        hr = g5_vbfd(&m.iface, 0x409, f[i], &out);
        t_check(hr == S_OK && out && !lstrcmpW(out, L"9"), __FILE__, __LINE__, "flags", "flags 0x%lx hr 0x%lx", (unsigned long)f[i], (unsigned long)hr);
        U(SysFreeString)(out);
    }
}

TEST(gap5_bstrfromdisp_matches_variantchangetypeex)
{
    MockDisp m; BSTR a = NULL; VARIANT src, dst; HRESULT h1, h2;
    REQUIRE(U(VarBstrFromDisp)); REQUIRE(U(VariantChangeTypeEx)); REQUIRE(U(SysFreeString)); REQUIRE(U(VariantClear));
    md_init(&m); V_VT(&m.value) = VT_R8; V_R8(&m.value) = 3.5;
    h1 = g5_vbfd(&m.iface, 0x409, 0, &a);
    VariantInit(&src); VariantInit(&dst);
    V_VT(&src) = VT_DISPATCH; V_DISPATCH(&src) = &m.iface;
    h2 = U(VariantChangeTypeEx)(&dst, &src, 0x409, 0, VT_BSTR);
    EXPECT_EQ(h1, h2);
    if (h1 == S_OK && h2 == S_OK) { CHECK(!lstrcmpW(a, V_BSTR(&dst))); CHECK(!lstrcmpW(a, L"3.5")); }
    U(SysFreeString)(a); U(VariantClear)(&dst);
    CHECK(m.ref == 1);
}
