/* State-changing endpoint-mapper tests (dx_ area) for the ANSI enumeration entry point RpcMgmtEpEltInqNextA.
 * A throw-away interface (fixed uuid, annotation prefix "olet") is registered in the RpcSs endpoint map with
 * RpcEpRegisterA, enumerated with RpcMgmtEpEltInqBegin/NextA and unregistered again. Leftovers of an aborted earlier run
 * are swept first. Every test skips when the endpoint mapper is unreachable. */
#include "gap7.h"
#include <rpcdcep.h>

static const RPC_SYNTAX_IDENTIFIER ndr20 = { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} };
static const UUID g_uuid = { 0x7a3c2700, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x97} };
static const UUID g_obj  = { 0x7a3c2701, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x01} };

static void RPC_ENTRY h_nop(RPC_MESSAGE *m) { m->BufferLength = 0; UIN(RT, I_RpcGetBuffer)(m); }
static RPC_DISPATCH_FUNCTION g_fn[1];
static RPC_DISPATCH_TABLE g_dt;
static RPC_SERVER_INTERFACE g_si;
static char g_ep[64];
static RPC_BINDING_VECTOR *g_all;
static struct { ULONG Count; RPC_BINDING_HANDLE BindingH[1]; } g_one;
static struct { ULONG Count; UUID *Uuid[1]; } g_uv;

static void iface_init(void)
{
    memset(&g_si, 0, sizeof g_si);
    g_fn[0] = h_nop; g_dt.DispatchTableCount = 1; g_dt.DispatchTable = g_fn;
    g_si.Length = sizeof g_si; g_si.InterfaceId.SyntaxGUID = g_uuid; g_si.InterfaceId.SyntaxVersion.MajorVersion = 1;
    g_si.TransferSyntax = ndr20; g_si.DispatchTable = &g_dt;
}
static DWORD WINAPI wait_thread(void *p) { (void)p; return U(RpcMgmtWaitServerListen)(); }

static BOOL mapper_unreachable(RPC_STATUS st)
{
    return st == 1722 /* RPC_S_SERVER_UNAVAILABLE */ || st == ERROR_ACCESS_DENIED || st == 1752 /* EPT_S_CANT_PERFORM_OP */
        || st == 1726 /* RPC_S_CALL_FAILED_DNE */ || st == 1727 /* RPC_S_CALL_FAILED */;
}
static void sweep(void)
{
    RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned guard = 0;
    if (t_is_wine()) return;
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    if (U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_MATCH_BY_IF, &id, RPC_C_VERS_ALL, NULL, &ctx) != RPC_S_OK) return;
    while (guard++ < 2000) {
        RPC_BINDING_HANDLE b = NULL; RPC_IF_ID got; UUID obj; RPC_CSTR ann = NULL;
        if (U(RpcMgmtEpEltInqNextA)(ctx, &got, &b, &obj, &ann) != RPC_S_OK) break;
        if (!memcmp(&got.Uuid, &g_uuid, sizeof(UUID)) && b) {
            struct { ULONG Count; RPC_BINDING_HANDLE BindingH[1]; } v1;
            v1.Count = 1; v1.BindingH[0] = b;
            U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&v1, NULL);
        }
        if (b) U(RpcBindingFree)(&b);
        if (ann) U(RpcStringFreeA)(&ann);
    }
    U(RpcMgmtEpEltInqDone)(&ctx);
}
static int binding_endpoint(RPC_BINDING_HANDLE h, char *out, size_t cap)
{
    RPC_CSTR s = NULL, ep = NULL; int ok = 0;
    out[0] = 0;
    if (U(RpcBindingToStringBindingA)(h, &s) != RPC_S_OK) return 0;
    if (U(RpcStringBindingParseA)(s, NULL, NULL, NULL, &ep, NULL) == RPC_S_OK && ep) { lstrcpynA(out, (char *)ep, (int)cap); ok = 1; U(RpcStringFreeA)(&ep); }
    U(RpcStringFreeA)(&s);
    return ok;
}
static BOOL up(void)
{
    RPC_STATUS st; ULONG i; char e[128];
    iface_init();
    wsprintfA(g_ep, "olet_dxgap7_%lu", GetCurrentProcessId());
    st = U(RpcServerUseProtseqEpA)((RPC_CSTR)"ncalrpc", 10, (RPC_CSTR)g_ep, NULL);
    if (st != RPC_S_OK) { t_note("UseProtseqEp = %lu", (unsigned long)st); return FALSE; }
    st = U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&g_si, NULL, NULL, 0, 10, NULL);
    if (st != RPC_S_OK) { t_note("RegisterIfEx = %lu", (unsigned long)st); return FALSE; }
    st = U(RpcServerInqBindings)(&g_all);
    if (st != RPC_S_OK) { t_note("InqBindings = %lu", (unsigned long)st); return FALSE; }
    g_one.Count = 0;
    for (i = 0; i < g_all->Count; i++)
        if (binding_endpoint(g_all->BindingH[i], e, sizeof e) && !lstrcmpA(e, g_ep)) { g_one.Count = 1; g_one.BindingH[0] = g_all->BindingH[i]; break; }
    if (!g_one.Count) { t_note("endpoint not among %lu bindings", (unsigned long)g_all->Count); return FALSE; }
    st = U(RpcServerListen)(1, 10, TRUE);
    if (st == 1713 /* RPC_S_ALREADY_LISTENING */) st = RPC_S_OK;
    return st == RPC_S_OK;
}
static void down(void)
{
    HANDLE th;
    if (g_one.Count) { U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&g_one, NULL); g_one.Count = 0; }
    if (g_all) { U(RpcBindingVectorFree)(&g_all); g_all = NULL; }
    U(RpcMgmtStopServerListening)(NULL);
    th = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
    if (th) { if (WaitForSingleObject(th, 10000) != WAIT_OBJECT_0) TerminateThread(th, 1); CloseHandle(th); }
    U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_si, NULL, 1);
}
#define NEED() do { \
    REQUIRE(U(RpcEpRegisterA) && U(RpcEpUnregister) && U(RpcMgmtEpEltInqBegin) && U(RpcMgmtEpEltInqNextA) && U(RpcMgmtEpEltInqDone) && U(RpcServerInqBindings)); \
    if (t_is_wine()) { t_skip("endpoint mapper enumeration is not validated under wine"); return; } \
    iface_init(); sweep(); \
    if (!up()) { down(); t_skip("cannot start ncalrpc server"); return; } \
} while (0)

/* returns 1 when our entry was found; fills the outputs */
static int find_entry(RPC_CSTR *ann_out, UUID *obj_out, RPC_BINDING_HANDLE *bind_out, RPC_IF_ID *id_out, RPC_STATUS *final_st, unsigned *seen)
{
    RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned guard = 0; int found = 0; RPC_STATUS st;
    *final_st = RPC_S_OK; *seen = 0;
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    st = U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_MATCH_BY_IF, &id, RPC_C_VERS_ALL, NULL, &ctx);
    if (st != RPC_S_OK) { *final_st = st; return 0; }
    while (guard++ < 2000) {
        RPC_BINDING_HANDLE b = NULL; RPC_IF_ID got; UUID obj; RPC_CSTR ann = NULL;
        st = U(RpcMgmtEpEltInqNextA)(ctx, &got, &b, &obj, &ann);
        if (st != RPC_S_OK) { *final_st = st; break; }
        (*seen)++;
        if (!found && !memcmp(&got.Uuid, &g_uuid, sizeof(UUID))) {
            found = 1; *id_out = got; *obj_out = obj; *bind_out = b; *ann_out = ann;
        } else {
            if (b) U(RpcBindingFree)(&b);
            if (ann) U(RpcStringFreeA)(&ann);
        }
    }
    U(RpcMgmtEpEltInqDone)(&ctx);
    return found;
}

TEST(dx_gap7_ep_inq_next_a_returns_ansi_annotation_object_and_binding)
{
    RPC_STATUS st; RPC_CSTR ann = NULL; UUID obj; RPC_BINDING_HANDLE b = NULL; RPC_IF_ID id; RPC_STATUS fin; unsigned seen; char ep[128];
    NEED();
    g_uv.Count = 1; g_uv.Uuid[0] = (UUID *)&g_obj;
    st = U(RpcEpRegisterA)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&g_one, (UUID_VECTOR *)&g_uv, (RPC_CSTR)"olet_gap7_annotation");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK && find_entry(&ann, &obj, &b, &id, &fin, &seen)) {
        CHECK(!memcmp(&id.Uuid, &g_uuid, sizeof(UUID))); EQ(id.VersMajor, 1); EQ(id.VersMinor, 0);
        CHECK(!memcmp(&obj, &g_obj, sizeof(UUID)));
        CHECK(ann != NULL);
        if (ann) { CHECKF(!strcmp((char *)ann, "olet_gap7_annotation"), "annotation \"%s\"", (char *)ann); U(RpcStringFreeA)(&ann); }
        CHECK(b != NULL);
        if (b) {
            CHECK(binding_endpoint(b, ep, sizeof ep));
            CHECKF(!lstrcmpA(ep, g_ep), "binding endpoint \"%s\" want \"%s\"", ep, g_ep);
            U(RpcBindingFree)(&b);
        }
        EQ(fin, RPC_X_NO_MORE_ENTRIES);                    /* the walk ends with RPC_X_NO_MORE_ENTRIES */
    } else CHECKF(0, "registered entry not enumerated (status %lu, %u elements)", (unsigned long)fin, seen);
    down();
}
TEST(dx_gap7_ep_inq_next_a_empty_annotation_registration)
{
    RPC_STATUS st; RPC_CSTR ann = NULL; UUID obj; RPC_BINDING_HANDLE b = NULL; RPC_IF_ID id; RPC_STATUS fin; unsigned seen;
    NEED();
    st = U(RpcEpRegisterA)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&g_one, NULL, NULL);
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK && find_entry(&ann, &obj, &b, &id, &fin, &seen)) {
        CHECK(!memcmp(&id.Uuid, &g_uuid, sizeof(UUID)));
        CHECK(ann != NULL);
        if (ann) { CHECKF(((char *)ann)[0] == 0, "annotation \"%s\" want empty", (char *)ann); U(RpcStringFreeA)(&ann); }
        t_note("object uuid of an entry registered without a UUID vector is %s", (obj.Data1 == 0 && obj.Data2 == 0 && obj.Data3 == 0) ? "nil" : "non-nil");
        if (b) U(RpcBindingFree)(&b);
    } else CHECKF(0, "registered entry not enumerated");
    down();
}
TEST(dx_gap7_ep_inq_next_a_after_unregister_entry_is_gone)
{
    RPC_STATUS st; RPC_CSTR ann = NULL; UUID obj; RPC_BINDING_HANDLE b = NULL; RPC_IF_ID id; RPC_STATUS fin; unsigned seen;
    NEED();
    st = U(RpcEpRegisterA)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&g_one, NULL, (RPC_CSTR)"olet_gap7_gone");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    CHECK(find_entry(&ann, &obj, &b, &id, &fin, &seen));
    if (ann) U(RpcStringFreeA)(&ann);
    if (b) U(RpcBindingFree)(&b);
    EQ(U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&g_one, NULL), RPC_S_OK);
    ann = NULL; b = NULL;
    CHECK(!find_entry(&ann, &obj, &b, &id, &fin, &seen));
    EQ(fin, RPC_X_NO_MORE_ENTRIES);
    if (ann) U(RpcStringFreeA)(&ann);
    if (b) U(RpcBindingFree)(&b);
    down();
}
