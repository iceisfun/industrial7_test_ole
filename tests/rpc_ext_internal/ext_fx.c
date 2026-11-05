#include "ext.h"

void ext_warm(void) {
    RPC_WSTR sb = NULL;
    if (U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)L"olet_ext_warm", NULL, &sb) == RPC_S_OK && sb) U(RpcStringFreeW)(&sb);
}

static volatile LONG g_epn;
void ext_ep(WCHAR *buf) {
    wsprintfW(buf, L"olet_ext_%lu_%ld", GetCurrentProcessId(), InterlockedIncrement(&g_epn));
}

static const RPC_SYNTAX_IDENTIFIER ndr_syntax = { { 0x8A885D04, 0x1CEB, 0x11C9, { 0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60 } }, { 2, 0 } };

static void reply(RPC_MESSAGE *m, const void *p, ULONG n) {
    m->BufferLength = n;
    if (U(I_RpcGetBuffer)(m) != RPC_S_OK) U(RpcRaiseException)(RPC_S_OUT_OF_MEMORY);
    if (n) memcpy(m->Buffer, p, n);
}

UDECL(RPC_STATUS, I_RpcServerInqLocalConnAddress, (RPC_BINDING_HANDLE, void *, ULONG *, ULONG *));
UDECL(RPC_STATUS, I_RpcServerInqRemoteConnAddress, (RPC_BINDING_HANDLE, void *, ULONG *, ULONG *));
UDECL(RPC_STATUS, I_RpcServerGetAssociationID, (RPC_BINDING_HANDLE, ULONG *));
UDECL(RPC_STATUS, I_RpcServerCheckClientRestriction, (RPC_BINDING_HANDLE));
UDECL(RPC_STATUS, I_RpcBindingToStaticStringBindingW, (RPC_BINDING_HANDLE, RPC_WSTR *));
UDECL(RPC_STATUS, I_RpcBindingInqSecurityContext, (RPC_BINDING_HANDLE, void **));
UDECL(RPC_STATUS, I_RpcBindingInqSecurityContextKeyInfo, (RPC_BINDING_HANDLE, void *));
UDECL(void, I_RpcSessionStrictContextHandle, (void));
UDECL(RPC_STATUS, I_RpcGetExtendedError, (void));

static void RPC_ENTRY p_nop(RPC_MESSAGE *m) { reply(m, NULL, 0); }
static void RPC_ENTRY p_connaddr(RPC_MESSAGE *m) {
    DWORD r[12]; unsigned char buf[64]; ULONG sz, fmt; int i;
    memset(r, 0xee, sizeof r);
    for (i = 0; i < 4; i++) {
        RPC_BINDING_HANDLE h = (i & 1) ? m->Handle : NULL;
        memset(buf, 0, sizeof buf); sz = sizeof buf; fmt = 0xdeadbeef;
        r[i * 3] = (i < 2) ? UN(I_RpcServerInqLocalConnAddress)(h, buf, &sz, &fmt) : UN(I_RpcServerInqRemoteConnAddress)(h, buf, &sz, &fmt);
        r[i * 3 + 1] = sz; r[i * 3 + 2] = fmt;
    }
    reply(m, r, sizeof r);
}
static void RPC_ENTRY p_assoc(RPC_MESSAGE *m) {
    DWORD r[6]; ULONG a = 0xdeadbeef, b = 0xdeadbeef;
    r[0] = UN(I_RpcServerGetAssociationID)(NULL, &a); r[1] = a;
    r[2] = UN(I_RpcServerGetAssociationID)(m->Handle, &b); r[3] = b;
    r[4] = UN(I_RpcServerCheckClientRestriction)(m->Handle);
    r[5] = UN(I_RpcServerCheckClientRestriction)(NULL);
    reply(m, r, sizeof r);
}
static void RPC_ENTRY p_static(RPC_MESSAGE *m) {
    struct { DWORD st, st2; WCHAR s[200]; } r; RPC_WSTR str = NULL;
    memset(&r, 0, sizeof r);
    r.st = UN(I_RpcBindingToStaticStringBindingW)(m->Handle, &str);
    if (r.st == RPC_S_OK && str) { lstrcpynW(r.s, (WCHAR *)str, 199); U(RpcStringFreeW)(&str); }
    str = NULL;
    r.st2 = UN(I_RpcBindingToStaticStringBindingW)(NULL, &str);
    reply(m, &r, sizeof r);
}
static void RPC_ENTRY p_secctx(RPC_MESSAGE *m) {
    DWORD r[8]; void *c = (void *)(ULONG_PTR)1; BYTE key[64];
    r[0] = UN(I_RpcBindingInqSecurityContext)(m->Handle, &c); r[1] = c == NULL;
    c = (void *)(ULONG_PTR)1;
    r[2] = UN(I_RpcBindingInqSecurityContext)(NULL, &c); r[3] = c == NULL;
    memset(key, 0, sizeof key);
    r[4] = UN(I_RpcBindingInqSecurityContextKeyInfo)(m->Handle, key);
    r[5] = UN(I_RpcBindingInqSecurityContextKeyInfo)(NULL, key);
    r[6] = r[7] = 0;
    reply(m, r, sizeof r);
}
static void RPC_ENTRY p_strict(RPC_MESSAGE *m) { UN(I_RpcSessionStrictContextHandle)(); reply(m, NULL, 0); }
UDECL(RPC_STATUS, I_RpcBindingInqClientTokenAttributes, (RPC_BINDING_HANDLE, LUID *, ULONG *, ULONG *));
UDECL(RPC_STATUS, I_RpcBindingHandleToAsyncHandle, (RPC_BINDING_HANDLE, void **));
static void RPC_ENTRY p_tokattr(RPC_MESSAGE *m) {
    DWORD r[12]; LUID l; ULONG a, f; void *c = (void *)(ULONG_PTR)1;
    memset(r, 0, sizeof r);
    l.LowPart = 0xdead; l.HighPart = 0xbeef; a = 0xdead; f = 0xdead;
    r[0] = UN(I_RpcBindingInqClientTokenAttributes)(NULL, &l, &a, &f); r[1] = l.LowPart; r[2] = l.HighPart; r[3] = a; r[4] = f;
    r[5] = UN(I_RpcBindingInqClientTokenAttributes)(m->Handle, NULL, NULL, NULL);
    r[6] = UN(I_RpcBindingHandleToAsyncHandle)(m->Handle, &c); r[7] = c == (void *)(ULONG_PTR)1;
    reply(m, r, sizeof r);
}
static void RPC_ENTRY p_exterr(RPC_MESSAGE *m) { DWORD r = UN(I_RpcGetExtendedError)(); reply(m, &r, 4); }

static volatile LONG g_uuidn;
static void ext_init(ext_srv *s) {
    memset(s, 0, sizeof *s);
    s->uuid.Data1 = 0x7a3c1000u + (GetCurrentProcessId() % 1000) * 16 + (unsigned)InterlockedIncrement(&g_uuidn); s->uuid.Data2 = 0x4c1d; s->uuid.Data3 = 0x4e7b; s->uuid.Data4[0] = 0x80; s->uuid.Data4[7] = 0x77;
    s->fn[0] = p_nop; s->fn[1] = p_connaddr; s->fn[2] = p_assoc; s->fn[3] = p_static; s->fn[4] = p_secctx; s->fn[5] = p_strict; s->fn[6] = p_exterr; s->fn[7] = p_tokattr;
    s->dt.DispatchTableCount = 8; s->dt.DispatchTable = s->fn;
    s->si.Length = sizeof s->si; s->si.InterfaceId.SyntaxGUID = s->uuid; s->si.InterfaceId.SyntaxVersion.MajorVersion = 1;
    s->si.TransferSyntax = ndr_syntax; s->si.DispatchTable = &s->dt;
    s->ci.Length = sizeof s->ci; s->ci.InterfaceId = s->si.InterfaceId; s->ci.TransferSyntax = ndr_syntax;
}

static DWORD WINAPI wait_thread(void *p) { (void)p; return U(RpcMgmtWaitServerListen)(); }
static void stop_listening(void) {
    if (U(RpcMgmtIsServerListening)(NULL) == RPC_S_OK) {
        HANDLE th;
        U(RpcMgmtStopServerListening)(NULL);
        th = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
        if (th) { WaitForSingleObject(th, 10000); CloseHandle(th); }
    }
}
UDECL(RPC_STATUS, I_RpcServerUseProtseqEp2W, (RPC_WSTR, RPC_WSTR, unsigned, RPC_WSTR, void *, void *));
static int up(ext_srv *s, int ep2) {
    RPC_WSTR sb = NULL;
    ext_init(s); ext_ep(s->ep);
    stop_listening();
    if (ep2) {
        struct { ULONG len, a, b; } pol = { 12, 0, 0 };
        if (UN(I_RpcServerUseProtseqEp2W)(NULL, (RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)s->ep, NULL, &pol) != RPC_S_OK) return 0;
    } else if (U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)s->ep, NULL) != RPC_S_OK) return 0;
    if (U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&s->si, NULL, NULL, 0, RPC_C_LISTEN_MAX_CALLS_DEFAULT, NULL) != RPC_S_OK) return 0;
    if (U(RpcServerListen)(1, 20, TRUE) != RPC_S_OK) return 0;
    if (U(RpcStringBindingComposeW)(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)s->ep, NULL, &sb) != RPC_S_OK) return 0;
    if (U(RpcBindingFromStringBindingW)(sb, &s->h) != RPC_S_OK) s->h = NULL;
    U(RpcStringFreeW)(&sb);
    return s->h != NULL;
}
int ext_up(ext_srv *s) { return up(s, 0); }
int ext_up_ep2(ext_srv *s) { return up(s, 1); }
void ext_down(ext_srv *s) {
    if (s->h) U(RpcBindingFree)(&s->h);
    stop_listening();
    U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&s->si, NULL, 1);
}
RPC_STATUS ext_call(ext_srv *s, RPC_BINDING_HANDLE h, unsigned proc, const void *in, ULONG inlen, void *out, ULONG outcap, ULONG *outlen) {
    RPC_MESSAGE m; RPC_STATUS st;
    memset(&m, 0, sizeof m);
    m.Handle = h ? h : s->h; m.DataRepresentation = 0x10; m.ProcNum = proc; m.BufferLength = inlen;
    m.RpcInterfaceInformation = &s->ci; m.TransferSyntax = (PRPC_SYNTAX_IDENTIFIER)&s->ci.TransferSyntax;
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
