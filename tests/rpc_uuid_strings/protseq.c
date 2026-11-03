/* RpcNetworkIsProtseqValid, RpcNetworkInqProtseqs, RpcProtseqVectorFree (A and W).
 *
 * Decomp of IsRpcProtocolSequenceSupported (rpcrt4 19041):
 *   - the literal "ncalrpc" (case sensitive, exact) -> RPC_S_OK
 *   - a string of more than 5 characters starting with the case-sensitive prefix "ncacn_" -> looked up case-insensitively in the
 *     list returned by RpcNetworkInqProtseqs: found -> RPC_S_OK, else RPC_S_PROTSEQ_NOT_SUPPORTED (1703)
 *   - exactly "mswmsg" -> RPC_S_PROTSEQ_NOT_SUPPORTED (1703)
 *   - everything else, including the empty string -> RPC_S_INVALID_RPC_PROTSEQ (1704)
 *   RpcNetworkIsProtseqValidA rejects strings longer than 0xffff characters with ERROR_INVALID_PARAMETER.
 * RpcNetworkInqProtseqs returns {ncacn_np, ncacn_ip_tcp, ncacn_http, ncalrpc} (static table, one more entry is hidden by flag). */
#include "rus.h"

struct vrow { const char *s; RPC_STATUS st; const char *why; };

static const struct vrow g_valid[] = {
    { "ncalrpc",       RPC_S_OK, "local rpc" },
    { "ncacn_np",      RPC_S_OK, "named pipes" },
    { "ncacn_ip_tcp",  RPC_S_OK, "tcp" },
    { "ncacn_http",    RPC_S_OK, "http" },
    { "ncacn_NP",      RPC_S_OK, "suffix compared case-insensitively" },
    { "ncacn_IP_TCP",  RPC_S_OK, "suffix compared case-insensitively 2" },
    { "ncacn_Http",    RPC_S_OK, "suffix compared case-insensitively 3" },
    { "NCACN_NP",      RUS_INVALID_RPC_PROTSEQ, "prefix match is case sensitive" },
    { "Ncacn_np",      RUS_INVALID_RPC_PROTSEQ, "prefix match is case sensitive 2" },
    { "NCALRPC",       RUS_INVALID_RPC_PROTSEQ, "ncalrpc is case sensitive" },
    { "Ncalrpc",       RUS_INVALID_RPC_PROTSEQ, "ncalrpc is case sensitive 2" },
    { "ncalrpcx",      RUS_INVALID_RPC_PROTSEQ, "ncalrpc must match exactly (longer)" },
    { "ncalrp",        RUS_INVALID_RPC_PROTSEQ, "ncalrpc prefix" },
    { "ncalrpc ",      RUS_INVALID_RPC_PROTSEQ, "trailing space" },
    { " ncalrpc",      RUS_INVALID_RPC_PROTSEQ, "leading space" },
    { "ncacn_np ",     RUS_PROTSEQ_NOT_SUPPORTED, "ncacn_ prefix but not in the list" },
    { "ncacn_",        RUS_PROTSEQ_NOT_SUPPORTED, "bare ncacn_ prefix" },
    { "ncacn_x",       RUS_PROTSEQ_NOT_SUPPORTED, "unknown ncacn_ protocol" },
    { "ncacn_nb_tcp",  RUS_PROTSEQ_NOT_SUPPORTED, "netbios over tcp is not offered" },
    { "ncacn_nb_nb",   RUS_PROTSEQ_NOT_SUPPORTED, "netbeui is not offered" },
    { "ncacn_spx",     RUS_PROTSEQ_NOT_SUPPORTED, "spx is not offered" },
    { "ncacn_at_dsp",  RUS_PROTSEQ_NOT_SUPPORTED, "appletalk is not offered" },
    { "ncacn_vns_spp", RUS_PROTSEQ_NOT_SUPPORTED, "banyan is not offered" },
    { "ncacn_ip_tcp1", RUS_PROTSEQ_NOT_SUPPORTED, "list lookup is exact" },
    { "ncacn_ip_tc",   RUS_PROTSEQ_NOT_SUPPORTED, "list lookup is exact 2" },
    { "mswmsg",        RUS_PROTSEQ_NOT_SUPPORTED, "recognised but unsupported" },
    { "MSWMSG",        RUS_INVALID_RPC_PROTSEQ, "mswmsg is case sensitive" },
    { "mswmsg1",       RUS_INVALID_RPC_PROTSEQ, "mswmsg must match exactly" },
    { "ncacn",         RUS_INVALID_RPC_PROTSEQ, "too short for the ncacn_ prefix" },
    { "ncacn_",        RUS_PROTSEQ_NOT_SUPPORTED, "six chars with prefix" },
    { "ncadg_ip_udp",  RUS_INVALID_RPC_PROTSEQ, "datagram protseqs are not recognised" },
    { "ncadg_ipx",     RUS_INVALID_RPC_PROTSEQ, "datagram ipx" },
    { "ncadg_mq",      RUS_INVALID_RPC_PROTSEQ, "message queue" },
    { "ncacn_http_",   RUS_PROTSEQ_NOT_SUPPORTED, "ncacn_http_ with extra char" },
    { "http",          RUS_INVALID_RPC_PROTSEQ, "http alone" },
    { "tcp",           RUS_INVALID_RPC_PROTSEQ, "tcp" },
    { "foo",           RUS_INVALID_RPC_PROTSEQ, "unknown" },
    { "",              RUS_INVALID_RPC_PROTSEQ, "empty" },
    { "n",             RUS_INVALID_RPC_PROTSEQ, "one char" },
    { "ncacn_np\\",    RUS_PROTSEQ_NOT_SUPPORTED, "stray backslash" },
    { "ncalrpc:",      RUS_INVALID_RPC_PROTSEQ, "colon suffix" },
};

/* adapted from the MS documentation of RpcNetworkIsProtseqValid; no Wine counterpart exists (LGPL not needed) */
TEST(rus_protseq_valid_w_table)
{
    NEED(RpcNetworkIsProtseqValidW);
    for (int i = 0; i < NSTR(g_valid); i++) {
        WCHAR w[64]; rus_widen(w, 64, g_valid[i].s);
        RPC_STATUS st = p_RpcNetworkIsProtseqValidW(w);
        CHECKF(st == g_valid[i].st, "[%d] \"%s\" (%s): got %lu want %lu", i, g_valid[i].s, g_valid[i].why, (unsigned long)st, (unsigned long)g_valid[i].st);
    }
}

TEST(rus_protseq_valid_a_table)
{
    NEED(RpcNetworkIsProtseqValidA);
    for (int i = 0; i < NSTR(g_valid); i++) {
        RPC_STATUS st = p_RpcNetworkIsProtseqValidA((unsigned char *)g_valid[i].s);
        CHECKF(st == g_valid[i].st, "[%d] \"%s\" (%s): got %lu want %lu", i, g_valid[i].s, g_valid[i].why, (unsigned long)st, (unsigned long)g_valid[i].st);
    }
}

TEST(rus_protseq_valid_a_too_long)
{
    NEED(RpcNetworkIsProtseqValidA);
    static char big[0x10003];
    memset(big, 'a', 0x10001); big[0x10001] = 0;
    EXPECT_EQ(p_RpcNetworkIsProtseqValidA((unsigned char *)big), RUS_INVALID_PARAMETER);
    big[0xffff] = 0; /* exactly 0xffff characters is accepted and judged on content */
    EXPECT_EQ(p_RpcNetworkIsProtseqValidA((unsigned char *)big), RUS_INVALID_RPC_PROTSEQ);
    memcpy(big, "ncacn_", 6); /* long ncacn_ string */
    EXPECT_EQ(p_RpcNetworkIsProtseqValidA((unsigned char *)big), RUS_PROTSEQ_NOT_SUPPORTED);
}

TEST(rus_protseq_valid_w_long_non_matching)
{
    NEED(RpcNetworkIsProtseqValidW);
    static WCHAR big[4097];
    for (int i = 0; i < 4096; i++) big[i] = L'z';
    big[4096] = 0;
    EXPECT_EQ(p_RpcNetworkIsProtseqValidW(big), RUS_INVALID_RPC_PROTSEQ);
}

static int vec_find_w(RPC_PROTSEQ_VECTORW *v, const char *name)
{
    for (unsigned i = 0; i < v->Count; i++) if (v->Protseq[i] && rus_wide_eq_ascii(v->Protseq[i], name)) return (int)i;
    return -1;
}

TEST(rus_protseq_inq_w)
{
    NEED(RpcNetworkInqProtseqsW); NEED(RpcProtseqVectorFreeW); NEED(RpcNetworkIsProtseqValidW);
    RPC_PROTSEQ_VECTORW *v = NULL;
    EXPECT_EQ(p_RpcNetworkInqProtseqsW(&v), RPC_S_OK);
    CHECK(v != NULL);
    if (!v) return;
    CHECKF(v->Count >= 4, "count %u", v->Count);
    int np = vec_find_w(v, "ncacn_np"), tcp = vec_find_w(v, "ncacn_ip_tcp"), http = vec_find_w(v, "ncacn_http"), lrpc = vec_find_w(v, "ncalrpc");
    CHECK(np >= 0 && tcp >= 0 && lrpc >= 0);
    if (http < 0) t_note("ncacn_http not listed");
    /* static table order */
    if (!(np < tcp && tcp < http && http < lrpc)) t_note("protseq order differs from np,tcp,http,lrpc");
    for (unsigned i = 0; i < v->Count; i++) {
        CHECKF(v->Protseq[i] != NULL, "entry %u NULL", i);
        if (v->Protseq[i]) {
            EXPECT_EQ(p_RpcNetworkIsProtseqValidW(v->Protseq[i]), RPC_S_OK);
            t_note("protseq[%u] = %ls", i, v->Protseq[i]);
        }
    }
    t_note("RpcNetworkInqProtseqsW count = %u", v->Count);
    EXPECT_EQ(p_RpcProtseqVectorFreeW(&v), RPC_S_OK);
    CHECK(v == NULL);
}

TEST(rus_protseq_inq_a)
{
    NEED(RpcNetworkInqProtseqsA); NEED(RpcProtseqVectorFreeA); NEED(RpcNetworkIsProtseqValidA);
    RPC_PROTSEQ_VECTORA *v = NULL;
    EXPECT_EQ(p_RpcNetworkInqProtseqsA(&v), RPC_S_OK);
    CHECK(v != NULL);
    if (!v) return;
    CHECKF(v->Count >= 4, "count %u", v->Count);
    int seen = 0;
    for (unsigned i = 0; i < v->Count; i++) {
        CHECK(v->Protseq[i] != NULL);
        if (!v->Protseq[i]) continue;
        EXPECT_EQ(p_RpcNetworkIsProtseqValidA(v->Protseq[i]), RPC_S_OK);
        if (!strcmp((char *)v->Protseq[i], "ncacn_np") || !strcmp((char *)v->Protseq[i], "ncacn_ip_tcp") ||
            !strcmp((char *)v->Protseq[i], "ncacn_http") || !strcmp((char *)v->Protseq[i], "ncalrpc")) seen++;
    }
    EXPECT_EQ(seen, 4);
    EXPECT_EQ(p_RpcProtseqVectorFreeA(&v), RPC_S_OK);
    CHECK(v == NULL);
}

TEST(rus_protseq_inq_a_matches_w)
{
    NEED(RpcNetworkInqProtseqsA); NEED(RpcProtseqVectorFreeA); NEED(RpcNetworkInqProtseqsW); NEED(RpcProtseqVectorFreeW);
    RPC_PROTSEQ_VECTORA *a = NULL; RPC_PROTSEQ_VECTORW *w = NULL;
    EXPECT_EQ(p_RpcNetworkInqProtseqsA(&a), RPC_S_OK);
    EXPECT_EQ(p_RpcNetworkInqProtseqsW(&w), RPC_S_OK);
    if (a && w) {
        EXPECT_EQ(a->Count, w->Count);
        for (unsigned i = 0; i < a->Count && i < w->Count; i++)
            CHECKF(rus_wide_eq_ascii(w->Protseq[i], (char *)a->Protseq[i]), "entry %u differs", i);
    }
    p_RpcProtseqVectorFreeA(&a); p_RpcProtseqVectorFreeW(&w);
}

TEST(rus_protseq_inq_repeatable)
{
    NEED(RpcNetworkInqProtseqsW); NEED(RpcProtseqVectorFreeW);
    unsigned first = 0;
    for (int i = 0; i < 100; i++) {
        RPC_PROTSEQ_VECTORW *v = NULL;
        if (p_RpcNetworkInqProtseqsW(&v) != RPC_S_OK || !v) { CHECK(0); break; }
        if (i == 0) first = v->Count; else EXPECT_EQ(v->Count, first);
        p_RpcProtseqVectorFreeW(&v);
    }
}

TEST(rus_protseq_vector_free_null_vector_is_ok)
{
    NEED(RpcProtseqVectorFreeW); NEED(RpcProtseqVectorFreeA);
    /* decomp: *ProtseqVector == NULL -> success, nothing to free */
    RPC_PROTSEQ_VECTORW *w = NULL;
    EXPECT_EQ(p_RpcProtseqVectorFreeW(&w), RPC_S_OK);
    CHECK(w == NULL);
    RPC_PROTSEQ_VECTORA *a = NULL;
    EXPECT_EQ(p_RpcProtseqVectorFreeA(&a), RPC_S_OK);
    CHECK(a == NULL);
}

TEST(rus_protseq_every_listed_protseq_is_valid_both_flavours)
{
    NEED(RpcNetworkInqProtseqsW); NEED(RpcProtseqVectorFreeW); NEED(RpcNetworkIsProtseqValidA); NEED(RpcNetworkIsProtseqValidW);
    RPC_PROTSEQ_VECTORW *v = NULL;
    if (p_RpcNetworkInqProtseqsW(&v) != RPC_S_OK || !v) { CHECK(0); return; }
    for (unsigned i = 0; i < v->Count; i++) {
        char narrow[64]; int k = 0;
        for (; v->Protseq[i][k] && k < 63; k++) narrow[k] = (char)v->Protseq[i][k];
        narrow[k] = 0;
        EXPECT_EQ(p_RpcNetworkIsProtseqValidA((unsigned char *)narrow), RPC_S_OK);
        EXPECT_EQ(p_RpcNetworkIsProtseqValidW(v->Protseq[i]), RPC_S_OK);
    }
    p_RpcProtseqVectorFreeW(&v);
}

TEST(rus_protseq_hvsocket_is_not_listed)
{
    NEED(RpcNetworkIsProtseqValidW); NEED(RpcNetworkInqProtseqsW); NEED(RpcProtseqVectorFreeW);
    /* ncacn_hvsocket is present in the static table but flagged out: not returned by inquiry, hence PROTSEQ_NOT_SUPPORTED */
    RPC_PROTSEQ_VECTORW *v = NULL;
    if (p_RpcNetworkInqProtseqsW(&v) != RPC_S_OK || !v) { CHECK(0); return; }
    int listed = vec_find_w(v, "ncacn_hvsocket") >= 0;
    RPC_STATUS st = p_RpcNetworkIsProtseqValidW((RPC_WSTR)L"ncacn_hvsocket");
    t_note("ncacn_hvsocket: listed=%d status=%lu", listed, (unsigned long)st);
    CHECK(listed ? st == RPC_S_OK : st == RUS_PROTSEQ_NOT_SUPPORTED);
    p_RpcProtseqVectorFreeW(&v);
}
