/* IStream semantics on streams inside compound files (docfiles), run against an in-memory docfile and a file docfile. */
#include "ses.h"

DF_TEST(ses_df_write_read_roundtrip) {
    IStream *s = ses_mkstream(stg, L"data"); if (!s) { CHECK(s != NULL); return; }
    unsigned char w[777], r[777]; ULONG n = 0xdead;
    ses_fill(w, sizeof w, 3);
    HR(IStream_Write(s, w, sizeof w, &n), S_OK); EQ(n, sizeof w);
    EQ(ses_pos(s), 777); EQ(ses_stream_size(s), 777);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    memset(r, 0, sizeof r); n = 0xdead;
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, sizeof r); CHECK(!memcmp(r, w, sizeof w));
    EQ(IStream_Release(s), 0);
}
DF_TEST(ses_df_new_stream_is_empty) {
    IStream *s = ses_mkstream(stg, L"empty"); if (!s) { CHECK(s != NULL); return; }
    char c; ULONG n = 9;
    EQ(ses_stream_size(s), 0); EQ(ses_pos(s), 0);
    HR(IStream_Read(s, &c, 1, &n), S_OK); EQ(n, 0);
    IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_df_read_eof_is_s_ok) {
    IStream *s = ses_mkstream(stg, L"eof"); if (!s) { CHECK(s != NULL); return; }
    char b[300]; ULONG n = 0xdead;
    ses_write_all(s, "Hello\n", 6);
    ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 3);
    HR(ses_seek(s, 10, STREAM_SEEK_SET, NULL), S_OK);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 0);
    HR(ses_seek(s, 10000, STREAM_SEEK_SET, NULL), S_OK);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 0);
    EQ(ses_pos(s), 10000);
    EQ(ses_stream_size(s), 6);
    IStream_Release(s);
}
DF_TEST(ses_df_read_null_pcb) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    char b[8] = {0};
    ses_write_all(s, "abcdef", 6); ses_seek(s, 2, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, b, 3, NULL), S_OK); EXPECT_EQ(memcmp(b, "cde", 3), 0); EQ(ses_pos(s), 5);
    IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_df_write_null_buffer_invalid_pointer) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    HRW(IStream_Write(s, NULL, 0, NULL), STG_E_INVALIDPOINTER);
    IStream_Release(s);
}
DF_TEST(ses_df_write_zero_and_null_pcb) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ULONG n = 9;
    HR(IStream_Write(s, "Hello\n", 0, NULL), S_OK);
    HR(IStream_Write(s, "Hello\n", 0, &n), S_OK); EQ(n, 0);
    HR(IStream_Write(s, "Hello\n", 6, NULL), S_OK);
    EQ(ses_stream_size(s), 6);
    IStream_Release(s);
}
DF_TEST(ses_df_seek_origins_and_return) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ULONGLONG np; unsigned char b[100]; ses_fill(b, 100, 1); ses_write_all(s, b, 100);
    HR(ses_seek(s, 10, STREAM_SEEK_SET, &np), S_OK); EQ(np, 10);
    HR(ses_seek(s, 5, STREAM_SEEK_CUR, &np), S_OK); EQ(np, 15);
    HR(ses_seek(s, -15, STREAM_SEEK_CUR, &np), S_OK); EQ(np, 0);
    HR(ses_seek(s, 0, STREAM_SEEK_END, &np), S_OK); EQ(np, 100);
    HR(ses_seek(s, -100, STREAM_SEEK_END, &np), S_OK); EQ(np, 0);
    HR(ses_seek(s, 20, STREAM_SEEK_END, &np), S_OK); EQ(np, 120);
    HR(IStream_Seek(s, LI(4), STREAM_SEEK_SET, NULL), S_OK); EQ(ses_pos(s), 4);
    EQ(ses_stream_size(s), 100); /* seeking never changes the size */
    IStream_Release(s);
}
DF_TEST(ses_df_seek_negative_invalid_function) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ses_write_all(s, "abcdef", 6); ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HRW(IStream_Seek(s, LI(-1), STREAM_SEEK_SET, NULL), STG_E_INVALIDFUNCTION);
    CHECKW(ses_pos(s) == 3);
    ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HRW(IStream_Seek(s, LI(-4), STREAM_SEEK_CUR, NULL), STG_E_INVALIDFUNCTION);
    CHECKW(ses_pos(s) == 3);
    ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HRW(IStream_Seek(s, LI(-7), STREAM_SEEK_END, NULL), STG_E_INVALIDFUNCTION);
    CHECKW(ses_pos(s) == 3);
    ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HR(IStream_Seek(s, LI(-3), STREAM_SEEK_CUR, NULL), S_OK); EQ(ses_pos(s), 0);
    IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_df_seek_invalid_origin) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ULARGE_INTEGER p;
    HR(IStream_Seek(s, LI(0), 3, &p), STG_E_INVALIDFUNCTION);
    HR(IStream_Seek(s, LI(0), 0xffffffff, &p), STG_E_INVALIDFUNCTION);
    HR(IStream_Seek(s, LI(0), 100, NULL), STG_E_INVALIDFUNCTION);
    IStream_Release(s);
}
DF_TEST(ses_df_seek_past_end_write_zero_fills) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char b[600]; ULONG n;
    ses_write_all(s, "AB", 2);
    ses_seek(s, 500, STREAM_SEEK_SET, NULL);
    EQ(ses_stream_size(s), 2);
    ses_write_all(s, "CD", 2);
    EQ(ses_stream_size(s), 502); EQ(ses_pos(s), 502);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); memset(b, 0xee, sizeof b);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 502);
    CHECK(b[0] == 'A' && b[1] == 'B' && b[500] == 'C' && b[501] == 'D');
    CHECKW(ses_allzero(b + 2, 498));
    IStream_Release(s);
}
DF_TEST(ses_df_seek_past_end_large_gap_crosses_ministream) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char b[64]; ULONG n;
    ses_write_all(s, "X", 1);
    ses_seek(s, 10000, STREAM_SEEK_SET, NULL);
    ses_write_all(s, "Y", 1);
    EQ(ses_stream_size(s), 10001);
    ses_seek(s, 9990, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 11); CHECKW(ses_allzero(b, 10) && b[10] == 'Y');
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 4, &n); CHECKW(b[0] == 'X' && ses_allzero(b + 1, 3));
    IStream_Release(s);
}
DF_TEST(ses_df_seek_end_positive_then_write) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ULONGLONG np; char b[16] = {0}; ULONG n;
    ses_write_all(s, "abc", 3);
    HR(ses_seek(s, 5, STREAM_SEEK_END, &np), S_OK); EQ(np, 8);
    ses_write_all(s, "Z", 1); EQ(ses_stream_size(s), 9);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 9, &n); EQ(n, 9);
    CHECKW(!memcmp(b, "abc\0\0\0\0\0Z", 9));
    IStream_Release(s);
}
DF_TEST(ses_df_setsize_grow_zero_filled) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char *b = malloc(6000); ULONG n; if (!b) return;
    ses_write_all(s, "hello", 5);
    HR(IStream_SetSize(s, ULI(5000)), S_OK);
    EQ(ses_stream_size(s), 5000); EQ(ses_pos(s), 5); /* SetSize leaves the seek pointer alone */
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); memset(b, 0xee, 6000);
    HR(IStream_Read(s, b, 6000, &n), S_OK); EQ(n, 5000);
    CHECK(!memcmp(b, "hello", 5)); CHECKW(ses_allzero(b + 5, 4995));
    free(b); IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_df_setsize_big_then_read_at_end) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    char b[0x100]; ULONG n; ULONGLONG np;
    ses_write_all(s, "Hello\n", 6);
    HR(IStream_SetSize(s, ULI(5000)), S_OK);
    HR(ses_seek(s, 4997, STREAM_SEEK_SET, &np), S_OK); EQ(np, 4997);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 3);
    HR(ses_seek(s, 5001, STREAM_SEEK_SET, &np), S_OK); EQ(np, 5001);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EQ(n, 0);
    IStream_Release(s);
}
DF_TEST(ses_df_setsize_shrink_preserves_prefix) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char w[10000], r[10000]; ULONG n;
    ses_fill(w, sizeof w, 8); ses_write_all(s, w, sizeof w);
    HR(IStream_SetSize(s, ULI(100)), S_OK); EQ(ses_stream_size(s), 100);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, 100); CHECK(!memcmp(r, w, 100));
    /* growing back exposes zeros, not the discarded bytes */
    HR(IStream_SetSize(s, ULI(10000)), S_OK);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); memset(r, 0xee, sizeof r);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, 10000);
    CHECK(!memcmp(r, w, 100)); CHECKW(ses_allzero(r + 100, 9900));
    IStream_Release(s);
}
DF_TEST(ses_df_setsize_zero_resets) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char w[6000]; ULONG n = 9; char c;
    memset(w, 7, sizeof w); ses_write_all(s, w, sizeof w);
    HR(IStream_SetSize(s, ULI(0)), S_OK); EQ(ses_stream_size(s), 0);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, &c, 1, &n), S_OK); EQ(n, 0);
    ses_write_all(s, "new", 3); EQ(ses_stream_size(s), 3);
    IStream_Release(s);
}
DF_TEST(ses_df_setsize_keeps_pointer_and_write_after) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ses_write_all(s, "0123456789", 10);
    HR(IStream_SetSize(s, ULI(4)), S_OK);
    EQ(ses_pos(s), 10);
    ses_write_all(s, "Z", 1); /* writing beyond the (shrunk) end re-extends with a zero gap */
    EQ(ses_stream_size(s), 11);
    unsigned char b[11]; ULONG n; ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 11, &n);
    CHECK(!memcmp(b, "0123", 4) && b[10] == 'Z'); CHECKW(ses_allzero(b + 4, 6));
    IStream_Release(s);
}
DF_TEST(ses_df_ministream_boundary_sizes) {
    static const unsigned sizes[] = { 1, 63, 64, 65, 511, 512, 513, 4094, 4095, 4096, 4097, 4608, 8191, 8192, 70000 };
    unsigned char *w = malloc(70000), *r = malloc(70000); if (!w || !r) return;
    int bad = 0, i;
    for (i = 0; i < (int)(sizeof sizes / sizeof *sizes); i++) {
        WCHAR nm[16]; wsprintfW(nm, L"s%u", sizes[i]);
        ses_fill(w, sizes[i], sizes[i]);
        if (ses_put(stg, nm, w, sizes[i]) != S_OK) bad++;
    }
    EQ(bad, 0);
    for (i = 0; i < (int)(sizeof sizes / sizeof *sizes); i++) {
        WCHAR nm[16]; wsprintfW(nm, L"s%u", sizes[i]);
        ses_fill(w, sizes[i], sizes[i]);
        long got = ses_get(stg, nm, r, 70000);
        CHECKF(got == (long)sizes[i] && !memcmp(r, w, sizes[i]), "size %u: got %ld", sizes[i], got);
    }
    free(w); free(r);
}
DF_TEST(ses_df_write_crossing_4096_boundary) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char w[9000], r[9000]; ULONG n;
    ses_fill(w, sizeof w, 5);
    ses_write_all(s, w, 4000);       /* still a mini stream */
    ses_write_all(s, w + 4000, 200); /* crosses the 4096 cut-off while writing */
    ses_write_all(s, w + 4200, 4800);
    EQ(ses_stream_size(s), 9000);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, 9000); CHECK(!memcmp(r, w, 9000));
    IStream_Release(s);
}
DF_TEST(ses_df_overwrite_in_place_across_transition) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char w[6000], r[6000]; ULONG n;
    ses_fill(w, sizeof w, 6); ses_write_all(s, w, sizeof w);
    ses_seek(s, 4000, STREAM_SEEK_SET, NULL); memset(w + 4000, 0x11, 300); ses_write_all(s, w + 4000, 300);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, r, sizeof r, &n);
    EQ(n, 6000); CHECK(!memcmp(r, w, 6000));
    IStream_Release(s);
}
DF_TEST(ses_df_shrink_large_to_mini_roundtrip) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    unsigned char w[20000], r[20000]; ULONG n;
    ses_fill(w, sizeof w, 12); ses_write_all(s, w, sizeof w);
    HR(IStream_SetSize(s, ULI(1000)), S_OK);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); HR(IStream_Read(s, r, sizeof r, &n), S_OK);
    EQ(n, 1000); CHECK(!memcmp(r, w, 1000));
    HR(IStream_SetSize(s, ULI(4096)), S_OK); EQ(ses_stream_size(s), 4096);
    HR(IStream_SetSize(s, ULI(4097)), S_OK); EQ(ses_stream_size(s), 4097);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, r, sizeof r, &n); EQ(n, 4097);
    CHECK(!memcmp(r, w, 1000)); CHECKW(ses_allzero(r + 1000, 3097));
    IStream_Release(s);
}
DF_TEST(ses_df_stream_4mb) {
    IStream *s = ses_mkstream(stg, L"big"); if (!s) return;
    unsigned char *w = malloc(1 << 16), *r = malloc(1 << 16); if (!w || !r) return;
    int bad = 0, i;
    for (i = 0; i < 64; i++) { ses_fill(w, 1 << 16, i); if (ses_write_all(s, w, 1 << 16) != S_OK) bad++; }
    EQ(bad, 0); EQ(ses_stream_size(s), 4u << 20);
    for (i = 0; i < 64; i++) {
        ULONG n = 0; ses_fill(w, 1 << 16, i); ses_seek(s, (LONGLONG)i << 16, STREAM_SEEK_SET, NULL);
        IStream_Read(s, r, 1 << 16, &n); if (n != (1u << 16) || memcmp(r, w, 1 << 16)) bad++;
    }
    EQ(bad, 0);
    IStream_Release(s);
    free(w); free(r);
}
DF_TEST(ses_df_stat_named) {
    IStream *s = ses_mkstream(stg, L"MyStream"); if (!s) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    ses_write_all(s, "12345", 5);
    HR(IStream_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
    EQ(st.type, STGTY_STREAM); EQ(st.cbSize.QuadPart, 5);
    CHECK(st.pwcsName != NULL);
    if (st.pwcsName) { EXPECT_WSTR(st.pwcsName, L"MyStream"); a->taskfree(st.pwcsName); }
    CHECK(IsEqualGUID(&st.clsid, &GUID_NULL));
    EQ(st.grfStateBits, 0);
    t_note("stream Stat grfMode=0x%lx grfLocksSupported=0x%lx", st.grfMode, st.grfLocksSupported);
    IStream_Release(s);
}
DF_TEST(ses_df_stat_noname) {
    IStream *s = ses_mkstream(stg, L"MyStream"); if (!s) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    HR(IStream_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(st.pwcsName == NULL); EQ(st.type, STGTY_STREAM); EQ(st.cbSize.QuadPart, 0);
    IStream_Release(s);
}
DF_TEST(ses_df_stat_times_zero_for_streams) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    STATSTG st; ses_write_all(s, "x", 1);
    HR(IStream_Stat(s, &st, STATFLAG_NONAME), S_OK);
    /* the compound file implementation keeps no timestamps for internal streams: reported as zero */
    CHECKW((st.mtime.dwLowDateTime | st.mtime.dwHighDateTime) == 0);
    CHECKW((st.ctime.dwLowDateTime | st.ctime.dwHighDateTime) == 0);
    CHECKW((st.atime.dwLowDateTime | st.atime.dwHighDateTime) == 0);
    IStream_Release(s);
}
DF_TEST(ses_df_stat_invalid_flag) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    STATSTG st;
    OBSERVE_HR(IStream_Stat(s, &st, 0x7));
    IStream_Release(s);
}
DF_TEST(ses_df_commit_revert_on_stream) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    ses_write_all(s, "abc", 3);
    HR(IStream_Commit(s, STGC_DEFAULT), S_OK);
    HR(IStream_Commit(s, STGC_DEFAULT), S_OK);
    HR(IStream_Commit(s, STGC_ONLYIFCURRENT | STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE), S_OK);
    OBSERVE_HR(IStream_Revert(s));
    EQ(ses_stream_size(s), 3); /* streams are never transacted: revert cannot discard */
    IStream_Release(s);
}
DF_TEST(ses_df_lock_region_observed) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    OBSERVE_HR(IStream_LockRegion(s, ULI(0), ULI(10), LOCK_WRITE));
    OBSERVE_HR(IStream_UnlockRegion(s, ULI(0), ULI(10), LOCK_WRITE));
    IStream_Release(s);
}
DF_TEST(ses_df_stream_qi) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    void *p = NULL;
    HR(IStream_QueryInterface(s, &IID_IStream, &p), S_OK); CHECK(p == (void *)s); if (p) IStream_Release((IStream *)p);
    p = NULL; HR(IStream_QueryInterface(s, &IID_IUnknown, &p), S_OK); CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p);
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
    p = (void *)1; HR(IStream_QueryInterface(s, &IID_IPersist, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = (void *)1; HR(IStream_QueryInterface(s, &IID_IPersistStream, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = (void *)1; HR(IStream_QueryInterface(s, &IID_IStorage, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = NULL;
    { HRESULT hr = IStream_QueryInterface(s, &IID_ISequentialStream, &p); t_note("docfile stream QI(ISequentialStream)=0x%08lx (docs: not supported)", (unsigned long)hr); if (hr == S_OK && p) IUnknown_Release((IUnknown *)p); }
    IStream_Release(s);
}
DF_TEST(ses_df_stream_refcounts) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    EQ(IStream_AddRef(s), 2); EQ(IStream_AddRef(s), 3);
    EQ(IStream_Release(s), 2); EQ(IStream_Release(s), 1); EQ(IStream_Release(s), 0);
}
DF_TEST(ses_df_clone_shared_data_independent_pointer) {
    IStream *s = ses_mkstream(stg, L"s"), *c = NULL; if (!s) return;
    char b[16] = {0};
    ses_write_all(s, "hello world", 11); ses_seek(s, 6, STREAM_SEEK_SET, NULL);
    HR(IStream_Clone(s, &c), S_OK); CHECK(c != NULL && c != s); if (!c) return;
    EQ(ses_pos(c), 6);
    ses_seek(c, 0, STREAM_SEEK_SET, NULL); EQ(ses_pos(s), 6);
    HR(IStream_Read(c, b, 5, NULL), S_OK); EXPECT_STR(b, "hello");
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); ses_write_all(s, "J", 1);
    ses_seek(c, 0, STREAM_SEEK_SET, NULL); memset(b, 0, 16); IStream_Read(c, b, 5, NULL); EXPECT_STR(b, "Jello");
    HR(IStream_SetSize(c, ULI(3)), S_OK); EQ(ses_stream_size(s), 3);
    ses_seek(c, 0, STREAM_SEEK_END, NULL); ses_write_all(c, "!!", 2); EQ(ses_stream_size(s), 5);
    IStream_Release(c); IStream_Release(s);
}
DF_TEST(ses_df_clone_survives_original_release) {
    IStream *s = ses_mkstream(stg, L"s"), *c = NULL; if (!s) return;
    char b[8] = {0};
    ses_write_all(s, "data!", 5); HR(IStream_Clone(s, &c), S_OK); if (!c) return;
    EQ(IStream_Release(s), 0);
    ses_seek(c, 0, STREAM_SEEK_SET, NULL); IStream_Read(c, b, 5, NULL); EXPECT_STR(b, "data!");
    IStream_Release(c);
}
DF_TEST(ses_df_clone_null_out) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    HRW(IStream_Clone(s, NULL), STG_E_INVALIDPOINTER);
    IStream_Release(s);
}
DF_TEST(ses_df_clone_blocks_reopen) {
    /* a clone is a second reference to the same open stream; the stream stays "open" until all refs are gone */
    IStream *s = ses_mkstream(stg, L"s"), *c = NULL, *r2 = NULL; if (!s) return;
    HR(IStream_Clone(s, &c), S_OK); IStream_Release(s);
    HRW(IStorage_OpenStream(stg, L"s", NULL, MODE_RD, 0, &r2), STG_E_ACCESSDENIED);
    if (r2) IStream_Release(r2);
    if (c) IStream_Release(c);
    HR(IStorage_OpenStream(stg, L"s", NULL, MODE_RD, 0, &r2), S_OK);
    if (r2) IStream_Release(r2);
}
DF_TEST(ses_df_copyto_counts_between_docfile_streams) {
    IStream *s = ses_mkstream(stg, L"src"), *d = ses_mkstream(stg, L"dst"); if (!s || !d) return;
    unsigned char w[9000], r[9000]; ULARGE_INTEGER rd, wr; ULONG n;
    ses_fill(w, sizeof w, 14); ses_write_all(s, w, sizeof w); ses_seek(s, 1000, STREAM_SEEK_SET, NULL);
    rd.QuadPart = wr.QuadPart = 0x77;
    HR(IStream_CopyTo(s, d, ULI(5000), &rd, &wr), S_OK);
    EQ(rd.QuadPart, 5000); EQ(wr.QuadPart, 5000);
    EQ(ses_pos(s), 6000); EQ(ses_pos(d), 5000); EQ(ses_stream_size(d), 5000);
    ses_seek(d, 0, STREAM_SEEK_SET, NULL); IStream_Read(d, r, sizeof r, &n); EQ(n, 5000); CHECK(!memcmp(r, w + 1000, 5000));
    IStream_Release(s); IStream_Release(d);
}
DF_TEST(ses_df_copyto_to_end_and_overshoot) {
    IStream *s = ses_mkstream(stg, L"src"), *d = ses_mkstream(stg, L"dst"); if (!s || !d) return;
    ULARGE_INTEGER rd, wr;
    ses_write_all(s, "0123456789", 10); ses_seek(s, 4, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(1000), &rd, &wr), S_OK);
    EQ(rd.QuadPart, 6); EQ(wr.QuadPart, 6); EQ(ses_pos(s), 10);
    HR(IStream_CopyTo(s, d, ULI(1000), &rd, &wr), S_OK); EQ(rd.QuadPart, 0); EQ(wr.QuadPart, 0);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(0xffffffffffffffffULL), &rd, &wr), S_OK);
    EQ(rd.QuadPart, 10); EQ(wr.QuadPart, 10); EQ(ses_stream_size(d), 16);
    IStream_Release(s); IStream_Release(d);
}
DF_TEST(ses_df_copyto_zero_and_null_counters) {
    IStream *s = ses_mkstream(stg, L"src"), *d = ses_mkstream(stg, L"dst"); if (!s || !d) return;
    ULARGE_INTEGER rd, wr; rd.QuadPart = wr.QuadPart = 5;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(0), &rd, &wr), S_OK); EQ(rd.QuadPart, 0); EQ(wr.QuadPart, 0);
    EQ(ses_pos(s), 0);
    HR(IStream_CopyTo(s, d, ULI(3), NULL, NULL), S_OK); EQ(ses_stream_size(d), 3);
    IStream_Release(s); IStream_Release(d);
}
DF_TEST(ses_df_copyto_into_hglobal_stream) {
    IStream *s = ses_mkstream(stg, L"src"), *m = NULL; if (!s) return;
    if (a->stm_hglobal(NULL, TRUE, &m) != S_OK) return;
    unsigned char w[7000], r[7000]; ULONG n; ULARGE_INTEGER rd, wr;
    ses_fill(w, sizeof w, 15); ses_write_all(s, w, sizeof w); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, m, ULI(7000), &rd, &wr), S_OK); EQ(rd.QuadPart, 7000); EQ(wr.QuadPart, 7000);
    ses_seek(m, 0, STREAM_SEEK_SET, NULL); IStream_Read(m, r, sizeof r, &n); EQ(n, 7000); CHECK(!memcmp(r, w, 7000));
    /* and the other direction */
    IStream *d = ses_mkstream(stg, L"back"); if (d) {
        ses_seek(m, 100, STREAM_SEEK_SET, NULL);
        HR(IStream_CopyTo(m, d, ULI(500), &rd, &wr), S_OK); EQ(rd.QuadPart, 500); EQ(wr.QuadPart, 500);
        IStream_Release(d);
    }
    IStream_Release(m); IStream_Release(s);
}
DF_TEST(ses_df_copyto_null_destination) {
    IStream *s = ses_mkstream(stg, L"src"); if (!s) return;
    ULARGE_INTEGER rd, wr;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HRW(IStream_CopyTo(s, NULL, ULI(3), &rd, &wr), STG_E_INVALIDPOINTER);
    IStream_Release(s);
}
DF_TEST(ses_df_read_only_stream_denies_modification) {
    ses_put(stg, L"ro", "abcdef", 6);
    IStream *s = NULL;
    HR(IStorage_OpenStream(stg, L"ro", NULL, MODE_RD, 0, &s), S_OK); if (!s) return;
    char b[8] = {0}; ULONG n = 9;
    HR(IStream_Read(s, b, 6, &n), S_OK); EQ(n, 6);
    HR(IStream_Write(s, "x", 1, &n), STG_E_ACCESSDENIED);
    HR(IStream_SetSize(s, ULI(2)), STG_E_ACCESSDENIED);
    EQ(ses_stream_size(s), 6);
    IStream_Release(s);
}
DF_TEST(ses_df_write_only_stream_denies_read) {
    ses_put(stg, L"wo", "abcdef", 6);
    IStream *s = NULL;
    HR(IStorage_OpenStream(stg, L"wo", NULL, STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, &s), S_OK); if (!s) return;
    char b[8]; ULONG n = 9;
    HRW(IStream_Read(s, b, 6, &n), STG_E_ACCESSDENIED);
    HR(IStream_Write(s, "xy", 2, &n), S_OK);
    IStream_Release(s);
}
DF_TEST(ses_df_stream_outlives_released_storage_ref) {
    IStream *s = ses_mkstream(stg, L"s"); if (!s) return;
    IStorage_AddRef(stg);
    EQ(IStorage_Release(stg), 1); /* back to our own reference; stream does not hold an extra count visible here */
    HR(ses_write_all(s, "ok", 2), S_OK);
    IStream_Release(s);
}
DF_TEST(ses_df_reopen_after_release_sees_data) {
    unsigned char w[5000], r[5000];
    ses_fill(w, sizeof w, 21);
    HR(ses_put(stg, L"keep", w, sizeof w), S_OK); /* stream released: direct mode, so data is already in the docfile */
    EQ(ses_get(stg, L"keep", r, sizeof r), 5000); CHECK(!memcmp(r, w, sizeof w));
    IStream *s = NULL;
    HR(IStorage_OpenStream(stg, L"keep", NULL, MODE_RW, 0, &s), S_OK);
    if (s) { EQ(ses_stream_size(s), 5000); EQ(ses_pos(s), 0); IStream_Release(s); }
}
DF_TEST(ses_df_open_modes_rw_stream) {
    ses_put(stg, L"m", "0123456789", 10);
    IStream *s = NULL; char b[16] = {0}; ULONG n;
    HR(IStorage_OpenStream(stg, L"m", NULL, MODE_RW, 0, &s), S_OK); if (!s) return;
    HR(IStream_Read(s, b, 10, &n), S_OK); EQ(n, 10);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); HR(ses_write_all(s, "AB", 2), S_OK);
    IStream_Release(s);
    memset(b, 0, 16); ses_get(stg, L"m", b, 16); EXPECT_STR(b, "AB23456789");
}
DF_TEST(ses_df_open_stream_stat_size_matches) {
    unsigned char w[4100]; memset(w, 1, sizeof w);
    static const unsigned sizes[] = { 0, 1, 4095, 4096, 4100 };
    for (unsigned i = 0; i < 5; i++) {
        WCHAR nm[8]; wsprintfW(nm, L"n%u", i);
        ses_put(stg, nm, w, sizes[i]);
        IStream *s = NULL; HR(IStorage_OpenStream(stg, nm, NULL, MODE_RD, 0, &s), S_OK);
        if (s) { EQ(ses_stream_size(s), sizes[i]); IStream_Release(s); }
    }
}
