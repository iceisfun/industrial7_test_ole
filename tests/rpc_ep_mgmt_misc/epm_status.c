/* Exact status/exception behaviour of small rpcrt4 services: status mapping, exception filter, RpcRaiseException,
 * the name-service stubs and the argument validation of the endpoint-mapper client API (no RpcSs needed for
 * any of the failing paths tested here). Expectations are taken from the Windows 10 22H2 rpcrt4 decompilation. */
#include "epx.h"

UDECL(int, RpcExceptionFilter, (unsigned long));
typedef void (WINAPI *raise_pfn)(RPC_STATUS);

/* ---------------------------------------------------------------- I_RpcMapWin32Status */
/* The 107-entry StatusMap table of Windows 10 22H2 rpcrt4 (rpc status, NT status); first match wins. */
static const struct { unsigned rpc, nt; } statusmap[] = {
#include "statusmap.inc"
};
#define NMAP (sizeof statusmap / sizeof statusmap[0])
static unsigned model_map(unsigned s) {
    unsigned i;
    for (i = 0; i < NMAP; i++) if (statusmap[i].rpc == s) return statusmap[i].nt;
    return (int)s < 0 ? s : 0xC0000001u;
}

TEST(epm_status_map_win32_table_0_to_10000) {
    unsigned s, bad = 0;
    REQUIRE(U(I_RpcMapWin32Status));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    for (s = 0; s < 10000; s++) {
        unsigned got = (unsigned)U(I_RpcMapWin32Status)((RPC_STATUS)s), want = model_map(s);
        if (got != want) { if (bad++ < 10) CHECKF(0, "I_RpcMapWin32Status(%u) = 0x%08x want 0x%08x", s, got, want); }
    }
    CHECK(bad == 0);
}

TEST(epm_status_map_win32_known_pairs) {
    static const struct { RPC_STATUS in; unsigned out; } k[] = {
        { 0, 0 },
        { ERROR_ACCESS_DENIED, 0xC0000022u },            /* STATUS_ACCESS_DENIED */
        { ERROR_INVALID_PARAMETER, 0xC000000Du },        /* STATUS_INVALID_PARAMETER */
        { ERROR_OUTOFMEMORY, 0xC0000017u },              /* STATUS_NO_MEMORY */
        { ERROR_INSUFFICIENT_BUFFER, 0xC0000023u },      /* STATUS_BUFFER_TOO_SMALL */
        { ERROR_NOACCESS, 0xC0000005u },                 /* STATUS_ACCESS_VIOLATION */
        { ERROR_INVALID_SECURITY_DESCR, 0xC0000079u },
        { RPC_S_INVALID_BINDING, 0xC0020003u },          /* RPC_NT_INVALID_BINDING */
    };
    unsigned i;
    REQUIRE(U(I_RpcMapWin32Status));
    for (i = 0; i < sizeof k / sizeof k[0]; i++)
        EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)(k[i].in), k[i].out);
    /* RPC_S_*  1700.. map to RPC_NT_* 0xC0020001.. contiguous at the start of the range */
    EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)(1700), 0xC0020001u);
    EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)(1701), 0xC0020002u);
    EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)(1702), 0xC0020003u);
    EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)(1703), 0xC0020004u);
}

TEST(epm_status_map_win32_unknown_codes) {
    static const unsigned unk[] = { 9999, 10000, 12345, 0x7FFFFFFFu, 0x10000000u, 0x40000000u };
    unsigned i;
    REQUIRE(U(I_RpcMapWin32Status));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    for (i = 0; i < sizeof unk / sizeof unk[0]; i++)
        EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)((RPC_STATUS)unk[i]), 0xC0000001u);   /* STATUS_UNSUCCESSFUL */
}

TEST(epm_status_map_win32_negative_values_pass_through) {
    static const unsigned neg[] = { 0x80000000u, 0x80000005u, 0xC0000001u, 0xC0000022u, 0xC0020003u, 0xFFFFFFFFu, 0xDEADBEEFu };
    unsigned i;
    REQUIRE(U(I_RpcMapWin32Status));
    for (i = 0; i < sizeof neg / sizeof neg[0]; i++)
        EXPECT_EQ((unsigned)U(I_RpcMapWin32Status)((RPC_STATUS)neg[i]), neg[i]);
}

/* ---------------------------------------------------------------- RpcExceptionFilter */
static int is_fatal(unsigned long c) {
    static const unsigned long f[] = { 0xC0000005, 0xC0000194, 0xC00000AA, 0x80000002, 0xC0000096, 0xC000001D, 0x80000003,
        0xC00000FD, 0xC0000235, 0xC0000006, 0xC0000420, 0xC0000409, 0x80000001, 0xC00002C9 };
    unsigned i;
    for (i = 0; i < sizeof f / sizeof f[0]; i++) if (f[i] == c) return 1;
    return 0;
}

static void filter_range(unsigned long lo, unsigned long hi, unsigned *bad, unsigned *n) {
    unsigned long c;
    for (c = lo; c <= hi; c++) {
        int a = U(I_RpcExceptionFilter)(c), b = UN(RpcExceptionFilter)(c);
        int want = is_fatal(c) ? 0 : 1;     /* EXCEPTION_CONTINUE_SEARCH for fatal ones, EXCEPTION_EXECUTE_HANDLER otherwise */
        (*n)++;
        if (a != want || b != want) { if ((*bad)++ < 10) CHECKF(0, "filter(0x%lx): I_=%d Rpc=%d want %d", c, a, b, want); }
    }
}
TEST(epm_status_exception_filter_ranges) {
    unsigned bad = 0, n = 0;
    REQUIRE(U(I_RpcExceptionFilter) && UN(RpcExceptionFilter));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    filter_range(0, 2000, &bad, &n);
    filter_range(0x40000000, 0x40000005, &bad, &n);
    filter_range(0x80000000, 0x80000005, &bad, &n);
    filter_range(0xC0000000, 0xC00002FF, &bad, &n);
    filter_range(0xE06D7363, 0xE06D7363, &bad, &n);
    filter_range(0xE0000001, 0xE0000010, &bad, &n);
    CHECK(bad == 0);
    CHECK(n > 2500);
}

TEST(epm_status_exception_filter_fatal_set_explicit) {
    static const struct { unsigned long c; const char *n; } f[] = {
        { 0xC0000005, "ACCESS_VIOLATION" }, { 0x80000002, "DATATYPE_MISALIGNMENT" }, { 0x80000003, "BREAKPOINT" },
        { 0xC000001D, "ILLEGAL_INSTRUCTION" }, { 0xC0000096, "PRIVILEGED_INSTRUCTION" }, { 0xC00000AA, "INSTRUCTION_MISALIGNMENT" },
        { 0xC00000FD, "STACK_OVERFLOW" }, { 0xC0000194, "POSSIBLE_DEADLOCK" }, { 0x80000001, "GUARD_PAGE_VIOLATION" },
        { 0xC0000006, "IN_PAGE_ERROR" }, { 0xC0000235, "HANDLE_NOT_CLOSABLE" }, { 0xC0000420, "ASSERTION_FAILURE" },
        { 0xC0000409, "STACK_BUFFER_OVERRUN" }, { 0xC00002C9, "REG_NAT_CONSUMPTION" } };
    unsigned i;
    REQUIRE(U(I_RpcExceptionFilter));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    for (i = 0; i < sizeof f / sizeof f[0]; i++) {
        int r = U(I_RpcExceptionFilter)(f[i].c);
        CHECKF(r == EXCEPTION_CONTINUE_SEARCH, "%s: %d", f[i].n, r);
    }
}

TEST(epm_status_exception_filter_nonfatal_samples) {
    static const unsigned long ok[] = { 0, 1, 14, 87, 1700, 1702, 1726, 0x40000000, 0xC0000008 /* INVALID_HANDLE */, 0xC0000017,
        0xC0000094 /* INT_DIVIDE_BY_ZERO */, 0xC0000095, 0xC000008C /* ARRAY_BOUNDS */, 0xC000008E, 0xC0000090, 0xC0000093,
        0xC0000025 /* NONCONTINUABLE */, 0xC0000026, 0x80000004 /* SINGLE_STEP */, 0xE06D7363 };
    unsigned i;
    REQUIRE(U(I_RpcExceptionFilter));
    for (i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        int r = U(I_RpcExceptionFilter)(ok[i]);
        CHECKF(r == EXCEPTION_EXECUTE_HANDLER, "0x%lx: %d", ok[i], r);
    }
}

/* ---------------------------------------------------------------- RpcRaiseException */
static RPC_STATUS g_raise_code;
static void do_raise(void *p) { ((raise_pfn)U(RpcRaiseException))(g_raise_code); }

TEST(epm_status_raise_exception_propagates_code) {
    static const RPC_STATUS codes[] = { 1, 14, 87, 1700, 1722, 1783, 0x6D8, 0x1234, 0xC0000008u, 0xE0001234u, 0x80000010u, 0x40000099u };
    unsigned i;
    REQUIRE(U(RpcRaiseException));
    for (i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        DWORD got;
        g_raise_code = codes[i];
        got = epx_try(do_raise, NULL);
        CHECKF(got == (DWORD)codes[i], "raised 0x%lx caught 0x%lx", (unsigned long)codes[i], (unsigned long)got);
    }
}

TEST(epm_status_raise_exception_record_is_noncontinuable_without_parameters) {
    REQUIRE(U(RpcRaiseException));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    g_raise_code = 0x1234;
    EQ(epx_try(do_raise, NULL), 0x1234);
    CHECK(epx_last_flags & EXCEPTION_NONCONTINUABLE);
    EXPECT_EQ(epx_last_nparams, 0);
}

TEST(epm_status_raise_exception_access_violation_is_translated) {
    /* RpcpRaiseException maps STATUS_ACCESS_VIOLATION (0xC0000005) to ERROR_NOACCESS (998) */
    REQUIRE(U(RpcRaiseException));
    WINE_STUB();   /* wine keeps the pre-2003 status map / filter / raise semantics */
    g_raise_code = 0xC0000005u;
    EQ(epx_try(do_raise, NULL), 998);
}

/* the filter accepts what RpcRaiseException raises for ordinary RPC statuses */
TEST(epm_status_raised_rpc_codes_pass_the_filter) {
    unsigned i;
    REQUIRE(U(I_RpcExceptionFilter));
    for (i = 1700; i < 1800; i++) {
        EQ(U(I_RpcExceptionFilter)(i), EXCEPTION_EXECUTE_HANDLER);
    }
}

/* ---------------------------------------------------------------- name service stubs */
TEST(epm_status_ns_inq_entry_name_cannot_support) {
    RPC_WSTR w = (RPC_WSTR)(ULONG_PTR)0x77; RPC_CSTR a = (RPC_CSTR)(ULONG_PTR)0x77; unsigned s;
    REQUIRE(U(RpcNsBindingInqEntryNameW) && U(RpcNsBindingInqEntryNameA));
    WINE_STUB();
    for (s = 0; s < 4; s++) {
        EQ(U(RpcNsBindingInqEntryNameW)(NULL, s, &w), EPX_CANNOT_SUPPORT);
        EQ(U(RpcNsBindingInqEntryNameA)(NULL, s, &a), EPX_CANNOT_SUPPORT);
    }
    EXPECT_EQ((ULONG_PTR)w, 0x77);       /* output untouched */
    EXPECT_EQ((ULONG_PTR)a, 0x77);
}

TEST(epm_status_ns_inq_entry_name_ignores_binding_validity) {
    RPC_WSTR w = NULL; RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcNsBindingInqEntryNameW));
    WINE_STUB();
    EQ(U(RpcNsBindingInqEntryNameW)((RPC_BINDING_HANDLE)epx_fake, 1, &w), EPX_CANNOT_SUPPORT);   /* handle is never looked at */
    h = epx_mkbind();
    if (h) { EQ(U(RpcNsBindingInqEntryNameW)(h, 1, &w), EPX_CANNOT_SUPPORT); U(RpcBindingFree)(&h); }
    CHECK(w == NULL);
}

UDECL(RPC_STATUS, I_RpcNsBindingSetEntryNameA, (RPC_BINDING_HANDLE, ULONG, RPC_CSTR));
UDECL(RPC_STATUS, I_RpcNsBindingSetEntryNameW, (RPC_BINDING_HANDLE, ULONG, RPC_WSTR));
UDECL(RPC_STATUS, I_RpcNsInterfaceExported, (ULONG, unsigned short *, RPC_SERVER_INTERFACE *));
UDECL(RPC_STATUS, I_RpcNsInterfaceUnexported, (ULONG, unsigned short *, RPC_SERVER_INTERFACE *));
TEST(epm_status_ns_internal_stubs_cannot_support) {
    REQUIRE(UN(I_RpcNsBindingSetEntryNameA) && UN(I_RpcNsBindingSetEntryNameW) && UN(I_RpcNsInterfaceExported) && UN(I_RpcNsInterfaceUnexported));
    WINE_STUB();
    EQ(UN(I_RpcNsBindingSetEntryNameA)(NULL, 0, NULL), EPX_CANNOT_SUPPORT);
    EQ(UN(I_RpcNsBindingSetEntryNameW)(NULL, 0, NULL), EPX_CANNOT_SUPPORT);
    EQ(UN(I_RpcNsBindingSetEntryNameW)(NULL, 3, (RPC_WSTR)L"/.:/x"), EPX_CANNOT_SUPPORT);
    EQ(UN(I_RpcNsInterfaceExported)(0, NULL, NULL), EPX_CANNOT_SUPPORT);
    EQ(UN(I_RpcNsInterfaceUnexported)(0, NULL, NULL), EPX_CANNOT_SUPPORT);
}

/* ---------------------------------------------------------------- endpoint-mapper client: validation paths */
static void fill(char *b, int n) { memset(b, 'a', n); b[n] = 0; }

TEST(epm_status_ep_register_a_annotation_and_vector_validation) {
    char ann[100];
    REQUIRE(U(RpcEpRegisterA) && U(RpcEpRegisterNoReplaceA));
    WINE_STUB();
    fill(ann, 64);
    EQ(U(RpcEpRegisterA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_INVALID_ENTRY);          /* annotation >= 64 chars */
    EQ(U(RpcEpRegisterNoReplaceA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_INVALID_ENTRY);
    fill(ann, 99);
    EQ(U(RpcEpRegisterA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_INVALID_ENTRY);
    fill(ann, 63);
    EQ(U(RpcEpRegisterA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_NOTHING_TO_EXPORT);      /* 63 is fine; NULL vector */
    EQ(U(RpcEpRegisterNoReplaceA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_NOTHING_TO_EXPORT);
    fill(ann, 0);
    EQ(U(RpcEpRegisterA)(NULL, NULL, NULL, (RPC_CSTR)ann), EPX_NOTHING_TO_EXPORT);
    EQ(U(RpcEpRegisterA)(NULL, NULL, NULL, NULL), EPX_NOTHING_TO_EXPORT);
    EQ(U(RpcEpRegisterNoReplaceA)(NULL, NULL, NULL, NULL), EPX_NOTHING_TO_EXPORT);
}

TEST(epm_status_ep_register_w_annotation_and_vector_validation) {
    WCHAR ann[100]; int i;
    REQUIRE(U(RpcEpRegisterW) && U(RpcEpRegisterNoReplaceW));
    WINE_STUB();
    for (i = 0; i < 64; i++) { ann[i] = L'a'; }
    ann[64] = 0;
    EQ(U(RpcEpRegisterW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_INVALID_ENTRY);
    EQ(U(RpcEpRegisterNoReplaceW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_INVALID_ENTRY);
    ann[63] = 0;
    EQ(U(RpcEpRegisterW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_NOTHING_TO_EXPORT);
    EQ(U(RpcEpRegisterNoReplaceW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_NOTHING_TO_EXPORT);
    EQ(U(RpcEpRegisterW)(NULL, NULL, NULL, NULL), EPX_NOTHING_TO_EXPORT);
    EQ(U(RpcEpRegisterNoReplaceW)(NULL, NULL, NULL, NULL), EPX_NOTHING_TO_EXPORT);
}

TEST(epm_status_ep_register_w_counts_ansi_length_of_annotation) {
    /* the W entry converts to the ANSI code page first; 63 ASCII chars stay 63 bytes */
    WCHAR ann[70]; int i;
    REQUIRE(U(RpcEpRegisterW));
    WINE_STUB();
    for (i = 0; i < 63; i++) { ann[i] = L'z'; }
    ann[63] = 0;
    EQ(U(RpcEpRegisterW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_NOTHING_TO_EXPORT);
    ann[63] = L'z'; ann[64] = 0;
    EQ(U(RpcEpRegisterW)(NULL, NULL, NULL, (RPC_WSTR)ann), EPX_INVALID_ENTRY);
}

TEST(epm_status_ep_unregister_null_vector) {
    REQUIRE(U(RpcEpUnregister));
    WINE_STUB();
    EQ(U(RpcEpUnregister)(NULL, NULL, NULL), EPX_NOTHING_TO_EXPORT);
}

TEST(epm_status_ep_resolve_binding_invalid_handles) {
    RPC_BINDING_HANDLE h;
    REQUIRE(U(RpcEpResolveBinding));
    WINE_STUB();
    EQ(U(RpcEpResolveBinding)(NULL, NULL), EPX_INVALID_BINDING);
    EQ(U(RpcEpResolveBinding)((RPC_BINDING_HANDLE)epx_fake, NULL), EPX_INVALID_BINDING);
    h = epx_mkbind();
    if (h) {   /* a handle with a dynamic... endpoint-less binding is valid input; here the endpoint is set so no mapper is needed */
        t_note("RpcEpResolveBinding(ncalrpc handle with endpoint) = %lu", (unsigned long)U(RpcEpResolveBinding)(h, NULL));
        U(RpcBindingFree)(&h);
    }
}

TEST(epm_status_mgmt_ep_inq_begin_argument_validation) {
    RPC_EP_INQ_HANDLE ctx = (RPC_EP_INQ_HANDLE)(ULONG_PTR)0x55; RPC_IF_ID id; UUID u; unsigned t, v;
    REQUIRE(U(RpcMgmtEpEltInqBegin));
    WINE_STUB();
    memset(&id, 0, sizeof id); memset(&u, 0, sizeof u);
    /* inquiry type: 0 ALL, 1 BY_IF, 2 BY_OBJ, 3 BY_BOTH; anything else is invalid */
    for (t = 4; t < 8; t++) EQ(U(RpcMgmtEpEltInqBegin)(NULL, t, &id, 1, &u, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 0xFFFFFFFFu, &id, 1, &u, &ctx), RPC_S_INVALID_ARG);
    /* BY_OBJ needs an object UUID */
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 2, &id, 1, NULL, &ctx), RPC_S_INVALID_ARG);
    /* BY_IF / BY_BOTH need an interface id and a version option in 1..5 */
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 1, NULL, 1, &u, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 3, NULL, 1, &u, &ctx), RPC_S_INVALID_ARG);
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 3, &id, 1, NULL, &ctx), RPC_S_INVALID_ARG);
    for (v = 0; v < 8; v++) {
        if (v >= 1 && v <= 5) continue;
        EQ(U(RpcMgmtEpEltInqBegin)(NULL, 1, &id, v, &u, &ctx), RPC_S_INVALID_ARG);
        EQ(U(RpcMgmtEpEltInqBegin)(NULL, 3, &id, v, &u, &ctx), RPC_S_INVALID_ARG);
    }
    EXPECT_EQ((ULONG_PTR)ctx, 0x55);     /* never written on failure */
}

TEST(epm_status_mgmt_ep_inq_done_and_next_reject_foreign_context) {
    void *block[16]; RPC_EP_INQ_HANDLE ctx; RPC_IF_ID id; UUID u;
    REQUIRE(U(RpcMgmtEpEltInqDone) && U(RpcMgmtEpEltInqNextA) && U(RpcMgmtEpEltInqNextW));
    WINE_STUB();
    memset(block, 0, sizeof block);
    ctx = (RPC_EP_INQ_HANDLE)block;
    EQ(U(RpcMgmtEpEltInqDone)(&ctx), RPC_S_INVALID_ARG);
    CHECK(ctx == (RPC_EP_INQ_HANDLE)block);        /* not cleared on failure */
    EQ(U(RpcMgmtEpEltInqNextA)(ctx, &id, NULL, &u, NULL), RPC_S_INVALID_ARG);
    EQ(U(RpcMgmtEpEltInqNextW)(ctx, &id, NULL, &u, NULL), RPC_S_INVALID_ARG);
}

static void do_epunreg(void *p) {
    U(RpcMgmtEpUnregister)(NULL, NULL, NULL, NULL);
}
TEST(epm_status_mgmt_ep_unregister_raises_cant_perform_op) {
    REQUIRE(U(RpcMgmtEpUnregister));
    WINE_STUB();
    EQ(epx_try(do_epunreg, NULL), EPX_CANT_PERFORM_OP);    /* the function body is just RpcRaiseException(EPT_S_CANT_PERFORM_OP) */
}
