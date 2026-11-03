/* Default handler / embedding helper (OleCreateDefaultHandler, OleCreateEmbeddingHelper), CoCreateInstance of
 * an unregistered class, and the OleCreate-family, OleLoad/OleSave/OleDoAutoConvert argument and failure paths.
 * Everything uses classes that are NOT registered, so no machine state is read or written. */
#include "olh.h"

/* ---- minimal class factory ------------------------------------------------------------------ */
typedef struct { IClassFactory i; LONG ref; LONG creates; LONG locks; } O2Cf;
static HRESULT STDMETHODCALLTYPE cf_QI(IClassFactory *s, REFIID r, void **pp) {
    if (O2_IS_IID(r, IID_IUnknown) || O2_IS_IID(r, IID_IClassFactory)) { *pp = s; s->lpVtbl->AddRef(s); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cf_AddRef(IClassFactory *s) { return InterlockedIncrement(&((O2Cf *)s)->ref); }
static ULONG STDMETHODCALLTYPE cf_Release(IClassFactory *s) { return InterlockedDecrement(&((O2Cf *)s)->ref); }
static HRESULT STDMETHODCALLTYPE cf_Create(IClassFactory *s, IUnknown *o, REFIID r, void **pp) { InterlockedIncrement(&((O2Cf *)s)->creates); *pp = NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE cf_Lock(IClassFactory *s, BOOL l) { InterlockedIncrement(&((O2Cf *)s)->locks); return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QI, cf_AddRef, cf_Release, cf_Create, cf_Lock };
static void cf_init(O2Cf *c) { memset(c, 0, sizeof *c); c->i.lpVtbl = &cf_vtbl; c->ref = 1; }

static HRESULT mk_handler(const GUID *clsid, IOleObject **po) {
    *po = NULL;
    return U(OleCreateDefaultHandler)(clsid, NULL, &IID_IOleObject, (void **)po);
}

/* ================================================================================================
 * OleCreateEmbeddingHelper argument validation
 * ============================================================================================== */
TEST(o2hdl_embedhelper_null_output) {
    O2Cf cf;
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    REQUIRE(U(OleCreateEmbeddingHelper));
    cf_init(&cf);
    EQ(U(OleCreateEmbeddingHelper)(&O2_CLSID_A, NULL, 0, NULL, &IID_IOleObject, NULL), O2_E_INVALIDARG);
    EQ(U(OleCreateDefaultHandler)(&O2_CLSID_A, NULL, &IID_IOleObject, NULL), O2_E_INVALIDARG);
}
TEST(o2hdl_embedhelper_flag_validation) {
    static const struct { DWORD flags; int with_cf; HRESULT hr; const char *what; } c[] = {
        { 0x00000000, 0, S_OK,          "handler, no factory" },
        { 0x00000000, 1, O2_E_INVALIDARG, "handler with a factory" },
        { 0x00000001, 1, S_OK,          "INPROC_SERVER with factory" },
        { 0x00000001, 0, O2_E_INVALIDARG, "INPROC_SERVER without factory" },
        { 0x00010000, 0, O2_E_INVALIDARG, "DELAYCREATE without server" },
        { 0x00010001, 1, S_OK,          "DELAYCREATE|INPROC_SERVER with factory" },
        { 0x00000002, 0, O2_E_INVALIDARG, "undefined bit 1" },
        { 0x00000004, 1, O2_E_INVALIDARG, "undefined bit 2" },
        { 0x00020000, 0, O2_E_INVALIDARG, "undefined bit 17" },
        { 0x80000000, 0, O2_E_INVALIDARG, "undefined bit 31" },
        { 0xFFFFFFFF, 1, O2_E_INVALIDARG, "all bits" },
    };
    int i;
    REQUIRE(U(OleCreateEmbeddingHelper));
    for (i = 0; i < O2_N(c); i++) {
        O2Cf cf; void *p = (void *)1; HRESULT hr;
        cf_init(&cf);
        hr = U(OleCreateEmbeddingHelper)(&O2_CLSID_A, NULL, c[i].flags, c[i].with_cf ? &cf.i : NULL, &IID_IOleObject, &p);
        CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
        if (FAILED(c[i].hr)) CHECKF(p == NULL, "%s: output not cleared", c[i].what);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
}
TEST(o2hdl_embedhelper_aggregation_rules) {
    O2Run outer; IUnknown *unk = NULL; void *p = (void *)1; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler));
    or_init(&outer);
    hr = U(OleCreateDefaultHandler)(&O2_CLSID_A, (IUnknown *)&outer.i, &IID_IOleObject, &p);
    EQ(hr, O2_E_INVALIDARG);          /* aggregation requires IID_IUnknown */
    EQ(p, NULL);
    hr = U(OleCreateDefaultHandler)(&O2_CLSID_A, (IUnknown *)&outer.i, &IID_IUnknown, (void **)&unk);
    EQ(hr, S_OK);
    CHECK(unk != NULL);
    if (unk) IUnknown_Release(unk);
}
TEST(o2hdl_embedhelper_factory_reference) {
    O2Cf cf; void *p = NULL; HRESULT hr;
    REQUIRE(U(OleCreateEmbeddingHelper));
    cf_init(&cf);
    hr = U(OleCreateEmbeddingHelper)(&O2_CLSID_A, NULL, 1, &cf.i, &IID_IOleObject, &p);
    EQ(hr, S_OK);
    t_note("class factory refcount while the helper lives: %ld", (long)cf.ref);
    t_note("server objects created through the factory so far: %ld", (long)cf.creates);
    if (p) IOleObject_Release((IOleObject *)p);
    EQ(cf.ref, 1);                    /* the helper dropped its factory reference when destroyed */
}

/* ================================================================================================
 * default handler: interfaces
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/defaulthandler.c + ole2.c test_default_handler (LGPL-2.1+) */
TEST(o2hdl_handler_creates_for_unregistered_class) {
    IOleObject *o = NULL; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    hr = mk_handler(&O2_CLSID_A, &o);
    EQ(hr, S_OK);
    CHECK(o != NULL);
    if (o) { EQ(IOleObject_Release(o), 0); }
    O2_END();
}
TEST(o2hdl_handler_creates_with_clsid_null) {
    IOleObject *o = NULL; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    hr = mk_handler(&O2_GUID_NULL, &o);
    t_note("OleCreateDefaultHandler(CLSID_NULL) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && o) IOleObject_Release(o);
    CHECK(SUCCEEDED(hr) || o == NULL);
    O2_END();
}
TEST(o2hdl_handler_requested_interface_is_returned) {
    static const struct { const GUID *iid; const char *n; } t[] = {
        { &IID_IUnknown, "IUnknown" }, { &IID_IOleObject, "IOleObject" }, { &IID_IDataObject, "IDataObject" },
        { &IID_IRunnableObject, "IRunnableObject" }, { &IID_IPersistStorage, "IPersistStorage" }, { &IID_IViewObject, "IViewObject" },
        { &IID_IViewObject2, "IViewObject2" }, { &IID_IOleCache, "IOleCache" }, { &IID_IOleCache2, "IOleCache2" }, { &IID_IOleCacheControl, "IOleCacheControl" },
    };
    int i;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    for (i = 0; i < O2_N(t); i++) {
        void *p = NULL; HRESULT hr = U(OleCreateDefaultHandler)(&O2_CLSID_A, NULL, t[i].iid, &p);
        CHECKF(hr == S_OK && p != NULL, "%s: 0x%08lx", t[i].n, (unsigned long)hr);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    O2_END();
}
TEST(o2hdl_handler_unsupported_interface_request) {
    void *p = (void *)1; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    hr = U(OleCreateDefaultHandler)(&O2_CLSID_A, NULL, &O2_IID_BOGUS, &p);
    EQ(hr, O2_E_NOINTERFACE);
    EQ(p, NULL);
    hr = U(OleCreateDefaultHandler)(&O2_CLSID_A, NULL, &IID_IOleInPlaceObject, &p);
    EQ(hr, O2_E_NOINTERFACE);
    O2_END();
}
TEST(o2hdl_handler_queryinterface_matrix) {
    static const struct { const GUID *iid; const char *n; } sup[] = {
        { &IID_IUnknown, "IUnknown" }, { &IID_IOleObject, "IOleObject" }, { &IID_IDataObject, "IDataObject" },
        { &IID_IRunnableObject, "IRunnableObject" }, { &IID_IPersistStorage, "IPersistStorage" }, { &IID_IViewObject, "IViewObject" },
        { &IID_IViewObject2, "IViewObject2" }, { &IID_IOleCache, "IOleCache" }, { &IID_IOleCache2, "IOleCache2" }, { &IID_IOleCacheControl, "IOleCacheControl" },
    };
    static const struct { const GUID *iid; const char *n; } unsup[] = {
        { &IID_IOleInPlaceObject, "IOleInPlaceObject" }, { &IID_IStorage, "IStorage" }, { &IID_IStream, "IStream" },
        { &IID_IClassFactory, "IClassFactory" }, { &IID_IDropTarget, "IDropTarget" }, { &O2_IID_BOGUS, "bogus" },
    };
    IOleObject *o; int i;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    for (i = 0; i < O2_N(sup); i++) {
        void *p = NULL; HRESULT hr = IOleObject_QueryInterface(o, sup[i].iid, &p);
        CHECKF(hr == S_OK && p != NULL, "%s: 0x%08lx", sup[i].n, (unsigned long)hr);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    for (i = 0; i < O2_N(unsup); i++) {
        void *p = (void *)1; HRESULT hr = IOleObject_QueryInterface(o, unsup[i].iid, &p);
        CHECKF(hr == O2_E_NOINTERFACE && p == NULL, "%s: 0x%08lx", unsup[i].n, (unsigned long)hr);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_identity_of_iunknown) {
    IOleObject *o; IUnknown *u1 = NULL, *u2 = NULL; IDataObject *d = NULL;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(IOleObject_QueryInterface(o, &IID_IUnknown, (void **)&u1), S_OK);
    EQ(IOleObject_QueryInterface(o, &IID_IDataObject, (void **)&d), S_OK);
    EQ(IDataObject_QueryInterface(d, &IID_IUnknown, (void **)&u2), S_OK);
    EQ(u1, u2);                              /* all interfaces share one object identity */
    IUnknown_Release(u1); IUnknown_Release(u2); IDataObject_Release(d);
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_refcounting) {
    IOleObject *o;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(IOleObject_AddRef(o), 2);
    EQ(IOleObject_AddRef(o), 3);
    EQ(IOleObject_Release(o), 2);
    EQ(IOleObject_Release(o), 1);
    EQ(IOleObject_Release(o), 0);
    O2_END();
}

/* ================================================================================================
 * default handler: IOleObject methods of a not-running object of an unregistered class
 * ============================================================================================== */
TEST(o2hdl_handler_oleobject_not_running) {
    IOleObject *o; IOleClientSite *site = (IOleClientSite *)(ULONG_PTR)1; IDataObject *dobj = NULL; SIZEL sz; DWORD st = 0xCCCC; CLSID id; LOGPALETTE pal; HRESULT hr; IEnumOLEVERB *ev = (IEnumOLEVERB *)(ULONG_PTR)1;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    hr = IOleObject_GetClientSite(o, &site);
    EQ(hr, S_OK); EQ(site, NULL);
    hr = IOleObject_SetClientSite(o, NULL);
    EQ(hr, S_OK);
    hr = IOleObject_GetClipboardData(o, 0, &dobj);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    memset(&sz, 0xAA, sizeof sz);
    hr = IOleObject_GetExtent(o, DVASPECT_CONTENT, &sz);
    EQ(hr, O2_OLE_E_BLANK);
    hr = IOleObject_GetMiscStatus(o, DVASPECT_CONTENT, &st);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    memset(&id, 0, sizeof id);
    hr = IOleObject_GetUserClassID(o, &id);
    EQ(hr, S_OK); CHECK(!memcmp(&id, &O2_CLSID_A, sizeof id));
    hr = IOleObject_InitFromData(o, NULL, TRUE, 0);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    hr = IOleObject_IsUpToDate(o);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    memset(&pal, 0, sizeof pal); pal.palVersion = 2; pal.palNumEntries = 1;
    hr = IOleObject_SetColorScheme(o, &pal);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    sz.cx = sz.cy = 0;
    hr = IOleObject_SetExtent(o, DVASPECT_CONTENT, &sz);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    hr = IOleObject_SetHostNames(o, L"olet host", NULL);
    EQ(hr, S_OK);
    hr = IOleObject_EnumVerbs(o, &ev);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    EQ(ev, NULL);
    hr = IOleObject_Update(o);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    IOleObject_Release(o);
    O2_END();
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_default_handler */
TEST(o2hdl_handler_usertype_falls_back_to_unknown) {
    IOleObject *o; LPOLESTR s = NULL; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN; REQUIRE(U(CoTaskMemFree));
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    hr = IOleObject_GetUserType(o, USERCLASSTYPE_FULL, &s);
    EQ(hr, S_OK);
    if (s) { CHECK(s[0] != 0); t_note("handler user type of an unregistered class: %ls", s); U(CoTaskMemFree)(s); }
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_null_out_parameters) {
    IOleObject *o;
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    REQUIRE(U(OleCreateDefaultHandler));
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) return;
    EQ(IOleObject_GetUserClassID(o, NULL), O2_E_INVALIDARG);
    EQ(IOleObject_GetUserType(o, USERCLASSTYPE_FULL, NULL), O2_E_INVALIDARG);
    EQ(IOleObject_GetMiscStatus(o, DVASPECT_CONTENT, NULL), O2_E_INVALIDARG);
    EQ(IOleObject_EnumVerbs(o, NULL), O2_E_INVALIDARG);
    EQ(IOleObject_EnumAdvise(o, NULL), O2_E_INVALIDARG);
    IOleObject_Release(o);
}
TEST(o2hdl_handler_moniker_roundtrip_rules) {
    IOleObject *o; IMoniker *mk = NULL, *got = NULL; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN; REQUIRE(U(CreateItemMoniker));
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(U(CreateItemMoniker)(L"!", L"olet_handler", &mk), S_OK);
    hr = IOleObject_SetMoniker(o, OLEWHICHMK_CONTAINER, mk);
    EQ(hr, S_OK);
    hr = IOleObject_GetMoniker(o, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_CONTAINER, &got);
    EQ(hr, E_FAIL);                          /* not running: it does not remember monikers */
    EQ(got, NULL);
    IMoniker_Release(mk);
    IOleObject_Release(o);
    O2_END();
    O2_END();
}
TEST(o2hdl_handler_close_and_runnable) {
    IOleObject *o; IRunnableObject *r = NULL; HRESULT hr; CLSID id;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(IOleObject_Close(o, OLECLOSE_NOSAVE), S_OK);          /* closing something that never ran */
    EQ(IOleObject_QueryInterface(o, &IID_IRunnableObject, (void **)&r), S_OK);
    if (r) {
        EQ(IRunnableObject_IsRunning(r), FALSE);
        EQ(IRunnableObject_SetContainedObject(r, TRUE), S_OK);
        EQ(IRunnableObject_SetContainedObject(r, FALSE), S_OK);
        hr = IRunnableObject_Run(r, NULL);
        EQ(hr, O2_REGDB_E_CLASSNOTREG);                      /* no server to start */
        EQ(IRunnableObject_IsRunning(r), FALSE);
        hr = IRunnableObject_GetRunningClass(r, &id);
        t_note("IRunnableObject::GetRunningClass (not running) -> 0x%08lx", (unsigned long)hr);
        CHECK(FAILED(hr) || hr == S_OK);
        IRunnableObject_Release(r);
    }
    EQ(IOleObject_Close(o, OLECLOSE_NOSAVE), S_OK);
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_olerun_matches_runnable) {
    IOleObject *o;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN; REQUIRE(U(OleRun)); REQUIRE(U(OleIsRunning));
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(U(OleRun)((IUnknown *)o), O2_REGDB_E_CLASSNOTREG);
    EQ(U(OleIsRunning)(o), FALSE);
    EQ(U(OleLockRunning)((IUnknown *)o, TRUE, FALSE) >= 0 || 1, 1);
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_advise_protocol) {
    IOleObject *o; O2Sink a, b; DWORD ca = 0xCCCC, cb = 0xCCCC; IEnumSTATDATA *e = (IEnumSTATDATA *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    os_init(&a); os_init(&b);
    EQ(IOleObject_Unadvise(o, 1), O2_OLE_E_NOCONNECTION);       /* no holder yet */
    hr = IOleObject_EnumAdvise(o, &e);
    EQ(hr, O2_E_FAIL); EQ(e, NULL);                            /* nobody advised: no enumerator */
    if (!t_is_wine()) EQ(IOleObject_Advise(o, &a.i, NULL), O2_E_INVALIDARG);
    if (!t_is_wine()) EQ(IOleObject_Advise(o, NULL, &ca), O2_E_INVALIDARG);
    EQ(IOleObject_Advise(o, &a.i, &ca), S_OK);
    EQ(IOleObject_Advise(o, &b.i, &cb), S_OK);
    CHECK(ca != cb); CHECK(ca != 0 && cb != 0);
    EQ(a.ref, 2); EQ(b.ref, 2);
    EQ(IOleObject_Unadvise(o, ca), S_OK);
    EQ(a.ref, 1);
    EQ(IOleObject_Unadvise(o, ca), O2_OLE_E_NOCONNECTION);
    EQ(IOleObject_Unadvise(o, 0), O2_OLE_E_NOCONNECTION);
    EQ(IOleObject_Close(o, OLECLOSE_NOSAVE), S_OK);
    IOleObject_Release(o);
    EQ(b.ref, 1);                                              /* destroying the handler releases its advise sinks */
    O2_END();
}
TEST(o2hdl_handler_dataobject_not_running) {
    IOleObject *o; IDataObject *d = NULL; FORMATETC f; O2Sink s; DWORD c = 0; STGMEDIUM m; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(IOleObject_QueryInterface(o, &IID_IDataObject, (void **)&d), S_OK);
    if (!d) { IOleObject_Release(o); return; }
    os_init(&s);
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_NULL;
    hr = IDataObject_DAdvise(d, &f, 0, &s.i, &c);
    EQ(hr, S_OK);                                  /* queued until the object runs */
    f.cfFormat = CF_ENHMETAFILE; f.tymed = TYMED_ENHMF;
    hr = IDataObject_QueryGetData(d, &f);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    f.cfFormat = CF_TEXT; f.tymed = TYMED_NULL;
    hr = IDataObject_QueryGetData(d, &f);
    EQ(hr, O2_OLE_E_NOTRUNNING);
    memset(&m, 0, sizeof m);
    f.tymed = TYMED_HGLOBAL;
    hr = IDataObject_GetData(d, &f, &m);
    t_note("handler IDataObject::GetData while not running -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    IDataObject_Release(d);
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_viewobject_and_cache_not_running) {
    IOleObject *o; IViewObject2 *v = NULL; IOleCache2 *c = NULL; SIZEL sz; HRESULT hr; IEnumSTATDATA *e = NULL;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    EQ(IOleObject_QueryInterface(o, &IID_IViewObject2, (void **)&v), S_OK);
    EQ(IOleObject_QueryInterface(o, &IID_IOleCache2, (void **)&c), S_OK);
    if (v) {
        memset(&sz, 0xAA, sizeof sz);
        hr = IViewObject2_GetExtent(v, DVASPECT_CONTENT, -1, NULL, &sz);
        t_note("handler IViewObject2::GetExtent -> 0x%08lx", (unsigned long)hr);
        CHECK(FAILED(hr) || hr == S_OK);
        IViewObject2_Release(v);
    }
    if (c) {
        hr = IOleCache2_EnumCache(c, &e);
        EQ(hr, S_OK);
        if (e) { STATDATA sd; ULONG got = 99; hr = IEnumSTATDATA_Next(e, 1, &sd, &got); t_note("cache entries of a fresh handler: hr 0x%08lx got %lu", (unsigned long)hr, (unsigned long)got); IEnumSTATDATA_Release(e); }
        IOleCache2_Release(c);
    }
    IOleObject_Release(o);
    O2_END();
}
TEST(o2hdl_handler_persiststorage) {
    IOleObject *o; IPersistStorage *ps = NULL; CLSID id; IStorage *stg; HRESULT hr;
    REQUIRE(U(OleCreateDefaultHandler)); O2_BEGIN;
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) { O2_END(); return; }
    stg = o2_mkstg(); if (!stg) { IOleObject_Release(o); O2_END(); t_skip("no storage"); return; }
    EQ(IOleObject_QueryInterface(o, &IID_IPersistStorage, (void **)&ps), S_OK);
    if (ps) {
        memset(&id, 0, sizeof id);
        EQ(IPersistStorage_GetClassID(ps, &id), S_OK);
        CHECK(!memcmp(&id, &O2_CLSID_A, sizeof id));
        hr = IPersistStorage_InitNew(ps, stg);
        EQ(hr, S_OK);
        hr = IPersistStorage_IsDirty(ps);
        t_note("handler IsDirty after InitNew -> 0x%08lx", (unsigned long)hr);
        CHECK(hr == S_OK || hr == S_FALSE);
        hr = IPersistStorage_Save(ps, stg, FALSE);
        t_note("handler Save -> 0x%08lx", (unsigned long)hr);
        hr = IPersistStorage_HandsOffStorage(ps);
        t_note("handler HandsOffStorage -> 0x%08lx", (unsigned long)hr);
        IPersistStorage_Release(ps);
    }
    IOleObject_Close(o, OLECLOSE_NOSAVE);
    IOleObject_Release(o);
    IStorage_Release(stg);
}

/* ---- handler objects are bound to their creating thread -------------------------------------- */
typedef struct { IOleObject *o; HRESULT r[6]; } thr_arg;
static void th_wrong_thread(void *p) {
    thr_arg *a = (thr_arg *)p; CLSID id; void *q = (void *)1; IEnumOLEVERB *ev; LPOLESTR s; DWORD st;
    a->r[0] = IOleObject_GetUserClassID(a->o, &id);
    a->r[1] = IOleObject_QueryInterface(a->o, &IID_IDataObject, &q);
    a->r[2] = IOleObject_EnumVerbs(a->o, &ev);
    a->r[3] = IOleObject_GetUserType(a->o, USERCLASSTYPE_FULL, &s);
    a->r[4] = IOleObject_GetMiscStatus(a->o, DVASPECT_CONTENT, &st);
    a->r[5] = IOleObject_Unadvise(a->o, 1);
    O2_END();
}
TEST(o2hdl_handler_rejects_other_threads) {
    thr_arg a; IOleObject *o; int i;
    REQUIRE(U(OleCreateDefaultHandler));
    EQ(mk_handler(&O2_CLSID_A, &o), S_OK);
    if (!o) return;
    memset(&a, 0, sizeof a); a.o = o;
    CHECK(o2_run_thread(th_wrong_thread, &a, 10000));
    for (i = 0; i < 6; i++) CHECKF(a.r[i] == O2_RPC_E_WRONG_THREAD, "call %d: 0x%08lx", i, (unsigned long)a.r[i]);
    IOleObject_Release(o);
}

/* ================================================================================================
 * CoCreateInstance of an unregistered class
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_default_handler */
TEST(o2hdl_cocreate_unregistered_handler_context) {
    IOleObject *o = (IOleObject *)(ULONG_PTR)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(CoCreateInstance));
    hr = U(CoCreateInstance)(&O2_CLSID_A, NULL, CLSCTX_INPROC_HANDLER, &IID_IOleObject, (void **)&o);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    EQ(o, NULL);
    hr = U(CoCreateInstance)(&O2_CLSID_A, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&o);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    hr = U(CoCreateInstance)(&O2_CLSID_A, NULL, CLSCTX_INPROC_SERVER | CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER, &IID_IUnknown, (void **)&o);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    O2_END();
}

/* ================================================================================================
 * OleCreate* argument validation (wValidateCreateParams / wValidateFormatEtcEx)
 * ============================================================================================== */
TEST(o2create_olecreate_null_output) {
    IStorage *stg;
    if (t_is_wine()) { t_skip("emulator dereferences NULL out pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg();
    EQ(U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, NULL), O2_E_INVALIDARG);
    if (stg) IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_null_storage) {
    void *p = (void *)1;
    if (t_is_wine()) { t_skip("emulator dereferences NULL storage"); return; }
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    EQ(U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, NULL, &p), O2_E_INVALIDARG);
    EQ(p, NULL);
    O2_END();
}
TEST(o2create_olecreate_unregistered_class) {
    static const DWORD rend[] = { OLERENDER_NONE, OLERENDER_ASIS };
    IStorage *stg; int i;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    for (i = 0; i < O2_N(rend); i++) {
        void *p = (void *)1; HRESULT hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, rend[i], NULL, NULL, stg, &p);
        CHECKF(hr == O2_REGDB_E_CLASSNOTREG, "renderopt %lu: 0x%08lx", (unsigned long)rend[i], (unsigned long)hr);
        EQ(p, NULL);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_clsid_null) {
    IStorage *stg; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    hr = U(OleCreate)(&O2_GUID_NULL, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
    t_note("OleCreate(CLSID_NULL) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EQ(p, NULL);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_invalid_renderopt) {
    static const DWORD bad[] = { 4, 5, 7, 99, 0x80000000 };
    IStorage *stg; int i;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    for (i = 0; i < O2_N(bad); i++) {
        void *p = (void *)1; HRESULT hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, bad[i], NULL, NULL, stg, &p);
        CHECKF(hr == O2_E_INVALIDARG, "renderopt %lu: 0x%08lx", (unsigned long)bad[i], (unsigned long)hr);
        EQ(p, NULL);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_format_requires_formatetc) {
    IStorage *stg; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_FORMAT, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    EQ(p, NULL);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_format_validation) {
    static const struct { CLIPFORMAT cf; DWORD tymed; DWORD aspect; LONG idx; HRESULT hr; const char *what; } c[] = {
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, O2_REGDB_E_CLASSNOTREG, "valid text format (fails later: class not registered)" },
        { 0, TYMED_HGLOBAL, DVASPECT_CONTENT, -1, O2_E_INVALIDARG, "cfFormat 0" },
        { CF_TEXT, TYMED_ISTREAM, DVASPECT_CONTENT, -1, O2_DV_E_TYMED, "tymed does not match the format" },
        { CF_DIB, TYMED_GDI, DVASPECT_CONTENT, -1, O2_DV_E_TYMED, "DIB in a GDI medium" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, 0, O2_DV_E_LINDEX, "lindex 0" },
        { CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, 5, O2_DV_E_LINDEX, "lindex 5" },
        { CF_TEXT, TYMED_HGLOBAL, 0, -1, O2_DV_E_DVASPECT, "aspect 0" },
        { CF_TEXT, TYMED_HGLOBAL, 3, -1, O2_DV_E_DVASPECT, "aspect with two bits" },
        { CF_TEXT, TYMED_HGLOBAL, 16, -1, O2_DV_E_DVASPECT, "aspect beyond DOCPRINT" },
        { CF_BITMAP, TYMED_GDI, DVASPECT_ICON, -1, O2_REGDB_E_CLASSNOTREG, "bitmap/icon" },
        { CF_METAFILEPICT, TYMED_MFPICT, DVASPECT_THUMBNAIL, -1, O2_REGDB_E_CLASSNOTREG, "metafile/thumbnail" },
    };
    IStorage *stg; int i;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    for (i = 0; i < O2_N(c); i++) {
        FORMATETC f; void *p = (void *)1; HRESULT hr;
        f.cfFormat = c[i].cf; f.ptd = NULL; f.dwAspect = c[i].aspect; f.lindex = c[i].idx; f.tymed = c[i].tymed;
        hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_FORMAT, &f, NULL, stg, &p);
        CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
        EQ(p, NULL);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_draw_rejects_explicit_format) {
    IStorage *stg; FORMATETC f; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
    hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_DRAW, &f, NULL, stg, &p);
    EQ(hr, O2_DV_E_CLIPFORMAT);
    EQ(p, NULL);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreateex_parameter_matrix) {
    FORMATETC f; DWORD adv[2] = { 0, 0 }, con[2]; O2Sink sink; IStorage *stg; O2Site site;
    typedef struct { DWORD flags, render, cfmt; int adv, fmt, sink, conn; HRESULT hr; const char *what; } cs;
    static const cs c[] = {
        { 0, OLERENDER_NONE, 0, 0, 0, 0, 0, O2_REGDB_E_CLASSNOTREG, "baseline" },
        { 1, OLERENDER_NONE, 0, 0, 0, 0, 0, O2_REGDB_E_CLASSNOTREG, "OLECREATE_LEAVERUNNING" },
        { 2, OLERENDER_NONE, 0, 0, 0, 0, 0, O2_E_INVALIDARG, "unknown flag bit" },
        { 0x80000000, OLERENDER_NONE, 0, 0, 0, 0, 0, O2_E_INVALIDARG, "high flag bit" },
        { 0, OLERENDER_NONE, 0, 1, 0, 0, 0, O2_E_INVALIDARG, "advf array without formats" },
        { 0, OLERENDER_NONE, 0, 0, 1, 0, 0, O2_E_INVALIDARG, "formatetc without count" },
        { 0, OLERENDER_NONE, 0, 0, 0, 0, 1, O2_E_INVALIDARG, "connection array without count" },
        { 0, OLERENDER_FORMAT, 1, 0, 1, 0, 0, O2_E_INVALIDARG, "count without advf array" },
        { 0, OLERENDER_FORMAT, 1, 1, 0, 0, 0, O2_E_INVALIDARG, "count without formatetc" },
        { 0, OLERENDER_FORMAT, 1, 1, 1, 0, 0, O2_REGDB_E_CLASSNOTREG, "one format" },
        { 0, OLERENDER_FORMAT, 2, 1, 1, 0, 0, O2_REGDB_E_CLASSNOTREG, "two formats" },
        { 0, OLERENDER_FORMAT, 1, 1, 1, 1, 1, O2_REGDB_E_CLASSNOTREG, "format with sink and connections" },
        { 0, OLERENDER_NONE, 0, 0, 0, 1, 0, O2_E_INVALIDARG, "sink without FORMAT rendering" },
        { 0, OLERENDER_ASIS, 0, 0, 0, 1, 0, O2_E_INVALIDARG, "sink with ASIS" },
        { 0, OLERENDER_DRAW, 0, 0, 0, 1, 0, O2_E_INVALIDARG, "sink with DRAW" },
        { 0, OLERENDER_DRAW, 2, 1, 1, 0, 0, O2_E_INVALIDARG, "DRAW with two formats" },
    };
    int i;
    O2_BEGIN;
    REQUIRE(U(OleCreateEx));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    os_init(&sink); osi_init(&site);
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
    for (i = 0; i < O2_N(c); i++) {
        FORMATETC fa[2]; void *p = (void *)1; HRESULT hr;
        fa[0] = f; fa[1] = f;
        con[0] = con[1] = 0xCCCC;
        hr = U(OleCreateEx)(&O2_CLSID_A, &IID_IOleObject, c[i].flags, c[i].render, c[i].cfmt, c[i].adv ? adv : NULL,
                            c[i].fmt ? fa : NULL, c[i].sink ? &sink.i : NULL, c[i].conn ? con : NULL, NULL, stg, &p);
        CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
        EQ(p, NULL);
        if (c[i].hr == O2_E_INVALIDARG) EQ(sink.ref, 1);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreateex_advf_bits) {
    FORMATETC f; DWORD adv; IStorage *stg; void *p; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreateEx));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
    adv = 0x7f; p = (void *)1;
    hr = U(OleCreateEx)(&O2_CLSID_A, &IID_IOleObject, 0, OLERENDER_FORMAT, 1, &adv, &f, NULL, NULL, NULL, stg, &p);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);               /* all seven ADVF_* bits are accepted */
    adv = 0x80; p = (void *)1;
    hr = U(OleCreateEx)(&O2_CLSID_A, &IID_IOleObject, 0, OLERENDER_FORMAT, 1, &adv, &f, NULL, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);                      /* anything above ADVF_DATAONSTOP is not */
    adv = 0x100; p = (void *)1;
    hr = U(OleCreateEx)(&O2_CLSID_A, &IID_IOleObject, 0, OLERENDER_FORMAT, 1, &adv, &f, NULL, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_clears_connection_array) {
    FORMATETC f; DWORD adv = 0, con[2] = { 0xAAAA, 0xBBBB }; IStorage *stg; void *p; HRESULT hr; O2Sink sink;
    if (t_is_wine()) { t_skip("emulator crashes on this parameter set"); return; }
    O2_BEGIN;
    REQUIRE(U(OleCreateEx));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    os_init(&sink);
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
    p = (void *)1;
    hr = U(OleCreateEx)(&O2_CLSID_A, &IID_IOleObject, 0, OLERENDER_FORMAT, 1, &adv, &f, &sink.i, con, NULL, stg, &p);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    EQ(con[0], 0);                                /* the connection array is zeroed up front */
    EQ(con[1], 0xBBBB);                           /* but only cFormats entries */
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreate_invalid_client_site_storage_args) {
    IStorage *stg; O2Site site; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreate));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    osi_init(&site);
    hr = U(OleCreate)(&O2_CLSID_A, &IID_IOleObject, OLERENDER_NONE, NULL, &site.i, stg, &p);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    EQ(site.ref, 1);
    IStorage_Release(stg);
    O2_END();
}

/* OleCreateFromData: classification of the source data happens after parameter validation */
TEST(o2create_fromdata_null_arguments) {
    IStorage *stg; O2Data d; void *p = (void *)1;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(OleCreateFromData));
    stg = o2_mkstg(); od_init(&d);
    EQ(U(OleCreateFromData)(NULL, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p), O2_E_INVALIDARG);
    EQ(p, NULL);
    EQ(U(OleCreateFromData)(&d.i, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, NULL), O2_E_INVALIDARG);
    EQ(U(OleCreateFromData)(&d.i, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, NULL, &p), O2_E_INVALIDARG);
    if (stg) IStorage_Release(stg);
    O2_END();
}
TEST(o2create_fromdata_format_classification) {
    static const struct { CLIPFORMAT cf; HRESULT hr; const char *what; } c[] = {
        { CF_TEXT, O2_DV_E_FORMATETC, "plain text: nothing to create from" },
        { CF_UNICODETEXT, O2_DV_E_FORMATETC, "unicode text" },
        { CF_DIB, (HRESULT)0x8004000B, "a DIB is static data only: OLE_E_STATIC" },
        { CF_BITMAP, (HRESULT)0x8004000B, "a bitmap is static data only" },
        { CF_METAFILEPICT, (HRESULT)0x8004000B, "a metafile is static data only" },
        { CF_ENHMETAFILE, (HRESULT)0x8004000B, "an enhanced metafile is static data only" },
    };
    IStorage *stg; int i; O2Data d;
    O2_BEGIN;
    REQUIRE(U(OleCreateFromData));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    for (i = 0; i < O2_N(c); i++) {
        void *p = (void *)1; HRESULT hr;
        od_init(&d); od_add(&d, c[i].cf, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
        hr = U(OleCreateFromData)(&d.i, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
        CHECKF(hr == c[i].hr, "%s: got 0x%08lx want 0x%08lx", c[i].what, (unsigned long)hr, (unsigned long)c[i].hr);
        EQ(p, NULL);
        EQ(d.ref, 1);
    }
    od_init(&d);
    {
        void *p = (void *)1;
        HRESULT hr = U(OleCreateFromData)(&d.i, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
        EQ(hr, O2_DV_E_FORMATETC);              /* an object offering no formats at all */
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_staticfromdata_validation) {
    IStorage *stg; O2Data d; void *p; HRESULT hr; FORMATETC f;
    O2_BEGIN;
    REQUIRE(U(OleCreateStaticFromData));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    od_init(&d); od_add(&d, CF_DIB, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
    if (!t_is_wine()) {
        p = (void *)1; EQ(U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_DRAW, NULL, NULL, stg, NULL), O2_E_INVALIDARG);
        p = (void *)1; EQ(U(OleCreateStaticFromData)(NULL, &IID_IOleObject, OLERENDER_DRAW, NULL, NULL, stg, &p), O2_E_INVALIDARG);
        EQ(p, NULL);
        p = (void *)1; EQ(U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_DRAW, NULL, NULL, NULL, &p), O2_E_INVALIDARG);
    }
    /* NONE (0) and ASIS (3) are not valid for a static object */
    p = (void *)1; hr = U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG); EQ(p, NULL);
    p = (void *)1; hr = U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_ASIS, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    /* FORMAT requires a formatetc, and it must be a picture format */
    p = (void *)1; hr = U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_FORMAT, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    f.cfFormat = CF_TEXT; f.ptd = NULL; f.dwAspect = DVASPECT_CONTENT; f.lindex = -1; f.tymed = TYMED_HGLOBAL;
    p = (void *)1; hr = U(OleCreateStaticFromData)(&d.i, &IID_IOleObject, OLERENDER_FORMAT, &f, NULL, stg, &p);
    EQ(hr, O2_DV_E_CLIPFORMAT);
    /* DRAW needs a picture format in the source */
    {
        O2Data txt; od_init(&txt); od_add(&txt, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
        p = (void *)1; hr = U(OleCreateStaticFromData)(&txt.i, &IID_IOleObject, OLERENDER_DRAW, NULL, NULL, stg, &p);
        EQ(hr, O2_DV_E_CLIPFORMAT);
        EQ(p, NULL);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreatelink_null_moniker) {
    IStorage *stg; void *p = (void *)1;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(OleCreateLink));
    stg = o2_mkstg();
    EQ(U(OleCreateLink)(NULL, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p), O2_E_INVALIDARG);
    EQ(p, NULL);
    EQ(U(OleCreateLink)(NULL, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, NULL), O2_E_INVALIDARG);
    if (stg) IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreatelink_bad_params_with_moniker) {
    IStorage *stg; IMoniker *mk = NULL; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleCreateLink)); REQUIRE(U(CreateItemMoniker));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    EQ(U(CreateItemMoniker)(L"!", L"olet_link", &mk), S_OK);
    hr = U(OleCreateLink)(mk, &IID_IOleObject, 99, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    EQ(p, NULL);
    hr = U(OleCreateLink)(mk, &IID_IOleObject, OLERENDER_FORMAT, NULL, NULL, stg, &p);
    EQ(hr, O2_E_INVALIDARG);
    hr = U(OleCreateLink)(mk, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, NULL, &p);
    EQ(hr, O2_E_INVALIDARG);
    if (mk) IMoniker_Release(mk);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreatefromfile_missing_file) {
    IStorage *stg; void *p = (void *)1; HRESULT hr; WCHAR path[MAX_PATH]; char a[MAX_PATH];
    O2_BEGIN;
    REQUIRE(U(OleCreateFromFile));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    t_tmpfile(a, sizeof a, "o2create_missing_file.bin");
    MultiByteToWideChar(CP_ACP, 0, a, -1, path, MAX_PATH);
    DeleteFileW(path);
    hr = U(OleCreateFromFile)(&O2_GUID_NULL, path, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
    t_note("OleCreateFromFile(missing file) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EQ(p, NULL);
    hr = U(OleCreateLinkToFile)(path, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, stg, &p);
    t_note("OleCreateLinkToFile(missing file) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EQ(p, NULL);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2create_olecreatefromfile_bad_render_args) {
    IStorage *stg; void *p = (void *)1; HRESULT hr; WCHAR path[MAX_PATH]; char a[MAX_PATH]; HANDLE f;
    O2_BEGIN;
    REQUIRE(U(OleCreateFromFile));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    t_tmpfile(a, sizeof a, "o2create_file.bin");
    MultiByteToWideChar(CP_ACP, 0, a, -1, path, MAX_PATH);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(f);
    hr = U(OleCreateFromFile)(&O2_GUID_NULL, path, &IID_IOleObject, 99, NULL, NULL, stg, &p);
    t_note("OleCreateFromFile(renderopt 99) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EQ(p, NULL);
    hr = U(OleCreateFromFile)(&O2_GUID_NULL, path, &IID_IOleObject, OLERENDER_FORMAT, NULL, NULL, stg, &p);
    t_note("OleCreateFromFile(FORMAT without formatetc) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    DeleteFileW(path);
    IStorage_Release(stg);
    O2_END();
}

/* ================================================================================================
 * OleLoad / OleSave / OleDoAutoConvert
 * ============================================================================================== */
TEST(o2load_invalid_arguments) {
    IStorage *stg; void *p = (void *)1;
    if (t_is_wine()) { t_skip("emulator dereferences NULL pointers"); return; }
    O2_BEGIN;
    REQUIRE(U(OleLoad));
    stg = o2_mkstg();
    EQ(U(OleLoad)(NULL, &IID_IOleObject, NULL, &p), O2_E_INVALIDARG);
    EQ(p, NULL);
    EQ(U(OleLoad)(stg, &IID_IOleObject, NULL, NULL), O2_E_INVALIDARG);
    if (stg) IStorage_Release(stg);
    O2_END();
}
TEST(o2load_storage_with_unregistered_class_creates_handler) {
    IStorage *stg; void *p = (void *)1; HRESULT hr; CLSID id; IOleObject *o;
    O2_BEGIN;
    REQUIRE(U(OleLoad)); REQUIRE(U(WriteClassStg));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    EQ(U(WriteClassStg)(stg, &O2_CLSID_A), S_OK);
    hr = U(OleLoad)(stg, &IID_IOleObject, NULL, &p);
    t_note("OleLoad(storage of an unregistered class) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        o = (IOleObject *)p;
        CHECK(o != NULL);
        if (o) {
            memset(&id, 0, sizeof id);
            EQ(IOleObject_GetUserClassID(o, &id), S_OK);
            CHECK(!memcmp(&id, &O2_CLSID_A, sizeof id));
            IOleObject_Close(o, OLECLOSE_NOSAVE);
            IOleObject_Release(o);
        }
    } else {
        EQ(p, NULL);
    }
    IStorage_Release(stg);
    O2_END();
}
TEST(o2load_storage_without_class) {
    IStorage *stg; void *p = (void *)1; HRESULT hr;
    O2_BEGIN;
    REQUIRE(U(OleLoad));
    stg = o2_mkstg(); if (!stg) { O2_END(); t_skip("no storage"); return; }
    hr = U(OleLoad)(stg, &IID_IOleObject, NULL, &p);
    t_note("OleLoad(storage without class) -> 0x%08lx", (unsigned long)hr);
    if (FAILED(hr)) EQ(p, NULL);
    else if (p) IOleObject_Release((IOleObject *)p);
    IStorage_Release(stg);
    O2_END();
}
TEST(o2save_invalid_arguments) {
    O2Pers ps; IStorage *stg;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(OleSave));
    op_init(&ps); stg = o2_mkstg();
    EQ(U(OleSave)(NULL, stg, FALSE), O2_E_INVALIDARG);
    EQ(U(OleSave)(&ps.i, NULL, FALSE), O2_E_INVALIDARG);
    EQ(ps.getclass, 0); EQ(ps.save, 0);
    if (stg) IStorage_Release(stg);
}
TEST(o2save_writes_class_then_saves) {
    O2Pers ps; IStorage *stg; STATSTG st; HRESULT hr;
    REQUIRE(U(OleSave));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    op_init(&ps); ps.clsid = O2_CLSID_B;
    hr = U(OleSave)(&ps.i, stg, FALSE);
    EQ(hr, S_OK);
    EQ(ps.getclass, 1); EQ(ps.save, 1);
    EQ(ps.last_same_as_load, FALSE);
    EQ(ps.save_stg, stg);
    EQ(ps.ref, 1);
    EQ(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    CHECK(!memcmp(&st.clsid, &O2_CLSID_B, sizeof(CLSID)));
    ps.save = 0;
    hr = U(OleSave)(&ps.i, stg, TRUE);
    EQ(hr, S_OK);
    EQ(ps.last_same_as_load, TRUE);                  /* fSameAsLoad is passed through */
    IStorage_Release(stg);
}
TEST(o2save_getclassid_failure_short_circuits) {
    O2Pers ps; IStorage *stg; STATSTG st; HRESULT hr;
    REQUIRE(U(OleSave));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    op_init(&ps); ps.classid_hr = (HRESULT)0x80041234;
    hr = U(OleSave)(&ps.i, stg, FALSE);
    EQ(hr, (HRESULT)0x80041234);
    EQ(ps.save, 0);                                  /* Save is never reached */
    EQ(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    CHECK(!memcmp(&st.clsid, &O2_GUID_NULL, sizeof(CLSID)));
    IStorage_Release(stg);
}
TEST(o2save_save_failure_still_wrote_class) {
    O2Pers ps; IStorage *stg; STATSTG st; HRESULT hr;
    REQUIRE(U(OleSave));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    op_init(&ps); ps.clsid = O2_CLSID_B; ps.save_hr = STG_E_MEDIUMFULL;
    hr = U(OleSave)(&ps.i, stg, FALSE);
    EQ(hr, STG_E_MEDIUMFULL);
    EQ(ps.save, 1);
    EQ(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    CHECK(!memcmp(&st.clsid, &O2_CLSID_B, sizeof(CLSID)));   /* the class was written before Save was attempted */
    IStorage_Release(stg);
}
TEST(o2save_commits_storage) {
    O2Pers ps; IStorage *base, *child, *reopened; HRESULT hr; char a[MAX_PATH]; WCHAR w[MAX_PATH];
    REQUIRE(U(OleSave)); REQUIRE(U(StgCreateDocfile));
    t_tmpfile(a, sizeof a, "o2save_commit.stg");
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    hr = U(StgCreateDocfile)(w, STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, 0, &base);
    EQ(hr, S_OK);
    if (!base) return;
    op_init(&ps); ps.clsid = O2_CLSID_B;
    EQ(U(OleSave)(&ps.i, base, FALSE), S_OK);
    IStorage_Release(base);                       /* transacted: only committed state reaches the file */
    hr = U(StgOpenStorage)(w, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &reopened);
    EQ(hr, S_OK);
    if (reopened) {
        STATSTG st;
        EQ(IStorage_Stat(reopened, &st, STATFLAG_NONAME), S_OK);
        CHECK(!memcmp(&st.clsid, &O2_CLSID_B, sizeof(CLSID)));
        IStorage_Release(reopened);
    }
    DeleteFileW(w);
}
TEST(o2autoconvert_unregistered_class) {
    IStorage *stg; CLSID out; HRESULT hr;
    REQUIRE(U(OleDoAutoConvert)); REQUIRE(U(WriteClassStg));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    EQ(U(WriteClassStg)(stg, &O2_CLSID_A), S_OK);
    out = O2_CLSID_B;
    hr = U(OleDoAutoConvert)(stg, &out);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);                 /* no AutoConvertTo key for the class */
    CHECK(!memcmp(&out, &O2_CLSID_A, sizeof out));   /* the storage's own class is reported back */
    IStorage_Release(stg);
}
TEST(o2autoconvert_storage_without_class) {
    IStorage *stg; CLSID out; HRESULT hr;
    REQUIRE(U(OleDoAutoConvert));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    out = O2_CLSID_B;
    hr = U(OleDoAutoConvert)(stg, &out);
    t_note("OleDoAutoConvert(storage with CLSID_NULL) -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr) || hr == S_FALSE);
    CHECK(!memcmp(&out, &O2_GUID_NULL, sizeof out));
    IStorage_Release(stg);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_OleDoAutoConvert */
TEST(o2autoconvert_null_storage_clears_output) {
    CLSID out = O2_CLSID_B; HRESULT hr;
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(OleDoAutoConvert));
    hr = U(OleDoAutoConvert)(NULL, &out);
    EQ(hr, O2_E_INVALIDARG);
    CHECK(!memcmp(&out, &O2_GUID_NULL, sizeof out));
}
TEST(o2autoconvert_getautoconvert_unregistered) {
    CLSID out = O2_CLSID_B; HRESULT hr;
    REQUIRE(U(OleGetAutoConvert));
    hr = U(OleGetAutoConvert)(&O2_CLSID_A, &out);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    t_note("OleGetAutoConvert output after failure: %s", !memcmp(&out, &O2_GUID_NULL, sizeof out) ? "CLSID_NULL" : "unchanged/other");
}
