/* Byte-level checks of files produced by StgCreateDocfile against [MS-CFB], and reading of
 * hand-crafted [MS-CFB] files. */
#include "cfb.h"

#define EXCL  STGM_SHARE_EXCLUSIVE
#define RW    STGM_READWRITE

static int load(const WCHAR *p, cfb_t *c, BYTE **buf) {
    LONG n = sco_read(p, buf);
    if (n < 0) return 0;
    return cfb_parse(c, *buf, n);
}

TEST(ondisk_empty_docfile_header) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_hdr.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(sco_rel(s), 0);
    BYTE *b; LONG n = sco_read(p, &b);
    CHECK(n >= 1024);
    CHECK(n % 512 == 0);
    t_note("empty docfile size = %ld", n);
    if (n >= 512) {
        CHECK(!memcmp(b, sco_sig, 8));
        BYTE z[16] = {0}; CHECK(!memcmp(b + 8, z, 16));                  /* header CLSID is zero */
        t_note("minor=0x%x major=%u", sco_u16(b + 0x18), sco_u16(b + 0x1A));
        CHECK(sco_u16(b + 0x1A) == 3);
        CHECK(sco_u16(b + 0x1C) == 0xFFFE);
        CHECK(sco_u16(b + 0x1E) == 9);
        CHECK(sco_u16(b + 0x20) == 6);
        BYTE z6[6] = {0}; CHECK(!memcmp(b + 0x22, z6, 6));               /* reserved */
        CHECK(sco_u32(b + 0x28) == 0);                                   /* dir sectors: 0 for v3 */
        CHECK(sco_u32(b + 0x2C) >= 1);                                   /* FAT sectors */
        CHECK(sco_u32(b + 0x30) < (unsigned long)(n / 512));             /* first dir sector in file */
        CHECK(sco_u32(b + 0x38) == 4096);                                /* mini cutoff */
        CHECK(sco_u32(b + 0x44) == CFB_ENDOFCHAIN);                      /* no DIFAT */
        CHECK(sco_u32(b + 0x48) == 0);
        unsigned long nf = sco_u32(b + 0x2C);
        for (unsigned long i = 0; i < nf && i < 109; i++) CHECKF(sco_u32(b + 0x4C + 4 * i) < (unsigned long)(n / 512), "DIFAT[%lu] out of file", i);
        for (unsigned long i = nf; i < 109; i++) CHECKF(sco_u32(b + 0x4C + 4 * i) == CFB_FREESECT, "DIFAT[%lu] not free", i);
        t_note("minifat start=0x%lx count=%lu", sco_u32(b + 0x3C), sco_u32(b + 0x40));
        t_note("transaction signature=0x%lx", sco_u32(b + 0x34));
    }
    free(b); sco_rm(p);
}

TEST(ondisk_empty_docfile_directory) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_dir.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK); sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        CHECK(cfb_nentries(&c) >= 4);
        const BYTE *r = cfb_ent(&c, 0);
        WCHAR nm[33]; cfb_ent_name(&c, 0, nm);
        CHECK(!lstrcmpW(nm, L"Root Entry"));
        CHECK(sco_u16(r + 0x40) == 22);
        CHECK(r[0x42] == 5);                                   /* root storage object */
        CHECK(r[0x43] == 0 || r[0x43] == 1);
        CHECK(sco_u32(r + 0x44) == CFB_NOSTREAM);              /* root has no siblings */
        CHECK(sco_u32(r + 0x48) == CFB_NOSTREAM);
        CHECK(sco_u32(r + 0x4C) == CFB_NOSTREAM);              /* nothing created yet */
        BYTE z[16] = {0}; CHECK(!memcmp(r + 0x50, z, 16));
        t_note("root entry: color=%u state=0x%lx ctime=%llx mtime=%llx start=0x%lx size=%lu", r[0x43], sco_u32(r + 0x60),
               (unsigned long long)(sco_u32(r + 0x64) | ((ULONGLONG)sco_u32(r + 0x68) << 32)),
               (unsigned long long)(sco_u32(r + 0x6C) | ((ULONGLONG)sco_u32(r + 0x70) << 32)), sco_u32(r + 0x74), sco_u32(r + 0x78));
        for (unsigned i = 1; i < cfb_nentries(&c); i++) {
            const BYTE *e = cfb_ent(&c, i);
            CHECKF(e[0x42] == 0, "entry %u in empty docfile has type %u", i, e[0x42]);
            if (e[0x42] == 0 && !(sco_u32(e + 0x44) == CFB_NOSTREAM && sco_u32(e + 0x48) == CFB_NOSTREAM && sco_u32(e + 0x4C) == CFB_NOSTREAM))
                t_note("free entry %u has sibling/child ids %lx/%lx/%lx (spec: NOSTREAM)", i, sco_u32(e + 0x44), sco_u32(e + 0x48), sco_u32(e + 0x4C));
        }
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

static int name_cmp(const WCHAR *a, const WCHAR *b) {   /* [MS-CFB] 2.6.4: length first, then uppercase code unit compare */
    size_t la = wcslen(a), lb = wcslen(b);
    if (la != lb) return la < lb ? -1 : 1;
    for (size_t i = 0; i < la; i++) { WCHAR x = (WCHAR)towupper(a[i]), y = (WCHAR)towupper(b[i]); if (x != y) return x < y ? -1 : 1; }
    return 0;
}
static void walk(cfb_t *c, DWORD id, WCHAR out[][33], unsigned *n, unsigned depth) {
    if (id == CFB_NOSTREAM || id >= cfb_nentries(c) || depth > 64 || *n > 100) return;
    const BYTE *e = cfb_ent(c, id);
    walk(c, sco_u32(e + 0x44), out, n, depth + 1);
    cfb_ent_name(c, id, out[*n]); (*n)++;
    walk(c, sco_u32(e + 0x48), out, n, depth + 1);
}

TEST(ondisk_directory_tree_order) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_tree.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    const WCHAR *names[] = { L"zeta", L"Alpha", L"bb", L"a", L"CCCCC", L"mid", L"Mid2", L"x", L"y", L"AB" };
    for (unsigned i = 0; i < SCO_ARRAY(names); i++) EQ(sco_put_stream(s, names[i], "d", 1), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        CHECK(cfb_nentries(&c) >= 12);
        WCHAR out[128][33]; unsigned n = 0;
        walk(&c, sco_u32(cfb_ent(&c, 0) + 0x4C), out, &n, 0);
        EQ(n, SCO_ARRAY(names));
        for (unsigned i = 1; i < n; i++) CHECKF(name_cmp(out[i - 1], out[i]) < 0, "in-order walk not sorted at %u (%ls, %ls)", i, out[i - 1], out[i]);
        for (unsigned i = 0; i < SCO_ARRAY(names); i++) CHECKF(cfb_find(&c, names[i]) > 0, "%ls not in directory", names[i]);
        /* each 512-byte sector holds 4 entries */
        CHECK(c.dirbytes % 512 == 0);
        CHECK(c.dirbytes >= 3 * 512);
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

TEST(ondisk_stream_sizes_mini_and_regular) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_mini.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    static const struct { const WCHAR *n; ULONG sz; } sp[] = {
        { L"s0", 0 }, { L"s1", 1 }, { L"s63", 63 }, { L"s64", 64 }, { L"s65", 65 }, { L"s511", 511 }, { L"s512", 512 },
        { L"s4095", 4095 }, { L"s4096", 4096 }, { L"s4097", 4097 }, { L"s10000", 10000 },
    };
    for (unsigned i = 0; i < SCO_ARRAY(sp); i++) {
        BYTE *d = (BYTE *)malloc(sp[i].sz + 1);
        for (ULONG k = 0; k < sp[i].sz; k++) d[k] = (BYTE)(k * 31 + i);
        EQ(sco_put_stream(s, sp[i].n, d, sp[i].sz), S_OK);
        free(d);
    }
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        for (unsigned i = 0; i < SCO_ARRAY(sp); i++) {
            int e = cfb_find(&c, sp[i].n);
            CHECKF(e > 0, "%ls missing", sp[i].n);
            if (e <= 0) continue;
            ULONGLONG sz = 0; BYTE *d = cfb_stream(&c, e, &sz);
            CHECKF(sz == sp[i].sz, "%ls: size %llu", sp[i].n, (unsigned long long)sz);
            CHECKF(d != NULL, "%ls: could not read via %s", sp[i].n, sp[i].sz < 4096 ? "mini stream" : "FAT");
            if (d) {
                int ok = 1; for (ULONG k = 0; k < sp[i].sz; k++) if (d[k] != (BYTE)(k * 31 + i)) { ok = 0; break; }
                CHECKF(ok, "%ls: content mismatch", sp[i].n);
            }
            free(d);
            const BYTE *en = cfb_ent(&c, e);
            CHECK(en[0x42] == 2);
            if (sp[i].sz == 0) t_note("empty stream start sector = 0x%lx", sco_u32(en + 0x74));
        }
        /* mini stream container: root entry size is a multiple of 64 and covers all small streams */
        unsigned long rsz = sco_u32(cfb_ent(&c, 0) + 0x78);
        CHECK(rsz % 64 == 0);
        CHECK(rsz >= 1 + 63 + 64 + 65 + 511 + 512 + 4095 - 63 * 7);
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

TEST(ondisk_file_growth_and_alignment) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_grow.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    LONGLONG prev = 0;
    for (int step = 1; step <= 4; step++) {
        WCHAR nm[16]; wsprintfW(nm, L"g%d", step);
        BYTE *d = (BYTE *)malloc(100000); memset(d, step, 100000);
        EQ(sco_put_stream(s, nm, d, 100000), S_OK);
        free(d);
        EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
        LONGLONG sz = sco_size(p);
        CHECKF(sz % 512 == 0, "size %lld not sector aligned", sz);
        CHECKF(sz >= 100000LL * step, "size %lld too small for %d streams", sz, step);
        CHECKF(sz < 100000LL * step + 40000, "size %lld has unexpectedly large overhead", sz);
        CHECKF(sz > prev, "size did not grow");
        prev = sz;
    }
    sco_rel(s); sco_rm(p);
}

/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) test_overwrite */
TEST(ondisk_commit_overwrite_does_not_grow_file) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_over.stg"); sco_rm(p);
    IStorage *s; IStream *st;
    EQ(sco_create(p, STGM_CREATE | RW | STGM_TRANSACTED, &s), S_OK);
    EQ(U(WriteClassStg)(s, &sco_CLS_A), S_OK);
    EQ(IStorage_CreateStream(s, L"CONTENTS", EXCL | RW, 0, 0, &st), S_OK);
    BYTE buf[4096]; memset(buf, 'a', sizeof buf);
    for (int i = 0; i < 4; i++) EQ(IStream_Write(st, buf, sizeof buf, NULL), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    LONGLONG orig = sco_size(p);
    LARGE_INTEGER z; z.QuadPart = 0;
    EQ(IStream_Seek(st, z, STREAM_SEEK_SET, NULL), S_OK);
    EQ(IStream_Write(st, "b", 1, NULL), S_OK);
    EQ(IStorage_Commit(s, STGC_OVERWRITE), S_OK);
    LONGLONG now = sco_size(p);
    CHECKF(now == orig, "file grew from %lld to %lld", orig, now);
    sco_relst(st);
    EQ(IStorage_RenameElement(s, L"CONTENTS", L"CONTENT2"), S_OK);
    EQ(IStorage_Commit(s, STGC_OVERWRITE), S_OK);
    now = sco_size(p);
    CHECKF(now == orig, "file grew from %lld to %lld after rename", orig, now);
    sco_rel(s); sco_rm(p);
}

TEST(ondisk_transacted_uncommitted_not_on_disk) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_tx.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL | STGM_TRANSACTED, &s), S_OK);
    EQ(sco_put_stream(s, L"pending", "xyz", 3), S_OK);
    cfb_t c; BYTE *b;
    int ok = load(p, &c, &b);
    t_note("transacted, uncommitted: file parses=%d size=%lld", ok, sco_size(p));
    if (ok) { CHECKF(cfb_find(&c, L"pending") < 0, "uncommitted stream visible on disk"); cfb_free(&c); }
    free(b);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    CHECK(load(p, &c, &b));
    if (c.ok) { CHECK(cfb_find(&c, L"pending") > 0); cfb_free(&c); }
    free(b);
    sco_rel(s); sco_rm(p);
}

TEST(ondisk_difat_for_large_file) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_difat.stg"); sco_rm(p);
    IStorage *s; IStream *st; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStream(s, L"big", EXCL | RW, 0, 0, &st), S_OK);
    static BYTE chunk[65536];
    for (unsigned i = 0; i < sizeof chunk; i++) chunk[i] = (BYTE)(i ^ (i >> 8));
    /* 109 FAT sectors map 109*128 sectors = ~7.1 MB; write 8 MB to force a DIFAT sector */
    for (int i = 0; i < 128; i++) { ULONG w = 0; HRESULT hr = IStream_Write(st, chunk, sizeof chunk, &w); if (hr != S_OK || w != sizeof chunk) { CHECK(0); break; } }
    sco_relst(st);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    EQ(sco_rel(s), 0);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        CHECK(sco_u32(b + 0x2C) > 109);
        CHECK(sco_u32(b + 0x48) >= 1);
        CHECK(sco_u32(b + 0x44) != CFB_ENDOFCHAIN);
        int e = cfb_find(&c, L"big");
        CHECK(e > 0);
        if (e > 0) {
            ULONGLONG sz; BYTE *d = cfb_stream(&c, e, &sz);
            CHECK(sz == 8u * 1024 * 1024);
            CHECK(d != NULL);
            if (d) { int ok = 1; for (unsigned long long i = 0; i < sz && ok; i++) if (d[i] != (BYTE)(((unsigned)(i & 65535)) ^ ((unsigned)(i & 65535) >> 8))) ok = 0; CHECK(ok); }
            free(d);
        }
        cfb_free(&c);
    }
    free(b);
    /* and ole32 can read it back */
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    EQ(IStorage_OpenStream(s, L"big", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    STATSTG ss; EQ(IStream_Stat(st, &ss, STATFLAG_NONAME), S_OK); CHECK(ss.cbSize.QuadPart == 8ull * 1024 * 1024);
    sco_relst(st); sco_rel(s); sco_rm(p);
}

TEST(ondisk_substorage_entry) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_sub.stg"); sco_rm(p);
    IStorage *s, *sub; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(IStorage_CreateStorage(s, L"Child", EXCL | RW, 0, 0, &sub), S_OK);
    EQ(IStorage_SetClass(sub, &sco_CLS_B), S_OK);
    EQ(sco_put_stream(sub, L"inner", "12345", 5), S_OK);
    EQ(IStorage_SetClass(s, &sco_CLS_A), S_OK);
    EQ(IStorage_SetStateBits(sub, 0x3, 0xF), S_OK);
    sco_rel(sub);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        int e = cfb_find(&c, L"Child");
        CHECK(e > 0);
        if (e > 0) {
            const BYTE *en = cfb_ent(&c, e);
            CHECK(en[0x42] == 1);                                   /* storage object */
            CHECK(!memcmp(en + 0x50, &sco_CLS_B, 16));
            CHECK(sco_u32(en + 0x4C) != CFB_NOSTREAM);              /* has a child */
            CHECK(sco_u32(en + 0x78) == 0);                         /* storages have no size */
            t_note("sub-storage state bits on disk = 0x%lx (set 0x3)", sco_u32(en + 0x60));
        }
        CHECK(!memcmp(cfb_ent(&c, 0) + 0x50, &sco_CLS_A, 16));
        int in = cfb_find(&c, L"inner");
        CHECK(in > 0);
        if (in > 0) { ULONGLONG sz; BYTE *d = cfb_stream(&c, in, &sz); CHECK(sz == 5 && d && !memcmp(d, "12345", 5)); free(d); }
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

TEST(ondisk_destroy_element_frees_entry) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_destroy.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    EQ(sco_put_stream(s, L"gone", "abc", 3), S_OK);
    EQ(sco_put_stream(s, L"stay", "def", 3), S_OK);
    EQ(IStorage_DestroyElement(s, L"gone"), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        CHECK(cfb_find(&c, L"gone") < 0);
        CHECK(cfb_find(&c, L"stay") > 0);
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

TEST(ondisk_sector_chains_consistent) {
    REQUIRE(U(StgCreateDocfile));
    WCHAR p[MAX_PATH]; sco_wpath(p, "od_chain.stg"); sco_rm(p);
    IStorage *s; EQ(sco_create(p, SCO_RW_EXCL, &s), S_OK);
    BYTE d[20000]; memset(d, 7, sizeof d);
    EQ(sco_put_stream(s, L"a", d, 20000), S_OK);
    EQ(sco_put_stream(s, L"b", d, 9000), S_OK);
    EQ(sco_put_stream(s, L"c", d, 30), S_OK);
    EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
    sco_rel(s);
    cfb_t c; BYTE *b;
    CHECK(load(p, &c, &b));
    if (c.ok) {
        /* every sector is either free, a FAT sector, or on exactly one chain; chains of big streams have ceil(size/512) sectors */
        unsigned nsec = (unsigned)(c.n / 512) - 1;
        unsigned used = 0, bad = 0;
        for (unsigned i = 0; i < nsec && i < c.nfat; i++) if (c.fat[i] != CFB_FREESECT) used++;
        for (unsigned i = nsec; i < c.nfat; i++) if (c.fat[i] != CFB_FREESECT) bad++;
        CHECKF(bad == 0, "%u FAT entries beyond the end of the file are in use", bad);
        t_note("sectors in file=%u used=%u", nsec, used);
        int ea = cfb_find(&c, L"a"), eb = cfb_find(&c, L"b");
        if (ea > 0) CHECK(cfb_chain_len(&c, sco_u32(cfb_ent(&c, ea) + 0x74)) == (20000 + 511) / 512);
        if (eb > 0) CHECK(cfb_chain_len(&c, sco_u32(cfb_ent(&c, eb) + 0x74)) == (9000 + 511) / 512);
        CHECK(cfb_chain_len(&c, sco_u32(c.b + 0x30)) == c.dirbytes / 512);
        cfb_free(&c);
    }
    free(b); sco_rm(p);
}

/* ---- reading hand-crafted [MS-CFB] files ---------------------------------------------------------- */
TEST(ondisk_open_handcrafted_empty) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "hc_empty.stg"); sco_rm(p);
    cfb_ent_t e[1] = { { L"Root Entry", 5, CFB_NOSTREAM, CFB_NOSTREAM, CFB_NOSTREAM, CFB_ENDOFCHAIN, 0, NULL, 0 } };
    LONG n; BYTE *b = cfb_build(&n, e, 1, CFB_ENDOFCHAIN, 0, NULL, 0, NULL);
    CHECK(sco_write_file(p, b, n)); free(b);
    EQ(U(StgIsStorageFile)(p), S_OK);
    IStorage *s;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    if (s) {
        IEnumSTATSTG *en = NULL; STATSTG st; ULONG got = 5;
        EQ(IStorage_EnumElements(s, 0, NULL, 0, &en), S_OK);
        HRESULT hr = IEnumSTATSTG_Next(en, 1, &st, &got);
        EQ(hr, S_FALSE); EQ(got, 0);
        IEnumSTATSTG_Release(en);
        memset(&st, 0xcc, sizeof st);
        EQ(IStorage_Stat(s, &st, STATFLAG_NONAME), S_OK);
        CHECK(sco_guid_eq(&st.clsid, &sco_CLSID_NULL));
        sco_rel(s);
    }
    EQ(sco_open(p, EXCL | RW, &s), S_OK);
    if (s) {
        EQ(sco_put_stream(s, L"added", "ok", 2), S_OK);
        EQ(IStorage_Commit(s, STGC_DEFAULT), S_OK);
        sco_rel(s);
        cfb_t c; BYTE *bb; CHECK(load(p, &c, &bb));
        if (c.ok) { CHECK(cfb_find(&c, L"added") > 0); cfb_free(&c); }
        free(bb);
    }
    sco_rm(p);
}

TEST(ondisk_open_handcrafted_clsid_and_regular_stream) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "hc_stream.stg"); sco_rm(p);
    /* stream "data" of 4096 bytes in sectors 2..9 */
    BYTE data[4096]; for (int i = 0; i < 4096; i++) data[i] = (BYTE)(i * 3 + 1);
    DWORD fat[8]; for (int i = 0; i < 7; i++) fat[i] = 3 + i; fat[7] = CFB_ENDOFCHAIN;
    cfb_ent_t e[2] = {
        { L"Root Entry", 5, CFB_NOSTREAM, CFB_NOSTREAM, 1, CFB_ENDOFCHAIN, 0, &sco_CLS_A, 0 },
        { L"data", 2, CFB_NOSTREAM, CFB_NOSTREAM, CFB_NOSTREAM, 2, 4096, NULL, 0 },
    };
    LONG n; BYTE *b = cfb_build(&n, e, 2, CFB_ENDOFCHAIN, 0, fat, 8, data);
    CHECK(sco_write_file(p, b, n)); free(b);
    IStorage *s; IStream *st;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    if (!s) { sco_rm(p); return; }
    STATSTG ss; EQ(IStorage_Stat(s, &ss, STATFLAG_NONAME), S_OK);
    CHECK(sco_guid_eq(&ss.clsid, &sco_CLS_A));
    EQ(IStorage_OpenStream(s, L"data", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    if (st) {
        BYTE out[4200]; ULONG got = 0;
        EQ(IStream_Read(st, out, sizeof out, &got), S_OK);
        EQ(got, 4096);
        CHECK(!memcmp(out, data, 4096));
        EQ(IStream_Stat(st, &ss, STATFLAG_NONAME), S_OK); EQ(ss.cbSize.QuadPart, 4096);
        sco_relst(st);
    }
    /* element names are case insensitive */
    HRESULT hr = IStorage_OpenStream(s, L"DATA", NULL, STGM_READ | EXCL, 0, &st);
    EQ(hr, S_OK); sco_relst(st);
    sco_rel(s); sco_rm(p);
}

TEST(ondisk_open_handcrafted_mini_stream) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "hc_mini.stg"); sco_rm(p);
    /* sector 2: mini stream container (512 bytes); sector 3: mini FAT.  stream "m" = 100 bytes at mini sectors 0,1 */
    BYTE extra[1024]; memset(extra, 0, sizeof extra);
    for (int i = 0; i < 100; i++) extra[i] = (BYTE)(200 - i);
    BYTE *mf = extra + 512;
    for (int i = 0; i < 128; i++) cfb_put32(mf + 4 * i, CFB_FREESECT);
    cfb_put32(mf + 0, 1); cfb_put32(mf + 4, CFB_ENDOFCHAIN);
    DWORD fat[2] = { CFB_ENDOFCHAIN, CFB_ENDOFCHAIN };
    cfb_ent_t e[2] = {
        { L"Root Entry", 5, CFB_NOSTREAM, CFB_NOSTREAM, 1, 2, 512, NULL, 0 },
        { L"m", 2, CFB_NOSTREAM, CFB_NOSTREAM, CFB_NOSTREAM, 0, 100, NULL, 0 },
    };
    LONG n; BYTE *b = cfb_build(&n, e, 2, 3, 1, fat, 2, extra);
    CHECK(sco_write_file(p, b, n)); free(b);
    IStorage *s; IStream *st;
    EQ(sco_open(p, EXCL | STGM_READ, &s), S_OK);
    if (!s) { sco_rm(p); return; }
    EQ(IStorage_OpenStream(s, L"m", NULL, STGM_READ | EXCL, 0, &st), S_OK);
    if (st) {
        BYTE out[200]; ULONG got = 0;
        EQ(IStream_Read(st, out, sizeof out, &got), S_OK); EQ(got, 100);
        int ok = 1; for (int i = 0; i < 100; i++) if (out[i] != (BYTE)(200 - i)) ok = 0;
        CHECK(ok);
        sco_relst(st);
    }
    sco_rel(s); sco_rm(p);
}

TEST(ondisk_open_handcrafted_corruptions) {
    REQUIRE(U(StgOpenStorage));
    WCHAR p[MAX_PATH]; sco_wpath(p, "hc_bad.stg"); sco_rm(p);
    cfb_ent_t e[1] = { { L"Root Entry", 5, CFB_NOSTREAM, CFB_NOSTREAM, CFB_NOSTREAM, CFB_ENDOFCHAIN, 0, NULL, 0 } };
    LONG n; BYTE *good = cfb_build(&n, e, 1, CFB_ENDOFCHAIN, 0, NULL, 0, NULL);
    struct { int off; BYTE v; const char *what; int must_fail; } c[] = {
        { 0, 0xD1, "signature byte 0", 1 }, { 7, 0xE0, "signature byte 7", 1 },
        { 0x1E, 0x08, "sector shift 8", 0 }, { 0x20, 0x07, "mini shift 7", 0 }, { 0x2C, 0x00, "zero FAT sectors", 0 },
        { 0x30, 0x40, "dir start beyond file", 0 }, { 0x4C, 0x40, "DIFAT[0] beyond file", 0 }, { 0x1C, 0xFF, "byte order 0xFFFF", 0 },
        { 0x1A, 0x09, "major version 9", 0 }, { 0x38, 0x00, "cutoff low byte 0", 0 },
    };
    for (unsigned i = 0; i < SCO_ARRAY(c); i++) {
        BYTE *m = (BYTE *)malloc(n); memcpy(m, good, n); m[c[i].off] = c[i].v;
        CHECK(sco_write_file(p, m, n)); free(m);
        IStorage *s = NULL;
        HRESULT hr = sco_open(p, EXCL | STGM_READ, &s);
        if (c[i].must_fail) CHECKF(FAILED(hr), "%s: open succeeded", c[i].what);
        t_note("open crafted file (%s) -> 0x%08lx", c[i].what, hr);
        sco_rel(s);
    }
    /* chain loop in directory: dir sector chain points to itself */
    if (t_is_wine()) { free(good); sco_rm(p); t_skip("directory chain loop crashes wine"); return; }
    BYTE *m = (BYTE *)malloc(n); memcpy(m, good, n);
    cfb_put32(m + 512 + 4, 1);       /* FAT[1] = 1 : directory chain loops */
    CHECK(sco_write_file(p, m, n)); free(m);
    IStorage *s = NULL;
    HRESULT hr = sco_open(p, EXCL | STGM_READ, &s);
    t_note("open crafted file (directory chain loop) -> 0x%08lx", hr);
    sco_rel(s);
    free(good);
    sco_rm(p);
}
