/* Shared fixtures for the rpcrt4 ncalrpc client/server tests.
 *
 * Nothing here links rpcrt4: all runtime calls are made through U()/UN() pointers.
 *
 * Two kinds of interfaces are built by hand:
 *   - the "xif" NDR interface (uuid 6f0e7a10-0001-4c3d-9a00-0000000000a1 v1.0), a hand-assembled
 *     MIDL_STUB_DESC / MIDL_SERVER_INFO / Oif format-string set with these procedures:
 *       0 void p_void(void)                               8 void p_ctx_open([out] CTX *h, int v)
 *       1 int p_add(int a, int b)                         9 int  p_ctx_get(CTX h)
 *       2 int p_wlen([in,string] wchar_t *s)             10 void p_ctx_close([in,out] CTX *h)
 *       3 int p_sum(int n, [size_is(n)] int *a)          11 void p_info([out] pid, level, islocal, authn)
 *       4 void p_seq(int n, [out,size_is(n)] int *a)     12 void p_echo([in,string] s, int n, [out,size_is(n)] o)
 *       5 void p_bump([in,out] S *s)  S={int a;short b;double c}
 *       6 void p_raise(int code)  (RpcRaiseException)    13 void p_stop(void)
 *       7 void p_sleep(int ms)                           14 int  p_counter(void)
 *                                                        15 int  p_ctx_peek(CTX h, [out] int *rundowns)
 *   - "raw" interfaces: RPC_DISPATCH_FUNCTION tables driven with I_RpcGetBuffer/I_RpcSendReceive
 *     on the client, with an arbitrary uuid/version so version matching can be exercised.
 *
 * Raw interface procedures (request/reply are plain byte buffers):
 *   0 echo         reply = request
 *   1 raise        request int32 code -> RpcRaiseException(code)
 *   2 sleep        request int32 ms
 *   3 whoami       reply 5 x DWORD: pid, authnLevel, isLocal, authnService, opnum
 *   4 nop          empty reply
 *   5 inqobject    reply 16 bytes: RpcBindingInqObject on the call handle (+ DWORD status)
 *   6 stop         RpcMgmtStopServerListening(NULL), reply DWORD status
 *   7 waitcancel   polls RpcServerTestCancel up to 5 s, reply DWORD status of last poll
 *   8 inqmsg       reply DWORD: msg->DataRepresentation, BufferLength, ProcNum, RpcFlags
 *   9 impersonate  RpcImpersonateClient(NULL) / OpenThreadToken / RpcRevertToSelf; reply 3 x DWORD
 *  10 authclient   RpcBindingInqAuthClientW(NULL...) reply: status, level, authn, authz
 *  11 attrs        request {version, flags}; RpcServerInqCallAttributesW; reply {status, level, svc, namelen, WCHAR name[256]}
 *  12 srvfromcli   RpcBindingServerFromClient(NULL); reply {status, WCHAR stringbinding[200]}
 * (3 whoami reply: pid, level, islocal, authnsvc, opnum, status, protseq, callstatus, calltype, nullsession, kernelmode)
 */
#ifndef RPCX_H
#define RPCX_H
#include "t.h"
#include <rpc.h>
#include <rpcdcep.h>
#include <rpcndr.h>
#include <rpcasync.h>

#ifndef RPC_C_OPT_CALL_TIMEOUT
#define RPC_C_OPT_CALL_TIMEOUT 12
#endif

/* functions missing from the mingw headers */
UDECL(RPC_STATUS, RpcServerRegisterIf3, (RPC_IF_HANDLE, UUID *, RPC_MGR_EPV *, unsigned int, unsigned int, unsigned int, RPC_IF_CALLBACK_FN *, void *));

/* ---- unique naming ---- */
void rx_ep(WCHAR *buf, size_t cch);            /* L"olet_<pid>_<n>" fresh endpoint name */
void rx_ep_unique(WCHAR *buf, size_t cch);     /* fresh, never recycled */
void rx_epa(char *buf, size_t cb);             /* unique, narrow */

/* ---- NDR "xif" interface ---- */
extern RPC_IF_HANDLE rx_xif_s_ifspec;          /* &RPC_SERVER_INTERFACE */
extern RPC_IF_HANDLE rx_xif_c_ifspec;
extern const RPC_SYNTAX_IDENTIFIER rx_ndr_syntax;
extern volatile LONG rx_calls;                 /* p_void/p_add/... counter (server side) */
extern volatile LONG rx_rundowns;              /* context rundown invocations */
extern volatile LONG rx_ctx_closes;
extern volatile LONG rx_sleep_started, rx_sleep_done;   /* p_sleep progress */
extern handle_t rx_ih;                         /* implicit handle used by the client stubs */
extern const UUID rx_xif_uuid;

enum { RX_VOID, RX_ADD, RX_WLEN, RX_SUM, RX_SEQ, RX_BUMP, RX_RAISE, RX_SLEEP, RX_CTX_OPEN, RX_CTX_GET,
       RX_CTX_CLOSE, RX_INFO, RX_ECHO, RX_STOP, RX_COUNTER, RX_CTX_PEEK, RX_NPROCS };

/* Run an NDR client call (NdrClientCall2) on a helper thread; returns 0 or the exception code that the
 * runtime raised to the caller (RX_TIMEOUT if it did not finish). *ret gets CLIENT_CALL_RETURN.Simple. */
#define RX_TIMEOUT 0xFFFFFFF0u
DWORD rx_ndr(unsigned proc, ULONG_PTR a0, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, LONG_PTR *ret);
/* Same with a different implicit handle for the duration of the call. */
DWORD rx_ndr_h(RPC_BINDING_HANDLE h, unsigned proc, ULONG_PTR a0, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, LONG_PTR *ret);

typedef struct { int a; short b; double c; } rx_S;

/* ---- raw interfaces ---- */
typedef struct rx_raw {
    RPC_SERVER_INTERFACE si;
    RPC_CLIENT_INTERFACE ci;
    RPC_DISPATCH_TABLE dt;
    RPC_DISPATCH_FUNCTION fn[16];
    UUID uuid;
} rx_raw;
void rx_raw_init(rx_raw *r, const UUID *id, unsigned short major, unsigned short minor);
void rx_raw_uuid(UUID *u, unsigned n);                 /* deterministic test uuid #n */
RPC_STATUS rx_raw_call(RPC_BINDING_HANDLE h, const rx_raw *r, unsigned proc, const void *in, ULONG inlen,
                       void *out, ULONG outcap, ULONG *outlen);
RPC_STATUS rx_raw_call_obj(RPC_BINDING_HANDLE h, const rx_raw *r, unsigned proc, const void *in, ULONG inlen,
                           void *out, ULONG outcap, ULONG *outlen, ULONG rpcflags);
extern volatile LONG rx_raw_calls;
extern volatile LONG rx_cb_calls;                      /* security callback invocations */
extern volatile LONG rx_cb_result;                     /* status the callback returns */
extern volatile void *rx_cb_last_if, *rx_cb_last_ctx;
extern volatile LONG rx_cb_attr_st, rx_cb_attr_pid, rx_cb_attr_level, rx_cb_attr_local;   /* InqCallAttributes(ctx) taken inside the callback */
RPC_STATUS RPC_ENTRY rx_sec_cb(RPC_IF_HANDLE ifh, void *ctx);

/* ---- server control ---- */
RPC_STATUS rx_use_ep(const WCHAR *ep);                 /* RpcServerUseProtseqEpW(L"ncalrpc", ...) */
RPC_BINDING_HANDLE rx_bind(const WCHAR *ep);           /* string binding -> handle (NULL on failure) */
RPC_BINDING_HANDLE rx_bind_obj(const WCHAR *ep, const UUID *obj);
void rx_unbind(RPC_BINDING_HANDLE h);
/* use endpoint + register interface (flags/maxcalls via RegisterIfEx when flags!=~0) + listen without waiting.
 * Returns RPC_S_OK or the first failing status. Cleans a leftover listening state first. */
RPC_STATUS rx_serve(RPC_IF_HANDLE ifs, const WCHAR *ep, UUID *mgrtype, void *mgrepv);
RPC_STATUS rx_serve_ex(RPC_IF_HANDLE ifs, const WCHAR *ep, UUID *mgrtype, void *mgrepv, unsigned flags,
                       unsigned maxcalls, RPC_IF_CALLBACK_FN *cb);
void rx_server_down(RPC_IF_HANDLE ifs, UUID *mgrtype);  /* stop, wait, unregister (all bounded) */
void rx_server_reset(void);                             /* stop listening if a previous test left it on */
int rx_listening(void);                                 /* RpcMgmtIsServerListening(NULL)==OK */

/* one-call fixture: fresh endpoint, xif registered and listening, client binding */
typedef struct { WCHAR ep[64]; RPC_BINDING_HANDLE h; } rx_fx;
int rx_fx_up(rx_fx *f);                         /* 1 on success */
void rx_fx_down(rx_fx *f);
/* shorthand for NDR client calls with the fixture binding */
DWORD rx_add(RPC_BINDING_HANDLE h, int a, int b, int *r);

/* run fn on a thread and wait <= ms; returns TRUE if it finished */
HANDLE rx_thread(LPTHREAD_START_ROUTINE fn, void *arg);
BOOL rx_join(HANDLE th, DWORD ms);

/* child-process server (see rx_child_server in rpcx.c): returns process handle or NULL */
HANDLE rx_spawn_server(const WCHAR *ep, const char *mode, PROCESS_INFORMATION *pi);
int rx_wait_server_ready(const WCHAR *ep, DWORD ms);   /* polls until a p_void call succeeds */

/* single-evaluation equality check (the harness EXPECT_EQ evaluates its arguments twice, which is wrong
 * for expressions that call into the runtime) */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); \
    t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)
#define STR_EQ_NOCASE(a, b) (!lstrcmpiW((a), (b)))
#endif
