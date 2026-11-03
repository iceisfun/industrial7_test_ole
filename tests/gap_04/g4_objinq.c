/* gap_04: RpcObjectSetInqFn and RpcMgmtSetAuthorizationFn.
 *
 * RpcObjectInqType (Win10 rpcrt4 ObjectInqType): nil object -> nil type, RPC_S_OK; object registered with
 * RpcObjectSetType -> its type; otherwise, with no inquiry function -> nil type and RPC_S_OBJECT_NOT_FOUND (1710);
 * with an inquiry function -> fn(obj, type, &status) is called and its status returned.
 *
 * Management interface (rpc_mgmt_* server stubs): the authorization function is called as
 *   fn(clientBinding, operation, &status) with operation 0=InqIfIds 1=InqServerPrincName 2=InqStats
 *   3=IsServerListening 4=StopServerListening; non-zero -> operation proceeds; zero -> the client gets *status,
 *   or RPC_S_ACCESS_DENIED when fn left it 0. The default function allows everything except operation 4. */
#include "rawfx.h"

#define NOWINE(why) do { if (t_is_wine()) { t_skip("%s", why); return; } } while (0)
#define G4_OBJECT_NOT_FOUND 1710

/* ------------------------------------------------------------------ RpcObjectSetInqFn */
static volatile LONG g_inq_calls;
static UUID *volatile g_inq_obj; static UUID *volatile g_inq_type; static UUID g_inq_obj_copy;
static UUID g_inq_answer; static RPC_STATUS g_inq_status;
static void __RPC_API inq_fn(UUID *obj, UUID *type, RPC_STATUS *status) {
    InterlockedIncrement(&g_inq_calls);
    g_inq_obj = obj; g_inq_type = type; g_inq_obj_copy = *obj;
    *type = g_inq_answer; *status = g_inq_status;
}
static void __RPC_API inq_fn2(UUID *obj, UUID *type, RPC_STATUS *status) {
    InterlockedIncrement(&g_inq_calls);
    memset(type, 0x22, sizeof *type); *status = 0;
}
static void new_uuid(UUID *u) { U(UuidCreate)(u); }
static void reset_inq(void) { g_inq_calls = 0; g_inq_obj = g_inq_type = NULL; g_inq_status = 0; memset(&g_inq_answer, 0, sizeof g_inq_answer); }

TEST(gap4_objectsetinqfn_returns_ok) {
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine");
    EQ(U(RpcObjectSetInqFn)(inq_fn), RPC_S_OK);
    EQ(U(RpcObjectSetInqFn)(inq_fn2), RPC_S_OK);
    EQ(U(RpcObjectSetInqFn)(NULL), RPC_S_OK);
    EQ(U(RpcObjectSetInqFn)(NULL), RPC_S_OK);
}
TEST(gap4_objectinqtype_unknown_without_fn) {
    UUID o, t; UUID zero; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    U(RpcObjectSetInqFn)(NULL);
    new_uuid(&o); memset(&t, 0xFF, sizeof t); memset(&zero, 0, sizeof zero);
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, G4_OBJECT_NOT_FOUND);
    CHECK(!memcmp(&t, &zero, sizeof t));              /* type is cleared on not-found */
}
TEST(gap4_objectinqtype_nil_object_never_calls_fn) {
    UUID o, t, zero; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    reset_inq(); memset(&zero, 0, sizeof zero); memset(&o, 0, sizeof o);
    U(RpcObjectSetInqFn)(inq_fn);
    memset(&t, 0xFF, sizeof t);
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, RPC_S_OK); CHECK(!memcmp(&t, &zero, sizeof t)); EQ(g_inq_calls, 0);
    memset(&t, 0xFF, sizeof t);
    st = U(RpcObjectInqType)(NULL, &t);                 /* NULL object == nil object */
    EQ(st, RPC_S_OK); CHECK(!memcmp(&t, &zero, sizeof t)); EQ(g_inq_calls, 0);
    U(RpcObjectSetInqFn)(NULL);
}
TEST(gap4_objectinqtype_fn_supplies_type) {
    UUID o, t, want; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    reset_inq(); new_uuid(&o); new_uuid(&want); g_inq_answer = want; g_inq_status = RPC_S_OK;
    U(RpcObjectSetInqFn)(inq_fn);
    memset(&t, 0, sizeof t);
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, RPC_S_OK);
    EQ(g_inq_calls, 1);
    CHECK(!memcmp(&t, &want, sizeof t));
    CHECK(g_inq_obj == &o);                             /* the caller's pointers are passed through */
    CHECK(g_inq_type == &t);
    CHECK(!memcmp(&g_inq_obj_copy, &o, sizeof o));
    U(RpcObjectSetInqFn)(NULL);
}
TEST(gap4_objectinqtype_fn_status_propagates) {
    static const RPC_STATUS codes[] = { RPC_S_OBJECT_NOT_FOUND, RPC_S_INVALID_OBJECT, RPC_S_ACCESS_DENIED, 1234, 0x80004005 };
    UUID o, t; unsigned k;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    new_uuid(&o);
    U(RpcObjectSetInqFn)(inq_fn);
    for (k = 0; k < sizeof codes / sizeof *codes; k++) {
        reset_inq(); g_inq_status = codes[k];
        EQ(U(RpcObjectInqType)(&o, &t), codes[k]);
        EQ(g_inq_calls, 1);
    }
    U(RpcObjectSetInqFn)(NULL);
}
TEST(gap4_objectinqtype_null_type_pointer) {
    UUID o, want; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    reset_inq(); new_uuid(&o); new_uuid(&want); g_inq_answer = want;
    U(RpcObjectSetInqFn)(inq_fn);
    st = U(RpcObjectInqType)(&o, NULL);                /* TypeUuid is optional: a scratch UUID is used */
    EQ(st, RPC_S_OK); EQ(g_inq_calls, 1);
    CHECK(g_inq_type != NULL);
    U(RpcObjectSetInqFn)(NULL);
    EQ(U(RpcObjectInqType)(&o, NULL), G4_OBJECT_NOT_FOUND);
}
TEST(gap4_objectinqtype_registered_object_wins) {
    UUID o, ty, t; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType)); REQUIRE(U(RpcObjectSetType));
    reset_inq(); new_uuid(&o); new_uuid(&ty); new_uuid(&g_inq_answer);
    U(RpcObjectSetInqFn)(inq_fn);
    EQ(U(RpcObjectSetType)(&o, &ty), RPC_S_OK);
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, RPC_S_OK); CHECK(!memcmp(&t, &ty, sizeof t));
    EQ(g_inq_calls, 0);                                 /* the fn is only a fallback */
    EQ(U(RpcObjectSetType)(&o, NULL), RPC_S_OK);       /* unregister */
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, RPC_S_OK); EQ(g_inq_calls, 1);               /* now the fallback is consulted */
    U(RpcObjectSetInqFn)(NULL);
    EQ(U(RpcObjectInqType)(&o, &t), G4_OBJECT_NOT_FOUND);
}
TEST(gap4_objectsetinqfn_replace_and_clear) {
    UUID o, t, zero; RPC_STATUS st;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    reset_inq(); new_uuid(&o); memset(&zero, 0x22, sizeof zero);
    U(RpcObjectSetInqFn)(inq_fn);
    U(RpcObjectSetInqFn)(inq_fn2);                      /* the second registration replaces the first */
    st = U(RpcObjectInqType)(&o, &t);
    EQ(st, RPC_S_OK); CHECK(!memcmp(&t, &zero, sizeof t)); EQ(g_inq_calls, 1);
    U(RpcObjectSetInqFn)(NULL);                         /* NULL removes it */
    g_inq_calls = 0;
    EQ(U(RpcObjectInqType)(&o, &t), G4_OBJECT_NOT_FOUND); EQ(g_inq_calls, 0);
}
static DWORD WINAPI inq_thread(void *p) { UUID t; *(RPC_STATUS *)p = U(RpcObjectInqType)(&g_inq_obj_copy, &t); return 0; }
TEST(gap4_objectsetinqfn_is_process_wide) {
    UUID o; RPC_STATUS st = 0xFFFF; HANDLE th;
    REQUIRE(U(RpcObjectSetInqFn)); NOWINE("RpcObjectSetInqFn is an unimplemented stub in wine"); REQUIRE(U(RpcObjectInqType));
    reset_inq(); new_uuid(&o); g_inq_obj_copy = o; g_inq_status = 4321;
    U(RpcObjectSetInqFn)(inq_fn);
    th = CreateThread(NULL, 0, inq_thread, &st, 0, NULL);
    CHECK(th != NULL);
    if (th) { CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0); CloseHandle(th); }
    EQ(st, 4321);                                       /* seen from another thread of the process */
    U(RpcObjectSetInqFn)(NULL);
}

/* ------------------------------------------------------------------ RpcMgmtSetAuthorizationFn */
typedef int (__RPC_API *mgmt_authz_fn)(RPC_BINDING_HANDLE, unsigned long, RPC_STATUS *);
UDECL(RPC_STATUS, RpcMgmtSetAuthorizationFn, (mgmt_authz_fn));

static volatile LONG az_ops[8]; static volatile LONG az_calls; static volatile ULONG_PTR az_last_binding;
static int az_result = 1; static RPC_STATUS az_status;
static int __RPC_API az_fn(RPC_BINDING_HANDLE b, unsigned long op, RPC_STATUS *st) {
    InterlockedIncrement(&az_calls); az_last_binding = (ULONG_PTR)b;
    if (op < 8) InterlockedIncrement(&az_ops[op]);
    if (op != 0) *st = az_status;            /* the InqIfIds stub passes no status pointer */
    return az_result;
}
static void az_reset(int result, RPC_STATUS status) { memset((void *)az_ops, 0, sizeof az_ops); az_calls = 0; az_last_binding = 0; az_result = result; az_status = status; }

TEST(gap4_mgmt_setauthorizationfn_returns_ok) {
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    EQ(UN(RpcMgmtSetAuthorizationFn)(az_fn), RPC_S_OK);
    EQ(UN(RpcMgmtSetAuthorizationFn)(NULL), RPC_S_OK);       /* NULL restores the default */
    EQ(UN(RpcMgmtSetAuthorizationFn)(NULL), RPC_S_OK);
}
TEST(gap4_mgmt_default_allows_listening_query_and_denies_stop) {
    rawfx f; RPC_STATUS st;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    RUP(f, 0xB1, 1, 0);
    EQ(U(RpcMgmtIsServerListening)(f.h), RPC_S_OK);
    st = U(RpcMgmtStopServerListening)(f.h);
    EQ(st, RPC_S_ACCESS_DENIED);                       /* default: operation 4 is not authorized for remote callers */
    CHECK(rx_listening());                             /* the server really is still listening */
    EQ(U(RpcMgmtIsServerListening)(f.h), RPC_S_OK);
    raw_down(&f);
}
TEST(gap4_mgmt_custom_fn_sees_operation_codes) {
    rawfx f; RPC_IF_ID_VECTOR *v = NULL; RPC_STATS_VECTOR *sv = NULL; RPC_STATUS st;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    RUP(f, 0xB2, 1, 0);
    az_reset(1, 0);
    EQ(UN(RpcMgmtSetAuthorizationFn)(az_fn), RPC_S_OK);
    EQ(U(RpcMgmtIsServerListening)(f.h), RPC_S_OK);
    EQ(az_ops[3], 1); EQ(az_calls, 1);
    CHECK(az_last_binding != 0);                       /* a server-side call binding is passed */
    st = U(RpcMgmtInqIfIds)(f.h, &v);
    EQ(st, RPC_S_OK); EQ(az_ops[0], 1);
    if (st == RPC_S_OK && v) { CHECK(v->Count >= 1); U(RpcIfIdVectorFree)(&v); }
    st = U(RpcMgmtInqStats)(f.h, &sv);
    EQ(st, RPC_S_OK); EQ(az_ops[2], 1);
    if (st == RPC_S_OK && sv) U(RpcMgmtStatsVectorFree)(&sv);
    {   RPC_WSTR pn = NULL;
        st = U(RpcMgmtInqServerPrincNameW)(f.h, RPC_C_AUTHN_WINNT, &pn);
        EQ(az_ops[1], 1);
        t_note("RpcMgmtInqServerPrincName(WINNT) -> %lu", (unsigned long)st);
        if (st == RPC_S_OK && pn) U(RpcStringFreeW)(&pn);
    }
    EQ(az_calls, 4);
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    raw_down(&f);
}
TEST(gap4_mgmt_custom_fn_denies) {
    rawfx f; RPC_IF_ID_VECTOR *v = NULL; RPC_STATS_VECTOR *sv = NULL;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    RUP(f, 0xB3, 1, 0);
    az_reset(0, 0);                                    /* deny without supplying a status */
    UN(RpcMgmtSetAuthorizationFn)(az_fn);
    EQ(U(RpcMgmtIsServerListening)(f.h), RPC_S_ACCESS_DENIED);
    EQ(U(RpcMgmtInqIfIds)(f.h, &v), RPC_S_ACCESS_DENIED);
    CHECK(v == NULL);
    EQ(U(RpcMgmtInqStats)(f.h, &sv), RPC_S_ACCESS_DENIED);
    CHECK(sv == NULL);
    EQ(az_calls, 3);
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    EQ(U(RpcMgmtIsServerListening)(f.h), RPC_S_OK);     /* default is back */
    raw_down(&f);
}
TEST(gap4_mgmt_custom_fn_denies_with_status) {
    rawfx f; RPC_STATUS st;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    RUP(f, 0xB4, 1, 0);
    az_reset(0, RPC_S_CALL_FAILED_DNE);
    UN(RpcMgmtSetAuthorizationFn)(az_fn);
    st = U(RpcMgmtIsServerListening)(f.h);
    CHECK(st != RPC_S_OK);
    t_note("deny with status RPC_S_CALL_FAILED_DNE -> client sees %lu", (unsigned long)st);
    EQ(az_ops[3], 1);
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    raw_down(&f);
}
TEST(gap4_mgmt_custom_fn_may_allow_stop) {
    rawfx f; RPC_STATUS st; DWORD t0;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    RUP(f, 0xB5, 1, 0);
    az_reset(1, 0);
    UN(RpcMgmtSetAuthorizationFn)(az_fn);
    st = U(RpcMgmtStopServerListening)(f.h);
    EQ(st, RPC_S_OK);
    EQ(az_ops[4], 1);
    t0 = GetTickCount();
    while (rx_listening() && GetTickCount() - t0 < 5000) Sleep(20);
    CHECK(!rx_listening());                            /* the remote stop took effect */
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    raw_down(&f);
}
TEST(gap4_mgmt_restore_default_denies_stop_again) {
    rawfx f;
    REQUIRE(UN(RpcMgmtSetAuthorizationFn));
    NOWINE("remote management authorization not implemented in wine");
    RUP(f, 0xB6, 1, 0);
    az_reset(1, 0);
    UN(RpcMgmtSetAuthorizationFn)(az_fn);
    UN(RpcMgmtSetAuthorizationFn)(NULL);
    EQ(U(RpcMgmtStopServerListening)(f.h), RPC_S_ACCESS_DENIED);
    EQ(az_calls, 0);                                   /* the custom fn is no longer called */
    CHECK(rx_listening());
    raw_down(&f);
}
