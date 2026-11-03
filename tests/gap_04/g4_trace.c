/* gap_04: sechost!SetTraceCallback(LPCGUID, PEVENT_CALLBACK) (advapi32 forwards here).
 * Win10 decompilation: NULL guid or NULL callback -> returns ERROR_INVALID_PARAMETER (87) and sets the last error to
 * 87; otherwise the (guid, callback) pair is added to a process-wide list (an existing entry for the same GUID has
 * its callback replaced) and ERROR_SUCCESS is returned. The callback is only invoked when a trace session delivers
 * events of that GUID, which these tests never start. */
#include "t.h"
#include <evntrace.h>
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)

static void WINAPI cb1(PEVENT_TRACE e) { (void)e; }
static void WINAPI cb2(PEVENT_TRACE e) { (void)e; }
static const GUID g_a = { 0x6a0b0000, 0x0001, 0x4c1d, { 0x80, 1, 'o', 'l', 'e', 't', 'g', '4' } };
static const GUID g_b = { 0x6a0b0000, 0x0002, 0x4c1d, { 0x80, 1, 'o', 'l', 'e', 't', 'g', '4' } };

TEST(gap4_settracecallback_null_arguments) {
    ULONG r;
    REQUIRE(UIN("sechost.dll", SetTraceCallback)); if (t_is_wine()) { t_skip("SetTraceCallback is an unimplemented stub in wine"); return; }
    SET_ERR(0); r = UIN("sechost.dll", SetTraceCallback)(NULL, cb1);
    EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = UIN("sechost.dll", SetTraceCallback)(&g_a, NULL);
    EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = UIN("sechost.dll", SetTraceCallback)(NULL, NULL);
    EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap4_settracecallback_register_and_replace) {
    ULONG r;
    REQUIRE(UIN("sechost.dll", SetTraceCallback)); if (t_is_wine()) { t_skip("SetTraceCallback is an unimplemented stub in wine"); return; }
    SET_ERR(0x1234);
    r = UIN("sechost.dll", SetTraceCallback)(&g_a, cb1);
    EQ(r, ERROR_SUCCESS);
    EQ(GetLastError(), 0x1234);                          /* success does not touch the last error */
    EQ(UIN("sechost.dll", SetTraceCallback)(&g_a, cb2), ERROR_SUCCESS);   /* same GUID: callback replaced */
    EQ(UIN("sechost.dll", SetTraceCallback)(&g_a, cb2), ERROR_SUCCESS);   /* idempotent */
    EQ(UIN("sechost.dll", SetTraceCallback)(&g_b, cb1), ERROR_SUCCESS);   /* another GUID: another entry */
}
TEST(gap4_settracecallback_many_guids) {
    GUID g = g_a; int i, bad = 0;
    REQUIRE(UIN("sechost.dll", SetTraceCallback)); if (t_is_wine()) { t_skip("SetTraceCallback is an unimplemented stub in wine"); return; }
    for (i = 0; i < 200; i++) { g.Data1 = 0x6a0c0000 + i; if (UIN("sechost.dll", SetTraceCallback)(&g, (i & 1) ? cb1 : cb2) != ERROR_SUCCESS) bad++; }
    EQ(bad, 0);
}
TEST(gap4_settracecallback_nil_guid_is_a_valid_key) {
    GUID z; memset(&z, 0, sizeof z);
    REQUIRE(UIN("sechost.dll", SetTraceCallback)); if (t_is_wine()) { t_skip("SetTraceCallback is an unimplemented stub in wine"); return; }
    EQ(UIN("sechost.dll", SetTraceCallback)(&z, cb1), ERROR_SUCCESS);
}
TEST(gap4_settracecallback_advapi32_forwarder) {
    ULONG r; HMODULE m = t_module("advapi32.dll"); FARPROC p;
    REQUIRE(UIN("sechost.dll", SetTraceCallback)); if (t_is_wine()) { t_skip("SetTraceCallback is an unimplemented stub in wine"); return; }
    p = m ? GetProcAddress(m, "SetTraceCallback") : NULL;
    if (!p) { t_note("advapi32 does not export SetTraceCallback"); return; }
    SET_ERR(0); r = ((ULONG (WINAPI *)(LPCGUID, PEVENT_CALLBACK))p)(NULL, cb1);
    EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EQ(((ULONG (WINAPI *)(LPCGUID, PEVENT_CALLBACK))p)(&g_b, cb2), ERROR_SUCCESS);
}
