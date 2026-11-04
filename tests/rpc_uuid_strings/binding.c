/* RPC binding handles built from string bindings: RpcBindingFromStringBinding / ToStringBinding, Copy, Free, Reset,
 * SetObject / InqObject, RpcMgmtSet/InqComTimeout, RpcBindingSet/InqOption, RpcBindingVectorFree.
 *
 * Decomp facts (rpcrt4 19041): invalid or NULL handle -> RPC_S_INVALID_BINDING (1702); RpcBindingInqObject(NULL) with no
 * call in progress -> RPC_S_NO_CALL_ACTIVE (1725); RpcMgmtSetComTimeout accepts 0..10 only (RPC_S_INVALID_TIMEOUT 1709),
 * a fresh handle has timeout 5; RpcBindingSetOption(DONT_LINGER, 0) -> ERROR_INVALID_PARAMETER whatever the handle; options above
 * 0x10 (except 0x12 for Set) -> ERROR_INVALID_PARAMETER. RpcBindingFromStringBinding: only the literal "ncalrpc" (case sensitive) and
 * protseqs starting with the case-sensitive prefix "ncacn_" are bound, anything else is RPC_S_INVALID_RPC_PROTSEQ (1704); ncalrpc with a
 * network address other than the local computer name -> RPC_S_INVALID_NET_ADDR (1707). The out handle is NULLed on every failure. */
#include "rus.h"
#include "modules.h"

static char g_ep[64];
static const char *ep_name(int n)
{
    sprintf(g_ep, "%s_rus_%lu_%d", T_TMP_PREFIX, (unsigned long)GetCurrentProcessId(), n);
    return g_ep;
}

static RPC_BINDING_HANDLE bind_w(const char *s, RPC_STATUS *st_out)
{
    __typeof__(&RpcBindingFromStringBindingW) f = U(RpcBindingFromStringBindingW);
    __typeof__(&RpcBindingFree) fb = U(RpcBindingFree);
    WCHAR w[512]; RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)0x1;
    rus_widen(w, 512, s);
    RPC_STATUS st = f(w, &h);
    if (st_out) *st_out = st;
    if (st != RPC_S_OK) {
        /* a lenient implementation may hand back a handle for input that must fail: do not leak it, report the status */
        if (h && h != (RPC_BINDING_HANDLE)(ULONG_PTR)0x1) fb(&h);
        return NULL;
    }
    return h;
}

static void tostr(RPC_BINDING_HANDLE h, char *dst, size_t cap, RPC_STATUS *st_out)
{
    __typeof__(&RpcBindingToStringBindingW) f = U(RpcBindingToStringBindingW);
    __typeof__(&RpcStringFreeW) fr = U(RpcStringFreeW);
    RPC_WSTR s = NULL;
    RPC_STATUS st = f(h, &s);
    if (st_out) *st_out = st;
    dst[0] = 0;
    if (st == RPC_S_OK && s) {
        size_t i = 0;
        for (; s[i] && i + 1 < cap; i++) dst[i] = (char)s[i];
        dst[i] = 0;
        fr(&s);
    }
}

/* ---------------------------------------------------------- round trips */

struct rt { const char *in, *out; const char *why; };
static const struct rt g_rt[] = {
    { "ncalrpc:[ep]", "ncalrpc:[ep]", "ncalrpc" },
    { "ncalrpc:", "ncalrpc:", "ncalrpc, no endpoint" },
    { "ncalrpc:[endpoint=ep]", "ncalrpc:[ep]", "endpoint= form is canonicalised" },
    { "8A885D04-1CEB-11C9-9FE8-08002B104860@ncalrpc:[ep]", "8a885d04-1ceb-11c9-9fe8-08002b104860@ncalrpc:[ep]", "uuid lowercased" },
    { "00000000-0000-0000-0000-000000000000@ncalrpc:[ep]", "ncalrpc:[ep]", "nil uuid dropped" },
    { "ncacn_ip_tcp:127.0.0.1[135]", "ncacn_ip_tcp:127.0.0.1[135]", "tcp" },
    { "ncacn_ip_tcp:host", "ncacn_ip_tcp:host", "tcp no endpoint" },
    { "ncacn_ip_tcp:[135]", "ncacn_ip_tcp:[135]", "tcp no address" },
    { "ncacn_ip_tcp:host[endpoint=135]", "ncacn_ip_tcp:host[135]", "tcp endpoint= form" },
    { "ncacn_np:.[\\\\pipe\\\\x]", "ncacn_np:.[\\\\pipe\\\\x]", "named pipe, backslashes survive the round trip" },
    { "ncacn_np:\\\\\\\\srv[\\\\pipe\\\\lsarpc]", "ncacn_np:\\\\\\\\srv[\\\\pipe\\\\lsarpc]", "named pipe unc" },
    { "ncacn_http:proxy.example.com[80]", "ncacn_http:proxy.example.com[80]", "http" },
    { "00000000-0000-0000-c000-000000000046@ncacn_ip_tcp:h[1]", "00000000-0000-0000-c000-000000000046@ncacn_ip_tcp:h[1]", "uuid + tcp" },
    { "ncacn_ip_tcp:a\\:b[1]", "ncacn_ip_tcp:a\\:b[1]", "escaped colon in address survives" },
};

TEST(rus_binding_string_roundtrip_table)
{
    NEED(RpcBindingFromStringBindingW); NEED(RpcBindingToStringBindingW); NEED(RpcStringFreeW); NEED(RpcBindingFree);
    for (int i = 0; i < NSTR(g_rt); i++) {
        RPC_STATUS st = 99;
        RPC_BINDING_HANDLE h = bind_w(g_rt[i].in, &st);
        CHECKF(st == RPC_S_OK && h, "[%d] %s: from-string status %lu", i, g_rt[i].why, (unsigned long)st);
        if (!h) continue;
        char out[512];
        tostr(h, out, sizeof out, &st);
        CHECKF(st == RPC_S_OK, "[%d] %s: to-string status %lu", i, g_rt[i].why, (unsigned long)st);
        CHECKF(!strcmp(out, g_rt[i].out), "[%d] %s: got \"%s\" want \"%s\"", i, g_rt[i].why, out, g_rt[i].out);
        EXPECT_EQ(p_RpcBindingFree(&h), RPC_S_OK);
        CHECK(h == NULL);
    }
}

TEST(rus_binding_string_roundtrip_table_a)
{
    NEED(RpcBindingFromStringBindingA); NEED(RpcBindingToStringBindingA); NEED(RpcStringFreeA); NEED(RpcBindingFree);
    for (int i = 0; i < NSTR(g_rt); i++) {
        RPC_BINDING_HANDLE h = NULL;
        RPC_STATUS st = p_RpcBindingFromStringBindingA((unsigned char *)g_rt[i].in, &h);
        CHECKF(st == RPC_S_OK && h, "[%d] %s: from-string status %lu", i, g_rt[i].why, (unsigned long)st);
        if (!h) continue;
        unsigned char *s = NULL;
        st = p_RpcBindingToStringBindingA(h, &s);
        CHECKF(st == RPC_S_OK && s, "[%d] to-string status %lu", i, (unsigned long)st);
        if (s) CHECKF(!strcmp((char *)s, g_rt[i].out), "[%d] %s: got \"%s\" want \"%s\"", i, g_rt[i].why, (char *)s, g_rt[i].out);
        p_RpcStringFreeA(&s);
        EXPECT_EQ(p_RpcBindingFree(&h), RPC_S_OK);
    }
}

TEST(rus_binding_fromstring_unique_endpoint_roundtrip)
{
    NEED(RpcStringBindingComposeW); NEED(RpcBindingFromStringBindingW); NEED(RpcBindingToStringBindingW); NEED(RpcStringFreeW); NEED(RpcBindingFree);
    WCHAR ep[64]; rus_widen(ep, 64, ep_name(1));
    RPC_WSTR sb = NULL;
    EXPECT_EQ(p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, ep, NULL, &sb), RPC_S_OK);
    RPC_BINDING_HANDLE h = NULL;
    EXPECT_EQ(p_RpcBindingFromStringBindingW(sb, &h), RPC_S_OK);
    RPC_WSTR back = NULL;
    EXPECT_EQ(p_RpcBindingToStringBindingW(h, &back), RPC_S_OK);
    CHECK(sb && back && !lstrcmpW(sb, back));
    p_RpcStringFreeW(&sb); p_RpcStringFreeW(&back);
    EXPECT_EQ(p_RpcBindingFree(&h), RPC_S_OK);
}

TEST(rus_binding_ncalrpc_local_computer_name_accepted)
{
    NEED(RpcBindingFromStringBindingW); NEED(RpcBindingFree);
    WCHAR name[MAX_COMPUTERNAME_LENGTH + 2]; DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(name, &n)) { t_skip("GetComputerNameW failed"); return; }
    WCHAR sb[128];
    wsprintfW(sb, L"ncalrpc:%ls[ep]", name);
    RPC_BINDING_HANDLE h = NULL;
    RPC_STATUS st = p_RpcBindingFromStringBindingW(sb, &h);
    CHECKF(st == RPC_S_OK, "status %lu", (unsigned long)st);
    if (h) p_RpcBindingFree(&h);
}

/* ------------------------------------------------------- from-string errors */

struct er { const char *in; RPC_STATUS st; const char *why; };
static const struct er g_er[] = {
    { "", RUS_INVALID_STRING_BINDING, "empty" },
    { "ncalrpc", RUS_INVALID_STRING_BINDING, "no colon" },
    { "ncalrpc:[ep", RUS_INVALID_ENDPOINT_FORMAT, "unterminated endpoint" },
    { "ncalrpc:[ep,o", RUS_INVALID_STRING_BINDING, "unterminated options" },
    { "xx@ncalrpc:[ep]", RUS_INVALID_STRING_UUID, "bad uuid" },
    { "{8a885d04-1ceb-11c9-9fe8-08002b104860}@ncalrpc:[ep]", RUS_INVALID_STRING_UUID, "braced uuid" },
    { "foo:[x]", RUS_INVALID_RPC_PROTSEQ, "unknown protseq" },
    { ":[x]", RUS_INVALID_RPC_PROTSEQ, "empty protseq" },
    { "NCALRPC:[x]", RUS_INVALID_RPC_PROTSEQ, "ncalrpc is case sensitive" },
    { "Ncalrpc:[x]", RUS_INVALID_RPC_PROTSEQ, "ncalrpc is case sensitive 2" },
    { "ncalrpcx:[x]", RUS_INVALID_RPC_PROTSEQ, "ncalrpc must match exactly" },
    { "ncalrp:[x]", RUS_INVALID_RPC_PROTSEQ, "prefix of ncalrpc" },
    { "NCACN_IP_TCP:h[1]", RUS_INVALID_RPC_PROTSEQ, "ncacn_ prefix is case sensitive" },
    { "ncacn:h[1]", RUS_INVALID_RPC_PROTSEQ, "ncacn without underscore" },
    { "ncadg_ip_udp:h[1]", RUS_INVALID_RPC_PROTSEQ, "datagram protseq is not bindable" },
    { "mswmsg:[x]", RUS_INVALID_RPC_PROTSEQ, "mswmsg is not bindable via string binding" },
    { "http:h[80]", RUS_INVALID_RPC_PROTSEQ, "http alone" },
    { "ncalrpc:bogus-host-olet-nonexistent[ep]", RUS_INVALID_NET_ADDR, "ncalrpc with a remote network address" },
    { "ncalrpc:127.0.0.1[ep]", RUS_INVALID_NET_ADDR, "ncalrpc with an ip address" },
    { "ncalrpc:localhost[ep]", RUS_INVALID_NET_ADDR, "ncalrpc with the name localhost" },
};

TEST(rus_binding_fromstring_errors_w)
{
    NEED(RpcBindingFromStringBindingW);
    for (int i = 0; i < NSTR(g_er); i++) {
        RPC_STATUS st = 0;
        RPC_BINDING_HANDLE h = bind_w(g_er[i].in, &st);
        CHECKF(st == g_er[i].st, "[%d] \"%s\" (%s): got %lu want %lu", i, g_er[i].in, g_er[i].why, (unsigned long)st, (unsigned long)g_er[i].st);
        CHECK(h == NULL);
    }
}

TEST(rus_binding_fromstring_errors_a)
{
    NEED(RpcBindingFromStringBindingA);
    for (int i = 0; i < NSTR(g_er); i++) {
        RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)0x1;
        RPC_STATUS st = p_RpcBindingFromStringBindingA((unsigned char *)g_er[i].in, &h);
        CHECKF(st == g_er[i].st, "[%d] \"%s\" (%s): got %lu want %lu", i, g_er[i].in, g_er[i].why, (unsigned long)st, (unsigned long)g_er[i].st);
        CHECKF(h == NULL, "[%d] handle not cleared on failure", i);
    }
}

TEST(rus_binding_fromstring_failure_clears_handle)
{
    NEED(RpcBindingFromStringBindingW);
    RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)0xdeadbeef;
    EXPECT_EQ(p_RpcBindingFromStringBindingW((RPC_WSTR)L"nonsense", &h), RUS_INVALID_STRING_BINDING);
    CHECK(h == NULL);
}

TEST(rus_binding_fromstring_too_long)
{
    NEED(RpcBindingFromStringBindingW);
    /* decomp: length >= 0x8000 characters -> ERROR_OUTOFMEMORY (not INVALID_PARAMETER as for RpcStringBindingParse) */
    static WCHAR big[0x8002];
    for (int i = 0; i < 0x8001; i++) big[i] = L'a';
    big[0x8001] = 0;
    RPC_BINDING_HANDLE h = (RPC_BINDING_HANDLE)(ULONG_PTR)1;
    EXPECT_EQ(p_RpcBindingFromStringBindingW(big, &h), RUS_OUTOFMEMORY);
    CHECK(h == NULL);
}

TEST(rus_binding_unknown_ncacn_protseq_note)
{
    NEED(RpcBindingFromStringBindingW); NEED(RpcBindingFree);
    /* routed through the loadable transport lookup; the exact status depends on the transport registry */
    RPC_STATUS st = 0;
    RPC_BINDING_HANDLE h = bind_w("ncacn_olet_unknown:host[1]", &st);
    t_note("ncacn_olet_unknown: status %lu", (unsigned long)st);
    CHECK(st != RPC_S_OK);
    CHECK(h == NULL);
    if (h) p_RpcBindingFree(&h);
}

/* ------------------------------------------------------------ object uuid */

TEST(rus_binding_object_from_string)
{
    NEED(RpcBindingInqObject); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("8A885D04-1CEB-11C9-9FE8-08002B104860@ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID u; memset(&u, 0xee, sizeof u);
    EXPECT_EQ(p_RpcBindingInqObject(h, &u), RPC_S_OK);
    UUID want = { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60} };
    CHECK(rus_uuid_eq(&u, &want));
    p_RpcBindingFree(&h);
}

TEST(rus_binding_object_default_is_nil)
{
    NEED(RpcBindingInqObject); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID u; memset(&u, 0xee, sizeof u);
    EXPECT_EQ(p_RpcBindingInqObject(h, &u), RPC_S_OK);
    unsigned char zero[16] = {0};
    CHECK(memcmp(&u, zero, 16) == 0);
    p_RpcBindingFree(&h);
}

TEST(rus_binding_set_inq_object)
{
    NEED(RpcBindingSetObject); NEED(RpcBindingInqObject); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID a = { 0x01020304, 0x0506, 0x0708, {9,10,11,12,13,14,15,16} }, got;
    EXPECT_EQ(p_RpcBindingSetObject(h, &a), RPC_S_OK);
    memset(&got, 0, sizeof got);
    EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
    CHECK(rus_uuid_eq(&a, &got));
    /* overwrite */
    UUID b = { 0xffffffff, 0xffff, 0xffff, {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe} };
    EXPECT_EQ(p_RpcBindingSetObject(h, &b), RPC_S_OK);
    EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
    CHECK(rus_uuid_eq(&b, &got));
    p_RpcBindingFree(&h);
}

TEST(rus_binding_set_object_nil_and_null_clear)
{
    NEED(RpcBindingSetObject); NEED(RpcBindingInqObject); NEED(RpcBindingToStringBindingW); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID a = { 0x01020304, 0x0506, 0x0708, {9,10,11,12,13,14,15,16} }, got, nil = {0};
    unsigned char zero[16] = {0};
    EXPECT_EQ(p_RpcBindingSetObject(h, &a), RPC_S_OK);
    EXPECT_EQ(p_RpcBindingSetObject(h, &nil), RPC_S_OK);
    memset(&got, 0xaa, sizeof got);
    EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
    CHECK(memcmp(&got, zero, 16) == 0);
    EXPECT_EQ(p_RpcBindingSetObject(h, &a), RPC_S_OK);
    EXPECT_EQ(p_RpcBindingSetObject(h, NULL), RPC_S_OK); /* decomp: NULL clears the object uuid */
    memset(&got, 0xaa, sizeof got);
    EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
    CHECK(memcmp(&got, zero, 16) == 0);
    char s[256]; tostr(h, s, sizeof s, NULL);
    EXPECT_STR(s, "ncalrpc:[ep]");
    p_RpcBindingFree(&h);
}

TEST(rus_binding_set_object_changes_string)
{
    NEED(RpcBindingSetObject); NEED(RpcBindingToStringBindingW); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncacn_ip_tcp:host[135]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID a = { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60} };
    EXPECT_EQ(p_RpcBindingSetObject(h, &a), RPC_S_OK);
    char s[256]; tostr(h, s, sizeof s, NULL);
    EXPECT_STR(s, "8a885d04-1ceb-11c9-9fe8-08002b104860@ncacn_ip_tcp:host[135]");
    EXPECT_EQ(p_RpcBindingSetObject(h, NULL), RPC_S_OK);
    tostr(h, s, sizeof s, NULL);
    EXPECT_STR(s, "ncacn_ip_tcp:host[135]");
    p_RpcBindingFree(&h);
}

/* -------------------------------------------------------------- copy / free */

TEST(rus_binding_copy_is_independent)
{
    NEED(RpcBindingCopy); NEED(RpcBindingSetObject); NEED(RpcBindingInqObject); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncacn_ip_tcp:host[135]", NULL), c = NULL;
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(p_RpcBindingCopy(h, &c), RPC_S_OK);
    CHECK(c != NULL && c != h);
    if (c) {
        char a[256], b[256];
        tostr(h, a, sizeof a, NULL); tostr(c, b, sizeof b, NULL);
        EXPECT_STR(b, a);
        UUID u = { 1, 2, 3, {4,5,6,7,8,9,10,11} }, got, nil = {0};
        EXPECT_EQ(p_RpcBindingSetObject(c, &u), RPC_S_OK);
        EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
        CHECK(rus_uuid_eq(&got, &nil));   /* original untouched */
        EXPECT_EQ(p_RpcBindingInqObject(c, &got), RPC_S_OK);
        CHECK(rus_uuid_eq(&got, &u));
        EXPECT_EQ(p_RpcBindingFree(&c), RPC_S_OK);
        CHECK(c == NULL);
        tostr(h, a, sizeof a, NULL);              /* original still valid after the copy is freed */
        EXPECT_STR(a, "ncacn_ip_tcp:host[135]");
    }
    EXPECT_EQ(p_RpcBindingFree(&h), RPC_S_OK);
}

TEST(rus_binding_copy_ncalrpc_matches)
{
    NEED(RpcBindingCopy); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("8a885d04-1ceb-11c9-9fe8-08002b104860@ncalrpc:[ep]", NULL), c = NULL;
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(p_RpcBindingCopy(h, &c), RPC_S_OK);
    if (c) {
        char a[256], b[256];
        tostr(h, a, sizeof a, NULL); tostr(c, b, sizeof b, NULL);
        EXPECT_STR(b, a);
        p_RpcBindingFree(&c);
    }
    p_RpcBindingFree(&h);
}

TEST(rus_binding_free_clears_and_second_free_fails)
{
    NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(p_RpcBindingFree(&h), RPC_S_OK);
    CHECK(h == NULL);
    EXPECT_EQ(p_RpcBindingFree(&h), RUS_INVALID_BINDING); /* handle is now NULL */
    CHECK(h == NULL);
}

TEST(rus_binding_many_create_free_cycles)
{
    NEED(RpcBindingFree);
    for (int i = 0; i < 300; i++) {
        RPC_BINDING_HANDLE h = bind_w(i & 1 ? "ncalrpc:[ep]" : "ncacn_ip_tcp:host[135]", NULL);
        if (!h) { CHECK(0); break; }
        if (p_RpcBindingFree(&h) != RPC_S_OK || h != NULL) { CHECK(0); break; }
    }
    CHECK(1);
}

/* ----------------------------------------------------------- invalid handle */

TEST(rus_binding_null_handle_errors)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingToStringBindingW); NEED(RpcBindingToStringBindingA); NEED(RpcBindingSetObject); NEED(RpcBindingInqObject);
    NEED(RpcBindingReset); NEED(RpcMgmtSetComTimeout); NEED(RpcMgmtInqComTimeout); NEED(RpcBindingInqOption);
    NEED(RpcBindingSetOption); NEED(RpcBindingFree);
    RPC_WSTR s = (RPC_WSTR)(ULONG_PTR)0x77;
    EXPECT_EQ(p_RpcBindingToStringBindingW(NULL, &s), RUS_INVALID_BINDING);
    CHECK(s == (RPC_WSTR)(ULONG_PTR)0x77);
    unsigned char *as = (unsigned char *)(ULONG_PTR)0x77;
    EXPECT_EQ(p_RpcBindingToStringBindingA(NULL, &as), RUS_INVALID_BINDING);
    UUID u = {1}, got; memset(&got, 0x5a, sizeof got);
    EXPECT_EQ(p_RpcBindingSetObject(NULL, &u), RUS_INVALID_BINDING);
    /* no call is in progress on this thread */
    EXPECT_EQ(p_RpcBindingInqObject(NULL, &got), RUS_NO_CALL_ACTIVE);
    { unsigned char same[16]; memset(same, 0x5a, 16); CHECK(memcmp(&got, same, 16) == 0); }
    EXPECT_EQ(p_RpcBindingReset(NULL), RUS_INVALID_BINDING);
    EXPECT_EQ(p_RpcMgmtSetComTimeout(NULL, 5), RUS_INVALID_BINDING);
    unsigned t = 0x55;
    EXPECT_EQ(p_RpcMgmtInqComTimeout(NULL, &t), RUS_INVALID_BINDING);
    CHECK(t == 0x55);
    ULONG_PTR v = 0x55;
    EXPECT_EQ(p_RpcBindingInqOption(NULL, 1, &v), RUS_INVALID_BINDING);
    /* Win10: a NULL handle with an option other than DONT_LINGER(0) is not rejected up front; it is handed to the
     * context-handle lookup, which reads the handle without a NULL check and faults (access violation).
     * Only the DONT_LINGER/NULL answer is stable, see rus_binding_setoption_dont_linger_zero_checked_first. */
    t_note("RpcBindingSetOption(NULL, option!=DONT_LINGER) faults on Win10 (no NULL check); not called");
    RPC_BINDING_HANDLE n = NULL;
    EXPECT_EQ(p_RpcBindingFree(&n), RUS_INVALID_BINDING);
}

TEST(rus_binding_setoption_dont_linger_zero_checked_first)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingSetOption); NEED(RpcBindingFree);
    /* option 13 (RPC_C_OPT_DONT_LINGER) with value 0 is rejected before the handle is even looked at */
    EXPECT_EQ(p_RpcBindingSetOption(NULL, RPC_C_OPT_DONT_LINGER, 0), RUS_INVALID_PARAMETER);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(p_RpcBindingSetOption(h, RPC_C_OPT_DONT_LINGER, 0), RUS_INVALID_PARAMETER);
    p_RpcBindingFree(&h);
}

/* ----------------------------------------------------------------- timeouts */

TEST(rus_binding_com_timeout_default_and_range)
{
    if (t_is_wine()) { t_skip("wine lacks RpcMgmtInqComTimeout"); return; }
    NEED(RpcMgmtSetComTimeout); NEED(RpcMgmtInqComTimeout); NEED(RpcBindingFree);
    const char *forms[] = { "ncalrpc:[ep]", "ncacn_ip_tcp:host[135]", "ncacn_np:.[\\\\pipe\\\\x]" };
    for (int f = 0; f < NSTR(forms); f++) {
        RPC_BINDING_HANDLE h = bind_w(forms[f], NULL);
        CHECKF(h != NULL, "%s", forms[f]);
        if (!h) continue;
        unsigned t = 0xdead;
        EXPECT_EQ(p_RpcMgmtInqComTimeout(h, &t), RPC_S_OK);
        EXPECT_EQ(t, RPC_C_BINDING_DEFAULT_TIMEOUT);
        for (unsigned v = 0; v <= 10; v++) {
            EXPECT_EQ(p_RpcMgmtSetComTimeout(h, v), RPC_S_OK);
            t = 0xdead;
            EXPECT_EQ(p_RpcMgmtInqComTimeout(h, &t), RPC_S_OK);
            EXPECT_EQ(t, v);
        }
        EXPECT_EQ(t, RPC_C_BINDING_INFINITE_TIMEOUT);
        unsigned bad[] = { 11, 12, 100, 0x7fffffff, 0x80000000, 0xffffffff };
        for (int b = 0; b < NSTR(bad); b++) {
            EXPECT_EQ(p_RpcMgmtSetComTimeout(h, bad[b]), RUS_INVALID_TIMEOUT);
            t = 0xdead;
            EXPECT_EQ(p_RpcMgmtInqComTimeout(h, &t), RPC_S_OK);
            EXPECT_EQ(t, 10); /* unchanged by the failed calls */
        }
        p_RpcBindingFree(&h);
    }
}

TEST(rus_binding_com_timeout_per_handle)
{
    if (t_is_wine()) { t_skip("wine lacks RpcMgmtInqComTimeout"); return; }
    NEED(RpcMgmtSetComTimeout); NEED(RpcMgmtInqComTimeout); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE a = bind_w("ncalrpc:[ep]", NULL), b = bind_w("ncalrpc:[ep]", NULL);
    CHECK(a && b);
    if (a && b) {
        EXPECT_EQ(p_RpcMgmtSetComTimeout(a, 2), RPC_S_OK);
        unsigned t = 0;
        EXPECT_EQ(p_RpcMgmtInqComTimeout(b, &t), RPC_S_OK);
        EXPECT_EQ(t, RPC_C_BINDING_DEFAULT_TIMEOUT);
        EXPECT_EQ(p_RpcMgmtInqComTimeout(a, &t), RPC_S_OK);
        EXPECT_EQ(t, 2);
    }
    if (a) p_RpcBindingFree(&a);
    if (b) p_RpcBindingFree(&b);
}

TEST(rus_binding_com_timeout_symbolic_values)
{
    EXPECT_EQ(RPC_C_BINDING_MIN_TIMEOUT, 0);
    EXPECT_EQ(RPC_C_BINDING_DEFAULT_TIMEOUT, 5);
    EXPECT_EQ(RPC_C_BINDING_MAX_TIMEOUT, 9);
    EXPECT_EQ(RPC_C_BINDING_INFINITE_TIMEOUT, 10);
}

/* ------------------------------------------------------------------ options */

TEST(rus_binding_options_out_of_range)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingSetOption); NEED(RpcBindingInqOption); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    ULONG_PTR v = 0x1234;
    /* Inq: anything above 0x10 (including 0x12) is ERROR_INVALID_PARAMETER and leaves the output alone */
    ULONG bad_inq[] = { 0x11, 0x12, 0x13, 0x20, 0x100, 0xffffffff };
    for (int i = 0; i < NSTR(bad_inq); i++) {
        v = 0x1234;
        EXPECT_EQ(p_RpcBindingInqOption(h, bad_inq[i], &v), RUS_INVALID_PARAMETER);
        EXPECT_EQ(v, 0x1234);
    }
    /* Set: 0x11, 0x13 and above are invalid for this transport; 0x12 is a valid slot */
    ULONG bad_set[] = { 0x11, 0x13, 0x20, 0x100, 0xffffffff };
    for (int i = 0; i < NSTR(bad_set); i++)
        EXPECT_EQ(p_RpcBindingSetOption(h, bad_set[i], 1), RUS_INVALID_PARAMETER);
    p_RpcBindingFree(&h);
}

TEST(rus_binding_options_dont_linger)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingSetOption); NEED(RpcBindingInqOption); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    ULONG_PTR v = 0xff;
    EXPECT_EQ(p_RpcBindingInqOption(h, RPC_C_OPT_DONT_LINGER, &v), RPC_S_OK);
    EXPECT_EQ(v, 0);
    EXPECT_EQ(p_RpcBindingSetOption(h, RPC_C_OPT_DONT_LINGER, 1), RPC_S_OK);
    v = 0;
    EXPECT_EQ(p_RpcBindingInqOption(h, RPC_C_OPT_DONT_LINGER, &v), RPC_S_OK);
    CHECK(v != 0);
    t_note("DONT_LINGER inquiry value after Set(1): 0x%llx", (unsigned long long)v);
    p_RpcBindingFree(&h);
}

TEST(rus_binding_options_unset_inquire_zero)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingInqOption); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    for (ULONG o = 0; o <= 0x10; o++) {
        ULONG_PTR v = 0x5555;
        RPC_STATUS st = p_RpcBindingInqOption(h, o, &v);
        CHECKF(st == RPC_S_OK, "option %lu status %lu", (unsigned long)o, (unsigned long)st);
        CHECKF(v == 0, "option %lu value 0x%llx", (unsigned long)o, (unsigned long long)v);
    }
    p_RpcBindingFree(&h);
}

TEST(rus_binding_options_set_inq_roundtrip)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcBindingSetOption); NEED(RpcBindingInqOption); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncalrpc:[ep]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    ULONG_PTR v = 0;
    EXPECT_EQ(p_RpcBindingSetOption(h, RPC_C_OPT_MQ_JOURNAL, 0x1234), RPC_S_OK);
    EXPECT_EQ(p_RpcBindingInqOption(h, RPC_C_OPT_MQ_JOURNAL, &v), RPC_S_OK);
    t_note("option 3 round trip value 0x%llx (expected 0x1234)", (unsigned long long)v);
    CHECK(v == 0x1234);
    p_RpcBindingFree(&h);
}

/* -------------------------------------------------------------- vector free */

TEST(rus_binding_server_bindings_vector)
{
    if (t_is_wine()) { t_skip("wine lacks RpcBindingInqOption / crashes here"); return; }
    NEED(RpcServerUseProtseqEpW); NEED(RpcServerInqBindings); NEED(RpcBindingVectorFree); NEED(RpcBindingToStringBindingW);
    NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    WCHAR ep[64]; rus_widen(ep, 64, ep_name(2));
    RPC_STATUS st = p_RpcServerUseProtseqEpW((RPC_WSTR)L"ncalrpc", 1, ep, NULL);
    if (st != RPC_S_OK) { t_skip("RpcServerUseProtseqEpW failed: %lu", (unsigned long)st); return; }
    RPC_BINDING_VECTOR *vec = NULL;
    EXPECT_EQ(p_RpcServerInqBindings(&vec), RPC_S_OK);
    CHECK(vec != NULL);
    if (!vec) return;
    CHECK(vec->Count >= 1);
    int found = 0;
    for (ULONG i = 0; i < vec->Count; i++) {
        RPC_WSTR sb = NULL;
        if (p_RpcBindingToStringBindingW(vec->BindingH[i], &sb) != RPC_S_OK || !sb) continue;
        RPC_WSTR proto = NULL, e = NULL;
        if (p_RpcStringBindingParseW(sb, NULL, &proto, NULL, &e, NULL) == RPC_S_OK) {
            if (proto && e && rus_wide_eq_ascii(proto, "ncalrpc") && !lstrcmpiW(e, ep)) found++;
            p_RpcStringFreeW(&proto); p_RpcStringFreeW(&e);
        }
        p_RpcStringFreeW(&sb);
    }
    EXPECT_EQ(found, 1);
    EXPECT_EQ(p_RpcBindingVectorFree(&vec), RPC_S_OK);
    CHECK(vec == NULL);
}

/* ------------------------------------------------------------ DceErrorInqText */

static void ref_msg_w(DWORD code, WCHAR *buf)
{
    if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, buf, 256, NULL))
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 1823, 0, buf, 256, NULL);
}
static void ref_msg_a(DWORD code, char *buf)
{
    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, code, 0, buf, 256, NULL))
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 1823, 0, buf, 256, NULL);
}

static const DWORD g_codes[] = { 0, 1, 2, 5, 6, 8, 14, 87, 122, 1700, 1701, 1702, 1703, 1704, 1705, 1706, 1707, 1708, 1709, 1710, 1711, 1712, 1713,
    1721, 1722, 1723, 1725, 1726, 1727, 1728, 1730, 1731, 1732, 1740, 1741, 1745, 1750, 1760, 1764, 1766, 1772, 1780, 1783, 1818, 1819,
    1820, 1821, 1822, 1823, 1824, 1825, 1900, 0x80070005, 0x80004005, 0xc0000005, 0xc0020001, 35, 0x7fffffff, 0xdeadbeef, 0xffffffff };

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+): the text is whatever FormatMessage returns, with a fixed fallback message */
TEST(rus_dceerror_inqtext_w_matches_formatmessage)
{
    NEED(DceErrorInqTextW);
    for (int i = 0; i < NSTR(g_codes); i++) {
        WCHAR got[300], want[300];
        for (int k = 0; k < 300; k++) got[k] = 0xabab;
        ref_msg_w(g_codes[i], want);
        RPC_STATUS st = p_DceErrorInqTextW(g_codes[i], got);
        CHECKF(st == RPC_S_OK, "code 0x%lx status %lu", (unsigned long)g_codes[i], (unsigned long)st);
        CHECKF(!lstrcmpW(got, want), "code 0x%lx text differs from FormatMessage (%ls)", (unsigned long)g_codes[i], got);
        int sentinel_ok = 1;
        for (int k = 256; k < 300; k++) if (got[k] != 0xabab) sentinel_ok = 0;
        CHECKF(sentinel_ok, "code 0x%lx wrote past 256 characters", (unsigned long)g_codes[i]);
    }
}

TEST(rus_dceerror_inqtext_a_matches_formatmessage)
{
    NEED(DceErrorInqTextA);
    for (int i = 0; i < NSTR(g_codes); i++) {
        char got[300], want[300];
        memset(got, 0xab, sizeof got);
        ref_msg_a(g_codes[i], want);
        RPC_STATUS st = p_DceErrorInqTextA(g_codes[i], (unsigned char *)got);
        CHECKF(st == RPC_S_OK, "code 0x%lx status %lu", (unsigned long)g_codes[i], (unsigned long)st);
        CHECKF(!strcmp(got, want), "code 0x%lx text differs from FormatMessage (%s)", (unsigned long)g_codes[i], got);
        int sentinel_ok = 1;
        for (int k = 256; k < 300; k++) if ((unsigned char)got[k] != 0xab) sentinel_ok = 0;
        CHECKF(sentinel_ok, "code 0x%lx wrote past 256 characters", (unsigned long)g_codes[i]);
    }
}

TEST(rus_dceerror_inqtext_known_codes_nonempty)
{
    NEED(DceErrorInqTextA); NEED(DceErrorInqTextW);
    /* RPC_S_* errors 1700..1783 all have system messages */
    for (DWORD c = 1700; c <= 1730; c++) {
        char a[300]; WCHAR w[300];
        EXPECT_EQ(p_DceErrorInqTextA(c, (unsigned char *)a), RPC_S_OK);
        EXPECT_EQ(p_DceErrorInqTextW(c, w), RPC_S_OK);
        CHECKF(a[0] != 0, "code %lu empty (A)", (unsigned long)c);
        CHECKF(w[0] != 0, "code %lu empty (W)", (unsigned long)c);
        CHECKF(lstrlenW(w) == (int)strlen(a) || (unsigned char)a[0] >= 0x80, "code %lu A/W lengths differ", (unsigned long)c);
    }
}

TEST(rus_dceerror_inqtext_unknown_code_uses_fallback_message)
{
    NEED(DceErrorInqTextW);
    /* a code with no system message yields the message for 1823 instead of an error */
    WCHAR fb[300], got[300];
    if (!FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, 1823, 0, fb, 256, NULL)) { t_skip("no message for 1823"); return; }
    DWORD unknown[] = { 0x7ffffffe, 0xfffffff0, 0x0badf00d };
    for (int i = 0; i < NSTR(unknown); i++) {
        WCHAR own[300];
        if (FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, unknown[i], 0, own, 256, NULL)) continue;
        EXPECT_EQ(p_DceErrorInqTextW(unknown[i], got), RPC_S_OK);
        CHECK(!lstrcmpW(got, fb));
    }
}

/* ------------------------------------------------------------ I_RpcMapWin32Status */

/* status -> NTSTATUS pairs read from the 107 entry table (StatusMap) of the 19041 rpcrt4 */
static const struct { LONG in; ULONG out; } g_map[] = {
{0,0x00000000U}, {1700,0xC0020001U}, {1701,0xC0020002U}, {1702,0xC0020003U}, {1703,0xC0020004U}, {1704,0xC0020005U}, {1705,0xC0020006U}, {1706,0xC0020007U}, {1707,0xC0020008U}, {1708,0xC0020009U}, {1709,0xC002000AU}, {1710,0xC002000BU}, {1711,0xC002000CU}, {1712,0xC002000DU}, {1713,0xC002000EU}, {1714,0xC002000FU}, {1715,0xC0020010U}, {1716,0xC0020011U}, {1717,0xC0020012U}, {1718,0xC0020013U}, {1806,0xC002004AU}, {1719,0xC0020014U}, {1720,0xC0020015U}, {1721,0xC0020016U}, {1722,0xC0020017U}, {1723,0xC0020018U}, {1724,0xC0020019U}, {1725,0xC002001AU}, {1726,0xC002001BU}, {1818,0xC0020050U}, {1727,0xC002001CU}, {1728,0xC002001DU}, {1730,0xC002001FU}, {1130,0xC0000205U}, {1732,0xC0020021U}, {1733,0xC0020022U}, {1734,0xC0020023U}, {1735,0xC0020024U}, {1736,0xC0020025U}, {1737,0xC0020026U}, {1739,0xC0020028U}, {1740,0xC0020029U}, {1741,0xC002002AU}, {1742,0xC002002BU}, {1743,0xC002002CU}, {1744,0xC002002DU}, {1745,0xC002002EU}, {1746,0xC002002FU}, {1747,0xC0020030U}, {1748,0xC0020031U}, {1749,0xC0020032U}, {1750,0xC0020033U}, {1751,0xC0020034U}, {1752,0xC0020035U}, {1753,0xC0020036U}, {1754,0xC0020037U}, {1755,0xC0020038U}, {1756,0xC0020039U}, {1757,0xC002003AU}, {1758,0xC002003BU}, {1759,0xC002003CU}, {1760,0xC002003DU}, {1761,0xC002003EU}, {1762,0xC002003FU}, {1763,0xC0020040U}, {1764,0xC0020041U}, {1765,0xC0020042U}, {1766,0xC0020043U}, {1767,0xC0020044U}, {1768,0xC0020045U}, {1769,0xC0020046U}, {1770,0xC0020047U}, {1771,0xC0020048U}, {1772,0xC0030001U}, {1773,0xC0030002U}, {1774,0xC0030003U}, {1775,0xC0030004U}, {6,0xC0030005U}, {1777,0xC0030006U}, {1778,0xC0030007U}, {1779,0xC0030008U}, {1780,0xC0030009U}, {1781,0xC003000AU}, {1782,0xC003000BU}, {1783,0xC003000CU}, {87,0xC000000DU}, {14,0xC0000017U}, {164,0xC0000017U}, {122,0xC0000023U}, {1338,0xC0000079U}, {5,0xC0000022U}, {998,0xC0000005U}, {1791,0xC0020049U}, {1898,0xC002004BU}, {1899,0xC002004CU}, {1900,0xC002004DU}, {1914,0xC0020062U}, {1915,0xC0020063U}, {1916,0xC003005FU}, {1918,0xC0030061U}, {1917,0xC0030060U}, {1907,0xC0000224U}, {1330,0xC0000071U}, {1331,0xC0000072U}, {1328,0xC000006FU}, {1909,0xC0000234U}, {1323,0xC000006AU}
};

TEST(rus_rpcmapwin32status_full_table)
{
    NEED(I_RpcMapWin32Status);
    for (int i = 0; i < NSTR(g_map); i++) {
        ULONG got = (ULONG)p_I_RpcMapWin32Status(g_map[i].in);
        CHECKF(got == g_map[i].out, "status %ld: got 0x%08lx want 0x%08lx", (long)g_map[i].in, (unsigned long)got, (unsigned long)g_map[i].out);
    }
}

TEST(rus_rpcmapwin32status_unmapped_positive_is_unsuccessful)
{
    NEED(I_RpcMapWin32Status);
    int bad = 0;
    for (LONG s = 0; s < 4000; s++) {
        int mapped = 0;
        for (int i = 0; i < NSTR(g_map); i++) if (g_map[i].in == s) mapped = 1;
        if (mapped) continue;
        if ((ULONG)p_I_RpcMapWin32Status(s) != 0xC0000001UL) bad++;
    }
    EXPECT_EQ(bad, 0);
    LONG big[] = { 4000, 9999, 10000, 12345, 0x10000, 0x7fffffff };
    for (int i = 0; i < NSTR(big); i++) EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(big[i]), 0xC0000001UL);
}

TEST(rus_rpcmapwin32status_negative_passes_through)
{
    NEED(I_RpcMapWin32Status);
    LONG neg[] = { -1, -2, (LONG)0x80070005, (LONG)0xC0000022, (LONG)0x80004005, (LONG)0x80000000, (LONG)0xC0020001, -100000 };
    for (int i = 0; i < NSTR(neg); i++)
        CHECKF(p_I_RpcMapWin32Status(neg[i]) == neg[i], "0x%lx", (unsigned long)neg[i]);
}

TEST(rus_rpcmapwin32status_rpc_range_is_contiguous_nt_range)
{
    NEED(I_RpcMapWin32Status);
    /* RPC_S_* 1700..1718 map to RPC_NT_* 0xC0020001.. in order */
    for (LONG s = 1700; s <= 1718; s++)
        EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(s), 0xC0020001UL + (ULONG)(s - 1700));
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(RPC_S_OK), 0);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_ACCESS_DENIED), 0xC0000022UL);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_OUTOFMEMORY), 0xC0000017UL);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_INVALID_PARAMETER), 0xC000000DUL);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_INSUFFICIENT_BUFFER), 0xC0000023UL);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_NOACCESS), 0xC0000005UL);
    EXPECT_EQ((ULONG)p_I_RpcMapWin32Status(ERROR_INVALID_HANDLE), 0xC0030005UL);
}

/* ----------------------------------------------------------------- exports */

TEST(rus_rpcrt4_exports_present)
{
    HMODULE m = t_module("rpcrt4.dll");
    CHECK(m != NULL);
    if (!m) return;
    const char *names[] = {
        "UuidCreate", "UuidCreateSequential", "UuidCreateNil", "UuidIsNil", "UuidEqual", "UuidCompare", "UuidHash",
        "UuidToStringA", "UuidToStringW", "UuidFromStringA", "UuidFromStringW", "RpcStringFreeA", "RpcStringFreeW",
        "RpcStringBindingComposeA", "RpcStringBindingComposeW", "RpcStringBindingParseA", "RpcStringBindingParseW",
        "RpcBindingFromStringBindingA", "RpcBindingFromStringBindingW", "RpcBindingToStringBindingA", "RpcBindingToStringBindingW",
        "RpcBindingCopy", "RpcBindingFree", "RpcBindingReset", "RpcBindingSetObject", "RpcBindingInqObject", "RpcBindingVectorFree",
        "RpcNetworkIsProtseqValidA", "RpcNetworkIsProtseqValidW", "RpcNetworkInqProtseqsA", "RpcNetworkInqProtseqsW",
        "RpcProtseqVectorFreeA", "RpcProtseqVectorFreeW", "DceErrorInqTextA", "DceErrorInqTextW",
        "RpcMgmtInqComTimeout", "RpcMgmtSetComTimeout", "RpcBindingSetOption", "RpcBindingInqOption",
        "RpcCertGeneratePrincipalNameA", "RpcCertGeneratePrincipalNameW", "RpcCertMatchPrincipalName", "I_RpcMapWin32Status",
    };
    for (int i = 0; i < NSTR(names); i++)
        CHECKF(GetProcAddress(m, names[i]) != NULL, "missing export %s", names[i]);
}

TEST(rus_rpcrt4_unsuffixed_names_not_exported)
{
    HMODULE m = t_module("rpcrt4.dll");
    CHECK(m != NULL);
    if (!m) return;
    /* only the A and W spellings exist; the generic names are macros in the SDK headers */
    const char *names[] = {
        "UuidToString", "UuidFromString", "RpcStringFree", "RpcStringBindingCompose", "RpcStringBindingParse",
        "RpcBindingFromStringBinding", "RpcBindingToStringBinding", "RpcNetworkIsProtseqValid", "RpcNetworkInqProtseqs",
        "RpcProtseqVectorFree", "DceErrorInqText", "RpcCertGeneratePrincipalName", "UuidCreateSequentialA",
    };
    for (int i = 0; i < NSTR(names); i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, names[i]);
        CHECKF(p == NULL, "%s unexpectedly exported", names[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}

/* ---------------------------------------------------------- more behaviour */

static DWORD WINAPI inqobj_thread(LPVOID p)
{
    __typeof__(&RpcBindingInqObject) f = U(RpcBindingInqObject);
    UUID u; memset(&u, 0, sizeof u);
    return (DWORD)f(NULL, &u);
}

TEST(rus_binding_inqobject_null_in_worker_thread_no_call)
{
    if (t_is_wine()) { t_skip("wine dereferences the thread's binding"); return; }
    NEED(RpcBindingInqObject);
    HANDLE th = CreateThread(NULL, 0, inqobj_thread, NULL, 0, NULL);
    DWORD w = WaitForSingleObject(th, 10000), code = 0;
    EXPECT_EQ(w, WAIT_OBJECT_0);
    if (w == WAIT_OBJECT_0) { GetExitCodeThread(th, &code); EXPECT_EQ(code, RUS_NO_CALL_ACTIVE); }
    CloseHandle(th);
}

TEST(rus_binding_tostring_a_matches_w)
{
    NEED(RpcBindingToStringBindingA); NEED(RpcBindingToStringBindingW); NEED(RpcStringFreeA); NEED(RpcStringFreeW); NEED(RpcBindingFree);
    for (int i = 0; i < NSTR(g_rt); i++) {
        RPC_BINDING_HANDLE h = bind_w(g_rt[i].in, NULL);
        if (!h) { CHECK(0); continue; }
        unsigned char *a = NULL; RPC_WSTR w = NULL;
        EXPECT_EQ(p_RpcBindingToStringBindingA(h, &a), RPC_S_OK);
        EXPECT_EQ(p_RpcBindingToStringBindingW(h, &w), RPC_S_OK);
        CHECKF(a && w && rus_wide_eq_ascii(w, (char *)a), "[%d] A/W differ", i);
        p_RpcStringFreeA(&a); p_RpcStringFreeW(&w);
        p_RpcBindingFree(&h);
    }
}

TEST(rus_binding_reset_observed)
{
    NEED(RpcBindingReset); NEED(RpcBindingFree);
    const char *forms[] = { "ncalrpc:[ep]", "ncacn_ip_tcp:host[135]" };
    for (int i = 0; i < NSTR(forms); i++) {
        RPC_BINDING_HANDLE h = bind_w(forms[i], NULL);
        CHECK(h != NULL);
        if (!h) continue;
        RPC_STATUS st = p_RpcBindingReset(h);
        char s[256]; tostr(h, s, sizeof s, NULL);
        t_note("RpcBindingReset(\"%s\") = %lu, string afterwards \"%s\"", forms[i], (unsigned long)st, s);
        CHECKF(st == RPC_S_OK || st == RUS_WRONG_KIND_OF_BINDING, "status %lu", (unsigned long)st);
        p_RpcBindingFree(&h);
    }
}

TEST(rus_binding_reset_keeps_object_uuid_observed)
{
    NEED(RpcBindingReset); NEED(RpcBindingSetObject); NEED(RpcBindingInqObject); NEED(RpcBindingFree);
    RPC_BINDING_HANDLE h = bind_w("ncacn_ip_tcp:host[135]", NULL);
    CHECK(h != NULL);
    if (!h) return;
    UUID u = { 9, 8, 7, {6,5,4,3,2,1,0,1} }, got;
    EXPECT_EQ(p_RpcBindingSetObject(h, &u), RPC_S_OK);
    RPC_STATUS st = p_RpcBindingReset(h);
    memset(&got, 0, sizeof got);
    EXPECT_EQ(p_RpcBindingInqObject(h, &got), RPC_S_OK);
    t_note("after RpcBindingReset (status %lu) object uuid kept: %d", (unsigned long)st, rus_uuid_eq(&u, &got));
    p_RpcBindingFree(&h);
}

TEST(rus_binding_copy_preserves_timeout_observed)
{
    NEED(RpcBindingCopy); NEED(RpcMgmtSetComTimeout); NEED(RpcMgmtInqComTimeout); NEED(RpcBindingFree);
    if (t_is_wine()) { t_skip("wine lacks RpcMgmtInqComTimeout"); return; }
    RPC_BINDING_HANDLE h = bind_w("ncacn_ip_tcp:host[135]", NULL), c = NULL;
    CHECK(h != NULL);
    if (!h) return;
    EXPECT_EQ(p_RpcMgmtSetComTimeout(h, 8), RPC_S_OK);
    EXPECT_EQ(p_RpcBindingCopy(h, &c), RPC_S_OK);
    unsigned t = 0xdead;
    if (c) { EXPECT_EQ(p_RpcMgmtInqComTimeout(c, &t), RPC_S_OK); t_note("timeout on copy of a handle with timeout 8: %u", t); p_RpcBindingFree(&c); }
    p_RpcBindingFree(&h);
}
