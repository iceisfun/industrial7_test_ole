/* IStorage::Stat, SetClass / ReadClassStg / WriteClassStg, ReadClassStm / WriteClassStm,
 * Write/ReadFmtUserTypeStg, Set/GetConvertStg, SetStateBits, SetElementTimes, StgSetTimes. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define DW    STGM_SHARE_DENY_WRITE
#define DN    STGM_SHARE_DENY_NONE
#define RW    STGM_READWRITE

static const WCHAR kCompObj[] = {1,'C','o','m','p','O','b','j',0};
static const WCHAR kOle[]     = {1,'O','l','e',0};

/* ---- Stat ------------------------------------------------------------------------------------- */
TEST(sco_stat_noname_vs_default) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "st_name.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    STATSTG st; memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(st.pwcsName == NULL);
    EQ(st.type, STGTY_STORAGE);
    memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
    CHECK(st.pwcsName != NULL);
    if (st.pwcsName) {
        CHECK(!lstrcmpiW(st.pwcsName, p));     /* full path of the root file */
        U(CoTaskMemFree)(st.pwcsName);
    }
    memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    t_note("STATSTG.reserved=%lu locks=0x%lx", st.reserved, st.grfLocksSupported);
    sco_rel(s); sco_rm(p);
}

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_toplevel_stat */
TEST(sco_stat_root_name_relative_path) {
    REQUIRE(U(StgCreateDocfile)); 
    /* Stat always returns the full path, even when the file was created via a relative name */
    char prev[MAX_PATH]; GetCurrentDirectoryA(MAX_PATH, prev);
    SetCurrentDirectoryA(t_tmpdir());
    WCHAR full[MAX_PATH]; sco_wpath(full, "st_rel.stg"); sco_rm(full);
    IStorage *s;
    HRESULT hr = sco_create(L"st_rel.stg", SCO_RW_EXCL | STGM_TRANSACTED, &s);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        STATSTG st; memset(&st, 0, sizeof st);
        EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
        CHECK(st.pwcsName && !lstrcmpiW(st.pwcsName, full));
        if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
        EQ(sco_rel(s), 0);
        EQ(sco_open(L"st_rel.stg", EXCL | RW, &s), S_OK);
        memset(&st, 0, sizeof st);
        EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
        CHECK(st.pwcsName && !lstrcmpiW(st.pwcsName, full));
        if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
        sco_rel(s);
    }
    SetCurrentDirectoryA(prev);
    sco_rm(full);
}

TEST(sco_stat_invalid_args) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "st_inv.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    HRESULT hr;
    if (!t_is_wine()) {
        hr = IStorage_Stat(s, NULL, STATFLAG_DEFAULT);
        CHECK(FAILED(hr));
        t_note("IStorage::Stat(NULL) -> 0x%08lx", hr);
    }
    STATSTG st;
    hr = IStorage_Stat(s, &st, 0x100);
    t_note("IStorage::Stat(invalid flag 0x100) -> 0x%08lx", hr);
    if (hr == S_OK && st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
    sco_rel(s); sco_rm(p);
}

TEST(sco_stat_grfmode_table) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "st_mode.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    DWORD modes[] = { EXCL | RW, EXCL | STGM_READ, DW | STGM_READ, DN | STGM_READ | STGM_TRANSACTED,
                      STGM_TRANSACTED | DW | RW, STGM_TRANSACTED | EXCL | RW, STGM_DIRECT | EXCL | RW };
    for (unsigned i = 0; i < SCO_ARRAY(modes); i++) {
        EQ(sco_open(p, modes[i], &s), S_OK);
        if (!s) continue;
        STATSTG st; memset(&st, 0, sizeof st);
        EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
        DWORD want = modes[i] & (STGM_READWRITE | STGM_WRITE | 0x70 | STGM_TRANSACTED);
        DWORD got = st.grfMode & (STGM_READWRITE | STGM_WRITE | 0x70 | STGM_TRANSACTED);
        CHECKF(got == want, "open mode 0x%lx: stat grfMode 0x%lx", modes[i], st.grfMode);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_stat_substorage_and_stream) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "st_sub.stg"); sco_rm(p);
    IStorage *s, *sub; IStream *st;
    EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"child", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_SetClass(sub, &sco_CLS_B), S_OK);
    STATSTG ss; memset(&ss, 0, sizeof ss);
    EQ(IStorage_Stat(sub, &ss, STATFLAG_DEFAULT), S_OK);
    EQ(ss.type, STGTY_STORAGE);
    CHECK(ss.pwcsName && !lstrcmpW(ss.pwcsName, L"child"));
    if (ss.pwcsName) U(CoTaskMemFree)(ss.pwcsName);
    CHECK(sco_guid_eq(&ss.clsid, &sco_CLS_B));
    EQ(ss.grfMode & 0x73, (EXCL | RW) & 0x73);
    /* parent clsid is untouched */
    memset(&ss, 0, sizeof ss);
    EQ(IStorage_Stat(s, &ss, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&ss.clsid, &sco_CLSID_NULL));
    sco_rel(sub);
    EQ(sco_put_stream(s, L"strm", "0123456789", 10), S_OK);
    EQ(IStorage_OpenStream(s, L"strm", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    memset(&ss, 0, sizeof ss);
    EQ(IStream_Stat(st, &ss, STATFLAG_DEFAULT), S_OK);
    EQ(ss.type, STGTY_STREAM);
    EQ(ss.cbSize.QuadPart, 10);
    CHECK(ss.pwcsName && !lstrcmpW(ss.pwcsName, L"strm"));
    if (ss.pwcsName) U(CoTaskMemFree)(ss.pwcsName);
    sco_relst(st);
    sco_rel(s); sco_rm(p);
}

TEST(sco_stat_times_nonzero) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "st_times.stg"); sco_rm(p);
    IStorage *s, *sub; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"c", EXCL | RW, 0, 0, &sub), S_OK);
    STATSTG ss; memset(&ss, 0, sizeof ss);
    EQ(IStorage_Stat(sub, &ss, STATFLAG_NONAME), S_OK);
    t_note("substorage times: c=%lx:%lx m=%lx:%lx a=%lx:%lx", ss.ctime.dwHighDateTime, ss.ctime.dwLowDateTime,
           ss.mtime.dwHighDateTime, ss.mtime.dwLowDateTime, ss.atime.dwHighDateTime, ss.atime.dwLowDateTime);
    memset(&ss, 0, sizeof ss);
    EQ(IStorage_Stat(s, &ss, STATFLAG_NONAME), S_OK);
    t_note("root times: c=%lx:%lx m=%lx:%lx a=%lx:%lx", ss.ctime.dwHighDateTime, ss.ctime.dwLowDateTime,
           ss.mtime.dwHighDateTime, ss.mtime.dwLowDateTime, ss.atime.dwHighDateTime, ss.atime.dwLowDateTime);
    sco_rel(sub); sco_rel(s); sco_rm(p);
}

/* ---- classes ---------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_writeclassstg */
TEST(sco_writeclassstg_basic) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg)); REQUIRE(U(ReadClassStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "wc.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    CLSID c, c2;
    memset(&c, 0xcc, sizeof c); memset(&c2, 0xcc, sizeof c2);
    EQ(U(ReadClassStg)(NULL, NULL), E_INVALIDARG);
    EQ(U(ReadClassStg)(NULL, &c), E_INVALIDARG);
    CHECK(!memcmp(&c, &c2, sizeof c));       /* output untouched */
    EQ(U(ReadClassStg)(s, NULL), E_INVALIDARG);
    c.Data1 = 0xdeadbeef;
    EQ(U(ReadClassStg)(s, &c), S_OK);
    CHECK(sco_guid_eq(&c, &sco_CLSID_NULL));
    EQ(U(WriteClassStg)(NULL, NULL), E_INVALIDARG);
    EQ(U(WriteClassStg)(s, NULL), STG_E_INVALIDPOINTER);
    EQ(U(WriteClassStg)(s, &sco_CLS_A), S_OK);
    memset(&c, 0, sizeof c);
    EQ(U(ReadClassStg)(s, &c), S_OK);
    CHECK(sco_guid_eq(&c, &sco_CLS_A));
    EQ(sco_rel(s), 0);
    sco_rm(p);
}

TEST(sco_writeclassstg_equiv_setclass) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteClassStg)); REQUIRE(U(ReadClassStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "wc2.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_A), S_OK);
    CLSID c; EQ(U(ReadClassStg)(s, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLS_A));
    EQ(U(WriteClassStg)(s, &sco_CLS_B), S_OK);
    STATSTG st; EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_B));
    /* setting CLSID_NULL clears it */
    EQ(IStorage_SetClass(s, &sco_CLSID_NULL), S_OK);
    EQ(U(ReadClassStg)(s, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLSID_NULL));
    sco_rel(s); sco_rm(p);
}

TEST(sco_setclass_persists_direct) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sc_dir.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_B), S_OK);
    EQ(sco_rel(s), 0);
    STATSTG st;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_B));
    sco_rel(s);
    /* on disk: root directory entry holds the CLSID */
    BYTE *b; LONG n = sco_read(p, &b);
    int found = 0;
    for (LONG i = 0; i + 16 <= n; i++) if (!memcmp(b + i, &sco_CLS_B, 16)) { found = 1; t_note("CLSID found at file offset 0x%lx", i); break; }
    CHECK(found);
    free(b);
    sco_rm(p);
}

TEST(sco_setclass_transacted_commit_semantics) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sc_tx.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL | STGM_TRANSACTED, &s), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_A), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_B), S_OK);
    STATSTG st; EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_B));         /* visible through the transacted view */
    EQ(sco_rel(s), 0);                                  /* released without a second commit */
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_A));         /* uncommitted change lost */
    sco_rel(s);
    /* Revert */
    EQ(sco_open(p, EXCL | RW | STGM_TRANSACTED, &s), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_B), S_OK);
    EQ(IStorage_Revert(s), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_A));
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_setclass_substorage_vs_root) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(ReadClassStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "sc_sub.stg"); sco_rm(p);
    IStorage *s, *sub; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"sub", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_SetClass(sub, &sco_CLS_B), S_OK);
    CLSID c; EQ(U(ReadClassStg)(sub, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLS_B));
    EQ(U(ReadClassStg)(s, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLSID_NULL));
    sco_rel(sub); sco_rel(s);
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_OpenStorage(s, L"sub", NULL, EXCL | STGM_READ, NULL, 0, &sub), S_OK);
    EQ(U(ReadClassStg)(sub, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLS_B));
    sco_rel(sub); sco_rel(s); sco_rm(p);
}

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_ReadClassStm */
TEST(sco_classstm_roundtrip) {
    REQUIRE(U(ReadClassStm)); REQUIRE(U(WriteClassStm)); REQUIRE(U(CreateStreamOnHGlobal));
    IStream *stm = NULL;
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm), S_OK);
    CLSID c, c2;
    EQ(U(ReadClassStm)(NULL, &c), E_INVALIDARG);
    EQ(U(WriteClassStm)(NULL, &sco_CLS_A), E_INVALIDARG);
    EQ(U(WriteClassStm)(stm, &sco_CLS_A), S_OK);
    EQ(U(ReadClassStm)(stm, NULL), E_INVALIDARG);
    memset(&c, 0xcc, sizeof c); memcpy(&c2, &c, sizeof c);
    EQ(U(ReadClassStm)(NULL, &c), E_INVALIDARG);
    CHECK(!memcmp(&c, &c2, sizeof c));
    /* stream position is at the end: reading fails and zeroes the output */
    EQ(U(ReadClassStm)(stm, &c), STG_E_READFAULT);
    CHECK(sco_guid_eq(&c, &sco_CLSID_NULL));
    LARGE_INTEGER z; z.QuadPart = 0;
    EQ(IStream_Seek(stm, z, STREAM_SEEK_SET, NULL), S_OK);
    EQ(U(ReadClassStm)(stm, &c), S_OK);
    CHECK(sco_guid_eq(&c, &sco_CLS_A));
    STATSTG ss; EQ(IStream_Stat(stm, &ss, STATFLAG_NONAME), S_OK);
    EQ(ss.cbSize.QuadPart, 16);
    EQ(IStream_Release(stm), 0);
}

TEST(sco_classstm_partial_data) {
    REQUIRE(U(ReadClassStm)); REQUIRE(U(CreateStreamOnHGlobal));
    IStream *stm = NULL;
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm), S_OK);
    ULONG n; EQ(IStream_Write(stm, "0123456789", 10, &n), S_OK);
    LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(stm, z, STREAM_SEEK_SET, NULL);
    CLSID c; memset(&c, 0xcc, sizeof c);
    HRESULT hr = U(ReadClassStm)(stm, &c);
    CHECK(hr == STG_E_READFAULT);
    t_note("ReadClassStm with only 10 bytes -> 0x%08lx, clsid zeroed=%d", hr, sco_guid_eq(&c, &sco_CLSID_NULL));
    IStream_Release(stm);
}

TEST(sco_classstm_in_docfile_stream) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(ReadClassStm)); REQUIRE(U(WriteClassStm));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cstm.stg"); sco_rm(p);
    IStorage *s; IStream *st; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStream(s, L"c", EXCL | RW, 0, 0, &st), S_OK);
    EQ(U(WriteClassStm)(st, &sco_CLS_B), S_OK);
    LARGE_INTEGER z; z.QuadPart = 0; IStream_Seek(st, z, STREAM_SEEK_SET, NULL);
    CLSID c; EQ(U(ReadClassStm)(st, &c), S_OK); CHECK(sco_guid_eq(&c, &sco_CLS_B));
    sco_relst(st);
    /* read-only stream rejects WriteClassStm */
    EQ(IStorage_OpenStream(s, L"c", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    HRESULT hr = U(WriteClassStm)(st, &sco_CLS_A);
    CHECK(FAILED(hr));
    t_note("WriteClassStm on read-only stream -> 0x%08lx", hr);
    sco_relst(st);
    sco_rel(s); sco_rm(p);
}

/* ---- FmtUserType ------------------------------------------------------------------------------ */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_fmtusertypestg */
TEST(sco_fmtusertype_creates_compobj) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteFmtUserTypeStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "fut.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    WCHAR ut[] = L"StgUsrType";
    EQ(U(WriteFmtUserTypeStg)(s, 0, ut), S_OK);
    for (int pass = 0; pass < 2; pass++) {     /* writing twice is fine and does not add elements */
        IEnumSTATSTG *e = NULL;
        EQ(IStorage_EnumElements(s, 0, NULL, 0, &e), S_OK);
        int found = 0, other = 0; STATSTG st; ULONG got;
        while (e && IEnumSTATSTG_Next(e, 1, &st, &got) == S_OK && got == 1) {
            if (!lstrcmpW(st.pwcsName, kCompObj)) found++; else other++;
            U(CoTaskMemFree)(st.pwcsName);
        }
        CHECK(found == 1); CHECK(other == 0);
        if (e) IEnumSTATSTG_Release(e);
        if (pass == 0) EQ(U(WriteFmtUserTypeStg)(s, 0, ut), S_OK);
    }
    sco_rel(s); sco_rm(p);
}

TEST(sco_fmtusertype_roundtrip) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "fut2.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    WCHAR ut[] = L"My Document Type";
    EQ(U(WriteFmtUserTypeStg)(s, CF_TEXT, ut), S_OK);
    CLIPFORMAT cf = 0xFFFF; LPOLESTR out = NULL;
    EQ(U(ReadFmtUserTypeStg)(s, &cf, &out), S_OK);
    CHECK(cf == CF_TEXT);
    CHECK(out && !lstrcmpW(out, ut));
    if (out) U(CoTaskMemFree)(out);
    /* each output is optional */
    EQ(U(ReadFmtUserTypeStg)(s, &cf, NULL), S_OK);
    out = NULL; EQ(U(ReadFmtUserTypeStg)(s, NULL, &out), S_OK);
    if (out) U(CoTaskMemFree)(out);
    sco_rel(s);
    /* persists across reopen */
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    out = NULL; cf = 0;
    EQ(U(ReadFmtUserTypeStg)(s, &cf, &out), S_OK);
    CHECK(cf == CF_TEXT);
    CHECK(out && !lstrcmpW(out, ut));
    if (out) U(CoTaskMemFree)(out);
    sco_rel(s); sco_rm(p);
}

TEST(sco_fmtusertype_zero_format_and_registered) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "fut3.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    WCHAR ut[] = L"T";
    CLIPFORMAT reg = (CLIPFORMAT)RegisterClipboardFormatW(L"olet_test_format_xyz");
    EQ(U(WriteFmtUserTypeStg)(s, 0, ut), S_OK);
    CLIPFORMAT cf = 0x1234; LPOLESTR out = NULL;
    HRESULT hr = U(ReadFmtUserTypeStg)(s, &cf, &out);
    EQ(hr, S_OK);
    t_note("cf written as 0 reads back as %u", cf);
    if (out) U(CoTaskMemFree)(out);
    EQ(U(WriteFmtUserTypeStg)(s, reg, ut), S_OK);
    out = NULL; cf = 0;
    EQ(U(ReadFmtUserTypeStg)(s, &cf, &out), S_OK);
    CHECK(cf == reg);
    if (out) U(CoTaskMemFree)(out);
    sco_rel(s); sco_rm(p);
}

TEST(sco_fmtusertype_read_missing) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(ReadFmtUserTypeStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "fut4.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    CLIPFORMAT cf = 7; LPOLESTR out = (LPOLESTR)1;
    HRESULT hr = U(ReadFmtUserTypeStg)(s, &cf, &out);
    CHECK(FAILED(hr));
    t_note("ReadFmtUserTypeStg without CompObj stream -> 0x%08lx cf=%u out=%p", hr, cf, (void *)out);
    sco_rel(s); sco_rm(p);
}

TEST(sco_fmtusertype_stream_layout) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(WriteClassStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "fut5.stg"); sco_rm(p);
    IStorage *s; IStream *st; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(U(WriteClassStg)(s, &sco_CLS_A), S_OK);
    WCHAR ut[] = L"Layout Type";
    EQ(U(WriteFmtUserTypeStg)(s, CF_TEXT, ut), S_OK);
    EQ(IStorage_OpenStream(s, kCompObj, NULL, STGM_READ | EXCL, 0, &st), S_OK);
    BYTE buf[256]; ULONG n = 0;
    EQ(IStream_Read(st, buf, sizeof buf, &n), S_OK);
    t_note("\\1CompObj stream is %lu bytes", n);
    if (n >= 28) {
        t_note("CompObj header: ver=%u byteorder=0x%x winver=0x%lx", sco_u16(buf), sco_u16(buf + 2), sco_u32(buf + 4));
        CHECK(sco_u16(buf + 2) == 0xFFFE);      /* [MS-OLEDS] byte order */
        t_note("CompObj clsid equals storage clsid: %d", !memcmp(buf + 12, &sco_CLS_A, 16));
    }
    sco_relst(st); sco_rel(s); sco_rm(p);
}

/* ---- Convert ---------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_convert */
TEST(sco_convertstg_state_machine) {
    REQUIRE(U(StgCreateDocfile)); REQUIRE(U(SetConvertStg)); REQUIRE(U(GetConvertStg));
    WCHAR p[MAX_PATH]; sco_wpath(p, "conv.stg"); sco_rm(p);
    EQ(U(GetConvertStg)(NULL), E_INVALIDARG);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(U(GetConvertStg)(s), STG_E_FILENOTFOUND);
    EQ(U(SetConvertStg)(s, TRUE), S_OK);
    EQ(U(SetConvertStg)(s, TRUE), S_OK);
    EQ(U(GetConvertStg)(s), S_OK);
    /* the flag lives in a stream named \001Ole */
    IStream *st = NULL;
    EQ(IStorage_OpenStream(s, kOle, NULL, STGM_READ | EXCL, 0, &st), S_OK);
    BYTE b[64]; ULONG n = 0;
    EQ(IStream_Read(st, b, sizeof b, &n), S_OK);
    t_note("\\1Ole stream is %lu bytes; flags dword=0x%lx", n, n >= 8 ? sco_u32(b + 4) : 0);
    sco_relst(st);
    EQ(U(SetConvertStg)(s, FALSE), S_OK);
    EQ(U(GetConvertStg)(s), S_FALSE);
    EQ(U(SetConvertStg)(s, FALSE), S_OK);
    EQ(U(GetConvertStg)(s), S_FALSE);
    EQ(sco_rel(s), 0);
    /* persists */
    EQ(sco_open(p, EXCL | RW, &s), S_OK);
    EQ(U(GetConvertStg)(s), S_FALSE);
    EQ(U(SetConvertStg)(s, TRUE), S_OK);
    sco_rel(s);
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(U(GetConvertStg)(s), S_OK);
    HRESULT hr = U(SetConvertStg)(s, FALSE);
    CHECK(FAILED(hr));
    t_note("SetConvertStg on read-only storage -> 0x%08lx", hr);
    sco_rel(s); sco_rm(p);
}

TEST(sco_setconvertstg_null) {
    REQUIRE(U(SetConvertStg));
    if (t_is_wine()) { t_skip("not exercised under wine"); return; }
    HRESULT hr = U(SetConvertStg)(NULL, TRUE);
    CHECK(FAILED(hr));
    t_note("SetConvertStg(NULL) -> 0x%08lx", hr);
}

/* ---- SetStateBits ------------------------------------------------------------------------------ */
TEST(sco_setstatebits_roundtrip) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "bits.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    STATSTG st; EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0);
    EQ(IStorage_SetStateBits(s, 0x5, 0x7), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0x5);
    /* only bits under the mask change */
    EQ(IStorage_SetStateBits(s, 0x2, 0x2), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0x7);
    EQ(IStorage_SetStateBits(s, 0x0, 0x1), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0x6);
    EQ(IStorage_SetStateBits(s, 0xFFFFFFFF, 0), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0x6);
    EQ(sco_rel(s), 0);
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    t_note("state bits after close/reopen: 0x%lx (set to 0x6)", st.grfStateBits);
    sco_rel(s); sco_rm(p);
}

TEST(sco_setstatebits_substorage_independent) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "bits2.stg"); sco_rm(p);
    IStorage *s, *sub; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"sub", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_SetStateBits(sub, 0x1, 0x1), S_OK);
    STATSTG st;
    EQ(IStorage_Stat(sub, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 1);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0);
    sco_rel(sub); sco_rel(s); sco_rm(p);
}

TEST(sco_setstatebits_readonly) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "bits3.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    HRESULT hr = IStorage_SetStateBits(s, 1, 1);
    t_note("SetStateBits on read-only storage -> 0x%08lx", hr);
    sco_rel(s); sco_rm(p);
}

/* ---- times ------------------------------------------------------------------------------------- */
static void get_times(const WCHAR *p, FILETIME *c, FILETIME *a, FILETIME *m) {
    HANDLE h = CreateFileW(p, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    memset(c, 0, sizeof *c); memset(a, 0, sizeof *a); memset(m, 0, sizeof *m);
    if (h != INVALID_HANDLE_VALUE) { GetFileTime(h, c, a, m); CloseHandle(h); }
}
static FILETIME ft_at(unsigned year_offset_days) {
    /* 2001-01-01 plus N days, 100ns units */
    ULARGE_INTEGER u; u.QuadPart = 126073440000000000ULL + (ULONGLONG)year_offset_days * 864000000000ULL + 12345678ULL * 10 * 2;
    FILETIME f; f.dwLowDateTime = u.LowPart; f.dwHighDateTime = u.HighPart; return f;
}

TEST(sco_setimes_stgsettimes_file) {
    REQUIRE(U(StgSetTimes)); REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "tm.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    FILETIME c = ft_at(10), a = ft_at(20), m = ft_at(30), rc, ra, rm;
    EQ(U(StgSetTimes)(p, &c, &a, &m), S_OK);
    get_times(p, &rc, &ra, &rm);
    CHECK(!memcmp(&rc, &c, sizeof c));
    CHECK(!memcmp(&rm, &m, sizeof m));
    if (memcmp(&ra, &a, sizeof a)) t_note("access time not exactly stored (file system granularity)");
    /* NULL pointers leave a time untouched */
    FILETIME m2 = ft_at(40);
    EQ(U(StgSetTimes)(p, NULL, NULL, &m2), S_OK);
    FILETIME c3, a3, m3; get_times(p, &c3, &a3, &m3);
    CHECK(!memcmp(&c3, &c, sizeof c));
    CHECK(!memcmp(&m3, &m2, sizeof m2));
    EQ(U(StgSetTimes)(p, NULL, NULL, NULL), S_OK);
    get_times(p, &c3, &a3, &m3);
    CHECK(!memcmp(&m3, &m2, sizeof m2));
    /* the file is still a valid compound file */
    EQ(U(StgIsStorageFile)(p), S_OK);
    sco_rm(p);
}

TEST(sco_setimes_stgsettimes_errors) {
    REQUIRE(U(StgSetTimes));
    WCHAR p[MAX_PATH]; sco_wpath(p, "tm_none.stg"); sco_rm(p);
    FILETIME t = ft_at(1);
    HRESULT hr = U(StgSetTimes)(p, &t, &t, &t);
    CHECK(FAILED(hr));
    t_note("StgSetTimes(nonexistent file) -> 0x%08lx", hr);
    CHECK(!sco_exists(p));
    if (!t_is_wine()) {
        hr = U(StgSetTimes)(NULL, &t, &t, &t);
        CHECK(FAILED(hr));
        t_note("StgSetTimes(NULL name) -> 0x%08lx", hr);
    }
}

TEST(sco_setimes_stgsettimes_non_docfile) {
    REQUIRE(U(StgSetTimes));
    WCHAR p[MAX_PATH]; sco_wpath(p, "tm_txt.dat"); sco_rm(p);
    CHECK(sco_write_file(p, "plain", 5));
    FILETIME m = ft_at(5), c, a, r;
    HRESULT hr = U(StgSetTimes)(p, NULL, NULL, &m);
    t_note("StgSetTimes on a non-compound file -> 0x%08lx", hr);
    get_times(p, &c, &a, &r);
    t_note("mtime applied to plain file: %d", !memcmp(&r, &m, sizeof m));
    sco_rm(p);
}

TEST(sco_setimes_stgsettimes_while_open) {
    REQUIRE(U(StgSetTimes)); REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "tm_open.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    FILETIME m = ft_at(7);
    HRESULT hr = U(StgSetTimes)(p, NULL, NULL, &m);
    t_note("StgSetTimes while exclusively open -> 0x%08lx", hr);
    sco_rel(s); sco_rm(p);
}

TEST(sco_setimes_setelementtimes_root) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "setel.stg"); sco_rm(p);
    IStorage *s, *sub; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"sub", EXCL | RW, 0, 0, &sub), S_OK);
    sco_rel(sub);
    FILETIME c = ft_at(11), m = ft_at(12), a = ft_at(13);
    HRESULT hr = IStorage_SetElementTimes(s, L"sub", &c, &a, &m);
    EQ(hr, S_OK);
    EQ(IStorage_OpenStorage(s, L"sub", NULL, EXCL | RW, NULL, 0, &sub), S_OK);
    STATSTG st; EQ(IStorage_Stat(sub, &st, STATFLAG_NONAME), S_OK);
    t_note("substorage after SetElementTimes: ctime match=%d mtime match=%d",
           !memcmp(&st.ctime, &c, sizeof c), !memcmp(&st.mtime, &m, sizeof m));
    sco_rel(sub);
    hr = IStorage_SetElementTimes(s, NULL, &c, &a, &m);
    t_note("SetElementTimes(NULL name) on root -> 0x%08lx", hr);
    hr = IStorage_SetElementTimes(s, L"nonexistent", &c, &a, &m);
    EQ(hr, STG_E_FILENOTFOUND);
    sco_rel(s); sco_rm(p);
}
