/* ILockBytes on HGLOBAL, StgCreateDocfileOnILockBytes, StgOpenStorageOnILockBytes, StgIsStorageILockBytes. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define RW    STGM_READWRITE

static HRESULT mk_ilb(ILockBytes **p) { *p = NULL; return U(CreateILockBytesOnHGlobal)(NULL, TRUE, p); }

TEST(sco_ilb_create_basic) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *ilb = NULL;
    EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &ilb), S_OK);
    CHECK(ilb != NULL);
    if (!ilb) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK);
    EQ(st.type, STGTY_LOCKBYTES);
    EQ(st.cbSize.QuadPart, 0);
    CHECK(st.pwcsName == NULL);
    EQ(ILockBytes_Release(ilb), 0);
}

TEST(sco_ilb_create_null_out) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    HRESULT hr = U(CreateILockBytesOnHGlobal)(NULL, TRUE, NULL);
    CHECK(FAILED(hr));
    t_note("CreateILockBytesOnHGlobal(NULL out) -> 0x%08lx", hr);
}

TEST(sco_ilb_readwrite_size) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    ULARGE_INTEGER off; ULONG n = 99; STATSTG st;
    BYTE w[100], r[100];
    for (int i = 0; i < 100; i++) w[i] = (BYTE)i;
    off.QuadPart = 0;
    EQ(ILockBytes_WriteAt(ilb, off, w, 100, &n), S_OK); EQ(n, 100);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK); EQ(st.cbSize.QuadPart, 100);
    memset(r, 0, sizeof r); n = 0;
    EQ(ILockBytes_ReadAt(ilb, off, r, 100, &n), S_OK); EQ(n, 100);
    CHECK(!memcmp(r, w, 100));
    /* partial read at end */
    off.QuadPart = 95; n = 0;
    n = 0; { HRESULT hp = ILockBytes_ReadAt(ilb, off, r, 10, &n);
      t_note("ReadAt straddling end (5 bytes available, 10 requested) -> 0x%08lx read=%lu", hp, n); }
    /* read past end */
    off.QuadPart = 500; n = 77;
    HRESULT hr = ILockBytes_ReadAt(ilb, off, r, 10, &n);
    t_note("ReadAt beyond end -> 0x%08lx, read=%lu", hr, n);
    CHECK(hr == S_OK || FAILED(hr));
    /* write past end grows; gap is zero-filled */
    off.QuadPart = 2000; n = 0;
    EQ(ILockBytes_WriteAt(ilb, off, w, 10, &n), S_OK); EQ(n, 10);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK); EQ(st.cbSize.QuadPart, 2010);
    off.QuadPart = 1000; memset(r, 0xee, sizeof r);
    n = 0; { HRESULT hg = ILockBytes_ReadAt(ilb, off, r, 100, &n); CHECK(hg == S_OK && n == 100); }
    int zero = 1; for (int i = 0; i < 100; i++) if (r[i]) zero = 0;
    t_note("bytes in the gap created by write past end are %s", zero ? "zero" : "not zero");
    /* SetSize shrinks / grows */
    off.QuadPart = 50;
    EQ(ILockBytes_SetSize(ilb, off), S_OK);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK); EQ(st.cbSize.QuadPart, 50);
    off.QuadPart = 4096;
    EQ(ILockBytes_SetSize(ilb, off), S_OK);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK); EQ(st.cbSize.QuadPart, 4096);
    EQ(ILockBytes_Flush(ilb), S_OK);
    EQ(ILockBytes_Release(ilb), 0);
}

TEST(sco_ilb_lockregion) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    ULARGE_INTEGER o, c; o.QuadPart = 0; c.QuadPart = 10;
    HRESULT hr = ILockBytes_LockRegion(ilb, o, c, LOCK_WRITE);
    t_note("HGLOBAL ILockBytes::LockRegion -> 0x%08lx", hr);
    hr = ILockBytes_UnlockRegion(ilb, o, c, LOCK_WRITE);
    t_note("HGLOBAL ILockBytes::UnlockRegion -> 0x%08lx", hr);
    ILockBytes_Release(ilb);
}

TEST(sco_ilb_gethglobal_user_supplied) {
    REQUIRE(U(CreateILockBytesOnHGlobal)); REQUIRE(U(GetHGlobalFromILockBytes));
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, 1000);
    CHECK(h != NULL);
    ILockBytes *ilb = NULL;
    EQ(U(CreateILockBytesOnHGlobal)(h, FALSE, &ilb), S_OK);
    HGLOBAL h2 = NULL;
    EQ(U(GetHGlobalFromILockBytes)(ilb, &h2), S_OK);
    CHECK(h2 == h);
    STATSTG st; EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK);
    EQ(st.cbSize.QuadPart, GlobalSize(h));
    ULARGE_INTEGER o; o.QuadPart = 0; ULONG n = 0;
    EQ(ILockBytes_WriteAt(ilb, o, "hello", 5, &n), S_OK);
    char *p = (char *)GlobalLock(h);
    CHECK(p && !memcmp(p, "hello", 5));
    GlobalUnlock(h);
    EQ(ILockBytes_Release(ilb), 0);
    /* fDeleteOnRelease was FALSE: the handle is still ours */
    CHECK(GlobalSize(h) != 0);
    GlobalFree(h);
}

TEST(sco_ilb_gethglobal_auto) {
    REQUIRE(U(GetHGlobalFromILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    HGLOBAL h = NULL;
    EQ(U(GetHGlobalFromILockBytes)(ilb, &h), S_OK);
    t_note("auto-allocated HGLOBAL = %p", (void *)h);
    ULARGE_INTEGER o; o.QuadPart = 0; ULONG n;
    EQ(ILockBytes_WriteAt(ilb, o, "abcdef", 6, &n), S_OK);
    HGLOBAL h2 = NULL;
    EQ(U(GetHGlobalFromILockBytes)(ilb, &h2), S_OK);
    CHECK(h2 != NULL);
    CHECK(GlobalSize(h2) >= 6);
    char *p = (char *)GlobalLock(h2);
    CHECK(p && !memcmp(p, "abcdef", 6));
    GlobalUnlock(h2);
    ILockBytes_Release(ilb);
}

TEST(sco_ilb_gethglobal_invalid_args) {
    REQUIRE(U(GetHGlobalFromILockBytes));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    HGLOBAL h = (HGLOBAL)1;
    HRESULT hr = U(GetHGlobalFromILockBytes)(NULL, &h);
    CHECK(FAILED(hr));
    t_note("GetHGlobalFromILockBytes(NULL ilb) -> 0x%08lx", hr);
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    hr = U(GetHGlobalFromILockBytes)(ilb, NULL);
    CHECK(FAILED(hr));
    t_note("GetHGlobalFromILockBytes(NULL out) -> 0x%08lx", hr);
    ILockBytes_Release(ilb);
}

TEST(sco_ilb_gethglobal_foreign_lockbytes) {
    REQUIRE(U(GetHGlobalFromILockBytes)); REQUIRE(U(StgCreateDocfile));
    /* a file-backed storage does not expose an ILockBytes of its own; use QI as a sanity check */
    WCHAR p[MAX_PATH]; sco_wpath(p, "ilb_foreign.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    ILockBytes *lb = (ILockBytes *)1;
    HRESULT hr = IStorage_QueryInterface(s, &sco_IID_ILockBytes, (void **)&lb);
    EQ(hr, E_NOINTERFACE);
    CHECK(lb == NULL);
    sco_rel(s); sco_rm(p);
}

/* ---- StgIsStorageILockBytes ------------------------------------------------------------------ */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_hglobal_storage_stat */
TEST(sco_isstorageilockbytes_empty_is_false) {
    REQUIRE(U(StgIsStorageILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    EQ(U(StgIsStorageILockBytes)(ilb), S_FALSE);
    ULARGE_INTEGER o; o.QuadPart = 0; BYTE junk[512]; memset(junk, 0xaa, sizeof junk); ULONG n;
    EQ(ILockBytes_WriteAt(ilb, o, junk, 512, &n), S_OK);
    EQ(U(StgIsStorageILockBytes)(ilb), S_FALSE);
    EQ(ILockBytes_Release(ilb), 0);
}

TEST(sco_isstorageilockbytes_after_create) {
    REQUIRE(U(StgIsStorageILockBytes)); REQUIRE(U(StgCreateDocfileOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    HRESULT hr = U(StgIsStorageILockBytes)(ilb);
    t_note("StgIsStorageILockBytes immediately after create (nothing flushed) -> 0x%08lx", hr);
    EQ(U(WriteClassStg)(s, &sco_CLS_A) , S_OK);
    EQ(U(StgIsStorageILockBytes)(ilb), S_OK);
    EQ(IStorage_Release(s), 0);
    EQ(U(StgIsStorageILockBytes)(ilb), S_OK);
    ILockBytes_Release(ilb);
}

TEST(sco_isstorageilockbytes_null) {
    REQUIRE(U(StgIsStorageILockBytes));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    HRESULT hr = U(StgIsStorageILockBytes)(NULL);
    CHECK(FAILED(hr));
    t_note("StgIsStorageILockBytes(NULL) -> 0x%08lx", hr);
}

TEST(sco_isstorageilockbytes_short_lockbytes) {
    REQUIRE(U(StgIsStorageILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    ULARGE_INTEGER o; o.QuadPart = 0; ULONG n;
    EQ(ILockBytes_WriteAt(ilb, o, sco_sig, 8, &n), S_OK);
    HRESULT hr = U(StgIsStorageILockBytes)(ilb);
    t_note("StgIsStorageILockBytes(8-byte signature only) -> 0x%08lx", hr);
    EQ(ILockBytes_SetSize(ilb, o), S_OK);
    hr = U(StgIsStorageILockBytes)(ilb);
    CHECK(hr == S_FALSE || FAILED(hr));
    t_note("StgIsStorageILockBytes(size 0) -> 0x%08lx", hr);
    ILockBytes_Release(ilb);
}

/* ---- StgCreateDocfileOnILockBytes ------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_hglobal_storage_stat */
TEST(sco_createdocfile_onilb_stat) {
    REQUIRE(U(StgCreateDocfileOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    EQ(U(WriteClassStg)(s, &sco_CLS_A), S_OK);
    STATSTG st; memset(&st, 0, sizeof st);
    EQ(IStorage_Stat(s, &st, 0), S_OK);
    CHECK(st.pwcsName == NULL);
    EQ(st.type, STGTY_STORAGE);
    EQ(st.grfMode, 0x12);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLS_A));
    EQ(IStorage_Release(s), 0);
    EQ(ILockBytes_Release(ilb), 0);
}

TEST(sco_createdocfile_onilb_invalid_args) {
    REQUIRE(U(StgCreateDocfileOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, NULL), STG_E_INVALIDPOINTER);
    EQ(U(StgCreateDocfileOnILockBytes)(NULL, STGM_CREATE | EXCL | RW, 0, &s), STG_E_INVALIDPOINTER);
    CHECK(s == NULL);
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, 0, 0, &s), STG_E_INVALIDFLAG);
    HRESULT hr0 = U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | STGM_READ, 0, &s);
    t_note("create on ILockBytes with CREATE|EXCL|READ -> 0x%08lx", hr0); sco_rel(s); s = NULL;
    hr0 = U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 1, &s);
    t_note("create on ILockBytes with reserved=1 -> 0x%08lx", hr0); sco_rel(s); s = NULL;
    HRESULT hr = U(StgCreateDocfileOnILockBytes)(ilb, EXCL | RW, 0, &s);
    t_note("create on ILockBytes without CREATE flag (empty) -> 0x%08lx", hr);
    sco_rel(s);
    ILockBytes_Release(ilb);
}

TEST(sco_createdocfile_onilb_overwrites_junk) {
    REQUIRE(U(StgCreateDocfileOnILockBytes)); REQUIRE(U(StgOpenStorageOnILockBytes));
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_hglobal_storage_creation */
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    char junk[512]; memset(junk, 0xaa, sizeof junk);
    ULARGE_INTEGER o; ULONG n;
    o.QuadPart = 0;    EQ(ILockBytes_WriteAt(ilb, o, junk, 512, &n), S_OK);
    o.QuadPart = 2000; EQ(ILockBytes_WriteAt(ilb, o, junk, 512, &n), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    EQ(IStorage_Release(s), 0);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &s), S_OK);
    STATSTG st; memset(&st, 0xcc, sizeof st);
    EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&st.clsid, &sco_CLSID_NULL));
    EQ(IStorage_Release(s), 0);
    EQ(ILockBytes_Stat(ilb, &st, STATFLAG_NONAME), S_OK);
    CHECKF(st.cbSize.QuadPart < 2512, "lockbytes size %llu not truncated", (unsigned long long)st.cbSize.QuadPart);
    EQ(ILockBytes_Release(ilb), 0);
}

TEST(sco_createdocfile_onilb_header_in_memory) {
    REQUIRE(U(StgCreateDocfileOnILockBytes)); REQUIRE(U(GetHGlobalFromILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    EQ(sco_put_stream(s, L"x", "payload", 7), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_Release(s), 0);
    HGLOBAL h = NULL;
    EQ(U(GetHGlobalFromILockBytes)(ilb, &h), S_OK);
    SIZE_T sz = GlobalSize(h);
    CHECK(sz >= 1024);
    BYTE *p = (BYTE *)GlobalLock(h);
    CHECK(p != NULL);
    if (p && sz >= 512) {
        CHECK(!memcmp(p, sco_sig, 8));
        CHECK(sco_u16(p + 0x1C) == 0xFFFE);   /* [MS-CFB] byte order */
        CHECK(sco_u16(p + 0x1E) == 9);        /* sector shift (512) */
        CHECK(sco_u16(p + 0x20) == 6);        /* mini sector shift */
        CHECK(sco_u32(p + 0x38) == 4096);     /* mini stream cutoff */
        t_note("ILockBytes docfile header: major=%u minor=0x%x fatSectors=%lu dirStart=%lu size=%llu",
               sco_u16(p + 0x1A), sco_u16(p + 0x18), sco_u32(p + 0x2C), sco_u32(p + 0x30), (unsigned long long)sz);
    }
    GlobalUnlock(h);
    ILockBytes_Release(ilb);
}

/* ---- StgOpenStorageOnILockBytes -------------------------------------------------------------- */
TEST(sco_openstorage_onilb_roundtrip) {
    REQUIRE(U(StgCreateDocfileOnILockBytes)); REQUIRE(U(StgOpenStorageOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL; IStream *st = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    EQ(sco_put_stream(s, L"s1", "abcdefghij", 10), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_Release(s), 0);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &s), S_OK);
    EQ(IStorage_OpenStream(s, L"s1", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    char b[16] = {0}; ULONG n = 0;
    EQ(IStream_Read(st, b, 16, &n), S_OK); EQ(n, 10);
    CHECK(!memcmp(b, "abcdefghij", 10));
    sco_relst(st);
    /* read-only open rejects writes */
    EQ(IStorage_CreateStream(s, L"x", EXCL | RW, 0, 0, &st), STG_E_ACCESSDENIED);
    EQ(IStorage_Release(s), 0);
    /* read/write reopen then modify */
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, RW | EXCL, NULL, 0, &s), S_OK);
    EQ(sco_put_stream(s, L"s2", "zz", 2), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_Release(s), 0);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &s), S_OK);
    EQ(IStorage_OpenStream(s, L"s2", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    sco_relst(st); sco_rel(s);
    ILockBytes_Release(ilb);
}

TEST(sco_openstorage_onilb_invalid) {
    REQUIRE(U(StgOpenStorageOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    HRESULT hr = U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &s);
    CHECK(FAILED(hr));
    CHECK(s == NULL);
    t_note("open on empty ILockBytes -> 0x%08lx", hr);
    EQ(U(StgOpenStorageOnILockBytes)(NULL, NULL, STGM_READ | EXCL, NULL, 0, &s), STG_E_INVALIDPOINTER);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, NULL), STG_E_INVALIDPOINTER);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, 0, NULL, 0, &s), STG_E_INVALIDFLAG);
    hr = U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 1, &s);
    CHECK(FAILED(hr));
    t_note("open on ILockBytes with reserved=1 -> 0x%08lx", hr);
    ILockBytes_Release(ilb);
}

TEST(sco_openstorage_onilb_garbage) {
    REQUIRE(U(StgOpenStorageOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    BYTE g[1024]; memset(g, 0x41, sizeof g); ULARGE_INTEGER o; o.QuadPart = 0; ULONG n;
    EQ(ILockBytes_WriteAt(ilb, o, g, sizeof g, &n), S_OK);
    IStorage *s = NULL;
    HRESULT hr = U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &s);
    CHECK(FAILED(hr));
    CHECK(s == NULL);
    t_note("open on garbage ILockBytes -> 0x%08lx", hr);
    ILockBytes_Release(ilb);
}

TEST(sco_openstorage_onilb_keeps_lockbytes_alive) {
    REQUIRE(U(StgCreateDocfileOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW, 0, &s), S_OK);
    /* the storage holds its own reference to the lockbytes */
    ULONG rc = ILockBytes_Release(ilb);
    CHECKF(rc >= 1, "lockbytes released to %lu while storage is open", rc);
    EQ(sco_put_stream(s, L"live", "1", 1), S_OK);
    EQ(IStorage_Release(s), 0);
}

TEST(sco_openstorage_onilb_transacted_commit_visible) {
    REQUIRE(U(StgCreateDocfileOnILockBytes)); REQUIRE(U(StgOpenStorageOnILockBytes));
    ILockBytes *ilb; EQ(mk_ilb(&ilb), S_OK);
    IStorage *s = NULL, *r = NULL;
    EQ(U(StgCreateDocfileOnILockBytes)(ilb, STGM_CREATE | EXCL | RW | STGM_TRANSACTED, 0, &s), S_OK);
    EQ(sco_put_stream(s, L"t", "123", 3), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(IStorage_Release(s), 0);
    EQ(U(StgOpenStorageOnILockBytes)(ilb, NULL, STGM_READ | EXCL, NULL, 0, &r), S_OK);
    IStream *st = NULL;
    EQ(IStorage_OpenStream(r, L"t", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    sco_relst(st); sco_rel(r);
    ILockBytes_Release(ilb);
}
