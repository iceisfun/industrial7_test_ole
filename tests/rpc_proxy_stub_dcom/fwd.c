/* NdrProxyForwardingFunction3..32 (delegating proxies forward a call to the base interface proxy) and the
 * IUnknown_*_Proxy helpers, driven with hand-laid-out proxy objects.  Layout (Win10 combase, x64):
 *   interface pointer P -> { vtbl; ...; IUnknown *pUnkOuter at P+0x10; channel at P+0x18; base interface at P+0x20 }
 *   NdrProxyForwardingFunctionN(P, a, b, c, ...) tail-calls  base->vtbl[N](base, a, b, c, ...)  */
#include "psd.h"

typedef struct Rec { int n; void *this_; ULONG_PTR a, b, c, d, e; } Rec;
static Rec g_rec;
static HRESULT WINAPI fw_3(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 3; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 3); }
static HRESULT WINAPI fw_4(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 4; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 4); }
static HRESULT WINAPI fw_5(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 5; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 5); }
static HRESULT WINAPI fw_6(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 6; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 6); }
static HRESULT WINAPI fw_7(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 7; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 7); }
static HRESULT WINAPI fw_8(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 8; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 8); }
static HRESULT WINAPI fw_9(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 9; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 9); }
static HRESULT WINAPI fw_10(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 10; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 10); }
static HRESULT WINAPI fw_11(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 11; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 11); }
static HRESULT WINAPI fw_12(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 12; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 12); }
static HRESULT WINAPI fw_13(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 13; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 13); }
static HRESULT WINAPI fw_14(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 14; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 14); }
static HRESULT WINAPI fw_15(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 15; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 15); }
static HRESULT WINAPI fw_16(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 16; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 16); }
static HRESULT WINAPI fw_17(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 17; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 17); }
static HRESULT WINAPI fw_18(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 18; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 18); }
static HRESULT WINAPI fw_19(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 19; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 19); }
static HRESULT WINAPI fw_20(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 20; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 20); }
static HRESULT WINAPI fw_21(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 21; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 21); }
static HRESULT WINAPI fw_22(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 22; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 22); }
static HRESULT WINAPI fw_23(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 23; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 23); }
static HRESULT WINAPI fw_24(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 24; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 24); }
static HRESULT WINAPI fw_25(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 25; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 25); }
static HRESULT WINAPI fw_26(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 26; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 26); }
static HRESULT WINAPI fw_27(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 27; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 27); }
static HRESULT WINAPI fw_28(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 28; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 28); }
static HRESULT WINAPI fw_29(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 29; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 29); }
static HRESULT WINAPI fw_30(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 30; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 30); }
static HRESULT WINAPI fw_31(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 31; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 31); }
static HRESULT WINAPI fw_32(void *t, ULONG_PTR a, ULONG_PTR b, ULONG_PTR c, ULONG_PTR d, ULONG_PTR e) { g_rec.n = 32; g_rec.this_ = t; g_rec.a = a; g_rec.b = b; g_rec.c = c; g_rec.d = d; g_rec.e = e; return (HRESULT)(0x1000 + 32); }
static void *fw_vtbl[33] = { 0, 0, 0,
    (void *)fw_3,
    (void *)fw_4,
    (void *)fw_5,
    (void *)fw_6,
    (void *)fw_7,
    (void *)fw_8,
    (void *)fw_9,
    (void *)fw_10,
    (void *)fw_11,
    (void *)fw_12,
    (void *)fw_13,
    (void *)fw_14,
    (void *)fw_15,
    (void *)fw_16,
    (void *)fw_17,
    (void *)fw_18,
    (void *)fw_19,
    (void *)fw_20,
    (void *)fw_21,
    (void *)fw_22,
    (void *)fw_23,
    (void *)fw_24,
    (void *)fw_25,
    (void *)fw_26,
    (void *)fw_27,
    (void *)fw_28,
    (void *)fw_29,
    (void *)fw_30,
    (void *)fw_31,
    (void *)fw_32
};

typedef struct FakeProxy { void *pad0; void **vtbl; void *pad1; void *pad2; void **base; } FakeProxy;   /* iface = &vtbl */
typedef HRESULT (WINAPI *fwd_fn)(void *, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

TEST(psdw_forwarding_functions_dispatch_to_base) {
    HMODULE c = t_module("combase.dll");
    if (t_is_wine()) { t_skip("wine's delegating proxies use a different layout"); return; }
    void *base_obj[1] = { fw_vtbl };                    /* base interface: a pointer to its vtable */
    FakeProxy fp; memset(&fp, 0, sizeof fp);
    fp.base = (void **)base_obj;
    /* iface pointer = &fp.vtbl ; base interface pointer is stored 0x20 bytes after the interface pointer: use a raw frame */
    void *frame[8] = { 0 };
    frame[0] = NULL;                                     /* vtbl slot of the delegating proxy (unused) */
    frame[4] = base_obj;                                 /* iface + 0x20 */
    int bad = 0;
    for (int n = 3; n <= 32; n++) {
        char nm[48]; wsprintfA(nm, "NdrProxyForwardingFunction%d", n);
        fwd_fn f = (fwd_fn)GetProcAddress(c, nm);
        if (!f) { bad++; t_check(0, __FILE__, __LINE__, "export", "combase lacks %s", nm); continue; }
        memset(&g_rec, 0, sizeof g_rec);
        HRESULT hr = f(frame, 0x11, 0x22, 0x33, 0x44, 0x55);
        if (hr != (HRESULT)(0x1000 + n) || g_rec.n != n || g_rec.this_ != (void *)base_obj || g_rec.a != 0x11 || g_rec.b != 0x22 || g_rec.c != 0x33 || g_rec.d != 0x44 || g_rec.e != 0x55) {
            bad++;
            t_check(0, __FILE__, __LINE__, "forward", "NdrProxyForwardingFunction%d: hr=0x%lx reached slot %d this=%p args %llx %llx %llx %llx %llx", n, hr, g_rec.n, g_rec.this_,
                    (unsigned long long)g_rec.a, (unsigned long long)g_rec.b, (unsigned long long)g_rec.c, (unsigned long long)g_rec.d, (unsigned long long)g_rec.e);
        }
    }
    EQ(bad, 0);
}

TEST(psdw_forwarding_function_ole32_alias) {
    HMODULE o = t_module("ole32.dll"), c = t_module("combase.dll");
    if (t_is_wine()) { t_skip("wine's ole32 has no such exports"); return; }
    void *base_obj[1] = { fw_vtbl };
    void *frame[8] = { 0 };
    frame[4] = base_obj;
    fwd_fn f = (fwd_fn)GetProcAddress(o, "NdrProxyForwardingFunction7");
    CHECK(f != NULL);
    if (f) {
        memset(&g_rec, 0, sizeof g_rec);
        EQHR(f(frame, 1, 2, 3, 4, 5), 0x1007);
        EQ(g_rec.n, 7);
    }
    CHECK(GetProcAddress(o, "NdrProxyForwardingFunction7") == GetProcAddress(c, "NdrProxyForwardingFunction7"));
}

/* ---- IUnknown_*_Proxy: delegate to the controlling unknown stored in the proxy object ------------------- */
typedef struct UProxy { void *pad; void *vtbl; void *pad2; IUnknown *outer; } UProxy;   /* iface = &vtbl, outer at iface+0x10 */

TEST(psdw_iunknown_proxy_helpers_delegate_to_outer) {
    if (!UIN("rpcrt4.dll", IUnknown_AddRef_Proxy)) { t_skip("no IUnknown_AddRef_Proxy"); return; }
    if (t_is_wine()) { t_skip("wine's proxy object layout differs"); return; }
    PsdMock *outer = psd_mock_new();
    outer->ref = 7;
    UProxy up; memset(&up, 0, sizeof up);
    up.outer = &outer->iface;
    IUnknown *iface = (IUnknown *)&up.vtbl;
    EQ(UIN("rpcrt4.dll", IUnknown_AddRef_Proxy)(iface), 8);
    EQ(UIN("rpcrt4.dll", IUnknown_Release_Proxy)(iface), 7);
    EQ(outer->n_addref, 1); EQ(outer->n_release, 1);
    void *pv = (void *)1;
    outer->qi_self = 0; outer->qi_out = (void *)0x1234; outer->qi_hr = S_OK;
    EQHR(UIN("rpcrt4.dll", IUnknown_QueryInterface_Proxy)(iface, &psd_IID_One, &pv), S_OK);
    EQ(pv, 0x1234);
    CHECK(IsEqualGUID(&outer->last_iid, &psd_IID_One));
    outer->qi_hr = E_NOINTERFACE; outer->qi_out = NULL;
    EQHR(UIN("rpcrt4.dll", IUnknown_QueryInterface_Proxy)(iface, &psd_IID_Two, &pv), E_NOINTERFACE);
    EQ(outer->n_qi, 2);
    psd_mock_free(outer);
}
