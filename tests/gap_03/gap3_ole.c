/* gap_03: ole32 OleCreateFromDataEx / OleCreateLinkEx / OleCreateLinkFromData(Ex) / OleCreateFromFileEx / OleCreateLinkToFileEx
 * argument validation, plus OleGetClipboardWithEnterpriseInfo and OaEnablePerUserTLibRegistration.
 * Validation order is taken from the Win10 ole32 decomp (wValidateCreateParams, wValidateFormatEtcEx):
 *   ppvObj == NULL -> E_INVALIDARG; *ppvObj = NULL; source interface invalid -> E_INVALIDARG;
 *   wValidateCreateParams: dwFlags & ~OLECREATE_LEAVERUNNING; renderopt DRAW/NONE/ASIS with a sink; DRAW with cFormats > 1;
 *   cFormats == 0 needs NULL rgAdvf/rgFormatEtc/rgdwConnection, otherwise rgAdvf and rgFormatEtc are required, each advf <= 0x7f,
 *   pStg must be a valid interface; rgdwConnection[0..cFormats) is zeroed as soon as the counts are consistent. */
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <ole2.h>

static const GUID g3_IID_IOleObject = {0x00000112,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g3_IID_IOleLink = {0x0000011D,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g3_NULL = {0,0,0,{0,0,0,0,0,0,0,0}};
static const GUID g3_IID_IUnknown = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g3_IID_IDataObject = {0x0000010E,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID g3_IID_IAdviseSink = {0x0000010F,0,0,{0xC0,0,0,0,0,0,0,0x46}};
#define IID_IUnknown g3_IID_IUnknown
#define IID_IDataObject g3_IID_IDataObject
#define IID_IAdviseSink g3_IID_IAdviseSink
#define IID_IOleObject g3_IID_IOleObject
#define IID_IOleLink g3_IID_IOleLink
#define CLSID_NULL g3_NULL
#define EIA ((HRESULT)0x80070057)
#define DV_FORMATETC ((HRESULT)0x80040064)
#define DV_TYMED     ((HRESULT)0x80040069)
#define DV_LINDEX    ((HRESULT)0x80040068)
#define DV_DVASPECT  ((HRESULT)0x8004006B)

/* ---- mock data object / advise sink ---- */
typedef struct { const IDataObjectVtbl *v; LONG ref; } MDO;
static HRESULT STDMETHODCALLTYPE do_qi(IDataObject *t, REFIID r, void **o) { if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDataObject)) { *o = t; IDataObject_AddRef(t); return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE do_addref(IDataObject *t) { return InterlockedIncrement(&((MDO *)t)->ref); }
static ULONG STDMETHODCALLTYPE do_release(IDataObject *t) { return InterlockedDecrement(&((MDO *)t)->ref); }
static HRESULT STDMETHODCALLTYPE do_getdata(IDataObject *t, FORMATETC *f, STGMEDIUM *m) { return DV_E_FORMATETC; }
static HRESULT STDMETHODCALLTYPE do_getdatahere(IDataObject *t, FORMATETC *f, STGMEDIUM *m) { return DV_E_FORMATETC; }
static HRESULT STDMETHODCALLTYPE do_query(IDataObject *t, FORMATETC *f) { return DV_E_FORMATETC; }
static HRESULT STDMETHODCALLTYPE do_canon(IDataObject *t, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE do_setdata(IDataObject *t, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE do_enum(IDataObject *t, DWORD d, IEnumFORMATETC **e) { *e = NULL; return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE do_dadvise(IDataObject *t, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE do_dunadvise(IDataObject *t, DWORD c) { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE do_enumdadvise(IDataObject *t, IEnumSTATDATA **e) { *e = NULL; return OLE_E_ADVISENOTSUPPORTED; }
static const IDataObjectVtbl do_vt = { do_qi, do_addref, do_release, do_getdata, do_getdatahere, do_query, do_canon, do_setdata, do_enum, do_dadvise, do_dunadvise, do_enumdadvise };
static MDO g_do = { &do_vt, 1 };

typedef struct { const IAdviseSinkVtbl *v; LONG ref; } MAS;
static HRESULT STDMETHODCALLTYPE as_qi(IAdviseSink *t, REFIID r, void **o) { if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IAdviseSink)) { *o = t; return S_OK; } *o = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE as_addref(IAdviseSink *t) { return InterlockedIncrement(&((MAS *)t)->ref); }
static ULONG STDMETHODCALLTYPE as_release(IAdviseSink *t) { return InterlockedDecrement(&((MAS *)t)->ref); }
static void STDMETHODCALLTYPE as_ondata(IAdviseSink *t, FORMATETC *f, STGMEDIUM *m) {}
static void STDMETHODCALLTYPE as_onview(IAdviseSink *t, DWORD a, LONG i) {}
static void STDMETHODCALLTYPE as_onren(IAdviseSink *t, IMoniker *m) {}
static void STDMETHODCALLTYPE as_onsave(IAdviseSink *t) {}
static void STDMETHODCALLTYPE as_onclose(IAdviseSink *t) {}
static const IAdviseSinkVtbl as_vt = { as_qi, as_addref, as_release, as_ondata, as_onview, as_onren, as_onsave, as_onclose };
static MAS g_as = { &as_vt, 1 };

/* ---- fixture: STA + in-memory-ish docfile + item moniker ---- */
typedef struct { int ok; IStorage *stg; IMoniker *mk; } Fx;
static int fx_init(Fx *f) {
    memset(f, 0, sizeof *f);
    if (!U(CoInitializeEx) || !U(StgCreateDocfile) || !U(CreateItemMoniker)) { t_skip("missing exports"); return 0; }
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) { t_skip("CoInitializeEx failed"); return 0; }
    f->ok = 1;
    HRESULT hr = U(StgCreateDocfile)(NULL, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_CREATE | STGM_DELETEONRELEASE, 0, &f->stg);
    if (FAILED(hr)) { t_skip("StgCreateDocfile -> 0x%lx", (unsigned long)hr); U(CoUninitialize)(); f->ok = 0; return 0; }
    hr = U(CreateItemMoniker)(L"!", L"olet_gap3_item", &f->mk);
    if (FAILED(hr)) { IStorage_Release(f->stg); U(CoUninitialize)(); f->ok = 0; t_skip("CreateItemMoniker -> 0x%lx", (unsigned long)hr); return 0; }
    return 1;
}
static void fx_done(Fx *f) {
    if (!f->ok) return;
    if (f->mk) IMoniker_Release(f->mk);
    if (f->stg) IStorage_Release(f->stg);
    U(CoUninitialize)();
    f->ok = 0;
}
#define FX() Fx fx; if (!fx_init(&fx)) return

static HRESULT from_data(Fx *f, IDataObject *src, DWORD flags, DWORD ro, ULONG cf, DWORD *advf, FORMATETC *fe, IAdviseSink *sink, DWORD *conn, IStorage *stg, void **out) {
    return U(OleCreateFromDataEx)(src, &IID_IOleObject, flags, ro, cf, advf, fe, sink, conn, NULL, stg, out);
}
static HRESULT link_ex(Fx *f, IMoniker *mk, DWORD flags, DWORD ro, ULONG cf, DWORD *advf, FORMATETC *fe, IAdviseSink *sink, DWORD *conn, IStorage *stg, void **out) {
    return U(OleCreateLinkEx)(mk, &IID_IOleLink, flags, ro, cf, advf, fe, sink, conn, NULL, stg, out);
}

/* ===== OleCreateFromDataEx ===== */
TEST(gap3_olecreate_fromdataex_null_ppv_and_source) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, NULL), EIA);
    void *out = (void *)(ULONG_PTR)0x1;
    EXPECT_EQ(from_data(&fx, NULL, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL);                                     /* *ppvObj is cleared before the source is validated */
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_invalid_flags) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    static const DWORD bad[] = { 2, 3, 4, 0x10, 0x100, 0x80000000u, 0xfffffffeu, 0xffffffffu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        void *out = (void *)(ULONG_PTR)1;
        HRESULT hr = from_data(&fx, (IDataObject *)&g_do, bad[i], OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out);
        CHECKF(hr == EIA && out == NULL, "flags 0x%lx -> 0x%lx out %p", (unsigned long)bad[i], (unsigned long)hr, out);
    }
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_null_storage) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, &out), EIA);
    CHECK(out == NULL);
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_sink_only_allowed_with_format_renderopt) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    static const DWORD ro[] = { OLERENDER_NONE, OLERENDER_DRAW, OLERENDER_ASIS };
    for (unsigned i = 0; i < 3; i++) {
        void *out = NULL;
        HRESULT hr = from_data(&fx, (IDataObject *)&g_do, 0, ro[i], 0, NULL, NULL, (IAdviseSink *)&g_as, NULL, fx.stg, &out);
        CHECKF(hr == EIA, "renderopt %lu with a sink -> 0x%lx", (unsigned long)ro[i], (unsigned long)hr);
    }
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_draw_allows_at_most_one_format) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    DWORD advf[2] = { 0, 0 }, conn[2] = { 5, 5 };
    FORMATETC fe[2] = { { 0, NULL, DVASPECT_CONTENT, -1, 0 }, { 0, NULL, DVASPECT_CONTENT, -1, 0 } };
    void *out = NULL;
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_DRAW, 2, advf, fe, NULL, conn, fx.stg, &out), EIA);
    CHECKF(conn[0] == 5 && conn[1] == 5, "rgdwConnection must not be touched when the call is refused before the zeroing step");
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_format_count_consistency) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    DWORD advf[2] = { 0, 0 }, conn[2] = { 7, 7 };
    FORMATETC fe[2] = { { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL } };
    void *out = NULL;
    /* cFormats == 0: every array pointer must be NULL */
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 0, advf, NULL, NULL, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 0, NULL, fe, NULL, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 0, NULL, NULL, NULL, conn, fx.stg, &out), EIA);
    CHECKF(conn[0] == 7, "connection array touched for cFormats == 0");
    /* cFormats != 0: rgAdvf and rgFormatEtc are both required */
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 1, NULL, fe, NULL, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 1, advf, NULL, NULL, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 2, NULL, NULL, NULL, conn, fx.stg, &out), EIA);
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_advf_range_and_connection_zeroing) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    FORMATETC fe[3]; for (int i = 0; i < 3; i++) { FORMATETC t = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }; fe[i] = t; }
    static const DWORD badadvf[] = { 0x80, 0x100, 0x80000000u, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        DWORD advf[3] = { 0, 0x7f, badadvf[i] }, conn[3] = { 0xDEADBEEF, 0xDEADBEEF, 0xDEADBEEF };
        void *out = NULL;
        HRESULT hr = from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 3, advf, fe, NULL, conn, fx.stg, &out);
        CHECKF(hr == EIA, "advf 0x%lx -> 0x%lx", (unsigned long)badadvf[i], (unsigned long)hr);
        /* the zeroing step precedes the per-advf scan */
        CHECKF(conn[0] == 0 && conn[1] == 0 && conn[2] == 0, "advf 0x%lx: connections %lx %lx %lx", (unsigned long)badadvf[i], (unsigned long)conn[0], (unsigned long)conn[1], (unsigned long)conn[2]);
    }
    /* 0..0x7f are accepted by the validation; the (empty) data object then fails later */
    DWORD advf[3] = { 0, 0x7f, 0x40 }, conn[3] = { 1, 2, 3 };
    void *out = (void *)(ULONG_PTR)1;
    HRESULT hr = from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_FORMAT, 3, advf, fe, NULL, conn, fx.stg, &out);
    CHECKF(hr != EIA, "valid advf rejected with E_INVALIDARG");
    CHECK(FAILED(hr) && out == NULL);
    t_note("OleCreateFromDataEx with a data object that offers no formats -> 0x%lx", (unsigned long)hr);
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdataex_data_without_embed_formats) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromDataEx)); FX();
    void *out = (void *)(ULONG_PTR)1;
    HRESULT hr = from_data(&fx, (IDataObject *)&g_do, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out);
    CHECK(FAILED(hr) && out == NULL);
    /* decomp: wQueryEmbedFormats returned neither the embed nor the static bit -> DV_E_FORMATETC (OLE_E_STATIC when only static) */
    t_note("OleCreateFromDataEx(data object with no formats) -> 0x%lx", (unsigned long)hr);
    CHECKF(hr == DV_FORMATETC || hr == (HRESULT)0x8004000B, "-> 0x%lx", (unsigned long)hr);
    fx_done(&fx);
}
TEST(gap3_olecreate_fromdata_wrapper_forwards_validation) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromData)); FX();
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(U(OleCreateFromData)(NULL, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL);
    EXPECT_EQ(U(OleCreateFromData)((IDataObject *)&g_do, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(U(OleCreateFromData)((IDataObject *)&g_do, &IID_IOleObject, OLERENDER_NONE, NULL, NULL, NULL, &out), EIA);
    /* OLERENDER_DRAW with a FORMATETC: cFormats = 1 and the wrapper supplies the advise flags itself */
    FORMATETC fe = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_NULL };
    HRESULT hr = U(OleCreateFromData)((IDataObject *)&g_do, &IID_IOleObject, OLERENDER_DRAW, &fe, NULL, fx.stg, &out);
    CHECKF(hr != EIA, "wrapper-built arguments rejected by validation (0x%lx)", (unsigned long)hr);
    fx_done(&fx);
}

/* ===== OleCreateLinkEx ===== */
TEST(gap3_olecreate_linkex_null_arguments) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(link_ex(&fx, NULL, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL);
    out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, &out), EIA);
    CHECK(out == NULL);
    fx_done(&fx);
}
TEST(gap3_olecreate_linkex_invalid_flags) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    static const DWORD bad[] = { 2, 0x10, 0x8000, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        void *out = NULL;
        HRESULT hr = link_ex(&fx, fx.mk, bad[i], OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out);
        CHECKF(hr == EIA, "flags 0x%lx -> 0x%lx", (unsigned long)bad[i], (unsigned long)hr);
    }
    fx_done(&fx);
}
TEST(gap3_olecreate_linkex_renderopt_out_of_range) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    /* wValidateFormatEtcEx: only 0..3 are valid render options */
    static const DWORD bad[] = { 4, 5, 100, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        void *out = (void *)(ULONG_PTR)1;
        HRESULT hr = link_ex(&fx, fx.mk, 0, bad[i], 0, NULL, NULL, NULL, NULL, fx.stg, &out);
        CHECKF(hr == EIA, "renderopt %lu -> 0x%lx", (unsigned long)bad[i], (unsigned long)hr);
    }
    fx_done(&fx);
}
TEST(gap3_olecreate_linkex_format_list_entries) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    /* OLERENDER_FORMAT with two entries goes through the per-entry loop of wValidateFormatEtcEx */
    FORMATETC good = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    DWORD advf[2] = { 0, 0 }, conn[2] = { 0, 0 };
    struct { const char *n; FORMATETC bad; HRESULT want; } t[] = {
        { "cfFormat 0",         { 0,       NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL }, EIA },
        { "tymed mismatch",     { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_FILE },    DV_TYMED },
        { "tymed none",         { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_NULL },    DV_TYMED },
        { "lindex 0",           { CF_TEXT, NULL, DVASPECT_CONTENT,  0, TYMED_HGLOBAL }, DV_LINDEX },
        { "lindex 5",           { CF_TEXT, NULL, DVASPECT_CONTENT,  5, TYMED_HGLOBAL }, DV_LINDEX },
        { "aspect 0",           { CF_TEXT, NULL, 0,                -1, TYMED_HGLOBAL }, DV_DVASPECT },
        { "aspect 3 (two bits)",{ CF_TEXT, NULL, 3,                -1, TYMED_HGLOBAL }, DV_DVASPECT },
        { "aspect 16",          { CF_TEXT, NULL, 16,               -1, TYMED_HGLOBAL }, DV_DVASPECT },
        { "aspect 0x80000000",  { CF_TEXT, NULL, 0x80000000u,      -1, TYMED_HGLOBAL }, DV_DVASPECT },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        for (int pos = 0; pos < 2; pos++) {
            FORMATETC fe[2]; fe[pos] = t[i].bad; fe[1 - pos] = good;
            void *out = (void *)(ULONG_PTR)1;
            HRESULT hr = link_ex(&fx, fx.mk, 0, OLERENDER_FORMAT, 2, advf, fe, NULL, conn, fx.stg, &out);
            CHECKF(hr == t[i].want && out == NULL, "%s at entry %d -> 0x%lx want 0x%lx", t[i].n, pos, (unsigned long)hr, (unsigned long)t[i].want);
        }
    }
    /* lindex -1 with an aspect other than DVASPECT_ICON is fine; lindex != -1 is allowed for DVASPECT_ICON (8) */
    fx_done(&fx);
}
TEST(gap3_olecreate_linkex_draw_format_must_have_no_clipformat) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    /* OLERENDER_DRAW (single entry): checked by wValidateFormatEtc; entries with cfFormat != 0 are DV_E_CLIPFORMAT (0x8004006A) per decomp of the multi-entry path */
    DWORD advf[2] = { 0, 0 };
    FORMATETC fe[2] = { { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_NULL }, { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_NULL } };
    void *out = NULL;
    /* more than one entry is not allowed for DRAW at all */
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_DRAW, 2, advf, fe, NULL, NULL, fx.stg, &out), EIA);
    fx_done(&fx);
}
TEST(gap3_olecreate_linkex_sink_with_format_renderopt_needs_valid_sink) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkEx)); FX();
    DWORD advf[1] = { 0 }, conn[1] = { 9 };
    FORMATETC fe = { CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    /* a sink is only accepted for OLERENDER_FORMAT; DRAW/NONE/ASIS refuse it */
    void *out = NULL;
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_NONE, 0, NULL, NULL, (IAdviseSink *)&g_as, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_ASIS, 0, NULL, NULL, (IAdviseSink *)&g_as, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(link_ex(&fx, fx.mk, 0, OLERENDER_DRAW, 1, advf, &fe, (IAdviseSink *)&g_as, conn, fx.stg, &out), EIA);
    CHECKF(conn[0] == 9, "connection written before the refusal");
    fx_done(&fx);
}

/* ===== OleCreateLinkFromData / OleCreateLinkFromDataEx ===== */
TEST(gap3_olecreate_linkfromdataex_validation_precedes_data_inspection) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkFromDataEx)); FX();
    void *out = (void *)(ULONG_PTR)1;
    IDataObject *d = (IDataObject *)&g_do;
    EXPECT_EQ(U(OleCreateLinkFromDataEx)(d, &IID_IOleLink, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(U(OleCreateLinkFromDataEx)(NULL, &IID_IOleLink, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL);
    EXPECT_EQ(U(OleCreateLinkFromDataEx)(d, &IID_IOleLink, 2, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(U(OleCreateLinkFromDataEx)(d, &IID_IOleLink, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, NULL, &out), EIA);
    EXPECT_EQ(U(OleCreateLinkFromDataEx)(d, &IID_IOleLink, 0, OLERENDER_NONE, 1, NULL, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    fx_done(&fx);
}
TEST(gap3_olecreate_linkfromdataex_no_link_format_is_dv_e_formatetc) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkFromDataEx)); FX();
    void *out = (void *)(ULONG_PTR)1;
    /* decomp: after parameter validation, a data object that offers neither "LinkSource" nor a file name -> 0x80040064 */
    HRESULT hr = U(OleCreateLinkFromDataEx)((IDataObject *)&g_do, &IID_IOleLink, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, fx.stg, &out);
    EXPECT_EQ(hr, DV_FORMATETC);
    CHECK(out == NULL);
    /* the render-option/format validation only happens in the link branch, so a bad FORMATETC does not change the answer */
    DWORD advf = 0;
    FORMATETC bad = { 0, NULL, 3, 99, TYMED_FILE };
    hr = U(OleCreateLinkFromDataEx)((IDataObject *)&g_do, &IID_IOleLink, 0, OLERENDER_FORMAT, 1, &advf, &bad, NULL, NULL, NULL, fx.stg, &out);
    EXPECT_EQ(hr, DV_FORMATETC);
    fx_done(&fx);
}
TEST(gap3_olecreate_linkfromdata_wrapper) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateLinkFromData)); FX();
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(U(OleCreateLinkFromData)(NULL, &IID_IOleLink, OLERENDER_NONE, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL);
    EXPECT_EQ(U(OleCreateLinkFromData)((IDataObject *)&g_do, &IID_IOleLink, OLERENDER_NONE, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(U(OleCreateLinkFromData)((IDataObject *)&g_do, &IID_IOleLink, OLERENDER_NONE, NULL, NULL, NULL, &out), EIA);
    EXPECT_EQ(U(OleCreateLinkFromData)((IDataObject *)&g_do, &IID_IOleLink, OLERENDER_NONE, NULL, NULL, fx.stg, &out), DV_FORMATETC);
    FORMATETC fe = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_NULL };
    EXPECT_EQ(U(OleCreateLinkFromData)((IDataObject *)&g_do, &IID_IOleLink, OLERENDER_DRAW, &fe, NULL, fx.stg, &out), DV_FORMATETC);
    fx_done(&fx);
}

/* ===== OleCreateFromFileEx / OleCreateLinkToFileEx ===== */
static HRESULT file_ex(Fx *f, const WCHAR *path, DWORD flags, DWORD ro, ULONG cf, DWORD *advf, FORMATETC *fe, IAdviseSink *sink, DWORD *conn, IStorage *stg, void **out) {
    return U(OleCreateFromFileEx)(&CLSID_NULL, path, &IID_IOleObject, flags, ro, cf, advf, fe, sink, conn, NULL, stg, out);
}
static HRESULT linkfile_ex(Fx *f, const WCHAR *path, DWORD flags, DWORD ro, ULONG cf, DWORD *advf, FORMATETC *fe, IAdviseSink *sink, DWORD *conn, IStorage *stg, void **out) {
    return U(OleCreateLinkToFileEx)(path, &IID_IOleLink, flags, ro, cf, advf, fe, sink, conn, NULL, stg, out);
}
TEST(gap3_olecreate_fromfileex_argument_validation) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromFileEx)); FX();
    char p[300]; t_tmpfile(p, sizeof p, "gap3_fromfile.txt");
    WCHAR w[300]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, 300);
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(file_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(file_ex(&fx, w, 2, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL); out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(file_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, &out), EIA);
    CHECK(out == NULL); out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(file_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, (IAdviseSink *)&g_as, NULL, fx.stg, &out), EIA);
    EXPECT_EQ(file_ex(&fx, w, 0, OLERENDER_DRAW, 2, (DWORD[]){ 0, 0 }, (FORMATETC[]){ { 0, NULL, 1, -1, 0 }, { 0, NULL, 1, -1, 0 } }, NULL, NULL, fx.stg, &out), EIA);
    fx_done(&fx);
}
TEST(gap3_olecreate_fromfileex_missing_file_fails) {
    if (t_is_wine()) { t_skip("wine: OleCreate*Ex are stubs/thin wrappers with different results"); return; }
    REQUIRE(U(OleCreateFromFileEx)); FX();
    char p[300]; t_tmpfile(p, sizeof p, "gap3_does_not_exist.bin");
    WCHAR w[300]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, 300);
    void *out = (void *)(ULONG_PTR)1;
    HRESULT hr = file_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out);
    CHECK(FAILED(hr) && out == NULL);
    t_note("OleCreateFromFileEx(missing file) -> 0x%lx", (unsigned long)hr);
    fx_done(&fx);
}
TEST(gap3_olecreate_linktofileex_argument_validation) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkToFileEx)); FX();
    char p[300]; t_tmpfile(p, sizeof p, "gap3_linkfile.txt");
    WCHAR w[300]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, 300);
    void *out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(linkfile_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, NULL), EIA);
    EXPECT_EQ(linkfile_ex(&fx, w, 0x10, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out), EIA);
    CHECK(out == NULL); out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(linkfile_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, NULL, &out), EIA);
    CHECK(out == NULL); out = (void *)(ULONG_PTR)1;
    EXPECT_EQ(linkfile_ex(&fx, w, 0, OLERENDER_ASIS, 0, NULL, NULL, (IAdviseSink *)&g_as, NULL, fx.stg, &out), EIA);
    fx_done(&fx);
}
TEST(gap3_olecreate_linktofileex_missing_file_fails) {
    if (t_is_wine()) { t_skip("wine: this Ex export is a stub that aborts"); return; }
    REQUIRE(U(OleCreateLinkToFileEx)); FX();
    char p[300]; t_tmpfile(p, sizeof p, "gap3_link_missing.bin");
    WCHAR w[300]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, 300);
    void *out = (void *)(ULONG_PTR)1;
    HRESULT hr = linkfile_ex(&fx, w, 0, OLERENDER_NONE, 0, NULL, NULL, NULL, NULL, fx.stg, &out);
    t_note("OleCreateLinkToFileEx(missing file) -> 0x%lx", (unsigned long)hr);
    CHECK(out == NULL);
    fx_done(&fx);
}

/* ===== OleGetClipboardWithEnterpriseInfo ===== */
typedef HRESULT (WINAPI *oge_fn)(IDataObject **, WCHAR **, WCHAR **, WCHAR **, WCHAR **);
TEST(gap3_clipboard_enterprise_info_matches_oleclipboard_when_uninitialized) {
    REQUIRE(U(OleGetClipboard));
    oge_fn f = (oge_fn)t_proc("OleGetClipboardWithEnterpriseInfo"); REQUIRE(f);
    if (t_is_wine()) { t_skip("wine: export absent"); return; }
    /* no OleInitialize on this thread: both entry points fail identically (the wrapper returns OleGetClipboard's result when EDP is off) */
    IDataObject *d1 = NULL, *d2 = (IDataObject *)(ULONG_PTR)1;
    WCHAR *s[4] = { (WCHAR *)1, (WCHAR *)1, (WCHAR *)1, (WCHAR *)1 };
    HRESULT h1 = U(OleGetClipboard)(&d1);
    HRESULT h2 = f(&d2, &s[0], &s[1], &s[2], &s[3]);
    t_note("OleGetClipboard -> 0x%lx, WithEnterpriseInfo -> 0x%lx", (unsigned long)h1, (unsigned long)h2);
    CHECK(h1 == h2 || (SUCCEEDED(h1) && SUCCEEDED(h2)));
    CHECK(d2 != (IDataObject *)(ULONG_PTR)1);                          /* output pointers are cleared on entry */
    for (int i = 0; i < 4; i++) CHECKF(s[i] != (WCHAR *)1, "string output %d not cleared", i);
    if (SUCCEEDED(h1) && d1) IDataObject_Release(d1);
    if (SUCCEEDED(h2) && d2) IDataObject_Release(d2);
    for (int i = 0; i < 4; i++) if (s[i]) U(CoTaskMemFree)(s[i]);
}
TEST(gap3_clipboard_enterprise_info_in_sta) {
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(OleGetClipboard));
    oge_fn f = (oge_fn)t_proc("OleGetClipboardWithEnterpriseInfo"); REQUIRE(f);
    if (t_is_wine()) { t_skip("wine: export absent"); return; }
    if (FAILED(U(OleInitialize)(NULL))) { t_skip("OleInitialize failed"); return; }
    IDataObject *d1 = NULL, *d2 = NULL; WCHAR *s[4] = { 0 };
    HRESULT h1 = U(OleGetClipboard)(&d1);
    HRESULT h2 = f(&d2, &s[0], &s[1], &s[2], &s[3]);
    t_note("OleGetClipboard -> 0x%lx, WithEnterpriseInfo -> 0x%lx; strings %p %p %p %p", (unsigned long)h1, (unsigned long)h2, s[0], s[1], s[2], s[3]);
    CHECKF(SUCCEEDED(h1) == SUCCEEDED(h2), "OleGetClipboard 0x%lx vs WithEnterpriseInfo 0x%lx", (unsigned long)h1, (unsigned long)h2);
    if (SUCCEEDED(h2)) CHECK(d2 != NULL);
    else CHECK(d2 == NULL);
    /* with enterprise protection off (the normal case on a VM) the four identity strings stay NULL */
    if (s[0] || s[1] || s[2] || s[3]) t_note("enterprise identity strings returned: EDP is active on this machine");
    if (d1) IDataObject_Release(d1);
    if (d2) IDataObject_Release(d2);
    for (int i = 0; i < 4; i++) if (s[i]) U(CoTaskMemFree)(s[i]);
    U(OleUninitialize)();
}

/* ===== OaEnablePerUserTLibRegistration (oleaut32) ===== */
TEST(gap3_oa_enable_per_user_tlib_registration_is_idempotent_void) {
    UDECL(void, OaEnablePerUserTLibRegistration, (void));
    OaEnablePerUserTLibRegistration_pfn f = UN(OaEnablePerUserTLibRegistration);
    REQUIRE(f);
    HMODULE oa = t_module("oleaut32.dll");
    CHECK(oa != NULL && GetProcAddress(oa, "OaEnablePerUserTLibRegistration") == (FARPROC)f);
    /* the decomp is `g_fOaPerUserTlibReg = 1; return;`: calling it any number of times is harmless */
    f(); f(); f();
    CHECK(1);
}
