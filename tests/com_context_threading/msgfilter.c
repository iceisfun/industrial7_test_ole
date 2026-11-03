/* CoRegisterMessageFilter / IMessageFilter callbacks, CoRegisterChannelHook / IChannelHook callbacks. */
#include "cx.h"

/* ================= CoRegisterMessageFilter: registration semantics ================= */
#define PREV_SENT ((IMessageFilter *)(ULONG_PTR)0xdeadbeef)

static void w_rmf_mta(void *p)
{
    CxFilter f; IMessageFilter *prev = PREV_SENT;
    GUID dummy;
    cx_filter_init(&f);
    U(CoGetCurrentLogicalThreadId)(&dummy);         /* make sure the thread has COM thread state */
    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&f, &prev), CX_HR_NOTSUPP);   /* MTA threads have no message filter */
    CXEQ(prev, PREV_SENT);                          /* out parameter untouched on failure */
    CXEQ(f.ref, 1);                                 /* nothing retained */
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, &prev), CX_HR_NOTSUPP);
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, NULL), CX_HR_NOTSUPP);
    CXEQ(prev, PREV_SENT);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_msgfilter_register_fails_in_mta) { REQUIRE(U(CoRegisterMessageFilter)); cx_run(CXM_MTA, w_rmf_mta, NULL); }
TEST(ctxthr_msgfilter_register_fails_without_apartment) { REQUIRE(U(CoRegisterMessageFilter)); cx_run(CXM_NONE, w_rmf_mta, NULL); }

static void w_rmf_sta(void *p)
{
    CxFilter f, g; IMessageFilter *prev;
    cx_filter_init(&f); cx_filter_init(&g);

    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, NULL), S_OK);          /* nothing registered, nothing to clear */
    prev = PREV_SENT;
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, &prev), S_OK);
    CXEQ(prev, 0);                                  /* "no previous filter" is reported as NULL */

    prev = PREV_SENT;
    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&f, &prev), S_OK);
    CXEQ(prev, 0);
    CXEQ(f.ref, 2);                                 /* COM keeps its own reference */

    prev = PREV_SENT;
    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&g, &prev), S_OK);   /* replace: previous handed back */
    CXEQ(prev, &f);
    CXEQ(f.ref, 2);                                 /* ownership of COM's reference moved to the caller */
    CXEQ(g.ref, 2);
    if (prev) IMessageFilter_Release(prev);
    CXEQ(f.ref, 1);

    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&g, NULL), S_OK);   /* re-registering the same one, previous dropped */
    CXEQ(g.ref, 2);                                 /* net: still one reference held by COM */

    prev = PREV_SENT;
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, &prev), S_OK);   /* unregister */
    CXEQ(prev, &g);
    CXEQ(g.ref, 2);
    IMessageFilter_Release(prev);
    CXEQ(g.ref, 1);

    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&f, NULL), S_OK);
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, NULL), S_OK);     /* unregistering with no out pointer releases it */
    CXEQ(f.ref, 1);
    CXEQ(f.qi_calls, 0);                            /* COM never queries the filter */
}
TEST(ctxthr_msgfilter_register_replace_and_refcounts) { REQUIRE(U(CoRegisterMessageFilter)); cx_run(CXM_STA, w_rmf_sta, NULL); }

typedef struct RmfShared { CxFilter *f; HANDLE registered, finish; IMessageFilter *prev_seen; HRESULT hr; } RmfShared;
static void w_rmf_owner(void *p)
{
    RmfShared *s = p;
    IMessageFilter *prev = NULL;
    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)s->f, &prev), S_OK);
    SetEvent(s->registered);
    WaitForSingleObject(s->finish, 8000);
    prev = NULL;
    CXEQ_HR(U(CoRegisterMessageFilter)(NULL, &prev), S_OK);
    CXEQ(prev, s->f);                               /* still ours: the other thread did not disturb it */
    if (prev) IMessageFilter_Release(prev);
}
static DWORD WINAPI rmf_owner_thread(LPVOID p) { cx_run_hr(CXM_STA, w_rmf_owner, p, NULL); return 0; }
static void w_rmf_other(void *p)
{
    RmfShared *s = p;
    s->prev_seen = PREV_SENT;
    s->hr = U(CoRegisterMessageFilter)(NULL, &s->prev_seen);
}
TEST(ctxthr_msgfilter_is_per_apartment)
{
    CxFilter f; RmfShared s; HANDLE th;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    memset(&s, 0, sizeof s);
    s.f = &f; s.registered = cx_event(TRUE, FALSE); s.finish = cx_event(TRUE, FALSE);
    th = CreateThread(NULL, 0, rmf_owner_thread, &s, 0, NULL);
    if (!th || WaitForSingleObject(s.registered, 8000) != WAIT_OBJECT_0) { CHECK(!"owner"); SetEvent(s.finish); return; }
    cx_run(CXM_STA, w_rmf_other, &s);
    CXEQ_HR(s.hr, S_OK);
    CXEQ(s.prev_seen, 0);                           /* another STA has no filter of its own */
    SetEvent(s.finish);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CXEQ(f.ref, 1);
    CloseHandle(s.registered); CloseHandle(s.finish);
}

static void w_rmf_left(void *p)
{
    CxFilter *f = p;
    IMessageFilter *prev = NULL;
    CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)f, &prev), S_OK);
    CXEQ(f->ref, 2);
}   /* leaves the filter registered on purpose */
TEST(ctxthr_msgfilter_released_when_apartment_ends)
{
    CxFilter f;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    cx_run(CXM_STA, w_rmf_left, &f);
    t_note("filter refcount after the registering STA thread uninitialized: %ld (1 = released by COM)", (long)f.ref);
}

/* ================= IMessageFilter callbacks on cross-apartment calls ================= */
/* index of the n-th logged incoming call on IStream, or -1 */
static int find_stream_call(const CxFilter *f, int nth)
{
    int i, seen = 0;
    for (i = 0; i < 8 && i < f->incoming; i++)
        if (f->log_have[i] && IsEqualGUID(&f->log_info[i].iid, &CX_IID_IStream)) { if (seen++ == nth) return i; }
    return -1;
}
static void w_mf_call(void *p)
{
    struct { CxSrv *s; HRESULT hr, hr2; DWORD tid; CxFilter *cli; } *a = p;
    IStream *cf = cx_srv_proxy(a->s);
    a->tid = GetCurrentThreadId();
    if (!cf) { a->hr = E_POINTER; return; }
    if (a->cli) { IMessageFilter *prev = NULL; U(CoRegisterMessageFilter)((IMessageFilter *)a->cli, &prev); if (prev) IMessageFilter_Release(prev); }
    a->hr = IStream_Revert(cf);
    a->hr2 = IStream_Revert(cf);
    if (a->cli) U(CoRegisterMessageFilter)(NULL, NULL);
    IStream_Release(cf);
}
typedef struct MfCall { CxSrv *s; HRESULT hr, hr2; DWORD tid; CxFilter *cli; } MfCall;

TEST(ctxthr_msgfilter_incoming_call_handled_mta_client)
{
    CxSrv s; CxFilter f; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f); f.ret_incoming = SERVERCALL_ISHANDLED;
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    CXEQ_HR(s.filter_hr, S_OK);
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(CXM_MTA, w_mf_call, &a);
    CXEQ_HR(a.hr, S_OK);
    CXEQ_HR(a.hr2, S_OK);
    CXEQ(s.cf.lock_calls, 2);
    CXEQ(s.cf.last_tid, s.tid);                      /* the object runs in its STA thread */
    CHECK(f.incoming >= 2);                          /* the filter saw each call (plus COM's own release traffic) before dispatch */
    CXEQ(f.incoming_tid, s.tid);                     /* ... on the apartment thread */
    CXEQ(f.retry, 0); CXEQ(f.pending, 0);
    {
        int k0 = find_stream_call(&f, 0), k1 = find_stream_call(&f, 1);
        CHECK(k0 >= 0 && k1 >= 0);                   /* both Revert calls were announced with their interface */
        if (k0 >= 0) {
            CXEQ(f.log_type[k0], CALLTYPE_TOPLEVEL);
            CXEQ(f.log_info[k0].wMethod, 9);         /* vtable index of IStream::Revert, IUnknown methods included */
            t_note("INTERFACEINFO.pUnk=%p (server object %p), htaskCaller=%lu (client tid %lu)", (void *)f.log_info[k0].pUnk, (void *)&s.cf,
                   (unsigned long)f.log_caller[k0], (unsigned long)a.tid);
        }
        if (k1 >= 0) CXEQ(f.log_type[k1], CALLTYPE_TOPLEVEL);
    }
    cx_srv_stop(&s);
}
TEST(ctxthr_msgfilter_incoming_call_handled_sta_client)
{
    CxSrv s; CxFilter f; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(CXM_STA, w_mf_call, &a);
    CXEQ_HR(a.hr, S_OK); CXEQ_HR(a.hr2, S_OK);
    CHECK(f.incoming >= 2);
    { int k0 = find_stream_call(&f, 0); CHECK(k0 >= 0); if (k0 >= 0) CXEQ(f.log_type[k0], CALLTYPE_TOPLEVEL); }
    cx_srv_stop(&s);
}

/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(ctxthr_msgfilter_incoming_call_rejected)
{
    CxSrv s; CxFilter f; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    f.ret_incoming_seq[0] = SERVERCALL_REJECTED; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(CXM_STA, w_mf_call, &a);                  /* caller without a filter of its own */
    CXEQ_HR(a.hr, CX_HR_CALLREJECTED);              /* first call: rejected, the method never ran */
    CXEQ_HR(a.hr2, S_OK);                           /* second call: handled */
    CXEQ(s.cf.lock_calls, 1);
    CHECK(f.incoming >= 2);
    cx_srv_stop(&s);
}
TEST(ctxthr_msgfilter_incoming_call_rejected_mta_client)
{
    CxSrv s; CxFilter f; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    f.ret_incoming_seq[0] = SERVERCALL_REJECTED; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(CXM_MTA, w_mf_call, &a);
    t_note("MTA client, server filter rejects: first call 0x%08lx, second 0x%08lx", (unsigned long)a.hr, (unsigned long)a.hr2);
    CXEQ_HR(a.hr, CX_HR_CALLREJECTED);
    CXEQ_HR(a.hr2, S_OK);
    CXEQ(s.cf.lock_calls, 1);
    cx_srv_stop(&s);
}

/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(ctxthr_msgfilter_retry_later_consults_caller_filter)
{
    CxSrv s; CxFilter f, cli; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f); cx_filter_init(&cli);
    f.ret_incoming_seq[0] = SERVERCALL_RETRYLATER; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    cli.ret_retry = 0;                               /* retry immediately */
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s; a.cli = &cli;
    cx_run(CXM_STA, w_mf_call, &a);
    CXEQ_HR(a.hr, S_OK);                             /* the retry succeeded */
    CXEQ_HR(a.hr2, S_OK);
    CXEQ(cli.retry, 1);
    CXEQ(cli.last_reject_type, SERVERCALL_RETRYLATER);
    CHECK(f.incoming >= 3);                          /* first attempt + retried call + second call */
    CXEQ(s.cf.lock_calls, 2);
    cx_srv_stop(&s);
}
TEST(ctxthr_msgfilter_retry_later_delay_is_honoured)
{
    CxSrv s; CxFilter f, cli; MfCall a; DWORD t0, el;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f); cx_filter_init(&cli);
    f.ret_incoming_seq[0] = SERVERCALL_RETRYLATER; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    cli.ret_retry = 250;                             /* wait 250 ms, then retry */
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s; a.cli = &cli;
    t0 = GetTickCount();
    cx_run(CXM_STA, w_mf_call, &a);
    el = GetTickCount() - t0;
    CXEQ_HR(a.hr, S_OK);
    CXEQ(cli.retry, 1);
    CHECK(el >= 200);                                /* generous lower bound only */
    cx_srv_stop(&s);
}
TEST(ctxthr_msgfilter_retry_later_caller_cancels)
{
    CxSrv s; CxFilter f, cli; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f); cx_filter_init(&cli);
    f.ret_incoming_seq[0] = SERVERCALL_RETRYLATER; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    cli.ret_retry = (DWORD)-1;                       /* give up */
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s; a.cli = &cli;
    cx_run(CXM_STA, w_mf_call, &a);
    CXEQ_HR(a.hr, CX_HR_CALLREJECTED);               /* documented: -1 cancels, the call fails with RPC_E_CALL_REJECTED */
    CXEQ(cli.retry, 1);
    CXEQ(cli.last_reject_type, SERVERCALL_RETRYLATER);
    CXEQ_HR(a.hr2, S_OK);
    CXEQ(s.cf.lock_calls, 1);
    cx_srv_stop(&s);
}
TEST(ctxthr_msgfilter_retry_later_without_caller_filter_observed)
{
    CxSrv s; CxFilter f; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&f);
    f.ret_incoming_seq[0] = SERVERCALL_RETRYLATER; f.ret_incoming_seq[1] = SERVERCALL_ISHANDLED; f.seq_len = 2;
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_STA, &f)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(CXM_STA, w_mf_call, &a);
    t_note("RETRYLATER with no caller filter: first call 0x%08lx, second 0x%08lx, server saw %ld incoming", (unsigned long)a.hr, (unsigned long)a.hr2, (long)f.incoming);
    cx_srv_stop(&s);
}

/* An MTA server never consults a message filter; the STA client's filter is not involved in a successful call. */
TEST(ctxthr_msgfilter_not_used_for_mta_server)
{
    CxSrv s; CxFilter cli; MfCall a;
    REQUIRE(U(CoRegisterMessageFilter));
    cx_filter_init(&cli);
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, CXM_MTA, NULL)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    memset(&a, 0, sizeof a); a.s = &s; a.cli = &cli;
    cx_run(CXM_STA, w_mf_call, &a);
    CXEQ_HR(a.hr, S_OK); CXEQ_HR(a.hr2, S_OK);
    CXEQ(cli.retry, 0);
    CXEQ(cli.incoming, 0);
    CHECK(s.cf.last_tid != 0 && s.cf.last_tid != a.tid);   /* ran on an MTA thread, not the client's */
    cx_srv_stop(&s);
}

/* ================= CoRegisterChannelHook ================= */
typedef struct HookState {
    volatile LONG active;
    LONG refs_added;
    LONG cgs, cfb, cn, sn, sgs, sfb;
    int bad_extent, bad_pid, bad_method, bad_data, bad_causality;
    GUID causality; int have_causality;
    ULONG server_data_size; unsigned char server_data;
    DWORD client_tid, server_tid;
    DWORD seen_method; int seen_iid_ok;
    HRESULT client_notify_hr;
    int bad_cbsize;
} HookState;
static HookState hs;
static LONG hook_ref = 1;

static int is_stream_call(REFIID riid) { return IsEqualGUID(riid, &CX_IID_IStream); }
static HRESULT WINAPI hk_QI(IChannelHook *i, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &CX_IID_IUnknown) || IsEqualGUID(riid, &CX_IID_IChannelHook)) { *ppv = i; InterlockedIncrement(&hook_ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI hk_AddRef(IChannelHook *i) { InterlockedIncrement(&hs.refs_added); return InterlockedIncrement(&hook_ref); }
static ULONG WINAPI hk_Release(IChannelHook *i) { return InterlockedDecrement(&hook_ref) > 0 ? hook_ref : 1; }
static void hk_check(REFGUID ext, REFIID riid, int need_object)
{
    SChannelHookCallInfo *info = (SChannelHookCallInfo *)riid;
    if (!IsEqualGUID(ext, &CX_EXTENT_OLET)) hs.bad_extent = 1;
    if (info->cbSize != sizeof *info) { hs.bad_cbsize = 1; return; }
    if (info->dwServerPid != GetCurrentProcessId()) hs.bad_pid = 1;
    if (info->iMethod != 9) hs.bad_method = 1;
    hs.seen_method = info->iMethod;
    if (need_object && !info->pObject) hs.bad_data = 1;
    if (!hs.have_causality) { hs.causality = info->uCausality; hs.have_causality = 1; }
    else if (!IsEqualGUID(&hs.causality, &info->uCausality)) hs.bad_causality = 1;
}
static void WINAPI hk_ClientGetSize(IChannelHook *i, REFGUID ext, REFIID riid, ULONG *size)
{
    *size = 0;
    if (!hs.active || !is_stream_call(riid)) return;
    InterlockedIncrement(&hs.cgs); hs.client_tid = GetCurrentThreadId();
    hk_check(ext, riid, 0);
    *size = 1;
}
static void WINAPI hk_ClientFillBuffer(IChannelHook *i, REFGUID ext, REFIID riid, ULONG *size, void *buf)
{
    if (!hs.active || !is_stream_call(riid)) { *size = 0; return; }
    InterlockedIncrement(&hs.cfb);
    hk_check(ext, riid, 0);
    *(unsigned char *)buf = 0xcc; *size = 1;
}
static void WINAPI hk_ClientNotify(IChannelHook *i, REFGUID ext, REFIID riid, ULONG cb, void *buf, DWORD rep, HRESULT hrFault)
{
    if (!hs.active || !is_stream_call(riid)) return;
    InterlockedIncrement(&hs.cn); hs.client_notify_hr = hrFault;
    hk_check(ext, riid, 0);
}
static void WINAPI hk_ServerNotify(IChannelHook *i, REFGUID ext, REFIID riid, ULONG cb, void *buf, DWORD rep)
{
    if (!hs.active || !is_stream_call(riid)) return;
    InterlockedIncrement(&hs.sn); hs.server_tid = GetCurrentThreadId();
    hk_check(ext, riid, 1);
    hs.server_data_size = cb; if (cb) hs.server_data = *(unsigned char *)buf;
}
static void WINAPI hk_ServerGetSize(IChannelHook *i, REFGUID ext, REFIID riid, HRESULT hrFault, ULONG *size)
{
    *size = 0;
    if (!hs.active || !is_stream_call(riid)) return;
    InterlockedIncrement(&hs.sgs);
    hk_check(ext, riid, 1);
}
static void WINAPI hk_ServerFillBuffer(IChannelHook *i, REFGUID ext, REFIID riid, ULONG *size, void *buf, HRESULT hrFault)
{
    *size = 0;
    if (hs.active && is_stream_call(riid)) InterlockedIncrement(&hs.sfb);
}
static const IChannelHookVtbl hook_vtbl = { hk_QI, hk_AddRef, hk_Release, hk_ClientGetSize, hk_ClientFillBuffer, hk_ClientNotify, hk_ServerNotify, hk_ServerGetSize, hk_ServerFillBuffer };
static IChannelHook the_hook = { &hook_vtbl };   /* channel hooks cannot be unregistered: static for the life of the process */

/* Each registration adds a hook entry; there is no duplicate check, so register ours exactly once. */
static HRESULT ensure_hook(void)
{
    static int done;
    HRESULT hr = S_OK;
    if (!done) { hr = U(CoRegisterChannelHook)(&CX_EXTENT_OLET, &the_hook); done = 1; }
    return hr;
}

/* a second, inert hook (never produces data) used to observe registration itself */
static LONG null_refs, null_qi;
static HRESULT WINAPI nh_QI(IChannelHook *i, REFIID riid, void **ppv) { InterlockedIncrement(&null_qi); *ppv = NULL; return E_NOINTERFACE; }
static ULONG WINAPI nh_AddRef(IChannelHook *i) { return InterlockedIncrement(&null_refs) + 1; }
static ULONG WINAPI nh_Release(IChannelHook *i) { return 2; }
static void WINAPI nh_CGS(IChannelHook *i, REFGUID e, REFIID r, ULONG *s) { *s = 0; }
static void WINAPI nh_CFB(IChannelHook *i, REFGUID e, REFIID r, ULONG *s, void *b) { *s = 0; }
static void WINAPI nh_CN(IChannelHook *i, REFGUID e, REFIID r, ULONG c, void *b, DWORD d, HRESULT h) { }
static void WINAPI nh_SN(IChannelHook *i, REFGUID e, REFIID r, ULONG c, void *b, DWORD d) { }
static void WINAPI nh_SGS(IChannelHook *i, REFGUID e, REFIID r, HRESULT h, ULONG *s) { *s = 0; }
static void WINAPI nh_SFB(IChannelHook *i, REFGUID e, REFIID r, ULONG *s, void *b, HRESULT h) { *s = 0; }
static const IChannelHookVtbl null_vtbl = { nh_QI, nh_AddRef, nh_Release, nh_CGS, nh_CFB, nh_CN, nh_SN, nh_SGS, nh_SFB };
static IChannelHook null_hook = { &null_vtbl };
CXG(CX_EXTENT_OLET2, 0x6f1a0c02, 0x3e5b, 0x4d27, 0x9a,0x10,0x4c,0x77,0x0b,0x2e,0x91,0xd5);

TEST(ctxthr_channelhook_register_takes_a_reference)
{
    REQUIRE(U(CoRegisterChannelHook));
    CXEQ(null_refs, 0);
    CXEQ_HR(U(CoRegisterChannelHook)(&CX_EXTENT_OLET2, &null_hook), S_OK);
    CXEQ(null_refs, 1);                              /* exactly one AddRef ... */
    CXEQ(null_qi, 0);                                /* ... and no QueryInterface */
    t_note("channel hooks cannot be unregistered; the hook object stays registered until process exit");
}

typedef struct HookCall { CxSrv *s; HRESULT hr; DWORD tid; } HookCall;
static void w_hook_call(void *p)
{
    HookCall *a = p;
    IStream *cf = cx_srv_proxy(a->s);
    a->tid = GetCurrentThreadId();
    if (!cf) { a->hr = E_POINTER; return; }
    hs.client_tid = 0;
    a->hr = IStream_Revert(cf);
    IStream_Release(cf);
}
static void run_hook_call(DWORD server_model, DWORD client_model)
{
    CxSrv s; HookCall a;
    memset(&hs.cgs, 0, sizeof hs - offsetof(HookState, cgs));
    hs.active = 1;
    memset(&s, 0, sizeof s);
    if (!cx_srv_start(&s, server_model, NULL)) { CHECK(!"server"); cx_srv_stop(&s); hs.active = 0; return; }
    memset(&a, 0, sizeof a); a.s = &s;
    cx_run(client_model, w_hook_call, &a);
    hs.active = 0;
    CXEQ_HR(a.hr, S_OK);
    CXEQ(s.cf.lock_calls, 1);
    cx_srv_stop(&s);
    /* every callback ran once for the call on IStream::Revert, all with our extent */
    CXEQ(hs.cgs, 1);
    CXEQ(hs.cfb, 1);
    CXEQ(hs.cn, 1);
    CXEQ(hs.sn, 1);
    CXEQ(hs.sgs, 1);
    CXEQ(hs.sfb, 0);                                 /* the server added no data, so no fill call */
    CXEQ(hs.bad_extent, 0);
    CXEQ(hs.bad_cbsize, 0);
    CXEQ(hs.bad_pid, 0);                             /* SChannelHookCallInfo.dwServerPid */
    CXEQ(hs.bad_method, 0);                          /* iMethod counts IUnknown: Revert is method 9 */
    CXEQ(hs.bad_data, 0);                            /* pObject is set on the server side */
    CXEQ(hs.bad_causality, 0);
    CXEQ(hs.server_data_size, 1);                    /* the client's buffer arrives intact */
    CXEQ(hs.server_data, 0xcc);
    CXEQ_HR(hs.client_notify_hr, S_OK);
    CXNE(hs.server_tid, hs.client_tid);
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(ctxthr_channelhook_callbacks_mta_client_sta_server) { REQUIRE(U(CoRegisterChannelHook)); CXEQ_HR(ensure_hook(), S_OK); run_hook_call(CXM_STA, CXM_MTA); }
TEST(ctxthr_channelhook_callbacks_sta_client_sta_server) { REQUIRE(U(CoRegisterChannelHook)); CXEQ_HR(ensure_hook(), S_OK); run_hook_call(CXM_STA, CXM_STA); }
