/* gap_03: rpcrt4 NDR engine entry points that earlier areas only reached through table macros:
 * NdrConformantStringBufferSize, NdrContextHandleSize, NdrConformantArray{BufferSize,Marshall},
 * NdrSimpleStruct{BufferSize,Marshall,Unmarshall}, NdrComplexStruct*, NdrComplexArray*.
 * Every call goes through GetProcAddress (U); BufferLength arithmetic is verified against the Win10 decomp
 * (NdrConformantStringBufferSize, NdrContextHandleSize). */
#include "g3ndr.h"
#define W32(x) (x) & 0xff, ((x) >> 8) & 0xff, ((x) >> 16) & 0xff, ((x) >> 24) & 0xff

static ULONG alg(ULONG v, ULONG a) { return (v + a - 1) & ~(a - 1); }

/* ---------------------------------------------------------------------------------------------------- */
/* NdrConformantStringBufferSize                                                                         */
/* ---------------------------------------------------------------------------------------------------- */
static ULONG str_bs(const unsigned char *fmt, void *mem, ULONG init, int with_array_info, DWORD *exc) {
    nctx c; ARRAY_INFO ai;
    memset(&ai, 0, sizeof ai);
    nc_client(&c, fmt, 16);
    c.sm.pArrayInfo = with_array_info ? &ai : NULL;
    c.sm.BufferLength = init;
    NTRY U(NdrConformantStringBufferSize)(&c.sm, (unsigned char *)mem, fmt); NEND;
    if (exc) *exc = n_code;
    ULONG r = c.sm.BufferLength;
    nc_done(&c);
    return r;
}

static const unsigned char gf_cs[] = { FC_C_CSTRING, FC_PAD };
static const unsigned char gf_ws[] = { FC_C_WSTRING, FC_PAD };

TEST(gap3_cstr_buffersize_header_table) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* decomp: BL = align4(BL)+4 (max count), then align4+8 (offset + actual count), then the characters incl. NUL */
    static const char *s[] = { "", "a", "ab", "abc", "hello", "0123456789abcdef" };
    for (ULONG init = 0; init < 10; init++)
        for (unsigned i = 0; i < sizeof s / sizeof s[0]; i++) {
            DWORD exc; ULONG n = (ULONG)strlen(s[i]) + 1;
            ULONG r = str_bs(gf_cs, (void *)s[i], init, 0, &exc);
            CHECKF(exc == 0 && r == alg(init, 4) + 12 + n, "init %lu '%s': BufferLength %lu want %lu (exc %lu)", init, s[i], r, alg(init, 4) + 12 + n, exc);
        }
}
TEST(gap3_cstr_buffersize_inside_array_has_no_conformance_word) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* with pArrayInfo set the max-count word is supplied by the enclosing array: only offset+actual count (8 bytes) */
    static const char *s[] = { "", "x", "hello" };
    for (ULONG init = 0; init < 9; init++)
        for (unsigned i = 0; i < 3; i++) {
            DWORD exc; ULONG n = (ULONG)strlen(s[i]) + 1;
            ULONG r = str_bs(gf_cs, (void *)s[i], init, 1, &exc);
            CHECKF(exc == 0 && r == alg(init, 4) + 8 + n, "init %lu '%s': %lu want %lu", init, s[i], r, alg(init, 4) + 8 + n);
        }
}
TEST(gap3_wstr_buffersize_counts_two_bytes_per_char) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    static const WCHAR e[] = { 0 }, a[] = { 'a', 0 }, hi[] = { 'h', 'i', 0 }, u[] = { 0x263a, 0xd83d, 0xde00, 0 };
    static const struct { const WCHAR *s; ULONG chars; } t[] = { { e, 0 }, { a, 1 }, { hi, 2 }, { u, 3 } };
    for (ULONG init = 0; init < 9; init++)
        for (unsigned i = 0; i < 4; i++) {
            DWORD exc; ULONG n = (t[i].chars + 1) * 2;
            ULONG r = str_bs(gf_ws, (void *)t[i].s, init, 0, &exc);
            CHECKF(exc == 0 && r == alg(init, 4) + 12 + n, "init %lu case %u: %lu want %lu", init, i, r, alg(init, 4) + 12 + n);
        }
}
TEST(gap3_cstr_buffersize_matches_marshalled_size) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrConformantStringMarshall)); REQUIRE(U(NdrClientInitializeNew));
    /* BufferSize from an aligned start must equal what NdrConformantStringMarshall then writes */
    static const char *s[] = { "", "q", "twelve chars", "a somewhat longer test string value" };
    for (unsigned i = 0; i < 4; i++) {
        nctx c; nc_client(&c, gf_cs, 128); memset(c.buf, 0, 128);
        c.sm.BufferLength = 0;
        U(NdrConformantStringBufferSize)(&c.sm, (unsigned char *)s[i], gf_cs);
        ULONG pred = c.sm.BufferLength;
        c.sm.Buffer = c.buf;
        NTRY U(NdrConformantStringMarshall)(&c.sm, (unsigned char *)s[i], gf_cs); NEND;
        CHECKF(n_code == 0 && nc_used(&c) == pred, "'%s': predicted %lu, marshalled %lu", s[i], pred, nc_used(&c));
        nc_done(&c);
    }
}
TEST(gap3_cstr_buffersize_bstring_format_uses_strlen) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* decomp: format bytes 0x22 and 0x23 share the strlen branch */
    static const unsigned char fb[] = { FC_C_BSTRING, FC_PAD };
    DWORD exc; ULONG r = str_bs(fb, "hello", 0, 0, &exc);
    CHECKF(exc == 0 && r == 12 + 6, "bstring: %lu exc %lu", r, exc);
}
TEST(gap3_cstr_buffersize_sstring_counts_zero_element) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* FC_C_SSTRING <element size>: length is the number of non-zero elements, +1 terminator element */
    static const unsigned char f4[] = { FC_C_SSTRING, 4 };
    unsigned v[4] = { 1, 2, 3, 0 };
    DWORD exc; ULONG r = str_bs(f4, v, 0, 0, &exc);
    CHECKF(exc == 0 && r == 12 + 4 * 4, "sstring4 x3: %lu (exc %lu)", r, exc);
    unsigned z[2] = { 0, 9 };
    r = str_bs(f4, z, 0, 0, &exc);
    CHECKF(exc == 0 && r == 12 + 4, "sstring4 empty: %lu (exc %lu)", r, exc);
    static const unsigned char f2[] = { FC_C_SSTRING, 2 };
    unsigned short w[3] = { 7, 8, 0 };
    r = str_bs(f2, w, 0, 0, &exc);
    CHECKF(exc == 0 && r == 12 + 3 * 2, "sstring2 x2: %lu (exc %lu)", r, exc);
}
TEST(gap3_cstr_buffersize_bad_format_raises_internal_error) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* decomp: first format byte outside 0x22..0x25 -> RpcRaiseException(RPC_S_INTERNAL_ERROR 1766) */
    static const unsigned char bad[] = { 0x00, 0x01, 0x21, 0x26, 0x5c, 0xff };
    for (unsigned i = 0; i < sizeof bad; i++) {
        unsigned char fmt[4] = { bad[i], FC_PAD, 0, 0 };
        DWORD exc; str_bs(fmt, "x", 0, 0, &exc);
        CHECKF(exc == X_INTERNAL, "format byte 0x%02x raised %lu want %d", bad[i], exc, X_INTERNAL);
    }
}
TEST(gap3_cstr_buffersize_sized_constant_bound) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    /* FC_STRING_SIZED + constant conformance: the declared maximum must cover the string incl. its NUL */
    static const struct { unsigned max; int ok; } t[] = { { 0, 0 }, { 1, 0 }, { 5, 0 }, { 6, 1 }, { 7, 1 }, { 100, 1 }, { 0xffff, 1 } };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_CONSTANT_CONFORMANCE, 0, S16(t[i].max) };
        DWORD exc; ULONG r = str_bs(fmt, "hello", 0, 0, &exc);
        if (t[i].ok) CHECKF(exc == 0 && r == 12 + 6, "max %u: exc %lu len %lu", t[i].max, exc, r);
        else CHECKF(exc == X_INVALID_BOUND, "max %u raised %lu want %d", t[i].max, exc, X_INVALID_BOUND);
    }
}
TEST(gap3_wstr_buffersize_sized_bound_is_in_characters) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    static const WCHAR hi[] = { 'h', 'i', 0 };
    static const struct { unsigned max; int ok; } t[] = { { 0, 0 }, { 2, 0 }, { 3, 1 }, { 4, 1 }, { 1000, 1 } };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        unsigned char fmt[] = { FC_C_WSTRING, FC_STRING_SIZED, FC_CONSTANT_CONFORMANCE, 0, S16(t[i].max) };
        DWORD exc; ULONG r = str_bs(fmt, (void *)hi, 0, 0, &exc);
        if (t[i].ok) CHECKF(exc == 0 && r == 12 + 6, "max %u: exc %lu len %lu", t[i].max, exc, r);
        else CHECKF(exc == X_INVALID_BOUND, "max %u raised %lu", t[i].max, exc);
    }
}
TEST(gap3_cstr_buffersize_sized_top_level_conformance) {
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char fmt[] = { FC_C_CSTRING, FC_STRING_SIZED, FC_TOP_LEVEL_CONFORMANCE | FC_ULONG, 0, S16(0) };
    static const struct { unsigned stackval; int ok; } t[] = { { 5, 0 }, { 6, 1 }, { 9, 1 }, { 0x80000000u, 0 }, { 0xffffffffu, 0 } };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        nctx c; nc_client(&c, fmt, 16);
        unsigned stack[4] = { t[i].stackval, 0, 0, 0 };
        c.sm.StackTop = (unsigned char *)stack;
        c.sm.BufferLength = 0;
        NTRY U(NdrConformantStringBufferSize)(&c.sm, (unsigned char *)"hello", fmt); NEND;
        if (t[i].ok) CHECKF(n_code == 0 && c.sm.BufferLength == 18, "stack %u: exc %lu len %lu", t[i].stackval, (unsigned long)n_code, c.sm.BufferLength);
        else CHECKF(n_code == X_INVALID_BOUND, "stack 0x%x raised %lu (a negative or too small count must raise)", t[i].stackval, (unsigned long)n_code);
        nc_done(&c);
    }
}

/* ---------------------------------------------------------------------------------------------------- */
/* NdrContextHandleSize                                                                                  */
/* ---------------------------------------------------------------------------------------------------- */
static const unsigned char gf_ctx[] = { FC_BIND_CONTEXT, 0x48, 0, 0 };
TEST(gap3_ctxhandle_size_adds_aligned_20_bytes) {
    REQUIRE(U(NdrContextHandleSize)); REQUIRE(U(NdrClientInitializeNew));
    /* decomp: BufferLength = align4(BufferLength) + 0x14 (4 byte attributes + 16 byte GUID) */
    for (ULONG init = 0; init < 17; init++) {
        nctx c; nc_client(&c, gf_ctx, 16);
        c.sm.BufferLength = init;
        NTRY U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx); NEND;
        CHECKF(n_code == 0 && c.sm.BufferLength == alg(init, 4) + 20, "init %lu: %lu want %lu (exc %lu)", init, c.sm.BufferLength, alg(init, 4) + 20, (unsigned long)n_code);
        nc_done(&c);
    }
}
TEST(gap3_ctxhandle_size_accumulates) {
    REQUIRE(U(NdrContextHandleSize)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, gf_ctx, 16);
    c.sm.BufferLength = 0;
    for (int i = 1; i <= 5; i++) {
        U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx);
        CHECKF(c.sm.BufferLength == (ULONG)(20 * i), "after %d handles: %lu", i, c.sm.BufferLength);
    }
    nc_done(&c);
}
TEST(gap3_ctxhandle_size_ignores_memory_pointer_value) {
    REQUIRE(U(NdrContextHandleSize)); REQUIRE(U(NdrClientInitializeNew));
    void *mem[3] = { NULL, &mem, (void *)(ULONG_PTR)0 };
    for (int i = 0; i < 2; i++) {
        nctx c; nc_client(&c, gf_ctx, 16); c.sm.BufferLength = 0;
        U(NdrContextHandleSize)(&c.sm, (unsigned char *)mem[i], gf_ctx);
        EXPECT_EQ(c.sm.BufferLength, 20);
        nc_done(&c);
    }
}
TEST(gap3_ctxhandle_size_overflow_raises_invalid_bound) {
    REQUIRE(U(NdrContextHandleSize)); REQUIRE(U(NdrClientInitializeNew));
    nctx c; nc_client(&c, gf_ctx, 16);
    c.sm.BufferLength = 0xFFFFFFE8u;
    NTRY U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx); NEND;
    CHECKF(n_code == 0 && c.sm.BufferLength == 0xFFFFFFFCu, "fits: exc %lu len 0x%lx", (unsigned long)n_code, c.sm.BufferLength);
    c.sm.BufferLength = 0xFFFFFFECu;     /* 0xEC + 0x14 == 0x100 -> overflow */
    NTRY U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx); NEND;
    CHECKF(n_code == X_INVALID_BOUND, "0xFFFFFFEC raised %lu want %d", (unsigned long)n_code, X_INVALID_BOUND);
    CHECKF(c.sm.BufferLength == 0xFFFFFFFFu, "saturated BufferLength 0x%lx", c.sm.BufferLength);
    c.sm.BufferLength = 0xFFFFFFFDu;     /* the +3 rounding itself wraps */
    NTRY U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx); NEND;
    CHECKF(n_code == X_INVALID_BOUND, "0xFFFFFFFD raised %lu", (unsigned long)n_code);
    c.sm.BufferLength = 0xFFFFFFFFu;
    NTRY U(NdrContextHandleSize)(&c.sm, NULL, gf_ctx); NEND;
    CHECKF(n_code == X_INVALID_BOUND, "0xFFFFFFFF raised %lu", (unsigned long)n_code);
    nc_done(&c);
}

/* ---------------------------------------------------------------------------------------------------- */
/* NdrConformantArrayBufferSize / Marshall                                                               */
/* ---------------------------------------------------------------------------------------------------- */
static void carray_case(const char *tag, unsigned char align, unsigned esz, unsigned count, unsigned char base) {
    unsigned char fmt[] = { FC_CARRAY, align, S16(esz), FC_CONSTANT_CONFORMANCE, 0, S16(count), base, FC_END };
    unsigned char *mem = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, esz * count + 8);
    for (unsigned i = 0; i < esz * count; i++) mem[i] = (unsigned char)(0x30 + i);
    ULONG al = align + 1;
    for (ULONG init = 0; init < 17; init += (init < 9 ? 1 : 4)) {
        ULONG exp = alg(alg(init, 4) + 4, al) + esz * count;
        nctx c; nc_client(&c, fmt, 16);
        c.sm.BufferLength = init;
        NTRY U(NdrConformantArrayBufferSize)(&c.sm, mem, fmt); NEND;
        CHECKF(n_code == 0, "%s init %lu raised %lu", tag, init, (unsigned long)n_code);
        CHECKF(c.sm.BufferLength >= exp, "%s init %lu: BufferSize %lu < wire need %lu", tag, init, c.sm.BufferLength, exp);
        if (c.sm.BufferLength != exp) t_note("%s init %lu: BufferSize %lu, tight bound %lu", tag, init, c.sm.BufferLength, exp);
        nc_done(&c);
    }
    /* exact marshalled size from an aligned (offset 0) start */
    nctx c; nc_client(&c, fmt, 64 + esz * count);
    memset(c.buf, 0, 64 + esz * count);
    c.sm.BufferLength = 0;
    U(NdrConformantArrayBufferSize)(&c.sm, mem, fmt);
    ULONG pred = c.sm.BufferLength;
    c.sm.Buffer = c.buf; c.sm.BufferStart = c.buf;
    NTRY U(NdrConformantArrayMarshall)(&c.sm, mem, fmt); NEND;
    ULONG want = alg(4, al) + esz * count;
    CHECKF(n_code == 0 && nc_used(&c) == want, "%s: marshalled %lu want %lu", tag, nc_used(&c), want);
    CHECKF(pred >= want, "%s: predicted %lu < marshalled %lu", tag, pred, want);
    CHECKF(*(unsigned *)c.buf == count, "%s: conformance word %u want %u", tag, *(unsigned *)c.buf, count);
    CHECKF(!memcmp(c.buf + alg(4, al), mem, esz * count), "%s: element bytes", tag);
    nc_done(&c);
    HeapFree(GetProcessHeap(), 0, mem);
}
TEST(gap3_carray_byte16) { REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew)); carray_case("byte16", 0, 1, 16, FC_BYTE); }
TEST(gap3_carray_short5) { REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew)); carray_case("short5", 1, 2, 5, FC_SHORT); }
TEST(gap3_carray_long3) { REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew)); carray_case("long3", 3, 4, 3, FC_LONG); }
TEST(gap3_carray_hyper2_data_starts_on_8) { REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew)); carray_case("hyper2", 7, 8, 2, FC_HYPER); }
TEST(gap3_carray_double1) { REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew)); carray_case("double1", 7, 8, 1, FC_DOUBLE); }
TEST(gap3_carray_zero_elements_still_sends_conformance) {
    REQUIRE(U(NdrConformantArrayBufferSize)); REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char fmt[] = { FC_CARRAY, 3, S16(4), FC_CONSTANT_CONFORMANCE, 0, S16(0), FC_LONG, FC_END };
    nctx c; nc_client(&c, fmt, 32); memset(c.buf, 0xEE, 32);
    unsigned dummy = 0;
    c.sm.Buffer = c.buf; c.sm.BufferStart = c.buf;
    NTRY U(NdrConformantArrayMarshall)(&c.sm, (unsigned char *)&dummy, fmt); NEND;
    CHECKF(n_code == 0 && nc_used(&c) == 4 && *(unsigned *)c.buf == 0, "used %lu word %u", nc_used(&c), *(unsigned *)c.buf);
    nc_done(&c);
}
TEST(gap3_carray_marshal_misaligned_buffer_start_pads_with_gap) {
    REQUIRE(U(NdrConformantArrayMarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char fmt[] = { FC_CARRAY, 3, S16(4), FC_CONSTANT_CONFORMANCE, 0, S16(2), FC_LONG, FC_END };
    unsigned mem[2] = { 0xAABBCCDD, 0x11223344 };
    for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, fmt, 64); memset(c.buf, 0, 64);
        c.sm.Buffer = c.buf + off;
        NTRY U(NdrConformantArrayMarshall)(&c.sm, (unsigned char *)mem, fmt); NEND;
        ULONG_PTR st = ((ULONG_PTR)c.buf + off + 3) & ~(ULONG_PTR)3;
        CHECKF(n_code == 0 && (ULONG_PTR)c.sm.Buffer == st + 12, "off %d: end %p start %p", off, c.sm.Buffer, (void *)st);
        CHECKF(*(unsigned *)st == 2 && *(unsigned *)(st + 4) == mem[0] && *(unsigned *)(st + 8) == mem[1], "off %d: wire contents", off);
        nc_done(&c);
    }
}

/* ---------------------------------------------------------------------------------------------------- */
/* NdrSimpleStruct{BufferSize,Marshall,Unmarshall}                                                       */
/* ---------------------------------------------------------------------------------------------------- */
struct g3s_a { char c; char pad[3]; int l; short s; short pad2; };
static const unsigned char g3f_a[] = { FC_STRUCT, 3, S16(12), FC_CHAR, FC_STRUCTPAD3, FC_LONG, FC_SHORT, FC_STRUCTPAD2, FC_END };
struct g3s_b { short a; short pad; float f; };
static const unsigned char g3f_b[] = { FC_STRUCT, 3, S16(8), FC_SHORT, FC_STRUCTPAD2, FC_FLOAT, FC_END };
struct g3s_c { char a; char pad[7]; long long h; double d; };
static const unsigned char g3f_c[] = { FC_STRUCT, 7, S16(24), FC_CHAR, FC_STRUCTPAD7, FC_HYPER, FC_DOUBLE, FC_END };
struct g3s_d { char a, b, c, d, e; };
static const unsigned char g3f_d[] = { FC_STRUCT, 0, S16(5), FC_CHAR, FC_CHAR, FC_CHAR, FC_CHAR, FC_CHAR, FC_END };

static const struct { const char *n; const unsigned char *fmt; unsigned size; unsigned align; } g3_ss[] = {
    { "a", g3f_a, 12, 4 }, { "b", g3f_b, 8, 4 }, { "c", g3f_c, 24, 8 }, { "d", g3f_d, 5, 1 },
};
TEST(gap3_sstruct_buffersize_aligns_then_adds_size) {
    REQUIRE(U(NdrSimpleStructBufferSize)); REQUIRE(U(NdrClientInitializeNew));
    for (unsigned k = 0; k < 4; k++)
        for (ULONG init = 0; init < 12; init++) {
            unsigned char mem[32] = { 0 };
            nctx c; nc_client(&c, g3_ss[k].fmt, 16); c.sm.BufferLength = init;
            NTRY U(NdrSimpleStructBufferSize)(&c.sm, mem, g3_ss[k].fmt); NEND;
            ULONG exp = alg(init, g3_ss[k].align) + g3_ss[k].size;
            CHECKF(n_code == 0 && c.sm.BufferLength == exp, "%s init %lu: %lu want %lu", g3_ss[k].n, init, c.sm.BufferLength, exp);
            nc_done(&c);
        }
}
TEST(gap3_sstruct_marshal_copies_memory_image_at_all_offsets) {
    REQUIRE(U(NdrSimpleStructMarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (unsigned k = 0; k < 4; k++) {
        unsigned char mem[32]; for (int i = 0; i < 32; i++) mem[i] = (unsigned char)(0xA0 + i);
        for (int off = 0; off < 9; off++) {
            nctx c; nc_client(&c, g3_ss[k].fmt, 64); memset(c.buf, 0, 64);
            c.sm.Buffer = c.buf + off;
            NTRY U(NdrSimpleStructMarshall)(&c.sm, mem, g3_ss[k].fmt); NEND;
            ULONG_PTR st = ((ULONG_PTR)c.buf + off + g3_ss[k].align - 1) & ~(ULONG_PTR)(g3_ss[k].align - 1);
            CHECKF(n_code == 0 && (ULONG_PTR)c.sm.Buffer == st + g3_ss[k].size, "%s off %d: end %p want %p", g3_ss[k].n, off, c.sm.Buffer, (void *)(st + g3_ss[k].size));
            CHECKF(!memcmp((void *)st, mem, g3_ss[k].size), "%s off %d: wire image", g3_ss[k].n, off);
            nc_done(&c);
        }
    }
}
TEST(gap3_sstruct_unmarshal_client_copies_into_caller_memory) {
    REQUIRE(U(NdrSimpleStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (unsigned k = 0; k < 4; k++) {
        unsigned char wire[32]; for (int i = 0; i < 32; i++) wire[i] = (unsigned char)(0x10 + i * 3);
        nctx c; nc_rx(&c, g3_ss[k].fmt, wire, g3_ss[k].size, 1);
        unsigned char dst[40]; memset(dst, 0x55, sizeof dst);
        unsigned char *p = dst + 8, *volatile r;
        NTRY r = U(NdrSimpleStructUnmarshall)(&c.sm, &p, g3_ss[k].fmt, 0); NEND;
        CHECKF(n_code == 0, "%s raised %lu", g3_ss[k].n, (unsigned long)n_code);
        CHECKF(p == dst + 8 && !memcmp(dst + 8, wire, g3_ss[k].size), "%s: destination contents", g3_ss[k].n);
        CHECKF(dst[7] == 0x55 && dst[8 + g3_ss[k].size] == 0x55, "%s: wrote outside the struct", g3_ss[k].n);
        CHECKF(nc_used(&c) == g3_ss[k].size, "%s: consumed %lu", g3_ss[k].n, nc_used(&c));
        nc_done(&c);
    }
}
TEST(gap3_sstruct_unmarshal_fmustalloc_allocates) {
    REQUIRE(U(NdrSimpleStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char wire[12] = { 'Z', 0, 0, 0, 1, 2, 3, 4, 5, 6, 0, 0 };
    nctx c; nc_rx(&c, g3f_a, wire, 12, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    NTRY U(NdrSimpleStructUnmarshall)(&c.sm, &p, g3f_a, 1); NEND;
    CHECKF(n_code == 0 && p != NULL && n_alloc_cnt == 1, "exc %lu p %p allocs %d", (unsigned long)n_code, p, n_alloc_cnt);
    if (p) { CHECK(!memcmp(p, wire, 12)); n_free(p); }
    nc_done(&c);
}
TEST(gap3_sstruct_unmarshal_truncated_buffer_raises_bad_stub_data) {
    REQUIRE(U(NdrSimpleStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    for (unsigned k = 0; k < 4; k++)
        for (unsigned len = 0; len < g3_ss[k].size; len++) {
            unsigned char wire[32] = { 0 };
            nctx c; nc_rx(&c, g3_ss[k].fmt, wire, len, 1);
            unsigned char dst[32], *p = dst;
            NTRY U(NdrSimpleStructUnmarshall)(&c.sm, &p, g3_ss[k].fmt, 0); NEND;
            CHECKF(n_code == X_BAD_STUB, "%s len %u raised %lu want %d", g3_ss[k].n, len, (unsigned long)n_code, X_BAD_STUB);
            nc_done(&c);
        }
}
TEST(gap3_sstruct_roundtrip_through_both_directions) {
    REQUIRE(U(NdrSimpleStructMarshall)); REQUIRE(U(NdrSimpleStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    struct g3s_c in = { 'Q', {0}, 0x0123456789ABCDEFll, -2.5 }, out;
    memset(&out, 0, sizeof out);
    nctx c; nc_client(&c, g3f_c, 64); memset(c.buf, 0, 64);
    c.sm.Buffer = c.buf;
    U(NdrSimpleStructMarshall)(&c.sm, (unsigned char *)&in, g3f_c);
    ULONG used = nc_used(&c);
    EXPECT_EQ(used, 24);
    nctx r; nc_rx(&r, g3f_c, c.buf, used, 1);
    unsigned char *p = (unsigned char *)&out;
    NTRY U(NdrSimpleStructUnmarshall)(&r.sm, &p, g3f_c, 0); NEND;
    EXPECT_EQ(n_code, 0);
    CHECK(out.a == 'Q' && out.h == in.h && out.d == in.d);
    nc_done(&c); nc_done(&r);
}

/* ---------------------------------------------------------------------------------------------------- */
/* NdrComplexStruct* / NdrComplexArray*                                                                  */
/* ---------------------------------------------------------------------------------------------------- */
/* struct { char c; long l; short s; } laid out as a bogus struct: wire is c, pad to 4, l, s (10 bytes, no tail pad) */
static const unsigned char g3f_bs[] = { FC_BOGUS_STRUCT, 3, S16(12), S16(0), S16(0), FC_CHAR, FC_STRUCTPAD3, FC_LONG, FC_SHORT, FC_STRUCTPAD2, FC_END };
/* struct { double d; char c; } : wire d (8) + c (1) = 9 bytes, struct alignment 8 */
static const unsigned char g3f_bs2[] = { FC_BOGUS_STRUCT, 7, S16(16), S16(0), S16(0), FC_DOUBLE, FC_CHAR, FC_STRUCTPAD7, FC_END };

TEST(gap3_cstruct_complex_marshal_wire_layout) {
    REQUIRE(U(NdrComplexStructMarshall)); REQUIRE(U(NdrClientInitializeNew));
    struct g3s_a v = { 'k', {0}, 0x01020304, 0x0506, 0 };
    nctx c; nc_client(&c, g3f_bs, 64); memset(c.buf, 0xEE, 64);
    c.sm.Buffer = c.buf;
    NTRY U(NdrComplexStructMarshall)(&c.sm, (unsigned char *)&v, g3f_bs); NEND;
    CHECKF(n_code == 0 && nc_used(&c) == 10, "used %lu exc %lu", nc_used(&c), (unsigned long)n_code);
    CHECK(c.buf[0] == 'k');
    EXPECT_EQ(*(unsigned *)(c.buf + 4), 0x01020304);
    EXPECT_EQ(*(unsigned short *)(c.buf + 8), 0x0506);
    t_note("complex struct wire alignment padding bytes: %02x %02x %02x", c.buf[1], c.buf[2], c.buf[3]);
    nc_done(&c);
}
TEST(gap3_cstruct_complex_marshal_aligns_start_to_struct_alignment) {
    REQUIRE(U(NdrComplexStructMarshall)); REQUIRE(U(NdrClientInitializeNew));
    struct g3s_a v = { 'k', {0}, 0x11223344, 0x5566, 0 };
    for (int off = 0; off < 8; off++) {
        nctx c; nc_client(&c, g3f_bs, 64); memset(c.buf, 0, 64);
        c.sm.Buffer = c.buf + off;
        NTRY U(NdrComplexStructMarshall)(&c.sm, (unsigned char *)&v, g3f_bs); NEND;
        ULONG_PTR st = ((ULONG_PTR)c.buf + off + 3) & ~(ULONG_PTR)3;
        CHECKF(n_code == 0 && (ULONG_PTR)c.sm.Buffer == st + 10, "off %d: end %p want %p", off, c.sm.Buffer, (void *)(st + 10));
        CHECKF(*(char *)st == 'k' && *(unsigned *)(st + 4) == 0x11223344 && *(unsigned short *)(st + 8) == 0x5566, "off %d: members", off);
        nc_done(&c);
    }
}
TEST(gap3_cstruct_complex_double_char_has_no_tail_padding_on_wire) {
    REQUIRE(U(NdrComplexStructMarshall)); REQUIRE(U(NdrClientInitializeNew));
    struct { double d; char c; char pad[7]; } v = { 1.5, 'x', {0} };
    nctx c; nc_client(&c, g3f_bs2, 64); memset(c.buf, 0, 64);
    c.sm.Buffer = c.buf;
    NTRY U(NdrComplexStructMarshall)(&c.sm, (unsigned char *)&v, g3f_bs2); NEND;
    CHECKF(n_code == 0 && nc_used(&c) == 9, "used %lu", nc_used(&c));
    CHECK(*(double *)c.buf == 1.5 && c.buf[8] == 'x');
    nc_done(&c);
}
TEST(gap3_cstruct_complex_buffersize_bounds_marshalled_size) {
    REQUIRE(U(NdrComplexStructBufferSize)); REQUIRE(U(NdrComplexStructMarshall)); REQUIRE(U(NdrClientInitializeNew));
    struct g3s_a v = { 'k', {0}, 1, 2, 0 };
    for (ULONG init = 0; init < 12; init++) {
        nctx c; nc_client(&c, g3f_bs, 64); c.sm.BufferLength = init;
        NTRY U(NdrComplexStructBufferSize)(&c.sm, (unsigned char *)&v, g3f_bs); NEND;
        ULONG need = alg(init, 4) + 10;
        CHECKF(n_code == 0 && c.sm.BufferLength >= need, "init %lu: %lu < %lu (exc %lu)", init, c.sm.BufferLength, need, (unsigned long)n_code);
        if (c.sm.BufferLength != need) t_note("complex struct init %lu: BufferSize %lu vs tight %lu", init, c.sm.BufferLength, need);
        nc_done(&c);
    }
}
TEST(gap3_cstruct_complex_unmarshal_restores_members) {
    REQUIRE(U(NdrComplexStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char wire[10] = { 'm', 0, 0, 0, 0x78, 0x56, 0x34, 0x12, 0xCD, 0xAB };
    nctx c; nc_rx(&c, g3f_bs, wire, 10, 1);
    struct g3s_a dst; memset(&dst, 0x77, sizeof dst);
    unsigned char *p = (unsigned char *)&dst;
    NTRY U(NdrComplexStructUnmarshall)(&c.sm, &p, g3f_bs, 0); NEND;
    CHECKF(n_code == 0, "raised %lu", (unsigned long)n_code);
    CHECKF(dst.c == 'm' && dst.l == 0x12345678 && (unsigned short)dst.s == 0xABCD, "members %c %x %x", dst.c, dst.l, dst.s);
    EXPECT_EQ(nc_used(&c), 10);
    nc_done(&c);
}
TEST(gap3_cstruct_complex_unmarshal_truncated_raises_bad_stub_data) {
    REQUIRE(U(NdrComplexStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char wire[10] = { 0 };
    for (unsigned len = 0; len < 10; len++) {
        nctx c; nc_rx(&c, g3f_bs, wire, len, 1);
        struct g3s_a dst; unsigned char *p = (unsigned char *)&dst;
        NTRY U(NdrComplexStructUnmarshall)(&c.sm, &p, g3f_bs, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu want %d", len, (unsigned long)n_code, X_BAD_STUB);
        nc_done(&c);
    }
}
TEST(gap3_cstruct_complex_unmarshal_fmustalloc_allocates) {
    REQUIRE(U(NdrComplexStructUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char wire[10] = { 'm', 0, 0, 0, 1, 0, 0, 0, 2, 0 };
    nctx c; nc_rx(&c, g3f_bs, wire, 10, 1);
    unsigned char *p = NULL; n_alloc_cnt = 0;
    NTRY U(NdrComplexStructUnmarshall)(&c.sm, &p, g3f_bs, 1); NEND;
    CHECKF(n_code == 0 && p && n_alloc_cnt == 1, "exc %lu p %p allocs %d", (unsigned long)n_code, p, n_alloc_cnt);
    if (p) { struct g3s_a *s = (struct g3s_a *)p; CHECK(s->c == 'm' && s->l == 1 && s->s == 2); n_free(p); }
    nc_done(&c);
}

/* complex (bogus) array of enum16: 4 bytes in memory, 2 on the wire, each value must be <= 0x7fff */
static const unsigned char g3f_ba[] = { FC_BOGUS_ARRAY, 3, S16(5), W32(0xffffffff), W32(0xffffffff), FC_ENUM16, FC_END };
TEST(gap3_carray_complex_enum16_marshal_wire_is_two_bytes_per_element) {
    REQUIRE(U(NdrComplexArrayMarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned mem[5] = { 0, 1, 0x7fff, 0x100, 0x1234 };
    nctx c; nc_client(&c, g3f_ba, 64); memset(c.buf, 0xEE, 64);
    c.sm.Buffer = c.buf;
    NTRY U(NdrComplexArrayMarshall)(&c.sm, (unsigned char *)mem, g3f_ba); NEND;
    CHECKF(n_code == 0 && nc_used(&c) == 10, "used %lu exc %lu", nc_used(&c), (unsigned long)n_code);
    for (int i = 0; i < 5; i++) CHECKF(((unsigned short *)c.buf)[i] == mem[i], "element %d = 0x%x", i, ((unsigned short *)c.buf)[i]);
    nc_done(&c);
}
TEST(gap3_carray_complex_enum16_marshal_out_of_range_raises) {
    REQUIRE(U(NdrComplexArrayMarshall)); REQUIRE(U(NdrClientInitializeNew));
    static const unsigned bad[] = { 0x8000, 0x10000, 0x7fffffff, 0xffffffffu };
    for (unsigned i = 0; i < 4; i++) {
        unsigned mem[5] = { 1, 2, 3, 4, 5 }; mem[3] = bad[i];
        nctx c; nc_client(&c, g3f_ba, 64); c.sm.Buffer = c.buf;
        NTRY U(NdrComplexArrayMarshall)(&c.sm, (unsigned char *)mem, g3f_ba); NEND;
        CHECKF(n_code == X_ENUM_RANGE, "value 0x%x raised %lu want %d", bad[i], (unsigned long)n_code, X_ENUM_RANGE);
        nc_done(&c);
    }
}
TEST(gap3_carray_complex_buffersize_and_unmarshal_roundtrip) {
    REQUIRE(U(NdrComplexArrayBufferSize)); REQUIRE(U(NdrComplexArrayMarshall)); REQUIRE(U(NdrComplexArrayUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned mem[5] = { 9, 8, 7, 6, 5 };
    nctx c; nc_client(&c, g3f_ba, 64); memset(c.buf, 0, 64);
    c.sm.BufferLength = 0;
    NTRY U(NdrComplexArrayBufferSize)(&c.sm, (unsigned char *)mem, g3f_ba); NEND;
    CHECKF(n_code == 0 && c.sm.BufferLength >= 10, "BufferSize %lu", c.sm.BufferLength);
    c.sm.Buffer = c.buf;
    U(NdrComplexArrayMarshall)(&c.sm, (unsigned char *)mem, g3f_ba);
    ULONG used = nc_used(&c);
    nctx r; nc_rx(&r, g3f_ba, c.buf, used, 1);
    unsigned out[5] = { 0 }; unsigned char *p = (unsigned char *)out;
    NTRY U(NdrComplexArrayUnmarshall)(&r.sm, &p, g3f_ba, 0); NEND;
    CHECKF(n_code == 0 && !memcmp(out, mem, sizeof mem), "round trip: exc %lu out %u %u %u %u %u", (unsigned long)n_code, out[0], out[1], out[2], out[3], out[4]);
    nc_done(&c); nc_done(&r);
}
TEST(gap3_carray_complex_unmarshal_truncated_raises) {
    REQUIRE(U(NdrComplexArrayUnmarshall)); REQUIRE(U(NdrClientInitializeNew));
    unsigned char wire[10] = { 0 };
    for (unsigned len = 0; len < 10; len++) {
        nctx c; nc_rx(&c, g3f_ba, wire, len, 1);
        unsigned out[5]; unsigned char *p = (unsigned char *)out;
        NTRY U(NdrComplexArrayUnmarshall)(&c.sm, &p, g3f_ba, 0); NEND;
        CHECKF(n_code == X_BAD_STUB, "len %u raised %lu want %d", len, (unsigned long)n_code, X_BAD_STUB);
        nc_done(&c);
    }
}
