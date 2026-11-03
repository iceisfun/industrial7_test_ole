/* StgCreateDocfile: STGM validity matrix, existing-file policy, DELETEONRELEASE, temp files,
 * sharing, error mapping, and basic properties of the file produced. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define DW    STGM_SHARE_DENY_WRITE
#define DN    STGM_SHARE_DENY_NONE
#define RW    STGM_READWRITE

/* ---- flag validity table, file does not exist ------------------------------------------------ */
typedef struct { DWORD mode; HRESULT hr; const char *what; } flagcase;

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_create_storage_modes */
static const flagcase bad_flags[] = {
    { 0,                                         STG_E_INVALIDFLAG, "0" },
    { STGM_CREATE,                               STG_E_INVALIDFLAG, "CREATE" },
    { STGM_CREATE | RW,                          STG_E_INVALIDFLAG, "CREATE|RW" },
    { STGM_CREATE | EXCL,                        STG_E_INVALIDFLAG, "CREATE|EXCL" },
    { STGM_CREATE | DW | RW,                     STG_E_INVALIDFLAG, "CREATE|DENY_WRITE|RW" },
    { EXCL | STGM_READ,                          STG_E_INVALIDFLAG, "EXCL|READ" },
    { STGM_PRIORITY,                             STG_E_INVALIDFLAG, "PRIORITY" },
    { EXCL | RW | STGM_TRANSACTED | STGM_WRITE,  STG_E_INVALIDFLAG, "EXCL|RW|TRANSACTED|WRITE" },
    { EXCL | RW | STGM_TRANSACTED | 0x8,         STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x8" },
    { EXCL | RW | STGM_TRANSACTED | 0x80,        STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x80" },
    { EXCL | RW | STGM_TRANSACTED | 0x800,       STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x800" },
    { EXCL | RW | STGM_TRANSACTED | 0x8000,      STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x8000" },
    { EXCL | RW | STGM_TRANSACTED | 0x80000,     STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x80000" },
    { EXCL | RW | STGM_TRANSACTED | 0x800000,    STG_E_INVALIDFLAG, "EXCL|RW|TRANS|0x800000" },
    { STGM_READ,                                 STG_E_INVALIDFLAG, "READ" },
    { EXCL,                                      STG_E_INVALIDFLAG, "EXCL" },
    { DW,                                        STG_E_INVALIDFLAG, "DENY_WRITE" },
    { DN,                                        STG_E_INVALIDFLAG, "DENY_NONE" },
    { DN | STGM_TRANSACTED,                      STG_E_INVALIDFLAG, "DENY_NONE|TRANSACTED" },
    { DN | RW,                                   STG_E_INVALIDFLAG, "DENY_NONE|RW" },
    { DN | STGM_WRITE,                           STG_E_INVALIDFLAG, "DENY_NONE|WRITE" },
    { DW | STGM_WRITE,                           STG_E_INVALIDFLAG, "DENY_WRITE|WRITE" },
    { DW | STGM_READ,                            STG_E_INVALIDFLAG, "DENY_WRITE|READ" },
    { STGM_TRANSACTED | DW | STGM_READ,          STG_E_INVALIDFLAG, "TRANS|DENY_WRITE|READ" },
    { STGM_CREATE | EXCL | STGM_READ,            STG_E_INVALIDFLAG, "CREATE|EXCL|READ" },
};

TEST(sco_createdocfile_flags_invalid_table) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "flg_bad.stg"); sco_rm(p);
    for (unsigned i = 0; i < SCO_ARRAY(bad_flags); i++) {
        IStorage *stg = NULL;
        HRESULT hr = U(StgCreateDocfile)(p, bad_flags[i].mode, 0, &stg);
        CHECKF(hr == bad_flags[i].hr, "%s: got 0x%08lx", bad_flags[i].what, hr);
        CHECKF(stg == NULL, "%s: out pointer set", bad_flags[i].what);
        CHECKF(!sco_exists(p), "%s: file was created", bad_flags[i].what);
        if (stg) sco_rel(stg);
        sco_rm(p);
    }
}

static const flagcase good_flags[] = {
    { EXCL | RW,                                       S_OK, "EXCL|RW" },
    { EXCL | RW | STGM_TRANSACTED,                     S_OK, "EXCL|RW|TRANSACTED" },
    { STGM_CREATE | EXCL | RW | STGM_TRANSACTED,       S_OK, "CREATE|EXCL|RW|TRANSACTED" },
    { STGM_CREATE | RW | STGM_TRANSACTED,              S_OK, "CREATE|RW|TRANSACTED (no share)" },
    { STGM_TRANSACTED | STGM_CREATE | DW | RW,         S_OK, "TRANSACTED|CREATE|DENY_WRITE|RW (excel)" },
    { STGM_CREATE | DN | RW | STGM_TRANSACTED,         S_OK, "CREATE|DENY_NONE|RW|TRANSACTED (wmp)" },
    { STGM_TRANSACTED | DW | RW,                       S_OK, "TRANSACTED|DENY_WRITE|RW" },
    { STGM_TRANSACTED | STGM_CREATE | DW | STGM_WRITE, S_OK, "TRANSACTED|CREATE|DENY_WRITE|WRITE" },
    { STGM_CREATE | EXCL | RW,                         S_OK, "CREATE|EXCL|RW (powerpoint)" },
    { STGM_DIRECT | RW | EXCL,                         S_OK, "DIRECT|RW|EXCL (msi)" },
    { STGM_FAILIFTHERE | RW | EXCL,                    S_OK, "FAILIFTHERE|RW|EXCL" },
};

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_create_storage_modes */
TEST(sco_createdocfile_flags_valid_table) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "flg_good.stg"); sco_rm(p);
    for (unsigned i = 0; i < SCO_ARRAY(good_flags); i++) {
        IStorage *stg = NULL;
        HRESULT hr = U(StgCreateDocfile)(p, good_flags[i].mode, 0, &stg);
        CHECKF(hr == S_OK, "%s: got 0x%08lx", good_flags[i].what, hr);
        if (hr == S_OK) {
            ULONG rc = IStorage_Release(stg);
            CHECKF(rc == 0, "%s: release returned %lu", good_flags[i].what, rc);
            CHECKF(sco_exists(p), "%s: file missing after release", good_flags[i].what);
        }
        sco_rm(p);
    }
}

TEST(sco_createdocfile_write_only_no_trans_no_create) {
    REQUIRE(U(StgCreateDocfile));
    /* Windows is picky about flag combinations; record the outcome only. */
    WCHAR p[MAX_PATH]; sco_wpath(p, "wo.stg"); sco_rm(p);
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(p, EXCL | STGM_WRITE, 0, &stg);
    t_note("StgCreateDocfile(EXCL|WRITE) on missing file -> 0x%08lx", hr);
    sco_rel(stg); sco_rm(p);
}

/* ---- parameters ------------------------------------------------------------------------------ */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_create_storage_modes */
TEST(sco_createdocfile_null_out_pointer) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "nullout.stg"); sco_rm(p);
    HRESULT hr = U(StgCreateDocfile)(p, SCO_RW_EXCL, 0, NULL);
    EQ(hr, STG_E_INVALIDPOINTER);
    CHECK(!sco_exists(p));
    sco_rm(p);
}

TEST(sco_createdocfile_reserved_nonzero) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "reserved.stg"); sco_rm(p);
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(p, SCO_RW_EXCL, 1, &stg);
    EQ(hr, STG_E_INVALIDPARAMETER);
    CHECK(stg == NULL);
    CHECK(!sco_exists(p));
    hr = U(StgCreateDocfile)(p, SCO_RW_EXCL, 0xFFFFFFFF, &stg);
    EQ(hr, STG_E_INVALIDPARAMETER);
    sco_rel(stg); sco_rm(p);
}

TEST(sco_createdocfile_null_name_zero_mode) {
    REQUIRE(U(StgCreateDocfile));
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(NULL, 0, 0, &stg);
    EQ(hr, STG_E_INVALIDFLAG);
    CHECK(stg == NULL);
    sco_rel(stg);
}

TEST(sco_createdocfile_empty_name) {
    REQUIRE(U(StgCreateDocfile));
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(L"", SCO_RW_EXCL, 0, &stg);
    CHECK(FAILED(hr));
    t_note("StgCreateDocfile(\"\") -> 0x%08lx", hr);
    sco_rel(stg);
}

/* ---- existing file policy -------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_create_storage_modes */
TEST(sco_createdocfile_existing_failifthere) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "exist.stg"); sco_rm(p);
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, EXCL | RW, &stg);
    EQ(hr, S_OK);
    EQ(sco_rel(stg), 0);
    LONGLONG sz = sco_size(p);

    DWORD modes[] = { EXCL | RW, EXCL | RW | STGM_TRANSACTED, EXCL | RW | STGM_TRANSACTED | STGM_FAILIFTHERE,
                      EXCL | STGM_WRITE, DW | RW | STGM_TRANSACTED };
    for (unsigned i = 0; i < SCO_ARRAY(modes); i++) {
        hr = sco_create(p, modes[i], &stg);
        CHECKF(hr == STG_E_FILEALREADYEXISTS, "mode 0x%lx -> 0x%08lx", modes[i], hr);
        CHECK(stg == NULL);
        sco_rel(stg);
        CHECKF(sco_size(p) == sz, "mode 0x%lx changed file size", modes[i]);
    }
    sco_rm(p);
}

TEST(sco_createdocfile_existing_create_truncates) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "trunc.stg"); sco_rm(p);
    IStorage *stg = NULL;
    BYTE data[3000]; memset(data, 0x5a, sizeof data);
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    EQ(sco_put_stream(stg, L"big", data, sizeof data), S_OK);
    EQ(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    EQ(sco_rel(stg), 0);

    /* CREATE replaces the contents: the old stream must be gone */
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    IStream *st = NULL;
    HRESULT hr = IStorage_OpenStream(stg, L"big", NULL, STGM_READ | EXCL, 0, &st);
    EQ(hr, STG_E_FILENOTFOUND);
    CHECK(st == NULL);
    sco_relst(st);
    EQ(sco_rel(stg), 0);
    sco_rm(p);
}

TEST(sco_createdocfile_existing_zero_length_file) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "zero.stg"); sco_rm(p);
    CHECK(sco_write_file(p, NULL, 0));
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, EXCL | RW, &stg);
    EQ(hr, STG_E_FILEALREADYEXISTS);
    sco_rel(stg);
    CHECK(sco_size(p) == 0);
    hr = sco_create(p, SCO_RW_EXCL, &stg);
    EQ(hr, S_OK);
    EQ(sco_rel(stg), 0);
    CHECK(sco_size(p) > 0);
    sco_rm(p);
}

TEST(sco_createdocfile_overwrites_non_docfile) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "overtxt.stg"); sco_rm(p);
    CHECK(sco_write_file(p, "hello, not a docfile", 20));
    IStorage *stg = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    EQ(sco_rel(stg), 0);
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n >= 512);
    if (n >= 8) CHECK(!memcmp(b, sco_sig, 8));
    free(b);
    sco_rm(p);
}

/* ---- DELETEONRELEASE ------------------------------------------------------------------------- */
TEST(sco_createdocfile_deleteonrelease_named) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dor.stg"); sco_rm(p);
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL | STGM_DELETEONRELEASE, &stg);
    EQ(hr, S_OK);
    CHECK(sco_exists(p));
    EQ(sco_rel(stg), 0);
    CHECK(!sco_exists(p));
    sco_rm(p);
}

TEST(sco_createdocfile_deleteonrelease_transacted) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dor_tx.stg"); sco_rm(p);
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL | STGM_TRANSACTED | STGM_DELETEONRELEASE, &stg);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        EQ(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
        EQ(sco_rel(stg), 0);
        CHECK(!sco_exists(p));
    }
    sco_rm(p);
}

TEST(sco_createdocfile_deleteonrelease_second_ref) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dor_ref.stg"); sco_rm(p);
    IStorage *stg = NULL;
    EQ(sco_create(p, SCO_RW_EXCL | STGM_DELETEONRELEASE, &stg), S_OK);
    if (!stg) return;
    EQ(IStorage_AddRef(stg), 2);
    EQ(IStorage_Release(stg), 1);
    CHECK(sco_exists(p));                    /* still referenced */
    EQ(IStorage_Release(stg), 0);
    CHECK(!sco_exists(p));
    sco_rm(p);
}

/* adapted from Wine dlls/ole32/tests/ole2.c (LGPL-2.1+) uses of StgCreateDocfile(NULL,...) */
TEST(sco_createdocfile_tempfile_null_name) {
    REQUIRE(U(StgCreateDocfile));
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(NULL, STGM_READWRITE | EXCL | STGM_CREATE | STGM_DELETEONRELEASE, 0, &stg);
    EQ(hr, S_OK);
    if (hr != S_OK) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    EQ(st.type, STGTY_STORAGE);
    CHECK(st.pwcsName == NULL);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLSID_NULL));
    IStream *s = NULL;
    EQ(IStorage_CreateStream(stg, L"s", STGM_READWRITE | EXCL, 0, 0, &s), S_OK);
    ULONG w = 0; EQ(IStream_Write(s, "abc", 3, &w), S_OK); EQ(w, 3);
    sco_relst(s);
    EQ(sco_rel(stg), 0);
}

TEST(sco_createdocfile_tempfile_no_create_flag) {
    REQUIRE(U(StgCreateDocfile));
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(NULL, STGM_READWRITE | EXCL | STGM_DELETEONRELEASE, 0, &stg);
    t_note("NULL name without STGM_CREATE -> 0x%08lx", hr);
    sco_rel(stg);
    hr = U(StgCreateDocfile)(NULL, STGM_READWRITE | EXCL | STGM_CREATE, 0, &stg);
    t_note("NULL name without DELETEONRELEASE -> 0x%08lx", hr);
    sco_rel(stg);
}

TEST(sco_createdocfile_tempfile_stat_name) {
    REQUIRE(U(StgCreateDocfile));
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfile)(NULL, STGM_READWRITE | EXCL | STGM_CREATE | STGM_DELETEONRELEASE, 0, &stg);
    if (hr != S_OK) { t_skip("no temp docfile"); return; }
    STATSTG st; memset(&st, 0, sizeof st);
    EQ(IStorage_Stat(stg, &st, STATFLAG_DEFAULT), S_OK);
    t_note("temp docfile STATSTG.pwcsName = %ls, grfMode = 0x%lx", st.pwcsName ? st.pwcsName : L"(null)", st.grfMode);
    if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
    sco_rel(stg);
}

/* ---- Windows error mapping ------------------------------------------------------------------- */
TEST(sco_createdocfile_path_not_found) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "nodir_xyz\\sub\\f.stg");
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &stg);
    EQ(hr, STG_E_PATHNOTFOUND);
    CHECK(stg == NULL);
    sco_rel(stg);
}

TEST(sco_createdocfile_readonly_attribute_file) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "ro_attr.stg"); sco_rm(p);
    IStorage *stg = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    EQ(sco_rel(stg), 0);
    SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY);
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &stg);
    EQ(hr, STG_E_ACCESSDENIED);
    sco_rel(stg);
    hr = sco_open(p, EXCL | RW, &stg);
    EQ(hr, STG_E_ACCESSDENIED);
    sco_rel(stg);
    /* read access to a read-only file is fine */
    hr = sco_open(p, EXCL | STGM_READ, &stg);
    EQ(hr, S_OK);
    sco_rel(stg);
    sco_rm(p);
}

TEST(sco_createdocfile_name_is_directory) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dirname");
    CreateDirectoryW(p, NULL);
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &stg);
    CHECK(FAILED(hr));
    t_note("StgCreateDocfile on a directory path -> 0x%08lx", hr);
    sco_rel(stg);
    RemoveDirectoryW(p);
}

TEST(sco_createdocfile_invalid_chars_name) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "bad<name>?.stg");
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &stg);
    CHECK(FAILED(hr));
    t_note("StgCreateDocfile with wildcard chars -> 0x%08lx", hr);
    sco_rel(stg);
}

TEST(sco_createdocfile_name_too_long) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[1200];
    sco_wpath(p, "x");
    size_t l = wcslen(p);
    for (int i = 0; i < 300 && l + 2 < 1190; i++) p[l++] = L'a';
    p[l] = 0;
    IStorage *stg = NULL;
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &stg);
    CHECK(FAILED(hr));
    t_note("StgCreateDocfile with 300-char leaf -> 0x%08lx", hr);
    sco_rel(stg);
}

/* ---- sharing --------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_open_storage */
TEST(sco_createdocfile_exclusive_blocks_open) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "excl.stg"); sco_rm(p);
    IStorage *a = NULL, *b = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &a), S_OK);
    HRESULT hr = sco_open(p, EXCL | STGM_READ, &b);
    EQ(hr, STG_E_SHAREVIOLATION);
    CHECK(b == NULL);
    hr = sco_open(p, DW | STGM_READ, &b);
    CHECKF(FAILED(hr), "open with DENY_WRITE while EXCLUSIVE creator holds it: 0x%08lx", hr);
    sco_rel(b);
    hr = sco_create(p, SCO_RW_EXCL, &b);
    CHECKF(FAILED(hr), "second create over open exclusive: 0x%08lx", hr);
    t_note("create over exclusively-held docfile -> 0x%08lx", hr);
    sco_rel(b);
    EQ(sco_rel(a), 0);
    sco_rm(p);
}

TEST(sco_createdocfile_deny_write_allows_reader) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dw.stg"); sco_rm(p);
    IStorage *a = NULL, *b = NULL, *c = NULL;
    EQ(sco_create(p, STGM_CREATE | DW | RW | STGM_TRANSACTED, &a), S_OK);
    HRESULT hr = sco_open(p, DW | STGM_READ, &b);
    t_note("open READ|DENY_WRITE while DENY_WRITE writer is open -> 0x%08lx", hr);
    sco_rel(b);
    hr = sco_open(p, DN | STGM_READ, &c);
    t_note("open READ|DENY_NONE while DENY_WRITE writer is open -> 0x%08lx", hr);
    sco_rel(c);
    sco_rel(a);
    sco_rm(p);
}

TEST(sco_createdocfile_deny_none_second_open) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dn.stg"); sco_rm(p);
    IStorage *a = NULL, *b = NULL;
    EQ(sco_create(p, STGM_CREATE | DN | RW | STGM_TRANSACTED, &a), S_OK);
    HRESULT hr = sco_open(p, DN | STGM_READ | STGM_TRANSACTED, &b);
    t_note("2nd open READ|DENY_NONE|TRANSACTED against DENY_NONE|TRANSACTED creator -> 0x%08lx", hr);
    sco_rel(b);
    sco_rel(a);
    sco_rm(p);
}

TEST(sco_createdocfile_file_deletable_after_release) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "del.stg"); sco_rm(p);
    IStorage *a = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &a), S_OK);
    SetLastError(0);
    BOOL ok = DeleteFileW(p);
    CHECKF(!ok, "file deletable while exclusively open (err %lu)", GetLastError());
    EQ(sco_rel(a), 0);
    CHECK(DeleteFileW(p));
}

/* ---- result object ---------------------------------------------------------------------------- */
TEST(sco_createdocfile_iunknown_identity_and_refcount) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "ident.stg"); sco_rm(p);
    IStorage *stg = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    if (!stg) return;
    IUnknown *u = NULL; IStorage *s2 = NULL; IStream *bad = (IStream *)1;
    EQ(IStorage_QueryInterface(stg, &sco_IID_IUnknown, (void **)&u), S_OK);
    EQ(IStorage_QueryInterface(stg, &sco_IID_IStorage, (void **)&s2), S_OK);
    CHECK((void *)s2 == (void *)stg);
    EQ(IUnknown_Release(u), 2);
    EQ(IStorage_Release(s2), 1);
    HRESULT hr = IStorage_QueryInterface(stg, &sco_IID_IStream, (void **)&bad);
    EQ(hr, E_NOINTERFACE);
    CHECK(bad == NULL);
    hr = IStorage_QueryInterface(stg, &sco_IID_IUnknown, NULL);
    CHECK(FAILED(hr));
    EQ(sco_rel(stg), 0);
    sco_rm(p);
}

TEST(sco_createdocfile_stat_after_create) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "stat1.stg"); sco_rm(p);
    IStorage *stg = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &stg), S_OK);
    if (!stg) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(stg, &st, STATFLAG_DEFAULT), S_OK);
    EQ(st.type, STGTY_STORAGE);
    CHECK(st.pwcsName != NULL);
    if (st.pwcsName) {
        /* Stat reports the full path of the file */
        CHECK(!lstrcmpiW(st.pwcsName, p));
        U(CoTaskMemFree)(st.pwcsName);
    }
    CHECK(sco_guid_eq(&st.clsid, &sco_CLSID_NULL));
    EQ(st.grfStateBits, 0);
    EQ(st.grfMode & STGM_CREATE, 0);
    EQ(st.grfMode & 0x73, (RW | EXCL) & 0x73);
    t_note("fresh docfile STATSTG: cbSize=%llu grfLocksSupported=0x%lx grfMode=0x%lx", (unsigned long long)st.cbSize.QuadPart, st.grfLocksSupported, st.grfMode);
    EQ(sco_rel(stg), 0);
    sco_rm(p);
}
