/* State-changing: CoTreatAsClass writes HKCU\Software\Classes\CLSID\{old}\TreatAs. Only keys created here are touched. */
#include "cg.h"

#define STA() LONG live0_; do { if (!cg_api()) return; live0_ = cg_live_objects; EQ(pCoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK); } while (0)
#define UNINIT() pCoUninitialize()

static const GUID cg_NULLGUID = {0};
static void clsid_key_path(const GUID *g, char *out, size_t cap) {
    snprintf(out, cap, "Software\\Classes\\CLSID\\{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3,
        g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}
static void sweep(void) {
    const GUID *g[] = { &cg_TC0, &cg_TC1, &cg_TC2, &cg_TC3 };
    for (int i = 0; i < 4; i++) { char p[256]; clsid_key_path(g[i], p, sizeof p); RegDeleteTreeA(HKEY_CURRENT_USER, p); }
}
/* create HKCU\Software\Classes\CLSID\{g} so that HKCR writes land in HKCU */
static int make_class(const GUID *g, const char *name) {
    char p[256]; HKEY k; clsid_key_path(g, p, sizeof p);
    if (RegCreateKeyExA(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) return 0;
    RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)name, (DWORD)strlen(name) + 1);
    RegCloseKey(k); return 1;
}
static HRESULT poll_treatas(__typeof__(&CoGetTreatAsClass) p, const GUID *in, const GUID *want, CLSID *out) {
    HRESULT hr = S_OK;
    for (int i = 0; i < 20; i++) { hr = p(in, out); if (IsEqualGUID(out, want)) return hr; Sleep(100); }
    return hr;
}

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(cgdx_treatas_set_get_clear) {
    STA();
    __typeof__(&CoTreatAsClass) pT = U(CoTreatAsClass); __typeof__(&CoGetTreatAsClass) pG = U(CoGetTreatAsClass);
    REQUIRE(pT); REQUIRE(pG);
    sweep();
    if (!make_class(&cg_TC0, "olet treatas source")) { t_skip("cannot create HKCU class key"); UNINIT(); return; }
    make_class(&cg_TC1, "olet treatas target");
    HRESULT hr = pT(&cg_TC0, &cg_TC1);
    if (hr == CG_E_CLASSNOTREG) { t_skip("HKCU class key not visible through HKCR (0x%08lx)", hr); sweep(); UNINIT(); return; }
    if (hr == E_ACCESSDENIED || hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED)) { t_skip("CoTreatAsClass needs write access: 0x%08lx", hr); sweep(); UNINIT(); return; }
    EQ(hr, S_OK);
    CLSID out = cg_IID_IPrivate;
    hr = poll_treatas(pG, &cg_TC0, &cg_TC1, &out);
    EQ(hr, S_OK);
    CHECK(IsEqualGUID(&out, &cg_TC1));
    /* CLSID_NULL removes the mapping */
    EQ(pT(&cg_TC0, &cg_NULLGUID), S_OK);
    out = cg_IID_IPrivate;
    hr = pG(&cg_TC0, &out);
    EQ(hr, S_FALSE);
    CHECK(IsEqualGUID(&out, &cg_TC0));
    sweep(); UNINIT();
}
TEST(cgdx_treatas_unknown_old_class) {
    STA();
    __typeof__(&CoTreatAsClass) pT = U(CoTreatAsClass); REQUIRE(pT);
    sweep();
    HRESULT hr = pT(&cg_TC2, &cg_TC1);
    CHECK(FAILED(hr));
    t_note("CoTreatAsClass on a CLSID with no registry key -> 0x%08lx", hr);
    if (SUCCEEDED(hr)) pT(&cg_TC2, &cg_NULLGUID);
    sweep(); UNINIT();
}
TEST(cgdx_treatas_clear_without_mapping) {
    STA();
    __typeof__(&CoTreatAsClass) pT = U(CoTreatAsClass); REQUIRE(pT);
    sweep();
    if (!make_class(&cg_TC0, "olet treatas source")) { t_skip("cannot create HKCU class key"); UNINIT(); return; }
    HRESULT hr = pT(&cg_TC0, &cg_NULLGUID);
    t_note("CoTreatAsClass(old, CLSID_NULL) with nothing mapped -> 0x%08lx", hr);
    CHECK(hr == S_OK || FAILED(hr));
    sweep(); UNINIT();
}
TEST(cgdx_treatas_redirects_creation) {
    STA();
    __typeof__(&CoTreatAsClass) pT = U(CoTreatAsClass); __typeof__(&CoGetTreatAsClass) pG = U(CoGetTreatAsClass);
    REQUIRE(pT); REQUIRE(pG);
    sweep();
    if (!make_class(&cg_TC0, "olet treatas source")) { t_skip("cannot create HKCU class key"); UNINIT(); return; }
    make_class(&cg_TC1, "olet treatas target");
    CGFactory fa, fb; DWORD ca = 0, cb = 0;
    cg_factory_init(&fa, &cg_TC0); cg_factory_init(&fb, &cg_TC1);
    EQ(cg_register(&fb, CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE, &cb), S_OK);
    HRESULT hr = pT(&cg_TC0, &cg_TC1);
    if (FAILED(hr)) { t_skip("CoTreatAsClass failed 0x%08lx", hr); pCoRevokeClassObject(cb); sweep(); UNINIT(); return; }
    CLSID out; poll_treatas(pG, &cg_TC0, &cg_TC1, &out);
    IUnknown *u = NULL; hr = E_FAIL;
    for (int i = 0; i < 30 && fb.creates == 0; i++) {
        hr = pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, (void **)&u);
        if (SUCCEEDED(hr) && u) { cg_release(u); u = NULL; }
        if (fb.creates == 0) Sleep(100);
    }
    EQ(hr, S_OK);
    EQ(fb.creates, 1);       /* the object came from the TreatAs target's factory */
    EQ(fa.creates, 0);
    /* with TreatAs removed and no registration for TC0 the class is gone again */
    EQ(pT(&cg_TC0, &cg_NULLGUID), S_OK);
    Sleep(300);
    void *p = (void *)(ULONG_PTR)1;
    hr = pCoCreateInstance(&cg_TC0, NULL, CLSCTX_INPROC_SERVER, &cg_IID_IUnknown, &p);
    EQ(hr, CG_E_CLASSNOTREG);
    pCoRevokeClassObject(cb);
    sweep(); UNINIT();
}
