/* gap_04: RpcServerUseProtseq{Ex,EpEx,IfEx}{A,W}, RpcServerUseAllProtseqs{Ex,IfEx} (ncalrpc / validation only).
 * Registering every protocol sequence is done in dx_gap_04 (child process).
 *
 * From the Win10 decompilation:
 *  - all *Ex entry points call I_RpcServerUseProtseq[Ep]2W/A, which reject Policy->Length < 12 with
 *    RPC_S_INVALID_BOUND (1734) before anything else;
 *  - the A forms convert the protocol sequence; length > 0xFFFF characters -> ERROR_INVALID_PARAMETER;
 *  - IfEx forms walk IfSpec->RpcProtseqEndpoint[]: the protseq must match (case-insensitively) an entry, the
 *    entry's endpoint is registered; no entry -> RPC_S_PROTSEQ_NOT_SUPPORTED (1703);
 *  - RpcServerUseAllProtseqsIfEx with an interface that lists no endpoints -> RPC_S_NO_PROTSEQS (1719). */
#include "rawfx.h"

#define G4_INVALID_BOUND 1734
#define G4_PROTSEQ_NOT_SUPPORTED 1703
#define G4_NO_PROTSEQS 1719
/* Win10: an unknown protocol sequence is reported as RPC_S_PROTSEQ_NOT_SUPPORTED (1703), not RPC_S_INVALID_RPC_PROTSEQ (1704) */
#define G4_INVALID_RPC_PROTSEQ 1703

#define NOWINE(why) do { if (t_is_wine()) { t_skip("%s", why); return; } } while (0)
static RPC_POLICY mkpol(unsigned len) { RPC_POLICY p; memset(&p, 0, sizeof p); p.Length = len; return p; }

/* does the server binding vector contain an ncalrpc binding to endpoint ep? */
static int has_endpoint(const WCHAR *ep) {
    RPC_BINDING_VECTOR *v = NULL; unsigned i; int found = 0; WCHAR needle[96];
    if (U(RpcServerInqBindings)(&v) != RPC_S_OK || !v) return 0;
    wsprintfW(needle, L"[%s]", ep);
    for (i = 0; i < v->Count; i++) {
        RPC_WSTR s = NULL;
        if (U(RpcBindingToStringBindingW)(v->BindingH[i], &s) == RPC_S_OK && s) {
            if (!_wcsnicmp((WCHAR *)s, L"ncalrpc", 7) && wcsstr((WCHAR *)s, needle)) found = 1;
            U(RpcStringFreeW)(&s);
        }
    }
    U(RpcBindingVectorFree)(&v);
    return found;
}
static int binding_count(void) {
    RPC_BINDING_VECTOR *v = NULL; int n = 0;
    if (U(RpcServerInqBindings)(&v) == RPC_S_OK && v) { n = (int)v->Count; U(RpcBindingVectorFree)(&v); }
    return n;
}

/* ---- RpcServerUseProtseqEpEx{W,A} ---------------------------------------------------------------- */
TEST(gap4_protseqepex_w_registers_endpoint) {
    WCHAR ep[64]; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_WSTR)ep, NULL, &p), RPC_S_OK);
    CHECK(has_endpoint(ep));
}
TEST(gap4_protseqepex_a_registers_endpoint) {
    char epa[64]; WCHAR ep[64]; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqEpExA)); NOWINE("RpcServerUseProtseqEpExA may be an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    rx_epa(epa, sizeof epa); MultiByteToWideChar(CP_ACP, 0, epa, -1, ep, 64);
    EQ(U(RpcServerUseProtseqEpExA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)epa, NULL, &p), RPC_S_OK);
    CHECK(has_endpoint(ep));
}
TEST(gap4_protseqepex_policy_length_boundary) {
    static const unsigned bad[] = { 0, 1, 4, 8, 11 };
    static const unsigned good[] = { 12, 13, 16, 64 };
    WCHAR ep[64]; char epa[64]; unsigned k; RPC_POLICY p;
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqEpExA)); NOWINE("RpcServerUseProtseqEpExA may be an unimplemented stub in wine");
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        p = mkpol(bad[k]);
        rx_ep_unique(ep, 64);
        EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL, &p), G4_INVALID_BOUND);
        CHECK(!has_endpoint(ep));
        rx_epa(epa, sizeof epa);
        EQ(U(RpcServerUseProtseqEpExA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)epa, NULL, &p), G4_INVALID_BOUND);
    }
    for (k = 0; k < sizeof good / sizeof *good; k++) {
        RPC_POLICY big[8]; memset(big, 0, sizeof big); big[0].Length = good[k];
        rx_ep_unique(ep, 64);
        EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL, big), RPC_S_OK);
        CHECK(has_endpoint(ep));
    }
}
TEST(gap4_protseqepex_policy_checked_before_protseq) {
    RPC_POLICY p = mkpol(4); WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine");
    rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"bogus_protseq", 10, (RPC_WSTR)ep, NULL, &p), G4_INVALID_BOUND);
}
TEST(gap4_protseqepex_invalid_protseq) {
    static const WCHAR *bad[] = { L"bogus_protseq", L"ncalrpcx", L"ncalr", L"tcp", L"ncacn_" };
    RPC_POLICY p = mkpol(sizeof p); WCHAR ep[64]; char epa[64]; unsigned k;
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqEpExA)); NOWINE("RpcServerUseProtseqEpExA may be an unimplemented stub in wine");
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        char narrow[64]; rx_ep_unique(ep, 64);
        EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)bad[k], 10, (RPC_WSTR)ep, NULL, &p), G4_INVALID_RPC_PROTSEQ);
        CHECK(!has_endpoint(ep));
        WideCharToMultiByte(CP_ACP, 0, bad[k], -1, narrow, sizeof narrow, NULL, NULL);
        rx_epa(epa, sizeof epa);
        EQ(U(RpcServerUseProtseqEpExA)((RPC_CSTR)narrow, 10, (RPC_CSTR)epa, NULL, &p), G4_INVALID_RPC_PROTSEQ);
    }
}
TEST(gap4_protseqepex_a_protseq_length_limit) {
    RPC_POLICY p = mkpol(sizeof p); char *s = malloc(0x10002); char epa[64];
    REQUIRE(U(RpcServerUseProtseqEpExA)); NOWINE("RpcServerUseProtseqEpExA may be an unimplemented stub in wine");
    if (!s) return;
    rx_epa(epa, sizeof epa);
    memset(s, 'a', 0x10001); s[0x10000] = 0;                   /* 0x10000 characters: over the limit */
    EQ(U(RpcServerUseProtseqEpExA)((RPC_CSTR)s, 10, (RPC_CSTR)epa, NULL, &p), ERROR_INVALID_PARAMETER);
    s[0xFFFF] = 0;                                              /* 0xFFFF characters: accepted by the length check, unknown protseq */
    EQ(U(RpcServerUseProtseqEpExA)((RPC_CSTR)s, 10, (RPC_CSTR)epa, NULL, &p), G4_INVALID_RPC_PROTSEQ);
    free(s);
}
TEST(gap4_protseqepex_duplicate_endpoint) {
    RPC_POLICY p = mkpol(sizeof p); WCHAR ep[64]; RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine");
    rx_ep_unique(ep, 64);
    EQ(U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL, &p), RPC_S_OK);
    st = U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL, &p);
    CHECKF(st == RPC_S_OK || st == RPC_S_DUPLICATE_ENDPOINT, "got %lu", (unsigned long)st);
    t_note("second registration of the same ncalrpc endpoint -> %lu", (unsigned long)st);
}
TEST(gap4_protseqepex_flags_ignored_values) {
    /* EndpointFlags / NICFlags are policy hints; zero and the documented bit are both accepted for ncalrpc */
    static const ULONG flags[] = { 0, RPC_C_BIND_TO_ALL_NICS, 0x4 };
    unsigned k; WCHAR ep[64];
    REQUIRE(U(RpcServerUseProtseqEpExW)); NOWINE("RpcServerUseProtseqEpExW may be an unimplemented stub in wine");
    for (k = 0; k < sizeof flags / sizeof *flags; k++) {
        RPC_POLICY p = mkpol(sizeof p); p.EndpointFlags = flags[k]; rx_ep_unique(ep, 64);
        RPC_STATUS st = U(RpcServerUseProtseqEpExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)ep, NULL, &p);
        EQ(st, RPC_S_OK);
        EQ(p.EndpointFlags, flags[k]);                          /* the caller's policy is not modified */
    }
}

/* ---- RpcServerUseProtseqEx{W,A} (dynamic endpoint) ------------------------------------------------- */
TEST(gap4_protseqex_w_dynamic_endpoint) {
    RPC_POLICY p = mkpol(sizeof p); int before, after;
    REQUIRE(U(RpcServerUseProtseqExW)); NOWINE("RpcServerUseProtseqExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    before = binding_count();
    EQ(U(RpcServerUseProtseqExW)((RPC_WSTR)L"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, NULL, &p), RPC_S_OK);
    after = binding_count();
    CHECKF(after >= 1 && after >= before, "bindings %d -> %d", before, after);
}
TEST(gap4_protseqex_a_dynamic_endpoint) {
    RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqExA)); NOWINE("RpcServerUseProtseqExA may be an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    EQ(U(RpcServerUseProtseqExA)((RPC_CSTR)"ncalrpc", RPC_C_PROTSEQ_MAX_REQS_DEFAULT, NULL, &p), RPC_S_OK);
    CHECK(binding_count() >= 1);
}
TEST(gap4_protseqex_policy_length_boundary) {
    static const unsigned bad[] = { 0, 3, 8, 11 };
    unsigned k;
    REQUIRE(U(RpcServerUseProtseqExW)); NOWINE("RpcServerUseProtseqExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqExA)); NOWINE("RpcServerUseProtseqExA may be an unimplemented stub in wine");
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        RPC_POLICY p = mkpol(bad[k]);
        EQ(U(RpcServerUseProtseqExW)((RPC_WSTR)L"ncalrpc", 10, NULL, &p), G4_INVALID_BOUND);
        EQ(U(RpcServerUseProtseqExA)((RPC_CSTR)"ncalrpc", 10, NULL, &p), G4_INVALID_BOUND);
    }
    { RPC_POLICY p = mkpol(12); EQ(U(RpcServerUseProtseqExW)((RPC_WSTR)L"ncalrpc", 10, NULL, &p), RPC_S_OK); }
}
TEST(gap4_protseqex_invalid_protseq) {
    RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqExW)); NOWINE("RpcServerUseProtseqExW may be an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqExA)); NOWINE("RpcServerUseProtseqExA may be an unimplemented stub in wine");
    EQ(U(RpcServerUseProtseqExW)((RPC_WSTR)L"bogus_protseq", 10, NULL, &p), G4_INVALID_RPC_PROTSEQ);
    EQ(U(RpcServerUseProtseqExA)((RPC_CSTR)"bogus_protseq", 10, NULL, &p), G4_INVALID_RPC_PROTSEQ);
}
TEST(gap4_protseqex_a_protseq_length_limit) {
    RPC_POLICY p = mkpol(sizeof p); char *s = malloc(0x10002);
    REQUIRE(U(RpcServerUseProtseqExA)); NOWINE("RpcServerUseProtseqExA may be an unimplemented stub in wine");
    if (!s) return;
    memset(s, 'a', 0x10001); s[0x10000] = 0;
    EQ(U(RpcServerUseProtseqExA)((RPC_CSTR)s, 10, NULL, &p), ERROR_INVALID_PARAMETER);
    free(s);
}

/* ---- RpcServerUseProtseqIfEx{W,A} -------------------------------------------------------------------- */
typedef struct { rx_raw r; RPC_PROTSEQ_ENDPOINT pe[3]; char ep[3][80]; } ifx;
static void ifx_init(ifx *f, unsigned n, unsigned count, const char *const *protseqs) {
    UUID u; unsigned i;
    memset(f, 0, sizeof *f);
    rx_raw_uuid(&u, n); rx_raw_init(&f->r, &u, 1, 0);
    for (i = 0; i < count; i++) {
        char epa[64]; rx_epa(epa, sizeof epa);
        lstrcpyA(f->ep[i], epa);
        f->pe[i].RpcProtocolSequence = (unsigned char *)protseqs[i];
        f->pe[i].Endpoint = (unsigned char *)f->ep[i];
    }
    f->r.si.RpcProtseqEndpointCount = count;
    f->r.si.RpcProtseqEndpoint = count ? f->pe : NULL;
}
static int ifx_ep_registered(ifx *f, unsigned i) { WCHAR w[80]; MultiByteToWideChar(CP_ACP, 0, f->ep[i], -1, w, 80); return has_endpoint(w); }

TEST(gap4_protseqifex_w_matching_entry) {
    static const char *const ps[] = { "ncalrpc", "ncacn_np" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    ifx_init(&f, 0xC1, 1, ps);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));                    /* the endpoint named in the interface is the one registered */
}
TEST(gap4_protseqifex_a_matching_entry) {
    static const char *const ps[] = { "ncalrpc" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExA)); NOWINE("RpcServerUseProtseqIfExA is an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    ifx_init(&f, 0xC2, 1, ps);
    EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
}
TEST(gap4_protseqifex_case_insensitive) {
    static const char *const ps[] = { "ncalrpc" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqIfExA)); NOWINE("RpcServerUseProtseqIfExA is an unimplemented stub in wine");
    ifx_init(&f, 0xC3, 1, ps);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"NCALRPC", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
    ifx_init(&f, 0xC4, 1, ps);
    EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"NcAlRpC", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
}
TEST(gap4_protseqifex_no_matching_entry) {
    static const char *const ps[] = { "ncalrpc" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqIfExA)); NOWINE("RpcServerUseProtseqIfExA is an unimplemented stub in wine");
    ifx_init(&f, 0xC5, 1, ps);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncacn_ip_tcp", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"ncacn_ip_tcp", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"bogus", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_PROTSEQ_NOT_SUPPORTED);
    CHECK(!ifx_ep_registered(&f, 0));                   /* nothing was registered */
}
TEST(gap4_protseqifex_interface_without_endpoints) {
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqIfExA)); NOWINE("RpcServerUseProtseqIfExA is an unimplemented stub in wine");
    ifx_init(&f, 0xC6, 0, NULL);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_PROTSEQ_NOT_SUPPORTED);
    EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_PROTSEQ_NOT_SUPPORTED);
}
TEST(gap4_protseqifex_policy_length_boundary) {
    static const char *const ps[] = { "ncalrpc" };
    static const unsigned bad[] = { 0, 4, 8, 11 };
    ifx f; unsigned k;
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine"); REQUIRE(U(RpcServerUseProtseqIfExA)); NOWINE("RpcServerUseProtseqIfExA is an unimplemented stub in wine");
    ifx_init(&f, 0xC7, 1, ps);
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        RPC_POLICY p = mkpol(bad[k]);
        EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_INVALID_BOUND);
        EQ(U(RpcServerUseProtseqIfExA)((RPC_CSTR)"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_INVALID_BOUND);
    }
    CHECK(!ifx_ep_registered(&f, 0));
}
TEST(gap4_protseqifex_multiple_entries_same_protseq) {
    static const char *const ps[] = { "ncalrpc", "ncalrpc", "ncacn_np" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine");
    ifx_init(&f, 0xC8, 3, ps);
    EQ(U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncalrpc", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
    CHECK(ifx_ep_registered(&f, 1));                    /* every matching entry is registered */
}
TEST(gap4_protseqifex_partial_prefix_quirk) {
    /* The Win10 matcher compares only strlen(Protseq) characters (wcsnicmp), so a prefix of an interface protseq matches. */
    static const char *const ps[] = { "ncalrpc" };
    ifx f; RPC_POLICY p = mkpol(sizeof p); RPC_STATUS st;
    REQUIRE(U(RpcServerUseProtseqIfExW)); NOWINE("RpcServerUseProtseqIfExW is an unimplemented stub in wine");
    ifx_init(&f, 0xC9, 1, ps);
    st = U(RpcServerUseProtseqIfExW)((RPC_WSTR)L"ncal", 10, (RPC_IF_HANDLE)&f.r.si, NULL, &p);
    t_note("UseProtseqIfExW(\"ncal\") on an ncalrpc interface -> %lu (registered: %d)", (unsigned long)st, st == 0 ? ifx_ep_registered(&f, 0) : 0);
}

/* ---- RpcServerUseAllProtseqs{Ex,IfEx} -- validation and ncalrpc-only interfaces ----------------------- */
TEST(gap4_allprotseqsex_policy_too_short) {
    static const unsigned bad[] = { 0, 4, 8, 11 };
    unsigned k;
    REQUIRE(U(RpcServerUseAllProtseqsEx)); NOWINE("RpcServerUseAllProtseqsEx is an unimplemented stub in wine");
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        RPC_POLICY p = mkpol(bad[k]);
        EQ(U(RpcServerUseAllProtseqsEx)(10, NULL, &p), G4_INVALID_BOUND);     /* checked before any protocol is touched */
    }
}
TEST(gap4_allprotseqsifex_policy_too_short) {
    static const char *const ps[] = { "ncalrpc" };
    static const unsigned bad[] = { 0, 4, 8, 11 };
    ifx f; unsigned k;
    REQUIRE(U(RpcServerUseAllProtseqsIfEx)); NOWINE("RpcServerUseAllProtseqsIfEx is an unimplemented stub in wine");
    ifx_init(&f, 0xCA, 1, ps);
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        RPC_POLICY p = mkpol(bad[k]);
        EQ(U(RpcServerUseAllProtseqsIfEx)(10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_INVALID_BOUND);
    }
    CHECK(!ifx_ep_registered(&f, 0));
}
TEST(gap4_allprotseqsifex_no_endpoints_in_interface) {
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseAllProtseqsIfEx)); NOWINE("RpcServerUseAllProtseqsIfEx is an unimplemented stub in wine");
    ifx_init(&f, 0xCB, 0, NULL);
    EQ(U(RpcServerUseAllProtseqsIfEx)(10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), G4_NO_PROTSEQS);
}
TEST(gap4_allprotseqsifex_registers_interface_endpoints) {
    static const char *const ps[] = { "ncalrpc", "ncalrpc" };
    ifx f; RPC_POLICY p = mkpol(sizeof p);
    REQUIRE(U(RpcServerUseAllProtseqsIfEx)); NOWINE("RpcServerUseAllProtseqsIfEx is an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    ifx_init(&f, 0xCC, 2, ps);
    EQ(U(RpcServerUseAllProtseqsIfEx)(10, (RPC_IF_HANDLE)&f.r.si, NULL, &p), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
    CHECK(ifx_ep_registered(&f, 1));                    /* all endpoints of the interface, whatever the protseq */
    CHECKF((p.EndpointFlags & 4) != 0, "EndpointFlags 0x%lx", (unsigned long)p.EndpointFlags);   /* the runtime leaves bit 4 set in the caller's policy */
}
TEST(gap4_allprotseqsifex_wrapper_without_policy) {
    static const char *const ps[] = { "ncalrpc" };
    ifx f;
    REQUIRE(U(RpcServerUseAllProtseqsIf)); NOWINE("RpcServerUseAllProtseqsIf is an unimplemented stub in wine"); REQUIRE(U(RpcServerInqBindings));
    ifx_init(&f, 0xCD, 1, ps);
    EQ(U(RpcServerUseAllProtseqsIf)(10, (RPC_IF_HANDLE)&f.r.si, NULL), RPC_S_OK);
    CHECK(ifx_ep_registered(&f, 0));
}
