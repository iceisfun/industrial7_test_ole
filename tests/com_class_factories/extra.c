/* Bulk / table-driven coverage of registration and activation behaviour. */
#include "cg.h"

#define STA() LONG live0_; do { if (!cg_api()) return; live0_ = cg_live_objects; EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK); } while (0)
#define UNINIT() pCoUninitialize()

static GUID derived(unsigned n) { GUID g = cg_TC0; g.Data1 = 0x6f1d0000u + n; return g; }

TEST(cgx_register_many_distinct_classes) {
    STA();
    enum { N = 64 };
    static CGFactory f[N]; GUID g[N]; DWORD c[N];
    for (int i = 0; i < N; i++) { g[i] = derived(i); cg_factory_init(&f[i], &g[i]); EQ(cg_register(&f[i], CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c[i]), S_OK); CHECK(c[i] != 0); }
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (c[i] == c[j]) { CHECKF(0, "cookies %d and %d collide (%lu)", i, j, c[i]); }
    for (int i = 0; i < N; i++) {
        void *p = NULL;
        EQ(pCoGetClassObject(&g[i], CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &p), S_OK);
        t_check(p == &f[i].iface || p != NULL, __FILE__, __LINE__, "factory returned", NULL);
        cg_release(p);
    }
    for (int i = N - 1; i >= 0; i--) EQ(pCoRevokeClassObject(c[i]), S_OK);
    for (int i = 0; i < N; i++) { EQ(f[i].ref, 1); }
    UNINIT();
}
TEST(cgx_register_revoke_loop) {
    STA();
    CGFactory f; cg_factory_init(&f, &cg_TC0);
    DWORD seen = 0;
    for (int i = 0; i < 200; i++) {
        DWORD c = 0;
        if (cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c) != S_OK || !c) { CHECKF(0, "iteration %d", i); break; }
        if (pCoRevokeClassObject(c) != S_OK) { CHECKF(0, "revoke iteration %d", i); break; }
        seen = c;
    }
    EQ(f.ref, 1);
    t_note("last cookie after 200 register/revoke cycles: %lu", seen);
    UNINIT();
}
TEST(cgx_create_loop) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    int bad = 0;
    for (int i = 0; i < 500; i++) {
        IUnknown *u = NULL;
        if (pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&u) != S_OK || !u) { bad++; continue; }
        cg_release(u);
    }
    EQ(bad, 0); EQ(f.creates, 500); EQ(cg_live_objects, live0_);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_no_factory_calls_after_revoke) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    EQ(pCoRevokeClassObject(c), S_OK);
    LONG q = f.qis, cr = f.creates;
    void *p = NULL;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p), CG_E_CLASSNOTREG);
    EQ(f.qis, q); EQ(f.creates, cr);
    UNINIT();
}
TEST(cgx_get_twice_same_factory_identity) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    void *a = NULL, *b = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &a), S_OK);
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &b), S_OK);
    CHECK(a == b);
    cg_release(a); cg_release(b);
    UNINIT();
    EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    UNINIT();
}
TEST(cgx_com_does_not_lock_server_for_get_and_create) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    IUnknown *u = NULL;
    EQ(pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&u), S_OK);
    cg_release(u);
    void *p = NULL; pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &p); cg_release(p);
    t_note("LockServer calls made by COM for in-proc activation: lock=%ld unlock=%ld", f.lockCalls, f.unlockCalls);
    EQ(f.locks, 0);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_register_inproc_and_local_together) {
    STA();
    CGFactory f; DWORD c = 0; cg_factory_init(&f, &cg_TC0);
    HRESULT hr = cg_register(&f, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        static const DWORD ctx[] = { CLSCTX_INPROC_SERVER, CLSCTX_LOCAL_SERVER, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER };
        for (int i = 0; i < 3; i++) {
            void *p = NULL;
            HRESULT h = pCoGetClassObject(&cg_TC0, ctx[i], NULL, &cg_IID_IClassFactory, &p);
            CHECKF(h == S_OK, "lookup ctx 0x%lx -> 0x%08lx", ctx[i], h);
            cg_release(p);
        }
        EQ(pCoRevokeClassObject(c), S_OK);
    }
    UNINIT();
}
TEST(cgx_local_registration_seen_by_inproc_lookup) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    void *p = (void *)(ULONG_PTR)1;
    HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &p);
    t_note("INPROC_SERVER lookup of a LOCAL_SERVER registration -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) cg_release(p); else CHECK(p == NULL);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_get_with_coserverinfo_local_name) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    COSERVERINFO si; memset(&si, 0, sizeof si);
    void *p = NULL;
    HRESULT hr = pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, &si, &cg_IID_IClassFactory, &p);
    t_note("CoGetClassObject with COSERVERINFO{NULL name} -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) cg_release(p);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_multi_qi_pattern_table) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    const GUID *good[] = { &cg_IID_IPersistFile, &cg_IID_IPersist, &cg_IID_IPersistStorage, &cg_IID_IUnknown };
    const GUID *bad[]  = { &cg_IID_IPrivate, &cg_IID_IClassFactory2, &cg_IID_IDispatch, &cg_IID_IMoniker };
    for (unsigned mask = 0; mask < 16; mask++) {
        MULTI_QI q[5]; int n = 5, nbad = 0;
        q[0].pIID = &cg_IID_IUnknown;        /* entry 0 is always supported */
        for (int i = 1; i < n; i++) { int isbad = (mask >> (i - 1)) & 1; q[i].pIID = isbad ? bad[i - 1] : good[i - 1]; nbad += isbad; }
        for (int i = 0; i < n; i++) { q[i].pItf = NULL; q[i].hr = E_NOTIMPL; }
        HRESULT hr = pCoCreateInstanceEx(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, NULL, n, q);
        HRESULT want = nbad ? CG_S_NOTALLINTERFACES : S_OK;
        CHECKF(hr == want, "mask %u hr=0x%08lx want 0x%08lx", mask, hr, want);
        for (int i = 0; i < n; i++) {
            int isbad = (i > 0) && ((mask >> (i - 1)) & 1);
            CHECKF((q[i].hr == (isbad ? CG_E_NOINTERFACE : S_OK)), "mask %u entry %d hr=0x%08lx", mask, i, q[i].hr);
            CHECKF((q[i].pItf == NULL) == isbad, "mask %u entry %d pItf=%p", mask, i, q[i].pItf);
            cg_release(q[i].pItf);
        }
    }
    EQ(cg_live_objects, live0_);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
static CGFactory g_mt_f; static LONG g_mt_bad;
static DWORD WINAPI th_create_many(LPVOID p) {
    HRESULT ih = pCoInitializeEx(NULL, COINIT_MULTITHREADED);
    for (int i = 0; i < 25; i++) {
        IUnknown *u = NULL;
        if (pCoCreateInstance(&cg_TC2, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&u) != S_OK || !u) InterlockedIncrement(&g_mt_bad);
        else cg_release(u);
    }
    if (SUCCEEDED(ih)) pCoUninitialize();
    return 0;
}
TEST(cgx_mta_concurrent_creation) {
    if (!cg_api()) return;
    LONG live0_ = cg_live_objects;
    EQ(pCoInitializeEx(NULL, COINIT_MULTITHREADED), S_OK);
    DWORD c; cg_factory_init(&g_mt_f, &cg_TC2); g_mt_bad = 0;
    EQ(cg_register(&g_mt_f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, th_create_many, NULL, 0, NULL);
    DWORD w = WaitForMultipleObjects(4, h, TRUE, 10000);
    EQ(w, WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) if (h[i]) CloseHandle(h[i]);
    EQ(g_mt_bad, 0);
    EQ(g_mt_f.creates, 100);
    EQ(cg_live_objects, live0_);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_sta_registration_class_requires_creating_thread) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC2);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    g_mt_bad = 0;
    HANDLE h = CreateThread(NULL, 0, th_create_many, NULL, 0, NULL);   /* MTA threads: cannot see the STA registration */
    EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0); CloseHandle(h);
    EQ(g_mt_bad, 25);
    EQ(f.creates, 0);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_registration_survives_other_coinitialize_cycles) {
    STA();
    CGFactory f; DWORD c; cg_factory_init(&f, &cg_TC0);
    EQ(cg_register(&f, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &c), S_OK);
    for (int i = 0; i < 5; i++) { EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_FALSE); UNINIT(); }
    void *p = NULL;
    EQ(pCoGetClassObject(&cg_TC0, CLSCTX_INPROC_SERVER, NULL, &cg_IID_IClassFactory, &p), S_OK);
    cg_release(p);
    EQ(pCoRevokeClassObject(c), S_OK);
    UNINIT();
}
TEST(cgx_coinitialize_mode_mismatch_blocks_nothing_for_registry) {
    if (!cg_api()) return;
    EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    HRESULT hr = pCoInitializeEx(NULL, COINIT_MULTITHREADED);
    EQ(hr, (HRESULT)0x80010106);   /* RPC_E_CHANGED_MODE */
    UNINIT();
}
