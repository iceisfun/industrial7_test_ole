/* ITypeLib / ITypeLib2 on the system stdole2.tlb (read-only walk). */
#include "tl.h"

#define SENT_TI ((ITypeInfo *)(ULONG_PTR)0x1badf00d)

static const struct { const WCHAR *name; TYPEKIND kind; } known_types[] = {
    { L"GUID", TKIND_RECORD },
    { L"IUnknown", TKIND_INTERFACE },
    { L"IDispatch", TKIND_INTERFACE },
    { L"IEnumVARIANT", TKIND_INTERFACE },
    { L"OLE_COLOR", TKIND_ALIAS },
    { L"OLE_HANDLE", TKIND_ALIAS },
    { L"OLE_XPOS_PIXELS", TKIND_ALIAS },
    { L"OLE_YPOS_PIXELS", TKIND_ALIAS },
    { L"OLE_XSIZE_PIXELS", TKIND_ALIAS },
    { L"OLE_YSIZE_PIXELS", TKIND_ALIAS },
    { L"OLE_XPOS_HIMETRIC", TKIND_ALIAS },
    { L"OLE_YPOS_HIMETRIC", TKIND_ALIAS },
    { L"OLE_XSIZE_HIMETRIC", TKIND_ALIAS },
    { L"OLE_YSIZE_HIMETRIC", TKIND_ALIAS },
    { L"OLE_XPOS_CONTAINER", TKIND_ALIAS },
    { L"OLE_YPOS_CONTAINER", TKIND_ALIAS },
    { L"OLE_XSIZE_CONTAINER", TKIND_ALIAS },
    { L"OLE_YSIZE_CONTAINER", TKIND_ALIAS },
    { L"OLE_OPTEXCLUSIVE", TKIND_ALIAS },
    { L"OLE_CANCELBOOL", TKIND_ALIAS },
    { L"OLE_ENABLEDEFAULTBOOL", TKIND_ALIAS },
    { L"OLE_TRISTATE", TKIND_ENUM },
    { L"FONTNAME", TKIND_ALIAS },
    { L"FONTSIZE", TKIND_ALIAS },
    { L"FONTBOLD", TKIND_ALIAS },
    { L"FONTITALIC", TKIND_ALIAS },
    { L"FONTUNDERSCORE", TKIND_ALIAS },
    { L"FONTSTRIKETHROUGH", TKIND_ALIAS },
    { L"IFont", TKIND_INTERFACE },
    { L"Font", TKIND_DISPATCH },
    { L"StdFont", TKIND_COCLASS },
    { L"IPicture", TKIND_INTERFACE },
    { L"Picture", TKIND_DISPATCH },
    { L"StdPicture", TKIND_COCLASS },
    { L"LoadPictureConstants", TKIND_ENUM },
    { L"StdFunctions", TKIND_MODULE },
};
#define NKNOWN (sizeof known_types / sizeof known_types[0])

TEST(tlli_lib_attr_stdole2)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    TLIBATTR *a = NULL;
    TEQ(ITypeLib_GetLibAttr(tl, &a), S_OK);
    REQUIRE(a);
    CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle));
    TEQ(a->wMajorVerNum, 2);
    WCHECK(a->wMinorVerNum == 0);
    TEQ(a->lcid, 0);
    CHECK(a->syskind == SYS_WIN32 || a->syskind == SYS_WIN64);
    t_note("stdole2.tlb: syskind=%d minor=%u flags=0x%x", a->syskind, a->wMinorVerNum, a->wLibFlags);
    CHECK((a->wLibFlags & ~(LIBFLAG_FRESTRICTED | LIBFLAG_FCONTROL | LIBFLAG_FHIDDEN | LIBFLAG_FHASDISKIMAGE)) == 0);
    ITypeLib_ReleaseTLibAttr(tl, a);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_attr_stable_and_independent_of_release)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    TLIBATTR *a = NULL, *b = NULL;
    TEQ(ITypeLib_GetLibAttr(tl, &a), S_OK);
    TEQ(ITypeLib_GetLibAttr(tl, &b), S_OK);
    REQUIRE(a && b);
    CHECK(tl_guid_eq(&a->guid, &b->guid) && a->lcid == b->lcid && a->syskind == b->syskind &&   /* not memcmp: trailing padding */
          a->wMajorVerNum == b->wMajorVerNum && a->wMinorVerNum == b->wMinorVerNum && a->wLibFlags == b->wLibFlags);
    ITypeLib_ReleaseTLibAttr(tl, a);
    CHECK(tl_guid_eq(&b->guid, &TL_LIBID_StdOle));      /* b stays valid after releasing a */
    ITypeLib_ReleaseTLibAttr(tl, b);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_attr_null_out)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    TEQ(ITypeLib_GetLibAttr(tl, NULL), E_INVALIDARG);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_attr_no_refcount_effect)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ULONG r1 = ITypeLib_AddRef(tl); ITypeLib_Release(tl);
    TLIBATTR *a = NULL;
    TEQ(ITypeLib_GetLibAttr(tl, &a), S_OK);
    ULONG r2 = ITypeLib_AddRef(tl); ITypeLib_Release(tl);
    TEQ(r1, r2);
    if (a) ITypeLib_ReleaseTLibAttr(tl, a);
    ULONG r3 = ITypeLib_AddRef(tl); ITypeLib_Release(tl);
    TEQ(r1, r3);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_refcount_protocol)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ULONG a = ITypeLib_AddRef(tl);
    ULONG b = ITypeLib_AddRef(tl);
    TEQ(b, a + 1);
    TEQ(ITypeLib_Release(tl), a);
    TEQ(ITypeLib_Release(tl), a - 1);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_queryinterface)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const GUID *iid; int ok; const char *n; } t[] = {
        { &TL_IID_IUnknown, 1, "IUnknown" },
        { &TL_IID_ITypeLib, 1, "ITypeLib" },
        { &TL_IID_ITypeLib2, 1, "ITypeLib2" },
        { &TL_IID_ITypeComp, 0, "ITypeComp" },
        { &TL_IID_IDispatch, 0, "IDispatch" },
        { &TL_IID_ITypeInfo, 0, "ITypeInfo" },
        { &TL_IID_ITypeInfo2, 0, "ITypeInfo2" },
        { &TL_IID_IFont, 0, "IFont" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        void *p = (void *)(ULONG_PTR)0x1badf00d;
        HRESULT hr = ITypeLib_QueryInterface(tl, t[i].iid, &p);
        if (t[i].ok) {
            CHECKF(hr == S_OK && p, "%s hr=0x%08lx", t[i].n, (unsigned long)hr);
            if (hr == S_OK && p) IUnknown_Release((IUnknown *)p);
        } else if (i == 3) {
            /* ITypeComp is exposed by the same object on Windows (CTypeLib2 has an ITypeComp vtbl) */
            t_note("ITypeLib QI ITypeComp -> 0x%08lx", (unsigned long)hr);
            if (hr == S_OK && p) IUnknown_Release((IUnknown *)p);
        } else {
            CHECKF(hr == E_NOINTERFACE, "%s hr=0x%08lx", t[i].n, (unsigned long)hr);
            CHECKF(p == NULL, "%s out not cleared", t[i].n);
        }
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_identity_through_qi)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    IUnknown *u1 = NULL, *u2 = NULL;
    ITypeLib2 *t2 = NULL;
    TEQ(ITypeLib_QueryInterface(tl, &TL_IID_IUnknown, (void **)&u1), S_OK);
    TEQ(ITypeLib_QueryInterface(tl, &TL_IID_ITypeLib2, (void **)&t2), S_OK);
    REQUIRE(u1 && t2);
    TEQ(ITypeLib2_QueryInterface(t2, &TL_IID_IUnknown, (void **)&u2), S_OK);
    CHECK(u1 == u2);
    ITypeLib *back = NULL;
    TEQ(ITypeLib2_QueryInterface(t2, &TL_IID_ITypeLib, (void **)&back), S_OK);
    CHECK(back == tl);
    if (back) ITypeLib_Release(back);
    if (u2) IUnknown_Release(u2);
    IUnknown_Release(u1);
    ITypeLib2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_count_and_iterate)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    CHECK(n >= NKNOWN - 1);
    t_note("stdole2.tlb has %u type infos", n);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEKIND k1 = (TYPEKIND)99, k2 = (TYPEKIND)98;
        TEQ(ITypeLib_GetTypeInfo(tl, i, &ti), S_OK);
        TEQ(ITypeLib_GetTypeInfoType(tl, i, &k1), S_OK);
        CHECK(ti != NULL);
        if (ti) {
            TYPEATTR *ta = NULL;
            if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
                k2 = ta->typekind;
                CHECKF(k1 == k2, "index %u: lib kind %d attr kind %d", i, k1, k2);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            } else CHECK(0);
            ITypeInfo_Release(ti);
        }
        CHECK(k1 >= TKIND_ENUM && k1 < TKIND_MAX);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_out_of_range)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    UINT bad[] = { n, n + 1, 1000, 0x7fffffff, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ITypeInfo *ti = SENT_TI; TYPEKIND k = (TYPEKIND)77;
        HRESULT hr = ITypeLib_GetTypeInfo(tl, bad[i], &ti);
        CHECKF(hr == TYPE_E_ELEMENTNOTFOUND, "GetTypeInfo(%u) -> 0x%08lx", bad[i], (unsigned long)hr);
        WCHECK(ti == NULL);                 /* decomp: *out = NULL before the index lookup */
        hr = ITypeLib_GetTypeInfoType(tl, bad[i], &k);
        CHECKF(hr == TYPE_E_ELEMENTNOTFOUND, "GetTypeInfoType(%u) -> 0x%08lx", bad[i], (unsigned long)hr);
        CHECK(k == (TYPEKIND)77);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_null_out)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    if (t_is_wine()) { t_skip("wine dereferences NULL out params"); ITypeLib_Release(tl); return; }
    TEQ(ITypeLib_GetTypeInfo(tl, 0, NULL), E_INVALIDARG);
    TEQ(ITypeLib_GetTypeInfoType(tl, 0, NULL), E_INVALIDARG);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    TEQ(ITypeLib_GetTypeInfo(tl, n, NULL), E_INVALIDARG);       /* argument check precedes index check */
    TEQ(ITypeLib_GetTypeInfoType(tl, n, NULL), E_INVALIDARG);
    TEQ(ITypeLib_GetTypeInfoOfGuid(tl, &TL_IID_IUnknown, NULL), E_INVALIDARG);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_same_object_each_time)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *a = NULL, *b = NULL;
    TEQ(ITypeLib_GetTypeInfo(tl, 0, &a), S_OK);
    TEQ(ITypeLib_GetTypeInfo(tl, 0, &b), S_OK);
    REQUIRE(a && b);
    CHECK(a == b);                           /* CTypeInfo2 objects are cached per typelib */
    ITypeInfo_Release(a); ITypeInfo_Release(b);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_known_types_by_name)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    for (unsigned i = 0; i < NKNOWN; i++) {
        int idx = tl_find_index(tl, known_types[i].name);
        CHECKF(idx >= 0, "type %ls not found", known_types[i].name);
        if (idx < 0) continue;
        TYPEKIND k = (TYPEKIND)99;
        TEQ(ITypeLib_GetTypeInfoType(tl, (UINT)idx, &k), S_OK);
        CHECKF(k == known_types[i].kind, "%ls: kind %d want %d", known_types[i].name, k, known_types[i].kind);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_names_unique_and_nonempty)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    BSTR *names = (BSTR *)calloc(n ? n : 1, sizeof(BSTR));
    UINT (WINAPI *sl)(BSTR) = (void *)t_proc_in("oleaut32.dll", "SysStringLen");
    for (UINT i = 0; i < n; i++) {
        TEQ(ITypeLib_GetDocumentation(tl, (INT)i, &names[i], NULL, NULL, NULL), S_OK);
        CHECKF(names[i] && names[i][0], "type %u has no name", i);
        if (names[i] && sl) CHECK(sl(names[i]) == (UINT)lstrlenW(names[i]));
    }
    for (UINT i = 0; i < n; i++)
        for (UINT j = i + 1; j < n; j++)
            if (names[i] && names[j]) CHECKF(lstrcmpiW(names[i], names[j]) != 0, "duplicate type name %ls", names[i]);
    for (UINT i = 0; i < n; i++) tl_freebstr(names[i]);
    free(names);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_of_guid_known)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const GUID *g; TYPEKIND k; const char *n; } t[] = {
        { &TL_IID_IUnknown, TKIND_INTERFACE, "IUnknown" },
        { &TL_IID_IDispatch, TKIND_INTERFACE, "IDispatch" },
        { &TL_IID_IEnumVARIANT, TKIND_INTERFACE, "IEnumVARIANT" },
        { &TL_IID_IFont, TKIND_INTERFACE, "IFont" },
        { &TL_IID_IPicture, TKIND_INTERFACE, "IPicture" },
        { &TL_CLSID_StdFont, TKIND_COCLASS, "StdFont" },
        { &TL_CLSID_StdPicture, TKIND_COCLASS, "StdPicture" },
        { &TL_GUID_OLE_COLOR, TKIND_ALIAS, "OLE_COLOR" },
        { &TL_GUID_OLE_HANDLE, TKIND_ALIAS, "OLE_HANDLE" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        ITypeInfo *ti = SENT_TI;
        HRESULT hr = ITypeLib_GetTypeInfoOfGuid(tl, t[i].g, &ti);
        CHECKF(hr == S_OK, "%s hr=0x%08lx", t[i].n, (unsigned long)hr);
        if (hr != S_OK || !ti || ti == SENT_TI) continue;
        TYPEATTR *ta = NULL;
        TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
        if (ta) {
            CHECKF(ta->typekind == t[i].k, "%s kind %d", t[i].n, ta->typekind);
            CHECKF(tl_guid_eq(&ta->guid, t[i].g), "%s guid", t[i].n);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_of_guid_missing)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = SENT_TI;
    TEQ(ITypeLib_GetTypeInfoOfGuid(tl, &TL_GUID_NONE, &ti), TYPE_E_ELEMENTNOTFOUND);
    WCHECK(ti == NULL);
    ti = SENT_TI;
    TEQ(ITypeLib_GetTypeInfoOfGuid(tl, &TL_LIBID_StdOle, &ti), TYPE_E_ELEMENTNOTFOUND);   /* a LIBID is not a type */
    WCHECK(ti == NULL);
    GUID z = {0};
    ti = SENT_TI;
    { HRESULT hr = ITypeLib_GetTypeInfoOfGuid(tl, &z, &ti); t_note("GetTypeInfoOfGuid(GUID_NULL) -> 0x%08lx", (unsigned long)hr); if (SUCCEEDED(hr) && ti) ITypeInfo_Release(ti); }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_typeinfo_of_guid_roundtrip_all)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    UINT withguid = 0;
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL, *ti2 = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) { CHECK(0); continue; }
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            static const GUID zero = {0};
            if (!tl_guid_eq(&ta->guid, &zero)) {
                withguid++;
                HRESULT hr = ITypeLib_GetTypeInfoOfGuid(tl, &ta->guid, &ti2);
                CHECKF(hr == S_OK, "type %u: GetTypeInfoOfGuid hr=0x%08lx", i, (unsigned long)hr);
                if (ti2) {
                    TYPEATTR *tb = NULL;
                    if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti2, &tb))) {
                        CHECK(tl_guid_eq(&tb->guid, &ta->guid));
                        ITypeInfo_ReleaseTypeAttr(ti2, tb);
                    }
                    ITypeInfo_Release(ti2);
                }
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    CHECK(withguid >= 10);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_doc_library)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    BSTR name = NULL, doc = NULL, help = NULL; DWORD ctx = 0xdead;
    TEQ(ITypeLib_GetDocumentation(tl, -1, &name, &doc, &ctx, &help), S_OK);
    CHECK(name && tl_wieq(name, L"stdole"));
    CHECK(doc && tl_wieq(doc, L"OLE Automation"));
    TEQ(ctx, 0);
    t_note("stdole2 library helpfile = %ls", help ? help : L"(null)");
    tl_freebstr(name); tl_freebstr(doc); tl_freebstr(help);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_doc_null_pointers_allowed)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    TEQ(ITypeLib_GetDocumentation(tl, -1, NULL, NULL, NULL, NULL), S_OK);
    BSTR n = NULL;
    TEQ(ITypeLib_GetDocumentation(tl, -1, &n, NULL, NULL, NULL), S_OK);
    CHECK(n != NULL);
    tl_freebstr(n);
    DWORD c = 0xdead;
    TEQ(ITypeLib_GetDocumentation(tl, -1, NULL, NULL, &c, NULL), S_OK);
    TEQ(c, 0);
    TEQ(ITypeLib_GetDocumentation(tl, 0, NULL, NULL, NULL, NULL), S_OK);
    n = NULL;
    BSTR d = NULL;
    TEQ(ITypeLib_GetDocumentation(tl, 0, NULL, &d, NULL, NULL), S_OK);
    tl_freebstr(d);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_doc_bad_index)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    int bad[] = { (int)n, (int)n + 1, -2, -100, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BSTR nm = NULL;
        HRESULT hr = ITypeLib_GetDocumentation(tl, bad[i], &nm, NULL, NULL, NULL);
        if (!(t_is_wine() && bad[i] < 0)) CHECKF(hr == TYPE_E_ELEMENTNOTFOUND, "index %d hr=0x%08lx", bad[i], (unsigned long)hr);
        if (SUCCEEDED(hr)) tl_freebstr(nm); else CHECK(nm == NULL);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_doc_each_type_matches_typeinfo)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        BSTR n1 = NULL, d1 = NULL, h1 = NULL, n2 = NULL, d2 = NULL, h2 = NULL;
        DWORD c1 = 1, c2 = 2; ITypeInfo *ti = NULL;
        TEQ(ITypeLib_GetDocumentation(tl, (INT)i, &n1, &d1, &c1, &h1), S_OK);
        TEQ(ITypeLib_GetTypeInfo(tl, i, &ti), S_OK);
        if (ti) {
            TEQ(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &n2, &d2, &c2, &h2), S_OK);
            CHECK(n1 && n2 && !lstrcmpW(n1, n2));
            CHECK((d1 == NULL) == (d2 == NULL));
            if (d1 && d2) CHECK(!lstrcmpW(d1, d2));
            CHECK(c1 == c2);
            ITypeInfo_Release(ti);
        }
        tl_freebstr(n1); tl_freebstr(d1); tl_freebstr(h1); tl_freebstr(n2); tl_freebstr(d2); tl_freebstr(h2);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_doc_helpstrings_of_objects)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const struct { const WCHAR *type; const WCHAR *doc; } t[] = {
        { L"IFont", L"Font Object" }, { L"IPicture", L"Picture Object" },
    };
    for (unsigned i = 0; i < 2; i++) {
        int idx = tl_find_index(tl, t[i].type);
        if (idx < 0) { CHECK(0); continue; }
        BSTR d = NULL;
        TEQ(ITypeLib_GetDocumentation(tl, idx, NULL, &d, NULL, NULL), S_OK);
        if (d) { if (!tl_wieq(d, t[i].doc)) t_note("%ls helpstring = %ls", t[i].type, d); }
        else t_note("%ls has no helpstring", t[i].type);
        tl_freebstr(d);
    }
    ITypeLib_Release(tl);
}

/* FindName: adapted from Wine dlls/oleaut32/tests/typelib.c test_FindName (LGPL-2.1+) */
TEST(tlli_lib_findname_args)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    WCHAR buf[100]; MEMBERID memid; ITypeInfo *ti; USHORT c;
    TEQ(ITypeLib_FindName(tl, NULL, 0, NULL, NULL, NULL), E_INVALIDARG);
    lstrcpyW(buf, L"GUID");
    TEQ(ITypeLib_FindName(tl, buf, 0, NULL, NULL, NULL), E_INVALIDARG);
    c = 0; ti = SENT_TI;
    TEQ(ITypeLib_FindName(tl, buf, 0, &ti, NULL, &c), E_INVALIDARG);
    CHECK(c == 0); CHECK(ti == SENT_TI);
    c = 1; ti = SENT_TI;
    TEQ(ITypeLib_FindName(tl, buf, 0, &ti, NULL, &c), E_INVALIDARG);
    CHECK(c == 1); CHECK(ti == SENT_TI);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_findname_type_names)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const WCHAR *names[] = { L"GUID", L"IUnknown", L"IDispatch", L"Font", L"StdFont", L"OLE_COLOR", L"StdFunctions", L"LoadPictureConstants" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        WCHAR buf[100]; MEMBERID memid = 0; ITypeInfo *ti = SENT_TI; USHORT c = 1;
        lstrcpyW(buf, names[i]);
        HRESULT hr = ITypeLib_FindName(tl, buf, 0, &ti, &memid, &c);
        CHECKF(hr == S_OK, "%ls hr=0x%08lx", names[i], (unsigned long)hr);
        CHECKF(c == 1, "%ls cFound=%u", names[i], c);
        CHECKF(memid == MEMBERID_NIL, "%ls memid=%ld", names[i], memid);   /* a type name matched: MEMBERID_NIL */
        CHECK(!lstrcmpW(buf, names[i]));
        if (c == 1 && ti != SENT_TI && ti) {
            BSTR nm = NULL;
            if (SUCCEEDED(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &nm, NULL, NULL, NULL)))
                CHECKF(tl_wieq(nm, names[i]), "%ls -> %ls", names[i], nm);
            tl_freebstr(nm);
            ITypeInfo_Release(ti);
        }
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_findname_case_insensitive)
{
    /* Windows behaviour (Wine todo): a case-variant match returns the library's spelling in the buffer */
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    static const WCHAR *in[]  = { L"guid", L"iunknown", L"IDISPATCH", L"stdfont" };
    static const WCHAR *out[] = { L"GUID", L"IUnknown", L"IDispatch", L"StdFont" };
    for (unsigned i = 0; i < 4; i++) {
        WCHAR buf[100]; MEMBERID memid = 0; ITypeInfo *ti = SENT_TI; USHORT c = 1;
        lstrcpyW(buf, in[i]);
        HRESULT hr = ITypeLib_FindName(tl, buf, 0, &ti, &memid, &c);
        CHECKF(hr == S_OK, "%ls hr=0x%08lx", in[i], (unsigned long)hr);
        WCHECK(c == 1);
        WCHECK(memid == MEMBERID_NIL);
        WCHECK(!lstrcmpW(buf, out[i]));
        if (c == 1 && ti && ti != SENT_TI) ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlli_lib_findname_missing)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    WCHAR buf[100]; MEMBERID memid = 1234; ITypeInfo *ti = SENT_TI; USHORT c = 1;
    lstrcpyW(buf, L"invalid");
    TEQ(ITypeLib_FindName(tl, buf, 0, &ti, &memid, &c), S_OK);
    TEQ(c, 0);
    CHECK(ti == SENT_TI);
    CHECK(!lstrcmpW(buf, L"invalid"));
    ITypeLib_Release(tl);
}

TEST(tlli_lib_findname_member_names)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    /* "Name" is a member of IFont and Font; ask for several matches */
    WCHAR buf[100]; MEMBERID ids[8]; ITypeInfo *tis[8]; USHORT c = 8;
    memset(tis, 0, sizeof tis);
    lstrcpyW(buf, L"name");
    TEQ(ITypeLib_FindName(tl, buf, 0, tis, ids, &c), S_OK);
    if (!t_is_wine()) CHECKF(c >= 2, "cFound=%u", c);
    CHECK(c <= 8);
    for (USHORT i = 0; i < c && i < 8; i++) {
        CHECK(tis[i] != NULL);
        CHECK(ids[i] != MEMBERID_NIL);        /* member matches carry a real memid */
        if (tis[i]) ITypeInfo_Release(tis[i]);
    }
    WCHECK(!lstrcmpW(buf, L"Name"));
    /* limit cFound to 1 */
    c = 1; ITypeInfo *one = NULL; MEMBERID id1 = 0;
    lstrcpyW(buf, L"Name");
    TEQ(ITypeLib_FindName(tl, buf, 0, &one, &id1, &c), S_OK);
    TEQ(c, 1);
    if (one) ITypeInfo_Release(one);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_isname)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    BOOL f = 99; WCHAR buf[100];
    lstrcpyW(buf, L"IUnknown");
    TEQ(ITypeLib_IsName(tl, buf, 0, &f), S_OK);
    CHECK(f == TRUE);
    lstrcpyW(buf, L"iunknown");
    f = 99;
    TEQ(ITypeLib_IsName(tl, buf, 0, &f), S_OK);
    WCHECK(f == TRUE);
    WCHECK(!lstrcmpW(buf, L"IUnknown"));       /* buffer is corrected to the library's casing */
    lstrcpyW(buf, L"name");
    f = 99;
    TEQ(ITypeLib_IsName(tl, buf, 0, &f), S_OK);
    WCHECK(f == TRUE);
    WCHECK(!lstrcmpW(buf, L"Name"));
    lstrcpyW(buf, L"olet_no_such_name");
    f = 99;
    TEQ(ITypeLib_IsName(tl, buf, 0, &f), S_OK);
    CHECK(f == FALSE);
    CHECK(!lstrcmpW(buf, L"olet_no_such_name"));
    ITypeLib_Release(tl);
}

TEST(tlli_lib_isname_null_args)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    WCHAR buf[100]; BOOL f = 99;
    if (t_is_wine()) { t_skip("wine dereferences NULL"); ITypeLib_Release(tl); return; }
    lstrcpyW(buf, L"IUnknown");
    TEQ(ITypeLib_IsName(tl, buf, 0, NULL), E_INVALIDARG);
    TEQ(ITypeLib_IsName(tl, NULL, 0, &f), E_INVALIDARG);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_gettypecomp)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeComp *tc = SENT_TI ? (ITypeComp *)(ULONG_PTR)0x1badf00d : NULL, *tc2 = NULL;
    TEQ(ITypeLib_GetTypeComp(tl, &tc), S_OK);
    CHECK(tc != NULL && tc != (ITypeComp *)(ULONG_PTR)0x1badf00d);
    TEQ(ITypeLib_GetTypeComp(tl, &tc2), S_OK);
    t_note("GetTypeComp twice identical = %d", tc == tc2);
    /* the typecomp of a library QIs back to the library's ITypeLib */
    ITypeLib *back = NULL;
    if (tc && SUCCEEDED(ITypeComp_QueryInterface(tc, &TL_IID_ITypeLib, (void **)&back)) && back) {
        CHECK(back == tl);
        ITypeLib_Release(back);
    }
    if (tc) ITypeComp_Release(tc);
    if (tc2) ITypeComp_Release(tc2);
    ITypeLib_Release(tl);
}

TEST(tlli_lib_gettypecomp_null)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    if (t_is_wine()) { t_skip("wine dereferences NULL"); ITypeLib_Release(tl); return; }
    TEQ(ITypeLib_GetTypeComp(tl, NULL), E_INVALIDARG);
    ITypeLib_Release(tl);
}

TEST(tlli_lib2_statistics)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeLib2 *t2 = NULL;
    TEQ(ITypeLib_QueryInterface(tl, &TL_IID_ITypeLib2, (void **)&t2), S_OK);
    REQUIRE(t2);
    ULONG u = 0, h = 0;
    TEQ(ITypeLib2_GetLibStatistics(t2, &u, &h), S_OK);
    CHECK(u > 0);
    t_note("GetLibStatistics: unique names %lu, hash size %lu", u, h);
    TEQ(ITypeLib2_GetLibStatistics(t2, NULL, NULL), S_OK);
    ITypeLib2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_lib2_documentation2)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeLib2 *t2 = NULL;
    TEQ(ITypeLib_QueryInterface(tl, &TL_IID_ITypeLib2, (void **)&t2), S_OK);
    REQUIRE(t2);
    BSTR doc = NULL, hf = NULL; DWORD ctx = 0xdead;
    TEQ(ITypeLib2_GetDocumentation2(t2, -1, 0x409, &doc, &ctx, &hf), S_OK);
    CHECK(doc && tl_wieq(doc, L"OLE Automation"));
    TEQ(ctx, 0);
    tl_freebstr(doc); tl_freebstr(hf);
    TEQ(ITypeLib2_GetDocumentation2(t2, -1, 0, NULL, NULL, NULL), S_OK);
    BSTR d2 = NULL;
    TEQ(ITypeLib2_GetDocumentation2(t2, 0, LOCALE_USER_DEFAULT, &d2, NULL, NULL), S_OK);
    tl_freebstr(d2);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    d2 = NULL;
    TEQ(ITypeLib2_GetDocumentation2(t2, (INT)n, 0x409, &d2, NULL, NULL), TYPE_E_ELEMENTNOTFOUND);
    ITypeLib2_Release(t2);
    ITypeLib_Release(tl);
}

TEST(tlli_lib2_custdata)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeLib2 *t2 = NULL;
    TEQ(ITypeLib_QueryInterface(tl, &TL_IID_ITypeLib2, (void **)&t2), S_OK);
    REQUIRE(t2);
    VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4;
    HRESULT hrc = ITypeLib2_GetCustData(t2, &TL_GUID_NONE, &v);   /* unknown guid */
    t_note("ITypeLib2::GetCustData(unknown) -> 0x%08lx vt=%d", (unsigned long)hrc, V_VT(&v));
    CHECK(hrc == S_OK || hrc == TYPE_E_ELEMENTNOTFOUND);
    if (hrc == S_OK) CHECK(V_VT(&v) == VT_EMPTY);
    CUSTDATA cd; memset(&cd, 0xcc, sizeof cd);
    TEQ(ITypeLib2_GetAllCustData(t2, &cd), S_OK);
    CHECK(cd.cCustData == 0 || cd.prgCustData != NULL);
    t_note("stdole2 library custom data items: %lu", cd.cCustData);
    if (cd.cCustData == 0) CHECK(cd.prgCustData == NULL);
    ITypeLib2_Release(t2);
    ITypeLib_Release(tl);
}
