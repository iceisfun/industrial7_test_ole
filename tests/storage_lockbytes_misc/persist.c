/* Persistence helpers: OleSave / OleLoad / OleSaveToStream / OleLoadFromStream / ReadOleStg / WriteOleStg /
 * CreateDataCache persistence. Custom in-process IPersistStorage/IPersistStream objects record their call
 * sequence; classes are made available to CoCreateInstance through CoRegisterClassObject (no registry writes).
 * Expectations come from ole32's decompilation: OleSave = GetClassID, WriteClassStg, Save(stg,fSameAsLoad), stg->Commit(0);
 * OleSaveToStream = GetClassID, WriteClassStm, Save(stm, TRUE); OleLoadFromStream = ReadClassStm, CoCreateInstance(0x415),
 * QI(IPersistStream), Load, QI(riid). */
#include "slm.h"

SLM_GUID(G_IOleCacheControl, 0x00000129, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);
SLM_GUID(G_IViewObject,      0x0000010d, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);

UDECL(HRESULT, ReadOleStg, (LPSTORAGE, DWORD *, DWORD *, DWORD *, IMoniker **, IStream **));
UDECL(HRESULT, WriteOleStg, (LPSTORAGE, void *, DWORD, IStream **));

enum { K_PSTORAGE, K_PSTREAM, K_NOPERSIST };
typedef struct {
    union { IPersistStorageVtbl *vs; IPersistStreamVtbl *vm; IUnknownVtbl *vu; } lp;   /* first member = vtable */
    int kind; LONG ref; CLSID clsid;
    char log[64]; int nlog;
    HRESULT fail_getclass, fail_save, fail_load;
    int last_flag;                 /* fSameAsLoad / fClearDirty seen by Save */
    BYTE rbuf[64]; ULONG rlen;     /* what Load read */
    IStorage *loaded_stg;
} PO;
static PO g_pool[16]; static int g_npool;
static PO *g_last;
static void plog(PO *o, char c) { if (o->nlog < 63) o->log[o->nlog++] = c; o->log[o->nlog] = 0; }

static HRESULT WINAPI po_QI(IUnknown *p, REFIID riid, void **pp) {
    PO *o = (PO *)p; *pp = NULL;
    if (IsEqualGUID(riid, &G_IUnknown)) *pp = p;
    else if (o->kind == K_PSTORAGE && (IsEqualGUID(riid, &G_IPersist) || IsEqualGUID(riid, &G_IPersistStorage))) *pp = p;
    else if (o->kind == K_PSTREAM && (IsEqualGUID(riid, &G_IPersist) || IsEqualGUID(riid, &G_IPersistStream))) *pp = p;
    if (!*pp) return E_NOINTERFACE;
    o->ref++; return S_OK;
}
static ULONG WINAPI po_AddRef(IUnknown *p) { return ++((PO *)p)->ref; }
static ULONG WINAPI po_Release(IUnknown *p) { return --((PO *)p)->ref; }
static HRESULT WINAPI po_GetClassID(void *p, CLSID *c) { PO *o = p; plog(o, 'C'); if (o->fail_getclass) return o->fail_getclass; *c = o->clsid; return S_OK; }
static HRESULT WINAPI po_IsDirty(void *p) { plog((PO *)p, 'D'); return S_FALSE; }
static HRESULT WINAPI ps_InitNew(IPersistStorage *p, IStorage *s) { plog((PO *)p, 'N'); return S_OK; }
static HRESULT WINAPI ps_Load(IPersistStorage *p, IStorage *s) {
    PO *o = (PO *)p; plog(o, 'L'); o->loaded_stg = s;
    if (o->fail_load) return o->fail_load;
    HRESULT hr; o->rlen = slm_get_stream(s, L"payload", o->rbuf, 64, &hr);
    return S_OK;
}
static HRESULT WINAPI ps_Save(IPersistStorage *p, IStorage *s, BOOL same) {
    PO *o = (PO *)p; plog(o, 'S'); o->last_flag = same;
    if (o->fail_save) return o->fail_save;
    return slm_put_stream(s, L"payload", "PSO-PAYLOAD", 11);
}
static HRESULT WINAPI ps_SaveCompleted(IPersistStorage *p, IStorage *s) { plog((PO *)p, 'c'); return S_OK; }
static HRESULT WINAPI ps_HandsOff(IPersistStorage *p) { plog((PO *)p, 'H'); return S_OK; }
static IPersistStorageVtbl ps_vtbl = { (void *)po_QI, (void *)po_AddRef, (void *)po_Release, (void *)po_GetClassID, (void *)po_IsDirty,
    ps_InitNew, ps_Load, ps_Save, ps_SaveCompleted, ps_HandsOff };
static HRESULT WINAPI pm_Load(IPersistStream *p, IStream *s) {
    PO *o = (PO *)p; plog(o, 'L'); if (o->fail_load) return o->fail_load;
    ULONG r = 0; HRESULT hr = IStream_Read(s, o->rbuf, 8, &r); o->rlen = r; return hr;
}
static HRESULT WINAPI pm_Save(IPersistStream *p, IStream *s, BOOL clear) {
    PO *o = (PO *)p; plog(o, 'S'); o->last_flag = clear;
    if (o->fail_save) return o->fail_save;
    ULONG w; return IStream_Write(s, "PSTREAM!", 8, &w);
}
static HRESULT WINAPI pm_GetSizeMax(IPersistStream *p, ULARGE_INTEGER *pcb) { pcb->QuadPart = 8; return S_OK; }
static IPersistStreamVtbl pm_vtbl = { (void *)po_QI, (void *)po_AddRef, (void *)po_Release, (void *)po_GetClassID, (void *)po_IsDirty,
    pm_Load, pm_Save, pm_GetSizeMax };
static IUnknownVtbl pu_vtbl = { po_QI, po_AddRef, po_Release };

static PO *po_new(int kind, const CLSID *clsid) {
    PO *o = &g_pool[g_npool++ % 16]; memset(o, 0, sizeof *o);
    o->kind = kind; o->ref = 1; o->clsid = *clsid;
    o->lp.vu = kind == K_PSTORAGE ? (IUnknownVtbl *)&ps_vtbl : kind == K_PSTREAM ? (IUnknownVtbl *)&pm_vtbl : &pu_vtbl;
    g_last = o;
    return o;
}
/* class factory handing out objects of a fixed kind */
typedef struct { IClassFactory cf; int kind; CLSID clsid; int made; } FACT;
static HRESULT WINAPI f_QI(IClassFactory *p, REFIID r, void **pp) {
    if (IsEqualGUID(r, &G_IUnknown) || IsEqualGUID(r, &G_IClassFactory)) { *pp = p; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_AddRef(IClassFactory *p) { return 2; }
static ULONG WINAPI f_Release(IClassFactory *p) { return 1; }
static HRESULT WINAPI f_Create(IClassFactory *p, IUnknown *outer, REFIID riid, void **pp) {
    FACT *f = (FACT *)p; *pp = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    PO *o = po_new(f->kind, &f->clsid); f->made++;
    HRESULT hr = po_QI((IUnknown *)o, riid, pp);
    o->ref--;                       /* drop the creation reference */
    return hr;
}
static HRESULT WINAPI f_Lock(IClassFactory *p, BOOL l) { return S_OK; }
static IClassFactoryVtbl f_vtbl = { f_QI, f_AddRef, f_Release, f_Create, f_Lock };
static DWORD reg_class(FACT *f, int kind, const CLSID *clsid) {
    DWORD cookie = 0;
    f->cf.lpVtbl = &f_vtbl; f->kind = kind; f->clsid = *clsid; f->made = 0;
    HRESULT hr = U(CoRegisterClassObject)(clsid, (IUnknown *)&f->cf, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cookie);
    return SUCCEEDED(hr) ? cookie : 0;
}
static void unreg_class(DWORD cookie) { if (cookie) U(CoRevokeClassObject)(cookie); }

static IStorage *mkstg(ILockBytes **lb, DWORD mode) { IStorage *s = NULL; if (FAILED(slm_mem_docfile(&s, lb, mode))) return NULL; return s; }
static IStream *mkstm(void) { IStream *s = NULL; if (FAILED(U(CreateStreamOnHGlobal)(NULL, TRUE, &s))) return NULL; return s; }
static void rewind_stm(IStream *s) { LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL); }
static ULONG stm_bytes(IStream *s, BYTE *buf, ULONG cap) {
    STATSTG st; memset(&st, 0, sizeof st); IStream_Stat(s, &st, STATFLAG_NONAME);
    rewind_stm(s); ULONG r = 0; IStream_Read(s, buf, st.cbSize.LowPart < cap ? st.cbSize.LowPart : cap, &r); return r;
}
static int begin(void) { if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return 0; } return 1; }

/* ---- OleSave ---------------------------------------------------------------- */
TEST(slm_osave_call_sequence_and_class) {
    REQUIRE(U(OleSave)); REQUIRE(U(ReadClassStg));
    if (!begin()) return;
    for (int same = 0; same <= 1; same++) {
        ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); break; }
        PO *o = po_new(K_PSTORAGE, &G_CLSID_SlmA);
        HRESULT hr = U(OleSave)((IPersistStorage *)o, stg, same);
        EXPECT_EQ(hr, S_OK);
        EXPECT_STR(o->log, "CS");                 /* no SaveCompleted/HandsOffStorage from OleSave itself */
        EXPECT_EQ(o->last_flag, same);
        CLSID c; memset(&c, 0, sizeof c);
        EXPECT_EQ(U(ReadClassStg)(stg, &c), S_OK);
        CHECK(IsEqualGUID(&c, &G_CLSID_SlmA));
        char b[16] = { 0 }; HRESULT h2; ULONG n = slm_get_stream(stg, L"payload", b, 11, &h2);
        EXPECT_EQ(n, 11); CHECK(!memcmp(b, "PSO-PAYLOAD", 11));
        EXPECT_EQ(o->ref, 1);                    /* OleSave does not keep the object */
        IStorage_Release(stg); ILockBytes_Release(lb);
    }
    slm_couninit();
}
TEST(slm_osave_getclassid_failure) {
    REQUIRE(U(OleSave));
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTORAGE, &G_CLSID_SlmA); o->fail_getclass = E_ACCESSDENIED;
    EXPECT_EQ(U(OleSave)((IPersistStorage *)o, stg, TRUE), E_ACCESSDENIED);
    EXPECT_STR(o->log, "C");
    STATSTG st; memset(&st, 0xCC, sizeof st); IStorage_Stat(stg, &st, STATFLAG_NONAME);
    CHECK(IsEqualGUID(&st.clsid, &G_CLSID_NULL));      /* class not written */
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_osave_save_failure_after_class_written) {
    REQUIRE(U(OleSave));
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTORAGE, &G_CLSID_SlmB); o->fail_save = E_UNEXPECTED;
    EXPECT_EQ(U(OleSave)((IPersistStorage *)o, stg, FALSE), E_UNEXPECTED);
    EXPECT_STR(o->log, "CS");
    STATSTG st; memset(&st, 0xCC, sizeof st); IStorage_Stat(stg, &st, STATFLAG_NONAME);
    CHECK(IsEqualGUID(&st.clsid, &G_CLSID_SlmB));      /* WriteClassStg happens before Save */
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_osave_commits_transacted_storage) {
    REQUIRE(U(OleSave)); REQUIRE(U(StgOpenStorageOnILockBytes));
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW | STGM_TRANSACTED); if (!stg) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTORAGE, &G_CLSID_SlmA);
    EXPECT_EQ(U(OleSave)((IPersistStorage *)o, stg, FALSE), S_OK);
    IStorage_Release(stg);                         /* no explicit Commit: OleSave committed already */
    IStorage *re = NULL;
    EXPECT_EQ(U(StgOpenStorageOnILockBytes)(lb, NULL, SM_OPEN_RO, NULL, 0, &re), S_OK);
    if (re) {
        char b[16] = { 0 }; HRESULT h2; ULONG n = slm_get_stream(re, L"payload", b, 11, &h2);
        EXPECT_EQ(n, 11);
        STATSTG st; memset(&st, 0, sizeof st); IStorage_Stat(re, &st, STATFLAG_NONAME);
        CHECK(IsEqualGUID(&st.clsid, &G_CLSID_SlmA));
        IStorage_Release(re);
    }
    ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_osave_invalid_args) {
    REQUIRE(U(OleSave));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTORAGE, &G_CLSID_SlmA);
    HRESULT hr = U(OleSave)(NULL, stg, FALSE); CHECK(FAILED(hr));
    hr = U(OleSave)((IPersistStorage *)o, NULL, FALSE); CHECK(FAILED(hr));
    EXPECT_STR(o->log, "");
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}

/* ---- OleSaveToStream / OleLoadFromStream -------------------------------------- */
TEST(slm_ostm_save_layout) {
    REQUIRE(U(OleSaveToStream));
    if (!begin()) return;
    IStream *s = mkstm(); if (!s) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTREAM, &G_CLSID_SlmA);
    EXPECT_EQ(U(OleSaveToStream)((IPersistStream *)o, s), S_OK);
    EXPECT_STR(o->log, "CS");
    EXPECT_EQ(o->last_flag, TRUE);                 /* fClearDirty is always TRUE */
    BYTE b[64]; ULONG n = stm_bytes(s, b, 64);
    EXPECT_EQ(n, 24);
    if (n == 24) { CHECK(IsEqualGUID((GUID *)b, &G_CLSID_SlmA)); CHECK(!memcmp(b + 16, "PSTREAM!", 8)); }
    IStream_Release(s);
    slm_couninit();
}
TEST(slm_ostm_save_failures) {
    REQUIRE(U(OleSaveToStream));
    if (!begin()) return;
    IStream *s = mkstm(); if (!s) { CHECK(0); slm_couninit(); return; }
    PO *o = po_new(K_PSTREAM, &G_CLSID_SlmA); o->fail_getclass = E_FAIL;
    EXPECT_EQ(U(OleSaveToStream)((IPersistStream *)o, s), E_FAIL);
    BYTE b[64]; EXPECT_EQ(stm_bytes(s, b, 64), 0);   /* nothing written when GetClassID fails */
    o = po_new(K_PSTREAM, &G_CLSID_SlmA); o->fail_save = STG_E_MEDIUMFULL;
    EXPECT_EQ(U(OleSaveToStream)((IPersistStream *)o, s), STG_E_MEDIUMFULL);
    EXPECT_STR(o->log, "CS");
    EXPECT_EQ(stm_bytes(s, b, 64), 16);              /* CLSID header already written */
    IStream_Release(s);
    slm_couninit();
}
TEST(slm_ostm_save_invalid_args) {
    REQUIRE(U(OleSaveToStream));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    IStream *s = mkstm(); PO *o = po_new(K_PSTREAM, &G_CLSID_SlmA);
    CHECK(FAILED(U(OleSaveToStream)(NULL, s)));
    CHECK(FAILED(U(OleSaveToStream)((IPersistStream *)o, NULL)));
    if (s) IStream_Release(s);
    slm_couninit();
}
TEST(slm_ostm_load_roundtrip) {
    REQUIRE(U(OleSaveToStream)); REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CoRegisterClassObject));
    if (!begin()) return;
    FACT f; DWORD ck = reg_class(&f, K_PSTREAM, &G_CLSID_SlmA);
    CHECK(ck != 0);
    IStream *s = mkstm(); if (!s || !ck) { CHECK(0); unreg_class(ck); slm_couninit(); return; }
    PO *src = po_new(K_PSTREAM, &G_CLSID_SlmA);
    EXPECT_EQ(U(OleSaveToStream)((IPersistStream *)src, s), S_OK);
    rewind_stm(s);
    void *p = NULL;
    HRESULT hr = U(OleLoadFromStream)(s, &G_IUnknown, &p);
    EXPECT_EQ(hr, S_OK);
    CHECK(p != NULL);
    EXPECT_EQ(f.made, 1);
    CHECK(g_last != src && g_last->kind == K_PSTREAM);
    EXPECT_STR(g_last->log, "L");
    EXPECT_EQ(g_last->rlen, 8);
    CHECK(!memcmp(g_last->rbuf, "PSTREAM!", 8));     /* Load saw the stream positioned just past the CLSID */
    if (p) { void *q = NULL; EXPECT_EQ(IUnknown_QueryInterface((IUnknown *)p, &G_IPersistStream, &q), S_OK); if (q) IUnknown_Release((IUnknown *)q); IUnknown_Release((IUnknown *)p); }
    /* the requested interface is the one returned */
    rewind_stm(s); p = NULL;
    EXPECT_EQ(U(OleLoadFromStream)(s, &G_IPersistStream, &p), S_OK);
    CHECK(p != NULL);
    if (p) IUnknown_Release((IUnknown *)p);
    IStream_Release(s); unreg_class(ck);
    slm_couninit();
}
TEST(slm_ostm_load_unsupported_riid) {
    REQUIRE(U(OleSaveToStream)); REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CoRegisterClassObject));
    if (!begin()) return;
    FACT f; DWORD ck = reg_class(&f, K_PSTREAM, &G_CLSID_SlmA);
    IStream *s = mkstm(); if (!s || !ck) { CHECK(0); unreg_class(ck); slm_couninit(); return; }
    PO *src = po_new(K_PSTREAM, &G_CLSID_SlmA);
    U(OleSaveToStream)((IPersistStream *)src, s); rewind_stm(s);
    void *p = (void *)1;
    EXPECT_EQ(U(OleLoadFromStream)(s, &G_IStorage, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_STR(g_last->log, "L");                    /* the object was created and loaded before the final QI failed */
    IStream_Release(s); unreg_class(ck);
    slm_couninit();
}
TEST(slm_ostm_load_object_without_ipersiststream) {
    REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CoRegisterClassObject));
    if (!begin()) return;
    FACT f; DWORD ck = reg_class(&f, K_NOPERSIST, &G_CLSID_SlmB);
    IStream *s = mkstm(); if (!s || !ck) { CHECK(0); unreg_class(ck); slm_couninit(); return; }
    ULONG w; IStream_Write(s, &G_CLSID_SlmB, 16, &w); IStream_Write(s, "PSTREAM!", 8, &w); rewind_stm(s);
    void *p = (void *)1;
    EXPECT_EQ(U(OleLoadFromStream)(s, &G_IUnknown, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    EXPECT_EQ(f.made, 1);
    EXPECT_STR(g_last->log, "");
    IStream_Release(s); unreg_class(ck);
    slm_couninit();
}
TEST(slm_ostm_load_unregistered_class) {
    REQUIRE(U(OleLoadFromStream));
    if (!begin()) return;
    IStream *s = mkstm(); if (!s) { CHECK(0); slm_couninit(); return; }
    ULONG w; IStream_Write(s, &G_CLSID_SlmC, 16, &w); IStream_Write(s, "PSTREAM!", 8, &w); rewind_stm(s);
    void *p = (void *)1;
    HRESULT hr = U(OleLoadFromStream)(s, &G_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("OleLoadFromStream(unregistered CLSID) = 0x%08lx (REGDB_E_CLASSNOTREG is 0x80040154)", hr);
    IStream_Release(s);
    slm_couninit();
}
TEST(slm_ostm_load_short_stream) {
    REQUIRE(U(OleLoadFromStream));
    if (!begin()) return;
    IStream *s = mkstm(); if (!s) { CHECK(0); slm_couninit(); return; }
    void *p = (void *)1;
    HRESULT hr = U(OleLoadFromStream)(s, &G_IUnknown, &p);      /* empty stream: no CLSID to read */
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("OleLoadFromStream(empty) = 0x%08lx", hr);
    ULONG w; IStream_Write(s, &G_CLSID_SlmC, 7, &w); rewind_stm(s); p = (void *)1;
    hr = U(OleLoadFromStream)(s, &G_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    IStream_Release(s);
    slm_couninit();
}
TEST(slm_ostm_load_null_out) {
    REQUIRE(U(OleLoadFromStream));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    IStream *s = mkstm();
    EXPECT_EQ(U(OleLoadFromStream)(s, &G_IUnknown, NULL), H_INVALIDARG);
    void *p = (void *)1;
    HRESULT hr = U(OleLoadFromStream)(NULL, &G_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    if (s) IStream_Release(s);
    slm_couninit();
}

/* ---- OleLoad ---------------------------------------------------------------------- */
TEST(slm_oload_creates_object_and_calls_load) {
    REQUIRE(U(OleLoad)); REQUIRE(U(WriteClassStg)); REQUIRE(U(CoRegisterClassObject));
    if (!begin()) return;
    FACT f; DWORD ck = reg_class(&f, K_PSTORAGE, &G_CLSID_SlmB);
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW);
    if (!stg || !ck) { CHECK(0); unreg_class(ck); slm_couninit(); return; }
    EXPECT_EQ(U(WriteClassStg)(stg, &G_CLSID_SlmB), S_OK);
    slm_put_stream(stg, L"payload", "PSO-PAYLOAD", 11);
    void *p = NULL;
    HRESULT hr = U(OleLoad)(stg, &G_IUnknown, NULL, &p);
    EXPECT_EQ(hr, S_OK);
    CHECK(p != NULL);
    EXPECT_EQ(f.made, 1);
    CHECK(strchr(g_last->log, 'L') != NULL);
    CHECK(g_last->loaded_stg == stg);
    EXPECT_EQ(g_last->rlen, 11);
    t_note("OleLoad call sequence on the loaded object: \"%s\"", g_last->log);
    if (p) IUnknown_Release((IUnknown *)p);
    IStorage_Release(stg); ILockBytes_Release(lb); unreg_class(ck);
    slm_couninit();
}
TEST(slm_oload_errors) {
    REQUIRE(U(OleLoad));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    EXPECT_EQ(U(OleLoad)(stg, &G_IUnknown, NULL, NULL), H_INVALIDARG);
    void *p = (void *)1;
    HRESULT hr = U(OleLoad)(NULL, &G_IUnknown, NULL, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    p = (void *)1;
    hr = U(OleLoad)(stg, &G_IUnknown, NULL, &p);     /* storage without a class */
    CHECK(FAILED(hr));
    t_note("OleLoad(storage with CLSID_NULL) = 0x%08lx", hr);
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}

/* ---- WriteOleStg / ReadOleStg --------------------------------------------------- */
TEST(slm_olestg_write_creates_stream) {
    REQUIRE(UN(WriteOleStg));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented WriteOleStg"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    IStream *out = NULL;
    EXPECT_EQ(UN(WriteOleStg)(stg, NULL, 0, &out), S_OK);
    CHECK(out != NULL);
    if (out) {
        LARGE_INTEGER z; z.QuadPart = 0; ULARGE_INTEGER pos;
        EXPECT_EQ(IStream_Seek(out, z, STREAM_SEEK_CUR, &pos), S_OK);
        EXPECT_EQ(pos.QuadPart, 16);
        IStream_Release(out);
    }
    BYTE b[32]; HRESULT hr; ULONG n = slm_get_stream(stg, L"\1Ole", b, 32, &hr);
    EXPECT_EQ(n, 16);                            /* version DWORD + 12 bytes of flags/options */
    if (n == 16) { DWORD ver = *(DWORD *)b; CHECK(ver == 0x02000001); int nz = 0; for (int i = 4; i < 16; i++) nz |= b[i]; EXPECT_EQ(nz, 0); }
    /* idempotent: a second write reuses the stream */
    EXPECT_EQ(UN(WriteOleStg)(stg, NULL, 0, NULL), S_OK);
    n = slm_get_stream(stg, L"\1Ole", b, 32, &hr); EXPECT_EQ(n, 16);
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_olestg_write_null_storage) {
    REQUIRE(UN(WriteOleStg));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented WriteOleStg"); return; }
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    IStream *out = (IStream *)(ULONG_PTR)1;
    HRESULT hr = UN(WriteOleStg)(NULL, NULL, 0, NULL);
    EXPECT_EQ(hr, H_INVALIDARG);
    slm_couninit();
}
TEST(slm_olestg_read_crafted_stream) {
    REQUIRE(UN(ReadOleStg));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented ReadOleStg"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    DWORD img[5] = { 0x02000001, 0x5, 3, 0, 0 };   /* version, flags, update option, reserved, null moniker */
    EXPECT_EQ(slm_put_stream(stg, L"\1Ole", img, sizeof img), S_OK);
    DWORD fl = 0xAA, up = 0xAA, rs = 0xAA; IMoniker *mk = (IMoniker *)(ULONG_PTR)1; IStream *st = (IStream *)(ULONG_PTR)1;
    HRESULT hr = UN(ReadOleStg)(stg, &fl, &up, &rs, &mk, &st);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(fl, 5); EXPECT_EQ(up, 3); EXPECT_EQ(rs, 0);
    CHECK(mk == NULL);
    CHECK(st != NULL && st != (IStream *)(ULONG_PTR)1);
    if (st && st != (IStream *)(ULONG_PTR)1) IStream_Release(st);
    /* all outputs are optional */
    EXPECT_EQ(UN(ReadOleStg)(stg, NULL, NULL, NULL, NULL, NULL), S_OK);
    mk = (IMoniker *)(ULONG_PTR)1; st = (IStream *)(ULONG_PTR)1;
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_olestg_read_errors) {
    REQUIRE(UN(ReadOleStg));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented ReadOleStg"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    IMoniker *mk = (IMoniker *)(ULONG_PTR)1; IStream *st = (IStream *)(ULONG_PTR)1;
    DWORD fl = 0, up = 0, rs = 0;
    /* no \1Ole stream */
    EXPECT_EQ(UN(ReadOleStg)(stg, &fl, &up, &rs, &mk, &st), STG_E_FILENOTFOUND);
    CHECK(mk == NULL); CHECK(st == NULL);              /* optional out pointers cleared up front */
    /* wrong version */
    DWORD bad[5] = { 0x01000001, 0, 0, 0, 0 };
    slm_put_stream(stg, L"\1Ole", bad, sizeof bad);
    HRESULT hr = UN(ReadOleStg)(stg, &fl, &up, &rs, NULL, NULL);
    EXPECT_EQ(hr, (HRESULT)0x8004006A);
    /* non-zero reserved word */
    DWORD bad2[5] = { 0x02000001, 0, 0, 7, 0 };
    slm_put_stream(stg, L"\1Ole", bad2, sizeof bad2);
    hr = UN(ReadOleStg)(stg, &fl, &up, &rs, NULL, NULL);
    EXPECT_EQ(hr, (HRESULT)0x8004006A);
    /* too short */
    slm_put_stream(stg, L"\1Ole", bad2, 6);
    hr = UN(ReadOleStg)(stg, &fl, &up, &rs, NULL, NULL);
    CHECK(FAILED(hr));
    t_note("ReadOleStg(6-byte \\1Ole) = 0x%08lx", hr);
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_olestg_read_null_storage) {
    REQUIRE(UN(ReadOleStg));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!begin()) return;
    IMoniker *mk = (IMoniker *)(ULONG_PTR)1; IStream *st = (IStream *)(ULONG_PTR)1;
    HRESULT hr = UN(ReadOleStg)(NULL, NULL, NULL, NULL, &mk, &st);
    CHECK(FAILED(hr));
    CHECK(mk == NULL && st == NULL);
    slm_couninit();
}
TEST(slm_olestg_write_then_read_with_moniker_tail) {
    REQUIRE(UN(WriteOleStg)); REQUIRE(UN(ReadOleStg));
    if (t_is_wine()) { t_skip("wine aborts on unimplemented WriteOleStg"); return; }
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    UN(WriteOleStg)(stg, NULL, 0, NULL);
    DWORD fl = 9, up = 9, rs = 9;
    /* WriteOleStg(NULL object) stops after the 16-byte header, so the moniker part is absent */
    HRESULT hr = UN(ReadOleStg)(stg, &fl, &up, &rs, NULL, NULL);
    t_note("ReadOleStg right after WriteOleStg(NULL object) = 0x%08lx, flags=%lu update=%lu", hr, fl, up);
    if (SUCCEEDED(hr)) { EXPECT_EQ(fl, 0); EXPECT_EQ(up, 0); }
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}

/* ---- CreateDataCache ------------------------------------------------------------- */
TEST(slm_cache_create_args) {
    REQUIRE(U(CreateDataCache));
    if (t_is_wine()) { t_note("not all argument paths are safe under wine"); }
    if (!begin()) return;
    EXPECT_EQ(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IOleCache2, NULL), H_INVALIDARG);
    void *p = (void *)1;
    HRESULT hr = U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IOleCache2, &p);
    EXPECT_EQ(hr, S_OK);
    CHECK(p != NULL && p != (void *)1);
    if (p && p != (void *)1) EXPECT_EQ(IUnknown_Release((IUnknown *)p), 0);
    /* an unsupported interface is refused and the output stays NULL-ish */
    p = (void *)1;
    hr = U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IStorage, &p);
    CHECK(FAILED(hr));
    t_note("CreateDataCache(IID_IStorage) = 0x%08lx", hr);
    if (SUCCEEDED(hr) && p && p != (void *)1) IUnknown_Release((IUnknown *)p);
    slm_couninit();
}
TEST(slm_cache_aggregation_rules) {
    REQUIRE(U(CreateDataCache));
    if (!begin()) return;
    CLB outer; clb_init(&outer);                    /* any object with a valid IUnknown vtable */
    void *p = (void *)1;
    HRESULT hr = U(CreateDataCache)((IUnknown *)&outer.iface, &G_CLSID_SlmA, &G_IOleCache2, &p);
    EXPECT_EQ(hr, H_INVALIDARG);                    /* aggregated creation demands IID_IUnknown */
    p = NULL;
    hr = U(CreateDataCache)((IUnknown *)&outer.iface, &G_CLSID_SlmA, &G_IUnknown, &p);
    EXPECT_EQ(hr, S_OK);
    CHECK(p != NULL);
    if (p) {
        /* the inner IUnknown does not delegate: QI for IUnknown returns the inner pointer itself */
        void *q = NULL;
        EXPECT_EQ(IUnknown_QueryInterface((IUnknown *)p, &G_IUnknown, &q), S_OK);
        CHECK(q == p);
        if (q) IUnknown_Release((IUnknown *)q);
        EXPECT_EQ(IUnknown_Release((IUnknown *)p), 0);
    }
    clb_free(&outer);
    slm_couninit();
}
TEST(slm_cache_interface_set) {
    REQUIRE(U(CreateDataCache));
    if (!begin()) return;
    void *p = NULL;
    if (FAILED(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IUnknown, &p)) || !p) { CHECK(0); slm_couninit(); return; }
    const GUID *want[] = { &G_IOleCache, &G_IOleCache2, &G_IOleCacheControl, &G_IPersistStorage, &G_IPersist, &G_IDataObject, &G_IViewObject, &G_IViewObject2 };
    const char *nm[] = { "IOleCache", "IOleCache2", "IOleCacheControl", "IPersistStorage", "IPersist", "IDataObject", "IViewObject", "IViewObject2" };
    for (int i = 0; i < 8; i++) {
        void *q = NULL;
        HRESULT hr = IUnknown_QueryInterface((IUnknown *)p, want[i], &q);
        CHECKF(hr == S_OK && q, "%s: 0x%08lx", nm[i], hr);
        if (q) IUnknown_Release((IUnknown *)q);
    }
    void *q = (void *)1;
    EXPECT_EQ(IUnknown_QueryInterface((IUnknown *)p, &G_IStorage, &q), E_NOINTERFACE); CHECK(q == NULL);
    q = (void *)1;
    EXPECT_EQ(IUnknown_QueryInterface((IUnknown *)p, &G_ILockBytes, &q), E_NOINTERFACE); CHECK(q == NULL);
    IUnknown_Release((IUnknown *)p);
    slm_couninit();
}
TEST(slm_cache_getclassid) {
    REQUIRE(U(CreateDataCache));
    if (!begin()) return;
    void *p = NULL;
    if (FAILED(U(CreateDataCache)(NULL, &G_CLSID_SlmB, &G_IPersistStorage, &p)) || !p) { CHECK(0); slm_couninit(); return; }
    CLSID c; memset(&c, 0, sizeof c);
    EXPECT_EQ(IPersistStorage_GetClassID((IPersistStorage *)p, &c), S_OK);
    CHECK(IsEqualGUID(&c, &G_CLSID_SlmB));
    IPersistStorage_Release((IPersistStorage *)p);
    slm_couninit();
}

static HGLOBAL make_dib(void) {
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 44);
    BITMAPINFOHEADER *h = GlobalLock(g);
    memset(h, 0, 44);
    h->biSize = 40; h->biWidth = 1; h->biHeight = 1; h->biPlanes = 1; h->biBitCount = 24; h->biCompression = BI_RGB; h->biSizeImage = 4;
    ((BYTE *)h)[40] = 0x11; ((BYTE *)h)[41] = 0x22; ((BYTE *)h)[42] = 0x33;
    GlobalUnlock(g);
    return g;
}
TEST(slm_cache_save_load_dib) {
    REQUIRE(U(CreateDataCache)); REQUIRE(U(ReleaseStgMedium));
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    void *p = NULL;
    if (FAILED(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IOleCache2, &p)) || !p) { CHECK(0); IStorage_Release(stg); ILockBytes_Release(lb); slm_couninit(); return; }
    IOleCache *oc = (IOleCache *)p; IPersistStorage *ps = NULL; IDataObject *dobj = NULL;
    IOleCache_QueryInterface(oc, &G_IPersistStorage, (void **)&ps);
    IOleCache_QueryInterface(oc, &G_IDataObject, (void **)&dobj);
    CHECK(ps && dobj);
    if (ps && dobj) {
        FORMATETC fe = { CF_DIB, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        EXPECT_EQ(IPersistStorage_InitNew(ps, stg), S_OK);
        DWORD conn = 0;
        HRESULT hr = IOleCache_Cache(oc, &fe, 0, &conn);
        CHECKF(SUCCEEDED(hr), "Cache(CF_DIB) = 0x%08lx", hr);
        STGMEDIUM med; memset(&med, 0, sizeof med); med.tymed = TYMED_HGLOBAL; med.hGlobal = make_dib();
        hr = IDataObject_SetData(dobj, &fe, &med, TRUE);
        CHECKF(SUCCEEDED(hr), "SetData(CF_DIB) = 0x%08lx", hr);
        STGMEDIUM got; memset(&got, 0, sizeof got);
        hr = IDataObject_GetData(dobj, &fe, &got);
        CHECKF(hr == S_OK, "GetData = 0x%08lx", hr);
        if (hr == S_OK) {
            EXPECT_EQ(got.tymed, TYMED_HGLOBAL);
            BYTE *b = GlobalLock(got.hGlobal);
            CHECK(b && GlobalSize(got.hGlobal) >= 44 && ((BITMAPINFOHEADER *)b)->biWidth == 1 && b[40] == 0x11 && b[42] == 0x33);
            if (b) GlobalUnlock(got.hGlobal);
            U(ReleaseStgMedium)(&got);
        }
        EXPECT_EQ(IPersistStorage_Save(ps, stg, FALSE), S_OK);
        EXPECT_EQ(IPersistStorage_SaveCompleted(ps, NULL), S_OK);

        int pres = 0;
        IEnumSTATSTG *en = NULL;
        if (SUCCEEDED(IStorage_EnumElements(stg, 0, NULL, 0, &en))) {
            STATSTG e; ULONG f = 0;
            while (IEnumSTATSTG_Next(en, 1, &e, &f) == S_OK && f == 1) {
                if (!wcsncmp(e.pwcsName, L"\2OlePres", 8)) pres++;
                U(CoTaskMemFree)(e.pwcsName);
            }
            IEnumSTATSTG_Release(en);
        }
        CHECK(pres >= 1);                            /* cached presentation persisted as \2OlePres### */

        /* a fresh cache loading the same storage sees the persisted presentation */
        void *p2 = NULL;
        if (SUCCEEDED(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IOleCache2, &p2)) && p2) {
            IPersistStorage *ps2 = NULL; IDataObject *d2 = NULL;
            IUnknown_QueryInterface((IUnknown *)p2, &G_IPersistStorage, (void **)&ps2);
            IUnknown_QueryInterface((IUnknown *)p2, &G_IDataObject, (void **)&d2);
            if (ps2 && d2) {
                EXPECT_EQ(IPersistStorage_Load(ps2, stg), S_OK);
                STGMEDIUM g2; memset(&g2, 0, sizeof g2);
                hr = IDataObject_GetData(d2, &fe, &g2);
                t_note("GetData(CF_DIB) on a cache loaded from storage = 0x%08lx", hr);
                if (hr == S_OK) {
                    BYTE *b = GlobalLock(g2.hGlobal);
                    CHECK(b && ((BITMAPINFOHEADER *)b)->biWidth == 1 && b[40] == 0x11);
                    if (b) GlobalUnlock(g2.hGlobal);
                    U(ReleaseStgMedium)(&g2);
                }
            }
            if (ps2) IPersistStorage_Release(ps2);
            if (d2) IDataObject_Release(d2);
            IUnknown_Release((IUnknown *)p2);
        }
    }
    if (ps) IPersistStorage_Release(ps);
    if (dobj) IDataObject_Release(dobj);
    IOleCache_Release(oc);
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_cache_load_empty_storage) {
    REQUIRE(U(CreateDataCache));
    if (!begin()) return;
    ILockBytes *lb; IStorage *stg = mkstg(&lb, SM_CREATE_RW); if (!stg) { CHECK(0); slm_couninit(); return; }
    void *p = NULL;
    if (FAILED(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IPersistStorage, &p)) || !p) { CHECK(0); slm_couninit(); return; }
    HRESULT hr = IPersistStorage_Load((IPersistStorage *)p, stg);
    t_note("cache Load(empty storage) = 0x%08lx", hr);
    IDataObject *d = NULL;
    IPersistStorage_QueryInterface((IPersistStorage *)p, &G_IDataObject, (void **)&d);
    if (d) {
        FORMATETC fe = { CF_DIB, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM m; memset(&m, 0, sizeof m);
        hr = IDataObject_GetData(d, &fe, &m);
        CHECK(FAILED(hr));                             /* nothing cached */
        t_note("GetData(CF_DIB) on empty cache = 0x%08lx", hr);
        IDataObject_Release(d);
    }
    IPersistStorage_Release((IPersistStorage *)p);
    IStorage_Release(stg); ILockBytes_Release(lb);
    slm_couninit();
}
TEST(slm_cache_isdirty_initial) {
    REQUIRE(U(CreateDataCache));
    if (!begin()) return;
    void *p = NULL;
    if (FAILED(U(CreateDataCache)(NULL, &G_CLSID_SlmA, &G_IPersistStorage, &p)) || !p) { CHECK(0); slm_couninit(); return; }
    HRESULT hr = IPersistStorage_IsDirty((IPersistStorage *)p);
    CHECKF(hr == S_OK || hr == S_FALSE, "IsDirty = 0x%08lx", hr);
    t_note("fresh data cache IsDirty = 0x%08lx", hr);
    IPersistStorage_Release((IPersistStorage *)p);
    slm_couninit();
}
