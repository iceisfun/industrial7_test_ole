/* CreateDataCache and the COleCache interfaces: IOleCache/IOleCache2, IPersistStorage, IViewObject(2),
 * IOleCacheControl and the cache's IDataObject. */
#include "olh.h"

#define G(n) UIN("gdi32.dll", n)

typedef struct { IOleCache2 *c2; IPersistStorage *ps; IViewObject *vo; IOleCacheControl *cc; IDataObject *dobj; } cache_t;

static HRESULT cache_open(cache_t *c, const GUID *clsid) {
    HRESULT hr;
    memset(c, 0, sizeof *c);
    hr = U(CreateDataCache)(NULL, clsid, &IID_IOleCache2, (void **)&c->c2);
    if (FAILED(hr)) return hr;
    IOleCache2_QueryInterface(c->c2, &IID_IPersistStorage, (void **)&c->ps);
    IOleCache2_QueryInterface(c->c2, &IID_IViewObject, (void **)&c->vo);
    IOleCache2_QueryInterface(c->c2, &IID_IOleCacheControl, (void **)&c->cc);
    IOleCache2_QueryInterface(c->c2, &IID_IDataObject, (void **)&c->dobj);
    return hr;
}
static void cache_close(cache_t *c) {
    if (c->dobj) IDataObject_Release(c->dobj);
    if (c->cc) IOleCacheControl_Release(c->cc);
    if (c->vo) IViewObject_Release(c->vo);
    if (c->ps) IPersistStorage_Release(c->ps);
    if (c->c2) EQ(IOleCache2_Release(c->c2), 0);
}
static HGLOBAL mk_mfpict(void) {
    return U(OleMetafilePictFromIconAndLabel)(LoadIconA(NULL, IDI_APPLICATION), L"cache", NULL, 0);
}
static FORMATETC mkfe(CLIPFORMAT cf, DWORD aspect, DWORD tymed) {
    FORMATETC f; f.cfFormat = cf; f.ptd = NULL; f.dwAspect = aspect; f.lindex = -1; f.tymed = tymed; return f;
}
static const GUID O2_CLSID_PICTURE_DIB     = {0x00000316, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID O2_CLSID_PICTURE_META    = {0x00000315, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID O2_CLSID_PICTURE_ENHMETA = {0x00000319, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

/* ================================================================================================
 * creation / aggregation / QueryInterface
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache */
TEST(o2cache_create_arguments) {
    IUnknown *unk = NULL; O2Run outer; void *p = NULL; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    or_init(&outer);
    /* with an aggregating outer the requested interface must be IUnknown */
    hr = U(CreateDataCache)((IUnknown *)&outer.i, &O2_GUID_NULL, &IID_IOleCache2, &p);
    EQ(hr, O2_E_INVALIDARG);
    hr = U(CreateDataCache)((IUnknown *)&outer.i, &O2_GUID_NULL, &IID_IUnknown, (void **)&unk);
    EQ(hr, S_OK);
    if (unk) IUnknown_Release(unk);
    EQ(outer.ref, 1);
    if (!t_is_wine()) EQ(U(CreateDataCache)(NULL, &O2_GUID_NULL, &IID_IUnknown, NULL), O2_E_INVALIDARG);
    p = (void *)1;
    hr = U(CreateDataCache)(NULL, &O2_GUID_NULL, &O2_IID_BOGUS, &p);
    EQ(hr, O2_E_NOINTERFACE);
    EQ(p, NULL);
}
TEST(o2cache_aggregation_inner_identity) {
    IUnknown *unk = NULL; IOleCache *oc = NULL; IOleCache2 *oc2 = NULL; O2Run outer; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    or_init(&outer);
    hr = U(CreateDataCache)((IUnknown *)&outer.i, &O2_GUID_NULL, &IID_IUnknown, (void **)&unk);
    EQ(hr, S_OK);
    if (!unk) return;
    EQ(IUnknown_QueryInterface(unk, &IID_IOleCache, (void **)&oc), S_OK);
    EQ(IUnknown_QueryInterface(unk, &IID_IOleCache2, (void **)&oc2), S_OK);
    CHECK((IUnknown *)oc != unk);          /* the non-delegating IUnknown is a separate interface */
    CHECK((IUnknown *)oc2 != unk);
    if (oc2) IOleCache2_Release(oc2);
    if (oc) IOleCache_Release(oc);
    IUnknown_Release(unk);
}
TEST(o2cache_nonaggregated_identity) {
    IUnknown *unk = NULL, *u2 = NULL; IOleCache *oc = NULL; IOleCache2 *oc2 = NULL;
    REQUIRE(U(CreateDataCache));
    EQ(U(CreateDataCache)(NULL, &O2_GUID_NULL, &IID_IUnknown, (void **)&unk), S_OK);
    if (!unk) return;
    EQ(IUnknown_QueryInterface(unk, &IID_IOleCache, (void **)&oc), S_OK);
    EQ(IUnknown_QueryInterface(unk, &IID_IOleCache2, (void **)&oc2), S_OK);
    EQ(IUnknown_QueryInterface(unk, &IID_IUnknown, (void **)&u2), S_OK);
    EQ(oc, unk); EQ(oc2, unk); EQ(u2, unk);   /* one vtable serves IUnknown/IOleCache/IOleCache2 */
    IUnknown_Release(u2); IOleCache2_Release(oc2); IOleCache_Release(oc);
    EQ(IUnknown_Release(unk), 0);
}
TEST(o2cache_refcount_basics) {
    IUnknown *unk = NULL;
    REQUIRE(U(CreateDataCache));
    EQ(U(CreateDataCache)(NULL, &O2_GUID_NULL, &IID_IUnknown, (void **)&unk), S_OK);
    if (!unk) return;
    EQ(IUnknown_AddRef(unk), 2);
    EQ(IUnknown_AddRef(unk), 3);
    EQ(IUnknown_Release(unk), 2);
    EQ(IUnknown_Release(unk), 1);
    EQ(IUnknown_Release(unk), 0);
}
TEST(o2cache_queryinterface_matrix) {
    static const struct { const GUID *iid; const char *n; } sup[] = {
        { &IID_IUnknown, "IUnknown" }, { &IID_IOleCache, "IOleCache" }, { &IID_IOleCache2, "IOleCache2" },
        { &IID_IPersistStorage, "IPersistStorage" }, { &IID_IViewObject, "IViewObject" }, { &IID_IViewObject2, "IViewObject2" },
        { &IID_IOleCacheControl, "IOleCacheControl" }, { &IID_IDataObject, "IDataObject" },
    };
    static const struct { const GUID *iid; const char *n; } unsup[] = {
        { &IID_IOleObject, "IOleObject" }, { &IID_IRunnableObject, "IRunnableObject" }, { &IID_IStorage, "IStorage" },
        { &IID_IStream, "IStream" }, { &IID_IOleInPlaceObject, "IOleInPlaceObject" }, { &IID_IDropTarget, "IDropTarget" },
        { &IID_IClassFactory, "IClassFactory" }, { &O2_IID_BOGUS, "bogus" },
    };
    cache_t c; int i;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    for (i = 0; i < O2_N(sup); i++) {
        void *p = NULL; HRESULT hr = IOleCache2_QueryInterface(c.c2, sup[i].iid, &p);
        CHECKF(hr == S_OK && p != NULL, "%s: 0x%08lx", sup[i].n, (unsigned long)hr);
        if (p) IUnknown_Release((IUnknown *)p);
    }
    for (i = 0; i < O2_N(unsup); i++) {
        void *p = (void *)1; HRESULT hr = IOleCache2_QueryInterface(c.c2, unsup[i].iid, &p);
        CHECKF(hr == O2_E_NOINTERFACE && p == NULL, "%s: 0x%08lx", unsup[i].n, (unsigned long)hr);
        if (SUCCEEDED(hr) && p) IUnknown_Release((IUnknown *)p);
    }
    cache_close(&c);
}
TEST(o2cache_getclassid) {
    cache_t c; CLSID id;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.ps) return;
    memset(&id, 0x55, sizeof id);
    EQ(IPersistStorage_GetClassID(c.ps, &id), S_OK);
    CHECK(!memcmp(&id, &O2_GUID_NULL, sizeof id));
    cache_close(&c);
    EQ(cache_open(&c, &O2_CLSID_B), S_OK);
    if (!c.ps) return;
    memset(&id, 0x55, sizeof id);
    EQ(IPersistStorage_GetClassID(c.ps, &id), S_OK);
    CHECK(!memcmp(&id, &O2_CLSID_B, sizeof id));
    cache_close(&c);
}

/* ================================================================================================
 * IOleCache::Cache / Uncache / EnumCache
 * ============================================================================================== */
TEST(o2cache_cache_null_arguments) {
    cache_t c; DWORD con; FORMATETC f = mkfe(CF_BITMAP, DVASPECT_THUMBNAIL, TYMED_GDI);
    if (t_is_wine()) { t_skip("emulator dereferences NULL pointers"); return; }
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, NULL, 0, &con), O2_E_INVALIDARG);
    EQ(IOleCache2_Cache(c.c2, NULL, 0, NULL), O2_E_INVALIDARG);
    EQ(IOleCache2_Cache(c.c2, &f, 0, NULL), O2_E_INVALIDARG);
    cache_close(&c);
}
TEST(o2cache_uncache_unknown_connection) {
    cache_t c;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Uncache(c.c2, 0xdeadbeef), O2_OLE_E_NOCONNECTION);
    EQ(IOleCache2_Uncache(c.c2, 0), O2_OLE_E_NOCONNECTION);
    EQ(IOleCache2_Uncache(c.c2, 1), O2_OLE_E_NOCONNECTION);
    cache_close(&c);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache: exhaustive Cache() matrix */
TEST(o2cache_cache_format_tymed_matrix) {
    cache_t c; int cf, i;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    for (cf = CF_TEXT; cf < 18; cf++) {
        for (i = 0; i < 7; i++) {
            DWORD tymed = 1u << i, con = 0xCCCC; HRESULT want, hr;
            FORMATETC f = mkfe((CLIPFORMAT)cf, DVASPECT_THUMBNAIL, tymed);
            if ((cf == CF_METAFILEPICT && tymed == TYMED_MFPICT) || (cf == CF_BITMAP && tymed == TYMED_GDI) ||
                (cf == CF_DIB && tymed == TYMED_HGLOBAL) || (cf == CF_ENHMETAFILE && tymed == TYMED_ENHMF)) want = S_OK;
            else if (tymed == TYMED_HGLOBAL) want = O2_CACHE_S_FORMATETC_NOTSUPPORTED;
            else want = O2_DV_E_TYMED;
            hr = IOleCache2_Cache(c.c2, &f, 0, &con);
            CHECKF(hr == want, "cf %d tymed %lu: got 0x%08lx want 0x%08lx", cf, (unsigned long)tymed, (unsigned long)hr, (unsigned long)want);
            if (SUCCEEDED(hr)) CHECKF(IOleCache2_Uncache(c.c2, con) == S_OK, "uncache cf %d tymed %lu", cf, (unsigned long)tymed);
        }
    }
    cache_close(&c);
}
TEST(o2cache_cache_user_defined_format) {
    cache_t c; DWORD con = 0xCCCC; HRESULT hr; FORMATETC f;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    f = mkfe((CLIPFORMAT)RegisterClipboardFormatA("olet_o2cache_fmt"), DVASPECT_CONTENT, TYMED_HGLOBAL);
    hr = IOleCache2_Cache(c.c2, &f, 0, &con);
    EQ(hr, O2_CACHE_S_FORMATETC_NOTSUPPORTED);
    f.tymed = TYMED_ISTREAM;
    hr = IOleCache2_Cache(c.c2, &f, 0, &con);
    EQ(hr, O2_DV_E_TYMED);
    cache_close(&c);
}
TEST(o2cache_cache_native_format) {
    cache_t c; DWORD con = 0xCCCC, con2 = 0xCCCC; HRESULT hr; FORMATETC f = mkfe(0, DVASPECT_ICON, TYMED_MFPICT);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    hr = IOleCache2_Cache(c.c2, &f, 0, &con);          /* cfFormat 0: "whatever the object uses" for the aspect */
    EQ(hr, S_OK);
    CHECK(con != 0xCCCC);
    hr = IOleCache2_Cache(c.c2, &f, 0, &con2);
    t_note("second identical Cache() -> 0x%08lx conn %lu (first %lu)", (unsigned long)hr, (unsigned long)con2, (unsigned long)con);
    CHECK(SUCCEEDED(hr));
    EQ(IOleCache2_Uncache(c.c2, con), S_OK);
    cache_close(&c);
}
TEST(o2cache_cache_distinct_connections) {
    cache_t c; DWORD a = 0, b = 0, d = 0;
    FORMATETC f1 = mkfe(CF_BITMAP, DVASPECT_CONTENT, TYMED_GDI), f2 = mkfe(0, DVASPECT_ICON, TYMED_MFPICT), f3 = mkfe(CF_ENHMETAFILE, DVASPECT_THUMBNAIL, TYMED_ENHMF);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    EQ(IOleCache2_Cache(c.c2, &f1, 0, &a), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f2, 0, &b), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f3, 0, &d), S_OK);
    CHECK(a != b && b != d && a != d);
    EQ(IOleCache2_Uncache(c.c2, b), S_OK);
    EQ(IOleCache2_Uncache(c.c2, b), O2_OLE_E_NOCONNECTION);
    EQ(IOleCache2_Uncache(c.c2, a), S_OK);
    EQ(IOleCache2_Uncache(c.c2, d), S_OK);
    cache_close(&c);
}
TEST(o2cache_cache_same_family_same_aspect) {
    /* a DIB and a bitmap in the same aspect are one cache entry: the second Cache() reports the first */
    cache_t c; DWORD a = 0, b = 0; HRESULT hr;
    FORMATETC f1 = mkfe(CF_BITMAP, DVASPECT_CONTENT, TYMED_GDI), f2 = mkfe(CF_DIB, DVASPECT_CONTENT, TYMED_HGLOBAL);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    EQ(IOleCache2_Cache(c.c2, &f1, 0, &a), S_OK);
    hr = IOleCache2_Cache(c.c2, &f2, 0, &b);
    t_note("Cache(CF_DIB) after Cache(CF_BITMAP) in one aspect -> 0x%08lx (conn %lu then %lu)", (unsigned long)hr, (unsigned long)a, (unsigned long)b);
    CHECK(SUCCEEDED(hr));
    cache_close(&c);
}
TEST(o2cache_enumcache) {
    cache_t c; IEnumSTATDATA *e = NULL; STATDATA sd[4]; ULONG got; DWORD a, b;
    FORMATETC f1 = mkfe(CF_BITMAP, DVASPECT_CONTENT, TYMED_GDI), f2 = mkfe(CF_METAFILEPICT, DVASPECT_ICON, TYMED_MFPICT);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    EQ(IOleCache2_EnumCache(c.c2, &e), S_OK);
    CHECK(e != NULL);
    if (e) { got = 99; EQ(IEnumSTATDATA_Next(e, 1, sd, &got), S_FALSE); EQ(got, 0); IEnumSTATDATA_Release(e); }
    EQ(IOleCache2_Cache(c.c2, &f1, ADVF_PRIMEFIRST, &a), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f2, 0, &b), S_OK);
    e = NULL; EQ(IOleCache2_EnumCache(c.c2, &e), S_OK);
    if (e) {
        HRESULT hr; int k, fa = 0, fb = 0;
        memset(sd, 0, sizeof sd); got = 99;
        hr = IEnumSTATDATA_Next(e, 4, sd, &got);
        CHECK(got >= 2 && got <= 4);
        EQ(hr, got == 4 ? S_OK : S_FALSE);
        for (k = 0; k < (int)got; k++) {
            if (sd[k].dwConnection == a) {
                fa = 1;
                EQ(sd[k].formatetc.cfFormat, CF_BITMAP); EQ(sd[k].formatetc.dwAspect, DVASPECT_CONTENT); EQ(sd[k].formatetc.tymed, TYMED_GDI);
                EQ(sd[k].advf & ADVF_PRIMEFIRST, ADVF_PRIMEFIRST);
            } else if (sd[k].dwConnection == b) {
                fb = 1;
                EQ(sd[k].formatetc.cfFormat, CF_METAFILEPICT); EQ(sd[k].formatetc.dwAspect, DVASPECT_ICON); EQ(sd[k].formatetc.tymed, TYMED_MFPICT);
            }
            t_note("enum entry %d: conn %lu cf %u aspect %lu tymed %lu advf %lu", k, (unsigned long)sd[k].dwConnection, sd[k].formatetc.cfFormat, (unsigned long)sd[k].formatetc.dwAspect, (unsigned long)sd[k].formatetc.tymed, (unsigned long)sd[k].advf);
        }
        CHECK(fa && fb);
        IEnumSTATDATA_Release(e);
    }
    cache_close(&c);
}

/* ================================================================================================
 * SetData / IViewObject / draw
 * ============================================================================================== */
static BOOL STDMETHODCALLTYPE cont_true(ULONG_PTR d) { static int dummy; dummy++; return TRUE; }
static volatile LONG g_cont_calls; static volatile ULONG_PTR g_cont_arg;
static BOOL STDMETHODCALLTYPE cont_count_true(ULONG_PTR d) { InterlockedIncrement(&g_cont_calls); g_cont_arg = d; return TRUE; }
static BOOL STDMETHODCALLTYPE cont_count_false(ULONG_PTR d) { InterlockedIncrement(&g_cont_calls); g_cont_arg = d; return FALSE; }

TEST(o2cache_setdata_aspect_not_cached_is_blank) {
    cache_t c; STGMEDIUM m; HGLOBAL g; DWORD con; HRESULT hr; FORMATETC node = mkfe(0, DVASPECT_ICON, TYMED_MFPICT), f = mkfe(CF_METAFILEPICT, DVASPECT_CONTENT, TYMED_MFPICT);
    REQUIRE(U(CreateDataCache)); REQUIRE(U(OleMetafilePictFromIconAndLabel));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    EQ(IOleCache2_Cache(c.c2, &node, 0, &con), S_OK);
    g = mk_mfpict(); CHECK(g != NULL);
    memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g;
    hr = IOleCache2_SetData(c.c2, &f, &m, FALSE);
    EQ(hr, O2_OLE_E_BLANK);                          /* no cache node for DVASPECT_CONTENT */
    f.dwAspect = DVASPECT_ICON;
    hr = IOleCache2_SetData(c.c2, &f, &m, FALSE);
    EQ(hr, S_OK);
    U(ReleaseStgMedium)(&m);
    cache_close(&c);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache */
TEST(o2cache_draw_cached_and_blank_aspects) {
    cache_t c; STGMEDIUM m; HGLOBAL g; DWORD con; HRESULT hr; HDC dc; RECTL rc = { 0, 0, 100, 100 };
    FORMATETC node = mkfe(0, DVASPECT_ICON, TYMED_MFPICT), f = mkfe(CF_METAFILEPICT, DVASPECT_ICON, TYMED_MFPICT);
    REQUIRE(U(CreateDataCache)); REQUIRE(U(OleMetafilePictFromIconAndLabel)); REQUIRE(G(CreateCompatibleDC)); REQUIRE(G(DeleteDC));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    dc = G(CreateCompatibleDC)(NULL);
    EQ(IOleCache2_Cache(c.c2, &node, 0, &con), S_OK);
    g = mk_mfpict();
    memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = g;
    EQ(IOleCache2_SetData(c.c2, &f, &m, FALSE), S_OK);
    U(ReleaseStgMedium)(&m);
    g_cont_calls = 0;
    hr = IViewObject_Draw(c.vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &rc, NULL, cont_count_true, 0xdeadbeef);
    EQ(hr, S_OK);
    CHECK(g_cont_calls >= 1);
    EQ(g_cont_arg, 0xdeadbeef);
    hr = IViewObject_Draw(c.vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &rc, NULL, cont_count_true, 0xdeadbeef);
    EQ(hr, O2_OLE_E_BLANK);
    hr = IViewObject_Draw(c.vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0xdeadbeef);
    EQ(hr, S_OK);
    g_cont_calls = 0;
    hr = IViewObject_Draw(c.vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &rc, NULL, cont_count_false, 0xdeadbeef);
    EQ(hr, E_ABORT);                                  /* the continue callback aborted the draw */
    CHECK(g_cont_calls >= 1);
    G(DeleteDC)(dc);
    cache_close(&c);
}
TEST(o2cache_draw_empty_cache_is_blank) {
    cache_t c; HDC dc; RECTL rc = { 0, 0, 10, 10 }; HRESULT hr;
    REQUIRE(U(CreateDataCache)); REQUIRE(G(CreateCompatibleDC)); REQUIRE(G(DeleteDC));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.vo) return;
    dc = G(CreateCompatibleDC)(NULL);
    hr = IViewObject_Draw(c.vo, DVASPECT_CONTENT, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0);
    EQ(hr, O2_OLE_E_BLANK);
    hr = IViewObject_Draw(c.vo, DVASPECT_ICON, -1, NULL, NULL, NULL, dc, &rc, NULL, NULL, 0);
    EQ(hr, O2_OLE_E_BLANK);
    G(DeleteDC)(dc);
    cache_close(&c);
}
TEST(o2cache_freeze_unfreeze) {
    cache_t c; STGMEDIUM m; DWORD con, fz = 0; HRESULT hr; FORMATETC node = mkfe(0, DVASPECT_ICON, TYMED_MFPICT), f = mkfe(CF_METAFILEPICT, DVASPECT_ICON, TYMED_MFPICT);
    REQUIRE(U(CreateDataCache)); REQUIRE(U(OleMetafilePictFromIconAndLabel));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.c2) return;
    EQ(IOleCache2_Cache(c.c2, &node, 0, &con), S_OK);
    memset(&m, 0, sizeof m); m.tymed = TYMED_MFPICT; m.hMetaFilePict = mk_mfpict();
    EQ(IOleCache2_SetData(c.c2, &f, &m, FALSE), S_OK);
    U(ReleaseStgMedium)(&m);
    hr = IViewObject_Freeze(c.vo, DVASPECT_ICON, -1, NULL, &fz);
    EQ(hr, S_OK);
    hr = IViewObject_Freeze(c.vo, DVASPECT_CONTENT, -1, NULL, &fz);
    EQ(hr, O2_OLE_E_BLANK);
    cache_close(&c);
}
TEST(o2cache_viewobject_setadvise_getadvise) {
    cache_t c; O2Sink s; HRESULT hr; DWORD asp = 0, advf = 0; IAdviseSink *got = NULL;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    if (!c.vo) return;
    os_init(&s);
    hr = IViewObject_SetAdvise(c.vo, DVASPECT_ICON, ADVF_PRIMEFIRST, &s.i);
    EQ(hr, S_OK);
    EQ(s.ref, 2);
    hr = IViewObject_GetAdvise(c.vo, &asp, &advf, &got);
    EQ(hr, S_OK);
    EQ(asp, DVASPECT_ICON); EQ(advf, ADVF_PRIMEFIRST); EQ(got, &s.i);
    if (got) IAdviseSink_Release(got);
    hr = IViewObject_SetAdvise(c.vo, DVASPECT_ICON, 0, NULL);       /* removing the sink */
    EQ(hr, S_OK);
    EQ(s.ref, 1);
    cache_close(&c);
}

/* ================================================================================================
 * IPersistStorage
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache */
TEST(o2cache_persist_initnew_save_cycle) {
    cache_t c; IStorage *stg; FORMATETC node = mkfe(0, DVASPECT_ICON, TYMED_MFPICT); DWORD con;
    REQUIRE(U(CreateDataCache));
    stg = o2_mkstg(); CHECK(stg != NULL);
    if (!stg) return;
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IPersistStorage_InitNew(c.ps, stg), S_OK);
    EQ(IPersistStorage_IsDirty(c.ps), S_OK);               /* a freshly initialised cache is dirty */
    EQ(IOleCache2_Cache(c.c2, &node, 0, &con), S_OK);
    EQ(IPersistStorage_Save(c.ps, stg, TRUE), S_OK);
    EQ(IPersistStorage_SaveCompleted(c.ps, NULL), S_OK);
    EQ(IPersistStorage_IsDirty(c.ps), S_FALSE);            /* clean after the save */
    cache_close(&c);
    IStorage_Release(stg);
}
TEST(o2cache_persist_initnew_twice) {
    cache_t c; IStorage *stg; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IPersistStorage_InitNew(c.ps, stg), S_OK);
    hr = IPersistStorage_InitNew(c.ps, stg);
    t_note("second InitNew -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == CO_E_ALREADYINITIALIZED || hr == S_OK || FAILED(hr));
    cache_close(&c);
    IStorage_Release(stg);
}
TEST(o2cache_persist_load_empty_storage) {
    cache_t c; IStorage *stg; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    stg = o2_mkstg(); if (!stg) { t_skip("no storage"); return; }
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    hr = IPersistStorage_Load(c.ps, stg);
    EQ(hr, S_OK);                          /* an empty storage has no presentations: nothing cached, not an error */
    EQ(IPersistStorage_IsDirty(c.ps), S_FALSE);
    cache_close(&c);
    IStorage_Release(stg);
}
TEST(o2cache_persist_hands_off_and_save_completed) {
    cache_t c; IStorage *stg, *stg2;
    REQUIRE(U(CreateDataCache));
    stg = o2_mkstg(); stg2 = o2_mkstg(); if (!stg || !stg2) { t_skip("no storage"); return; }
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IPersistStorage_InitNew(c.ps, stg), S_OK);
    EQ(IPersistStorage_HandsOffStorage(c.ps), S_OK);
    EQ(IPersistStorage_SaveCompleted(c.ps, stg2), S_OK);      /* a new storage is accepted after HandsOff */
    cache_close(&c);
    IStorage_Release(stg); IStorage_Release(stg2);
}

typedef struct { char names[8][40]; int n; } stream_list;
static void list_streams(IStorage *stg, stream_list *out) {
    IEnumSTATSTG *e = NULL; STATSTG st; out->n = 0;
    if (FAILED(IStorage_EnumElements(stg, 0, NULL, 0, &e)) || !e) return;
    while (IEnumSTATSTG_Next(e, 1, &st, NULL) == S_OK) {
        if (out->n < 8) WideCharToMultiByte(CP_ACP, 0, st.pwcsName, -1, out->names[out->n], 40, NULL, NULL);
        out->n++;
        U(CoTaskMemFree)(st.pwcsName);
    }
    IEnumSTATSTG_Release(e);
}
static int has_stream(const stream_list *l, const char *name) { int i; for (i = 0; i < l->n && i < 8; i++) if (!strcmp(l->names[i], name)) return 1; return 0; }

/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache_save_data */
TEST(o2cache_save_presentation_streams_without_data) {
    cache_t c; IStorage *doc; stream_list l; DWORD con; STATSTG st; CLSID before = O2_CLSID_A;
    FORMATETC f1 = mkfe(CF_DIB, DVASPECT_CONTENT, TYMED_HGLOBAL), f2 = mkfe(CF_METAFILEPICT, DVASPECT_CONTENT, TYMED_MFPICT), f3 = mkfe(CF_ENHMETAFILE, DVASPECT_CONTENT, TYMED_ENHMF);
    REQUIRE(U(CreateDataCache));
    doc = o2_mkstg(); if (!doc) { t_skip("no storage"); return; }
    EQ(IStorage_SetClass(doc, &before), S_OK);
    EQ(cache_open(&c, &O2_CLSID_B), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f1, 0, &con), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f2, 0, &con), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f3, 0, &con), S_OK);
    EQ(IPersistStorage_Save(c.ps, doc, FALSE), S_OK);
    EQ(IPersistStorage_IsDirty(c.ps), S_OK);               /* saving into a different storage leaves the cache dirty */
    list_streams(doc, &l);
    EQ(l.n, 3);
    CHECK(has_stream(&l, "\2OlePres000")); CHECK(has_stream(&l, "\2OlePres001")); CHECK(has_stream(&l, "\2OlePres002"));
    EQ(IStorage_Stat(doc, &st, STATFLAG_NONAME), S_OK);
    CHECK(!memcmp(&st.clsid, &before, sizeof before));     /* the cache does not change the storage class */
    cache_close(&c);
    IStorage_Release(doc);
}
TEST(o2cache_save_static_picture_classes_use_contents_stream) {
    static const struct { const GUID *clsid; CLIPFORMAT cf; DWORD tymed; const char *n; } t[] = {
        { &O2_CLSID_PICTURE_DIB, CF_DIB, TYMED_HGLOBAL, "Picture_Dib" },
        { &O2_CLSID_PICTURE_META, CF_METAFILEPICT, TYMED_MFPICT, "Picture_Metafile" },
        { &O2_CLSID_PICTURE_ENHMETA, CF_ENHMETAFILE, TYMED_ENHMF, "Picture_EnhMetafile" },
    };
    int i;
    REQUIRE(U(CreateDataCache));
    for (i = 0; i < O2_N(t); i++) {
        cache_t c; IStorage *doc; stream_list l; DWORD con; FORMATETC f = mkfe(t[i].cf, DVASPECT_CONTENT, t[i].tymed);
        doc = o2_mkstg(); if (!doc) { t_skip("no storage"); return; }
        EQ(cache_open(&c, t[i].clsid), S_OK);
        CHECK(SUCCEEDED(IOleCache2_Cache(c.c2, &f, 0, &con)));   /* the class already seeds this entry: S_OK or CACHE_S_SAMECACHE */
        EQ(IPersistStorage_Save(c.ps, doc, FALSE), S_OK);
        list_streams(doc, &l);
        CHECKF(l.n == 1 && has_stream(&l, "CONTENTS"), "%s: %d streams, first '%s'", t[i].n, l.n, l.n ? l.names[0] : "");
        cache_close(&c);
        IStorage_Release(doc);
    }
}
TEST(o2cache_save_empty_cache_writes_no_presentation) {
    cache_t c; IStorage *doc; stream_list l;
    REQUIRE(U(CreateDataCache));
    doc = o2_mkstg(); if (!doc) { t_skip("no storage"); return; }
    EQ(cache_open(&c, &O2_CLSID_B), S_OK);
    EQ(IPersistStorage_Save(c.ps, doc, FALSE), S_OK);
    list_streams(doc, &l);
    EQ(l.n, 0);
    cache_close(&c);
    IStorage_Release(doc);
}

/* ================================================================================================
 * IOleCacheControl and the cache's IDataObject
 * ============================================================================================== */
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) test_data_cache */
TEST(o2cache_cachecontrol_onrun_advises_source) {
    cache_t c; O2Data d; DWORD con; CLIPFORMAT cf1, cf2, cf3;
    FORMATETC f0 = mkfe(CF_METAFILEPICT, DVASPECT_CONTENT, TYMED_MFPICT), f1, f2, f3;
    REQUIRE(U(CreateDataCache));
    cf1 = (CLIPFORMAT)RegisterClipboardFormatA("olet_o2c_1"); cf2 = (CLIPFORMAT)RegisterClipboardFormatA("olet_o2c_2"); cf3 = (CLIPFORMAT)RegisterClipboardFormatA("olet_o2c_3");
    f1 = mkfe(cf1, DVASPECT_CONTENT, TYMED_HGLOBAL); f2 = mkfe(cf2, DVASPECT_CONTENT, TYMED_HGLOBAL); f3 = mkfe(cf3, DVASPECT_CONTENT, TYMED_HGLOBAL);
    od_init(&d);
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f0, 0, &con), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f1, 0, &con), O2_CACHE_S_FORMATETC_NOTSUPPORTED);
    EQ(IOleCache2_Cache(c.c2, &f2, ADVF_PRIMEFIRST, &con), O2_CACHE_S_FORMATETC_NOTSUPPORTED);
    EQ(d.dadvise, 0);                                           /* nothing is advised before the object runs */
    EQ(IOleCacheControl_OnRun(c.cc, &d.i), S_OK);
    EQ(d.dadvise, 3);                                           /* one DAdvise per cache node */
    EQ(IOleCache2_Cache(c.c2, &f3, 0, &con), O2_CACHE_S_FORMATETC_NOTSUPPORTED);
    EQ(d.dadvise, 4);                                           /* nodes added while running are advised at once */
    EQ(d.dunadvise, 0);
    cache_close(&c);
    EQ(d.dunadvise, 4);                                         /* destroying the cache unadvises everything */
    EQ(d.ref, 1);
}
TEST(o2cache_cachecontrol_onstop_unadvises) {
    cache_t c; O2Data d; DWORD con; FORMATETC f0 = mkfe(CF_BITMAP, DVASPECT_CONTENT, TYMED_GDI);
    REQUIRE(U(CreateDataCache));
    od_init(&d);
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f0, 0, &con), S_OK);
    EQ(IOleCacheControl_OnRun(c.cc, &d.i), S_OK);
    EQ(d.dadvise, 1);
    EQ(IOleCacheControl_OnStop(c.cc), S_OK);
    EQ(d.dunadvise, 1);
    EQ(d.ref, 1);
    cache_close(&c);
    EQ(d.dunadvise, 1);
}
TEST(o2cache_dataobject_getdata_before_data_is_blank) {
    cache_t c; DWORD con; STGMEDIUM m; HRESULT hr; CLIPFORMAT cf = (CLIPFORMAT)RegisterClipboardFormatA("olet_o2c_blank");
    FORMATETC f0 = mkfe(CF_METAFILEPICT, DVASPECT_CONTENT, TYMED_MFPICT), f1 = mkfe(cf, DVASPECT_CONTENT, TYMED_HGLOBAL);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f0, 0, &con), S_OK);
    memset(&m, 0, sizeof m);
    hr = IDataObject_GetData(c.dobj, &f0, &m);
    EQ(hr, O2_OLE_E_BLANK);
    EQ(IOleCache2_Cache(c.c2, &f1, 0, &con), O2_CACHE_S_FORMATETC_NOTSUPPORTED);
    hr = IDataObject_GetData(c.dobj, &f1, &m);
    EQ(hr, O2_OLE_E_BLANK);
    cache_close(&c);
}
TEST(o2cache_dataobject_getdata_not_cached_format) {
    cache_t c; STGMEDIUM m; HRESULT hr; FORMATETC f = mkfe(CF_TEXT, DVASPECT_CONTENT, TYMED_HGLOBAL);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    memset(&m, 0, sizeof m);
    hr = IDataObject_GetData(c.dobj, &f, &m);
    t_note("GetData(CF_TEXT) on an empty cache -> 0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    hr = IDataObject_QueryGetData(c.dobj, &f);
    t_note("QueryGetData(CF_TEXT) on an empty cache -> 0x%08lx", (unsigned long)hr);
    CHECK(hr != S_OK);
    cache_close(&c);
}
TEST(o2cache_dataobject_enumformatetc_empty) {
    cache_t c; IEnumFORMATETC *e = NULL; FORMATETC f; ULONG got = 99; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    hr = IDataObject_EnumFormatEtc(c.dobj, DATADIR_GET, &e);
    t_note("EnumFormatEtc(GET) on an empty cache -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && e) {
        EQ(IEnumFORMATETC_Next(e, 1, &f, &got), S_FALSE);
        EQ(got, 0);
        IEnumFORMATETC_Release(e);
    }
    cache_close(&c);
}
TEST(o2cache_dataobject_dadvise) {
    cache_t c; O2Sink s; DWORD con = 0; HRESULT hr; FORMATETC f = mkfe(CF_TEXT, DVASPECT_CONTENT, TYMED_NULL);
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    os_init(&s);
    hr = IDataObject_DAdvise(c.dobj, &f, 0, &s.i, &con);
    t_note("cache IDataObject::DAdvise -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) { EQ(IDataObject_DUnadvise(c.dobj, con), S_OK); EQ(s.ref, 1); }
    else EQ(s.ref, 1);
    cache_close(&c);
}
TEST(o2cache_initcache_without_matching_data) {
    cache_t c; O2Data d; DWORD con; FORMATETC f = mkfe(CF_DIB, DVASPECT_CONTENT, TYMED_HGLOBAL); HRESULT hr;
    REQUIRE(U(CreateDataCache));
    od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "text");
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f, 0, &con), S_OK);
    hr = IOleCache2_InitCache(c.c2, &d.i);
    EQ(hr, O2_CACHE_E_NOCACHE_UPDATED);
    EQ(d.ref, 1);
    cache_close(&c);
}
TEST(o2cache_updatecache_without_running_object) {
    cache_t c; O2Data d; DWORD con; FORMATETC f = mkfe(CF_DIB, DVASPECT_CONTENT, TYMED_HGLOBAL); HRESULT hr;
    REQUIRE(U(CreateDataCache));
    od_init(&d);
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    EQ(IOleCache2_Cache(c.c2, &f, 0, &con), S_OK);
    hr = IOleCache2_UpdateCache(c.c2, &d.i, UPDFCACHE_ALL, NULL);
    t_note("UpdateCache(no matching data) -> 0x%08lx", (unsigned long)hr);
    EQ(d.ref, 1);
    cache_close(&c);
}
TEST(o2cache_discardcache_without_storage) {
    cache_t c; HRESULT hr;
    REQUIRE(U(CreateDataCache));
    EQ(cache_open(&c, &O2_GUID_NULL), S_OK);
    hr = IOleCache2_DiscardCache(c.c2, DISCARDCACHE_NOSAVE);
    t_note("DiscardCache(NOSAVE) on an empty cache -> 0x%08lx", (unsigned long)hr);
    CHECK(hr == S_OK || FAILED(hr));
    cache_close(&c);
}
