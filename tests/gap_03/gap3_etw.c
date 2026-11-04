/* gap_03: sechost ETW consumer-side exports SetTraceCallback / RemoveTraceCallback / QueryTraceProcessingHandle.
 * All expectations come from the Win10 sechost decomp: the callback list is a process-global list keyed by the
 * 16-byte GUID, guarded by a lock; errors are returned AND stored with RtlSetLastWin32Error. */
#include "g3once.h"
#include <evntrace.h>

#define WSKIP() do { if (t_is_wine()) { t_skip("wine: sechost trace-callback exports are unimplemented (abort)"); return; } } while (0)
static void WINAPI g_cb(PEVENT_TRACE t) { (void)t; }
static void WINAPI g_cb2(PEVENT_TRACE t) { (void)t; }

static GUID g3guid(DWORD salt) {
    GUID g = {0x6a3c0de0, 0x1111, 0x4e7c, {0xa1, 0xb2, 0xc3, 0xd4, 0, 0, 0, 0}};
    DWORD pid = GetCurrentProcessId();
    memcpy(g.Data4 + 4, &pid, 4); g.Data1 ^= salt; return g;
}

/* The callback list is process global, so the order dependent parts run in a fresh process. */
CHILD(gap3_etw_cbs) {
    __typeof__(&SetTraceCallback) set = U(SetTraceCallback);
    __typeof__(&RemoveTraceCallback) rem = U(RemoveTraceCallback);
    if (!set || !rem) return 0;
    GUID g1 = g3guid(1), g2 = g3guid(2), g3 = g3guid(3);
    ULONG r;
    SET_ERR(0);
    r = rem(NULL);
    if (r != ERROR_INVALID_PARAMETER || GetLastError() != ERROR_INVALID_PARAMETER) return 1;
    SET_ERR(0);
    ULONG r0 = rem(&g1);                                   /* nothing registered yet: 87 (list not set up) or 4200 */
    if (r0 != ERROR_INVALID_PARAMETER && r0 != ERROR_WMI_GUID_NOT_FOUND) return 2;
    if (GetLastError() != r0) return 3;
    SET_ERR(0);
    if (set(&g1, g_cb) != 0) return 4;
    if (GetLastError() != 0) return 5;                     /* success does not touch last error */
    SET_ERR(0xBEEF);
    if (rem(&g1) != 0) return 6;
    if (GetLastError() != 0xBEEF) return 7;                /* success leaves last error alone */
    SET_ERR(0);
    r = rem(&g1);
    if (r != ERROR_WMI_GUID_NOT_FOUND || GetLastError() != ERROR_WMI_GUID_NOT_FOUND) return 8;
    /* several GUIDs, removal in a different order */
    if (set(&g1, g_cb) || set(&g2, g_cb) || set(&g3, g_cb)) return 9;
    if (rem(&g2) != 0 || rem(&g1) != 0) return 10;
    if (rem(&g2) != ERROR_WMI_GUID_NOT_FOUND) return 11;
    if (rem(&g3) != 0) return 12;
    /* re-registering the same GUID replaces the callback (one entry) */
    if (set(&g1, g_cb) || set(&g1, g_cb2)) return 13;
    if (rem(&g1) != 0) return 14;
    if (rem(&g1) != ERROR_WMI_GUID_NOT_FOUND) return 15;
    /* unknown GUID once the list exists */
    if (rem(&g3) != ERROR_WMI_GUID_NOT_FOUND) return 16;
    /* the all-zero GUID is an ordinary key */
    GUID z; memset(&z, 0, sizeof z);
    if (set(&z, g_cb) != 0 || rem(&z) != 0 || rem(&z) != ERROR_WMI_GUID_NOT_FOUND) return 17;
    /* GUIDs that differ only in the second half of the 16 bytes are distinct keys */
    GUID a = g1, b = g1; b.Data4[7] ^= 1;
    if (set(&a, g_cb) || set(&b, g_cb)) return 18;
    if (rem(&a) != 0) return 19;
    if (rem(&a) != ERROR_WMI_GUID_NOT_FOUND) return 20;
    if (rem(&b) != 0) return 21;
    return 0 | (int)(r0 << 8);
}
TEST(gap3_etw_callback_list_semantics) {
    WSKIP();
    REQUIRE(U(SetTraceCallback)); REQUIRE(U(RemoveTraceCallback));
    DWORD rc = t_run_child("gap3_etw_cbs", "", 15000);
    CHECKF((rc & 0xff) == 0, "child failed at step %lu (exit 0x%lx)", (unsigned long)(rc & 0xff), (unsigned long)rc);
    t_note("RemoveTraceCallback with no callbacks registered returned %lu", (unsigned long)(rc >> 8));
}
TEST(gap3_etw_remove_callback_null_guid) {
    WSKIP();
    REQUIRE(U(RemoveTraceCallback));
    SET_ERR(0);
    ULONG r = U(RemoveTraceCallback)(NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap3_etw_set_callback_invalid_arguments) {
    WSKIP();
    REQUIRE(U(SetTraceCallback));
    GUID g = g3guid(9);
    SET_ERR(0);
    EXPECT_EQ(U(SetTraceCallback)(NULL, g_cb), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(SetTraceCallback)(&g, NULL), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(SetTraceCallback)(NULL, NULL), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* a rejected registration must not have created an entry */
    if (U(RemoveTraceCallback)) EXPECT_EQ(U(RemoveTraceCallback)(&g), ERROR_WMI_GUID_NOT_FOUND);
}
TEST(gap3_etw_set_remove_callback_roundtrip_in_proc) {
    WSKIP();
    REQUIRE(U(SetTraceCallback)); REQUIRE(U(RemoveTraceCallback));
    GUID g = g3guid(10);
    EXPECT_EQ(U(SetTraceCallback)(&g, g_cb), 0);
    EXPECT_EQ(U(RemoveTraceCallback)(&g), 0);
    EXPECT_EQ(U(RemoveTraceCallback)(&g), ERROR_WMI_GUID_NOT_FOUND);
}

/* ---- QueryTraceProcessingHandle: the handle lookup (and "closed" state) comes before the class switch ---- */
TEST(gap3_etw_queryprocessinghandle_unknown_handle) {
    UDECL(ULONG, QueryTraceProcessingHandle_, (TRACEHANDLE, int, PVOID, ULONG, PVOID, ULONG, PULONG));
    typedef ULONG (WINAPI *qp_fn)(TRACEHANDLE, int, PVOID, ULONG, PVOID, ULONG, PULONG);
    qp_fn q = (qp_fn)U(QueryTraceProcessingHandle);
    REQUIRE(q);
    if (t_is_wine()) { t_skip("wine: QueryTraceProcessingHandle is unimplemented (aborts)"); return; }
    static const TRACEHANDLE hs[] = { 0, 1, 0x1234, 0x7fffffffULL, (TRACEHANDLE)-1, (TRACEHANDLE)0x100000000ULL };
    static const int classes[] = { 0, 1, 2, 3, 4, -1 };
    for (unsigned i = 0; i < sizeof hs / sizeof hs[0]; i++)
        for (unsigned k = 0; k < sizeof classes / sizeof classes[0]; k++) {
            ULONG ret = 0xDEADBEEF;
            char out[0x80]; memset(out, 0x5A, sizeof out);
            ULONG r = q(hs[i], classes[k], NULL, 0, out, sizeof out, &ret);
            CHECKF(r == ERROR_INVALID_HANDLE, "handle %llx class %d -> %lu", (unsigned long long)hs[i], classes[k], r);
            CHECKF(ret == 0, "handle %llx class %d: ReturnLength %lu must be zeroed first", (unsigned long long)hs[i], classes[k], ret);
            CHECKF(out[0] == 0x5A && out[0x7f] == 0x5A, "output buffer touched for an unknown handle");
        }
}
TEST(gap3_etw_queryprocessinghandle_returns_error_code_not_last_error) {
    typedef ULONG (WINAPI *qp_fn)(TRACEHANDLE, int, PVOID, ULONG, PVOID, ULONG, PULONG);
    qp_fn q = (qp_fn)U(QueryTraceProcessingHandle);
    REQUIRE(q);
    if (t_is_wine()) { t_skip("wine: QueryTraceProcessingHandle is unimplemented (aborts)"); return; }
    ULONG ret;
    SET_ERR(0x1234);
    ULONG r = q(0x4242, 1, NULL, 0, NULL, 0, &ret);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    EXPECT_ERR(0x1234);          /* decomp: no SetLastError on this path */
}
