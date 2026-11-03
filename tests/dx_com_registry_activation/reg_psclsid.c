/* Interface\{IID}\ProxyStubClsid32 registration, CoGetPSClsid and CoRegisterPSClsid. */
#include "dxcommon.h"

static const GUID NULLG;
static void mkiface(GUID *iid, const char *ps /* NULL = no ProxyStubClsid32 */, char *key) {
    char k[128], gs[40], sub[160]; dx_newguid(iid, DXK_IFACE); dx_guidstr(iid, gs); snprintf(k, sizeof k, "Interface\\%s", gs); dx_track(k);
    dx_set(k, NULL, "IOletTestInterface");
    if (ps) { snprintf(sub, sizeof sub, "%s\\ProxyStubClsid32", k); dx_set(sub, NULL, ps); }
    if (key) strcpy(key, k);
}
static int wait_pump(HANDLE h, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    for (;;) {
        DWORD left = end - GetTickCount(); if ((int)left < 0) return 0;
        DWORD w = MsgWaitForMultipleObjects(1, &h, FALSE, left, QS_ALLINPUT);
        if (w == WAIT_OBJECT_0) return 1;
        if (w == WAIT_TIMEOUT) return 0;
        MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
    }
}
static DWORD WINAPI mta_get(LPVOID a) {
    void **args = a; GUID got; memset(&got, 0xcc, sizeof got);
    HRESULT hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { *(HRESULT *)args[1] = hr; return 1; }
    hr = U(CoGetPSClsid)((const GUID *)args[0], &got);
    *(HRESULT *)args[1] = hr; *(GUID *)args[2] = got;
    U(CoUninitialize)(); return 0;
}

TEST(dxps_getpsclsid_from_registry) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, ps, got, ps2; char gs[40]; dx_newguid(&ps, DXK_NORMAL); dx_guidstr(&ps, gs); mkiface(&iid, gs, NULL);
    dx_coinit(COINIT_APARTMENTTHREADED); memset(&got, 0xcc, sizeof got);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK); CHECK(IsEqualGUID(&got, &ps));
    dx_couninit();
    (void)ps2;
}
TEST(dxps_getpsclsid_same_in_mta) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, ps, got; char gs[40]; dx_newguid(&ps, DXK_NORMAL); dx_guidstr(&ps, gs); mkiface(&iid, gs, NULL);
    HRESULT hr = E_FAIL; void *args[3] = { &iid, &hr, &got }; memset(&got, 0xcc, sizeof got);
    HANDLE t = CreateThread(NULL, 0, mta_get, args, 0, NULL);
    CHECK(t && wait_pump(t, 10000)); if (t) CloseHandle(t);
    EQ(hr, S_OK); CHECK(IsEqualGUID(&got, &ps));
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxps_getpsclsid_unregistered_iid) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, got; dx_newguid(&iid, DXK_IFACE); dx_coinit(COINIT_APARTMENTTHREADED);
    memset(&got, 0xcc, sizeof got);
    EQ(U(CoGetPSClsid)(&iid, &got), REGDB_E_IIDNOTREG);
    t_note("output after REGDB_E_IIDNOTREG: %08lx (0xcccccccc = untouched)", got.Data1);
    dx_couninit();
}
TEST(dxps_getpsclsid_invalid_args) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, got; dx_newguid(&iid, DXK_IFACE); dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoGetPSClsid)(&IID_IClassFactory, NULL), E_INVALIDARG);   /* Wine: CoGetPSClsid for null clsid */
    if (t_is_wine()) { t_skip("NULL IID dereferences in this Wine"); dx_couninit(); return; }
    EQ(U(CoGetPSClsid)(NULL, &got), E_INVALIDARG);
    EQ(U(CoGetPSClsid)(NULL, NULL), E_INVALIDARG);
    dx_couninit();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxps_getpsclsid_system_interfaces) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    dx_coinit(COINIT_APARTMENTTHREADED);
    static const IID *ids[] = { &IID_IClassFactory, &IID_IPersist, &IID_IStream, &IID_IEnumOLEVERB, &IID_IOleWindow };
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
        GUID got; memset(&got, 0, sizeof got); HRESULT hr = U(CoGetPSClsid)(ids[i], &got);
        CHECKF(hr == S_OK && !IsEqualGUID(&got, &NULLG), "iid #%u hr=%08lx", i, hr);
    }
    dx_couninit();
}
TEST(dxps_getpsclsid_interface_key_without_psclsid) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, got; mkiface(&iid, NULL, NULL); dx_coinit(COINIT_APARTMENTTHREADED);
    HRESULT hr = U(CoGetPSClsid)(&iid, &got);
    CHECK(FAILED(hr)); t_note("Interface key without ProxyStubClsid32: hr=%08lx", hr);
    dx_couninit();
}
TEST(dxps_getpsclsid_only_16bit_value) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, ps, got; char gs[40], k[128], sub[160]; dx_newguid(&ps, DXK_NORMAL); dx_guidstr(&ps, gs);
    mkiface(&iid, NULL, k); snprintf(sub, sizeof sub, "%s\\ProxyStubClsid", k); dx_set(sub, NULL, gs);   /* 16-bit proxy only */
    dx_coinit(COINIT_APARTMENTTHREADED);
    HRESULT hr = U(CoGetPSClsid)(&iid, &got);
    CHECK(FAILED(hr)); t_note("only ProxyStubClsid (16-bit): hr=%08lx", hr);
    dx_couninit();
}
TEST(dxps_getpsclsid_garbage_value) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, got; mkiface(&iid, "not-a-clsid", NULL); dx_coinit(COINIT_APARTMENTTHREADED);
    HRESULT hr = U(CoGetPSClsid)(&iid, &got);
    CHECK(FAILED(hr)); t_note("garbage ProxyStubClsid32 value: hr=%08lx", hr);
    dx_couninit();
}
TEST(dxps_getpsclsid_tracks_registry_changes) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, a, b, got; char ga[40], gb[40], k[128], sub[160]; dx_newguid(&a, DXK_NORMAL); dx_newguid(&b, DXK_NORMAL); dx_guidstr(&a, ga); dx_guidstr(&b, gb);
    mkiface(&iid, ga, k); snprintf(sub, sizeof sub, "%s\\ProxyStubClsid32", k);
    dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK); CHECK(IsEqualGUID(&got, &a));
    dx_set(sub, NULL, gb);
    HRESULT hr = U(CoGetPSClsid)(&iid, &got);
    t_note("after rewriting ProxyStubClsid32: hr=%08lx now %s (a cache would still return the old value)", hr, IsEqualGUID(&got, &b) ? "new" : "old");
    dx_couninit();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxps_registerpsclsid_overrides_registry) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid, a, b, got; char ga[40], k[128]; dx_newguid(&a, DXK_NORMAL); dx_newguid(&b, DXK_NORMAL); dx_guidstr(&a, ga);
    mkiface(&iid, ga, k); dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoRegisterPSClsid)(&iid, &b), S_OK);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK); CHECK(IsEqualGUID(&got, &b));   /* runtime registration wins over the registry */
    char sub[160], v[80]; snprintf(sub, sizeof sub, "%s\\ProxyStubClsid32", k);
    CHECK(dx_get(sub, NULL, v, sizeof v) == 0); EQ(strcmp(v, ga), 0);      /* the registry is not modified */
    dx_couninit();
}
TEST(dxps_registerpsclsid_unregistered_interface) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid, ps, got; dx_newguid(&iid, DXK_IFACE); dx_newguid(&ps, DXK_NORMAL); dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoRegisterPSClsid)(&iid, &ps), S_OK);   /* neither GUID needs to exist anywhere */
    memset(&got, 0xcc, sizeof got);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK); CHECK(IsEqualGUID(&got, &ps));
    char k[128], gs[40]; dx_guidstr(&iid, gs); snprintf(k, sizeof k, "Interface\\%s", gs);
    CHECK(!dx_exists(k));
    dx_couninit();
}
TEST(dxps_registerpsclsid_visible_across_apartments) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid, ps, got; dx_newguid(&iid, DXK_IFACE); dx_newguid(&ps, DXK_NORMAL); dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoRegisterPSClsid)(&iid, &ps), S_OK);
    HRESULT hr = E_FAIL; void *args[3] = { &iid, &hr, &got }; memset(&got, 0xcc, sizeof got);
    HANDLE t = CreateThread(NULL, 0, mta_get, args, 0, NULL);
    CHECK(t && wait_pump(t, 10000)); if (t) CloseHandle(t);
    t_note("PS CLSID registered in the STA, queried from an MTA thread: hr=%08lx", hr);
    dx_couninit();
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxps_registerpsclsid_forgotten_after_uninit) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid, ps, got; dx_newguid(&iid, DXK_IFACE); dx_newguid(&ps, DXK_NORMAL);
    dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoRegisterPSClsid)(&iid, &ps), S_OK);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK);
    dx_couninit();   /* last apartment of the process goes away */
    dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoGetPSClsid)(&iid, &got), REGDB_E_IIDNOTREG);
    dx_couninit();
}
TEST(dxps_registerpsclsid_invalid_args) {
    REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid; dx_newguid(&iid, DXK_IFACE); dx_coinit(COINIT_APARTMENTTHREADED);
    if (t_is_wine()) { t_skip("NULL arguments dereference in this Wine"); dx_couninit(); return; }
    EQ(U(CoRegisterPSClsid)(&iid, NULL), E_INVALIDARG);
    EQ(U(CoRegisterPSClsid)(NULL, &iid), E_INVALIDARG);
    EQ(U(CoRegisterPSClsid)(NULL, NULL), E_INVALIDARG);
    dx_couninit();
}
TEST(dxps_registerpsclsid_overwrite_runtime) {
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoRegisterPSClsid)); DX_BEGIN();
    GUID iid, a, b, got; dx_newguid(&iid, DXK_IFACE); dx_newguid(&a, DXK_NORMAL); dx_newguid(&b, DXK_NORMAL); dx_coinit(COINIT_APARTMENTTHREADED);
    EQ(U(CoRegisterPSClsid)(&iid, &a), S_OK);
    HRESULT hr = U(CoRegisterPSClsid)(&iid, &b);
    EQ(U(CoGetPSClsid)(&iid, &got), S_OK);
    t_note("re-registering an IID: hr=%08lx; CoGetPSClsid now returns the %s one", hr, IsEqualGUID(&got, &b) ? "second" : "first");
    dx_couninit();
}
TEST(dxps_interface_key_in_32bit_view) {
    REQUIRE(U(CoGetPSClsid)); DX_BEGIN();
    GUID iid, ps, got; char gs[40], k[128], sub[160]; dx_newguid(&iid, DXK_IFACE); dx_newguid(&ps, DXK_NORMAL); dx_guidstr(&ps, gs);
    char igs[40]; dx_guidstr(&iid, igs); snprintf(k, sizeof k, "Interface\\%s", igs); dx_track(k);
    dx_view(KEY_WOW64_32KEY); snprintf(sub, sizeof sub, "%s\\ProxyStubClsid32", k); dx_set(sub, NULL, gs); dx_view(0);
    dx_coinit(COINIT_APARTMENTTHREADED); memset(&got, 0xcc, sizeof got);
    HRESULT hr = U(CoGetPSClsid)(&iid, &got);
    t_note("Interface key only in the 32-bit view, queried by a 64-bit process: hr=%08lx (Wine's test expects S_OK there)", hr);
    dx_couninit();
}
TEST(dxps_standard_marshaling_uses_registered_proxy_clsid) {
    /* CoMarshalInterface of IPersist works only because CoGetPSClsid(IID_IPersist) resolves: smoke check of the pair */
    REQUIRE(U(CoGetPSClsid)); REQUIRE(U(CoGetClassObject)); DX_BEGIN();
    dx_coinit(COINIT_APARTMENTTHREADED);
    GUID ps; EQ(U(CoGetPSClsid)(&IID_IPersist, &ps), S_OK);
    IUnknown *f = NULL; HRESULT hr = U(CoGetClassObject)(&ps, CLSCTX_INPROC_SERVER, NULL, &IID_IPSFactoryBuffer, (void **)&f);
    EQ(hr, S_OK); if (f) f->lpVtbl->Release(f);
    dx_couninit();
}
