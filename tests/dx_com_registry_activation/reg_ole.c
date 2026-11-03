/* OLE registry readers: OleRegGetUserType, OleRegGetMiscStatus, OleRegEnumVerbs, OleRegEnumFormatEtc,
 * fed from classes this test registers under HKCU\Software\Classes\CLSID\{..}. */
#include "dxcommon.h"

static void mkcls(GUID *g, char *key, const char *deflt) {
    char k[128]; dx_newguid(g, DXK_NORMAL); dx_clsidkey(g, k, sizeof k); dx_track(k);
    if (deflt) dx_set(k, NULL, deflt); else dx_mkkey(k);
    if (key) strcpy(key, k);
}
static void setsub(const char *key, const char *sub, const char *name, const char *val) { char p[256]; snprintf(p, sizeof p, "%s\\%s", key, sub); dx_set(p, name, val); }

/* ---------------------------------------------------------------- OleRegGetUserType */
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxole_usertype_forms) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; char k[128], nm[40]; mkcls(&c, k, "Default Name");
    for (int i = 0; i <= 4; i++) { char v[16]; snprintf(nm, sizeof nm, "AuxUserType\\%d", i); snprintf(v, sizeof v, "Aux Name %d", i); setsub(k, nm, NULL, v); }
    static const struct { DWORD form; const WCHAR *want; } t[] = {
        { 0, L"Aux Name 0" }, { USERCLASSTYPE_FULL, L"Default Name" }, { USERCLASSTYPE_SHORT, L"Aux Name 2" },
        { USERCLASSTYPE_APPNAME, L"Aux Name 3" }, { 4, L"Aux Name 4" }, { 5, L"Default Name" }, { 6, L"Default Name" }, { 100, L"Default Name" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        LPOLESTR s = (LPOLESTR)(ULONG_PTR)1;
        HRESULT hr = U(OleRegGetUserType)(&c, t[i].form, &s);
        CHECKF(hr == S_OK, "form %lu: hr=%08lx", t[i].form, hr);
        if (hr == S_OK) { CHECKF(!lstrcmpW(s, t[i].want), "form %lu: got %ls want %ls", t[i].form, s, t[i].want); U(CoTaskMemFree)(s); }
    }
}
TEST(dxole_usertype_aux_missing_falls_back_to_default) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "Only Default");
    for (DWORD f = 0; f <= 4; f++) {
        LPOLESTR s = NULL; HRESULT hr = U(OleRegGetUserType)(&c, f, &s);
        CHECKF(hr == S_OK && s && !lstrcmpW(s, L"Only Default"), "form %lu hr=%08lx", f, hr);
        if (s) U(CoTaskMemFree)(s);
    }
}
TEST(dxole_usertype_partial_aux) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "Full Name"); setsub(k, "AuxUserType\\2", NULL, "Short Name");
    LPOLESTR s = NULL;
    EQ(U(OleRegGetUserType)(&c, USERCLASSTYPE_SHORT, &s), S_OK); CHECK(s && !lstrcmpW(s, L"Short Name")); if (s) U(CoTaskMemFree)(s);
    s = NULL; EQ(U(OleRegGetUserType)(&c, USERCLASSTYPE_APPNAME, &s), S_OK); CHECK(s && !lstrcmpW(s, L"Full Name")); if (s) U(CoTaskMemFree)(s);
}
TEST(dxole_usertype_empty_aux_value_falls_back) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "Full Name 2"); setsub(k, "AuxUserType\\2", NULL, "");
    LPOLESTR s = NULL;
    EQ(U(OleRegGetUserType)(&c, USERCLASSTYPE_SHORT, &s), S_OK);   /* decomp: empty aux string is discarded */
    CHECK(s && !lstrcmpW(s, L"Full Name 2")); if (s) U(CoTaskMemFree)(s);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxole_usertype_unregistered_and_invalid) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL);
    for (DWORD f = 0; f <= 4; f++) {
        LPOLESTR s = (LPOLESTR)(ULONG_PTR)0xdeadbeef;
        EQ(U(OleRegGetUserType)(&c, f, &s), REGDB_E_CLASSNOTREG); CHECK(s == NULL);
        EQ(U(OleRegGetUserType)(&c, f, NULL), E_INVALIDARG);
    }
}
TEST(dxole_usertype_empty_default_is_invalid_value) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, "");
    LPOLESTR s = (LPOLESTR)(ULONG_PTR)1;
    EQ(U(OleRegGetUserType)(&c, USERCLASSTYPE_FULL, &s), REGDB_E_INVALIDVALUE);   /* decomp: 0x80040153 */
    CHECK(s == NULL);
}
TEST(dxole_usertype_no_default_value) {
    REQUIRE(U(OleRegGetUserType)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, NULL);
    LPOLESTR s = (LPOLESTR)(ULONG_PTR)1;
    HRESULT hr = U(OleRegGetUserType)(&c, USERCLASSTYPE_FULL, &s);
    CHECK(FAILED(hr)); CHECK(s == NULL); t_note("class key without default value: hr=%08lx", hr);
}
TEST(dxole_usertype_system_class) {
    REQUIRE(U(OleRegGetUserType));
    static const GUID StdFont = {0x0be35203,0x8f91,0x11ce,{0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51}};
    for (DWORD f = 0; f <= 4; f++) {
        LPOLESTR s = NULL; EQ(U(OleRegGetUserType)(&StdFont, f, &s), S_OK);
        CHECK(s && *s); if (s) U(CoTaskMemFree)(s);   /* Wine: same string for all forms ("Standard Font") */
    }
}

/* ---------------------------------------------------------------- OleRegGetMiscStatus */
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(dxole_miscstatus_unregistered_and_invalid) {
    REQUIRE(U(OleRegGetMiscStatus)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); DWORD st = 0xdeadbeef;
    EQ(U(OleRegGetMiscStatus)(&c, DVASPECT_ICON, NULL), E_INVALIDARG);
    EQ(U(OleRegGetMiscStatus)(&c, DVASPECT_ICON, &st), REGDB_E_CLASSNOTREG);
    EQ(st, 0);   /* output cleared before the lookup */
}
TEST(dxole_miscstatus_no_key_gives_zero) {
    REQUIRE(U(OleRegGetMiscStatus)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, "no misc status"); DWORD st = 0xdeadbeef;
    EQ(U(OleRegGetMiscStatus)(&c, DVASPECT_CONTENT, &st), S_OK); EQ(st, 0);
}
TEST(dxole_miscstatus_default_and_aspects) {
    REQUIRE(U(OleRegGetMiscStatus)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "misc"); setsub(k, "MiscStatus", NULL, "131473"); setsub(k, "MiscStatus\\1", NULL, "132");   /* aspect 1 = DVASPECT_CONTENT */
    setsub(k, "MiscStatus\\4", NULL, "7");   /* key name = decimal aspect value: 4 = DVASPECT_ICON */
    static const struct { DWORD aspect; DWORD want; } t[] = {
        { DVASPECT_CONTENT, 132 }, { DVASPECT_THUMBNAIL, 131473 }, { DVASPECT_ICON, 7 }, { DVASPECT_DOCPRINT, 131473 }, { 0, 131473 }, { 99, 131473 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        DWORD st = 0xdeadbeef; HRESULT hr = U(OleRegGetMiscStatus)(&c, t[i].aspect, &st);
        CHECKF(hr == S_OK && st == t[i].want, "aspect %lu: hr=%08lx status=%lu want %lu", t[i].aspect, hr, st, t[i].want);
    }
}
TEST(dxole_miscstatus_aspect_without_default) {
    REQUIRE(U(OleRegGetMiscStatus)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "misc2"); setsub(k, "MiscStatus\\1", NULL, "5");
    DWORD st = 0xdeadbeef;
    EQ(U(OleRegGetMiscStatus)(&c, DVASPECT_CONTENT, &st), S_OK); EQ(st, 5);
    st = 0xdeadbeef; HRESULT hr = U(OleRegGetMiscStatus)(&c, DVASPECT_ICON, &st);
    CHECK(hr == S_OK); EQ(st, 0);   /* nothing for the aspect and no default: zero */
}
TEST(dxole_miscstatus_bad_value_text) {
    REQUIRE(U(OleRegGetMiscStatus)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "misc3"); setsub(k, "MiscStatus", NULL, "not a number");
    DWORD st = 0xdeadbeef; HRESULT hr = U(OleRegGetMiscStatus)(&c, DVASPECT_CONTENT, &st);
    t_note("non-numeric MiscStatus: hr=%08lx status=%lu", hr, st);
}
TEST(dxole_miscstatus_system_class_stdfont) {
    REQUIRE(U(OleRegGetMiscStatus));
    static const GUID StdFont = {0x0be35203,0x8f91,0x11ce,{0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51}};
    DWORD st = (DWORD)-1;
    EQ(U(OleRegGetMiscStatus)(&StdFont, DVASPECT_ICON, &st), S_OK); EQ(st, 0);   /* Wine's test of the same call */
}

/* ---------------------------------------------------------------- OleRegEnumVerbs */
TEST(dxole_enumverbs_invalid_and_unregistered) {
    REQUIRE(U(OleRegEnumVerbs)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); IEnumOLEVERB *e = (IEnumOLEVERB *)(ULONG_PTR)1;
    EQ(U(OleRegEnumVerbs)(&c, NULL), E_INVALIDARG);
    EQ(U(OleRegEnumVerbs)(&c, &e), REGDB_E_CLASSNOTREG); CHECK(e == NULL);
}
TEST(dxole_enumverbs_class_without_verb_key) {
    REQUIRE(U(OleRegEnumVerbs)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, "no verbs"); IEnumOLEVERB *e = (IEnumOLEVERB *)(ULONG_PTR)1;
    HRESULT hr = U(OleRegEnumVerbs)(&c, &e);
    EQ(hr, REGDB_E_CLASSNOTREG);   /* decomp: failure to open CLSID\{..}\Verb maps to REGDB_E_CLASSNOTREG */
    CHECK(e == NULL);
}
TEST(dxole_enumverbs_empty_verb_key) {
    REQUIRE(U(OleRegEnumVerbs)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "empty verbs"); char p[160]; snprintf(p, sizeof p, "%s\\Verb", k); dx_mkkey(p);
    IEnumOLEVERB *e = (IEnumOLEVERB *)(ULONG_PTR)1;
    EQ(U(OleRegEnumVerbs)(&c, &e), OLEOBJ_E_NOVERBS); CHECK(e == NULL);
}
TEST(dxole_enumverbs_two_verbs) {
    REQUIRE(U(OleRegEnumVerbs)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "verbs"); setsub(k, "Verb\\0", NULL, "&Edit,0,2"); setsub(k, "Verb\\1", NULL, "&Open,0,2");
    IEnumOLEVERB *e = NULL;
    EQ(U(OleRegEnumVerbs)(&c, &e), S_OK); CHECK(e != NULL);
    if (!e) return;
    OLEVERB v[4]; ULONG got = 99; memset(v, 0, sizeof v);
    HRESULT hr = e->lpVtbl->Next(e, 4, v, &got);
    EQ(hr, S_FALSE); EQ(got, 2);   /* fewer than requested */
    if (got == 2) {
        EQ(v[0].lVerb, 0); EQ(v[1].lVerb, 1);
        t_note("verb 0: name=%ls fuFlags=%lu grfAttribs=%lu; verb 1: name=%ls fuFlags=%lu grfAttribs=%lu", v[0].lpszVerbName, v[0].fuFlags, v[0].grfAttribs, v[1].lpszVerbName, v[1].fuFlags, v[1].grfAttribs);
        CHECK(v[0].lpszVerbName && wcsstr(v[0].lpszVerbName, L"Edit")); CHECK(v[1].lpszVerbName && wcsstr(v[1].lpszVerbName, L"Open"));
        EQ(v[0].grfAttribs, 2); EQ(v[1].grfAttribs, 2);   /* third field of the registry string */
        for (int i = 0; i < 2; i++) U(CoTaskMemFree)(v[i].lpszVerbName);
    }
    /* exhausted */
    got = 99; hr = e->lpVtbl->Next(e, 1, v, &got); EQ(hr, S_FALSE); EQ(got, 0);
    /* Reset, Skip, Clone */
    EQ(e->lpVtbl->Reset(e), S_OK);
    EQ(e->lpVtbl->Skip(e, 1), S_OK);
    got = 99; hr = e->lpVtbl->Next(e, 1, v, &got); EQ(hr, S_OK); EQ(got, 1);
    if (got == 1) { EQ(v[0].lVerb, 1); U(CoTaskMemFree)(v[0].lpszVerbName); }
    EQ(e->lpVtbl->Reset(e), S_OK);
    IEnumOLEVERB *e2 = NULL; EQ(e->lpVtbl->Clone(e, &e2), S_OK);
    if (e2) { got = 99; hr = e2->lpVtbl->Next(e2, 1, v, &got); EQ(got, 1); if (got == 1) U(CoTaskMemFree)(v[0].lpszVerbName); e2->lpVtbl->Release(e2); }
    e->lpVtbl->Release(e);
}
TEST(dxole_enumverbs_next_without_fetched_pointer) {
    REQUIRE(U(OleRegEnumVerbs)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "verbs2"); setsub(k, "Verb\\0", NULL, "&Play,0,2");
    IEnumOLEVERB *e = NULL; EQ(U(OleRegEnumVerbs)(&c, &e), S_OK);
    if (!e) return;
    OLEVERB v; memset(&v, 0, sizeof v);
    EQ(e->lpVtbl->Next(e, 1, &v, NULL), S_OK);   /* pceltFetched may be NULL when celt == 1 */
    EQ(v.lVerb, 0); U(CoTaskMemFree)(v.lpszVerbName);
    e->lpVtbl->Release(e);
}

/* ---------------------------------------------------------------- OleRegEnumFormatEtc */
TEST(dxole_enumformatetc_invalid_args) {
    REQUIRE(U(OleRegEnumFormatEtc)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, "fmt"); IEnumFORMATETC *e = (IEnumFORMATETC *)(ULONG_PTR)1;
    EQ(U(OleRegEnumFormatEtc)(&c, DATADIR_GET, NULL), E_INVALIDARG);
    static const DWORD bad[] = { 4, 5, 8, 0x10, 0x80000000u, 0xFFFFFFFCu, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        e = (IEnumFORMATETC *)(ULONG_PTR)1;
        HRESULT hr = U(OleRegEnumFormatEtc)(&c, bad[i], &e);
        CHECKF(hr == E_INVALIDARG && e == NULL, "dwDirection 0x%lx: hr=%08lx e=%p", bad[i], hr, e);
    }
}
TEST(dxole_enumformatetc_unregistered_class) {
    REQUIRE(U(OleRegEnumFormatEtc)); DX_BEGIN();
    GUID c; dx_newguid(&c, DXK_NORMAL); IEnumFORMATETC *e = (IEnumFORMATETC *)(ULONG_PTR)1;
    HRESULT hr = U(OleRegEnumFormatEtc)(&c, DATADIR_GET, &e);
    CHECK(FAILED(hr)); t_note("unregistered class: hr=%08lx (docs: REGDB_E_CLASSNOTREG)", hr);
    if (hr == S_OK && e) e->lpVtbl->Release(e);
}
TEST(dxole_enumformatetc_registered_formats) {
    REQUIRE(U(OleRegEnumFormatEtc)); DX_BEGIN();
    GUID c; char k[128]; mkcls(&c, k, "fmt2");
    setsub(k, "DataFormats\\GetSet\\0", NULL, "1,1,1,1");      /* CF_TEXT, content, TYMED_HGLOBAL, DATADIR_GET */
    setsub(k, "DataFormats\\GetSet\\1", NULL, "2,1,16,2");     /* CF_BITMAP, content, TYMED_GDI, DATADIR_SET */
    setsub(k, "DataFormats\\GetSet\\2", NULL, "13,1,1,3");     /* CF_UNICODETEXT, content, TYMED_HGLOBAL, get+set */
    static const DWORD dirs[] = { DATADIR_GET, DATADIR_SET, 0, 3 };
    for (unsigned d = 0; d < 4; d++) {
        IEnumFORMATETC *e = NULL; HRESULT hr = U(OleRegEnumFormatEtc)(&c, dirs[d], &e);
        t_note("dwDirection %lu: hr=%08lx", dirs[d], hr);
        CHECK(hr == S_OK || FAILED(hr));
        if (hr == S_OK && e) {
            FORMATETC f[8]; ULONG got = 0; memset(f, 0, sizeof f);
            HRESULT h2 = e->lpVtbl->Next(e, 8, f, &got);
            t_note("  Next -> hr=%08lx fetched=%lu", h2, got);
            for (ULONG i = 0; i < got; i++) t_note("  [%lu] cf=%u aspect=%lu tymed=%lu lindex=%ld ptd=%p", i, f[i].cfFormat, f[i].dwAspect, f[i].tymed, f[i].lindex, (void *)f[i].ptd);
            e->lpVtbl->Release(e);
        }
    }
}
TEST(dxole_enumformatetc_class_without_getset_key) {
    REQUIRE(U(OleRegEnumFormatEtc)); DX_BEGIN();
    GUID c; mkcls(&c, NULL, "fmt3"); IEnumFORMATETC *e = (IEnumFORMATETC *)(ULONG_PTR)1;
    HRESULT hr = U(OleRegEnumFormatEtc)(&c, DATADIR_GET, &e);
    CHECK(FAILED(hr)); t_note("class without DataFormats\\GetSet: hr=%08lx (docs: OLE_E_REGDB_KEY)", hr);
    if (hr == S_OK && e) e->lpVtbl->Release(e);
}
