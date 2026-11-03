/* APIs that only mean something inside a server call: an in-process ncalrpc server whose RPC_DISPATCH_FUNCTIONs
 * run the probes on the runtime's call thread and report back raw DWORDs. The client side uses the low-level
 * I_RpcGetBuffer / I_RpcSendReceive / I_RpcFreeBuffer path (no NDR stubs). */
#include "epx.h"

static const RPC_SYNTAX_IDENTIFIER ndr20 = { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} };

enum { P_PROBE, P_RAISE, P_SSALLOC, P_YIELD, P_COPY, P_COUNT };

/* result slots of P_PROBE */
enum {
    R_HANDLE_NONNULL, R_HANDLE_EQ_MSG, R_LOCAL_ST_NULL, R_LOCAL_FLAG_NULL, R_LOCAL_ST_H, R_LOCAL_FLAG_H,
    R_PID_ST_NULL, R_PID_NULL, R_PID_ST_H, R_PID_H, R_CANCEL_ST_NULL, R_CANCEL_ST_H,
    R_AUTHCLIENT_ST, R_AUTHZ_ST, R_AUTHZ_FREE_ST, R_SETCANCEL_ST, R_SUB_BADNOTIF, R_SUB_BADTYPE,
    R_SERVFROMCLIENT_ST, R_SERVFROMCLIENT_OUT_NULL, R_CLIENTFN_ON_CALLHANDLE_ST, R_CLIENTFN2_ST, R_MAXCALLS_ST, R_FREE_CALLHANDLE_ST,
    R_COMTIMEOUT_ST, R_COPY_ST, R_COUNT
};

UDECL(RPC_STATUS, RpcBindingInqMaxCalls, (RPC_BINDING_HANDLE, unsigned *));

static void reply(RPC_MESSAGE *m, const void *p, ULONG n) {
    m->BufferLength = n;
    if (U(I_RpcGetBuffer)(m) != RPC_S_OK) { ((void (WINAPI *)(RPC_STATUS))U(RpcRaiseException))(RPC_S_OUT_OF_MEMORY); }
    if (n) memcpy(m->Buffer, p, n);
}

static void RPC_ENTRY h_probe(RPC_MESSAGE *m) {
    DWORD r[R_COUNT]; unsigned loc = 0xAA; ULONG pid = 0xAA; LUID l0; PVOID actx = NULL; RPC_ASYNC_NOTIFICATION_INFO info;
    RPC_AUTHZ_HANDLE pr; ULONG a, b, c; RPC_BINDING_HANDLE sb = (RPC_BINDING_HANDLE)(ULONG_PTR)1; RPC_BINDING_HANDLE ch;
    unsigned mc = 0; unsigned ct = 0; RPC_BINDING_HANDLE cp = NULL;
    memset(r, 0xFF, sizeof r); memset(&l0, 0, sizeof l0); memset(&info, 0, sizeof info);
    ch = U(I_RpcGetCurrentCallHandle)();
    r[R_HANDLE_NONNULL] = ch != NULL;
    r[R_HANDLE_EQ_MSG] = ch == m->Handle;
    r[R_LOCAL_ST_NULL] = U(I_RpcBindingIsClientLocal)(NULL, &loc); r[R_LOCAL_FLAG_NULL] = loc; loc = 0xAA;
    r[R_LOCAL_ST_H] = U(I_RpcBindingIsClientLocal)(m->Handle, &loc); r[R_LOCAL_FLAG_H] = loc;
    r[R_PID_ST_NULL] = U(I_RpcBindingInqLocalClientPID)(NULL, &pid); r[R_PID_NULL] = pid; pid = 0xAA;
    r[R_PID_ST_H] = U(I_RpcBindingInqLocalClientPID)(m->Handle, &pid); r[R_PID_H] = pid;
    r[R_CANCEL_ST_NULL] = U(RpcServerTestCancel)(NULL);
    r[R_CANCEL_ST_H] = U(RpcServerTestCancel)(m->Handle);
    r[R_AUTHCLIENT_ST] = U(RpcBindingInqAuthClientW)(NULL, &pr, NULL, &a, &b, &c);
    r[R_AUTHZ_ST] = U(RpcGetAuthorizationContextForClient)(NULL, FALSE, NULL, NULL, l0, 0, NULL, &actx);
    r[R_AUTHZ_FREE_ST] = (r[R_AUTHZ_ST] == 0 && actx) ? U(RpcFreeAuthorizationContext)(&actx) : 0xFFFF;
    r[R_SETCANCEL_ST] = U(RpcMgmtSetCancelTimeout)(RPC_C_CANCEL_INFINITE_TIMEOUT);
    r[R_SUB_BADNOTIF] = U(RpcServerSubscribeForNotification)(NULL, 0, RpcNotificationTypeEvent, &info);
    r[R_SUB_BADTYPE] = U(RpcServerSubscribeForNotification)(NULL, 1, RpcNotificationTypeHwnd, &info);
    r[R_SERVFROMCLIENT_ST] = U(RpcBindingServerFromClient)(NULL, &sb);
    r[R_SERVFROMCLIENT_OUT_NULL] = sb == NULL;
    if (r[R_SERVFROMCLIENT_ST] == 0 && sb) U(RpcBindingFree)(&sb);
    r[R_CLIENTFN_ON_CALLHANDLE_ST] = U(RpcBindingInqAuthInfoW)(m->Handle, NULL, &a, &b, NULL, &c);
    r[R_CLIENTFN2_ST] = U(RpcMgmtInqComTimeout)(m->Handle, &ct);
    r[R_MAXCALLS_ST] = UN(RpcBindingInqMaxCalls) ? UN(RpcBindingInqMaxCalls)(m->Handle, &mc) : 0xFFFE;
    r[R_COMTIMEOUT_ST] = mc;
    cp = NULL;
    r[R_COPY_ST] = U(I_RpcBindingCopy)(m->Handle, &cp);
    if (r[R_COPY_ST] == 0 && cp) U(RpcBindingFree)(&cp);
    reply(m, r, sizeof r);
}
static void RPC_ENTRY h_raise(RPC_MESSAGE *m) { int code = *(int *)m->Buffer; ((void (WINAPI *)(RPC_STATUS))U(RpcRaiseException))((RPC_STATUS)code); }
static void RPC_ENTRY h_ssalloc(RPC_MESSAGE *m) {
    DWORD r[4]; void *p;
    memset(r, 0, sizeof r);
    r[0] = (DWORD)(ULONG_PTR)U(RpcSsGetThreadHandle)() != 0;        /* before: pooled thread, normally no context */
    U(RpcSsEnableAllocate)();
    p = U(RpcSsAllocate)(128);
    if (p) memset(p, 0x5A, 128);
    r[1] = p != NULL;
    r[2] = U(RpcSsGetThreadHandle)() != NULL;
    U(RpcSsDisableAllocate)();
    r[3] = U(RpcSsGetThreadHandle)() == NULL;
    reply(m, r, sizeof r);
}
static void RPC_ENTRY h_yield(RPC_MESSAGE *m) { DWORD one = 1; U(RpcServerYield)(); reply(m, &one, 4); }
static void RPC_ENTRY h_copy(RPC_MESSAGE *m) { DWORD one = 1; reply(m, &one, 4); }

static RPC_DISPATCH_FUNCTION g_fn[P_COUNT];
static RPC_DISPATCH_TABLE g_dt;
static RPC_SERVER_INTERFACE g_si;
static RPC_CLIENT_INTERFACE g_ci;
static const UUID g_uuid = { 0x7a3c1100, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x77} };

static void iface_init(void) {
    memset(&g_si, 0, sizeof g_si); memset(&g_ci, 0, sizeof g_ci);
    g_fn[P_PROBE] = h_probe; g_fn[P_RAISE] = h_raise; g_fn[P_SSALLOC] = h_ssalloc; g_fn[P_YIELD] = h_yield; g_fn[P_COPY] = h_copy;
    g_dt.DispatchTableCount = P_COUNT; g_dt.DispatchTable = g_fn; g_dt.Reserved = 0;
    g_si.Length = sizeof g_si;
    g_si.InterfaceId.SyntaxGUID = g_uuid; g_si.InterfaceId.SyntaxVersion.MajorVersion = 1;
    g_si.TransferSyntax = ndr20; g_si.DispatchTable = &g_dt;
    g_ci.Length = sizeof g_ci; g_ci.InterfaceId = g_si.InterfaceId; g_ci.TransferSyntax = ndr20;
}

static DWORD WINAPI wait_thread(void *p) { return U(RpcMgmtWaitServerListen)(); }
static WCHAR g_ep[64];
static RPC_STATUS srv_up(void) {
    RPC_STATUS st;
    iface_init();
    epx_unique_ep(g_ep, 64);
    st = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)g_ep, NULL);
    if (st != RPC_S_OK) return st;
    st = U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&g_si, NULL, NULL, 0, 10, NULL);
    if (st != RPC_S_OK) return st;
    return U(RpcServerListen)(1, 10, TRUE);
}
static void srv_down(void) {
    HANDLE th;
    U(RpcMgmtStopServerListening)(NULL);
    th = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
    if (th) { if (WaitForSingleObject(th, 10000) != WAIT_OBJECT_0) TerminateThread(th, 1); CloseHandle(th); }
    U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_si, NULL, 1);
}
static RPC_BINDING_HANDLE cli_bind(void) {
    RPC_WSTR sb = NULL; RPC_BINDING_HANDLE h = NULL;
    if (U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)g_ep, NULL, &sb) != RPC_S_OK) return NULL;
    if (U(RpcBindingFromStringBindingW)(sb, &h) != RPC_S_OK) h = NULL;
    U(RpcStringFreeW)(&sb);
    return h;
}
static RPC_STATUS call(RPC_BINDING_HANDLE h, unsigned proc, const void *in, ULONG inlen, void *out, ULONG outcap, ULONG *outlen) {
    RPC_MESSAGE m; RPC_STATUS st;
    memset(&m, 0, sizeof m);
    m.Handle = h; m.DataRepresentation = 0x10; m.ProcNum = proc; m.BufferLength = inlen;
    m.RpcInterfaceInformation = &g_ci; m.TransferSyntax = &g_ci.TransferSyntax;
    if (outlen) *outlen = 0;
    st = U(I_RpcGetBuffer)(&m);
    if (st != RPC_S_OK) return st;
    if (inlen) memcpy(m.Buffer, in, inlen);
    st = U(I_RpcSendReceive)(&m);
    if (st != RPC_S_OK) return st;
    if (outlen) *outlen = m.BufferLength;
    if (out && outcap) memcpy(out, m.Buffer, m.BufferLength < outcap ? m.BufferLength : outcap);
    U(I_RpcFreeBuffer)(&m);
    return RPC_S_OK;
}

#define SERVER_TEST_SETUP(var) \
    RPC_BINDING_HANDLE var; \
    REQUIRE(U(I_RpcGetBuffer) && U(I_RpcSendReceive) && U(I_RpcFreeBuffer) && U(RpcServerListen) && U(RpcServerRegisterIfEx)); \
    WINE_STUB(); \
    if (srv_up() != RPC_S_OK) { srv_down(); t_skip("cannot start ncalrpc server"); return; } \
    var = cli_bind(); \
    if (!var) { srv_down(); t_skip("cannot bind"); return; }
#define SERVER_TEST_TEARDOWN(var) do { U(RpcBindingFree)(&var); srv_down(); } while (0)

TEST(epm_server_probe_in_call_handles_and_pid) {
    DWORD r[R_COUNT]; ULONG n = 0; RPC_STATUS st;
    SERVER_TEST_SETUP(h);
    st = call(h, P_PROBE, NULL, 0, r, sizeof r, &n);
    EQ(st, RPC_S_OK);
    EQ(n, sizeof r);
    if (st == RPC_S_OK && n == sizeof r) {
        EQ(r[R_HANDLE_NONNULL], 1);
        t_note("I_RpcGetCurrentCallHandle == RPC_MESSAGE.Handle: %lu", (unsigned long)r[R_HANDLE_EQ_MSG]);
        EQ(r[R_LOCAL_ST_NULL], RPC_S_OK); EQ(r[R_LOCAL_FLAG_NULL], 1);       /* ncalrpc clients are always local */
        EQ(r[R_LOCAL_ST_H], RPC_S_OK);    EQ(r[R_LOCAL_FLAG_H], 1);
        EQ(r[R_PID_ST_NULL], RPC_S_OK);   EQ(r[R_PID_NULL], GetCurrentProcessId());
        EQ(r[R_PID_ST_H], RPC_S_OK);      EQ(r[R_PID_H], GetCurrentProcessId());
    }
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_probe_in_call_cancel_and_timeout) {
    DWORD r[R_COUNT]; ULONG n = 0;
    SERVER_TEST_SETUP(h);
    if (call(h, P_PROBE, NULL, 0, r, sizeof r, &n) == RPC_S_OK && n == sizeof r) {
        EQ(r[R_CANCEL_ST_NULL], RPC_S_OK);       /* not cancelled: RpcServerTestCancel returns 0 */
        EQ(r[R_CANCEL_ST_H], RPC_S_OK);
        EQ(r[R_SETCANCEL_ST], RPC_S_OK);
    } else CHECK(0);
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_probe_in_call_notification_validation) {
    DWORD r[R_COUNT]; ULONG n = 0;
    SERVER_TEST_SETUP(h);
    if (call(h, P_PROBE, NULL, 0, r, sizeof r, &n) == RPC_S_OK && n == sizeof r) {
        EQ(r[R_SUB_BADNOTIF], EPX_CANNOT_SUPPORT);      /* notification 0 is not a valid event */
        EQ(r[R_SUB_BADTYPE], RPC_S_INVALID_ARG);        /* HWND notifications are not supported */
    } else CHECK(0);
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_probe_in_call_binding_kinds) {
    DWORD r[R_COUNT]; ULONG n = 0;
    SERVER_TEST_SETUP(h);
    if (call(h, P_PROBE, NULL, 0, r, sizeof r, &n) == RPC_S_OK && n == sizeof r) {
        t_note("RpcBindingServerFromClient(NULL) in call = %lu (out %s)", (unsigned long)r[R_SERVFROMCLIENT_ST], r[R_SERVFROMCLIENT_OUT_NULL] ? "NULL" : "set");
        /* client-side inquiry functions refuse a server call handle */
        EQ(r[R_CLIENTFN_ON_CALLHANDLE_ST], EPX_INVALID_BINDING);
        EQ(r[R_CLIENTFN2_ST], EPX_INVALID_BINDING);
        t_note("I_RpcBindingCopy(call handle) = %lu, RpcBindingInqMaxCalls max=%lu", (unsigned long)r[R_COPY_ST], (unsigned long)r[R_COMTIMEOUT_ST]);
        t_note("RpcBindingInqAuthClientW(NULL) over unsecured ncalrpc = %lu", (unsigned long)r[R_AUTHCLIENT_ST]);
        t_note("RpcGetAuthorizationContextForClient(NULL) over unsecured ncalrpc = %lu, free = %lu", (unsigned long)r[R_AUTHZ_ST], (unsigned long)r[R_AUTHZ_FREE_ST]);
    } else CHECK(0);
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_ss_allocate_in_dispatch_thread) {
    DWORD r[4]; ULONG n = 0;
    SERVER_TEST_SETUP(h);
    EQ(call(h, P_SSALLOC, NULL, 0, r, sizeof r, &n), RPC_S_OK);
    EQ(n, sizeof r);
    t_note("call thread already had an allocation context: %lu", (unsigned long)r[0]);
    EQ(r[1], 1); EQ(r[2], 1); EQ(r[3], 1);
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_yield_in_call_returns) {
    DWORD one = 0; ULONG n = 0;
    SERVER_TEST_SETUP(h);
    EQ(call(h, P_YIELD, NULL, 0, &one, 4, &n), RPC_S_OK);
    EQ(one, 1);
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_raise_in_call_code_reaches_client) {
    static const int codes[] = { 1700, 0x6D8, 14, 0x1234 };
    unsigned i;
    SERVER_TEST_SETUP(h);
    for (i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        RPC_STATUS st = call(h, P_RAISE, &codes[i], 4, NULL, 0, NULL);
        t_note("server raised %d -> client call status %lu", codes[i], (unsigned long)st);
        CHECK(st != RPC_S_OK);
    }
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_get_free_buffer_without_send) {
    RPC_MESSAGE m; RPC_STATUS st;
    SERVER_TEST_SETUP(h);
    memset(&m, 0, sizeof m);
    m.Handle = h; m.DataRepresentation = 0x10; m.ProcNum = P_COPY; m.BufferLength = 64;
    m.RpcInterfaceInformation = &g_ci; m.TransferSyntax = &g_ci.TransferSyntax;
    st = U(I_RpcGetBuffer)(&m);
    EQ(st, RPC_S_OK);
    CHECK(m.Buffer != NULL);
    if (st == RPC_S_OK && m.Buffer) {
        memset(m.Buffer, 0xA5, 64);          /* the whole requested length must be writable */
        EQ(U(I_RpcFreeBuffer)(&m), RPC_S_OK);
    }
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_get_buffer_zero_and_large_lengths) {
    static const ULONG lens[] = { 0, 1, 4096, 1 << 16, 1 << 20 };
    unsigned i;
    SERVER_TEST_SETUP(h);
    for (i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        RPC_MESSAGE m; RPC_STATUS st;
        memset(&m, 0, sizeof m);
        m.Handle = h; m.DataRepresentation = 0x10; m.ProcNum = P_COPY; m.BufferLength = lens[i];
        m.RpcInterfaceInformation = &g_ci; m.TransferSyntax = &g_ci.TransferSyntax;
        st = U(I_RpcGetBuffer)(&m);
        CHECKF(st == RPC_S_OK, "I_RpcGetBuffer(len %lu) = %lu", (unsigned long)lens[i], (unsigned long)st);
        if (st == RPC_S_OK) {
            if (lens[i]) { ((unsigned char *)m.Buffer)[0] = 1; ((unsigned char *)m.Buffer)[lens[i] - 1] = 2; }
            U(I_RpcFreeBuffer)(&m);
        }
    }
    SERVER_TEST_TEARDOWN(h);
}

TEST(epm_server_calls_after_server_down_fail_unavailable) {
    DWORD one = 0; ULONG n = 0; RPC_STATUS st;
    SERVER_TEST_SETUP(h);
    EQ(call(h, P_COPY, NULL, 0, &one, 4, &n), RPC_S_OK);
    srv_down();
    st = call(h, P_COPY, NULL, 0, &one, 4, &n);
    t_note("call after server shutdown = %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    U(RpcBindingFree)(&h);
}
