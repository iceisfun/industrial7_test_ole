/* gap_06: OleCreateLinkToFile, OleCreatePropertyFrame(Indirect), OleConvert*OLESTREAM* argument-order checks.
 * Decomp facts (Win10 ole32):
 *  OleCreateLinkToFile -> OleCreateLinkToFileEx(dwFlags=0, cFormats=(pFormatEtc!=NULL), rgAdvf={ADVF_PRIMEFIRST}):
 *    ppvObj==NULL => E_INVALIDARG; *ppvObj is zeroed; wValidateCreateParams (all E_INVALIDARG): dwFlags has bits other than 1;
 *    OLERENDER_DRAW with cFormats>1; pAdviseSink non-NULL unless renderopt==OLERENDER_FORMAT (or DRAW with cFormats<=1... see table);
 *    cFormats==0 with non-NULL rgAdvf/rgFormatEtc/rgdwConn; cFormats!=0 with NULL rgAdvf/rgFormatEtc; any advf bit outside 0x7f;
 *    invalid pStg; invalid pAdviseSink/pClientSite. When validation passes the file is resolved: a missing file => STG_E_FILENOTFOUND.
 *  OleConvertOLESTREAMToIStorage: storage validated BEFORE the stream is touched. ...ToIStorageEx: all out pointers non-NULL else E_INVALIDARG,
 *    then tymed (NULL(0) or ISTREAM(4) else DV_E_TYMED); ISTREAM needs a valid interface.
 *  OleConvertIStorageToOLESTREAMEx: with cfFormat!=0: hMetaFilePict==NULL => DV_E_STGMEDIUM, dwSize==0 => E_INVALIDARG,
 *    tymed other than HGLOBAL(1)/ISTREAM(4) => DV_E_TYMED, ISTREAM must be a valid interface. */
#include "gap6.h"
#include <ole2.h>

#define E_DV_TYMED     ((HRESULT)0x80040069L)
#define E_DV_STGMEDIUM ((HRESULT)0x80040066L)
#define E_OS_GET       ((HRESULT)0x800401C0L)
#define E_STG_FILENOTFOUND ((HRESULT)0x80030002L)

UDECL(HRESULT, OleCreateLinkToFile_, (LPCOLESTR, REFIID, DWORD, LPFORMATETC, LPOLECLIENTSITE, LPSTORAGE, void **));
UDECL(HRESULT, OleCreateLinkToFileEx_, (LPCOLESTR, REFIID, DWORD, DWORD, ULONG, DWORD *, LPFORMATETC, IAdviseSink *, DWORD *, LPOLECLIENTSITE, LPSTORAGE, void **));
typedef OleCreateLinkToFile__pfn LINK_FN;
typedef OleCreateLinkToFileEx__pfn LINKX_FN;
#define LINK  ((LINK_FN)t_proc("OleCreateLinkToFile"))
#define LINKX ((LINKX_FN)t_proc("OleCreateLinkToFileEx"))

static int coinit(void) { HRESULT hr = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); return SUCCEEDED(hr); }

/* ---- minimal advise sink ---- */
typedef struct { IAdviseSink iface; LONG ref; } AS;
static HRESULT WINAPI as_qi(IAdviseSink *i, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI as_ar(IAdviseSink *i) { return ++((AS *)i)->ref; }
static ULONG WINAPI as_rl(IAdviseSink *i) { return --((AS *)i)->ref; }
static void WINAPI as_dc(IAdviseSink *i, FORMATETC *f, STGMEDIUM *m) {}
static void WINAPI as_vc(IAdviseSink *i, DWORD a, LONG b) {}
static void WINAPI as_r(IAdviseSink *i) {}
static void WINAPI as_s(IAdviseSink *i) {}
static void WINAPI as_c(IAdviseSink *i) {}
static IAdviseSinkVtbl as_vtbl = { as_qi, as_ar, as_rl, as_dc, as_vc, as_r, as_s, as_c };

static IStorage *mem_stg(void) {
    ILockBytes *lb = NULL; IStorage *stg = NULL;
    if (FAILED(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &lb))) return NULL;
    HRESULT hr = U(StgCreateDocfileOnILockBytes)(lb, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    lb->lpVtbl->Release(lb);
    return SUCCEEDED(hr) ? stg : NULL;
}
static void missing_path(WCHAR *out, size_t cch, const char *tag) {
    wsprintfW(out, L"%hs\\gap6_missing_%hs_%lu.bin", t_tmpdir(), tag, GetCurrentProcessId());
}

TEST(gap6_linktofile_ppv_null) {
    REQUIRE(LINK); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "ppvnull");
    EQ(LINK(p, &G6_IID_IUnknown, 0, NULL, NULL, stg, NULL), E_INVALIDARG);
    EQ(LINK(NULL, &G6_IID_IUnknown, 0, NULL, NULL, stg, NULL), E_INVALIDARG);
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
TEST(gap6_linktofile_ppv_zeroed_on_failure) {
    REQUIRE(LINK); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "zeroed");
    void *pv = (void *)(ULONG_PTR)0x1234;
    HRESULT hr = LINK(p, &G6_IID_IUnknown, 0, NULL, NULL, NULL /* invalid storage */, &pv);
    EQ(hr, E_INVALIDARG);
    CHECK(pv == NULL);
    U(CoUninitialize)();
}
TEST(gap6_linktofile_null_storage_invalid) {
    REQUIRE(LINK); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "nostg");
    void *pv = NULL;
    EQ(LINK(p, &G6_IID_IUnknown, 2 /* OLERENDER_FORMAT */, NULL, NULL, NULL, &pv), E_INVALIDARG);
    EQ(LINK(p, &G6_IID_IUnknown, 0, NULL, NULL, NULL, &pv), E_INVALIDARG);
    U(CoUninitialize)();
}
TEST(gap6_linktofile_missing_file_after_validation) {
    REQUIRE(LINK); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "missing");
    void *pv = (void *)1;
    HRESULT hr = LINK(p, &G6_IID_IUnknown, 2 /* OLERENDER_FORMAT, no format: cFormats = 0 */, NULL, NULL, stg, &pv);
    EQ(hr, E_STG_FILENOTFOUND);
    CHECK(pv == NULL);
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
TEST(gap6_linkex_validation_table) {
    REQUIRE(LINKX); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "table");
    AS sink; sink.iface.lpVtbl = &as_vtbl; sink.ref = 1;
    FORMATETC fe[2]; memset(fe, 0, sizeof fe);
    fe[0].cfFormat = CF_TEXT; fe[0].dwAspect = DVASPECT_CONTENT; fe[0].lindex = -1; fe[0].tymed = TYMED_HGLOBAL; fe[1] = fe[0];
    DWORD advf[2] = { 0, 0 }; DWORD conn[2] = { 0xAAAA, 0xAAAA };
    enum { NONE = 0, DRAW = 1, FORMAT = 2, ASIS = 3 };
    struct { const char *why; DWORD flags, render; ULONG n; DWORD *pa; FORMATETC *pf; IAdviseSink *sink; DWORD *pc; HRESULT want; } t[] = {
        { "baseline none",              0, NONE,   0, NULL, NULL, NULL, NULL, E_STG_FILENOTFOUND },
        { "flags leave-running",        1, NONE,   0, NULL, NULL, NULL, NULL, E_STG_FILENOTFOUND },
        { "flags bit 1",                2, NONE,   0, NULL, NULL, NULL, NULL, E_INVALIDARG },
        { "flags bit 31",      0x80000000, NONE,   0, NULL, NULL, NULL, NULL, E_INVALIDARG },
        { "flags 3",                    3, NONE,   0, NULL, NULL, NULL, NULL, E_INVALIDARG },
        { "draw, two formats",          0, DRAW,   2, advf, fe,   NULL, conn, E_INVALIDARG },
        { "draw, one format",           0, DRAW,   1, advf, fe,   NULL, conn, E_STG_FILENOTFOUND },
        { "none + sink",                0, NONE,   0, NULL, NULL, &sink.iface, NULL, E_INVALIDARG },
        { "draw + sink",                0, DRAW,   0, NULL, NULL, &sink.iface, NULL, E_INVALIDARG },
        { "asis + sink",                0, ASIS,   0, NULL, NULL, &sink.iface, NULL, E_INVALIDARG },
        { "format + sink",              0, FORMAT, 0, NULL, NULL, &sink.iface, NULL, E_STG_FILENOTFOUND },
        { "n=0 with advf",              0, NONE,   0, advf, NULL, NULL, NULL, E_INVALIDARG },
        { "n=0 with formatetc",         0, NONE,   0, NULL, fe,   NULL, NULL, E_INVALIDARG },
        { "n=0 with connections",       0, NONE,   0, NULL, NULL, NULL, conn, E_INVALIDARG },
        { "n=1 without advf",           0, FORMAT, 1, NULL, fe,   NULL, NULL, E_INVALIDARG },
        { "n=1 without formatetc",      0, FORMAT, 1, advf, NULL, NULL, NULL, E_INVALIDARG },
        { "n=1 ok, no connections",     0, FORMAT, 1, advf, fe,   NULL, NULL, E_STG_FILENOTFOUND },
        { "n=2 ok",                     0, FORMAT, 2, advf, fe,   NULL, conn, E_STG_FILENOTFOUND },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *pv = (void *)1; advf[0] = advf[1] = 0; conn[0] = conn[1] = 0xAAAA;
        HRESULT hr = LINKX(p, &G6_IID_IUnknown, t[i].flags, t[i].render, t[i].n, t[i].pa, t[i].pf, t[i].sink, t[i].pc, NULL, stg, &pv);
        CHECKF(hr == t[i].want, "%s: hr 0x%lx want 0x%lx", t[i].why, hr, t[i].want);
        CHECKF(pv == NULL, "%s: ppv not zeroed", t[i].why);
    }
    /* advf bits: only the low 7 bits are valid */
    static const struct { DWORD advf; HRESULT want; } a[] = { {0, E_STG_FILENOTFOUND}, {0x7f, E_STG_FILENOTFOUND}, {0x40, E_STG_FILENOTFOUND}, {0x80, E_INVALIDARG}, {0x100, E_INVALIDARG}, {0xFFFFFFFF, E_INVALIDARG}, {0x8000, E_INVALIDARG} };
    for (unsigned i = 0; i < sizeof a / sizeof *a; i++) {
        void *pv = NULL; DWORD ad = a[i].advf;
        HRESULT hr = LINKX(p, &G6_IID_IUnknown, 0, FORMAT, 1, &ad, fe, NULL, NULL, NULL, stg, &pv);
        CHECKF(hr == a[i].want, "advf 0x%lx: hr 0x%lx want 0x%lx", a[i].advf, hr, a[i].want);
    }
    if (stg) stg->lpVtbl->Release(stg);
    EQ(sink.ref, 1);
    U(CoUninitialize)();
}
TEST(gap6_linkex_connections_zeroed_by_validation) {
    REQUIRE(LINKX); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg();
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "conn");
    FORMATETC fe[2]; memset(fe, 0, sizeof fe);
    fe[0].cfFormat = CF_TEXT; fe[0].dwAspect = DVASPECT_CONTENT; fe[0].lindex = -1; fe[0].tymed = TYMED_HGLOBAL; fe[1] = fe[0];
    DWORD advf[2] = { 0, 0 }, conn[3] = { 0x1111, 0x2222, 0x3333 };
    void *pv = NULL;
    HRESULT hr = LINKX(p, &G6_IID_IUnknown, 0, 2, 2, advf, fe, NULL, conn, NULL, stg, &pv);
    EQ(hr, E_STG_FILENOTFOUND);
    CHECK(conn[0] == 0 && conn[1] == 0);        /* cFormats entries cleared */
    EQ(conn[2], 0x3333);                        /* nothing beyond cFormats touched */
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
TEST(gap6_linktofile_wrapper_forwards_formatetc_as_primeFirst) {
    REQUIRE(LINK); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine does not implement OleCreateLinkToFile(Ex)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg();
    WCHAR p[MAX_PATH]; missing_path(p, MAX_PATH, "wrap");
    FORMATETC fe; memset(&fe, 0, sizeof fe);
    fe.cfFormat = CF_TEXT; fe.dwAspect = DVASPECT_CONTENT; fe.lindex = -1; fe.tymed = TYMED_HGLOBAL;
    void *pv = NULL;
    EQ(LINK(p, &G6_IID_IUnknown, 2, &fe, NULL, stg, &pv), E_STG_FILENOTFOUND);   /* one format, validated, then the file lookup */
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}

/* ---- OleCreatePropertyFrame ---- */
TEST(gap6_propframe_exports) {
    CHECK(t_proc("OleCreatePropertyFrame") != NULL);
    CHECK(t_proc("OleCreatePropertyFrameIndirect") != NULL);
}
TEST(gap6_propframe_indirect_null_params) {
    typedef HRESULT (WINAPI *fn_t)(void *);
    fn_t f = (fn_t)t_proc("OleCreatePropertyFrameIndirect");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine would read through the NULL pointer"); return; }
    HRESULT hr = f(NULL);
    t_note("OleCreatePropertyFrameIndirect(NULL) = 0x%lx", hr);
    CHECK(FAILED(hr));
}

/* ---- OLESTREAM conversion: validation order, observed through the callback tables ---- */
typedef struct { const OLESTREAMVTBL *lpstbl; DWORD gets, puts, last_get, last_put; const BYTE *src; DWORD srclen, pos; } OS;
static DWORD CALLBACK os_get(LPOLESTREAM s, void *buf, DWORD n) {
    OS *o = (OS *)s; o->gets++; o->last_get = n;
    DWORD k = o->srclen - o->pos < n ? o->srclen - o->pos : n;
    memcpy(buf, o->src + o->pos, k); o->pos += k; return k;
}
static DWORD CALLBACK os_put(LPOLESTREAM s, const void *buf, DWORD n) { OS *o = (OS *)s; o->puts++; o->last_put = n; return n; }
static const OLESTREAMVTBL os_vtbl = { os_get, os_put };
static void os_init(OS *o, const void *src, DWORD len) { memset(o, 0, sizeof *o); o->lpstbl = &os_vtbl; o->src = src; o->srclen = len; }

TEST(gap6_olestream_to_storage_checks_storage_first) {
    REQUIRE(U(OleConvertOLESTREAMToIStorage));
    OS o; os_init(&o, "\x01\x05\x00\x00\x02\x00\x00\x00", 8);
    EQ(U(OleConvertOLESTREAMToIStorage)((LPOLESTREAM)&o, NULL, NULL), E_INVALIDARG);
    EQ(o.gets, 0);                                          /* the stream is never read */
}
TEST(gap6_olestream_to_storage_first_reads_are_dwords) {
    REQUIRE(U(OleConvertOLESTREAMToIStorage)); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine reads the OLE1 header differently (extra Get calls, other error ids)"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    OS o; os_init(&o, "", 0);
    EQ(U(OleConvertOLESTREAMToIStorage)((LPOLESTREAM)&o, stg, NULL), E_OS_GET);
    EQ(o.gets, 1); EQ(o.last_get, 4);                       /* version DWORD; OLE1StreamToUL fails at once */
    static const BYTE v[] = { 0x01, 0x05, 0x00, 0x00 };
    os_init(&o, v, 4);
    EQ(U(OleConvertOLESTREAMToIStorage)((LPOLESTREAM)&o, stg, NULL), E_OS_GET);
    EQ(o.gets, 2); EQ(o.last_get, 4);                       /* format DWORD */
    static const BYTE v3[] = { 0x01, 0x05, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00 };
    os_init(&o, v3, 8);
    EQ(U(OleConvertOLESTREAMToIStorage)((LPOLESTREAM)&o, stg, NULL), (HRESULT)0x800401C2);   /* CONVERT10_E_OLESTREAM_FMT */
    EQ(o.gets, 2);
    EQ(o.puts, 0);
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
TEST(gap6_olestream_to_storage_ex_validation_order) {
    REQUIRE(U(OleConvertOLESTREAMToIStorageEx)); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented OleConvertOLESTREAMToIStorageEx"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    OS o; os_init(&o, "", 0);
    CLIPFORMAT cf = 0; LONG w = 0, h = 0; DWORD sz = 0;
    STGMEDIUM m; memset(&m, 0, sizeof m); m.tymed = 77;
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, NULL, &w, &h, &sz, &m), E_INVALIDARG);   /* NULL cf wins over the bad tymed */
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, NULL, &h, &sz, &m), E_INVALIDARG);
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, NULL, &sz, &m), E_INVALIDARG);
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, &h, NULL, &m), E_INVALIDARG);
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, &h, &sz, NULL), E_INVALIDARG);
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, &h, &sz, &m), E_DV_TYMED);
    m.tymed = TYMED_HGLOBAL;
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, &h, &sz, &m), E_DV_TYMED);     /* only NULL(0) or ISTREAM(4) accepted */
    m.tymed = TYMED_ISTREAM; m.pstm = NULL;
    EQ(U(OleConvertOLESTREAMToIStorageEx)((LPOLESTREAM)&o, stg, &cf, &w, &h, &sz, &m), E_INVALIDARG);  /* ISTREAM needs a valid interface */
    EQ(o.gets, 0);
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
TEST(gap6_storage_to_olestream_ex_validation_order) {
    REQUIRE(U(OleConvertIStorageToOLESTREAMEx)); REQUIRE(U(CoInitializeEx));
    if (t_is_wine()) { t_skip("wine returns E_NOTIMPL for OleConvertIStorageToOLESTREAMEx"); return; }
    if (!coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = mem_stg(); CHECK(stg != NULL);
    OS o; os_init(&o, "", 0);
    STGMEDIUM m; memset(&m, 0, sizeof m);
    char data[8] = { 0 };
    m.tymed = 77; m.hGlobal = NULL;
    EQ(U(OleConvertIStorageToOLESTREAMEx)(stg, CF_TEXT, 1, 1, 8, &m, (LPOLESTREAM)&o), E_DV_STGMEDIUM);       /* NULL payload checked first */
    m.hGlobal = (HGLOBAL)data;
    EQ(U(OleConvertIStorageToOLESTREAMEx)(stg, CF_TEXT, 1, 1, 0, &m, (LPOLESTREAM)&o), E_INVALIDARG);          /* size 0 before tymed */
    EQ(U(OleConvertIStorageToOLESTREAMEx)(stg, CF_TEXT, 1, 1, 8, &m, (LPOLESTREAM)&o), E_DV_TYMED);            /* 77 */
    m.tymed = TYMED_FILE;
    EQ(U(OleConvertIStorageToOLESTREAMEx)(stg, CF_TEXT, 1, 1, 8, &m, (LPOLESTREAM)&o), E_DV_TYMED);
    m.tymed = TYMED_ISTREAM; m.pstm = NULL;
    EQ(U(OleConvertIStorageToOLESTREAMEx)(stg, CF_TEXT, 1, 1, 8, &m, (LPOLESTREAM)&o), E_DV_STGMEDIUM);       /* pstm is the same union slot: NULL payload */
    EQ(o.puts, 0);
    if (stg) stg->lpVtbl->Release(stg);
    U(CoUninitialize)();
}
