/* winrt_roapi: CreateErrorInfo / SetErrorInfo / GetErrorInfo (thread-local IErrorInfo, shared by ole32/oleaut32/combase). */
#include "ro_common.h"

/* vtable slots */
#define EI_GetGUID 3
#define EI_GetSource 4
#define EI_GetDescription 5
#define EI_GetHelpFile 6
#define EI_GetHelpContext 7
#define CE_SetGUID 3
#define CE_SetSource 4
#define CE_SetDescription 5
#define CE_SetHelpFile 6
#define CE_SetHelpContext 7
typedef HRESULT (WINAPI *p_getguid)(void *, GUID *);
typedef HRESULT (WINAPI *p_getbstr)(void *, BSTR *);
typedef HRESULT (WINAPI *p_getctx)(void *, DWORD *);
typedef HRESULT (WINAPI *p_setguid)(void *, REFGUID);
typedef HRESULT (WINAPI *p_setstr)(void *, LPOLESTR);
typedef HRESULT (WINAPI *p_setctx)(void *, DWORD);

#define NEED_EI() do { REQUIRE(U(CreateErrorInfo)); REQUIRE(U(SetErrorInfo)); REQUIRE(U(GetErrorInfo)); \
    REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString)); } while (0)
static void ei_clear(void) { U(SetErrorInfo)(0, NULL); }
static void *mk(void) { ICreateErrorInfo *c = NULL; U(CreateErrorInfo)(&c); return c; }

/* ============ CreateErrorInfo ============ */
TEST(winrt_roapi_errinfo_create_null_out) {
    REQUIRE(U(CreateErrorInfo));
    RO_EQ(U(CreateErrorInfo)(NULL), RO_E_INVALIDARG);
}
TEST(winrt_roapi_errinfo_create_initial_state) {
    NEED_EI();
    ICreateErrorInfo *c = (ICreateErrorInfo *)0xdeadbeef;
    RO_EQ(U(CreateErrorInfo)(&c), S_OK);
    CHECK(c && c != (void *)0xdeadbeef);
    if (!c || c == (void *)0xdeadbeef) return;
    RO_EQ(ro_refcount(c), 1);
    IErrorInfo *e = NULL;
    RO_EQ(ro_qi(c, &RIID_IErrorInfo, (void **)&e), S_OK);
    GUID g; memset(&g, 0xAA, sizeof g);
    RO_EQ(((p_getguid)SLOT(e, EI_GetGUID))(e, &g), S_OK);
    CHECK(!memcmp(&g, &RIID_NULLG, sizeof g));                    /* RIID_NULLG by default (decomp) */
    DWORD ctx = 0x1234;
    RO_EQ(((p_getctx)SLOT(e, EI_GetHelpContext))(e, &ctx), S_OK);
    RO_EQ(ctx, 0);
    BSTR s = (BSTR)1;
    HRESULT hr = ((p_getbstr)SLOT(e, EI_GetSource))(e, &s);
    t_note("unset GetSource hr=%08lx bstr=%p", (unsigned long)hr, s);
    s = (BSTR)1; hr = ((p_getbstr)SLOT(e, EI_GetDescription))(e, &s);
    t_note("unset GetDescription hr=%08lx bstr=%p", (unsigned long)hr, s);
    s = (BSTR)1; hr = ((p_getbstr)SLOT(e, EI_GetHelpFile))(e, &s);
    t_note("unset GetHelpFile hr=%08lx bstr=%p", (unsigned long)hr, s);
    RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_create_distinct_objects) {
    NEED_EI();
    ICreateErrorInfo *a = mk(), *b = mk();
    CHECK(a && b && a != b);
    RO_RELEASE(a); RO_RELEASE(b);
}
TEST(winrt_roapi_errinfo_qi_matrix) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    static const struct { const GUID *iid; const char *n; int ok; } t[] = {
        { &RIID_IUnknown, "IUnknown", 1 }, { &RIID_IErrorInfo, "IErrorInfo", 1 }, { &RIID_ICreateErrorInfo, "ICreateErrorInfo", 1 },
        { &RIID_IMarshal, "IMarshal", 1 }, { &RIID_ISupportErrorInfo, "ISupportErrorInfo", 0 },
        { &RIID_IRestrictedErrorInfo, "IRestrictedErrorInfo", 0 }, { &RIID_IInspectable, "IInspectable", 0 },
        { &RIID_IAgileObject, "IAgileObject", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *o = (void *)0xdeadbeef;
        HRESULT hr = ro_qi(c, t[i].iid, &o);
        if (t[i].ok) { CHECKF(hr == S_OK && o, "%s hr=%08lx", t[i].n, (unsigned long)hr); if (hr == S_OK) RO_RELEASE(*(IUnknown **)&o); }
        else { CHECKF(hr == RO_E_NOINTERFACE, "%s hr=%08lx", t[i].n, (unsigned long)hr); CHECKF(o == NULL, "%s out=%p", t[i].n, o); }
    }
    RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_identity_across_interfaces) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e);
    IUnknown *uc = NULL, *ue = NULL, *um = NULL; IUnknown *m = NULL;
    ro_qi(c, &RIID_IUnknown, (void **)&uc);
    ro_qi(e, &RIID_IUnknown, (void **)&ue);
    ro_qi(c, &RIID_IMarshal, (void **)&m);
    if (m) ro_qi(m, &RIID_IUnknown, (void **)&um);
    CHECK(uc && uc == ue && uc == um);
    CHECK((void *)e != (void *)c);                 /* distinct interface pointers on one object */
    RO_RELEASE(uc); RO_RELEASE(ue); RO_RELEASE(um); RO_RELEASE(m); RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_refcount_shared_across_interfaces) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e);
    RO_EQ(ro_refcount(c), 2); RO_EQ(ro_refcount(e), 2);
    RO_UNK(e)->lpVtbl->Release(RO_UNK(e));
    RO_EQ(ro_refcount(c), 1);
    RO_RELEASE(c);
}

/* ============ property roundtrip ============ */
TEST(winrt_roapi_errinfo_set_get_roundtrip) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e); REQUIRE(e);
    static const GUID g = { 0x12345678, 0x9abc, 0xdef0, { 1,2,3,4,5,6,7,8 } };
    RO_EQ(((p_setguid)SLOT(c, CE_SetGUID))(c, &g), S_OK);
    RO_EQ(((p_setstr)SLOT(c, CE_SetSource))(c, L"olet.source"), S_OK);
    RO_EQ(((p_setstr)SLOT(c, CE_SetDescription))(c, L"olet description"), S_OK);
    RO_EQ(((p_setstr)SLOT(c, CE_SetHelpFile))(c, L"C:\\olet\\help.chm"), S_OK);
    RO_EQ(((p_setctx)SLOT(c, CE_SetHelpContext))(c, 0xCAFE0001), S_OK);
    GUID g2 = {0}; BSTR s = NULL; DWORD ctx = 0;
    RO_EQ(((p_getguid)SLOT(e, EI_GetGUID))(e, &g2), S_OK); CHECK(!memcmp(&g, &g2, sizeof g));
    RO_EQ(((p_getbstr)SLOT(e, EI_GetSource))(e, &s), S_OK); CHECK(s && !lstrcmpW(s, L"olet.source")); if (s) U(SysFreeString)(s); s = NULL;
    RO_EQ(((p_getbstr)SLOT(e, EI_GetDescription))(e, &s), S_OK); CHECK(s && !lstrcmpW(s, L"olet description")); if (s) U(SysFreeString)(s); s = NULL;
    RO_EQ(((p_getbstr)SLOT(e, EI_GetHelpFile))(e, &s), S_OK); CHECK(s && !lstrcmpW(s, L"C:\\olet\\help.chm")); if (s) U(SysFreeString)(s); s = NULL;
    RO_EQ(((p_getctx)SLOT(e, EI_GetHelpContext))(e, &ctx), S_OK); RO_EQ(ctx, 0xCAFE0001);
    RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_set_copies_string) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e); REQUIRE(e);
    WCHAR buf[16]; lstrcpyW(buf, L"original");
    RO_EQ(((p_setstr)SLOT(c, CE_SetDescription))(c, buf), S_OK);
    lstrcpyW(buf, L"CHANGED!");
    BSTR s = NULL, s2 = NULL;
    RO_EQ(((p_getbstr)SLOT(e, EI_GetDescription))(e, &s), S_OK);
    CHECK(s && !lstrcmpW(s, L"original"));
    RO_EQ(((p_getbstr)SLOT(e, EI_GetDescription))(e, &s2), S_OK);
    CHECK(s2 && !lstrcmpW(s2, L"original"));
    t_note("GetDescription returns a fresh BSTR each call: %d", s != s2);
    if (s) U(SysFreeString)(s); if (s2) U(SysFreeString)(s2);
    RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_set_overwrites) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e); REQUIRE(e);
    ((p_setstr)SLOT(c, CE_SetSource))(c, L"first");
    ((p_setstr)SLOT(c, CE_SetSource))(c, L"second");
    ((p_setctx)SLOT(c, CE_SetHelpContext))(c, 1);
    ((p_setctx)SLOT(c, CE_SetHelpContext))(c, 2);
    BSTR s = NULL; DWORD ctx = 0;
    ((p_getbstr)SLOT(e, EI_GetSource))(e, &s); CHECK(s && !lstrcmpW(s, L"second")); if (s) U(SysFreeString)(s);
    ((p_getctx)SLOT(e, EI_GetHelpContext))(e, &ctx); RO_EQ(ctx, 2);
    RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_empty_and_embedded_strings) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e); REQUIRE(e);
    RO_EQ(((p_setstr)SLOT(c, CE_SetDescription))(c, L""), S_OK);
    BSTR s = NULL;
    RO_EQ(((p_getbstr)SLOT(e, EI_GetDescription))(e, &s), S_OK);
    t_note("empty description returns bstr=%p len=%u", s, s ? U(SysStringLen)(s) : 0);
    if (s) { CHECK(s[0] == 0); U(SysFreeString)(s); }
    static WCHAR big[4096]; for (int i = 0; i < 4095; i++) big[i] = L'A' + i % 26;
    RO_EQ(((p_setstr)SLOT(c, CE_SetDescription))(c, big), S_OK);
    s = NULL; RO_EQ(((p_getbstr)SLOT(e, EI_GetDescription))(e, &s), S_OK);
    CHECK(s && U(SysStringLen)(s) == 4095 && !memcmp(s, big, 4095 * 2)); if (s) U(SysFreeString)(s);
    RO_RELEASE(e); RO_RELEASE(c);
}
TEST(winrt_roapi_errinfo_set_null_string_record) {
    NEED_EI();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *e = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&e); REQUIRE(e);
    ((p_setstr)SLOT(c, CE_SetSource))(c, L"x");
    HRESULT hr = ((p_setstr)SLOT(c, CE_SetSource))(c, NULL);
    t_note("SetSource(NULL) = %08lx", (unsigned long)hr);
    BSTR s = (BSTR)1; hr = ((p_getbstr)SLOT(e, EI_GetSource))(e, &s);
    t_note("GetSource after SetSource(NULL) = %08lx %ls", (unsigned long)hr, (s > (BSTR)1) ? s : L"(null)");
    if (s > (BSTR)1) U(SysFreeString)(s);
    RO_RELEASE(e); RO_RELEASE(c);
}

/* ============ SetErrorInfo / GetErrorInfo ============ */
RO_TEST(winrt_roapi_errinfo_get_none) {
    NEED_EI(); ei_clear();
    IErrorInfo *e = (IErrorInfo *)0xdeadbeef;
    RO_EQ(U(GetErrorInfo)(0, &e), S_FALSE);
    CHECK(e == NULL);
}
RO_TEST(winrt_roapi_errinfo_get_invalid_args) {
    NEED_EI(); ei_clear();
    RO_EQ(U(GetErrorInfo)(0, NULL), RO_E_INVALIDARG);
    RO_EQ(U(GetErrorInfo)(1, NULL), RO_E_INVALIDARG);
    static const ULONG bad[] = { 1, 2, 0x80000000, 0xffffffff };
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    U(SetErrorInfo)(0, ci);
    for (unsigned i = 0; i < 4; i++) {
        IErrorInfo *e = (IErrorInfo *)0xdeadbeef;
        RO_EQ(U(GetErrorInfo)(bad[i], &e), RO_E_INVALIDARG);
        CHECK(e == NULL);
    }
    IErrorInfo *e = NULL;                           /* the stored object survives rejected calls */
    RO_EQ(U(GetErrorInfo)(0, &e), S_OK); CHECK(e != NULL);
    RO_RELEASE(e); RO_RELEASE(ci); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_set_reserved_nonzero) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    RO_EQ(U(SetErrorInfo)(1, ci), RO_E_INVALIDARG);
    RO_EQ(U(SetErrorInfo)(0xffffffff, NULL), RO_E_INVALIDARG);
    IErrorInfo *e = (IErrorInfo *)1;
    RO_EQ(U(GetErrorInfo)(0, &e), S_FALSE); CHECK(e == NULL);   /* nothing was stored */
    RO_EQ(ro_refcount(c), 2);                                    /* us + ci, no leaked ref */
    RO_RELEASE(ci); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_set_get_roundtrip_and_consume) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    ((p_setstr)SLOT(c, CE_SetDescription))(c, L"thread error");
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    RO_EQ(U(SetErrorInfo)(0, ci), S_OK);
    IErrorInfo *g = NULL;
    RO_EQ(U(GetErrorInfo)(0, &g), S_OK);
    CHECK(g == ci);
    BSTR s = NULL;
    if (g) { ((p_getbstr)SLOT(g, EI_GetDescription))(g, &s); CHECK(s && !lstrcmpW(s, L"thread error")); if (s) U(SysFreeString)(s); }
    IErrorInfo *again = (IErrorInfo *)1;
    RO_EQ(U(GetErrorInfo)(0, &again), S_FALSE); CHECK(again == NULL);   /* GetErrorInfo clears */
    RO_RELEASE(g); RO_RELEASE(ci); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_refcounts) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    RO_EQ(ro_refcount(c), 2);
    U(SetErrorInfo)(0, ci);
    RO_EQ(ro_refcount(c), 3);                           /* TLS holds one reference */
    IErrorInfo *g = NULL; U(GetErrorInfo)(0, &g);
    RO_EQ(ro_refcount(c), 3);                           /* ownership moves from TLS to the caller */
    RO_RELEASE(g);
    RO_EQ(ro_refcount(c), 2);
    U(SetErrorInfo)(0, ci);
    RO_EQ(ro_refcount(c), 3);
    U(SetErrorInfo)(0, NULL);                           /* clearing releases */
    RO_EQ(ro_refcount(c), 2);
    RO_RELEASE(ci); RO_EQ(ro_refcount(c), 1); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_set_replaces_previous) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *a = mk(), *b = mk(); REQUIRE(a); REQUIRE(b);
    IErrorInfo *ia = NULL, *ib = NULL; ro_qi(a, &RIID_IErrorInfo, (void **)&ia); ro_qi(b, &RIID_IErrorInfo, (void **)&ib);
    U(SetErrorInfo)(0, ia);
    U(SetErrorInfo)(0, ib);
    RO_EQ(ro_refcount(a), 2);                           /* a released by TLS */
    RO_EQ(ro_refcount(b), 3);
    IErrorInfo *g = NULL; U(GetErrorInfo)(0, &g);
    CHECK(g == ib);
    RO_RELEASE(g); RO_RELEASE(ia); RO_RELEASE(ib); RO_RELEASE(a); RO_RELEASE(b);
}
RO_TEST(winrt_roapi_errinfo_set_same_object_twice) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    U(SetErrorInfo)(0, ci); U(SetErrorInfo)(0, ci);
    RO_EQ(ro_refcount(c), 3);
    IErrorInfo *g = NULL; RO_EQ(U(GetErrorInfo)(0, &g), S_OK); CHECK(g == ci);
    RO_RELEASE(g); RO_RELEASE(ci); RO_RELEASE(c);
}
static IErrorInfo *g_xt_obj; static HRESULT g_xt_hr;
static DWORD WINAPI xt_get(void *p) { IErrorInfo *e = (IErrorInfo *)1; g_xt_hr = U(GetErrorInfo)(0, &e); g_xt_obj = e; return 0; }
static DWORD WINAPI xt_set_exit(void *p) { U(SetErrorInfo)(0, (IErrorInfo *)p); return 0; }
RO_TEST(winrt_roapi_errinfo_is_thread_local) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    U(SetErrorInfo)(0, ci);
    CHECK(ro_run_thread(xt_get, NULL, 5000) == 0);
    RO_EQ(g_xt_hr, S_FALSE); CHECK(g_xt_obj == NULL);
    IErrorInfo *g = NULL; RO_EQ(U(GetErrorInfo)(0, &g), S_OK); CHECK(g == ci);
    RO_RELEASE(g); RO_RELEASE(ci); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_thread_exit_drops_reference) {
    NEED_EI(); ei_clear();
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    ULONG base = ro_refcount(c);
    CHECK(ro_run_thread(xt_set_exit, ci, 5000) == 0);
    ULONG after = ro_refcount(c);
    t_note("refcount before=%lu after thread that set error exits=%lu", base, after);
    CHECK(after == base || after == base + 1);
    RO_RELEASE(ci); RO_RELEASE(c);
}
/* the three DLLs share one slot */
RO_TEST(winrt_roapi_errinfo_shared_between_modules) {
    NEED_EI();
    typedef HRESULT (WINAPI *set_t)(ULONG, IErrorInfo *); typedef HRESULT (WINAPI *get_t)(ULONG, IErrorInfo **);
    static const char *mods[] = { "ole32.dll", "oleaut32.dll", "combase.dll" };
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
        set_t s = (set_t)t_proc_in(mods[i], "SetErrorInfo"); get_t g = (get_t)t_proc_in(mods[j], "GetErrorInfo");
        if (!s || !g) { t_note("%s!SetErrorInfo or %s!GetErrorInfo missing", mods[i], mods[j]); continue; }
        ei_clear();
        RO_EQ(s(0, ci), S_OK);
        IErrorInfo *o = NULL;
        HRESULT hr = g(0, &o);
        CHECKF(hr == S_OK && o == ci, "%s set / %s get: hr=%08lx o=%p", mods[i], mods[j], (unsigned long)hr, o);
        RO_RELEASE(o);
    }
    RO_RELEASE(ci); RO_RELEASE(c);
}
RO_TEST(winrt_roapi_errinfo_co_aliases) {
    NEED_EI();
    FARPROC cs = t_proc_in("combase.dll", "CoSetErrorInfo"), cg = t_proc_in("combase.dll", "CoGetErrorInfo"), cc = t_proc_in("combase.dll", "CoCreateErrorInfo");
    REQUIRE(cs); REQUIRE(cg); REQUIRE(cc);
    CHECK(cs == t_proc_in("combase.dll", "SetErrorInfo"));
    CHECK(cg == t_proc_in("combase.dll", "GetErrorInfo"));
    CHECK(cc == t_proc_in("combase.dll", "CreateErrorInfo"));
}
RO_TEST(winrt_roapi_errinfo_works_without_com_init) {
    NEED_EI(); ei_clear();
    CHECK(ro_apt(NULL, NULL) == RO_E_NOTINIT);
    ICreateErrorInfo *c = mk(); CHECK(c != NULL); if (!c) return;
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    RO_EQ(U(SetErrorInfo)(0, ci), S_OK);
    IErrorInfo *g = NULL; RO_EQ(U(GetErrorInfo)(0, &g), S_OK); CHECK(g == ci);
    CHECK(ro_apt(NULL, NULL) == RO_E_NOTINIT);        /* the calls do not initialize COM */
    RO_RELEASE(g); RO_RELEASE(ci); RO_RELEASE(c);
}

/* ============ custom IErrorInfo implementation ============ */
typedef struct { IErrorInfo i; LONG ref, nqi, nrel, nadd; int qi_ok; } MyEI;
static HRESULT WINAPI my_qi(IErrorInfo *s, REFIID r, void **o) {
    MyEI *m = (MyEI *)s; InterlockedIncrement(&m->nqi);
    if (!memcmp(r, &RIID_IUnknown, 16) || (m->qi_ok && !memcmp(r, &RIID_IErrorInfo, 16))) { *o = s; InterlockedIncrement(&m->ref); return S_OK; }
    *o = NULL; return RO_E_NOINTERFACE;
}
static ULONG WINAPI my_add(IErrorInfo *s) { MyEI *m = (MyEI *)s; InterlockedIncrement(&m->nadd); return InterlockedIncrement(&m->ref); }
static ULONG WINAPI my_rel(IErrorInfo *s) { MyEI *m = (MyEI *)s; InterlockedIncrement(&m->nrel); return InterlockedDecrement(&m->ref); }
static HRESULT WINAPI my_guid(IErrorInfo *s, GUID *g) { *g = RIID_NULLG; return S_OK; }
static HRESULT WINAPI my_bstr(IErrorInfo *s, BSTR *b) { *b = NULL; return S_OK; }
static HRESULT WINAPI my_ctx(IErrorInfo *s, DWORD *d) { *d = 0; return S_OK; }
static IErrorInfoVtbl my_vtbl = { my_qi, my_add, my_rel, my_guid, my_bstr, my_bstr, my_bstr, my_ctx };
static void my_init(MyEI *m, int qi_ok) { memset(m, 0, sizeof *m); m->i.lpVtbl = &my_vtbl; m->ref = 1; m->qi_ok = qi_ok; }

RO_TEST(winrt_roapi_errinfo_custom_object_addref_release) {
    NEED_EI(); ei_clear();
    MyEI m; my_init(&m, 1);
    RO_EQ(U(SetErrorInfo)(0, &m.i), S_OK);
    RO_EQ(m.nadd, 1); RO_EQ(m.ref, 2);                  /* SetErrorInfo AddRefs through the vtable */
    IErrorInfo *g = NULL;
    RO_EQ(U(GetErrorInfo)(0, &g), S_OK);
    CHECK(g == &m.i);
    RO_EQ(m.nqi, 1);                                    /* IErrorInfo requested via QueryInterface */
    RO_EQ(m.ref, 2);                                    /* TLS ref released, QI ref handed out */
    RO_UNK(g)->lpVtbl->Release(RO_UNK(g));
    RO_EQ(m.ref, 1);
    U(SetErrorInfo)(0, &m.i); U(SetErrorInfo)(0, NULL);
    RO_EQ(m.ref, 1);
}
RO_TEST(winrt_roapi_errinfo_custom_object_qi_failure_keeps_error) {
    NEED_EI(); ei_clear();
    MyEI m; my_init(&m, 0);
    U(SetErrorInfo)(0, &m.i);
    IErrorInfo *g = (IErrorInfo *)1;
    RO_EQ(U(GetErrorInfo)(0, &g), RO_E_NOINTERFACE);     /* decomp: QI failure is propagated, object stays in TLS */
    CHECK(g == NULL);
    RO_EQ(m.ref, 2);
    m.qi_ok = 1;
    RO_EQ(U(GetErrorInfo)(0, &g), S_OK);
    CHECK(g == &m.i);
    if (g) RO_UNK(g)->lpVtbl->Release(RO_UNK(g));
    U(SetErrorInfo)(0, NULL);
    RO_EQ(m.ref, 1);
}

/* ============ marshaling the error object ============ */
static IStream *g_stm; static IErrorInfo *g_unm; static HRESULT g_unm_hr;
static DWORD WINAPI unm_thread(void *p) {
    U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    g_unm = NULL; g_unm_hr = U(CoUnmarshalInterface)(g_stm, &RIID_IErrorInfo, (void **)&g_unm);
    BSTR s = NULL;
    if (g_unm) { ((p_getbstr)SLOT(g_unm, EI_GetDescription))(g_unm, &s); if (!s || lstrcmpW(s, L"marshaled")) g_unm_hr = E_FAIL; if (s) U(SysFreeString)(s); }
    U(CoUninitialize)();
    return 0;
}
RO_TEST(winrt_roapi_errinfo_marshal_by_value) {
    NEED_EI(); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(CoInitializeEx));
    RO_EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    ICreateErrorInfo *c = mk(); REQUIRE(c);
    ((p_setstr)SLOT(c, CE_SetDescription))(c, L"marshaled");
    IErrorInfo *ci = NULL; ro_qi(c, &RIID_IErrorInfo, (void **)&ci);
    IStream *st = NULL; RO_EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &st), S_OK); REQUIRE(st);
    HRESULT hr = U(CoMarshalInterface)(st, &RIID_IErrorInfo, (IUnknown *)ci, MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    RO_EQ(hr, S_OK);
    if (hr == S_OK) {
        LARGE_INTEGER z = {0}; st->lpVtbl->Seek(st, z, STREAM_SEEK_SET, NULL);
        g_stm = st;
        CHECK(ro_run_thread(unm_thread, NULL, 10000) == 0);
        RO_EQ(g_unm_hr, S_OK);                         /* the description survived marshaling into another apartment */
        CHECK(g_unm != NULL);
    t_note("unmarshaled error object is the original object: %d", (void *)g_unm == (void *)ci);
        RO_RELEASE(g_unm);
    }
    RO_RELEASE(st); RO_RELEASE(ci); RO_RELEASE(c);
    U(CoUninitialize)();
}
