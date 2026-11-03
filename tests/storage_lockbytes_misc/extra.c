/* Extra round-trip tests: docfile persisted on an HGLOBAL ILockBytes. */
#include "slm.h"

static ULARGE_INTEGER XUL(ULONGLONG v) { ULARGE_INTEGER u; u.QuadPart = v; return u; }

TEST(slm_x_docfile_on_hglobal_roundtrip) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    REQUIRE(U(StgCreateDocfileOnILockBytes));
    REQUIRE(U(StgOpenStorageOnILockBytes));
    REQUIRE(U(StgIsStorageILockBytes));
    if (!slm_coinit()) { CHECK(0); return; }
    ILockBytes *lb = NULL;
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &lb), S_OK);
    if (!lb) { slm_couninit(); return; }
    IStorage *stg = NULL;
    HRESULT hr = U(StgCreateDocfileOnILockBytes)(lb, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    if (stg) {
        IStream *s = NULL;
        EXPECT_EQ(IStorage_CreateStream(stg, L"data", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
        if (s) {
            ULONG w = 0;
            EXPECT_EQ(IStream_Write(s, "slm-roundtrip", 13, &w), S_OK);
            EXPECT_EQ(w, 13);
            IStream_Release(s);
        }
        EXPECT_EQ(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
        IStorage_Release(stg);
    }
    EXPECT_EQ(U(StgIsStorageILockBytes)(lb), S_OK);
    stg = NULL;
    hr = U(StgOpenStorageOnILockBytes)(lb, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    if (stg) {
        IStream *s = NULL;
        EXPECT_EQ(IStorage_OpenStream(stg, L"data", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s), S_OK);
        if (s) {
            char b[32] = {0}; ULONG r = 0;
            EXPECT_EQ(IStream_Read(s, b, 31, &r), S_OK);
            EXPECT_EQ(r, 13);
            CHECK(memcmp(b, "slm-roundtrip", 13) == 0);
            IStream_Release(s);
        }
        IStorage_Release(stg);
    }
    STATSTG st; memset(&st, 0, sizeof st);
    EXPECT_EQ(ILockBytes_Stat(lb, &st, STATFLAG_NONAME), S_OK);
    CHECK(st.cbSize.QuadPart >= 1024 && (st.cbSize.QuadPart % 512) == 0);
    ILockBytes_Release(lb);
    slm_couninit();
}

TEST(slm_x_docfile_on_hglobal_signature) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    REQUIRE(U(StgCreateDocfileOnILockBytes));
    if (!slm_coinit()) { CHECK(0); return; }
    ILockBytes *lb = NULL; IStorage *stg = NULL;
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &lb), S_OK);
    if (lb && SUCCEEDED(U(StgCreateDocfileOnILockBytes)(lb, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg))) {
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
        static const BYTE sig[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
        BYTE b[8] = {0}; ULONG r = 0;
        EXPECT_EQ(ILockBytes_ReadAt(lb, XUL(0), b, 8, &r), S_OK);
        EXPECT_EQ(r, 8);
        CHECK(memcmp(b, sig, 8) == 0);
    } else CHECK(0);
    if (lb) ILockBytes_Release(lb);
    slm_couninit();
}

TEST(slm_x_open_storage_on_empty_lockbytes_fails) {
    REQUIRE(U(CreateILockBytesOnHGlobal));
    REQUIRE(U(StgOpenStorageOnILockBytes));
    REQUIRE(U(StgIsStorageILockBytes));
    if (!slm_coinit()) { CHECK(0); return; }
    ILockBytes *lb = NULL;
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &lb), S_OK);
    if (lb) {
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        HRESULT hr = U(StgOpenStorageOnILockBytes)(lb, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &stg);
        CHECK(FAILED(hr));
        if (SUCCEEDED(hr) && stg) IStorage_Release(stg);
        t_note("empty lockbytes open hr=0x%08lx", (unsigned long)hr);
        CHECK(U(StgIsStorageILockBytes)(lb) != S_OK);
        ILockBytes_Release(lb);
    }
    slm_couninit();
}
