/* RpcStringBindingCompose / RpcStringBindingParse (A and W).
 *
 * Facts taken from the Windows 10 19041 decompilation of DCE_BINDING::StringBindingCompose, DCE_BINDING::DCE_BINDING(wchar*)
 * and RpcStringBindingParseW:
 *   string binding := [objuuid '@'] protseq ':' netaddr ['[' endpoint [',' options] ']']
 *   compose escapes  ,  :  @  [  \  ]  with a backslash in protseq/netaddr/endpoint/options ('=' is NOT escaped)
 *   compose with an endpoint that is empty/NULL and options non-empty yields "[,options]"
 *   compose: a nil / empty object uuid string is omitted, others are canonicalised to lowercase
 *   compose with all parts NULL yields ":" ; a NULL StringBinding out pointer is accepted (returns 0)
 *   parse: NULL string -> ERROR_INVALID_PARAMETER ; length >= 0x8000 -> ERROR_INVALID_PARAMETER
 *          no unescaped ':' -> RPC_S_INVALID_STRING_BINDING ; bad uuid before the first unescaped '@' -> RPC_S_INVALID_STRING_UUID
 *          '[' with no ']' -> RPC_S_INVALID_ENDPOINT_FORMAT (no comma) or RPC_S_INVALID_STRING_BINDING (after a comma)
 *          every requested output is an allocated string, empty ("") when the part is absent; outputs untouched on failure
 *          the protseq is NOT validated by parse. */
#include "rus.h"

typedef struct {
    char obj[512], proto[512], addr[512], ep[512], opts[512];
} parsed_t;

static void narrow_free_w(RPC_WSTR *w, char *dst)
{
    __typeof__(&RpcStringFreeW) f = U(RpcStringFreeW);
    dst[0] = 0;
    if (!*w) { strcpy(dst, "<NULL>"); return; }
    int i = 0;
    for (; (*w)[i] && i < 500; i++) dst[i] = (*w)[i] < 0x80 ? (char)(*w)[i] : '?';
    dst[i] = 0;
    f(w);
}

static RPC_STATUS do_parse_w(const char *in, parsed_t *o)
{
    __typeof__(&RpcStringBindingParseW) f = U(RpcStringBindingParseW);
    WCHAR win[2048]; RPC_WSTR a = NULL, b = NULL, c = NULL, d = NULL, e = NULL;
    rus_widen(win, 2048, in);
    RPC_STATUS st = f(win, &a, &b, &c, &d, &e);
    if (st == RPC_S_OK) {
        narrow_free_w(&a, o->obj); narrow_free_w(&b, o->proto); narrow_free_w(&c, o->addr);
        narrow_free_w(&d, o->ep); narrow_free_w(&e, o->opts);
    }
    return st;
}

static void narrow_free_a(unsigned char **s, char *dst)
{
    __typeof__(&RpcStringFreeA) f = U(RpcStringFreeA);
    if (!*s) { strcpy(dst, "<NULL>"); return; }
    strncpy(dst, (char *)*s, 500); dst[500] = 0;
    f(s);
}

static RPC_STATUS do_parse_a(const char *in, parsed_t *o)
{
    __typeof__(&RpcStringBindingParseA) f = U(RpcStringBindingParseA);
    unsigned char *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL;
    RPC_STATUS st = f((unsigned char *)in, &a, &b, &c, &d, &e);
    if (st == RPC_S_OK) {
        narrow_free_a(&a, o->obj); narrow_free_a(&b, o->proto); narrow_free_a(&c, o->addr);
        narrow_free_a(&d, o->ep); narrow_free_a(&e, o->opts);
    }
    return st;
}

/* ------------------------------------------------------------------ Compose */

struct crow { const char *obj, *proto, *addr, *ep, *opts, *want; const char *what; };

static const struct crow g_compose[] = {
    { NULL, NULL, NULL, NULL, NULL,                               ":",                              "all NULL" },
    { NULL, "ncalrpc", NULL, NULL, NULL,                          "ncalrpc:",                       "protseq only" },
    { NULL, "ncalrpc", NULL, "ep", NULL,                          "ncalrpc:[ep]",                   "protseq + endpoint" },
    { NULL, "ncalrpc", "", "ep", "",                              "ncalrpc:[ep]",                   "empty strings behave like NULL" },
    { NULL, "ncacn_ip_tcp", "127.0.0.1", "135", NULL,             "ncacn_ip_tcp:127.0.0.1[135]",    "tcp" },
    { NULL, "ncacn_ip_tcp", "host", NULL, NULL,                   "ncacn_ip_tcp:host",              "tcp no endpoint" },
    { NULL, "ncacn_np", ".", "\\pipe\\x", NULL,                   "ncacn_np:.[\\\\pipe\\\\x]",      "backslashes doubled" },
    { NULL, "ncacn_np", "\\\\srv", "\\pipe\\x", NULL,             "ncacn_np:\\\\\\\\srv[\\\\pipe\\\\x]", "unc address doubled" },
    { NULL, "ncacn_ip_tcp", "h", NULL, "Security=x",              "ncacn_ip_tcp:h[,Security=x]",    "options only: empty endpoint slot" },
    { NULL, "ncacn_ip_tcp", "h", "", "Security=x",                "ncacn_ip_tcp:h[,Security=x]",    "empty endpoint + options" },
    { NULL, "ncacn_ip_tcp", "h", "135", "a=b",                    "ncacn_ip_tcp:h[135,a=b]",        "endpoint + options, '=' not escaped" },
    { "8A885D04-1CEB-11C9-9FE8-08002B104860", "ncalrpc", NULL, "ep", NULL,
                                                                  "8a885d04-1ceb-11c9-9fe8-08002b104860@ncalrpc:[ep]", "uuid lowercased" },
    { "8a885d04-1ceb-11c9-9fe8-08002b104860", "ncalrpc", NULL, NULL, NULL,
                                                                  "8a885d04-1ceb-11c9-9fe8-08002b104860@ncalrpc:", "uuid without endpoint" },
    { "00000000-0000-0000-0000-000000000000", "ncalrpc", NULL, "ep", NULL,
                                                                  "ncalrpc:[ep]",                   "nil uuid omitted" },
    { "", "ncalrpc", NULL, "ep", NULL,                            "ncalrpc:[ep]",                   "empty uuid string omitted" },
    { NULL, "ncacn_ip_tcp", "a:b", "1", NULL,                     "ncacn_ip_tcp:a\\:b[1]",          "colon in address" },
    { NULL, "ncacn_ip_tcp", "a@b", "1", NULL,                     "ncacn_ip_tcp:a\\@b[1]",          "at in address" },
    { NULL, "ncacn_ip_tcp", "a,b", "1", NULL,                     "ncacn_ip_tcp:a\\,b[1]",          "comma in address" },
    { NULL, "ncacn_ip_tcp", "a[b]", "1", NULL,                    "ncacn_ip_tcp:a\\[b\\][1]",       "brackets in address" },
    { NULL, "ncacn_ip_tcp", "a\\b", "1", NULL,                    "ncacn_ip_tcp:a\\\\b[1]",         "backslash in address" },
    { NULL, "ncalrpc", NULL, "a]b", NULL,                         "ncalrpc:[a\\]b]",                "bracket in endpoint" },
    { NULL, "ncalrpc", NULL, "a,b", NULL,                         "ncalrpc:[a\\,b]",                "comma in endpoint" },
    { NULL, "ncalrpc", NULL, NULL, "a,b",                         "ncalrpc:[,a\\,b]",               "comma in options" },
    { NULL, "ncalrpc", NULL, "e", "a]b",                          "ncalrpc:[e,a\\]b]",              "bracket in options" },
    { NULL, "a:b", NULL, NULL, NULL,                              "a\\:b:",                         "colon in protseq" },
    { NULL, "ncacn_ip_tcp", "a=b c-d.e_f", "1", NULL,             "ncacn_ip_tcp:a=b c-d.e_f[1]",    "= space - . _ are literal" },
    { NULL, "p", ",:@[\\]", NULL, NULL,                           "p:\\,\\:\\@\\[\\\\\\]",          "all six specials" },
    { NULL, "ncacn_ip_tcp", "fe80::1", "135", NULL,               "ncacn_ip_tcp:fe80\\:\\:1[135]",  "ipv6 literal colons escaped" },
    { NULL, "ncacn_http", "proxy.example.com", "80", NULL,        "ncacn_http:proxy.example.com[80]", "http" },
    { NULL, "ncacn_np", "\\\\server", "\\pipe\\lsarpc", NULL,     "ncacn_np:\\\\\\\\server[\\\\pipe\\\\lsarpc]", "lsarpc style" },
    { NULL, NULL, "host", "1", NULL,                              ":host[1]",                       "NULL protseq" },
    { NULL, "ncalrpc", NULL, "ep", "o",                           "ncalrpc:[ep,o]",                 "ncalrpc endpoint + options" },
};

TEST(rus_compose_w_table)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringFreeW);
    for (int i = 0; i < NSTR(g_compose); i++) {
        const struct crow *r = &g_compose[i];
        WCHAR w[5][128]; RPC_WSTR p[5]; const char *src[5] = { r->obj, r->proto, r->addr, r->ep, r->opts };
        for (int k = 0; k < 5; k++) { if (src[k]) { rus_widen(w[k], 128, src[k]); p[k] = w[k]; } else p[k] = NULL; }
        RPC_WSTR out = NULL;
        RPC_STATUS st = p_RpcStringBindingComposeW(p[0], p[1], p[2], p[3], p[4], &out);
        CHECKF(st == RPC_S_OK, "[%d] %s: status %lu", i, r->what, (unsigned long)st);
        CHECKF(out && rus_wide_eq_ascii(out, r->want), "[%d] %s: want \"%s\"", i, r->what, r->want);
        p_RpcStringFreeW(&out);
    }
}

TEST(rus_compose_a_table)
{
    NEED(RpcStringBindingComposeA); NEED(RpcStringFreeA);
    for (int i = 0; i < NSTR(g_compose); i++) {
        const struct crow *r = &g_compose[i];
        unsigned char *out = NULL;
        RPC_STATUS st = p_RpcStringBindingComposeA((unsigned char *)r->obj, (unsigned char *)r->proto, (unsigned char *)r->addr,
                                                   (unsigned char *)r->ep, (unsigned char *)r->opts, &out);
        CHECKF(st == RPC_S_OK, "[%d] %s: status %lu", i, r->what, (unsigned long)st);
        CHECKF(out && !strcmp((char *)out, r->want), "[%d] %s: got \"%s\" want \"%s\"", i, r->what, out ? (char *)out : "<NULL>", r->want);
        p_RpcStringFreeA(&out);
    }
}

TEST(rus_compose_invalid_object_uuid)
{
    NEED(RpcStringBindingComposeA); NEED(RpcStringBindingComposeW); NEED(RpcStringFreeA);
    const char *bad[] = { "{8a885d04-1ceb-11c9-9fe8-08002b104860}", "xyz", "8a885d04-1ceb-11c9-9fe8-08002b10486", "8a885d04-1ceb-11c9-9fe8-08002b104860 " };
    for (int i = 0; i < NSTR(bad); i++) {
        unsigned char *out = NULL; WCHAR w[64]; RPC_WSTR wout = NULL;
        rus_widen(w, 64, bad[i]);
        RPC_STATUS sa = p_RpcStringBindingComposeA((unsigned char *)bad[i], (unsigned char *)"ncalrpc", NULL, (unsigned char *)"e", NULL, &out);
        RPC_STATUS sw = p_RpcStringBindingComposeW(w, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)L"e", NULL, &wout);
        CHECKF(sa == RUS_INVALID_STRING_UUID, "A \"%s\" -> %lu", bad[i], (unsigned long)sa);
        CHECKF(sw == RUS_INVALID_STRING_UUID, "W \"%s\" -> %lu", bad[i], (unsigned long)sw);
        CHECK(out == NULL && wout == NULL);
    }
}

TEST(rus_compose_null_out_pointer_is_success)
{
    NEED(RpcStringBindingComposeW);
    /* decomp: a NULL StringBinding pointer returns 0 before anything is built */
    if (t_is_wine()) { t_skip("wine dereferences the NULL out pointer"); return; }
    EXPECT_EQ(p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)L"e", NULL, NULL), RPC_S_OK);
}

TEST(rus_compose_result_is_nul_terminated_exact_length)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringFreeW);
    RPC_WSTR out = NULL;
    EXPECT_EQ(p_RpcStringBindingComposeW((RPC_WSTR)L"8a885d04-1ceb-11c9-9fe8-08002b104860", (RPC_WSTR)L"ncacn_ip_tcp",
                                         (RPC_WSTR)L"host", (RPC_WSTR)L"135", (RPC_WSTR)L"o", &out), RPC_S_OK);
    if (out) {
        EXPECT_EQ(lstrlenW(out), 36 + 1 + 12 + 1 + 4 + 1 + 3 + 1 + 1 + 1); /* uuid @ ncacn_ip_tcp : host [ 135 , o ] */
        p_RpcStringFreeW(&out);
    }
}

TEST(rus_compose_long_components)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringFreeW);
    WCHAR host[1025]; for (int i = 0; i < 1024; i++) host[i] = L'h'; host[1024] = 0;
    RPC_WSTR out = NULL;
    EXPECT_EQ(p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncacn_ip_tcp", host, (RPC_WSTR)L"135", NULL, &out), RPC_S_OK);
    if (out) { EXPECT_EQ(lstrlenW(out), 12 + 1 + 1024 + 5); p_RpcStringFreeW(&out); }
}

TEST(rus_compose_too_long_component)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringFreeW);
    /* decomp: protseq, endpoint and options are limited to 0x7fff characters; longer -> ERROR_OUTOFMEMORY (14) */
    static WCHAR big[0x8001];
    for (int i = 0; i < 0x8000; i++) big[i] = L'e';
    big[0x8000] = 0;
    RPC_WSTR out = NULL;
    RPC_STATUS st = p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, big, NULL, &out);
    EXPECT_EQ(st, RUS_OUTOFMEMORY);
    CHECK(out == NULL);
    st = p_RpcStringBindingComposeW(NULL, big, NULL, (RPC_WSTR)L"e", NULL, &out);
    EXPECT_EQ(st, RUS_OUTOFMEMORY);
    st = p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, (RPC_WSTR)L"e", big, &out);
    EXPECT_EQ(st, RUS_OUTOFMEMORY);
    big[0x7fff] = 0; /* exactly 0x7fff characters is accepted */
    st = p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncalrpc", NULL, big, NULL, &out);
    EXPECT_EQ(st, RPC_S_OK);
    p_RpcStringFreeW(&out);
}

/* -------------------------------------------------------------------- Parse */

struct prow { const char *in; RPC_STATUS st; const char *obj, *proto, *addr, *ep, *opts; const char *what; };

static const struct prow g_parse[] = {
    { "ncalrpc:[ep]", 0, "", "ncalrpc", "", "ep", "", "basic ncalrpc" },
    { "ncalrpc:", 0, "", "ncalrpc", "", "", "", "no brackets: endpoint is empty string" },
    { "ncacn_ip_tcp:host[135]", 0, "", "ncacn_ip_tcp", "host", "135", "", "tcp" },
    { "ncacn_ip_tcp:host", 0, "", "ncacn_ip_tcp", "host", "", "", "tcp, no endpoint" },
    { "ncacn_ip_tcp:127.0.0.1[135]", 0, "", "ncacn_ip_tcp", "127.0.0.1", "135", "", "ipv4" },
    { "ncacn_ip_tcp:[135]", 0, "", "ncacn_ip_tcp", "", "135", "", "empty address" },
    { "00000000-0000-0000-c000-000000000046@ncacn_np:.[\\\\pipe\\\\test]", 0,
      "00000000-0000-0000-c000-000000000046", "ncacn_np", ".", "\\pipe\\test", "", "doubled backslashes collapse" },
    { "00000000-0000-0000-c000-000000000046@ncacn_np:.[\\pipe\\test]", 0,
      "00000000-0000-0000-c000-000000000046", "ncacn_np", ".", "pipetest", "", "single backslash is an escape that vanishes (Wine test string)" },
    { "00000000-0000-0000-c000-000000000046@ncacn_np:.[endpoint=\\pipe\\test]", 0,
      "00000000-0000-0000-c000-000000000046", "ncacn_np", ".", "pipetest", "", "endpoint= form" },
    { "00000000-0000-0000-c000-000000000046@ncacn_np:.[endpoint=test]", 0,
      "00000000-0000-0000-c000-000000000046", "ncacn_np", ".", "test", "", "endpoint=test" },
    { "8A885D04-1CEB-11C9-9FE8-08002B104860@ncalrpc:[x]", 0,
      "8a885d04-1ceb-11c9-9fe8-08002b104860", "ncalrpc", "", "x", "", "uuid output is lowercase" },
    { "00000000-0000-0000-0000-000000000000@ncalrpc:[x]", 0, "", "ncalrpc", "", "x", "", "nil uuid parses to empty string" },
    { "ncacn_ip_tcp:host[135,Security=Foo]", 0, "", "ncacn_ip_tcp", "host", "135", "Security=Foo", "options" },
    { "ncacn_ip_tcp:host[135,a,b]", 0, "", "ncacn_ip_tcp", "host", "135", "a,b", "options keep later commas" },
    { "ncacn_ip_tcp:host[endpoint=135]", 0, "", "ncacn_ip_tcp", "host", "135", "", "endpoint= key" },
    { "ncacn_ip_tcp:host[ENDPOINT=135,x]", 0, "", "ncacn_ip_tcp", "host", "135", "x", "endpoint key is case-insensitive, with options" },
    { "ncacn_ip_tcp:host[Endpoint=135]", 0, "", "ncacn_ip_tcp", "host", "135", "", "mixed case endpoint key" },
    { "ncacn_ip_tcp:host[a=b]", 0, "", "ncacn_ip_tcp", "host", "", "a=b", "unknown key: endpoint empty, whole text is options" },
    { "ncacn_ip_tcp:host[1=2,x]", 0, "", "ncacn_ip_tcp", "host", "", "1=2,x", "unknown key before comma" },
    { "ncacn_ip_tcp:host[,opt]", 0, "", "ncacn_ip_tcp", "host", "", "opt", "leading comma: empty endpoint" },
    { "ncacn_ip_tcp:a\\:b[1]", 0, "", "ncacn_ip_tcp", "a:b", "1", "", "escaped colon in address" },
    { "ncacn_ip_tcp:a\\@b[1]", 0, "", "ncacn_ip_tcp", "a@b", "1", "", "escaped at in address" },
    { "ncacn_ip_tcp:a\\[b\\][1]", 0, "", "ncacn_ip_tcp", "a[b]", "1", "", "escaped brackets in address" },
    { "ncalrpc:[a\\]b]", 0, "", "ncalrpc", "", "a]b", "", "escaped bracket in endpoint" },
    { "ncalrpc:[a\\,b]", 0, "", "ncalrpc", "", "a,b", "", "escaped comma in endpoint" },
    { "ncalrpc:[,a\\,b]", 0, "", "ncalrpc", "", "", "a,b", "escaped comma in options" },
    { "a\\:b:host", 0, "", "a:b", "host", "", "", "escaped colon in protseq" },
    { "ncacn_ip_tcp:fe80\\:\\:1[135]", 0, "", "ncacn_ip_tcp", "fe80::1", "135", "", "ipv6" },
    { "ncacn_ip_tcp:fe80::1[135]", 0, "", "ncacn_ip_tcp", "fe80::1", "135", "", "unescaped colons after the first are address text" },
    { ":[x]", 0, "", "", "", "x", "", "empty protseq is not validated by parse" },
    { "foo:bar", 0, "", "foo", "bar", "", "", "unknown protseq is not validated by parse" },
    { "ncacn_ip_tcp:h\\\\x[1]", 0, "", "ncacn_ip_tcp", "h\\x", "1", "", "escaped backslash in address" },

    /* errors */
    { "", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "empty string" },
    { "ncalrpc", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "no colon" },
    { "abc", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "no colon 2" },
    { "00000000-0000-0000-c000-000000000046@ncacn_np", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "uuid ok, no colon (Wine test)" },
    { "00000000-0000-0000-c000-000000000046@", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "uuid ok, nothing after the at sign" },
    { "ncalrpc:[ep", RUS_INVALID_ENDPOINT_FORMAT, 0, 0, 0, 0, 0, "unterminated endpoint" },
    { "ncalrpc:[", RUS_INVALID_ENDPOINT_FORMAT, 0, 0, 0, 0, 0, "bare bracket" },
    { "ncalrpc:[ep,opt", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "unterminated options" },
    { "ncalrpc:[,opt", RUS_INVALID_STRING_BINDING, 0, 0, 0, 0, 0, "unterminated options, empty endpoint" },
    { "{00000000-0000-0000-c000-000000000046}@ncalrpc:", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "braced uuid" },
    { "@ncalrpc:", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "empty uuid before at sign is invalid" },
    { "x@ncalrpc:", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "short uuid" },
    { "00000000-0000-0000-c000-00000000004@ncalrpc:", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "35 char uuid" },
    { "ncacn_ip_tcp:user@host", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "unescaped at after the colon is read as a uuid separator" },
    { "ncacn_ip_tcp:host[135]@x", RUS_INVALID_STRING_UUID, 0, 0, 0, 0, 0, "at sign anywhere" },
};

static void check_row_w_a(const struct prow *r, int i, int use_w)
{
    parsed_t o; memset(&o, 0, sizeof o);
    RPC_STATUS st = use_w ? do_parse_w(r->in, &o) : do_parse_a(r->in, &o);
    CHECKF(st == r->st, "[%d] %c \"%s\" (%s): status %lu want %lu", i, use_w ? 'W' : 'A', r->in, r->what, (unsigned long)st, (unsigned long)r->st);
    if (st == RPC_S_OK && r->st == RPC_S_OK) {
        CHECKF(!strcmp(o.obj, r->obj), "[%d] %s: obj \"%s\" want \"%s\"", i, r->what, o.obj, r->obj);
        CHECKF(!strcmp(o.proto, r->proto), "[%d] %s: protseq \"%s\" want \"%s\"", i, r->what, o.proto, r->proto);
        CHECKF(!strcmp(o.addr, r->addr), "[%d] %s: netaddr \"%s\" want \"%s\"", i, r->what, o.addr, r->addr);
        CHECKF(!strcmp(o.ep, r->ep), "[%d] %s: endpoint \"%s\" want \"%s\"", i, r->what, o.ep, r->ep);
        CHECKF(!strcmp(o.opts, r->opts), "[%d] %s: options \"%s\" want \"%s\"", i, r->what, o.opts, r->opts);
    }
}

TEST(rus_parse_w_table)
{
    NEED(RpcStringBindingParseW);
    for (int i = 0; i < NSTR(g_parse); i++) check_row_w_a(&g_parse[i], i, 1);
}

TEST(rus_parse_a_table)
{
    NEED(RpcStringBindingParseA);
    for (int i = 0; i < NSTR(g_parse); i++) check_row_w_a(&g_parse[i], i, 0);
}

TEST(rus_parse_failure_leaves_outputs_untouched)
{
    NEED(RpcStringBindingParseW); NEED(RpcStringBindingParseA);
    WCHAR in[] = L"ncalrpc:[ep"; /* RPC_S_INVALID_ENDPOINT_FORMAT */
    RPC_WSTR a = (RPC_WSTR)(ULONG_PTR)0x11, b = (RPC_WSTR)(ULONG_PTR)0x22, c = (RPC_WSTR)(ULONG_PTR)0x33,
             d = (RPC_WSTR)(ULONG_PTR)0x44, e = (RPC_WSTR)(ULONG_PTR)0x55;
    EXPECT_EQ(p_RpcStringBindingParseW(in, &a, &b, &c, &d, &e), RUS_INVALID_ENDPOINT_FORMAT);
    CHECK(a == (RPC_WSTR)(ULONG_PTR)0x11 && b == (RPC_WSTR)(ULONG_PTR)0x22 && c == (RPC_WSTR)(ULONG_PTR)0x33 &&
          d == (RPC_WSTR)(ULONG_PTR)0x44 && e == (RPC_WSTR)(ULONG_PTR)0x55);
    unsigned char *aa = (unsigned char *)(ULONG_PTR)0x11, *ab = (unsigned char *)(ULONG_PTR)0x22,
                  *ac = (unsigned char *)(ULONG_PTR)0x33, *ad = (unsigned char *)(ULONG_PTR)0x44, *ae = (unsigned char *)(ULONG_PTR)0x55;
    EXPECT_EQ(p_RpcStringBindingParseA((unsigned char *)"@x:", &aa, &ab, &ac, &ad, &ae), RUS_INVALID_STRING_UUID);
    CHECK(aa == (unsigned char *)(ULONG_PTR)0x11 && ab == (unsigned char *)(ULONG_PTR)0x22 && ac == (unsigned char *)(ULONG_PTR)0x33 &&
          ad == (unsigned char *)(ULONG_PTR)0x44 && ae == (unsigned char *)(ULONG_PTR)0x55);
}

TEST(rus_parse_all_outputs_optional)
{
    NEED(RpcStringBindingParseW); NEED(RpcStringBindingParseA); NEED(RpcStringFreeW); NEED(RpcStringFreeA);
    WCHAR in[] = L"8a885d04-1ceb-11c9-9fe8-08002b104860@ncacn_ip_tcp:host[135,o]";
    /* validate only */
    EXPECT_EQ(p_RpcStringBindingParseW(in, NULL, NULL, NULL, NULL, NULL), RPC_S_OK);
    EXPECT_EQ(p_RpcStringBindingParseA((unsigned char *)"ncalrpc:[x]", NULL, NULL, NULL, NULL, NULL), RPC_S_OK);
    /* validation still happens with no outputs requested */
    EXPECT_EQ(p_RpcStringBindingParseW((RPC_WSTR)L"x@ncalrpc:", NULL, NULL, NULL, NULL, NULL), RUS_INVALID_STRING_UUID);
    EXPECT_EQ(p_RpcStringBindingParseW((RPC_WSTR)L"ncalrpc", NULL, NULL, NULL, NULL, NULL), RUS_INVALID_STRING_BINDING);
    EXPECT_EQ(p_RpcStringBindingParseA((unsigned char *)"ncalrpc", NULL, NULL, NULL, NULL, NULL), RUS_INVALID_STRING_BINDING);
    /* each output on its own */
    RPC_WSTR s = NULL;
    EXPECT_EQ(p_RpcStringBindingParseW(in, &s, NULL, NULL, NULL, NULL), RPC_S_OK);
    CHECK(s && rus_wide_eq_ascii(s, "8a885d04-1ceb-11c9-9fe8-08002b104860")); p_RpcStringFreeW(&s);
    EXPECT_EQ(p_RpcStringBindingParseW(in, NULL, &s, NULL, NULL, NULL), RPC_S_OK);
    CHECK(s && rus_wide_eq_ascii(s, "ncacn_ip_tcp")); p_RpcStringFreeW(&s);
    EXPECT_EQ(p_RpcStringBindingParseW(in, NULL, NULL, &s, NULL, NULL), RPC_S_OK);
    CHECK(s && rus_wide_eq_ascii(s, "host")); p_RpcStringFreeW(&s);
    EXPECT_EQ(p_RpcStringBindingParseW(in, NULL, NULL, NULL, &s, NULL), RPC_S_OK);
    CHECK(s && rus_wide_eq_ascii(s, "135")); p_RpcStringFreeW(&s);
    EXPECT_EQ(p_RpcStringBindingParseW(in, NULL, NULL, NULL, NULL, &s), RPC_S_OK);
    CHECK(s && rus_wide_eq_ascii(s, "o")); p_RpcStringFreeW(&s);
    unsigned char *a = NULL;
    EXPECT_EQ(p_RpcStringBindingParseA((unsigned char *)"ncacn_np:.[\\\\pipe\\\\x]", NULL, &a, NULL, NULL, NULL), RPC_S_OK);
    if (a) { EXPECT_STR((char *)a, "ncacn_np"); p_RpcStringFreeA(&a); }
}

TEST(rus_parse_absent_parts_are_empty_strings_not_null)
{
    NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    RPC_WSTR o = NULL, p = NULL, n = NULL, e = NULL, op = NULL;
    EXPECT_EQ(p_RpcStringBindingParseW((RPC_WSTR)L"ncalrpc:", &o, &p, &n, &e, &op), RPC_S_OK);
    CHECK(o != NULL && o[0] == 0);
    CHECK(p != NULL && p[0] == L'n');
    CHECK(n != NULL && n[0] == 0);
    CHECK(e != NULL && e[0] == 0);
    CHECK(op != NULL && op[0] == 0);
    p_RpcStringFreeW(&o); p_RpcStringFreeW(&p); p_RpcStringFreeW(&n); p_RpcStringFreeW(&e); p_RpcStringFreeW(&op);
}

TEST(rus_parse_null_string)
{
    NEED(RpcStringBindingParseW);
    if (t_is_wine()) { t_skip("wine dereferences the NULL string"); return; }
    RPC_WSTR p = NULL;
    EXPECT_EQ(p_RpcStringBindingParseW(NULL, NULL, &p, NULL, NULL, NULL), RUS_INVALID_PARAMETER);
    CHECK(p == NULL);
}

TEST(rus_parse_length_limit_w)
{
    NEED(RpcStringBindingParseW);
    /* decomp: length >= 0x8000 characters -> ERROR_INVALID_PARAMETER ; 0x7fff characters are examined */
    static WCHAR big[0x8002];
    for (int i = 0; i < 0x8001; i++) big[i] = L'a';
    big[0x8001] = 0;
    EXPECT_EQ(p_RpcStringBindingParseW(big, NULL, NULL, NULL, NULL, NULL), RUS_INVALID_PARAMETER);
    big[0x8000] = 0;
    EXPECT_EQ(p_RpcStringBindingParseW(big, NULL, NULL, NULL, NULL, NULL), RUS_INVALID_PARAMETER);
    big[0x7fff] = 0;
    EXPECT_EQ(p_RpcStringBindingParseW(big, NULL, NULL, NULL, NULL, NULL), RUS_INVALID_STRING_BINDING); /* no colon */
    big[10] = L':';
    EXPECT_EQ(p_RpcStringBindingParseW(big, NULL, NULL, NULL, NULL, NULL), RPC_S_OK);
}

TEST(rus_parse_trailing_text_after_bracket)
{
    NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    parsed_t o; memset(&o, 0, sizeof o);
    RPC_STATUS st = do_parse_w("ncalrpc:[ep]trailing", &o);
    t_note("parse of \"ncalrpc:[ep]trailing\": status %lu endpoint \"%s\" (decomp suggests text after ']' is ignored)", (unsigned long)st, o.ep);
    CHECKF(st == RPC_S_OK || st == RUS_INVALID_STRING_BINDING || st == RUS_INVALID_ENDPOINT_FORMAT, "status %lu", (unsigned long)st);
}

/* ------------------------------------------------- compose -> parse round trip */

TEST(rus_compose_parse_roundtrip_w)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    for (int i = 0; i < NSTR(g_compose); i++) {
        const struct crow *r = &g_compose[i];
        if (!r->proto) continue;
        WCHAR w[5][128]; RPC_WSTR p[5]; const char *src[5] = { r->obj, r->proto, r->addr, r->ep, r->opts };
        for (int k = 0; k < 5; k++) { if (src[k]) { rus_widen(w[k], 128, src[k]); p[k] = w[k]; } else p[k] = NULL; }
        RPC_WSTR sb = NULL;
        if (p_RpcStringBindingComposeW(p[0], p[1], p[2], p[3], p[4], &sb) != RPC_S_OK) { CHECK(0); continue; }
        RPC_WSTR o = NULL, pr = NULL, n = NULL, e = NULL, op = NULL;
        RPC_STATUS st = p_RpcStringBindingParseW(sb, &o, &pr, &n, &e, &op);
        CHECKF(st == RPC_S_OK, "[%d] %s: parse status %lu", i, r->what, (unsigned long)st);
        if (st == RPC_S_OK) {
            const char *wobj = (r->obj && r->obj[0] && strcmp(r->obj, "00000000-0000-0000-0000-000000000000")) ? NULL : "";
            if (wobj) CHECKF(rus_wide_eq_ascii(o, ""), "[%d] %s: obj not empty", i, r->what);
            CHECKF(rus_wide_eq_ascii(pr, r->proto), "[%d] %s: protseq", i, r->what);
            CHECKF(rus_wide_eq_ascii(n, r->addr ? r->addr : ""), "[%d] %s: netaddr", i, r->what);
            CHECKF(rus_wide_eq_ascii(e, r->ep ? r->ep : ""), "[%d] %s: endpoint", i, r->what);
            CHECKF(rus_wide_eq_ascii(op, r->opts ? r->opts : ""), "[%d] %s: options", i, r->what);
            p_RpcStringFreeW(&o); p_RpcStringFreeW(&pr); p_RpcStringFreeW(&n); p_RpcStringFreeW(&e); p_RpcStringFreeW(&op);
        }
        p_RpcStringFreeW(&sb);
    }
}

TEST(rus_compose_parse_endpoint_with_equals_does_not_roundtrip)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    /* compose does not escape '=', and parse reads "key=value" in the endpoint slot: an endpoint containing '=' whose key is not
     * "endpoint" comes back as an EMPTY endpoint with the text moved into the options. */
    RPC_WSTR sb = NULL;
    EXPECT_EQ(p_RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncacn_ip_tcp", (RPC_WSTR)L"h", (RPC_WSTR)L"a=b", NULL, &sb), RPC_S_OK);
    CHECK(sb && rus_wide_eq_ascii(sb, "ncacn_ip_tcp:h[a=b]"));
    RPC_WSTR e = NULL, op = NULL;
    EXPECT_EQ(p_RpcStringBindingParseW(sb, NULL, NULL, NULL, &e, &op), RPC_S_OK);
    CHECK(e && rus_wide_eq_ascii(e, ""));
    CHECK(op && rus_wide_eq_ascii(op, "a=b"));
    p_RpcStringFreeW(&e); p_RpcStringFreeW(&op); p_RpcStringFreeW(&sb);
}

TEST(rus_compose_parse_random_roundtrip)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringBindingParseW); NEED(RpcStringFreeW);
    /* random components built from a hostile alphabet that includes every escaped character but not '=' */
    static const WCHAR alpha[] = L"ab1.-_ ,:@[]\\";
    unsigned seed = 0xA11CE;
    for (int iter = 0; iter < 400; iter++) {
        WCHAR f[4][24];
        for (int k = 0; k < 4; k++) {
            int len = 1 + rus_rand(&seed) % 12;
            for (int j = 0; j < len; j++) f[k][j] = alpha[rus_rand(&seed) % (sizeof(alpha) / sizeof(WCHAR) - 1)];
            f[k][len] = 0;
        }
        /* protseq must be non-empty and options is only representable when non-empty; all four are non-empty here */
        RPC_WSTR sb = NULL;
        RPC_STATUS st = p_RpcStringBindingComposeW(NULL, f[0], f[1], f[2], f[3], &sb);
        if (st != RPC_S_OK) { CHECK(0); continue; }
        RPC_WSTR pr = NULL, n = NULL, e = NULL, op = NULL;
        st = p_RpcStringBindingParseW(sb, NULL, &pr, &n, &e, &op);
        CHECKF(st == RPC_S_OK, "iter %d status %lu", iter, (unsigned long)st);
        if (st == RPC_S_OK) {
            CHECKF(!lstrcmpW(pr, f[0]), "iter %d protseq", iter);
            CHECKF(!lstrcmpW(n, f[1]), "iter %d netaddr", iter);
            CHECKF(!lstrcmpW(e, f[2]), "iter %d endpoint", iter);
            CHECKF(!lstrcmpW(op, f[3]), "iter %d options", iter);
            p_RpcStringFreeW(&pr); p_RpcStringFreeW(&n); p_RpcStringFreeW(&e); p_RpcStringFreeW(&op);
        }
        p_RpcStringFreeW(&sb);
    }
}

TEST(rus_compose_a_matches_w_for_random)
{
    NEED(RpcStringBindingComposeW); NEED(RpcStringBindingComposeA); NEED(RpcStringFreeW); NEED(RpcStringFreeA);
    static const char alpha[] = "ab1.-_ ,:@[]\\=";
    unsigned seed = 0x5EED;
    for (int iter = 0; iter < 200; iter++) {
        char fa[4][24]; WCHAR fw[4][24];
        for (int k = 0; k < 4; k++) {
            int len = 1 + rus_rand(&seed) % 10;
            for (int j = 0; j < len; j++) fa[k][j] = alpha[rus_rand(&seed) % (sizeof(alpha) - 1)];
            fa[k][len] = 0; rus_widen(fw[k], 24, fa[k]);
        }
        unsigned char *sa = NULL; RPC_WSTR sw = NULL;
        EXPECT_EQ(p_RpcStringBindingComposeA(NULL, (unsigned char *)fa[0], (unsigned char *)fa[1], (unsigned char *)fa[2], (unsigned char *)fa[3], &sa), RPC_S_OK);
        EXPECT_EQ(p_RpcStringBindingComposeW(NULL, fw[0], fw[1], fw[2], fw[3], &sw), RPC_S_OK);
        CHECKF(sa && sw && rus_wide_eq_ascii(sw, (char *)sa), "iter %d A/W differ", iter);
        p_RpcStringFreeA(&sa); p_RpcStringFreeW(&sw);
    }
}

TEST(rus_parse_a_matches_w_for_all_rows)
{
    NEED(RpcStringBindingParseA); NEED(RpcStringBindingParseW); NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    for (int i = 0; i < NSTR(g_parse); i++) {
        parsed_t a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        RPC_STATUS sa = do_parse_a(g_parse[i].in, &a), sw = do_parse_w(g_parse[i].in, &w);
        CHECKF(sa == sw, "[%d] \"%s\": A=%lu W=%lu", i, g_parse[i].in, (unsigned long)sa, (unsigned long)sw);
        if (sa == RPC_S_OK && sw == RPC_S_OK)
            CHECKF(!strcmp(a.obj, w.obj) && !strcmp(a.proto, w.proto) && !strcmp(a.addr, w.addr) && !strcmp(a.ep, w.ep) && !strcmp(a.opts, w.opts),
                   "[%d] \"%s\" outputs differ", i, g_parse[i].in);
    }
}
