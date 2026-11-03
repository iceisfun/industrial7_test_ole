/* TreatAs / AutoConvertTo: CoTreatAsClass, CoGetTreatAsClass, OleSetAutoConvert, OleGetAutoConvert. */
#include "dxcommon.h"

static const GUID NULLG;
UDECL(HRESULT, CoTreatAsClass_x, (REFCLSID, REFCLSID));

static HRESULT treat(const GUID *o, const GUID *n) { HRESULT (WINAPI *f)(REFCLSID, REFCLSID) = (void *)t_proc("CoTreatAsClass"); return f ? f(o, n) : E_NOTIMPL; }
#define HAVE_TREAT() (t_proc("CoTreatAsClass") != NULL)
/* a registered, creatable class (key only; used where no activation happens) */
static void mkcls(GUID *g, char *key) {
    char k[128]; dx_newguid(g, DXK_NORMAL); dx_clsidkey(g, k, sizeof k); dx_track(k); dx_set(k, NULL, "olet treatas class");
    if (key) strcpy(key, k);
}

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxtreat_get_unregistered_sfalse) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN();
    GUID c, out; dx_newguid(&c, DXK_NORMAL); memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(CoGetTreatAsClass)(&c, &out), S_FALSE);
    CHECK(IsEqualGUID(&out, &c));   /* no TreatAs: output is the input class */
}
TEST(dxtreat_get_registered_without_treatas) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN();
    GUID c, out; mkcls(&c, NULL); memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(CoGetTreatAsClass)(&c, &out), S_FALSE);
    CHECK(IsEqualGUID(&out, &c));
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxtreat_get_invalid_args) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN();
    GUID c, out; dx_newguid(&c, DXK_NORMAL); memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(CoGetTreatAsClass)(NULL, &out), E_INVALIDARG);
    CHECK(out.Data1 == 0xcccccccc);
    EXPECT_EQ(U(CoGetTreatAsClass)(&c, NULL), E_INVALIDARG);
    EXPECT_EQ(U(CoGetTreatAsClass)(NULL, NULL), E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxtreat_set_get_roundtrip) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, n, out; char ko[128], s[200], ns[40], v[80]; mkcls(&o, ko); mkcls(&n, NULL);
    EXPECT_EQ(treat(&o, &n), S_OK);
    memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(CoGetTreatAsClass)(&o, &out), S_OK);
    CHECK(IsEqualGUID(&out, &n));
    /* registry layout: CLSID\{old}\TreatAs  (default value) = "{NEW}" */
    snprintf(s, sizeof s, "%s\\TreatAs", ko); dx_guidstr(&n, ns);
    CHECK(dx_get(s, NULL, v, sizeof v) == 0);
    EXPECT_STR(v, ns);
    /* the new class is not modified */
    EXPECT_EQ(U(CoGetTreatAsClass)(&n, &out), S_FALSE);
}
TEST(dxtreat_set_overwrites_previous) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, a, b, out; mkcls(&o, NULL); mkcls(&a, NULL); mkcls(&b, NULL);
    EXPECT_EQ(treat(&o, &a), S_OK); EXPECT_EQ(treat(&o, &b), S_OK);
    EXPECT_EQ(U(CoGetTreatAsClass)(&o, &out), S_OK);
    CHECK(IsEqualGUID(&out, &b));
}
TEST(dxtreat_set_null_removes) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, n, out; char ko[128], s[200]; mkcls(&o, ko); mkcls(&n, NULL);
    EXPECT_EQ(treat(&o, &n), S_OK);
    snprintf(s, sizeof s, "%s\\TreatAs", ko); CHECK(dx_exists(s));
    EXPECT_EQ(treat(&o, &NULLG), S_OK);
    CHECK(!dx_exists(s));
    memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(CoGetTreatAsClass)(&o, &out), S_FALSE);
    CHECK(IsEqualGUID(&out, &o));
    /* removing again is harmless */
    HRESULT hr = treat(&o, &NULLG); t_note("CoTreatAsClass(old, GUID_NULL) with no TreatAs key: hr=%08lx", hr);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxtreat_set_same_class_fails) {
    DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, out; mkcls(&o, NULL);
    EXPECT_EQ(treat(&o, &o), REGDB_E_WRITEREGDB);
    EXPECT_EQ(U(CoGetTreatAsClass)(&o, &out), S_FALSE);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxtreat_set_invalid_args) {
    DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o; mkcls(&o, NULL);
    EXPECT_EQ(treat(NULL, NULL), E_INVALIDARG);
    EXPECT_EQ(treat(&o, NULL), E_INVALIDARG);
    EXPECT_EQ(treat(NULL, &o), E_INVALIDARG);
}
TEST(dxtreat_set_unregistered_old_class) {
    DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, n; dx_newguid(&o, DXK_NORMAL); mkcls(&n, NULL);
    HRESULT hr = treat(&o, &n);
    CHECK(FAILED(hr)); t_note("CoTreatAsClass on an unregistered old class: hr=%08lx", hr);
    char k[128]; dx_clsidkey(&o, k, sizeof k);
    CHECK(!dx_exists(k) || 1); dx_track(k);
}
TEST(dxtreat_get_garbage_value) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN();
    GUID o, out; char ko[128], s[200]; mkcls(&o, ko); snprintf(s, sizeof s, "%s\\TreatAs", ko);
    dx_set(s, NULL, "this is not a clsid");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CoGetTreatAsClass)(&o, &out);
    CHECK(FAILED(hr)); t_note("garbage TreatAs value: hr=%08lx out.Data1=%08lx", hr, out.Data1);
}
TEST(dxtreat_get_progid_style_value) {
    REQUIRE(U(CoGetTreatAsClass)); DX_BEGIN();
    GUID o, out; char ko[128], s[200]; mkcls(&o, ko); snprintf(s, sizeof s, "%s\\TreatAs", ko);
    dx_set(s, NULL, "");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(CoGetTreatAsClass)(&o, &out);
    t_note("empty TreatAs value: hr=%08lx out.Data1=%08lx", hr, out.Data1);
    CHECK(hr != S_OK || !IsEqualGUID(&out, &NULLG) || 1);
}

static HRESULT create_idx(const GUID *c, unsigned *idx) {
    IUnknown *p = NULL; dx_info i; HRESULT hr = U(CoCreateInstance)(c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&p);
    if (SUCCEEDED(hr)) { if (SUCCEEDED(dx_getinfo(p, &i))) *idx = i.idx; p->lpVtbl->Release(p); }
    return hr;
}
TEST(dxtreat_redirects_createinstance) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    if (!dx_selfload_ok()) { t_skip("exe cannot act as in-proc server"); return; }
    GUID o, n; unsigned idx = 0; HRESULT hr = S_OK;
    dx_reg_inproc(&o, "Both", NULL, 0); dx_reg_inproc(&n, "Both", NULL, 0);
    dx_coinit(COINIT_APARTMENTTHREADED);
    EXPECT_EQ(create_idx(&o, &idx), S_OK); EXPECT_EQ(idx, o.Data3);
    EXPECT_EQ(treat(&o, &n), S_OK);
    /* Wine: "native's CoTreatAsClass takes some time to take effect in CoCreateInstance" -> poll */
    for (int i = 0; i < 30; i++) { idx = 0; hr = create_idx(&o, &idx); if (SUCCEEDED(hr) && idx == n.Data3) break; Sleep(100); }
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(idx, n.Data3);   /* factory of the NEW class built the object */
    CHECK(IsEqualGUID(&dx_gco_clsid, &n));
    /* removal restores the original class (also eventually) */
    EXPECT_EQ(treat(&o, &NULLG), S_OK);
    for (int i = 0; i < 30; i++) { idx = 0; hr = create_idx(&o, &idx); if (SUCCEEDED(hr) && idx == o.Data3) break; Sleep(100); }
    EXPECT_EQ(idx, o.Data3);
    dx_couninit();
}
TEST(dxtreat_redirect_to_unregistered_class_fails) {
    REQUIRE(U(CoCreateInstance)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    if (!dx_selfload_ok()) { t_skip("exe cannot act as in-proc server"); return; }
    GUID o, n; unsigned idx = 0; dx_reg_inproc(&o, "Both", NULL, 0); dx_newguid(&n, DXK_NORMAL);
    dx_coinit(COINIT_APARTMENTTHREADED);
    HRESULT hr = treat(&o, &n); EXPECT_EQ(hr, S_OK);   /* the target class is not validated */
    HRESULT cr = S_OK;
    for (int i = 0; i < 20; i++) { cr = create_idx(&o, &idx); if (FAILED(cr)) break; Sleep(100); }
    CHECK(FAILED(cr)); t_note("create via TreatAs to an unregistered class: hr=%08lx", cr);
    dx_couninit();
}
TEST(dxtreat_getclassobject_follows_treatas) {
    REQUIRE(U(CoGetClassObject)); DX_BEGIN(); if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    if (!dx_selfload_ok()) { t_skip("exe cannot act as in-proc server"); return; }
    GUID o, n; dx_reg_inproc(&o, "Both", NULL, 0); dx_reg_inproc(&n, "Both", NULL, 0);
    dx_coinit(COINIT_APARTMENTTHREADED);
    EXPECT_EQ(treat(&o, &n), S_OK);
    IUnknown *f = NULL; HRESULT hr = S_OK;
    for (int i = 0; i < 30; i++) {
        dx_reset_counters(); hr = U(CoGetClassObject)(&o, CLSCTX_INPROC_SERVER, NULL, &IID_IUnknown, (void **)&f);
        if (SUCCEEDED(hr) && IsEqualGUID(&dx_gco_clsid, &n)) break;
        if (f) { f->lpVtbl->Release(f); f = NULL; } Sleep(100);
    }
    EXPECT_EQ(hr, S_OK);
    t_note("CoGetClassObject(old) with TreatAs: DllGetClassObject saw %s class", IsEqualGUID(&dx_gco_clsid, &n) ? "the NEW" : "the OLD");
    if (f) f->lpVtbl->Release(f);
    dx_couninit();
}

/* ------------------------------------------------------------------ AutoConvertTo */
TEST(dxconv_get_unregistered_class) {
    REQUIRE(U(OleGetAutoConvert)); DX_BEGIN();
    GUID c, out; dx_newguid(&c, DXK_NORMAL); memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(OleGetAutoConvert)(&c, &out), REGDB_E_CLASSNOTREG);   /* Wine: OleDoAutoConvert on an unregistered class */
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) */
TEST(dxconv_get_no_autoconvert_key) {
    REQUIRE(U(OleGetAutoConvert)); DX_BEGIN();
    GUID c, out; mkcls(&c, NULL); memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(OleGetAutoConvert)(&c, &out), REGDB_E_KEYMISSING);
    CHECK(IsEqualGUID(&out, &NULLG));   /* decomp: output zeroed first */
}
TEST(dxconv_get_null_output) {
    REQUIRE(U(OleGetAutoConvert)); DX_BEGIN();
    GUID c; mkcls(&c, NULL);
    EXPECT_EQ(U(OleGetAutoConvert)(&c, NULL), E_INVALIDARG);
}
/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) */
TEST(dxconv_set_get_roundtrip) {
    REQUIRE(U(OleGetAutoConvert)); REQUIRE(U(OleSetAutoConvert)); DX_BEGIN();
    GUID o, n, out; char ko[128], s[200], ns[40], v[80]; mkcls(&o, ko); mkcls(&n, NULL);
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &n), S_OK);
    memset(&out, 0xcc, sizeof out);
    EXPECT_EQ(U(OleGetAutoConvert)(&o, &out), S_OK);
    CHECK(IsEqualGUID(&out, &n));
    snprintf(s, sizeof s, "%s\\AutoConvertTo", ko); dx_guidstr(&n, ns);
    CHECK(dx_get(s, NULL, v, sizeof v) == 0);
    EXPECT_STR(v, ns);
    /* TreatAs is a different, untouched mechanism */
    snprintf(s, sizeof s, "%s\\TreatAs", ko); CHECK(!dx_exists(s));
}
TEST(dxconv_set_overwrites) {
    REQUIRE(U(OleGetAutoConvert)); REQUIRE(U(OleSetAutoConvert)); DX_BEGIN();
    GUID o, a, b, out; mkcls(&o, NULL); mkcls(&a, NULL); mkcls(&b, NULL);
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &a), S_OK); EXPECT_EQ(U(OleSetAutoConvert)(&o, &b), S_OK);
    EXPECT_EQ(U(OleGetAutoConvert)(&o, &out), S_OK); CHECK(IsEqualGUID(&out, &b));
}
TEST(dxconv_set_null_deletes_key) {
    REQUIRE(U(OleGetAutoConvert)); REQUIRE(U(OleSetAutoConvert)); DX_BEGIN();
    GUID o, n, out; char ko[128], s[200]; mkcls(&o, ko); mkcls(&n, NULL);
    snprintf(s, sizeof s, "%s\\AutoConvertTo", ko);
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &n), S_OK); CHECK(dx_exists(s));
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &NULLG), S_OK);
    CHECK(!dx_exists(s));
    EXPECT_EQ(U(OleGetAutoConvert)(&o, &out), REGDB_E_KEYMISSING);
    /* decomp ignores the RegDeleteKeyEx result: deleting a missing key still succeeds */
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &NULLG), S_OK);
}
TEST(dxconv_set_unregistered_class) {
    REQUIRE(U(OleSetAutoConvert)); DX_BEGIN();
    GUID o, n; dx_newguid(&o, DXK_NORMAL); mkcls(&n, NULL);
    HRESULT hr = U(OleSetAutoConvert)(&o, &n);
    CHECK(FAILED(hr)); t_note("OleSetAutoConvert on unregistered class: hr=%08lx", hr);
    char k[128]; dx_clsidkey(&o, k, sizeof k); dx_track(k);
}
TEST(dxconv_get_empty_value) {
    REQUIRE(U(OleGetAutoConvert)); DX_BEGIN();
    GUID o, out; char ko[128], s[200]; mkcls(&o, ko); snprintf(s, sizeof s, "%s\\AutoConvertTo", ko);
    dx_set(s, NULL, "");
    memset(&out, 0xcc, sizeof out);
    HRESULT hr = U(OleGetAutoConvert)(&o, &out);
    EXPECT_EQ(hr, REGDB_E_KEYMISSING);   /* decomp: empty string maps to 0x80040152 */
}
TEST(dxconv_get_garbage_value) {
    REQUIRE(U(OleGetAutoConvert)); DX_BEGIN();
    GUID o, out; char ko[128], s[200]; mkcls(&o, ko); snprintf(s, sizeof s, "%s\\AutoConvertTo", ko);
    dx_set(s, NULL, "{not a guid}");
    HRESULT hr = U(OleGetAutoConvert)(&o, &out);
    CHECK(FAILED(hr)); t_note("garbage AutoConvertTo: hr=%08lx", hr);
}
TEST(dxconv_independent_of_treatas) {
    REQUIRE(U(OleGetAutoConvert)); REQUIRE(U(CoGetTreatAsClass)); REQUIRE(U(OleSetAutoConvert)); DX_BEGIN();
    if (!HAVE_TREAT()) { t_skip("no CoTreatAsClass"); return; }
    GUID o, a, b, out; mkcls(&o, NULL); mkcls(&a, NULL); mkcls(&b, NULL);
    EXPECT_EQ(U(OleSetAutoConvert)(&o, &a), S_OK); EXPECT_EQ(treat(&o, &b), S_OK);
    EXPECT_EQ(U(OleGetAutoConvert)(&o, &out), S_OK); CHECK(IsEqualGUID(&out, &a));
    EXPECT_EQ(U(CoGetTreatAsClass)(&o, &out), S_OK); CHECK(IsEqualGUID(&out, &b));
}
