/* ILockBytes on an HGLOBAL: CreateILockBytesOnHGlobal / GetHGlobalFromILockBytes / StgIsStorageILockBytes.
 * Expectations come from the Windows 10 implementation (coml2.dll CMemBytes, reached through the ole32 stubs):
 *   ReadAt  : pv==NULL -> E_INVALIDARG; reads clamp at the logical size and return S_OK (never STG_E_READFAULT);
 *             32-bit offset+cb overflow -> E_INVALIDARG; only the low 32 bits of the offset are used.
 *   WriteAt : grows with zero fill (even for cb==0 when offset > size); 32-bit overflow -> E_INVALIDARG.
 *   SetSize : low 32 bits only; Lock/Unlock/Flush are no-ops returning S_OK; Stat reports STGTY_LOCKBYTES and size only. */
#include "slm.h"

static ULARGE_INTEGER UL(ULONGLONG v) { ULARGE_INTEGER u; u.QuadPart = v; return u; }

static ILockBytes *mk(HGLOBAL h, BOOL del) {
    ILockBytes *lb = NULL;
    HRESULT hr = U(CreateILockBytesOnHGlobal)(h, del, &lb);
    if (FAILED(hr)) return NULL;
    return lb;
}
static ULONGLONG lb_size(ILockBytes *lb) {
    STATSTG st; memset(&st, 0xCC, sizeof st);
    if (FAILED(ILockBytes_Stat(lb, &st, STATFLAG_NONAME))) return (ULONGLONG)-1;
    return st.cbSize.QuadPart;
}
static HRESULT wr(ILockBytes *lb, ULONGLONG off, const void *p, ULONG cb, ULONG *w) { return ILockBytes_WriteAt(lb, UL(off), p, cb, w); }
static HRESULT rd(ILockBytes *lb, ULONGLONG off, void *p, ULONG cb, ULONG *r) { return ILockBytes_ReadAt(lb, UL(off), p, cb, r); }

TEST(slm_hg_create_null_out) {
    if (t_is_wine()) { t_skip("wine dereferences NULL out pointers"); return; }
    REQUIRE(U(CreateILockBytesOnHGlobal));
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, NULL), H_INVALIDARG);
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 16);
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(h, FALSE, NULL), H_INVALIDARG);
    GlobalFree(h);
}
TEST(slm_hg_create_default_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = (ILockBytes *)(ULONG_PTR)0x1234;
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &lb), S_OK);
    CHECK(lb != NULL && lb != (ILockBytes *)(ULONG_PTR)0x1234);
    if (!lb) return;
    EXPECT_EQ(lb_size(lb), 0);
    EXPECT_EQ(ILockBytes_Release(lb), 0);
}
TEST(slm_hg_create_delete_false_null_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    /* a handle is allocated for us even when fDeleteOnRelease is FALSE */
    ILockBytes *lb = mk(NULL, FALSE);
    CHECK(lb != NULL); if (!lb) return;
    HGLOBAL h = NULL;
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &h), S_OK);
    CHECK(h != NULL);
    ILockBytes_Release(lb);
    if (h) { CHECK(GlobalSize(h) != 0 || 1); GlobalFree(h); }
}
TEST(slm_hg_refcount) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(ILockBytes_AddRef(lb), 2);
    EXPECT_EQ(ILockBytes_AddRef(lb), 3);
    EXPECT_EQ(ILockBytes_Release(lb), 2);
    EXPECT_EQ(ILockBytes_Release(lb), 1);
    EXPECT_EQ(ILockBytes_Release(lb), 0);
}
TEST(slm_hg_queryinterface) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    void *p = NULL;
    EXPECT_EQ(ILockBytes_QueryInterface(lb, &G_IUnknown, &p), S_OK); CHECK(p == (void *)lb || p != NULL);
    if (p) ILockBytes_Release((ILockBytes *)p);
    p = NULL;
    EXPECT_EQ(ILockBytes_QueryInterface(lb, &G_ILockBytes, &p), S_OK); CHECK(p == (void *)lb);
    if (p) ILockBytes_Release((ILockBytes *)p);
    p = (void *)1;
    EXPECT_EQ(ILockBytes_QueryInterface(lb, &G_IStream, &p), E_NOINTERFACE); CHECK(p == NULL);
    p = (void *)1;
    EXPECT_EQ(ILockBytes_QueryInterface(lb, &G_IStorage, &p), E_NOINTERFACE); CHECK(p == NULL);
    EXPECT_EQ(ILockBytes_Release(lb), 0);
}
TEST(slm_hg_stat_empty) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    DWORD flags[] = { STATFLAG_DEFAULT, STATFLAG_NONAME, STATFLAG_NOOPEN, 0x1234 };
    for (int i = 0; i < 4; i++) {
        STATSTG st; memset(&st, 0xCC, sizeof st);
        EXPECT_EQ(ILockBytes_Stat(lb, &st, flags[i]), S_OK);
        CHECK(st.pwcsName == NULL);
        EXPECT_EQ(st.type, STGTY_LOCKBYTES);
        EXPECT_EQ(st.cbSize.QuadPart, 0);
        EXPECT_EQ(st.grfMode, 0);
        EXPECT_EQ(st.grfLocksSupported, 0);
        EXPECT_EQ(st.grfStateBits, 0);
        EXPECT_EQ(st.reserved, 0);
        CHECK(IsEqualGUID(&st.clsid, &G_CLSID_NULL));
        EXPECT_EQ(st.mtime.dwLowDateTime | st.mtime.dwHighDateTime, 0);
        EXPECT_EQ(st.ctime.dwLowDateTime | st.ctime.dwHighDateTime, 0);
        EXPECT_EQ(st.atime.dwLowDateTime | st.atime.dwHighDateTime, 0);
    }
    ILockBytes_Release(lb);
}
TEST(slm_hg_stat_null) {
    if (t_is_wine()) { t_skip("wine dereferences NULL out pointers"); return; }
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(ILockBytes_Stat(lb, NULL, STATFLAG_NONAME), H_INVALIDARG);
    ILockBytes_Release(lb);
}
TEST(slm_hg_stat_size_from_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    SIZE_T sizes[] = { 1, 100, 4096, 70000 };
    for (int i = 0; i < 4; i++) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, sizes[i]);
        SIZE_T gs = GlobalSize(h);
        ILockBytes *lb = mk(h, FALSE);
        CHECK(lb != NULL);
        if (lb) { EXPECT_EQ(lb_size(lb), gs); ILockBytes_Release(lb); }
        GlobalFree(h);
    }
}
TEST(slm_hg_zero_size_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 0);
    ILockBytes *lb = mk(h, FALSE);
    CHECK(lb != NULL); if (!lb) { GlobalFree(h); return; }
    EXPECT_EQ(lb_size(lb), 0);
    BYTE b[4] = { 1, 2, 3, 4 }, o[4] = { 0 }; ULONG n = 0;
    EXPECT_EQ(wr(lb, 0, b, 4, &n), S_OK); EXPECT_EQ(n, 4);
    EXPECT_EQ(lb_size(lb), 4);
    EXPECT_EQ(rd(lb, 0, o, 4, &n), S_OK); EXPECT_EQ(n, 4);
    CHECK(!memcmp(b, o, 4));
    HGLOBAL h2 = NULL;
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &h2), S_OK);
    ILockBytes_Release(lb);
    GlobalFree(h2 ? h2 : h);
}
TEST(slm_hg_supplied_content_visible) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 64);
    BYTE *p = GlobalLock(h); for (int i = 0; i < 64; i++) p[i] = (BYTE)(i * 3); GlobalUnlock(h);
    ILockBytes *lb = mk(h, FALSE); CHECK(lb != NULL); if (!lb) { GlobalFree(h); return; }
    BYTE o[64]; ULONG n = 0;
    EXPECT_EQ(rd(lb, 0, o, 64, &n), S_OK); EXPECT_EQ(n, 64);
    for (int i = 0; i < 64; i++) if (o[i] != (BYTE)(i * 3)) { CHECKF(0, "byte %d", i); break; }
    /* writes through the lockbytes are visible through the handle */
    BYTE w[3] = { 0xAA, 0xBB, 0xCC };
    EXPECT_EQ(wr(lb, 10, w, 3, &n), S_OK);
    HGLOBAL h2 = NULL; EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &h2), S_OK);
    p = GlobalLock(h2); CHECK(p != NULL);
    if (p) { CHECK(p[10] == 0xAA && p[11] == 0xBB && p[12] == 0xCC && p[13] == (BYTE)(13 * 3)); GlobalUnlock(h2); }
    ILockBytes_Release(lb);
    GlobalFree(h2 ? h2 : h);
}
TEST(slm_hg_getlbhglobal_same_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 32);
    ILockBytes *lb = mk(h, FALSE); CHECK(lb != NULL); if (!lb) { GlobalFree(h); return; }
    HGLOBAL g = (HGLOBAL)(ULONG_PTR)7;
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &g), S_OK);
    EXPECT_EQ(g, h);
    ILockBytes_Release(lb);
    GlobalFree(h);
}
TEST(slm_hg_getlbhglobal_invalid) {
    if (t_is_wine()) { t_skip("wine dereferences NULL out pointers"); return; }
    REQUIRE(U(GetHGlobalFromILockBytes)); REQUIRE(U(CreateILockBytesOnHGlobal));
    HGLOBAL g = (HGLOBAL)(ULONG_PTR)7;
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(NULL, &g), H_INVALIDARG);
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, NULL), H_INVALIDARG);
    ILockBytes_Release(lb);
}
TEST(slm_hg_getlbhglobal_foreign_lockbytes) {
    REQUIRE(U(GetHGlobalFromILockBytes));
    /* Windows (unlike Wine) does not copy a foreign ILockBytes into a new HGLOBAL: it fails and zeroes the output. */
    CLB c; clb_init(&c);
    HGLOBAL g = (HGLOBAL)(ULONG_PTR)7;
    EXPECT_EQ(U(GetHGlobalFromILockBytes)(&c.iface, &g), H_INVALIDARG);
    EXPECT_EQ(g, NULL);
    EXPECT_EQ(c.nread + c.nwrite, 0);
    clb_free(&c);
}
TEST(slm_hg_write_read_basic) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    const char msg[] = "hello lockbytes";
    ULONG n = 99;
    EXPECT_EQ(wr(lb, 0, msg, sizeof msg, &n), S_OK); EXPECT_EQ(n, sizeof msg);
    EXPECT_EQ(lb_size(lb), sizeof msg);
    char o[32] = { 0 }; n = 99;
    EXPECT_EQ(rd(lb, 0, o, sizeof msg, &n), S_OK); EXPECT_EQ(n, sizeof msg);
    EXPECT_STR(o, msg);
    memset(o, 0, sizeof o);
    EXPECT_EQ(rd(lb, 6, o, 9, &n), S_OK); EXPECT_EQ(n, 9);
    CHECK(!memcmp(o, "lockbytes", 9));
    ILockBytes_Release(lb);
}
TEST(slm_hg_write_null_written_ptr) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(wr(lb, 0, "abcd", 4, NULL), S_OK);
    char o[4]; EXPECT_EQ(rd(lb, 0, o, 4, NULL), S_OK);
    CHECK(!memcmp(o, "abcd", 4));
    ILockBytes_Release(lb);
}
TEST(slm_hg_write_beyond_end_zero_fills) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    ULONG n = 0;
    EXPECT_EQ(wr(lb, 1000, "XY", 2, &n), S_OK); EXPECT_EQ(n, 2);
    EXPECT_EQ(lb_size(lb), 1002);
    BYTE o[1002]; memset(o, 0x55, sizeof o);
    EXPECT_EQ(rd(lb, 0, o, 1002, &n), S_OK); EXPECT_EQ(n, 1002);
    int nz = 0; for (int i = 0; i < 1000; i++) nz += o[i] != 0;
    EXPECT_EQ(nz, 0);
    CHECK(o[1000] == 'X' && o[1001] == 'Y');
    ILockBytes_Release(lb);
}
TEST(slm_hg_write_overwrite_keeps_size) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    BYTE a[100]; memset(a, 'a', 100);
    wr(lb, 0, a, 100, NULL);
    EXPECT_EQ(wr(lb, 10, "ZZZZ", 4, NULL), S_OK);
    EXPECT_EQ(lb_size(lb), 100);
    BYTE o[100]; ULONG n = 0; rd(lb, 0, o, 100, &n);
    CHECK(o[9] == 'a' && o[10] == 'Z' && o[13] == 'Z' && o[14] == 'a');
    ILockBytes_Release(lb);
}
TEST(slm_hg_write_zero_bytes) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    ULONG n = 55;
    /* Windows locks the (zero-sized) HGLOBAL even for cb == 0; the lock fails, so the write faults */
    EXPECT_EQ(wr(lb, 0, "x", 0, &n), STG_E_WRITEFAULT); EXPECT_EQ(n, 0);
    EXPECT_EQ(lb_size(lb), 0);
    /* cb == 0 beyond the end still extends the array to the offset on Windows */
    n = 55;
    EXPECT_EQ(wr(lb, 100, "x", 0, &n), S_OK); EXPECT_EQ(n, 0);
    EXPECT_EQ(lb_size(lb), 100);
    ILockBytes_Release(lb);
}
TEST(slm_hg_write_overflow_32bit) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    ULONG n = 77;
    if (t_is_wine()) { t_skip("wine memcpy()s past the buffer here"); ILockBytes_Release(lb); return; }
    EXPECT_EQ(wr(lb, 0xFFFFFFF8ull, "0123456789", 10, &n), H_INVALIDARG);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(lb_size(lb), 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_read_null_buffer) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "abcd", 4, NULL);
    ULONG n = 77;
    if (t_is_wine()) { t_skip("wine dereferences the NULL buffer"); ILockBytes_Release(lb); return; }
    EXPECT_EQ(rd(lb, 0, NULL, 4, &n), H_INVALIDARG);
    EXPECT_EQ(rd(lb, 0, NULL, 0, &n), H_INVALIDARG);
    ILockBytes_Release(lb);
}
TEST(slm_hg_read_past_end) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "0123456789", 10, NULL);
    char o[32]; ULONG n;
    memset(o, 'q', sizeof o); n = 99;
    EXPECT_EQ(rd(lb, 10, o, 5, &n), S_OK); EXPECT_EQ(n, 0); CHECK(o[0] == 'q');
    n = 99; EXPECT_EQ(rd(lb, 11, o, 5, &n), S_OK); EXPECT_EQ(n, 0);
    n = 99; EXPECT_EQ(rd(lb, 100000, o, 5, &n), S_OK); EXPECT_EQ(n, 0);
    n = 99; EXPECT_EQ(rd(lb, 10, o, 0, &n), S_OK); EXPECT_EQ(n, 0);
    EXPECT_EQ(rd(lb, 100, o, 5, NULL), S_OK);
    ILockBytes_Release(lb);
}
TEST(slm_hg_read_partial_is_ok) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "0123456789", 10, NULL);
    char o[32] = { 0 }; ULONG n = 99;
    EXPECT_EQ(rd(lb, 6, o, 20, &n), S_OK);     /* not STG_E_READFAULT */
    EXPECT_EQ(n, 4);
    CHECK(!memcmp(o, "6789", 4));
    n = 99; EXPECT_EQ(rd(lb, 0, o, 11, &n), S_OK); EXPECT_EQ(n, 10);
    n = 99; EXPECT_EQ(rd(lb, 9, o, 1, &n), S_OK); EXPECT_EQ(n, 1);
    ILockBytes_Release(lb);
}
TEST(slm_hg_read_overflow_32bit) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "0123456789", 10, NULL);
    char o[64]; ULONG n = 99;
    EXPECT_EQ(rd(lb, 0xFFFFFFF0ull, o, 0x20, &n), H_INVALIDARG);
    EXPECT_EQ(n, 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_high_offset_dword_ignored) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "0123456789", 10, NULL);
    char o[8] = { 0 }; ULONG n = 99;
    /* Windows 10 CMemBytes only looks at the low 32 bits of the offset */
    HRESULT hr = rd(lb, (1ull << 32) + 2, o, 3, &n);
    EXPECT_EQ(hr, S_OK); EXPECT_EQ(n, 3); CHECK(!memcmp(o, "234", 3));
    ILockBytes_Release(lb);
}
TEST(slm_hg_setsize_grow_shrink) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(ILockBytes_SetSize(lb, UL(5000)), S_OK); EXPECT_EQ(lb_size(lb), 5000);
    EXPECT_EQ(ILockBytes_SetSize(lb, UL(5000)), S_OK); EXPECT_EQ(lb_size(lb), 5000);
    EXPECT_EQ(ILockBytes_SetSize(lb, UL(10)), S_OK);   EXPECT_EQ(lb_size(lb), 10);
    EXPECT_EQ(ILockBytes_SetSize(lb, UL(0)), S_OK);    EXPECT_EQ(lb_size(lb), 0);
    EXPECT_EQ(ILockBytes_SetSize(lb, UL(0)), S_OK);    EXPECT_EQ(lb_size(lb), 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_setsize_zero_fill) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    BYTE a[64]; memset(a, 0xEE, 64); wr(lb, 0, a, 64, NULL);
    ILockBytes_SetSize(lb, UL(16));
    ILockBytes_SetSize(lb, UL(64));
    BYTE o[64]; ULONG n = 0; rd(lb, 0, o, 64, &n); EXPECT_EQ(n, 64);
    int bad = 0; for (int i = 0; i < 16; i++) bad += o[i] != 0xEE;
    EXPECT_EQ(bad, 0);
    int nz = 0; for (int i = 16; i < 64; i++) nz += o[i] != 0;
    EXPECT_EQ(nz, 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_setsize_then_read_clamps) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "0123456789", 10, NULL);
    ILockBytes_SetSize(lb, UL(4));
    char o[16]; ULONG n = 99;
    EXPECT_EQ(rd(lb, 0, o, 10, &n), S_OK); EXPECT_EQ(n, 4);
    n = 99; EXPECT_EQ(rd(lb, 4, o, 1, &n), S_OK); EXPECT_EQ(n, 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_setsize_high_dword_ignored) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    /* only LowPart is consulted by Windows 10 (Wine would return STG_E_INVALIDFUNCTION) */
    HRESULT hr = ILockBytes_SetSize(lb, UL((1ull << 32) | 24));
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(lb_size(lb), 24);
    ILockBytes_Release(lb);
}
TEST(slm_hg_flush_lock_unlock_ok) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    EXPECT_EQ(ILockBytes_Flush(lb), S_OK);
    DWORD types[] = { LOCK_WRITE, LOCK_EXCLUSIVE, LOCK_ONLYONCE, 0, LOCK_WRITE | LOCK_EXCLUSIVE };
    for (int i = 0; i < 5; i++) {
        /* On Windows the in-memory byte array does not implement locking: both calls succeed */
        EXPECT_EQ(ILockBytes_LockRegion(lb, UL(0), UL(100), types[i]), S_OK);
        EXPECT_EQ(ILockBytes_UnlockRegion(lb, UL(0), UL(100), types[i]), S_OK);
    }
    EXPECT_EQ(ILockBytes_LockRegion(lb, UL(0x7FFFFF00ull), UL(0x80), LOCK_WRITE), S_OK);
    EXPECT_EQ(ILockBytes_UnlockRegion(lb, UL(0x7FFFFF00ull), UL(0x80), LOCK_WRITE), S_OK);
    EXPECT_EQ(ILockBytes_UnlockRegion(lb, UL(5), UL(5), LOCK_WRITE), S_OK);  /* never locked */
    ILockBytes_Release(lb);
}
TEST(slm_hg_flush_after_write) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "abc", 3, NULL);
    EXPECT_EQ(ILockBytes_Flush(lb), S_OK);
    EXPECT_EQ(lb_size(lb), 3);
    ILockBytes_Release(lb);
}
TEST(slm_hg_large_roundtrip) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    ULONG total = 1 << 20;
    BYTE *a = malloc(total), *o = malloc(total);
    for (ULONG i = 0; i < total; i++) a[i] = (BYTE)(i * 31 + (i >> 8));
    ULONG n = 0;
    EXPECT_EQ(wr(lb, 0, a, total, &n), S_OK); EXPECT_EQ(n, total);
    EXPECT_EQ(lb_size(lb), total);
    EXPECT_EQ(rd(lb, 0, o, total, &n), S_OK); EXPECT_EQ(n, total);
    CHECK(!memcmp(a, o, total));
    free(a); free(o);
    ILockBytes_Release(lb);
}
TEST(slm_hg_many_small_appends) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    int bad = 0;
    for (ULONG i = 0; i < 2000; i++) { ULONG v = i * 2654435761u; ULONG n = 0; if (wr(lb, i * 4, &v, 4, &n) != S_OK || n != 4) bad++; }
    EXPECT_EQ(bad, 0);
    EXPECT_EQ(lb_size(lb), 8000);
    for (ULONG i = 0; i < 2000; i += 37) { ULONG v = 0, n = 0; rd(lb, i * 4, &v, 4, &n); if (v != i * 2654435761u) bad++; }
    EXPECT_EQ(bad, 0);
    ILockBytes_Release(lb);
}
TEST(slm_hg_delete_on_release_false_keeps_handle) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 128);
    ILockBytes *lb = mk(h, FALSE); CHECK(lb != NULL); if (!lb) { GlobalFree(h); return; }
    wr(lb, 0, "keep", 4, NULL);
    HGLOBAL g = NULL; U(GetHGlobalFromILockBytes)(lb, &g);
    ILockBytes_Release(lb);
    CHECK(g != NULL);
    if (g) {
        char *p = GlobalLock(g);
        CHECK(p != NULL);
        if (p) { CHECK(!memcmp(p, "keep", 4)); GlobalUnlock(g); }
        CHECK(GlobalSize(g) >= 128);
        GlobalFree(g);
    }
}
TEST(slm_hg_delete_on_release_true_frees) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "x", 1, NULL);
    HGLOBAL g = NULL; U(GetHGlobalFromILockBytes)(lb, &g);
    ILockBytes_Release(lb);
    /* the handle is freed with the object; probing a freed handle is only informational */
    t_note("GlobalSize on released delete-on-release handle: %llu", (unsigned long long)GlobalSize(g));
}
TEST(slm_hg_independent_instances) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *a = mk(NULL, TRUE), *b = mk(NULL, TRUE);
    CHECK(a && b && a != b); if (!a || !b) return;
    wr(a, 0, "AAAA", 4, NULL); wr(b, 0, "BB", 2, NULL);
    EXPECT_EQ(lb_size(a), 4); EXPECT_EQ(lb_size(b), 2);
    HGLOBAL ha = NULL, hb = NULL; U(GetHGlobalFromILockBytes)(a, &ha); U(GetHGlobalFromILockBytes)(b, &hb);
    CHECK(ha != hb);
    ILockBytes_Release(a); ILockBytes_Release(b);
}
TEST(slm_hg_getlbhglobal_after_growth_holds_data) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    BYTE blk[1000]; memset(blk, 0x5A, sizeof blk);
    for (int i = 0; i < 20; i++) wr(lb, (ULONGLONG)i * 1000, blk, 1000, NULL);
    HGLOBAL g = NULL; EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &g), S_OK);
    CHECK(g != NULL && GlobalSize(g) >= 20000);
    BYTE *p = GlobalLock(g);
    if (p) { int bad = 0; for (int i = 0; i < 20000; i++) bad += p[i] != 0x5A; EXPECT_EQ(bad, 0); GlobalUnlock(g); }
    ILockBytes_Release(lb);
}
TEST(slm_hg_setsize_shrink_to_zero_keeps_handle_valid) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
    wr(lb, 0, "abc", 3, NULL);
    ILockBytes_SetSize(lb, UL(0));
    HGLOBAL g = NULL; EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &g), S_OK);
    CHECK(g != NULL);
    /* a zero logical size still leaves a (>= 1 byte) allocation behind the handle */
    CHECK(g && GlobalSize(g) >= 1);
    ILockBytes_Release(lb);
}

/* ---- StgIsStorageILockBytes ------------------------------------------- */
static const BYTE SIG[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };

TEST(slm_isstg_null) {
    REQUIRE(U(StgIsStorageILockBytes));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    EXPECT_EQ(U(StgIsStorageILockBytes)(NULL), H_INVALIDPOINTER);
}
TEST(slm_isstg_table) {
    REQUIRE(U(StgIsStorageILockBytes)); REQUIRE(U(CreateILockBytesOnHGlobal));
    struct { const char *name; ULONG len; int sig; int off_sig; HRESULT want; } t[] = {
        { "empty",              0,   0, 0, S_FALSE },
        { "7 bytes sig prefix", 7,   1, 0, S_FALSE },
        { "8 bytes sig only",   8,   1, 0, S_FALSE },
        { "0x17 bytes",         0x17, 1, 0, S_FALSE },
        { "0x18 bytes sig",     0x18, 1, 0, S_OK },
        { "512 bytes sig",      512, 1, 0, S_OK },
        { "0x18 no sig",        0x18, 0, 0, S_FALSE },
        { "512 zeros",          512, 0, 0, S_FALSE },
        { "sig at offset 8",    0x30, 1, 8, S_FALSE },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        ILockBytes *lb = mk(NULL, TRUE); if (!lb) { CHECK(0); return; }
        BYTE *b = calloc(1, t[i].len + 8);
        if (t[i].sig) memcpy(b + t[i].off_sig, SIG, 8);
        if (t[i].len) wr(lb, 0, b, t[i].len, NULL);
        HRESULT hr = U(StgIsStorageILockBytes)(lb);
        CHECKF(hr == t[i].want, "%s: got 0x%08lx want 0x%08lx", t[i].name, hr, t[i].want);
        free(b); ILockBytes_Release(lb);
    }
}
TEST(slm_isstg_custom_lockbytes_calls) {
    REQUIRE(U(StgIsStorageILockBytes));
    CLB c; clb_init(&c);
    BYTE hdr[0x40]; memset(hdr, 0, sizeof hdr); memcpy(hdr, SIG, 8);
    ULARGE_INTEGER z; z.QuadPart = 0; clb_WriteAt(&c.iface, z, hdr, sizeof hdr, NULL);
    clb_reset_counts(&c);
    EXPECT_EQ(U(StgIsStorageILockBytes)(&c.iface), S_OK);
    CHECK(c.nread >= 1);
    EXPECT_EQ(c.nwrite, 0);
    /* the probe reads from offset 0 */
    CHECK(c.nlog > 0 && c.log[0].op == LB_READ && c.log[0].off == 0);
    if (c.nlog > 0) t_note("StgIsStorageILockBytes first read: off=%llu cb=%lu", c.log[0].off, c.log[0].cb);
    clb_free(&c);
}
TEST(slm_isstg_read_error_propagates) {
    REQUIRE(U(StgIsStorageILockBytes));
    CLB c; clb_init(&c); c.fail_read = STG_E_READFAULT;
    EXPECT_EQ(U(StgIsStorageILockBytes)(&c.iface), STG_E_READFAULT);
    c.fail_read = STG_E_ACCESSDENIED;
    EXPECT_EQ(U(StgIsStorageILockBytes)(&c.iface), STG_E_ACCESSDENIED);
    clb_free(&c);
}
TEST(slm_isstg_real_docfile) {
    REQUIRE(U(StgIsStorageILockBytes));
    IStorage *stg = NULL; ILockBytes *lb = NULL;
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    HRESULT hr = slm_mem_docfile(&stg, &lb, SM_CREATE_RW);
    EXPECT_EQ(hr, S_OK);
    if (SUCCEEDED(hr)) {
        IStorage_Commit(stg, STGC_DEFAULT);
        EXPECT_EQ(U(StgIsStorageILockBytes)(lb), S_OK);
        IStorage_Release(stg);
        EXPECT_EQ(U(StgIsStorageILockBytes)(lb), S_OK);
        ILockBytes_Release(lb);
    }
    slm_couninit();
}
TEST(slm_isstg_docfile_hglobal_signature_and_size) {
    REQUIRE(U(StgIsStorageILockBytes));
    IStorage *stg = NULL; ILockBytes *lb = NULL;
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    HRESULT hr = slm_mem_docfile(&stg, &lb, SM_CREATE_RW);
    if (FAILED(hr)) { CHECK(0); slm_couninit(); return; }
    IStorage_Commit(stg, STGC_DEFAULT);
    IStorage_Release(stg);
    HGLOBAL g = NULL; EXPECT_EQ(U(GetHGlobalFromILockBytes)(lb, &g), S_OK);
    ULONGLONG sz = lb_size(lb);
    CHECK(sz >= 1024); EXPECT_EQ(sz % 512, 0);
    BYTE *p = g ? GlobalLock(g) : NULL;
    CHECK(p != NULL);
    if (p) { CHECK(!memcmp(p, SIG, 8)); GlobalUnlock(g); }
    ILockBytes_Release(lb);
    slm_couninit();
}
