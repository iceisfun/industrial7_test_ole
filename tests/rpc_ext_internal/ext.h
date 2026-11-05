/* Shared fixture of the rpc_ext_internal area: one in-process raw ncalrpc server (RPC_DISPATCH_FUNCTION table driven
 * through I_RpcGetBuffer / I_RpcSendReceive), plus a few helpers. Nothing here links rpcrt4: every runtime call goes
 * through U()/UN()/t_proc_in() pointers.
 *
 * Server procedures (request and reply are plain byte buffers; reply is an array of DWORD):
 *   0 nop        empty reply
 *   1 connaddr   I_RpcServerInqLocalConnAddress / I_RpcServerInqRemoteConnAddress with NULL and with the call handle
 *   2 assoc      I_RpcServerGetAssociationID(NULL / call handle), I_RpcServerCheckClientRestriction(call handle)
 *   3 static     I_RpcBindingToStaticStringBindingW(call handle); reply: status, WCHAR string binding
 *   4 secctx     I_RpcBindingInqSecurityContext / ...KeyInfo, NULL and the call handle
 *   5 strict     I_RpcSessionStrictContextHandle() inside a call, then a nop
 *   6 exterr     I_RpcGetExtendedError()
 *   7 tokattr    I_RpcBindingInqClientTokenAttributes, I_RpcBindingHandleToAsyncHandle on the call handle
 */
#ifndef EXT_H
#define EXT_H
#include "t.h"
#include <rpc.h>
#include <rpcdcep.h>
#include <rpcndr.h>
#include <rpcasync.h>

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); \
    t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)

typedef struct ext_srv {
    RPC_SERVER_INTERFACE si;
    RPC_CLIENT_INTERFACE ci;
    RPC_DISPATCH_TABLE dt;
    RPC_DISPATCH_FUNCTION fn[8];
    UUID uuid;
    WCHAR ep[64];
    RPC_BINDING_HANDLE h;
} ext_srv;

UDECL(void, I_RpcRequestMutex, (void **));
UDECL(void, I_RpcClearMutex, (void *));
UDECL(void, I_RpcDeleteMutex, (void *));
UDECL(void, I_RpcPauseExecution, (ULONG));
UDECL(void, NdrUnmarshallBasetypeInline, (void *, ULONGLONG *, unsigned char));

/* The first use of the lock functions before any other call into the RPC runtime of the process faults on Windows 10
 * (the lock object comes from the runtime's own allocator, which is set up by the first ordinary RPC call). Every test
 * that uses them calls this first. */
void ext_warm(void);
int ext_up(ext_srv *s);                 /* fresh endpoint, interface registered, listening, client binding in s->h; 1 on success */
int ext_up_ep2(ext_srv *s);          /* like ext_up, but the endpoint is registered with I_RpcServerUseProtseqEp2W */
void ext_down(ext_srv *s);
/* one raw call; reply bytes copied to out (up to outcap), *outlen = reply length */
RPC_STATUS ext_call(ext_srv *s, RPC_BINDING_HANDLE h, unsigned proc, const void *in, ULONG inlen, void *out, ULONG outcap, ULONG *outlen);
void ext_ep(WCHAR *buf);                /* unique endpoint name */
#endif
