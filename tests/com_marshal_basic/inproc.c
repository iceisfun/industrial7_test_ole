/* In-process standard marshaling: lifetime (external connections, locks), CoLockObjectExternal, CoDisconnectObject,
 * inter-thread helpers and cross-apartment calls through the resulting proxies. */
#include "mb.h"
#include <stdlib.h>

#define OBJ g_obj[0]
#define SETUP_EC() REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface)); REQUIRE(U(CoReleaseMarshalData)); mb_obj_reset(1); MB_INIT_STA()

static HRESULT marshal0(IStream *s, DWORD fl, DWORD ctx) { return U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), ctx, NULL, fl); }

/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_normal_marshal_then_release) {
    SETUP_EC();
    IStream *s = mb_stream();
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    CHECK(OBJ.ext > 0);                       /* IExternalConnection::AddConnection(EXTCONN_STRONG) */
    CHECK(OBJ.ref > 1);                       /* stub manager holds the object */
    mb_rewind(s);
    EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ext, 0);
    CHECK(OBJ.last_close);                    /* fLastReleaseCloses */
    EQ(OBJ.ref, 1);
    IStream_Release(s); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_normal_marshal_unmarshal_release) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *cf = NULL;
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&cf), S_OK);
    CHECK(cf != NULL);
    EQ(OBJ.ext, 0);                           /* the strong marshal reference was consumed by unmarshal */
    CHECK(!OBJ.last_close);
    t_note("same-apartment unmarshal returns %s pointer", cf == MB_CF(0) ? "the original" : "a different");
    LONG refs_mid = OBJ.ref;
    if (cf) IClassFactory_Release(cf);
    EQ(OBJ.ext, 0);
    CHECKF(OBJ.ref <= refs_mid, "ref grew on release");
    EQ(OBJ.ref, 1);
    IStream_Release(s); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_normal_unmarshal_twice_fails) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *c1 = NULL, *c2 = (IClassFactory *)1;
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c1), S_OK);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c2), MB_CO_E_OBJNOTCONNECTED);
    EQ(c2, 0);
    if (c1) IClassFactory_Release(c1);
    EQ(OBJ.ref, 1);
    IStream_Release(s); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_unmarshal_unsupported_iid_releases_marshal_data) {
    SETUP_EC();
    IStream *s = mb_stream(); IUnknown *u = (IUnknown *)1;
    EQ(U(CoMarshalInterface)(s, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IParseDisplayName, (void **)&u), MB_E_NOINTERFACE);
    EQ(u, 0);
    EQ(OBJ.ext, 0);                           /* Wine (verified on Windows): the failed unmarshal still consumed the marshal data */
    EQ(OBJ.ref, 1);
    mb_rewind(s);
    t_note("CoReleaseMarshalData after failed unmarshal = 0x%lx", (unsigned long)U(CoReleaseMarshalData)(s));
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_release_marshal_data_twice) {
    SETUP_EC();
    IStream *s = mb_stream();
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    mb_rewind(s);
    EQ(U(CoReleaseMarshalData)(s), S_OK);
    mb_rewind(s);
    HRESULT hr = U(CoReleaseMarshalData)(s);
    t_note("second CoReleaseMarshalData = 0x%lx", (unsigned long)hr);
    EQ(OBJ.ext, 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_marshal_n_times_release_all) {
    SETUP_EC();
    IStream *s[8];
    for (int i = 0; i < 8; i++) { s[i] = mb_stream(); EQ(marshal0(s[i], MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK); }
    CHECK(OBJ.ext > 0);
    for (int i = 0; i < 8; i++) { mb_rewind(s[i]); EQ(U(CoReleaseMarshalData)(s[i]), S_OK); }
    EQ(OBJ.ext, 0);
    EQ(OBJ.ref, 1);
    for (int i = 0; i < 8; i++) IStream_Release(s[i]);
    MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_tablestrong) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *c1 = NULL, *c2 = NULL;
    EQ(marshal0(s, MSHLFLAGS_TABLESTRONG, MSHCTX_INPROC), S_OK);
    CHECK(OBJ.ext > 0); CHECK(OBJ.ref > 1);
    mb_rewind(s); EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c1), S_OK);
    mb_rewind(s); EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c2), S_OK);   /* table entries can be unmarshaled repeatedly */
    if (c1) IClassFactory_Release(c1);
    if (c2) IClassFactory_Release(c2);
    CHECK(OBJ.ref > 1);                       /* the table still keeps it alive */
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ext, 0); CHECK(OBJ.last_close);
    EQ(OBJ.ref, 1);
    IStream_Release(s); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_tableweak) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *c1 = NULL, *c2 = NULL;
    EQ(marshal0(s, MSHLFLAGS_TABLEWEAK, MSHCTX_INPROC), S_OK);
    EQ(OBJ.ext, 0);                           /* weak: no strong external connection */
    mb_rewind(s); EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c1), S_OK);
    mb_rewind(s); EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c2), S_OK);
    t_note("TABLEWEAK after 2 unmarshals: ext=%ld weak=%ld ref=%ld", OBJ.ext, OBJ.ext_weak, OBJ.ref);
    if (c1) IClassFactory_Release(c1);
    if (c2) IClassFactory_Release(c2);
    EQ(OBJ.ext, 0);
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ext, 0);
    t_note("TABLEWEAK end: ref=%ld locks=%ld", OBJ.ref, OBJ.locks);
    U(CoDisconnectObject)(MB_UNK(0), 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_noping) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *c1 = NULL;
    EQ(marshal0(s, MSHLFLAGS_NOPING, MSHCTX_INPROC), S_OK);
    mb_rewind(s); EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&c1), S_OK);
    if (c1) IClassFactory_Release(c1);
    t_note("NOPING: ext=%ld ref=%ld", OBJ.ext, OBJ.ref);
    U(CoDisconnectObject)(MB_UNK(0), 0);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_object_without_externalconnection) {
    REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoReleaseMarshalData)); mb_obj_reset(0); MB_INIT_STA();
    IStream *s = mb_stream();
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    t_note("object without IExternalConnection: QI(IExternalConnection) calls=%lu", OBJ.qi_extconn);
    CHECK(OBJ.ref > 1);
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(OBJ.ref, 1);
    EQ(OBJ.add_conn, 0);
    IStream_Release(s); MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_lockobjectexternal_basic) {
    SETUP_EC();
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, TRUE), S_OK);
    CHECK(OBJ.ext > 0); CHECK(OBJ.ref > 1);
    EQ(U(CoLockObjectExternal)(MB_UNK(0), FALSE, TRUE), S_OK);
    EQ(OBJ.ext, 0); CHECK(OBJ.last_close);
    EQ(OBJ.ref, 1);
    MB_DONE();
}
TEST(mbasic_life_lockobjectexternal_no_last_unlock_release) {
    SETUP_EC();
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, FALSE), S_OK);
    CHECK(OBJ.ext > 0);
    EQ(U(CoLockObjectExternal)(MB_UNK(0), FALSE, FALSE), S_OK);
    EQ(OBJ.ext, 0); CHECK(!OBJ.last_close);
    EQ(OBJ.ref, 1);
    MB_DONE();
}
TEST(mbasic_life_lockobjectexternal_nested) {
    SETUP_EC();
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, TRUE), S_OK);
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, TRUE), S_OK);
    EQ(U(CoLockObjectExternal)(MB_UNK(0), FALSE, TRUE), S_OK);
    CHECK(OBJ.ext > 0);                       /* still one lock outstanding */
    EQ(U(CoLockObjectExternal)(MB_UNK(0), FALSE, TRUE), S_OK);
    EQ(OBJ.ext, 0);
    EQ(OBJ.ref, 1);
    MB_DONE();
}
TEST(mbasic_life_lockobjectexternal_with_marshal) {
    SETUP_EC();
    IStream *s = mb_stream();
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, TRUE), S_OK);
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
    CHECK(OBJ.ext > 0);                       /* the explicit lock keeps the stub alive */
    CHECK(OBJ.ref > 1);
    EQ(U(CoLockObjectExternal)(MB_UNK(0), FALSE, TRUE), S_OK);
    EQ(OBJ.ext, 0); EQ(OBJ.ref, 1);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_unlock_without_lock) {
    SETUP_EC();
    HRESULT hr = U(CoLockObjectExternal)(MB_UNK(0), FALSE, TRUE);
    t_note("CoLockObjectExternal(FALSE) with nothing locked = 0x%lx", (unsigned long)hr);
    EQ(OBJ.ref, 1);
    MB_DONE();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_life_disconnect_releases_locks_not_ext_count) {
    SETUP_EC();
    EQ(U(CoLockObjectExternal)(MB_UNK(0), TRUE, TRUE), S_OK);
    CHECK(OBJ.ref > 1);
    EQ(U(CoDisconnectObject)(MB_UNK(0), 0), S_OK);
    EQ(OBJ.ref, 1);                           /* stub manager gone */
    CHECK(OBJ.ext > 0);                       /* no ReleaseConnection is sent on disconnect */
    MB_DONE();
}
TEST(mbasic_life_disconnect_unmarshaled_object_is_ok) {
    SETUP_EC();
    EQ(U(CoDisconnectObject)(MB_UNK(0), 0), S_OK);   /* never marshaled: nothing to do */
    EQ(OBJ.ref, 1);
    MB_DONE();
}
TEST(mbasic_life_disconnect_after_marshal_invalidates_data) {
    SETUP_EC();
    IStream *s = mb_stream(); IClassFactory *cf = (IClassFactory *)1;
    EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    EQ(U(CoDisconnectObject)(MB_UNK(0), 0), S_OK);
    EQ(OBJ.ref, 1);
    mb_rewind(s);
    HRESULT hr = U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&cf);
    t_note("unmarshal after CoDisconnectObject = 0x%lx ptr=%p", (unsigned long)hr, cf);
    if (SUCCEEDED(hr) && cf) IClassFactory_Release(cf);
    mb_rewind(s); U(CoReleaseMarshalData)(s);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_life_remarshal_after_release) {
    SETUP_EC();
    for (int i = 0; i < 3; i++) {
        IStream *s = mb_stream();
        EQ(marshal0(s, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
        mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
        EQ(OBJ.ext, 0); EQ(OBJ.ref, 1);
        IStream_Release(s);
    }
    MB_DONE();
}
TEST(mbasic_life_marshal_two_objects_independent) {
    SETUP_EC();
    IStream *a = mb_stream(), *b = mb_stream();
    EQ(marshal0(a, MSHLFLAGS_NORMAL, MSHCTX_INPROC), S_OK);
    EQ(U(CoMarshalInterface)(b, &IID_IClassFactory, MB_UNK(1), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    mb_rewind(a); EQ(U(CoReleaseMarshalData)(a), S_OK);
    EQ(g_obj[0].ext, 0); CHECK(g_obj[1].ext > 0);
    mb_rewind(b); EQ(U(CoReleaseMarshalData)(b), S_OK);
    EQ(g_obj[1].ext, 0);
    IStream_Release(a); IStream_Release(b); MB_DONE();
}
TEST(mbasic_life_marshal_destctx_variants_release) {
    SETUP_EC();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC };
    for (int i = 0; i < 4; i++) {
        IStream *s = mb_stream();
        EQ(marshal0(s, MSHLFLAGS_NORMAL, ctxs[i]), S_OK);
        CHECK(OBJ.ext > 0);
        mb_rewind(s); EQ(U(CoReleaseMarshalData)(s), S_OK);
        EQ(OBJ.ext, 0); EQ(OBJ.ref, 1);
        IStream_Release(s);
    }
    MB_DONE();
}

/* ---------------------------------------------------------------- cross-apartment */

typedef struct XA { IStream *stm; HRESULT hr_get, hr_call, hr_call2, hr_unlock; void *got; DWORD tid; HANDLE got_ev, go_ev; BOOL mta; LONG locks_seen; } XA;

static DWORD CALLBACK xa_worker(void *p) {
    XA *x = p; IClassFactory *cf = NULL;
    if (x->mta) MB_INIT_MTA(); else MB_INIT_STA();
    x->tid = GetCurrentThreadId();
    x->hr_get = U(CoGetInterfaceAndReleaseStream)(x->stm, &IID_IClassFactory, (void **)&cf);
    x->got = cf;
    if (cf) {
        x->hr_call = IClassFactory_LockServer(cf, TRUE);
        x->locks_seen = OBJ.locks;
        x->hr_unlock = IClassFactory_LockServer(cf, FALSE);
        if (x->got_ev) { SetEvent(x->got_ev); if (x->go_ev) WaitForSingleObject(x->go_ev, 10000); }
        x->hr_call2 = IClassFactory_LockServer(cf, TRUE);
        IClassFactory_Release(cf);
    } else if (x->got_ev) SetEvent(x->got_ev);
    MB_DONE();
    return 0;
}

static void run_interthread(BOOL mta, const char *what) {
    XA x; memset(&x, 0, sizeof x); x.mta = mta;
    HRESULT hr = U(CoMarshalInterThreadInterfaceInStream)(&IID_IClassFactory, MB_UNK(0), &x.stm);
    EQ(hr, S_OK); CHECK(x.stm != NULL);
    if (!x.stm) return;
    DWORD code;
    BOOL ok = mb_join(mb_thread(xa_worker, &x), 10000, &code);
    CHECKF(ok, "%s worker timed out", what);
    EQ(x.hr_get, S_OK);
    CHECK(x.got != NULL);
    CHECK(x.got != (void *)MB_CF(0));                 /* other apartment: a proxy, not the object */
    EQ(x.hr_call, S_OK);                              /* call crossed back into the owning STA */
    EQ(x.hr_unlock, S_OK);
    EQ(x.locks_seen, 1);
    EQ(OBJ.locks, 1);                                 /* worker's second LockServer(TRUE) */
    EQ(OBJ.last_tid, GetCurrentThreadId());           /* executed in the object's apartment */
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_xapt_interthread_to_mta) {
    SETUP_EC();
    run_interthread(TRUE, "mta");
    MB_DONE();
}
TEST(mbasic_xapt_interthread_to_sta) {
    SETUP_EC();
    run_interthread(FALSE, "sta");
    MB_DONE();
}
TEST(mbasic_xapt_interthread_stream_consumed_after_get) {
    SETUP_EC();
    /* the stream produced by CoMarshalInterThreadInterfaceInStream is a marshal stream positioned at its start */
    IStream *s = NULL;
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IClassFactory, MB_UNK(0), &s), S_OK);
    CHECK(s != NULL);
    if (s) {
        EQ(mb_pos(s), 0);                             /* rewound for the reader */
        CHECK(mb_size(s) >= 68);
        ULONG n; BYTE *b = mb_dump(s, &n); MBObjRef o;
        CHECK(mb_parse_objref(b, n, &o));
        EQ(o.signature, MB_OBJREF_SIG); EQ(o.flags, 1);
        free(b);
        IClassFactory *cf = NULL;
        EQ(U(CoGetInterfaceAndReleaseStream)(s, &IID_IClassFactory, (void **)&cf), S_OK);
        if (cf) IClassFactory_Release(cf);
        EQ(OBJ.ref, 1);
    }
    MB_DONE();
}
TEST(mbasic_xapt_interthread_same_apartment) {
    SETUP_EC();
    IStream *s = NULL; IClassFactory *cf = NULL;
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IClassFactory, MB_UNK(0), &s), S_OK);
    EQ(U(CoGetInterfaceAndReleaseStream)(s, &IID_IClassFactory, (void **)&cf), S_OK);
    t_note("same-apartment inter-thread get returns %s", cf == MB_CF(0) ? "the original pointer" : "a different pointer");
    if (cf) IClassFactory_Release(cf);
    MB_DONE();
}
TEST(mbasic_xapt_interthread_unsupported_iid) {
    SETUP_EC();
    IStream *s = NULL; IUnknown *u = (IUnknown *)1;
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IUnknown, MB_UNK(0), &s), S_OK);
    EQ(U(CoGetInterfaceAndReleaseStream)(s, &IID_IParseDisplayName, (void **)&u), MB_E_NOINTERFACE);
    EQ(u, 0);
    MB_DONE();
}
TEST(mbasic_xapt_interthread_unsupported_marshal_iid) {
    SETUP_EC();
    IStream *s = (IStream *)1;
    HRESULT hr = U(CoMarshalInterThreadInterfaceInStream)(&MB_IID_Bogus, MB_UNK(0), &s);
    EQ(hr, MB_E_NOINTERFACE);
    t_note("*ppstm after failure = %p", s);
    MB_DONE();
}
/* disconnect while a proxy exists in another apartment */
static DWORD CALLBACK xa_disc_worker(void *p) {
    XA *x = p; IClassFactory *cf = NULL;
    MB_INIT_MTA();
    x->hr_get = U(CoGetInterfaceAndReleaseStream)(x->stm, &IID_IClassFactory, (void **)&cf);
    SetEvent(x->got_ev);
    WaitForSingleObject(x->go_ev, 10000);
    if (cf) { x->hr_call = IClassFactory_LockServer(cf, TRUE); x->hr_call2 = IClassFactory_LockServer(cf, FALSE); IClassFactory_Release(cf); }
    MB_DONE();
    return 0;
}
TEST(mbasic_xapt_disconnect_gives_rpc_e_disconnected) {
    SETUP_EC();
    XA x; memset(&x, 0, sizeof x);
    x.got_ev = CreateEventA(NULL, FALSE, FALSE, NULL); x.go_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IClassFactory, MB_UNK(0), &x.stm), S_OK);
    HANDLE th = mb_thread(xa_disc_worker, &x);
    CHECK(mb_wait_pump(x.got_ev, 10000));
    EQ(x.hr_get, S_OK);
    EQ(U(CoDisconnectObject)(MB_UNK(0), 0), S_OK);
    SetEvent(x.go_ev);
    CHECK(mb_join(th, 10000, NULL));
    CHECKF(x.hr_call == MB_RPC_E_DISCONNECTED || x.hr_call == MB_CO_E_OBJNOTCONNECTED, "call after disconnect hr=0x%lx", (unsigned long)x.hr_call);
    t_note("call through proxy after CoDisconnectObject = 0x%lx", (unsigned long)x.hr_call);
    EQ(OBJ.locks, 0);                                  /* the object never saw the call */
    CloseHandle(x.got_ev); CloseHandle(x.go_ev);
    MB_DONE();
}
TEST(mbasic_xapt_proxy_identity_and_iunknown) {
    SETUP_EC();
    XA x; memset(&x, 0, sizeof x); x.mta = TRUE;
    EQ(U(CoMarshalInterThreadInterfaceInStream)(&IID_IClassFactory, MB_UNK(0), &x.stm), S_OK);
    CHECK(mb_join(mb_thread(xa_worker, &x), 10000, NULL));
    EQ(x.hr_get, S_OK);
    for (int i = 0; i < 100 && OBJ.ref != 1; i++) { MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&m); Sleep(20); }
    EQ(OBJ.ref, 1);                                   /* proxy gone, stub gone: no leaked references (bounded wait) */
    MB_DONE();
}
