/* File moniker (CreateFileMoniker).  Facts are from the Win10 decompilation of CFileMoniker
 * (Initialize, GenerateDisplayName, Hash/CalcFileMonikerHash, IsEqual, GetComparisonData, Reduce, Enum,
 * IsRunning, GetTimeOfLastChange, ComposeWith/Peel, GetSizeMax). */
#include "mr.h"

#define NEED_FILE() do { MR_NEED_BASE(); } while (0)

static WCHAR g_np[64];   /* unique "non-existent directory" prefix, filled lazily */
static const WCHAR *np(void) {
    if (!g_np[0]) wsprintfW(g_np, L"C:\\olet_mr_%lu_nonexistent", GetCurrentProcessId());
    return g_np;
}
static IMoniker *file_at(const WCHAR *rest) { WCHAR b[260]; wsprintfW(b, L"%ls\\%ls", np(), rest); return mr_file(b); }

/* ---------------------------------------------------------------- creation */
TEST(mr_file_create_null_path) {
    IMoniker *m = (IMoniker *)1; NEED_FILE(); MR_COM_BEGIN();
    EXPECT_EQ(U(CreateFileMoniker)(NULL, &m), MK_E_SYNTAX);
    EXPECT_EQ(m, NULL);
    MR_COM_END();
}

TEST(mr_file_create_empty_path) {
    IMoniker *m = (IMoniker *)1; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    hr = U(CreateFileMoniker)(L"", &m);
    t_note("CreateFileMoniker(\"\") -> %08lx moniker=%p", hr, m);
    if (hr == S_OK && m) { CHECK_TYPE(m, MKSYS_FILEMONIKER); CHECK_DN(m, L""); IMoniker_Release(m); }
    MR_COM_END();
}

TEST(mr_file_create_without_com_init) {
    struct { HRESULT hr; IMoniker *m; ULONG rel; } r = { E_FAIL, NULL, 99 };
    void run(void *p) { typeof(r) *q = p; q->hr = U(CreateFileMoniker)(L"C:\\olet_mr_noinit.txt", &q->m);
        if (q->m) q->rel = IMoniker_Release(q->m); }
    NEED_FILE();
    if (!mr_run_thread(run, &r, 10000)) return;
    EXPECT_EQ(r.hr, S_OK);
    EXPECT_EQ(r.rel, 0);
}

TEST(mr_file_create_too_long_path) {
    /* the stored path must be shorter than MAX_PATH (decomp: ccPath < 0x104) */
    static WCHAR p[400]; IMoniker *m = (IMoniker *)1; HRESULT hr; int i;
    NEED_FILE(); MR_COM_BEGIN();
    lstrcpyW(p, L"C:\\olet_mr_long\\");
    for (i = lstrlenW(p); i < 380; i++) p[i] = L'a';
    p[380] = 0;
    hr = U(CreateFileMoniker)(p, &m);
    t_note("CreateFileMoniker(380 chars) -> %08lx", hr);
    if (SUCCEEDED(hr) && m) IMoniker_Release(m);
    MR_COM_END();
}

/* ---------------------------------------------------------------- identity and interfaces */
TEST(mr_file_interfaces) {
    IMoniker *m; void *p; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"if.txt");
    if (m) {
        static const struct { const GUID *iid; const char *n; } ok[] = {
            { &mr_IID_IUnknown, "IUnknown" }, { &mr_IID_IMoniker, "IMoniker" }, { &mr_IID_IPersistStream, "IPersistStream" },
            { &mr_IID_IROTData, "IROTData" }, { &mr_IID_IMarshal, "IMarshal" } };
        for (int i = 0; i < 5; i++) {
            p = NULL; HRESULT hr = IMoniker_QueryInterface(m, ok[i].iid, &p);
            CHECKF(hr == S_OK && p, "QI %s hr=%08lx", ok[i].n, hr);
            if (p) IUnknown_Release((IUnknown *)p);
        }
        /* Windows 10: the file moniker does not answer IPersist directly */
        p = (void *)1; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IPersist, &p), E_NOINTERFACE); EXPECT_EQ(p, NULL);
        p = (void *)1; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IDispatch, &p), E_NOINTERFACE); EXPECT_EQ(p, NULL);
        p = (void *)1; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_IID_IBindCtx, &p), E_NOINTERFACE);
        /* the class-id QueryInterface idiom returns the object itself */
        p = NULL; EXPECT_EQ(IMoniker_QueryInterface(m, &mr_CLSID_FileMoniker, &p), S_OK);
        EXPECT_EQ(p, m);
        if (p) IMoniker_Release((IMoniker *)p);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_class_and_system_type) {
    IMoniker *m; CLSID c; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"cls.txt");
    if (m) {
        EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK);
        CHECK(mr_guid_eq(&c, &mr_CLSID_FileMoniker));
        CHECK_TYPE(m, MKSYS_FILEMONIKER);
        EXPECT_EQ(IMoniker_IsDirty(m), S_FALSE);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_refcounting) {
    IMoniker *m; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"rc.txt");
    if (m) {
        EXPECT_EQ(IMoniker_AddRef(m), 2);
        EXPECT_EQ(IMoniker_AddRef(m), 3);
        EXPECT_EQ(IMoniker_Release(m), 2);
        EXPECT_EQ(IMoniker_Release(m), 1);
        EXPECT_EQ(IMoniker_Release(m), 0);
    }
    MR_COM_END();
}

/* ---------------------------------------------------------------- display names */
TEST(mr_file_display_names_roundtrip) {
    static const WCHAR *names[] = {
        L"C:\\olet_mr_dn\\a.txt", L"c:\\olet_mr_dn\\Mixed.Case.TXT", L"C:\\olet_mr_dn\\sub dir\\file name.ext",
        L"\\\\olet-no-such-server\\share\\dir\\f.txt", L"\\olet_mr_rooted\\f.txt", L"olet_mr_rel.txt",
        L"olet_mr_reldir\\sub\\f.txt", L"C:\\olet_mr_dn\\a", L"C:\\", L"C:\\olet_mr_dn\\\u00e4\u00f6\u00fc.txt",
        L"C:\\olet_mr_dn\\\u20ac\u2020\u0100.txt" };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        IMoniker *m = mr_file(names[i]);
        if (m) {
            WCHAR *s = NULL; IBindCtx *bc = mr_bc();
            HRESULT hr = IMoniker_GetDisplayName(m, bc, NULL, &s);
            CHECKF(hr == S_OK && s && !lstrcmpW(s, names[i]), "name %u hr=%08lx got \"%ls\" want \"%ls\"", i, hr, s ? s : L"(null)", names[i]);
            if (s) U(CoTaskMemFree)(s);
            IBindCtx_Release(bc); IMoniker_Release(m);
        }
    }
    MR_COM_END();
}

TEST(mr_file_display_name_leading_dotdot_is_counted) {
    /* "..\" segments are split off into an anti-count; the display name re-expands them */
    static const WCHAR *names[] = { L"..\\olet_mr_a.txt", L"..\\..\\olet_mr_a.txt", L"..\\..\\..\\olet_mr_b\\c.txt" };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < 3; i++) { IMoniker *m = mr_file(names[i]); if (m) { CHECK_DN(m, names[i]); IMoniker_Release(m); } }
    MR_COM_END();
}

TEST(mr_file_display_name_error_paths) {
    IMoniker *m; WCHAR *s = (WCHAR *)1; IBindCtx *bc; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"dn.txt"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_GetDisplayName(m, bc, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_GetDisplayName(m, NULL, NULL, &s), E_INVALIDARG);
        EXPECT_EQ(s, NULL);
        /* the "moniker to the left" is ignored but must be a valid interface pointer when non-NULL */
        EXPECT_EQ(IMoniker_GetDisplayName(m, bc, NULL, &s), S_OK);
        if (s) U(CoTaskMemFree)(s);
    }
    if (m) IMoniker_Release(m);
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_long_name_expansion_not_applied_to_missing_files) {
    /* CreateFileMoniker normalises through GetLongPathName, which cannot expand a path that does not exist,
     * so a short-looking non-existent name is kept verbatim */
    IMoniker *m; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(L"C:\\OLETMR~1\\NOFILE~1.TXT");
    if (m) { CHECK_DN(m, L"C:\\OLETMR~1\\NOFILE~1.TXT"); IMoniker_Release(m); }
    MR_COM_END();
}

/* ---------------------------------------------------------------- hash and equality */
static DWORD expect_hash(const WCHAR *path, int anti) {
    DWORD h = 0; WCHAR buf[300]; lstrcpynW(buf, path, 300); CharUpperW(buf);
    for (const WCHAR *c = buf; *c; c++) h = h * 3 ^ *c;
    return h + anti;
}

TEST(mr_file_hash) {
    static const struct { const WCHAR *path; int anti; const WCHAR *stripped; } t[] = {
        { L"C:\\olet_mr_h\\a.txt", 0, L"C:\\olet_mr_h\\a.txt" },
        { L"c:\\olet_mr_h\\A.TXT", 0, L"c:\\olet_mr_h\\A.TXT" },
        { L"olet_mr_h.txt", 0, L"olet_mr_h.txt" },
        { L"..\\olet_mr_h.txt", 1, L"olet_mr_h.txt" },
        { L"..\\..\\olet_mr_h.txt", 2, L"olet_mr_h.txt" },
        { L"\\\\srv\\share\\f", 0, L"\\\\srv\\share\\f" } };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < 6; i++) {
        IMoniker *m = mr_file(t[i].path);
        if (m) {
            DWORD h = 0; HRESULT hr = IMoniker_Hash(m, &h);
            DWORD want = expect_hash(t[i].stripped, t[i].anti);
            CHECKF(hr == S_OK && h == want, "%ls: hr=%08lx hash=%08lx want %08lx", t[i].path, hr, h, want);
            IMoniker_Release(m);
        }
    }
    MR_COM_END();
}

TEST(mr_file_hash_case_insensitive_and_null) {
    IMoniker *a, *b; NEED_FILE(); MR_COM_BEGIN();
    a = mr_file(L"C:\\olet_mr_H\\Abc.TXT"); b = mr_file(L"c:\\OLET_MR_h\\abc.txt");
    if (a && b) {
        EXPECT_EQ(mr_hash(a), mr_hash(b));
        EXPECT_EQ(IMoniker_Hash(a, NULL), E_INVALIDARG);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_file_is_equal) {
    IMoniker *a, *b, *c, *d, *it; NEED_FILE(); MR_COM_BEGIN();
    a = mr_file(L"test.bmp"); b = mr_file(L"TEST.bmp"); c = mr_file(L"other.bmp"); d = mr_file(L"..\\test.bmp");
    it = mr_item(L"!", L"test.bmp");
    if (a && b && c && d && it) {
        EXPECT_EQ(IMoniker_IsEqual(a, a), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, b), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(b, a), S_OK);
        EXPECT_EQ(IMoniker_IsEqual(a, c), S_FALSE);
        EXPECT_EQ(IMoniker_IsEqual(a, d), S_FALSE);          /* different anti count */
        EXPECT_EQ(IMoniker_IsEqual(a, it), S_FALSE);         /* different moniker class */
        EXPECT_EQ(IMoniker_IsEqual(a, NULL), E_INVALIDARG);
    }
    mr_rel(a); mr_rel(b); mr_rel(c); mr_rel(d); mr_rel(it);
    MR_COM_END();
}

/* ---------------------------------------------------------------- IROTData */
TEST(mr_file_comparison_data) {
    IMoniker *m; IROTData *rd = NULL; BYTE buf[600]; ULONG len = 0; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(L"c:\\olet_mr_cd\\MiXed.txt");
    if (m && IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
        static const WCHAR up[] = L"C:\\OLET_MR_CD\\MIXED.TXT";
        memset(buf, 0xaa, sizeof buf);
        hr = IROTData_GetComparisonData(rd, buf, sizeof buf, &len);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(len, 16 + sizeof up);
        CHECK(mr_guid_eq((GUID *)buf, &mr_CLSID_FileMoniker));
        CHECK(!memcmp(buf + 16, up, sizeof up));
        /* too small: an error, and no partial claim of success */
        hr = IROTData_GetComparisonData(rd, buf, 16, &len);
        CHECKF(FAILED(hr), "tiny buffer hr=%08lx", hr);
        t_note("GetComparisonData(cb=16) -> %08lx len=%lu", hr, len);
        IROTData_Release(rd);
    } else CHECK(0);
    mr_rel(m);
    MR_COM_END();
}

TEST(mr_file_comparison_data_includes_dotdot) {
    IMoniker *m; IROTData *rd = NULL; BYTE buf[600]; ULONG len = 0; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(L"..\\..\\olet_mr_cd2.txt");
    if (m && IMoniker_QueryInterface(m, &mr_IID_IROTData, (void **)&rd) == S_OK) {
        static const WCHAR up[] = L"..\\..\\OLET_MR_CD2.TXT";
        EXPECT_EQ(IROTData_GetComparisonData(rd, buf, sizeof buf, &len), S_OK);
        EXPECT_EQ(len, 16 + sizeof up);
        CHECK(!memcmp(buf + 16, up, sizeof up));
        IROTData_Release(rd);
    } else CHECK(0);
    mr_rel(m);
    MR_COM_END();
}

/* ---------------------------------------------------------------- persistence */
TEST(mr_file_save_stream_layout) {
    /* [MS-OLEDS] file moniker stream: WORD cAnti; DWORD cbAnsi; char ansi[cbAnsi]; WORD endServer; WORD 0xDEAD;
     * 20 reserved bytes; [DWORD cbUnicode ...] */
    static const char path[] = "C:\\olet_mr_sv\\layout.txt";
    IMoniker *m; BYTE buf[512]; ULONG n; HRESULT hr = E_FAIL; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(L"C:\\olet_mr_sv\\layout.txt");
    if (m) {
        n = mr_save(m, buf, sizeof buf, &hr);
        EXPECT_EQ(hr, S_OK);
        CHECK(n >= 2 + 4 + sizeof path + 2 + 2 + 20);
        if (n >= 2 + 4 + sizeof path + 4) {
            EXPECT_EQ(mr_rd16(buf), 0);
            EXPECT_EQ(mr_rd32(buf + 2), sizeof path);
            CHECK(!memcmp(buf + 6, path, sizeof path));
            EXPECT_EQ(mr_rd16(buf + 6 + sizeof path + 2), 0xDEAD);
            WORD es = mr_rd16(buf + 6 + sizeof path);
            if (es != 0xFFFF) t_note("endServer = %04x for a local path", es);
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_save_anti_count) {
    IMoniker *m; BYTE buf[512]; ULONG n; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(L"..\\..\\..\\olet_mr_sv2.txt");
    if (m) {
        n = mr_save(m, buf, sizeof buf, &hr);
        EXPECT_EQ(hr, S_OK);
        if (n > 8) {
            EXPECT_EQ(mr_rd16(buf), 3);
            EXPECT_EQ(mr_rd32(buf + 2), sizeof "olet_mr_sv2.txt");
            CHECK(!memcmp(buf + 6, "olet_mr_sv2.txt", sizeof "olet_mr_sv2.txt"));
        }
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_save_load_roundtrip) {
    static const WCHAR *names[] = { L"C:\\olet_mr_rt\\a.txt", L"..\\..\\olet_mr_rt.txt", L"\\\\srv\\sh\\f.txt",
        L"olet_mr_rt.bin", L"C:\\olet_mr_rt\\\u00e4\u00f6.txt", L"C:\\olet_mr_rt\\\u20ac\u0100\u0101.txt" };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < 6; i++) {
        IMoniker *m = mr_file(names[i]), *l = mr_file(L"scratch"); BYTE buf[1024]; HRESULT hr; ULONG n;
        if (m && l) {
            n = mr_save(m, buf, sizeof buf, &hr);
            CHECKF(hr == S_OK, "save %u hr=%08lx", i, hr);
            hr = mr_load_into(l, buf, n);
            CHECKF(hr == S_OK, "load %u hr=%08lx", i, hr);
            CHECKF(mr_is_equal(m, l), "reloaded moniker %u differs", i);
            { WCHAR *a = NULL, *b = NULL; IBindCtx *bc = mr_bc();
              IMoniker_GetDisplayName(m, bc, NULL, &a); IMoniker_GetDisplayName(l, bc, NULL, &b);
              CHECKF(a && b && !lstrcmpW(a, b), "display %u: \"%ls\" vs \"%ls\"", i, a ? a : L"-", b ? b : L"-");
              if (a) U(CoTaskMemFree)(a); if (b) U(CoTaskMemFree)(b); IBindCtx_Release(bc); }
        }
        mr_rel(m); mr_rel(l);
    }
    MR_COM_END();
}

TEST(mr_file_get_size_max_covers_save) {
    static const WCHAR *names[] = { L"C:\\olet_mr_gs\\a.txt", L"..\\x", L"C:\\olet_mr_gs\\\u20ac\u0100.txt", L"\\\\s\\h\\f" };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < 4; i++) {
        IMoniker *m = mr_file(names[i]); ULARGE_INTEGER mx; BYTE buf[1024]; HRESULT hr; ULONG n;
        if (m) {
            HRESULT h2 = IMoniker_GetSizeMax(m, &mx);
            n = mr_save(m, buf, sizeof buf, &hr);
            CHECKF(h2 == S_OK && hr == S_OK && mx.QuadPart >= n, "%u: max=%llu saved=%lu", i, mx.QuadPart, n);
            EXPECT_EQ(mx.HighPart, 0);
            EXPECT_EQ(IMoniker_GetSizeMax(m, NULL), E_INVALIDARG);
            IMoniker_Release(m);
        }
    }
    MR_COM_END();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_file_load_tolerates_reserved_garbage) {
    /* Load ignores the anti-count word, the version word and the reserved block of a saved moniker */
    static const WCHAR name[] = L"C:\\olet_mr_ld\\garbage.doc";
    IMoniker *m, *l; BYTE buf[512]; ULONG n, i, lp; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = mr_file(name); l = mr_file(name);
    if (m && l) {
        n = mr_save(m, buf, sizeof buf, &hr);
        lp = 6 + (ULONG)(lstrlenW(name) + 1) + 2;          /* offset of the 0xDEAD version word */
        if (hr == S_OK && n > lp + 22) {
            buf[lp] = 0x98; buf[lp + 1] = 0xBA;
            for (i = 0; i < 20; i++) buf[lp + 2 + i] = 0xFE;
            hr = mr_load_into(l, buf, n);
            EXPECT_EQ(hr, S_OK);
        } else CHECKF(0, "unexpected save size %lu hr=%08lx", n, hr);
    }
    mr_rel(m); mr_rel(l);
    MR_COM_END();
}

TEST(mr_file_load_truncated_stream_fails) {
    IMoniker *m, *l; BYTE buf[512]; ULONG n; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"trunc.txt"); l = mr_file(L"x.y");
    if (m && l) {
        n = mr_save(m, buf, sizeof buf, &hr);
        for (ULONG cut = 0; cut < 10 && cut < n; cut += 3) {
            hr = mr_load_into(l, buf, cut);
            CHECKF(FAILED(hr), "load of %lu bytes hr=%08lx", cut, hr);
        }
    }
    mr_rel(m); mr_rel(l);
    MR_COM_END();
}

TEST(mr_file_save_null_stream) {
    IMoniker *m; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"sv.txt");
    if (m) { HRESULT hr = IMoniker_Save(m, NULL, TRUE); CHECKF(FAILED(hr), "Save(NULL) hr=%08lx", hr); IMoniker_Release(m); }
    MR_COM_END();
}

/* ---------------------------------------------------------------- marshaling */
TEST(mr_file_marshal_inproc_roundtrip) {
    /* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
    IMoniker *m, *u = NULL; IStream *s = NULL; LARGE_INTEGER z; z.QuadPart = 0; NEED_FILE(); REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface));
    MR_COM_BEGIN();
    m = file_at(L"marsh.txt");
    if (m && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
        EXPECT_EQ(U(CoUnmarshalInterface)(s, &mr_IID_IMoniker, (void **)&u), S_OK);
        if (u) { EXPECT_EQ(IMoniker_IsEqual(m, u), S_OK); CHECK(u != m); IMoniker_Release(u); }
        IStream_Release(s);
    }
    mr_rel(m);
    MR_COM_END();
}

TEST(mr_file_marshal_data_format) {
    /* custom marshal: OBJREF "MEOW", flags=OBJREF_CUSTOM(4), IID_IMoniker, CLSID of the moniker, cbExtension=0, DWORD size, saved bytes */
    IMoniker *m; IStream *s = NULL; LARGE_INTEGER z; BYTE buf[1024], sv[512]; ULONG got = 0, nsv; HRESULT hr; ULARGE_INTEGER end; z.QuadPart = 0;
    NEED_FILE(); REQUIRE(U(CoMarshalInterface)); MR_COM_BEGIN();
    m = file_at(L"mdata.txt");
    if (m && U(CreateStreamOnHGlobal)(NULL, TRUE, &s) == S_OK) {
        nsv = mr_save(m, sv, sizeof sv, &hr);
        EXPECT_EQ(U(CoMarshalInterface)(s, &mr_IID_IMoniker, (IUnknown *)m, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
        IStream_Seek(s, z, STREAM_SEEK_CUR, &end);
        IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
        IStream_Read(s, buf, sizeof buf, &got);
        EXPECT_EQ(got, end.QuadPart);
        if (got >= 48) {
            EXPECT_EQ(mr_rd32(buf), 0x574f454d);              /* 'MEOW' */
            EXPECT_EQ(mr_rd32(buf + 4), 4);
            CHECK(mr_guid_eq((GUID *)(buf + 8), &mr_IID_IMoniker));
            CHECK(mr_guid_eq((GUID *)(buf + 24), &mr_CLSID_FileMoniker));
            EXPECT_EQ(mr_rd32(buf + 40), 0);                  /* cbExtension */
            { ULARGE_INTEGER mx; mx.QuadPart = 0; IMoniker_GetSizeMax(m, &mx);
              EXPECT_EQ(mr_rd32(buf + 44), mx.LowPart); }   /* the size word is IPersistStream::GetSizeMax */
            EXPECT_EQ(got, 48 + nsv);
            CHECK(!memcmp(buf + 48, sv, nsv < got - 48 ? nsv : got - 48));
        } else CHECKF(0, "marshal data too short: %lu", got);
        IStream_Release(s);
    }
    mr_rel(m);
    MR_COM_END();
}

TEST(mr_file_marshal_across_threads) {
    IMoniker *m; struct { IStream *s; HRESULT hr; IMoniker *got; int eq; } r;
    void run(void *p) {
        typeof(r) *q = p; IMoniker *u = NULL;
        if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return;
        q->hr = U(CoGetInterfaceAndReleaseStream)(q->s, &mr_IID_IMoniker, (void **)&u);
        if (u) { q->got = u; q->eq = 1; }
        U(CoUninitialize)();
    }
    NEED_FILE(); REQUIRE(U(CoMarshalInterThreadInterfaceInStream)); REQUIRE(U(CoGetInterfaceAndReleaseStream));
    MR_COM_BEGIN();
    m = file_at(L"thr.txt");
    memset(&r, 0, sizeof r);
    if (m && U(CoMarshalInterThreadInterfaceInStream)(&mr_IID_IMoniker, (IUnknown *)m, &r.s) == S_OK) {
        if (mr_run_thread(run, &r, 10000)) {
            EXPECT_EQ(r.hr, S_OK);
            CHECK(r.got != NULL);
            /* the unmarshalled copy lives in another apartment; it is a by-value copy so calls work directly */
            if (r.got) { EXPECT_EQ(IMoniker_IsEqual(r.got, m), S_OK); IMoniker_Release(r.got); }
        }
    }
    mr_rel(m);
    MR_COM_END();
}

TEST(mr_file_imarshal_unmarshal_class) {
    IMoniker *m; IMarshal *im = NULL; CLSID c; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"im.txt");
    if (m && IMoniker_QueryInterface(m, &mr_IID_IMarshal, (void **)&im) == S_OK) {
        HRESULT hr = IMarshal_GetUnmarshalClass(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &c);
        EXPECT_EQ(hr, S_OK);
        if (hr == S_OK) CHECK(mr_guid_eq(&c, &mr_CLSID_FileMoniker));
        DWORD sz = 0;
        hr = IMarshal_GetMarshalSizeMax(im, &mr_IID_IMoniker, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &sz);
        EXPECT_EQ(hr, S_OK);
        CHECK(sz > 48);
        IMarshal_Release(im);
    } else CHECK(0);
    mr_rel(m);
    MR_COM_END();
}

/* ---------------------------------------------------------------- reduce / enum / inverse */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_file_reduce) {
    IMoniker *m, *r = NULL, *l; IBindCtx *bc; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"red.txt"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_Reduce(m, NULL, MKRREDUCE_ALL, NULL, &r), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Reduce(m, bc, MKRREDUCE_ALL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_Reduce(m, bc, MKRREDUCE_ALL, NULL, &r), MK_S_REDUCED_TO_SELF);
        EXPECT_EQ(r, m);
        if (r) IMoniker_Release(r);
        l = mr_anti(1); r = NULL;
        EXPECT_EQ(IMoniker_Reduce(m, bc, MKRREDUCE_ONE, &l, &r), MK_S_REDUCED_TO_SELF);
        EXPECT_EQ(r, m);
        EXPECT_NE(l, NULL);                      /* *ppmkToLeft untouched */
        if (r) IMoniker_Release(r);
        mr_rel(l);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_enum) {
    IMoniker *m; IEnumMoniker *e; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"enum.txt");
    if (m) {
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, TRUE, &e), S_OK); EXPECT_EQ(e, NULL);
        e = (IEnumMoniker *)1; EXPECT_EQ(IMoniker_Enum(m, FALSE, &e), S_OK); EXPECT_EQ(e, NULL);
        EXPECT_EQ(IMoniker_Enum(m, FALSE, NULL), E_INVALIDARG);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_inverse) {
    IMoniker *m, *inv = (IMoniker *)1; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"inv.txt");
    if (m) {
        EXPECT_EQ(IMoniker_Inverse(m, &inv), S_OK);
        CHECK_TYPE(inv, MKSYS_ANTIMONIKER);
        CHECK_DN(inv, L"\\..");
        EXPECT_EQ(mr_hash(inv), 0x80000001);
        mr_rel(inv);
        if (!t_is_wine()) EXPECT_EQ(IMoniker_Inverse(m, NULL), E_INVALIDARG);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

/* ---------------------------------------------------------------- composition */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_file_compose_with_anti) {
    IMoniker *m, *a, *r = (IMoniker *)1; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"cmp.txt");
    if (m) {
        a = mr_anti(1);
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK);          /* F + A -> () */
        EXPECT_EQ(r, NULL);
        mr_rel(a);
        a = mr_anti(2); r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, TRUE, &r), S_OK);          /* F + A2 -> A */
        CHECK_TYPE(r, MKSYS_ANTIMONIKER);
        if (r) EXPECT_EQ(mr_hash(r), 0x80000001);
        mr_rel(r); mr_rel(a);
        a = mr_anti(4); r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, a, FALSE, &r), S_OK);         /* F + A4 -> A3 */
        /* Windows 10: file + anti(n) builds a generic composite of n-1 anti monikers (not one anti moniker with
           count n-1); its hash is the xor of the component hashes, so three antis hash to 0x80000001 */
        CHECK_TYPE(r, MKSYS_GENERICCOMPOSITE);
        if (r) EXPECT_EQ(mr_hash(r), 0x80000001);
        mr_rel(r); mr_rel(a);
        IMoniker_Release(m);
    }
    MR_COM_END();
}

TEST(mr_file_compose_with_other_kinds) {
    IMoniker *m, *it, *r = (IMoniker *)1; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"cmp2.txt"); it = mr_item(L"!", L"x");
    if (m && it) {
        EXPECT_EQ(IMoniker_ComposeWith(m, it, TRUE, &r), MK_E_NEEDGENERIC);
        EXPECT_EQ(r, NULL);
        r = NULL;
        EXPECT_EQ(IMoniker_ComposeWith(m, it, FALSE, &r), S_OK);
        CHECK_TYPE(r, MKSYS_GENERICCOMPOSITE);
        { WCHAR w[260]; wsprintfW(w, L"%ls\\cmp2.txt!x", np()); CHECK_DN(r, w); }
        mr_rel(r);
        EXPECT_EQ(IMoniker_ComposeWith(m, NULL, FALSE, &r), E_INVALIDARG);
        EXPECT_EQ(IMoniker_ComposeWith(m, it, FALSE, NULL), E_INVALIDARG);
    }
    mr_rel(m); mr_rel(it);
    MR_COM_END();
}

TEST(mr_file_compose_file_with_file) {
    static const struct { const WCHAR *l, *r, *want; } t[] = {
        { L"C:\\olet_mr_c\\dir", L"sub\\f.txt", L"C:\\olet_mr_c\\dir\\sub\\f.txt" },
        { L"C:\\olet_mr_c\\dir\\", L"sub\\f.txt", L"C:\\olet_mr_c\\dir\\sub\\f.txt" },
        { L"C:\\olet_mr_c\\dir\\a.txt", L"..\\b.txt", L"C:\\olet_mr_c\\dir\\b.txt" },
        { L"C:\\olet_mr_c\\dir\\a.txt", L"..\\..\\b.txt", L"C:\\olet_mr_c\\b.txt" },
        { L"C:\\olet_mr_c\\dir\\a.txt", L"\\b.txt", L"C:\\olet_mr_c\\dir\\a.txt\\b.txt" },
    };
    NEED_FILE(); MR_COM_BEGIN();
    for (unsigned i = 0; i < 5; i++) {
        IMoniker *l = mr_file(t[i].l), *r = mr_file(t[i].r), *c = NULL;
        if (l && r) {
            HRESULT hr = IMoniker_ComposeWith(l, r, TRUE, &c);
            CHECKF(hr == S_OK && c, "%u: hr=%08lx", i, hr);
            if (c) { CHECK_TYPE(c, MKSYS_FILEMONIKER); CHECK_DN(c, t[i].want); IMoniker_Release(c); }
        }
        mr_rel(l); mr_rel(r);
    }
    MR_COM_END();
}

TEST(mr_file_compose_file_with_file_errors) {
    IMoniker *l, *r, *c = (IMoniker *)1; NEED_FILE(); MR_COM_BEGIN();
    /* a right operand that is absolute (drive or UNC) cannot be appended */
    l = mr_file(L"C:\\olet_mr_c\\dir\\a.txt"); r = mr_file(L"D:\\x.txt");
    if (l && r) { EXPECT_EQ(IMoniker_ComposeWith(l, r, TRUE, &c), MK_E_SYNTAX); }
    mr_rel(r); r = mr_file(L"\\\\srv\\share\\x.txt");
    if (l && r) { EXPECT_EQ(IMoniker_ComposeWith(l, r, TRUE, &c), MK_E_SYNTAX); }
    mr_rel(r);
    /* peeling more segments than the left path has */
    mr_rel(l); l = mr_file(L"C:\\a.txt"); r = mr_file(L"..\\..\\z.txt");
    if (l && r) { EXPECT_EQ(IMoniker_ComposeWith(l, r, TRUE, &c), MK_E_SYNTAX); }
    mr_rel(l); mr_rel(r);
    MR_COM_END();
}

TEST(mr_file_compose_with_empty_file_moniker_returns_self) {
    IMoniker *l, *r, *c = NULL; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    l = file_at(L"self.txt");
    hr = U(CreateFileMoniker)(L"", &r);
    if (t_is_wine()) t_skip("Wine crashes composing with an empty file moniker");
    else if (l && hr == S_OK && r) {
        EXPECT_EQ(IMoniker_ComposeWith(l, r, TRUE, &c), S_OK);
        EXPECT_EQ(c, l);
        mr_rel(c); IMoniker_Release(r);
    } else t_note("empty file moniker: %08lx", hr);
    mr_rel(l);
    MR_COM_END();
}

/* ---------------------------------------------------------------- running / change time */
TEST(mr_file_is_running_checks_arguments) {
    IMoniker *m; IBindCtx *bc; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"run.txt"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_IsRunning(m, NULL, NULL, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_FALSE);           /* not in the ROT */
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_is_running_with_newly_running_moniker) {
    /* with pmkNewlyRunning the answer is IsEqual(newly running, this), the ROT is not consulted */
    IMoniker *a, *b, *c; IBindCtx *bc; NEED_FILE(); MR_COM_BEGIN();
    a = file_at(L"nr.txt"); b = file_at(L"NR.TXT"); c = file_at(L"other.txt"); bc = mr_bc();
    if (a && b && c && bc) {
        EXPECT_EQ(IMoniker_IsRunning(a, bc, NULL, b), S_OK);
        EXPECT_EQ(IMoniker_IsRunning(a, bc, NULL, c), S_FALSE);
    }
    mr_rel(a); mr_rel(b); mr_rel(c); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_is_running_finds_rot_entry) {
    IMoniker *m; IBindCtx *bc; IRunningObjectTable *rot = NULL; mr_obj o; DWORD ck = 0; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"inrot.txt"); bc = mr_bc(); mr_obj_init(&o);
    if (m && bc && U(GetRunningObjectTable)(0, &rot) == S_OK) {
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_FALSE);
        EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
        EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_OK);
        IRunningObjectTable_Revoke(rot, ck);
        IRunningObjectTable_Release(rot);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_time_of_last_change_missing_file) {
    IMoniker *m; IBindCtx *bc; FILETIME ft; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"missing.txt"); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), MK_E_NOOBJECT);
        if (!t_is_wine()) {
            EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, NULL, NULL, &ft), E_INVALIDARG);
            EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, NULL), E_INVALIDARG);
        }
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_time_of_last_change_existing_file) {
    char sys[MAX_PATH]; WCHAR wp[MAX_PATH + 16]; WIN32_FILE_ATTRIBUTE_DATA fa; IMoniker *m; IBindCtx *bc; FILETIME ft;
    NEED_FILE(); MR_COM_BEGIN();
    GetSystemDirectoryA(sys, sizeof sys); strcat(sys, "\\kernel32.dll");
    MultiByteToWideChar(CP_ACP, 0, sys, -1, wp, MAX_PATH + 16);
    if (!GetFileAttributesExW(wp, GetFileExInfoStandard, &fa)) { t_skip("no kernel32.dll"); MR_COM_END(); return; }
    m = mr_file(wp); bc = mr_bc();
    if (m && bc) {
        memset(&ft, 0, sizeof ft);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), S_OK);
        EXPECT_EQ(ft.dwLowDateTime, fa.ftLastWriteTime.dwLowDateTime);
        EXPECT_EQ(ft.dwHighDateTime, fa.ftLastWriteTime.dwHighDateTime);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_time_of_last_change_scratch_file_tracks_writes) {
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; HANDLE h; IMoniker *m; IBindCtx *bc; FILETIME ft, want, w1, w2;
    DWORD n; NEED_FILE(); MR_COM_BEGIN();
    t_tmpfile(path, sizeof path, "ft_track.dat");
    h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no fixture"); MR_COM_END(); return; }
    want.dwLowDateTime = 0x11111111; want.dwHighDateTime = 0x01c00000;
    WriteFile(h, "abc", 3, &n, NULL);
    SetFileTime(h, NULL, NULL, &want);
    CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    m = mr_file(wp); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &ft), S_OK);
        w1 = ft;
        EXPECT_EQ(ft.dwLowDateTime, want.dwLowDateTime);
        EXPECT_EQ(ft.dwHighDateTime, want.dwHighDateTime);
        h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        want.dwLowDateTime = 0x22222222; want.dwHighDateTime = 0x01c00001;
        SetFileTime(h, NULL, NULL, &want); CloseHandle(h);
        EXPECT_EQ(IMoniker_GetTimeOfLastChange(m, bc, NULL, &w2), S_OK);
        EXPECT_EQ(w2.dwLowDateTime, want.dwLowDateTime);
        EXPECT_EQ(w2.dwHighDateTime, want.dwHighDateTime);
        CHECK(w1.dwLowDateTime != w2.dwLowDateTime);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    DeleteFileA(path);
    MR_COM_END();
}

/* ---------------------------------------------------------------- binding */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_file_bind_to_storage_modes) {
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; IStorage *st = NULL; IMoniker *m; IBindCtx *bc; BIND_OPTS bo; STATSTG sg; HRESULT hr;
    NEED_FILE(); REQUIRE(U(StgCreateStorageEx)); MR_COM_BEGIN();
    t_tmpfile(path, sizeof path, "bts.stg");
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    hr = U(StgCreateStorageEx)(wp, STGM_SHARE_EXCLUSIVE | STGM_READWRITE | STGM_CREATE, STGFMT_STORAGE, 0, NULL, NULL, &mr_IID_IStorage, (void **)&st);
    if (hr != S_OK) { t_skip("cannot create storage fixture: %08lx", hr); MR_COM_END(); return; }
    IStorage_Release(st); st = NULL;
    m = mr_file(wp); bc = mr_bc();
    if (m && bc) {
        EXPECT_EQ(IMoniker_BindToStorage(m, NULL, NULL, &mr_IID_IStorage, (void **)&st), E_INVALIDARG);
        /* default bind options: STGM_READWRITE without a share mode is not a valid storage mode */
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IStorage, (void **)&st), STG_E_INVALIDFLAG);
        memset(&bo, 0, sizeof bo); bo.cbStruct = sizeof bo; bo.grfMode = STGM_READWRITE | STGM_SHARE_DENY_WRITE;
        EXPECT_EQ(IBindCtx_SetBindOptions(bc, &bo), S_OK);
        EXPECT_EQ(IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IStorage, (void **)&st), STG_E_INVALIDFLAG);
        bo.grfMode = STGM_READ | STGM_SHARE_DENY_WRITE;
        EXPECT_EQ(IBindCtx_SetBindOptions(bc, &bo), S_OK);
        st = NULL;
        hr = IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IStorage, (void **)&st);
        EXPECT_EQ(hr, S_OK);
        if (st) {
            memset(&sg, 0, sizeof sg);
            EXPECT_EQ(IStorage_Stat(st, &sg, STATFLAG_NONAME), S_OK);
            EXPECT_EQ(sg.grfMode, STGM_READ | STGM_SHARE_DENY_WRITE);
            IStorage_Release(st);
        }
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    DeleteFileA(path);
    MR_COM_END();
}

TEST(mr_file_bind_to_storage_missing_file) {
    IMoniker *m; IBindCtx *bc; BIND_OPTS bo; IStorage *st = NULL; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"nostg.stg"); bc = mr_bc();
    if (m && bc) {
        memset(&bo, 0, sizeof bo); bo.cbStruct = sizeof bo; bo.grfMode = STGM_READ | STGM_SHARE_DENY_WRITE;
        IBindCtx_SetBindOptions(bc, &bo);
        hr = IMoniker_BindToStorage(m, bc, NULL, &mr_IID_IStorage, (void **)&st);
        CHECKF(FAILED(hr) && st == NULL, "hr=%08lx st=%p", hr, st);
        t_note("BindToStorage(missing file) -> %08lx", hr);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_bind_to_object_missing_file) {
    IMoniker *m; IBindCtx *bc; IUnknown *u = (IUnknown *)1; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"noobj.doc"); bc = mr_bc();
    if (m && bc) {
        if (!t_is_wine()) EXPECT_EQ(IMoniker_BindToObject(m, NULL, NULL, &mr_IID_IUnknown, (void **)&u), E_INVALIDARG);
        hr = IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u);
        CHECKF(FAILED(hr), "hr=%08lx", hr);
        t_note("BindToObject(missing file) -> %08lx", hr);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

/* ---------------------------------------------------------------- prefix / relative path */
TEST(mr_file_common_prefix_identical) {
    IMoniker *a, *b, *p = NULL; NEED_FILE(); MR_COM_BEGIN();
    a = file_at(L"cp\\x.txt"); b = file_at(L"CP\\X.TXT");
    if (a && b) {
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, &p), MK_S_US);
        if (p) { CHECK(mr_is_equal(p, a)); IMoniker_Release(p); }
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, b, NULL), E_INVALIDARG);
        EXPECT_EQ(IMoniker_CommonPrefixWith(a, NULL, &p), E_INVALIDARG);
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_file_common_prefix_siblings) {
    IMoniker *a, *b, *p = NULL; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    a = mr_file(L"C:\\olet_mr_cp\\one\\a.txt"); b = mr_file(L"C:\\olet_mr_cp\\two\\b.txt");
    if (a && b) {
        hr = IMoniker_CommonPrefixWith(a, b, &p);
        EXPECT_EQ(hr, S_OK);
        if (p) {
            WCHAR *s = NULL; IBindCtx *bc = mr_bc();
            CHECK_TYPE(p, MKSYS_FILEMONIKER);
            IMoniker_GetDisplayName(p, bc, NULL, &s);
            CHECKF(s && !_wcsnicmp(s, L"C:\\olet_mr_cp\\", 14), "prefix \"%ls\"", s ? s : L"-");
            t_note("common prefix of sibling files: \"%ls\"", s ? s : L"-");
            if (s) U(CoTaskMemFree)(s);
            IBindCtx_Release(bc); IMoniker_Release(p);
        }
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

TEST(mr_file_relative_path_to) {
    IMoniker *a, *b, *r = NULL; HRESULT hr; NEED_FILE(); MR_COM_BEGIN();
    a = mr_file(L"C:\\olet_mr_rp\\one\\a.txt"); b = mr_file(L"C:\\olet_mr_rp\\two\\b.txt");
    if (a && b) {
        EXPECT_EQ(IMoniker_RelativePathTo(a, NULL, &r), E_INVALIDARG);
        EXPECT_EQ(IMoniker_RelativePathTo(a, b, NULL), E_INVALIDARG);
        hr = IMoniker_RelativePathTo(a, b, &r);
        t_note("RelativePathTo(sibling dirs) -> %08lx", hr);
        if (hr == S_OK && r) { CHECK_TYPE(r, MKSYS_FILEMONIKER); CHECK_DN(r, L"..\\..\\two\\b.txt"); IMoniker_Release(r); }
    }
    mr_rel(a); mr_rel(b);
    MR_COM_END();
}

/* ---------------------------------------------------------------- misc */
TEST(mr_file_parse_display_name_is_not_supported) {
    IMoniker *m, *o = (IMoniker *)1; IBindCtx *bc; ULONG eaten = 7; HRESULT hr; WCHAR s[] = L"!x"; NEED_FILE(); MR_COM_BEGIN();
    m = file_at(L"pdn.txt"); bc = mr_bc();
    if (m && bc) {
        hr = IMoniker_ParseDisplayName(m, bc, NULL, s, &eaten, &o);
        CHECKF(FAILED(hr), "hr=%08lx", hr);
        t_note("File moniker ParseDisplayName -> %08lx eaten=%lu", hr, eaten);
    }
    mr_rel(m); if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_file_many_monikers_released) {
    NEED_FILE(); MR_COM_BEGIN();
    for (int i = 0; i < 2000; i++) {
        WCHAR b[96]; IMoniker *m = NULL; wsprintfW(b, L"C:\\olet_mr_many\\%d.txt", i);
        if (U(CreateFileMoniker)(b, &m) != S_OK || !m) { CHECKF(0, "create %d", i); break; }
        if (IMoniker_Release(m) != 0) { CHECKF(0, "leak %d", i); break; }
    }
    MR_COM_END();
}
