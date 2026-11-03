/* CoGetCurrentProcess, CoGetCurrentLogicalThreadId, CoGetContextToken, CoGetObjectContext (+IComThreadingInfo,
 * IContextCallback), CoBuildVersion, CoIsOle1Class, CoFreeUnusedLibraries(Ex), CoFreeAllLibraries, CoInitializeWOW. */
#include "cia.h"

/* ----- CoGetCurrentProcess ----------------------------------------------------------------------- */
TEST(cia_ctx_current_process_basic)
{
    DWORD a, b;
    REQUIRE(U(CoGetCurrentProcess));
    a = U(CoGetCurrentProcess)();
    CHECK(a != 0);
    CHECK(a != GetCurrentProcessId());
    CHECK(a != GetCurrentThreadId());
    b = U(CoGetCurrentProcess)();
    EXPECT_EQ(a, b);                 /* stable for one thread */
}

TEST(cia_ctx_current_process_works_without_com_init)
{
    DWORD a;
    REQUIRE(U(CoGetCurrentProcess)); REQUIRE(U(CoGetApartmentType));
    CIA_CHECK_NOTINIT();
    a = U(CoGetCurrentProcess)();
    CHECK(a != 0);
    CIA_CHECK_NOTINIT();             /* it must not initialize COM as a side effect */
}

TEST(cia_ctx_current_process_stable_over_init_cycle)
{
    DWORD a, b, c;
    REQUIRE(U(CoGetCurrentProcess)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    a = U(CoGetCurrentProcess)();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    b = U(CoGetCurrentProcess)();
    cUninit();
    c = U(CoGetCurrentProcess)();
    EXPECT_EQ(a, b);
    if (b != c) t_note("CoGetCurrentProcess changed across CoUninitialize: %lu -> %lu", b, c);
}

static void w_curproc(void *p) { *(DWORD *)p = U(CoGetCurrentProcess)(); }
TEST(cia_ctx_current_process_differs_between_threads)
{
    cia_w a, b; DWORD ia = 0, ib = 0, ia2 = 0, im = 0;
    REQUIRE(U(CoGetCurrentProcess));
    if (!cia_w_start(&a)) return;
    if (!cia_w_start(&b)) { cia_w_stop(&a); return; }
    im = U(CoGetCurrentProcess)();
    CHECK(cia_w_call(&a, w_curproc, &ia));
    CHECK(cia_w_call(&b, w_curproc, &ib));
    CHECK(cia_w_call(&a, w_curproc, &ia2));
    CHECK(ia != 0 && ib != 0 && im != 0);
    CHECK(ia != ib);
    CHECK(ia != im);
    CHECK(ib != im);
    EXPECT_EQ(ia, ia2);
    cia_w_stop(&a); cia_w_stop(&b);
}

/* ----- CoGetCurrentLogicalThreadId ----------------------------------------------------------- */
TEST(cia_ctx_logical_thread_null)
{
    REQUIRE(U(CoGetCurrentLogicalThreadId));
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(NULL), CIA_E_INVALIDARG);
}

TEST(cia_ctx_logical_thread_basic)
{
    GUID a, b;
    REQUIRE(U(CoGetCurrentLogicalThreadId));
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&a), S_OK);
    CHECK(!IsEqualGUID(&a, &CIA_GUID_NULL));
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&b), S_OK);
    CHECK(IsEqualGUID(&a, &b));
}

TEST(cia_ctx_logical_thread_works_without_com_init)
{
    GUID a;
    REQUIRE(U(CoGetCurrentLogicalThreadId)); REQUIRE(U(CoGetApartmentType));
    memset(&a, 0, sizeof a);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&a), S_OK);
    CHECK(!IsEqualGUID(&a, &CIA_GUID_NULL));
    CIA_CHECK_NOTINIT();
}

static void w_logid(void *p) { memset(p, 0, sizeof(GUID)); *((HRESULT *)((char *)p + sizeof(GUID))) = U(CoGetCurrentLogicalThreadId)((GUID *)p); }
TEST(cia_ctx_logical_thread_differs_between_threads)
{
    struct { GUID g; HRESULT hr; } x, y; GUID m;
    REQUIRE(U(CoGetCurrentLogicalThreadId));
    memset(&x, 0, sizeof x); memset(&y, 0, sizeof y); memset(&m, 0, sizeof m);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&m), S_OK);
    CHECK(cia_run(w_logid, &x)); CHECK(cia_run(w_logid, &y));
    EXPECT_HR(x.hr, S_OK); EXPECT_HR(y.hr, S_OK);
    CHECK(!IsEqualGUID(&x.g, &CIA_GUID_NULL) && !IsEqualGUID(&y.g, &CIA_GUID_NULL));
    CHECK(!IsEqualGUID(&x.g, &m));
    CHECK(!IsEqualGUID(&x.g, &y.g));   /* each thread gets a freshly generated id */
}

TEST(cia_ctx_logical_thread_stable_across_sta_init_cycle)
{
    GUID a, b;
    REQUIRE(U(CoGetCurrentLogicalThreadId)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&a), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&b), S_OK);
    cUninit();
    CHECK(IsEqualGUID(&a, &b));
}

/* ----- CoGetContextToken ------------------------------------------------------------------------ */
TEST(cia_ctx_token_null_pointer)
{
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(U(CoGetContextToken)(NULL), CIA_E_POINTER);      /* checked before the initialization state */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(NULL), CIA_E_POINTER);
    cUninit();
}

TEST(cia_ctx_token_uninitialized)
{
    ULONG_PTR tok = 0xdeadbeef;
    REQUIRE(U(CoGetContextToken));
    EXPECT_HR(U(CoGetContextToken)(&tok), CIA_NOTINIT);
    EXPECT_EQ(tok, 0xdeadbeef);                                 /* output untouched on failure */
}

TEST(cia_ctx_token_sta_stable)
{
    ULONG_PTR a = 0, b = 0;
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&a), S_OK);
    CHECK(a != 0);
    EXPECT_HR(U(CoGetContextToken)(&b), S_OK);
    EXPECT_EQ(a, b);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);          /* nested init does not change the context */
    b = 0; EXPECT_HR(U(CoGetContextToken)(&b), S_OK);
    EXPECT_EQ(a, b);
    cUninit(); cUninit();
}

TEST(cia_ctx_token_mta_nonzero)
{
    ULONG_PTR a = 0, b = 0;
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&a), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&b), S_OK);
    CHECK(a != 0); EXPECT_EQ(a, b);
    cUninit();
}

static void w_token(void *p) { struct { HRESULT hr; ULONG_PTR tok; } *r = p; r->tok = 0; r->hr = U(CoGetContextToken)(&r->tok); }
static void w_token_sta(void *p) { struct { HRESULT hr; ULONG_PTR tok; } *r = p; cInitEx(NULL, CIA_COINIT_STA); w_token(p); cUninit(); }
TEST(cia_ctx_token_differs_between_stas)
{
    struct { HRESULT hr; ULONG_PTR tok; } r; ULONG_PTR m = 0;
    cia_w a;
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&m), S_OK);
    if (!cia_w_start(&a)) { cUninit(); return; }
    memset(&r, 0, sizeof r);
    CHECK(cia_w_call(&a, w_token_sta, &r));
    EXPECT_HR(r.hr, S_OK);
    CHECK(r.tok != 0 && r.tok != m);        /* every apartment has its own default context */
    cia_w_stop(&a);
    cUninit();
}

static void w_mta_token_hold(void *p) { struct { HRESULT hr; ULONG_PTR tok; } *r = p; cInitEx(NULL, CIA_COINIT_MTA); w_token(p); }
TEST(cia_ctx_token_shared_by_all_mta_threads)
{
    struct { HRESULT hr; ULONG_PTR tok; } ra, rb; ULONG_PTR m = 0;
    cia_w a, b;
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    if (!cia_w_start(&b)) { cia_w_stop(&a); return; }
    memset(&ra, 0, sizeof ra); memset(&rb, 0, sizeof rb);
    CHECK(cia_w_call(&a, w_mta_token_hold, &ra));
    CHECK(cia_w_call(&b, w_mta_token_hold, &rb));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&m), S_OK);
    EXPECT_HR(ra.hr, S_OK); EXPECT_HR(rb.hr, S_OK);
    CHECK(ra.tok != 0);
    EXPECT_EQ(ra.tok, rb.tok);              /* one MTA, one default context */
    EXPECT_EQ(ra.tok, m);
    cUninit();
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL)); CHECK(cia_w_call(&b, cia_cmd_uninit, NULL));
    cia_w_stop(&a); cia_w_stop(&b);
}

TEST(cia_ctx_token_sta_vs_mta_differ)
{
    ULONG_PTR s = 0; struct { HRESULT hr; ULONG_PTR tok; } r; cia_w a;
    REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    memset(&r, 0, sizeof r);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&s), S_OK);
    CHECK(cia_w_call(&a, w_mta_token_hold, &r));    /* both apartments alive at the same time */
    EXPECT_HR(r.hr, S_OK);
    CHECK(s != 0 && r.tok != 0 && s != r.tok);
    CHECK(cia_w_call(&a, cia_cmd_uninit, NULL));
    cUninit();
    cia_w_stop(&a);
}

/* ----- CoGetObjectContext ----------------------------------------------------------------------- */
TEST(cia_ctx_objctx_uninitialized)
{
    IComThreadingInfo *ti = (IComThreadingInfo *)(ULONG_PTR)0x1; void *p = (void *)(ULONG_PTR)0x1;
    REQUIRE(U(CoGetObjectContext));
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&ti), CIA_NOTINIT);
    CHECK(ti == NULL);                           /* *ppv is cleared first, even on failure */
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IObjContext, &p), CIA_NOTINIT);
    CHECK(p == NULL);
}

TEST(cia_ctx_objctx_unknown_iid)
{
    void *p = (void *)(ULONG_PTR)0x1;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IStream, &p), CIA_E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)(ULONG_PTR)0x1;
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IClassFactory, &p), CIA_E_NOINTERFACE);
    CHECK(p == NULL);
    cUninit();
}

TEST(cia_ctx_objctx_supported_interfaces_sta)
{
    static const GUID *iids[] = { &CIA_IID_IUnknown, &CIA_IID_IObjContext, &CIA_IID_IComThreadingInfo, &CIA_IID_IContextCallback };
    size_t i;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    for (i = 0; i < 4; i++) {
        IUnknown *u = NULL; HRESULT hr = U(CoGetObjectContext)(iids[i], (void **)&u);
        CHECKF(hr == S_OK && u != NULL, "iid #%u hr=0x%08lx", (unsigned)i, (unsigned long)hr);
        if (u) IUnknown_Release(u);
    }
    cUninit();
}

TEST(cia_ctx_objctx_supported_interfaces_mta)
{
    static const GUID *iids[] = { &CIA_IID_IUnknown, &CIA_IID_IObjContext, &CIA_IID_IComThreadingInfo, &CIA_IID_IContextCallback };
    size_t i;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    for (i = 0; i < 4; i++) {
        IUnknown *u = NULL; HRESULT hr = U(CoGetObjectContext)(iids[i], (void **)&u);
        CHECKF(hr == S_OK && u != NULL, "iid #%u hr=0x%08lx", (unsigned long)i, (unsigned long)hr);
        if (u) IUnknown_Release(u);
    }
    cUninit();
}

TEST(cia_ctx_objctx_matches_context_token)
{
    ULONG_PTR tok = 0; IUnknown *ctx = NULL;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&tok), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IObjContext, (void **)&ctx), S_OK);
    CHECK(ctx != NULL && (ULONG_PTR)ctx == tok);   /* the token IS the IObjContext interface pointer */
    if (ctx) IUnknown_Release(ctx);
    cUninit();
}

TEST(cia_ctx_objctx_threadinginfo_same_instance)
{
    IComThreadingInfo *a = NULL, *b = NULL;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&a), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&b), S_OK);
    CHECK(a == b);
    if (a) IComThreadingInfo_Release(a);
    if (b) IComThreadingInfo_Release(b);
    cUninit();
}

TEST(cia_ctx_threadinginfo_sta)
{
    IComThreadingInfo *ti = NULL; APTTYPE at = (APTTYPE)99; THDTYPE tt = (THDTYPE)99; GUID id, id2, nul, saved;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetCurrentLogicalThreadId)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&ti), S_OK);
    if (!ti) { cUninit(); return; }
    EXPECT_HR(IComThreadingInfo_GetCurrentApartmentType(ti, &at), S_OK);
    EXPECT_EQ(at, CIA_APT_MAINSTA);
    EXPECT_HR(IComThreadingInfo_GetCurrentThreadType(ti, &tt), S_OK);
    EXPECT_EQ(tt, THDTYPE_PROCESSMESSAGES);          /* STA threads pump messages */
    EXPECT_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, NULL), CIA_E_INVALIDARG);
    memset(&id, 0, sizeof id); memset(&id2, 0, sizeof id2);
    EXPECT_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &id), S_OK);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&id2), S_OK);
    CHECK(IsEqualGUID(&id, &id2) && !IsEqualGUID(&id, &CIA_GUID_NULL));
    saved = id;
    /* SetCurrentLogicalThreadId replaces the thread's logical thread id */
    memset(&nul, 0, sizeof nul);
    EXPECT_HR(IComThreadingInfo_SetCurrentLogicalThreadId(ti, &nul), S_OK);
    memset(&id, 0x55, sizeof id);
    EXPECT_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &id), S_OK);
    CHECK(IsEqualGUID(&id, &CIA_GUID_NULL));
    memset(&id, 0x55, sizeof id);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&id), S_OK);
    CHECK(IsEqualGUID(&id, &CIA_GUID_NULL));
    EXPECT_HR(IComThreadingInfo_SetCurrentLogicalThreadId(ti, &saved), S_OK);
    EXPECT_HR(U(CoGetCurrentLogicalThreadId)(&id), S_OK);
    CHECK(IsEqualGUID(&id, &saved));
    IComThreadingInfo_Release(ti);
    cUninit();
}

TEST(cia_ctx_threadinginfo_mta)
{
    IComThreadingInfo *ti = NULL; APTTYPE at = (APTTYPE)99; THDTYPE tt = (THDTYPE)99;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&ti), S_OK);
    if (!ti) { cUninit(); return; }
    EXPECT_HR(IComThreadingInfo_GetCurrentApartmentType(ti, &at), S_OK);
    EXPECT_EQ(at, CIA_APT_MTA);
    EXPECT_HR(IComThreadingInfo_GetCurrentThreadType(ti, &tt), S_OK);
    EXPECT_EQ(tt, THDTYPE_BLOCKMESSAGES);            /* MTA threads do not pump */
    IComThreadingInfo_Release(ti);
    cUninit();
}

/* ----- IContextCallback ------------------------------------------------------------------------------ */
struct cb_state { ULONG_PTR token_in_cb; DWORD tid_in_cb; int calls; HRESULT ret; DWORD dispid_seen; void *user_seen; };
static HRESULT WINAPI cb_fn(ComCallData *d)
{
    struct cb_state *st = (struct cb_state *)d->pUserDefined;
    st->calls++;
    st->tid_in_cb = GetCurrentThreadId();
    st->dispid_seen = d->dwDispid;
    st->user_seen = d->pUserDefined;
    st->token_in_cb = 0;
    U(CoGetContextToken)(&st->token_in_cb);
    return st->ret;
}

TEST(cia_ctx_callback_same_context_runs_inline)
{
    IContextCallback *cc = NULL; struct cb_state st; ComCallData d; ULONG_PTR tok = 0; HRESULT hr;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IContextCallback, (void **)&cc), S_OK);
    if (!cc) { cUninit(); return; }
    EXPECT_HR(U(CoGetContextToken)(&tok), S_OK);
    memset(&st, 0, sizeof st); st.ret = S_FALSE;
    memset(&d, 0, sizeof d); d.dwDispid = 0x1234; d.pUserDefined = &st;
    hr = IContextCallback_ContextCallback(cc, cb_fn, &d, &CIA_IID_IContextCallback, 2, NULL);
    EXPECT_HR(hr, S_FALSE);                    /* the callback's own return value is passed back */
    EXPECT_EQ(st.calls, 1);
    EXPECT_EQ(st.tid_in_cb, GetCurrentThreadId());
    EXPECT_EQ(st.token_in_cb, tok);
    EXPECT_EQ(st.dispid_seen, 0x1234);
    CHECK(st.user_seen == &st);
    st.ret = E_FAIL; st.calls = 0;
    hr = IContextCallback_ContextCallback(cc, cb_fn, &d, &CIA_IID_IContextCallback, 2, NULL);
    EXPECT_HR(hr, E_FAIL);
    EXPECT_EQ(st.calls, 1);
    st.ret = 0x00040000; st.calls = 0;
    hr = IContextCallback_ContextCallback(cc, cb_fn, &d, &CIA_IID_IContextCallback, 2, NULL);
    EXPECT_HR(hr, 0x00040000);
    IContextCallback_Release(cc);
    cUninit();
}

TEST(cia_ctx_callback_same_context_mta)
{
    IContextCallback *cc = NULL; struct cb_state st; ComCallData d; ULONG_PTR tok = 0; HRESULT hr;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IContextCallback, (void **)&cc), S_OK);
    if (!cc) { cUninit(); return; }
    EXPECT_HR(U(CoGetContextToken)(&tok), S_OK);
    memset(&st, 0, sizeof st); st.ret = S_FALSE;
    memset(&d, 0, sizeof d); d.pUserDefined = &st;
    hr = IContextCallback_ContextCallback(cc, cb_fn, &d, &CIA_IID_IContextCallback, 2, NULL);
    EXPECT_HR(hr, S_FALSE);
    EXPECT_EQ(st.calls, 1);
    EXPECT_EQ(st.token_in_cb, tok);
    IContextCallback_Release(cc);
    cUninit();
}

struct xcb { IContextCallback *cc; struct cb_state st; HRESULT hr; DWORD caller_tid; };
static DWORD WINAPI xcb_thread(void *p)
{
    struct xcb *x = (struct xcb *)p; ComCallData d;
    cInitEx(NULL, CIA_COINIT_MTA);
    x->caller_tid = GetCurrentThreadId();
    memset(&d, 0, sizeof d); d.pUserDefined = &x->st;
    x->hr = IContextCallback_ContextCallback(x->cc, cb_fn, &d, &CIA_IID_IContextCallback, 2, NULL);
    cUninit();
    return 0;
}
TEST(cia_ctx_callback_from_other_apartment_runs_in_target_context)
{
    IContextCallback *cc = NULL; struct xcb x; ULONG_PTR tok = 0; HANDLE th; DWORD start, rc;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IContextCallback, (void **)&cc), S_OK);
    EXPECT_HR(U(CoGetContextToken)(&tok), S_OK);
    if (!cc) { cUninit(); return; }
    memset(&x, 0, sizeof x); x.cc = cc; x.st.ret = S_FALSE; x.hr = E_UNEXPECTED;
    th = CreateThread(NULL, 0, xcb_thread, &x, 0, NULL);
    CHECK(th != NULL);
    start = GetTickCount();
    for (;;) {                                   /* this STA must pump for the cross-apartment call to be dispatched */
        MSG msg;
        rc = MsgWaitForMultipleObjects(1, &th, FALSE, 200, QS_ALLINPUT);
        if (rc == WAIT_OBJECT_0) break;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        if (GetTickCount() - start > 10000) break;
    }
    CHECK(rc == WAIT_OBJECT_0);
    if (rc == WAIT_OBJECT_0) {
        EXPECT_HR(x.hr, S_FALSE);
        EXPECT_EQ(x.st.calls, 1);
        EXPECT_EQ(x.st.token_in_cb, tok);        /* ran inside the target STA's context ... */
        EXPECT_EQ(x.st.tid_in_cb, GetCurrentThreadId());   /* ... on its thread */
        CHECK(x.caller_tid != GetCurrentThreadId());
    } else TerminateThread(th, 0);
    CloseHandle(th);
    IContextCallback_Release(cc);
    cUninit();
}

/* ----- CoBuildVersion --------------------------------------------------------------------------- */
TEST(cia_ctx_build_version)
{
    DWORD v;
    REQUIRE(UIN("ole32.dll", CoBuildVersion));
    v = UIN("ole32.dll", CoBuildVersion)();
    EXPECT_EQ(v, 0x170338);                        /* constant returned by Windows 10 ole32 (rmm 0x17, rup 0x338) */
    CHECK(t_proc_in("ole32.dll", "OleBuildVersion") == (FARPROC)UIN("ole32.dll", CoBuildVersion));  /* same function, two names */
}

/* ----- CoIsOle1Class ------------------------------------------------------------------------------ */
TEST(cia_ctx_isole1class_null)
{
    REQUIRE(U(CoIsOle1Class));
    EXPECT_EQ(U(CoIsOle1Class)(NULL), 0);
}

TEST(cia_ctx_isole1class_unregistered)
{
    static const GUID g[] = {
        {0,0,0,{0,0,0,0,0,0,0,0}},
        {0x00030000,0,0,{0xC0,0,0,0,0,0,0,0x46}},
        {0x0003deed,0x9a3c,0x4f5a,{0x8d,0x2e,0x71,0xb5,0xc6,0xe0,0xf1,0xa4}},
        {0x0004deed,0x9a3c,0x4f5a,{0x8d,0x2e,0x71,0xb5,0xc6,0xe0,0xf1,0xa4}},
        {0x0002deed,0x9a3c,0x4f5a,{0x8d,0x2e,0x71,0xb5,0xc6,0xe0,0xf1,0xa4}},
        {0x0005deed,0x9a3c,0x4f5a,{0x8d,0x2e,0x71,0xb5,0xc6,0xe0,0xf1,0xa4}},
        {0xdeed0003,0x9a3c,0x4f5a,{0x8d,0x2e,0x71,0xb5,0xc6,0xe0,0xf1,0xa4}},
        {0xFFFFFFFF,0xFFFF,0xFFFF,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}},
    };
    size_t i;
    REQUIRE(U(CoIsOle1Class));
    for (i = 0; i < sizeof g / sizeof g[0]; i++) {
        int r = U(CoIsOle1Class)(&g[i]);
        CHECKF(r == 0, "clsid #%u -> %d", (unsigned)i, r);
    }
}

/* ----- library unloading -------------------------------------------------------------------------- */
TEST(cia_ctx_free_unused_libraries_no_com)
{
    REQUIRE(U(CoFreeUnusedLibraries)); REQUIRE(U(CoFreeUnusedLibrariesEx)); REQUIRE(U(CoGetApartmentType));
    CIA_CHECK_NOTINIT();
    U(CoFreeUnusedLibraries)();
    U(CoFreeUnusedLibrariesEx)(0, 0);
    U(CoFreeUnusedLibrariesEx)(INFINITE, 0);
    CIA_CHECK_NOTINIT();                          /* these never initialize COM */
}

TEST(cia_ctx_free_unused_libraries_in_sta_and_mta)
{
    DWORD delays[] = { 0, 1, 1000, 0xFFFFFFFF };
    int i, mt;
    REQUIRE(U(CoFreeUnusedLibraries)); REQUIRE(U(CoFreeUnusedLibrariesEx)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (mt = 0; mt < 2; mt++) {
        EXPECT_HR(cInitEx(NULL, mt ? CIA_COINIT_MTA : CIA_COINIT_STA), S_OK);
        U(CoFreeUnusedLibraries)();
        for (i = 0; i < 4; i++) U(CoFreeUnusedLibrariesEx)(delays[i], 0);
        CIA_CHECK_APT(S_OK, mt ? CIA_APT_MTA : CIA_APT_MAINSTA, CIA_Q_NONE);
        cUninit();
    }
}

TEST(cia_ctx_free_unused_libraries_leaves_ordinary_dlls_loaded)
{
    HMODULE h;
    REQUIRE(U(CoFreeUnusedLibraries)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    h = LoadLibraryA("version.dll");
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    U(CoFreeUnusedLibraries)();                   /* only COM-loaded server DLLs are candidates */
    CHECK(GetModuleHandleA("version.dll") == h);
    cUninit();
    FreeLibrary(h);
}

TEST(cia_ctx_free_unused_libraries_repeated)
{
    int i;
    REQUIRE(U(CoFreeUnusedLibraries)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    for (i = 0; i < 100; i++) U(CoFreeUnusedLibraries)();
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
}

static void w_free_unused(void *p) { U(CoFreeUnusedLibraries)(); U(CoFreeUnusedLibrariesEx)(0, 0); *(int *)p = 1; }
TEST(cia_ctx_free_unused_libraries_on_worker_thread)
{
    int done = 0;
    REQUIRE(U(CoFreeUnusedLibraries)); REQUIRE(U(CoFreeUnusedLibrariesEx));
    CHECK(cia_run(w_free_unused, &done));
    EXPECT_EQ(done, 1);
}

TEST(cia_ctx_free_all_libraries_is_harmless)
{
    REQUIRE(UIN("ole32.dll", CoFreeAllLibraries)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    UIN("ole32.dll", CoFreeAllLibraries)();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    UIN("ole32.dll", CoFreeAllLibraries)();
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
    CHECK(t_proc_in("combase.dll", "CoFreeAllLibraries") == NULL);   /* ole32-only export on Windows 10 */
}

/* ----- CoInitializeWOW ---------------------------------------------------------------------------- */
static void w_wow(void *p)
{
    HRESULT *r = (HRESULT *)p;
    r[0] = UN(CoInitializeWOW)(NULL, NULL);
    r[1] = UN(CoInitializeWOW)(NULL, NULL);
    r[2] = cia_apt_hr();
    r[3] = cInitEx(NULL, CIA_COINIT_MTA);
    cUninit(); cUninit();
    r[4] = cia_apt_hr();
}
TEST(cia_ctx_initialize_wow_is_sta_init)
{
    HRESULT r[5] = {0};
    REQUIRE(UN(CoInitializeWOW)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    CHECK(cia_run(w_wow, r));                      /* run on a throw-away thread: it tags the thread's COM state as WOW */
    EXPECT_HR(r[0], S_OK); EXPECT_HR(r[1], S_FALSE); EXPECT_HR(r[2], S_OK);
    EXPECT_HR(r[3], CIA_CHANGED_MODE);             /* equivalent to CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) */
    EXPECT_HR(r[4], CIA_NOTINIT);
}
