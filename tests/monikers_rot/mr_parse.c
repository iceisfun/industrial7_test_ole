/* Class moniker, objref moniker, MkParseDisplayName, BindMoniker, CoGetObject. */
#include "mr.h"

#define SETUP() do { MR_NEED_BASE(); MR_COM_BEGIN(); } while (0)

/* ====================================================================== class moniker */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_basic_properties) {
    IMoniker *m = NULL; CLSID c; void *p; SETUP();
    EXPECT_EQ(U(CreateClassMoniker)(&mr_CLS_CatMgr, &m), S_OK);
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK); CHECK(mr_guid_eq(&c, &mr_CLSID_ClassMoniker));
        CHECK_TYPE(m, MKSYS_CLASSMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        CHECK_DN(m, L"clsid:0002E005-0000-0000-C000-000000000046:");
        EXPECT_EQ(mr_hash(m), mr_CLS_CatMgr.Data1);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_ClassMoniker, &p), S_OK); EXPECT_EQ(p, m); if (p) IMoniker_Release(m);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IPersist, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IROTData, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_class_create_edge_cases) {
    IMoniker *m = NULL; SETUP();
    if (!t_is_wine()) EXPECT_EQ(U(CreateClassMoniker)(&mr_CLS_CatMgr, NULL), E_INVALIDARG);
    EXPECT_EQ(U(CreateClassMoniker)(&mr_GUID_NULL, &m), S_OK);        /* GUID_NULL is a legal class id */
    if (m) { CHECK_DN(m, L"clsid:00000000-0000-0000-0000-000000000000:"); EXPECT_EQ(mr_hash(m), 0); IMoniker_Release(m); }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_persistence) {
    static const BYTE saved[] = { 0x05,0xe0,0x02,0x00, 0,0, 0,0, 0xc0,0,0,0,0,0,0,0x46, 0,0,0,0 };
    IMoniker *m; BYTE buf[128]; HRESULT hr; ULONG n, len = 0; ULARGE_INTEGER mx; IROTData *rd = NULL; SETUP();
    m = mr_class(&mr_CLS_CatMgr);
    if (m) {
        n = mr_save(m, buf, sizeof buf, &hr);
        CHECKF(hr == S_OK && n == 20 && !memcmp(buf, saved, 20), "save: hr=%08lx n=%lu", hr, n);
        EXPECT_EQ(IMoniker_GetSizeMax(m, &mx), S_OK); EXPECT_EQ(mx.QuadPart, 20);
        if (IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
            EXPECT_EQ(IROTData_GetComparisonData(rd, buf, sizeof buf, &len), S_OK);
            EXPECT_EQ(len, 32);
            if (len == 32) { CHECK(mr_guid_eq((GUID *)buf, &mr_CLSID_ClassMoniker)); CHECK(mr_guid_eq((GUID *)(buf + 16), &mr_CLS_CatMgr)); }
            IROTData_Release(rd);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_load_with_extra_data) {
    /* CLSID, DWORD byte count of extra data, extra data (UTF-16, with terminator) */
    IMoniker *m = NULL; BYTE in[64], out[64]; DWORD ext = 5 * sizeof(WCHAR); HRESULT hr; ULONG n, len = 0; ULARGE_INTEGER mx; IROTData *rd = NULL;
    BYTE cmp[128]; SETUP();
    EXPECT_EQ(U(CreateClassMoniker)(&mr_GUID_NULL, &m), S_OK);
    if (m) {
        memcpy(in, &mr_CLS_CatMgr, 16); memcpy(in + 16, &ext, 4); memcpy(in + 20, L"data", ext);
        EXPECT_EQ(mr_load_into(m, in, 20 + ext), S_OK);
        EXPECT_EQ(IMoniker_GetSizeMax(m, &mx), S_OK); EXPECT_EQ(mx.QuadPart, 30);
        CHECK_DN(m, L"clsid:0002E005-0000-0000-C000-000000000046data:");
        n = mr_save(m, out, sizeof out, &hr);
        CHECKF(hr == S_OK && n == 30 && !memcmp(out, in, 30), "resave: hr=%08lx n=%lu", hr, n);
        /* extra data never takes part in the comparison data */
        if (IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
            EXPECT_EQ(IROTData_GetComparisonData(rd, cmp, sizeof cmp, &len), S_OK);
            EXPECT_EQ(len, 32);
            IROTData_Release(rd);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_class_load_truncated_fails) {
    IMoniker *m = NULL; SETUP();
    EXPECT_EQ(U(CreateClassMoniker)(&mr_GUID_NULL, &m), S_OK);
    if (m) {
        BYTE in[20]; memcpy(in, &mr_CLS_CatMgr, 16); memset(in + 16, 0, 4);
        for (ULONG cut = 0; cut < 20; cut += 4) { HRESULT hr = mr_load_into(m, in, cut); CHECKF(FAILED(hr), "load of %lu bytes hr=%08lx", cut, hr); }
        EXPECT_EQ(mr_load_into(m, in, 20), S_OK);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_class_is_equal_and_hash) {
    IMoniker *a = mr_class(&mr_CLS_X), *b = mr_class(&mr_CLS_X), *c = mr_class(&mr_CLS_Y), *it = mr_item(L"!", L"x"); SETUP();
    if (a && b && c && it) {
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, c), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, it), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, NULL), E_INVALIDARG);
        EXPECT_EQ(mr_hash(a), mr_CLS_X.Data1);
        EXPECT_EQ(mr_hash(c), mr_CLS_Y.Data1);
    }
    mr_rel(a); mr_rel(b); mr_rel(c); mr_rel(it);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_running_and_time) {
    IMoniker *m = mr_class(&mr_CLS_CatMgr); IBindCtx *bc = mr_bc(); FILETIME ft; SETUP();
    if (m && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, NULL, NULL, NULL), E_NOTIMPL);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), E_NOTIMPL);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), MK_E_UNAVAILABLE);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_bind_to_registered_class) {
    /* CLSID_StdComponentCategoriesMgr is registered by every Windows install (comcat.dll) */
    IMoniker *m = mr_class(&mr_CLS_CatMgr); IBindCtx *bc = mr_bc(); IUnknown *u = NULL; SETUP();
    if (m && bc) {
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) { IUnknown_Release(u); u = NULL; }
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_class_bind_to_unregistered_class) {
    IMoniker *m = mr_class(&mr_CLS_X); IBindCtx *bc = mr_bc(); IUnknown *u = (IUnknown *)1; SETUP();
    if (m && bc) {
        /* Windows 10 reports ERROR_MOD_NOT_FOUND for an unregistered CLSID here (not REGDB_E_CLASSNOTREG) */
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
        EXPECT_EQ(u, NULL);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_inverse_reduce_enum) {
    IMoniker *m = mr_class(&mr_CLS_X), *inv = NULL, *r = NULL; IEnumMoniker *e; SETUP();
    if (m) {
        EXPECT_EQ(IMoniker_Inverse(m, &inv), S_OK); CHECK_TYPE(inv, MKSYS_ANTIMONIKER); mr_rel(inv);
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r), MK_S_REDUCED_TO_SELF); EXPECT_EQ(r, m); mr_rel(r);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, TRUE, &e), S_OK); EXPECT_EQ(e, NULL);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, FALSE, &e), S_OK); EXPECT_EQ(e, NULL);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_class_compose_with_anti) {
    IMoniker *m = mr_class(&mr_CLS_X), *a, *c, *r; SETUP();
    if (m) {
        a = mr_anti(1); r = (IMoniker *)1; EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); EXPECT_EQ(r, NULL); mr_rel(a);
        a = mr_anti(2); r = (IMoniker *)1; EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); EXPECT_EQ(r, NULL); mr_rel(a);
        c = mr_desc("CA1I1"); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, TRUE, &r), MK_E_NEEDGENERIC);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ITEMMONIKER); mr_rel(r); mr_rel(c);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_class_marshal_data) {
    IMoniker *m = mr_class(&mr_CLS_CatMgr); IStream *s; BYTE buf[128]; ULONG got = 0; LARGE_INTEGER z; z.QuadPart = 0;
    SETUP(); REQUIRE(U(CoMarshalInterface));
    if (m && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL); IStream_Read(s, buf, sizeof buf, &got);
        EXPECT_EQ(got, 68);
        if (got == 68) {
            EXPECT_EQ(mr_rd32(buf), 0x574f454d); EXPECT_EQ(mr_rd32(buf + 4), 4);
            CHECK(mr_guid_eq((GUID *)(buf + 24), &mr_CLSID_ClassMoniker));
            EXPECT_EQ(mr_rd32(buf + 44), 20);
            CHECK(mr_guid_eq((GUID *)(buf + 48), &mr_CLS_CatMgr));
        }
        IStream_Release(s);
    }
    mr_rel(m);
    MR_COM_END();
}

/* ====================================================================== objref moniker */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_objref_basic_properties) {
    mr_obj o; IMoniker *m = NULL, *inv = NULL; CLSID c; void *p; IMarshal *im = NULL; SETUP();
    mr_obj_init(&o);
    if (!t_is_wine()) EXPECT_EQ(U(CreateObjrefMoniker)(mr_obj_unk(&o), NULL), E_INVALIDARG);
    EXPECT_EQ(U(CreateObjrefMoniker)(mr_obj_unk(&o), &m), S_OK);
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK); CHECK(mr_guid_eq(&c, &mr_CLSID_ObjrefMoniker));
        CHECK_TYPE(m, MKSYS_OBJREFMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        EXPECT_EQ(mr_hash(m), (DWORD)(ULONG_PTR)mr_obj_unk(&o));
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_ObjrefMoniker, &p), S_OK); EXPECT_EQ(p, m); if (p) IMoniker_Release(m);
        p = (void *)1; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IROTData, &p), E_NOINTERFACE); EXPECT_EQ(p, NULL);
        if (IMoniker_QueryInterface(m, &mr_IID_IMarshal, (void **)&im) == S_OK) {
            DWORD sz = 0;
            EXPECT_EQ(IMarshal_GetUnmarshalClass(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, 0, &c), S_OK);
            CHECK(mr_guid_eq(&c, &mr_CLSID_ObjrefMoniker));
            EXPECT_EQ(IMarshal_GetMarshalSizeMax(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, 0, &sz), S_OK);
            CHECK(sz > 0);
            IMarshal_Release(im);
        } else CHECK(0);
        EXPECT_EQ(IMoniker_Inverse(m, &inv), S_OK); CHECK_TYPE(inv, MKSYS_ANTIMONIKER); mr_rel(inv);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_objref_display_name_is_base64_objref) {
    mr_obj o; IMoniker *m = NULL; IBindCtx *bc; WCHAR *s = NULL; HRESULT hr; SETUP();
    mr_obj_init(&o);
    bc = mr_bc();
    if (U(CreateObjrefMoniker)(mr_obj_unk(&o), &m) == S_OK && m && bc) {
        hr = IMoniker_GetDisplayName(m, bc, NULL, &s);
        EXPECT_EQ(hr, S_OK);
        if (hr == S_OK && s) {
            int len = lstrlenW(s), bad = 0;
            CHECK(len > 8 && !wcsncmp(s, L"objref:", 7) && s[len - 1] == L':');
            for (int i = 7; i < len - 1; i++) {
                WCHAR c = s[i];
                if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')) bad++;
            }
            EXPECT_EQ(bad, 0);
            /* the encoded OBJREF starts with "MEOW" (OBJREF_SIGNATURE), whose base64 prefix is "TUVPVw" */
            CHECK(len > 13 && !wcsncmp(s + 7, L"TUVPV", 5));
            U(CoTaskMemFree)(s);
        }
        IMoniker_Release(m);
    }
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_objref_bind_and_running) {
    mr_obj o; IMoniker *m = NULL, *nul = NULL; IBindCtx *bc; IUnknown *u = NULL; FILETIME ft; SETUP();
    mr_obj_init(&o);
    bc = mr_bc();
    if (U(CreateObjrefMoniker)(mr_obj_unk(&o), &m) == S_OK && m && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_OK);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), MK_E_UNAVAILABLE);
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) { IUnknown_Release(u); u = NULL; }
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) { IUnknown_Release(u); u = NULL; }
        IEnumMoniker *e = (IEnumMoniker *)1;
        EXPECT_EQ(IMoniker_Enum(m, TRUE, &e), S_OK); EXPECT_EQ(e, NULL);
        e = (IEnumMoniker *)1;
        EXPECT_EQ(IMoniker_Enum(m, FALSE, &e), S_OK); EXPECT_EQ(e, NULL);
    }
    if (U(CreateObjrefMoniker)(NULL, &nul) == S_OK && nul && bc) {
        u = (IUnknown *)1;
        EXPECT_EQ(IMoniker_BindToObject(nul, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_UNEXPECTED);
        EXPECT_EQ(IMoniker_BindToStorage(nul, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_UNEXPECTED);
    }
    mr_rel(m); mr_rel(nul); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_objref_marshal_and_equal) {
    mr_obj o; IMoniker *m = NULL, *u = NULL; IStream *s; LARGE_INTEGER z; z.QuadPart = 0; SETUP(); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface));
    mr_obj_init(&o);
    if (U(CreateObjrefMoniker)(mr_obj_unk(&o), &m) == S_OK && m && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
        EXPECT_EQ(U(CoUnmarshalInterface)(s, &mr_IID_IMoniker, (void **)&u), S_OK);
        if (u) { EXPECT_EQ(IMoniker_IsEqual(m, u), S_OK); IMoniker_Release(u); }
        IStream_Release(s);
    }
    mr_rel(m);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_objref_common_prefix) {
    mr_obj o; IMoniker *a = NULL, *b = NULL, *outer = NULL, *p = (IMoniker *)1; SETUP();
    mr_obj_init(&o);
    U(CreateObjrefMoniker)(mr_obj_unk(&o), &a); U(CreateObjrefMoniker)(mr_obj_unk(&o), &b);
    if (t_is_wine()) t_skip("Wine does not implement the objref moniker");
    else if (a && b && U(CreateObjrefMoniker)((IUnknown *)a, &outer) == S_OK && outer) {
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, NULL, &p), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, &p), MK_S_US); EXPECT_EQ(p, a); mr_rel(p);
        EXPECT_EQ(IMoniker_IsEqual(a, outer), S_FALSE);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, outer, &p), MK_E_NOPREFIX);
    }
    mr_rel(a); mr_rel(b); mr_rel(outer);
    MR_COM_END();
}

/* ====================================================================== MkParseDisplayName */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_invalid_arguments) {
    IBindCtx *bc; IMoniker *pmk; ULONG eaten; HRESULT hr; SETUP();
    bc = mr_bc();
    for (int mask = 0; mask < 16 && bc; mask++) {
        IBindCtx *b = (mask & 1) ? bc : NULL; const WCHAR *name = (mask & 2) ? L"" : NULL;
        ULONG *pe = (mask & 4) ? &eaten : NULL; IMoniker **pm = (mask & 8) ? &pmk : NULL;
        eaten = 0xdeadbeef; pmk = (IMoniker *)0xdeadbeef;
        hr = U(MkParseDisplayName)(b, name, pe, pm);
        CHECKF(hr == E_INVALIDARG, "mask %d: hr=%08lx", mask, hr);
        CHECKF(eaten == 0xdeadbeef && pmk == (IMoniker *)0xdeadbeef, "mask %d: outputs modified", mask);
    }
    /* valid name but NULL bind context / eaten / output */
    eaten = 0xdeadbeef; pmk = (IMoniker *)0xdeadbeef;
    EXPECT_EQ(U(MkParseDisplayName)(NULL, L"clsid:00000000-0000-0000-0000-000000000000:", &eaten, &pmk), E_INVALIDARG);
    EXPECT_EQ(U(MkParseDisplayName)(bc, L"clsid:00000000-0000-0000-0000-000000000000:", NULL, &pmk), E_INVALIDARG);
    EXPECT_EQ(U(MkParseDisplayName)(bc, L"clsid:00000000-0000-0000-0000-000000000000:", &eaten, NULL), E_INVALIDARG);
    EXPECT_EQ(pmk, (IMoniker *)0xdeadbeef);
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_unparseable_names) {
    static const WCHAR *names[] = { L"NonExistentProgId:", L"clsid:" };
    static const WCHAR *odd[] = { L"olet_mr_no_such_thing_xyz", L"olet_mr_no:such:thing", L"@olet_mr_NoSuchProgId", L"@", L"!!!", L"clsid:{" };
    IBindCtx *bc; SETUP();
    bc = mr_bc();
    for (unsigned i = 0; i < 6 && bc; i++) {
        IMoniker *m = (IMoniker *)1; ULONG eaten = 0xdeadbeef;
        HRESULT hr = U(MkParseDisplayName)(bc, odd[i], &eaten, &m);
        t_note("MkParseDisplayName(\"%ls\") -> %08lx eaten=%lu", odd[i], hr, eaten);
        if (SUCCEEDED(hr) && m && m != (IMoniker *)1) IMoniker_Release(m);
    }
    for (unsigned i = 0; i < 2 && bc; i++) {
        IMoniker *m = (IMoniker *)1; ULONG eaten = 0xdeadbeef;
        HRESULT hr = U(MkParseDisplayName)(bc, names[i], &eaten, &m);
        CHECKF(hr == MK_E_SYNTAX, "\"%ls\": hr=%08lx", names[i], hr);
        CHECKF(eaten == 0, "\"%ls\": eaten=%lu", names[i], eaten);
        CHECKF(m == NULL, "\"%ls\": moniker=%p", names[i], m);
        if (SUCCEEDED(hr) && m && m != (IMoniker *)1) IMoniker_Release(m);
    }
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_clsid_forms) {
    static const struct { const WCHAR *name; ULONG eaten; HRESULT hr; } t[] = {
        { L"clsid:11111111-0000-0000-2222-444444444444;extra data:", 54, S_OK },
        { L"clsid:11111111-0000-0000-2222-444444444444extra data", 52, S_OK },
        { L"clsid:11111111-0000-0000-2222-444444444444:", 43, S_OK },
        { L"clsid:11111111-0000-0000-2222-444444444444", 42, S_OK },
        { L"clsid:{11111111-0000-0000-2222-444444444444}", 44, S_OK },
        { L"clsid:{11111111-0000-0000-2222-444444444444", 0, MK_E_SYNTAX },
        { L"clsid:11111111-0000-0000-2222-444444444444}", 43, S_OK },
    };
    IBindCtx *bc; SETUP();
    bc = mr_bc();
    for (unsigned i = 0; i < 7 && bc; i++) {
        IMoniker *m = NULL; ULONG eaten = 0xdeadbeef;
        HRESULT hr = U(MkParseDisplayName)(bc, t[i].name, &eaten, &m);
        CHECKF(hr == t[i].hr, "%u: hr=%08lx want %08lx", i, hr, t[i].hr);
        CHECKF(eaten == t[i].eaten, "%u: eaten=%lu want %lu", i, eaten, t[i].eaten);
        if (SUCCEEDED(hr)) { CHECK_TYPE(m, MKSYS_CLASSMONIKER); if (m) { CHECKF(mr_hash(m) == 0x11111111, "%u hash", i); IMoniker_Release(m); } }
        else CHECKF(m == NULL, "%u: moniker not NULL on failure", i);
    }
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_clsid_extended_syntax_equality) {
    IBindCtx *bc; IMoniker *a = NULL, *b = NULL; ULONG ea, eb; ULARGE_INTEGER mx; SETUP();
    bc = mr_bc();
    if (bc && U(MkParseDisplayName)(bc, L"clsid:11111111-0000-0000-2222-444444444444;extra data:", &ea, &a) == S_OK &&
        U(MkParseDisplayName)(bc, L"clsid:11111111-0000-0000-2222-444444444444;different extra data:", &eb, &b) == S_OK) {
        CHECK_DN(a, L"clsid:11111111-0000-0000-2222-444444444444;extra data:");
        EXPECT_EQ(IMoniker_GetSizeMax(a, &mx), S_OK); EXPECT_EQ(mx.QuadPart, 44);
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);      /* only the CLSID takes part in equality */
        EXPECT_EQ(IMoniker_IsEqual(b, a), S_OK);
    } else CHECK(0);
    mr_rel(a); mr_rel(b); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_parse_clsid_case_and_braces_normalised) {
    IBindCtx *bc; IMoniker *a = NULL, *b = NULL; ULONG ea, eb; SETUP();
    bc = mr_bc();
    if (bc && U(MkParseDisplayName)(bc, L"CLSID:0002e005-0000-0000-c000-000000000046:", &ea, &a) == S_OK &&
        U(MkParseDisplayName)(bc, L"clsid:{0002E005-0000-0000-C000-000000000046}:", &eb, &b) == S_OK) {
        CHECK_DN(a, L"clsid:0002E005-0000-0000-C000-000000000046:");
        CHECK_DN(b, L"clsid:0002E005-0000-0000-C000-000000000046:");
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);
    } else t_note("clsid: prefix case / brace variants did not both parse");
    mr_rel(a); mr_rel(b); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_parse_clsid_binds_to_class) {
    IBindCtx *bc; IMoniker *m = NULL; ULONG eaten; IUnknown *u = NULL; SETUP();
    bc = mr_bc();
    if (bc && U(MkParseDisplayName)(bc, L"clsid:0002E005-0000-0000-C000-000000000046:", &eaten, &m) == S_OK && m) {
        EXPECT_EQ(eaten, 43);
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
    } else CHECK(0);
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_parse_existing_file) {
    char sys[MAX_PATH]; WCHAR wp[MAX_PATH]; IBindCtx *bc; IMoniker *m = NULL; ULONG eaten = 0; HRESULT hr; SETUP();
    GetSystemDirectoryA(sys, sizeof sys); strcat(sys, "\\kernel32.dll");
    MultiByteToWideChar(CP_ACP, 0, sys, -1, wp, MAX_PATH);
    bc = mr_bc();
    if (bc) {
        hr = U(MkParseDisplayName)(bc, wp, &eaten, &m);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(eaten, lstrlenW(wp));
        if (m) { CHECK_TYPE(m, MKSYS_FILEMONIKER); CHECK(1); IMoniker_Release(m); }
        IBindCtx_Release(bc);
    }
    MR_COM_END();
}

TEST(mr_parse_existing_temp_file_with_item) {
    char path[MAX_PATH]; WCHAR wp[MAX_PATH + 16]; HANDLE h; IBindCtx *bc; IMoniker *m = NULL; ULONG eaten = 0; HRESULT hr; SETUP();
    t_tmpfile(path, sizeof path, "parse_item.dat");
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no fixture"); MR_COM_END(); return; }
    CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    bc = mr_bc();
    if (bc) {
        hr = U(MkParseDisplayName)(bc, wp, &eaten, &m);
        EXPECT_EQ(hr, S_OK); EXPECT_EQ(eaten, lstrlenW(wp));
        if (m) { CHECK_TYPE(m, MKSYS_FILEMONIKER); IMoniker_Release(m); m = NULL; }
        lstrcatW(wp, L"!Item1");
        eaten = 0;
        hr = U(MkParseDisplayName)(bc, wp, &eaten, &m);
        t_note("MkParseDisplayName(file!Item1) -> %08lx eaten=%lu", hr, eaten);
        if (hr == S_OK && m) { CHECK_TYPE(m, MKSYS_GENERICCOMPOSITE); CHECK_DN(m, wp); EXPECT_EQ(eaten, lstrlenW(wp)); IMoniker_Release(m); }
        IBindCtx_Release(bc);
    }
    DeleteFileA(path);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_running_object_name_becomes_file_moniker) {
    /* a name that is not a file is still parsed as a file moniker when that moniker is in the ROT */
    WCHAR name[64]; IBindCtx *bc; IMoniker *reg, *m = NULL; IRunningObjectTable *rot = NULL; mr_obj o; DWORD ck = 0; ULONG eaten = 0; SETUP();
    mr_obj_init(&o);
    mr_unique_name(name, 64, L"RunningName");
    bc = mr_bc(); reg = mr_file(name);
    if (bc && reg && U(GetRunningObjectTable)(0, &rot) == S_OK) {
        IMoniker *pm = (IMoniker *)1; ULONG e0 = 99;
        { HRESULT h0 = U(MkParseDisplayName)(bc, name, &e0, &pm);   /* not yet running */
          CHECKF(FAILED(h0), "unregistered name parsed: %08lx", h0); t_note("parse of a non-running, non-file name -> %08lx", h0); }
        EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), reg, &ck), S_OK);
        EXPECT_EQ(U(MkParseDisplayName)(bc, name, &eaten, &m), S_OK);
        EXPECT_EQ(eaten, lstrlenW(name));
        if (m) { CHECK_TYPE(m, MKSYS_FILEMONIKER); IMoniker_Release(m); }
        IRunningObjectTable_Revoke(rot, ck);
        IRunningObjectTable_Release(rot);
    }
    mr_rel(reg); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_clsid_has_precedence_over_running_object) {
    WCHAR name[] = L"clsid:20D04FE0-3AEA-1069-A2D8-08002B30309D:"; IBindCtx *bc; IMoniker *reg, *m = NULL; IRunningObjectTable *rot = NULL; mr_obj o; DWORD ck = 0; ULONG eaten = 0; SETUP();
    mr_obj_init(&o);
    bc = mr_bc(); reg = mr_file(name);
    if (bc && reg && U(GetRunningObjectTable)(0, &rot) == S_OK) {
        EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), reg, &ck), S_OK);
        EXPECT_EQ(U(MkParseDisplayName)(bc, name, &eaten, &m), S_OK);
        EXPECT_EQ(eaten, lstrlenW(name));
        if (m) { CHECK_TYPE(m, MKSYS_CLASSMONIKER); IMoniker_Release(m); }
        IRunningObjectTable_Revoke(rot, ck);
        IRunningObjectTable_Release(rot);
    }
    mr_rel(reg); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* --- a class that implements IParseDisplayName, reached through a registered ProgID ------------------ */
static const WCHAR *g_expect_name;
typedef struct { const IParseDisplayNameVtbl *lpVtbl; } pdn_obj;
static HRESULT WINAPI pdn_QI(IParseDisplayName *i, REFIID r, void **pp) {
    if (mr_guid_eq(r, &mr_IID_IUnknown) || mr_guid_eq(r, &mr_IID_IParseDisplayName)) { *pp = i; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI pdn_AddRef(IParseDisplayName *i) { return 2; }
static ULONG WINAPI pdn_Release(IParseDisplayName *i) { return 1; }
static HRESULT WINAPI pdn_Parse(IParseDisplayName *i, IBindCtx *bc, LPOLESTR name, ULONG *eaten, IMoniker **out) {
    if (!g_expect_name || lstrcmpW(name, g_expect_name)) return E_INVALIDARG;
    *eaten = lstrlenW(name);
    return U(CreateAntiMoniker)(out);
}
static const IParseDisplayNameVtbl pdn_vtbl = { pdn_QI, pdn_AddRef, pdn_Release, pdn_Parse };
static pdn_obj g_pdn = { &pdn_vtbl };

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_parse_progid_through_registered_class_object) {
    /* "StdFont:" and "@StdFont" resolve the ProgID to CLSID_StdFont and ask its class object for IParseDisplayName */
    DWORD reg = 0; IBindCtx *bc; HRESULT hr; SETUP(); REQUIRE(U(CoRegisterClassObject)); REQUIRE(U(CoRevokeClassObject));
    hr = U(CoRegisterClassObject)(&mr_CLS_StdFont, (IUnknown *)&g_pdn, CLSCTX_INPROC_SERVER, REGCLS_MULTI_SEPARATE, &reg);
    EXPECT_EQ(hr, S_OK);
    bc = mr_bc();
    if (hr == S_OK && bc) {
        static const WCHAR *good[] = { L"StdFont:", L"@StdFont" };
        for (int i = 0; i < 2; i++) {
            IMoniker *m = NULL; ULONG eaten = 0;
            g_expect_name = good[i];
            hr = U(MkParseDisplayName)(bc, good[i], &eaten, &m);
            CHECKF(hr == S_OK, "\"%ls\": hr=%08lx", good[i], hr);
            EXPECT_EQ(eaten, lstrlenW(good[i]));
            if (m) { CHECK_TYPE(m, MKSYS_ANTIMONIKER); IMoniker_Release(m); }
        }
        { IMoniker *m = (IMoniker *)1; ULONG eaten = 99;
          g_expect_name = L"StdFont";
          hr = U(MkParseDisplayName)(bc, L"StdFont", &eaten, &m);       /* no colon, no leading '@', not a file */
          EXPECT_EQ(hr, MK_E_SYNTAX); EXPECT_EQ(eaten, 0); EXPECT_EQ(m, NULL); }
    }
    if (reg) U(CoRevokeClassObject)(reg);
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* ====================================================================== BindMoniker / CoGetObject */
TEST(mr_bindmoniker_pointer_moniker) {
    mr_obj o; IMoniker *m; IUnknown *u = (IUnknown *)1; SETUP(); REQUIRE(U(BindMoniker));
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o));
    if (m) {
        EXPECT_EQ(U(BindMoniker)(m, 0, &mr_IID_IUnknown, (void **)&u), S_OK);
        EXPECT_EQ(u, mr_obj_unk(&o));
        if (u) IUnknown_Release(u);
        u = (IUnknown *)1;
        EXPECT_EQ(U(BindMoniker)(m, 0, &mr_IID_IDispatch, (void **)&u), E_NOINTERFACE);
        EXPECT_EQ(u, NULL);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_bindmoniker_invalid_arguments) {
    mr_obj o; IMoniker *m; IUnknown *u = (IUnknown *)1; SETUP(); REQUIRE(U(BindMoniker));
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o));
    if (m && t_is_wine()) t_skip("Wine dereferences invalid BindMoniker arguments");
    else if (m) {
        EXPECT_EQ(U(BindMoniker)(m, 0, &mr_IID_IUnknown, NULL), E_INVALIDARG);
        EXPECT_EQ(U(BindMoniker)(NULL, 0, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
        EXPECT_EQ(u, NULL);                                   /* cleared before validation */
        u = (IUnknown *)1;
        EXPECT_EQ(U(BindMoniker)(m, 1, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);   /* grfOpt must be 0 */
        EXPECT_EQ(u, NULL);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_bindmoniker_class_and_item) {
    IMoniker *c, *it; IUnknown *u = NULL; HRESULT hr; SETUP(); REQUIRE(U(BindMoniker));
    c = mr_class(&mr_CLS_CatMgr);
    if (c) { EXPECT_EQ(U(BindMoniker)(c, 0, &mr_IID_IUnknown, (void **)&u), S_OK); if (u) { IUnknown_Release(u); u = NULL; } IMoniker_Release(c); }
    c = mr_class(&mr_CLS_X);
    if (c) { hr = U(BindMoniker)(c, 0, &mr_IID_IUnknown, (void **)&u);
        /* Windows 10: an unregistered CLSID bound through a class moniker reports ERROR_MOD_NOT_FOUND, not REGDB_E_CLASSNOTREG */
        t_note("BindMoniker(class moniker, unregistered CLSID) -> %08lx", (unsigned long)hr);
        EXPECT_EQ(hr, HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND));
        if (u) { IUnknown_Release(u); u = NULL; }
        IMoniker_Release(c); }
    it = mr_item(L"!", L"x");
    if (it) { u = (IUnknown *)1; hr = U(BindMoniker)(it, 0, &mr_IID_IUnknown, (void **)&u); EXPECT_EQ(hr, E_INVALIDARG); IMoniker_Release(it); }
    MR_COM_END();
}

TEST(mr_cogetobject_clsid_moniker) {
    IUnknown *u = NULL; HRESULT hr; SETUP(); REQUIRE(U(CoGetObject));
    hr = U(CoGetObject)(L"clsid:0002E005-0000-0000-C000-000000000046:", NULL, &mr_IID_IUnknown, (void **)&u);
    EXPECT_EQ(hr, S_OK);
    if (u) IUnknown_Release(u);
    u = (IUnknown *)1;
    hr = U(CoGetObject)(L"olet_mr_NoSuchThing:", NULL, &mr_IID_IUnknown, (void **)&u);
    CHECKF(FAILED(hr), "unparseable name hr=%08lx", hr);
    t_note("CoGetObject(\"olet_mr_NoSuchThing:\") -> %08lx", hr);
    MR_COM_END();
}

/* ====================================================================== misc */
TEST(mr_cocreate_system_monikers) {
    static const struct { const GUID *clsid; DWORD type; const char *n; } t[] = {
        { &mr_CLSID_FileMoniker, MKSYS_FILEMONIKER, "file" }, { &mr_CLSID_ItemMoniker, MKSYS_ITEMMONIKER, "item" },
        { &mr_CLSID_AntiMoniker, MKSYS_ANTIMONIKER, "anti" }, { &mr_CLSID_CompositeMoniker, MKSYS_GENERICCOMPOSITE, "composite" },
        { &mr_CLSID_ClassMoniker, MKSYS_CLASSMONIKER, "class" }, { &mr_CLSID_PointerMoniker, MKSYS_POINTERMONIKER, "pointer" },
        { &mr_CLSID_ObjrefMoniker, MKSYS_OBJREFMONIKER, "objref" } };
    SETUP(); REQUIRE(U(CoCreateInstance));
    for (unsigned i = 0; i < 7; i++) {
        IMoniker *m = NULL; CLSID c;
        HRESULT hr = U(CoCreateInstance)(t[i].clsid, NULL, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&m);
        CHECKF(hr == S_OK && m, "%s: hr=%08lx", t[i].n, hr);
        if (m) {
            EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK);
            CHECKF(mr_guid_eq(&c, t[i].clsid), "%s: class id", t[i].n);
            CHECK_TYPE(m, t[i].type);
            IMoniker_Release(m);
        }
    }
    MR_COM_END();
}

TEST(mr_cocreate_system_monikers_reject_aggregation) {
    static const GUID *ids[] = { &mr_CLSID_FileMoniker, &mr_CLSID_ItemMoniker, &mr_CLSID_AntiMoniker, &mr_CLSID_CompositeMoniker };
    mr_obj outer; SETUP(); REQUIRE(U(CoCreateInstance));
    mr_obj_init(&outer);
    for (unsigned i = 0; i < 4; i++) {
        IMoniker *m = (IMoniker *)1;
        HRESULT hr = U(CoCreateInstance)(ids[i], mr_obj_unk(&outer), CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&m);
        CHECKF(FAILED(hr), "%u: aggregated creation hr=%08lx", i, hr);
        t_note("aggregated %u -> %08lx", i, hr);
        if (SUCCEEDED(hr) && m) IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_bindctx_roundtrip_with_moniker_binding) {
    /* the bind context's object params used by monikers: "ExceededDeadline"/"ConnectManually" style keys are plain strings */
    IBindCtx *bc; mr_obj o; IMoniker *m; IUnknown *u = NULL; SETUP();
    mr_obj_init(&o);
    bc = mr_bc(); m = mr_ptr(mr_obj_unk(&o));
    if (bc && m) {
        EXPECT_EQ(IBindCtx_RegisterObjectBound(bc, mr_obj_unk(&o)), S_OK);
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
        IBindCtx_ReleaseBoundObjects(bc);
        EXPECT_EQ(o.ref, 2);                 /* only the pointer moniker's reference remains */
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    EXPECT_EQ(o.ref, 1);
    MR_COM_END();
}
