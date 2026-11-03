/* Item, anti and pointer monikers. Data tables (saved bytes, comparison data, sizes, hashes) are the
 * Windows-generated values from Wine's ole32 moniker.c, cross-checked against the Win10 decompilation. */
#include "mr.h"

#define SETUP() do { MR_NEED_BASE(); MR_COM_BEGIN(); } while (0)

static DWORD item_hash(const char *s) { DWORD h = 0; for (; *s; s++) h = (h * 3) ^ (DWORD)(unsigned char)toupper((unsigned char)*s); return h; }

/* ====================================================================== item moniker */
typedef struct { const WCHAR *delim; const WCHAR *item; const WCHAR *display; BYTE saved[24]; ULONG saved_len;
                 WCHAR cmp[16]; ULONG max; } item_case;

static const item_case item_cases[] = {
    { L"!",  L"Test", L"!Test",   { 2,0,0,0,'!',0, 5,0,0,0,'T','e','s','t',0 }, 15,   L"!TEST",  54 },
    { NULL,  L"Test", L"Test",    { 0,0,0,0, 5,0,0,0,'T','e','s','t',0 },            13,   L"TEST",   46 },
    { L"",   L"Test", L"Test",    { 1,0,0,0,0, 5,0,0,0,'T','e','s','t',0 },          14,   L"TEST",   50 },
    { L"&&", L"Test", L"&&Test",  { 3,0,0,0,'&','&',0, 5,0,0,0,'T','e','s','t',0 },  16,   L"&&TEST", 58 },
    { L"ab", L"Test", L"abTest",  { 3,0,0,0,'a','b',0, 5,0,0,0,'T','e','s','t',0 },  16,   L"ABTEST", 58 },
};

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_display_name_table) {
    SETUP();
    for (unsigned i = 0; i < 5; i++) { IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item); if (m) { CHECK_DN(m, item_cases[i].display); IMoniker_Release(m); } }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_saved_data_table) {
    SETUP();
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item); BYTE buf[128]; HRESULT hr; ULONG n;
        if (!m) continue;
        n = mr_save(m, buf, sizeof buf, &hr);
        CHECKF(hr == S_OK && n == item_cases[i].saved_len && !memcmp(buf, item_cases[i].saved, n), "case %u: hr=%08lx len=%lu want %lu", i, hr, n, item_cases[i].saved_len);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_get_size_max_table) {
    SETUP();
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item); ULARGE_INTEGER mx;
        if (!m) continue;
        HRESULT hr = IMoniker_GetSizeMax(m, &mx);
        CHECKF(hr == S_OK && mx.QuadPart == item_cases[i].max, "case %u: hr=%08lx max=%llu want %lu", i, hr, mx.QuadPart, item_cases[i].max);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_comparison_data_table) {
    SETUP();
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item); IROTData *rd = NULL; BYTE buf[128]; ULONG len = 0;
        if (!m) continue;
        if (IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
            HRESULT hr = IROTData_GetComparisonData(rd, buf, sizeof buf, &len);
            ULONG want = 16 + (lstrlenW(item_cases[i].cmp) + 1) * 2;
            CHECKF(hr == S_OK && len == want, "case %u: hr=%08lx len=%lu want %lu", i, hr, len, want);
            if (len == want) {
                CHECKF(mr_guid_eq((GUID *)buf, &mr_CLSID_ItemMoniker), "case %u class", i);
                CHECKF(!memcmp(buf + 16, item_cases[i].cmp, want - 16), "case %u data", i);
            }
            IROTData_Release(rd);
        } else CHECK(0);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_item_marshal_data_table) {
    /* "MEOW", OBJREF_CUSTOM, IID_IMoniker, CLSID_ItemMoniker, cbExtension 0, GetSizeMax, saved bytes */
    SETUP(); REQUIRE(U(CoMarshalInterface));
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item); IStream *s = NULL; BYTE buf[256]; ULONG got = 0; LARGE_INTEGER z; z.QuadPart = 0;
        if (!m) continue;
        if (U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
            HRESULT hr = U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
            EXPECT_EQ(hr, S_OK);
            IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
            IStream_Read(s, buf, sizeof buf, &got);
            CHECKF(got == 48 + item_cases[i].saved_len, "case %u: marshal len %lu", i, got);
            if (got >= 48) {
                CHECKF(mr_rd32(buf) == 0x574f454d && mr_rd32(buf + 4) == 4 && mr_guid_eq((GUID *)(buf + 8), &mr_IID_IMoniker) &&
                       mr_guid_eq((GUID *)(buf + 24), &mr_CLSID_ItemMoniker) && mr_rd32(buf + 40) == 0 && mr_rd32(buf + 44) == item_cases[i].max,
                       "case %u: header", i);
                CHECKF(!memcmp(buf + 48, item_cases[i].saved, got - 48 < item_cases[i].saved_len ? got - 48 : item_cases[i].saved_len), "case %u: payload", i);
            }
            IStream_Release(s);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_load_unicode_stream_variants) {
    /* streams whose strings start with the 0xFFFF marker load as the same names as their ANSI equivalents */
    static const BYTE s1[] = { 0x05,0,0,0, 0xff,0xff,0x00,'!',0x00, 0x02,0,0,0, 'A',0x00 };
    static const BYTE s2[] = { 0x02,0,0,0, '!',0x00, 0x05,0,0,0, 0xff,0xff,0x00,'B',0x00 };
    static const BYTE s3[] = { 0x05,0,0,0, 0xff,0xff,0x00,'!',0x00, 0x06,0,0,0, 0xff,0xff,0xff,0x00,'C',0x00 };
    static const struct { const BYTE *d; ULONG n; const WCHAR *name; } t[] = {
        { s1, sizeof s1, L"!A" }, { s2, sizeof s2, L"!B" }, { s3, sizeof s3, L"!C" } };
    SETUP();
    for (unsigned i = 0; i < 3; i++) {
        IMoniker *m = mr_item(NULL, L"object");
        if (!m) continue;
        HRESULT hr = mr_load_into(m, t[i].d, t[i].n);
        CHECKF(hr == S_OK, "load %u hr=%08lx", i, hr);
        CHECK_DN(m, t[i].name);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_item_save_load_roundtrip) {
    SETUP();
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *m = mr_item(item_cases[i].delim, item_cases[i].item), *l = mr_item(L"#", L"other"); BYTE buf[128]; HRESULT hr; ULONG n;
        if (!m || !l) { mr_rel(m); mr_rel(l); continue; }
        n = mr_save(m, buf, sizeof buf, &hr);
        hr = mr_load_into(l, buf, n);
        CHECKF(hr == S_OK, "load %u hr=%08lx", i, hr);
        CHECK_DN(l, item_cases[i].display);
        CHECKF(mr_is_equal(m, l), "case %u not equal after reload", i);
        mr_rel(m); mr_rel(l);
    }
    MR_COM_END();
}

TEST(mr_item_load_truncated_fails) {
    IMoniker *m; BYTE buf[64]; HRESULT hr; ULONG n; SETUP();
    m = mr_item(L"!", L"Test");
    if (m) {
        n = mr_save(m, buf, sizeof buf, &hr);
        for (ULONG cut = 0; cut + 1 < n; cut += 2) {
            IMoniker *l = mr_item(NULL, L"zz");
            if (!l) break;
            hr = mr_load_into(l, buf, cut);
            CHECKF(FAILED(hr), "load of %lu/%lu bytes hr=%08lx", cut, n, hr);
            IMoniker_Release(l);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_item_identity) {
    IMoniker *m; CLSID c; void *p; SETUP();
    m = mr_item(L"!", L"Test");
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK); CHECK(mr_guid_eq(&c, &mr_CLSID_ItemMoniker));
        CHECK_TYPE(m, MKSYS_ITEMMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_ItemMoniker, &p), S_OK); EXPECT_EQ(p, m); if (p) IMoniker_Release(m);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IROTData, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IPersistStream, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = (void *)1; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IDispatch, &p), E_NOINTERFACE);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_item_create_argument_edge_cases) {
    IMoniker *m = (IMoniker *)1; SETUP();
    if (!t_is_wine()) EXPECT_EQ(U(CreateItemMoniker)(L"!", L"Item", NULL), E_INVALIDARG);
    /* NULL or empty item names are accepted */
    HRESULT hr = S_OK;
    if (!t_is_wine()) { hr = U(CreateItemMoniker)(L"!", NULL, &m);
        t_note("CreateItemMoniker(\"!\", NULL) -> %08lx", hr);
        if (hr == S_OK && m) IMoniker_Release(m); }
    m = NULL; hr = U(CreateItemMoniker)(L"!", L"", &m);
    t_note("CreateItemMoniker(\"!\", \"\") -> %08lx", hr);
    if (hr == S_OK && m) { CHECK_DN(m, L"!"); IMoniker_Release(m); }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_hash_table) {
    static const struct { const WCHAR *d, *i; DWORD h; } t[] = {
        { L"!", L"Test", 0x73c }, { L"%", L"Test", 0x73c }, { L"%", L"TEST", 0x73c },
        { L"%", L"T", 0x54 }, { L"%", L"A", 0x41 }, { L"%", L"a", 0x41 } };
    SETUP();
    for (unsigned i = 0; i < 6; i++) { IMoniker *m = mr_item(t[i].d, t[i].i); if (m) { DWORD h = mr_hash(m); CHECKF(h == t[i].h, "%u: %08lx want %08lx", i, h, t[i].h); IMoniker_Release(m); } }
    MR_COM_END();
}

TEST(mr_item_hash_formula) {
    /* h = (h * 3) ^ toupper(c) over the item name, independent of the delimiter */
    static const char *names[] = { "x", "Item1", "a longer item name 0123456789", "MiXeD" };
    SETUP();
    for (unsigned i = 0; i < 4; i++) {
        WCHAR w[64]; MultiByteToWideChar(CP_ACP, 0, names[i], -1, w, 64);
        IMoniker *m = mr_item(L"!", w);
        if (m) { DWORD h = mr_hash(m); CHECKF(h == item_hash(names[i]), "%s: %08lx want %08lx", names[i], h, item_hash(names[i])); IMoniker_Release(m); }
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_is_equal_table) {
    static const struct { const WCHAR *d1, *i1, *d2, *i2; HRESULT hr; } t[] = {
        { L"!", L"Item1", L"!", L"ITEM1", S_OK }, { NULL, L"Item1", L"!", L"ITEM1", S_OK },
        { L"", L"Item1", L"!", L"ITEM1", S_OK }, { L"&", L"Item1", L"!", L"ITEM1", S_OK },
        { L"&&", L"Item1", L"&", L"&Item1", S_FALSE }, { NULL, L"Item1", NULL, L"Item2", S_FALSE },
        { NULL, L"Item1", NULL, L"ITEM1", S_OK } };
    SETUP();
    for (unsigned i = 0; i < 7; i++) {
        IMoniker *a = mr_item(t[i].d1, t[i].i1), *b = mr_item(t[i].d2, t[i].i2);
        if (a && b) {
            HRESULT h1 = IMoniker_IsEqual(a, b), h2 = IMoniker_IsEqual(b, a);
            CHECKF(h1 == t[i].hr && h2 == t[i].hr, "%u: %08lx/%08lx want %08lx", i, h1, h2, t[i].hr);
        }
        mr_rel(a); mr_rel(b);
    }
    { IMoniker *a = mr_item(L"!", L"x"); if (a) { EXPECT_EQ(IMoniker_IsEqual(a, NULL), E_INVALIDARG); IMoniker_Release(a); } }
    MR_COM_END();
}

TEST(mr_item_is_equal_other_classes) {
    IMoniker *a, *f, *an; SETUP();
    a = mr_item(L"!", L"x"); f = mr_file(L"C:\\olet_mr_i\\x"); an = mr_anti(1);
    if (a && f && an) { EXPECT_EQ(IMoniker_IsEqual(a, f), S_FALSE); EXPECT_EQ(IMoniker_IsEqual(a, an), S_FALSE); }
    mr_rel(a); mr_rel(f); mr_rel(an);
    MR_COM_END();
}

TEST(mr_item_reduce_enum_inverse) {
    IMoniker *m, *r = NULL, *inv = NULL; IEnumMoniker *e; SETUP();
    m = mr_item(L"!", L"x");
    if (m) {
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r), MK_S_REDUCED_TO_SELF);
        EXPECT_EQ(r, m); mr_rel(r);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, TRUE, &e), S_OK); EXPECT_EQ(e, NULL);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, FALSE, &e), S_OK); EXPECT_EQ(e, NULL);
        if (!t_is_wine()) EXPECT_EQ(IMoniker_Enum(m, FALSE, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Inverse(m, &inv), S_OK);
        CHECK_TYPE(inv, MKSYS_ANTIMONIKER); CHECK_DN(inv, L"\\..");
        mr_rel(inv);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_bind_without_container_is_invalid) {
    /* an item moniker only has meaning relative to its container (the moniker to its left) */
    IMoniker *m; IBindCtx *bc; IUnknown *u = (IUnknown *)1; ULONG eaten = 5; IMoniker *o = NULL; WCHAR dn[] = L"display name"; SETUP();
    m = mr_item(L"!", L"Test"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
        EXPECT_EQ(IMoniker_ParseDisplayName(m, bc, NULL, dn, &eaten, &o), MK_E_SYNTAX);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_is_running) {
    IMoniker *m, *same, *other; IBindCtx *bc; IRunningObjectTable *rot = NULL; mr_obj o; DWORD ck = 0; SETUP();
    m = mr_item(L"!", L"Test"); same = mr_item(L"!", L"Test"); other = mr_item(L"!", L"Item123"); bc = mr_bc(); mr_obj_init(&o);
    if (m && same && other && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, NULL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_FALSE);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, same), S_OK);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, other), S_FALSE);
        if (U(GetRunningObjectTable)(0, &rot) == S_OK) {
            EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
            EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_OK);
            EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, other), S_FALSE);   /* a newly running moniker overrides the ROT */
            EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_OK);
            IRunningObjectTable_Revoke(rot, ck);
            IRunningObjectTable_Release(rot);
        }
    }
    mr_rel(m); mr_rel(same); mr_rel(other); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_compose_with_anti) {
    IMoniker *m, *a, *r, *c; SETUP();
    m = mr_item(L"!", L"Item");
    if (m) {
        a = mr_anti(1); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); EXPECT_EQ(r, NULL); mr_rel(a);
        a = mr_anti(2); r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); CHECK_TYPE(r, MKSYS_ANTIMONIKER);
        if (r) EXPECT_EQ(mr_hash(r), 0x80000001);
        mr_rel(r); mr_rel(a);
        /* I + (A,A3) -> A3: simplification goes through the generic-composite machinery */
        c = mr_desc("CA1A3"); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, TRUE, &r), MK_E_NEEDGENERIC);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ANTIMONIKER);
        if (r) EXPECT_EQ(mr_hash(r), 0x80000003);
        mr_rel(r); mr_rel(c);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_item_compose_with_item) {
    IMoniker *a, *b, *r = (IMoniker *)1; SETUP();
    a = mr_item(L"!", L"A"); b = mr_item(L"!", L"B");
    if (a && b) {
        EXPECT_EQ(IMoniker_ComposeWith(a, b, TRUE, &r), MK_E_NEEDGENERIC);
        EXPECT_EQ(r, NULL);
        EXPECT_EQ(IMoniker_ComposeWith(a, b, FALSE, &r), S_OK);
        CHECK_TYPE(r, MKSYS_GENERICCOMPOSITE); CHECK_DN(r, L"!A!B");
        mr_rel(r);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_item_common_prefix_and_relative_path) {
    IMoniker *a, *b, *c, *p = (IMoniker *)1, *r = (IMoniker *)1; SETUP();
    a = mr_item(L"!", L"Item"); b = mr_item(L"#", L"Item"); c = mr_item(L"!", L"Item2");
    if (a && b && c) {
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, &p), MK_S_US);        /* equal items (delimiter ignored) */
        EXPECT_EQ(p, a); mr_rel(p);
        p = (IMoniker *)1;
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, c, &p), MK_E_NOPREFIX);
        EXPECT_EQ(p, NULL);
        a->lpVtbl->Release(a); a = mr_item(L"!", L"I1"); b->lpVtbl->Release(b); b = mr_item(L"!", L"I2");
        EXPECT_EQ(IMoniker_RelativePathTo(a, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, NULL, &r), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, b, NULL), E_INVALIDARG);
        r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_RelativePathTo(a, b, &r), MK_E_NOTBINDABLE);
        EXPECT_EQ(r, NULL);
    }
    mr_rel(a); mr_rel(b); mr_rel(c);
    MR_COM_END();
}

TEST(mr_item_cocreate_uninitialized) {
    /* CLSID_ItemMoniker is creatable (ole32 class factory), but cannot be aggregated */
    IMoniker *m = NULL, *m2 = (IMoniker *)1; HRESULT hr; SETUP(); REQUIRE(U(CoCreateInstance));
    hr = U(CoCreateInstance)(&mr_CLSID_ItemMoniker, NULL, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&m);
    EXPECT_EQ(hr, S_OK);
    if (m) {
        hr = U(CoCreateInstance)(&mr_CLSID_ItemMoniker, (IUnknown *)m, CLSCTX_INPROC_SERVER, &mr_IID_IMoniker, (void **)&m2);
        CHECKF(FAILED(hr), "aggregated creation hr=%08lx", hr);
        if (SUCCEEDED(hr) && m2 != (IMoniker *)1) IMoniker_Release(m2);
        CHECK_TYPE(m, MKSYS_ITEMMONIKER);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* ====================================================================== anti moniker */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_anti_basic_properties) {
    IMoniker *m, *inv = (IMoniker *)1; CLSID c; void *p; HRESULT hr; SETUP();
    hr = U(CreateAntiMoniker)(&m);
    EXPECT_EQ(hr, S_OK);
    if (!t_is_wine()) EXPECT_EQ(U(CreateAntiMoniker)(NULL), E_INVALIDARG);
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK); CHECK(mr_guid_eq(&c, &mr_CLSID_AntiMoniker));
        CHECK_TYPE(m, MKSYS_ANTIMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        EXPECT_EQ(mr_hash(m), 0x80000001);
        CHECK_DN(m, L"\\..");
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_AntiMoniker, &p), S_OK); EXPECT_EQ(p, m); if (p) IMoniker_Release(m);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IROTData, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IPersistStream, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        EXPECT_EQ(IMoniker_Inverse(m, &inv), MK_E_NOINVERSE);
        EXPECT_EQ(inv, NULL);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_anti_persistence) {
    static const BYTE saved1[] = { 1,0,0,0 }, saved2[] = { 2,0,0,0 };
    IMoniker *m; BYTE buf[64]; HRESULT hr; ULONG n; ULARGE_INTEGER mx; IROTData *rd = NULL; ULONG len = 0; SETUP();
    for (DWORD lvl = 1; lvl <= 2; lvl++) {
        m = mr_anti(lvl);
        if (!m) continue;
        n = mr_save(m, buf, sizeof buf, &hr);
        CHECKF(hr == S_OK && n == 4 && !memcmp(buf, lvl == 1 ? saved1 : saved2, 4), "level %lu: hr=%08lx n=%lu", lvl, hr, n);
        EXPECT_EQ(IMoniker_GetSizeMax(m, &mx), S_OK);
        EXPECT_EQ(mx.QuadPart, 20);
        if (IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
            EXPECT_EQ(IROTData_GetComparisonData(rd, buf, sizeof buf, &len), S_OK);
            EXPECT_EQ(len, 20);
            if (len == 20) { CHECK(mr_guid_eq((GUID *)buf, &mr_CLSID_AntiMoniker)); EXPECT_EQ(mr_rd32(buf + 16), lvl); }
            IROTData_Release(rd);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_anti_load_levels) {
    IMoniker *m, *m2; SETUP();
    m = mr_anti(2); m2 = mr_anti(1);
    if (m && m2) {
        EXPECT_EQ(mr_hash(m), 0x80000002);
        CHECK_DN(m, L"\\..\\..");
        CHECK_DN(m2, L"\\..");
        /* the combining count is limited to 0xfffff */
        { IStream *s; LARGE_INTEGER z; DWORD v; z.QuadPart = 0;
          if (U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
              v = 0xfffff; IStream_Write(s, &v, 4, NULL); IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
              EXPECT_EQ(IMoniker_Load(m, s), S_OK); EXPECT_EQ(mr_hash(m), 0x800fffff);
              IStream_Seek(s, z, STREAM_SEEK_SET, NULL); v = 0xfffff + 1; IStream_Write(s, &v, 4, NULL); IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
              EXPECT_EQ(IMoniker_Load(m, s), E_INVALIDARG); EXPECT_EQ(mr_hash(m), 0x800fffff);   /* unchanged */
              IStream_Seek(s, z, STREAM_SEEK_SET, NULL); v = 0; IStream_Write(s, &v, 4, NULL); IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
              EXPECT_EQ(IMoniker_Load(m, s), S_OK); EXPECT_EQ(mr_hash(m), 0x80000000); CHECK_DN(m, L"");
              EXPECT_EQ(IMoniker_IsEqual(m, m2), S_FALSE); EXPECT_EQ(IMoniker_IsEqual(m2, m), S_FALSE);
              IStream_Seek(s, z, STREAM_SEEK_SET, NULL); v = 1; IStream_Write(s, &v, 4, NULL); IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
              EXPECT_EQ(IMoniker_Load(m, s), S_OK);
              EXPECT_EQ(IMoniker_IsEqual(m, m2), S_OK); EXPECT_EQ(IMoniker_IsEqual(m2, m), S_OK);
              EXPECT_EQ(IMoniker_IsEqual(m, NULL), E_INVALIDARG);
              IStream_Release(s);
          } }
    }
    mr_rel(m); mr_rel(m2);
    MR_COM_END();
}

TEST(mr_anti_not_bindable) {
    IMoniker *m; IBindCtx *bc; IUnknown *u = (IUnknown *)1; FILETIME ft; SETUP();
    m = mr_anti(1); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_FALSE);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), E_NOTIMPL);
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_NOTIMPL);
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_NOTIMPL);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_anti_compose_with_anti) {
    IMoniker *a, *b, *r = (IMoniker *)1; SETUP();
    a = mr_anti(1); b = mr_anti(1);
    if (a && b) {
        EXPECT_EQ(IMoniker_ComposeWith(a, b, TRUE, &r), MK_E_NEEDGENERIC);
        EXPECT_EQ(r, NULL);
        EXPECT_EQ(IMoniker_ComposeWith(a, b, FALSE, &r), S_OK);
        CHECK_TYPE(r, MKSYS_GENERICCOMPOSITE);
        CHECK_DN(r, L"\\..\\..");
        mr_rel(r);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_anti_reduce_enum) {
    IMoniker *m, *r = NULL; IEnumMoniker *e; SETUP();
    m = mr_anti(1);
    if (m) {
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r), MK_S_REDUCED_TO_SELF); EXPECT_EQ(r, m); mr_rel(r);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, TRUE, &e), S_OK); EXPECT_EQ(e, NULL);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, FALSE, &e), S_OK); EXPECT_EQ(e, NULL);
        EXPECT_EQ(IMoniker_Enum(m, FALSE, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Enum(m, TRUE, NULL), E_INVALIDARG);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_anti_common_prefix) {
    /* the shorter anti moniker is the common prefix; equal ones give MK_S_US with the receiver */
    static const struct { DWORD a, b; HRESULT hr; int wantb; } t[] = {
        { 0, 1, MK_S_ME, 0 }, { 1, 0, MK_S_HIM, 1 }, { 10, 5, MK_S_HIM, 1 }, { 5, 10, MK_S_ME, 0 },
        { 0, 0, MK_S_US, 0 }, { 2, 2, MK_S_US, 0 } };
    SETUP();
    for (unsigned i = 0; i < 6; i++) {
        IMoniker *a = mr_anti(t[i].a), *b = mr_anti(t[i].b), *p = NULL;
        if (a && b) {
            HRESULT hr = IMoniker_CommonPrefixWith(a, b, &p);
            CHECKF(hr == t[i].hr, "%u: hr=%08lx want %08lx", i, hr, t[i].hr);
            CHECKF(p == (t[i].wantb ? b : a), "%u: wrong prefix object", i);
            mr_rel(p);
        }
        mr_rel(a); mr_rel(b);
    }
    MR_COM_END();
}

TEST(mr_anti_relative_path_to) {
    IMoniker *a, *i1, *a2, *r = (IMoniker *)1; SETUP();
    a = mr_anti(1); i1 = mr_item(L"!", L"I1"); a2 = mr_anti(2);
    if (a && i1 && a2) {
        EXPECT_EQ(IMoniker_RelativePathTo(a, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, NULL, &r), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, i1, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, i1, &r), MK_S_HIM); EXPECT_EQ(r, i1); mr_rel(r);
        EXPECT_EQ(IMoniker_RelativePathTo(a, a2, &r), MK_S_HIM); EXPECT_EQ(r, a2); mr_rel(r);
        EXPECT_EQ(IMoniker_RelativePathTo(a2, a, &r), MK_S_HIM); EXPECT_EQ(r, a); mr_rel(r);
    }
    mr_rel(a); mr_rel(i1); mr_rel(a2);
    MR_COM_END();
}

TEST(mr_anti_marshal_data) {
    IMoniker *m; IStream *s; BYTE buf[128]; ULONG got = 0; LARGE_INTEGER z; z.QuadPart = 0; SETUP(); REQUIRE(U(CoMarshalInterface));
    for (DWORD lvl = 1; lvl <= 2; lvl++) {
        m = mr_anti(lvl);
        if (m && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
            EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
            IStream_Seek(s, z, STREAM_SEEK_SET, NULL); IStream_Read(s, buf, sizeof buf, &got);
            EXPECT_EQ(got, 52);
            if (got == 52) {
                EXPECT_EQ(mr_rd32(buf), 0x574f454d);
                CHECK(mr_guid_eq((GUID *)(buf + 24), &mr_CLSID_AntiMoniker));
                EXPECT_EQ(mr_rd32(buf + 44), 20);
                EXPECT_EQ(mr_rd32(buf + 48), lvl);
            }
            IStream_Release(s);
        }
        mr_rel(m);
    }
    MR_COM_END();
}

/* ====================================================================== pointer moniker */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_pointer_basic_properties) {
    mr_obj o; IMoniker *m = NULL, *inv = NULL; CLSID c; void *p; SETUP();
    mr_obj_init(&o);
    EXPECT_EQ(U(CreatePointerMoniker)(mr_obj_unk(&o), NULL), E_INVALIDARG);
    EXPECT_EQ(U(CreatePointerMoniker)(mr_obj_unk(&o), &m), S_OK);
    EXPECT_EQ(o.ref, 2);                                    /* holds a reference */
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK); CHECK(mr_guid_eq(&c, &mr_CLSID_PointerMoniker));
        CHECK_TYPE(m, MKSYS_POINTERMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        EXPECT_EQ(mr_hash(m), (DWORD)(ULONG_PTR)mr_obj_unk(&o));
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_PointerMoniker, &p), S_OK); EXPECT_EQ(p, m); if (p) IMoniker_Release(m);
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IMarshal, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
        EXPECT_EQ(IMoniker_Inverse(m, &inv), S_OK); CHECK_TYPE(inv, MKSYS_ANTIMONIKER); mr_rel(inv);
        IMoniker_Release(m);
    }
    EXPECT_EQ(o.ref, 1);
    MR_COM_END();
}

TEST(mr_pointer_no_persistence) {
    mr_obj o; IMoniker *m; IStream *s; WCHAR *dn = (WCHAR *)1; IBindCtx *bc; void *p; SETUP();
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o)); bc = mr_bc();
    if (m && bc && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        EXPECT_EQ(IMoniker_Save(m, s, TRUE), E_NOTIMPL);
        EXPECT_EQ(IMoniker_GetDisplayName(m, bc, NULL, &dn), E_NOTIMPL);
        EXPECT_EQ(IMoniker_GetDisplayName(m, bc, NULL, NULL), E_INVALIDARG);
        dn = (WCHAR *)1;
        EXPECT_EQ(IMoniker_GetDisplayName(m, NULL, NULL, &dn), E_INVALIDARG);
        EXPECT_EQ(dn, NULL);
        p = (void *)1;
        EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IROTData, &p), E_NOINTERFACE);
        EXPECT_EQ(p, NULL);
        IStream_Release(s);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_pointer_binding) {
    mr_obj o; IMoniker *m, *nul; IBindCtx *bc; IUnknown *u = NULL; FILETIME ft; SETUP();
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o)); nul = mr_ptr(NULL); bc = mr_bc();
    if (m && nul && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_OK);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), E_NOTIMPL);
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        EXPECT_EQ(u, mr_obj_unk(&o));
        if (u) IUnknown_Release(u);
        u = NULL;
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IUnknown, (void **)&u), S_OK);
        EXPECT_EQ(u, mr_obj_unk(&o));
        if (u) IUnknown_Release(u);
        u = (IUnknown *)1;
        EXPECT_EQ(IMoniker_BindToObject(m, bc, NULL, &mr_IID_IDispatch, (void **)&u), E_NOINTERFACE);
        EXPECT_EQ(IMoniker_BindToObject(nul, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_UNEXPECTED);
        EXPECT_EQ(IMoniker_BindToStorage(nul, bc, NULL, &mr_IID_IUnknown, (void **)&u), E_UNEXPECTED);
    }
    EXPECT_EQ(o.ref, 1 + (m ? 1 : 0));
    mr_rel(m); mr_rel(nul); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_pointer_enum_reduce) {
    mr_obj o; IMoniker *m, *r = NULL; IEnumMoniker *e = (IEnumMoniker *)1; HRESULT hr; SETUP();
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o));
    if (m) {
        hr = IMoniker_Enum(m, TRUE, &e);
        t_note("pointer moniker Enum -> %08lx", hr);
        hr = IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r);
        t_note("pointer moniker Reduce(NULL bc) -> %08lx", hr);
        mr_rel(r);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_pointer_is_equal_and_common_prefix) {
    mr_obj o, o2; IMoniker *a, *b, *c, *outer, *p = (IMoniker *)1; SETUP();
    mr_obj_init(&o); mr_obj_init(&o2);
    a = mr_ptr(mr_obj_unk(&o)); b = mr_ptr(mr_obj_unk(&o)); c = mr_ptr(mr_obj_unk(&o2));
    outer = mr_ptr((IUnknown *)a);
    if (a && b && c && outer) {
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, c), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, outer), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, NULL, &p), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, &p), MK_S_US); EXPECT_EQ(p, a); mr_rel(p);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, outer, &p), MK_E_NOPREFIX);
    }
    mr_rel(a); mr_rel(b); mr_rel(c); mr_rel(outer);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_pointer_compose_with) {
    mr_obj o; IMoniker *m, *a, *r, *c; SETUP();
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o));
    if (m) {
        a = mr_anti(1); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); EXPECT_EQ(r, NULL); mr_rel(a);
        a = mr_anti(2); r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK); CHECK_TYPE(r, MKSYS_ANTIMONIKER); mr_rel(r); mr_rel(a);
        c = mr_desc("CA1A3"); r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, TRUE, &r), MK_E_NEEDGENERIC);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ANTIMONIKER);
        if (r) EXPECT_EQ(mr_hash(r), 0x80000003);
        mr_rel(r); mr_rel(c);
        c = mr_desc("CA1I1"); r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, c, FALSE, &r), S_OK); CHECK_TYPE(r, MKSYS_ITEMMONIKER); mr_rel(r); mr_rel(c);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_pointer_relative_path_to) {
    mr_obj o; IMoniker *m, *i1, *r = (IMoniker *)1; SETUP();
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o)); i1 = mr_item(L"!", L"I1");
    if (m && i1) {
        EXPECT_EQ(IMoniker_RelativePathTo(m, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(m, NULL, &r), E_INVALIDARG);
        EXPECT_EQ(r, NULL);
        EXPECT_EQ(IMoniker_RelativePathTo(m, i1, NULL), E_INVALIDARG);
        r = (IMoniker *)1;
        EXPECT_EQ(IMoniker_RelativePathTo(m, i1, &r), E_NOTIMPL);
        EXPECT_EQ(r, NULL);
    }
    mr_rel(m); mr_rel(i1);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_pointer_marshal_roundtrip) {
    mr_obj o; IMoniker *m, *u = NULL; IStream *s; IMarshal *im = NULL; CLSID c; DWORD sz = 0; LARGE_INTEGER z; z.QuadPart = 0;
    SETUP(); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface));
    mr_obj_init(&o);
    m = mr_ptr(mr_obj_unk(&o));
    if (m && IMoniker_QueryInterface(m, &mr_IID_IMarshal, (void **)&im) == S_OK) {
        EXPECT_EQ(IMarshal_GetUnmarshalClass(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, 0, &c), S_OK);
        CHECK(mr_guid_eq(&c, &mr_CLSID_PointerMoniker));
        EXPECT_EQ(IMarshal_GetMarshalSizeMax(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, 0, &sz), S_OK);
        CHECK(sz > 0);
        IMarshal_Release(im);
        if (U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
            EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
            CHECK(o.ref > 1);
            IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
            EXPECT_EQ(U(CoUnmarshalInterface)(s, &mr_IID_IMoniker, (void **)&u), S_OK);
            if (u) { EXPECT_EQ(IMoniker_IsEqual(m, u), S_OK); IMoniker_Release(u); }
            IStream_Release(s);
        }
    }
    mr_rel(m);
    MR_COM_END();
}
