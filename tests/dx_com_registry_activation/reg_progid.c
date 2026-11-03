/* ProgID <-> CLSID registration lookups: CLSIDFromProgID(Ex), ProgIDFromCLSID, CLSIDFromString fallback.
 * All registrations live under HKCU\Software\Classes (olet<pid>.* / CLSID\{7E57C0DE-...-"olet"pid}). */
#include "dxcommon.h"

static const GUID NULLG;
static const GUID CLSID_StdFont = {0x0be35203,0x8f91,0x11ce,{0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51}};

/* Registers CLSID\{g}\ProgID=prog, [VersionIndependentProgID=vi], prog\CLSID={g}, [vi\CLSID, vi\CurVer=prog]. */
static void mk(GUID *g, const char *prog, const char *vi, int vi_has_clsid) {
    char ck[128], s[160], gs[40];
    dx_newguid(g, DXK_NORMAL); dx_guidstr(g, gs); dx_clsidkey(g, ck, sizeof ck); dx_track(ck);
    dx_set(ck, NULL, "olet progid test");
    snprintf(s, sizeof s, "%s\\ProgID", ck); dx_set(s, NULL, prog);
    dx_track(prog); snprintf(s, sizeof s, "%s\\CLSID", prog); dx_set(s, NULL, gs);
    if (vi) {
        snprintf(s, sizeof s, "%s\\VersionIndependentProgID", ck); dx_set(s, NULL, vi);
        dx_track(vi);
        snprintf(s, sizeof s, "%s\\CurVer", vi); dx_set(s, NULL, prog);
        if (vi_has_clsid) { snprintf(s, sizeof s, "%s\\CLSID", vi); dx_set(s, NULL, gs); }
    }
}
static WCHAR *W(const char *a, WCHAR *w, size_t n) { for (size_t i = 0; i < n; i++) { w[i] = (unsigned char)a[i]; if (!a[i]) break; } return w; }

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromprogid_registered) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; char p[80]; WCHAR w[80];
    dx_progid(p, sizeof p, "Basic.1"); mk(&g, p, NULL, 0);
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(p, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
TEST(dxprog_clsidfromprogid_case_insensitive) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; char p[80], up[80]; WCHAR w[80];
    dx_progid(p, sizeof p, "CaseTest.1"); mk(&g, p, NULL, 0);
    snprintf(up, sizeof up, "%s", p); CharUpperA(up);
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(up, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
    CharLowerA(up);
    EQ(U(CLSIDFromProgID)(W(up, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromprogid_invalid_args) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(L"olet.whatever", NULL), E_INVALIDARG);
    EQ(U(CLSIDFromProgID)(NULL, &out), E_INVALIDARG);
    /* invalid args do not touch the output buffer */
    CHECK(out.Data1 == 0xcccccccc);
    EQ(U(CLSIDFromProgID)(NULL, NULL), E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromprogid_notfound) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; WCHAR w[80]; char p[80];
    dx_progid(p, sizeof p, "NoSuchProgId.1");
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(p, w, 80), &out), CO_E_CLASSSTRING);
    CHECK(IsEqualGUID(&out, &NULLG));   /* output is zeroed before the lookup */
}
TEST(dxprog_clsidfromprogid_empty_string) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(L"", &out), CO_E_CLASSSTRING);
    CHECK(IsEqualGUID(&out, &NULLG));
}
TEST(dxprog_clsidfromprogid_no_clsid_subkey) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; WCHAR w[80]; char p[80], s[120];
    dx_progid(p, sizeof p, "NoClsid.1"); dx_track(p);
    dx_set(p, NULL, "a progid without a CLSID subkey");
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(p, w, 80), &out), CO_E_CLASSSTRING);
    CHECK(IsEqualGUID(&out, &NULLG));
    /* empty CLSID default value is no better */
    snprintf(s, sizeof s, "%s\\CLSID", p); dx_set(s, NULL, "");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CLSIDFromProgID)(W(p, w, 80), &out);
    CHECK(FAILED(hr)); t_note("empty CLSID value: hr=%08lx", hr);
}
TEST(dxprog_clsidfromprogid_garbage_clsid_value) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; WCHAR w[80]; char p[80], s[120];
    dx_progid(p, sizeof p, "Garbage.1"); dx_track(p);
    snprintf(s, sizeof s, "%s\\CLSID", p); dx_set(s, NULL, "not-a-guid");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CLSIDFromProgID)(W(p, w, 80), &out);
    CHECK(FAILED(hr)); t_note("garbage CLSID value: hr=%08lx out.Data1=%08lx", hr, out.Data1);
}
TEST(dxprog_clsidfromprogid_without_com_init) {
    /* decomp: only TLS pre-allocation, no CO_E_NOTINITIALIZED check */
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; char p[80]; WCHAR w[80];
    dx_progid(p, sizeof p, "NoInit.1"); mk(&g, p, NULL, 0);
    EQ(U(CLSIDFromProgID)(W(p, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
TEST(dxprog_clsidfromprogid_unicode_name) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; WCHAR key[160], gs[40], prog[80]; char ck[128];
    dx_newguid(&g, DXK_NORMAL); dx_guidwstr(&g, gs); dx_clsidkey(&g, ck, sizeof ck); dx_track(ck);
    snwprintf(prog, 80, L"olet%lu.\x00e9\x4e2d\x6587.1", GetCurrentProcessId());
    WCHAR sub[200]; snwprintf(sub, 200, L"%ls\\CLSID", prog);
    if (dx_setw(sub, NULL, gs)) { t_skip("cannot create unicode key"); return; }
    { char a[160]; WideCharToMultiByte(CP_UTF8, 0, prog, -1, a, sizeof a, NULL, NULL); /* tracked through the sweep by prefix */ }
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(prog, &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
    { HKEY k; RegOpenKeyExA(dx_root(), "Software\\Classes", 0, KEY_ALL_ACCESS, &k); RegDeleteTreeW(k, prog); RegCloseKey(k); }
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromprogid_stdfont_system) {
    REQUIRE(U(CLSIDFromProgID)); REQUIRE(U(ProgIDFromCLSID));
    GUID out = {0}; LPOLESTR s = NULL;
    EQ(U(CLSIDFromProgID)(L"StdFont", &out), S_OK);
    CHECK(IsEqualGUID(&out, &CLSID_StdFont));
    EQ(U(ProgIDFromCLSID)(&CLSID_StdFont, &s), S_OK);
    if (s) { CHECK(!lstrcmpiW(s, L"StdFont")); U(CoTaskMemFree)(s); }
}
TEST(dxprog_hkcr_merged_view_sees_hkcu) {
    DX_BEGIN();
    GUID g; char p[80], s[160], gs[40]; HKEY k = NULL; char val[80] = "";
    dx_progid(p, sizeof p, "Merged.1"); mk(&g, p, NULL, 0);
    dx_guidstr(&g, gs);
    EQ(RegOpenKeyExA(HKEY_CLASSES_ROOT, p, 0, KEY_READ, &k), ERROR_SUCCESS);
    if (k) RegCloseKey(k);
    snprintf(s, sizeof s, "CLSID\\%s", gs);
    EQ(RegOpenKeyExA(HKEY_CLASSES_ROOT, s, 0, KEY_READ, &k), ERROR_SUCCESS);
    if (k) RegCloseKey(k);
    LONG n = sizeof val;
    snprintf(s, sizeof s, "%s\\CLSID", p);
    EQ(RegQueryValueA(HKEY_CLASSES_ROOT, s, val, &n), ERROR_SUCCESS);
    EXPECT_STR(val, gs);
}
TEST(dxprog_progidfromclsid_registered) {
    REQUIRE(U(ProgIDFromCLSID)); DX_BEGIN();
    GUID g; char p[80], vi[80]; LPOLESTR s = (LPOLESTR)(ULONG_PTR)1; WCHAR w[80];
    dx_progid(p, sizeof p, "Round.2"); dx_progid(vi, sizeof vi, "Round"); mk(&g, p, vi, 1);
    EQ(U(ProgIDFromCLSID)(&g, &s), S_OK);
    CHECK(s && s != (LPOLESTR)1);
    if (s && s != (LPOLESTR)1) {
        CHECK(!lstrcmpW(s, W(p, w, 80)));   /* the ProgID value, not the version independent one */
        U(CoTaskMemFree)(s);
    }
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_progidfromclsid_notregistered) {
    REQUIRE(U(ProgIDFromCLSID)); DX_BEGIN();
    GUID g; LPOLESTR s = (LPOLESTR)(ULONG_PTR)0xdeadbeef; dx_newguid(&g, DXK_NORMAL);
    EQ(U(ProgIDFromCLSID)(&g, &s), REGDB_E_CLASSNOTREG);
    CHECK(s == NULL);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_progidfromclsid_invalid_args) {
    REQUIRE(U(ProgIDFromCLSID)); DX_BEGIN();
    GUID g; LPOLESTR s = (LPOLESTR)(ULONG_PTR)0xdeadbeef; dx_newguid(&g, DXK_NORMAL);
    EQ(U(ProgIDFromCLSID)(&g, NULL), E_INVALIDARG);
    EQ(U(ProgIDFromCLSID)(NULL, &s), E_INVALIDARG);
    CHECK(s == (LPOLESTR)(ULONG_PTR)0xdeadbeef);   /* decomp: rejected before the output is cleared */
    EQ(U(ProgIDFromCLSID)(NULL, NULL), E_INVALIDARG);
}
TEST(dxprog_progidfromclsid_class_without_progid) {
    REQUIRE(U(ProgIDFromCLSID)); DX_BEGIN();
    GUID g; char ck[128]; LPOLESTR s = (LPOLESTR)(ULONG_PTR)0xdeadbeef;
    dx_newguid(&g, DXK_NORMAL); dx_clsidkey(&g, ck, sizeof ck); dx_track(ck); dx_set(ck, NULL, "no progid here");
    HRESULT hr = U(ProgIDFromCLSID)(&g, &s);
    EQ(hr, REGDB_E_CLASSNOTREG);   /* Wine's test of the same shape (actctx classes without default progid) */
    CHECK(s == NULL);
}
TEST(dxprog_progidfromclsid_only_version_independent) {
    REQUIRE(U(ProgIDFromCLSID)); DX_BEGIN();
    GUID g; char ck[128], s2[160]; LPOLESTR s = NULL;
    dx_newguid(&g, DXK_NORMAL); dx_clsidkey(&g, ck, sizeof ck); dx_track(ck); dx_set(ck, NULL, "vi only");
    snprintf(s2, sizeof s2, "%s\\VersionIndependentProgID", ck); dx_set(s2, NULL, "olet.vionly");
    HRESULT hr = U(ProgIDFromCLSID)(&g, &s);
    t_note("class with only VersionIndependentProgID: hr=%08lx progid=%ls", hr, s ? s : L"(null)");
    if (s) U(CoTaskMemFree)(s);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_curver_chain) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; WCHAR w[100]; char base[80], v1[80], v2[80], s[200], gs[40], ck[128];
    dx_newguid(&g, DXK_NORMAL); dx_guidstr(&g, gs); dx_clsidkey(&g, ck, sizeof ck); dx_track(ck); dx_set(ck, NULL, "curver chain");
    dx_progid(base, sizeof base, "Doc"); dx_progid(v1, sizeof v1, "Doc.1"); dx_progid(v2, sizeof v2, "Doc.2");
    dx_track(base); dx_track(v1); dx_track(v2);
    snprintf(s, sizeof s, "%s\\CurVer", base); dx_set(s, NULL, v1);
    snprintf(s, sizeof s, "%s\\CurVer", v1); dx_set(s, NULL, v2);
    snprintf(s, sizeof s, "%s\\CurVer", v2); dx_set(s, NULL, base);   /* cycle: Doc -> .1 -> .2 -> Doc */
    snprintf(s, sizeof s, "%s\\CLSID", v2); dx_set(s, NULL, gs);
    /* the ProgID that carries the CLSID resolves directly */
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(v2, w, 100), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
    /* ProgIDs without a CLSID follow CurVer to the one that has it (Wine marks this todo_wine: Windows succeeds) */
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(base, w, 100), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(v1, w, 100), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_curver_loop_and_empty) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID out; WCHAR w[100]; char a[80], b[80], c[80], s[200];
    dx_progid(a, sizeof a, "Loop.3"); dx_progid(b, sizeof b, "Loop.4"); dx_progid(c, sizeof c, "Loop.5");
    dx_track(a); dx_track(b); dx_track(c);
    snprintf(s, sizeof s, "%s\\CurVer", a); dx_set(s, NULL, b);
    snprintf(s, sizeof s, "%s\\CurVer", b); dx_set(s, NULL, a);
    snprintf(s, sizeof s, "%s\\CurVer", c); dx_set(s, NULL, "");
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(a, w, 100), &out), CO_E_CLASSSTRING);
    CHECK(IsEqualGUID(&out, &NULLG));
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgID)(W(c, w, 100), &out), CO_E_CLASSSTRING);
    CHECK(IsEqualGUID(&out, &NULLG));
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromstring_progid_fallback) {
    REQUIRE(U(CLSIDFromString)); DX_BEGIN();
    GUID g, out; char p[80], gs[40]; WCHAR w[80];
    dx_progid(p, sizeof p, "FromString.1"); mk(&g, p, NULL, 0);
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromString)(W(p, w, 80), &out), S_OK);   /* not a {guid}: treated as a ProgID */
    CHECK(IsEqualGUID(&out, &g));
    dx_guidstr(&g, gs);
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromString)(W(gs, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
TEST(dxprog_clsidfromstring_unknown_progid) {
    REQUIRE(U(CLSIDFromString)); DX_BEGIN();
    GUID out; WCHAR w[80]; char p[80]; dx_progid(p, sizeof p, "Unknown.9");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CLSIDFromString)(W(p, w, 80), &out);
    EQ(hr, CO_E_CLASSSTRING);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxprog_clsidfromprogidex_registered) {
    REQUIRE(U(CLSIDFromProgIDEx)); DX_BEGIN();
    GUID g, out; char p[80]; WCHAR w[80]; dx_progid(p, sizeof p, "Ex.1"); mk(&g, p, NULL, 0);
    memset(&out, 0xcc, sizeof out);
    EQ(U(CLSIDFromProgIDEx)(W(p, w, 80), &out), S_OK);
    CHECK(IsEqualGUID(&out, &g));
}
TEST(dxprog_clsidfromprogidex_failures) {
    REQUIRE(U(CLSIDFromProgIDEx)); DX_BEGIN();
    GUID out; char p[80]; WCHAR w[80]; dx_progid(p, sizeof p, "ExNone.1");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CLSIDFromProgIDEx)(W(p, w, 80), &out);
    CHECK(FAILED(hr)); t_note("CLSIDFromProgIDEx unknown progid: hr=%08lx (CLSIDFromProgID gives CO_E_CLASSSTRING; Ex may try download)", hr);
    EQ(U(CLSIDFromProgIDEx)(NULL, &out), E_INVALIDARG);
    EQ(U(CLSIDFromProgIDEx)(L"x", NULL), E_INVALIDARG);
}
TEST(dxprog_clsidfromprogid_forwarder_modules) {
    /* both exports resolve to the same behaviour regardless of module */
    DX_BEGIN();
    GUID g, a, b; char p[80]; WCHAR w[80]; dx_progid(p, sizeof p, "Fwd.1"); mk(&g, p, NULL, 0);
    HRESULT (WINAPI *f1)(LPCOLESTR, LPCLSID) = (void *)t_proc_in("ole32.dll", "CLSIDFromProgID");
    HRESULT (WINAPI *f2)(LPCOLESTR, LPCLSID) = (void *)t_proc_in("combase.dll", "CLSIDFromProgID");
    if (!f1 || !f2) { t_skip("missing export"); return; }
    EQ(f1(W(p, w, 80), &a), S_OK); EQ(f2(W(p, w, 80), &b), S_OK);
    CHECK(IsEqualGUID(&a, &g) && IsEqualGUID(&b, &g));
    EQ(f1(NULL, &a), E_INVALIDARG); EQ(f2(NULL, &b), E_INVALIDARG);
}
TEST(dxprog_progid_removed_after_delete) {
    REQUIRE(U(CLSIDFromProgID)); DX_BEGIN();
    GUID g, out; char p[80]; WCHAR w[80]; dx_progid(p, sizeof p, "Del.1"); mk(&g, p, NULL, 0);
    EQ(U(CLSIDFromProgID)(W(p, w, 80), &out), S_OK);
    dx_del(p);
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CLSIDFromProgID)(W(p, w, 80), &out);
    CHECK(FAILED(hr)); t_note("after deleting the ProgID key: hr=%08lx", hr);
}
TEST(dxprog_cleanup_removes_tracked) {
    DX_BEGIN();
    GUID g; char p[80], ck[128]; dx_progid(p, sizeof p, "Clean.1"); mk(&g, p, NULL, 0);
    dx_clsidkey(&g, ck, sizeof ck);
    CHECK(dx_exists(p)); CHECK(dx_exists(ck));
    dx_cleanup();
    CHECK(!dx_exists(p)); CHECK(!dx_exists(ck));
}
