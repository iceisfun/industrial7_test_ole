/* State-changing endpoint-mapper tests (dx_ area): registers a throw-away interface in the RpcSs endpoint map,
 * resolves and enumerates it, and unregisters it again. Needs the RpcSs service; every test skips when the mapper
 * is unreachable. Leftovers from an aborted earlier run (same fixed interface uuid, annotation prefix "olet") are
 * swept at the start of each test. */
#include "epx.h"

static const RPC_SYNTAX_IDENTIFIER ndr20 = { {0x8a885d04, 0x1ceb, 0x11c9, {0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60}}, {2, 0} };
static const UUID g_uuid = { 0x7a3c2200, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x88} };
static const UUID g_obj1 = { 0x7a3c2201, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x01} };
static const UUID g_obj2 = { 0x7a3c2202, 0x4c1d, 0x4e7b, {0x80, 0x01, 0, 0, 0, 0, 0, 0x02} };

static void RPC_ENTRY h_copy(RPC_MESSAGE *m) {
    DWORD v = 0x600D;
    m->BufferLength = 4;
    if (U(I_RpcGetBuffer)(m) != RPC_S_OK) return;
    memcpy(m->Buffer, &v, 4);
}
static RPC_DISPATCH_FUNCTION g_fn[1];
static RPC_DISPATCH_TABLE g_dt;
static RPC_SERVER_INTERFACE g_si;
static RPC_CLIENT_INTERFACE g_ci;
static WCHAR g_ep[64];
static RPC_BINDING_VECTOR *g_vec;      /* the one-binding vector we register (points at g_one) */
static RPC_BINDING_VECTOR *g_all;
static struct { ULONG Count; RPC_BINDING_HANDLE BindingH[1]; } g_one;

static void iface_init(void) {
    memset(&g_si, 0, sizeof g_si); memset(&g_ci, 0, sizeof g_ci);
    g_fn[0] = h_copy; g_dt.DispatchTableCount = 1; g_dt.DispatchTable = g_fn;
    g_si.Length = sizeof g_si; g_si.InterfaceId.SyntaxGUID = g_uuid; g_si.InterfaceId.SyntaxVersion.MajorVersion = 1;
    g_si.TransferSyntax = ndr20; g_si.DispatchTable = &g_dt;
    g_ci.Length = sizeof g_ci; g_ci.InterfaceId = g_si.InterfaceId; g_ci.TransferSyntax = ndr20;
}
static DWORD WINAPI wait_thread(void *p) { return U(RpcMgmtWaitServerListen)(); }

/* Remove every entry of our fixed interface uuid that an earlier (crashed) run may have left behind. */
static void sweep(void) {
    RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned guard = 0;
    if (!U(RpcMgmtEpEltInqBegin) || t_is_wine()) return;
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    if (U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_MATCH_BY_IF, &id, RPC_C_VERS_ALL, NULL, &ctx) != RPC_S_OK) return;
    for (;;) {
        RPC_BINDING_HANDLE b = NULL; RPC_IF_ID got; UUID obj; RPC_WSTR ann = NULL;
        if (guard++ > 2000) break;
        if (U(RpcMgmtEpEltInqNextW)(ctx, &got, &b, &obj, &ann) != RPC_S_OK) break;
        if (memcmp(&got.Uuid, &g_uuid, sizeof(UUID)) == 0 && b) {
            struct { ULONG Count; RPC_BINDING_HANDLE BindingH[1]; } v1;      /* RPC_BINDING_VECTOR layout */
            v1.Count = 1; v1.BindingH[0] = b;
            U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, (RPC_BINDING_VECTOR *)&v1, NULL);
        }
        if (b) U(RpcBindingFree)(&b);
        if (ann) U(RpcStringFreeW)(&ann);
    }
    U(RpcMgmtEpEltInqDone)(&ctx);
}

static DWORD endpoint_of(RPC_BINDING_HANDLE h, WCHAR *out, size_t cch);
/* start a listening ncalrpc server for g_si; returns FALSE (after cleanup) when impossible */
static BOOL up(void) {
    RPC_STATUS st;
    iface_init();
    epx_unique_ep(g_ep, 64);
    st = U(RpcServerUseProtseqEpW)((RPC_WSTR)L"ncalrpc", 10, (RPC_WSTR)g_ep, NULL);
    if (st != RPC_S_OK) { t_note("UseProtseqEp = %lu", (unsigned long)st); return FALSE; }
    st = U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&g_si, NULL, NULL, 0, 10, NULL);
    if (st != RPC_S_OK) { t_note("RegisterIfEx = %lu", (unsigned long)st); return FALSE; }
    st = U(RpcServerInqBindings)(&g_all);
    if (st != RPC_S_OK) { t_note("InqBindings = %lu", (unsigned long)st); return FALSE; }
    {   /* earlier tests' endpoints stay in the server: register only this test's binding */
        ULONG i; WCHAR e[128];
        g_one.Count = 0;
        for (i = 0; i < g_all->Count; i++) {
            if (endpoint_of(g_all->BindingH[i], e, 128) == RPC_S_OK && !lstrcmpW(e, g_ep)) { g_one.Count = 1; g_one.BindingH[0] = g_all->BindingH[i]; break; }
        }
        if (!g_one.Count) { t_note("endpoint not found among %lu bindings", (unsigned long)g_all->Count); return FALSE; }
        g_vec = (RPC_BINDING_VECTOR *)&g_one;
    }
    st = U(RpcServerListen)(1, 10, TRUE);
    if (st == 1713 /* RPC_S_ALREADY_LISTENING */) st = RPC_S_OK;
    if (st != RPC_S_OK) t_note("RpcServerListen = %lu", (unsigned long)st);
    return st == RPC_S_OK;
}
static void down(void) {
    HANDLE th;
    if (g_vec) { U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, g_vec, NULL); g_vec = NULL; }
    if (g_all) { U(RpcBindingVectorFree)(&g_all); g_all = NULL; }
    U(RpcMgmtStopServerListening)(NULL);
    th = CreateThread(NULL, 0, wait_thread, NULL, 0, NULL);
    if (th) { if (WaitForSingleObject(th, 10000) != WAIT_OBJECT_0) TerminateThread(th, 1); CloseHandle(th); }
    U(RpcServerUnregisterIf)((RPC_IF_HANDLE)&g_si, NULL, 1);
}
static RPC_BINDING_HANDLE partial_bind(const UUID *obj) {
    RPC_WSTR sb = NULL, os = NULL; RPC_BINDING_HANDLE h = NULL;
    if (obj && U(UuidToStringW)(obj, &os) != RPC_S_OK) return NULL;
    if (U(RpcStringBindingComposeW)(os, (RPC_WSTR)L"ncalrpc", NULL, NULL, NULL, &sb) != RPC_S_OK) return NULL;
    if (os) U(RpcStringFreeW)(&os);
    if (U(RpcBindingFromStringBindingW)(sb, &h) != RPC_S_OK) h = NULL;
    U(RpcStringFreeW)(&sb);
    return h;
}
static BOOL mapper_unreachable(RPC_STATUS st) {
    return st == 1722 /* RPC_S_SERVER_UNAVAILABLE */ || st == ERROR_ACCESS_DENIED || st == 1752 /* EPT_S_CANT_PERFORM_OP */ || st == 1726 /* RPC_S_CALL_FAILED_DNE */ || st == 1727 /* RPC_S_CALL_FAILED */;
}
static DWORD endpoint_of(RPC_BINDING_HANDLE h, WCHAR *out, size_t cch) {
    RPC_WSTR s = NULL, ep = NULL; RPC_STATUS st;
    out[0] = 0;
    st = U(RpcBindingToStringBindingW)(h, &s);
    if (st != RPC_S_OK) return st;
    st = U(RpcStringBindingParseW)(s, NULL, NULL, NULL, &ep, NULL);
    if (st == RPC_S_OK && ep) { lstrcpynW(out, (WCHAR *)ep, (int)cch); U(RpcStringFreeW)(&ep); }
    U(RpcStringFreeW)(&s);
    return st;
}

#define NEED_EPM() do { \
    REQUIRE(U(RpcEpRegisterW) && U(RpcEpUnregister) && U(RpcEpResolveBinding) && U(RpcServerInqBindings) && U(RpcServerListen) && U(I_RpcGetBuffer)); \
    sweep(); \
    if (!up()) { down(); t_skip("cannot start ncalrpc server"); return; } \
} while (0)
#define NEED_INQ() do { REQUIRE(U(RpcMgmtEpEltInqBegin) && U(RpcMgmtEpEltInqNextW) && U(RpcMgmtEpEltInqDone)); WINE_STUB(); } while (0)

static RPC_STATUS reg(const UUID *obj, const WCHAR *ann) {
    UUID_VECTOR uv; RPC_STATUS st;
    if (obj) { uv.Count = 1; uv.Uuid[0] = (UUID *)obj; }
    st = U(RpcEpRegisterW)((RPC_IF_HANDLE)&g_si, g_vec, obj ? &uv : NULL, (RPC_WSTR)ann);
    return st;
}

TEST(dx_ep_register_resolve_call_unregister) {
    RPC_BINDING_HANDLE h; RPC_STATUS st; WCHAR ep[128];
    WINE_STUB();   /* wine's endpoint mapper does not track registrations per endpoint */
    NEED_EPM();
    st = reg(NULL, L"olet-dx-ep");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    h = partial_bind(NULL);
    CHECK(h != NULL);
    if (h) {
        EQ(endpoint_of(h, ep, 128), RPC_S_OK);
        CHECK(ep[0] == 0);                                           /* no endpoint before resolving */
        EQ(U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci), RPC_S_OK);
        EQ(endpoint_of(h, ep, 128), RPC_S_OK);
        EXPECT_WSTR(ep, g_ep);                                       /* the mapper returned our endpoint */
        {   /* call through the resolved handle */
            RPC_MESSAGE m; DWORD v = 0;
            memset(&m, 0, sizeof m);
            m.Handle = h; m.DataRepresentation = 0x10; m.ProcNum = 0; m.BufferLength = 0;
            m.RpcInterfaceInformation = &g_ci; m.TransferSyntax = &g_ci.TransferSyntax;
            if (U(I_RpcGetBuffer)(&m) == RPC_S_OK && U(I_RpcSendReceive)(&m) == RPC_S_OK) {
                if (m.BufferLength >= 4) v = *(DWORD *)m.Buffer;
                U(I_RpcFreeBuffer)(&m);
            }
            EQ(v, 0x600D);
        }
        U(RpcBindingFree)(&h);
    }
    EQ(U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, g_vec, NULL), RPC_S_OK);
    h = partial_bind(NULL);
    if (h) {
        st = U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci);
        t_note("RpcEpResolveBinding after unregister = %lu (EPT_S_NOT_REGISTERED is 1753)", (unsigned long)st);
        CHECK(st != RPC_S_OK);
        U(RpcBindingFree)(&h);
    }
    down();
}

TEST(dx_ep_unregister_twice_is_ok) {
    RPC_STATUS st;
    NEED_EPM();
    st = reg(NULL, L"olet-dx-ep2");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    EQ(U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, g_vec, NULL), RPC_S_OK);
    st = U(RpcEpUnregister)((RPC_IF_HANDLE)&g_si, g_vec, NULL);
    t_note("second RpcEpUnregister = %lu", (unsigned long)st);
    down();
}

TEST(dx_ep_register_twice_replaces_entry) {
    RPC_STATUS st; RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned found = 0, guard = 0;
    NEED_EPM(); NEED_INQ();
    st = reg(NULL, L"olet-dx-first");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    EQ(reg(NULL, L"olet-dx-second"), RPC_S_OK);       /* RpcEpRegister replaces an identical entry (annotation changes) */
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_MATCH_BY_IF, &id, RPC_C_VERS_EXACT, NULL, &ctx), RPC_S_OK);
    while (guard++ < 1000) {
        RPC_IF_ID got; UUID obj; RPC_WSTR ann = NULL; RPC_STATUS s2 = U(RpcMgmtEpEltInqNextW)(ctx, &got, NULL, &obj, &ann);
        if (s2 != RPC_S_OK) { EQ(s2, 1772); break; }
        if (!memcmp(&got.Uuid, &g_uuid, sizeof(UUID))) { found++; t_note("entry annotation \"%ls\"", ann ? (WCHAR *)ann : L""); }
        if (ann) U(RpcStringFreeW)(&ann);
    }
    EQ(found, 1);
    EQ(U(RpcMgmtEpEltInqDone)(&ctx), RPC_S_OK);
    CHECK(ctx == NULL);
    down();
}

TEST(dx_ep_inq_by_interface_finds_entry_and_ends_with_no_more_entries) {
    RPC_STATUS st; RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned found = 0, guard = 0; WCHAR eps[128];
    NEED_EPM(); NEED_INQ();
    st = reg(NULL, L"olet-dx-inq");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_MATCH_BY_IF, &id, RPC_C_VERS_ALL, NULL, &ctx), RPC_S_OK);
    CHECK(ctx != NULL);
    while (guard++ < 1000) {
        RPC_IF_ID got; UUID obj; RPC_WSTR ann = NULL; RPC_BINDING_HANDLE b = NULL;
        RPC_STATUS s2 = U(RpcMgmtEpEltInqNextW)(ctx, &got, &b, &obj, &ann);
        if (s2 != RPC_S_OK) { EQ(s2, 1772); break; }       /* RPC_X_NO_MORE_ENTRIES ends the walk */
        if (!memcmp(&got.Uuid, &g_uuid, sizeof(UUID))) {
            found++;
            EQ(got.VersMajor, 1); EQ(got.VersMinor, 0);
            CHECK(ann != NULL);
            if (ann) EXPECT_WSTR((WCHAR *)ann, L"olet-dx-inq");
            CHECK(b != NULL);
            if (b) { EQ(endpoint_of(b, eps, 128), RPC_S_OK); EXPECT_WSTR(eps, g_ep); }
            CHECK(!memcmp(&obj, &(UUID){0}, sizeof(UUID)));      /* registered without an object uuid -> nil */
        }
        if (b) U(RpcBindingFree)(&b);
        if (ann) U(RpcStringFreeW)(&ann);
    }
    EQ(found, 1);
    /* the walk stays at the end */
    {
        RPC_IF_ID got; UUID obj;
        EQ(U(RpcMgmtEpEltInqNextW)(ctx, &got, NULL, &obj, NULL), 1772);
    }
    EQ(U(RpcMgmtEpEltInqDone)(&ctx), RPC_S_OK);
    CHECK(ctx == NULL);
    down();
}

TEST(dx_ep_inq_by_object_and_both) {
    RPC_STATUS st; RPC_EP_INQ_HANDLE ctx = NULL; RPC_IF_ID id; unsigned found = 0, guard = 0, t;
    NEED_EPM(); NEED_INQ();
    st = reg(&g_obj1, L"olet-dx-obj");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    id.Uuid = g_uuid; id.VersMajor = 1; id.VersMinor = 0;
    for (t = 2; t <= 3; t++) {          /* RPC_C_EP_MATCH_BY_OBJ, RPC_C_EP_MATCH_BY_BOTH */
        found = 0; guard = 0; ctx = NULL;
        EQ(U(RpcMgmtEpEltInqBegin)(NULL, t, &id, RPC_C_VERS_ALL, (UUID *)&g_obj1, &ctx), RPC_S_OK);
        while (guard++ < 1000) {
            RPC_IF_ID got; UUID obj; RPC_STATUS s2 = U(RpcMgmtEpEltInqNextW)(ctx, &got, NULL, &obj, NULL);
            if (s2 != RPC_S_OK) break;
            if (!memcmp(&got.Uuid, &g_uuid, sizeof(UUID))) { found++; CHECK(!memcmp(&obj, &g_obj1, sizeof(UUID))); }
        }
        CHECKF(found == 1, "match type %u found %u", t, found);
        U(RpcMgmtEpEltInqDone)(&ctx);
    }
    /* another object uuid matches nothing of ours */
    found = 0; guard = 0; ctx = NULL;
    EQ(U(RpcMgmtEpEltInqBegin)(NULL, 3, &id, RPC_C_VERS_ALL, (UUID *)&g_obj2, &ctx), RPC_S_OK);
    while (guard++ < 1000) {
        RPC_IF_ID got; UUID obj; if (U(RpcMgmtEpEltInqNextW)(ctx, &got, NULL, &obj, NULL) != RPC_S_OK) break;
        if (!memcmp(&got.Uuid, &g_uuid, sizeof(UUID))) found++;
    }
    EQ(found, 0);
    U(RpcMgmtEpEltInqDone)(&ctx);
    down();
}

TEST(dx_ep_resolve_honours_object_uuid) {
    RPC_STATUS st; RPC_BINDING_HANDLE h; WCHAR ep[128];
    WINE_STUB();   /* wine's endpoint mapper does not track registrations per endpoint */
    NEED_EPM();
    st = reg(&g_obj1, L"olet-dx-res");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    h = partial_bind(&g_obj1);
    if (h) {
        EQ(U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci), RPC_S_OK);
        EQ(endpoint_of(h, ep, 128), RPC_S_OK);
        EXPECT_WSTR(ep, g_ep);
        U(RpcBindingFree)(&h);
    } else CHECK(0);
    h = partial_bind(&g_obj2);                 /* a different object uuid is not registered */
    if (h) {
        st = U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci);
        t_note("resolve with unregistered object uuid = %lu", (unsigned long)st);
        CHECK(st != RPC_S_OK);
        U(RpcBindingFree)(&h);
    } else CHECK(0);
    down();
}

TEST(dx_ep_register_no_replace_registers) {
    RPC_STATUS st; RPC_BINDING_HANDLE h;
    NEED_EPM();
    REQUIRE(U(RpcEpRegisterNoReplaceW));
    st = U(RpcEpRegisterNoReplaceW)((RPC_IF_HANDLE)&g_si, g_vec, NULL, (RPC_WSTR)L"olet-dx-nr");
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    h = partial_bind(NULL);
    if (h) { EQ(U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci), RPC_S_OK); U(RpcBindingFree)(&h); }
    st = U(RpcEpRegisterNoReplaceW)((RPC_IF_HANDLE)&g_si, g_vec, NULL, (RPC_WSTR)L"olet-dx-nr2");
    t_note("second RpcEpRegisterNoReplace of the same entry = %lu", (unsigned long)st);
    down();
}

TEST(dx_ep_register_a_variant_and_long_annotation) {
    RPC_STATUS st; char ann[64]; RPC_BINDING_HANDLE h;
    NEED_EPM();
    REQUIRE(U(RpcEpRegisterA));
    memset(ann, 'q', 63); ann[63] = 0;                       /* longest accepted annotation */
    st = U(RpcEpRegisterA)((RPC_IF_HANDLE)&g_si, g_vec, NULL, (RPC_CSTR)ann);
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    h = partial_bind(NULL);
    if (h) { EQ(U(RpcEpResolveBinding)(h, (RPC_IF_HANDLE)&g_ci), RPC_S_OK); U(RpcBindingFree)(&h); }
    st = U(RpcEpRegisterA)((RPC_IF_HANDLE)&g_si, g_vec, NULL, (RPC_CSTR)"x");
    EQ(st, RPC_S_OK);
    down();
}

TEST(dx_ep_inq_begin_all_elements_walk_is_bounded_and_clean) {
    RPC_STATUS st; RPC_EP_INQ_HANDLE ctx = NULL; unsigned n = 0;
    NEED_EPM(); NEED_INQ();
    st = U(RpcMgmtEpEltInqBegin)(NULL, RPC_C_EP_ALL_ELTS, NULL, 0, NULL, &ctx);
    if (mapper_unreachable(st)) { t_skip("endpoint mapper unreachable (%lu)", (unsigned long)st); down(); return; }
    EQ(st, RPC_S_OK);
    while (n < 5000) {
        RPC_IF_ID got; UUID obj; RPC_WSTR ann = NULL; RPC_STATUS s2 = U(RpcMgmtEpEltInqNextW)(ctx, &got, NULL, &obj, &ann);
        if (s2 != RPC_S_OK) { EQ(s2, 1772); break; }
        n++;
        if (ann) U(RpcStringFreeW)(&ann);
    }
    t_note("endpoint map holds %u element(s)", n);
    CHECK(n > 0);                          /* a running Windows always has system entries (e.g. the DCOM activation interfaces) */
    EQ(U(RpcMgmtEpEltInqDone)(&ctx), RPC_S_OK);
    down();
}
