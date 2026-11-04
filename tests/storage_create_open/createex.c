/* StgCreateStorageEx / StgOpenStorageEx: formats, options, riid, sector size. */
#include "sco.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define DW    STGM_SHARE_DENY_WRITE
#define RW    STGM_READWRITE

static const GUID sco_IID_IPropertySetStorage = {0x0000013A,0,0,{0xC0,0,0,0,0,0,0,0x46}};

static HRESULT cex(const WCHAR *p, DWORD mode, DWORD fmt, DWORD attrs, STGOPTIONS *o, const GUID *riid, void **out) {
    *out = NULL;
    return U(StgCreateStorageEx)(p, mode, fmt, attrs, o, NULL, riid, out);
}
static HRESULT oex(const WCHAR *p, DWORD mode, DWORD fmt, DWORD attrs, STGOPTIONS *o, const GUID *riid, void **out) {
    *out = NULL;
    return U(StgOpenStorageEx)(p, mode, fmt, attrs, o, NULL, riid, out);
}

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_stgcreatestorageex */
TEST(sco_createex_docfile_with_options) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cex1.stg"); sco_rm(p);
    STGOPTIONS o = {1, 0, 4096};
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s), S_OK);
    CHECK(s != NULL);
    EQ(sco_rel(s), 0);
    CHECK(sco_exists(p));
    sco_rm(p);
}

TEST(sco_createex_null_options_all_formats) {
    REQUIRE(U(StgCreateStorageEx));
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_stgcreatestorageex */
    struct { DWORD fmt; const char *n; int hard; } f[] = {
        { STGFMT_STORAGE, "STORAGE", 1 }, { STGFMT_DOCFILE, "DOCFILE", 1 }, { STGFMT_ANY, "ANY", 0 },
        { STGFMT_NATIVE, "NATIVE", 0 }, { STGFMT_FILE, "FILE", 0 },
    };
    WCHAR p[MAX_PATH]; sco_wpath(p, "cex_fmt.stg");
    for (unsigned i = 0; i < SCO_ARRAY(f); i++) {
        sco_rm(p);
        IStorage *s = NULL;
        HRESULT hr = cex(p, EXCL | RW | STGM_CREATE, f[i].fmt, 0, NULL, &sco_IID_IStorage, (void **)&s);
        if (f[i].hard) CHECKF(hr == S_OK, "%s: 0x%08lx", f[i].n, hr);
        else t_note("StgCreateStorageEx(STGFMT_%s, NULL options) -> 0x%08lx", f[i].n, hr);
        if (hr == S_OK) {
            BYTE *b; LONG n = sco_read(p, &b);
            CHECKF(n >= 512, "%s: file size %ld", f[i].n, n);
            if (n >= 8) {
                int sig = !memcmp(b, sco_sig, 8);
                if (f[i].hard) CHECKF(sig, "%s: no compound file signature", f[i].n);
                else t_note("STGFMT_%s created file %s a compound file signature", f[i].n, sig ? "with" : "without");
            }
            free(b);
        } else CHECK(s == NULL);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_createex_sector_size_512_header) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cex512.stg"); sco_rm(p);
    STGOPTIONS o = {1, 0, 512};
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s), S_OK);
    EQ(sco_put_stream(s, L"x", "1", 1), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n >= 1024 && n % 512 == 0);
    if (n >= 512) {
        CHECK(!memcmp(b, sco_sig, 8));
        CHECK(sco_u16(b + 0x1A) == 3);
        CHECK(sco_u16(b + 0x1E) == 9);
        CHECK(sco_u16(b + 0x20) == 6);
    }
    free(b);
    sco_rm(p);
}

TEST(sco_createex_sector_size_4096_header) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cex4096.stg"); sco_rm(p);
    STGOPTIONS o = {1, 0, 4096};
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s), S_OK);
    EQ(sco_put_stream(s, L"x", "1", 1), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(sco_rel(s), 0);
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n >= 4096);
    t_note("4096-sector docfile: size=%ld", n);
    if (n >= 512) {
        CHECK(!memcmp(b, sco_sig, 8));
        t_note("4096-sector header: major=%u shift=%u mini=%u dirSectors=%lu", sco_u16(b + 0x1A), sco_u16(b + 0x1E), sco_u16(b + 0x20), sco_u32(b + 0x28));
        /* [MS-CFB]: major version 4 <=> sector shift 12; version 3 <=> shift 9 */
        unsigned maj = sco_u16(b + 0x1A), sh = sco_u16(b + 0x1E);
        CHECK((maj == 3 && sh == 9) || (maj == 4 && sh == 12));
        if (n % 4096) t_note("file size %ld is not a multiple of 4096", n);
    }
    free(b);
    /* reopens normally */
    IStorage *r = NULL; IStream *st = NULL;
    EQ(sco_open(p, EXCL | STGM_READ, &r), S_OK);
    EQ(IStorage_OpenStream(r, L"x", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    sco_relst(st); sco_rel(r);
    sco_rm(p);
}

TEST(sco_createex_sector_size_invalid) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexbad.stg"); sco_rm(p);
    ULONG sizes[] = { 0, 1, 256, 511, 513, 1024, 2048, 8192, 65536 };
    for (unsigned i = 0; i < SCO_ARRAY(sizes); i++) {
        STGOPTIONS o = {1, 0, sizes[i]};
        IStorage *s = NULL;
        HRESULT hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s);
        /* documented: ulSectorSize must be 512 or 4096 */
        CHECKF(hr == STG_E_INVALIDPARAMETER, "sector size %lu -> 0x%08lx", sizes[i], hr);
        CHECK(s == NULL);
        sco_rel(s);
        CHECKF(!sco_exists(p), "file created for bad sector size %lu", sizes[i]);
        sco_rm(p);
    }
}

TEST(sco_createex_options_versions) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexver.stg");
    for (USHORT v = 0; v <= 4; v++) {
        sco_rm(p);
        STGOPTIONS o; memset(&o, 0, sizeof o);
        o.usVersion = v; o.ulSectorSize = 512; o.pwcsTemplateFile = NULL;
        IStorage *s = NULL;
        HRESULT hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s);
        t_note("STGOPTIONS.usVersion=%u sectorSize=512 -> 0x%08lx", v, hr);
        if (v == 1 || v == 2) CHECKF(hr == S_OK, "usVersion %u: 0x%08lx", v, hr);
        sco_rel(s);
    }
    sco_rm(p);
}

TEST(sco_createex_simple_mode_4096) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexsimple.stg"); sco_rm(p);
    STGOPTIONS o = {1, 0, 4096};
    IStorage *s = NULL;
    HRESULT hr = cex(p, STGM_SIMPLE | EXCL | RW | STGM_CREATE, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s);
    t_note("STGM_SIMPLE with 4096 sector size -> 0x%08lx (documented: may not be specified)", hr);
    sco_rel(s); sco_rm(p);
    o.ulSectorSize = 512;
    hr = cex(p, STGM_SIMPLE | EXCL | RW | STGM_CREATE, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s);
    CHECK(hr == S_OK);
    sco_rel(s); sco_rm(p);
    hr = cex(p, STGM_SIMPLE | STGM_TRANSACTED | EXCL | RW | STGM_CREATE, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s);
    CHECK(FAILED(hr));
    t_note("STGM_SIMPLE|STGM_TRANSACTED -> 0x%08lx", hr);
    sco_rel(s); sco_rm(p);
}

TEST(sco_createex_flag_validity) {
    REQUIRE(U(StgCreateStorageEx));
    /* same validity rules as StgCreateDocfile */
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexflags.stg"); sco_rm(p);
    DWORD bad[] = { 0, STGM_CREATE, EXCL | STGM_READ, STGM_READ, DW | STGM_READ, EXCL | RW | STGM_TRANSACTED | 0x80 };
    for (unsigned i = 0; i < SCO_ARRAY(bad); i++) {
        IStorage *s = NULL;
        HRESULT hr = cex(p, bad[i], STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s);
        CHECKF(hr == STG_E_INVALIDFLAG, "mode 0x%lx -> 0x%08lx", bad[i], hr);
        sco_rel(s);
        CHECK(!sco_exists(p));
    }
}

TEST(sco_createex_existing_file_policy) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexexist.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), S_OK);
    sco_rel(s);
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), STG_E_FILEALREADYEXISTS);
    CHECK(s == NULL);
    EQ(cex(p, EXCL | RW | STGM_FAILIFTHERE, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), STG_E_FILEALREADYEXISTS);
    EQ(cex(p, EXCL | RW | STGM_CREATE, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), S_OK);
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_createex_transacted_creates_file_immediately) {
    REQUIRE(U(StgCreateStorageEx));
    /* documented: with STGM_TRANSACTED and no existing file, the file is created immediately */
    WCHAR p[MAX_PATH]; sco_wpath(p, "cextx.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW | STGM_TRANSACTED, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), S_OK);
    CHECK(sco_exists(p));
    sco_rel(s);
    sco_rm(p);
}

TEST(sco_createex_temp_file) {
    REQUIRE(U(StgCreateStorageEx));
    IStorage *s = NULL;
    HRESULT hr = cex(NULL, EXCL | RW | STGM_CREATE | STGM_DELETEONRELEASE, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        STATSTG st; memset(&st, 0, sizeof st);
        EQ(IStorage_Stat(s, &st, STATFLAG_DEFAULT), S_OK);
        t_note("temp docfile name: %ls", st.pwcsName ? st.pwcsName : L"(null)");
        if (st.pwcsName) U(CoTaskMemFree)(st.pwcsName);
        sco_rel(s);
    }
}

TEST(sco_createex_riid_propertysetstorage) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexpss.stg"); sco_rm(p);
    void *o = NULL;
    HRESULT hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IPropertySetStorage, &o);
    EQ(hr, S_OK);   /* documented: IStorage or IPropertySetStorage */
    if (o) IUnknown_Release((IUnknown *)o);
    sco_rm(p);
}

TEST(sco_createex_riid_other) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexriid.stg"); sco_rm(p);
    void *o = (void *)1;
    HRESULT hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStream, &o);
    CHECK(FAILED(hr));
    t_note("StgCreateStorageEx(riid=IStream) -> 0x%08lx", hr);
    CHECK(o == NULL);
    if (o) IUnknown_Release((IUnknown *)o);
    sco_rm(p);
    hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IUnknown, &o);
    t_note("StgCreateStorageEx(riid=IUnknown) -> 0x%08lx", hr);
    if (o) IUnknown_Release((IUnknown *)o);
    sco_rm(p);
}

TEST(sco_createex_grfattrs) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexattr.stg"); sco_rm(p);
    IStorage *s = NULL;
    HRESULT hr = cex(p, EXCL | RW, STGFMT_DOCFILE, FILE_FLAG_NO_BUFFERING, NULL, &sco_IID_IStorage, (void **)&s);
    t_note("DOCFILE with FILE_FLAG_NO_BUFFERING -> 0x%08lx (documented as allowed)", hr);
    sco_rel(s); sco_rm(p);
    hr = cex(p, EXCL | RW, STGFMT_DOCFILE, 0x12345678, NULL, &sco_IID_IStorage, (void **)&s);
    t_note("DOCFILE with random grfAttrs -> 0x%08lx (documented: 0 or NO_BUFFERING)", hr);
    sco_rel(s); sco_rm(p);
    hr = cex(p, EXCL | RW, STGFMT_STORAGE, 1, NULL, &sco_IID_IStorage, (void **)&s);
    t_note("STORAGE with grfAttrs=1 -> 0x%08lx (documented: must be 0)", hr);
    sco_rel(s); sco_rm(p);
}

TEST(sco_createex_null_out_pointer) {
    REQUIRE(U(StgCreateStorageEx));
    if (t_is_wine()) { t_skip("not exercised under wine"); return; }
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexnull.stg"); sco_rm(p);
    HRESULT hr = U(StgCreateStorageEx)(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, NULL, &sco_IID_IStorage, NULL);
    CHECK(FAILED(hr));
    t_note("StgCreateStorageEx(ppObjectOpen = NULL) -> 0x%08lx", hr);
    hr = U(StgCreateStorageEx)(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, NULL, NULL, (void **)&p);
    t_note("StgCreateStorageEx(riid = NULL) -> 0x%08lx", hr);
    sco_rm(p);
}

TEST(sco_createex_invalid_format) {
    REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "cexfmt.stg"); sco_rm(p);
    DWORD fmts[] = { 2, 6, 7, 100, 0xFFFFFFFF };
    for (unsigned i = 0; i < SCO_ARRAY(fmts); i++) {
        IStorage *s = NULL;
        HRESULT hr = cex(p, EXCL | RW, fmts[i], 0, NULL, &sco_IID_IStorage, (void **)&s);
        CHECKF(FAILED(hr), "stgfmt %lu succeeded", fmts[i]);
        t_note("StgCreateStorageEx(stgfmt=%lu) -> 0x%08lx", fmts[i], hr);
        sco_rel(s); sco_rm(p);
    }
}

/* ---- StgOpenStorageEx -------------------------------------------------------------------------- */
TEST(sco_openex_docfile_formats) {
    REQUIRE(U(StgOpenStorageEx)); REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex1.stg"); sco_rm(p);
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&s), S_OK);
    EQ(sco_put_stream(s, L"k", "v", 1), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    DWORD fmts[] = { STGFMT_DOCFILE, STGFMT_ANY, STGFMT_STORAGE };
    const char *nm[] = { "DOCFILE", "ANY", "STORAGE" };
    for (unsigned i = 0; i < 3; i++) {
        IStorage *o = NULL;
        HRESULT hr = oex(p, EXCL | STGM_READ, fmts[i], 0, NULL, &sco_IID_IStorage, (void **)&o);
        CHECKF(hr == S_OK, "STGFMT_%s: 0x%08lx", nm[i], hr);
        if (hr == S_OK) {
            IStream *st = NULL;
            CHECK(IStorage_OpenStream(o, L"k", NULL, STGM_READ | EXCL, 0, &st) == S_OK);
            sco_relst(st);
            EQ(sco_rel(o), 0);
        }
    }
    sco_rm(p);
}

TEST(sco_openex_nonexistent) {
    REQUIRE(U(StgOpenStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_none.stg"); sco_rm(p);
    DWORD fmts[] = { STGFMT_DOCFILE, STGFMT_ANY, STGFMT_STORAGE };
    for (unsigned i = 0; i < 3; i++) {
        IStorage *o = NULL;
        HRESULT hr = oex(p, EXCL | STGM_READ, fmts[i], 0, NULL, &sco_IID_IStorage, (void **)&o);
        CHECKF(hr == STG_E_FILENOTFOUND, "fmt %lu: 0x%08lx", fmts[i], hr);
        CHECK(o == NULL);
    }
    CHECK(!sco_exists(p));
}

TEST(sco_openex_non_docfile_files) {
    REQUIRE(U(StgOpenStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_txt.dat"); sco_rm(p);
    char buf[1024]; memset(buf, 'q', sizeof buf);
    CHECK(sco_write_file(p, buf, sizeof buf));
    DWORD fmts[] = { STGFMT_DOCFILE, STGFMT_ANY, STGFMT_STORAGE };
    const char *nm[] = { "DOCFILE", "ANY", "STORAGE" };
    for (unsigned i = 0; i < 3; i++) {
        IStorage *o = NULL;
        HRESULT hr = oex(p, EXCL | STGM_READ, fmts[i], 0, NULL, &sco_IID_IStorage, (void **)&o);
        CHECKF(FAILED(hr), "STGFMT_%s on a plain file succeeded", nm[i]);
        CHECK(o == NULL);
        t_note("StgOpenStorageEx(STGFMT_%s) on a non-compound file -> 0x%08lx", nm[i], hr);
        sco_rel(o);
    }
    /* zero length file */
    CHECK(sco_write_file(p, NULL, 0));
    IStorage *o = NULL;
    HRESULT hr = oex(p, EXCL | STGM_READ, STGFMT_ANY, 0, NULL, &sco_IID_IStorage, (void **)&o);
    CHECK(FAILED(hr));
    t_note("StgOpenStorageEx(STGFMT_ANY) on zero-length file -> 0x%08lx", hr);
    sco_rel(o);
    sco_rm(p);
}

TEST(sco_openex_riid_propertysetstorage) {
    REQUIRE(U(StgOpenStorageEx)); REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_pss.stg"); sco_rm(p);
    IStorage *s = NULL; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    void *o = NULL;
    EQ(oex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IPropertySetStorage, &o), S_OK);
    if (o) IUnknown_Release((IUnknown *)o);
    sco_rm(p);
}

TEST(sco_openex_flag_validity) {
    REQUIRE(U(StgOpenStorageEx)); REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_flags.stg"); sco_rm(p);
    IStorage *s = NULL; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    DWORD bad[] = { 0, STGM_CREATE | EXCL | STGM_READ, STGM_SHARE_DENY_NONE | STGM_READ, DW | RW };
    for (unsigned i = 0; i < SCO_ARRAY(bad); i++) {
        IStorage *o = NULL;
        HRESULT hr = oex(p, bad[i], STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&o);
        /* Windows 10: CREATE|EXCL|READ is rejected with STG_E_INVALIDPARAMETER, the others with INVALIDFLAG */
        HRESULT want = bad[i] == (STGM_CREATE | EXCL | STGM_READ) ? STG_E_INVALIDPARAMETER : STG_E_INVALIDFLAG;
        CHECKF(hr == want, "mode 0x%lx: 0x%08lx", bad[i], hr);
        sco_rel(o);
    }
    sco_rm(p);
}

TEST(sco_openex_share_violation) {
    REQUIRE(U(StgOpenStorageEx)); REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_share.stg"); sco_rm(p);
    IStorage *s = NULL; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    IStorage *a = NULL, *b = NULL;
    EQ(oex(p, EXCL | STGM_READ, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&a), S_OK);
    EQ(oex(p, EXCL | STGM_READ, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&b), STG_E_SHAREVIOLATION);
    sco_rel(a);
    sco_rm(p);
}

TEST(sco_openex_sector_size_preserved) {
    REQUIRE(U(StgOpenStorageEx)); REQUIRE(U(StgCreateStorageEx));
    WCHAR p[MAX_PATH]; sco_wpath(p, "oex_4k.stg"); sco_rm(p);
    STGOPTIONS o = {1, 0, 4096};
    IStorage *s = NULL;
    EQ(cex(p, EXCL | RW, STGFMT_DOCFILE, 0, &o, &sco_IID_IStorage, (void **)&s), S_OK);
    EQ(sco_put_stream(s, L"big", "x", 1), S_OK);
    sco_rel(s);
    IStorage *r = NULL;
    STGOPTIONS ro = {1, 0, 0};
    HRESULT hr = oex(p, EXCL | RW, STGFMT_DOCFILE, 0, &ro, &sco_IID_IStorage, (void **)&r);
    t_note("StgOpenStorageEx on 4096-sector file with options{sector 0} -> 0x%08lx", hr);
    sco_rel(r);
    hr = oex(p, EXCL | RW, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&r);
    EQ(hr, S_OK);
    if (r) { IStream *st = NULL; CHECK(IStorage_OpenStream(r, L"big", NULL, EXCL | RW, 0, &st) == S_OK); sco_relst(st); }
    sco_rel(r);
    sco_rm(p);
}

TEST(sco_openex_null_name) {
    REQUIRE(U(StgOpenStorageEx));
    IStorage *o = NULL;
    HRESULT hr = oex(NULL, EXCL | STGM_READ, STGFMT_DOCFILE, 0, NULL, &sco_IID_IStorage, (void **)&o);
    CHECK(FAILED(hr));
    t_note("StgOpenStorageEx(NULL name) -> 0x%08lx", hr);
    sco_rel(o);
}
