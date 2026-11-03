/* Transacted storages: Commit/Revert, nested transactions, read-only access, persistence and file growth. */
#include "ses.h"

static IStorage *reopen(const ses_api *a, ILockBytes *ilb, DWORD mode) {
    IStorage *s = NULL; if (a->open_on_ilb(ilb, NULL, mode, NULL, 0, &s) != S_OK) return NULL; return s;
}
#define TXMODE (STGM_TRANSACTED | MODE_RW)
/* new transacted in-memory docfile with one initial (empty) commit so a header exists */
static IStorage *tx_new(const ses_api *a, ILockBytes **ilb) {
    IStorage *s = ses_mem(a, MODE_TXCREATE, ilb); if (!s) return NULL;
    if (IStorage_Commit(s, STGC_DEFAULT) != S_OK) { IStorage_Release(s); return NULL; }
    return s;
}
#define TXSETUP SES_REQUIRE(a); ILockBytes *ilb = NULL; IStorage *stg = tx_new(&a, &ilb); if (!stg) { t_skip("cannot create transacted docfile"); return; }
#define TXDONE ILockBytes_Release(ilb)

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
TEST(ses_tx_uncommitted_changes_lost_on_release) {
    TXSETUP
    ses_put(stg, L"keep", "k", 1);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    ses_put(stg, L"lost", "l", 1);
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"lostdir", MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE); CHECK(stg != NULL);
    if (stg) {
        CHECK(ses_exists_stream(stg, L"keep")); CHECK(!ses_exists_stream(stg, L"lost")); CHECK(!ses_exists_storage(stg, L"lostdir"));
        IStorage_Release(stg);
    }
    TXDONE;
}
TEST(ses_tx_commit_makes_changes_durable) {
    TXSETUP
    unsigned char w[9000], r[9000]; ses_fill(w, sizeof w, 91);
    ses_put(stg, L"big", w, sizeof w); ses_put(stg, L"small", "s", 1);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE); if (!stg) { TXDONE; return; }
    EQ(ses_get(stg, L"big", r, sizeof r), 9000); CHECK(!memcmp(r, w, sizeof w));
    EQ(ses_get(stg, L"small", r, sizeof r), 1);
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_commit_without_changes_ok) {
    TXSETUP
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_commit_flag_combinations) {
    TXSETUP
    static const DWORD ok_flags[] = { STGC_DEFAULT, STGC_OVERWRITE, STGC_ONLYIFCURRENT, STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE, STGC_CONSOLIDATE,
        STGC_OVERWRITE | STGC_ONLYIFCURRENT, STGC_OVERWRITE | STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE,
        STGC_ONLYIFCURRENT | STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE, STGC_OVERWRITE | STGC_CONSOLIDATE,
        STGC_OVERWRITE | STGC_ONLYIFCURRENT | STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE | STGC_CONSOLIDATE };
    for (unsigned i = 0; i < sizeof ok_flags / sizeof *ok_flags; i++) {
        WCHAR nm[8]; wsprintfW(nm, L"f%u", i); ses_put(stg, nm, "x", 1);
        HRESULT hr = IStorage_Commit(stg, ok_flags[i]);
        CHECKF(SUCCEEDED(hr), "flags 0x%lx hr=0x%08lx", ok_flags[i], (unsigned long)hr);
        if (hr != S_OK) t_note("Commit(0x%lx) = 0x%08lx (success code)", ok_flags[i], (unsigned long)hr);
    }
    IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE);
    if (stg) { int miss = 0; for (unsigned i = 0; i < 10; i++) { WCHAR nm[8]; wsprintfW(nm, L"f%u", i); if (!ses_exists_stream(stg, nm)) miss++; } EQ(miss, 0); IStorage_Release(stg); }
    TXDONE;
}
TEST(ses_tx_commit_invalid_flags) {
    TXSETUP
    HRW(IStorage_Commit(stg, 0x10), STG_E_INVALIDFLAG);
    HRW(IStorage_Commit(stg, 0x80), STG_E_INVALIDFLAG);
    HRW(IStorage_Commit(stg, 0xfffffff0), STG_E_INVALIDFLAG);
    HRW(IStorage_Commit(stg, STGC_DEFAULT | 0x100), STG_E_INVALIDFLAG);
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_commit_overwrite_persists) {
    TXSETUP
    ses_put(stg, L"a", "first", 5);
    HR(IStorage_Commit(stg, STGC_OVERWRITE), S_OK);
    IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE); char b[8];
    if (stg) { EQ(ses_get(stg, L"a", b, 8), 5); IStorage_Release(stg); }
    TXDONE;
}
TEST(ses_tx_revert_discards_new_elements) {
    TXSETUP
    ses_put(stg, L"kept", "k", 1); IStorage_Commit(stg, STGC_DEFAULT);
    ses_put(stg, L"new", "n", 1);
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"newdir", MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    HR(IStorage_Revert(stg), S_OK);
    CHECK(ses_exists_stream(stg, L"kept")); CHECK(!ses_exists_stream(stg, L"new")); CHECK(!ses_exists_storage(stg, L"newdir"));
    HR(ses_put(stg, L"new", "again", 5), S_OK); /* name is free again */
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_revert_restores_destroyed_element) {
    TXSETUP
    ses_put(stg, L"victim", "alive", 5); IStorage_Commit(stg, STGC_DEFAULT);
    HR(IStorage_DestroyElement(stg, L"victim"), S_OK);
    CHECK(!ses_exists_stream(stg, L"victim"));
    HR(IStorage_Revert(stg), S_OK);
    char b[8]; EQ(ses_get(stg, L"victim", b, 8), 5);
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_revert_restores_renamed_element) {
    TXSETUP
    ses_put(stg, L"before", "r", 1); IStorage_Commit(stg, STGC_DEFAULT);
    HR(IStorage_RenameElement(stg, L"before", L"after"), S_OK);
    HR(IStorage_Revert(stg), S_OK);
    CHECK(ses_exists_stream(stg, L"before")); CHECK(!ses_exists_stream(stg, L"after"));
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_revert_restores_stream_content) {
    TXSETUP
    ses_put(stg, L"s", "original", 8); IStorage_Commit(stg, STGC_DEFAULT);
    IStream *s = NULL; HR(IStorage_OpenStream(stg, L"s", NULL, MODE_RW, 0, &s), S_OK);
    if (s) { ses_write_all(s, "CHANGED!", 8); ses_write_all(s, "more", 4); IStream_Release(s); }
    char b[16] = {0}; EQ(ses_get(stg, L"s", b, 16), 12); /* visible inside the transaction */
    HR(IStorage_Revert(stg), S_OK);
    memset(b, 0, 16); EQ(ses_get(stg, L"s", b, 16), 8); EXPECT_STR(b, "original");
    IStorage_Release(stg); TXDONE;
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
TEST(ses_tx_revert_invalidates_open_children) {
    TXSETUP
    IStream *s = ses_mkstream(stg, L"s"); IStorage *g = NULL; IStorage_CreateStorage(stg, L"g", MODE_RW, 0, 0, &g);
    if (!s || !g) { TXDONE; return; }
    HR(IStorage_Revert(stg), S_OK);
    STATSTG st;
    HR(IStream_Write(s, "x", 1, NULL), STG_E_REVERTED);
    { ULONG rn = 0; HR(IStream_Read(s, &st, 1, &rn), STG_E_REVERTED); }
    HR(IStream_Stat(s, &st, STATFLAG_NONAME), STG_E_REVERTED);
    HR(ses_seek(s, 0, STREAM_SEEK_SET, NULL), STG_E_REVERTED);
    HR(IStorage_Stat(g, &st, STATFLAG_NONAME), STG_E_REVERTED);
    IStream *s2 = NULL; HR(IStorage_CreateStream(g, L"n", MODE_RW, 0, 0, &s2), STG_E_REVERTED);
    if (!t_is_wine()) HR(IStorage_Commit(g, STGC_DEFAULT), STG_E_REVERTED); /* wine faults here */
    EQ(IStream_Release(s), 0); EQ(IStorage_Release(g), 0); /* Release is always allowed */
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_revert_enum_and_ops_on_reverted_child) {
    TXSETUP
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"g", MODE_RW, 0, 0, &g); if (!g) { TXDONE; return; }
    IEnumSTATSTG *e = NULL; HR(IStorage_EnumElements(g, 0, NULL, 0, &e), S_OK);
    IStorage_Revert(stg);
    if (e) { HR(IEnumSTATSTG_Reset(e), STG_E_REVERTED); IEnumSTATSTG_Release(e); }
    IStream *s = NULL;
    HR(IStorage_OpenStream(g, L"x", NULL, MODE_RD, 0, &s), STG_E_REVERTED);
    HR(IStorage_DestroyElement(g, L"x"), STG_E_REVERTED);
    HR(IStorage_RenameElement(g, L"x", L"y"), STG_E_REVERTED);
    IStorage_Release(g); IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_revert_then_new_objects_work) {
    TXSETUP
    IStream *old = ses_mkstream(stg, L"s"); if (!old) { TXDONE; return; }
    IStorage_Revert(stg);
    IStream *fresh = ses_mkstream(stg, L"s"); CHECK(fresh != NULL);
    if (fresh) { HR(ses_write_all(fresh, "ok", 2), S_OK); IStream_Release(fresh); }
    HR(IStream_Write(old, "x", 1, NULL), STG_E_REVERTED);
    IStream_Release(old); IStorage_Release(stg); TXDONE;
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
TEST(ses_tx_revert_noop_in_direct_mode) {
    SES_REQUIRE(a);
    IStorage *stg = ses_mem(&a, MODE_CREATE, NULL); if (!stg) return;
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    HR(IStorage_Revert(stg), S_OK);
    HR(IStream_Write(s, "this works\n", 11, NULL), S_OK);
    IStream_Release(s);
    CHECK(ses_exists_stream(stg, L"s"));
    IStorage_Release(stg);
}
TEST(ses_tx_commit_direct_root_and_child) {
    SES_REQUIRE(a);
    IStorage *stg = ses_mem(&a, MODE_CREATE, NULL); if (!stg) return;
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"g", MODE_RW, 0, 0, &g);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    if (g) { HR(IStorage_Commit(g, STGC_DEFAULT), S_OK); IStorage_Release(g); }
    IStorage_Release(stg);
}
TEST(ses_tx_commit_keeps_open_streams_valid) {
    TXSETUP
    IStream *s = ses_mkstream(stg, L"s"); if (!s) { TXDONE; return; }
    ses_write_all(s, "abc", 3);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    HR(ses_write_all(s, "def", 3), S_OK); /* still usable; later writes are part of the next transaction */
    EQ(ses_stream_size(s), 6);
    IStream_Release(s);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE); char b[8] = {0};
    if (stg) { EQ(ses_get(stg, L"s", b, 8), 6); EXPECT_EQ(memcmp(b, "abcdef", 6), 0); IStorage_Release(stg); }
    TXDONE;
}
TEST(ses_tx_stream_writes_after_commit_not_durable_without_second_commit) {
    TXSETUP
    IStream *s = ses_mkstream(stg, L"s"); if (!s) { TXDONE; return; }
    ses_write_all(s, "abc", 3); IStorage_Commit(stg, STGC_DEFAULT);
    ses_write_all(s, "def", 3); IStream_Release(s);
    IStorage_Release(stg); /* no second commit */
    stg = reopen(&a, ilb, TXMODE); char b[8];
    if (stg) { EQ(ses_get(stg, L"s", b, 8), 3); IStorage_Release(stg); }
    TXDONE;
}
/* ---- nested transactions ---- */
TEST(ses_tx_nested_child_commit_publishes_to_parent_only) {
    TXSETUP
    IStorage *g = NULL;
    HR(IStorage_CreateStorage(stg, L"child", STGM_TRANSACTED | MODE_RW, 0, 0, &g), S_OK); if (!g) { TXDONE; return; }
    ses_put(g, L"inner", "in", 2);
    HR(IStorage_Commit(g, STGC_DEFAULT), S_OK);
    IStorage_Release(g);
    IStorage *g2 = NULL; HR(IStorage_OpenStorage(stg, L"child", NULL, STGM_TRANSACTED | MODE_RW, NULL, 0, &g2), S_OK);
    if (g2) { CHECK(ses_exists_stream(g2, L"inner")); IStorage_Release(g2); } /* visible at the parent level */
    IStorage_Release(stg); /* parent never committed */
    stg = reopen(&a, ilb, TXMODE);
    if (stg) { CHECK(!ses_exists_storage(stg, L"child")); IStorage_Release(stg); }
    TXDONE;
}
TEST(ses_tx_nested_child_and_parent_commit_durable) {
    TXSETUP
    IStorage *g = NULL;
    HR(IStorage_CreateStorage(stg, L"child", STGM_TRANSACTED | MODE_RW, 0, 0, &g), S_OK); if (!g) { TXDONE; return; }
    ses_put(g, L"inner", "in", 2);
    HR(IStorage_Commit(g, STGC_DEFAULT), S_OK); IStorage_Release(g);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK); IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE);
    if (stg) {
        IStorage *o = NULL; HR(IStorage_OpenStorage(stg, L"child", NULL, MODE_RW, NULL, 0, &o), S_OK);
        if (o) { char b[4]; EQ(ses_get(o, L"inner", b, 4), 2); IStorage_Release(o); }
        IStorage_Release(stg);
    }
    TXDONE;
}
TEST(ses_tx_nested_parent_commit_without_child_commit_loses_child_changes) {
    TXSETUP
    IStorage *g = NULL;
    HR(IStorage_CreateStorage(stg, L"child", STGM_TRANSACTED | MODE_RW, 0, 0, &g), S_OK); if (!g) { TXDONE; return; }
    ses_put(g, L"uncommitted", "u", 1);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK); /* "commit only publishes known changes" */
    IStorage_Release(g); IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE);
    if (stg) {
        IStorage *o = NULL;
        if (IStorage_OpenStorage(stg, L"child", NULL, MODE_RW, NULL, 0, &o) == S_OK) { CHECK(!ses_exists_stream(o, L"uncommitted")); IStorage_Release(o); }
        else t_note("child storage itself not present after parent commit");
        IStorage_Release(stg);
    }
    TXDONE;
}
TEST(ses_tx_nested_child_revert) {
    TXSETUP
    IStorage *g = NULL;
    HR(IStorage_CreateStorage(stg, L"child", STGM_TRANSACTED | MODE_RW, 0, 0, &g), S_OK); if (!g) { TXDONE; return; }
    ses_put(g, L"a", "1", 1); HR(IStorage_Commit(g, STGC_DEFAULT), S_OK);
    ses_put(g, L"b", "2", 1);
    IStream *open = ses_mkstream(g, L"c");
    HR(IStorage_Revert(g), S_OK);
    CHECK(ses_exists_stream(g, L"a")); CHECK(!ses_exists_stream(g, L"b")); CHECK(!ses_exists_stream(g, L"c"));
    if (open) { HR(IStream_Write(open, "x", 1, NULL), STG_E_REVERTED); IStream_Release(open); }
    IStorage_Release(g);
    CHECK(ses_exists_storage(stg, L"child")); /* the parent is unaffected */
    IStorage_Release(stg); TXDONE;
}
TEST(ses_tx_nested_three_levels) {
    TXSETUP
    IStorage *l1 = NULL, *l2 = NULL;
    HR(IStorage_CreateStorage(stg, L"l1", STGM_TRANSACTED | MODE_RW, 0, 0, &l1), S_OK); if (!l1) { TXDONE; return; }
    HR(IStorage_CreateStorage(l1, L"l2", STGM_TRANSACTED | MODE_RW, 0, 0, &l2), S_OK); if (!l2) { TXDONE; return; }
    ses_put(l2, L"deep", "d", 1);
    HR(IStorage_Commit(l2, STGC_DEFAULT), S_OK); IStorage_Release(l2);
    HR(IStorage_Commit(l1, STGC_DEFAULT), S_OK); IStorage_Release(l1);
    HR(IStorage_Commit(stg, STGC_DEFAULT), S_OK); IStorage_Release(stg);
    stg = reopen(&a, ilb, TXMODE);
    if (stg) {
        IStorage *o1 = NULL, *o2 = NULL;
        if (IStorage_OpenStorage(stg, L"l1", NULL, MODE_RW, NULL, 0, &o1) == S_OK && IStorage_OpenStorage(o1, L"l2", NULL, MODE_RW, NULL, 0, &o2) == S_OK) {
            CHECK(ses_exists_stream(o2, L"deep"));
        } else CHECK(0);
        if (o2) IStorage_Release(o2); if (o1) IStorage_Release(o1);
        IStorage_Release(stg);
    }
    TXDONE;
}
TEST(ses_tx_nested_transacted_in_direct_parent_observed) {
    SES_REQUIRE(a);
    IStorage *stg = ses_mem(&a, MODE_CREATE, NULL); if (!stg) return;
    IStorage *g = NULL;
    HRESULT hr = IStorage_CreateStorage(stg, L"child", STGM_TRANSACTED | MODE_RW, 0, 0, &g);
    t_note("transacted child under a direct root -> 0x%08lx", (unsigned long)hr);
    if (g) IStorage_Release(g);
    IStorage_Release(stg);
}
TEST(ses_tx_stat_grfmode_observed) {
    TXSETUP
    STATSTG st; HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    t_note("transacted root Stat grfMode=0x%lx", st.grfMode);
    IStorage_Release(stg); TXDONE;
}
/* ---- read-only access ---- */
static IStorage *ro_file(const ses_api *a, WCHAR *wp, DWORD rmode) {
    IStorage *w = ses_file(a, MODE_CREATE, wp); IStorage *r = NULL;
    if (!w) return NULL;
    ses_put(w, L"stream", "content", 7);
    IStorage *g = NULL; IStorage_CreateStorage(w, L"sub", MODE_RW, 0, 0, &g); if (g) { ses_put(g, L"inner", "in", 2); IStorage_Release(g); }
    IStorage_Release(w);
    if (a->open_storage(wp, NULL, rmode, NULL, 0, &r) != S_OK) return NULL;
    return r;
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
TEST(ses_ro_create_denied) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    IStream *s = NULL; IStorage *g = NULL;
    HR(IStorage_CreateStream(r, L"new", MODE_RW, 0, 0, &s), STG_E_ACCESSDENIED);
    HR(IStorage_CreateStorage(r, L"newg", MODE_RW, 0, 0, &g), STG_E_ACCESSDENIED);
    CHECK(!ses_exists_stream(r, L"new"));
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_modify_ops_denied) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    HRW(IStorage_DestroyElement(r, L"stream"), STG_E_ACCESSDENIED);
    HRW(IStorage_RenameElement(r, L"stream", L"renamed"), STG_E_ACCESSDENIED);
    HRW(IStorage_SetClass(r, &SES_CLSID_A), STG_E_ACCESSDENIED);
    CHECKW(ses_exists_stream(r, L"stream"));
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_read_works) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    char b[16] = {0}; EQ(ses_get(r, L"stream", b, 16), 7); EXPECT_STR(b, "content");
    IStorage *g = NULL; HR(IStorage_OpenStorage(r, L"sub", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &g), S_OK);
    if (g) { EQ(ses_get(g, L"inner", b, 16), 2); IStorage_Release(g); }
    WCHAR l[4][40]; EQ(ses_list(&a, r, l, NULL, NULL, 4), 2);
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_stream_write_denied) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    IStream *s = NULL;
    HR(IStorage_OpenStream(r, L"stream", NULL, MODE_RD, 0, &s), S_OK);
    if (s) { HR(IStream_Write(s, "x", 1, NULL), STG_E_ACCESSDENIED); HR(IStream_SetSize(s, ULI(1)), STG_E_ACCESSDENIED); IStream_Release(s); }
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+): a writable stream in a read-only storage */
    HR(IStorage_OpenStream(r, L"stream", NULL, MODE_RW, 0, &s), STG_E_INVALIDFLAG);
    if (s) IStream_Release(s);
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_commit_denied_when_transacted) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE | STGM_TRANSACTED); if (!r) { t_skip("open ro transacted"); return; }
    HR(IStorage_Commit(r, STGC_DEFAULT), STG_E_ACCESSDENIED);
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_open_readwrite_substorage_denied_observed) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    IStorage *g = NULL;
    OBSERVE_HR(IStorage_OpenStorage(r, L"sub", NULL, MODE_RW, NULL, 0, &g)); if (g) IStorage_Release(g);
    IStorage_Release(r); ses_delete(wp);
}
TEST(ses_ro_file_unchanged_after_failed_writes) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *r = ro_file(&a, wp, STGM_READ | STGM_SHARE_DENY_WRITE); if (!r) { t_skip("open ro"); return; }
    HANDLE h = INVALID_HANDLE_VALUE; (void)h;
    IStream *s = NULL; IStorage_CreateStream(r, L"nope", MODE_RW, 0, 0, &s);
    IStorage_Release(r);
    IStorage *w = NULL; HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &w), S_OK);
    if (w) { CHECK(!ses_exists_stream(w, L"nope")); CHECK(ses_exists_stream(w, L"stream")); IStorage_Release(w); }
    ses_delete(wp);
}
/* ---- persistence and file size (real files) ---- */
static DWORD fsize(const WCHAR *wp) {
    WIN32_FILE_ATTRIBUTE_DATA d; if (!GetFileAttributesExW(wp, GetFileExInfoStandard, &d)) return (DWORD)-1; return d.nFileSizeLow;
}
TEST(ses_file_size_is_sector_multiple) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    ses_put(s, L"one", "1", 1);
    IStorage_Release(s);
    DWORD sz = fsize(wp);
    CHECKF(sz != (DWORD)-1 && sz >= 1024 && sz % 512 == 0, "file size %lu", (unsigned long)sz);
    t_note("docfile with one 1-byte stream: %lu bytes", (unsigned long)sz);
    ses_delete(wp);
}
TEST(ses_file_size_grows_with_large_stream) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    IStorage_Release(s); DWORD empty = fsize(wp);
    static const unsigned sizes[] = { 100000, 1000000, 3000000 };
    for (unsigned i = 0; i < 3; i++) {
        IStorage *d = NULL; if (a.open_storage(wp, NULL, MODE_RW, NULL, 0, &d) != S_OK) { CHECK(0); break; }
        IStream *x = NULL; IStorage_DestroyElement(d, L"big");
        x = ses_mkstream(d, L"big");
        if (x) { HR(IStream_SetSize(x, ULI(sizes[i])), S_OK); IStream_Release(x); }
        IStorage_Release(d);
        DWORD sz = fsize(wp);
        CHECKF(sz >= sizes[i] && sz % 512 == 0, "stream %u -> file %lu", sizes[i], (unsigned long)sz);
        t_note("empty docfile %lu bytes; with %u-byte stream %lu bytes", (unsigned long)empty, sizes[i], (unsigned long)sz);
    }
    ses_delete(wp);
}
TEST(ses_file_size_ministream_small_overhead) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    for (int i = 0; i < 20; i++) { WCHAR nm[8]; wsprintfW(nm, L"m%d", i); ses_put(s, nm, "0123456789", 10); }
    IStorage_Release(s);
    DWORD sz = fsize(wp);
    CHECKF(sz != (DWORD)-1 && sz < 65536, "20 tiny streams made a %lu byte file", (unsigned long)sz); /* 4 KiB cut-off => mini stream, not one sector each */
    ses_delete(wp);
}
TEST(ses_file_persist_streams_across_reopen) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    static const unsigned sizes[] = { 0, 1, 511, 4095, 4096, 4097, 70000 };
    unsigned char *w = malloc(70000), *r = malloc(70000); if (!w || !r) return;
    for (unsigned i = 0; i < 7; i++) { WCHAR nm[8]; wsprintfW(nm, L"p%u", i); ses_fill(w, sizes[i], i); ses_put(s, nm, w, sizes[i]); }
    IStorage *g = NULL; IStorage_CreateStorage(s, L"dir", MODE_RW, 0, 0, &g); if (g) { IStorage_SetClass(g, &SES_CLSID_B); ses_put(g, L"in", "inner", 5); IStorage_Release(g); }
    IStorage_SetClass(s, &SES_CLSID_A);
    IStorage_Release(s);
    HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s), S_OK);
    if (s) {
        int bad = 0;
        for (unsigned i = 0; i < 7; i++) { WCHAR nm[8]; wsprintfW(nm, L"p%u", i); ses_fill(w, sizes[i], i); long n = ses_get(s, nm, r, 70000); if (n != (long)sizes[i] || memcmp(r, w, sizes[i])) bad++; }
        EQ(bad, 0);
        STATSTG st; HR(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A));
        if (IStorage_OpenStorage(s, L"dir", NULL, MODE_RW, NULL, 0, &g) == S_OK) {
            IStorage_Stat(g, &st, STATFLAG_NONAME); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_B));
            char b[8]; EQ(ses_get(g, L"in", b, 8), 5); IStorage_Release(g);
        } else CHECK(0);
        IStorage_Release(s);
    }
    free(w); free(r); ses_delete(wp);
}
TEST(ses_file_is_storage_file_after_create) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    IStorage_Release(s);
    HR(a.is_file(wp), S_OK);
    ses_delete(wp);
}
TEST(ses_file_zero_length_stream_persists) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    IStream *x = ses_mkstream(s, L"empty"); if (x) IStream_Release(x);
    IStorage_Release(s);
    HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s), S_OK);
    if (s) { IStream *o = NULL; HR(IStorage_OpenStream(s, L"empty", NULL, MODE_RD, 0, &o), S_OK); if (o) { EQ(ses_stream_size(o), 0); IStream_Release(o); } IStorage_Release(s); }
    ses_delete(wp);
}
TEST(ses_file_transacted_uncommitted_not_written) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_TXCREATE, wp); if (!s) { t_skip("file"); return; }
    HR(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    ses_put(s, L"pending", "p", 1);
    IStorage_Release(s);
    HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s), S_OK);
    if (s) { CHECK(!ses_exists_stream(s, L"pending")); IStorage_Release(s); }
    ses_delete(wp);
}
TEST(ses_file_transacted_commit_writes) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_TXCREATE, wp); if (!s) { t_skip("file"); return; }
    ses_put(s, L"c", "committed", 9);
    HR(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    IStorage_Release(s);
    HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s), S_OK);
    if (s) { char b[16]; EQ(ses_get(s, L"c", b, 16), 9); IStorage_Release(s); }
    ses_delete(wp);
}
TEST(ses_file_transacted_destroy_then_commit_consolidate_shrinks_or_equal) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_TXCREATE, wp); if (!s) { t_skip("file"); return; }
    IStream *x = ses_mkstream(s, L"big"); if (x) { IStream_SetSize(x, ULI(2000000)); IStream_Release(x); }
    HR(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    DWORD big = fsize(wp);
    IStorage_DestroyElement(s, L"big");
    HRESULT hr = IStorage_Commit(s, STGC_CONSOLIDATE);
    CHECK(SUCCEEDED(hr));
    IStorage_Release(s);
    DWORD after = fsize(wp);
    CHECKF(after <= big, "size grew: %lu -> %lu", (unsigned long)big, (unsigned long)after);
    t_note("2 MB stream committed: %lu bytes; after destroy+CONSOLIDATE commit: %lu bytes (hr=0x%08lx)", (unsigned long)big, (unsigned long)after, (unsigned long)hr);
    ses_delete(wp);
}
TEST(ses_file_deleteonrelease_removes_file) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE | STGM_DELETEONRELEASE, wp); if (!s) { t_skip("file"); return; }
    ses_put(s, L"x", "1", 1);
    CHECK(GetFileAttributesW(wp) != INVALID_FILE_ATTRIBUTES);
    IStorage_Release(s);
    CHECK(GetFileAttributesW(wp) == INVALID_FILE_ATTRIBUTES);
}
TEST(ses_file_exclusive_open_blocks_second_open) {
    SES_REQUIRE(a); WCHAR wp[MAX_PATH]; IStorage *s = ses_file(&a, MODE_CREATE, wp); if (!s) { t_skip("file"); return; }
    IStorage *s2 = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s2);
    CHECKF(hr == STG_E_SHAREVIOLATION || hr == STG_E_LOCKVIOLATION, "hr=0x%08lx", (unsigned long)hr);
    if (hr == S_OK && s2) IStorage_Release(s2); else CHECKW(s2 == NULL);
    IStorage_Release(s);
    HR(a.open_storage(wp, NULL, MODE_RW, NULL, 0, &s2), S_OK);
    if (s2) IStorage_Release(s2);
    ses_delete(wp);
}
