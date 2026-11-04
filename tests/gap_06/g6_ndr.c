/* gap_06: NdrClientCall2, NdrClientCall3, NdrAsyncClientCall - argument validation that needs no server.
 * Decomp facts (Win10 rpcrt4): these entry points report errors by RpcRaiseException (the exception code is the RPC status):
 *  NdrClientCall3: no syntax-info entry whose TransferSyntax.Data1 equals pProxyInfo->pTransferSyntax->Data1 (or nCount==0) => 1730
 *    (RPC_S_UNSUPPORTED_TRANS_SYN); entry found but the proc's format offset is 0xFFFF (NDR syntax, 16-bit table) or (void*)-1 (NDR64, pointer
 *    table) => 1745 (RPC_S_PROCNUM_OUT_OF_RANGE).
 *  NdrClientCall2: MIDL_STUB_DESC.Version > 0xA0000 => 1829; explicit handle of an unknown kind => 1766 (RPC_S_INTERNAL_ERROR);
 *    explicit context handle that is NULL without the "can be NULL" flag => 1775 (RPC_X_SS_IN_NULL_CONTEXT);
 *    NULL (or non-binding) primitive handle reaching NdrGetBuffer => 1702 (RPC_S_INVALID_BINDING).
 *  NdrAsyncClientCall: a NULL RPC_ASYNC_STATE* first argument => 1914 (RPC_S_INVALID_ASYNC_HANDLE) unless the proc maps comm faults. */
#include "gap6.h"
#include <rpc.h>
#include <rpcndr.h>
#include <rpcasync.h>

#define NdrFcShort(s) (unsigned char)(s), (unsigned char)((s) >> 8)
#define NdrFcLong(l)  (unsigned char)(l), (unsigned char)((l) >> 8), (unsigned char)((l) >> 16), (unsigned char)((l) >> 24)

/* ---- run a call on a helper thread; report the exception code it raised (0 if it returned) ---- */
static volatile DWORD g_tid, g_code;
static LONG CALLBACK veh(EXCEPTION_POINTERS *ep) {
    if (GetCurrentThreadId() != g_tid) return EXCEPTION_CONTINUE_SEARCH;
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    /* rpcrt4 probes caller handles under SEH on Windows: access violations (and other hardware faults) are first-chance noise here, the
     * frames inside rpcrt4 turn them into RPC statuses. Only an AV that nobody handles (the unhandled filter below) counts. */
    if (c == 0xC0000005 || c == 0xC00000FD) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000A || c == 0x406D1388 || c == 0x40010005) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c;
    ExitThread(0xE0C0DE01);
    return EXCEPTION_CONTINUE_SEARCH;
}
static LONG WINAPI ufilter(EXCEPTION_POINTERS *ep) {
    if (GetCurrentThreadId() != g_tid) return EXCEPTION_CONTINUE_SEARCH;
    g_code = ep->ExceptionRecord->ExceptionCode;
    ExitThread(0xE0C0DE01);
    return EXCEPTION_CONTINUE_SEARCH;
}
typedef void (*call_fn)(void *);
typedef struct { call_fn fn; void *arg; } job_t;
static DWORD WINAPI job_thread(void *p) { job_t *j = p; g_tid = t_veh_ignore_tid = GetCurrentThreadId(); j->fn(j->arg); return 0; }
/* rpcrt4 creates its private heap lazily, on the first API that needs the runtime (RpcBindingFromStringBinding and friends). The
 * NdrClientCall2 / NdrAsyncClientCall error paths allocate from it; in a process that has not touched RPC yet that heap is NULL and the call
 * dies with an access violation in RtlAllocateHeap (Win10 ntdll) instead of reporting a status. Start the runtime before every probe. */
static void rpc_warmup(void) {
    static int done;
    if (done) return;
    done = 1;
    RPC_STATUS (WINAPI *from)(RPC_WSTR, RPC_BINDING_HANDLE *) = (void *)t_proc("RpcBindingFromStringBindingW");
    RPC_STATUS (WINAPI *fr)(RPC_BINDING_HANDLE *) = (void *)t_proc("RpcBindingFree");
    RPC_BINDING_HANDLE b = NULL;
    if (from && from((RPC_WSTR)L"ncalrpc:[olet_g6_warmup]", &b) == RPC_S_OK && fr) fr(&b);
    t_note("rpcrt4 runtime started before the NdrClientCall2/NdrAsyncClientCall probes (its heap is created lazily; unstarted, those paths fault in RtlAllocateHeap)");
}
static DWORD raised(call_fn fn, void *arg) {
    rpc_warmup();    /* exception code, 0 = returned normally, 0xFFFFFFF0 = timeout */
    job_t j = { fn, arg };
    g_code = 0;
    PVOID h = AddVectoredExceptionHandler(1, veh);
    LPTOP_LEVEL_EXCEPTION_FILTER oldf = SetUnhandledExceptionFilter(ufilter);
    HANDLE th = CreateThread(NULL, 0, job_thread, &j, 0, NULL);
    DWORD res = 0xFFFFFFF0;
    if (th) {
        if (WaitForSingleObject(th, 10000) == WAIT_OBJECT_0) {
            DWORD ec = 0; GetExitCodeThread(th, &ec);
            res = (ec == 0xE0C0DE01) ? g_code : 0;
        } else TerminateThread(th, 1);
        CloseHandle(th);
    }
    t_veh_ignore_tid = 0;
    SetUnhandledExceptionFilter(oldf);
    RemoveVectoredExceptionHandler(h);
    return res;
}

/* ---- NdrClientCall3 ---- */
static const RPC_SYNTAX_IDENTIFIER NDR32 = { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} };
static const RPC_SYNTAX_IDENTIFIER NDR64 = { {0x71710533, 0xbeba, 0x4937, {0x83, 0x19, 0xb5, 0xdb, 0xef, 0x9c, 0xcc, 0x36}}, {1, 0} };
static const RPC_SYNTAX_IDENTIFIER OTHER = { {0x12345678, 0x1111, 0x2222, {1, 2, 3, 4, 5, 6, 7, 8}}, {1, 0} };
static const unsigned short OFF_TAB_BAD[4] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
static const ULONG_PTR OFF_PTR_BAD[4] = { (ULONG_PTR)-1, (ULONG_PTR)-1, (ULONG_PTR)-1, (ULONG_PTR)-1 };
static const unsigned char PROC_DUMMY[16] = { 0 };
static const MIDL_STUB_DESC STUB0 = { 0 };

typedef struct { MIDL_STUBLESS_PROXY_INFO *pi; ULONG proc; } c3_arg;
static void do_call3(void *p) { c3_arg *a = p; LONG ret = 0; U(NdrClientCall3)(a->pi, a->proc, &ret); }

static DWORD call3(const RPC_SYNTAX_IDENTIFIER *want, ULONG_PTR count, MIDL_SYNTAX_INFO *si, ULONG proc) {
    MIDL_STUBLESS_PROXY_INFO pi; memset(&pi, 0, sizeof pi);
    pi.pStubDesc = (PMIDL_STUB_DESC)&STUB0; pi.pTransferSyntax = (PRPC_SYNTAX_IDENTIFIER)want; pi.nCount = count; pi.pSyntaxInfo = si;
    c3_arg a = { &pi, proc };
    return raised(do_call3, &a);
}
TEST(gap6_ndrcall3_no_syntax_info_raises_unsupported_syntax) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    EQ(call3(&NDR32, 0, NULL, 0), RPC_S_UNSUPPORTED_TRANS_SYN);
    EQ(call3(&NDR64, 0, NULL, 3), RPC_S_UNSUPPORTED_TRANS_SYN);
}
TEST(gap6_ndrcall3_syntax_mismatch) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    MIDL_SYNTAX_INFO si; memset(&si, 0, sizeof si);
    si.TransferSyntax = NDR32; si.ProcString = PROC_DUMMY; si.FmtStringOffset = OFF_TAB_BAD;
    EQ(call3(&NDR64, 1, &si, 0), RPC_S_UNSUPPORTED_TRANS_SYN);       /* only NDR32 offered, NDR64 wanted */
    EQ(call3(&OTHER, 1, &si, 0), RPC_S_UNSUPPORTED_TRANS_SYN);
    si.TransferSyntax = NDR64; si.FmtStringOffset = (const unsigned short *)OFF_PTR_BAD;
    EQ(call3(&NDR32, 1, &si, 0), RPC_S_UNSUPPORTED_TRANS_SYN);
}
TEST(gap6_ndrcall3_ndr32_unused_proc_offset) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    MIDL_SYNTAX_INFO si; memset(&si, 0, sizeof si);
    si.TransferSyntax = NDR32; si.ProcString = PROC_DUMMY; si.FmtStringOffset = OFF_TAB_BAD;
    for (ULONG p = 0; p < 4; p++) EQ(call3(&NDR32, 1, &si, p), RPC_S_PROCNUM_OUT_OF_RANGE);
}
TEST(gap6_ndrcall3_ndr64_unused_proc_offset) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    MIDL_SYNTAX_INFO si; memset(&si, 0, sizeof si);
    si.TransferSyntax = NDR64; si.ProcString = PROC_DUMMY; si.FmtStringOffset = (const unsigned short *)OFF_PTR_BAD;
    for (ULONG p = 0; p < 4; p++) EQ(call3(&NDR64, 1, &si, p), RPC_S_PROCNUM_OUT_OF_RANGE);
}
TEST(gap6_ndrcall3_second_entry_selected_by_data1) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    MIDL_SYNTAX_INFO si[2]; memset(si, 0, sizeof si);
    si[0].TransferSyntax = NDR64; si[0].ProcString = PROC_DUMMY; si[0].FmtStringOffset = (const unsigned short *)OFF_PTR_BAD;
    si[1].TransferSyntax = NDR32; si[1].ProcString = PROC_DUMMY; si[1].FmtStringOffset = OFF_TAB_BAD;
    EQ(call3(&NDR32, 2, si, 1), RPC_S_PROCNUM_OUT_OF_RANGE);          /* matched at index 1 */
    EQ(call3(&NDR64, 2, si, 1), RPC_S_PROCNUM_OUT_OF_RANGE);          /* matched at index 0 */
    EQ(call3(&OTHER, 2, si, 1), RPC_S_UNSUPPORTED_TRANS_SYN);
    EQ(call3(&NDR32, 1, si, 1), RPC_S_UNSUPPORTED_TRANS_SYN);         /* nCount=1 hides the second entry */
}
TEST(gap6_ndrcall3_match_is_on_data1_only) {
    REQUIRE(U(NdrClientCall3));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    RPC_SYNTAX_IDENTIFIER alias = NDR32;
    alias.SyntaxGUID.Data2 = 0x9999; alias.SyntaxVersion.MajorVersion = 77;     /* only Data1 is compared */
    MIDL_SYNTAX_INFO si; memset(&si, 0, sizeof si);
    si.TransferSyntax = NDR32; si.ProcString = PROC_DUMMY; si.FmtStringOffset = OFF_TAB_BAD;
    EQ(call3(&alias, 1, &si, 0), RPC_S_PROCNUM_OUT_OF_RANGE);
}

/* ---- NdrClientCall2 ---- */
/* Oif procedure headers, hand-assembled in the layout MIDL emits (see tests/rpc_server_client_ncalrpc/rpcx_fmt.h). */
static const unsigned char FMT_EXPL_PRIM[] = { 0, 0,
    0x00, 0x48, NdrFcLong(0), NdrFcShort(0), NdrFcShort(8),
    0x32, 0x00, NdrFcShort(0),                              /* explicit FC_BIND_PRIMITIVE at stack offset 0 */
    NdrFcShort(0), NdrFcShort(0), 0x40, 0x00, 0x0a, 0x01, NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0) };
static const unsigned char FMT_EXPL_CTX[] = { 0, 0,
    0x00, 0x48, NdrFcLong(0), NdrFcShort(0), NdrFcShort(8),
    0x30, 0x40, NdrFcShort(0), 0x00, 0x00,                 /* FC_BIND_CONTEXT, flags: in, not NULL-able */
    NdrFcShort(0), NdrFcShort(0), 0x40, 0x00, 0x0a, 0x01, NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0) };
static const unsigned char FMT_EXPL_BADKIND[] = { 0, 0,
    0x00, 0x48, NdrFcLong(0), NdrFcShort(0), NdrFcShort(8),
    0x55, 0x00, NdrFcShort(0),                              /* handle kind that does not exist */
    NdrFcShort(0), NdrFcShort(0), 0x40, 0x00, 0x0a, 0x01, NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0) };
static const unsigned char FMT_IMPL_PRIM[] = { 0, 0,
    0x32, 0x48, NdrFcLong(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), 0x40, 0x00, 0x0a, 0x01, NdrFcShort(0), NdrFcShort(0), NdrFcShort(0), NdrFcShort(0) };

static void *g_dummy_ih;                                   /* implicit handle variable, kept NULL */
static void * __RPC_USER g6_alloc(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n); }
static void __RPC_USER g6_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }
static const RPC_CLIENT_INTERFACE CI = { sizeof(RPC_CLIENT_INTERFACE),
    { {0x6f0e7a10, 0x0006, 0x4c3d, {0x9a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa6}}, {1, 0} },
    { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} }, 0, 0, 0, 0, 0, 0 };
static MIDL_STUB_DESC mk_stub(unsigned long version) {
    MIDL_STUB_DESC d; memset(&d, 0, sizeof d);
    d.RpcInterfaceInformation = (void *)&CI; d.pfnAllocate = g6_alloc; d.pfnFree = g6_free;
    d.IMPLICIT_HANDLE_INFO.pPrimitiveHandle = (handle_t *)&g_dummy_ih;
    d.Version = version; d.MIDLVersion = 0x50200ca;
    return d;
}
typedef struct { MIDL_STUB_DESC *sd; const unsigned char *fmt; ULONG_PTR a0, a1; } c2_arg;
static void do_call2(void *p) { c2_arg *a = p; U(NdrClientCall2)(a->sd, (PFORMAT_STRING)(a->fmt + 2), a->a0, a->a1); }
static DWORD call2(unsigned long stub_version, const unsigned char *fmt, ULONG_PTR a0) {
    MIDL_STUB_DESC sd = mk_stub(stub_version);
    c2_arg a = { &sd, fmt, a0, 0 };
    return raised(do_call2, &a);
}
TEST(gap6_ndrcall2_explicit_null_primitive_handle_invalid_binding) {
    REQUIRE(U(NdrClientCall2));
    EQ(call2(0x50002, FMT_EXPL_PRIM, 0), RPC_S_INVALID_BINDING);
}
TEST(gap6_ndrcall2_explicit_non_binding_handle_invalid_binding) {
    REQUIRE(U(NdrClientCall2));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    static unsigned char fake[256];                     /* readable memory that is not a binding handle (no signature) */
    EQ(call2(0x50002, FMT_EXPL_PRIM, (ULONG_PTR)fake), RPC_S_INVALID_BINDING);
}
TEST(gap6_ndrcall2_implicit_null_primitive_handle_invalid_binding) {
    REQUIRE(U(NdrClientCall2));
    EQ(call2(0x50002, FMT_IMPL_PRIM, 0), RPC_S_INVALID_BINDING);
}
TEST(gap6_ndrcall2_explicit_null_context_handle) {
    REQUIRE(U(NdrClientCall2));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    EQ(call2(0x50002, FMT_EXPL_CTX, 0), RPC_X_SS_IN_NULL_CONTEXT);
}
TEST(gap6_ndrcall2_unknown_explicit_handle_kind) {
    REQUIRE(U(NdrClientCall2));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    EQ(call2(0x50002, FMT_EXPL_BADKIND, 0), RPC_S_INTERNAL_ERROR);
}
TEST(gap6_ndrcall2_stub_version_too_new) {
    REQUIRE(U(NdrClientCall2));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    EQ(call2(0xA0001, FMT_EXPL_PRIM, 0), 1829);
    EQ(call2(0x7FFFFFFF, FMT_EXPL_PRIM, 0), 1829);
    EQ(call2(0xA0000, FMT_EXPL_PRIM, 0), RPC_S_INVALID_BINDING);       /* 0xA0000 itself is accepted */
}

/* ---- NdrAsyncClientCall ---- */
static void do_async(void *p) { c2_arg *a = p; U(NdrAsyncClientCall)(a->sd, (PFORMAT_STRING)(a->fmt + 2), a->a0, a->a1); }
TEST(gap6_ndrasync_null_async_state) {
    REQUIRE(U(NdrAsyncClientCall));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    MIDL_STUB_DESC sd = mk_stub(0x50002);
    c2_arg a = { &sd, FMT_EXPL_PRIM, 0 /* pAsync */, 0 /* handle */ };
    EQ(raised(do_async, &a), RPC_S_INVALID_ASYNC_HANDLE);
}
TEST(gap6_ndrasync_null_async_state_both_handle_kinds) {
    REQUIRE(U(NdrAsyncClientCall));
    if (t_is_wine()) { t_skip("wine's rpcrt4 does not implement this path like Windows (see decomp notes in the file header)"); return; }
    const unsigned char *f[2] = { FMT_EXPL_PRIM, FMT_IMPL_PRIM };
    for (int i = 0; i < 2; i++) {
        MIDL_STUB_DESC sd = mk_stub(0x50002);
        c2_arg a = { &sd, f[i], 0, 0 };
        DWORD c = raised(do_async, &a);
        CHECKF(c == RPC_S_INVALID_ASYNC_HANDLE, "fmt %d code %lu", i, c);
    }
}
TEST(gap6_ndr_exports_have_distinct_addresses) {
    REQUIRE(U(NdrClientCall2)); REQUIRE(U(NdrClientCall3)); REQUIRE(U(NdrAsyncClientCall));
    CHECK((void *)U(NdrClientCall2) != (void *)U(NdrClientCall3));
    CHECK((void *)U(NdrClientCall2) != (void *)U(NdrAsyncClientCall));
    CHECK((void *)U(NdrClientCall3) != (void *)U(NdrAsyncClientCall));
}
