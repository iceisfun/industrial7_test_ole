/* BSTR_UserSize / BSTR_UserMarshal / BSTR_UserUnmarshal / BSTR_UserFree and the 64 (NDR64) siblings.
 *
 * Wire layout (decomp of oleaut32 19041):
 *   NDR32: pad to 4, then {u32 cnt, u32 byteLen (-1 for NULL), u32 cnt, cnt WCHARs}   cnt = (byteLen+1)/2
 *   NDR64: pad to 8, then {u64 cnt, u32 byteLen (-1 for NULL), u32 cnt, cnt WCHARs}
 * UserSize: align + 12 (16) + byteLen rounded up to even. The odd trailing byte of an odd-byte-length BSTR is
 * copied as part of the last WCHAR. */
#include "ums.h"

typedef struct { const char *name; const WCHAR *data; ULONG bytes; int null; } bcase;
static const bcase bcases[] = {
    { "null", NULL, 0, 1 },
    { "empty", L"", 0, 0 },
    { "a", L"a", 2, 0 },
    { "ab", L"ab", 4, 0 },
    { "abc", L"abc", 6, 0 },
    { "odd3", L"ab", 3, 0 },
    { "odd1", L"a", 1, 0 },
    { "marshal13", L"marshal test1", 26, 0 },
    { "embeddedNul", L"a\0b\0c", 10, 0 },
    { "hi", L"\x4e2d\x6587\xd83d\xde00", 8, 0 },
    { "len32", L"0123456789abcdef0123456789abcdef", 64, 0 },
};
static BSTR mk(const bcase *c) { return c->null ? NULL : SysAllocStringByteLen((const char *)c->data, c->bytes); }

static const ULONG starts[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 15, 16, 17, 100 };

#define NEEDB NEED_FAM(UF_BSTR)

TEST(ums_bstr_size_table)
{
    unsigned i, j, k; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
    for (i = 0; i < ARRAY_N(bcases); i++) {
        BSTR b = mk(&bcases[i]);
        for (j = 0; j < ARRAY_N(starts); j++)
        for (k = 0; k < 3; k++) {
            UCtx c; ULONG got, want;
            uctx_init(&c, 0, ums_ctxs[k], USER_MARSHAL_CB_BUFFER_SIZE);
            got = u_size(&c, UF_BSTR, v, starts[j], &b, NULL);
            want = ums_size_bstr(v, b, starts[j]);
            t_check(got == want, __FILE__, __LINE__, "BSTR_UserSize", "v64=%d case %s start %lu ctx %lu got %lu want %lu",
                    v, bcases[i].name, starts[j], ums_ctxs[k], got, want);
            uctx_free(&c);
        }
        SysFreeString(b);
    }
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) */
TEST(ums_bstr_size_wine_values)
{
    BSTR b = SysAllocString(L"marshal test1"), nb = NULL, ob = SysAllocStringByteLen("abc", 3);
    UCtx c; NEEDB;
    uctx_init(&c, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
    /* BSTRs are DWORD aligned: 13 chars = 26 bytes + 12 */
    EQ(u_size(&c, UF_BSTR, 0, 1, &b, NULL), 42);
    EQ(u_size(&c, UF_BSTR, 0, 0, &b, NULL), 38);
    EQ(u_size(&c, UF_BSTR, 0, 0, &nb, NULL), 12);
    EQ(u_size(&c, UF_BSTR, 0, 0, &ob, NULL), 16);   /* 3 bytes rounded to 4, +12 */
    /* NDR64: align 8, 16 byte header */
    if (!u_have(UF_BSTR, 1)) goto done;
    EQ(u_size(&c, UF_BSTR, 1, 0, &b, NULL), 42);
    EQ(u_size(&c, UF_BSTR, 1, 1, &b, NULL), 50);
    EQ(u_size(&c, UF_BSTR, 1, 0, &nb, NULL), 16);
    EQ(u_size(&c, UF_BSTR, 1, 0, &ob, NULL), 20);
done:
    uctx_free(&c);
    SysFreeString(b); SysFreeString(ob);
}

TEST(ums_bstr_size_null_pointer)
{
    ULONG s; int v; NEEDB; WINE_SKIP("NULL BSTR* crashes");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
        for (s = 0; s < 10; s++) {
            UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            /* a NULL BSTR* (not a NULL BSTR) contributes nothing and does not even align */
            EQ(u_size(&c, UF_BSTR, v, s, NULL, NULL), s);
            uctx_free(&c);
        }
}

TEST(ums_bstr_marshal_layout)
{
    unsigned i; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
    for (i = 0; i < ARRAY_N(bcases); i++) {
        BSTR b = mk(&bcases[i]);
        UCtx c; ULONG size; unsigned char *next, exp[256], mask[256];
        uctx_init(&c, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &b, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
        next = u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL);
        t_check(next == c.buf + size, __FILE__, __LINE__, "marshal end == buf + UserSize", "v64=%d %s: advanced %ld, size %lu", v, bcases[i].name, (long)(next - c.buf), size);
        CHECK(uctx_canary_ok(&c));
        memset(exp, 0xcc, sizeof exp); memset(mask, 0, sizeof mask);
        EQ(ums_model_bstr(v, b, 0, exp, mask), size);
        CHECK_WIRE(c.buf, exp, mask, size, bcases[i].name);
        uctx_free(&c);
        SysFreeString(b);
    }
}

TEST(ums_bstr_marshal_fields)
{
    /* explicit field checks (not via the model) */
    WCHAR s[] = L"marshal test1";
    BSTR b = SysAllocString(s);
    UCtx c; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        unsigned char *w;
        uctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        EQ(u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL) - c.buf, v ? 42 : 38);
        w = c.buf;
        if (v) { EQ(rd64(w), 13); EQ(rd32(w + 8), 26); EQ(rd32(w + 12), 13); CHECK(!memcmp(w + 16, s, 26)); }
        else   { EQ(rd32(w), 13); EQ(rd32(w + 4), 26); EQ(rd32(w + 8), 13); CHECK(!memcmp(w + 12, s, 26)); }
        uctx_free(&c);
    }
    SysFreeString(b);
}

TEST(ums_bstr_marshal_null_bstr_fields)
{
    BSTR b = NULL; UCtx c; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        uctx_init(&c, 32, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        EQ(u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL) - c.buf, v ? 16 : 12);
        if (v) { EQ(rd64(c.buf), 0); EQ(rd32(c.buf + 8), 0xffffffffu); EQ(rd32(c.buf + 12), 0); }
        else   { EQ(rd32(c.buf), 0); EQ(rd32(c.buf + 4), 0xffffffffu); EQ(rd32(c.buf + 8), 0); }
        /* nothing written after the header */
        CHECK(c.buf[v ? 16 : 12] == 0xcc);
        uctx_free(&c);
    }
}

TEST(ums_bstr_marshal_at_offsets)
{
    unsigned i; ULONG off; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
    for (i = 0; i < ARRAY_N(bcases); i += 2) {
        BSTR b = mk(&bcases[i]);
        for (off = 0; off <= 9; off++) {
            UCtx c; ULONG size; unsigned char *next, exp[256], mask[256];
            uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&c, UF_BSTR, v, off, &b, NULL);
            uctx_free(&c);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            next = u_marshal(&c, UF_BSTR, v, c.buf + off, &b, NULL);
            t_check(next == c.buf + size, __FILE__, __LINE__, "end == buf+UserSize(off)", "v64=%d %s off %lu: %ld vs %lu", v, bcases[i].name, off, (long)(next - c.buf), size);
            CHECK(uctx_canary_ok(&c));
            memset(exp, 0xcc, sizeof exp); memset(mask, 0, sizeof mask);
            ums_model_bstr(v, b, off, exp, mask);
            CHECK_WIRE(c.buf, exp, mask, size, "padding stays untouched");
            uctx_free(&c);
        }
        SysFreeString(b);
    }
}

TEST(ums_bstr_marshal_null_pointer)
{
    UCtx c; int v; NEEDB; WINE_SKIP("NULL BSTR* crashes");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        uctx_init(&c, 32, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        /* NULL BSTR*: returns the input pointer unchanged (even unaligned) and writes nothing */
        CHECK(u_marshal(&c, UF_BSTR, v, c.buf + 1, NULL, NULL) == c.buf + 1);
        CHECK(c.buf[0] == 0xcc && c.buf[1] == 0xcc && c.buf[4] == 0xcc);
        uctx_free(&c);
    }
}

static void roundtrip_one(int v, const bcase *bc, ULONG ctx)
{
    BSTR b = mk(bc), b2 = NULL;
    UCtx c; ULONG size; unsigned char *m, *u;
    uctx_init(&c, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
    size = u_size(&c, UF_BSTR, v, 0, &b, NULL);
    uctx_free(&c);
    uctx_init(&c, size, ctx, USER_MARSHAL_CB_MARSHALL);
    m = u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL);
    uctx_reread(&c, size, ctx);
    u = u_unmarshal(&c, UF_BSTR, v, c.buf, &b2, NULL);
    t_check(u == m, __FILE__, __LINE__, "unmarshal end == marshal end", "v64=%d %s", v, bc->name);
    if (bc->null) CHECK(b2 == NULL);
    else {
        CHECKF(b2 != NULL, "%s unmarshaled to NULL", bc->name);
        if (b2) {
            if (!(t_is_wine() && (bc->bytes & 1))) t_check(SysStringByteLen(b2) == bc->bytes, __FILE__, __LINE__, "byte length preserved", "%s: got %u want %lu", bc->name, SysStringByteLen(b2), bc->bytes);
            t_check(!memcmp(b2, bc->data, bc->bytes), __FILE__, __LINE__, "content preserved", "%s", bc->name);
            if (!(t_is_wine() && (bc->bytes & 1))) t_check(SysStringLen(b2) == bc->bytes / 2, __FILE__, __LINE__, "char length", "%s", bc->name);
            /* a real BSTR: terminating NUL wchar follows the byte length (rounded to wchar) */
            t_check(((WCHAR *)b2)[bc->bytes / 2] == 0 || (bc->bytes & 1), __FILE__, __LINE__, "NUL terminated", "%s", bc->name);
        }
    }
    u_free(&c, UF_BSTR, v, &b2, NULL);
    CHECK(b2 == NULL);
    uctx_free(&c);
    SysFreeString(b);
}

TEST(ums_bstr_roundtrip_table)
{
    unsigned i, k; int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
        for (i = 0; i < ARRAY_N(bcases); i++)
            for (k = 0; k < 3; k++)
                roundtrip_one(v, &bcases[i], ums_ctxs[k]);
}

TEST(ums_bstr_roundtrip_large)
{
    int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        ULONG n = 40000, i; WCHAR *w = malloc(n * 2); bcase bc;
        for (i = 0; i < n; i++) w[i] = (WCHAR)(0x20 + (i % 0x5f));
        bc.name = "large"; bc.data = w; bc.bytes = n * 2; bc.null = 0;
        roundtrip_one(v, &bc, MSHCTX_DIFFERENTMACHINE);
        free(w);
    }
}

TEST(ums_bstr_unmarshal_replaces_existing)
{
    int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = SysAllocString(L"replacement"), dst = SysAllocString(L"a much longer previous value that must go away");
        UCtx c; ULONG size; unsigned char *m;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        EQ(u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, NULL), m);
        CHECK(dst != NULL);
        if (dst) { CHECK(!lstrcmpW(dst, L"replacement")); EQ(SysStringLen(dst), 11); }
        SysFreeString(dst); SysFreeString(src); uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal_null_clears_existing)
{
    int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = NULL, dst = SysAllocString(L"will be released");
        UCtx c; ULONG size; unsigned char *m;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        EQ(u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, NULL), m);
        CHECK(dst == NULL);
        uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal_empty_is_not_null)
{
    int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = SysAllocStringLen(L"", 0), dst = NULL;
        UCtx c; ULONG size;
        CHECK(src != NULL);
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        /* byteLen field is 0, not -1 */
        EQ(rd32(c.buf + (v ? 8 : 4)), 0);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, NULL);
        CHECK(dst != NULL);
        if (dst) EQ(SysStringByteLen(dst), 0);
        SysFreeString(dst); SysFreeString(src); uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal_unaligned_start)
{
    /* the unmarshaler rounds the buffer pointer up (4 for NDR32, 8 for NDR64) before reading */
    int v; ULONG off; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
        for (off = 1; off <= 7; off++) {
            BSTR src = SysAllocString(L"offset"), dst = NULL;
            UCtx c; ULONG size; unsigned char *m, *u;
            uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&c, UF_BSTR, v, off, &src, NULL);
            uctx_free(&c);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            m = u_marshal(&c, UF_BSTR, v, c.buf + off, &src, NULL);
            uctx_reread(&c, size, MSHCTX_LOCAL);
            u = u_unmarshal(&c, UF_BSTR, v, c.buf + off, &dst, NULL);
            t_check(u == m && u == c.buf + size, __FILE__, __LINE__, "offset roundtrip", "v64=%d off %lu", v, off);
            CHECK(dst && !lstrcmpW(dst, L"offset"));
            SysFreeString(dst); SysFreeString(src); uctx_free(&c);
        }
}

TEST(ums_bstr_unmarshal_truncated_buffer)
{
    /* every buffer length shorter than the full image must raise RPC_X_BAD_STUB_DATA, never read past the end */
    int v; ULONG n; NEEDB; WINE_SKIP("no shield");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = SysAllocString(L"truncate me"); UCtx c; ULONG size;
        unsigned char *wire;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        wire = malloc(size); memcpy(wire, c.buf, size);
        uctx_free(&c);
        for (n = 0; n < size; n++) {
            UCtx d; BSTR dst = NULL; DWORD exc = 0;
            uctx_wire(&d, wire, n, MSHCTX_LOCAL);
            u_unmarshal(&d, UF_BSTR, v, d.buf, &dst, &exc);
            t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "truncated -> 0x6f7", "v64=%d len %lu/%lu exc 0x%lx", v, n, size, exc);
            if (dst) SysFreeString(dst);
            CHECK(uctx_canary_ok(&d));
            uctx_free(&d);
        }
        /* the full image works */
        { UCtx d; BSTR dst = NULL; uctx_wire(&d, wire, size, MSHCTX_LOCAL); u_unmarshal(&d, UF_BSTR, v, d.buf, &dst, NULL); CHECK(dst && !lstrcmpW(dst, L"truncate me")); SysFreeString(dst); uctx_free(&d); }
        free(wire); SysFreeString(src);
    }
}

TEST(ums_bstr_unmarshal32_count_mismatch)
{
    /* NDR32 strict check: the two counts must agree */
    unsigned char w[12 + 16]; UCtx c; BSTR dst = NULL; DWORD exc = 0; NEEDB; WINE_SKIP("no shield");
    memset(w, 0, sizeof w);
    wr32(w, 4); wr32(w + 4, 8); wr32(w + 8, 5);
    memcpy(w + 12, L"abcd", 8);
    uctx_wire(&c, w, sizeof w, MSHCTX_LOCAL);
    u_unmarshal(&c, UF_BSTR, 0, c.buf, &dst, &exc);
    EQ(exc, RPC_X_BAD_STUB_DATA_);
    CHECK(dst == NULL);
    uctx_free(&c);
}

TEST(ums_bstr_unmarshal32_bad_bytelen)
{
    /* byteLen must be 2*cnt, 2*cnt-1 or -1 */
    static const unsigned bad[] = { 0, 1, 6, 9, 10, 100, 0x7fffffffu };
    static const unsigned good[] = { 7, 8, 0xffffffffu };
    unsigned i; NEEDB; WINE_SKIP("no shield");
    for (i = 0; i < ARRAY_N(bad) + ARRAY_N(good); i++) {
        unsigned bl = i < ARRAY_N(bad) ? bad[i] : good[i - ARRAY_N(bad)];
        unsigned char w[12 + 8]; UCtx c; BSTR dst = NULL; DWORD exc = 0xabc;
        memset(w, 0, sizeof w);
        wr32(w, 4); wr32(w + 4, bl); wr32(w + 8, 4);
        memcpy(w + 12, L"abcd", 8);
        uctx_wire(&c, w, sizeof w, MSHCTX_LOCAL);
        u_unmarshal(&c, UF_BSTR, 0, c.buf, &dst, &exc);
        if (i < ARRAY_N(bad)) t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "bad byteLen rejected", "byteLen %u exc 0x%lx", bl, exc);
        else {
            t_check(exc == 0, __FILE__, __LINE__, "good byteLen accepted", "byteLen %u exc 0x%lx", bl, exc);
            if (bl == 0xffffffffu) CHECK(dst == NULL);
            else if (dst) EQ(SysStringByteLen(dst), bl);
        }
        if (dst) SysFreeString(dst);
        uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal64_bytelen_normalised)
{
    /* NDR64 unmarshal has no shield check: an inconsistent byteLen is replaced by 2*cnt (the cnt in the 2nd dword) */
    static const struct { unsigned bl, want; } t[] = { { 8, 8 }, { 7, 7 }, { 100, 8 }, { 0, 8 }, { 9, 8 }, { 6, 8 } };
    unsigned i; NEEDB;
    if (!u_have(UF_BSTR, 1)) { t_skip("no NDR64 exports"); return; }
    for (i = 0; i < ARRAY_N(t); i++) {
        unsigned char w[16 + 8]; UCtx c; BSTR dst = NULL; DWORD exc = 0;
        memset(w, 0, sizeof w);
        wr64(w, 4); wr32(w + 8, t[i].bl); wr32(w + 12, 4);
        memcpy(w + 16, L"abcd", 8);
        uctx_wire(&c, w, sizeof w, MSHCTX_LOCAL);
        u_unmarshal(&c, UF_BSTR, 1, c.buf, &dst, &exc);
        t_check(exc == 0, __FILE__, __LINE__, "no exception", "byteLen %u exc 0x%lx", t[i].bl, exc);
        if (dst) {
            t_check(SysStringByteLen(dst) == t[i].want, __FILE__, __LINE__, "byte length", "wire %u got %u want %u", t[i].bl, SysStringByteLen(dst), t[i].want);
            SysFreeString(dst);
        }
        uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal_huge_count)
{
    int v; NEEDB; WINE_SKIP("no shield");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        unsigned char w[32]; UCtx c; BSTR dst = NULL; DWORD exc = 0;
        memset(w, 0, sizeof w);
        if (v) { wr64(w, 0x40000000); wr32(w + 8, 0x80000000u); wr32(w + 12, 0x40000000); }
        else   { wr32(w, 0x40000000); wr32(w + 4, 0x80000000u); wr32(w + 8, 0x40000000); }
        uctx_wire(&c, w, sizeof w, MSHCTX_LOCAL);
        u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, &exc);
        t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "huge count rejected without allocation", "v64=%d exc 0x%lx", v, exc);
        CHECK(dst == NULL);
        uctx_free(&c);
    }
}

TEST(ums_bstr_unmarshal_bad_callback)
{
    /* NdrGetUserMarshalInfo failure: bad signature -> ERROR_INVALID_PARAMETER raised as an exception */
    int v; NEEDB; WINE_SKIP("no NdrGetUserMarshalInfo use");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = SysAllocString(L"x"), dst = NULL; UCtx c; ULONG size; DWORD exc = 0;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        c.umcb.Signature = 0x12345678;
        u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, &exc);
        EQ(exc, ERROR_INVALID_PARAMETER);
        if (dst) SysFreeString(dst);
        SysFreeString(src); uctx_free(&c);
    }
}

TEST(ums_bstr_buffer_pointer_outside_rpc_buffer)
{
    /* NdrGetUserMarshalInfo: stub.Buffer outside [rpc.Buffer, rpc.Buffer+BufferLength] -> 0x6f8 (RPC_X_INVALID_BUFFER) */
    int v; NEEDB; WINE_SKIP("no NdrGetUserMarshalInfo use");
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        BSTR src = SysAllocString(L"x"), dst = NULL; UCtx c; ULONG size; DWORD exc = 0;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&c, UF_BSTR, v, 0, &src, NULL);
        uctx_free(&c);
        uctx_init(&c, size + 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_BSTR, v, c.buf, &src, NULL);
        uctx_reread(&c, size, MSHCTX_LOCAL);
        c.stub.Buffer = c.buf + size + 8;
        u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, &exc);
        EQ(exc, 0x6f8);
        c.stub.Buffer = c.buf - 8;
        exc = 0;
        u_unmarshal(&c, UF_BSTR, v, c.buf, &dst, &exc);
        EQ(exc, 0x6f8);
        SysFreeString(src); uctx_free(&c);
    }
}

TEST(ums_bstr_free_semantics)
{
    int v; NEEDB;
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        UCtx c; BSTR b = SysAllocString(L"free me"), n = NULL;
        uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        u_free(&c, UF_BSTR, v, &b, NULL);
        CHECK(b == NULL);
        u_free(&c, UF_BSTR, v, &b, NULL);     /* already NULL: no-op */
        CHECK(b == NULL);
        u_free(&c, UF_BSTR, v, &n, NULL);
        CHECK(n == NULL);
        if (!t_is_wine()) u_free(&c, UF_BSTR, v, NULL, NULL);   /* NULL BSTR*: no-op, no crash */
        uctx_free(&c);
    }
}

TEST(ums_bstr_32_and_64_agree_on_content)
{
    unsigned i; NEEDB;
    if (!u_have(UF_BSTR, 1)) { t_skip("no NDR64 exports"); return; }
    for (i = 0; i < ARRAY_N(bcases); i++) {
        BSTR b = mk(&bcases[i]), r[2] = { NULL, NULL }; int v;
        ULONG sz[2];
        for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
            UCtx c;
            uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            sz[v] = u_size(&c, UF_BSTR, v, 0, &b, NULL);
            uctx_free(&c);
            uctx_init(&c, sz[v], MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL);
            uctx_reread(&c, sz[v], MSHCTX_LOCAL);
            u_unmarshal(&c, UF_BSTR, v, c.buf, &r[v], NULL);
            uctx_free(&c);
        }
        CHECK((r[0] == NULL) == (r[1] == NULL));
        if (r[0] && r[1]) { EQ(SysStringByteLen(r[0]), SysStringByteLen(r[1])); CHECK(!memcmp(r[0], r[1], SysStringByteLen(r[0]))); }
        EQ(sz[1] - sz[0], 4);   /* only the header differs: 16 vs 12 */
        SysFreeString(b); SysFreeString(r[0]); SysFreeString(r[1]);
    }
}

TEST(ums_bstr_odd_byte_length_wire)
{
    /* the wire copies cnt WCHARs: the byte after an odd-length BSTR's last byte travels too (Wine test, extended) */
    BSTR b = SysAllocStringByteLen("abc", 3), r = NULL;
    UCtx c; int v; NEEDB;
    ((char *)b)[3] = 'd';
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR) {
        unsigned char *w, *m;
        uctx_init(&c, 32, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL);
        w = c.buf + (v ? 16 : 12);
        EQ(rd32(c.buf + (v ? 12 : 0)), 2);       /* cnt */
        EQ(rd32(c.buf + (v ? 8 : 4)), 3);        /* byteLen */
        CHECK(w[0] == 'a' && w[1] == 'b' && w[2] == 'c' && w[3] == 'd');
        EQ(m - c.buf, (v ? 16 : 12) + 4);
        uctx_reread(&c, 32, MSHCTX_LOCAL);
        u_unmarshal(&c, UF_BSTR, v, c.buf, &r, NULL);
        CHECK(r != NULL);
        if (r && !t_is_wine()) { EQ(SysStringByteLen(r), 3); CHECK(!memcmp(r, "abc", 3)); }
        SysFreeString(r); r = NULL; uctx_free(&c);
    }
    SysFreeString(b);
}
