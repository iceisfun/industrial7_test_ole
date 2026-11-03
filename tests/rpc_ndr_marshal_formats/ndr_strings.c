/* NDR strings: conformant (FC_C_CSTRING/FC_C_WSTRING), sized, non-conformant (FC_CSTRING/FC_WSTRING). */
#include "ndrh.h"
#define W32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

static const fam *cfam(void) { static fam f; f = (fam)FAM_NOFREE(NdrConformantString); return fam_ok(&f) ? &f : NULL; }
static const fam *nfam(void) { static fam f; f = (fam)FAM_NOFREE(NdrNonConformantString); return fam_ok(&f) ? &f : NULL; }

static const char *g_str[] = { "", "a", "ab", "abc", "abcd", "abcde", "This is a test string", "hello\x80\xff high bytes" };
#define NSTR ((int)(sizeof g_str / sizeof g_str[0]))
static const unsigned char fC[] = { FC_C_CSTRING, FC_PAD };
static const unsigned char fW[] = { FC_C_WSTRING, FC_PAD };

static int wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }
static const WCHAR g_wstr[][24] = { { 0 }, { 'a', 0 }, { 'a', 'b', 0 }, { 'a', 'b', 'c', 0 }, { 0x263a, 0xd83d, 0xde00, 0 }, { 'T','h','i','s',' ','i','s',' ','a',' ','t','e','s','t',0 } };
#define NWSTR ((int)(sizeof g_wstr / sizeof g_wstr[0]))

/* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
TEST(ndr_cstr_marshal_wire_layout) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NSTR; i++) {
        unsigned n = (unsigned)strlen(g_str[i]) + 1;
        nctx c; nc_client(&c, fC, 128); memset(c.buf, 0xCD, 128);
        ULONG pred; ULONG used = n_marshal(f, &c, (unsigned char *)g_str[i], fC, &pred);
        CHECKF(used == 12 + n, "'%s': used %lu want %u", g_str[i], used, 12 + n);
        CHECKF(pred >= 12 + n, "'%s': pred %lu", g_str[i], pred);
        CHECKF(*(unsigned *)c.buf == n && *(unsigned *)(c.buf + 4) == 0 && *(unsigned *)(c.buf + 8) == n, "'%s': header %u %u %u", g_str[i],
               *(unsigned *)c.buf, *(unsigned *)(c.buf + 4), *(unsigned *)(c.buf + 8));
        CHECKF(!memcmp(c.buf + 12, g_str[i], n), "'%s': data", g_str[i]);
        nc_done(&c);
    }
}
TEST(ndr_cstr_marshal_zeroes_final_dword_padding) {
    /* decomp: NdrpConformantStringMarshall clears the last (partial) dword before copying the characters */
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 1; i < 4; i++) {
        char s[8] = "xyz"; s[i] = 0;
        unsigned n = i + 1;
        nctx c; nc_client(&c, fC, 64);
        n_marshal(f, &c, (unsigned char *)s, fC, NULL);
        unsigned pad = ((n + 3) & ~3u) - n;
        for (unsigned k = 0; k < pad; k++) CHECKF(c.buf[12 + n + k] == 0, "len %u pad byte %u = 0x%02x", n, k, c.buf[12 + n + k]);
        EXPECT_EQ(nc_used(&c), 12 + n);
        nc_done(&c);
    }
}
TEST(ndr_cstr_marshal_misaligned_start) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int off = 0; off < 4; off++) {
        nctx c; nc_client(&c, fC, 128);
        c.sm.Buffer = c.buf + off;
        f->ma(&c.sm, (unsigned char *)"hello", fC);
        ULONG_PTR st = ((ULONG_PTR)c.buf + off + 3) & ~3ull;
        CHECKF(*(unsigned *)st == 6 && *(unsigned *)(st + 8) == 6 && !memcmp((void *)(st + 12), "hello", 6) && (ULONG_PTR)c.sm.Buffer == st + 18, "off %d", off);
        nc_done(&c);
    }
}
TEST(ndr_cstr_memorysize_is_max_count) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NSTR; i++) {
        unsigned n = (unsigned)strlen(g_str[i]) + 1;
        unsigned char w[12 + 32] = { W32(n), W32(0), W32(n) }; memcpy(w + 12, g_str[i], n);
        nctx c; nc_rx(&c, fC, w, 12 + n, 1);
        c.sm.MemorySize = 0;
        ULONG r = sa_ms(f, &c, fC, g_str[i]);
        CHECKF(r == n, "'%s': memsize %lu want %u", g_str[i], r, n);
        CHECKF(nc_used(&c) == 12 + n, "'%s': consumed %lu", g_str[i], nc_used(&c));
        nc_done(&c);
    }
}
TEST(ndr_cstr_unmarshal_server_points_into_buffer) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NSTR; i++) {
        unsigned n = (unsigned)strlen(g_str[i]) + 1;
        unsigned char w[12 + 32] = { W32(n), W32(0), W32(n) }; memcpy(w + 12, g_str[i], n);
        nctx c; nc_rx(&c, fC, w, 12 + n, 0);
        unsigned char *p = NULL; n_alloc_cnt = 0;
        sa_um(f, &c, &p, fC, 0, g_str[i]);
        CHECKF(p == c.buf + 12, "'%s': p=%p want %p", g_str[i], p, c.buf + 12);
        CHECKF(n_alloc_cnt == 0, "'%s': allocs", g_str[i]);
        CHECKF(nc_used(&c) == 12 + n, "'%s': consumed", g_str[i]);
        nc_done(&c);
    }
}
TEST(ndr_cstr_unmarshal_must_alloc_copies) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NSTR; i++) {
        unsigned n = (unsigned)strlen(g_str[i]) + 1;
        unsigned char w[12 + 32] = { W32(n), W32(0), W32(n) }; memcpy(w + 12, g_str[i], n);
        for (int client = 0; client < 2; client++) {
            nctx c; nc_rx(&c, fC, w, 12 + n, client);
            unsigned char *p = NULL; n_alloc_cnt = 0;
            sa_um(f, &c, &p, fC, 1, g_str[i]);
            CHECKF(p && (p < c.buf || p >= c.buf + c.cap), "'%s' client=%d: not a copy", g_str[i], client);
            CHECKF(p && !memcmp(p, g_str[i], n), "'%s' client=%d: data", g_str[i], client);
            CHECKF(n_alloc_cnt == 1, "'%s' client=%d: allocs %d", g_str[i], client, n_alloc_cnt);
            if (p) n_free(p);
            nc_done(&c);
        }
    }
}
TEST(ndr_cstr_unmarshal_client_into_caller_buffer) {
    /* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+): client copies into existing storage */
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    unsigned char w[12 + 8] = { W32(6), W32(0), W32(6), 'h', 'e', 'l', 'l', 'o', 0 };
    nctx c; nc_rx(&c, fC, w, 18, 1);
    char dst[16]; memset(dst, 'x', 5); dst[5] = 0;   /* Windows inspects the existing string to learn the capacity */
    unsigned char *p = (unsigned char *)dst; n_alloc_cnt = 0;
    sa_um(f, &c, &p, fC, 0, __func__);
    CHECK(p == (unsigned char *)dst);
    CHECK(!strcmp(dst, "hello"));
    EXPECT_EQ(n_alloc_cnt, 0);
    nc_done(&c);
}
TEST(ndr_cstr_unmarshal_errors_raise_invalid_bound) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const struct { unsigned char w[16]; unsigned len; DWORD code; const char *why; } t[] = {
        { { W32(4), W32(1), W32(3), 'a', 'b', 0 }, 15, X_INVALID_BOUND, "offset != 0" },
        { { W32(0), W32(0), W32(0) }, 12, X_INVALID_BOUND, "actual == 0" },
        { { W32(3), W32(0), W32(4), 'a', 'b', 'c', 0 }, 16, X_INVALID_BOUND, "actual > max" },
        { { W32(4), W32(0), W32(3), 'a', 'b', 0 }, 15, X_INVALID_BOUND, "max != actual (unsized)" },
        { { W32(3), W32(0), W32(3), 'a', 'b', 'c' }, 15, X_INVALID_BOUND, "no terminating NUL" },
        { { W32(3), W32(0), W32(3), 'a', 'b' }, 14, X_INVALID_BOUND, "data truncated" },
        { { W32(3), W32(0), W32(3), 'a', 'b', 0 }, 15, 0, "control: valid" },
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        nctx c; nc_rx(&c, fC, t[k].w, t[k].len, 0);
        unsigned char *p = NULL;
        NTRY f->um(&c.sm, &p, fC, 0); NEND;
        CHECKF(n_code == t[k].code, "%s: raised %lu want %lu", t[k].why, (unsigned long)n_code, (unsigned long)t[k].code);
        nc_done(&c);
    }
}
TEST(ndr_cstr_unmarshal_truncated_header_raises_bad_stub_data) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    unsigned char w[12] = { W32(1), W32(0), W32(1) };
    for (unsigned len = 0; len < 12; len += 4) {
        nctx c; nc_rx(&c, fC, w, len, 0);
        unsigned char *p = NULL;
        NTRY f->um(&c.sm, &p, fC, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu", len, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(ndr_cstr_unmarshal_accepts_embedded_string_shorter_than_buffer_end) {
    /* trailing bytes after the string are left unconsumed */
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    unsigned char w[24] = { W32(2), W32(0), W32(2), 'a', 0, 0, 0, 0xAA, 0xAA, 0xAA, 0xAA };
    nctx c; nc_rx(&c, fC, w, 24, 0);
    unsigned char *p = NULL;
    sa_um(f, &c, &p, fC, 0, __func__);
    EXPECT_EQ(nc_used(&c), 14);
    nc_done(&c);
}

/* ---- wide ---- */
TEST(ndr_wstr_marshal_wire_layout) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NWSTR; i++) {
        unsigned n = wlen(g_wstr[i]) + 1;
        nctx c; nc_client(&c, fW, 128); memset(c.buf, 0, 128);
        ULONG pred; ULONG used = n_marshal(f, &c, (unsigned char *)g_wstr[i], fW, &pred);
        CHECKF(used == 12 + 2 * n, "#%d: used %lu want %u", i, used, 12 + 2 * n);
        CHECKF(pred >= 12 + 2 * n, "#%d: pred", i);
        CHECKF(*(unsigned *)c.buf == n && *(unsigned *)(c.buf + 4) == 0 && *(unsigned *)(c.buf + 8) == n, "#%d header", i);
        CHECKF(!memcmp(c.buf + 12, g_wstr[i], 2 * n), "#%d data", i);
        nc_done(&c);
    }
}
TEST(ndr_wstr_memorysize_counts_bytes) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NWSTR; i++) {
        unsigned n = wlen(g_wstr[i]) + 1;
        unsigned char w[12 + 64] = { W32(n), W32(0), W32(n) }; memcpy(w + 12, g_wstr[i], 2 * n);
        nctx c; nc_rx(&c, fW, w, 12 + 2 * n, 1);
        c.sm.MemorySize = 0;
        ULONG r = sa_ms(f, &c, fW, __func__);
        CHECKF(r == 2 * n, "#%d: memsize %lu want %u", i, r, 2 * n);
        CHECKF(nc_used(&c) == 12 + 2 * n, "#%d: consumed", i);
        nc_done(&c);
    }
}
TEST(ndr_wstr_unmarshal_server_points_into_buffer) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    for (int i = 0; i < NWSTR; i++) {
        unsigned n = wlen(g_wstr[i]) + 1;
        unsigned char w[12 + 64] = { W32(n), W32(0), W32(n) }; memcpy(w + 12, g_wstr[i], 2 * n);
        nctx c; nc_rx(&c, fW, w, 12 + 2 * n, 0);
        unsigned char *p = NULL;
        sa_um(f, &c, &p, fW, 0, __func__);
        CHECKF(p == c.buf + 12, "#%d pointer", i);
        nc_done(&c);
    }
}
TEST(ndr_wstr_unmarshal_errors_raise_invalid_bound) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const struct { unsigned char w[24]; unsigned len; DWORD code; const char *why; } t[] = {
        { { W32(2), W32(0), W32(2), 'a', 0, 'b', 0 }, 18, X_INVALID_BOUND, "wide: no terminating NUL" },
        { { W32(2), W32(0), W32(2), 'a', 0 }, 14, X_INVALID_BOUND, "wide: data truncated" },
        { { W32(2), W32(0), W32(3), 'a', 0, 'b', 0, 0, 0 }, 22, X_INVALID_BOUND, "wide: actual > max" },
        { { W32(2), W32(0), W32(2), 'a', 0, 0, 0 }, 18, 0, "wide: control valid" },
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        nctx c; nc_rx(&c, fW, t[k].w, t[k].len, 0);
        unsigned char *p = NULL;
        NTRY f->um(&c.sm, &p, fW, 0); NEND;
        CHECKF(n_code == t[k].code, "%s: raised %lu want %lu", t[k].why, (unsigned long)n_code, (unsigned long)t[k].code);
        nc_done(&c);
    }
}

/* ---- via pointers (the way MIDL emits them) ---- */
TEST(ndr_cstr_through_ref_pointer) {
    /* adapted from Wine dlls/rpcrt4/tests/ndr_marshall.c (LGPL-2.1+) */
    REQUIRE(U(NdrPointerBufferSize)); REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char f[] = { FC_RP, 0x08, FC_C_CSTRING, FC_PAD };
    char msg[] = "This is a test string";
    nctx c; nc_client(&c, f, 256);
    c.sm.BufferLength = 0;
    U(NdrPointerBufferSize)(&c.sm, (unsigned char *)msg, f);
    CHECKF(c.sm.BufferLength >= sizeof msg + 12, "size %lu", c.sm.BufferLength);
    memset(c.buf, 0, 256);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)msg, f);
    EXPECT_EQ(nc_used(&c), sizeof msg + 12);
    CHECK(!memcmp(c.buf + 12, msg, sizeof msg));
    nc_done(&c);
}
TEST(ndr_cstr_through_unique_pointer) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char f[] = { FC_UP, 0x08, FC_C_CSTRING, FC_PAD };
    nctx c; nc_client(&c, f, 64);
    memset(c.buf, 0, 64);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)"hi", f);
    EXPECT_EQ(nc_used(&c), 4 + 12 + 3);
    EXPECT_EQ(*(unsigned *)c.buf, 0x20000);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 3);
    EXPECT_EQ(*(unsigned *)(c.buf + 12), 3);
    ULONG used = nc_used(&c);
    nc_done(&c);
    nc_client(&c, f, 64);
    memset(c.buf, 0, 64);
    U(NdrPointerMarshall)(&c.sm, NULL, f);
    EXPECT_EQ(nc_used(&c), 4);
    EXPECT_EQ(*(unsigned *)c.buf, 0);
    nc_done(&c);
}
TEST(ndr_cstr_unique_pointer_roundtrip_server) {
    REQUIRE(U(NdrPointerMarshall)); REQUIRE(U(NdrPointerUnmarshall)); REQUIRE(U(NdrServerInitializeNew)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned char f[] = { FC_UP, 0x08, FC_C_CSTRING, FC_PAD };
    nctx c; nc_client(&c, f, 64); memset(c.buf, 0, 64);
    U(NdrPointerMarshall)(&c.sm, (unsigned char *)"round", f);
    ULONG used = nc_used(&c);
    nctx s; nc_rx(&s, f, c.buf, used, 0);
    unsigned char *p = NULL;
    U(NdrPointerUnmarshall)(&s.sm, &p, f, 0);
    CHECK(p && !strcmp((char *)p, "round"));
    EXPECT_EQ(nc_used(&s), used);
    nc_done(&s); nc_done(&c);
}

/* ---- sized (size_is) conformant strings ---- */
TEST(ndr_cstr_sized_const_maxcount) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_CONSTANT_CONFORMANCE, 0, S16(40) };
    nctx c; nc_client(&c, fmt, 128); memset(c.buf, 0, 128);
    ULONG pred; ULONG used = n_marshal(f, &c, (unsigned char *)"short", fmt, &pred);
    CHECKF(used == 12 + 6, "used %lu", used);
    CHECKF(pred >= 18, "pred %lu", pred);
    EXPECT_EQ(*(unsigned *)c.buf, 40);          /* max count from size_is, not the string length */
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0);
    EXPECT_EQ(*(unsigned *)(c.buf + 8), 6);
    CHECK(!memcmp(c.buf + 12, "short", 6));
    nc_done(&c);
}
TEST(ndr_cstr_sized_from_stack) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(8) };
    unsigned stack[4] = { 0, 0, 25, 0 };
    nctx c; nc_client(&c, fmt, 128); memset(c.buf, 0, 128);
    c.sm.StackTop = (unsigned char *)stack;
    n_marshal(f, &c, (unsigned char *)"abc", fmt, NULL);
    EXPECT_EQ(*(unsigned *)c.buf, 25);
    EXPECT_EQ(*(unsigned *)(c.buf + 8), 4);
    nc_done(&c);
}
TEST(ndr_cstr_sized_buffersize_rejects_string_longer_than_size_is) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_CONSTANT_CONFORMANCE, 0, S16(4) };
    nctx c; nc_client(&c, fmt, 16);
    c.sm.BufferLength = 0;
    NTRY f->bs(&c.sm, (unsigned char *)"abcd", fmt); NEND;   /* needs 5 bytes, size_is 4 */
    EXPECT_EQ(n_code, X_INVALID_BOUND);
    c.sm.BufferLength = 0;
    NTRY f->bs(&c.sm, (unsigned char *)"abc", fmt); NEND;    /* 4 bytes fits exactly */
    EXPECT_EQ(n_code, 0);
    nc_done(&c);
}
TEST(ndr_cstr_sized_unmarshal_max_larger_than_actual_ok) {
    const fam *f = cfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_CONSTANT_CONFORMANCE, 0, S16(40) };
    unsigned char w[18] = { W32(40), W32(0), W32(6), 's', 'h', 'o', 'r', 't', 0 };
    nctx c; nc_rx(&c, fmt, w, 18, 0);
    unsigned char *p = NULL;
    NTRY f->um(&c.sm, &p, fmt, 0); NEND;
    EXPECT_EQ(n_code, 0);
    EXPECT_EQ(nc_used(&c), 18);
    CHECK(p && !strcmp((char *)p, "short"));
    nc_done(&c);
}

/* ================= non-conformant strings ================= */
TEST(ndr_ncstr_marshal_wire_layout) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_CSTRING, FC_PAD, S16(10) };
    static const char *s[] = { "", "a", "This is", "123456789" };
    for (int i = 0; i < 4; i++) {
        unsigned n = (unsigned)strlen(s[i]) + 1;
        char mem[10] = { 0 }; strcpy(mem, s[i]);
        nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
        ULONG pred; ULONG used = n_marshal(f, &c, (unsigned char *)mem, fmt, &pred);
        CHECKF(used == 8 + n, "'%s': used %lu want %u", s[i], used, 8 + n);
        CHECKF(pred >= 8 + n, "'%s': pred %lu", s[i], pred);
        CHECKF(*(unsigned *)c.buf == 0 && *(unsigned *)(c.buf + 4) == n, "'%s': header", s[i]);
        CHECKF(!memcmp(c.buf + 8, mem, n), "'%s': data", s[i]);
        nc_done(&c);
    }
}
TEST(ndr_ncstr_wide_marshal_wire_layout) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_WSTRING, FC_PAD, S16(10) };
    WCHAR mem[10] = { 'w', 'i', 'd', 'e', 0 };
    nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
    ULONG used = n_marshal(f, &c, (unsigned char *)mem, fmt, NULL);
    EXPECT_EQ(used, 8 + 10);
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 5);
    CHECK(!memcmp(c.buf + 8, mem, 10));
    nc_done(&c);
}
TEST(ndr_ncstr_unmarshal_client_and_server) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_CSTRING, FC_PAD, S16(10) };
    unsigned char w[16] = { W32(0), W32(8), 'T', 'h', 'i', 's', ' ', 'i', 's', 0 };
    for (int client = 0; client < 2; client++) {
        nctx c; nc_rx(&c, fmt, w, 16, client);
        unsigned char *p = NULL; n_alloc_cnt = 0;
        sa_um(f, &c, &p, fmt, 0, __func__);
        CHECKF(p && !strcmp((char *)p, "This is"), "client=%d value", client);
        CHECKF(nc_used(&c) == 16, "client=%d consumed %lu", client, nc_used(&c));
        if (p && (p < c.buf || p >= c.buf + c.cap)) n_free(p);
        nc_done(&c);
    }
}
TEST(ndr_ncstr_unmarshal_into_existing_memory) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_CSTRING, FC_PAD, S16(10) };
    unsigned char w[16] = { W32(0), W32(3), 'a', 'b', 0 };
    nctx c; nc_rx(&c, fmt, w, 11, 1);
    char dst[10]; memset(dst, 'Z', 10);
    unsigned char *p = (unsigned char *)dst; n_alloc_cnt = 0;
    sa_um(f, &c, &p, fmt, 0, __func__);
    CHECK(p == (unsigned char *)dst);
    CHECK(!strcmp(dst, "ab") && dst[3] == 'Z');
    EXPECT_EQ(n_alloc_cnt, 0);
    nc_done(&c);
}
TEST(ndr_ncstr_unmarshal_errors) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmt[] = { FC_CSTRING, FC_PAD, S16(10) };
    static const struct { unsigned char w[24]; unsigned len; DWORD code; const char *why; } t[] = {
        { { W32(1), W32(2), 'a', 0 }, 11, X_INVALID_BOUND, "offset != 0" },
        { { W32(0), W32(11), 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 0 }, 19, X_INVALID_BOUND, "actual > declared size" },
        { { W32(0), W32(0) }, 8, X_INVALID_BOUND, "actual == 0" },
        { { W32(0), W32(3), 'a', 'b', 'c' }, 11, X_INVALID_BOUND, "missing NUL" },
        { { W32(0), W32(3), 'a', 'b' }, 10, X_BAD_STUB, "truncated data" },
        { { W32(0) }, 4, X_BAD_STUB, "truncated header" },
        { { W32(0), W32(10), 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 0 }, 18, 0, "control: max size ok" },
    };
    for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
        nctx c; nc_rx(&c, fmt, t[k].w, t[k].len, 0);
        unsigned char *p = NULL;
        NTRY f->um(&c.sm, &p, fmt, 0); NEND;
        CHECKF(n_code == t[k].code, "%s: raised %lu want %lu", t[k].why, (unsigned long)n_code, (unsigned long)t[k].code);
        nc_done(&c);
    }
}
TEST(ndr_ncstr_memorysize_uses_declared_size) {
    const fam *f = nfam(); if (!f) { t_skip("exports"); return; }
    static const unsigned char fmtc[] = { FC_CSTRING, FC_PAD, S16(10) };
    static const unsigned char fmtw[] = { FC_WSTRING, FC_PAD, S16(10) };
    unsigned char w[32] = { W32(0), W32(3), 'a', 'b', 0, 0, 0, 0 };
    nctx c; nc_rx(&c, fmtc, w, 11, 1);
    c.sm.MemorySize = 0;
    ULONG r = sa_ms(f, &c, fmtc, "c");
    EXPECT_EQ(r, 10);
    EXPECT_EQ(nc_used(&c), 11);
    nc_done(&c);
    { unsigned char ww[32] = { W32(0), W32(3), 'a', 0, 'b', 0, 0, 0 }; memcpy(w, ww, 14); }
    nc_rx(&c, fmtw, w, 14, 1);
    c.sm.MemorySize = 1;   /* wide: memory size is first rounded to 2 */
    r = sa_ms(f, &c, fmtw, "w");
    EXPECT_EQ(r, 2 + 20);
    EXPECT_EQ(nc_used(&c), 14);
    nc_done(&c);
}
