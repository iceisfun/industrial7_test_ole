/* IStorage::CopyTo and IStorage::MoveElementTo (exclusions, merging, recursion, error codes). */
#include "ses.h"

/* dest
 *  |-StorageA
 *  |  `StreamA: "StreamA"
 *  |-StorageB
 *  |  `StreamB: "StreamB"
 *  `StreamC: "StreamC"          (same tree as the Wine copy tests use) */
static HRESULT build_tree(IStorage *d) {
    IStorage *g = NULL; HRESULT hr;
    if ((hr = IStorage_CreateStorage(d, L"StorageA", MODE_RW, 0, 0, &g)) != S_OK) return hr;
    ses_put(g, L"StreamA", "StreamA", 7); IStorage_Release(g); g = NULL;
    if ((hr = IStorage_CreateStorage(d, L"StorageB", MODE_RW, 0, 0, &g)) != S_OK) return hr;
    ses_put(g, L"StreamB", "StreamB", 7); IStorage_Release(g);
    return ses_put(d, L"StreamC", "StreamC", 7);
}
static int has_stream_in(IStorage *root, const WCHAR *stgname, const WCHAR *stm, const char *content) {
    IStorage *g = NULL; char b[32] = {0}; long n; int ok;
    if (stgname) { if (IStorage_OpenStorage(root, stgname, NULL, MODE_RW, NULL, 0, &g) != S_OK) return 0; root = g; }
    n = ses_get(root, stm, b, sizeof b);
    ok = (n == (long)strlen(content) && !memcmp(b, content, n));
    if (g) IStorage_Release(g);
    return ok;
}
/* some emulators leave MoveElementTo unimplemented (E_NOTIMPL); Windows implements it */
#define NOMOVE(stg) do { if (t_is_wine() && IStorage_MoveElementTo(stg, NULL, NULL, NULL, 0) == E_NOTIMPL) { t_skip("MoveElementTo not implemented by this emulator"); return; } } while (0)
#define DST_MEM(a, d) IStorage *d = ses_mem(a, MODE_CREATE, NULL); if (!d) { t_skip("mem docfile"); return; }

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_cp_copyto_whole_tree) {
    DST_MEM(a, dst);
    HR(build_tree(stg), S_OK);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    CHECK(has_stream_in(dst, L"StorageA", L"StreamA", "StreamA"));
    CHECK(has_stream_in(dst, L"StorageB", L"StreamB", "StreamB"));
    CHECK(has_stream_in(dst, NULL, L"StreamC", "StreamC"));
    /* the source is unchanged */
    CHECK(has_stream_in(stg, L"StorageA", L"StreamA", "StreamA"));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_null_dest) {
    HRW(IStorage_CopyTo(stg, 0, NULL, NULL, NULL), STG_E_INVALIDPOINTER);
}
DF_TEST(ses_cp_copyto_empty_source) {
    DST_MEM(a, dst);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    WCHAR l[2][40]; EQ(ses_list(a, dst, l, NULL, NULL, 2), 0);
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_copies_clsid) {
    DST_MEM(a, dst);
    IStorage_SetClass(stg, &SES_CLSID_B);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    STATSTG st; HR(IStorage_Stat(dst, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_B));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_copies_substorage_clsid) {
    DST_MEM(a, dst);
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g); if (!g) return;
    IStorage_SetClass(g, &SES_CLSID_A); IStorage_Release(g);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    IStorage *o = NULL; HR(IStorage_OpenStorage(dst, L"S", NULL, MODE_RW, NULL, 0, &o), S_OK);
    if (o) { STATSTG st; IStorage_Stat(o, &st, STATFLAG_NONAME); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A)); IStorage_Release(o); }
    IStorage_Release(dst);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_cp_copyto_snb_excludes_top_level_names_only) {
    DST_MEM(a, dst);
    build_tree(stg);
    const WCHAR *snb[] = { L"StorageA", L"StreamB", L"StreamC", NULL };
    HR(IStorage_CopyTo(stg, 0, NULL, (SNB)snb, dst), S_OK);
    CHECK(!ses_exists_storage(dst, L"StorageA"));
    CHECK(!ses_exists_stream(dst, L"StreamC"));
    CHECK(has_stream_in(dst, L"StorageB", L"StreamB", "StreamB")); /* StreamB is not a top-level name, so it is not excluded */
    IStorage_Release(dst);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_cp_copyto_iid_exclude_storage) {
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_CopyTo(stg, 1, &IID_IStorage, NULL, dst), S_OK);
    CHECK(!ses_exists_storage(dst, L"StorageA")); CHECK(!ses_exists_storage(dst, L"StorageB"));
    CHECK(has_stream_in(dst, NULL, L"StreamC", "StreamC"));
    IStorage_Release(dst);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_cp_copyto_iid_exclude_stream) {
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_CopyTo(stg, 1, &IID_IStream, NULL, dst), S_OK);
    CHECK(!ses_exists_stream(dst, L"StreamC"));
    IStorage *g = NULL; HR(IStorage_OpenStorage(dst, L"StorageA", NULL, MODE_RW, NULL, 0, &g), S_OK);
    if (g) { CHECK(!ses_exists_stream(g, L"StreamA")); IStorage_Release(g); } /* nested streams excluded too */
    CHECK(ses_exists_storage(dst, L"StorageB"));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_iid_exclude_both) {
    DST_MEM(a, dst);
    build_tree(stg);
    IID both[2] = { IID_IStream, IID_IStorage };
    HR(IStorage_CopyTo(stg, 2, both, NULL, dst), S_OK);
    WCHAR l[4][40]; EQ(ses_list(a, dst, l, NULL, NULL, 4), 0);
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_iid_exclude_unrelated_iid) {
    DST_MEM(a, dst);
    build_tree(stg);
    IID other = IID_IDispatch;
    HR(IStorage_CopyTo(stg, 1, &other, NULL, dst), S_OK);
    CHECK(has_stream_in(dst, NULL, L"StreamC", "StreamC"));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_merge_replaces_stream_keeps_extras) {
    DST_MEM(a, dst);
    ses_put(dst, L"Same", "old-old", 7); ses_put(dst, L"OnlyDst", "keep", 4);
    ses_put(stg, L"Same", "new", 3); ses_put(stg, L"OnlySrc", "src", 3);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    char b[16] = {0};
    EQ(ses_get(dst, L"Same", b, 16), 3); EXPECT_EQ(memcmp(b, "new", 3), 0);
    EQ(ses_get(dst, L"OnlyDst", b, 16), 4);
    EQ(ses_get(dst, L"OnlySrc", b, 16), 3);
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_merge_storage_keeps_old_children) {
    DST_MEM(a, dst);
    IStorage *g = NULL;
    IStorage_CreateStorage(dst, L"S", MODE_RW, 0, 0, &g); if (!g) return; ses_put(g, L"oldchild", "o", 1); IStorage_Release(g);
    IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g); if (!g) return; ses_put(g, L"newchild", "n", 1); IStorage_Release(g);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    IStorage *o = NULL; HR(IStorage_OpenStorage(dst, L"S", NULL, MODE_RW, NULL, 0, &o), S_OK);
    if (o) { CHECK(ses_exists_stream(o, L"newchild")); CHECK(ses_exists_stream(o, L"oldchild")); IStorage_Release(o); }
    IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_large_streams) {
    DST_MEM(a, dst);
    unsigned char *w = malloc(100000), *r = malloc(100000); if (!w || !r) return;
    ses_fill(w, 100000, 4); ses_put(stg, L"big", w, 100000); ses_put(stg, L"mini", w, 3000);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, dst), S_OK);
    EQ(ses_get(dst, L"big", r, 100000), 100000); CHECK(!memcmp(r, w, 100000));
    EQ(ses_get(dst, L"mini", r, 100000), 3000); CHECK(!memcmp(r, w, 3000));
    free(w); free(r); IStorage_Release(dst);
}
DF_TEST(ses_cp_copyto_into_own_child_denied) {
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
    IStorage *g = NULL, *gg = NULL;
    HR(IStorage_CreateStorage(stg, L"child", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    HR(IStorage_CreateStorage(g, L"grand", MODE_RW, 0, 0, &gg), S_OK);
    ses_put(stg, L"x", "1", 1);
    HR(IStorage_CopyTo(stg, 0, NULL, NULL, g), STG_E_ACCESSDENIED);
    if (gg) { HR(IStorage_CopyTo(stg, 0, NULL, NULL, gg), STG_E_ACCESSDENIED); IStorage_Release(gg); }
    HR(IStorage_CopyTo(g, 0, NULL, NULL, stg), S_OK); /* copying a child into its parent is fine */
    IStorage_Release(g);
}
DF_TEST(ses_cp_copyto_to_self_denied) {
    ses_put(stg, L"x", "1", 1);
    OBSERVE_HR(IStorage_CopyTo(stg, 0, NULL, NULL, stg));
}
DF_TEST(ses_cp_copyto_dest_open_readonly_denied) {
    WCHAR wp[MAX_PATH]; IStorage *ro = NULL; ses_api api; ses_load(&api);
    /* a read-only destination: build a docfile, close it and reopen read-only */
    IStorage *tmp = ses_file(&api, MODE_CREATE, wp); if (!tmp) return;
    ses_put(tmp, L"x", "1", 1); IStorage_Release(tmp);
    HR(api.open_storage(wp, NULL, STGM_READ | STGM_SHARE_DENY_WRITE, NULL, 0, &ro), S_OK);
    if (ro) {
        ses_put(stg, L"y", "2", 1);
        HRW(IStorage_CopyTo(stg, 0, NULL, NULL, ro), STG_E_ACCESSDENIED);
        IStorage_Release(ro);
    }
    ses_delete(wp);
}
/* ---- MoveElementTo ---- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_cp_move_stream_copy_keeps_source) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"StreamC", STGMOVE_COPY), S_OK);
    CHECK(has_stream_in(stg, NULL, L"StreamC", "StreamC")); CHECK(has_stream_in(dst, NULL, L"StreamC", "StreamC"));
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"StreamC", STGMOVE_COPY), S_OK); /* COPY overwrites silently */
    IStorage_Release(dst);
}
DF_TEST(ses_cp_move_stream_move_removes_source) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"Moved", STGMOVE_MOVE), S_OK);
    CHECK(!ses_exists_stream(stg, L"StreamC")); CHECK(has_stream_in(dst, NULL, L"Moved", "StreamC"));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_move_target_exists) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    build_tree(stg); ses_put(dst, L"T", "tt", 2);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"T", STGMOVE_MOVE), STG_E_FILEALREADYEXISTS);
    CHECK(ses_exists_stream(stg, L"StreamC"));
    char b[8]; EQ(ses_get(dst, L"T", b, 8), 2);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"T", STGMOVE_COPY), S_OK);
    EQ(ses_get(dst, L"T", b, 8), 7);
    IStorage_Release(dst);
}
DF_TEST(ses_cp_move_storage_copy_and_move) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_MoveElementTo(stg, L"StorageA", dst, L"CopyOfA", STGMOVE_COPY), S_OK);
    CHECK(ses_exists_storage(stg, L"StorageA")); CHECK(has_stream_in(dst, L"CopyOfA", L"StreamA", "StreamA"));
    HR(IStorage_MoveElementTo(stg, L"StorageA", dst, L"CopyOfA", STGMOVE_MOVE), STG_E_FILEALREADYEXISTS);
    HR(IStorage_MoveElementTo(stg, L"StorageB", dst, L"MovedB", STGMOVE_MOVE), S_OK);
    CHECK(!ses_exists_storage(stg, L"StorageB")); CHECK(has_stream_in(dst, L"MovedB", L"StreamB", "StreamB"));
    HR(IStorage_MoveElementTo(stg, L"StorageB", dst, L"MovedB", STGMOVE_COPY), STG_E_FILENOTFOUND);
    IStorage_Release(dst);
}
DF_TEST(ses_cp_move_errors) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    build_tree(stg);
    HR(IStorage_MoveElementTo(stg, NULL, NULL, NULL, 0), STG_E_INVALIDNAME);
    HR(IStorage_MoveElementTo(stg, L"nothing", dst, L"x", STGMOVE_COPY), STG_E_FILENOTFOUND);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, NULL, 0), STG_E_INVALIDNAME);
    HR(IStorage_MoveElementTo(stg, L"StreamC", NULL, L"StreamC", STGMOVE_COPY), STG_E_INVALIDPOINTER);
    HR(IStorage_MoveElementTo(stg, L"StreamC", dst, L"StreamC", ~0u), STG_E_INVALIDFLAG);
    HR(IStorage_MoveElementTo(stg, L"StreamC", NULL, L"StreamC", ~0u), STG_E_INVALIDFLAG);
    HR(IStorage_MoveElementTo(stg, L"StreamC", stg, L"StreamC", STGMOVE_COPY), STG_E_ACCESSDENIED); /* onto itself */
    CHECK(has_stream_in(stg, NULL, L"StreamC", "StreamC"));
    IStorage_Release(dst);
}
DF_TEST(ses_cp_move_within_same_storage_new_name) {
    NOMOVE(stg);
    ses_put(stg, L"orig", "content", 7);
    OBSERVE_HR(IStorage_MoveElementTo(stg, L"orig", stg, L"dup", STGMOVE_COPY));
}
DF_TEST(ses_cp_move_large_stream) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    unsigned char *w = malloc(50000), *r = malloc(50000); if (!w || !r) return;
    ses_fill(w, 50000, 71); ses_put(stg, L"big", w, 50000);
    HR(IStorage_MoveElementTo(stg, L"big", dst, L"big", STGMOVE_MOVE), S_OK);
    EQ(ses_get(dst, L"big", r, 50000), 50000); CHECK(!memcmp(r, w, 50000));
    CHECK(!ses_exists_stream(stg, L"big"));
    free(w); free(r); IStorage_Release(dst);
}
DF_TEST(ses_cp_move_open_source_stream_observed) {
    NOMOVE(stg);
    DST_MEM(a, dst);
    IStream *s = ses_mkstream(stg, L"open"); if (!s) return;
    ses_write_all(s, "data", 4);
    HRESULT hr = IStorage_MoveElementTo(stg, L"open", dst, L"open", STGMOVE_COPY);
    t_note("MoveElementTo(COPY) of an open stream -> 0x%08lx", (unsigned long)hr);
    IStream_Release(s); IStorage_Release(dst);
}
/* ---- IStream::CopyTo between docfile and cb semantics, across storages ---- */
DF_TEST(ses_cp_streamcopy_between_storages) {
    DST_MEM(a, dst);
    IStream *s = ses_mkstream(stg, L"s"), *d = ses_mkstream(dst, L"d"); if (!s || !d) return;
    unsigned char w[12000], r[12000]; ULARGE_INTEGER rd, wr; ULONG n;
    ses_fill(w, sizeof w, 33); ses_write_all(s, w, sizeof w); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(sizeof w), &rd, &wr), S_OK); EQ(rd.QuadPart, 12000); EQ(wr.QuadPart, 12000);
    ses_seek(d, 0, STREAM_SEEK_SET, NULL); IStream_Read(d, r, sizeof r, &n); EQ(n, 12000); CHECK(!memcmp(w, r, sizeof w));
    IStream_Release(s); IStream_Release(d); IStorage_Release(dst);
}
DF_TEST(ses_cp_streamcopy_to_readonly_stream_fails) {
    ses_put(stg, L"srcs", "abcdef", 6); ses_put(stg, L"dsts", "zz", 2);
    IStream *s = NULL, *d = NULL;
    HR(IStorage_OpenStream(stg, L"srcs", NULL, MODE_RW, 0, &s), S_OK);
    HR(IStorage_OpenStream(stg, L"dsts", NULL, MODE_RD, 0, &d), S_OK);
    if (s && d) {
        ULARGE_INTEGER rd, wr;
        HRESULT hr = IStream_CopyTo(s, d, ULI(6), &rd, &wr);
        CHECKF(FAILED(hr), "copy into a read-only stream must fail, got 0x%08lx", (unsigned long)hr);
        t_note("CopyTo into read-only stream -> 0x%08lx read=%llu written=%llu", (unsigned long)hr, (unsigned long long)rd.QuadPart, (unsigned long long)wr.QuadPart);
    }
    if (s) IStream_Release(s); if (d) IStream_Release(d);
}
DF_TEST(ses_cp_streamcopy_overlapping_clone_ranges) {
    IStream *s = ses_mkstream(stg, L"s"), *c = NULL; if (!s) return;
    ses_write_all(s, "0123456789", 10); IStream_Clone(s, &c); if (!c) return;
    ULARGE_INTEGER rd, wr;
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); ses_seek(c, 10, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, c, ULI(10), &rd, &wr), S_OK); /* append own content through a clone */
    EQ(rd.QuadPart, 10); EQ(wr.QuadPart, 10); EQ(ses_stream_size(s), 20);
    char b[24] = {0}; ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 20, NULL); EXPECT_STR(b, "01234567890123456789");
    IStream_Release(c); IStream_Release(s);
}
