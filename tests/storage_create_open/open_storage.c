/* StgOpenStorage / StgIsStorageFile / simple mode / priority mode / snbExclude. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define DW    STGM_SHARE_DENY_WRITE
#define DN    STGM_SHARE_DENY_NONE
#define DR    STGM_SHARE_DENY_READ
#define RW    STGM_READWRITE

/* make a docfile with one stream + one substorage, committed, closed. */
static int mkdoc(const WCHAR *p) {
    IStorage *s = NULL, *sub = NULL;
    if (sco_create(p, SCO_RW_EXCL, &s) != S_OK) return 0;
    int ok = sco_put_stream(s, L"data", "0123456789", 10) == S_OK;
    ok = ok && IStorage_CreateStorage(s, L"sub", STGM_READWRITE | EXCL, 0, 0, &sub) == S_OK;
    if (sub) IStorage_Release(sub);
    ok = ok && IStorage_Commit(s, STGC_DEFAULT) == S_OK;
    IStorage_Release(s);
    return ok;
}

/* ---- StgIsStorageFile ------------------------------------------------------------------------ */
TEST(sco_isstoragefile_docfile) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf1.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    EQ(U(StgIsStorageFile)(p), S_OK);
    sco_rm(p);
}

TEST(sco_isstoragefile_not_found) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_nofile.stg"); sco_rm(p);
    EQ(U(StgIsStorageFile)(p), STG_E_FILENOTFOUND);
    CHECK(!sco_exists(p));
}

TEST(sco_isstoragefile_text_file) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_txt.stg"); sco_rm(p);
    char buf[1024]; memset(buf, 'x', sizeof buf);
    CHECK(sco_write_file(p, buf, sizeof buf));
    EQ(U(StgIsStorageFile)(p), S_FALSE);
    sco_rm(p);
}

TEST(sco_isstoragefile_short_and_empty_files) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_short.stg"); sco_rm(p);
    HRESULT hr;
    CHECK(sco_write_file(p, NULL, 0));
    hr = U(StgIsStorageFile)(p);
    t_note("StgIsStorageFile(empty file) -> 0x%08lx", hr);
    CHECK(hr == S_FALSE || FAILED(hr));
    CHECK(sco_write_file(p, sco_sig, 4));
    hr = U(StgIsStorageFile)(p);
    t_note("StgIsStorageFile(4-byte partial signature) -> 0x%08lx", hr);
    CHECK(hr != S_OK);
    CHECK(sco_write_file(p, sco_sig, 8));
    hr = U(StgIsStorageFile)(p);
    t_note("StgIsStorageFile(8-byte signature only) -> 0x%08lx", hr);
    sco_rm(p);
}

TEST(sco_isstoragefile_signature_only_is_s_ok_on_512_byte_header) {
    REQUIRE(U(StgIsStorageFile));
    /* The check looks at the 8-byte signature; a 512 byte block with the signature but garbage after it. */
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_sig.stg"); sco_rm(p);
    BYTE b[512]; memset(b, 0, sizeof b); memcpy(b, sco_sig, 8);
    CHECK(sco_write_file(p, b, sizeof b));
    HRESULT hr = U(StgIsStorageFile)(p);
    t_note("StgIsStorageFile(signature + zero header) -> 0x%08lx", hr);
    IStorage *s = NULL;
    hr = sco_open(p, EXCL | STGM_READ, &s);
    t_note("StgOpenStorage(signature + zero header) -> 0x%08lx", hr);
    CHECK(FAILED(hr));
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_isstoragefile_null_name) {
    REQUIRE(U(StgIsStorageFile));
    HRESULT hr = U(StgIsStorageFile)(NULL);
    CHECK(FAILED(hr));
    t_note("StgIsStorageFile(NULL) -> 0x%08lx", hr);
}

TEST(sco_isstoragefile_uncommitted_transacted) {
    REQUIRE(U(StgIsStorageFile));
    /* documented: a transacted root created but not yet committed still returns S_OK */
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_tx.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(sco_create(p, STGM_CREATE | RW | EXCL | STGM_TRANSACTED, &s), S_OK);
    EQ(U(StgIsStorageFile)(p), S_OK);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_isstoragefile_while_open_exclusive) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_open.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    HRESULT hr = U(StgIsStorageFile)(p);
    t_note("StgIsStorageFile on exclusively open docfile -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_isstoragefile_directory) {
    REQUIRE(U(StgIsStorageFile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "isf_dir");
    CreateDirectoryW(p, NULL);
    HRESULT hr = U(StgIsStorageFile)(p);
    CHECK(hr != S_OK);
    t_note("StgIsStorageFile(directory) -> 0x%08lx", hr);
    RemoveDirectoryW(p);
}

/* ---- StgOpenStorage: errors ------------------------------------------------------------------ */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_open_storage */
TEST(sco_openstorage_zero_length_file) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_zero.stg"); sco_rm(p);
    CHECK(sco_write_file(p, NULL, 0));
    IStorage *s = NULL;
    EQ(U(StgOpenStorage)(p, NULL, (STGM_NOSCRATCH | STGM_TRANSACTED | DW | RW), NULL, 0, &s), STG_E_FILEALREADYEXISTS);
    s = NULL;
    EQ(U(StgOpenStorage)(p, NULL, EXCL | RW, NULL, 0, &s), STG_E_FILEALREADYEXISTS);
    CHECK(s == NULL);
    CHECK(sco_size(p) == 0);
    sco_rm(p);
}

TEST(sco_openstorage_garbage_file) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_garb.stg"); sco_rm(p);
    BYTE g[2048]; for (int i = 0; i < 2048; i++) g[i] = (BYTE)(i * 7 + 3);
    CHECK(sco_write_file(p, g, sizeof g));
    IStorage *s = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &s), STG_E_FILEALREADYEXISTS);
    CHECK(s == NULL);
    EQ(sco_open(p, EXCL | RW, &s), STG_E_FILEALREADYEXISTS);
    CHECK(sco_size(p) == 2048);
    sco_rm(p);
}

TEST(sco_openstorage_signature_but_truncated) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_trunc.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n > 512);
    /* keep only the first 512 bytes (header sector): FAT/directory are gone */
    CHECK(sco_write_file(p, b, 512));
    free(b);
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, EXCL | STGM_READ, &s);
    CHECK(FAILED(hr));
    t_note("StgOpenStorage(header-only truncated docfile) -> 0x%08lx", hr);
    sco_rel(s);
    CHECK(sco_write_file(p, sco_sig, 8));
    hr = sco_open(p, EXCL | STGM_READ, &s);
    CHECK(FAILED(hr));
    t_note("StgOpenStorage(8 byte file) -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_openstorage_corrupt_header_fields) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH], q[MAX_PATH]; sco_wpath(p, "o_corrupt.stg"); sco_wpath(q, "o_corrupt2.stg");
    sco_rm(p); sco_rm(q);
    CHECK(mkdoc(p));
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n >= 512);
    if (n < 512) { free(b); sco_rm(p); return; }
    struct { int off; BYTE val; const char *what; } c[] = {
        { 0x1E, 0x0F, "sector shift 15" }, { 0x1E, 0x00, "sector shift 0" }, { 0x20, 0x0A, "mini shift 10" },
        { 0x1C, 0x00, "byte order 0" }, { 0x1A, 0x07, "major version 7" }, { 0x2C, 0xFF, "FAT sector count" },
    };
    for (unsigned i = 0; i < SCO_ARRAY(c); i++) {
        BYTE *m = (BYTE *)malloc(n); memcpy(m, b, n); m[c[i].off] = c[i].val;
        CHECK(sco_write_file(q, m, n));
        IStorage *s = NULL;
        HRESULT hr = sco_open(q, EXCL | STGM_READ, &s);
        t_note("StgOpenStorage(header: %s) -> 0x%08lx", c[i].what, hr);
        sco_rel(s); free(m);
    }
    free(b);
    sco_rm(p); sco_rm(q);
}

TEST(sco_openstorage_nonexistent_does_not_create) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_none.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &s), STG_E_FILENOTFOUND);
    CHECK(s == NULL);
    EQ(sco_open(p, STGM_DIRECT | EXCL | RW, &s), STG_E_FILENOTFOUND);
    EQ(sco_open(p, STGM_TRANSACTED | DW | RW, &s), STG_E_FILENOTFOUND);
    CHECK(!sco_exists(p));
}

TEST(sco_openstorage_path_not_found) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "no_such_dir\\x.stg");
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, EXCL | STGM_READ, &s);
    CHECK(hr == STG_E_PATHNOTFOUND || hr == STG_E_FILENOTFOUND);
    t_note("StgOpenStorage in missing directory -> 0x%08lx", hr);
}

typedef struct { DWORD mode; HRESULT hr; const char *what; } mcase;

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_open_storage */
static const mcase open_bad[] = {
    { 0,                               STG_E_INVALIDFLAG, "0" },
    { STGM_CREATE | EXCL | STGM_READ,  STG_E_INVALIDFLAG, "CREATE|EXCL|READ" },
    { DN | STGM_READ,                  STG_E_INVALIDFLAG, "DENY_NONE|READ (direct)" },
    { DR | STGM_READ,                  STG_E_INVALIDFLAG, "DENY_READ|READ" },
    { DW | RW,                         STG_E_INVALIDFLAG, "DENY_WRITE|RW (direct)" },
    { STGM_PRIORITY | RW,              STG_E_INVALIDFLAG, "PRIORITY|RW" },
    { STGM_TRANSACTED | STGM_PRIORITY, STG_E_INVALIDFLAG, "TRANSACTED|PRIORITY" },
    { STGM_SIMPLE | STGM_PRIORITY,     STG_E_INVALIDFLAG, "SIMPLE|PRIORITY" },
    { STGM_DELETEONRELEASE | STGM_PRIORITY, STG_E_INVALIDFUNCTION, "DELETEONRELEASE|PRIORITY" },
    { STGM_NOSCRATCH | STGM_PRIORITY,  STG_E_INVALIDFLAG, "NOSCRATCH|PRIORITY" },
    { STGM_NOSNAPSHOT | STGM_PRIORITY, STG_E_INVALIDFLAG, "NOSNAPSHOT|PRIORITY" },
};

TEST(sco_openstorage_flags_invalid_table) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_flags.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    for (unsigned i = 0; i < SCO_ARRAY(open_bad); i++) {
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, open_bad[i].mode, &s);
        CHECKF(hr == open_bad[i].hr, "%s: got 0x%08lx", open_bad[i].what, hr);
        CHECKF(s == NULL, "%s: out pointer set", open_bad[i].what);
        sco_rel(s);
    }
    sco_rm(p);
}

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_open_storage */
static const mcase open_good[] = {
    { DN | STGM_READ | STGM_TRANSACTED,          S_OK, "DENY_NONE|READ|TRANSACTED (XLViewer)" },
    { DW | STGM_READ,                           S_OK, "DENY_WRITE|READ" },
    { EXCL | STGM_READ,                         S_OK, "EXCL|READ" },
    { EXCL | RW,                                S_OK, "EXCL|RW" },
    { STGM_DIRECT | EXCL | RW,                  S_OK, "DIRECT|EXCL|RW" },
    { STGM_DIRECT | DW | STGM_READ,             S_OK, "DIRECT|DENY_WRITE|READ" },
    { STGM_NOSCRATCH | STGM_TRANSACTED | DW | RW, S_OK, "NOSCRATCH|TRANSACTED|DW|RW (word)" },
    { STGM_TRANSACTED | DW | RW,                S_OK, "TRANSACTED|DW|RW" },
    { STGM_TRANSACTED | EXCL | RW,              S_OK, "TRANSACTED|EXCL|RW" },
    { STGM_TRANSACTED | EXCL | STGM_READ,       S_OK, "TRANSACTED|EXCL|READ" },
    { STGM_PRIORITY | DN,                       S_OK, "PRIORITY|DENY_NONE (visio)" },
    { STGM_PRIORITY | EXCL,                     S_OK, "PRIORITY|EXCL" },
    { STGM_PRIORITY | DW,                       S_OK, "PRIORITY|DENY_WRITE" },
    { STGM_PRIORITY | DR,                       S_OK, "PRIORITY|DENY_READ" },
    { STGM_PRIORITY,                            S_OK, "PRIORITY (project)" },
};

TEST(sco_openstorage_flags_valid_table) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_good.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    for (unsigned i = 0; i < SCO_ARRAY(open_good); i++) {
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, open_good[i].mode, &s);
        CHECKF(hr == open_good[i].hr, "%s: got 0x%08lx", open_good[i].what, hr);
        if (hr == S_OK) { ULONG rc = IStorage_Release(s); CHECKF(rc == 0, "%s: rc %lu", open_good[i].what, rc); }
    }
    sco_rm(p);
}

TEST(sco_openstorage_null_name) {
    REQUIRE(U(StgOpenStorage));
    IStorage *s = NULL;
    EQ(U(StgOpenStorage)(NULL, NULL, EXCL, NULL, 0, &s), STG_E_INVALIDNAME);
    CHECK(s == NULL);
    EQ(U(StgOpenStorage)(NULL, NULL, EXCL | STGM_READ, NULL, 0, &s), STG_E_INVALIDNAME);
}

TEST(sco_openstorage_null_out) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_nullout.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    EQ(U(StgOpenStorage)(p, NULL, EXCL | STGM_READ, NULL, 0, NULL), STG_E_INVALIDPOINTER);
    sco_rm(p);
}

TEST(sco_openstorage_reserved_nonzero) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_res.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *s = NULL;
    EQ(U(StgOpenStorage)(p, NULL, EXCL | STGM_READ, NULL, 1, &s), STG_E_INVALIDPARAMETER);
    CHECK(s == NULL);
    sco_rm(p);
}

TEST(sco_openstorage_share_violation) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_share.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *a = NULL, *b = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &a), S_OK);
    EQ(sco_open(p, EXCL | STGM_READ, &b), STG_E_SHAREVIOLATION);
    CHECK(b == NULL);
    HRESULT hr = sco_open(p, DW | STGM_READ, &b);
    CHECK(FAILED(hr));
    t_note("DENY_WRITE|READ open against EXCL|READ holder -> 0x%08lx", hr);
    sco_rel(b);
    EQ(IStorage_Release(a), 0);
    /* after release, open works again */
    EQ(sco_open(p, EXCL | STGM_READ, &a), S_OK);
    sco_rel(a);
    sco_rm(p);
}

TEST(sco_openstorage_deny_write_readers_coexist) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_dwdw.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *a = NULL, *b = NULL;
    EQ(sco_open(p, DW | STGM_READ, &a), S_OK);
    EQ(sco_open(p, DW | STGM_READ, &b), S_OK);
    sco_rel(a); sco_rel(b);
    sco_rm(p);
}

TEST(sco_openstorage_read_only_storage_rejects_writes) {
    REQUIRE(U(StgOpenStorage));
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_open_storage / test_readonly */
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_ro.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *s = NULL, *s2 = NULL; IStream *st = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_CreateStream(s, L"winetest", STGM_WRITE | EXCL, 0, 0, &st), STG_E_ACCESSDENIED);
    CHECK(st == NULL);
    EQ(IStorage_CreateStorage(s, L"winetest", STGM_WRITE | EXCL, 0, 0, &s2), STG_E_ACCESSDENIED);
    EQ(IStorage_DestroyElement(s, L"data"), STG_E_ACCESSDENIED);
    EQ(IStorage_DestroyElement(s, L"nonexistent"), STG_E_ACCESSDENIED);
    HRESULT hr2 = IStorage_SetClass(s, &sco_CLS_A);
    CHECK(FAILED(hr2));
    t_note("SetClass on read-only storage -> 0x%08lx", hr2);
    hr2 = IStorage_RenameElement(s, L"data", L"data2");
    CHECK(FAILED(hr2));
    t_note("RenameElement on read-only storage -> 0x%08lx", hr2);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_openstorage_readwrite_roundtrip_content) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_rt.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *s = NULL; IStream *st = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_OpenStream(s, L"data", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    char buf[16] = {0}; ULONG n = 0;
    EQ(IStream_Read(st, buf, 16, &n), S_OK);
    EQ(n, 10);
    CHECK(!memcmp(buf, "0123456789", 10));
    sco_relst(st);
    IStorage *sub = NULL;
    EQ(IStorage_OpenStorage(s, L"sub", NULL, STGM_READ | EXCL, NULL, 0, &sub), S_OK);
    sco_rel(sub);
    sco_rel(s);
    sco_rm(p);
}

/* ---- priority mode and pstgPriority ----------------------------------------------------------- */
TEST(sco_openstorage_priority_then_normal) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_prio.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *pr = NULL, *s = NULL;
    EQ(sco_open(p, STGM_PRIORITY, &pr), S_OK);
    /* adapted from Wine dlls/ole32/tests/storage32.c: priority opens can be stacked */
    IStorage *pr2 = NULL;
    EQ(sco_open(p, STGM_PRIORITY, &pr2), S_OK);
    sco_rel(pr2);
    HRESULT hr = sco_open(p, DW | STGM_READ, &s);
    t_note("normal open while priority storage held -> 0x%08lx", hr);
    sco_rel(s);
    STATSTG st; memset(&st, 0, sizeof st);
    hr = IStorage_Stat(pr, &st, STATFLAG_DEFAULT);
    t_note("Stat on priority storage -> 0x%08lx grfMode=0x%lx", hr, st.grfMode);
    if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
    sco_rel(pr);
    sco_rm(p);
}

TEST(sco_openstorage_pstgpriority_overrides_name) {
    REQUIRE(U(StgOpenStorage));
    /* documented: when pstgPriority is non-NULL it specifies the file; pwcsName is ignored */
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_pp.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    IStorage *pr = NULL, *s = NULL;
    EQ(sco_open(p, STGM_PRIORITY, &pr), S_OK);
    HRESULT hr = U(StgOpenStorage)(NULL, pr, EXCL | STGM_READ, NULL, 0, &s);
    t_note("StgOpenStorage(NULL, pstgPriority, ...) -> 0x%08lx", hr);
    if (hr == S_OK) {
        IStream *st = NULL;
        EQ(IStorage_OpenStream(s, L"data", NULL, STGM_READ | EXCL, 0, &st), S_OK);
        sco_relst(st);
        sco_rel(s);
    }
    /* pstgPriority may have been released by the call: do not touch it again */
    sco_rm(p);
}

/* ---- snbExclude -------------------------------------------------------------------------------- */
TEST(sco_openstorage_snbexclude_empties_stream) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "o_snb.stg"); sco_rm(p);
    CHECK(mkdoc(p));
    /* SNB is a double-NUL terminated list of names; documented: excluded streams have length zero */
    WCHAR names[] = L"data\0\0";
    WCHAR *snb[2] = { names, NULL };
    IStorage *s = NULL;
    HRESULT hr = U(StgOpenStorage)(p, NULL, STGM_TRANSACTED | RW | EXCL, (SNB)snb, 0, &s);
    t_note("StgOpenStorage(snbExclude, TRANSACTED|RW|EXCL) -> 0x%08lx", hr);
    if (hr == S_OK) {
        IStream *st = NULL; STATSTG ss;
        if (IStorage_OpenStream(s, L"data", NULL, STGM_READ | EXCL, 0, &st) == S_OK) {
            memset(&ss, 0xcc, sizeof ss);
            if (IStream_Stat(st, &ss, STATFLAG_NONAME) == S_OK)
                t_note("excluded stream length = %llu (documented: 0)", (unsigned long long)ss.cbSize.QuadPart);
            sco_relst(st);
        }
        sco_rel(s);
    }
    hr = U(StgOpenStorage)(p, NULL, DW | STGM_READ, (SNB)snb, 0, &s);
    t_note("StgOpenStorage(snbExclude, READ|DENY_WRITE) -> 0x%08lx", hr);
    sco_rel(s);
    hr = U(StgOpenStorage)(p, NULL, EXCL | RW, (SNB)snb, 0, &s);
    t_note("StgOpenStorage(snbExclude, direct RW) -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
}

/* ---- simple mode -------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_simple */
TEST(sco_simple_mode_create_and_reopen) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "simple.stg"); sco_rm(p);
    IStorage *s = NULL, *s2 = NULL; IStream *st = NULL;
    EQ(sco_create(p, STGM_SIMPLE | STGM_CREATE | EXCL | RW, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"Stg", EXCL | RW, 0, 0, &s2), STG_E_INVALIDFUNCTION);
    sco_rel(s2); s2 = NULL;
    EQ(IStorage_CreateStream(s, L"CONTENTS", STGM_CREATE | EXCL | RW, 0, 0, &st), STG_E_INVALIDFLAG);
    EQ(IStorage_CreateStream(s, L"CONTENTS", EXCL | RW, 0, 0, &st), S_OK);
    ULARGE_INTEGER u; u.QuadPart = 6000;
    EQ(IStream_SetSize(st, u), S_OK);
    ULONG c = 0; EQ(IStream_Write(st, "foo", 3, &c), S_OK); EQ(c, 3);
    sco_relst(st);
    EQ(IStorage_CreateStream(s, L"Small", EXCL | RW, 0, 0, &st), S_OK);
    u.QuadPart = 100; EQ(IStream_SetSize(st, u), S_OK);
    EQ(IStream_Write(st, "foo", 3, &c), S_OK);
    sco_relst(st);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_Release(s), 0);

    EQ(sco_open(p, STGM_SIMPLE | EXCL | RW, &s), S_OK);
    EQ(IStorage_OpenStorage(s, L"Stg", NULL, EXCL | RW, NULL, 0, &s2), STG_E_INVALIDFUNCTION);
    EQ(IStorage_OpenStream(s, L"CONTENTS", NULL, EXCL | RW, 0, &st), S_OK);
    STATSTG ss;
    EQ(IStream_Stat(st, &ss, STATFLAG_NONAME), S_OK);
    EQ(ss.cbSize.QuadPart, 6000);
    sco_relst(st);
    EQ(IStorage_OpenStream(s, L"Small", NULL, EXCL | RW, 0, &st), S_OK);
    EQ(IStream_Stat(st, &ss, STATFLAG_NONAME), S_OK);
    EQ(ss.cbSize.QuadPart, 4096);   /* simple mode pads small streams to a 4096 minimum */
    sco_relst(st);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_simple_mode_invalid_combinations) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "simple_bad.stg"); sco_rm(p);
    IStorage *s = NULL;
    HRESULT hr = sco_create(p, STGM_SIMPLE | STGM_TRANSACTED | STGM_CREATE | EXCL | RW, &s);
    CHECK(FAILED(hr));
    t_note("SIMPLE|TRANSACTED create -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
    hr = sco_create(p, STGM_SIMPLE | STGM_CREATE | DW | RW, &s);
    t_note("SIMPLE|DENY_WRITE|RW create -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_simple_mode_stream_names_stat) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "simple_enum.stg"); sco_rm(p);
    IStorage *s = NULL; IStream *st = NULL;
    EQ(sco_create(p, STGM_SIMPLE | STGM_CREATE | EXCL | RW, &s), S_OK);
    EQ(IStorage_CreateStream(s, L"a", EXCL | RW, 0, 0, &st), S_OK);
    sco_relst(st);
    /* other methods return STG_E_INVALIDFUNCTION in simple mode (documented) */
    IEnumSTATSTG *e = NULL;
    HRESULT hr = IStorage_EnumElements(s, 0, NULL, 0, &e);
    EQ(hr, STG_E_INVALIDFUNCTION);
    if (e) IEnumSTATSTG_Release(e);
    EQ(IStorage_DestroyElement(s, L"a"), STG_E_INVALIDFUNCTION);
    EQ(IStorage_RenameElement(s, L"a", L"b"), STG_E_INVALIDFUNCTION);
    EQ(IStorage_SetClass(s, &sco_CLS_A), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    sco_rm(p);
}
