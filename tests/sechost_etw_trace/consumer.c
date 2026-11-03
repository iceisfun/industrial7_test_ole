#include "etw.h"
/* Consumer side and misc controller exports: OpenTraceW/ProcessTrace/CloseTrace, EnableTraceEx2,
 * EnumerateTraceGuidsEx, TraceSet/QueryInformation, GetTraceLoggerHandle/EnableLevel. Validation
 * paths verified in the Win10 decomp (sechost / advapi32->ntdll Etw*). */

#define INVALID_PT ((TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE)

static GUID test_guid(DWORD salt) {
    GUID g = {0x01e7c0de, 0x7e57, 0x4e7c, {0xa1, 0xb2, 0xc3, 0xd4, 0, 0, 0, 0}};
    DWORD pid = GetCurrentProcessId();
    memcpy(g.Data4 + 4, &pid, 4); g.Data1 ^= salt; return g;
}

/* ---- CloseTrace ---- */
TEST(etw_close_invalid_handles) {
    __typeof__(&CloseTrace) c = ET(CloseTrace); ETW_REQ(c);
    static const TRACEHANDLE hs[] = {0, (TRACEHANDLE)-1, 1, 2, 0x1234, 0x100000000ULL, (TRACEHANDLE)-2};
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++) {
        ULONG r = c(hs[i]);
        CHECKF(r == ERROR_INVALID_HANDLE, "handle %llx got %lu", (unsigned long long)hs[i], r);
    }
}

/* ---- OpenTraceW ---- */
TEST(etw_open_null_logfile) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); ETW_REQ(o);
    SET_ERR(0);
    TRACEHANDLE h = o(NULL);
    EXPECT_EQ(h, INVALID_PT); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_open_missing_file) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); ETW_REQ(o);
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    char p[300]; t_tmpfile(p, sizeof p, "does_not_exist.etl");
    WCHAR w[300]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, 300);
    lf.LogFileName = w; lf.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    SET_ERR(0);
    TRACEHANDLE h = o(&lf);
    EXPECT_EQ(h, INVALID_PT);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND, "last error %lu", e);
    t_note("OpenTraceW(missing file) last error = %lu", e);
    if (h != INVALID_PT) ET(CloseTrace)(h);
}
TEST(etw_open_no_file_no_realtime) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); ETW_REQ(o);
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD; /* neither LogFileName nor LoggerName */
    SET_ERR(0);
    TRACEHANDLE h = o(&lf);
    EXPECT_EQ(h, INVALID_PT);
    t_note("OpenTraceW(no names) last error = %lu", GetLastError());
    if (h != INVALID_PT) ET(CloseTrace)(h);
}
TEST(etw_open_realtime_no_name) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); ETW_REQ(o);
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    SET_ERR(0);
    TRACEHANDLE h = o(&lf);
    t_note("OpenTraceW(real-time, LoggerName=NULL) -> %llx last error %lu", (unsigned long long)h, GetLastError());
    if (h != INVALID_PT) ET(CloseTrace)(h);
}
TEST(etw_open_realtime_nonexistent_session_lifecycle) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); __typeof__(&CloseTrace) c = ET(CloseTrace); ETW_REQ(o); REQUIRE(c);
    WCHAR n[64]; etw_session_name(n, 64, "ors");
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    lf.LoggerName = n; lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = NULL;
    TRACEHANDLE h = o(&lf);
    if (h == INVALID_PT) { t_note("OpenTraceW(real-time, missing session) failed: %lu", GetLastError()); return; }
    /* A real-time handle is only a local context; the session is looked up at ProcessTrace time. */
    EXPECT_EQ(c(h), 0);
    EXPECT_EQ(c(h), ERROR_INVALID_HANDLE);   /* handle is gone after close */
}
TEST(etw_open_distinct_handles_and_double_close) {
    __typeof__(&OpenTraceW) o = ET(OpenTraceW); __typeof__(&CloseTrace) c = ET(CloseTrace); ETW_REQ(o); REQUIRE(c);
    WCHAR n[64]; etw_session_name(n, 64, "odh");
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    lf.LoggerName = n; lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    TRACEHANDLE a = o(&lf), b = o(&lf);
    if (a == INVALID_PT || b == INVALID_PT) { t_note("real-time OpenTraceW failed (%lu)", GetLastError()); if (a != INVALID_PT) c(a); if (b != INVALID_PT) c(b); return; }
    CHECK(a != b);
    CHECK(a != 0 && b != 0);
    EXPECT_EQ(c(a), 0);
    EXPECT_EQ(c(b), 0);
    EXPECT_EQ(c(a), ERROR_INVALID_HANDLE);
}

/* ---- ProcessTrace argument validation ---- */
TEST(etw_process_zero_count) {
    __typeof__(&ProcessTrace) p = ET(ProcessTrace); ETW_REQ(p);
    TRACEHANDLE h = 1;
    EXPECT_EQ(p(&h, 0, NULL, NULL), ERROR_BAD_LENGTH);
    EXPECT_EQ(p(NULL, 0, NULL, NULL), ERROR_BAD_LENGTH);
}
TEST(etw_process_null_array) {
    __typeof__(&ProcessTrace) p = ET(ProcessTrace); ETW_REQ(p);
    EXPECT_EQ(p(NULL, 1, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(p(NULL, 5, NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_process_start_after_end) {
    __typeof__(&ProcessTrace) p = ET(ProcessTrace); ETW_REQ(p);
    TRACEHANDLE h = 1; FILETIME s, e;
    s.dwLowDateTime = 0; s.dwHighDateTime = 0x01d00000;   /* later */
    e.dwLowDateTime = 0; e.dwHighDateTime = 0x01c00000;   /* earlier */
    EXPECT_EQ(p(&h, 1, &s, &e), ERROR_INVALID_TIME);
}
TEST(etw_process_unknown_handle) {
    __typeof__(&ProcessTrace) p = ET(ProcessTrace); ETW_REQ(p);
    static const TRACEHANDLE hs[] = {1, 0x1234, 0x7ffffff0};
    for (unsigned i = 0; i < 3; i++) {
        TRACEHANDLE h = hs[i];
        ULONG r = p(&h, 1, NULL, NULL);
        CHECKF(r == ERROR_INVALID_HANDLE, "handle %llx got %lu", (unsigned long long)h, r);
    }
    TRACEHANDLE hv = INVALID_PT;
    EXPECT_EQ(p(&hv, 1, NULL, NULL), ERROR_INVALID_HANDLE);
}

/* ---- EnableTraceEx2 ---- */
TEST(etw_enable_null_provider) {
    __typeof__(&EnableTraceEx2) en = ET(EnableTraceEx2); ETW_REQ(en);
    SET_ERR(0);
    ULONG r = en(0x10001, NULL, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
TEST(etw_enable_zero_and_invalid_handle) {
    __typeof__(&EnableTraceEx2) en = ET(EnableTraceEx2); ETW_REQ(en);
    GUID g = test_guid(1);
    EXPECT_EQ(en(0, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(en(INVALID_PT, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_enable_bad_control_code) {
    __typeof__(&EnableTraceEx2) en = ET(EnableTraceEx2); ETW_REQ(en);
    GUID g = test_guid(2);
    for (ULONG code = 3; code < 8; code++) {
        ULONG r = en(0x10001, &g, code, 4, 0, 0, 0, NULL);
        CHECKF(r == ERROR_INVALID_PARAMETER, "code %lu got %lu", code, r);
    }
    EXPECT_EQ(en(0x10001, &g, 0xffffffffu, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
}
TEST(etw_enable_parameters_validation) {
    __typeof__(&EnableTraceEx2) en = ET(EnableTraceEx2); ETW_REQ(en);
    GUID g = test_guid(3);
    ENABLE_TRACE_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.Version = 7; /* only ENABLE_TRACE_PARAMETERS_VERSION 1 and _2 exist */
    EXPECT_EQ(en(0x10001, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, &ep), ERROR_INVALID_PARAMETER);
    ep.Version = 0;
    EXPECT_EQ(en(0x10001, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, &ep), ERROR_INVALID_PARAMETER);
    ep.Version = ENABLE_TRACE_PARAMETERS_VERSION; ep.ControlFlags = 1; /* reserved, must be zero */
    EXPECT_EQ(en(0x10001, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, &ep), ERROR_INVALID_PARAMETER);
}
TEST(etw_enable_logger_id_out_of_range) {
    __typeof__(&EnableTraceEx2) en = ET(EnableTraceEx2); ETW_REQ(en);
    GUID g = test_guid(5);
    EXPECT_EQ(en(0x7ffe, &g, EVENT_CONTROL_CODE_ENABLE_PROVIDER, 4, 0, 0, 0, NULL), ERROR_INVALID_PARAMETER);
}

/* ---- EnumerateTraceGuidsEx ---- */
TEST(etw_enumguids_null_returnlength) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    BYTE buf[64];
    SET_ERR(0);
    EXPECT_EQ(en(TraceGuidQueryList, NULL, 0, buf, sizeof buf, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_enumguids_bad_class) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    BYTE buf[64]; ULONG rl = 0xaa;
    static const int cls[] = {3, 4, 5, 50, 99, -1};
    for (unsigned i = 0; i < sizeof cls / sizeof cls[0]; i++) {
        SET_ERR(0); rl = 0xaa;
        ULONG r = en((TRACE_QUERY_INFO_CLASS)cls[i], NULL, 0, buf, sizeof buf, &rl);
        CHECKF(r == ERROR_INVALID_PARAMETER, "class %d got %lu", cls[i], r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(etw_enumguids_queryinfo_requires_guid) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    BYTE buf[256]; ULONG rl = 0; GUID g = test_guid(6);
    EXPECT_EQ(en(TraceGuidQueryInfo, NULL, 0, buf, sizeof buf, &rl), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(en(TraceGuidQueryInfo, &g, 0, buf, sizeof buf, &rl), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(en(TraceGuidQueryInfo, &g, 15, buf, sizeof buf, &rl), ERROR_INVALID_PARAMETER);
}
TEST(etw_enumguids_unregistered_guid_info) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    BYTE buf[1024]; ULONG rl = 0xaa; GUID g = test_guid(7);
    ULONG r = en(TraceGuidQueryInfo, &g, sizeof g, buf, sizeof buf, &rl);
    /* A GUID nobody registered: the kernel reports it as unknown (or access denied without privilege). */
    CHECK(r != 0 || rl >= 8);
    t_note("QueryInfo(unregistered guid) -> %lu rl=%lu", r, rl);
}
TEST(etw_enumguids_list_size_protocol) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    ULONG rl = 0; SET_ERR(0);
    ULONG r = en(TraceGuidQueryList, NULL, 0, NULL, 0, &rl);
    CHECKF(r == ERROR_INSUFFICIENT_BUFFER || r == ERROR_ACCESS_DENIED || r == 0, "got %lu", r);
    t_note("QueryList size query -> %lu needed=%lu", r, rl);
    if (r == ERROR_INSUFFICIENT_BUFFER) {
        CHECK(rl >= 8);
        BYTE *buf = (BYTE *)LocalAlloc(LPTR, rl + 4096);
        if (buf) {
            ULONG rl2 = 0;
            r = en(TraceGuidQueryList, NULL, 0, buf, rl + 4096, &rl2);
            CHECKF(r == 0 || r == ERROR_INSUFFICIENT_BUFFER, "second call %lu", r);
            if (r == 0) { PTRACE_GUID_INFO gi = (PTRACE_GUID_INFO)buf; t_note("registered provider GUIDs: %lu", gi->InstanceCount); CHECK(rl2 <= rl + 4096); }
            LocalFree(buf);
        }
    }
}
TEST(etw_enumguids_process_class_returnlength) {
    __typeof__(&EnumerateTraceGuidsEx) en = ET(EnumerateTraceGuidsEx); ETW_REQ(en);
    ULONG rl = 0xaa; ULONG r = en(TraceGuidQueryProcess, NULL, 0, NULL, 0, &rl);
    t_note("TraceGuidQueryProcess size query -> %lu rl=%lu", r, rl);
    CHECK(r == 0 || r == ERROR_INSUFFICIENT_BUFFER || r == ERROR_MORE_DATA);
}

/* ---- TraceSetInformation / TraceQueryInformation ---- */
TEST(etw_setinfo_stack_tracing_length_rules) {
    __typeof__(&TraceSetInformation) ts = ET(TraceSetInformation); ETW_REQ(ts);
    BYTE buf[0x18 * 4 + 8]; memset(buf, 0, sizeof buf);
    /* TraceStackTracingInfo (3): array of CLASSIC_EVENT_ID, 24 bytes each, at most 256 */
    static const ULONG bad[] = {1, 23, 25, 47, 0x18 * 257, 0x18 * 300 + 1};
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ULONG r = ts(0, TraceStackTracingInfo, buf, bad[i]);
        CHECKF(r == 0x5b6, "len %lu got %lu", bad[i], r);
    }
}
TEST(etw_setinfo_stack_tracing_null_buffer) {
    __typeof__(&TraceSetInformation) ts = ET(TraceSetInformation); ETW_REQ(ts);
    EXPECT_EQ(ts(0, TraceStackTracingInfo, NULL, 0x18), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ts(0, TraceStackTracingInfo, NULL, 0x18 * 3), ERROR_INVALID_PARAMETER);
}
TEST(etw_queryinfo_system_handle_class_small_buffer) {
    __typeof__(&TraceQueryInformation) tq = ET(TraceQueryInformation); ETW_REQ(tq);
    ULONG ret = 0xaa; BYTE buf[8] = {0};
    ULONG r = tq(0x10001, (TRACE_INFO_CLASS)4, buf, sizeof buf, &ret);
    t_note("TraceQueryInformation(class 4, 8-byte buffer) -> %lu ret=%lu", r, ret);
    EXPECT_EQ(ret, 0x20); /* required size is reported even when the buffer is too small */
    CHECK(r != 0);
}
TEST(etw_queryinfo_unknown_class) {
    __typeof__(&TraceQueryInformation) tq = ET(TraceQueryInformation); ETW_REQ(tq);
    ULONG ret = 0; BYTE buf[64] = {0};
    ULONG r = tq(0x10001, (TRACE_INFO_CLASS)200, buf, sizeof buf, &ret);
    CHECK(r != 0);
    t_note("TraceQueryInformation(class 200) -> %lu", r);
}

/* ---- Trace handle helpers exported from advapi32 (forwarded to ntdll Etw*) ---- */
TEST(etw_getloggerhandle_null) {
    __typeof__(&GetTraceLoggerHandle) g = ET(GetTraceLoggerHandle); REQUIRE(g);
    SET_ERR(0);
    TRACEHANDLE h = g(NULL);
    EXPECT_EQ(h, INVALID_PT); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_getloggerhandle_short_wnode) {
    __typeof__(&GetTraceLoggerHandle) g = ET(GetTraceLoggerHandle); REQUIRE(g);
    WNODE_HEADER w; memset(&w, 0, sizeof w);
    w.BufferSize = 0x2f; w.HistoricalContext = 1;
    SET_ERR(0);
    EXPECT_EQ(g(&w), INVALID_PT); EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(etw_getloggerhandle_ids) {
    __typeof__(&GetTraceLoggerHandle) g = ET(GetTraceLoggerHandle); REQUIRE(g);
    WNODE_HEADER w; memset(&w, 0, sizeof w); w.BufferSize = sizeof w;
    w.HistoricalContext = 0x00ab0001; EXPECT_EQ(g(&w), 0x00ab0001);
    w.HistoricalContext = 0xffff;      EXPECT_EQ(g(&w), 0xffff);   /* 0xFFFF is the "any" logger id */
    w.HistoricalContext = 0x7777;      /* far beyond the maximum logger id */
    SET_ERR(0);
    EXPECT_EQ(g(&w), INVALID_PT); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(etw_getenablelevel_handles) {
    __typeof__(&GetTraceEnableLevel) g = ET(GetTraceEnableLevel); REQUIRE(g);
    SET_ERR(0);
    EXPECT_EQ(g(0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(g(0x00050001), 5);             /* level is bits 16..23 of the handle */
    EXPECT_EQ(g(0x00ff0002), 0xff);
    SET_ERR(0);
    EXPECT_EQ(g(0x00050000 | 0x7777), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- RegisterTraceGuids / UnregisterTraceGuids (classic provider API) ---- */
static ULONG WINAPI req_cb(WMIDPREQUESTCODE c, PVOID ctx, ULONG *bs, PVOID buf) { return ERROR_SUCCESS; }
TEST(etw_regguids_invalid_args) {
    __typeof__(&RegisterTraceGuidsW) r = ET(RegisterTraceGuidsW); REQUIRE(r);
    GUID cg = test_guid(8); TRACEHANDLE h = 0x77;
    ULONG e;
    e = r(NULL, NULL, &cg, 0, NULL, NULL, NULL, &h);   CHECK(e != 0); t_note("RequestAddress=NULL -> %lu", e);
    e = r((WMIDPREQUEST)req_cb, NULL, NULL, 0, NULL, NULL, NULL, &h);  CHECK(e != 0); t_note("ControlGuid=NULL -> %lu", e);
    e = r((WMIDPREQUEST)req_cb, NULL, &cg, 0, NULL, NULL, NULL, NULL); CHECK(e != 0); t_note("RegHandle=NULL -> %lu", e);
    e = r((WMIDPREQUEST)req_cb, NULL, &cg, 0x10001, NULL, NULL, NULL, &h); CHECK(e != 0); t_note("GuidCount=0x10001 -> %lu", e);
    if (e == ERROR_INVALID_PARAMETER) CHECK(h == 0x77);
}
TEST(etw_regguids_register_unregister) {
    __typeof__(&RegisterTraceGuidsW) r = ET(RegisterTraceGuidsW); __typeof__(&UnregisterTraceGuids) u = ET(UnregisterTraceGuids);
    REQUIRE(r); REQUIRE(u);
    GUID cg = test_guid(9); TRACEHANDLE h = 0;
    ULONG e = r((WMIDPREQUEST)req_cb, NULL, &cg, 0, NULL, NULL, NULL, &h);
    EXPECT_EQ(e, 0); CHECK(h != 0);
    if (e == 0) {
        /* registration handles carry a non-zero generation in the top 16 bits and have bit 0 clear */
        CHECK((h >> 48) != 0); CHECK((h & 1) == 0);
        EXPECT_EQ(u(h), 0);
        ULONG e2 = u(h);
        CHECK(e2 != 0);
        t_note("second UnregisterTraceGuids -> %lu", e2);
    }
}
TEST(etw_unregguids_invalid) {
    __typeof__(&UnregisterTraceGuids) u = ET(UnregisterTraceGuids); REQUIRE(u);
    ULONG e = u(0); CHECK(e != 0); t_note("UnregisterTraceGuids(0) -> %lu", e);
    e = u(1); CHECK(e != 0);
}
TEST(etw_regguids_a_variant_resolves) {
    __typeof__(&RegisterTraceGuidsA) r = ET(RegisterTraceGuidsA); REQUIRE(r);
    __typeof__(&UnregisterTraceGuids) u = ET(UnregisterTraceGuids);
    GUID cg = test_guid(10); TRACEHANDLE h = 0;
    ULONG e = r((WMIDPREQUEST)req_cb, NULL, &cg, 0, NULL, NULL, NULL, &h);
    EXPECT_EQ(e, 0);
    if (e == 0 && u) EXPECT_EQ(u(h), 0);
}
