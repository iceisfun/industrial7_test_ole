#include "etw.h"
/* ControlTraceW/A, QueryTraceW, StopTraceW, FlushTraceW, QueryAllTraces validation. Rejected in
 * sechost (decomp of ControlTraceW / EtwpValidateTraceProperties / EtwpQueryAllTraces) or answered
 * by the kernel with "no such logger" for names that cannot exist; needs no elevation. */

static void nosuch(WCHAR *n, size_t cap, const char *tag) { etw_session_name(n, cap, tag); }

/* ---- NULL / short Properties ---- */
TEST(etw_ctl_null_props_all_codes) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    WCHAR n[64]; nosuch(n, 64, "cnp");
    for (ULONG code = 0; code <= 5; code++) {
        SET_ERR(0);
        ULONG r = ct(0, n, NULL, code);
        CHECKF(r == ERROR_INVALID_PARAMETER, "code %lu got %lu", code, r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(etw_ctl_a_null_props) {
    __typeof__(&ControlTraceA) ct = ET(ControlTraceA); ETW_REQ(ct);
    char n[64]; etw_session_name_a(n, 64, "acnp");
    SET_ERR(0); EXPECT_EQ(ct(0, n, NULL, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
#define CBUF_TEST(tag, sz) \
TEST(etw_ctl_bufsize_##tag) { \
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct); \
    PB b; pb_default(&b); b.p.Wnode.BufferSize = (sz); WCHAR n[64]; nosuch(n, 64, "cb"); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY), ETW_ERR_BAD_LENGTH); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_STOP), ETW_ERR_BAD_LENGTH); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_UPDATE), ETW_ERR_BAD_LENGTH); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_FLUSH), ETW_ERR_BAD_LENGTH); \
}
CBUF_TEST(zero, 0)
CBUF_TEST(one, 1)
CBUF_TEST(just_under, 0x77)
CBUF_TEST(wnode_only, 0x30)

#define COFF_TEST(tag, field, value) \
TEST(etw_ctl_##tag) { \
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct); \
    PB b; pb_default(&b); b.p.field = (value); WCHAR n[64]; nosuch(n, 64, "co"); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER); \
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_STOP), ERROR_INVALID_PARAMETER); \
}
COFF_TEST(loggername_off_in_header, LoggerNameOffset, 0x40)
COFF_TEST(loggername_off_past_end, LoggerNameOffset, 0x401)
COFF_TEST(logfile_off_in_header, LogFileNameOffset, 0x40)
COFF_TEST(logfile_off_past_end, LogFileNameOffset, 0x401)

/* ---- handle 0 requires a non-empty name ---- */
TEST(etw_ctl_handle0_null_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b);
    SET_ERR(0);
    ULONG r = ct(0, NULL, &b.p, EVENT_TRACE_CONTROL_QUERY);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_handle0_empty_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b);
    SET_ERR(0);
    EXPECT_EQ(ct(0, L"", &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ct(0, L"", &b.p, EVENT_TRACE_CONTROL_STOP), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_a_handle0_null_name) {
    __typeof__(&ControlTraceA) ct = ET(ControlTraceA); ETW_REQ(ct);
    PB b; pb_default(&b);
    EXPECT_EQ(ct(0, NULL, &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ct(0, "", &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_kernel_guid_with_other_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_init(&b, 0x400, &kSystemTraceControlGuid, EVENT_TRACE_BUFFERING_MODE, 0, NULL);
    WCHAR n[64]; nosuch(n, 64, "ckg");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_ckcl_guid_with_other_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_init(&b, 0x400, &kCKCLGuid, EVENT_TRACE_BUFFERING_MODE, 0, NULL);
    WCHAR n[64]; nosuch(n, 64, "cck");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY), ERROR_INVALID_PARAMETER);
}

/* ---- unknown control codes ---- */
#define CODE_TEST(code) \
TEST(etw_ctl_bad_code_##code) { \
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct); \
    PB b; pb_default(&b); WCHAR n[64]; nosuch(n, 64, "bc"); \
    SET_ERR(0); \
    ULONG r = ct(0, n, &b.p, (code)); \
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER); \
}
CODE_TEST(6)
CODE_TEST(7)
CODE_TEST(100)
CODE_TEST(0xffffffffu)

/* ---- sessions that do not exist ---- */
TEST(etw_ctl_query_nonexistent_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "q1");
    SET_ERR(0);
    ULONG r = ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY);
    EXPECT_EQ(r, ETW_ERR_INSTANCE_NOT_FOUND);
    if (GetLastError() != r) t_note("query: GetLastError=%lu return=%lu", GetLastError(), r);
}
TEST(etw_ctl_stop_nonexistent_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "s1");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_STOP), ETW_ERR_INSTANCE_NOT_FOUND);
}
TEST(etw_ctl_flush_nonexistent_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "f1");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_FLUSH), ETW_ERR_INSTANCE_NOT_FOUND);
}
TEST(etw_ctl_update_nonexistent_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; b.p.FlushTimer = 3; WCHAR n[64]; nosuch(n, 64, "u1");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_UPDATE), ETW_ERR_INSTANCE_NOT_FOUND);
}
TEST(etw_ctl_a_query_nonexistent_name) {
    __typeof__(&ControlTraceA) ct = ET(ControlTraceA); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; char n[64]; etw_session_name_a(n, 64, "aq");
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_QUERY), ETW_ERR_INSTANCE_NOT_FOUND);
    EXPECT_EQ(ct(0, n, &b.p, EVENT_TRACE_CONTROL_STOP), ETW_ERR_INSTANCE_NOT_FOUND);
}
TEST(etw_ctl_stopTraceW_nonexistent) {
    __typeof__(&StopTraceW) sp = ET(StopTraceW); ETW_REQ(sp);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "st");
    SET_ERR(0);
    EXPECT_EQ(sp(0, n, &b.p), ETW_ERR_INSTANCE_NOT_FOUND);
    /* StopTraceW is ControlTraceW(.., STOP): the same validation applies. */
    EXPECT_EQ(sp(0, n, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(sp(0, NULL, &b.p), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_querytraceW_wrapper) {
    __typeof__(&QueryTraceW) q = ET(QueryTraceW); ETW_REQ(q);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "qw");
    EXPECT_EQ(q(0, n, &b.p), ETW_ERR_INSTANCE_NOT_FOUND);
    EXPECT_EQ(q(0, n, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_flushtraceW_wrapper) {
    __typeof__(&FlushTraceW) f = ET(FlushTraceW); ETW_REQ(f);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; WCHAR n[64]; nosuch(n, 64, "fw");
    EXPECT_EQ(f(0, n, &b.p), ETW_ERR_INSTANCE_NOT_FOUND);
    EXPECT_EQ(f(0, n, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_ctl_queryA_flushA_wrappers) {
    __typeof__(&QueryTraceA) q = ET(QueryTraceA); __typeof__(&FlushTraceA) f = ET(FlushTraceA); ETW_REQ(q);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b; char n[64]; etw_session_name_a(n, 64, "qa");
    EXPECT_EQ(q(0, n, &b.p), ETW_ERR_INSTANCE_NOT_FOUND);
    if (f) EXPECT_EQ(f(0, n, &b.p), ETW_ERR_INSTANCE_NOT_FOUND);
}
TEST(etw_ctl_bogus_handle) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    /* A handle whose logger id is out of range or inactive: documented as invalid handle / not found. */
    static const TRACEHANDLE hs[] = {1, 0x1234, 0xffff, 0x7fffffffffffffffULL};
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b;
        ULONG r = ct(hs[i], NULL, &b.p, EVENT_TRACE_CONTROL_QUERY);
        CHECKF(r == ERROR_INVALID_HANDLE || r == ETW_ERR_INSTANCE_NOT_FOUND || r == ERROR_INVALID_PARAMETER || r == ERROR_ACCESS_DENIED, "handle %llx got %lu", (unsigned long long)hs[i], r);
        t_note("ControlTrace(handle=%llx, NULL name, query) -> %lu", (unsigned long long)hs[i], r);
    }
}
TEST(etw_ctl_invalid_processtrace_handle_value) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = sizeof b;
    ULONG r = ct((TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE, NULL, &b.p, EVENT_TRACE_CONTROL_QUERY);
    CHECK(r != 0);
    t_note("ControlTrace(INVALID_PROCESSTRACE_HANDLE) -> %lu", r);
}

/* ---- the well-known kernel loggers need privilege; a query by name must not crash and must report
 * either absence or access denied. ---- */
TEST(etw_ctl_query_nt_kernel_logger_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_init(&b, sizeof b, NULL, 0, 0, NULL);
    ULONG r = ct(0, L"NT Kernel Logger", &b.p, EVENT_TRACE_CONTROL_QUERY);
    CHECKF(r == 0 || r == ETW_ERR_INSTANCE_NOT_FOUND || r == ERROR_ACCESS_DENIED, "got %lu", r);
    /* the name forces the SystemTraceControl GUID into the caller's structure */
    CHECK(IsEqualGUID(&b.p.Wnode.Guid, &kSystemTraceControlGuid) || r == ERROR_ACCESS_DENIED);
    t_note("query 'NT Kernel Logger' -> %lu (elevated=%d)", r, etw_is_elevated());
}
TEST(etw_ctl_query_ckcl_name) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_init(&b, sizeof b, NULL, 0, 0, NULL);
    ULONG r = ct(0, L"Circular Kernel Context Logger", &b.p, EVENT_TRACE_CONTROL_QUERY);
    CHECKF(r == 0 || r == ETW_ERR_INSTANCE_NOT_FOUND || r == ERROR_ACCESS_DENIED, "got %lu", r);
    t_note("query CKCL -> %lu", r);
}
TEST(etw_ctl_query_wellknown_eventlog_system) {
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW); ETW_REQ(ct);
    PB b; pb_init(&b, sizeof b, NULL, 0, 0, NULL);
    ULONG r = ct(0, L"EventLog-System", &b.p, EVENT_TRACE_CONTROL_QUERY);
    CHECKF(r == 0 || r == ERROR_ACCESS_DENIED || r == ETW_ERR_INSTANCE_NOT_FOUND, "got %lu", r);
    if (r == 0) {
        CHECK(b.p.LoggerNameOffset >= ETW_PROPS_SIZE);
        
        t_note("NumberOfBuffers=%lu MinimumBuffers=%lu", b.p.NumberOfBuffers, b.p.MinimumBuffers);
        t_note("EventLog-System buffers=%lu size=%luKB mode=0x%lx", b.p.NumberOfBuffers, b.p.BufferSize, b.p.LogFileMode);
    } else t_note("EventLog-System query -> %lu", r);
}

/* ---- QueryAllTraces ---- */
TEST(etw_qat_null_count) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    PB b; pb_default(&b); EVENT_TRACE_PROPERTIES *a[1] = {&b.p};
    SET_ERR(0);
    EXPECT_EQ(q(a, 1, NULL), ERROR_INVALID_PARAMETER); /* ETW returns the status; it does not set the thread last-error */ EXPECT_ERR(0);
    EXPECT_EQ(q(NULL, 0, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_qat_null_array_with_count) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    ULONG c = 0x55;
    EXPECT_EQ(q(NULL, 1, &c), ERROR_INVALID_PARAMETER);
}
TEST(etw_qat_null_first_element) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    EVENT_TRACE_PROPERTIES *a[2] = {NULL, NULL}; ULONG c = 0;
    EXPECT_EQ(q(a, 2, &c), ERROR_INVALID_PARAMETER);
}
TEST(etw_qat_a_null_count) {
    __typeof__(&QueryAllTracesA) q = ET(QueryAllTracesA); ETW_REQ(q);
    EXPECT_EQ(q(NULL, 0, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_qat_size_query_protocol) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    /* Count 0 / no array: returns the number of active sessions in *LoggerCount, with
     * ERROR_MORE_DATA when there is at least one. */
    ULONG c = 0xdead; SET_ERR(0);
    ULONG r = q(NULL, 0, &c);
    CHECKF(c != 0xdead, "LoggerCount untouched");
    EXPECT_EQ(r, c ? ERROR_MORE_DATA : 0);
    t_note("active sessions visible to this token: %lu", c);
}
TEST(etw_qat_full_fill_consistent) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    enum { N = 64 }; static PB bufs[N]; EVENT_TRACE_PROPERTIES *a[N]; ULONG c = 0, c0 = 0;
    q(NULL, 0, &c0);
    for (int i = 0; i < N; i++) { memset(&bufs[i], 0, sizeof bufs[i]); bufs[i].p.Wnode.BufferSize = sizeof bufs[i]; bufs[i].p.LoggerNameOffset = PB_NAME_OFF; bufs[i].p.LogFileNameOffset = PB_FILE_OFF; a[i] = &bufs[i].p; }
    ULONG r = q(a, N, &c);
    EXPECT_EQ(r, 0);
    CHECK(c <= N);
    /* sessions can come and go, but a stable VM keeps the count within a small window */
    if (c != c0) t_note("count changed between calls: %lu -> %lu", c0, c);
    for (ULONG i = 0; i < c && i < N; i++) {
        CHECKF(bufs[i].p.Wnode.BufferSize == sizeof bufs[i], "session %lu BufferSize preserved", i);
        CHECKF(bufs[i].p.LoggerNameOffset >= ETW_PROPS_SIZE, "session %lu name offset 0x%lx", i, bufs[i].p.LoggerNameOffset);
        const WCHAR *nm = (WCHAR *)(bufs[i].raw + bufs[i].p.LoggerNameOffset);
        CHECK(nm[0] != 0);
    }
}
TEST(etw_qat_too_small_array_more_data) {
    __typeof__(&QueryAllTracesW) q = ET(QueryAllTracesW); ETW_REQ(q);
    ULONG c0 = 0; q(NULL, 0, &c0);
    if (c0 < 2) { t_skip("fewer than two visible sessions"); return; }
    static PB b; memset(&b, 0, sizeof b); b.p.Wnode.BufferSize = sizeof b; b.p.LoggerNameOffset = PB_NAME_OFF; b.p.LogFileNameOffset = PB_FILE_OFF;
    EVENT_TRACE_PROPERTIES *a[1] = {&b.p}; ULONG c = 0;
    ULONG r = q(a, 1, &c);
    EXPECT_EQ(r, ERROR_MORE_DATA);
    CHECK(c >= 2);
}
