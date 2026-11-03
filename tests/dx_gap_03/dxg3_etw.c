/* dx_gap_03: QueryTraceProcessingHandle on a REAL processing handle (real-time session + OpenTraceW).
 * Gated on being able to start a session (administrator / Performance Log Users); the session is named olet + pid and is
 * stopped in the tail; leftovers from an earlier crashed run with the same name are swept first. */
#include "t.h"
#include <evntrace.h>
#include <evntcons.h>

typedef ULONG (WINAPI *qp_fn)(TRACEHANDLE, int, PVOID, ULONG, PVOID, ULONG, PULONG);
static void WINAPI rec_cb(PEVENT_RECORD r) { (void)r; }

typedef struct { EVENT_TRACE_PROPERTIES p; WCHAR name[96]; } Props;
static void init_props(Props *p) {
    memset(p, 0, sizeof *p);
    p->p.Wnode.BufferSize = sizeof *p;
    p->p.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    p->p.Wnode.ClientContext = 1;
    p->p.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    p->p.LoggerNameOffset = offsetof(Props, name);
    p->p.BufferSize = 64; p->p.MinimumBuffers = 2; p->p.MaximumBuffers = 4;
}

TEST(dxg3_etw_queryprocessinghandle_real_handle) {
    if (t_is_wine()) { t_skip("wine: ETW consumer APIs unimplemented"); return; }
    qp_fn q = (qp_fn)U(QueryTraceProcessingHandle); REQUIRE(q);
    REQUIRE(U(StartTraceW)); REQUIRE(U(ControlTraceW)); REQUIRE(U(OpenTraceW)); REQUIRE(U(CloseTrace));
    WCHAR sname[64]; wsprintfW(sname, L"olet_gap3_etw_%lu", GetCurrentProcessId());
    Props sw; init_props(&sw); U(ControlTraceW)(0, sname, &sw.p, EVENT_TRACE_CONTROL_STOP);     /* sweep */
    Props p; init_props(&p);
    TRACEHANDLE sess = 0;
    ULONG r = U(StartTraceW)(&sess, sname, &p.p);
    if (r != ERROR_SUCCESS) { t_skip("StartTraceW -> %lu (needs administrator)", r); return; }
    EVENT_TRACE_LOGFILEW lf; memset(&lf, 0, sizeof lf);
    lf.LoggerName = sname;
    lf.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    lf.EventRecordCallback = rec_cb;
    TRACEHANDLE h = U(OpenTraceW)(&lf);
    if (h == (TRACEHANDLE)INVALID_PROCESSTRACE_HANDLE) {
        t_note("OpenTraceW -> error %lu", GetLastError());
        CHECK(0);
    } else {
        ULONG ret = 0xDEADBEEF; char out[0x80];
        /* class 1 (partition information): the output buffer must be at least 0x30 bytes, otherwise ERROR_INSUFFICIENT_BUFFER */
        r = q(h, 1, NULL, 0, NULL, 0, &ret);
        CHECKF(r == ERROR_INSUFFICIENT_BUFFER, "class 1, no buffer -> %lu", r);
        r = q(h, 1, NULL, 0, out, 0x2f, &ret);
        CHECKF(r == ERROR_INSUFFICIENT_BUFFER, "class 1, 0x2f bytes -> %lu", r);
        ret = 0xDEADBEEF; memset(out, 0x77, sizeof out);
        r = q(h, 1, NULL, 0, out, sizeof out, &ret);
        t_note("class 1 with a 0x80 byte buffer -> %lu, ReturnLength %lu (ERROR_SUCCESS = partition ids filled in, ERROR_NOT_FOUND = none)", r, ret);
        CHECK(r == ERROR_SUCCESS || r == ERROR_NOT_FOUND);
        if (r == ERROR_SUCCESS) CHECK(ret == 0x30);
        /* a class the engine does not know completes with ERROR_SUCCESS and an empty result */
        static const int unknown[] = { 0, 4, 5, 99, -1 };
        for (unsigned i = 0; i < sizeof unknown / sizeof unknown[0]; i++) {
            ret = 0xDEADBEEF;
            r = q(h, unknown[i], NULL, 0, out, sizeof out, &ret);
            CHECKF(r == ERROR_SUCCESS && ret == 0, "class %d -> %lu ReturnLength %lu", unknown[i], r, ret);
        }
        ULONG cr = U(CloseTrace)(h);
        t_note("CloseTrace -> %lu", cr);
        ret = 0xDEADBEEF;
        r = q(h, 1, NULL, 0, out, sizeof out, &ret);
        CHECKF(r == ERROR_INVALID_HANDLE && ret == 0, "after CloseTrace -> %lu ReturnLength %lu", r, ret);
    }
    Props st; init_props(&st);
    U(ControlTraceW)(sess, NULL, &st.p, EVENT_TRACE_CONTROL_STOP);
    Props st2; init_props(&st2);
    U(ControlTraceW)(0, sname, &st2.p, EVENT_TRACE_CONTROL_STOP);   /* tail, in case the handle form failed */
}
