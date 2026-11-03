/* CreateStreamOnHGlobal / GetHGlobalFromStream basics and CoMarshalHresult / CoUnmarshalHresult. */
#include "mb.h"
#include <stdlib.h>

/* ---------------------------------------------------------------- CreateStreamOnHGlobal */

TEST(mbasic_cso_null_hglobal_creates_empty_stream) {
    REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(GetHGlobalFromStream));
    IStream *s = NULL; STATSTG st; HGLOBAL h = NULL;
    HRESULT hr = U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    EQ(hr, S_OK); CHECK(s != NULL);
    if (!s) return;
    memset(&st, 0xAA, sizeof st);
    EQ(IStream_Stat(s, &st, STATFLAG_NONAME), S_OK);
    EQ(st.type, STGTY_STREAM);
    CHECK(st.cbSize.QuadPart == 0);
    CHECK(st.pwcsName == NULL);
    hr = U(GetHGlobalFromStream)(s, &h);
    EQ(hr, S_OK); CHECK(h != NULL);
    IStream_Release(s);
}

TEST(mbasic_cso_null_ppstm_fails) {
    REQUIRE(U(CreateStreamOnHGlobal));
    HRESULT hr = U(CreateStreamOnHGlobal)(NULL, TRUE, NULL);
    CHECKF(FAILED(hr), "hr=0x%lx", hr);
    t_note("CreateStreamOnHGlobal(NULL, TRUE, NULL) = 0x%lx", (unsigned long)hr);
}

TEST(mbasic_cso_ppstm_cleared_on_entry) {
    REQUIRE(U(CreateStreamOnHGlobal));
    /* decomp: *ppstm is zeroed before any failure path runs; on success it is replaced. */
    IStream *s = (IStream *)(ULONG_PTR)0x1;
    HRESULT hr = U(CreateStreamOnHGlobal)(NULL, TRUE, &s);
    EQ(hr, S_OK);
    CHECK(s != NULL && s != (IStream *)(ULONG_PTR)0x1);
    if (s && s != (IStream *)(ULONG_PTR)1) IStream_Release(s);
}

TEST(mbasic_cso_write_read_roundtrip) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    const char data[] = "hello, marshal world";
    ULONG w = 0, r = 0; char buf[64];
    EQ(IStream_Write(s, data, sizeof data, &w), S_OK); EQ(w, sizeof data);
    EQ(mb_pos(s), sizeof data);
    EQ(mb_size(s), sizeof data);
    mb_rewind(s);
    memset(buf, 0, sizeof buf);
    EQ(IStream_Read(s, buf, sizeof buf, &r), S_OK);   /* short read is S_OK, never S_FALSE, for an HGLOBAL stream */
    EQ(r, sizeof data);
    CHECK(!memcmp(buf, data, sizeof data));
    EQ(IStream_Read(s, buf, 1, &r), S_OK);          /* at EOF */
    EQ(r, 0);
    IStream_Release(s);
}

TEST(mbasic_cso_read_pcb_null_ok) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    char b[4] = {1, 2, 3, 4};
    EQ(IStream_Write(s, b, 4, NULL), S_OK);
    mb_rewind(s);
    char o[4] = {0};
    EQ(IStream_Read(s, o, 4, NULL), S_OK);
    CHECK(!memcmp(o, b, 4));
    IStream_Release(s);
}

TEST(mbasic_cso_seek_semantics) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    char b[100]; memset(b, 7, sizeof b);
    IStream_Write(s, b, sizeof b, NULL);
    LARGE_INTEGER mv; ULARGE_INTEGER np;
    mv.QuadPart = 10; EQ(IStream_Seek(s, mv, STREAM_SEEK_SET, &np), S_OK); EQ(np.QuadPart, 10);
    mv.QuadPart = 5;  EQ(IStream_Seek(s, mv, STREAM_SEEK_CUR, &np), S_OK); EQ(np.QuadPart, 15);
    mv.QuadPart = -5; EQ(IStream_Seek(s, mv, STREAM_SEEK_CUR, &np), S_OK); EQ(np.QuadPart, 10);
    mv.QuadPart = 0;  EQ(IStream_Seek(s, mv, STREAM_SEEK_END, &np), S_OK); EQ(np.QuadPart, 100);
    mv.QuadPart = -100; EQ(IStream_Seek(s, mv, STREAM_SEEK_END, &np), S_OK); EQ(np.QuadPart, 0);
    /* seeking before the start fails and leaves the pointer alone */
    mv.QuadPart = 20; IStream_Seek(s, mv, STREAM_SEEK_SET, NULL);
    mv.QuadPart = -21; np.QuadPart = 0xCAFE;
    EQ(IStream_Seek(s, mv, STREAM_SEEK_CUR, &np), STG_E_SEEKERROR);
    EQ(mb_pos(s), 20);
    /* bad origin */
    mv.QuadPart = 0;
    EQ(IStream_Seek(s, mv, 3, NULL), STG_E_SEEKERROR);
    EQ(mb_pos(s), 20);
    /* seeking beyond the end is allowed, reads then return 0 bytes */
    mv.QuadPart = 1000; EQ(IStream_Seek(s, mv, STREAM_SEEK_SET, &np), S_OK); EQ(np.QuadPart, 1000);
    ULONG r = 99; char o[8];
    EQ(IStream_Read(s, o, 8, &r), S_OK); EQ(r, 0);
    EQ(mb_size(s), 100);
    IStream_Release(s);
}

TEST(mbasic_cso_write_beyond_end_extends) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    LARGE_INTEGER mv; mv.QuadPart = 50;
    IStream_Seek(s, mv, STREAM_SEEK_SET, NULL);
    EQ(IStream_Write(s, "AB", 2, NULL), S_OK);
    EQ(mb_size(s), 52);
    ULONG n; BYTE *d = mb_dump(s, &n);
    EQ(n, 52);
    int zero = 1; for (int i = 0; i < 50 && i < (int)n; i++) if (d[i]) zero = 0;
    if (!zero) t_note("gap bytes written by a seek-past-end Write are not zero"); /* unverified: not decomp-checked */
    CHECK(n >= 52 && d[50] == 'A' && d[51] == 'B');
    free(d);
    IStream_Release(s);
}

TEST(mbasic_cso_setsize_grow_shrink) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    ULARGE_INTEGER n; n.QuadPart = 4096;
    EQ(IStream_SetSize(s, n), S_OK);
    EQ(mb_size(s), 4096);
    EQ(mb_pos(s), 0);       /* SetSize does not move the seek pointer */
    n.QuadPart = 10;
    EQ(IStream_SetSize(s, n), S_OK);
    EQ(mb_size(s), 10);
    n.QuadPart = 0;
    EQ(IStream_SetSize(s, n), S_OK);
    EQ(mb_size(s), 0);
    IStream_Release(s);
}

TEST(mbasic_cso_existing_hglobal_size_and_data) {
    REQUIRE(U(CreateStreamOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 64);
    CHECK(h != NULL); if (!h) return;
    BYTE *p = GlobalLock(h); for (int i = 0; i < 64; i++) p[i] = (BYTE)i; GlobalUnlock(h);
    IStream *s = NULL;
    EQ(U(CreateStreamOnHGlobal)(h, TRUE, &s), S_OK);
    if (!s) { GlobalFree(h); return; }
    EQ(mb_size(s), GlobalSize(h));           /* decomp: initial size is GlobalSize */
    BYTE buf[64]; ULONG r = 0;
    EQ(IStream_Read(s, buf, 64, &r), S_OK); EQ(r, 64);
    int ok = 1; for (int i = 0; i < 64; i++) if (buf[i] != i) ok = 0;
    CHECK(ok);
    HGLOBAL h2 = NULL;
    EQ(U(GetHGlobalFromStream)(s, &h2), S_OK);
    EQ(h2, h);                               /* same handle until the stream has to grow */
    IStream_Release(s);                             /* fDeleteOnRelease: handle freed by the stream */
}

TEST(mbasic_cso_no_delete_keeps_hglobal) {
    REQUIRE(U(CreateStreamOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 32);
    CHECK(h != NULL); if (!h) return;
    IStream *s = NULL;
    EQ(U(CreateStreamOnHGlobal)(h, FALSE, &s), S_OK);
    if (!s) { GlobalFree(h); return; }
    IStream_Write(s, "xyz", 3, NULL);
    IStream_Release(s);
    CHECK(GlobalSize(h) >= 3);                      /* still a valid allocation */
    BYTE *p = GlobalLock(h);
    CHECK(p && p[0] == 'x' && p[1] == 'y' && p[2] == 'z');
    GlobalUnlock(h);
    EQ(GlobalFree(h), NULL);
}

TEST(mbasic_cso_stream_grows_past_hglobal) {
    REQUIRE(U(CreateStreamOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 16);
    CHECK(h != NULL); if (!h) return;
    IStream *s = NULL;
    EQ(U(CreateStreamOnHGlobal)(h, FALSE, &s), S_OK);
    if (!s) { GlobalFree(h); return; }
    BYTE big[4096]; memset(big, 0x5A, sizeof big);
    EQ(IStream_Write(s, big, sizeof big, NULL), S_OK);
    HGLOBAL h2 = NULL;
    EQ(U(GetHGlobalFromStream)(s, &h2), S_OK);
    CHECK(h2 != NULL);
    CHECK(GlobalSize(h2) >= sizeof big);
    IStream_Release(s);
    GlobalFree(h2);
}

TEST(mbasic_cso_stream_release_no_delete_null_hglobal_handle_survives) {
    REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(GetHGlobalFromStream));
    IStream *s = NULL; HGLOBAL h = NULL;
    EQ(U(CreateStreamOnHGlobal)(NULL, FALSE, &s), S_OK);
    if (!s) return;
    IStream_Write(s, "keep", 4, NULL);
    EQ(U(GetHGlobalFromStream)(s, &h), S_OK);
    IStream_Release(s);
    CHECK(h != NULL);
    if (h) {
        CHECK(GlobalSize(h) >= 4);
        char *p = GlobalLock(h); CHECK(p && !memcmp(p, "keep", 4)); GlobalUnlock(h);
        GlobalFree(h);
    }
}

TEST(mbasic_ghfs_invalid_args) {
    REQUIRE(U(GetHGlobalFromStream));
    HGLOBAL h = (HGLOBAL)(ULONG_PTR)1;
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    EQ(U(GetHGlobalFromStream)(NULL, &h), MB_E_INVALIDARG);
    EQ(U(GetHGlobalFromStream)(s, NULL), MB_E_INVALIDARG);
    IStream_Release(s);
}

TEST(mbasic_ghfs_foreign_stream_rejected) {
    REQUIRE(U(GetHGlobalFromStream));
    /* decomp: the object must carry the 'STRM' signature of ole32's own memory stream */
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); return; }
    HGLOBAL h = (HGLOBAL)(ULONG_PTR)1;
    EQ(U(GetHGlobalFromStream)(&w->s, &h), MB_E_INVALIDARG);
    IStream_Release(&w->s);
}

TEST(mbasic_cso_clone_shares_data) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(), *c = NULL; if (!s) { CHECK(0); return; }
    IStream_Write(s, "0123456789", 10, NULL);
    EQ(IStream_Clone(s, &c), S_OK); CHECK(c != NULL);
    if (c) {
        mb_rewind(c);
        char b[10]; ULONG r = 0;
        EQ(IStream_Read(c, b, 10, &r), S_OK); EQ(r, 10);
        CHECK(!memcmp(b, "0123456789", 10));
        EQ(mb_pos(s), 10);                   /* seek pointers are independent */
        IStream_Write(c, "X", 1, NULL);             /* write through the clone ... */
        EQ(mb_size(s), 11);                  /* ... is visible through the original */
        IStream_Release(c);
    }
    IStream_Release(s);
}

TEST(mbasic_cso_copyto) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *a = mb_stream(), *b = mb_stream(); if (!a || !b) { CHECK(0); return; }
    IStream_Write(a, "ABCDEFGHIJ", 10, NULL); mb_rewind(a);
    ULARGE_INTEGER n, rd, wr; n.QuadPart = 6;
    EQ(IStream_CopyTo(a, b, n, &rd, &wr), S_OK);
    EQ(rd.QuadPart, 6); EQ(wr.QuadPart, 6);
    EQ(mb_pos(a), 6); EQ(mb_size(b), 6);
    n.QuadPart = 100;
    EQ(IStream_CopyTo(a, b, n, &rd, &wr), S_OK);
    EQ(rd.QuadPart, 4); EQ(wr.QuadPart, 4);
    EQ(mb_size(b), 10);
    IStream_Release(a); IStream_Release(b);
}

TEST(mbasic_cso_commit_revert_lock) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    ULARGE_INTEGER o, c; o.QuadPart = 0; c.QuadPart = 1;
    EQ(IStream_Commit(s, STGC_DEFAULT), S_OK);
    t_note("IStream_Revert on HGLOBAL stream = 0x%lx", (unsigned long)IStream_Revert(s));
    t_note("HGLOBAL stream LockRegion=0x%lx UnlockRegion=0x%lx", (unsigned long)IStream_LockRegion(s, o, c, LOCK_WRITE), (unsigned long)IStream_UnlockRegion(s, o, c, LOCK_WRITE));
    IStream_Release(s);
}

TEST(mbasic_cso_qi_interfaces) {
    REQUIRE(U(CreateStreamOnHGlobal));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    void *p = NULL;
    EQ(IStream_QueryInterface(s, &IID_ISequentialStream, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EQ(IStream_QueryInterface(s, &IID_IUnknown, &p), S_OK); EQ(p, s); if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EQ(IStream_QueryInterface(s, &IID_IStorage, &p), MB_E_NOINTERFACE); EQ(p, 0);
    IStream_Release(s);
}

/* ---------------------------------------------------------------- CoMarshalHresult / CoUnmarshalHresult */

static const HRESULT hr_values[] = { S_OK, S_FALSE, E_FAIL, E_NOTIMPL, E_NOINTERFACE, E_UNEXPECTED, E_OUTOFMEMORY,
    (HRESULT)0x80070057, (HRESULT)0xDEADBEEF, (HRESULT)0x00040202, (HRESULT)0x80030001, (HRESULT)0xFFFFFFFF, (HRESULT)0x80010108, 1, 0x7FFFFFFF };

/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_hresult_marshal_stream_layout) {
    REQUIRE(U(CoMarshalHresult)); REQUIRE(U(CoUnmarshalHresult));
    for (unsigned i = 0; i < sizeof hr_values / sizeof *hr_values; i++) {
        IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
        HRESULT hr = U(CoMarshalHresult)(s, hr_values[i]);
        EQ(hr, S_OK);
        EQ(mb_pos(s), 4);
        EQ(mb_size(s), 4);
        ULONG n; BYTE *d = mb_dump(s, &n);
        EQ(n, 4);
        CHECK(n == 4 && !memcmp(d, &hr_values[i], 4));   /* raw little-endian HRESULT, no header */
        free(d);
        IStream_Release(s);
    }
}

TEST(mbasic_hresult_roundtrip_all_values) {
    REQUIRE(U(CoMarshalHresult)); REQUIRE(U(CoUnmarshalHresult));
    for (unsigned i = 0; i < sizeof hr_values / sizeof *hr_values; i++) {
        IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
        U(CoMarshalHresult)(s, hr_values[i]);
        mb_rewind(s);
        HRESULT out = 0x12345678;
        HRESULT hr = U(CoUnmarshalHresult)(s, &out);
        EQ(hr, S_OK);
        EQ((DWORD)out, (DWORD)hr_values[i]);
        EQ(mb_pos(s), 4);
        IStream_Release(s);
    }
}

TEST(mbasic_hresult_unmarshal_reads_from_current_position) {
    REQUIRE(U(CoMarshalHresult)); REQUIRE(U(CoUnmarshalHresult));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    U(CoMarshalHresult)(s, 0x11111111); U(CoMarshalHresult)(s, 0x22222222); U(CoMarshalHresult)(s, 0x33333333);
    EQ(mb_size(s), 12);
    mb_rewind(s);
    HRESULT a = 0, b = 0, c = 0;
    EQ(U(CoUnmarshalHresult)(s, &a), S_OK);
    EQ(U(CoUnmarshalHresult)(s, &b), S_OK);
    EQ(U(CoUnmarshalHresult)(s, &c), S_OK);
    EQ(a, 0x11111111); EQ(b, 0x22222222); EQ(c, 0x33333333);
    EQ(mb_pos(s), 12);
    IStream_Release(s);
}

TEST(mbasic_hresult_marshal_overwrites_at_position) {
    REQUIRE(U(CoMarshalHresult));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    U(CoMarshalHresult)(s, 1); U(CoMarshalHresult)(s, 2);
    LARGE_INTEGER mv; mv.QuadPart = 4; IStream_Seek(s, mv, STREAM_SEEK_SET, NULL);
    U(CoMarshalHresult)(s, 3);
    EQ(mb_size(s), 8);
    ULONG n; BYTE *d = mb_dump(s, &n);
    DWORD v[2]; memcpy(v, d, 8);
    EQ(v[0], 1); EQ(v[1], 3);
    free(d);
    IStream_Release(s);
}

TEST(mbasic_hresult_marshal_null_stream) {
    REQUIRE(U(CoMarshalHresult));
    if (t_is_wine()) { t_skip("wine crashes on NULL stream"); return; }
    EQ(U(CoMarshalHresult)(NULL, E_FAIL), MB_E_INVALIDARG);   /* decomp: IsValidInterface fails -> E_INVALIDARG */
}

TEST(mbasic_hresult_unmarshal_invalid_args) {
    REQUIRE(U(CoUnmarshalHresult));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    HRESULT out = 0x1234;
    if (!t_is_wine()) EQ(U(CoUnmarshalHresult)(NULL, &out), MB_E_INVALIDARG);
    EQ((DWORD)out, 0x1234);
    EQ(U(CoUnmarshalHresult)(s, NULL), MB_E_INVALIDARG);
    IStream_Release(s);
}

TEST(mbasic_hresult_unmarshal_empty_stream_yields_unexpected) {
    REQUIRE(U(CoUnmarshalHresult));
    /* decomp: a local pre-initialised to 0x8000FFFF (E_UNEXPECTED) is passed to IStream::Read(4) and copied out
     * if Read returned success, without checking the byte count. An exhausted HGLOBAL stream reads 0 bytes with S_OK. */
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    HRESULT out = 0x1234;
    HRESULT hr = U(CoUnmarshalHresult)(s, &out);
    EQ(hr, S_OK);
    EQ((DWORD)out, 0x8000FFFF);
    IStream_Release(s);
}

TEST(mbasic_hresult_unmarshal_short_stream_partial) {
    REQUIRE(U(CoUnmarshalHresult));
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    IStream_Write(s, "\x01\x02", 2, NULL); mb_rewind(s);
    HRESULT out = 0x1234;
    HRESULT hr = U(CoUnmarshalHresult)(s, &out);
    EQ(hr, S_OK);
    t_note("2-byte stream: CoUnmarshalHresult out=0x%lx (partial read, bytes beyond the read count are the 0x8000FFFF preload)", (unsigned long)out);
    IStream_Release(s);
}

TEST(mbasic_hresult_marshal_one_4byte_write) {
    REQUIRE(U(CoMarshalHresult));
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); return; }
    EQ(U(CoMarshalHresult)(&w->s, 0xC0000005), S_OK);
    EQ(w->writes, 1); EQ(w->last_write_cb, 4);
    EQ(w->reads, 0);
    IStream_Release(&w->s);
}

TEST(mbasic_hresult_unmarshal_one_4byte_read) {
    REQUIRE(U(CoUnmarshalHresult)); REQUIRE(U(CoMarshalHresult));
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); return; }
    U(CoMarshalHresult)(&w->s, 0x80004005); mb_rewind(&w->s);
    HRESULT out = 0;
    EQ(U(CoUnmarshalHresult)(&w->s, &out), S_OK);
    EQ(w->reads, 1); EQ(w->last_read_cb, 4);
    EQ((DWORD)out, 0x80004005);
    IStream_Release(&w->s);
}

TEST(mbasic_hresult_marshal_stream_write_failure_propagates) {
    REQUIRE(U(CoMarshalHresult));
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); return; }
    w->fail_write = TRUE;
    EQ(U(CoMarshalHresult)(&w->s, 5), STG_E_ACCESSDENIED);
    IStream_Release(&w->s);
}

TEST(mbasic_hresult_unmarshal_read_failure_leaves_output) {
    REQUIRE(U(CoUnmarshalHresult));
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); return; }
    w->fail_read = TRUE;
    HRESULT out = 0x7777;
    EQ(U(CoUnmarshalHresult)(&w->s, &out), STG_E_ACCESSDENIED);
    EQ((DWORD)out, 0x7777);                    /* decomp: *out written only on success */
    IStream_Release(&w->s);
}

TEST(mbasic_hresult_marshal_needs_no_com_init) {
    REQUIRE(U(CoMarshalHresult)); REQUIRE(U(CoUnmarshalHresult));
    /* neither function touches apartment state: they work without CoInitialize */
    IStream *s = mb_stream(); if (!s) { CHECK(0); return; }
    EQ(U(CoMarshalHresult)(s, 0x1357), S_OK);
    mb_rewind(s);
    HRESULT o = 0; EQ(U(CoUnmarshalHresult)(s, &o), S_OK); EQ(o, 0x1357);
    IStream_Release(s);
}
