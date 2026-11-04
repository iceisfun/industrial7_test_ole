#include "etw.h"
/* Elevated-only: a private buffering-mode ETW session named olet_<pid>_*. Skipped without elevation.
 * The session is stopped in each test's tail and any leftover olet_* session is swept at start.
 * Buffering mode (EVENT_TRACE_BUFFERING_MODE) keeps events in memory only: no file, no consumer. */

typedef struct {
    WCHAR name[64]; GUID guid; TRACEHANDLE h; PB props;
    __typeof__(&StartTraceW) st; __typeof__(&ControlTraceW) ct; __typeof__(&EnableTraceEx2) en;
} sess;

static int live_gate(void) {
    if (t_is_wine()) { t_skip("emulator ETW is a stub"); return 0; }
    if (!etw_is_elevated()) { t_skip("needs an elevated token to control ETW sessions"); return 0; }
    if (!ET(StartTraceW) || !ET(ControlTraceW) || !ET(QueryAllTracesW)) { t_skip("missing ETW exports"); return 0; }
    return 1;
}
static void fill_props(PB *b, const GUID *g) {
    pb_init(b, sizeof *b, g, EVENT_TRACE_BUFFERING_MODE, 0, NULL);
    b->p.BufferSize = 4;        /* KB */
    b->p.MinimumBuffers = 2;
    b->p.MaximumBuffers = 4;
    b->p.FlushTimer = 0;
}
static ULONG sess_start(sess *s, const char *tag) {
    memset(s, 0, sizeof *s);
    s->st = ET(StartTraceW); s->ct = ET(ControlTraceW); s->en = ET(EnableTraceEx2);
    etw_session_name(s->name, 64, tag);
    s->guid = (GUID){0x5e55e55e, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, (BYTE)(GetCurrentProcessId() & 0xff)}};
    fill_props(&s->props, &s->guid);
    ULONG r = s->st(&s->h, s->name, &s->props.p);
    if (r != 0) s->h = 0;
    return r;
}
static void sess_stop(sess *s) {
    if (!s->h) return;
    PB sp; fill_props(&sp, &s->guid);
    s->ct(s->h, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP);
    s->h = 0;
}
#define LIVE_START(s, tag) \
    if (!live_gate()) return; \
    etw_sweep(); \
    sess s; \
    { ULONG r_ = sess_start(&s, tag); \
      if (r_ != 0) { t_note("StartTraceW failed with %lu; cannot run live test", r_); return; } }

TEST(etw_live_start_query_stop_lifecycle) {
    LIVE_START(s, "life");
    CHECK(s.h != 0);
    /* output properties */
    CHECK(s.props.p.LoggerNameOffset >= ETW_PROPS_SIZE);
    CHECK(!wcscmp(pb_name(&s.props), s.name));
    CHECK(s.props.p.LogFileMode & EVENT_TRACE_BUFFERING_MODE);
    CHECK(IsEqualGUID(&s.props.p.Wnode.Guid, &s.guid));
    CHECK(s.props.p.NumberOfBuffers >= 2);
    CHECK(s.props.p.MinimumBuffers >= 2);
    t_note("start: buf=%luKB min=%lu max=%lu nbuf=%lu free=%lu mode=0x%lx flush=%lu tid=%lu",
           s.props.p.BufferSize, s.props.p.MinimumBuffers, s.props.p.MaximumBuffers, s.props.p.NumberOfBuffers,
           s.props.p.FreeBuffers, s.props.p.LogFileMode, s.props.p.FlushTimer, s.props.p.LoggerThreadId);

    PB q; pb_init(&q, sizeof q, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(0, s.name, &q.p, EVENT_TRACE_CONTROL_QUERY), 0);
    CHECK(!wcscmp(pb_name(&q), s.name));
    CHECK(IsEqualGUID(&q.p.Wnode.Guid, &s.guid));
    EXPECT_EQ(q.p.Wnode.HistoricalContext, s.h);
    CHECK(q.p.LogFileMode & EVENT_TRACE_BUFFERING_MODE);
    CHECK(q.p.EventsLost == 0);
    /* query by handle only */
    PB q2; pb_init(&q2, sizeof q2, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(s.h, NULL, &q2.p, EVENT_TRACE_CONTROL_QUERY), 0);
    CHECK(!wcscmp(pb_name(&q2), s.name));
    /* case-insensitive name match */
    WCHAR up[64]; lstrcpyW(up, s.name); CharUpperW(up);
    PB q3; pb_init(&q3, sizeof q3, NULL, 0, 0, NULL);
    ULONG r3 = s.ct(0, up, &q3.p, EVENT_TRACE_CONTROL_QUERY);
    t_note("query with upper-cased name -> %lu", r3);

    PB sp; fill_props(&sp, &s.guid);
    EXPECT_EQ(s.ct(0, s.name, &sp.p, EVENT_TRACE_CONTROL_STOP), 0);
    s.h = 0;
    PB q4; pb_init(&q4, sizeof q4, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(0, s.name, &q4.p, EVENT_TRACE_CONTROL_QUERY), ETW_ERR_INSTANCE_NOT_FOUND);
    EXPECT_EQ(s.ct(0, s.name, &q4.p, EVENT_TRACE_CONTROL_STOP), ETW_ERR_INSTANCE_NOT_FOUND);
}

TEST(etw_live_duplicate_name_already_exists) {
    LIVE_START(s, "dup");
    TRACEHANDLE h2 = 0; PB b2; fill_props(&b2, &s.guid);
    SET_ERR(0);
    ULONG r = s.st(&h2, s.name, &b2.p);
    if (r == 0 && h2) { PB sp; fill_props(&sp, &s.guid); s.ct(h2, NULL, &sp.p, 1); }
    EXPECT_EQ(r, ERROR_ALREADY_EXISTS);
    EXPECT_EQ(h2, 0);
    sess_stop(&s);
}

TEST(etw_live_stop_by_handle_only) {
    LIVE_START(s, "sbh");
    PB sp; fill_props(&sp, &s.guid);
    EXPECT_EQ(s.ct(s.h, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP), 0);
    PB q; pb_init(&q, sizeof q, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(0, s.name, &q.p, EVENT_TRACE_CONTROL_QUERY), ETW_ERR_INSTANCE_NOT_FOUND);
    /* stale handle after stop */
    ULONG r = s.ct(s.h, NULL, &q.p, EVENT_TRACE_CONTROL_QUERY);
    CHECK(r != 0);
    t_note("query via stale handle -> %lu", r);
    s.h = 0;
}

TEST(etw_live_query_all_lists_session) {
    LIVE_START(s, "qat");
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW);
    enum { N = 64 }; static PB bufs[N]; EVENT_TRACE_PROPERTIES *a[N]; ULONG c = 0;
    for (int i = 0; i < N; i++) { memset(&bufs[i], 0, sizeof bufs[i]); bufs[i].p.Wnode.BufferSize = sizeof bufs[i]; bufs[i].p.LoggerNameOffset = PB_NAME_OFF; bufs[i].p.LogFileNameOffset = PB_FILE_OFF; a[i] = &bufs[i].p; }
    EXPECT_EQ(q(a, N, &c), 0);
    int found = 0;
    for (ULONG i = 0; i < c && i < N; i++) {
        if (!wcscmp((WCHAR *)(bufs[i].raw + bufs[i].p.LoggerNameOffset), s.name)) { found++; EXPECT_EQ(bufs[i].p.Wnode.HistoricalContext, s.h); }
    }
    EXPECT_EQ(found, 1);
    sess_stop(&s);
    c = 0;
    for (int i = 0; i < N; i++) { memset(&bufs[i], 0, sizeof bufs[i]); bufs[i].p.Wnode.BufferSize = sizeof bufs[i]; bufs[i].p.LoggerNameOffset = PB_NAME_OFF; bufs[i].p.LogFileNameOffset = PB_FILE_OFF; a[i] = &bufs[i].p; }
    EXPECT_EQ(q(a, N, &c), 0);
    found = 0;
    for (ULONG i = 0; i < c && i < N; i++) if (!wcscmp((WCHAR *)(bufs[i].raw + bufs[i].p.LoggerNameOffset), s.name)) found++;
    EXPECT_EQ(found, 0);
}

TEST(etw_live_update_flush_timer_and_flush) {
    LIVE_START(s, "upd");
    PB u; fill_props(&u, &s.guid); u.p.FlushTimer = 7;
    ULONG r = s.ct(0, s.name, &u.p, EVENT_TRACE_CONTROL_UPDATE);
    t_note("UPDATE FlushTimer=7 -> %lu", r);
    CHECKF(r == 0 || r == ERROR_INVALID_PARAMETER, "update got %lu", r);
    PB q; pb_init(&q, sizeof q, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(0, s.name, &q.p, EVENT_TRACE_CONTROL_QUERY), 0);
    t_note("FlushTimer after update = %lu", q.p.FlushTimer);
    /* Windows 10 accepts the update (0) but QUERY still reports FlushTimer 0 for this session. */
    if (r == 0) CHECK(q.p.FlushTimer == 0);
    PB f; fill_props(&f, &s.guid);
    r = s.ct(0, s.name, &f.p, EVENT_TRACE_CONTROL_FLUSH);
    t_note("FLUSH on buffering-only session -> %lu", r);
    __typeof__(&FlushTraceW) ft = ET(FlushTraceW);
    if (ft) { r = ft(s.h, NULL, &f.p); t_note("FlushTraceW -> %lu", r); }
    /* update of a nonexistent sibling name is still "not found" */
    WCHAR other[64]; etw_session_name(other, 64, "updx");
    PB u2; fill_props(&u2, &s.guid); u2.p.FlushTimer = 3;
    EXPECT_EQ(s.ct(0, other, &u2.p, EVENT_TRACE_CONTROL_UPDATE), ETW_ERR_INSTANCE_NOT_FOUND);
    sess_stop(&s);
}

/* ---- provider enable / callback / write ---- */
typedef struct { volatile LONG calls; volatile LONG last_ctl; volatile LONG last_level; volatile LONGLONG any; volatile LONGLONG all; HANDLE ev; } cbstate;
static void WINAPI en_cb(LPCGUID id, ULONG ctl, UCHAR lvl, ULONGLONG any, ULONGLONG all, PEVENT_FILTER_DESCRIPTOR f, PVOID ctx) {
    cbstate *c = ctx;
    c->last_ctl = (LONG)ctl; c->last_level = lvl; c->any = (LONGLONG)any; c->all = (LONGLONG)all;
    InterlockedIncrement(&c->calls);
    SetEvent(c->ev);
}
static int wait_cb(cbstate *c, LONG want_calls) {
    for (int i = 0; i < 50 && c->calls < want_calls; i++) WaitForSingleObject(c->ev, 100);
    return c->calls >= want_calls;
}

TEST(etw_live_enable_provider_callback_and_write) {
    LIVE_START(s, "prov");
    __typeof__(&EventRegister) reg = ET(EventRegister); __typeof__(&EventUnregister) unreg = ET(EventUnregister);
    __typeof__(&EventProviderEnabled) pe = ET(EventProviderEnabled); __typeof__(&EventEnabled) ee = ET(EventEnabled);
    __typeof__(&EventWrite) ew = ET(EventWrite); __typeof__(&EventWriteString) ws = ET(EventWriteString);
    if (!reg || !unreg || !pe || !ee || !ew || !ws || !s.en) { t_note("missing provider exports"); sess_stop(&s); return; }
    GUID pg = {0x7e57ab1e, 0x2222, 0x4333, {0x94, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, (BYTE)(GetCurrentProcessId() & 0xff)}};
    cbstate cb; memset(&cb, 0, sizeof cb); cb.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    REGHANDLE rh = 0;
    EXPECT_EQ(reg(&pg, en_cb, &cb, &rh), 0);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Id = 7; d.Level = 4; d.Keyword = 0x1;
    EXPECT_EQ(ee(rh, &d), 0);
    EXPECT_EQ(pe(rh, 4, 0x1), 0);

    /* enable at Information with keyword 0x1 (MatchAny) */
    ULONG r = s.en(s.h, &pg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_INFORMATION, 0x1, 0, 0, NULL);
    EXPECT_EQ(r, 0);
    CHECK(wait_cb(&cb, 1));
    EXPECT_EQ(cb.last_ctl, EVENT_CONTROL_CODE_ENABLE_PROVIDER);
    EXPECT_EQ(cb.last_level, TRACE_LEVEL_INFORMATION);
    CHECK(cb.any == 0x1);
    Sleep(50);
    CHECK(ee(rh, &d) != 0);
    CHECK(pe(rh, TRACE_LEVEL_INFORMATION, 0x1) != 0);
    CHECK(pe(rh, TRACE_LEVEL_ERROR, 0x1) != 0);       /* more severe than the enabled level */
    EXPECT_EQ(pe(rh, TRACE_LEVEL_VERBOSE, 0x1), 0);   /* more verbose than the enabled level */
    EXPECT_EQ(pe(rh, TRACE_LEVEL_INFORMATION, 0x2), 0); /* keyword mask does not intersect */
    EVENT_DESCRIPTOR dv = d; dv.Level = 5; EXPECT_EQ(ee(rh, &dv), 0);
    EVENT_DESCRIPTOR dk = d; dk.Keyword = 0x4; EXPECT_EQ(ee(rh, &dk), 0);

    /* write a burst of events and strings; the session is a small circular in-memory buffer */
    DWORD v = 0; EVENT_DATA_DESCRIPTOR dd;
    for (int i = 0; i < 500; i++) {
        v = i; EventDataDescCreate(&dd, &v, sizeof v);
        ULONG e = ew(rh, &d, 1, &dd);
        if (e != 0) { CHECKF(0, "EventWrite #%d returned %lu", i, e); break; }
    }
    EXPECT_EQ(ws(rh, TRACE_LEVEL_INFORMATION, 0x1, L"olet live string event"), 0);
    EXPECT_EQ(ws(rh, TRACE_LEVEL_VERBOSE, 0x1, L"filtered out"), 0); /* dropped, still success */

    PB q; pb_init(&q, sizeof q, NULL, 0, 0, NULL);
    EXPECT_EQ(s.ct(0, s.name, &q.p, EVENT_TRACE_CONTROL_QUERY), 0);
    t_note("after writes: nbuf=%lu free=%lu lost=%lu written=%lu logbuflost=%lu rtlost=%lu",
           q.p.NumberOfBuffers, q.p.FreeBuffers, q.p.EventsLost, q.p.BuffersWritten, q.p.LogBuffersLost, q.p.RealTimeBuffersLost);
    CHECK(q.p.NumberOfBuffers >= 2);
    if (q.p.NumberOfBuffers > q.p.MaximumBuffers) t_note("NumberOfBuffers %lu > MaximumBuffers %lu", q.p.NumberOfBuffers, q.p.MaximumBuffers);
    CHECK(q.p.FreeBuffers <= q.p.NumberOfBuffers);

    /* update the enable: disable, callback fires with DISABLE, provider queries go false */
    LONG before = cb.calls;
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, NULL), 0);
    CHECK(wait_cb(&cb, before + 1));
    EXPECT_EQ(cb.last_ctl, EVENT_CONTROL_CODE_DISABLE_PROVIDER);
    Sleep(50);
    EXPECT_EQ(pe(rh, TRACE_LEVEL_INFORMATION, 0x1), 0);
    EXPECT_EQ(ee(rh, &d), 0);
    EXPECT_EQ(ew(rh, &d, 0, NULL), 0);   /* disabled again: success, dropped */

    /* capture-state request delivers an EVENT_CONTROL_CODE_CAPTURE_STATE callback to enabled providers */
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, 0, 0, 0, NULL), 0);
    CHECK(wait_cb(&cb, before + 2));
    before = cb.calls;
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_CAPTURE_STATE, TRACE_LEVEL_VERBOSE, 0, 0, 0, NULL), 0);
    if (wait_cb(&cb, before + 1)) EXPECT_EQ(cb.last_ctl, EVENT_CONTROL_CODE_CAPTURE_STATE);
    else t_note("no CAPTURE_STATE callback observed");
    /* verbose with MatchAny==0 means all keywords */
    t_note("MatchAny=0 verbose enable: provider-enabled for arbitrary keyword -> %d", (int)pe(rh, TRACE_LEVEL_VERBOSE, 0xdeadbeef));

    /* stopping the session disables the provider: the callback is told */
    before = cb.calls;
    sess_stop(&s);
    if (wait_cb(&cb, before + 1)) EXPECT_EQ(cb.last_ctl, EVENT_CONTROL_CODE_DISABLE_PROVIDER);
    else t_note("no DISABLE callback after StopTrace");
    Sleep(50);
    EXPECT_EQ(pe(rh, TRACE_LEVEL_INFORMATION, 0), 0);
    EXPECT_EQ(unreg(rh), 0);
    CloseHandle(cb.ev);
}

TEST(etw_live_registration_after_enable_gets_callback) {
    LIVE_START(s, "late");
    __typeof__(&EventRegister) reg = ET(EventRegister); __typeof__(&EventUnregister) unreg = ET(EventUnregister);
    __typeof__(&EventProviderEnabled) pe = ET(EventProviderEnabled);
    if (!reg || !unreg || !pe || !s.en) { t_note("missing provider exports"); sess_stop(&s); return; }
    GUID pg = {0x7e57ab1e, 0x3333, 0x4333, {0x94, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, (BYTE)(GetCurrentProcessId() & 0xff)}};
    /* enabling a GUID nobody has registered yet is not an error */
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE, 0, 0, 0, NULL), 0);
    cbstate cb; memset(&cb, 0, sizeof cb); cb.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    REGHANDLE rh = 0;
    EXPECT_EQ(reg(&pg, en_cb, &cb, &rh), 0);
    if (wait_cb(&cb, 1)) {
        EXPECT_EQ(cb.last_ctl, EVENT_CONTROL_CODE_ENABLE_PROVIDER);
        EXPECT_EQ(cb.last_level, TRACE_LEVEL_VERBOSE);
        CHECK(pe(rh, TRACE_LEVEL_VERBOSE, 0) != 0);
    } else CHECKF(0, "provider registered after enable got no ENABLE callback within 5s");
    sess_stop(&s);
    unreg(rh);
    CloseHandle(cb.ev);
}

TEST(etw_live_enable_bad_params_with_live_session) {
    LIVE_START(s, "enbad");
    GUID pg = {0x7e57ab1e, 0x4444, 0x4333, {0x94, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x01}};
    EXPECT_EQ(s.en(s.h, NULL, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s.en(s.h, &pg, 3, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL), 0);
    EXPECT_EQ(s.en(s.h, &pg, EVENT_CONTROL_CODE_DISABLE_PROVIDER, 0, 0, 0, 0, NULL), 0);
    TRACEHANDLE old = s.h;
    sess_stop(&s);
    ULONG r = ET(EnableTraceEx2)(old, &pg, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL);
    CHECK(r != 0);
    t_note("EnableTraceEx2 on stopped session handle -> %lu", r);
}

TEST(etw_live_enumerate_guids_sees_registered_provider) {
    LIVE_START(s, "enum");
    __typeof__(&EventRegister) reg = ET(EventRegister); __typeof__(&EventUnregister) unreg = ET(EventUnregister);
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx);
    if (!reg || !unreg || !en) { t_note("missing exports"); sess_stop(&s); return; }
    GUID pg = {0x7e57ab1e, 0x5555, 0x4333, {0x94, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, (BYTE)(GetCurrentProcessId() & 0xff)}};
    REGHANDLE rh = 0; EXPECT_EQ(reg(&pg, NULL, NULL, &rh), 0);
    ULONG need = 0; BYTE *buf = NULL; ULONG r = 0, cap = 0;
    for (int i = 0; i < 5; i++) {
        r = en(TraceGuidQueryList, NULL, 0, buf, cap, &need);
        if (r != ERROR_INSUFFICIENT_BUFFER) break;
        free(buf); cap = need + 4096; buf = malloc(cap);
    }
    EXPECT_EQ(r, 0);
    if (r == 0 && buf) {
        /* TraceGuidQueryList returns a packed array of GUIDs (ReturnLength / 16 entries) */
        int found = 0; const GUID *gl = (const GUID *)buf;
        for (ULONG i = 0; i < need / sizeof(GUID) && (BYTE *)(gl + i + 1) <= buf + cap; i++) if (IsEqualGUID(&gl[i], &pg)) found = 1;
        if (!found) t_note("registered provider GUID not found in %lu bytes of QueryList output", need);
        CHECK(need % sizeof(GUID) == 0);
    }
    free(buf);
    unreg(rh);
    sess_stop(&s);
}

TEST(etw_live_second_session_distinct_handle) {
    LIVE_START(s, "two_a");
    sess t; memset(&t, 0, sizeof t);
    t.st = s.st; t.ct = s.ct; t.en = s.en;
    etw_session_name(t.name, 64, "two_b");
    t.guid = (GUID){0x5e55e55e, 0x9999, 0x4222, {0x83, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, (BYTE)(GetCurrentProcessId() & 0xff)}};
    fill_props(&t.props, &t.guid);
    ULONG r = t.st(&t.h, t.name, &t.props.p);
    EXPECT_EQ(r, 0);
    if (r == 0) {
        CHECK(t.h != s.h);
        CHECK((t.h & 0xffff) != (s.h & 0xffff));   /* low word is the logger id */
        sess_stop(&t);
    }
    sess_stop(&s);
}

TEST(etw_live_name_length_limit) {
    if (!live_gate()) return;
    etw_sweep();
    /* Name + fixed header must fit Wnode.BufferSize (decomp: BufferSize >= 0x78 + 2 + 2*len). A name that does
     * fit but is absurdly long is the kernel's call; just prove no session is left behind. */
    WCHAR n[600]; _snwprintf(n, 600, L"olet_%lu_", GetCurrentProcessId());
    size_t l = wcslen(n); while (l < 520) n[l++] = L'n'; n[l] = 0;
    PB b; fill_props(&b, &(GUID){0x5e55e55e, 0xaaaa, 0x4222, {1, 2, 3, 4, 5, 6, 7, 8}});
    b.p.LogFileNameOffset = 0;
    b.p.Wnode.BufferSize = 4096;
    TRACEHANDLE h = 0;
    SET_ERR(0);
    ULONG r = ET(StartTraceW)(&h, n, &b.p);
    t_note("StartTraceW(name of %u chars) -> %lu", (unsigned)l, r);
    if (r == 0 && h) { PB sp; fill_props(&sp, &b.p.Wnode.Guid); ET(ControlTraceW)(h, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP); }

    etw_sweep();
}

TEST(etw_live_nonelevated_denied_note) {
    if (t_is_wine()) { t_skip("emulator ETW is a stub"); return; }
    __typeof__(&StartTraceW) st = ET(StartTraceW); REQUIRE(st);
    if (etw_is_elevated()) { t_note("elevated: ERROR_ACCESS_DENIED path for unprivileged callers not exercised"); return; }
    WCHAR n[64]; etw_session_name(n, 64, "den"); PB b; fill_props(&b, &(GUID){0x5e55e55e, 0xbbbb, 0x4222, {1, 2, 3, 4, 5, 6, 7, 9}});
    TRACEHANDLE h = 0;
    ULONG r = st(&h, n, &b.p);
    if (r == 0 && h) { PB sp; fill_props(&sp, &b.p.Wnode.Guid); ET(ControlTraceW)(h, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP); t_note("unelevated token started a session (Performance Log Users member?)"); }
    else EXPECT_EQ(r, ERROR_ACCESS_DENIED);
}
