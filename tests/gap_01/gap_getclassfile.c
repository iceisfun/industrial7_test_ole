#include "g1.h"
/* GetClassFile (ole32.dll) -> GetClassFileEx(file, clsid, &g1_null). Decomp facts:
 *   NULL clsid -> E_INVALIDARG before the file is looked at; NULL file or a file that cannot be opened for reading
 *   (CreateFileW with FILE_SHARE_READ|WRITE, OPEN_EXISTING) -> MK_E_CANTOPENFILE; every failure zeroes *clsid.
 *   Compound file -> CLSID stored by WriteClassStg/SetClass (when non-NULL); otherwise the FileType patterns and
 *   finally the extension (.ext -> ProgID -> CLSID); no match -> MK_E_INVALIDEXTENSION. */

static const GUID g1_null = { 0 };
#define GCF U(GetClassFile)
#define E_CANTOPEN ((HRESULT)0x800401EAL)      /* MK_E_CANTOPENFILE */
#define E_BADEXT   ((HRESULT)0x800401E6L)      /* MK_E_INVALIDEXTENSION */

static const GUID clsid_a = { 0x7e57c1a5, 0x0001, 0x4a1a, { 0x91, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x01 } };
static const GUID clsid_b = { 0xfeedface, 0xabcd, 0x1234, { 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88 } };
static const GUID clsid_c = { 0x00000001, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID clsid_d = { 0xffffffff, 0xffff, 0xffff, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff } };

static void path_w(WCHAR *w, const char *leaf) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf); g1_wide(p, w, MAX_PATH); }

/* Create a compound file carrying `cls` (when non-NULL). Returns 1 on success. */
static int make_docfile(const WCHAR *path, const GUID *cls)
{
    IStorage *stg = NULL; HRESULT hr;
    hr = U(StgCreateDocfile)(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    if (FAILED(hr) || !stg) return 0;
    if (cls) { hr = U(WriteClassStg)(stg, cls); if (FAILED(hr)) { stg->lpVtbl->Release(stg); return 0; } }
    stg->lpVtbl->Commit(stg, STGC_DEFAULT);
    stg->lpVtbl->Release(stg);
    return 1;
}

static int make_raw(const WCHAR *path, const void *data, DWORD n)
{
    DWORD w; HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (n) WriteFile(h, data, n, &w, NULL);
    CloseHandle(h);
    return 1;
}

TEST(gap1_getclassfile_null_clsid)
{
    WCHAR p[MAX_PATH]; HRESULT hr;
    REQUIRE(GCF);
    path_w(p, "gcf_nullclsid.bin");
    hr = GCF(p, NULL); EXPECT_EQ(hr, E_INVALIDARG);
    hr = GCF(NULL, NULL); EXPECT_EQ(hr, E_INVALIDARG);
    hr = GCF(L"", NULL); EXPECT_EQ(hr, E_INVALIDARG);
}

TEST(gap1_getclassfile_null_path)
{
    GUID g; HRESULT hr;
    REQUIRE(GCF);
    memset(&g, 0xaa, sizeof g);
    hr = GCF(NULL, &g); EXPECT_EQ(hr, E_CANTOPEN);
    CHECK(IsEqualGUID(&g, &g1_null));
}

TEST(gap1_getclassfile_unopenable_paths)
{
    WCHAR p[MAX_PATH], dir[MAX_PATH]; GUID g; HRESULT hr; char d[MAX_PATH];
    REQUIRE(GCF);
    path_w(p, "gcf_missing.doc");
    memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, E_CANTOPEN); CHECK(IsEqualGUID(&g, &g1_null));
    memset(&g, 0xaa, sizeof g); hr = GCF(L"", &g);
    EXPECT_EQ(hr, E_CANTOPEN); CHECK(IsEqualGUID(&g, &g1_null));
    /* a directory cannot be opened (no FILE_FLAG_BACKUP_SEMANTICS) */
    snprintf(d, sizeof d, "%s", t_tmpdir()); g1_wide(d, dir, MAX_PATH);
    memset(&g, 0xaa, sizeof g); hr = GCF(dir, &g);
    EXPECT_EQ(hr, E_CANTOPEN); CHECK(IsEqualGUID(&g, &g1_null));
    memset(&g, 0xaa, sizeof g); hr = GCF(L"\\\\?\\Z:\\olet_no_such_dir\\x.doc", &g);
    EXPECT_EQ(hr, E_CANTOPEN); CHECK(IsEqualGUID(&g, &g1_null));
}

TEST(gap1_getclassfile_sharing_violation)
{
    WCHAR p[MAX_PATH]; GUID g; HRESULT hr; HANDLE h;
    REQUIRE(GCF);
    path_w(p, "gcf_locked.bin");
    CHECK(make_raw(p, "hello", 5));
    h = CreateFileW(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);       /* deny everyone */
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, E_CANTOPEN); CHECK(IsEqualGUID(&g, &g1_null));
    CloseHandle(h);
    /* once released the same file is openable and is simply "unknown extension" */
    hr = GCF(p, &g); EXPECT_EQ(hr, E_BADEXT);
}

TEST(gap1_getclassfile_docfile_class)
{
    static const GUID *const tab[] = { &clsid_a, &clsid_b, &clsid_c, &clsid_d };
    unsigned i;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg));
    for (i = 0; i < 4; i++) {
        WCHAR p[MAX_PATH]; char leaf[32]; GUID g; HRESULT hr;
        snprintf(leaf, sizeof leaf, "gcf_doc%u.olet_xyz", i);   /* extension is irrelevant for compound files */
        path_w(p, leaf);
        CHECK(make_docfile(p, tab[i]));
        memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
        EXPECT_EQ(hr, S_OK); CHECK(IsEqualGUID(&g, tab[i]));
    }
}

TEST(gap1_getclassfile_docfile_extension_irrelevant)
{
    static const char *const ext[] = { "", ".", ".bin", ".doc", ".OLET_UPPER", ".a.b.c" };
    unsigned i;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg));
    for (i = 0; i < sizeof ext / sizeof *ext; i++) {
        WCHAR p[MAX_PATH]; char leaf[48]; GUID g; HRESULT hr;
        snprintf(leaf, sizeof leaf, "gcf_ext%u%s", i, ext[i]);
        path_w(p, leaf);
        CHECK(make_docfile(p, &clsid_a));
        memset(&g, 0, sizeof g); hr = GCF(p, &g);
        CHECKF(hr == S_OK, "'%s' hr %lx", ext[i], hr); CHECK(IsEqualGUID(&g, &clsid_a));
    }
}

TEST(gap1_getclassfile_docfile_without_class)
{
    /* a compound file whose class is CLSID_NULL falls through to the extension lookup */
    WCHAR p[MAX_PATH]; GUID g; HRESULT hr;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile));
    path_w(p, "gcf_noclass.olet_nosuchext");
    CHECK(make_docfile(p, NULL));
    memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, E_BADEXT); CHECK(IsEqualGUID(&g, &g1_null));
    /* explicitly writing CLSID_NULL is the same thing */
    path_w(p, "gcf_nullclass.olet_nosuchext");
    CHECK(make_docfile(p, &g1_null));
    memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, E_BADEXT); CHECK(IsEqualGUID(&g, &g1_null));
}

TEST(gap1_getclassfile_docfile_long_prefix_and_readonly)
{
    WCHAR p[MAX_PATH], q[MAX_PATH + 8]; GUID g; HRESULT hr;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg));
    path_w(p, "gcf_prefixed.olet_xyz");
    CHECK(make_docfile(p, &clsid_b));
    wcscpy(q, L"\\\\?\\"); wcscat(q, p);
    memset(&g, 0, sizeof g); hr = GCF(q, &g);
    EXPECT_EQ(hr, S_OK); CHECK(IsEqualGUID(&g, &clsid_b));
    /* read-only attribute does not matter: the file is only read */
    CHECK(SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY));
    memset(&g, 0, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, S_OK); CHECK(IsEqualGUID(&g, &clsid_b));
    SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
}

TEST(gap1_getclassfile_docfile_open_elsewhere_shared)
{
    /* the file is opened with FILE_SHARE_READ|WRITE, so another reader/writer that allows both does not block it */
    WCHAR p[MAX_PATH]; GUID g; HRESULT hr; HANDLE h;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg));
    path_w(p, "gcf_shared.olet_xyz");
    CHECK(make_docfile(p, &clsid_c));
    h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    memset(&g, 0, sizeof g); hr = GCF(p, &g);
    EXPECT_EQ(hr, S_OK); CHECK(IsEqualGUID(&g, &clsid_c));
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

TEST(gap1_getclassfile_plain_file_unknown_extension)
{
    static const struct { const char *leaf; const char *data; DWORD n; } t[] = {
        { "gcf_p0.olet_nosuchext", "", 0 },
        { "gcf_p1.olet_nosuchext", "plain text\r\n", 12 },
        { "gcf_p2", "no extension at all", 19 },
        { "gcf_p3.", "trailing dot", 12 },
        { "gcf_p4.olet_nosuchext", "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1", 8 },   /* compound file signature only: not a storage */
        { "gcf_p5.olet_nosuchext", "\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 24 },
    };
    unsigned i;
    REQUIRE(GCF);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR p[MAX_PATH]; GUID g; HRESULT hr;
        path_w(p, t[i].leaf);
        CHECK(make_raw(p, t[i].data, t[i].n));
        memset(&g, 0xaa, sizeof g); hr = GCF(p, &g);
        CHECKF(hr == E_BADEXT, "%s hr %lx", t[i].leaf, hr);
        CHECKF(IsEqualGUID(&g, &g1_null), "%s clsid not zeroed", t[i].leaf);
    }
}

TEST(gap1_getclassfile_hresult_values)
{
    /* the two documented failure codes have these exact values */
    EXPECT_EQ(E_CANTOPEN, MK_E_CANTOPENFILE);
    EXPECT_EQ(E_BADEXT, MK_E_INVALIDEXTENSION);
}

TEST(gap1_getclassfile_matches_storage_api)
{
    /* for compound files GetClassFile and ReadClassStg(StgOpenStorage) agree */
    WCHAR p[MAX_PATH]; IStorage *stg = NULL; GUID viaStg, viaGcf; HRESULT hr;
    REQUIRE(GCF); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg)); REQUIRE(U(StgOpenStorage)); REQUIRE(U(ReadClassStg));
    path_w(p, "gcf_agree.olet_xyz");
    CHECK(make_docfile(p, &clsid_d));
    hr = U(StgOpenStorage)(p, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &stg);
    CHECK(SUCCEEDED(hr)); if (FAILED(hr)) return;
    hr = U(ReadClassStg)(stg, &viaStg); EXPECT_EQ(hr, S_OK);
    stg->lpVtbl->Release(stg);
    hr = GCF(p, &viaGcf); EXPECT_EQ(hr, S_OK);
    CHECK(IsEqualGUID(&viaStg, &viaGcf));
}
