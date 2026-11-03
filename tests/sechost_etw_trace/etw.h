/* Shared helpers for the sechost_etw_trace area. */
#ifndef ETW_H
#define ETW_H
#include "t.h"
#include <wmistr.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <wchar.h>

/* ETW controller/consumer exports live in sechost (advapi32 forwards to it); the Event* provider API
 * is forwarded advapi32 -> ntdll Etw*. Resolve in that order. */
static inline FARPROC etw_res(const char *name) {
    FARPROC p = t_proc_in("sechost.dll", name);
    if (!p) p = t_proc_in("advapi32.dll", name);
    if (!p) { char b[96]; _snprintf(b, sizeof b, "Etw%s", name); p = t_proc_in("ntdll.dll", b); }
    return p;
}
/* Wine's ETW controller functions are stubs that return success for everything; the expectations below
 * are Windows behaviour, so skip the controller tests under an emulator (no-op on Windows). */
#define ETW_REQ(p) do { REQUIRE(p); if (t_is_wine()) { t_skip("emulator ETW controller is a stub"); return; } } while (0)
#define ET(name) ((__typeof__(&name))etw_res(#name))

/* Win10 value of EVENT_TRACE_PROPERTIES fixed size (WNODE_HEADER 0x30 + 0x48). */
#define ETW_PROPS_SIZE 0x78
#define ETW_ERR_BAD_LENGTH 24
#define ETW_ERR_INVALID_PARAMETER 87
#define ETW_ERR_INSTANCE_NOT_FOUND 4201

static const GUID kSystemTraceControlGuid = {0x9e814aad, 0x3204, 0x11d2, {0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39}};
static const GUID kCKCLGuid = {0x54dea73a, 0xed1f, 0x42a4, {0xaf, 0x71, 0x3e, 0x63, 0xd0, 0x56, 0xf1, 0x74}};
static const GUID kZeroGuid = {0};

/* Properties buffer with room for logger name (offset 0x78) and log file name (offset 0x178). */
typedef union { EVENT_TRACE_PROPERTIES p; BYTE raw[4096]; } PB;
#define PB_NAME_OFF 0x78
#define PB_FILE_OFF 0x178

static inline void pb_init(PB *b, DWORD bufsize, const GUID *guid, DWORD mode, DWORD maxfile, const WCHAR *logfile) {
    memset(b, 0, sizeof *b);
    b->p.Wnode.BufferSize = bufsize;
    b->p.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    b->p.Wnode.ClientContext = 1;
    if (guid) b->p.Wnode.Guid = *guid;
    b->p.LogFileMode = mode;
    b->p.MaximumFileSize = maxfile;
    b->p.LoggerNameOffset = PB_NAME_OFF;
    if (logfile) {
        b->p.LogFileNameOffset = PB_FILE_OFF;
        lstrcpyW((WCHAR *)(b->raw + PB_FILE_OFF), logfile);
    }
}
/* Default valid-shaped buffer (buffering mode, no file). */
static inline void pb_default(PB *b) { pb_init(b, 0x400, NULL, EVENT_TRACE_BUFFERING_MODE, 0, NULL); }
static inline WCHAR *pb_name(PB *b) { return (WCHAR *)(b->raw + b->p.LoggerNameOffset); }

static inline void etw_session_name(WCHAR *out, size_t cap, const char *tag) {
    _snwprintf(out, cap, L"olet_%lu_%S", GetCurrentProcessId(), tag);
    out[cap - 1] = 0;
}
static inline void etw_session_name_a(char *out, size_t cap, const char *tag) {
    _snprintf(out, cap, "olet_%lu_%s", GetCurrentProcessId(), tag);
    out[cap - 1] = 0;
}

static inline int etw_is_elevated(void) {
    HANDLE tok; TOKEN_ELEVATION el; DWORD n = 0; int r = 0;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        if (GetTokenInformation(tok, TokenElevation, &el, sizeof el, &n)) r = el.TokenIsElevated != 0;
        CloseHandle(tok);
    }
    return r;
}

/* Stop any session named olet_*; used by sweeps and as defensive cleanup. */
static inline void etw_sweep(void) {
    __typeof__(&QueryAllTracesW) qat = ET(QueryAllTracesW);
    __typeof__(&ControlTraceW) ct = ET(ControlTraceW);
    if (!qat || !ct) return;
    enum { N = 64 };
    static PB bufs[N]; EVENT_TRACE_PROPERTIES *arr[N]; ULONG cnt = 0;
    for (int i = 0; i < N; i++) { memset(&bufs[i], 0, sizeof bufs[i]); bufs[i].p.Wnode.BufferSize = sizeof bufs[i]; bufs[i].p.LoggerNameOffset = PB_NAME_OFF; bufs[i].p.LogFileNameOffset = PB_FILE_OFF; arr[i] = &bufs[i].p; }
    ULONG r = qat(arr, N, &cnt);
    if (r != 0 && r != ERROR_MORE_DATA) return;
    if (cnt > N) cnt = N;
    for (ULONG i = 0; i < cnt; i++) {
        WCHAR *nm = (WCHAR *)(bufs[i].raw + bufs[i].p.LoggerNameOffset);
        if (bufs[i].p.LoggerNameOffset && !wcsncmp(nm, L"olet_", 5)) {
            PB sp; pb_default(&sp); sp.p.Wnode.BufferSize = sizeof sp;
            ct(bufs[i].p.Wnode.HistoricalContext, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP);
        }
    }
}
/* Run a StartTraceW that must fail; stop the session defensively if it unexpectedly succeeds. */
static inline ULONG etw_start_fail(const WCHAR *name, PB *b, TRACEHANDLE *h) {
    __typeof__(&StartTraceW) st = ET(StartTraceW);
    SetLastError(0xdeadbeef);
    *h = 0;
    ULONG r = st(h, name, &b->p);
    if (r == 0 && *h) { PB sp; pb_default(&sp); sp.p.Wnode.BufferSize = sizeof sp; ET(ControlTraceW)(*h, NULL, &sp.p, EVENT_TRACE_CONTROL_STOP); }
    return r;
}
#endif
