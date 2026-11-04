/* Shared helpers for the rpc_ndr_marshal_formats area.
 * Everything in rpcrt4 is reached via U()/UIN() (GetProcAddress); only kernel32 scaffolding is direct. */
#ifndef NDRH_H
#define NDRH_H
#include "t.h"
#include <rpc.h>
#include <rpcndr.h>
#include <ndrtypes.h>
#include <midles.h>
#include <setjmp.h>
#include <objbase.h>

/* Wine aborts the process on calls to unimplemented rpcrt4 exports: skip those there (no-op on Windows). */
#define WSTUB(n) do { if (t_is_wine()) { t_skip("unimplemented in wine: " n); return; } } while (0)

#define S16(x) (unsigned char)((x) & 0xff), (unsigned char)(((x) >> 8) & 0xff)

/* RPC exception codes raised by the NDR engine (RpcRaiseException) */
#define X_NULL_REF      1780  /* RPC_X_NULL_REF_POINTER */
#define X_ENUM_RANGE    1781  /* RPC_X_ENUM_VALUE_OUT_OF_RANGE */
#define X_BYTE_COUNT    1782  /* RPC_X_BYTE_COUNT_TOO_SMALL */
#define X_BAD_STUB      1783  /* RPC_X_BAD_STUB_DATA */
#define X_INVALID_BOUND 1734  /* RPC_X_INVALID_BOUND */
#define X_INTERNAL      1766  /* RPC_S_INTERNAL_ERROR */
#define X_INVALID_ES    1827  /* RPC_X_INVALID_ES_ACTION */

/* ---- exception capture (VEH + non-unwinding builtin longjmp; no SEH keywords in mingw) ---- */
/* Recovery uses the compiler's non-unwinding __builtin_setjmp/__builtin_longjmp: the CRT longjmp unwinds frame by frame
   (RtlUnwindEx) from inside the handler and, on Windows 10, occasionally walks off the top of the stack while unwinding
   through the RPC runtime's raise path, which killed the process with an access violation. */
static void *n_jb[5];
static volatile LONG n_armed;
static volatile DWORD n_code;
static LONG CALLBACK n_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!n_armed) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == 0x4001000A || c == 0x40010005 || c == 0x80000003 || c == 0x4001000B) return EXCEPTION_CONTINUE_SEARCH;
    n_code = c; n_armed = 0;
    __builtin_longjmp(n_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
/* Windows 10: server side NdrServerInitialize* needs the RPC runtime to be initialised (it allocates a per-thread object
   from the runtime's heap) and raises an access violation inside ntdll when it is not. rpcrt4 initialises itself lazily on
   the first public API call, so make one before any test runs; then create the per-thread object. */
static void n_warm_ctor(void) __attribute__((constructor));
static void n_warm_ctor(void) {
    HMODULE m = LoadLibraryA("rpcrt4.dll");
    unsigned char *(WINAPI *init)(PRPC_MESSAGE, PMIDL_STUB_MESSAGE, PMIDL_STUB_DESC) = m ? (void *)GetProcAddress(m, "NdrServerInitializeNew") : NULL;
    long (WINAPI *valid)(const wchar_t *) = m ? (void *)GetProcAddress(m, "RpcNetworkIsProtseqValidW") : NULL;
    RPC_MESSAGE rpc; MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC d; unsigned char b[8];
    if (!init) return;
    if (valid) valid(L"ncalrpc");
    memset(&rpc, 0, sizeof rpc); memset(&sm, 0, sizeof sm); memset(&d, 0, sizeof d);
    d.Version = 0x20000; d.MIDLVersion = 0x50100a4; d.mFlags = 1;
    rpc.Buffer = b; rpc.BufferLength = sizeof b; rpc.RpcFlags = RPC_BUFFER_COMPLETE;
    init(&rpc, &sm, &d);
}
static void n_init(void) {
    static int done;
    if (!done) { done = 1; AddVectoredExceptionHandler(1, n_veh); }
}
#define NTRY    do { n_init(); n_code = 0; if (!__builtin_setjmp(n_jb)) { n_armed = 1;
#define NEND    n_armed = 0; } } while (0)

/* ---- allocator hooks ---- */
static volatile int n_alloc_cnt, n_free_cnt;
static void *CALLBACK n_alloc(SIZE_T n) { n_alloc_cnt++; return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void CALLBACK n_free(void *p) { n_free_cnt++; HeapFree(GetProcessHeap(), 0, p); }

/* ---- stub message context ---- */
typedef struct nctx {
    RPC_MESSAGE rpc;
    MIDL_STUB_MESSAGE sm;
    MIDL_STUB_DESC desc;
    unsigned char *buf;
    ULONG cap;
} nctx;

static void n_fill_desc(MIDL_STUB_DESC *d, const unsigned char *fmt) {
    memset(d, 0, sizeof *d);
    d->pfnAllocate = n_alloc;
    d->pfnFree = n_free;
    d->pFormatTypes = fmt;
    d->fCheckBounds = 1;
    d->Version = 0x20000;
    d->MIDLVersion = 0x50100a4;
    d->mFlags = 1;
}
/* client-side message with an empty (0xCD filled) buffer of cap bytes, for marshalling */
static void nc_client(nctx *c, const unsigned char *fmt, ULONG cap) {
    memset(c, 0, sizeof *c);
    n_fill_desc(&c->desc, fmt);
    U(NdrClientInitializeNew)(&c->rpc, &c->sm, &c->desc, 0);
    c->cap = cap;
    c->buf = HeapAlloc(GetProcessHeap(), 0, cap ? cap : 1);
    memset(c->buf, 0xCD, cap);
    c->sm.RpcMsg->Buffer = c->sm.BufferStart = c->sm.Buffer = c->buf;
    c->sm.BufferEnd = c->buf + cap;
    c->sm.BufferLength = cap;
    c->rpc.BufferLength = cap;
}
/* message that reads data[0..len) (private exact-size copy) ; client!=0 -> IsClient (reply side) */
static void nc_rx(nctx *c, const unsigned char *fmt, const void *data, ULONG len, int client) {
    memset(c, 0, sizeof *c);
    n_fill_desc(&c->desc, fmt);
    c->cap = len;
    c->buf = HeapAlloc(GetProcessHeap(), 0, len ? len : 1);
    if (len) memcpy(c->buf, data, len);
    if (client) {
        U(NdrClientInitializeNew)(&c->rpc, &c->sm, &c->desc, 0);
        c->sm.RpcMsg->Buffer = c->buf;
        c->rpc.BufferLength = len;
    } else {
        c->rpc.Buffer = c->buf;
        c->rpc.BufferLength = len;
        c->rpc.RpcFlags = RPC_BUFFER_COMPLETE;
        U(NdrServerInitializeNew)(&c->rpc, &c->sm, &c->desc);
    }
    c->sm.BufferStart = c->sm.Buffer = c->buf;
    c->sm.BufferEnd = c->buf + len;
    c->sm.BufferLength = len;
    c->sm.MemorySize = 0;
}
static void nc_done(nctx *c) { if (c->buf) HeapFree(GetProcessHeap(), 0, c->buf); c->buf = NULL; }
static ULONG nc_used(nctx *c) { return (ULONG)(c->sm.Buffer - c->buf); }
static int nc_eq(nctx *c, const void *wire, ULONG len) { return nc_used(c) == len && !memcmp(c->buf, wire, len); }

/* ---- family function tables ---- */
typedef void  (WINAPI *bs_fn)(PMIDL_STUB_MESSAGE, unsigned char *, PFORMAT_STRING);
typedef unsigned char *(WINAPI *ma_fn)(PMIDL_STUB_MESSAGE, unsigned char *, PFORMAT_STRING);
typedef unsigned char *(WINAPI *um_fn)(PMIDL_STUB_MESSAGE, unsigned char **, PFORMAT_STRING, unsigned char);
typedef ULONG (WINAPI *ms_fn)(PMIDL_STUB_MESSAGE, PFORMAT_STRING);
typedef void  (WINAPI *fr_fn)(PMIDL_STUB_MESSAGE, unsigned char *, PFORMAT_STRING);
typedef struct { const char *name; bs_fn bs; ma_fn ma; um_fn um; ms_fn ms; fr_fn fr; } fam;
#define FAM(N) { #N, (bs_fn)U(N##BufferSize), (ma_fn)U(N##Marshall), (um_fn)U(N##Unmarshall), (ms_fn)U(N##MemorySize), (fr_fn)U(N##Free) }
#define FAM_NOFREE(N) { #N, (bs_fn)U(N##BufferSize), (ma_fn)U(N##Marshall), (um_fn)U(N##Unmarshall), (ms_fn)U(N##MemorySize), NULL }
static int fam_ok(const fam *f) { return f->bs && f->ma && f->um && f->ms; }

/* Marshal mem via family f; returns bytes written into c's buffer (c must be nc_client'ed with enough cap).
 * *bsz receives the BufferSize prediction (computed first from BufferLength 0). */
static ULONG n_marshal(const fam *f, nctx *c, unsigned char *mem, const unsigned char *fmt, ULONG *bsz) {
    c->sm.BufferLength = 0;
    NTRY f->bs(&c->sm, mem, fmt); NEND;
    t_check(n_code == 0, "ndrh.h", __LINE__, "BufferSize must not raise", "%s BufferSize raised %lu", f->name, (unsigned long)n_code);
    if (bsz) *bsz = c->sm.BufferLength;
    c->sm.Buffer = c->buf; c->sm.BufferStart = c->buf;
    NTRY f->ma(&c->sm, mem, fmt); NEND;
    t_check(n_code == 0, "ndrh.h", __LINE__, "Marshall must not raise", "%s Marshall raised %lu", f->name, (unsigned long)n_code);
    return nc_used(c);
}
/* Exception-safe wrappers: a raised RPC exception becomes a CHECK failure (and returns 0/NULL) instead of killing the run. */
static ULONG sa_ms(const fam *f, nctx *c, const unsigned char *fmt, const char *tag) {
    volatile ULONG r = 0;
    NTRY r = f->ms(&c->sm, fmt); NEND;
    t_check(n_code == 0, "ndrh.h", __LINE__, "MemorySize must not raise", "%s: %s MemorySize raised %lu", tag, f->name, (unsigned long)n_code);
    return r;
}
static unsigned char *sa_um(const fam *f, nctx *c, unsigned char **pp, const unsigned char *fmt, int must, const char *tag) {
    unsigned char *volatile r = NULL;
    NTRY r = f->um(&c->sm, pp, fmt, (unsigned char)must); NEND;
    t_check(n_code == 0, "ndrh.h", __LINE__, "Unmarshall must not raise", "%s: %s Unmarshall raised %lu", tag, f->name, (unsigned long)n_code);
    return r;
}
static void sa_ma(const fam *f, nctx *c, unsigned char *mem, const unsigned char *fmt, const char *tag) {
    NTRY f->ma(&c->sm, mem, fmt); NEND;
    t_check(n_code == 0, "ndrh.h", __LINE__, "Marshall must not raise", "%s: %s Marshall raised %lu", tag, f->name, (unsigned long)n_code);
}
#endif
