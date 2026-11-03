/* Out-of-process activation: CLSID\{..}\LocalServer32 = "<this exe>" --child dxlsrv ...; the child registers the class
 * with CoRegisterClassObject and serves IClassFactory/IPersist until told to quit (named event) or after 25 s. */
#include "dxcommon.h"

#define NEED() do { DX_BEGIN(); } while (0)

static void rel(IUnknown *u) { if (u) u->lpVtbl->Release(u); }
/* returns the server pid reported by the instance (0 on failure) */
static HRESULT lcreate(const GUID *c, DWORD ctx, IPersist **p, dx_info *i) {
    HRESULT hr = U(CoCreateInstance)(c, NULL, ctx, &IID_IPersist, (void **)p);
    memset(i, 0, sizeof *i);
    if (SUCCEEDED(hr) && *p) dx_getinfo((IUnknown *)*p, i);
    return hr;
}

TEST(dxlocal_createinstance_basic) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *p = NULL; dx_info i;
    HRESULT hr = lcreate(&c, CLSCTX_LOCAL_SERVER, &p, &i);
    EQ(hr, S_OK);
    if (p) {
        CHECK(!dx_is_direct((IUnknown *)p));   /* a proxy, not our vtable */
        CHECK(i.marker_ok); EQ(i.idx, c.Data3);
        CHECK(i.pid != 0 && i.pid != GetCurrentProcessId());
        CHECK(i.flags & DXF_LOCAL);
        CHECK(i.flags & DXF_EMBEDDING);   /* COM appends -Embedding to the LocalServer32 command line */
        t_note("server pid %lu tid %lu apt %d flags %x", i.pid, i.tid, i.apt, i.flags);
        p->lpVtbl->Release(p);
    }
    dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_inproc_context_does_not_launch) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    void *p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    CHECK(p == NULL);
    p = (void *)1;
    EQ(U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_HANDLER, &IID_IUnknown, &p), REGDB_E_CLASSNOTREG);
    dx_couninit();
}
TEST(dxlocal_clsctx_all_and_server) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *p = NULL; dx_info i, j;
    EQ(lcreate(&c, CLSCTX_ALL, &p, &i), S_OK);
    IPersist *q = NULL; EQ(lcreate(&c, CLSCTX_SERVER, &q, &j), S_OK);
    CHECK(i.pid != 0 && i.pid != GetCurrentProcessId());
    rel((IUnknown *)p); rel((IUnknown *)q); dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_multiuse_shares_server_process) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *a = NULL, *b = NULL; dx_info ia, ib;
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &a, &ia), S_OK); EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &b, &ib), S_OK);
    CHECK(ia.pid != 0); EQ(ib.pid, ia.pid);   /* REGCLS_MULTIPLEUSE: one registered class object serves both */
    rel((IUnknown *)a); rel((IUnknown *)b); dx_local_stop(&c, ia.pid); dx_couninit();
}
TEST(dxlocal_singleuse_launches_new_process) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "single", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *a = NULL, *b = NULL; dx_info ia, ib;
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &a, &ia), S_OK);
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &b, &ib), S_OK);
    CHECK(ia.pid != 0 && ib.pid != 0 && ia.pid != ib.pid);   /* REGCLS_SINGLEUSE: the class object is revoked after one use */
    rel((IUnknown *)a); rel((IUnknown *)b);
    dx_local_stop(&c, ia.pid); dx_local_stop(&c, ib.pid); dx_couninit();
}
TEST(dxlocal_mta_server) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "mta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_MULTITHREADED);
    IPersist *p = NULL; dx_info i;
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &p, &i), S_OK);
    if (p) { EQ(i.apt, APTTYPE_MTA); t_note("MTA server instance created on tid %lu", i.tid); }
    rel((IUnknown *)p); dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_sta_server_apartment) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_MULTITHREADED);
    IPersist *p = NULL; dx_info i;
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &p, &i), S_OK);
    if (p) CHECK(i.apt == APTTYPE_STA || i.apt == APTTYPE_MAINSTA);
    rel((IUnknown *)p); dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_getclassobject_remote_factory) {
    REQUIRE(U(CoGetClassObject)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IClassFactory *f = NULL; IPersist *p = NULL; dx_info i; memset(&i, 0, sizeof i);
    EQ(U(CoGetClassObject)(&c, CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void **)&f), S_OK);
    if (f) {
        CHECK(!((void *)f == NULL));
        EQ(f->lpVtbl->CreateInstance(f, NULL, &IID_IPersist, (void **)&p), S_OK);   /* through the IClassFactory proxy */
        if (p) { dx_getinfo((IUnknown *)p, &i); CHECK(i.pid != GetCurrentProcessId()); p->lpVtbl->Release(p); }
        EQ(f->lpVtbl->LockServer(f, TRUE), S_OK); EQ(f->lpVtbl->LockServer(f, FALSE), S_OK);
        void *x = (void *)1;
        EQ(f->lpVtbl->CreateInstance(f, NULL, &IID_IStream, &x), E_NOINTERFACE);   /* the server's own error comes back */
        CHECK(x == NULL);
        f->lpVtbl->Release(f);
    }
    dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_aggregation_across_process_refused) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    static IUnknownVtbl vt; static IUnknown outer = { &vt };
    void *p = (void *)1;
    HRESULT hr = U(CoCreateInstance)(&c, &outer, CLSCTX_LOCAL_SERVER, &IID_IUnknown, &p);
    EQ(hr, CLASS_E_NOAGGREGATION); CHECK(p == NULL);
    dx_local_stop(&c, 0); dx_couninit();
}
TEST(dxlocal_unsupported_iid_from_server) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    void *p = (void *)1;
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_LOCAL_SERVER, &IID_IStream, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL); t_note("out-of-proc CoCreateInstance for an IID the server refuses: hr=%08lx", hr);
    dx_local_stop(&c, 0); dx_couninit();
}
TEST(dxlocal_createinstanceex_multi_qi) {
    REQUIRE(U(CoCreateInstanceEx)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    MULTI_QI q[3]; memset(q, 0, sizeof q); q[0].pIID = &IID_IPersist; q[1].pIID = &IID_IStream; q[2].pIID = &IID_IUnknown;
    HRESULT hr = U(CoCreateInstanceEx)(&c, NULL, CLSCTX_LOCAL_SERVER, NULL, 3, q);
    EQ(hr, CO_S_NOTALLINTERFACES);
    EQ(q[0].hr, S_OK); EQ(q[1].hr, E_NOINTERFACE); EQ(q[2].hr, S_OK);
    dx_info i; memset(&i, 0, sizeof i); if (q[0].pItf) dx_getinfo(q[0].pItf, &i);
    for (int k = 0; k < 3; k++) rel(q[k].pItf);
    dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_server_executable_missing) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[160]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet missing exe");
    snprintf(sub, sizeof sub, "%s\\LocalServer32", k); dx_set(sub, NULL, "C:\\olet\\no\\such\\server.exe");
    dx_coinit(COINIT_APARTMENTTHREADED); void *p = (void *)1; DWORD t0 = GetTickCount();
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL); CHECK(GetTickCount() - t0 < 30000);
    t_note("LocalServer32 pointing to a missing exe: hr=%08lx in %lu ms", hr, GetTickCount() - t0);
    dx_couninit();
}
TEST(dxlocal_server_exits_without_registering) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char k[128], sub[200], cmd[600]; dx_newguid(&c, DXK_NORMAL); dx_clsidkey(&c, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet exits");
    snprintf(sub, sizeof sub, "%s\\LocalServer32", k); snprintf(cmd, sizeof cmd, "\"%s\" --child dxexit", dx_server_path()); dx_set(sub, NULL, cmd);
    dx_coinit(COINIT_APARTMENTTHREADED); void *p = (void *)1; DWORD t0 = GetTickCount();
    HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, &p);
    CHECK(FAILED(hr)); CHECK(p == NULL);
    t_note("server process exited without CoRegisterClassObject: hr=%08lx after %lu ms (CO_E_SERVER_EXEC_FAILURE=80080005)", hr, GetTickCount() - t0);
    dx_couninit();
}
TEST(dxlocal_server_killed_proxy_fails) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *p = NULL; dx_info i; EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &p, &i), S_OK);
    if (p && i.pid) {
        HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, i.pid);
        if (h) {
            TerminateProcess(h, 3); WaitForSingleObject(h, 5000); CloseHandle(h);
            CLSID id; HRESULT hr = p->lpVtbl->GetClassID(p, &id);
            CHECK(FAILED(hr));
            t_note("call on a proxy whose server was killed: hr=%08lx (RPC_E_DISCONNECTED=80010108, RPC_S_SERVER_UNAVAILABLE=800706ba)", hr);
        }
        p->lpVtbl->Release(p);
    }
    dx_local_stop(&c, 0); dx_couninit();
}
TEST(dxlocal_activation_by_progid) {
    REQUIRE(U(CoCreateInstance)); REQUIRE(U(CLSIDFromProgID)); NEED();
    GUID c, got; char k[128], p[80], s[200], gs[40]; WCHAR w[80];
    dx_reg_local(&c, "sta", "multi", k, sizeof k); dx_local_prepare(&c); dx_guidstr(&c, gs);
    dx_progid(p, sizeof p, "Local.1"); dx_track(p); snprintf(s, sizeof s, "%s\\CLSID", p); dx_set(s, NULL, gs);
    snprintf(s, sizeof s, "%s\\ProgID", k); dx_set(s, NULL, p);
    for (int n = 0; n < 80; n++) { w[n] = (unsigned char)p[n]; if (!p[n]) break; }
    EQ(U(CLSIDFromProgID)(w, &got), S_OK); CHECK(IsEqualGUID(&got, &c));
    dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *ip = NULL; dx_info i; EQ(lcreate(&got, CLSCTX_LOCAL_SERVER, &ip, &i), S_OK);
    CHECK(i.idx == c.Data3);
    rel((IUnknown *)ip); dx_local_stop(&c, i.pid); dx_couninit();
}
TEST(dxlocal_restart_after_exit) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_local(&c, "sta", "multi", NULL, 0); dx_local_prepare(&c); dx_coinit(COINIT_APARTMENTTHREADED);
    IPersist *a = NULL, *b = NULL; dx_info ia, ib;
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &a, &ia), S_OK);
    rel((IUnknown *)a); dx_local_stop(&c, ia.pid);   /* server asked to quit and waited for */
    dx_local_prepare(&c);                              /* re-arm the quit event */
    EQ(lcreate(&c, CLSCTX_LOCAL_SERVER, &b, &ib), S_OK);
    CHECK(ia.pid != 0 && ib.pid != 0 && ia.pid != ib.pid);
    rel((IUnknown *)b); dx_local_stop(&c, ib.pid); dx_couninit();
}
