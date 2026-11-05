/* combase rare exports: process-level helpers that are not in the SDK headers (CoDecodeProxy, CoRegisterConsoleHandles,
 * CoRegisterRacActivationToken, UpdateProcessTracing, UpdateDCOMSettings, GetCatalogHelper, DcomChannelSetHResult) and the
 * CStdStubBuffer_Invoke method gate. Argument validation and failure paths are asserted; values that come from the
 * resolver service (rpcss) are recorded with t_note. Expectations are the Windows 10 22H2 (19045) results. */
#include "t.h"
#include <objbase.h>
#include <setjmp.h>

#define E_ACCESSDENIED_ ((HRESULT)0x80070005)
#define RPC_X_NULL_REF_POINTER_ ((HRESULT)0x800706F4)
#define RPC_X_BAD_STUB_DATA_ ((HRESULT)0x800706F7)
#define RPC_E_CALL_COMPLETE_ ((HRESULT)0x80010117)
#define CO_E_NOTINITIALIZED_ ((HRESULT)0x800401F0)

typedef struct { DWORD pid; DWORD tid; UINT64 addr; } ServerInfo_;
UDECL(HRESULT, CoDecodeProxy, (DWORD, UINT64, ServerInfo_ *));
UDECL(HRESULT, CoReactivateObject, (IUnknown *, IUnknown *));
UDECL(HRESULT, CoRegisterConsoleHandles, (HANDLE, HANDLE, HANDLE, UINT64 *));
UDECL(HRESULT, CoRevokeConsoleHandles, (UINT64));
UDECL(HRESULT, CoRegisterRacActivationToken, (HANDLE, UINT64 *));
UDECL(HRESULT, CoRevokeRacActivationToken, (UINT64));
UDECL(HRESULT, UpdateProcessTracing, (DWORD, int));
UDECL(void, UpdateDCOMSettings, (void));
UDECL(HRESULT, GetCatalogHelper, (const GUID *, void **));
UDECL(HRESULT, DcomChannelSetHResult, (void *, ULONG *, HRESULT));

static const GUID X_IID_IUnknown = {0, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID X_IID_IRpcStubBuffer = {0xD5F56AFC, 0x593B, 0x101A, {0xB5, 0x69, 0x08, 0x00, 0x2B, 0x2D, 0xBF, 0x7A}};
static const GUID X_IID_Bogus = {0x6f1c9a31, 0x3c1d, 0x4b7e, {0x9a, 0x10, 0x6f, 0x6c, 0x74, 0x65, 0x00, 0x31}};
static const GUID X_CLSID_StdGlobalInterfaceTable = {0x00000323, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

#define XEQ(a, b) EXPECT_EQ((ULONG_PTR)(ULONG)(a), (ULONG_PTR)(ULONG)(b))

/* ---- CoDecodeProxy ---- */
TEST(combase_ext_decode_proxy_invalid_args) {
    ServerInfo_ si; memset(&si, 0xCC, sizeof si);
    REQUIRE(UN(CoDecodeProxy));
    XEQ(UN(CoDecodeProxy)(0, 0x1000, &si), E_INVALIDARG);
    XEQ(UN(CoDecodeProxy)(GetCurrentProcessId(), 0, &si), E_INVALIDARG);
    XEQ(UN(CoDecodeProxy)(GetCurrentProcessId(), 0x1000, NULL), E_INVALIDARG);
    XEQ(UN(CoDecodeProxy)(0, 0, NULL), E_INVALIDARG);
    EXPECT_EQ(si.pid, 0xCCCCCCCC);                         /* the argument gate leaves the output alone */
    EXPECT_EQ(si.addr, 0xCCCCCCCCCCCCCCCCull);
}
TEST(combase_ext_decode_proxy_not_a_proxy_is_refused) {
    ServerInfo_ si; HRESULT hr;
    REQUIRE(UN(CoDecodeProxy));
    memset(&si, 0xCC, sizeof si);
    hr = UN(CoDecodeProxy)(GetCurrentProcessId(), 0x1000, &si);
    t_note("CoDecodeProxy(own pid, 0x1000) = %08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    EXPECT_EQ(si.pid, 0); EXPECT_EQ(si.tid, 0); EXPECT_EQ(si.addr, 0);   /* cleared before the lookup */
    memset(&si, 0xCC, sizeof si);
    hr = UN(CoDecodeProxy)(0x0FFFFFF0, 0x1000, &si);
    t_note("CoDecodeProxy(unknown pid, 0x1000) = %08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
}

/* ---- CoReactivateObject ---- */
TEST(combase_ext_reactivate_object_invalid_args) {
    IUnknown *u = NULL; HRESULT hr;
    REQUIRE(UN(CoReactivateObject)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoCreateInstance));
    XEQ(UN(CoReactivateObject)(NULL, NULL), E_INVALIDARG);
    XEQ(UN(CoReactivateObject)((IUnknown *)1, NULL), E_INVALIDARG);
    XEQ(UN(CoReactivateObject)(NULL, (IUnknown *)1), E_INVALIDARG);
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) { t_skip("CoInitializeEx"); return; }
    hr = U(CoCreateInstance)(&X_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &X_IID_IUnknown, (void **)&u);
    XEQ(hr, S_OK);
    if (u) {
        /* the second object has to be a standard-marshaling identity; a plain in-process object is not one */
        XEQ(UN(CoReactivateObject)(u, u), E_INVALIDARG);
        u->lpVtbl->Release(u);
    }
    U(CoUninitialize)();
}

/* ---- CoRegisterConsoleHandles / CoRevokeConsoleHandles ---- */
TEST(combase_ext_console_handles_gates) {
    UINT64 tok = 0x77; HANDLE r, w, r2, w2, ev;
    REQUIRE(UN(CoRegisterConsoleHandles)); REQUIRE(UN(CoRevokeConsoleHandles));
    CHECK(CreatePipe(&r, &w, NULL, 0)); CHECK(CreatePipe(&r2, &w2, NULL, 0));
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    XEQ(UN(CoRegisterConsoleHandles)(NULL, w, w2, &tok), E_INVALIDARG);
    XEQ(UN(CoRegisterConsoleHandles)(INVALID_HANDLE_VALUE, w, w2, &tok), E_INVALIDARG);
    XEQ(UN(CoRegisterConsoleHandles)(r, NULL, w2, &tok), E_INVALIDARG);
    XEQ(UN(CoRegisterConsoleHandles)(r, w, INVALID_HANDLE_VALUE, &tok), E_INVALIDARG);
    XEQ(UN(CoRegisterConsoleHandles)(ev, w, w2, &tok), E_INVALIDARG);      /* not pipes */
    XEQ(UN(CoRegisterConsoleHandles)(r, ev, w2, &tok), E_INVALIDARG);
    XEQ(UN(CoRegisterConsoleHandles)(r, w, ev, &tok), E_INVALIDARG);
    EXPECT_EQ(tok, 0x77);
    CloseHandle(ev); CloseHandle(r); CloseHandle(w); CloseHandle(r2); CloseHandle(w2);
}
TEST(combase_ext_console_handles_register_revoke) {
    UINT64 tok = 0x77, tok2 = 0x77; HANDLE r, w, w2; HRESULT hr;
    REQUIRE(UN(CoRegisterConsoleHandles)); REQUIRE(UN(CoRevokeConsoleHandles));
    { HANDLE r2, w3, r3; CHECK(CreatePipe(&r, &w3, NULL, 0)); CHECK(CreatePipe(&r2, &w, NULL, 0)); CHECK(CreatePipe(&r3, &w2, NULL, 0));
      CloseHandle(w3); CloseHandle(r2); CloseHandle(r3); }   /* r: a pipe read end, w and w2: pipe write ends */
    XEQ(UN(CoRegisterConsoleHandles)(r, w, w2, NULL), RPC_X_NULL_REF_POINTER_);   /* the client stub refuses a NULL out pointer */
    hr = UN(CoRegisterConsoleHandles)(r, w, w2, &tok);
    XEQ(hr, S_OK);
    t_note("console token = %llx", (unsigned long long)tok);
    CHECK(tok != 0 && tok != 0x77);
    hr = UN(CoRegisterConsoleHandles)(r, w, w2, &tok2);
    XEQ(hr, S_OK);
    CHECK(tok2 != 0 && tok2 != tok);                                   /* a second registration is a second token */
    XEQ(UN(CoRevokeConsoleHandles)(tok), S_OK);
    XEQ(UN(CoRevokeConsoleHandles)(tok), S_OK);                         /* revoking twice, zero, or an unknown value is S_OK */
    XEQ(UN(CoRevokeConsoleHandles)(0), S_OK);
    XEQ(UN(CoRevokeConsoleHandles)(0x1234abcd), S_OK);
    XEQ(UN(CoRevokeConsoleHandles)(tok2), S_OK);
    CloseHandle(r); CloseHandle(w); CloseHandle(w2);
}

/* ---- CoRegisterRacActivationToken / CoRevokeRacActivationToken ---- */
TEST(combase_ext_rac_token_gates_and_lifecycle) {
    UINT64 tok = 0x77; HANDLE ev, tk = NULL; HRESULT hr;
    REQUIRE(UN(CoRegisterRacActivationToken)); REQUIRE(UN(CoRevokeRacActivationToken));
    XEQ(UN(CoRegisterRacActivationToken)(NULL, &tok), E_INVALIDARG);
    EXPECT_EQ(tok, 0x77);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    XEQ(UN(CoRegisterRacActivationToken)(ev, &tok), RPC_X_BAD_STUB_DATA_);   /* not a token handle */
    EXPECT_EQ(tok, 0x77);
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tk) || OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tk));
    XEQ(UN(CoRegisterRacActivationToken)(tk, NULL), RPC_X_NULL_REF_POINTER_);
    hr = UN(CoRegisterRacActivationToken)(tk, &tok);
    t_note("CoRegisterRacActivationToken(process token) = %08lx token %llx", (unsigned long)hr, (unsigned long long)tok);
    XEQ(hr, S_OK);
    CHECK(tok != 0 && tok != 0x77);
    XEQ(UN(CoRevokeRacActivationToken)(tok), S_OK);
    XEQ(UN(CoRevokeRacActivationToken)(tok), S_OK);
    XEQ(UN(CoRevokeRacActivationToken)(0), S_OK);
    XEQ(UN(CoRevokeRacActivationToken)(0x1234abcd), S_OK);
    CloseHandle(ev); CloseHandle(tk);
}

/* ---- UpdateProcessTracing / UpdateDCOMSettings ---- */
TEST(combase_ext_update_process_tracing) {
    HRESULT a, b;
    REQUIRE(UN(UpdateProcessTracing)); REQUIRE(UN(UpdateDCOMSettings));
    a = UN(UpdateProcessTracing)(0, 0); b = UN(UpdateProcessTracing)(0, 1);
    t_note("UpdateProcessTracing(0) off=%08lx on=%08lx", (unsigned long)a, (unsigned long)b);
    CHECK(FAILED(a)); CHECK(FAILED(b));            /* the service refuses pid 0 (E_INVALIDARG elevated, E_ACCESSDENIED restricted) */
    a = UN(UpdateProcessTracing)(GetCurrentProcessId(), 0);
    b = UN(UpdateProcessTracing)(GetCurrentProcessId(), 1);
    t_note("UpdateProcessTracing(own pid) off=%08lx on=%08lx", (unsigned long)a, (unsigned long)b);
    CHECK(FAILED(a)); CHECK(FAILED(b));
    XEQ(a, b);                                                          /* the switch value does not change the answer */
    UN(UpdateDCOMSettings)();                                            /* void: returns */
    UN(UpdateDCOMSettings)();
}

/* ---- GetCatalogHelper ---- */
TEST(combase_ext_catalog_helper_iunknown_only) {
    void *p = (void *)1, *q = (void *)1; HRESULT hr; ULONG a, b;
    REQUIRE(UN(GetCatalogHelper)); REQUIRE(U(CoInitializeEx));
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) { t_skip("CoInitializeEx"); return; }
    hr = UN(GetCatalogHelper)(&X_IID_Bogus, &q);
    XEQ(hr, E_NOINTERFACE); CHECK(q == NULL);
    q = (void *)1;
    hr = UN(GetCatalogHelper)(&X_IID_IRpcStubBuffer, &q);
    XEQ(hr, E_NOINTERFACE); CHECK(q == NULL);
    hr = UN(GetCatalogHelper)(&X_IID_IUnknown, &p);
    XEQ(hr, S_OK);
    CHECK(p != NULL && p != (void *)1);
    if (p && p != (void *)1) {
        IUnknown *u = (IUnknown *)p;
        a = u->lpVtbl->AddRef(u); b = u->lpVtbl->Release(u);
        CHECK(a >= 2); EXPECT_EQ(b, a - 1);                             /* the singleton counts its references */
        q = NULL;
        XEQ(u->lpVtbl->QueryInterface(u, &X_IID_IUnknown, &q), S_OK);
        if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
        u->lpVtbl->Release(u);
    }
    U(CoUninitialize)();
}

/* ---- DcomChannelSetHResult ---- */
typedef struct { int mode; HRESULT r[6]; } SetHr;
static DWORD WINAPI set_hr_thread(void *p) {
    SetHr *s = p; ULONG x = 0; void *msg = NULL;
    if (s->mode) U(CoInitializeEx)(NULL, s->mode == 1 ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    s->r[0] = UN(DcomChannelSetHResult)(NULL, NULL, 0);
    s->r[1] = UN(DcomChannelSetHResult)(NULL, NULL, 5);
    s->r[2] = UN(DcomChannelSetHResult)(NULL, &x, 0);
    s->r[3] = UN(DcomChannelSetHResult)(NULL, &x, 5);
    s->r[4] = UN(DcomChannelSetHResult)(&msg, &x, 5);
    s->r[5] = UN(DcomChannelSetHResult)(&msg, NULL, 0);
    if (s->mode) U(CoUninitialize)();
    return 0;
}
static void set_hr_run(int mode, SetHr *s) {
    HANDLE h; s->mode = mode; memset(s->r, 0xEE, sizeof s->r);
    h = CreateThread(NULL, 0, set_hr_thread, s, 0, NULL);
    WaitForSingleObject(h, 20000); CloseHandle(h);
    t_note("DcomChannelSetHResult mode %d: %08lx %08lx %08lx %08lx %08lx %08lx", mode, (unsigned long)s->r[0], (unsigned long)s->r[1],
           (unsigned long)s->r[2], (unsigned long)s->r[3], (unsigned long)s->r[4], (unsigned long)s->r[5]);
}
TEST(combase_ext_channel_set_hresult_without_a_call) {
    SetHr un, sta, mta;
    REQUIRE(UN(DcomChannelSetHResult)); REQUIRE(U(CoInitializeEx));
    set_hr_run(0, &un); set_hr_run(1, &sta); set_hr_run(2, &mta);
    /* r[0]: no message, no reserved, hr 0     r[1]: hr 5 (needs the thread's server-call slot)    r[2], r[3]: reserved pointer
     * r[4], r[5]: a message pointer whose call object is NULL */
    XEQ(un.r[0], S_OK); XEQ(sta.r[0], S_OK); XEQ(mta.r[0], S_OK);
    XEQ(un.r[1], CO_E_NOTINITIALIZED_);                       /* COM never initialised on the thread */
    XEQ(sta.r[1], RPC_E_CALL_COMPLETE_);                     /* initialised, but no server call in progress */
    XEQ(mta.r[1], RPC_E_CALL_COMPLETE_);
    XEQ(un.r[2], E_INVALIDARG); XEQ(sta.r[2], E_INVALIDARG); XEQ(mta.r[2], E_INVALIDARG);   /* a reserved pointer is refused first */
    XEQ(un.r[3], E_INVALIDARG); XEQ(sta.r[3], E_INVALIDARG); XEQ(mta.r[3], E_INVALIDARG);
    XEQ(un.r[4], S_OK); XEQ(sta.r[4], S_OK); XEQ(mta.r[4], S_OK);
    XEQ(un.r[5], S_OK); XEQ(sta.r[5], S_OK); XEQ(mta.r[5], S_OK);
}
/* ---- CStdStubBuffer_Invoke: method range gate ---- */
static jmp_buf x_jb; static volatile LONG x_armed; static volatile DWORD x_got, x_tid;
static LONG CALLBACK x_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!x_armed || GetCurrentThreadId() != x_tid) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000A || c == 0x406D1388 || c == 0x80000003) return EXCEPTION_CONTINUE_SEARCH;
    x_got = c; x_armed = 0; longjmp(x_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
typedef HRESULT (WINAPI *invoke_fn)(void *, void *, void *);
TEST(combase_ext_std_stub_invoke_method_range) {
    invoke_fn f = (invoke_fn)t_proc_in("combase.dll", "CStdStubBuffer_Invoke");
    static const ULONG bad[] = { 0, 1, 2, 4, 5, 0x8003, 0xffffffffu };
    /* a stub vtable preceded by its CInterfaceStubHeader: piid, pServerInfo, DispatchTableCount, pDispatchTable */
    struct { const GUID *piid; void *info; ULONG count; ULONG pad; void *table; void *vtbl[10]; } hdr;
    struct { void *vtbl; LONG refs; void *server; } stub;
    unsigned i;
    if (!f) { t_skip("missing export: CStdStubBuffer_Invoke"); return; }
    if (t_is_wine()) { t_skip("wine dispatches instead of raising"); return; }
    memset(&hdr, 0, sizeof hdr); hdr.piid = &X_IID_IUnknown; hdr.count = 4;
    memset(&stub, 0, sizeof stub); stub.vtbl = hdr.vtbl; stub.refs = 1;
    AddVectoredExceptionHandler(1, x_veh);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        RPCOLEMESSAGE msg; DWORD got = 0; int caught;
        memset(&msg, 0, sizeof msg); msg.iMethod = bad[i]; msg.dataRepresentation = 0x10;
        x_got = 0; x_tid = GetCurrentThreadId();
        caught = 0;
        if (setjmp(x_jb) == 0) { x_armed = 1; f(&stub, &msg, NULL); x_armed = 0; } else caught = 1;
        got = x_got;
        CHECKF(caught && got == 0x6d1, "iMethod %lu: caught=%d code=0x%lx", bad[i], caught, got);   /* RPC_S_PROCNUM_OUT_OF_RANGE */
    }
}
