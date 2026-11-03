/* ICreateTypeLib2 / CreateTypeLib2 / CreateTypeLib: library-level creation, attributes, documentation,
 * custom data, type info enumeration and the on-disk header. Verified against the Windows 10 oleaut32
 * decompilation of CreateTypeLib2/CreateTypeLib (E_INVALIDARG argument checks) and the documented
 * ICreateTypeLib2 contract. */
#include "tlc.h"

TEST(tlcl_createtypelib2_null_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    tl_path(path, "nullargs.tlb");
    ICreateTypeLib2 *p = NULL;
    if (t_is_wine()) { t_skip("Wine dereferences NULL szFile/ppctlib"); return; }
    EQHR(pCreateTypeLib2(SYS_WIN32, NULL, &p), E_INVALIDARG);
    EQHR(pCreateTypeLib2(SYS_WIN32, path, NULL), E_INVALIDARG);
    EQHR(pCreateTypeLib2(SYS_WIN32, NULL, NULL), E_INVALIDARG);
    EQHR(pCreateTypeLib2(SYS_WIN64, NULL, &p), E_INVALIDARG);
    CHECK(p == NULL);
}

TEST(tlcl_createtypelib_null_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    tl_path(path, "nullargs_old.tlb");
    ICreateTypeLib *p = NULL;
    if (t_is_wine()) { t_skip("Wine dereferences NULL szFile/ppctlib"); return; }
    EQHR(pCreateTypeLib(SYS_WIN32, NULL, &p), E_INVALIDARG);
    EQHR(pCreateTypeLib(SYS_WIN32, path, NULL), E_INVALIDARG);
    EQHR(pCreateTypeLib(SYS_WIN32, NULL, NULL), E_INVALIDARG);
    CHECK(p == NULL);
}

TEST(tlcl_oacreatetypelib2_alias_export)
{
    TL_BEGIN();
    /* decomp: 0x4aa30 is exported both as CreateTypeLib2 (ordinal 180) and OACreateTypeLib2 (352) */
    FARPROC a = t_proc_in("oleaut32.dll", "CreateTypeLib2");
    FARPROC b = t_proc_in("oleaut32.dll", "OACreateTypeLib2");
    CHECK(a != NULL);
    CHECK(b != NULL);
    CHECK(a == b);
}

TEST(tlcl_create_out_ptr_nulled_on_success_path)
{
    TL_BEGIN();
    for (int k = 0; k < 2; k++) {
        SYSKIND sk = k ? SYS_WIN64 : SYS_WIN32;
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = NULL;
        tl_path(path, k ? "cr64.tlb" : "cr32.tlb");
        OK_(pCreateTypeLib2(sk, path, &ctl));
        CHECK(ctl != NULL);
        if (ctl) {
            EQ(ICreateTypeLib2_AddRef(ctl), 2);
            EQ(ICreateTypeLib2_Release(ctl), 1);
            EQ(ICreateTypeLib2_Release(ctl), 0);
        }
    }
}

TEST(tlcl_qi_interfaces)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "qi.tlb", path);
    if (!ctl) return;
    void *p;
    p = NULL; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_IUnknown, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_ICreateTypeLib, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_ICreateTypeLib2, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib2, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeInfo, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)1; EQHR(ICreateTypeLib2_QueryInterface(ctl, &IID_IDispatch, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    EQ(ICreateTypeLib2_Release(ctl), 0);
}

TEST(tlcl_qi_identity)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "qiid.tlb", path);
    if (!ctl) return;
    IUnknown *u1 = NULL, *u2 = NULL;
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_IUnknown, (void **)&u1));
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
    OK_(ITypeLib_QueryInterface(tl, &IID_IUnknown, (void **)&u2));
    CHECK(u1 == u2);
    CHECK(u1 != NULL);
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (tl) ITypeLib_Release(tl);
    EQ(ICreateTypeLib2_Release(ctl), 0);
}

TEST(tlcl_empty_lib_state)
{
    TL_BEGIN();
    for (int k = 0; k < 2; k++) {
        SYSKIND sk = k ? SYS_WIN64 : SYS_WIN32;
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, k ? "empty64.tlb" : "empty32.tlb", path);
        if (!ctl) continue;
        ITypeLib *tl = NULL;
        OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
        if (tl) {
            EQ(ITypeLib_GetTypeInfoCount(tl), 0);
            ITypeInfo *ti = (ITypeInfo *)1;
            EQHR(ITypeLib_GetTypeInfo(tl, 0, NULL), E_INVALIDARG);
            ti = NULL;
            EQHR(ITypeLib_GetTypeInfo(tl, 0, &ti), TYPE_E_ELEMENTNOTFOUND);
            TYPEKIND kind;
            EQHR(ITypeLib_GetTypeInfoType(tl, 0, &kind), TYPE_E_ELEMENTNOTFOUND);
            EQHR(ITypeLib_GetTypeInfoType(tl, 0, NULL), E_INVALIDARG);
            EQHR(ITypeLib_GetLibAttr(tl, NULL), E_INVALIDARG);
            TLIBATTR *a = NULL;
            OK_(ITypeLib_GetLibAttr(tl, &a));
            if (a) {
                EQ(a->syskind, sk);
                EQ(a->wMajorVerNum, 0);
                EQ(a->wMinorVerNum, 0);
                EQ(a->wLibFlags, 0);
                ITypeLib_ReleaseTLibAttr(tl, a);
            }
            BSTR name = (BSTR)(ULONG_PTR)0xdeadbeef, doc = (BSTR)(ULONG_PTR)0xdeadbeef, hf = (BSTR)(ULONG_PTR)0xdeadbeef;
            DWORD hc = 0x1234;
            OK_(ITypeLib_GetDocumentation(tl, -1, &name, &doc, &hc, &hf));
            CHECK(name == NULL);
            CHECK(doc == NULL);
            CHECK(hf == NULL);
            EQ(hc, 0);
            OK_(ITypeLib_GetDocumentation(tl, -1, NULL, NULL, NULL, NULL));
            name = NULL;
            EQHR(ITypeLib_GetDocumentation(tl, 0, &name, NULL, NULL, NULL), TYPE_E_ELEMENTNOTFOUND);
            ITypeLib_Release(tl);
        }
        EQ(ICreateTypeLib2_Release(ctl), 0);
    }
}

TEST(tlcl_syskind_roundtrip)
{
    TL_BEGIN();
    for (int k = 0; k < 2; k++) {
        SYSKIND sk = k ? SYS_WIN64 : SYS_WIN32;
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, k ? "sk64.tlb" : "sk32.tlb", path);
        if (!ctl) continue;
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) continue;
        TLIBATTR *a = NULL;
        OK_(ITypeLib_GetLibAttr(tl, &a));
        if (a) {
            EQ(a->syskind, sk);
            EQ(a->wLibFlags, LIBFLAG_FHASDISKIMAGE);
            EQ(a->wMajorVerNum, 0);
            EQ(a->wMinorVerNum, 0);
            EQ(a->lcid, 0);
            CHECK(IsEqualGUID(&a->guid, &GUID_NULL));
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        EQ(ITypeLib_GetTypeInfoCount(tl), 0);
        EQ(ITypeLib_Release(tl), 0);
    }
}

TEST(tlcl_setters_roundtrip)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "setters.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"OleTestLib"));
    OK_(ICreateTypeLib2_SetVersion(ctl, 3, 7));
    OK_(ICreateTypeLib2_SetGuid(ctl, &TLC_GUID_LIB));
    OK_(ICreateTypeLib2_SetDocString(ctl, (LPOLESTR)L"A test library"));
    OK_(ICreateTypeLib2_SetHelpFileName(ctl, (LPOLESTR)L"olet.hlp"));
    OK_(ICreateTypeLib2_SetHelpContext(ctl, 0x1234));
    OK_(ICreateTypeLib2_SetLcid(ctl, 0x409));
    OK_(ICreateTypeLib2_SetLibFlags(ctl, LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN));
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    TLIBATTR *a = NULL;
    OK_(ITypeLib_GetLibAttr(tl, &a));
    if (a) {
        EQ(a->syskind, SYS_WIN64);
        EQ(a->wMajorVerNum, 3);
        EQ(a->wMinorVerNum, 7);
        CHECK(IsEqualGUID(&a->guid, &TLC_GUID_LIB));
        EQ(a->lcid, 0x409);
        EQ(a->wLibFlags & (LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN), LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN);
        EQ(a->wLibFlags & LIBFLAG_FHASDISKIMAGE, LIBFLAG_FHASDISKIMAGE);
        EQ(a->wLibFlags & LIBFLAG_FCONTROL, 0);
        t_note("wLibFlags after reload = 0x%x", a->wLibFlags);
        ITypeLib_ReleaseTLibAttr(tl, a);
    }
    BSTR name = NULL, doc = NULL, hf = NULL;
    DWORD hc = 0;
    OK_(ITypeLib_GetDocumentation(tl, -1, &name, &doc, &hc, &hf));
    char b[256];
    if (name) { tl_w2a(name, b, sizeof b); EXPECT_STR(b, "OleTestLib"); } else CHECK(name != NULL);
    if (doc) { tl_w2a(doc, b, sizeof b); EXPECT_STR(b, "A test library"); } else CHECK(doc != NULL);
    t_note("lib helpcontext after reload = 0x%lx", hc);
    if (hf) { tl_w2a(hf, b, sizeof b); t_note("helpfile after reload = \"%s\"", b); CHECK(strstr(b, "olet.hlp") != NULL); }
    else CHECK(hf != NULL);
    pSysFreeString(name); pSysFreeString(doc); pSysFreeString(hf);
    EQ(ITypeLib_Release(tl), 0);
}

TEST(tlcl_setters_visible_before_save)
{
    TL_BEGIN();
    static OLECHAR helpfile[] = L"C:\\bogus.hlp";
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "presave.tlb", path);
    if (!ctl) return;
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"typelib"));
    OK_(ICreateTypeLib2_SetHelpFileName(ctl, helpfile));
    BSTR name = NULL, hf = NULL;
    OK_(ITypeLib_GetDocumentation(tl, -1, &name, NULL, NULL, &hf));
    CHECK(name && !lstrcmpW(name, L"typelib"));
    CHECK(hf && !lstrcmpW(hf, helpfile));
    pSysFreeString(name); pSysFreeString(hf);
    ITypeLib_Release(tl);
    EQ(ICreateTypeLib2_Release(ctl), 0);
}

TEST(tlcl_setname_twice_last_wins)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "setname2.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"First"));
    HRESULT hr = ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"Second");
    t_note("second SetName hr=0x%08lx", (unsigned long)hr);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    BSTR n = NULL;
    OK_(ITypeLib_GetDocumentation(tl, -1, &n, NULL, NULL, NULL));
    if (n) {
        char b[64]; tl_w2a(n, b, sizeof b);
        t_note("lib name after two SetName = %s", b);
        CHECK(hr == S_OK ? !strcmp(b, "Second") : !strcmp(b, "First"));
    }
    pSysFreeString(n);
    ITypeLib_Release(tl);
}

TEST(tlcl_setname_null_note)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "setnull.tlb", path);
    if (!ctl) return;
    t_note("SetName(NULL)=0x%08lx", (unsigned long)ICreateTypeLib2_SetName(ctl, NULL));
    t_note("SetDocString(NULL)=0x%08lx", (unsigned long)ICreateTypeLib2_SetDocString(ctl, NULL));
    t_note("SetHelpFileName(NULL)=0x%08lx", (unsigned long)ICreateTypeLib2_SetHelpFileName(ctl, NULL));
    if (!t_is_wine()) t_note("SetGuid(NULL)=0x%08lx", (unsigned long)ICreateTypeLib2_SetGuid(ctl, NULL));
    EQ(ICreateTypeLib2_Release(ctl), 0);
}

TEST(tlcl_libflags_variants)
{
    TL_BEGIN();
    static const WORD sets[] = { 0, LIBFLAG_FRESTRICTED, LIBFLAG_FCONTROL, LIBFLAG_FHIDDEN };
    for (unsigned i = 0; i < 4; i++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "flags.tlb", path);
        if (!ctl) return;
        OK_(ICreateTypeLib2_SetLibFlags(ctl, sets[i]));
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        TLIBATTR *a = NULL;
        OK_(ITypeLib_GetLibAttr(tl, &a));
        if (a) {
            CHECKF((a->wLibFlags & sets[i]) == sets[i], "flags set 0x%x got 0x%x", sets[i], a->wLibFlags);
            EQ(a->wLibFlags & LIBFLAG_FHASDISKIMAGE, LIBFLAG_FHASDISKIMAGE);
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlcl_version_roundtrip_table)
{
    TL_BEGIN();
    static const WORD v[][2] = { {0,0}, {1,0}, {0,1}, {2,5}, {255,255}, {1000,2000}, {0xffff,0xffff} };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ver.tlb", path);
        if (!ctl) return;
        OK_(ICreateTypeLib2_SetVersion(ctl, v[i][0], v[i][1]));
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        TLIBATTR *a = NULL;
        OK_(ITypeLib_GetLibAttr(tl, &a));
        if (a) {
            CHECKF(a->wMajorVerNum == v[i][0] && a->wMinorVerNum == v[i][1], "ver %u.%u got %u.%u", v[i][0], v[i][1],
                   a->wMajorVerNum, a->wMinorVerNum);
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlcl_lcid_roundtrip_table)
{
    TL_BEGIN();
    static const LCID l[] = { 0, 0x409, 0x809, 0x407, 0x007, 0x009, 0x40c };
    for (unsigned i = 0; i < sizeof l / sizeof l[0]; i++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "lcid.tlb", path);
        if (!ctl) return;
        OK_(ICreateTypeLib2_SetLcid(ctl, l[i]));
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        TLIBATTR *a = NULL;
        OK_(ITypeLib_GetLibAttr(tl, &a));
        if (a) { CHECKF(a->lcid == l[i], "lcid 0x%lx got 0x%lx", l[i], a->lcid); ITypeLib_ReleaseTLibAttr(tl, a); }
        ITypeLib_Release(tl);
    }
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_create_typelib_lcid: on-disk MSFT header */
TEST(tlcl_file_header_msft)
{
    TL_BEGIN();
    static const LCID l[] = { 0, 0x409, 0x809, 0x007, 0x407, 0x009 };
    for (unsigned i = 0; i < sizeof l / sizeof l[0]; i++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "hdr.tlb", path);
        if (!ctl) return;
        OK_(ICreateTypeLib2_SetLcid(ctl, l[i]));
        OK_(ICreateTypeLib2_SetVersion(ctl, 3, 4));
        OK_(ICreateTypeLib2_SaveAllChanges(ctl));
        EQ(ICreateTypeLib2_Release(ctl), 0);
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        CHECK(f != INVALID_HANDLE_VALUE);
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD h[8] = {0}, rd = 0;
        ReadFile(f, h, sizeof h, &rd, NULL);
        CloseHandle(f);
        EQ(rd, sizeof h);
        EQ(h[0], 0x5446534d); /* "MSFT" */
        EQ(h[1], 0x00010002);
        EQ(h[2], 0xffffffff);
        EQ(h[3], l[i] ? l[i] : 0x409);
        EQ(h[4], l[i]);
        EQ(h[6], 0x00040003);
        EQ(h[7], 0);
    }
}

TEST(tlcl_file_exists_after_save)
{
    TL_BEGIN();
    for (int k = 0; k < 2; k++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(k ? SYS_WIN64 : SYS_WIN32, "exist.tlb", path);
        if (!ctl) return;
        OK_(ICreateTypeLib2_SaveAllChanges(ctl));
        EQ(ICreateTypeLib2_Release(ctl), 0);
        DWORD at = GetFileAttributesW(path);
        CHECK(at != INVALID_FILE_ATTRIBUTES);
        HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD sz = GetFileSize(f, NULL);
            CHECKF(sz >= 0x80 && sz < (1 << 20), "file size %lu", sz);
            CloseHandle(f);
        }
        DeleteFileW(path);
    }
}

TEST(tlcl_save_twice_and_modify)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "save2.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetVersion(ctl, 1, 1));
    OK_(ICreateTypeLib2_SaveAllChanges(ctl));
    OK_(ICreateTypeLib2_SetVersion(ctl, 2, 2));
    HRESULT hr = ICreateTypeLib2_SaveAllChanges(ctl);
    t_note("second SaveAllChanges hr=0x%08lx", (unsigned long)hr);
    EQ(ICreateTypeLib2_Release(ctl), 0);
    ITypeLib *tl = NULL;
    OK_(pLoadTypeLibEx(path, REGKIND_NONE, &tl));
    if (tl) {
        TLIBATTR *a = NULL;
        OK_(ITypeLib_GetLibAttr(tl, &a));
        if (a) {
            if (hr == S_OK) { EQ(a->wMajorVerNum, 2); EQ(a->wMinorVerNum, 2); }
            else t_note("version after failed second save %u.%u", a->wMajorVerNum, a->wMinorVerNum);
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlcl_loaded_lib_is_not_creatable)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ro.tlb", path);
    if (!ctl) return;
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    void *p = (void *)1;
    HRESULT hr = ITypeLib_QueryInterface(tl, &IID_ICreateTypeLib2, &p);
    t_note("loaded lib QI(ICreateTypeLib2) = 0x%08lx", (unsigned long)hr);
    if (hr == S_OK && p) IUnknown_Release((IUnknown *)p);
    p = NULL;
    OK_(ITypeLib_QueryInterface(tl, &IID_ITypeLib2, &p));
    if (p) IUnknown_Release((IUnknown *)p);
    ITypeLib_Release(tl);
}

TEST(tlcl_custdata_lib)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "cust.tlb", path);
    if (!ctl) return;
    VARIANT v;
    pVariantInit(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 1;
    OK_(ICreateTypeLib2_SetCustData(ctl, (GUID *)&TLC_GUID_CUST, &v));
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = pSysAllocString(L"custom string");
    OK_(ICreateTypeLib2_SetCustData(ctl, (GUID *)&TLC_GUID_CUST2, &v));
    pVariantClear(&v);
    ITypeLib2 *tl2 = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib2, (void **)&tl2));
    if (tl2) {
        VARIANT r; pVariantInit(&r);
        OK_(ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_CUST, &r));
        EQ(V_VT(&r), VT_I4); EQ(V_I4(&r), 1);
        pVariantClear(&r);
        OK_(ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_CUST2, &r));
        EQ(V_VT(&r), VT_BSTR);
        CHECK(V_VT(&r) == VT_BSTR && V_BSTR(&r) && !lstrcmpW(V_BSTR(&r), L"custom string"));
        pVariantClear(&r);
                V_VT(&r) = VT_UI4; V_UI4(&r) = 17;
        t_note("lib GetCustData(unknown guid) = 0x%08lx", (unsigned long)ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_A, &r));
        CUSTDATA cd; memset(&cd, 0xcc, sizeof cd);
        OK_(ITypeLib2_GetAllCustData(tl2, &cd));
        EQ(cd.cCustData, 2);
        if (cd.cCustData == 2 && cd.prgCustData) {
            int seen = 0;
            for (unsigned i = 0; i < 2; i++) {
                if (IsEqualGUID(&cd.prgCustData[i].guid, &TLC_GUID_CUST)) { seen |= 1; EQ(V_VT(&cd.prgCustData[i].varValue), VT_I4); }
                if (IsEqualGUID(&cd.prgCustData[i].guid, &TLC_GUID_CUST2)) { seen |= 2; EQ(V_VT(&cd.prgCustData[i].varValue), VT_BSTR); }
            }
            EQ(seen, 3);
        }
        ClearCustData(&cd);
        ITypeLib2_Release(tl2);
    }
    /* survives save/load */
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    OK_(ITypeLib_QueryInterface(tl, &IID_ITypeLib2, (void **)&tl2));
    if (tl2) {
        VARIANT r; pVariantInit(&r);
        OK_(ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_CUST, &r));
        EQ(V_VT(&r), VT_I4); EQ(V_I4(&r), 1);
        pVariantClear(&r);
        OK_(ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_CUST2, &r));
        CHECK(V_VT(&r) == VT_BSTR && V_BSTR(&r) && !lstrcmpW(V_BSTR(&r), L"custom string"));
        pVariantClear(&r);
        ITypeLib2_Release(tl2);
    }
    ITypeLib_Release(tl);
}

TEST(tlcl_custdata_replace_value)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "custrep.tlb", path);
    if (!ctl) return;
    VARIANT v; pVariantInit(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 5;
    OK_(ICreateTypeLib2_SetCustData(ctl, (GUID *)&TLC_GUID_CUST, &v));
    V_I4(&v) = 9;
    HRESULT hr = ICreateTypeLib2_SetCustData(ctl, (GUID *)&TLC_GUID_CUST, &v);
    t_note("SetCustData same guid twice hr=0x%08lx", (unsigned long)hr);
    ITypeLib2 *tl2 = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib2, (void **)&tl2));
    if (tl2) {
        VARIANT r; pVariantInit(&r);
        OK_(ITypeLib2_GetCustData(tl2, (GUID *)&TLC_GUID_CUST, &r));
        EQ(V_VT(&r), VT_I4);
        t_note("value after replace = %ld", (long)V_I4(&r));
        CHECK(V_I4(&r) == (hr == S_OK ? 9 : 5));
        CUSTDATA cd;
        OK_(ITypeLib2_GetAllCustData(tl2, &cd));
        t_note("cCustData after replace = %lu", cd.cCustData);
        ClearCustData(&cd);
        ITypeLib2_Release(tl2);
    }
    ICreateTypeLib2_Release(ctl);
}

TEST(tlcl_getlibstatistics_stubs)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "stats.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *a = tl_cti(ctl, L"A", TKIND_ENUM);
    ICreateTypeInfo *b = tl_cti(ctl, L"B", TKIND_RECORD);
    if (a) ICreateTypeInfo_Release(a);
    if (b) ICreateTypeInfo_Release(b);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeLib2 *tl2 = NULL;
    OK_(ITypeLib_QueryInterface(tl, &IID_ITypeLib2, (void **)&tl2));
    if (tl2) {
        ULONG uniq = 0, total = 0;
        HRESULT hr = ITypeLib2_GetLibStatistics(tl2, &uniq, &total);
        t_note("GetLibStatistics hr=0x%08lx unique=%lu total=%lu", (unsigned long)hr, uniq, total);
        ITypeLib2_Release(tl2);
    }
    EQ(ITypeLib_GetTypeInfoCount(tl), 2);
    ITypeLib_Release(tl);
}

TEST(tlcl_createtypeinfo_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "cti_args.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *cti = NULL;
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, NULL, TKIND_INTERFACE, &cti), E_INVALIDARG);
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface1", TKIND_INTERFACE, NULL), E_INVALIDARG);
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, NULL, TKIND_INTERFACE, NULL), E_INVALIDARG);
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
    if (tl) { EQ(ITypeLib_GetTypeInfoCount(tl), 0); ITypeLib_Release(tl); }
    OK_(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface1", TKIND_INTERFACE, &cti));
    CHECK(cti != NULL);
    if (cti) ICreateTypeInfo_Release(cti);
    EQ(ICreateTypeLib2_Release(ctl), 0);
}

TEST(tlcl_createtypeinfo_name_conflict)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "nameconf.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *a = tl_cti(ctl, L"interface1", TKIND_INTERFACE), *b = NULL;
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface1", TKIND_INTERFACE, &b), TYPE_E_NAMECONFLICT);
    CHECK(b == NULL);
    /* the conflict is across type kinds */
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface1", TKIND_ENUM, &b), TYPE_E_NAMECONFLICT);
    EQHR(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface1", TKIND_RECORD, &b), TYPE_E_NAMECONFLICT);
    /* names compare case-insensitively */
    HRESULT hr = ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"INTERFACE1", TKIND_INTERFACE, &b);
    t_note("create \"INTERFACE1\" next to \"interface1\" hr=0x%08lx", (unsigned long)hr);
    if (b) ICreateTypeInfo_Release(b);
    b = NULL;
    ICreateTypeInfo *c = NULL;
    OK_(ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"interface2", TKIND_INTERFACE, &c));
    if (a) ICreateTypeInfo_Release(a);
    if (c) ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlcl_createtypeinfo_all_kinds)
{
    TL_BEGIN();
    static const struct { const WCHAR *n; TYPEKIND k; } t[] = {
        {L"E", TKIND_ENUM}, {L"R", TKIND_RECORD}, {L"M", TKIND_MODULE}, {L"I", TKIND_INTERFACE},
        {L"D", TKIND_DISPATCH}, {L"C", TKIND_COCLASS}, {L"A", TKIND_ALIAS}, {L"U", TKIND_UNION} };
    for (int s = 0; s < 2; s++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(s ? SYS_WIN64 : SYS_WIN32, "kinds.tlb", path);
        if (!ctl) return;
        for (unsigned i = 0; i < 8; i++) {
            ICreateTypeInfo *c = tl_cti(ctl, t[i].n, t[i].k);
            if (!c) continue;
            if (t[i].k == TKIND_ALIAS) {
                TYPEDESC td = { .vt = VT_I4 };
                OK_(ICreateTypeInfo_SetTypeDescAlias(c, &td));
            }
            ICreateTypeInfo_Release(c);
        }
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        EQ(ITypeLib_GetTypeInfoCount(tl), 8);
        for (unsigned i = 0; i < 8; i++) {
            TYPEKIND k = (TYPEKIND)99;
            OK_(ITypeLib_GetTypeInfoType(tl, i, &k));
            CHECKF(k == t[i].k, "index %u kind %d want %d", i, k, t[i].k);
            ITypeInfo *ti = tl_ti(tl, i);
            if (!ti) continue;
            char nm[32]; tl_doc_name(ti, MEMBERID_NIL, nm, sizeof nm);
            char want[8]; tl_w2a(t[i].n, want, sizeof want);
            EXPECT_STR(nm, want);
            TYPEATTR *ta = NULL;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) {
                EQ(ta->typekind, t[i].k);
                EQ(ta->cFuncs, 0);
                EQ(ta->cVars, 0);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            }
            ITypeInfo_Release(ti);
        }
        TYPEKIND k;
        EQHR(ITypeLib_GetTypeInfoType(tl, 8, &k), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo *ti = NULL;
        EQHR(ITypeLib_GetTypeInfo(tl, 8, &ti), TYPE_E_ELEMENTNOTFOUND);
        EQ(ITypeLib_Release(tl), 0);
    }
}

TEST(tlcl_typeinfo_guid_lookup)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "guidlook.tlb", path);
    if (!ctl) return;
    const GUID *g[3] = { &TLC_GUID_A, &TLC_GUID_B, &TLC_GUID_C };
    const WCHAR *n[3] = { L"IAlpha", L"IBeta", L"IGamma" };
    for (int i = 0; i < 3; i++) {
        ICreateTypeInfo *c = tl_cti(ctl, n[i], TKIND_DISPATCH);
        if (!c) continue;
        OK_(ICreateTypeInfo_SetGuid(c, g[i]));
        ICreateTypeInfo_Release(c);
    }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    for (int i = 2; i >= 0; i--) {
        ITypeInfo *ti = NULL;
        OK_(ITypeLib_GetTypeInfoOfGuid(tl, g[i], &ti));
        if (ti) {
            char nm[32], want[32]; tl_doc_name(ti, MEMBERID_NIL, nm, sizeof nm); tl_w2a(n[i], want, sizeof want);
            EXPECT_STR(nm, want);
            TYPEATTR *ta = NULL;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) { CHECK(IsEqualGUID(&ta->guid, g[i])); ITypeInfo_ReleaseTypeAttr(ti, ta); }
            ITypeInfo_Release(ti);
        }
    }
    ITypeInfo *ti = (ITypeInfo *)1;
    ti = NULL;
    EQHR(ITypeLib_GetTypeInfoOfGuid(tl, &TLC_GUID_D, &ti), TYPE_E_ELEMENTNOTFOUND);
    CHECK(ti == NULL);
    ITypeLib_Release(tl);
}

TEST(tlcl_findname_and_isname)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "findname.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"Widget", TKIND_DISPATCH);
    if (c) {
        OK_(tl_add_func_k(c, 0, FUNC_DISPATCH, INVOKE_FUNC, VT_VOID, 0x100, NULL, 0));
        LPOLESTR nm[1] = { (LPOLESTR)L"Frobnicate" };
        OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 1));
        ICreateTypeInfo_Release(c);
    }
    c = tl_cti(ctl, L"Gadget", TKIND_ENUM);
    if (c) ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    WCHAR buf[32];
    BOOL r;
    lstrcpyW(buf, L"WIDGET");
    OK_(ITypeLib_IsName(tl, buf, 0, &r));
    t_note("IsName(WIDGET)=%d buf=%ls", r, buf);
    lstrcpyW(buf, L"Nothing");
    OK_(ITypeLib_IsName(tl, buf, 0, &r));
    CHECK(!r);
    ITypeInfo *ti[4]; MEMBERID id[4]; USHORT found = 4;
    memset(ti, 0, sizeof ti);
    lstrcpyW(buf, L"Gadget");
    OK_(ITypeLib_FindName(tl, buf, 0, ti, id, &found));
    EQ(found, 1);
    if (found >= 1 && ti[0]) {
        TYPEKIND k; TYPEATTR *ta = NULL;
        if (ITypeInfo_GetTypeAttr(ti[0], &ta) == S_OK) { EQ(ta->typekind, TKIND_ENUM); ITypeInfo_ReleaseTypeAttr(ti[0], ta); }
        ITypeInfo_Release(ti[0]);
    }
    found = 4; memset(ti, 0, sizeof ti);
    lstrcpyW(buf, L"Frobnicate");
    OK_(ITypeLib_FindName(tl, buf, 0, ti, id, &found));
    EQ(found, 1);
    if (found >= 1 && ti[0]) { EQ(id[0], 0x100); ITypeInfo_Release(ti[0]); }
    found = 4;
    lstrcpyW(buf, L"Missing");
    OK_(ITypeLib_FindName(tl, buf, 0, ti, id, &found));
    EQ(found, 0);
    ITypeLib_Release(tl);
}

TEST(tlcl_typeinfo_order_is_creation_order)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "order.tlb", path);
    if (!ctl) return;
    static const WCHAR *names[] = { L"Zeta", L"Alpha", L"Mid", L"beta", L"Omega" };
    for (unsigned i = 0; i < 5; i++) {
        ICreateTypeInfo *c = tl_cti(ctl, names[i], TKIND_RECORD);
        if (c) ICreateTypeInfo_Release(c);
    }
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_QueryInterface(ctl, &IID_ITypeLib, (void **)&tl));
    if (tl) {
        for (unsigned i = 0; i < 5; i++) {
            ITypeInfo *ti = tl_ti(tl, i);
            if (!ti) continue;
            char nm[16], w[16]; tl_doc_name(ti, MEMBERID_NIL, nm, sizeof nm); tl_w2a(names[i], w, sizeof w);
            EXPECT_STR(nm, w);
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
    tl = tl_reload(ctl, path);
    if (!tl) return;
    for (unsigned i = 0; i < 5; i++) {
        ITypeInfo *ti = tl_ti(tl, i);
        if (!ti) continue;
        char nm[16], w[16]; tl_doc_name(ti, MEMBERID_NIL, nm, sizeof nm); tl_w2a(names[i], w, sizeof w);
        EXPECT_STR(nm, w);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlcl_typeinfo_containing_typelib)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "contain.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"ContainLib"));
    ICreateTypeInfo *c = tl_cti(ctl, L"T", TKIND_RECORD);
    if (c) ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        ITypeLib *tl2 = NULL; UINT idx = 99;
        OK_(ITypeInfo_GetContainingTypeLib(ti, &tl2, &idx));
        EQ(idx, 0);
        CHECK(tl2 != NULL);
        if (tl2) {
            BSTR n = NULL;
            OK_(ITypeLib_GetDocumentation(tl2, -1, &n, NULL, NULL, NULL));
            CHECK(n && !lstrcmpW(n, L"ContainLib"));
            pSysFreeString(n);
            ITypeLib_Release(tl2);
        }
        t_note("GetContainingTypeLib(NULL out) = 0x%08lx", (unsigned long)ITypeInfo_GetContainingTypeLib(ti, NULL, &idx));
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlcl_stdole2_baseline)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    ITypeInfo *disp = tl_stdole_ti(so, &IID_IDispatch);
    if (unk) {
        TYPEATTR *ta = NULL;
        OK_(ITypeInfo_GetTypeAttr(unk, &ta));
        if (ta) { EQ(ta->cbSizeVft, 3 * sizeof(void *)); EQ(ta->cFuncs, 3); ITypeInfo_ReleaseTypeAttr(unk, ta); }
        ITypeInfo_Release(unk);
    }
    if (disp) {
        TYPEATTR *ta = NULL;
        OK_(ITypeInfo_GetTypeAttr(disp, &ta));
        if (ta) { EQ(ta->cbSizeVft, 7 * sizeof(void *)); EQ(ta->cImplTypes, 1); ITypeInfo_ReleaseTypeAttr(disp, ta); }
        ITypeInfo_Release(disp);
    }
    ITypeLib_Release(so);
}

TEST(tlcl_loadtypelibex_missing_file)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    tl_path(path, "does_not_exist_olet.tlb");
    ITypeLib *tl = (ITypeLib *)1;
    HRESULT hr = pLoadTypeLibEx(path, REGKIND_NONE, &tl);
    CHECK(FAILED(hr));
    EQ(hr, TYPE_E_CANTLOADLIBRARY);
    CHECK(tl == NULL);
}

TEST(tlcl_loadtypelibex_garbage_file)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    tl_path(path, "garbage.tlb");
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    char junk[256]; memset(junk, 'x', sizeof junk);
    DWORD w; WriteFile(f, junk, sizeof junk, &w, NULL); CloseHandle(f);
    ITypeLib *tl = (ITypeLib *)1;
    HRESULT hr = pLoadTypeLibEx(path, REGKIND_NONE, &tl);
    CHECK(FAILED(hr));
    t_note("LoadTypeLibEx(garbage file) = 0x%08lx", (unsigned long)hr);
    CHECK(tl == NULL);
    DeleteFileW(path);
}

TEST(tlcl_loadtypelibex_null_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    tl_path(path, "ntl.tlb");
    EQHR(pLoadTypeLibEx(path, REGKIND_NONE, NULL), E_INVALIDARG);
    ITypeLib *tl = NULL;
    EQHR(pLoadTypeLibEx(NULL, REGKIND_NONE, &tl), E_INVALIDARG);
}
