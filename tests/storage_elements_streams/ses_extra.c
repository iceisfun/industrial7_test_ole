/* Additional storage/stream scenarios added during review. */
#include "ses.h"

/* two streams written alternately keep independent contents (sector chains do not cross) */
DF_TEST(ses_xt_interleaved_streams_independent) {
    IStream *a1 = ses_mkstream(stg, L"A"), *b1 = ses_mkstream(stg, L"B");
    unsigned char wa[9000], wb[9000], ra[9000], rb[9000]; ULONG n; int i;
    if (!a1 || !b1) { CHECK(a1 && b1); return; }
    ses_fill(wa, sizeof wa, 11); ses_fill(wb, sizeof wb, 22);
    for (i = 0; i < 9000; i += 300) {
        HR(IStream_Write(a1, wa + i, 300, &n), S_OK); EQ(n, 300);
        HR(IStream_Write(b1, wb + i, 300, &n), S_OK); EQ(n, 300);
    }
    EQ(ses_stream_size(a1), 9000); EQ(ses_stream_size(b1), 9000);
    IStream_Release(a1); IStream_Release(b1);
    EQ(ses_get(stg, L"A", ra, sizeof ra), 9000); CHECK(!memcmp(ra, wa, sizeof wa));
    EQ(ses_get(stg, L"B", rb, sizeof rb), 9000); CHECK(!memcmp(rb, wb, sizeof wb));
}
/* byte-at-a-time writes across the 4096 byte mini stream cutoff */
DF_TEST(ses_xt_byte_writes_cross_ministream_cutoff) {
    IStream *s = ses_mkstream(stg, L"bytes"); unsigned char w[5000], r[5000]; ULONG n; int i;
    if (!s) { CHECK(s != NULL); return; }
    ses_fill(w, sizeof w, 5);
    for (i = 0; i < 5000; i++) { n = 9; if (IStream_Write(s, w + i, 1, &n) != S_OK || n != 1) { CHECK(0); break; } }
    EQ(ses_stream_size(s), 5000);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, 5000); CHECK(!memcmp(r, w, sizeof w));
    IStream_Release(s);
}
/* rename moves the element to its new sorted position and keeps its data */
DF_TEST(ses_xt_rename_resorts_enumeration) {
    WCHAR nm[8][40]; DWORD ty[8]; ULONGLONG sz[8]; int c;
    HR(ses_put(stg, L"bb", "x", 1), S_OK);
    HR(ses_put(stg, L"cc", "yy", 2), S_OK);
    HR(IStorage_RenameElement(stg, L"cc", L"a"), S_OK);
    c = ses_list(a, stg, nm, ty, sz, 8);
    EQ(c, 2);
    if (c == 2) {
        CHECK(!lstrcmpW(nm[0], L"a")); EQ(sz[0], 2); EQ(ty[0], STGTY_STREAM);
        CHECK(!lstrcmpW(nm[1], L"bb")); EQ(sz[1], 1);
    }
    CHECK(!ses_exists_stream(stg, L"cc"));
}
/* shorter names sort before longer ones even when alphabetically later; same length by case-insensitive compare */
DF_TEST(ses_xt_enum_order_mixed_lengths_and_case) {
    static const WCHAR *in[] = { L"zz", L"Abc", L"b", L"ABD", L"aa", L"Z" };
    static const WCHAR *want[] = { L"b", L"Z", L"aa", L"zz", L"Abc", L"ABD" };
    WCHAR nm[8][40]; int c, i;
    for (i = 0; i < 6; i++) HR(ses_put(stg, in[i], "1", 1), S_OK);
    c = ses_list(a, stg, nm, NULL, NULL, 8); EQ(c, 6);
    for (i = 0; i < c && i < 6; i++) CHECK(!lstrcmpW(nm[i], want[i]));
}
/* destroying a storage removes its stream data from enumeration and from a subsequent same-name create */
DF_TEST(ses_xt_destroy_storage_then_create_stream_same_name) {
    IStorage *g = NULL; IStream *s = NULL; WCHAR nm[4][40]; DWORD ty[4]; int c;
    HR(IStorage_CreateStorage(stg, L"dup", MODE_RW, 0, 0, &g), S_OK); if (g) IStorage_Release(g);
    HR(IStorage_DestroyElement(stg, L"dup"), S_OK);
    HR(IStorage_CreateStream(stg, L"dup", MODE_RW, 0, 0, &s), S_OK); if (s) IStream_Release(s);
    c = ses_list(a, stg, nm, ty, NULL, 4); EQ(c, 1);
    if (c == 1) EQ(ty[0], STGTY_STREAM);
    CHECK(!ses_exists_storage(stg, L"dup"));
}
/* stream data survives destroying and renaming neighbours (directory/FAT updates do not corrupt chains) */
DF_TEST(ses_xt_neighbour_ops_preserve_data) {
    unsigned char w[6000], r[6000]; int i; WCHAR n[8];
    ses_fill(w, sizeof w, 77);
    HR(ses_put(stg, L"keep", w, sizeof w), S_OK);
    for (i = 0; i < 20; i++) { n[0] = L'n'; n[1] = (WCHAR)(L'a' + i); n[2] = 0; HR(ses_put(stg, n, w, 100 + i * 300), S_OK); }
    for (i = 0; i < 20; i += 2) { n[0] = L'n'; n[1] = (WCHAR)(L'a' + i); n[2] = 0; HR(IStorage_DestroyElement(stg, n), S_OK); }
    HR(IStorage_RenameElement(stg, L"keep", L"kept"), S_OK);
    EQ(ses_get(stg, L"kept", r, sizeof r), 6000); CHECK(!memcmp(r, w, sizeof w));
    for (i = 1; i < 20; i += 2) { n[0] = L'n'; n[1] = (WCHAR)(L'a' + i); n[2] = 0; EQ(ses_get(stg, n, r, sizeof r), 100 + i * 300); CHECK(!memcmp(r, w, 100 + i * 300)); }
}
/* SetSize then Write at the new end keeps the grown region zero-filled on docfile streams */
DF_TEST(ses_xt_setsize_then_write_at_end) {
    IStream *s = ses_mkstream(stg, L"s"); unsigned char r[300]; ULONG n;
    if (!s) { CHECK(s != NULL); return; }
    HR(IStream_SetSize(s, ULI(200)), S_OK); EQ(ses_stream_size(s), 200);
    ses_seek(s, 0, STREAM_SEEK_END, NULL); EQ(ses_pos(s), 200);
    HR(ses_write_all(s, "tail", 4), S_OK); EQ(ses_stream_size(s), 204);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); memset(r, 0xee, sizeof r);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EQ(n, 204);
    CHECKW(ses_allzero(r, 200)); CHECK(!memcmp(r + 200, "tail", 4));
    IStream_Release(s);
}
/* an HGLOBAL-backed stream and a docfile stream copy into each other byte-exactly, in both directions */
DF_TEST(ses_xt_copy_hglobal_to_docfile_and_back) {
    IStream *m = NULL, *d = ses_mkstream(stg, L"d"), *m2 = NULL; unsigned char w[7000], r[7000]; ULONG n; ULARGE_INTEGER rd, wr;
    if (!d || a->stm_hglobal(NULL, TRUE, &m) != S_OK || a->stm_hglobal(NULL, TRUE, &m2) != S_OK) { CHECK(0); return; }
    ses_fill(w, sizeof w, 9); HR(ses_write_all(m, w, sizeof w), S_OK); ses_seek(m, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(m, d, ULI(sizeof w), &rd, &wr), S_OK); EQ(rd.QuadPart, 7000); EQ(wr.QuadPart, 7000);
    EQ(ses_stream_size(d), 7000);
    ses_seek(d, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(d, m2, ULI(sizeof w), &rd, &wr), S_OK); EQ(rd.QuadPart, 7000); EQ(wr.QuadPart, 7000);
    ses_seek(m2, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(m2, r, sizeof r, &n), S_OK); EQ(n, 7000); CHECK(!memcmp(r, w, sizeof w));
    IStream_Release(m); IStream_Release(m2); IStream_Release(d);
}
/* Stat of an enumerated child storage reports STGTY_STORAGE and a CLSID set before enumeration */
DF_TEST(ses_xt_enum_reports_child_clsid) {
    IStorage *g = NULL; IEnumSTATSTG *e = NULL; STATSTG st; ULONG f = 0;
    HR(IStorage_CreateStorage(stg, L"sub", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    HR(IStorage_SetClass(g, &SES_CLSID_A), S_OK); IStorage_Release(g);
    HR(IStorage_EnumElements(stg, 0, NULL, 0, &e), S_OK); if (!e) return;
    memset(&st, 0, sizeof st);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EQ(f, 1);
    EQ(st.type, STGTY_STORAGE);
    CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A));
    if (st.pwcsName) a->taskfree(st.pwcsName);
    IEnumSTATSTG_Release(e);
}
