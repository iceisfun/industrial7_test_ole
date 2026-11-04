/* CoSwitchCallContext/CoGetCallContext, call cancellation (CoEnable/DisableCallCancellation, CoSetCancelObject,
 * CoGetCancelObject, CoCancelCall, CoTestCancel). */
#include "cx.h"

/* ================= CoSwitchCallContext / CoGetCallContext ================= */
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
static void w_cc_basic(void *p)
{
    CxObj a, b;
    IUnknown *old = (IUnknown *)0xdead, *got = NULL;
    cx_obj_init(&a, 0, 1); cx_obj_init(&b, 0, 1);

    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, (void **)&got), CX_HR_CALLCOMPLETE);   /* no call in progress */
    CXEQ(got, 0);

    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    CXEQ(old, 0);                                  /* nothing was installed before */
    CXEQ(a.ref, 1);                                /* installing takes no reference */

    got = NULL;
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, (void **)&got), S_OK);
    CXEQ(got, &a);
    CXEQ(a.ref, 2);                                /* the QueryInterface result is a counted reference */
    CXEQ(a.qi_calls, 1);
    IUnknown_Release(got);
    CXEQ(a.ref, 1);

    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&b), &old), S_OK);
    CXEQ(old, &a);                                 /* the previous context is handed back, no reference added */
    CXEQ(a.ref, 1); CXEQ(b.ref, 1);
    got = NULL;
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, (void **)&got), S_OK);
    CXEQ(got, &b);
    IUnknown_Release(got);

    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);   /* uninstall */
    CXEQ(old, &b);
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, (void **)&got), CX_HR_CALLCOMPLETE);
    CXEQ(a.ref, 1); CXEQ(b.ref, 1);
    CXEQ(b.qi_calls, 1);
}
TEST(ctxthr_callctx_switch_and_get_sta) { REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext)); cx_run(CXM_STA, w_cc_basic, NULL); }
TEST(ctxthr_callctx_switch_and_get_mta) { REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext)); cx_run(CXM_MTA, w_cc_basic, NULL); }
/* no initialization state is consulted */
TEST(ctxthr_callctx_switch_and_get_uninitialized_thread) { REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext)); cx_run(CXM_NONE, w_cc_basic, NULL); }

static void w_cc_nullout(void *p)
{
    CxObj a;
    IUnknown *old = NULL;
    cx_obj_init(&a, 0, 1);
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, NULL), E_INVALIDARG);
    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, NULL), E_INVALIDARG);   /* output pointer is checked even with a context */
    CXEQ(a.qi_calls, 0);
    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);
}
TEST(ctxthr_callctx_get_null_output) { CX_WINE_SKIP(); REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext)); cx_run(CXM_STA, w_cc_nullout, NULL); }

static void w_cc_thread(void *p)
{
    IUnknown *got = NULL;
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, (void **)&got), CX_HR_CALLCOMPLETE);   /* per-thread state */
}
TEST(ctxthr_callctx_is_per_thread)
{
    CxObj a; IUnknown *old = NULL;
    REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext));
    cx_obj_init(&a, 0, 1);
    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    cx_run(CXM_STA, w_cc_thread, NULL);
    cx_run(CXM_MTA, w_cc_thread, NULL);
    CXEQ(a.qi_calls, 0);
    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);
    CXEQ(old, &a);
}

static void w_cc_qi_fail(void *p)
{
    CxObj a; IUnknown *old = NULL; void *got = (void *)0xdead;
    cx_obj_init(&a, 0, 0);                         /* only IUnknown */
    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IUnknown, &got), S_OK);
    CXEQ(got, &a); IUnknown_Release((IUnknown *)got);
    /* these IIDs are the ones with a built-in fallthrough: the context's failure is returned as is */
    got = (void *)0xdead;
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_IServerSecurity, &got), CX_HR_NOINTERFACE);
    CXEQ(a.ref, 1);
    CXEQ_HR(U(CoGetCallContext)(&CX_IID_ICancelMethodCalls, &got), CX_HR_NOINTERFACE);
    CXEQ(a.ref, 1);
    CXEQ(a.qi_calls, 3);
    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);
}
TEST(ctxthr_callctx_unsupported_iid_returns_qi_failure) { REQUIRE(U(CoSwitchCallContext)); REQUIRE(U(CoGetCallContext)); cx_run(CXM_STA, w_cc_qi_fail, NULL); }

/* ================= CoTestCancel ================= */
static void w_tc(void *p)
{
    CxObj a; IUnknown *old = NULL;
    HRESULT hr;
    cx_obj_init(&a, 1, 0);
    hr = U(CoTestCancel)();
    CXEQ_HR(hr, CX_HR_CALLCOMPLETE);               /* no call context: no call to test */
    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    a.test_ret = S_OK;
    CXEQ_HR(U(CoTestCancel)(), S_OK);
    CXEQ(a.test_calls, 1);
    a.test_ret = CX_HR_CALLPENDING;
    CXEQ_HR(U(CoTestCancel)(), CX_HR_CALLPENDING);
    a.test_ret = CX_HR_CALLCANCELED;
    CXEQ_HR(U(CoTestCancel)(), CX_HR_CALLCANCELED);
    a.test_ret = CX_HR_FAIL;
    CXEQ_HR(U(CoTestCancel)(), CX_HR_FAIL);
    CXEQ(a.test_calls, 4);
    CXEQ(a.ref, 1);                                /* the reference taken for the test is released again */
    CXEQ(a.cancel_calls, 0);
    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);
    CXEQ_HR(U(CoTestCancel)(), CX_HR_CALLCOMPLETE);
}
TEST(ctxthr_testcancel_delegates_to_call_context_sta) { CX_WINE_SKIP(); REQUIRE(U(CoTestCancel)); REQUIRE(U(CoSwitchCallContext)); cx_run(CXM_STA, w_tc, NULL); }
TEST(ctxthr_testcancel_delegates_to_call_context_mta) { CX_WINE_SKIP(); REQUIRE(U(CoTestCancel)); REQUIRE(U(CoSwitchCallContext)); cx_run(CXM_MTA, w_tc, NULL); }

static void w_tc_noiface(void *p)
{
    CxObj a; IUnknown *old = NULL;
    cx_obj_init(&a, 0, 0);                         /* call context without ICancelMethodCalls */
    CXEQ_HR(U(CoSwitchCallContext)(cx_obj_unk(&a), &old), S_OK);
    CXEQ_HR(U(CoTestCancel)(), CX_HR_NOINTERFACE);
    CXEQ(a.test_calls, 0);
    CXEQ_HR(U(CoSwitchCallContext)(NULL, &old), S_OK);
}
TEST(ctxthr_testcancel_context_without_cancel_interface) { CX_WINE_SKIP(); REQUIRE(U(CoTestCancel)); REQUIRE(U(CoSwitchCallContext)); cx_run(CXM_STA, w_tc_noiface, NULL); }

/* ================= CoEnableCallCancellation / CoDisableCallCancellation ================= */
static void w_cancel_enable(void *p)
{
    int i;
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), CX_HR_CANCELDISABLED);   /* never enabled on this thread */
    CXEQ_HR(U(CoEnableCallCancellation)(NULL), S_OK);
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), S_OK);
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), CX_HR_CANCELDISABLED);
    for (i = 0; i < 5; i++) CXEQ_HR(U(CoEnableCallCancellation)(NULL), S_OK);   /* nests */
    for (i = 0; i < 5; i++) CXEQ_HR(U(CoDisableCallCancellation)(NULL), S_OK);
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), CX_HR_CANCELDISABLED);
}
TEST(ctxthr_cancel_enable_disable_counts_sta) { CX_WINE_SKIP(); REQUIRE(U(CoEnableCallCancellation)); REQUIRE(U(CoDisableCallCancellation)); cx_run(CXM_STA, w_cancel_enable, NULL); }
TEST(ctxthr_cancel_enable_disable_counts_mta) { CX_WINE_SKIP(); REQUIRE(U(CoEnableCallCancellation)); REQUIRE(U(CoDisableCallCancellation)); cx_run(CXM_MTA, w_cancel_enable, NULL); }
TEST(ctxthr_cancel_enable_disable_counts_uninitialized_thread) { CX_WINE_SKIP(); REQUIRE(U(CoEnableCallCancellation)); REQUIRE(U(CoDisableCallCancellation)); cx_run(CXM_NONE, w_cancel_enable, NULL); }

static void w_cancel_reserved(void *p)
{
    int dummy;
    CXEQ_HR(U(CoEnableCallCancellation)(&dummy), E_INVALIDARG);    /* pReserved must be NULL */
    CXEQ_HR(U(CoDisableCallCancellation)(&dummy), E_INVALIDARG);
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), CX_HR_CANCELDISABLED);   /* the rejected enable did not count */
    CXEQ_HR(U(CoEnableCallCancellation)(NULL), S_OK);
    CXEQ_HR(U(CoDisableCallCancellation)(&dummy), E_INVALIDARG);   /* nor does a rejected disable */
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), S_OK);
}
TEST(ctxthr_cancel_enable_disable_reserved_must_be_null) { CX_WINE_SKIP(); REQUIRE(U(CoEnableCallCancellation)); REQUIRE(U(CoDisableCallCancellation)); cx_run(CXM_STA, w_cancel_reserved, NULL); }

static void w_cancel_count_thread(void *p)
{
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), CX_HR_CANCELDISABLED);   /* a new thread starts at zero */
}
TEST(ctxthr_cancel_enable_count_is_per_thread)
{ CX_WINE_SKIP();
    REQUIRE(U(CoEnableCallCancellation)); REQUIRE(U(CoDisableCallCancellation));
    CXEQ_HR(U(CoEnableCallCancellation)(NULL), S_OK);
    cx_run(CXM_STA, w_cancel_count_thread, NULL);
    cx_run(CXM_MTA, w_cancel_count_thread, NULL);
    CXEQ_HR(U(CoDisableCallCancellation)(NULL), S_OK);
}

/* ================= CoSetCancelObject / CoGetCancelObject / CoCancelCall ================= */
static void w_sco_basic(void *p)
{
    CxObj a; void *got = (void *)0xdead;
    cx_obj_init(&a, 1, 0);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);     /* nothing registered: resetting is a no-op */
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);
    CXEQ(got, 0xdead);                             /* untouched on failure */

    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&a)), S_OK);
    CXEQ(a.qi_calls, 1);
    CXEQ(a.ref, 2);                                /* the registration holds a reference */
    got = NULL;
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), S_OK);   /* 0 means the calling thread */
    CXEQ(got, &a);
    CXEQ(a.ref, 3);
    IUnknown_Release((IUnknown *)got);
    got = NULL;
    CXEQ_HR(U(CoGetCancelObject)(GetCurrentThreadId(), &CX_IID_IUnknown, &got), S_OK);
    CXEQ(got, &a);
    IUnknown_Release((IUnknown *)got);
    CXEQ(a.ref, 2);

    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);     /* unregister */
    CXEQ(a.ref, 1);
    got = (void *)0xdead;
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);
}
TEST(ctxthr_setcancelobject_register_query_unregister_sta) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_STA, w_sco_basic, NULL); }
TEST(ctxthr_setcancelobject_register_query_unregister_mta) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_MTA, w_sco_basic, NULL); }
TEST(ctxthr_setcancelobject_register_query_unregister_uninitialized_thread) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_NONE, w_sco_basic, NULL); }

static void w_sco_noiface(void *p)
{
    CxObj a;
    void *got = (void *)0xdead;
    cx_obj_init(&a, 0, 0);
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&a)), CX_HR_NOINTERFACE);    /* must implement ICancelMethodCalls */
    CXEQ(a.ref, 1);
    CXEQ(a.qi_calls, 1);
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);   /* nothing registered */
}
TEST(ctxthr_setcancelobject_requires_cancel_interface) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_STA, w_sco_noiface, NULL); }

static void w_gco_args(void *p)
{
    CxObj a; void *got = (void *)0xdead;
    cx_obj_init(&a, 1, 0);
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, NULL), E_INVALIDARG);
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&a)), S_OK);
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, NULL), E_INVALIDARG);
    got = (void *)0xdead;
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_Bogus, &got), CX_HR_NOINTERFACE);   /* QueryInterface on the registered object */
    CXEQ(a.ref, 2);
    CXEQ_HR(U(CoGetCancelObject)(0xFFFFFFF0u, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);   /* no such thread */
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);
}
TEST(ctxthr_getcancelobject_argument_handling) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_STA, w_gco_args, NULL); }

static void w_sco_stack(void *p)
{
    CxObj a, b; void *got = NULL;
    cx_obj_init(&a, 1, 0); cx_obj_init(&b, 1, 0);
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&a)), S_OK);
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&b)), S_OK);       /* nested call: innermost wins */
    CXEQ(a.ref, 2); CXEQ(b.ref, 2);
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), S_OK);
    CXEQ(got, &b); IUnknown_Release((IUnknown *)got);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);                 /* pops b */
    CXEQ(b.ref, 1); CXEQ(a.ref, 2);
    got = NULL;
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), S_OK);
    CXEQ(got, &a); IUnknown_Release((IUnknown *)got);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);                 /* pops a */
    CXEQ(a.ref, 1);
    got = (void *)0xdead;
    CXEQ_HR(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);                 /* extra reset stays harmless */
}
TEST(ctxthr_setcancelobject_nesting_is_a_stack) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoGetCancelObject)); cx_run(CXM_MTA, w_sco_stack, NULL); }

static void w_cancel_self(void *p)
{
    CxObj a;
    cx_obj_init(&a, 1, 0);
    CXEQ_HR(U(CoCancelCall)(0, 5), CX_HR_NOCONTEXT);           /* nothing to cancel */
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&a)), S_OK);
    a.cancel_ret = S_OK;
    CXEQ_HR(U(CoCancelCall)(0, 5), S_OK);
    CXEQ(a.cancel_calls, 1); CXEQ(a.last_cancel_secs, 5); CXEQ(a.cancel_tid, GetCurrentThreadId());
    a.cancel_ret = CX_HR_CALLCOMPLETE;
    CXEQ_HR(U(CoCancelCall)(GetCurrentThreadId(), 0), CX_HR_CALLCOMPLETE);   /* the Cancel result is returned */
    CXEQ(a.last_cancel_secs, 0);
    a.cancel_ret = CX_HR_CALLCANCELED;
    CXEQ_HR(U(CoCancelCall)(0, 0xFFFFFFFFu), CX_HR_CALLCANCELED);
    CXEQ(a.last_cancel_secs, 0xFFFFFFFFu);                      /* RPC_C_CANCEL_INFINITE_TIMEOUT passes through */
    a.cancel_ret = CX_HR_FAIL;
    CXEQ_HR(U(CoCancelCall)(0, 1), CX_HR_FAIL);
    CXEQ(a.cancel_calls, 4);
    CXEQ(a.ref, 2);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);
    CXEQ_HR(U(CoCancelCall)(0, 5), CX_HR_NOCONTEXT);
}
TEST(ctxthr_cancelcall_own_thread_invokes_cancel_object) { CX_WINE_SKIP(); REQUIRE(U(CoSetCancelObject)); REQUIRE(U(CoCancelCall)); cx_run(CXM_STA, w_cancel_self, NULL); }

/* Cancelling a thread other than the caller's. */
typedef struct XCancel { HANDLE registered, finish; DWORD tid; CxObj obj; } XCancel;
static void w_xcancel_target(void *p)
{
    XCancel *x = p;
    x->tid = GetCurrentThreadId();
    CXEQ_HR(U(CoSetCancelObject)(cx_obj_unk(&x->obj)), S_OK);
    SetEvent(x->registered);
    WaitForSingleObject(x->finish, 8000);
    CXEQ_HR(U(CoSetCancelObject)(NULL), S_OK);
}
static DWORD WINAPI xcancel_thread(LPVOID p) { cx_run_hr(CXM_STA, w_xcancel_target, p, NULL); return 0; }
TEST(ctxthr_cancelcall_reaches_cancel_object_of_another_thread)
{ CX_WINE_SKIP();
    XCancel x; HANDLE th; void *got = NULL; DWORD tid;
    REQUIRE(U(CoCancelCall)); REQUIRE(U(CoGetCancelObject));
    memset(&x, 0, sizeof x);
    cx_obj_init(&x.obj, 1, 0);
    x.registered = cx_event(TRUE, FALSE); x.finish = cx_event(TRUE, FALSE);
    th = CreateThread(NULL, 0, xcancel_thread, &x, 0, NULL);
    if (!th || WaitForSingleObject(x.registered, 8000) != WAIT_OBJECT_0) { CHECK(!"target thread"); SetEvent(x.finish); return; }
    tid = x.tid;
    CXEQ_HR(U(CoGetCancelObject)(tid, &CX_IID_ICancelMethodCalls, &got), S_OK);
    CXEQ(got, &x.obj); if (got) IUnknown_Release((IUnknown *)got);
    CXEQ(U(CoGetCancelObject)(0, &CX_IID_ICancelMethodCalls, &got) == S_OK, 0);   /* the calling thread has none */
    x.obj.cancel_ret = S_OK;
    CXEQ_HR(U(CoCancelCall)(tid, 3), S_OK);
    CXEQ(x.obj.cancel_calls, 1); CXEQ(x.obj.last_cancel_secs, 3);
    CXEQ(x.obj.cancel_tid, GetCurrentThreadId());  /* Cancel runs on the thread that asked */
    SetEvent(x.finish);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CXEQ_HR(U(CoCancelCall)(tid, 3), CX_HR_NOCONTEXT);          /* the thread is gone / unregistered */
    CloseHandle(x.registered); CloseHandle(x.finish);
}

/* ================= real cancellation of an outstanding cross-apartment call ================= */
typedef struct Inflight {
    CxSrv *s; DWORD model; BOOL enable;
    HRESULT call_hr, enable_hr, disable_hr, second_hr;
    DWORD tid; HANDLE started, done, proceed, returned; int proxy_ok;
} Inflight;
static void w_inflight(void *p)
{
    Inflight *f = p;
    IStream *cf = cx_srv_proxy(f->s);          /* the proxy belongs to this thread's apartment */
    f->tid = GetCurrentThreadId();
    if (!cf) { SetEvent(f->started); return; }
    f->proxy_ok = 1;
    if (f->enable) f->enable_hr = U(CoEnableCallCancellation)(NULL);
    SetEvent(f->started);
    f->call_hr = IStream_Revert(cf);
    SetEvent(f->returned);
    WaitForSingleObject(f->proceed, 8000);
    f->second_hr = IStream_Revert(cf);
    if (f->enable) f->disable_hr = U(CoDisableCallCancellation)(NULL);
    IStream_Release(cf);
}
static DWORD WINAPI inflight_thread(LPVOID p)
{
    Inflight *f = p;
    cx_run_hr(f->model, w_inflight, p, NULL);
    SetEvent(f->done);
    return 0;
}
static void run_inflight_mode(DWORD client_model, BOOL enable, int via_object)
{
    CxSrv s; Inflight f; HANDLE th;
    HANDLE entered = cx_event(TRUE, FALSE), release = cx_event(TRUE, FALSE);
    HRESULT cancel_hr = E_UNEXPECTED;
    int reached;
    memset(&s, 0, sizeof s);
    s.cf.entered = entered; s.cf.release = release; s.cf.poll_cancel = 1;
    if (!cx_srv_start(&s, CXM_STA, NULL)) { CHECK(!"server"); cx_srv_stop(&s); goto out; }
    memset(&f, 0, sizeof f);
    f.s = &s; f.model = client_model; f.enable = enable;
    f.started = cx_event(TRUE, FALSE); f.done = cx_event(TRUE, FALSE); f.proceed = cx_event(TRUE, FALSE); f.returned = cx_event(TRUE, FALSE);
    th = CreateThread(NULL, 0, inflight_thread, &f, 0, NULL);
    if (!th) { CHECK(!"CreateThread"); cx_srv_stop(&s); goto out; }
    WaitForSingleObject(f.started, 8000);
    reached = f.proxy_ok && WaitForSingleObject(entered, 8000) == WAIT_OBJECT_0;   /* the call is now inside the server method */
    CHECK(reached);
    if (reached) {
        if (via_object) {
            ICancelMethodCalls *obj = NULL;
            HRESULT gh = U(CoGetCancelObject)(f.tid, &CX_IID_ICancelMethodCalls, (void **)&obj);
            if (enable) {
                CXEQ_HR(gh, S_OK);
                if (obj) {
                    CXEQ_HR(ICancelMethodCalls_TestCancel(obj), CX_HR_CALLPENDING);   /* documented: not canceled yet */
                    cancel_hr = ICancelMethodCalls_Cancel(obj, 0);
                } else cancel_hr = gh;
            } else {
                /* Win10: the cancel object is handed out even when cancellation is not enabled; cancelling through it is what is refused */
                CXEQ_HR(gh, S_OK);
                cancel_hr = obj ? ICancelMethodCalls_Cancel(obj, 0) : gh;
                t_note("cancel object with cancellation disabled: Cancel 0x%08lx", (unsigned long)cancel_hr);
            }
            if (obj) {
                HRESULT after = E_UNEXPECTED;
                if (enable) { WaitForSingleObject(f.returned, 6000); after = ICancelMethodCalls_TestCancel(obj); t_note("TestCancel on the cancel object after cancelling: 0x%08lx", (unsigned long)after); }
                ICancelMethodCalls_Release(obj);
            }
        } else cancel_hr = U(CoCancelCall)(f.tid, 0);
        if (enable) {
            CXEQ_HR(cancel_hr, S_OK);
            /* the client returns without waiting for the server method, which is still blocked */
            CHECK(WaitForSingleObject(f.returned, 6000) == WAIT_OBJECT_0);
            CXEQ_HR(f.call_hr, CX_HR_CALLCANCELED);
            CXEQ(WaitForSingleObject(release, 0), WAIT_TIMEOUT);   /* the server method had not been released yet */
        } else {
            CXEQ_HR(cancel_hr, CX_HR_CANCELDISABLED);
        }
    }
    SetEvent(release);                              /* let the server method return */
    SetEvent(f.proceed);
    CHECK(WaitForSingleObject(f.done, 10000) == WAIT_OBJECT_0);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    if (reached && !enable) CXEQ_HR(f.call_hr, S_OK);   /* uncancelled call completes normally */
    if (enable) { CXEQ_HR(f.enable_hr, S_OK); CXEQ_HR(f.disable_hr, S_OK); }
    t_note("in-flight cancel (client %s, enabled=%d): CoCancelCall 0x%08lx, call 0x%08lx, follow-up call 0x%08lx, server CoTestCancel polls=%ld last=0x%08lx",
           client_model == CXM_STA ? "STA" : "MTA", (int)enable, (unsigned long)cancel_hr, (unsigned long)f.call_hr,
           (unsigned long)f.second_hr, (long)s.cf.test_polls, (unsigned long)s.cf.test_last);
    cx_srv_stop(&s);
    CloseHandle(f.started); CloseHandle(f.done); CloseHandle(f.proceed); CloseHandle(f.returned);
out:
    CloseHandle(entered); CloseHandle(release);
}
static void run_inflight(DWORD client_model, BOOL enable) { run_inflight_mode(client_model, enable, 0); }
TEST(ctxthr_cancel_inflight_call_from_mta_client)  { CX_WINE_SKIP(); REQUIRE(U(CoCancelCall)); run_inflight(CXM_MTA, TRUE); }
TEST(ctxthr_cancel_inflight_call_from_sta_client)  { CX_WINE_SKIP(); REQUIRE(U(CoCancelCall)); run_inflight(CXM_STA, TRUE); }
TEST(ctxthr_cancel_inflight_call_without_enable_mta) { CX_WINE_SKIP(); REQUIRE(U(CoCancelCall)); run_inflight(CXM_MTA, FALSE); }
TEST(ctxthr_cancel_inflight_call_without_enable_sta) { CX_WINE_SKIP(); REQUIRE(U(CoCancelCall)); run_inflight(CXM_STA, FALSE); }
TEST(ctxthr_cancel_inflight_via_cancel_object_mta_client) { CX_WINE_SKIP(); REQUIRE(U(CoGetCancelObject)); run_inflight_mode(CXM_MTA, TRUE, 1); }
TEST(ctxthr_cancel_inflight_via_cancel_object_sta_client) { CX_WINE_SKIP(); REQUIRE(U(CoGetCancelObject)); run_inflight_mode(CXM_STA, TRUE, 1); }
TEST(ctxthr_cancel_inflight_cancel_object_unavailable_without_enable) { CX_WINE_SKIP(); REQUIRE(U(CoGetCancelObject)); run_inflight_mode(CXM_MTA, FALSE, 1); }

/* A thread that exists but has no call in flight has nothing to cancel. */
TEST(ctxthr_cancel_idle_thread_has_no_cancel_object)
{
    CxPark idle[2]; void *got = (void *)0xdead; int i;
    CX_WINE_SKIP();
    REQUIRE(U(CoGetCancelObject)); REQUIRE(U(CoCancelCall));
    for (i = 0; i < 2; i++) {
        if (!cx_park_start(&idle[i], i ? CXM_MTA : CXM_STA)) { CHECK(!"park"); cx_park_stop(&idle[i]); continue; }
        got = (void *)0xdead;
        CXEQ_HR(U(CoGetCancelObject)(idle[i].tid, &CX_IID_ICancelMethodCalls, &got), CX_HR_NOCONTEXT);
        CXEQ(got, 0xdead);
        CXEQ_HR(U(CoCancelCall)(idle[i].tid, 0), CX_HR_NOCONTEXT);
        cx_park_stop(&idle[i]);
    }
}
