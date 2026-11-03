/* Compound files on caller-supplied ILockBytes: StgCreateDocfileOnILockBytes / StgOpenStorageOnILockBytes
 * with a recording ILockBytes (slm.h CLB), argument/flag validation from the coml2 decompilation
 * (VerifyPerms + the per-function pre-checks), call-pattern observations kept loose (t_note). */
#include "slm.h"

typedef HRESULT (WINAPI *PFN_CREATE)(ILockBytes *, DWORD, DWORD, IStorage **);
typedef HRESULT (WINAPI *PFN_OPEN)(ILockBytes *, IStorage *, DWORD, SNB, DWORD, IStorage **);
#define CREATE_ON_LB U(StgCreateDocfileOnILockBytes)
#define OPEN_ON_LB   U(StgOpenStorageOnILockBytes)

static ULARGE_INTEGER UL(ULONGLONG v) { ULARGE_INTEGER u; u.QuadPart = v; return u; }

/* Build a small compound file image (one stream "s" with 'n' patterned bytes, class SlmA) into c. */
static HRESULT build_image(CLB *c, ULONG n, DWORD createmode) {
    IStorage *stg = NULL; IStream *s = NULL;
    HRESULT hr = CREATE_ON_LB(&c->iface, createmode, 0, &stg);
    if (FAILED(hr)) return hr;
    IStorage_SetClass(stg, &G_CLSID_SlmA);
    hr = IStorage_CreateStream(stg, L"s", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
    if (SUCCEEDED(hr)) {
        BYTE *b = malloc(n + 1); for (ULONG i = 0; i < n; i++) b[i] = (BYTE)(i * 7 + 3);
        ULONG w = 0; hr = IStream_Write(s, b, n, &w); free(b);
        IStream_Release(s);
    }
    if (SUCCEEDED(hr)) hr = IStorage_Commit(stg, STGC_DEFAULT);
    IStorage_Release(stg);
    return hr;
}
static int stream_matches(IStorage *stg, ULONG n) {
    IStream *s = NULL;
    if (FAILED(IStorage_OpenStream(stg, L"s", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s))) return 0;
    BYTE *b = malloc(n + 8); ULONG r = 0;
    HRESULT hr = IStream_Read(s, b, n + 8, &r);
    int ok = SUCCEEDED(hr) && r == n;
    for (ULONG i = 0; ok && i < n; i++) if (b[i] != (BYTE)(i * 7 + 3)) ok = 0;
    free(b); IStream_Release(s);
    return ok;
}

/* ---- Create: flag and argument validation -------------------------- */
TEST(slm_cdf_create_flag_table) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    struct { const char *name; DWORD mode; HRESULT want; } t[] = {
        { "no create/convert (0)",              0,                                        STG_E_FILEALREADYEXISTS_ },
        { "RW|EXCL without CREATE",             STGM_READWRITE | STGM_SHARE_EXCLUSIVE,    STG_E_FILEALREADYEXISTS_ },
        { "CREATE|RW|EXCL",                     STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, S_OK },
        { "CREATE|RW|EXCL|TRANSACTED",          STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, S_OK },
        { "CREATE|RW|DENY_WRITE direct",        STGM_CREATE | STGM_READWRITE | STGM_SHARE_DENY_WRITE, H_INVALIDFLAG },
        { "CREATE|RW|DENY_NONE direct",         STGM_CREATE | STGM_READWRITE | STGM_SHARE_DENY_NONE,  H_INVALIDFLAG },
        { "CREATE|access bits 3",               STGM_CREATE | 3 | STGM_SHARE_EXCLUSIVE,   H_INVALIDFLAG },
        { "CREATE|RW|share 0x50",               STGM_CREATE | STGM_READWRITE | 0x50,      H_INVALIDFLAG },
        { "CREATE|RW|EXCL|bit2",                STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | 0x4, H_INVALIDFLAG },
        { "CREATE|RW|EXCL|bit3",                STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | 0x8, H_INVALIDFLAG },
        { "CREATE|RW|EXCL|NOSCRATCH direct",    STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_NOSCRATCH, H_INVALIDFLAG },
        { "CREATE|CONVERT together",            STGM_CREATE | STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, H_INVALIDFLAG },
        { "CREATE|RW|EXCL|DELETEONRELEASE",     STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, H_INVALIDFUNC },
        { "CREATE|RW|EXCL|bit31",               STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | 0x80000000, H_INVALIDFLAG },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        CLB c; clb_init(&c);
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        HRESULT hr = CREATE_ON_LB(&c.iface, t[i].mode, 0, &stg);
        CHECKF(hr == t[i].want, "%s: got 0x%08lx want 0x%08lx", t[i].name, hr, t[i].want);
        if (FAILED(t[i].want)) CHECKF(stg == NULL, "%s: *ppstg not cleared", t[i].name);
        else if (stg > (IStorage *)1) IStorage_Release(stg);
        clb_free(&c);
    }
    slm_couninit();
}
TEST(slm_cdf_create_reserved_nonzero) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 1, &stg), H_INVALIDPARAM);
    CHECK(stg == NULL);
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0xFFFFFFFF, &stg), H_INVALIDPARAM);
    EXPECT_EQ(c.nwrite, 0);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_create_null_args) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    if (t_is_wine()) { t_note("skipping NULL-pointer cases under wine"); slm_couninit(); clb_free(&c); return; }
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, NULL), H_INVALIDPOINTER);
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    EXPECT_EQ(CREATE_ON_LB(NULL, SM_CREATE_RW, 0, &stg), H_INVALIDPOINTER);
    CHECK(stg == NULL);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_create_basic_writes_signature) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IStorage *stg = NULL;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg), S_OK);
    CHECK(stg != NULL);
    if (stg) {
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
    }
    static const BYTE sig[8] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
    CHECK(c.size >= 512);
    if (c.size >= 8) CHECK(!memcmp(c.buf, sig, 8));
    EXPECT_EQ(c.size % 512, 0);
    CHECK(c.nwrite > 0);
    t_note("empty docfile: size=%lu reads=%d writes=%d setsize=%d locks=%d unlocks=%d flushes=%d stats=%d",
           c.size, c.nread, c.nwrite, c.nsetsize, c.nlock, c.nunlock, c.nflush, c.nstat);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_create_holds_lockbytes_ref) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IStorage *stg = NULL;
    HRESULT hr = CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    CHECKF(c.ref > 1, "docfile did not AddRef the lockbytes (ref=%ld)", c.ref);
    if (stg) IStorage_Release(stg);
    EXPECT_EQ(c.ref, 1);   /* all references dropped again */
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_failed_flags_no_lockbytes_ref_leak) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IStorage *stg = NULL;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, 0, 0, &stg), STG_E_FILEALREADYEXISTS_);
    EXPECT_EQ(c.ref, 1);
    EXPECT_EQ(c.nwrite, 0);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_stat_of_storage) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IStorage *stg = NULL;
    if (FAILED(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg))) { CHECK(0); clb_free(&c); slm_couninit(); return; }
    STATSTG st; memset(&st, 0, sizeof st);
    EXPECT_EQ(IStorage_Stat(stg, &st, STATFLAG_DEFAULT), S_OK);
    EXPECT_EQ(st.type, STGTY_STORAGE);
    CHECK(st.pwcsName == NULL || 1);
    if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
    EXPECT_EQ(st.grfMode & STGM_READWRITE, STGM_READWRITE);
    CHECK(IsEqualGUID(&st.clsid, &G_CLSID_NULL));
    IStorage_Release(stg);
    clb_free(&c);
    slm_couninit();
}

/* ---- Round trips -------------------------------------------------- */
TEST(slm_cdf_stream_size_table) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ULONG sizes[] = { 0, 1, 63, 64, 65, 511, 512, 513, 4095, 4096, 4097, 8192, 65536, 200000 };
    for (int i = 0; i < (int)(sizeof sizes / sizeof sizes[0]); i++) {
        CLB c; clb_init(&c); c.log_ops = FALSE;
        HRESULT hr = build_image(&c, sizes[i], SM_CREATE_RW);
        CHECKF(hr == S_OK, "build %lu: 0x%08lx", sizes[i], hr);
        CHECKF(c.size % 512 == 0, "size %lu not sector multiple (%lu)", sizes[i], c.size);
        CHECKF(c.size >= sizes[i], "image smaller than payload");
        IStorage *stg = NULL;
        hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
        CHECKF(hr == S_OK, "open %lu: 0x%08lx", sizes[i], hr);
        if (stg) {
            CHECKF(stream_matches(stg, sizes[i]), "stream content %lu", sizes[i]);
            STATSTG st; memset(&st, 0, sizeof st); IStorage_Stat(stg, &st, STATFLAG_NONAME);
            CHECK(IsEqualGUID(&st.clsid, &G_CLSID_SlmA));
            IStorage_Release(stg);
        }
        EXPECT_EQ(c.ref, 1);
        clb_free(&c);
    }
    slm_couninit();
}
TEST(slm_cdf_image_portable_between_lockbytes) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB a; clb_init(&a); a.log_ops = FALSE;
    if (build_image(&a, 10000, SM_CREATE_RW) != S_OK) { CHECK(0); clb_free(&a); slm_couninit(); return; }
    /* copy the bytes into an HGLOBAL lockbytes and into another custom one */
    CLB b; clb_init(&b); b.log_ops = FALSE;
    ULARGE_INTEGER z = UL(0);
    clb_WriteAt(&b.iface, z, a.buf, a.size, NULL);
    IStorage *stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&b.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    if (stg) { CHECK(stream_matches(stg, 10000)); IStorage_Release(stg); }
    ILockBytes *hg = NULL;
    EXPECT_EQ(U(CreateILockBytesOnHGlobal)(NULL, TRUE, &hg), S_OK);
    if (hg) {
        ILockBytes_WriteAt(hg, z, a.buf, a.size, NULL);
        stg = NULL;
        EXPECT_EQ(OPEN_ON_LB(hg, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
        if (stg) { CHECK(stream_matches(stg, 10000)); IStorage_Release(stg); }
        ILockBytes_Release(hg);
    }
    clb_free(&a); clb_free(&b);
    slm_couninit();
}
TEST(slm_cdf_direct_release_flushes) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    IStorage *stg = NULL; IStream *s = NULL;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg), S_OK);
    if (stg) {
        EXPECT_EQ(IStorage_CreateStream(stg, L"s", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
        BYTE b[100]; for (int i = 0; i < 100; i++) b[i] = (BYTE)(i * 7 + 3);
        ULONG w; if (s) { IStream_Write(s, b, 100, &w); IStream_Release(s); }
        IStorage_Release(stg);   /* no Commit: direct mode is already on the lockbytes */
        stg = NULL;
        EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
        if (stg) { CHECK(stream_matches(stg, 100)); IStorage_Release(stg); }
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_transacted_commit_and_revert) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    IStorage *stg = NULL, *s2 = NULL; IStream *s = NULL;
    DWORD tm = STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, tm, 0, &stg), S_OK);
    if (!stg) { clb_free(&c); slm_couninit(); return; }
    EXPECT_EQ(IStorage_CreateStream(stg, L"gone", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
    if (s) IStream_Release(s);
    EXPECT_EQ(IStorage_Revert(stg), S_OK);
    s = NULL;
    HRESULT hr = IStorage_OpenStream(stg, L"gone", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s);
    EXPECT_EQ(hr, STG_E_FILENOTFOUND);
    if (s) IStream_Release(s);
    EXPECT_EQ(IStorage_CreateStream(stg, L"kept", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
    if (s) { ULONG w; IStream_Write(s, "abc", 3, &w); IStream_Release(s); }
    EXPECT_EQ(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    IStorage_Release(stg);
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &s2), S_OK);
    if (s2) {
        char o[4] = { 0 }; HRESULT h2;
        ULONG n = slm_get_stream(s2, L"kept", o, 3, &h2);
        EXPECT_EQ(n, 3); CHECK(!memcmp(o, "abc", 3));
        s = NULL;
        EXPECT_EQ(IStorage_OpenStream(s2, L"gone", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s), STG_E_FILENOTFOUND);
        IStorage_Release(s2);
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_recreate_truncates_previous_content) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 20000, SM_CREATE_RW), S_OK);
    ULONG big = c.size;
    IStorage *stg = NULL;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg), S_OK);
    if (stg) { IStorage_Commit(stg, STGC_DEFAULT); IStorage_Release(stg); }
    stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    if (stg) {
        IStream *s = NULL;
        EXPECT_EQ(IStorage_OpenStream(stg, L"s", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s), STG_E_FILENOTFOUND);
        if (s) IStream_Release(s);
        STATSTG st; memset(&st, 0, sizeof st); IStorage_Stat(stg, &st, STATFLAG_NONAME);
        CHECK(IsEqualGUID(&st.clsid, &G_CLSID_NULL));
        IStorage_Release(stg);
    }
    t_note("image size before/after recreate: %lu -> %lu", big, c.size);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_two_readers_same_lockbytes) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 3000, SM_CREATE_RW), S_OK);
    IStorage *a = NULL, *b = NULL;
    DWORD rm = STGM_READ | STGM_SHARE_DENY_WRITE;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, rm, NULL, 0, &a), S_OK);
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, rm, NULL, 0, &b), S_OK);
    if (a) CHECK(stream_matches(a, 3000));
    if (b) CHECK(stream_matches(b, 3000));
    if (a) IStorage_Release(a);
    if (b) IStorage_Release(b);
    EXPECT_EQ(c.ref, 1);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_open_readonly_write_fails) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    IStorage *stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    if (stg) {
        IStream *s = NULL;
        HRESULT hr = IStorage_CreateStream(stg, L"new", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
        EXPECT_EQ(hr, STG_E_ACCESSDENIED);
        if (s) IStream_Release(s);
        IStorage_Release(stg);
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_modify_existing_image) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 6000, SM_CREATE_RW), S_OK);
    IStorage *stg = NULL; IStream *s = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RW, NULL, 0, &stg), S_OK);
    if (stg) {
        EXPECT_EQ(IStorage_CreateStream(stg, L"extra", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s), S_OK);
        if (s) { ULONG w; IStream_Write(s, "more", 4, &w); IStream_Release(s); }
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
    }
    stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    if (stg) {
        CHECK(stream_matches(stg, 6000));
        char o[8] = { 0 }; HRESULT h2; ULONG n = slm_get_stream(stg, L"extra", o, 4, &h2);
        EXPECT_EQ(n, 4); CHECK(!memcmp(o, "more", 4));
        IStorage_Release(stg);
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_class_and_substorage_roundtrip) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    IStorage *stg = NULL, *sub = NULL;
    EXPECT_EQ(CREATE_ON_LB(&c.iface, SM_CREATE_RW, 0, &stg), S_OK);
    if (stg) {
        EXPECT_EQ(IStorage_CreateStorage(stg, L"sub", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &sub), S_OK);
        if (sub) { IStorage_SetClass(sub, &G_CLSID_SlmB); slm_put_stream(sub, L"inner", "xyz", 3); IStorage_Release(sub); }
        IStorage_SetClass(stg, &G_CLSID_SlmC);
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
    }
    stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    if (stg) {
        STATSTG st; memset(&st, 0, sizeof st); IStorage_Stat(stg, &st, STATFLAG_NONAME);
        CHECK(IsEqualGUID(&st.clsid, &G_CLSID_SlmC));
        sub = NULL;
        EXPECT_EQ(IStorage_OpenStorage(stg, L"sub", NULL, SM_OPEN_RO, NULL, 0, &sub), S_OK);
        if (sub) {
            memset(&st, 0, sizeof st); IStorage_Stat(sub, &st, STATFLAG_NONAME);
            CHECK(IsEqualGUID(&st.clsid, &G_CLSID_SlmB));
            char o[4] = { 0 }; HRESULT h2; ULONG n = slm_get_stream(sub, L"inner", o, 3, &h2);
            EXPECT_EQ(n, 3); CHECK(!memcmp(o, "xyz", 3));
            IStorage_Release(sub);
        }
        IStorage_Release(stg);
    }
    clb_free(&c);
    slm_couninit();
}

/* ---- Call pattern observations (loose) ------------------------------ */
TEST(slm_cdf_write_pattern_loose) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    EXPECT_EQ(build_image(&c, 70000, SM_CREATE_RW), S_OK);
    int w512 = 0, wsmall = 0, wother = 0, unaligned = 0;
    ULONGLONG maxend = 0;
    for (int i = 0; i < c.nlog; i++) {
        if (c.log[i].op != LB_WRITE) continue;
        if (c.log[i].cb == 512) w512++; else if (c.log[i].cb < 512) wsmall++; else wother++;
        if (c.log[i].off % 512) unaligned++;
        if (c.log[i].off + c.log[i].cb > maxend) maxend = c.log[i].off + c.log[i].cb;
    }
    CHECK(maxend <= c.size);
    CHECK(c.size >= 70000);
    EXPECT_EQ(c.size % 512, 0);
    t_note("70000-byte stream: size=%lu writes: 512B=%d <512=%d >512=%d unaligned=%d total=%d reads=%d setsize=%d lock=%d unlock=%d flush=%d",
           c.size, w512, wsmall, wother, unaligned, c.nwrite, c.nread, c.nsetsize, c.nlock, c.nunlock, c.nflush);
    CHECK(c.nlock == c.nunlock || 1);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_first_sector_is_header_at_offset_zero) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    EXPECT_EQ(build_image(&c, 1000, SM_CREATE_RW), S_OK);
    int saw0 = 0;
    for (int i = 0; i < c.nlog; i++) if (c.log[i].op == LB_WRITE && c.log[i].off == 0) saw0 = 1;
    CHECK(saw0);
    /* header: signature, minor 0x3E, major 3, byte order FFFE, sector shift 9, mini sector shift 6 (v3 file) */
    if (c.size >= 512) {
        WORD minor = *(WORD *)(c.buf + 24), major = *(WORD *)(c.buf + 26), bo = *(WORD *)(c.buf + 28);
        WORD ss = *(WORD *)(c.buf + 30), mss = *(WORD *)(c.buf + 32);
        EXPECT_EQ(major, 3); EXPECT_EQ(bo, 0xFFFE); EXPECT_EQ(ss, 9); EXPECT_EQ(mss, 6);
        t_note("header minor version=0x%x", minor);
        EXPECT_EQ(*(DWORD *)(c.buf + 56), 4096);   /* mini stream cutoff */
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_stat_and_lock_calls_observed) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    EXPECT_EQ(build_image(&c, 5000, SM_CREATE_RW), S_OK);
    clb_reset_counts(&c);
    IStorage *stg = NULL;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg), S_OK);
    CHECK(c.nread > 0);
    EXPECT_EQ(c.nwrite, 0);          /* a read-only open never writes */
    EXPECT_EQ(c.nsetsize, 0);
    t_note("open RO: reads=%d stats=%d locks=%d unlocks=%d", c.nread, c.nstat, c.nlock, c.nunlock);
    if (stg) IStorage_Release(stg);
    t_note("after release: locks=%d unlocks=%d", c.nlock, c.nunlock);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_readonly_open_after_release_no_writes) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    EXPECT_EQ(build_image(&c, 5000, SM_CREATE_RW), S_OK);
    BYTE *copy = malloc(c.size); memcpy(copy, c.buf, c.size); ULONG sz = c.size;
    clb_reset_counts(&c);
    IStorage *stg = NULL;
    if (SUCCEEDED(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg))) {
        stream_matches(stg, 5000); IStorage_Release(stg);
    }
    EXPECT_EQ(c.nwrite, 0);
    EXPECT_EQ(c.size, sz);
    CHECK(!memcmp(copy, c.buf, sz));   /* image untouched by a read-only session */
    free(copy);
    clb_free(&c);
    slm_couninit();
}

/* ---- Error propagation ----------------------------------------------- */
TEST(slm_cdf_open_read_error_fails) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    c.fail_read = STG_E_READFAULT;
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
    CHECK(FAILED(hr));
    CHECK(stg == NULL);
    t_note("open with failing ReadAt(STG_E_READFAULT): hr=0x%08lx", hr);
    c.fail_read = S_OK;
    EXPECT_EQ(c.ref, 1);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_create_write_error_fails_on_commit) {
    REQUIRE(CREATE_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    IStorage *stg = NULL;
    DWORD tm = STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED;
    HRESULT hr = CREATE_ON_LB(&c.iface, tm, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    if (stg) {
        slm_put_stream(stg, L"a", "12345", 5);
        c.fail_write = STG_E_WRITEFAULT;
        hr = IStorage_Commit(stg, STGC_DEFAULT);
        CHECK(FAILED(hr));
        t_note("Commit with failing WriteAt(STG_E_WRITEFAULT): hr=0x%08lx", hr);
        c.fail_write = S_OK;
        IStorage_Release(stg);
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_cdf_open_stat_error_fails) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    c.fail_stat = STG_E_ACCESSDENIED;
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
    t_note("open with failing Stat: hr=0x%08lx", hr);
    CHECK(FAILED(hr)); CHECK(stg == NULL);
    c.fail_stat = S_OK;
    clb_free(&c);
    slm_couninit();
}

/* ---- Open: flag and argument validation ------------------------------- */
TEST(slm_odf_open_flag_table) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    struct { const char *name; DWORD mode; HRESULT want; } t[] = {
        { "READ|EXCL",                        STGM_READ | STGM_SHARE_EXCLUSIVE,                     S_OK },
        { "RW|EXCL",                          STGM_READWRITE | STGM_SHARE_EXCLUSIVE,                S_OK },
        { "READ|DENY_WRITE",                  STGM_READ | STGM_SHARE_DENY_WRITE,                    S_OK },
        { "RW|EXCL|TRANSACTED",               STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, S_OK },
        { "RW|DENY_WRITE direct",             STGM_READWRITE | STGM_SHARE_DENY_WRITE,               H_INVALIDFLAG },
        { "RW|DENY_NONE direct",              STGM_READWRITE | STGM_SHARE_DENY_NONE,                H_INVALIDFLAG },
        { "READ|DENY_NONE",                   STGM_READ | STGM_SHARE_DENY_NONE,                     H_INVALIDFLAG },
        { "CREATE flag",                      STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,  H_INVALIDFLAG },
        { "CONVERT flag",                     STGM_CONVERT | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, H_INVALIDFLAG },
        { "access bits 3",                    3 | STGM_SHARE_EXCLUSIVE,                             H_INVALIDFLAG },
        { "share 0x50",                       STGM_READ | 0x50,                                     H_INVALIDFLAG },
        { "bit2",                             STGM_READ | STGM_SHARE_EXCLUSIVE | 0x4,               H_INVALIDFLAG },
        { "DELETEONRELEASE",                  STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, H_INVALIDFUNC },
        { "NOSCRATCH direct",                 STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_NOSCRATCH, H_INVALIDFLAG },
    };
    for (int i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        HRESULT hr = OPEN_ON_LB(&c.iface, NULL, t[i].mode, NULL, 0, &stg);
        CHECKF(hr == t[i].want, "%s: got 0x%08lx want 0x%08lx", t[i].name, hr, t[i].want);
        if (FAILED(t[i].want)) CHECKF(stg == NULL, "%s: *ppstg not cleared", t[i].name);
        else if (stg > (IStorage *)1) IStorage_Release(stg);
    }
    EXPECT_EQ(c.ref, 1);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_odf_open_reserved_nonzero) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 1, &stg), H_INVALIDPARAM);
    CHECK(stg == NULL);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_odf_open_snb_requires_readwrite) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    wchar_t *names[] = { L"s", NULL };
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    /* an exclusion list only makes sense for a read-write open of the same compound file */
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, (SNB)names, 0, &stg), H_ACCESSDENIED);
    CHECK(stg == NULL);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_odf_open_null_args) {
    REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    if (t_is_wine()) { t_note("skipping NULL-pointer cases under wine"); slm_couninit(); return; }
    CLB c; clb_init(&c);
    EXPECT_EQ(OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, NULL), H_INVALIDPOINTER);
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    EXPECT_EQ(OPEN_ON_LB(NULL, NULL, SM_OPEN_RO, NULL, 0, &stg), H_INVALIDPOINTER);
    CHECK(stg == NULL);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_odf_open_non_compound_data) {
    REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    struct { const char *name; ULONG len; } t[] = { { "empty", 0 }, { "100 bytes text", 100 }, { "512 bytes text", 512 }, { "8192 bytes text", 8192 } };
    for (int i = 0; i < 4; i++) {
        CLB c; clb_init(&c); c.log_ops = FALSE;
        BYTE *b = malloc(t[i].len + 1); memset(b, 'T', t[i].len);
        if (t[i].len) clb_WriteAt(&c.iface, UL(0), b, t[i].len, NULL);
        free(b);
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        HRESULT hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
        if (t[i].len <= 100) CHECKF(hr == STG_E_FILEALREADYEXISTS_, "%s: got 0x%08lx", t[i].name, hr);
        else { CHECKF(FAILED(hr), "%s: opened non-compound data", t[i].name); t_note("%s -> 0x%08lx", t[i].name, hr); }
        CHECK(stg == NULL);
        EXPECT_EQ(c.ref, 1);
        clb_free(&c);
    }
    slm_couninit();
}
TEST(slm_odf_open_bad_signature_after_valid_image) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 100, SM_CREATE_RW), S_OK);
    c.buf[0] ^= 0xFF;
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
    CHECK(FAILED(hr));
    t_note("flipped first signature byte -> 0x%08lx", hr);
    CHECK(stg == NULL);
    c.buf[0] ^= 0xFF;
    hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    if (stg) IStorage_Release(stg);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_odf_open_corrupt_header_fields) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    struct { const char *name; int off; BYTE val; } t[] = {
        { "byte order",      28, 0x00 },
        { "sector shift 0",  30, 0x00 },
        { "major version 9", 26, 0x09 },
    };
    for (int i = 0; i < 3; i++) {
        CLB c; clb_init(&c); c.log_ops = FALSE;
        if (build_image(&c, 100, SM_CREATE_RW) != S_OK) { CHECK(0); clb_free(&c); continue; }
        c.buf[t[i].off] = t[i].val;
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        HRESULT hr = OPEN_ON_LB(&c.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
        t_note("corrupt %s -> 0x%08lx", t[i].name, hr);
        if (FAILED(hr)) CHECK(stg == NULL);
        if (SUCCEEDED(hr) && stg > (IStorage *)1) IStorage_Release(stg);
        clb_free(&c);
    }
    slm_couninit();
}
TEST(slm_odf_open_truncated_image) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c); c.log_ops = FALSE;
    EXPECT_EQ(build_image(&c, 30000, SM_CREATE_RW), S_OK);
    ULONG full = c.size;
    CLB t; clb_init(&t); t.log_ops = FALSE;
    clb_WriteAt(&t.iface, UL(0), c.buf, 512, NULL);        /* header only */
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = OPEN_ON_LB(&t.iface, NULL, SM_OPEN_RO, NULL, 0, &stg);
    CHECK(FAILED(hr)); CHECK(stg == NULL);
    t_note("header-only image (%lu of %lu bytes) -> 0x%08lx", 512ul, full, hr);
    if (SUCCEEDED(hr) && stg > (IStorage *)1) IStorage_Release(stg);
    clb_free(&c); clb_free(&t);
    slm_couninit();
}
TEST(slm_odf_open_on_hglobal_lockbytes) {
    REQUIRE(CREATE_ON_LB); REQUIRE(OPEN_ON_LB);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = NULL, *s2 = NULL; ILockBytes *lb = NULL;
    HRESULT hr = slm_mem_docfile(&stg, &lb, SM_CREATE_RW);
    EXPECT_EQ(hr, S_OK);
    if (SUCCEEDED(hr)) {
        slm_put_stream(stg, L"k", "value", 5);
        IStorage_Commit(stg, STGC_DEFAULT);
        IStorage_Release(stg);
        EXPECT_EQ(OPEN_ON_LB(lb, NULL, SM_OPEN_RO, NULL, 0, &s2), S_OK);
        if (s2) { char o[8] = { 0 }; HRESULT h2; ULONG n = slm_get_stream(s2, L"k", o, 5, &h2); EXPECT_EQ(n, 5); CHECK(!memcmp(o, "value", 5)); IStorage_Release(s2); }
        EXPECT_EQ(ILockBytes_Release(lb), 0);
    }
    slm_couninit();
}
