/* Shared helpers for the rpc_ep_mgmt_misc area. Nothing here links rpcrt4: every runtime call is made via U()/UN(). */
#ifndef EPX_H
#define EPX_H
#include "t.h"
#include <rpc.h>
#include <rpcdcep.h>
#include <rpcndr.h>
#include <rpcasync.h>

/* RPC status codes used by name in the tests (values from winerror.h / MS docs) */
#ifndef RPC_S_NO_CALL_ACTIVE
#define RPC_S_NO_CALL_ACTIVE 1725
#endif
#define EPX_INVALID_BINDING       1702
#define EPX_WRONG_KIND_OF_BINDING 1701
#define EPX_NO_CALL_ACTIVE        1725
#define EPX_CANNOT_SUPPORT        1764
#define EPX_ENTRY_NOT_FOUND       1761
#define EPX_NOTHING_TO_EXPORT     1718
#define EPX_INVALID_ENTRY         1751
#define EPX_CANT_PERFORM_OP       1752
#define EPX_UNKNOWN_AUTHN_SERVICE 1747
#define EPX_BINDING_HAS_NO_AUTH   1746

/* Wine stubs (aborts the process on call) most of the APIs of this area; on Windows this is a no-op. */
#ifndef EPM_NOSTUB
#define WINE_STUB() do { if (t_is_wine()) { t_skip("stubbed in wine"); return; } } while (0)
#else
#define WINE_STUB() ((void)0)
#endif

/* Like EXPECT_EQ but evaluates the call exactly once (EXPECT_EQ evaluates its arguments twice). */
#define EQ(call, want) do { ULONG_PTR v_ = (ULONG_PTR)(call); t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #call " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)

typedef void (*epx_fn)(void *);

/* Run fn(arg) on the calling thread; returns 0, or the code of the exception it raised (the call is abandoned). */
DWORD epx_try(epx_fn fn, void *arg);
extern DWORD epx_last_flags, epx_last_nparams;   /* ExceptionFlags / NumberParameters of the last caught exception */

/* Run fn(arg) on a fresh thread; TRUE if it finished within ms (thread handle closed). */
BOOL epx_run(epx_fn fn, void *arg, DWORD ms);

/* 256 zero bytes: valid memory that is NOT an RPC handle (wrong magic) */
extern unsigned char epx_fake[256];

/* A client binding handle "ncalrpc:[<unique endpoint>]" (no server needed), NULL on failure. */
RPC_BINDING_HANDLE epx_mkbind(void);
void epx_unique_ep(WCHAR *buf, int cch);

/* generic reply plumbing for the in-process raw server (epm_misc_server.c) */
#endif
