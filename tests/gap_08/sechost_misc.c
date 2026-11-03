/* sechost / oleaut32 / ole32 thin spots: LsaLookupNames2/Sids2 argument gates, LookupAccountSidLocalA buffer protocol,
 * StartServiceA access rights, GetServiceDirectory / GetServiceRegistryStateKey gates, StopTraceW / QueryAllTracesA /
 * RegisterTraceGuidsA wrappers, oleaut32 DllCanUnloadNow, HWND_UserMarshal truncation. */
#include "gap8.h"
#include <ntsecapi.h>
#include <winsvc.h>
#include <evntrace.h>
#include <wmistr.h>

#define SH "sechost.dll"
#define NOWINE() do { if (t_is_wine()) { t_skip("emulator stub / unimplemented"); return; } } while (0)

/* ---------------------------------------------------------------- LSA */
#define LNAMES2 UIN(SH, LsaLookupNames2)
#define LSIDS2  UIN(SH, LsaLookupSids2)
#define STATUS_INVALID_PARAMETER_ ((NTSTATUS)0xC000000D)
#define STATUS_INVALID_HANDLE_    ((NTSTATUS)0xC0000008)

TEST(gap8_lsa_lookupnames2_null_outputs_invalid_parameter)
{
    LSA_REFERENCED_DOMAIN_LIST *dom = (void *)(ULONG_PTR)0x5555; LSA_TRANSLATED_SID2 *sids = (void *)(ULONG_PTR)0x6666;
    LSA_UNICODE_STRING n; WCHAR w[] = L"Everyone"; REQUIRE(LNAMES2); NOWINE();
    n.Buffer = w; n.Length = 16; n.MaximumLength = 18;
    G8_EQ(LNAMES2(NULL, 0, 1, &n, &dom, NULL), STATUS_INVALID_PARAMETER_);
    G8_EQ((ULONG_PTR)dom, 0x5555);                          /* rejected before any output is written */
    G8_EQ(LNAMES2(NULL, 0, 1, &n, NULL, &sids), STATUS_INVALID_PARAMETER_);
    G8_EQ((ULONG_PTR)sids, 0x6666);
    G8_EQ(LNAMES2(NULL, 0, 1, &n, NULL, NULL), STATUS_INVALID_PARAMETER_);
    G8_EQ(LNAMES2((LSA_HANDLE)(ULONG_PTR)0x1234, 0xffffffff, 0, NULL, NULL, NULL), STATUS_INVALID_PARAMETER_);
}

TEST(gap8_lsa_lookupsids2_null_names_output_invalid_parameter)
{
    LSA_REFERENCED_DOMAIN_LIST *dom = (void *)(ULONG_PTR)0x5555; REQUIRE(LSIDS2); NOWINE();
    G8_EQ(LSIDS2(NULL, 0, 0, NULL, &dom, NULL), STATUS_INVALID_PARAMETER_);
    G8_EQ((ULONG_PTR)dom, 0x5555);
    G8_EQ(LSIDS2(NULL, 0, 1, NULL, NULL, NULL), STATUS_INVALID_PARAMETER_);
    G8_EQ(LSIDS2((LSA_HANDLE)(ULONG_PTR)0x1234, 0x40000000, 1, NULL, &dom, NULL), STATUS_INVALID_PARAMETER_);   /* the NULL check comes first */
}

TEST(gap8_lsa_lookupsids2_high_flags_need_a_handle)
{
    static const ULONG flags[] = { 0x40000000, 0x80000000, 0xC0000000, 0x40000001, 0xC000FFFF };
    unsigned i; REQUIRE(LSIDS2); NOWINE();
    for (i = 0; i < sizeof flags / sizeof *flags; i++) {
        LSA_REFERENCED_DOMAIN_LIST *dom = (void *)(ULONG_PTR)0x5555; LSA_TRANSLATED_NAME *names = (void *)(ULONG_PTR)0x6666;
        NTSTATUS st = LSIDS2(NULL, flags[i], 0, NULL, &dom, &names);
        CHECKF(st == STATUS_INVALID_HANDLE_, "flags 0x%lx -> 0x%lx", flags[i], st);
        G8_EQ((ULONG_PTR)names, 0x6666); G8_EQ((ULONG_PTR)dom, 0x5555);
    }
}

/* ------------------------------------------------- LookupAccountSidLocalA */
typedef struct { BYTE rev, n; BYTE auth[6]; DWORD sub[5]; } SIDB;
static void mksid(SIDB *s, BYTE last_auth, int n, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
    memset(s, 0, sizeof *s); s->rev = 1; s->n = (BYTE)n; s->auth[5] = last_auth;
    s->sub[0] = a; s->sub[1] = b; s->sub[2] = c; s->sub[3] = d; s->sub[4] = e;
}
#define LSAA UIN(SH, LookupAccountSidLocalA)
#define LSAW UIN(SH, LookupAccountSidLocalW)

static const struct { const char *tag; BYTE auth; int n; DWORD a, b; } k_sids[] = {
    { "Everyone", 1, 1, 0, 0 }, { "SYSTEM", 5, 1, 18, 0 }, { "LOCAL SERVICE", 5, 1, 19, 0 }, { "NETWORK SERVICE", 5, 1, 20, 0 },
    { "Administrators", 5, 2, 32, 544 }, { "Users", 5, 2, 32, 545 }, { "Authenticated Users", 5, 1, 11, 0 },
    { "NETWORK", 5, 1, 2, 0 }, { "INTERACTIVE", 5, 1, 4, 0 }, { "Local", 2, 1, 0, 0 },
};
static int all_ascii(const WCHAR *w) { for (; *w; w++) if (*w > 0x7f) return 0; return 1; }

TEST(gap8_lookupsidlocala_matches_w_variant_table)
{
    unsigned i; REQUIRE(LSAA); REQUIRE(LSAW); NOWINE();
    for (i = 0; i < sizeof k_sids / sizeof *k_sids; i++) {
        SIDB s; WCHAR wn[256], wd[256]; char an[256], ad[256]; DWORD wnc = 256, wdc = 256, anc = 256, adc = 256; SID_NAME_USE wu = 99, au = 98; BOOL wr, ar;
        mksid(&s, k_sids[i].auth, k_sids[i].n, k_sids[i].a, k_sids[i].b, 0, 0, 0);
        wr = LSAW(&s, wn, &wnc, wd, &wdc, &wu);
        memset(an, 0xcc, sizeof an); memset(ad, 0xcc, sizeof ad);
        ar = LSAA(&s, an, &anc, ad, &adc, &au);
        CHECKF(wr == ar, "%s: W=%d A=%d", k_sids[i].tag, wr, ar);
        if (!wr || !ar) { t_note("%s: not resolvable here (W err/A err)", k_sids[i].tag); continue; }
        G8_EQ(au, wu);
        if (all_ascii(wn) && all_ascii(wd)) {
            char nn[256], dd[256]; unsigned k;
            for (k = 0; k <= wnc; k++) nn[k] = (char)wn[k];
            for (k = 0; k <= wdc; k++) dd[k] = (char)wd[k];
            CHECKF(!strcmp(an, nn), "%s: A name \"%s\" vs W \"%s\"", k_sids[i].tag, an, nn);
            CHECKF(!strcmp(ad, dd), "%s: A domain \"%s\" vs W \"%s\"", k_sids[i].tag, ad, dd);
            G8_EQ(anc, wnc); G8_EQ(adc, wdc);              /* lengths exclude the NUL, in characters */
            G8_EQ(strlen(an), anc); G8_EQ(strlen(ad), adc);
        } else t_note("%s: non-ASCII account names, A/W text comparison skipped", k_sids[i].tag);
    }
}

TEST(gap8_lookupsidlocala_size_query_protocol)
{
    unsigned i; REQUIRE(LSAA); REQUIRE(LSAW); NOWINE();
    for (i = 0; i < sizeof k_sids / sizeof *k_sids; i++) {
        SIDB s; WCHAR wn[256], wd[256]; DWORD wnc = 256, wdc = 256, nc = 0, dc = 0; SID_NAME_USE wu = 99, au = 77; BOOL r;
        mksid(&s, k_sids[i].auth, k_sids[i].n, k_sids[i].a, k_sids[i].b, 0, 0, 0);
        if (!LSAW(&s, wn, &wnc, wd, &wdc, &wu) || !all_ascii(wn) || !all_ascii(wd)) continue;
        SET_ERR(0);
        r = LSAA(&s, NULL, &nc, NULL, &dc, &au);
        CHECKF(!r, "%s: size query succeeded", k_sids[i].tag);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        G8_EQ(nc, wnc + 1); G8_EQ(dc, wdc + 1);            /* required sizes include the terminating NUL */
        G8_EQ(au, 77);                                     /* *peUse is only written on success */
    }
}

TEST(gap8_lookupsidlocala_exact_and_short_buffers)
{
    SIDB s; WCHAR wn[256], wd[256]; DWORD wnc = 256, wdc = 256; SID_NAME_USE wu; REQUIRE(LSAA); REQUIRE(LSAW); NOWINE();
    mksid(&s, 5, 2, 32, 544, 0, 0, 0);                      /* BUILTIN\Administrators: both strings non-empty */
    if (!LSAW(&s, wn, &wnc, wd, &wdc, &wu) || !all_ascii(wn) || !all_ascii(wd) || !wnc || !wdc) { t_skip("account names not usable"); return; }
    {   /* exact: len+1 for both */
        char an[64], ad[64]; DWORD nc = wnc + 1, dc = wdc + 1; SID_NAME_USE u = 55;
        memset(an, 0xcc, sizeof an); memset(ad, 0xcc, sizeof ad);
        CHECK(LSAA(&s, an, &nc, ad, &dc, &u));
        G8_EQ(nc, wnc); G8_EQ(dc, wdc); G8_EQ(u, wu);
        G8_EQ(an[wnc], 0); G8_EQ(ad[wdc], 0);
        G8_EQ((unsigned char)an[wnc + 1], 0xcc); G8_EQ((unsigned char)ad[wdc + 1], 0xcc);   /* nothing written past the NUL */
    }
    {   /* name one short: fails, BOTH required sizes reported, nothing written, peUse untouched */
        char an[64], ad[64]; DWORD nc = wnc, dc = wdc + 10; SID_NAME_USE u = 55;
        memset(an, 0xcc, sizeof an); memset(ad, 0xcc, sizeof ad); SET_ERR(0);
        CHECK(!LSAA(&s, an, &nc, ad, &dc, &u)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        G8_EQ(nc, wnc + 1); G8_EQ(dc, wdc + 1);
        G8_EQ((unsigned char)an[0], 0xcc); G8_EQ((unsigned char)ad[0], 0xcc); G8_EQ(u, 55);
    }
    {   /* domain one short */
        char an[64], ad[64]; DWORD nc = wnc + 10, dc = wdc; SID_NAME_USE u = 55;
        memset(an, 0xcc, sizeof an); memset(ad, 0xcc, sizeof ad); SET_ERR(0);
        CHECK(!LSAA(&s, an, &nc, ad, &dc, &u)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        G8_EQ(nc, wnc + 1); G8_EQ(dc, wdc + 1);
        G8_EQ((unsigned char)an[0], 0xcc); G8_EQ((unsigned char)ad[0], 0xcc);
    }
    {   /* zero-sized name with a usable domain buffer */
        char ad[64]; DWORD nc = 0, dc = 64; SID_NAME_USE u = 55; SET_ERR(0);
        CHECK(!LSAA(&s, NULL, &nc, ad, &dc, &u)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        G8_EQ(nc, wnc + 1);
    }
}

TEST(gap8_lookupsidlocala_null_buffers_with_room_succeed)
{
    SIDB s; WCHAR wn[256], wd[256]; DWORD wnc = 256, wdc = 256; SID_NAME_USE wu; REQUIRE(LSAA); REQUIRE(LSAW); NOWINE();
    mksid(&s, 5, 2, 32, 544, 0, 0, 0);
    if (!LSAW(&s, wn, &wnc, wd, &wdc, &wu) || !all_ascii(wn) || !all_ascii(wd)) { t_skip("account names not usable"); return; }
    {   /* Name == NULL but the size says there is room: the name is simply not returned */
        char ad[64]; DWORD nc = 64, dc = 64; SID_NAME_USE u = 55;
        CHECK(LSAA(&s, NULL, &nc, ad, &dc, &u));
        G8_EQ(u, wu); G8_EQ(strlen(ad), wdc); G8_EQ(nc, wnc); G8_EQ(dc, wdc);
    }
    {   /* ReferencedDomainName == NULL likewise */
        char an[64]; DWORD nc = 64, dc = 64; SID_NAME_USE u = 55;
        CHECK(LSAA(&s, an, &nc, NULL, &dc, &u));
        G8_EQ(u, wu); G8_EQ(strlen(an), wnc);
    }
    {   /* both NULL */
        DWORD nc = 64, dc = 64; SID_NAME_USE u = 55;
        CHECK(LSAA(&s, NULL, &nc, NULL, &dc, &u)); G8_EQ(u, wu);
    }
}

TEST(gap8_lookupsidlocala_empty_domain_needs_one_char)
{
    SIDB s; char an[64], ad[8]; DWORD nc = 64, dc = 0; SID_NAME_USE u = 55; REQUIRE(LSAA); NOWINE();
    mksid(&s, 1, 1, 0, 0, 0, 0, 0);                         /* Everyone: the domain string is empty */
    SET_ERR(0);
    if (LSAA(&s, an, &nc, ad, &dc, &u)) { t_skip("unexpected success for dc=0"); return; }
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    G8_EQ(dc, 1);                                           /* the empty domain still requires room for the NUL */
    nc = 64; dc = 1; memset(ad, 0xcc, sizeof ad);
    CHECK(LSAA(&s, an, &nc, ad, &dc, &u));
    G8_EQ(dc, 0); G8_EQ(ad[0], 0); G8_EQ(u, SidTypeWellKnownGroup);
}

TEST(gap8_lookupsidlocala_unmapped_sid)
{
    SIDB s; char an[64], ad[64]; DWORD nc = 64, dc = 64; SID_NAME_USE u = 55; REQUIRE(LSAA); NOWINE();
    mksid(&s, 5, 5, 21, 0x11111111, 0x22222222, 0x33333333, 0x0badf00d);
    SET_ERR(0);
    CHECK(!LSAA(&s, an, &nc, ad, &dc, &u));
    EXPECT_ERR(ERROR_NONE_MAPPED);
    G8_EQ(nc, 64); G8_EQ(dc, 64); G8_EQ(u, 55);
}

/* --------------------------------------------------------- StartServiceA */
TEST(gap8_startservicea_requires_service_start_right)
{
    SC_HANDLE scm, svc; BOOL r; REQUIRE(UIN(SH, StartServiceA)); REQUIRE(UIN(SH, OpenSCManagerW)); REQUIRE(UIN(SH, OpenServiceW)); REQUIRE(UIN(SH, CloseServiceHandle)); NOWINE();
    scm = UIN(SH, OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { t_skip("cannot connect to the SCM (err %lu)", GetLastError()); return; }
    svc = UIN(SH, OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS);
    if (!svc) { t_skip("cannot open RpcSs (err %lu)", GetLastError()); UIN(SH, CloseServiceHandle)(scm); return; }
    SET_ERR(0);
    r = UIN(SH, StartServiceA)(svc, 0, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    r = UIN(SH, StartServiceA)(svc, 0, NULL);               /* repeatable, no state consumed */
    CHECK(!r); EXPECT_ERR(ERROR_ACCESS_DENIED);
    UIN(SH, CloseServiceHandle)(svc); UIN(SH, CloseServiceHandle)(scm);
}

TEST(gap8_startservicea_running_service_already_running)
{
    SC_HANDLE scm, svc; BOOL r; SERVICE_STATUS st; REQUIRE(UIN(SH, StartServiceA)); REQUIRE(UIN(SH, OpenSCManagerW)); REQUIRE(UIN(SH, OpenServiceW));
    REQUIRE(UIN(SH, CloseServiceHandle)); REQUIRE(UIN(SH, QueryServiceStatus)); NOWINE();
    scm = UIN(SH, OpenSCManagerW)(NULL, NULL, SC_MANAGER_CONNECT);
    if (!scm) { t_skip("no SCM"); return; }
    svc = UIN(SH, OpenServiceW)(scm, L"RpcSs", SERVICE_QUERY_STATUS | SERVICE_START);
    if (!svc) { t_skip("no SERVICE_START right on RpcSs (err %lu)", GetLastError()); UIN(SH, CloseServiceHandle)(scm); return; }
    if (!UIN(SH, QueryServiceStatus)(svc, &st) || st.dwCurrentState != SERVICE_RUNNING) { t_skip("RpcSs not running"); goto out; }
    SET_ERR(0);
    r = UIN(SH, StartServiceA)(svc, 0, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_SERVICE_ALREADY_RUNNING);
out:
    UIN(SH, CloseServiceHandle)(svc); UIN(SH, CloseServiceHandle)(scm);
}

/* ------------------------------- GetServiceDirectory / RegistryStateKey */
typedef DWORD (WINAPI *gsd_fn)(SERVICE_STATUS_HANDLE, DWORD, PWCHAR, DWORD, PDWORD);
typedef DWORD (WINAPI *gsr_fn)(SERVICE_STATUS_HANDLE, DWORD, DWORD, HKEY *);

TEST(gap8_getservicedirectory_argument_gates_fail_identically)
{
    gsd_fn f = (gsd_fn)t_proc_in(SH, "GetServiceDirectory"); WCHAR buf[8]; DWORD req = 0x1234, e[6]; unsigned i; REQUIRE(f); NOWINE();
    /* each of these is rejected on the client side, before any RPC: NULL handle, type != 0, NULL required-length pointer */
    memset(buf, 0xcc, sizeof buf);
    e[0] = f(NULL, 0, buf, 8, &req);
    e[1] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 1, buf, 8, &req);
    e[2] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 2, buf, 8, &req);
    e[3] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 0xffffffff, buf, 8, &req);
    e[4] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 0, buf, 8, NULL);
    e[5] = f(NULL, 7, NULL, 0, NULL);
    for (i = 0; i < 6; i++) { CHECKF(e[i] != 0, "case %u returned success", i); CHECKF(e[i] == e[0], "case %u returned %lu, case 0 returned %lu", i, e[i], e[0]); }
    G8_EQ(req, 0x1234); G8_EQ(buf[0], 0xcccc);              /* nothing written */
    t_note("GetServiceDirectory argument-gate error: %lu", e[0]);
}

TEST(gap8_getserviceregistrystatekey_argument_gates_fail_identically)
{
    gsr_fn f = (gsr_fn)t_proc_in(SH, "GetServiceRegistryStateKey"); HKEY k = (HKEY)(ULONG_PTR)0x4242; DWORD e[7]; unsigned i; REQUIRE(f); NOWINE();
    e[0] = f(NULL, 0, KEY_READ, &k);
    e[1] = f(NULL, 1, KEY_READ, &k);
    e[2] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 2, KEY_READ, &k);
    e[3] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 3, KEY_READ, &k);
    e[4] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 0xffffffff, KEY_READ, &k);
    e[5] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 0, KEY_READ, NULL);
    e[6] = f((SERVICE_STATUS_HANDLE)(ULONG_PTR)0x10, 1, KEY_READ, NULL);
    for (i = 0; i < 7; i++) { CHECKF(e[i] != 0, "case %u returned success", i); CHECKF(e[i] == e[0], "case %u returned %lu, case 0 returned %lu", i, e[i], e[0]); }
    G8_EQ((ULONG_PTR)k, 0x4242);
    t_note("GetServiceRegistryStateKey argument-gate error: %lu", e[0]);
}

/* ------------------------------------------------------------- ETW */
typedef struct { EVENT_TRACE_PROPERTIES p; WCHAR name[128]; WCHAR file[128]; } PROPS;
static void props_init(PROPS *p)
{
    memset(p, 0, sizeof *p); p->p.Wnode.BufferSize = sizeof *p;
    p->p.LoggerNameOffset = offsetof(PROPS, name); p->p.LogFileNameOffset = offsetof(PROPS, file);
}

TEST(gap8_etw_stoptracew_is_controltracew_stop)
{
    ULONG (WINAPI *stop)(TRACEHANDLE, LPCWSTR, PEVENT_TRACE_PROPERTIES) = (void *)t_proc_in(SH, "StopTraceW");
    ULONG (WINAPI *ctl)(TRACEHANDLE, LPCWSTR, PEVENT_TRACE_PROPERTIES, ULONG) = (void *)t_proc_in(SH, "ControlTraceW");
    WCHAR nm[96]; PROPS a, b; unsigned i; REQUIRE(stop); REQUIRE(ctl); NOWINE();
    _snwprintf(nm, 96, L"olet_gap8_stop_%lu", GetCurrentProcessId()); nm[95] = 0;
    {
        struct { TRACEHANDLE h; const WCHAR *n; int props; } t[] = {
            { 0, NULL, 0 }, { 0, L"", 1 }, { 0, nm, 1 }, { 0, NULL, 1 }, { 0, L"olet_gap8_other", 1 },
        };
        for (i = 0; i < sizeof t / sizeof *t; i++) {
            ULONG rs, rc; props_init(&a); props_init(&b);
            rs = stop(t[i].h, t[i].n, t[i].props ? &a.p : NULL);
            rc = ctl(t[i].h, t[i].n, t[i].props ? &b.p : NULL, EVENT_TRACE_CONTROL_STOP);
            CHECKF(rs == rc, "case %u: StopTraceW=%lu ControlTraceW(STOP)=%lu", i, rs, rc);
            CHECKF(rs != 0, "case %u unexpectedly stopped something", i);
            if (!t[i].props) G8_EQ(rs, ERROR_INVALID_PARAMETER);
        }
    }
}

TEST(gap8_etw_queryalltraces_a_argument_gates)
{
    ULONG (WINAPI *q)(PEVENT_TRACE_PROPERTIES *, ULONG, PULONG) = (void *)t_proc_in(SH, "QueryAllTracesA");
    PEVENT_TRACE_PROPERTIES arr[2] = { NULL, NULL }; ULONG c = 0x55; REQUIRE(q); NOWINE();
    G8_EQ(q(NULL, 0, NULL), ERROR_INVALID_PARAMETER);       /* NULL session count */
    G8_EQ(q(arr, 1, NULL), ERROR_INVALID_PARAMETER);
    G8_EQ(q(NULL, 1, &c), ERROR_INVALID_PARAMETER);         /* count promised, no array */
    G8_EQ(c, 0x55);
    G8_EQ(q(arr, 2, &c), ERROR_INVALID_PARAMETER);          /* first element NULL */
    G8_EQ(c, 0x55);
}

TEST(gap8_etw_queryalltraces_a_matches_w_count)
{
    ULONG (WINAPI *qa)(PEVENT_TRACE_PROPERTIES *, ULONG, PULONG) = (void *)t_proc_in(SH, "QueryAllTracesA");
    ULONG (WINAPI *qw)(PEVENT_TRACE_PROPERTIES *, ULONG, PULONG) = (void *)t_proc_in(SH, "QueryAllTracesW");
    ULONG ca = 0xdead, cw = 0xdead, ra, rw; REQUIRE(qa); REQUIRE(qw); NOWINE();
    ra = qa(NULL, 0, &ca); rw = qw(NULL, 0, &cw);
    CHECKF(ca != 0xdead && cw != 0xdead, "counts not written");
    G8_EQ(ra, ca ? ERROR_MORE_DATA : 0); G8_EQ(rw, cw ? ERROR_MORE_DATA : 0);
    if (ca != cw) t_note("session count differs between A (%lu) and W (%lu): sessions changed between calls?", ca, cw);
}

TEST(gap8_etw_queryalltraces_a_fills_properties)
{
    ULONG (WINAPI *qa)(PEVENT_TRACE_PROPERTIES *, ULONG, PULONG) = (void *)t_proc_in(SH, "QueryAllTracesA");
    enum { N = 64 }; static PROPS bufs[N]; PEVENT_TRACE_PROPERTIES arr[N]; ULONG c = 0, r, i; REQUIRE(qa); NOWINE();
    for (i = 0; i < N; i++) { props_init(&bufs[i]); arr[i] = &bufs[i].p; }
    r = qa(arr, N, &c);
    if (r == ERROR_MORE_DATA) { t_skip("more than %d sessions", N); return; }
    G8_EQ(r, 0); CHECK(c <= N);
    for (i = 0; i < c && i < N; i++) {
        CHECKF(bufs[i].p.Wnode.BufferSize == sizeof(PROPS), "session %lu BufferSize clobbered", i);
        CHECKF(bufs[i].p.Wnode.HistoricalContext != 0, "session %lu has no trace handle", i);
        { char *nm = (char *)&bufs[i] + bufs[i].p.LoggerNameOffset; CHECKF(nm[0] != 0, "session %lu has an empty ANSI logger name", i); }
    }
}

static ULONG WINAPI reg_cb(WMIDPREQUESTCODE code, PVOID ctx, ULONG *size, PVOID buf) { return ERROR_SUCCESS; }

TEST(gap8_etw_registertraceguidsa_ignores_mof_strings)
{
    ULONG (WINAPI *r)(WMIDPREQUEST, PVOID, LPCGUID, ULONG, PTRACE_GUID_REGISTRATION, LPCSTR, LPCSTR, PTRACEHANDLE) = (void *)t_proc_in(SH, "RegisterTraceGuidsA");
    ULONG (WINAPI *u)(TRACEHANDLE) = (void *)t_proc_in(SH, "UnregisterTraceGuids");
    GUID g = { 0x6f1d1c0e, 0x1a8a, 0x4b8f, { 0x9a,0x00,0x6f,0x1d,0x47,0x61,0x70,0x08 } }; TRACEHANDLE h1 = 0, h2 = 0; ULONG e1, e2; REQUIRE(r); REQUIRE(u); NOWINE();
    g.Data1 ^= GetCurrentProcessId();
    e1 = r(reg_cb, NULL, &g, 0, NULL, NULL, NULL, &h1);
    G8_EQ(e1, 0);
    if (e1 == 0) { CHECK(h1 != 0); G8_EQ(u(h1), 0); }
    /* the A wrapper forwards MofImagePath / MofResourceName as NULL: arbitrary strings are not examined */
    e2 = r(reg_cb, NULL, &g, 0, NULL, "C:\\olet\\no\\such\\image.dll", "no-such-resource", &h2);
    G8_EQ(e2, e1);
    if (e2 == 0) { CHECK(h2 != 0); G8_EQ(u(h2), 0); }
}

TEST(gap8_etw_registertraceguidsa_argument_errors_match_w)
{
    ULONG (WINAPI *ra)(WMIDPREQUEST, PVOID, LPCGUID, ULONG, PTRACE_GUID_REGISTRATION, LPCSTR, LPCSTR, PTRACEHANDLE) = (void *)t_proc_in(SH, "RegisterTraceGuidsA");
    ULONG (WINAPI *rw)(WMIDPREQUEST, PVOID, LPCGUID, ULONG, PTRACE_GUID_REGISTRATION, LPCWSTR, LPCWSTR, PTRACEHANDLE) = (void *)t_proc_in(SH, "RegisterTraceGuidsW");
    GUID g = { 0x7f1d1c0e, 0x1a8a, 0x4b8f, { 0x9a,0x00,0x6f,0x1d,0x47,0x61,0x70,0x09 } }; TRACEHANDLE h = 0x77; ULONG ea, ew; REQUIRE(ra); REQUIRE(rw); NOWINE();
    ea = ra(reg_cb, NULL, NULL, 0, NULL, NULL, NULL, &h); ew = rw(reg_cb, NULL, NULL, 0, NULL, NULL, NULL, &h);
    CHECKF(ea == ew && ea != 0, "NULL ControlGuid: A=%lu W=%lu", ea, ew);
    ea = ra(reg_cb, NULL, &g, 0, NULL, NULL, NULL, NULL); ew = rw(reg_cb, NULL, &g, 0, NULL, NULL, NULL, NULL);
    CHECKF(ea == ew && ea != 0, "NULL RegistrationHandle: A=%lu W=%lu", ea, ew);
    ea = ra(NULL, NULL, &g, 0, NULL, NULL, NULL, &h); ew = rw(NULL, NULL, &g, 0, NULL, NULL, NULL, &h);
    CHECKF(ea == ew, "NULL callback: A=%lu W=%lu", ea, ew);
    if (ea == 0) { ULONG (WINAPI *u)(TRACEHANDLE) = (void *)t_proc_in(SH, "UnregisterTraceGuids"); if (u) u(h); }
    t_note("RegisterTraceGuids NULL guid -> %lu", ea);
}

/* ----------------------------------------------- oleaut32 DllCanUnloadNow */
TEST(gap8_oleaut32_dllcanunloadnow_is_always_false)
{
    HRESULT (WINAPI *f)(void) = (void *)t_proc_in("oleaut32.dll", "DllCanUnloadNow"); BSTR b; BSTR (WINAPI *alloc)(const OLECHAR *) = (void *)t_proc_in("oleaut32.dll", "SysAllocString");
    void (WINAPI *fr)(BSTR) = (void *)t_proc_in("oleaut32.dll", "SysFreeString"); int i; REQUIRE(f);
    G8_EQ(f(), S_FALSE);
    G8_EQ(f(), S_FALSE);
    if (alloc && fr) { b = alloc(L"olet gap8"); G8_EQ(f(), S_FALSE); fr(b); }
    G8_EQ(f(), S_FALSE);                                    /* still S_FALSE with nothing outstanding: oleaut32 never unloads */
    for (i = 0; i < 50; i++) if (f() != S_FALSE) { CHECK(0); break; }
}

/* ------------------------------------------------- HWND_UserMarshal */
typedef BYTE *(__cdecl *marsh_fn)(ULONG *, BYTE *, HWND *);
TEST(gap8_hwnd_usermarshal_stores_low_32_bits)
{
    static const struct { ULONG64 v; ULONG want; } t[] = {
        { 0x123456789ull, 0x23456789 }, { 0x7fffffff, 0x7fffffff }, { 0xfffffffeull, 0xfffffffe }, { 0xfffffffffffffffeull, 0xfffffffe },
        { 0x0000000100000001ull, 1 }, { 0x1000, 0x1000 }, { 1, 1 },
    };
    const char *names[2] = { "HWND_UserMarshal", "HWND_UserMarshal64" }; unsigned i; int k;
    for (k = 0; k < 2; k++) {
        marsh_fn f = (marsh_fn)t_proc_in("ole32.dll", names[k]); if (!f) { t_skip("missing %s", names[k]); return; }
        for (i = 0; i < sizeof t / sizeof *t; i++) {
            ULONG flags = 0; HWND h = (HWND)(ULONG_PTR)t[i].v; BYTE raw[40], *buf = raw, *end;
            memset(raw, 0xcc, sizeof raw);
            while (((ULONG_PTR)buf & 3) != 0) buf++;
            end = f(&flags, buf, &h);
            G8_EQ(end, buf + 8);
            G8_EQ(*(ULONG *)buf, 0x48746457);               /* 'WdtH' */
            G8_EQ(*(ULONG *)(buf + 4), t[i].want);
            G8_EQ(buf[8], 0xcc);
        }
    }
}

TEST(gap8_startservicea_null_handle_is_invalid_handle)
{
    BOOL r; REQUIRE(UIN(SH, StartServiceA)); NOWINE();
    SET_ERR(0);
    r = UIN(SH, StartServiceA)(NULL, 0, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (UIN(SH, StartServiceW)) { SET_ERR(0); r = UIN(SH, StartServiceW)(NULL, 0, NULL); CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE); }
}
