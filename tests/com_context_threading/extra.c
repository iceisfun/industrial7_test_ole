/* State visible inside incoming cross-apartment calls, STA waits that service calls, COINIT flag handling, RoInitialize apartments. */
#include "cx.h"

/* ================= what a server method sees ================= */
typedef struct ProbeRes { CxSrv *s; HRESULT hr; DWORD tid; GUID lid; HRESULT lid_hr; ULONG_PTR srv_token; } ProbeRes;
static void srv_get_token(CxSrv *s) { U(CoGetContextToken)((ULONG_PTR *)s->user); }
static void w_probe_call(void *p)
{
    ProbeRes *a = p;
    IStream *cf = cx_srv_proxy(a->s);
    a->tid = GetCurrentThreadId();
    a->lid_hr = U(CoGetCurrentLogicalThreadId)(&a->lid);
    if (!cf) { a->hr = E_POINTER; return; }
    a->hr = IStream_Revert(cf);
    IStream_Release(cf);
}
static void run_probe(DWORD server_model, DWORD client_model)
{
    CxSrv s; ProbeRes a; ULONG_PTR tok = 0;
    memset(&s, 0, sizeof s);
    s.on_start = srv_get_token; s.user = &tok;
    if (!cx_srv_start(&s, server_model, NULL)) { CHECK(!"server"); cx_srv_stop(&s); return; }
    s.cf.probe = 1;
    memset(&a, 0, sizeof a); a.s = &s; a.srv_token = tok;
    cx_run(client_model, w_probe_call, &a);
    CXEQ_HR(a.hr, S_OK);
    CXEQ(s.cf.lock_calls, 1);
    CXEQ_HR(s.cf.p_apt_hr, S_OK);
    CXEQ_HR(s.cf.p_tok_hr, S_OK);
    CXEQ(s.cf.p_token, tok);                           /* the method runs in the object's context */
    if (server_model == CXM_STA) {
        CXEQ(s.cf.p_tid, s.tid);
        CHECK(cx_is_sta_type(s.cf.p_type));
    } else {
        CXEQ(s.cf.p_type, APTTYPE_MTA);
        if (client_model == CXM_MTA) CXEQ(s.cf.p_tid, a.tid);   /* same apartment: the unmarshaled pointer is the object itself, a direct call */
        else CXNE(s.cf.p_tid, a.tid);                  /* from an STA the call crosses apartments and runs on an MTA thread */
    }
    CXEQ(s.cf.p_qual, APTTYPEQUALIFIER_NONE);
    CXEQ_HR(s.cf.p_lid_hr, S_OK);
    t_note("server %s / client %s: incoming call has logical thread id %s the caller's", server_model == CXM_STA ? "STA" : "MTA",
           client_model == CXM_STA ? "STA" : "MTA", IsEqualGUID(&s.cf.p_lid, &a.lid) ? "equal to" : "different from");
    t_note("inside the call: CoGetCallContext(IUnknown)=0x%08lx CoGetCallContext(ICancelMethodCalls)=0x%08lx CoTestCancel=0x%08lx",
           (unsigned long)s.cf.p_ctx_unk_hr, (unsigned long)s.cf.p_ctx_cancel_hr, (unsigned long)s.cf.p_test_hr);
    cx_srv_stop(&s);
}
TEST(ctxthr_incoming_call_state_sta_server_mta_client) { REQUIRE(U(CoGetApartmentType)); run_probe(CXM_STA, CXM_MTA); }
TEST(ctxthr_incoming_call_state_sta_server_sta_client) { REQUIRE(U(CoGetApartmentType)); run_probe(CXM_STA, CXM_STA); }
TEST(ctxthr_incoming_call_state_mta_server_sta_client) { REQUIRE(U(CoGetApartmentType)); run_probe(CXM_MTA, CXM_STA); }
TEST(ctxthr_incoming_call_state_mta_server_mta_client) { REQUIRE(U(CoGetApartmentType)); run_probe(CXM_MTA, CXM_MTA); }

/* ================= an STA blocked in CoWaitForMultipleHandles still services incoming calls ================= */
typedef struct WaitSrv { CxCF cf; CxFilter *filter; HANDLE ready, stop, thread; IStream *stream; HRESULT wait_hr; ULONG wait_idx; DWORD tid; HRESULT marshal_hr; DWORD flags; } WaitSrv;
static DWORD WINAPI waitsrv_thread(LPVOID p)
{
    WaitSrv *w = p;
    w->tid = GetCurrentThreadId();
    cx_init(CXM_STA);
    if (w->filter) { IMessageFilter *prev = NULL; U(CoRegisterMessageFilter)((IMessageFilter *)w->filter, &prev); if (prev) IMessageFilter_Release(prev); }
    w->marshal_hr = U(CoMarshalInterThreadInterfaceInStream)(&CX_IID_IStream, (IUnknown *)&w->cf, &w->stream);
    SetEvent(w->ready);
    w->wait_idx = 0xdeadbeef;
    w->wait_hr = U(CoWaitForMultipleHandles)(w->flags, 7000, 1, &w->stop, &w->wait_idx);
    if (w->filter) U(CoRegisterMessageFilter)(NULL, NULL);
    cx_uninit();
    return 0;
}
typedef struct WaitCli { WaitSrv *w; HRESULT hr; DWORD ms; } WaitCli;
static void w_waitsrv_client(void *p)
{
    WaitCli *c = p;
    void *pv = NULL;
    HRESULT hr = U(CoGetInterfaceAndReleaseStream)(c->w->stream, &CX_IID_IStream, &pv);
    DWORD t0 = GetTickCount();
    c->w->stream = NULL;
    if (CX_FAILED(hr)) { c->hr = hr; return; }
    c->hr = IStream_Revert((IStream *)pv);
    c->ms = GetTickCount() - t0;
    IStream_Release((IStream *)pv);
}
static void run_waitsrv(DWORD flags, DWORD client_model, int with_filter)
{
    WaitSrv w; WaitCli c; CxFilter f;
    memset(&w, 0, sizeof w);
    cx_cf_init(&w.cf); cx_filter_init(&f);
    w.flags = flags; w.filter = with_filter ? &f : NULL;
    w.ready = cx_event(TRUE, FALSE); w.stop = cx_event(TRUE, FALSE);
    w.thread = CreateThread(NULL, 0, waitsrv_thread, &w, 0, NULL);
    if (!w.thread || WaitForSingleObject(w.ready, 8000) != WAIT_OBJECT_0 || CX_FAILED(w.marshal_hr)) { CHECK(!"server"); SetEvent(w.stop); return; }
    memset(&c, 0, sizeof c); c.w = &w;
    cx_run(client_model, w_waitsrv_client, &c);
    CXEQ_HR(c.hr, S_OK);                               /* serviced while the server thread was inside the wait */
    CXEQ(w.cf.lock_calls, 1);
    CXEQ(w.cf.last_tid, w.tid);
    CHECK(c.ms < 5000);
    if (with_filter) {
        CHECK(f.incoming >= 1);
        t_note("filter saw call type %lu while the STA was inside CoWaitForMultipleHandles(0x%lx)", (unsigned long)f.log_type[0], (unsigned long)flags);
    }
    SetEvent(w.stop);
    WaitForSingleObject(w.thread, 10000);
    CXEQ_HR(w.wait_hr, S_OK);                          /* the wait itself completes normally afterwards */
    CXEQ(w.wait_idx, 0);
    CloseHandle(w.thread); CloseHandle(w.ready); CloseHandle(w.stop);
}
TEST(ctxthr_cowait_sta_services_incoming_calls_mta_client)  { REQUIRE(U(CoWaitForMultipleHandles)); run_waitsrv(0, CXM_MTA, 0); }
TEST(ctxthr_cowait_sta_services_incoming_calls_sta_client)  { REQUIRE(U(CoWaitForMultipleHandles)); run_waitsrv(0, CXM_STA, 0); }
TEST(ctxthr_cowait_sta_services_incoming_calls_through_filter) { REQUIRE(U(CoWaitForMultipleHandles)); run_waitsrv(0, CXM_MTA, 1); }
TEST(ctxthr_cowait_sta_services_incoming_calls_with_dispatch_flags) { REQUIRE(U(CoWaitForMultipleHandles)); run_waitsrv(COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, CXM_MTA, 0); }

/* ================= COINIT flag handling ================= */
static void w_flags(void *p)
{
    static const struct { DWORD flags; int want_sta; } t[] = {
        { COINIT_MULTITHREADED, 0 }, { COINIT_APARTMENTTHREADED, 1 },
        { COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE, 0 }, { COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE, 1 },
        { COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY, 0 }, { COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY, 1 },
        { COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY, 1 } };
    int i;
    for (i = 0; i < 7; i++) {
        APTTYPE at = (APTTYPE)0x5a; APTTYPEQUALIFIER q = (APTTYPEQUALIFIER)0x5a;
        HRESULT hr = U(CoInitializeEx)(NULL, t[i].flags);
        CHECKF(hr == S_OK, "flags 0x%lx: 0x%08lx", (unsigned long)t[i].flags, (unsigned long)hr);
        if (hr == S_OK) {
            HRESULT ah = U(CoGetApartmentType)(&at, &q);
            CHECKF(ah == S_OK && (t[i].want_sta ? cx_is_sta_type(at) : at == APTTYPE_MTA), "flags 0x%lx: apt %d hr 0x%08lx", (unsigned long)t[i].flags, (int)at, (unsigned long)ah);
            U(CoUninitialize)();
        }
    }
}
TEST(ctxthr_coinit_flag_combinations_select_apartment) { REQUIRE(U(CoInitializeEx)); cx_run(CXM_NONE, w_flags, NULL); }

static void w_flags_bad(void *p)
{
    HRESULT hr = U(CoInitializeEx)((void *)0x1, COINIT_MULTITHREADED);
    t_note("CoInitializeEx with a non-NULL reserved pointer: 0x%08lx", (unsigned long)hr);
    if (!CX_FAILED(hr)) U(CoUninitialize)();
    hr = U(CoInitializeEx)(NULL, 0x40);
    t_note("CoInitializeEx with an undefined COINIT bit (0x40): 0x%08lx", (unsigned long)hr);
    if (!CX_FAILED(hr)) U(CoUninitialize)();
    hr = U(CoInitializeEx)(NULL, 0x10);
    t_note("CoInitializeEx with an undefined COINIT bit (0x10): 0x%08lx", (unsigned long)hr);
    if (!CX_FAILED(hr)) U(CoUninitialize)();
}
TEST(ctxthr_coinit_undefined_flags_observed) { REQUIRE(U(CoInitializeEx)); cx_run(CXM_NONE, w_flags_bad, NULL); }

/* ================= RoInitialize apartments ================= */
UDECL(HRESULT, RoInitialize, (int type));
UDECL(void, RoUninitialize, (void));
static void w_ro_mta(void *p)
{
    APTTYPE at; APTTYPEQUALIFIER q = (APTTYPEQUALIFIER)0x5a;
    HRESULT hr = UN(RoInitialize)(1 /* RO_INIT_MULTITHREADED */);
    CXEQ_HR(hr, S_OK);
    at = (APTTYPE)0x5a;
    CXEQ_HR(U(CoGetApartmentType)(&at, &q), S_OK);
    CXEQ(at, APTTYPE_MTA); CXEQ(q, APTTYPEQUALIFIER_NONE);
    CXEQ_HR(UN(RoInitialize)(1), S_FALSE);                  /* nested */
    CXEQ_HR(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), CX_HR_CHANGEDMODE);   /* the apartment is already MTA */
    UN(RoUninitialize)(); UN(RoUninitialize)();
    q = (APTTYPEQUALIFIER)0x5a;
    CXEQ_HR(U(CoGetApartmentType)(&at, &q), CX_HR_NOTINIT);
}
TEST(ctxthr_ro_initialize_multithreaded)
{
    REQUIRE(UN(RoInitialize)); REQUIRE(UN(RoUninitialize));
    if (t_is_wine()) { t_skip("RoInitialize under Wine is not representative"); return; }
    cx_run(CXM_NONE, w_ro_mta, NULL);
}
static void w_ro_invalid(void *p)
{
    CXEQ_HR(UN(RoInitialize)(2), E_INVALIDARG);
    CXEQ_HR(UN(RoInitialize)(-1), E_INVALIDARG);
    CXEQ_HR(UN(RoInitialize)(0x100), E_INVALIDARG);
    { APTTYPE at; APTTYPEQUALIFIER q; CXEQ_HR(U(CoGetApartmentType)(&at, &q), CX_HR_NOTINIT); }   /* nothing was initialized */
}
TEST(ctxthr_ro_initialize_invalid_type)
{
    REQUIRE(UN(RoInitialize));
    if (t_is_wine()) { t_skip("RoInitialize under Wine is not representative"); return; }
    cx_run(CXM_NONE, w_ro_invalid, NULL);
}
static void w_ro_sta(void *p)
{
    APTTYPE at = (APTTYPE)0x5a; APTTYPEQUALIFIER q = (APTTYPEQUALIFIER)0x5a;
    CxFilter f; IMessageFilter *prev = NULL;
    HRESULT hr;
    cx_filter_init(&f);
    hr = UN(RoInitialize)(0 /* RO_INIT_SINGLETHREADED */);
    CXEQ_HR(hr, S_OK);
    CXEQ_HR(U(CoGetApartmentType)(&at, &q), S_OK);
    CHECK(cx_is_sta_type(at));
    t_note("RoInitialize(RO_INIT_SINGLETHREADED): apartment type %d qualifier %d (6 = APPLICATION_STA)", (int)at, (int)q);
    if (q == APTTYPEQUALIFIER_APPLICATION_STA) {
        /* an ASTA has no message filter */
        CXEQ_HR(U(CoRegisterMessageFilter)((IMessageFilter *)&f, &prev), CX_HR_NOTSUPP);
        CXEQ(f.ref, 1);
    }
    UN(RoUninitialize)();
}
TEST(ctxthr_ro_initialize_singlethreaded_observed)
{
    REQUIRE(UN(RoInitialize)); REQUIRE(UN(RoUninitialize));
    if (t_is_wine()) { t_skip("RoInitialize under Wine is not representative"); return; }
    cx_run(CXM_NONE, w_ro_sta, NULL);
}
