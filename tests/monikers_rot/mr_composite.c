/* Generic composite monikers: CreateGenericComposite, flattening/associativity, anti-moniker
 * simplification, enumeration, inverse, prefixes and persistence. Based on the Win10 decompilation of
 * CreateGenericComposite / CCompositeMoniker and on Windows-generated data in Wine's moniker.c. */
#include "mr.h"

#define SETUP() do { MR_NEED_BASE(); MR_COM_BEGIN(); } while (0)

static DWORD ihash(const char *s) { DWORD h = 0; for (; *s; s++) h = (h * 3) ^ (DWORD)(unsigned char)toupper((unsigned char)*s); return h; }

/* ------------------------------------------------------------ creation / simplification */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_simplify_table) {
    static const struct { const char *l, *r; DWORD type; const WCHAR *name; } t[] = {
        { "I1", "I2", MKSYS_GENERICCOMPOSITE, L"!I1!I2" },
        { "I1", "A2", MKSYS_ANTIMONIKER, L"\\.." },
        { "A1", "A1", MKSYS_GENERICCOMPOSITE, L"\\..\\.." },
        { "A2", "A1", MKSYS_GENERICCOMPOSITE, L"\\..\\..\\.." },
        { "CI1I2", "A1", MKSYS_ITEMMONIKER, L"!I1" },
        { "I1", "A1", MKSYS_NONE, NULL },
        { "CI1I2", "A2", MKSYS_NONE, NULL },
        { "CI1I2", "A3", MKSYS_ANTIMONIKER, L"\\.." },
        { "CI1I3", "CA1I2", MKSYS_GENERICCOMPOSITE, L"!I1!I2" },
    };
    SETUP();
    for (unsigned i = 0; i < 9; i++) {
        IMoniker *l = mr_desc(t[i].l), *r = mr_desc(t[i].r), *c = (IMoniker *)1;
        if (l && r) {
            HRESULT hr = U(CreateGenericComposite)(l, r, &c);
            CHECKF(hr == S_OK, "%u: hr=%08lx", i, hr);
            if (t[i].type == MKSYS_NONE) CHECKF(c == NULL, "%u: expected NULL moniker", i);
            else if (c && c != (IMoniker *)1) { CHECK_TYPE(c, t[i].type); CHECK_DN(c, t[i].name); }
            if (c && c != (IMoniker *)1) IMoniker_Release(c);
        }
        mr_rel(l); mr_rel(r);
    }
    MR_COM_END();
}

TEST(mr_comp_null_operands) {
    IMoniker *i, *c = (IMoniker *)1; SETUP();
    EXPECT_EQ(U(CreateGenericComposite)(NULL, NULL, &c), S_OK);
    EXPECT_EQ(c, NULL);
    i = mr_item(L"!", L"x");
    if (i) {
        c = NULL;
        EXPECT_EQ(U(CreateGenericComposite)(i, NULL, &c), S_OK);
        EXPECT_EQ(c, i);                 /* the very same object, AddRef'd */
        EXPECT_EQ(IMoniker_Release(i), 1);
        c = NULL;
        EXPECT_EQ(U(CreateGenericComposite)(NULL, i, &c), S_OK);
        EXPECT_EQ(c, i);
        EXPECT_EQ(IMoniker_Release(i), 1);
        IMoniker_Release(i);
    }
    MR_COM_END();
}

TEST(mr_comp_refcounts) {
    IMoniker *a, *b, *c = NULL; SETUP();
    a = mr_item(L"!", L"A"); b = mr_item(L"!", L"B");
    if (a && b) {
        EXPECT_EQ(U(CreateGenericComposite)(a, b, &c), S_OK);
        EXPECT_EQ(IMoniker_AddRef(a), 3);       /* the composite holds one reference */
        EXPECT_EQ(IMoniker_Release(a), 2);
        if (c) { EXPECT_EQ(IMoniker_Release(c), 0); }
        EXPECT_EQ(IMoniker_AddRef(a), 2);       /* released again with the composite */
        IMoniker_Release(a);
        IMoniker_Release(a); IMoniker_Release(b);
        a = b = NULL;
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_comp_basic_properties) {
    IMoniker *a, *b, *c; CLSID cl; void *p; SETUP();
    a = mr_item(L"!", L"Test"); b = mr_item(L"#", L"Wine"); c = a && b ? mr_comp(a, b) : NULL;
    if (c) {
        EXPECT_EQ(IMoniker_GetClassID(c, &cl), S_OK); CHECK(mr_guid_eq(&cl, &mr_CLSID_CompositeMoniker));
        CHECK_TYPE(c, MKSYS_GENERICCOMPOSITE);
        EXPECT_EQ(IMoniker_IsDirty(c), S_FALSE);
        CHECK_DN(c, L"!Test#Wine");
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(c, &mr_IID_IROTData, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(c, &mr_IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(c, &mr_IID_IPersistStream, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(c, &mr_IID_IPersist, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(c, &mr_CLSID_CompositeMoniker, &p), S_OK); EXPECT_EQ(p, c); if (p) IMoniker_Release(c);
        IMoniker_Release(c);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_hash_is_xor_of_components) {
    IMoniker *a, *b, *c; SETUP();
    a = mr_item(L"!", L"Test"); b = mr_item(L"#", L"Wine"); c = a && b ? mr_comp(a, b) : NULL;
    if (c) {
        EXPECT_EQ(mr_hash(c), 0xd87);
        EXPECT_EQ(mr_hash(c), ihash("Test") ^ ihash("Wine"));
        IMoniker_Release(c);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_comp_flattening_and_associativity) {
    /* (I1,(I2,I3)) and ((I1,I2),I3) are the same moniker: same components, equal, same hash/name */
    IMoniker *l = mr_desc("CI1CI2I3"), *r = mr_desc("CCI1I2I3"), *s = mr_desc("CI1I2"); SETUP();
    if (l && r) {
        CHECK_DN(l, L"!I1!I2!I3"); CHECK_DN(r, L"!I1!I2!I3");
        EXPECT_EQ(IMoniker_IsEqual(l, r), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(r, l), S_OK);
        EXPECT_EQ(mr_hash(l), mr_hash(r));
        if (s) EXPECT_EQ(IMoniker_IsEqual(l, s), S_FALSE);
        for (int k = 0; k < 2; k++) {
            IEnumMoniker *e = NULL; int n = 0; IMoniker *x;
            EXPECT_EQ(IMoniker_Enum(k ? r : l, TRUE, &e), S_OK);
            while (e && IEnumMoniker_Next(e, 1, &x, NULL) == S_OK) { n++; CHECK_TYPE(x, MKSYS_ITEMMONIKER); IMoniker_Release(x); }
            EXPECT_EQ(n, 3);
            if (e) IEnumMoniker_Release(e);
        }
    }
    mr_rel(l); mr_rel(r); mr_rel(s);
    MR_COM_END();
}

TEST(mr_comp_deep_nesting) {
    IMoniker *acc = NULL; WCHAR want[400]; SETUP();
    want[0] = 0;
    for (int i = 0; i < 40; i++) {
        WCHAR n[8]; IMoniker *it, *c = NULL;
        wsprintfW(n, L"n%d", i);
        it = mr_item(L"!", n);
        if (!it) break;
        lstrcatW(want, L"!"); lstrcatW(want, n);
        if (!acc) acc = it;
        else { c = mr_comp(acc, it); IMoniker_Release(acc); IMoniker_Release(it); acc = c; if (!acc) break; }
    }
    if (acc) { CHECK_DN(acc, want); CHECK_TYPE(acc, MKSYS_GENERICCOMPOSITE); IMoniker_Release(acc); }
    MR_COM_END();
}

/* ------------------------------------------------------------ persistence */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_save_layout) {
    /* DWORD count, then for every flattened component its CLSID followed by its persisted data */
    IMoniker *a, *b, *c; BYTE buf[256], ia[64], ib[64]; HRESULT hr; ULONG n, na, nb; SETUP();
    a = mr_item(L"!", L"Test"); b = mr_item(L"#", L"Wine"); c = a && b ? mr_comp(a, b) : NULL;
    if (c) {
        na = mr_save(a, ia, sizeof ia, &hr); nb = mr_save(b, ib, sizeof ib, &hr);
        n = mr_save(c, buf, sizeof buf, &hr);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(n, 4 + 16 + na + 16 + nb);
        if (n == 4 + 16 + na + 16 + nb) {
            EXPECT_EQ(mr_rd32(buf), 2);
            CHECK(mr_guid_eq((GUID *)(buf + 4), &mr_CLSID_ItemMoniker));
            CHECK(!memcmp(buf + 20, ia, na));
            CHECK(mr_guid_eq((GUID *)(buf + 20 + na), &mr_CLSID_ItemMoniker));
            CHECK(!memcmp(buf + 36 + na, ib, nb));
        }
        IMoniker_Release(c);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_comp_comparison_data) {
    /* CLSID_CompositeMoniker, then the comparison data of every component */
    IMoniker *a, *b, *c; IROTData *rd = NULL; BYTE buf[512]; ULONG len = 0; SETUP();
    a = mr_item(L"!", L"Test"); b = mr_item(L"#", L"Wine"); c = a && b ? mr_comp(a, b) : NULL;
    if (c && IMoniker_QueryInterface(c, &mr_IID_IROTData, (void **)&rd) == S_OK) {
        EXPECT_EQ(IROTData_GetComparisonData(rd, buf, sizeof buf, &len), S_OK);
        EXPECT_EQ(len, 16 + (16 + 12) + (16 + 12));
        if (len == 72) {
            static const WCHAR t1[] = L"!TEST", t2[] = L"#WINE";
            CHECK(mr_guid_eq((GUID *)buf, &mr_CLSID_CompositeMoniker));
            CHECK(mr_guid_eq((GUID *)(buf + 16), &mr_CLSID_ItemMoniker));
            CHECK(!memcmp(buf + 32, t1, sizeof t1));
            CHECK(mr_guid_eq((GUID *)(buf + 44), &mr_CLSID_ItemMoniker));
            CHECK(!memcmp(buf + 60, t2, sizeof t2));
        }
        IROTData_Release(rd);
    } else CHECK(0);
    mr_rel(c); mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_comp_comparison_data_with_pointer_component_fails) {
    IROTData *rd = NULL; BYTE buf[256]; ULONG len = 7; HRESULT hr; SETUP();
    {
        mr_obj o; IMoniker *p, *i = mr_item(L"!", L"I1"), *comp;
        mr_obj_init(&o); p = mr_ptr(mr_obj_unk(&o)); comp = p && i ? mr_comp(p, i) : NULL;
        if (comp && IMoniker_QueryInterface(comp, &mr_IID_IROTData, (void **)&rd) == S_OK) {
            hr = IROTData_GetComparisonData(rd, buf, sizeof buf, &len);
            CHECKF(FAILED(hr), "hr=%08lx", hr);
            t_note("composite with pointer component: GetComparisonData -> %08lx len=%lu", hr, len);
            IROTData_Release(rd);
        }
        mr_rel(comp); mr_rel(p); mr_rel(i);
    }
    MR_COM_END();
}

TEST(mr_comp_save_load_roundtrip) {
    IMoniker *c, *empty = NULL; BYTE buf[512]; HRESULT hr; ULONG n; SETUP(); REQUIRE(U(CoCreateInstance));
    c = mr_desc("CI1CI2I3");
    hr = U(CoCreateInstance)(&mr_CLSID_CompositeMoniker, NULL, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&empty);
    EXPECT_EQ(hr, S_OK);
    if (c && empty) {
        n = mr_save(c, buf, sizeof buf, &hr);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(mr_load_into(empty, buf, n), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(c, empty), S_OK);
        CHECK_DN(empty, L"!I1!I2!I3");
        CHECK_TYPE(empty, MKSYS_GENERICCOMPOSITE);
    }
    mr_rel(c); mr_rel(empty);
    MR_COM_END();
}

TEST(mr_comp_load_garbage_fails) {
    IMoniker *empty = NULL; static const BYTE one[] = { 1,0,0,0 }; static const BYTE huge[] = { 0xff,0xff,0xff,0x7f };
    HRESULT hr; SETUP(); REQUIRE(U(CoCreateInstance));
    hr = U(CoCreateInstance)(&mr_CLSID_CompositeMoniker, NULL, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&empty);
    if (hr == S_OK && empty) {
        hr = mr_load_into(empty, (const BYTE *)"", 0); CHECKF(FAILED(hr), "empty stream hr=%08lx", hr);
        hr = mr_load_into(empty, one, 4); CHECKF(FAILED(hr), "truncated stream hr=%08lx", hr);
        hr = mr_load_into(empty, huge, 4); CHECKF(FAILED(hr), "absurd count hr=%08lx", hr);
        IMoniker_Release(empty);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_uninitialized_instance) {
    IMoniker *m = NULL; DWORD h = 0xdeadbeef; HRESULT hr; SETUP(); REQUIRE(U(CoCreateInstance));
    hr = U(CoCreateInstance)(&mr_CLSID_CompositeMoniker, NULL, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&m);
    EXPECT_EQ(hr, S_OK);
    if (m) {
        EXPECT_EQ(IMoniker_Hash(m, &h), E_UNEXPECTED);
        EXPECT_EQ(h, 0xdeadbeef);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_comp_marshal_roundtrip) {
    IMoniker *c, *u = NULL; IStream *s; LARGE_INTEGER z; BYTE buf[512]; ULONG got = 0; z.QuadPart = 0;
    SETUP(); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface));
    c = mr_desc("CI1I2");
    if (c && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)c, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL); IStream_Read(s, buf, sizeof buf, &got);
        CHECK(got > 48);
        if (got > 48) { EXPECT_EQ(mr_rd32(buf), 0x574f454d); CHECK(mr_guid_eq((GUID *)(buf + 24), &mr_CLSID_CompositeMoniker)); }
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
        EXPECT_EQ(U(CoUnmarshalInterface)(s, &mr_IID_IMoniker, (void **)&u), S_OK);
        if (u) { EXPECT_EQ(IMoniker_IsEqual(c, u), S_OK); CHECK_DN(u, L"!I1!I2"); IMoniker_Release(u); }
        IStream_Release(s);
    }
    mr_rel(c);
    MR_COM_END();
}

TEST(mr_comp_marshal_across_apartments) {
    IMoniker *c; struct { IStream *s; HRESULT hr; HRESULT eq; HRESULT dn; } r;
    void run(void *p) {
        typeof(r) *q = p; IMoniker *u = NULL; IBindCtx *bc = NULL; WCHAR *s = NULL;
        if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) return;
        q->hr = U(CoGetInterfaceAndReleaseStream)(q->s, &mr_IID_IMoniker, (void **)&u);
        if (u && U(CreateBindCtx)(0, &bc) == S_OK) {
            q->dn = IMoniker_GetDisplayName(u, bc, NULL, &s);
            if (s && lstrcmpW(s, L"!I1!I2")) q->dn = E_FAIL;
            if (s) U(CoTaskMemFree)(s);
            IBindCtx_Release(bc);
        }
        if (u) IMoniker_Release(u);
        U(CoUninitialize)();
    }
    SETUP(); REQUIRE(U(CoMarshalInterThreadInterfaceInStream)); REQUIRE(U(CoGetInterfaceAndReleaseStream));
    memset(&r, 0, sizeof r); r.hr = r.dn = E_FAIL;
    c = mr_desc("CI1I2");
    if (c && U(CoMarshalInterThreadInterfaceInStream)(&mr_IID_IMoniker, (IUnknown *)c, &r.s) == S_OK) {
        if (mr_run_thread(run, &r, 10000)) { EXPECT_EQ(r.hr, S_OK); EXPECT_EQ(r.dn, S_OK); }
    }
    mr_rel(c);
    MR_COM_END();
}

/* ------------------------------------------------------------ running / binding / time */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_is_running_and_bind_errors) {
    IMoniker *a, *b, *c; IBindCtx *bc; IUnknown *u = (IUnknown *)1; FILETIME ft; SETUP();
    a = mr_item(L"!", L"Test"); b = mr_item(L"#", L"Wine"); c = a && b ? mr_comp(a, b) : NULL; bc = mr_bc();
    if (c && bc) {
        EXPECT_EQ(IMoniker_IsRunning(c, NULL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_IsRunning(c, bc, NULL, NULL), S_FALSE);
        EXPECT_EQ(IMoniker_IsRunning(c, bc, NULL, c), S_OK);
        EXPECT_EQ(IMoniker_IsRunning(c, bc, a, c), S_FALSE);
        EXPECT_EQ(IMoniker_IsRunning(c, NULL, a, c), E_INVALIDARG);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(c, bc, NULL, &ft), MK_E_NOTBINDABLE);
        EXPECT_EQ(IMoniker_BindToStorage(c, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
        EXPECT_EQ(IMoniker_BindToObject(c, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
    }
    mr_rel(c); mr_rel(a); mr_rel(b); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_time_of_last_change_argument_checks) {
    IMoniker *c, *left; IBindCtx *bc; FILETIME ft; SETUP();
    c = mr_desc("CI1I2"); left = mr_desc("I1"); bc = mr_bc();
    if (c && left && bc) {
        EXPECT_EQ(IMoniker_ComposeWith(left, c, TRUE, &(IMoniker *){NULL}), MK_E_NEEDGENERIC);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(c, bc, left, &ft), MK_E_NOTBINDABLE);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(c, bc, NULL, &ft), MK_E_NOTBINDABLE);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(c, bc, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(c, NULL, NULL, &ft), E_INVALIDARG);
    }
    mr_rel(c); mr_rel(left); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_comp_rot_registration_binds) {
    /* a composite registered in the ROT is found by IsRunning on an equal composite and by BindToObject */
    IMoniker *c1, *c2; IBindCtx *bc; IRunningObjectTable *rot = NULL; mr_obj o; DWORD ck = 0; IUnknown *u = NULL; SETUP();
    c1 = mr_desc("CI1I2"); c2 = mr_desc("CI1I2"); bc = mr_bc(); mr_obj_init(&o);
    if (c1 && c2 && bc && U(GetRunningObjectTable)(0, &rot) == S_OK) {
        EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), c1, &ck), S_OK);
        EXPECT_EQ(IMoniker_IsRunning(c2, bc, NULL, NULL), S_OK);
        EXPECT_EQ(IMoniker_BindToObject(c2, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
        IRunningObjectTable_Revoke(rot, ck);
        EXPECT_EQ(IMoniker_IsRunning(c2, bc, NULL, NULL), S_FALSE);
        IRunningObjectTable_Release(rot);
    }
    mr_rel(c1); mr_rel(c2); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* ------------------------------------------------------------ inverse / reduce / enum */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_inverse) {
    static const struct { const char *d; HRESULT hr; DWORD type; const WCHAR *name; } t[] = {
        { "CI1I2", S_OK, MKSYS_GENERICCOMPOSITE, L"\\..\\.." },
        { "CA1A2", MK_E_NOINVERSE, MKSYS_NONE, NULL },
        { "CI1A2", MK_E_NOINVERSE, MKSYS_NONE, NULL },
    };
    SETUP();
    for (unsigned i = 0; i < 3; i++) {
        IMoniker *m = mr_desc(t[i].d), *inv = (IMoniker *)1;
        if (!m) continue;
        HRESULT hr = IMoniker_Inverse(m, &inv);
        CHECKF(hr == t[i].hr, "%s: hr=%08lx", t[i].d, hr);
        if (t[i].type == MKSYS_NONE) CHECKF(inv == NULL, "%s: inverse not NULL", t[i].d);
        else if (inv && inv != (IMoniker *)1) { CHECK_TYPE(inv, t[i].type); CHECK_DN(inv, t[i].name); }
        if (inv && inv != (IMoniker *)1) IMoniker_Release(inv);
        mr_rel(m);
    }
    { IMoniker *m = mr_desc("CI1I2"); if (m) { if (!t_is_wine()) EXPECT_EQ(IMoniker_Inverse(m, NULL), E_INVALIDARG); IMoniker_Release(m); } }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_reduce) {
    IMoniker *m, *r = (IMoniker *)1; IBindCtx *bc; SETUP();
    m = mr_desc("CI1I2"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Reduce(m, bc, MKRREDUCE_ALL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r), E_INVALIDARG);
        r = NULL;
        EXPECT_EQ(IMoniker_Reduce(m, bc, MKRREDUCE_ALL, NULL, &r), MK_S_REDUCED_TO_SELF);
        EXPECT_EQ(r, m);
        mr_rel(r);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_enum_forward_and_reverse) {
    static const WCHAR *fwd[] = { L"!I1", L"!I2", L"!I3" };
    IMoniker *m, *x; SETUP();
    m = mr_desc("CI1CI2I3");
    if (m) {
        for (int dir = 0; dir < 2; dir++) {
            IEnumMoniker *e = NULL; ULONG len;
            EXPECT_EQ(IMoniker_Enum(m, dir == 0, &e), S_OK);       /* TRUE: forward, FALSE: reverse */
            if (!e) continue;
            EXPECT_EQ(IEnumMoniker_Next(e, 0, NULL, NULL), E_INVALIDARG);
            x = (IMoniker *)1; EXPECT_EQ(IEnumMoniker_Next(e, 0, &x, NULL), S_OK);
            len = 1; EXPECT_EQ(IEnumMoniker_Next(e, 0, &x, &len), S_OK); EXPECT_EQ(len, 0);
            EXPECT_EQ(IEnumMoniker_Skip(e, 0), S_OK);
            for (int i = 0; i < 3; i++) {
                len = 0; x = NULL;
                EXPECT_EQ(IEnumMoniker_Next(e, 1, &x, &len), S_OK);
                EXPECT_EQ(len, 1);
                CHECK_DN(x, fwd[dir == 0 ? i : 2 - i]);
                mr_rel(x);
            }
            len = 1; EXPECT_EQ(IEnumMoniker_Next(e, 1, &x, &len), S_FALSE); EXPECT_EQ(len, 0);
            EXPECT_EQ(IEnumMoniker_Skip(e, 0), S_OK);
            EXPECT_EQ(IEnumMoniker_Skip(e, 1), S_FALSE);
            EXPECT_EQ(IEnumMoniker_Reset(e), S_OK);
            EXPECT_EQ(IEnumMoniker_Skip(e, 2), S_OK);
            len = 0; x = NULL;
            EXPECT_EQ(IEnumMoniker_Next(e, 1, &x, &len), S_OK);
            CHECK_DN(x, fwd[dir == 0 ? 2 : 0]);
            mr_rel(x);
            IEnumMoniker_Release(e);
        }
        EXPECT_EQ(IMoniker_Enum(m, FALSE, NULL), E_INVALIDARG);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_comp_enum_clone_not_implemented) {
    IMoniker *m; IEnumMoniker *e = NULL, *e2 = (IEnumMoniker *)1; SETUP();
    m = mr_desc("CI1I2");
    if (m && IMoniker_Enum(m, TRUE, &e) == S_OK && e) {
        EXPECT_EQ(IEnumMoniker_Clone(e, NULL), E_INVALIDARG);
        EXPECT_EQ(IEnumMoniker_Clone(e, &e2), E_NOTIMPL);
        EXPECT_EQ(e2, NULL);
        IEnumMoniker_Release(e);
    }
    mr_rel(m);
    MR_COM_END();
}

TEST(mr_comp_enum_multi_fetch) {
    IMoniker *m, *got[8]; IEnumMoniker *e = NULL; ULONG f = 0; HRESULT hr; SETUP();
    m = mr_desc("CCI1I2CI3I4");
    if (m && IMoniker_Enum(m, TRUE, &e) == S_OK && e) {
        hr = IEnumMoniker_Next(e, 3, got, &f);
        EXPECT_EQ(hr, S_OK); EXPECT_EQ(f, 3);
        if (f == 3) { CHECK_DN(got[0], L"!I1"); CHECK_DN(got[1], L"!I2"); CHECK_DN(got[2], L"!I3"); }
        for (ULONG i = 0; i < f; i++) mr_rel(got[i]);
        hr = IEnumMoniker_Next(e, 3, got, &f);
        EXPECT_EQ(hr, S_FALSE); EXPECT_EQ(f, 1);
        if (f == 1) { CHECK_DN(got[0], L"!I4"); mr_rel(got[0]); }
        IEnumMoniker_Release(e);
    }
    mr_rel(m);
    MR_COM_END();
}

/* ------------------------------------------------------------ equality / compose / prefix */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_is_equal) {
    IMoniker *a = mr_desc("CI1I2"), *b = mr_desc("CI1I2"), *c = mr_desc("CI1I3"), *i = mr_desc("I1"); SETUP();
    if (a && b && c && i) {
        EXPECT_EQ(IMoniker_IsEqual(a, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_IsEqual(a, a), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, c), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, i), S_FALSE);
    }
    mr_rel(a); mr_rel(b); mr_rel(c); mr_rel(i);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_compose_with) {
    IMoniker *m = mr_desc("CI1I2"), *i3 = mr_desc("I3"), *r = (IMoniker *)1; SETUP();
    if (m && i3) {
        EXPECT_EQ(IMoniker_ComposeWith(m, NULL, FALSE, &r), S_OK);
        EXPECT_EQ(r, m); mr_rel(r);
        r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, NULL, TRUE, &r), MK_E_NEEDGENERIC);
        EXPECT_EQ(r, NULL);
        r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, i3, TRUE, &r), MK_E_NEEDGENERIC);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, i3, FALSE, &r), S_OK);
        CHECK_DN(r, L"!I1!I2!I3");
        mr_rel(r);
    }
    mr_rel(m); mr_rel(i3);
    MR_COM_END();
}

TEST(mr_comp_compose_with_anti_cancels_last_component) {
    IMoniker *m = mr_desc("CI1I2"), *a1 = mr_anti(1), *a2 = mr_anti(2), *a3 = mr_anti(3), *r = NULL; SETUP();
    if (m && a1 && a2 && a3) {
        EXPECT_EQ(IMoniker_ComposeWith(m, a1, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ITEMMONIKER); CHECK_DN(r, L"!I1"); mr_rel(r); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, a2, FALSE, &r), S_OK); EXPECT_EQ(r, NULL);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, a3, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ANTIMONIKER); mr_rel(r);
    }
    mr_rel(m); mr_rel(a1); mr_rel(a2); mr_rel(a3);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_common_prefix) {
    IMoniker *m = mr_desc("CI1I2"), *m1 = mr_desc("CI1I2"), *i1 = mr_desc("I1"), *i2 = mr_desc("I2"), *p = (IMoniker *)1; SETUP();
    if (m && m1 && i1 && i2) {
        EXPECT_EQ(IMoniker_CommonPrefixWith(m, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(m, m1, NULL), E_INVALIDARG);
        p = (IMoniker *)1; EXPECT_EQ(IMoniker_CommonPrefixWith(m, NULL, &p), E_INVALIDARG); EXPECT_EQ(p, NULL);
        EXPECT_EQ(IMoniker_CommonPrefixWith(m, m, &p), MK_S_US);
        if (p) { EXPECT_NE(p, m); CHECK(mr_is_equal(p, m)); mr_rel(p); }
        EXPECT_EQ(IMoniker_CommonPrefixWith(m, m1, &p), MK_S_US);
        if (p) { CHECK(p != m && p != m1); mr_rel(p); }
        p = (IMoniker *)1; EXPECT_EQ(IMoniker_CommonPrefixWith(m, i2, &p), MK_E_NOPREFIX);
        EXPECT_EQ(IMoniker_CommonPrefixWith(m, i1, &p), MK_S_HIM);
        if (p) { CHECK(mr_is_equal(p, i1)); mr_rel(p); }
        EXPECT_EQ(IMoniker_CommonPrefixWith(i1, m, &p), MK_S_ME);
        if (p) { CHECK(mr_is_equal(p, i1)); mr_rel(p); }
    }
    mr_rel(m); mr_rel(m1); mr_rel(i1); mr_rel(i2);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_comp_common_prefix_partial_overlap) {
    /* (I1,(I2,I3)) x ((I1,I2),I4) -> (I1,I2) */
    IMoniker *a = mr_desc("CI1CI2I3"), *b = mr_desc("CCI1I2I4"), *p = NULL; SETUP();
    if (a && b) {
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, &p), S_OK);
        CHECK_TYPE(p, MKSYS_GENERICCOMPOSITE); CHECK_DN(p, L"!I1!I2");
        mr_rel(p);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

/* ------------------------------------------------------------ top-level helpers */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_func_moniker_common_prefix_with) {
    IMoniker *m = (IMoniker *)1, *item, *f1, *f2, *c1, *c2; SETUP(); REQUIRE(U(MonikerCommonPrefixWith));
    if (t_is_wine()) { t_skip("Wine crashes in MonikerCommonPrefixWith for non-composite monikers"); MR_COM_END(); return; }
    /* two non-composites never have a prefix through this entry point */
    if (!t_is_wine()) { EXPECT_EQ(U(MonikerCommonPrefixWith)(NULL, NULL, &m), MK_E_NOPREFIX); EXPECT_EQ(m, NULL); }
    item = mr_item(L"!", L"Item"); f1 = mr_file(L"C:\\test.txt"); f2 = mr_file(L"C:\\a\\test.txt");
    if (item && f1 && f2) {
        if (!t_is_wine()) {
        m = (IMoniker *)1; EXPECT_EQ(U(MonikerCommonPrefixWith)(item, NULL, &m), MK_E_NOPREFIX); EXPECT_EQ(m, NULL);
        m = (IMoniker *)1; EXPECT_EQ(U(MonikerCommonPrefixWith)(NULL, item, &m), MK_E_NOPREFIX); EXPECT_EQ(m, NULL); }
        EXPECT_EQ(U(MonikerCommonPrefixWith)(item, item, &m), MK_E_NOPREFIX);
        EXPECT_EQ(U(MonikerCommonPrefixWith)(f1, f1, &m), MK_E_NOPREFIX);
        EXPECT_EQ(U(MonikerCommonPrefixWith)(f1, f2, &m), MK_E_NOPREFIX);
        c1 = mr_comp(f1, item); c2 = mr_comp(f2, item);
        if (c1 && c2) {
            EXPECT_EQ(U(MonikerCommonPrefixWith)(f1, c1, &m), MK_S_ME);    /* F x (F,I) */
            EXPECT_EQ(m, f1); mr_rel(m);
            EXPECT_EQ(U(MonikerCommonPrefixWith)(f1, c2, &m), S_OK);       /* F1 x (F2,I) -> "C:\" */
            CHECK_TYPE(m, MKSYS_FILEMONIKER); CHECK_DN(m, L"C:\\"); mr_rel(m);
            EXPECT_EQ(U(MonikerCommonPrefixWith)(c2, f1, &m), S_OK);
            CHECK_TYPE(m, MKSYS_FILEMONIKER); CHECK_DN(m, L"C:\\"); mr_rel(m);
            EXPECT_EQ(U(MonikerCommonPrefixWith)(c1, f1, &m), MK_S_HIM);   /* (F,I) x F */
            EXPECT_EQ(m, f1); mr_rel(m);
            EXPECT_EQ(U(MonikerCommonPrefixWith)(c1, c1, &m), MK_S_US);
            CHECK_TYPE(m, MKSYS_GENERICCOMPOSITE); CHECK_DN(m, L"C:\\test.txt!Item");
            EXPECT_NE(m, c1); mr_rel(m);
            m = (IMoniker *)1;
            EXPECT_EQ(U(MonikerCommonPrefixWith)(c1, c2, &m), S_OK);       /* (F1,I) x (F2,I) -> () */
            EXPECT_EQ(m, NULL);
        }
        mr_rel(c1); mr_rel(c2);
    }
    mr_rel(item); mr_rel(f1); mr_rel(f2);
    MR_COM_END();
}

TEST(mr_func_moniker_relative_path_to) {
    IMoniker *a, *b, *r = (IMoniker *)1; SETUP(); REQUIRE(U(MonikerRelativePathTo));
    if (t_is_wine()) { t_skip("Wine aborts on the unimplemented MonikerRelativePathTo"); MR_COM_END(); return; }
    a = mr_item(L"!", L"A"); b = mr_item(L"!", L"B");
    if (a && b) {
        /* two non-composites: the relative path is the target itself, reported as MK_S_HIM (decomp) */
        EXPECT_EQ(U(MonikerRelativePathTo)(a, b, &r, TRUE), MK_S_HIM);
        EXPECT_EQ(r, b); mr_rel(r);
        EXPECT_EQ(U(MonikerRelativePathTo)(a, b, NULL, TRUE), E_INVALIDARG);
        r = (IMoniker *)1;
        EXPECT_EQ(U(MonikerRelativePathTo)(NULL, b, &r, TRUE), E_INVALIDARG);
        EXPECT_EQ(r, NULL);
        EXPECT_EQ(U(MonikerRelativePathTo)(a, NULL, &r, TRUE), E_INVALIDARG);
        { HRESULT hr = U(MonikerRelativePathTo)(a, b, &r, FALSE); t_note("MonikerRelativePathTo(reserved=FALSE) -> %08lx", hr); mr_rel(r); }
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}
