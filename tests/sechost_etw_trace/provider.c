#include "etw.h"
/* User-mode ETW provider API: EventRegister/Unregister/Enabled/Write, ActivityIdControl/SetInformation.
 * Resolved via advapi32 (forwarder to ntdll Etw*) or ntdll. Behaviour from the Win10 ntdll decomp
 * (EtwEventRegister, EtwEventActivityIdControl, EtwEventWriteString, EtwEventEnabled, ...). */

static GUID prov_guid(DWORD salt) {
    GUID g = {0x0be7c0de, 0x7e57, 0x4e7c, {0xa1, 0xb2, 0xc3, 0xd4, 0, 0, 0, 0}};
    DWORD pid = GetCurrentProcessId();
    memcpy(g.Data4 + 4, &pid, 4); g.Data1 ^= salt; return g;
}
static void WINAPI never_cb(LPCGUID id, ULONG ctl, UCHAR lvl, ULONGLONG any, ULONGLONG all, PEVENT_FILTER_DESCRIPTOR f, PVOID ctx) {
    if (ctx) InterlockedIncrement((LONG *)ctx);
}
static EVENT_DESCRIPTOR desc(UCHAR level, ULONGLONG kw) {
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Id = 1; d.Level = level; d.Keyword = kw; return d;
}

/* ---- EventRegister ---- */
TEST(etw_prov_register_null_guid) {
    __typeof__(&EventRegister) r = ET(EventRegister); REQUIRE(r);
    REGHANDLE h = 0x55; SET_ERR(0);
    EXPECT_EQ(r(NULL, NULL, NULL, &h), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_prov_register_null_handle_ptr) {
    __typeof__(&EventRegister) r = ET(EventRegister); REQUIRE(r);
    GUID g = prov_guid(1); SET_ERR(0);
    EXPECT_EQ(r(&g, NULL, NULL, NULL), ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(etw_prov_register_context_without_callback) {
    __typeof__(&EventRegister) r = ET(EventRegister); REQUIRE(r);
    GUID g = prov_guid(2); REGHANDLE h = 0; LONG ctx = 0; SET_ERR(0);
    /* a callback context with no callback is rejected before any registration happens */
    EXPECT_EQ(r(&g, NULL, &ctx, &h), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(h, 0);
}
TEST(etw_prov_register_unregister_roundtrip) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); REQUIRE(r); REQUIRE(u);
    GUID g = prov_guid(3); REGHANDLE h = 0;
    EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    CHECK(h != 0);
    CHECK((h & 1) == 0);          /* low bit clear: handle is a registration pointer */
    CHECK((h >> 48) != 0);        /* generation in the top 16 bits */
    EXPECT_EQ(u(h), 0);
}
TEST(etw_prov_register_with_callback_not_invoked_when_disabled) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); REQUIRE(r); REQUIRE(u);
    GUID g = prov_guid(4); REGHANDLE h = 0; LONG calls = 0;
    EXPECT_EQ(r(&g, never_cb, &calls, &h), 0);
    Sleep(50);
    /* No session enables this brand-new GUID, so no ENABLE/DISABLE notification is delivered. */
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(u(h), 0);
    EXPECT_EQ(calls, 0);
}
TEST(etw_prov_register_same_guid_twice) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); REQUIRE(r); REQUIRE(u);
    GUID g = prov_guid(5); REGHANDLE a = 0, b = 0;
    EXPECT_EQ(r(&g, NULL, NULL, &a), 0);
    EXPECT_EQ(r(&g, NULL, NULL, &b), 0);
    CHECK(a != b);
    EXPECT_EQ(u(a), 0);
    EXPECT_EQ(u(b), 0);
}
TEST(etw_prov_register_many_distinct) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); REQUIRE(r); REQUIRE(u);
    REGHANDLE hs[16];
    for (int i = 0; i < 16; i++) { GUID g = prov_guid(100 + i); hs[i] = 0; EXPECT_EQ(r(&g, NULL, NULL, &hs[i]), 0); }
    for (int i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) CHECK(hs[i] != hs[j]);
    for (int i = 0; i < 16; i++) EXPECT_EQ(u(hs[i]), 0);
}

/* ---- EventUnregister ---- */
TEST(etw_prov_unregister_invalid) {
    __typeof__(&EventUnregister) u = ET(EventUnregister); REQUIRE(u);
    /* ntdll EtwNotificationUnregister: generation (top 16 bits) == 0 or an odd handle -> ERROR_INVALID_HANDLE.
     * Handles with a non-zero generation are dereferenced as pointers, so only these two are safe to probe. */
    SET_ERR(0);
    EXPECT_EQ(u(0), ERROR_INVALID_HANDLE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(u(1), ERROR_INVALID_HANDLE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(u(0xffffffffffffffffULL), ERROR_INVALID_HANDLE);
}
/* A second EventUnregister of a freed registration reads freed heap memory (generation check), so it is not tested. */

/* ---- enabled queries on a registered but un-enabled provider ---- */
TEST(etw_prov_enabled_queries_false) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister);
    __typeof__(&EventEnabled) en = ET(EventEnabled); __typeof__(&EventProviderEnabled) pe = ET(EventProviderEnabled);
    REQUIRE(r); REQUIRE(u); REQUIRE(en); REQUIRE(pe);
    GUID g = prov_guid(7); REGHANDLE h = 0; EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    EVENT_DESCRIPTOR d = desc(4, 0);
    EXPECT_EQ(en(h, &d), 0);
    d = desc(0, 0); EXPECT_EQ(en(h, &d), 0);
    d = desc(5, 0xffffffffffffffffULL); EXPECT_EQ(en(h, &d), 0);
    EXPECT_EQ(pe(h, 0, 0), 0);
    EXPECT_EQ(pe(h, 5, 0xff), 0);
    EXPECT_EQ(u(h), 0);
}
TEST(etw_prov_enabled_queries_invalid_handles) {
    __typeof__(&EventEnabled) en = ET(EventEnabled); __typeof__(&EventProviderEnabled) pe = ET(EventProviderEnabled);
    REQUIRE(en); REQUIRE(pe);
    EVENT_DESCRIPTOR d = desc(4, 0);
    EXPECT_EQ(en(0, &d), 0);
    EXPECT_EQ(en(1, &d), 0);
    EXPECT_EQ(en(0, NULL), 0);
    EXPECT_EQ(pe(0, 0, 0), 0);
    EXPECT_EQ(pe(1, 4, 1), 0);
    EXPECT_EQ(pe(0xffffffffffffffffULL, 4, 1), 0);   /* odd handle */
}
TEST(etw_prov_enabled_null_descriptor) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); __typeof__(&EventEnabled) en = ET(EventEnabled);
    REQUIRE(r); REQUIRE(u); REQUIRE(en);
    GUID g = prov_guid(8); REGHANDLE h = 0; EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    EXPECT_EQ(en(h, NULL), 0);   /* NULL descriptor is simply "not enabled" */
    EXPECT_EQ(u(h), 0);
}

/* ---- EventWrite / EventWriteString ---- */
TEST(etw_prov_writestring_invalid_handle) {
    __typeof__(&EventWriteString) w = ET(EventWriteString); REQUIRE(w);
    EXPECT_EQ(w(0, 4, 0, L"x"), ERROR_INVALID_HANDLE);
    EXPECT_EQ(w(1, 4, 0, L"x"), ERROR_INVALID_HANDLE);                       /* odd handle */
    EXPECT_EQ(w(0xffffffffffffffffULL, 4, 0, NULL), ERROR_INVALID_HANDLE);   /* handle check precedes the string check */
}
TEST(etw_prov_writestring_null_string) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); __typeof__(&EventWriteString) w = ET(EventWriteString);
    REQUIRE(r); REQUIRE(u); REQUIRE(w);
    GUID g = prov_guid(9); REGHANDLE h = 0; EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    /* The string check precedes the enabled check, so it fires even though nothing is listening. */
    EXPECT_EQ(w(h, 4, 0, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(w(h, 4, 0, L"hello"), 0);   /* disabled provider: success, event dropped */
    EXPECT_EQ(w(h, 0, 0xff, L""), 0);
    EXPECT_EQ(u(h), 0);
}
TEST(etw_prov_write_disabled_succeeds) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister); __typeof__(&EventWrite) w = ET(EventWrite);
    REQUIRE(r); REQUIRE(u); REQUIRE(w);
    GUID g = prov_guid(10); REGHANDLE h = 0; EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    EVENT_DESCRIPTOR d = desc(4, 1);
    EXPECT_EQ(w(h, &d, 0, NULL), 0);
    DWORD v = 0x12345678; EVENT_DATA_DESCRIPTOR dd; EventDataDescCreate(&dd, &v, sizeof v);
    EXPECT_EQ(w(h, &d, 1, &dd), 0);
    EXPECT_EQ(u(h), 0);
}
TEST(etw_prov_write_invalid_handle) {
    __typeof__(&EventWrite) w = ET(EventWrite); REQUIRE(w);
    EVENT_DESCRIPTOR d = desc(4, 1);
    ULONG e = w(0, &d, 0, NULL);
    t_note("EventWrite(handle 0) -> %lu", e);
}

/* ---- EventActivityIdControl: per-thread activity id ---- */
static int guid_eq(const GUID *a, const GUID *b) { return IsEqualGUID(a, b); }
TEST(etw_activity_null_pointer) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    for (ULONG code = 0; code <= 6; code++) {
        ULONG e = c(code, NULL);
        CHECKF(e == ERROR_INVALID_PARAMETER, "code %lu got %lu", code, e);
    }
}
TEST(etw_activity_invalid_codes) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID g = prov_guid(20), orig = g;
    static const ULONG codes[] = {0, 6, 7, 100, 0xffffffffu};
    for (unsigned i = 0; i < sizeof codes / sizeof codes[0]; i++) {
        ULONG e = c(codes[i], &g);
        CHECKF(e == ERROR_INVALID_PARAMETER, "code %lu got %lu", codes[i], e);
    }
    CHECK(guid_eq(&g, &orig));   /* buffer untouched on failure */
}
TEST(etw_activity_set_get_roundtrip) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID saved, a = prov_guid(21), got;
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_GET_ID, &saved), 0);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_SET_ID, &a), 0);
    memset(&got, 0xcc, sizeof got);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_GET_ID, &got), 0);
    CHECK(guid_eq(&got, &a));
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_SET_ID, &saved), 0);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_GET_ID, &got), 0);
    CHECK(guid_eq(&got, &saved));
}
TEST(etw_activity_get_set_swaps) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID saved, a = prov_guid(22), b = prov_guid(23), x, got;
    c(EVENT_ACTIVITY_CTRL_GET_ID, &saved);
    c(EVENT_ACTIVITY_CTRL_SET_ID, &a);
    x = b;
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_GET_SET_ID, &x), 0);
    CHECK(guid_eq(&x, &a));                 /* previous value returned in place */
    c(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(guid_eq(&got, &b));               /* new value installed */
    c(EVENT_ACTIVITY_CTRL_SET_ID, &saved);
}
TEST(etw_activity_create_id_does_not_set) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID saved, a = prov_guid(24), n1, n2, got;
    c(EVENT_ACTIVITY_CTRL_GET_ID, &saved);
    c(EVENT_ACTIVITY_CTRL_SET_ID, &a);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_CREATE_ID, &n1), 0);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_CREATE_ID, &n2), 0);
    CHECK(!guid_eq(&n1, &kZeroGuid)); CHECK(!guid_eq(&n2, &kZeroGuid));
    CHECK(!guid_eq(&n1, &n2));
    c(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(guid_eq(&got, &a));               /* CREATE_ID leaves the thread's id untouched */
    c(EVENT_ACTIVITY_CTRL_SET_ID, &saved);
}
TEST(etw_activity_create_set_id) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID saved, a = prov_guid(25), x, got;
    c(EVENT_ACTIVITY_CTRL_GET_ID, &saved);
    c(EVENT_ACTIVITY_CTRL_SET_ID, &a);
    EXPECT_EQ(c(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &x), 0);
    c(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(!guid_eq(&got, &kZeroGuid));
    CHECK(!guid_eq(&got, &a));              /* current id replaced by a fresh GUID */
    t_note("CREATE_SET_ID returned previous=%d new=%d", guid_eq(&x, &a), guid_eq(&x, &got));
    c(EVENT_ACTIVITY_CTRL_SET_ID, &saved);
}
typedef struct { GUID seen_initial, seen_after; HANDLE done; } actctx;
static DWORD WINAPI act_thread(LPVOID p) {
    actctx *x = p; __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl);
    c(EVENT_ACTIVITY_CTRL_GET_ID, &x->seen_initial);
    GUID m = prov_guid(40); c(EVENT_ACTIVITY_CTRL_SET_ID, &m);
    c(EVENT_ACTIVITY_CTRL_GET_ID, &x->seen_after);
    SetEvent(x->done); return 0;
}
TEST(etw_activity_is_per_thread) {
    __typeof__(&EventActivityIdControl) c = ET(EventActivityIdControl); REQUIRE(c);
    GUID saved, mine = prov_guid(41), got;
    c(EVENT_ACTIVITY_CTRL_GET_ID, &saved);
    c(EVENT_ACTIVITY_CTRL_SET_ID, &mine);
    actctx x; memset(&x, 0xcc, sizeof x); x.done = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE t = CreateThread(NULL, 0, act_thread, &x, 0, NULL);
    CHECK(t != NULL);
    if (t) {
        DWORD w = WaitForSingleObject(x.done, 5000);
        EXPECT_EQ(w, WAIT_OBJECT_0);
        if (w == WAIT_OBJECT_0) {
            CHECK(guid_eq(&x.seen_initial, &kZeroGuid));      /* a new thread starts with a zero id */
            GUID m = prov_guid(40); CHECK(guid_eq(&x.seen_after, &m));
        }
        WaitForSingleObject(t, 2000); CloseHandle(t);
    }
    c(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(guid_eq(&got, &mine));            /* other thread's SET_ID did not leak here */
    CloseHandle(x.done);
    c(EVENT_ACTIVITY_CTRL_SET_ID, &saved);
}

/* ---- EventSetInformation (ntdll EtwEventSetInformation: class switch comes before any handle use) ---- */
TEST(etw_prov_setinfo_unsupported_classes) {
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(s);
    char buf[16] = {0};
    static const int cls[] = {1, 4, 5, 99, 0x7fff};
    for (unsigned i = 0; i < sizeof cls / sizeof cls[0]; i++) {
        ULONG e = s(0, (EVENT_INFO_CLASS)cls[i], buf, sizeof buf);
        CHECKF(e == ERROR_NOT_SUPPORTED, "class %d got %lu", cls[i], e);
    }
}
TEST(etw_prov_setinfo_class0_needs_zero_length) {
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(s);
    char buf[16] = {0};
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)0, buf, 4), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)0, NULL, 1), ERROR_INVALID_PARAMETER);
}
TEST(etw_prov_setinfo_traits_length_rules) {
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(s);
    char buf[16] = {0};
    /* class 2 (provider traits): needs a buffer of 3..65535 bytes */
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)2, NULL, 16), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)2, buf, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)2, buf, 2), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)2, buf, -1), ERROR_INVALID_PARAMETER);
}
TEST(etw_prov_setinfo_descriptor_type_arg_rules) {
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(s);
    char buf[16] = {0};
    /* class 3 (use descriptor type): exactly one byte of non-NULL data */
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)3, NULL, 1), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)3, NULL, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)3, buf, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(s(0, (EVENT_INFO_CLASS)3, buf, 2), ERROR_INVALID_PARAMETER);
}
TEST(etw_prov_setinfo_invalid_handle) {
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(s);
    char buf[16] = {0};
    ULONG e = s(0, (EVENT_INFO_CLASS)2, buf, sizeof buf);
    CHECK(e != 0); t_note("EventSetInformation(handle 0, traits) -> %lu", e);
}
TEST(etw_prov_setinfo_descriptor_type_registered) {
    __typeof__(&EventRegister) r = ET(EventRegister); __typeof__(&EventUnregister) u = ET(EventUnregister);
    __typeof__(&EventSetInformation) s = ET(EventSetInformation); REQUIRE(r); REQUIRE(u); REQUIRE(s);
    GUID g = prov_guid(51); REGHANDLE h = 0; EXPECT_EQ(r(&g, NULL, NULL, &h), 0);
    BYTE one = 1;
    ULONG e = s(h, (EVENT_INFO_CLASS)3, &one, 1);
    t_note("EventSetInformation(UseDescriptorType, 1 byte) -> %lu", e);
    EXPECT_EQ(u(h), 0);
}
