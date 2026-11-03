#include "etw.h"
/* StartTraceW/StartTraceA argument validation. Everything here is rejected in user mode (sechost),
 * before NtTraceControl, so it needs no elevation and creates no session. Verified against the
 * Win10 decomp of StartTraceW / EtwpValidateTraceProperties / EtwpValidateLogFileMode. */

static void note_last(ULONG r, const char *what) {
    DWORD e = GetLastError();
    if (e != r) t_note("%s: GetLastError=%lu differs from return %lu", what, e, r);
}

/* ---- NULL arguments ---- */
TEST(etw_start_null_handle_ptr) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    PB b; pb_default(&b); WCHAR n[64]; etw_session_name(n, 64, "nh");
    SET_ERR(0);
    ULONG r = st(NULL, n, &b.p);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_start_null_properties) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "np"); TRACEHANDLE h = 0x1234;
    SET_ERR(0);
    ULONG r = st(&h, n, NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(h, 0x1234); /* output untouched on early failure */
}
TEST(etw_start_null_both) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    SET_ERR(0);
    ULONG r = st(NULL, NULL, NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_start_null_name) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    PB b; pb_default(&b); TRACEHANDLE h = 0;
    SET_ERR(0);
    ULONG r = st(&h, NULL, &b.p);
    if (r == 0 && h) { PB sp; pb_default(&sp); ET(ControlTraceW)(h, NULL, &sp.p, 1); }
    CHECK(r != 0);
    t_note("StartTraceW(name=NULL) returned %lu", r);
}
TEST(etw_start_a_null_args) {
    __typeof__(&StartTraceA) st = ET(StartTraceA); ETW_REQ(st);
    PB b; pb_default(&b); char n[64]; etw_session_name_a(n, 64, "an"); TRACEHANDLE h = 0;
    SET_ERR(0); EXPECT_EQ(st(NULL, n, &b.p), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(st(&h, n, NULL), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(st(NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
}

/* ---- Wnode.BufferSize below the fixed structure size -> ERROR_BAD_LENGTH ---- */
#define BUFSZ_TEST(tag, sz) \
TEST(etw_start_bufsize_##tag) { \
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st); \
    PB b; pb_default(&b); b.p.Wnode.BufferSize = (sz); WCHAR n[64]; etw_session_name(n, 64, "bs"); TRACEHANDLE h; \
    ULONG r = etw_start_fail(n, &b, &h); \
    EXPECT_EQ(r, ETW_ERR_BAD_LENGTH); note_last(r, "bufsize"); \
}
BUFSZ_TEST(zero, 0)
BUFSZ_TEST(one, 1)
BUFSZ_TEST(sizeof_wnode, 0x30)
BUFSZ_TEST(just_under, 0x77)
BUFSZ_TEST(quarter, 0x3c)

/* The A variant must apply the same validation. */
TEST(etw_start_a_bufsize_bad_length) {
    __typeof__(&StartTraceA) st = ET(StartTraceA); ETW_REQ(st);
    PB b; pb_default(&b); b.p.Wnode.BufferSize = 0x77; char n[64]; etw_session_name_a(n, 64, "abs"); TRACEHANDLE h = 0;
    ULONG r = st(&h, n, &b.p);
    if (r == 0 && h) ET(ControlTraceA)(h, NULL, &b.p, 1);
    EXPECT_EQ(r, ETW_ERR_BAD_LENGTH);
}

/* ---- name / log-file offsets that point into the fixed header or past the buffer -> 87 ---- */
#define OFF_TEST(tag, field, value) \
TEST(etw_start_##tag) { \
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st); \
    PB b; pb_default(&b); b.p.field = (value); WCHAR n[64]; etw_session_name(n, 64, "off"); TRACEHANDLE h; \
    ULONG r = etw_start_fail(n, &b, &h); \
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); note_last(r, #tag); \
}
OFF_TEST(loggername_off_1, LoggerNameOffset, 1)
OFF_TEST(loggername_off_in_header, LoggerNameOffset, 0x77)
OFF_TEST(loggername_off_0x30, LoggerNameOffset, 0x30)
OFF_TEST(loggername_off_past_buffer, LoggerNameOffset, 0x401)
OFF_TEST(loggername_off_huge, LoggerNameOffset, 0xffffff00)
OFF_TEST(logfile_off_1, LogFileNameOffset, 1)
OFF_TEST(logfile_off_in_header, LogFileNameOffset, 0x77)
OFF_TEST(logfile_off_past_buffer, LogFileNameOffset, 0x401)
OFF_TEST(logfile_off_huge, LogFileNameOffset, 0xffffff00)

/* ---- name length vs Wnode.BufferSize: need 0x78 + 2 + 2*len (no logfile) ---- */
TEST(etw_start_name_buffer_boundary) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    /* Mode 0 and no file fails later with ERROR_BAD_PATHNAME, which proves the length check passed. */
    static const int lens[] = {1, 5, 20, 100, 200};
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        WCHAR n[260]; int len = lens[i];
        for (int k = 0; k < len; k++) n[k] = L'a' + (k % 26);
        n[len] = 0;
        DWORD need = ETW_PROPS_SIZE + 2 + 2 * len;
        PB b; TRACEHANDLE h;
        pb_init(&b, need - 1, NULL, 0, 0, NULL); b.p.LoggerNameOffset = 0;
        ULONG r = etw_start_fail(n, &b, &h);
        CHECKF(r == ETW_ERR_BAD_LENGTH, "len %d size %lu-1: got %lu", len, need, r);
        pb_init(&b, need, NULL, 0, 0, NULL); b.p.LoggerNameOffset = 0;
        r = etw_start_fail(n, &b, &h);
        CHECKF(r == ERROR_BAD_PATHNAME, "len %d size %lu: got %lu", len, need, r);
    }
}
TEST(etw_start_name_longer_than_buffer) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[1100]; for (int k = 0; k < 1099; k++) n[k] = L'x'; n[1099] = 0;
    PB b; pb_default(&b); b.p.Wnode.BufferSize = ETW_PROPS_SIZE; b.p.LoggerNameOffset = 0; TRACEHANDLE h;
    EXPECT_EQ(etw_start_fail(n, &b, &h), ETW_ERR_BAD_LENGTH);
}

/* ---- empty name ---- */
TEST(etw_start_empty_name) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    PB b; pb_default(&b); TRACEHANDLE h;
    ULONG r = etw_start_fail(L"", &b, &h);
    EXPECT_EQ(r, ERROR_INVALID_NAME); note_last(r, "empty name");
}
TEST(etw_start_a_empty_name) {
    __typeof__(&StartTraceA) st = ET(StartTraceA); ETW_REQ(st);
    PB b; pb_default(&b); TRACEHANDLE h = 0;
    ULONG r = st(&h, "", &b.p);
    if (r == 0 && h) ET(ControlTraceA)(h, NULL, &b.p, 1);
    EXPECT_EQ(r, ERROR_INVALID_NAME);
}

/* ---- kernel logger GUID rules ---- */
TEST(etw_start_kernel_guid_wrong_name) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "kg"); PB b; TRACEHANDLE h;
    pb_init(&b, 0x400, &kSystemTraceControlGuid, EVENT_TRACE_BUFFERING_MODE, 0, NULL);
    ULONG r = etw_start_fail(n, &b, &h);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); note_last(r, "kernel guid / other name");
}
TEST(etw_start_ckcl_guid_wrong_name) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "ck"); PB b; TRACEHANDLE h;
    pb_init(&b, 0x400, &kCKCLGuid, EVENT_TRACE_BUFFERING_MODE, 0, NULL);
    ULONG r = etw_start_fail(n, &b, &h);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); note_last(r, "ckcl guid / other name");
}
TEST(etw_start_kernel_name_forces_guid) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    /* Name "NT Kernel Logger" (case-insensitive) overwrites Wnode.Guid with SystemTraceControlGuid before
     * further validation; mode 0 without a file then fails with ERROR_BAD_PATHNAME. */
    static const WCHAR *names[] = {L"NT Kernel Logger", L"nt kernel logger", L"NT KERNEL LOGGER"};
    for (int i = 0; i < 3; i++) {
        PB b; TRACEHANDLE h; pb_init(&b, 0x400, &kZeroGuid, 0, 0, NULL);
        ULONG r = etw_start_fail(names[i], &b, &h);
        CHECKF(r == ERROR_BAD_PATHNAME, "name %d got %lu", i, r);
        CHECK(IsEqualGUID(&b.p.Wnode.Guid, &kSystemTraceControlGuid));
    }
}
TEST(etw_start_ckcl_name_forces_guid) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    PB b; TRACEHANDLE h; pb_init(&b, 0x400, &kZeroGuid, 0, 0, NULL);
    ULONG r = etw_start_fail(L"Circular Kernel Context Logger", &b, &h);
    EXPECT_EQ(r, ERROR_BAD_PATHNAME);
    CHECK(IsEqualGUID(&b.p.Wnode.Guid, &kCKCLGuid));
}
TEST(etw_start_kernel_name_prefix_not_kernel) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    /* Only an exact (case-insensitive) match counts; a different name leaves the GUID untouched. */
    PB b; TRACEHANDLE h; pb_init(&b, 0x400, &kZeroGuid, 0, 0, NULL);
    ULONG r = etw_start_fail(L"NT Kernel Logger2", &b, &h);
    EXPECT_EQ(r, ERROR_BAD_PATHNAME);
    CHECK(IsEqualGUID(&b.p.Wnode.Guid, &kZeroGuid));
}

/* ---- LogFileMode combination rules (EtwpValidateLogFileMode) ---- */
typedef struct { const char *tag; DWORD mode, maxfile; int file; ULONG want; } modecase;
static const modecase kModes[] = {
    {"none_nofile",            0,                                   0,  0, ERROR_BAD_PATHNAME},
    {"seq_only_nofile",        EVENT_TRACE_FILE_MODE_SEQUENTIAL,    0,  0, ERROR_BAD_PATHNAME},
    {"private_realtime",       0x100 | 0x800,                       0,  0, 87},
    {"private_realtime_file",  0x100 | 0x800,                       0,  1, 87},
    {"append_realtime",        0x100 | EVENT_TRACE_FILE_MODE_APPEND,0,  0, 87},
    {"append_circular",        EVENT_TRACE_FILE_MODE_APPEND | EVENT_TRACE_FILE_MODE_CIRCULAR, 0, 1, 87},
    {"prealloc_nomax",         EVENT_TRACE_FILE_MODE_PREALLOCATE,   0,  1, 87},
    {"prealloc_newfile",       EVENT_TRACE_FILE_MODE_PREALLOCATE | EVENT_TRACE_FILE_MODE_NEWFILE, 10, 1, 87},
    {"prealloc_nofile",        EVENT_TRACE_FILE_MODE_PREALLOCATE | EVENT_TRACE_BUFFERING_MODE, 10, 0, 87},
    {"kbytes_nomax",           0x2000,                              0,  1, 87},
    {"kbytes_nofile",          0x2000 | EVENT_TRACE_BUFFERING_MODE, 10, 0, 87},
    {"relogger_not_private",   0x10000 | EVENT_TRACE_BUFFERING_MODE,0,  0, 87},
    {"newfile_nomax",          EVENT_TRACE_FILE_MODE_NEWFILE,       0,  1, 87},
    {"newfile_circular",       EVENT_TRACE_FILE_MODE_NEWFILE | EVENT_TRACE_FILE_MODE_CIRCULAR, 5, 1, 87},
    {"newfile_nofile",         EVENT_TRACE_FILE_MODE_NEWFILE | EVENT_TRACE_BUFFERING_MODE, 5, 0, 87},
};
TEST(etw_start_logfilemode_rules) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "lm");
    WCHAR file[300]; char tmp[300]; t_tmpfile(tmp, sizeof tmp, "etw_lm_%d.etl");
    MultiByteToWideChar(CP_ACP, 0, tmp, -1, file, 300);
    for (unsigned i = 0; i < sizeof kModes / sizeof kModes[0]; i++) {
        const modecase *c = &kModes[i]; PB b; TRACEHANDLE h;
        pb_init(&b, 0x800, NULL, c->mode, c->maxfile, c->file ? file : NULL);
        ULONG r = etw_start_fail(n, &b, &h);
        CHECKF(r == c->want, "case %s: got %lu want %lu", c->tag, r, c->want);
    }
}
TEST(etw_start_logfilemode_files_not_created) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "lmf"); WCHAR file[300]; char tmp[300];
    t_tmpfile(tmp, sizeof tmp, "etw_nocreate.etl"); MultiByteToWideChar(CP_ACP, 0, tmp, -1, file, 300);
    PB b; TRACEHANDLE h; pb_init(&b, 0x800, NULL, EVENT_TRACE_FILE_MODE_PREALLOCATE, 0, file);
    EXPECT_EQ(etw_start_fail(n, &b, &h), ERROR_INVALID_PARAMETER);
    CHECK(GetFileAttributesW(file) == INVALID_FILE_ATTRIBUTES);
}

/* ---- extended properties flag (0x800000 in Wnode.Flags) needs the larger 0x90 structure ---- */
TEST(etw_start_filter_flag_needs_big_struct) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "fl"); PB b; TRACEHANDLE h;
    pb_default(&b); b.p.Wnode.BufferSize = 0x80; b.p.Wnode.Flags |= 0x800000;
    EXPECT_EQ(etw_start_fail(n, &b, &h), ETW_ERR_BAD_LENGTH);
}
TEST(etw_start_filter_flag_count_without_ptr) {
    __typeof__(&StartTraceW) st = ET(StartTraceW); ETW_REQ(st);
    WCHAR n[64]; etw_session_name(n, 64, "fl2"); PB b; TRACEHANDLE h;
    pb_default(&b); b.p.Wnode.Flags |= 0x800000;
    *(DWORD *)(b.raw + 0x7c) = 1; /* FilterDescCount without FilterDesc pointer */
    EXPECT_EQ(etw_start_fail(n, &b, &h), ERROR_INVALID_PARAMETER);
}

/* ---- table of the same NULL/short-size rejections through StartTraceA ---- */
TEST(etw_start_a_offset_rejected) {
    __typeof__(&StartTraceA) st = ET(StartTraceA); ETW_REQ(st);
    PB b; pb_default(&b); b.p.LoggerNameOffset = 0x10; char n[64]; etw_session_name_a(n, 64, "ao"); TRACEHANDLE h = 0;
    ULONG r = st(&h, n, &b.p);
    if (r == 0 && h) ET(ControlTraceA)(h, NULL, &b.p, 1);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
