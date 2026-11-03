/* Sharing against external openers and other processes, unicode names, lifetimes, CopyTo, CONVERT. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define DW    STGM_SHARE_DENY_WRITE
#define DN    STGM_SHARE_DENY_NONE
#define RW    STGM_READWRITE

static void mkdoc1(const WCHAR *p) {
    IStorage *s;
    if (sco_create(p, SCO_RW_EXCL, &s) == S_OK) { sco_put_stream(s, L"x", "hello", 5); IStorage_Commit(s, STGC_DEFAULT); IStorage_Release(s); }
}

/* ---- external Win32 holders ------------------------------------------------------------------- */
typedef struct { DWORD access, share; DWORD mode; int expect_fail; const char *what; } holdcase;
static const holdcase hold_cases[] = {
    { GENERIC_READ,                 0,                                  EXCL | STGM_READ, 1, "holder share=0 vs EXCL|READ" },
    { GENERIC_READ,                 0,                                  DW | STGM_READ,   1, "holder share=0 vs DW|READ" },
    { GENERIC_READ | GENERIC_WRITE, 0,                                  EXCL | RW,        1, "holder RW share=0 vs EXCL|RW" },
    { GENERIC_READ,                 FILE_SHARE_READ,                    EXCL | STGM_READ, 1, "holder read vs EXCL|READ (exclusive denies sharing)" },
    { GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,                    DW | STGM_READ,   1, "holder writer vs DW|READ (DW means no writers)" },
    { GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, EXCL | RW,        1, "holder RW sharing all vs EXCL|RW" },
    { GENERIC_READ,                 FILE_SHARE_READ,                    DW | STGM_READ,   0, "holder read vs DW|READ" },
};

TEST(sco_share_external_holder_table) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sh_ext.stg"); sco_rm(p);
    mkdoc1(p);
    for (unsigned i = 0; i < SCO_ARRAY(hold_cases); i++) {
        HANDLE h = CreateFileW(p, hold_cases[i].access, hold_cases[i].share, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "holder open failed %lu", GetLastError()); continue; }
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, hold_cases[i].mode, &s);
        if (hold_cases[i].expect_fail) {
            CHECKF(FAILED(hr), "%s: succeeded", hold_cases[i].what);
            if (hr == STG_E_SHAREVIOLATION) CHECK(1); else t_note("%s -> 0x%08lx (expected STG_E_SHAREVIOLATION)", hold_cases[i].what, hr);
            CHECKF(hr == STG_E_SHAREVIOLATION || hr == STG_E_LOCKVIOLATION, "%s: 0x%08lx", hold_cases[i].what, hr);
        } else {
            CHECKF(hr == S_OK, "%s: 0x%08lx", hold_cases[i].what, hr);
        }
        sco_rel(s);
        CloseHandle(h);
    }
    sco_rm(p);
}

TEST(sco_share_open_holds_file_handle) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sh_hold.stg"); sco_rm(p);
    mkdoc1(p);
    IStorage *s; EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    SetLastError(0);
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h); else EXPECT_ERR(ERROR_SHARING_VIOLATION);
    sco_rel(s);
    h = CreateFileW(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    sco_rm(p);
}

TEST(sco_share_readonly_file_attribute_open_modes) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sh_ro.stg"); sco_rm(p);
    mkdoc1(p);
    SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY);
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, EXCL | RW, &s);
    EQ(hr, STG_E_ACCESSDENIED);
    CHECK(s == NULL);
    hr = sco_open(p, STGM_TRANSACTED | DW | RW, &s);
    t_note("TRANSACTED|DW|RW on read-only file -> 0x%08lx", hr);
    sco_rel(s);
    hr = sco_open(p, DW | STGM_READ, &s);
    EQ(hr, S_OK);
    sco_rel(s);
    sco_rm(p);
}

/* ---- another process ---------------------------------------------------------------------------- */
CHILD(sco_hold) {
    /* argv[1] = path of docfile, argv[2] = path of ready-marker. Open exclusively, signal, wait. */
    WCHAR p[MAX_PATH], m[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, p, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, m, MAX_PATH);
    IStorage *s = NULL;
    HRESULT hr = U(StgOpenStorage)(p, NULL, STGM_SHARE_EXCLUSIVE | STGM_READ, NULL, 0, &s);
    if (hr != S_OK) return 2;
    HANDLE h = CreateFileW(m, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    Sleep(8000);
    IStorage_Release(s);
    return 0;
}

TEST(sco_share_other_process_exclusive_holder) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH], mk[MAX_PATH]; sco_wpath(p, "sh_proc.stg"); sco_wpath(mk, "sh_proc.ready"); sco_rm(p); sco_rm(mk);
    mkdoc1(p);
    char pa[MAX_PATH], ma[MAX_PATH], args[2 * MAX_PATH + 8];
    sco_apath(pa, "sh_proc.stg"); sco_apath(ma, "sh_proc.ready");
    snprintf(args, sizeof args, "\"%s\" \"%s\"", pa, ma);
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("sco_hold", args, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (!h) { sco_rm(p); return; }
    int ready = 0;
    for (int i = 0; i < 100 && !ready; i++) { if (sco_exists(mk)) ready = 1; else Sleep(50); }
    CHECK(ready);
    if (ready) {
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, EXCL | STGM_READ, &s);
        EQ(hr, STG_E_SHAREVIOLATION);
        sco_rel(s);
        hr = sco_open(p, DW | STGM_READ, &s);
        CHECK(FAILED(hr));
        sco_rel(s);
        hr = sco_create(p, SCO_RW_EXCL, &s);
        CHECK(FAILED(hr));
        t_note("create over file held exclusively by another process -> 0x%08lx", hr);
        sco_rel(s);
    }
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
    IStorage *s = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    sco_rel(s);
    sco_rm(p); sco_rm(mk);
}

CHILD(sco_creator) {
    WCHAR p[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, p, MAX_PATH);
    IStorage *s = NULL;
    HRESULT hr = U(StgCreateDocfile)(p, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &s);
    if (hr != S_OK) return 2;
    IStream *st = NULL;
    if (IStorage_CreateStream(s, L"fromchild", STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &st) != S_OK) return 3;
    ULONG w; IStream_Write(st, "child data", 10, &w);
    IStream_Release(st);
    IStorage_Commit(s, STGC_DEFAULT);
    IStorage_Release(s);
    return 42;
}

TEST(sco_process_file_created_by_child_readable) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "pc_child.stg"); sco_rm(p);
    char pa[MAX_PATH], args[MAX_PATH + 4]; sco_apath(pa, "pc_child.stg");
    snprintf(args, sizeof args, "\"%s\"", pa);
    DWORD rc = t_run_child("sco_creator", args, 20000);
    EQ(rc, 42);
    EQ(U(StgIsStorageFile)(p), S_OK);
    IStorage *s; IStream *st;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    if (s) {
        EQ(IStorage_OpenStream(s, L"fromchild", NULL, STGM_READ | EXCL, 0, &st), S_OK);
        char b[16] = {0}; ULONG n = 0;
        if (st) { IStream_Read(st, b, 15, &n); CHECK(n == 10 && !memcmp(b, "child data", 10)); sco_relst(st); }
        sco_rel(s);
    }
    sco_rm(p);
}

/* ---- names ----------------------------------------------------------------------------------- */
TEST(sco_names_unicode_file_name) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR leaf[] = { 'u','n','i',0xE9,0x4E2D,0x6587,'.','s','t','g',0 };
    WCHAR p[MAX_PATH]; char a[MAX_PATH]; sco_apath(a, "x"); 
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, p, MAX_PATH);
    wcscat(p, leaf);
    sco_rm(p);
    IStorage *s;
    HRESULT hr = sco_create(p, SCO_RW_EXCL, &s);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        STATSTG st; EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
        CHECK(st.pwcsName && !lstrcmpiW(st.pwcsName, p));
        if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
        sco_rel(s);
        CHECK(sco_exists(p));
        EQ(U(StgIsStorageFile)(p), S_OK);
        EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_names_case_insensitive_path) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH], q[MAX_PATH]; sco_wpath(p, "CaseName.stg"); sco_rm(p);
    mkdoc1(p);
    sco_wpath(q, "casename.STG");
    IStorage *s;
    EQ(sco_open(q, EXCL | STGM_READ, &s), S_OK);
    if (s) {
        STATSTG st; EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
        t_note("Stat name for case-different open: %ls", st.pwcsName ? st.pwcsName : L"(null)");
        if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_names_dotdot_and_forward_slash) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sl.stg"); sco_rm(p);
    mkdoc1(p);
    WCHAR q[MAX_PATH]; sco_wpath(q, "sl_sub");
    CreateDirectoryW(q, NULL);
    wcscat(q, L"\\..\\sl.stg");
    IStorage *s;
    HRESULT hr = sco_open(q, EXCL | STGM_READ, &s);
    EQ(hr, S_OK);
    sco_rel(s);
    for (WCHAR *c = p; *c; c++) if (*c == L'\\') *c = L'/';
    hr = sco_open(p, EXCL | STGM_READ, &s);
    t_note("StgOpenStorage with forward slashes -> 0x%08lx", hr);
    sco_rel(s);
    sco_wpath(p, "sl.stg"); sco_rm(p);
    sco_wpath(q, "sl_sub"); RemoveDirectoryW(q);
}

/* ---- object lifetime --------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_parent_free */
TEST(sco_lifetime_children_reverted_when_parent_released) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "lt.stg"); sco_rm(p);
    IStorage *s, *sub, *sub2; IStream *st;
    EQ(sco_create(p, SCO_RW_EXCL | STGM_TRANSACTED, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"PERMSTG", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_CreateStream(sub, L"CONTENTS", EXCL | RW, 0, 0, &st), S_OK);
    EQ(IStorage_CreateStorage(sub, L"PERMSTG", EXCL | RW, 0, 0, &sub2), S_OK);
    EQ(IStorage_Release(sub), 0);                 /* free the parent */
    EQ(IStream_Write(st, "this should fail\n", 17, NULL), STG_E_REVERTED);
    STATSTG ss;
    EQ(IStorage_Stat(sub2, &ss, STATFLAG_NONAME), STG_E_REVERTED);
    EQ(IStorage_SetStateBits(sub2, 1, 1), STG_E_REVERTED);
    EQ(IStream_Release(st), 0);
    EQ(IStorage_Release(sub2), 0);
    EQ(IStorage_Release(s), 0);
    CHECK(DeleteFileW(p));
}

TEST(sco_lifetime_root_release_with_open_children) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "lt_root.stg"); sco_rm(p);
    IStorage *s, *sub; IStream *st;
    EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"sub", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_CreateStream(sub, L"st", EXCL | RW, 0, 0, &st), S_OK);
    ULONG rc = IStorage_Release(s);
    EQ(rc, 0);
    ULONG w = 0; HRESULT hr = IStream_Write(st, "alive", 5, &w);
    t_note("stream write after the root storage was released -> 0x%08lx (w=%lu)", hr, w);
    IStorage *other = NULL;
    hr = sco_open(p, EXCL | STGM_READ, &other);
    t_note("open while only child objects remain -> 0x%08lx", hr);
    sco_rel(other);
    IStream_Release(st);
    IStorage_Release(sub);
    EQ(sco_open(p, EXCL | STGM_READ, &other), S_OK);
    sco_rel(other);
    sco_rm(p);
}

TEST(sco_lifetime_released_storage_flushes_direct_mode) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "lt2.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(sco_put_stream(s, L"nocommit", "data", 4), S_OK);
    EQ(sco_rel(s), 0);                          /* direct mode: no explicit Commit */
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    IStream *st; EQ(IStorage_OpenStream(s, L"nocommit", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    sco_relst(st); sco_rel(s); sco_rm(p);
}

TEST(sco_lifetime_transacted_release_discards) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "lt3.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL | STGM_TRANSACTED, &s), S_OK);
    EQ(sco_put_stream(s, L"lost", "data", 4), S_OK);
    EQ(sco_rel(s), 0);                          /* transacted: release without commit discards */
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    IStream *st = NULL;
    EQ(IStorage_OpenStream(s, L"lost", NULL, STGM_READ | EXCL, 0, &st), STG_E_FILENOTFOUND);
    sco_relst(st); sco_rel(s); sco_rm(p);
}

/* ---- CopyTo and CONVERT ------------------------------------------------------------------------ */
TEST(sco_copyto_preserves_class_and_streams) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR a[MAX_PATH], b[MAX_PATH]; sco_wpath(a, "cp_a.stg"); sco_wpath(b, "cp_b.stg"); sco_rm(a); sco_rm(b);
    IStorage *sa, *sb, *sub;
    EQ(sco_create(a, SCO_RW_EXCL, &sa), S_OK);
    EQ(IStorage_SetClass(sa, &sco_CLS_A), S_OK);
    EQ(sco_put_stream(sa, L"one", "1111", 4), S_OK);
    EQ(IStorage_CreateStorage(sa, L"dir", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_SetClass(sub, &sco_CLS_B), S_OK);
    EQ(sco_put_stream(sub, L"two", "22", 2), S_OK);
    sco_rel(sub);
    EQ(sco_create(b, SCO_RW_EXCL, &sb), S_OK);
    EQ(IStorage_CopyTo(sa, 0, NULL, NULL, sb), S_OK);
    STATSTG st; EQ(IStorage_Stat(sb, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_A));
    IStream *s1;
    EQ(IStorage_OpenStream(sb, L"one", NULL, STGM_READ | EXCL, 0, &s1), S_OK); sco_relst(s1);
    EQ(IStorage_OpenStorage(sb, L"dir", NULL, EXCL | RW, NULL, 0, &sub), S_OK);
    if (sub) {
        EQ(IStorage_Stat(sub, &st, STATFLAG_NONAME), S_OK);
        CHECK(sco_guid_eq(&st.clsid, &sco_CLS_B));
        EQ(IStorage_OpenStream(sub, L"two", NULL, EXCL | STGM_READ, 0, &s1), S_OK); sco_relst(s1);
        sco_rel(sub);
    }
    sco_rel(sa); sco_rel(sb); sco_rm(a); sco_rm(b);
}

TEST(sco_copyto_exclude_clsid_iid) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR a[MAX_PATH], b[MAX_PATH]; sco_wpath(a, "cp2_a.stg"); sco_wpath(b, "cp2_b.stg"); sco_rm(a); sco_rm(b);
    IStorage *sa, *sb;
    EQ(sco_create(a, SCO_RW_EXCL, &sa), S_OK);
    EQ(IStorage_SetClass(sa, &sco_CLS_A), S_OK);
    EQ(sco_put_stream(sa, L"one", "1", 1), S_OK);
    EQ(sco_create(b, SCO_RW_EXCL, &sb), S_OK);
    IID ex[1]; ex[0] = sco_IID_IStream;
    EQ(IStorage_CopyTo(sa, 1, ex, NULL, sb), S_OK);   /* exclude streams */
    IStream *s1 = NULL;
    EQ(IStorage_OpenStream(sb, L"one", NULL, STGM_READ | EXCL, 0, &s1), STG_E_FILENOTFOUND);
    sco_relst(s1);
    STATSTG st; EQ(IStorage_Stat(sb, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_A));
    sco_rel(sa); sco_rel(sb); sco_rm(a); sco_rm(b);
}

TEST(sco_convert_flag_on_plain_file) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "conv_plain.dat"); sco_rm(p);
    CHECK(sco_write_file(p, "hello", 5));
    IStorage *s = NULL;
    HRESULT hr = sco_create(p, STGM_CONVERT | EXCL | RW, &s);
    t_note("StgCreateDocfile(STGM_CONVERT) over a plain file -> 0x%08lx", hr);
    if (hr == S_OK) {
        IStream *st = NULL;
        HRESULT h2 = IStorage_OpenStream(s, L"CONTENTS", NULL, STGM_READ | EXCL, 0, &st);
        t_note("CONTENTS stream after convert -> 0x%08lx", h2);
        if (h2 == S_OK) {
            char b[8] = {0}; ULONG n = 0; IStream_Read(st, b, 8, &n);
            CHECK(n == 5 && !memcmp(b, "hello", 5));
            sco_relst(st);
        }
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_open_noscratch_nosnapshot) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "ns.stg"); sco_rm(p);
    mkdoc1(p);
    DWORD modes[] = { STGM_NOSNAPSHOT | STGM_TRANSACTED | DW | STGM_READ, STGM_NOSNAPSHOT | STGM_TRANSACTED | DN | STGM_READ,
                      STGM_NOSCRATCH | STGM_TRANSACTED | RW | DW, STGM_NOSNAPSHOT | STGM_TRANSACTED | EXCL | RW };
    for (unsigned i = 0; i < SCO_ARRAY(modes); i++) {
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, modes[i], &s);
        t_note("StgOpenStorage mode 0x%lx -> 0x%08lx", modes[i], hr);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_open_deleteonrelease) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "dor_open.stg"); sco_rm(p);
    mkdoc1(p);
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, STGM_DELETEONRELEASE | EXCL | RW, &s);
    t_note("StgOpenStorage(DELETEONRELEASE|EXCL|RW) -> 0x%08lx", hr);
    if (hr == S_OK) { sco_rel(s); t_note("file exists after release: %d", sco_exists(p)); }
    sco_rm(p);
}

TEST(sco_open_write_only_modes) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "wo_open.stg"); sco_rm(p);
    mkdoc1(p);
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, EXCL | STGM_WRITE, &s);
    t_note("StgOpenStorage(EXCL|WRITE) -> 0x%08lx", hr);
    sco_rel(s);
    hr = sco_open(p, STGM_TRANSACTED | DW | STGM_WRITE, &s);
    t_note("StgOpenStorage(TRANSACTED|DW|WRITE) -> 0x%08lx", hr);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_review_commit_roundtrip_clsid_and_stream) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "rv_rt.stg"); sco_rm(p);
    IStorage *s = NULL; IStream *st = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_A), S_OK);
    EQ(IStorage_CreateStream(s, L"data", EXCL | RW, 0, 0, &st), S_OK);
    ULONG w = 0; EQ(IStream_Write(st, "hello", 5, &w), S_OK); EQ(w, 5);
    IStream_Release(st);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    EQ(U(StgIsStorageFile)(p), S_OK);
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    STATSTG ss; EQ(IStorage_Stat(s, &ss, STATFLAG_NONAME), S_OK);
    CHECK(IsEqualGUID(&ss.clsid, &sco_CLS_A));
    EQ(IStorage_OpenStream(s, L"data", NULL, EXCL | STGM_READ, 0, &st), S_OK);
    char buf[8] = {0}; ULONG r = 0; EQ(IStream_Read(st, buf, 8, &r), S_OK);
    EQ(r, 5); CHECK(!memcmp(buf, "hello", 5));
    IStream_Release(st); sco_rel(s); sco_rm(p);
}

TEST(sco_review_open_missing_stream_and_storage) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "rv_miss.stg"); sco_rm(p);
    IStorage *s = NULL, *sub = NULL; IStream *st = NULL;
    EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_OpenStream(s, L"nope", NULL, EXCL | STGM_READ, 0, &st), STG_E_FILENOTFOUND);
    CHECK(st == NULL);
    EQ(IStorage_OpenStorage(s, L"nope", NULL, EXCL | RW, NULL, 0, &sub), STG_E_FILENOTFOUND);
    CHECK(sub == NULL);
    sco_rel(s); sco_rm(p);
}
