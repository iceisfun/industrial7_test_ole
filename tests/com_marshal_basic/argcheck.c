/* Argument validation and error HRESULTs of the marshaling API (verified against combase decomp). */
#include "mb.h"
#include <stdlib.h>

static const GUID IID_INoMarshal_ = {0xecc8691b, 0xc1db, 0x4dc0, {0x85, 0x5e, 0x65, 0xf6, 0xc5, 0x51, 0xaf, 0x49}};

/* object exposing INoMarshal (no methods) */
typedef struct NM { IUnknown u; LONG ref; } NM;
static HRESULT WINAPI NM_QI(IUnknown *i, REFIID r, void **p) {
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_INoMarshal_)) { *p = i; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI NM_AddRef(IUnknown *i) { return 2; }
static ULONG WINAPI NM_Release(IUnknown *i) { return 1; }
static const IUnknownVtbl nm_vtbl = { NM_QI, NM_AddRef, NM_Release };
static NM g_nm = { { &nm_vtbl }, 1 };

/* plain IUnknown used as a destination context object */
static HRESULT WINAPI DC_QI(IUnknown *i, REFIID r, void **p) { if (IsEqualGUID(r, &IID_IUnknown)) { *p = i; return S_OK; } *p = NULL; return E_NOINTERFACE; }
static const IUnknownVtbl dc_vtbl = { DC_QI, NM_AddRef, NM_Release };
static IUnknown g_dc = { &dc_vtbl };

static void write_bytes(IStream *s, const void *b, ULONG n) { IStream_Write(s, b, n, NULL); mb_rewind(s); }

/* ---------------- CoMarshalInterface ---------------- */

TEST(mbasic_args_marshal_null_stream) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    if (t_is_wine()) { t_skip("wine"); MB_DONE(); return; }
    EQ(U(CoMarshalInterface)(NULL, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_marshal_null_object) {
    REQUIRE(U(CoMarshalInterface)); MB_INIT_STA();
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IUnknown, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), MB_E_INVALIDARG);
    EQ(mb_size(s), 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_marshal_bad_destctx) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    static const DWORD bad[] = { 6, 7, 8, 100, 0x10000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IStream *s = mb_stream();
        HRESULT hr = U(CoMarshalInterface)(s, &IID_IUnknown, MB_UNK(0), bad[i], NULL, MSHLFLAGS_NORMAL);
        CHECKF(hr == MB_E_INVALIDARG, "ctx %lu hr=0x%lx", bad[i], (unsigned long)hr);
        EQ(mb_size(s), 0);
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_marshal_bad_flags) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    static const DWORD bad[] = { 0x40, 0x80, 0x100, 0x8000, 0x80000, 0x1000000, 0x40000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IStream *s = mb_stream();
        HRESULT hr = U(CoMarshalInterface)(s, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, bad[i]);
        CHECKF(hr == MB_E_INVALIDARG, "flags 0x%lx hr=0x%lx", bad[i], (unsigned long)hr);
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_marshal_destctx_ptr_requires_differentmachine) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_INPROC };
    for (unsigned i = 0; i < 3; i++) {
        IStream *s = mb_stream();
        HRESULT hr = U(CoMarshalInterface)(s, &IID_IUnknown, MB_UNK(0), ctxs[i], &g_dc, MSHLFLAGS_NORMAL);
        /* decomp: pvDestContext != NULL with dwDestContext != MSHCTX_DIFFERENTMACHINE -> E_INVALIDARG (marshal.hxx) */
        CHECKF(hr == MB_E_INVALIDARG, "ctx %lu with pvDestContext hr=0x%lx", ctxs[i], (unsigned long)hr);
        EQ(mb_size(s), 0);
        IStream_Release(s);
    }
    MB_DONE();
}
/* decomp: dwDestContext < 6 passes argument validation (CROSSCTX etc. included), 6 and above is E_INVALIDARG */
TEST(mbasic_args_marshal_extended_contexts_not_invalidarg) {
    REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoGetMarshalSizeMax)); REQUIRE(U(CoReleaseMarshalData)); mb_obj_reset(1); MB_INIT_STA();
    for (DWORD ctx = 4; ctx <= 5; ctx++) {
        IStream *s = mb_stream(); ULONG sz = 0;
        HRESULT h1 = U(CoGetMarshalSizeMax)(&sz, &IID_IClassFactory, MB_UNK(0), ctx, NULL, MSHLFLAGS_NORMAL);
        HRESULT h2 = U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), ctx, NULL, MSHLFLAGS_NORMAL);
        CHECKF(h1 != MB_E_INVALIDARG, "getsize ctx %lu hr=0x%lx", ctx, (unsigned long)h1);
        CHECKF(h2 != MB_E_INVALIDARG, "marshal ctx %lu hr=0x%lx", ctx, (unsigned long)h2);
        t_note("ctx %lu: getsize 0x%lx marshal 0x%lx", ctx, (unsigned long)h1, (unsigned long)h2);
        if (h2 == S_OK) { mb_rewind(s); U(CoReleaseMarshalData)(s); }
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_marshal_valid_contexts_and_flags) {
    REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoReleaseMarshalData)); mb_obj_reset(1); MB_INIT_STA();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC };
    static const DWORD fl[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK, MSHLFLAGS_NOPING };
    for (unsigned c = 0; c < 4; c++) for (unsigned f = 0; f < 4; f++) {
        IStream *s = mb_stream();
        HRESULT hr = U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), ctxs[c], NULL, fl[f]);
        CHECKF(hr == S_OK, "ctx %lu flags %lu hr=0x%lx", ctxs[c], fl[f], (unsigned long)hr);
        if (hr == S_OK) { mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK); }
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_marshal_unsupported_interface) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    IStream *s = mb_stream();
    /* object does not implement the IID: QI fails -> E_NOINTERFACE */
    EQ(U(CoMarshalInterface)(s, &MB_IID_Bogus, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), MB_E_NOINTERFACE);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_marshal_nomarshal_object) {
    REQUIRE(U(CoMarshalInterface)); MB_INIT_STA();
    IStream *s = mb_stream();
    HRESULT hr = U(CoMarshalInterface)(s, &IID_IUnknown, &g_nm.u, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    EQ(hr, 0x80004021);        /* CO_E_NOT_SUPPORTED: object QI'd successfully for INoMarshal */
    EQ(mb_size(s), 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_marshal_requires_com_init_or_implicit) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1);
    IStream *s = mb_stream();
    HRESULT hr = U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    t_note("CoMarshalInterface without CoInitialize = 0x%lx", (unsigned long)hr);
    if (hr == S_OK) { mb_rewind(s); U(CoReleaseMarshalData)(s); }
    IStream_Release(s);
}

/* ---------------- CoGetMarshalSizeMax ---------------- */

TEST(mbasic_args_getsize_invalid) {
    REQUIRE(U(CoGetMarshalSizeMax)); if (t_is_wine()) { t_skip("wine crashes on invalid args"); return; } mb_obj_reset(1); MB_INIT_STA();
    ULONG sz = 0x1234;
    EQ(U(CoGetMarshalSizeMax)(NULL, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), MB_E_INVALIDARG);
    EQ(U(CoGetMarshalSizeMax)(&sz, &IID_IUnknown, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), MB_E_INVALIDARG);
    EQ(U(CoGetMarshalSizeMax)(&sz, &IID_IUnknown, MB_UNK(0), 6, NULL, MSHLFLAGS_NORMAL), MB_E_INVALIDARG);
    EQ(U(CoGetMarshalSizeMax)(&sz, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, 0x40), MB_E_INVALIDARG);
    EQ(sz, 0x1234);
    MB_DONE();
}
TEST(mbasic_args_getsize_bounds_actual_marshal) {
    REQUIRE(U(CoGetMarshalSizeMax)); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoReleaseMarshalData));
    mb_obj_reset(1); MB_INIT_STA();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC };
    static const DWORD fl[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK, MSHLFLAGS_NOPING };
    for (unsigned c = 0; c < 4; c++) for (unsigned f = 0; f < 4; f++) {
        ULONG sz = 0; IStream *s = mb_stream();
        HRESULT h1 = U(CoGetMarshalSizeMax)(&sz, &IID_IClassFactory, MB_UNK(0), ctxs[c], NULL, fl[f]);
        EQ(h1, S_OK);
        HRESULT h2 = U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), ctxs[c], NULL, fl[f]);
        EQ(h2, S_OK);
        CHECKF(mb_size(s) <= sz, "ctx %lu fl %lu: marshaled %llu > max %lu", ctxs[c], fl[f], mb_size(s), sz);
        CHECKF(sz >= 68, "max size %lu smaller than a minimal standard OBJREF", sz);
        if (h2 == S_OK) { mb_rewind(s); U(CoReleaseMarshalData)(s); }
        IStream_Release(s);
    }
    MB_DONE();
}

/* ---------------- CoUnmarshalInterface / CoReleaseMarshalData errors ---------------- */

TEST(mbasic_args_unmarshal_null_args) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    IStream *s = mb_stream(); void *p = (void *)1;
    if (!t_is_wine()) EQ(U(CoUnmarshalInterface)(NULL, &IID_IUnknown, &p), MB_E_INVALIDARG);
    EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, NULL), MB_E_INVALIDARG);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_empty_stream_readfault) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    IStream *s = mb_stream(); void *p = (void *)1;
    EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, &p), MB_STG_E_READFAULT);
    EQ(p, 0);                          /* *ppv is cleared before anything else */
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_short_header_readfault) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    for (ULONG n = 1; n < 24; n += 7) {
        BYTE b[24]; memset(b, 0, sizeof b); memcpy(b, "MEOW", 4);
        IStream *s = mb_stream(); void *p = (void *)1;
        write_bytes(s, b, n);
        HRESULT hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
        CHECKF(hr == MB_STG_E_READFAULT, "len %lu hr=0x%lx", n, (unsigned long)hr);
        EQ(p, 0);
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_unmarshal_bad_signature) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    BYTE b[128]; memset(b, 0, sizeof b);
    memcpy(b, "WOEM", 4); *(DWORD *)(b + 4) = 1;
    IStream *s = mb_stream(); void *p = (void *)1;
    write_bytes(s, b, sizeof b);
    EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, &p), MB_RPC_E_INVALID_OBJREF);
    EQ(p, 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_bad_objref_flags) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    /* flags 0, any bit above 0x8, or contradictory combinations are RPC_E_INVALID_OBJREF (ReadObjRef) */
    static const DWORD flags[] = { 0, 3, 5, 6, 7, 9, 0xA, 0xC, 0xF, 0x10, 0x11, 0x20, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof flags / sizeof *flags; i++) {
        BYTE b[128]; memset(b, 0, sizeof b);
        DWORD sig = MB_OBJREF_SIG; memcpy(b, &sig, 4); memcpy(b + 4, &flags[i], 4);
        IStream *s = mb_stream(); void *p = (void *)1;
        write_bytes(s, b, sizeof b);
        HRESULT hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
        CHECKF(hr == MB_RPC_E_INVALID_OBJREF, "flags 0x%lx hr=0x%lx", flags[i], (unsigned long)hr);
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_args_unmarshal_truncated_standard_objref) {
    REQUIRE(U(CoUnmarshalInterface)); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoReleaseMarshalData));
    mb_obj_reset(1); MB_INIT_STA();
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    ULONG n; BYTE *d = mb_dump(s, &n);
    static const ULONG cuts[] = { 24, 30, 63, 64, 66 };
    for (unsigned i = 0; i < 5; i++) {
        if (cuts[i] >= n) continue;
        IStream *t = mb_stream(); void *p = (void *)1;
        write_bytes(t, d, cuts[i]);
        HRESULT hr = U(CoUnmarshalInterface)(t, &IID_IClassFactory, &p);
        CHECKF(hr == MB_STG_E_READFAULT, "cut %lu hr=0x%lx", cuts[i], (unsigned long)hr);
        EQ(p, 0);
        IStream_Release(t);
    }
    mb_rewind(s); U(CoReleaseMarshalData)(s);
    free(d); IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_extended_objref_unsupported_stream) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    BYTE b[128]; memset(b, 0, sizeof b);
    DWORD sig = MB_OBJREF_SIG, fl = 8; memcpy(b, &sig, 4); memcpy(b + 4, &fl, 4);
    IStream *s = mb_stream(); void *p = (void *)1;
    write_bytes(s, b, sizeof b);
    HRESULT hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("OBJREF_EXTENDED with zero payload -> 0x%lx", (unsigned long)hr);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_custom_unknown_class) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    BYTE b[24 + 24]; memset(b, 0, sizeof b);
    DWORD sig = MB_OBJREF_SIG, fl = 4; memcpy(b, &sig, 4); memcpy(b + 4, &fl, 4);
    memcpy(b + 8, &IID_IUnknown, 16);
    memcpy(b + 24, &MB_IID_Bogus, 16);      /* clsid nobody registered */
    IStream *s = mb_stream(); void *p = (void *)1;
    write_bytes(s, b, sizeof b);
    HRESULT hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    EQ(p, 0);
    t_note("custom OBJREF with unregistered CLSID -> 0x%lx", (unsigned long)hr);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_release_marshal_data_errors) {
    REQUIRE(U(CoReleaseMarshalData)); MB_INIT_STA();
    IStream *s = mb_stream();
    if (!t_is_wine()) EQ(U(CoReleaseMarshalData)(NULL), MB_E_INVALIDARG);
    EQ(U(CoReleaseMarshalData)(s), MB_STG_E_READFAULT);
    BYTE b[64]; memset(b, 0, sizeof b); memcpy(b, "XXXX", 4);
    IStream *t = mb_stream(); write_bytes(t, b, sizeof b);
    EQ(U(CoReleaseMarshalData)(t), MB_RPC_E_INVALID_OBJREF);
    IStream_Release(t); IStream_Release(s); MB_DONE();
}
TEST(mbasic_args_unmarshal_read_failure_propagates) {
    REQUIRE(U(CoUnmarshalInterface)); MB_INIT_STA();
    MBStm *w = mb_wstream_new(); void *p = (void *)1;
    w->fail_read = TRUE;
    HRESULT hr = U(CoUnmarshalInterface)(&w->s, &IID_IUnknown, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    EQ(p, 0);
    t_note("read failure STG_E_ACCESSDENIED surfaces as 0x%lx", (unsigned long)hr);
    IStream_Release(&w->s); MB_DONE();
}

/* ---------------- CoGetStandardMarshal / CoGetStdMarshalEx ---------------- */

TEST(mbasic_args_getstdmarshal_invalid) {
    REQUIRE(U(CoGetStandardMarshal)); if (t_is_wine()) { t_skip("wine crashes"); return; } mb_obj_reset(1); MB_INIT_STA();
    IMarshal *m = (IMarshal *)1;
    EQ(U(CoGetStandardMarshal)(&IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, NULL), MB_E_INVALIDARG);
    EQ(U(CoGetStandardMarshal)(&IID_IUnknown, MB_UNK(0), 6, NULL, MSHLFLAGS_NORMAL, &m), MB_E_INVALIDARG);
    EQ(U(CoGetStandardMarshal)(&IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, 0x40, &m), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_getstdmarshal_nomarshal) {
    REQUIRE(U(CoGetStandardMarshal)); MB_INIT_STA();
    IMarshal *m = (IMarshal *)1;
    EQ(U(CoGetStandardMarshal)(&IID_IUnknown, &g_nm.u, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &m), 0x80004021);
    MB_DONE();
}
TEST(mbasic_args_getstdmarshal_null_unk_gives_unmarshaler) {
    REQUIRE(U(CoGetStandardMarshal)); MB_INIT_STA();
    IMarshal *m = NULL;
    HRESULT hr = U(CoGetStandardMarshal)(&IID_IUnknown, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &m);
    EQ(hr, S_OK); CHECK(m != NULL);
    if (m) IMarshal_Release(m);
    MB_DONE();
}
TEST(mbasic_args_getstdmarshalex_invalid) {
    REQUIRE(U(CoGetStdMarshalEx)); if (t_is_wine()) { t_skip("CoGetStdMarshalEx unimplemented in wine"); return; } mb_obj_reset(1); MB_INIT_STA();
    IUnknown *o = (IUnknown *)1;
    if (!t_is_wine()) EQ(U(CoGetStdMarshalEx)(NULL, 1, &o), MB_E_INVALIDARG);
    EQ(U(CoGetStdMarshalEx)(MB_UNK(0), 0, &o), MB_E_INVALIDARG);
    EQ(U(CoGetStdMarshalEx)(MB_UNK(0), 3, &o), MB_E_INVALIDARG);
    EQ(U(CoGetStdMarshalEx)(MB_UNK(0), 1, NULL), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_getstdmarshalex_nomarshal) {
    REQUIRE(U(CoGetStdMarshalEx)); if (t_is_wine()) { t_skip("CoGetStdMarshalEx unimplemented in wine"); return; } MB_INIT_STA();
    IUnknown *o = (IUnknown *)1;
    EQ(U(CoGetStdMarshalEx)(&g_nm.u, 1, &o), 0x80004021);
    EQ(o, 0);
    MB_DONE();
}
TEST(mbasic_args_getstdmarshalex_server) {
    REQUIRE(U(CoGetStdMarshalEx)); if (t_is_wine()) { t_skip("CoGetStdMarshalEx unimplemented in wine"); return; } mb_obj_reset(1); MB_INIT_STA();
    IUnknown *o = NULL;
    HRESULT hr = U(CoGetStdMarshalEx)(MB_UNK(0), 1, &o);
    EQ(hr, S_OK); CHECK(o != NULL);
    if (o) IUnknown_Release(o);
    MB_DONE();
}

/* ---------------- lock / disconnect / handler ---------------- */

TEST(mbasic_args_lockobjectexternal_null) {
    REQUIRE(U(CoLockObjectExternal)); MB_INIT_STA();
    if (!t_is_wine()) EQ(U(CoLockObjectExternal)(NULL, TRUE, TRUE), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_disconnect_invalid) {
    REQUIRE(U(CoDisconnectObject)); mb_obj_reset(1); MB_INIT_STA();
    EQ(U(CoDisconnectObject)(NULL, 0), MB_E_INVALIDARG);
    EQ(U(CoDisconnectObject)(MB_UNK(0), 1), MB_E_INVALIDARG);     /* dwReserved must be 0 */
    EQ(U(CoDisconnectObject)(MB_UNK(0), 0xFFFFFFFF), MB_E_INVALIDARG);
    MB_DONE();
}
static DWORD CALLBACK disc_noinit(void *a) {
    HRESULT hr = U(CoDisconnectObject)(MB_UNK(0), 0);
    return (DWORD)hr;
}
TEST(mbasic_args_disconnect_uninitialized_thread) {
    REQUIRE(U(CoDisconnectObject)); mb_obj_reset(1);
    DWORD code = 0;
    mb_join(mb_thread(disc_noinit, NULL), 5000, &code);
    CHECKF(code == (DWORD)MB_CO_E_NOTINITIALIZED, "code=0x%lx", code);   /* decomp: IsApartmentInitialized()==0 */
}
TEST(mbasic_args_ishandlerconnected) {
    REQUIRE(U(CoIsHandlerConnected)); mb_obj_reset(1); MB_INIT_STA();
    EQ(U(CoIsHandlerConnected)(NULL), FALSE);
    EQ(U(CoIsHandlerConnected)(MB_UNK(0)), TRUE);       /* not a proxy -> treated as connected */
    MB_DONE();
}

/* ---------------- inter-thread helpers ---------------- */

TEST(mbasic_args_interthread_invalid) {
    REQUIRE(U(CoMarshalInterThreadInterfaceInStream)); mb_obj_reset(1); MB_INIT_STA();
    IStream *s = (IStream *)1;
    if (t_is_wine()) { t_skip("wine crashes"); MB_DONE(); return; }
    EQ(U(CoMarshalInterThreadInterfaceInStream)(NULL, MB_UNK(0), &s), MB_E_INVALIDARG);
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IUnknown, NULL, &s), MB_E_INVALIDARG);
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IUnknown, MB_UNK(0), NULL), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_getinterfaceandreleasestream_invalid_keeps_stream) {
    REQUIRE(U(CoGetInterfaceAndReleaseStream)); MB_INIT_STA();
    MBStm *w = mb_wstream_new(); void *p = (void *)1;
    EQ(U(CoGetInterfaceAndReleaseStream)(&w->s, NULL, &p), MB_E_INVALIDARG);
    EQ(U(CoGetInterfaceAndReleaseStream)(&w->s, &IID_IUnknown, NULL), MB_E_INVALIDARG);
    CHECK(!w->destroyed);                       /* stream NOT released on argument failure */
    EQ(w->ref, 1);
    IStream_Release(&w->s);
    CHECK(w->destroyed);
    if (!t_is_wine()) EQ(U(CoGetInterfaceAndReleaseStream)(NULL, &IID_IUnknown, &p), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_getinterfaceandreleasestream_releases_on_failure) {
    REQUIRE(U(CoGetInterfaceAndReleaseStream)); MB_INIT_STA();
    MBStm *w = mb_wstream_new(); void *p = (void *)1;
    IStream_AddRef(&w->s);                      /* ref 2: ours + the one the call must drop */
    HRESULT hr = U(CoGetInterfaceAndReleaseStream)(&w->s, &IID_IUnknown, &p);
    EQ(hr, MB_STG_E_READFAULT);                 /* empty stream */
    EQ(w->ref, 1);                              /* released even though unmarshal failed */
    IStream_Release(&w->s);
    MB_DONE();
}

/* ---------------- CoGetPSClsid / CoRegisterPSClsid ---------------- */

TEST(mbasic_args_getpsclsid_invalid) {
    REQUIRE(U(CoGetPSClsid)); MB_INIT_STA();
    CLSID c;
    EQ(U(CoGetPSClsid)(NULL, &c), MB_E_INVALIDARG);
    EQ(U(CoGetPSClsid)(&IID_IUnknown, NULL), MB_E_INVALIDARG);
    MB_DONE();
}
TEST(mbasic_args_getpsclsid_unregistered_iid) {
    REQUIRE(U(CoGetPSClsid)); MB_INIT_STA();
    CLSID c; memset(&c, 0x55, sizeof c);
    EQ(U(CoGetPSClsid)(&MB_IID_Bogus, &c), MB_REGDB_E_IIDNOTREG);
    MB_DONE();
}
TEST(mbasic_args_getpsclsid_idispatch) {
    REQUIRE(U(CoGetPSClsid)); MB_INIT_STA();
    CLSID c; memset(&c, 0, sizeof c);
    EQ(U(CoGetPSClsid)(&IID_IDispatch, &c), S_OK);
    static const GUID psd = {0x00020420, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    static const GUID psa = {0x00020424, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    /* decomp: PSDispatch -> CLSID_PSDispatch, PSAutomation -> CLSID_PSAutomation; registry-sourced otherwise */
    CHECK(IsEqualGUID(&c, &psd) || IsEqualGUID(&c, &psa));
    MB_DONE();
}
TEST(mbasic_args_getpsclsid_stock_interfaces) {
    REQUIRE(U(CoGetPSClsid)); MB_INIT_STA();
    const struct { const char *n; const GUID *iid; } t[] = { {"IUnknown", &IID_IUnknown}, {"IStream", &IID_IStream}, {"IStorage", &IID_IStorage},
        {"IClassFactory", &IID_IClassFactory}, {"IMarshal", &IID_IMarshal}, {"ITypeInfo", &IID_ITypeInfo}, {"IEnumUnknown", &IID_IEnumUnknown} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        CLSID c; memset(&c, 0, sizeof c);
        HRESULT hr = U(CoGetPSClsid)(t[i].iid, &c);
        WCHAR w[64]; U(StringFromGUID2)(&c, w, 64);
        t_note("CoGetPSClsid(%s) = 0x%lx %ls", t[i].n, (unsigned long)hr, w);
    }
    MB_DONE();
}
TEST(mbasic_args_registerpsclsid_roundtrip) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); MB_INIT_STA();
    CLSID c;
    HRESULT hr = U(CoRegisterPSClsid)(&MB_IID_Bogus, &MB_CLSID_Custom);
    EQ(hr, S_OK);
    memset(&c, 0, sizeof c);
    EQ(U(CoGetPSClsid)(&MB_IID_Bogus, &c), S_OK);
    CHECK(IsEqualGUID(&c, &MB_CLSID_Custom));
    MB_DONE();
}
TEST(mbasic_args_registerpsclsid_survives_uninit_in_process) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid));
    static const GUID iid2 = {0x6f1e7a10, 0x4d5b, 0x4c3a, {0x9a, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x04}};
    MB_INIT_STA();
    EQ(U(CoRegisterPSClsid)(&iid2, &MB_CLSID_Handler), S_OK);
    MB_DONE();
    MB_INIT_STA();
    CLSID c; HRESULT hr = U(CoGetPSClsid)(&iid2, &c);
    t_note("PS registration after CoUninitialize/CoInitialize: hr=0x%lx", (unsigned long)hr);
    MB_DONE();
}
