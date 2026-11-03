/* CoCreateFreeThreadedMarshaler: standalone and aggregated behaviour. */
#include "mb.h"
#include <stdlib.h>

#define OBJ g_obj[0]
static const GUID CLSID_FtmLocal1809 = {0x00000359, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

static IMarshal *mk_ftm(IUnknown *outer, IUnknown **unk_out) {
    IUnknown *u = NULL; IMarshal *m = NULL;
    if (FAILED(U(CoCreateFreeThreadedMarshaler)(outer, &u)) || !u) return NULL;
    if (FAILED(IUnknown_QueryInterface(u, &IID_IMarshal, (void **)&m))) m = NULL;
    if (unk_out) *unk_out = u; else IUnknown_Release(u);
    return m;
}
#define FTM_SETUP() REQUIRE(U(CoCreateFreeThreadedMarshaler)); mb_obj_reset(1); MB_INIT_STA()

TEST(mbasic_ftm_null_out) {
    REQUIRE(U(CoCreateFreeThreadedMarshaler)); MB_INIT_STA();
    if (t_is_wine()) { t_skip("wine"); MB_DONE(); return; }
    EQ(U(CoCreateFreeThreadedMarshaler)(NULL, NULL), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_ftm_standalone_interfaces) {
    FTM_SETUP();
    IUnknown *u = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(NULL, &u), S_OK); CHECK(u != NULL);
    if (u) {
        void *p = NULL;
        EQ(IUnknown_QueryInterface(u, &IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EQ(IUnknown_QueryInterface(u, &IID_IUnknown, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = (void *)1; EQ(IUnknown_QueryInterface(u, &IID_IClassFactory, &p), MB_E_NOINTERFACE); EQ(p, 0);
        p = (void *)1; EQ(IUnknown_QueryInterface(u, &MB_IID_Bogus, &p), MB_E_NOINTERFACE); EQ(p, 0);
        p = NULL; HRESULT hr = IUnknown_QueryInterface(u, &IID_IAgileObject, &p);
        t_note("FTM QI(IAgileObject) = 0x%lx", (unsigned long)hr); if (p) IUnknown_Release((IUnknown *)p);
        IUnknown_Release(u);
    }
    MB_DONE();
}
TEST(mbasic_ftm_aggregated_qi_delegates_to_outer) {
    FTM_SETUP();
    IUnknown *inner = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(MB_UNK(0), &inner), S_OK);
    CHECK(inner != NULL);
    OBJ.marshal_inner = inner;
    IMarshal *m = NULL;
    EQ(IUnknown_QueryInterface(MB_UNK(0), &IID_IMarshal, (void **)&m), S_OK);
    CHECK(m != NULL);
    if (m) {
        CLSID c;
        EQ(IMarshal_GetUnmarshalClass(m, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &c), S_OK);
        CHECK(IsEqualGUID(&c, &MB_CLSID_InProcFTM));
        IMarshal_Release(m);
    }
    OBJ.marshal_inner = NULL;
    if (inner) IUnknown_Release(inner);
    MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_unmarshal_class) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    CLSID c;
    EQ(IMarshal_GetUnmarshalClass(m, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &c), S_OK);
    CHECK(IsEqualGUID(&c, &MB_CLSID_InProcFTM));
    static const DWORD others[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE };
    for (int i = 0; i < 3; i++) {
        memset(&c, 0, sizeof c);
        EQ(IMarshal_GetUnmarshalClass(m, &IID_IClassFactory, MB_UNK(0), others[i], NULL, MSHLFLAGS_NORMAL, &c), S_OK);
        CHECKF(IsEqualGUID(&c, &MB_CLSID_StdMarshal) || IsEqualGUID(&c, &CLSID_FtmLocal1809), "ctx %lu: not StdMarshal / 00000359", others[i]);
        t_note("FTM GetUnmarshalClass ctx %lu -> %08lx-...", others[i], c.Data1);
    }
    IMarshal_Release(m); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_marshal_size_and_layout_inproc) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    ULONG sz = 0;
    EQ(IMarshal_GetMarshalSizeMax(m, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &sz), S_OK);
    EQ(sz, 4 + 8 + 16);                       /* flags + pointer-sized value + GUID */
    static const DWORD fls[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK };
    for (int i = 0; i < 3; i++) {
        IStream *s = mb_stream();
        EQ(IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, fls[i]), S_OK);
        EQ(mb_size(s), 28);
        ULONG n; BYTE *b = mb_dump(s, &n);
        CHECK(n >= 12);
        if (n >= 12) {
            DWORD f; void *p; memcpy(&f, b, 4); memcpy(&p, b + 4, 8);
            EQ(f, fls[i]);
            CHECK(p == (void *)MB_UNK(0));
        }
        free(b);
        mb_rewind(s); EQ(IMarshal_ReleaseMarshalData(m, s), S_OK);
        if (fls[i] != MSHLFLAGS_TABLEWEAK) EQ(OBJ.ref, 1);
        IStream_Release(s);
        if (fls[i] == MSHLFLAGS_TABLEWEAK) OBJ.ref = 1;
    }
    IMarshal_Release(m); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_normal_lifetime) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream(); IUnknown *p = NULL;
    EQ(IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    CHECK(OBJ.ref > 1);                       /* marshal data holds a reference */
    mb_rewind(s);
    EQ(IMarshal_UnmarshalInterface(m, s, &IID_IUnknown, (void **)&p), S_OK);
    CHECK(p == MB_UNK(0));                    /* same-process: the raw pointer */
    if (p) IUnknown_Release(p);
    EQ(OBJ.ref, 1);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_tablestrong_lifetime) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream(); IUnknown *p = NULL;
    EQ(IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLESTRONG), S_OK);
    CHECK(OBJ.ref > 1);
    mb_rewind(s); EQ(IMarshal_UnmarshalInterface(m, s, &IID_IUnknown, (void **)&p), S_OK);
    if (p) IUnknown_Release(p);
    CHECK(OBJ.ref > 1);                       /* table reference survives unmarshal */
    mb_rewind(s); EQ(IMarshal_ReleaseMarshalData(m, s), S_OK);
    EQ(OBJ.ref, 1);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_tableweak_lifetime) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream(); IUnknown *p = NULL;
    EQ(IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLEWEAK), S_OK);
    EQ(OBJ.ref, 1);                           /* weak: nothing added */
    mb_rewind(s); EQ(IMarshal_UnmarshalInterface(m, s, &IID_IUnknown, (void **)&p), S_OK);
    CHECK(OBJ.ref > 1);                       /* unmarshal hands out an AddRef'd pointer */
    if (p) IUnknown_Release(p);
    EQ(OBJ.ref, 1);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_ftm_disconnect_is_noop) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream();
    EQ(IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    LONG r = OBJ.ref;
    EQ(IMarshal_DisconnectObject(m, 0), S_OK);
    EQ(OBJ.ref, r);
    mb_rewind(s); EQ(IMarshal_ReleaseMarshalData(m, s), S_OK);
    EQ(OBJ.ref, 1);
    /* marshal data is not validated: unmarshal after release still returns the raw pointer */
    IUnknown *p = NULL;
    mb_rewind(s); EQ(IMarshal_UnmarshalInterface(m, s, &IID_IUnknown, (void **)&p), S_OK);
    CHECK(p == MB_UNK(0));
    if (p) IUnknown_Release(p);
    t_note("ref after unmarshal+release of stale data = %ld", OBJ.ref);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}
TEST(mbasic_ftm_local_context_uses_standard_marshal_format) {
    FTM_SETUP();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream();
    HRESULT hr = IMarshal_MarshalInterface(m, s, &IID_IClassFactory, MB_UNK(0), MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    EQ(hr, S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n);
    CHECK(n > 4);
    if (n >= 4) { DWORD sig; memcpy(&sig, b, 4); EQ(sig, MB_OBJREF_SIG); }
    free(b);
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ext, 0);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}
TEST(mbasic_ftm_unmarshal_garbage_stream) {
    REQUIRE(U(CoCreateFreeThreadedMarshaler)); if (t_is_wine()) { t_skip("wine crashes"); return; }
    mb_obj_reset(1); MB_INIT_STA();
    IMarshal *m = mk_ftm(NULL, NULL); CHECK(m != NULL); if (!m) { MB_DONE(); return; }
    IStream *s = mb_stream(); void *p = (void *)1;
    HRESULT hr = IMarshal_UnmarshalInterface(m, s, &IID_IUnknown, &p);   /* empty stream */
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("FTM UnmarshalInterface on empty stream = 0x%lx", (unsigned long)hr);
    IStream_Release(s); IMarshal_Release(m); MB_DONE();
}

/* ---- aggregated: the real use, CoMarshalInterface on an object that aggregates the FTM ---- */
typedef struct FA { IStream *stm; void *ptr; DWORD tid; HRESULT hr_u, hr_call; BOOL mta; } FA;
static DWORD CALLBACK ft_worker(void *p) {
    FA *f = p; IClassFactory *cf = NULL;
    if (f->mta) MB_INIT_MTA(); else MB_INIT_STA();
    f->tid = GetCurrentThreadId();
    f->hr_u = U(CoUnmarshalInterface)(f->stm, &IID_IClassFactory, (void **)&cf);
    f->ptr = cf;
    if (cf) { f->hr_call = IClassFactory_LockServer(cf, TRUE); IClassFactory_LockServer(cf, FALSE); IClassFactory_Release(cf); }
    MB_DONE();
    return 0;
}
static void agg_cross(BOOL mta) {
    mb_obj_reset(1);
    IUnknown *inner = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(MB_UNK(0), &inner), S_OK);
    OBJ.marshal_inner = inner;
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n); MBObjRef o;
    CHECK(mb_parse_objref(b, n, &o));
    EQ(o.flags, 4);                                         /* custom marshaling */
    CHECK(IsEqualGUID(&o.clsid, &MB_CLSID_InProcFTM));
    t_note("FTM OBJREF_CUSTOM payload = %lu bytes", o.cdata_len);
    free(b);
    mb_rewind(s);
    FA f; memset(&f, 0, sizeof f); f.stm = s; f.mta = mta;
    CHECK(mb_join(mb_thread(ft_worker, &f), 10000, NULL));
    EQ(f.hr_u, S_OK);
    CHECK(f.ptr == (void *)MB_CF(0));                       /* no proxy in any apartment of the process */
    EQ(f.hr_call, S_OK);
    EQ(OBJ.last_tid, f.tid);                                /* ran directly on the caller's thread */
    EQ(OBJ.locks, 0);
    OBJ.marshal_inner = NULL;
    IStream_Release(s);
    if (inner) IUnknown_Release(inner);
}
TEST(mbasic_ftm_aggregated_unmarshal_in_mta_gives_raw_pointer) {
    FTM_SETUP(); agg_cross(TRUE); MB_DONE();
}
TEST(mbasic_ftm_aggregated_unmarshal_in_other_sta_gives_raw_pointer) {
    FTM_SETUP(); agg_cross(FALSE); MB_DONE();
}
TEST(mbasic_ftm_aggregated_local_context_standard_objref) {
    FTM_SETUP();
    IUnknown *inner = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(MB_UNK(0), &inner), S_OK);
    OBJ.marshal_inner = inner;
    IStream *s = mb_stream();
    HRESULT hr = U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    EQ(hr, S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n); MBObjRef o; int ok = mb_parse_objref(b, n, &o);
    t_note("aggregated FTM, MSHCTX_LOCAL: parse=%d flags=%lu size=%lu", ok, ok ? o.flags : 0, n);
    CHECK(n >= 4);
    free(b);
    mb_rewind(s); U(CoReleaseMarshalData)(s);
    OBJ.marshal_inner = NULL;
    IStream_Release(s); if (inner) IUnknown_Release(inner);
    MB_DONE();
}
TEST(mbasic_ftm_aggregated_release_data_balances_refs) {
    FTM_SETUP();
    IUnknown *inner = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(MB_UNK(0), &inner), S_OK);
    OBJ.marshal_inner = inner;
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    LONG r = OBJ.ref;
    CHECK(r > 1);
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ref, 1);
    OBJ.marshal_inner = NULL;
    IStream_Release(s); if (inner) IUnknown_Release(inner);
    MB_DONE();
}
TEST(mbasic_ftm_aggregated_getmarshalsizemax) {
    FTM_SETUP();
    IUnknown *inner = NULL;
    EQ(U(CoCreateFreeThreadedMarshaler)(MB_UNK(0), &inner), S_OK);
    OBJ.marshal_inner = inner;
    ULONG sz = 0, actual;
    EQ(U(CoGetMarshalSizeMax)(&sz, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    actual = (ULONG)mb_size(s);
    CHECKF(sz >= actual, "max %lu < actual %lu", sz, actual);
    t_note("aggregated FTM: GetMarshalSizeMax=%lu actual=%lu", sz, actual);
    mb_rewind(s); U(CoReleaseMarshalData)(s);
    OBJ.marshal_inner = NULL;
    IStream_Release(s); if (inner) IUnknown_Release(inner);
    MB_DONE();
}
