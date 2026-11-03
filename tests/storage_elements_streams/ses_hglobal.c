/* IStream on HGLOBAL (CreateStreamOnHGlobal / GetHGlobalFromStream).
 * Expectations come from the Windows 10 combase CMemStm decompilation (Read/Write/Seek/SetSize/CopyTo/Stat/
 * Clone/QueryInterface/LockRegion) and from the CreateStreamOnHGlobal/GetHGlobalFromStream decompilation. */
#include "ses.h"

static IStream *mk(const ses_api *a, HGLOBAL h, BOOL del) { IStream *s = NULL; if (a->stm_hglobal(h, del, &s) != S_OK) return NULL; return s; }

/* adapted from Wine dlls/ole32/tests/hglobalstream.c (LGPL-2.1+) */
TEST(ses_hg_create_null_basic) {
    SES_REQUIRE(a);
    IStream *s = (IStream *)(ULONG_PTR)1;
    HR(a.stm_hglobal(NULL, TRUE, &s), S_OK);
    CHECK(s != NULL);
    if (!s) return;
    EXPECT_EQ(ses_stream_size(s), 0);
    EXPECT_EQ(ses_pos(s), 0);
    EQ(IStream_Release(s), 0);
}
TEST(ses_hg_create_null_ppstm) {
    SES_REQUIRE(a);
    /* decomp: ppstm == NULL -> E_INVALIDARG (checked before anything is allocated) */
    HR(a.stm_hglobal(NULL, TRUE, NULL), E_INVALIDARG);
    HR(a.stm_hglobal(NULL, FALSE, NULL), E_INVALIDARG);
}
TEST(ses_hg_create_clears_out_on_entry) {
    SES_REQUIRE(a);
    /* both delete modes return a usable stream */
    for (int del = 0; del < 2; del++) {
        IStream *s = NULL; HGLOBAL h = NULL;
        HR(a.stm_hglobal(NULL, del, &s), S_OK);
        if (!s) continue;
        HR(a.hglobal_from_stm(s, &h), S_OK);
        CHECK(h != NULL);
        IStream_Release(s);
        if (!del && h) CHECK(GlobalFree(h) == NULL);
    }
}
TEST(ses_hg_query_interface_set) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    void *p;
    p = NULL; HR(IStream_QueryInterface(s, &IID_IUnknown, &p), S_OK); CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; HR(IStream_QueryInterface(s, &IID_ISequentialStream, &p), S_OK); CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; HR(IStream_QueryInterface(s, &IID_IStream, &p), S_OK); CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p);
    IStream_Release(s);
}
TEST(ses_hg_query_interface_identity) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    IUnknown *u1 = NULL, *u2 = NULL; IStream *s2 = NULL;
    HR(IStream_QueryInterface(s, &IID_IUnknown, (void **)&u1), S_OK);
    HR(IUnknown_QueryInterface(u1, &IID_IUnknown, (void **)&u2), S_OK);
    EXPECT_EQ(u1, u2);
    HR(IUnknown_QueryInterface(u1, &IID_IStream, (void **)&s2), S_OK);
    EXPECT_EQ(s2, s);
    if (u1) IUnknown_Release(u1); if (u2) IUnknown_Release(u2); if (s2) IStream_Release(s2);
    IStream_Release(s);
}
TEST(ses_hg_query_interface_unsupported) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    static const IID *iids[] = { &IID_IPersist, &IID_IPersistStream, &IID_IMarshal, &IID_IStorage, &IID_IPropertySetStorage, &IID_IDispatch, &IID_ILockBytes };
    for (unsigned i = 0; i < sizeof iids / sizeof *iids; i++) {
        void *p = (void *)(ULONG_PTR)1;
        HR(IStream_QueryInterface(s, iids[i], &p), E_NOINTERFACE);
        CHECKF(p == NULL, "iid index %u: *ppv must be NULL on failure", i);
    }
    IStream_Release(s);
}
TEST(ses_hg_query_interface_null_ppv) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HR(IStream_QueryInterface(s, &IID_IStream, NULL), E_INVALIDARG);
    IStream_Release(s);
}
TEST(ses_hg_refcounting) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    EQ(IStream_AddRef(s), 2);
    EQ(IStream_AddRef(s), 3);
    EQ(IStream_Release(s), 2);
    void *p; HR(IStream_QueryInterface(s, &IID_IStream, &p), S_OK);
    EQ(IStream_Release(s), 2);
    EQ(IStream_Release(s), 1);
    EQ(IStream_Release(s), 0);
}
TEST(ses_hg_write_read_roundtrip) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char w[1000], r[1000]; ULONG n = 0xdead;
    ses_fill(w, sizeof w, 1);
    HR(IStream_Write(s, w, sizeof w, &n), S_OK); EXPECT_EQ(n, sizeof w);
    EXPECT_EQ(ses_pos(s), 1000);
    EXPECT_EQ(ses_stream_size(s), 1000);
    HR(ses_seek(s, 0, STREAM_SEEK_SET, NULL), S_OK);
    n = 0xdead; memset(r, 0, sizeof r);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EXPECT_EQ(n, sizeof r);
    CHECK(!memcmp(w, r, sizeof w));
    IStream_Release(s);
}
TEST(ses_hg_write_null_pcb) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HR(IStream_Write(s, "abcdef", 6, NULL), S_OK);
    EXPECT_EQ(ses_pos(s), 6);
    IStream_Release(s);
}
TEST(ses_hg_write_zero_bytes) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONG n = 77;
    HR(IStream_Write(s, "x", 0, &n), S_OK); EXPECT_EQ(n, 0);
    EXPECT_EQ(ses_stream_size(s), 0);
    HR(ses_seek(s, 10, STREAM_SEEK_SET, NULL), S_OK);
    n = 77; HR(IStream_Write(s, "x", 0, &n), S_OK); EXPECT_EQ(n, 0);
    EXPECT_EQ(ses_stream_size(s), 0); /* zero-length write past the end does not extend */
    IStream_Release(s);
}
TEST(ses_hg_read_zero_bytes) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    ULONG n = 77; char c = 'Q';
    HR(IStream_Read(s, &c, 0, &n), S_OK); EXPECT_EQ(n, 0); EXPECT_EQ(c, 'Q');
    HR(IStream_Read(s, NULL, 0, &n), S_OK); /* decomp: NULL buffer is only rejected when cb != 0 */
    IStream_Release(s);
}
TEST(ses_hg_read_null_buffer_nonzero) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    ULONG n = 77;
    if (t_is_wine()) { t_skip("wine faults on NULL buffer"); IStream_Release(s); return; }
    HR(IStream_Read(s, NULL, 3, &n), E_INVALIDARG);
    EXPECT_EQ(ses_pos(s), 0);
    IStream_Release(s);
}
TEST(ses_hg_read_null_pcb) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    char b[8] = {0};
    ses_write_all(s, "abcdef", 6); ses_seek(s, 1, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, b, 3, NULL), S_OK);
    EXPECT_EQ(memcmp(b, "bcd", 3), 0);
    EXPECT_EQ(ses_pos(s), 4);
    IStream_Release(s);
}
TEST(ses_hg_read_short_at_eof_is_ok) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    char b[16]; ULONG n = 0xdead;
    ses_write_all(s, "0123456789", 10); ses_seek(s, 6, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); /* S_OK, not S_FALSE, when fewer bytes than requested */
    EXPECT_EQ(n, 4); EXPECT_EQ(memcmp(b, "6789", 4), 0);
    EXPECT_EQ(ses_pos(s), 10);
    n = 0xdead;
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EXPECT_EQ(n, 0); /* at EOF: S_OK with 0 bytes */
    EXPECT_EQ(ses_pos(s), 10);
    IStream_Release(s);
}
TEST(ses_hg_read_past_eof_position_unchanged) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    char b[16]; ULONG n = 0xdead;
    ses_write_all(s, "abc", 3);
    HR(ses_seek(s, 100, STREAM_SEEK_SET, NULL), S_OK);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EXPECT_EQ(n, 0);
    EXPECT_EQ(ses_pos(s), 100);
    EXPECT_EQ(ses_stream_size(s), 3);
    IStream_Release(s);
}
TEST(ses_hg_seek_origins) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    unsigned char b[100]; ses_fill(b, sizeof b, 2); ses_write_all(s, b, sizeof b);
    HR(ses_seek(s, 10, STREAM_SEEK_SET, &np), S_OK); EXPECT_EQ(np, 10);
    HR(ses_seek(s, 5, STREAM_SEEK_CUR, &np), S_OK); EXPECT_EQ(np, 15);
    HR(ses_seek(s, -5, STREAM_SEEK_CUR, &np), S_OK); EXPECT_EQ(np, 10);
    HR(ses_seek(s, 0, STREAM_SEEK_END, &np), S_OK); EXPECT_EQ(np, 100);
    HR(ses_seek(s, -100, STREAM_SEEK_END, &np), S_OK); EXPECT_EQ(np, 0);
    HR(ses_seek(s, -1, STREAM_SEEK_END, &np), S_OK); EXPECT_EQ(np, 99);
    HR(ses_seek(s, 50, STREAM_SEEK_END, &np), S_OK); EXPECT_EQ(np, 150); /* past the end is allowed */
    EXPECT_EQ(ses_stream_size(s), 100);
    IStream_Release(s);
}
TEST(ses_hg_seek_null_out) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HR(IStream_Seek(s, LI(7), STREAM_SEEK_SET, NULL), S_OK);
    EXPECT_EQ(ses_pos(s), 7);
    IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/hglobalstream.c (LGPL-2.1+) */
TEST(ses_hg_seek_negative_set_fails) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    ses_write_all(s, "abcdef", 6); ses_seek(s, 4, STREAM_SEEK_SET, NULL);
    HR(ses_seek(s, -1, STREAM_SEEK_SET, &np), STG_E_SEEKERROR);
    EXPECT_EQ(np, 4); /* *plibNewPosition reports the unchanged position on failure */
    EXPECT_EQ(ses_pos(s), 4);
    IStream_Release(s);
}
TEST(ses_hg_seek_negative_cur_before_start) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    ses_write_all(s, "abcdef", 6); ses_seek(s, 4, STREAM_SEEK_SET, NULL);
    HR(ses_seek(s, -5, STREAM_SEEK_CUR, &np), STG_E_SEEKERROR); EXPECT_EQ(np, 4);
    HR(ses_seek(s, -4, STREAM_SEEK_CUR, &np), S_OK); EXPECT_EQ(np, 0); /* landing exactly on 0 is fine */
    HR(ses_seek(s, -7, STREAM_SEEK_END, &np), STG_E_SEEKERROR); EXPECT_EQ(np, 0);
    IStream_Release(s);
}
TEST(ses_hg_seek_invalid_origin) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    ses_write_all(s, "abcdef", 6);
    HR(ses_seek(s, 0, 3, &np), STG_E_SEEKERROR); EXPECT_EQ(np, 6);
    HR(ses_seek(s, 0, 0xffffffff, &np), STG_E_SEEKERROR);
    HR(ses_seek(s, 0, 100, &np), STG_E_SEEKERROR);
    EXPECT_EQ(ses_pos(s), 6);
    IStream_Release(s);
}
TEST(ses_hg_seek_limit_0x7fffffff) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    HR(ses_seek(s, 0x7fffffff, STREAM_SEEK_SET, &np), S_OK); EXPECT_EQ(np, 0x7fffffff);
    HR(ses_seek(s, 0x80000000LL, STREAM_SEEK_SET, &np), STG_E_SEEKERROR); EXPECT_EQ(np, 0x7fffffff);
    /* relative seeks may carry the 32-bit pointer beyond 2 GB; nothing is allocated by seeking */
    HR(ses_seek(s, 1, STREAM_SEEK_CUR, &np), S_OK); EXPECT_EQ(np, 0x80000000ULL);
    HR(ses_seek(s, 0, STREAM_SEEK_SET, &np), S_OK); EXPECT_EQ(np, 0);
    IStream_Release(s);
}
TEST(ses_hg_seek_cur_overflow_fails) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    HR(ses_seek(s, 0x7ffffff0, STREAM_SEEK_SET, NULL), S_OK);
    HR(ses_seek(s, 0x7ffffff0, STREAM_SEEK_CUR, &np), S_OK); EXPECT_EQ(np, 0xffffffe0ULL);
    HRW(ses_seek(s, 0x7fffffff, STREAM_SEEK_CUR, &np), STG_E_SEEKERROR); /* 32-bit wrap rejected */
    CHECKW(np == 0xffffffe0ULL);
    IStream_Release(s);
}
TEST(ses_hg_seek_offset_high_dword_ignored) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULONGLONG np;
    /* decomp: only the low 32 bits of the offset are examined */
    HR(ses_seek(s, 0x100000005LL, STREAM_SEEK_SET, &np), S_OK);
    EXPECT_EQ(np, 5);
    IStream_Release(s);
}
TEST(ses_hg_write_past_end_zero_fills) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char b[64]; ULONG n;
    ses_write_all(s, "AB", 2);
    HR(ses_seek(s, 40, STREAM_SEEK_SET, NULL), S_OK);
    ses_write_all(s, "CD", 2);
    EXPECT_EQ(ses_stream_size(s), 42);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    memset(b, 0xee, sizeof b);
    HR(IStream_Read(s, b, 64, &n), S_OK); EXPECT_EQ(n, 42);
    CHECK(b[0] == 'A' && b[1] == 'B' && b[40] == 'C' && b[41] == 'D');
    CHECKW(ses_allzero(b + 2, 38));
    IStream_Release(s);
}
TEST(ses_hg_write_overflow_position) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char b[64] = {0}; ULONG n = 55;
    ses_seek(s, 0x7ffffff0, STREAM_SEEK_SET, NULL); ses_seek(s, 0x7ffffff0, STREAM_SEEK_CUR, NULL); /* pos = 0xffffffe0 */
    /* decomp: pos + cb wraps the 32-bit position -> STG_E_WRITEFAULT, nothing written, pos kept */
    if (t_is_wine()) { t_skip("wine wraps the 32-bit position instead of failing"); IStream_Release(s); return; }
    HR(IStream_Write(s, b, sizeof b, &n), STG_E_WRITEFAULT);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(ses_pos(s), 0xffffffe0ULL);
    EXPECT_EQ(ses_stream_size(s), 0);
    IStream_Release(s);
}
TEST(ses_hg_write_overwrite_middle) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    char b[16] = {0};
    ses_write_all(s, "0123456789", 10);
    ses_seek(s, 3, STREAM_SEEK_SET, NULL); ses_write_all(s, "xyz", 3);
    EXPECT_EQ(ses_stream_size(s), 10);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 10, NULL);
    EXPECT_STR(b, "012xyz6789");
    IStream_Release(s);
}
TEST(ses_hg_setsize_grow_zero_filled) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char b[300]; ULONG n;
    HR(IStream_SetSize(s, ULI(300)), S_OK);
    EXPECT_EQ(ses_stream_size(s), 300);
    EXPECT_EQ(ses_pos(s), 0); /* SetSize does not move the seek pointer */
    memset(b, 0xaa, sizeof b);
    HR(IStream_Read(s, b, sizeof b, &n), S_OK); EXPECT_EQ(n, 300); CHECKW(ses_allzero(b, 300));
    IStream_Release(s);
}
TEST(ses_hg_setsize_shrink_then_grow_zeroes) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char w[200], r[200]; ULONG n;
    memset(w, 0x5a, sizeof w); ses_write_all(s, w, sizeof w);
    HR(IStream_SetSize(s, ULI(50)), S_OK); EXPECT_EQ(ses_stream_size(s), 50);
    HR(IStream_SetSize(s, ULI(200)), S_OK); EXPECT_EQ(ses_stream_size(s), 200);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(s, r, sizeof r, &n), S_OK); EXPECT_EQ(n, 200);
    CHECK(!memcmp(r, w, 50)); CHECKW(ses_allzero(r + 50, 150)); /* regrown tail is zero, not the old data */
    IStream_Release(s);
}
TEST(ses_hg_setsize_keeps_position) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ses_write_all(s, "0123456789", 10);
    HR(IStream_SetSize(s, ULI(4)), S_OK);
    EXPECT_EQ(ses_pos(s), 10); /* pointer may now lie beyond the end */
    char c; ULONG n = 9; HR(IStream_Read(s, &c, 1, &n), S_OK); EXPECT_EQ(n, 0);
    IStream_Release(s);
}
TEST(ses_hg_setsize_zero) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ses_write_all(s, "0123456789", 10);
    HR(IStream_SetSize(s, ULI(0)), S_OK);
    EXPECT_EQ(ses_stream_size(s), 0);
    HR(IStream_SetSize(s, ULI(0)), S_OK); /* same size: no-op */
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    char c; ULONG n = 9; HR(IStream_Read(s, &c, 1, &n), S_OK); EXPECT_EQ(n, 0);
    IStream_Release(s);
}
TEST(ses_hg_setsize_high_dword_ignored) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ULARGE_INTEGER u; u.HighPart = 0xffffffff; u.LowPart = 0;
    ses_write_all(s, "abc", 3);
    HR(IStream_SetSize(s, u), S_OK); /* decomp: only the low 32 bits are used, so this is SetSize(0) */
    EXPECT_EQ(ses_stream_size(s), 0);
    u.HighPart = 7; u.LowPart = 33;
    HR(IStream_SetSize(s, u), S_OK);
    EXPECT_EQ(ses_stream_size(s), 33);
    IStream_Release(s);
}
TEST(ses_hg_stat_fields) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    ses_write_all(s, "abcdefgh", 8);
    HR(IStream_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
    EXPECT_EQ(st.type, STGTY_STREAM);
    EXPECT_EQ(st.cbSize.QuadPart, 8);
    CHECK(st.pwcsName == NULL); /* memory streams are unnamed even without STATFLAG_NONAME */
    CHECK(IsEqualGUID(&st.clsid, &GUID_NULL));
    EXPECT_EQ(st.grfMode, 0);
    EXPECT_EQ(st.grfLocksSupported, 0);
    EXPECT_EQ(st.grfStateBits, 0);
    EXPECT_EQ(st.mtime.dwLowDateTime | st.mtime.dwHighDateTime, 0);
    EXPECT_EQ(st.ctime.dwLowDateTime | st.ctime.dwHighDateTime, 0);
    EXPECT_EQ(st.atime.dwLowDateTime | st.atime.dwHighDateTime, 0);
    HR(IStream_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(st.pwcsName == NULL);
    IStream_Release(s);
}
TEST(ses_hg_stat_null) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    if (t_is_wine()) { t_skip("wine faults on NULL STATSTG"); IStream_Release(s); return; }
    HR(IStream_Stat(s, NULL, STATFLAG_DEFAULT), E_INVALIDARG);
    IStream_Release(s);
}
TEST(ses_hg_commit_revert_noop) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    ses_write_all(s, "abc", 3);
    HR(IStream_Commit(s, STGC_DEFAULT), S_OK);
    HR(IStream_Commit(s, STGC_OVERWRITE | STGC_ONLYIFCURRENT | STGC_DANGEROUSLYCOMMITMERELYTODISKCACHE | STGC_CONSOLIDATE), S_OK);
    HR(IStream_Revert(s), S_OK);
    EXPECT_EQ(ses_stream_size(s), 3); /* revert has no effect */
    IStream_Release(s);
}
/* adapted from Wine dlls/ole32/tests/hglobalstream.c (LGPL-2.1+) */
TEST(ses_hg_lock_unlock_region_invalid_function) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HR(IStream_LockRegion(s, ULI(0), ULI(10), LOCK_WRITE), STG_E_INVALIDFUNCTION);
    HR(IStream_LockRegion(s, ULI(0), ULI(10), LOCK_EXCLUSIVE), STG_E_INVALIDFUNCTION);
    HR(IStream_LockRegion(s, ULI(0), ULI(10), LOCK_ONLYONCE), STG_E_INVALIDFUNCTION);
    HR(IStream_LockRegion(s, ULI(5), ULI(0), 0), STG_E_INVALIDFUNCTION);
    HRW(IStream_UnlockRegion(s, ULI(0), ULI(10), LOCK_WRITE), STG_E_INVALIDFUNCTION);
    HRW(IStream_UnlockRegion(s, ULI(0), ULI(10), 0), STG_E_INVALIDFUNCTION);
    IStream_Release(s);
}
TEST(ses_hg_clone_shares_data_independent_pointer) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *c = NULL; if (!s) return;
    char b[16] = {0};
    ses_write_all(s, "hello world", 11);
    ses_seek(s, 6, STREAM_SEEK_SET, NULL);
    HR(IStream_Clone(s, &c), S_OK); CHECK(c != NULL && c != s);
    if (!c) { IStream_Release(s); return; }
    EXPECT_EQ(ses_pos(c), 6); /* clone starts at the same position */
    ses_seek(c, 0, STREAM_SEEK_SET, NULL);
    EXPECT_EQ(ses_pos(s), 6); /* independent seek pointers */
    HR(IStream_Read(c, b, 5, NULL), S_OK); EXPECT_STR(b, "hello");
    EXPECT_EQ(ses_pos(s), 6);
    /* writes through one are visible through the other */
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); ses_write_all(s, "J", 1);
    ses_seek(c, 0, STREAM_SEEK_SET, NULL); memset(b, 0, sizeof b); IStream_Read(c, b, 5, NULL); EXPECT_STR(b, "Jello");
    /* size changes are shared */
    HR(IStream_SetSize(s, ULI(3)), S_OK); EXPECT_EQ(ses_stream_size(c), 3);
    HR(IStream_SetSize(c, ULI(20)), S_OK); EXPECT_EQ(ses_stream_size(s), 20);
    IStream_Release(c); IStream_Release(s);
}
TEST(ses_hg_clone_outlives_original) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *c = NULL; if (!s) return;
    char b[8] = {0};
    ses_write_all(s, "data!", 5);
    HR(IStream_Clone(s, &c), S_OK); if (!c) { IStream_Release(s); return; }
    EQ(IStream_Release(s), 0);
    ses_seek(c, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_Read(c, b, 5, NULL), S_OK); EXPECT_STR(b, "data!");
    IStream_Release(c);
}
TEST(ses_hg_clone_null) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    if (t_is_wine()) { t_skip("wine faults on NULL ppstm"); IStream_Release(s); return; }
    HR(IStream_Clone(s, NULL), E_INVALIDARG);
    IStream_Release(s);
}
TEST(ses_hg_clone_of_clone_refcount) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *c = NULL, *c2 = NULL; if (!s) return;
    HR(IStream_Clone(s, &c), S_OK); if (c) HR(IStream_Clone(c, &c2), S_OK);
    if (c) { EQ(IStream_AddRef(c), 2); IStream_Release(c); }
    if (c2) EQ(IStream_Release(c2), 0);
    if (c) EQ(IStream_Release(c), 0);
    EQ(IStream_Release(s), 0);
}
TEST(ses_hg_copyto_basic_counts) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    unsigned char w[500], r[500]; ULARGE_INTEGER rd, wr; ULONG n;
    ses_fill(w, sizeof w, 9); ses_write_all(s, w, sizeof w); ses_seek(s, 100, STREAM_SEEK_SET, NULL);
    rd.QuadPart = wr.QuadPart = 0x1111;
    HR(IStream_CopyTo(s, d, ULI(250), &rd, &wr), S_OK);
    EXPECT_EQ(rd.QuadPart, 250); EXPECT_EQ(wr.QuadPart, 250);
    EXPECT_EQ(ses_pos(s), 350); EXPECT_EQ(ses_pos(d), 250); EXPECT_EQ(ses_stream_size(d), 250);
    ses_seek(d, 0, STREAM_SEEK_SET, NULL); HR(IStream_Read(d, r, 250, &n), S_OK); EXPECT_EQ(n, 250);
    CHECK(!memcmp(r, w + 100, 250));
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_clamped_to_remaining) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    ULARGE_INTEGER rd, wr;
    ses_write_all(s, "0123456789", 10); ses_seek(s, 4, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(1000), &rd, &wr), S_OK);
    EXPECT_EQ(rd.QuadPart, 6); EXPECT_EQ(wr.QuadPart, 6);
    EXPECT_EQ(ses_pos(s), 10);
    /* at EOF: nothing left to copy */
    HR(IStream_CopyTo(s, d, ULI(5), &rd, &wr), S_OK); EXPECT_EQ(rd.QuadPart, 0); EXPECT_EQ(wr.QuadPart, 0);
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_max_cb_and_high_dword) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    ULARGE_INTEGER rd, wr;
    ses_write_all(s, "0123456789", 10); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(0xffffffffffffffffULL), &rd, &wr), S_OK);
    EXPECT_EQ(rd.QuadPart, 10); EXPECT_EQ(wr.QuadPart, 10);
    ses_seek(s, 2, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(0x100000002ULL), &rd, &wr), S_OK); /* high dword set: treated as "everything" */
    EXPECT_EQ(rd.QuadPart, 8); EXPECT_EQ(wr.QuadPart, 8);
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_zero_cb) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    ULARGE_INTEGER rd, wr; rd.QuadPart = wr.QuadPart = 9;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(0), &rd, &wr), S_OK);
    EXPECT_EQ(rd.QuadPart, 0); EXPECT_EQ(wr.QuadPart, 0);
    EXPECT_EQ(ses_pos(s), 0); EXPECT_EQ(ses_stream_size(d), 0);
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_null_counters) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    ses_write_all(s, "abc", 3); ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(3), NULL, NULL), S_OK);
    EXPECT_EQ(ses_stream_size(d), 3);
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_source_beyond_end_seekerror) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *d = mk(&a, NULL, TRUE); if (!s || !d) return;
    ULARGE_INTEGER rd, wr; rd.QuadPart = wr.QuadPart = 9;
    ses_write_all(s, "abc", 3); ses_seek(s, 50, STREAM_SEEK_SET, NULL);
    /* decomp: position > size -> STG_E_SEEKERROR with both counters zeroed */
    HRW(IStream_CopyTo(s, d, ULI(5), &rd, &wr), STG_E_SEEKERROR);
    EXPECT_EQ(rd.QuadPart, 0); EXPECT_EQ(wr.QuadPart, 0);
    EXPECT_EQ(ses_pos(s), 50);
    /* position == size is not beyond the end */
    ses_seek(s, 3, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, d, ULI(5), &rd, &wr), S_OK);
    IStream_Release(s); IStream_Release(d);
}
TEST(ses_hg_copyto_self_clone) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE), *c = NULL; if (!s) return;
    ULARGE_INTEGER rd, wr; ULONG n; char b[32] = {0};
    ses_write_all(s, "abc", 3); IStream_Clone(s, &c); if (!c) return;
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); ses_seek(c, 3, STREAM_SEEK_SET, NULL);
    HR(IStream_CopyTo(s, c, ULI(3), &rd, &wr), S_OK);
    EXPECT_EQ(rd.QuadPart, 3); EXPECT_EQ(wr.QuadPart, 3);
    EXPECT_EQ(ses_stream_size(s), 6);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL); IStream_Read(s, b, 31, &n); EXPECT_EQ(n, 6); EXPECT_STR(b, "abcabc");
    IStream_Release(c); IStream_Release(s);
}
TEST(ses_hg_big_stream_16mb) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    unsigned char *w = malloc(1 << 20), *r = malloc(1 << 20);
    if (!w || !r) { t_skip("alloc"); return; }
    for (int i = 0; i < 16; i++) { ses_fill(w, 1 << 20, i); ses_write_all(s, w, 1 << 20); }
    EXPECT_EQ(ses_stream_size(s), 16u << 20);
    int bad = 0;
    for (int i = 0; i < 16; i++) {
        ULONG n = 0; ses_fill(w, 1 << 20, i);
        ses_seek(s, (LONGLONG)i << 20, STREAM_SEEK_SET, NULL);
        IStream_Read(s, r, 1 << 20, &n); if (n != (1u << 20) || memcmp(r, w, 1 << 20)) bad++;
    }
    EXPECT_EQ(bad, 0);
    free(w); free(r); IStream_Release(s);
}
/* ---- ownership of the HGLOBAL ---- */
TEST(ses_hg_user_hglobal_initial_content) {
    SES_REQUIRE(a);
    static const char txt[] = "initial content";
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizeof txt);
    if (!h) { t_skip("GlobalAlloc"); return; }
    char *p = GlobalLock(h); memcpy(p, txt, sizeof txt); GlobalUnlock(h);
    IStream *s = mk(&a, h, FALSE); CHECK(s != NULL); if (!s) { GlobalFree(h); return; }
    EXPECT_EQ(ses_stream_size(s), GlobalSize(h)); /* initial size is the block size */
    char b[64] = {0}; ULONG n = 0;
    HR(IStream_Read(s, b, sizeof b, &n), S_OK);
    EXPECT_EQ(n, GlobalSize(h)); CHECK(!memcmp(b, txt, sizeof txt));
    IStream_Release(s);
    CHECK(GlobalFree(h) == NULL); /* fDeleteOnRelease == FALSE: caller still owns it */
}
TEST(ses_hg_user_hglobal_zero_size) {
    SES_REQUIRE(a);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 0);
    if (!h) { t_skip("GlobalAlloc"); return; }
    IStream *s = mk(&a, h, FALSE); CHECK(s != NULL); if (!s) { GlobalFree(h); return; }
    EXPECT_EQ(ses_stream_size(s), 0);
    HR(IStream_Write(s, "abc", 3, NULL), S_OK);
    EXPECT_EQ(ses_stream_size(s), 3);
    HGLOBAL h2 = NULL; HR(a.hglobal_from_stm(s, &h2), S_OK); CHECK(h2 != NULL);
    char *p = h2 ? GlobalLock(h2) : NULL; CHECK(p && !memcmp(p, "abc", 3));
    if (p) GlobalUnlock(h2);
    IStream_Release(s);
    if (h2) CHECK(GlobalFree(h2) == NULL);
}
TEST(ses_hg_written_data_visible_in_hglobal) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, FALSE); if (!s) return;
    HGLOBAL h = NULL; unsigned char w[777];
    ses_fill(w, sizeof w, 77); ses_write_all(s, w, sizeof w);
    HR(a.hglobal_from_stm(s, &h), S_OK);
    CHECK(h != NULL && GlobalSize(h) >= sizeof w);
    unsigned char *p = h ? GlobalLock(h) : NULL; CHECK(p && !memcmp(p, w, sizeof w)); if (p) GlobalUnlock(h);
    IStream_Release(s);
    if (h) CHECK(GlobalFree(h) == NULL);
}
TEST(ses_hg_hglobal_from_stream_basic) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HGLOBAL h1 = NULL, h2 = NULL;
    HR(a.hglobal_from_stm(s, &h1), S_OK); CHECK(h1 != NULL);
    HR(a.hglobal_from_stm(s, &h2), S_OK); EXPECT_EQ(h1, h2);
    IStream_Release(s);
}
TEST(ses_hg_hglobal_from_stream_invalid) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    HGLOBAL h = (HGLOBAL)(ULONG_PTR)0x55;
    HR(a.hglobal_from_stm(s, NULL), E_INVALIDARG);
    HR(a.hglobal_from_stm(NULL, &h), E_INVALIDARG);
    EXPECT_EQ(h, 0x55);
    IStream_Release(s);
}
TEST(ses_hg_hglobal_from_stream_foreign_stream) {
    SES_REQUIRE(a);
    /* a docfile stream is not a memory stream */
    IStorage *stg = ses_mem(&a, MODE_CREATE, NULL); if (!stg) { t_skip("mem docfile"); return; }
    IStream *s = ses_mkstream(stg, L"S"); CHECK(s != NULL);
    HGLOBAL h = (HGLOBAL)(ULONG_PTR)0x55;
    if (s) { HR(a.hglobal_from_stm(s, &h), E_INVALIDARG); CHECKW(h == (HGLOBAL)(ULONG_PTR)0x55); IStream_Release(s); }
    IStorage_Release(stg);
}
TEST(ses_hg_hglobal_from_stream_after_growth) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, FALSE); if (!s) return;
    HGLOBAL h = NULL; unsigned char w[5000];
    ses_fill(w, sizeof w, 5);
    for (int i = 0; i < 20; i++) ses_write_all(s, w, sizeof w);
    HR(a.hglobal_from_stm(s, &h), S_OK);
    CHECK(h && GlobalSize(h) >= 100000);
    unsigned char *p = h ? GlobalLock(h) : NULL;
    CHECK(p && !memcmp(p + 95000, w, 5000));
    if (p) GlobalUnlock(h);
    IStream_Release(s);
    if (h) GlobalFree(h);
}
TEST(ses_hg_no_delete_survives_release_after_getter) {
    SES_REQUIRE(a);
    /* hGlobal==NULL && !fDeleteOnRelease: handle must be fetched with GetHGlobalFromStream to be kept */
    IStream *s = mk(&a, NULL, FALSE); if (!s) return;
    HGLOBAL h = NULL;
    ses_write_all(s, "persist", 7);
    HR(a.hglobal_from_stm(s, &h), S_OK);
    IStream_Release(s);
    CHECK(h != NULL && GlobalSize(h) >= 7);
    char *p = h ? GlobalLock(h) : NULL; CHECK(p && !memcmp(p, "persist", 7)); if (p) GlobalUnlock(h);
    if (h) CHECK(GlobalFree(h) == NULL);
}
TEST(ses_hg_user_hglobal_delete_on_release_frees) {
    SES_REQUIRE(a);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 64); if (!h) return;
    IStream *s = mk(&a, h, TRUE); CHECK(s != NULL);
    if (s) EQ(IStream_Release(s), 0);
    else GlobalFree(h);
    /* we do not touch h afterwards: ownership transferred to the stream */
}
TEST(ses_hg_clone_shares_hglobal_ownership) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, FALSE), *c = NULL; if (!s) return;
    HGLOBAL h1 = NULL, h2 = NULL;
    HR(IStream_Clone(s, &c), S_OK); if (!c) { IStream_Release(s); return; }
    ses_write_all(c, "xyz", 3);
    HR(a.hglobal_from_stm(s, &h1), S_OK);
    HR(a.hglobal_from_stm(c, &h2), S_OK);
    EXPECT_EQ(h1, h2);
    IStream_Release(c); IStream_Release(s);
    if (h1) GlobalFree(h1);
}
TEST(ses_hg_marshal_data_through_stream_copy) {
    SES_REQUIRE(a);
    /* typical use: build a blob in one stream, hand the HGLOBAL to another stream, read it back */
    IStream *a1 = mk(&a, NULL, FALSE); if (!a1) return;
    HGLOBAL h = NULL; unsigned char w[300], r[300]; ULONG n = 0;
    ses_fill(w, sizeof w, 31); ses_write_all(a1, w, sizeof w);
    HR(a.hglobal_from_stm(a1, &h), S_OK); IStream_Release(a1);
    IStream *b1 = mk(&a, h, TRUE); CHECK(b1 != NULL); if (!b1) return;
    EXPECT_EQ(ses_stream_size(b1), GlobalSize(h));
    HR(IStream_Read(b1, r, sizeof r, &n), S_OK);
    CHECK(n >= sizeof w && !memcmp(r, w, sizeof w));
    IStream_Release(b1);
}
TEST(ses_hg_many_small_writes) {
    SES_REQUIRE(a);
    IStream *s = mk(&a, NULL, TRUE); if (!s) return;
    int bad = 0;
    for (int i = 0; i < 10000; i++) { unsigned char c = (unsigned char)i; if (ses_write_all(s, &c, 1) != S_OK) bad++; }
    EXPECT_EQ(bad, 0); EXPECT_EQ(ses_stream_size(s), 10000);
    ses_seek(s, 0, STREAM_SEEK_SET, NULL);
    for (int i = 0; i < 10000; i++) { unsigned char c = 0; ULONG n = 0; IStream_Read(s, &c, 1, &n); if (n != 1 || c != (unsigned char)i) bad++; }
    EXPECT_EQ(bad, 0);
    IStream_Release(s);
}
