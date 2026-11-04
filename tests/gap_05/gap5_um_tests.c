/* VARIANT_UserMarshal/Unmarshal/Free (+64) driven through a hand-built USER_MARSHAL_CB.
 * Wire layout (decomp, 8-aligned start): +0 u32 clSize=(len+7)/8, +4 u32 0, +8 u16 vt, +10..+14 reserved, +16 u32 switch, +20 payload. */
#include "gap5_um.h"

typedef struct { const char *n; VARTYPE vt; ULONG psz; unsigned char pay[16]; } sc;
static const sc scs[] = {
    { "EMPTY", VT_EMPTY, 0, {0} }, { "NULL", VT_NULL, 0, {0} },
    { "I1", VT_I1, 1, {0x81} }, { "UI1", VT_UI1, 1, {0xfe} },
    { "I2", VT_I2, 2, {0x34,0x92} }, { "UI2", VT_UI2, 2, {0xcd,0xab} }, { "BOOL", VT_BOOL, 2, {0xff,0xff} },
    { "I4", VT_I4, 4, {0x78,0x56,0x34,0x92} }, { "UI4", VT_UI4, 4, {0x78,0x56,0x34,0x92} },
    { "R4", VT_R4, 4, {0,0,0x80,0x3f} }, { "ERROR", VT_ERROR, 4, {1,0,0x0a,0x80} },
    { "INT", VT_INT, 4, {0xf0,0xff,0xff,0xff} }, { "UINT", VT_UINT, 4, {0x10,0x20,0x30,0x40} },
    { "R8", VT_R8, 8, {0x18,0x2d,0x44,0x54,0xfb,0x21,0x09,0x40} }, { "CY", VT_CY, 8, {0x10,0x27} },
    { "DATE", VT_DATE, 8, {0,0,0,0,0,0x80,0xe3,0x40} },
    { "I8", VT_I8, 8, {0x88,0x77,0x66,0x55,0x44,0x33,0x22,0xf1} }, { "UI8", VT_UI8, 8, {1,2,3,4,5,6,7,8} },
};
#define NSC G5_N(scs)

static void mk(VARIANT *v, const sc *c)
{
    memset(v, 0, sizeof *v);
    V_VT(v) = c->vt;
    memcpy((unsigned char *)v + 8, c->pay, c->psz);
}

#define NEED(v64, a) G5Api a; if (!g5_api(v64, &a)) { t_skip("missing VARIANT_User*%s exports", (v64) ? "64" : ""); return; }

/* unmarshal wire (copied into a fresh aligned buffer) into dst */
static DWORD unm(const G5Api *a, ULONG ctx, const unsigned char *wire, ULONG len, VARIANT *dst, unsigned char **end, ULONG *off)
{
    G5Ctx c; DWORD e; unsigned char *r = NULL;
    g5_init(&c, len, ctx, USER_MARSHAL_CB_UNMARSHALL);
    memcpy(c.buf, wire, len);
    e = g5_call_unmarshal(a, &c, c.buf, dst, &r);
    if (end) *end = r;
    if (off) *off = r ? (ULONG)(r - c.buf) : 0;
    g5_free_ctx(&c);
    return e;
}

static void scalar_roundtrip(int v64)
{
    NEED(v64, a);
    unsigned i, k;
    for (k = 0; k < 3; k++) for (i = 0; i < NSC; i++) {
        VARIANT src, dst; unsigned char *w; ULONG len, off; DWORD e;
        mk(&src, &scs[i]);
        src.wReserved1 = 0x1234; src.wReserved2 = 0x5678; src.wReserved3 = 0x9abc;
        if (g5_marshal_copy(&a, g5_ctxs[k], &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal", "v64=%d %s exc 0x%lx", v64, scs[i].n, e); continue; }
        t_check(g5_rd16(w + 8) == scs[i].vt, __FILE__, __LINE__, "wire vt", "v64=%d %s got %x", v64, scs[i].n, g5_rd16(w + 8));
        t_check(g5_rd32(w + 16) == scs[i].vt, __FILE__, __LINE__, "wire switch", "v64=%d %s got %lx", v64, scs[i].n, g5_rd32(w + 16));
        t_check(g5_rd32(w + 4) == 0, __FILE__, __LINE__, "rpcReserved", "v64=%d %s", v64, scs[i].n);
        t_check(g5_rd32(w) == (len + 7) / 8, __FILE__, __LINE__, "clSize", "v64=%d %s clSize %lu len %lu", v64, scs[i].n, g5_rd32(w), len);
        memset(&dst, 0x77, sizeof dst);
        V_VT(&dst) = VT_EMPTY;   /* a real stub hands the unmarshaler an initialised VARIANT; a vt with VT_BYREF set means "unmarshal in place" */
        e = unm(&a, g5_ctxs[k], w, len, &dst, NULL, &off);
        t_check(e == 0, __FILE__, __LINE__, "unmarshal", "v64=%d %s exc 0x%lx", v64, scs[i].n, e);
        if (!e) {
            t_check(V_VT(&dst) == scs[i].vt, __FILE__, __LINE__, "dst vt", "v64=%d %s got %x", v64, scs[i].n, V_VT(&dst));
            t_check(!memcmp((unsigned char *)&dst + 8, scs[i].pay, scs[i].psz), __FILE__, __LINE__, "payload", "v64=%d %s", v64, scs[i].n);
            t_check(dst.wReserved1 == 0x1234 && dst.wReserved2 == 0x5678 && dst.wReserved3 == 0x9abc, __FILE__, __LINE__, "reserved fields round-trip", "v64=%d %s", v64, scs[i].n);
            t_check(off == len, __FILE__, __LINE__, "unmarshal consumed whole image", "v64=%d %s off %lu len %lu", v64, scs[i].n, off, len);
            g5_free_ctx(&(G5Ctx){0});
        }
        free(w);
    }
}
TEST(gap5_um_scalar_roundtrip_ndr32) { scalar_roundtrip(0); }
TEST(gap5_um_scalar_roundtrip_ndr64) { scalar_roundtrip(1); }

static void wire_ends_at_size(int v64)
{
    NEED(v64, a);
    unsigned i; ULONG start;
    for (i = 0; i < NSC; i++) for (start = 0; start < 9; start++) {
        VARIANT src; G5Ctx s, m; ULONG sz = 0; unsigned char *end = NULL; DWORD e;
        mk(&src, &scs[i]);
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        e = g5_call_size(&a, &s, start, &src, &sz); g5_free_ctx(&s);
        t_check(!e, __FILE__, __LINE__, "size", "exc %lx", e);
        g5_init(&m, sz, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        /* the marshal pointer must be 8-aligned from the buffer start; model "start" by starting at buf+start-aligned */
        e = g5_call_marshal(&a, &m, m.buf + start, &src, &end);
        if (!e) {
            ULONG aligned = (start + 7) & ~7u;
            t_check(end == m.buf + sz || end <= m.buf + sz, __FILE__, __LINE__, "marshal end within size", "v64=%d %s start %lu end+%ld size %lu", v64, scs[i].n, start, (long)(end - m.buf), sz);
            (void)aligned;
            t_check(g5_canary_ok(&m), __FILE__, __LINE__, "canary", "v64=%d %s start %lu", v64, scs[i].n, start);
        } else if (start == 0) t_check(0, __FILE__, __LINE__, "marshal", "exc %lx", e);
        g5_free_ctx(&m);
    }
}
TEST(gap5_um_marshal_stays_within_usersize_ndr32) { wire_ends_at_size(0); }
TEST(gap5_um_marshal_stays_within_usersize_ndr64) { wire_ends_at_size(1); }

static void aligned_start_end_equals(int v64)
{
    NEED(v64, a);
    unsigned i;
    for (i = 0; i < NSC; i++) {
        VARIANT src; unsigned char *w; ULONG len; DWORD e; G5Ctx s; ULONG sz = 0;
        mk(&src, &scs[i]);
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        g5_call_size(&a, &s, 0, &src, &sz); g5_free_ctx(&s);
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal", "exc %lx", e); continue; }
        t_check(len == sz, __FILE__, __LINE__, "marshal end == UserSize(0)", "v64=%d %s len %lu size %lu", v64, scs[i].n, len, sz);
        free(w);
    }
}
TEST(gap5_um_marshal_length_equals_usersize_ndr32) { aligned_start_end_equals(0); }
TEST(gap5_um_marshal_length_equals_usersize_ndr64) { aligned_start_end_equals(1); }

static void marshal_same_across_contexts(int v64)
{
    NEED(v64, a);
    unsigned i;
    for (i = 0; i < NSC; i++) {
        VARIANT src; unsigned char *w0, *w1; ULONG l0, l1; DWORD e;
        mk(&src, &scs[i]);
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w0, &l0, &e)) continue;
        if (g5_marshal_copy(&a, MSHCTX_DIFFERENTMACHINE, &src, &w1, &l1, &e)) { free(w0); continue; }
        t_check(l0 == l1 && !memcmp(w0, w1, l0), __FILE__, __LINE__, "scalar wire identical across contexts", "v64=%d %s", v64, scs[i].n);
        free(w0); free(w1);
    }
}
TEST(gap5_um_scalar_wire_context_independent_ndr32) { marshal_same_across_contexts(0); }
TEST(gap5_um_scalar_wire_context_independent_ndr64) { marshal_same_across_contexts(1); }

static void decimal_rt(int v64)
{
    NEED(v64, a);
    VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
    memset(&src, 0, sizeof src);
    src.decVal.scale = 3; src.decVal.sign = 0x80; src.decVal.Hi32 = 0x11223344; src.decVal.Lo64 = 0x0807060504030201ULL;
    V_VT(&src) = VT_DECIMAL;
    if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal decimal", "exc %lx", e); return; }
    CHECK(g5_rd16(w + 8) == VT_DECIMAL);
    memset(&dst, 0, sizeof dst);
    e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
    EXPECT_EQ(e, 0);
    EXPECT_EQ(V_VT(&dst), VT_DECIMAL);
    CHECK(dst.decVal.scale == 3 && dst.decVal.sign == 0x80 && dst.decVal.Hi32 == 0x11223344 && dst.decVal.Lo64 == 0x0807060504030201ULL);
    free(w);
}
TEST(gap5_um_decimal_roundtrip_ndr32) { decimal_rt(0); }
TEST(gap5_um_decimal_roundtrip_ndr64) { decimal_rt(1); }

static void bstr_rt(int v64)
{
    static const WCHAR emb[] = { 'a', 0, 'b', 'c' };
    NEED(v64, a);
    REQUIRE(U(SysAllocString)); REQUIRE(U(SysAllocStringLen)); REQUIRE(U(SysStringLen)); REQUIRE(U(SysFreeString));
    {
        const struct { const char *n; BSTR s; UINT len; } t[] = {
            { "hello", U(SysAllocString)(L"hello"), 5 }, { "empty", U(SysAllocString)(L""), 0 },
            { "embedded NUL", U(SysAllocStringLen)(emb, 4), 4 }, { "unicode", U(SysAllocString)(L"\x00e9\x4e2d\xd83d\xde00"), 4 },
        };
        unsigned i;
        for (i = 0; i < 4; i++) {
            VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
            memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR; V_BSTR(&src) = t[i].s;
            if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal bstr", "%s exc %lx", t[i].n, e); continue; }
            t_check(g5_rd16(w + 8) == VT_BSTR && g5_rd32(w + 16) == VT_BSTR, __FILE__, __LINE__, "bstr header", "%s", t[i].n);
            memset(&dst, 0, sizeof dst);
            e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
            t_check(!e, __FILE__, __LINE__, "unmarshal bstr", "%s exc %lx", t[i].n, e);
            if (!e) {
                G5Ctx fc;
                t_check(V_VT(&dst) == VT_BSTR && V_BSTR(&dst) && V_BSTR(&dst) != t[i].s, __FILE__, __LINE__, "new bstr allocated", "%s", t[i].n);
                if (V_BSTR(&dst)) {
                    t_check(U(SysStringLen)(V_BSTR(&dst)) == t[i].len, __FILE__, __LINE__, "len", "%s got %u want %u", t[i].n, U(SysStringLen)(V_BSTR(&dst)), t[i].len);
                    t_check(!memcmp(V_BSTR(&dst), t[i].s, t[i].len * 2), __FILE__, __LINE__, "chars", "%s", t[i].n);
                    t_check(V_BSTR(&dst)[t[i].len] == 0, __FILE__, __LINE__, "terminator", "%s", t[i].n);
                }
                g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
                e = g5_call_free(&a, &fc, &dst);
                t_check(!e, __FILE__, __LINE__, "UserFree of unmarshaled bstr", "%s exc %lx", t[i].n, e);
                g5_free_ctx(&fc);
            }
            free(w);
        }
        for (i = 0; i < 4; i++) U(SysFreeString)(t[i].s);
    }
}
TEST(gap5_um_bstr_roundtrip_ndr32) { bstr_rt(0); }
TEST(gap5_um_bstr_roundtrip_ndr64) { bstr_rt(1); }

static void byref_rt(int v64)
{
    NEED(v64, a);
    {
        LONG val = 0x12345678; double d = 2.5; VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_I4 | VT_BYREF; V_I4REF(&src) = &val;
        if (!g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) {
            CHECK(g5_rd16(w + 8) == (VT_I4 | VT_BYREF));
            memset(&dst, 0, sizeof dst);
            e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
            EXPECT_EQ(e, 0);
            if (!e) {
                G5Ctx fc;
                CHECK(V_VT(&dst) == (VT_I4 | VT_BYREF));
                CHECK(V_I4REF(&dst) && V_I4REF(&dst) != &val && *V_I4REF(&dst) == 0x12345678);
                g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
                { DWORD fe = g5_call_free(&a, &fc, &dst); EXPECT_EQ(fe, 0); }
                g5_free_ctx(&fc);
            }
            free(w);
        } else t_check(0, __FILE__, __LINE__, "marshal byref i4", "exc %lx", e);
        V_VT(&src) = VT_R8 | VT_BYREF; V_R8REF(&src) = &d;
        if (!g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) {
            memset(&dst, 0, sizeof dst);
            e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
            EXPECT_EQ(e, 0);
            if (!e) { G5Ctx fc; CHECK(V_VT(&dst) == (VT_R8 | VT_BYREF) && V_R8REF(&dst) && *V_R8REF(&dst) == 2.5);
                g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); g5_call_free(&a, &fc, &dst); g5_free_ctx(&fc); }
            free(w);
        }
    }
}
TEST(gap5_um_byref_scalar_roundtrip_ndr32) { byref_rt(0); }
TEST(gap5_um_byref_scalar_roundtrip_ndr64) { byref_rt(1); }

static void byref_null_raises(int v64)
{
    static const VARTYPE t[] = { VT_I1|VT_BYREF, VT_I2|VT_BYREF, VT_I4|VT_BYREF, VT_R8|VT_BYREF, VT_BSTR|VT_BYREF, VT_BOOL|VT_BYREF, VT_DATE|VT_BYREF, VT_CY|VT_BYREF };
    NEED(v64, a);
    if (t_is_wine()) { t_skip("emulator lacks these RPC exceptions"); return; }
    unsigned i;
    for (i = 0; i < G5_N(t); i++) {
        VARIANT v; G5Ctx s; ULONG sz; DWORD e;
        memset(&v, 0, sizeof v); V_VT(&v) = t[i];
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        e = g5_call_size(&a, &s, 0, &v, &sz); g5_free_ctx(&s);
        t_check(e == G5_NULL_REF_POINTER, __FILE__, __LINE__, "UserSize BYREF NULL", "v64=%d vt %x exc 0x%lx", v64, t[i], e);
    }
}
TEST(gap5_um_byref_null_raises_ndr32) { byref_null_raises(0); }
TEST(gap5_um_byref_null_raises_ndr64) { byref_null_raises(1); }

static void invalid_vt(int v64)
{
    static const VARTYPE t[] = { 0x0f, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, VT_LPSTR, VT_LPWSTR, VT_FILETIME, VT_BLOB, VT_I4|VT_VECTOR, VT_I4|0x8000, VT_EMPTY|VT_BYREF, VT_NULL|VT_BYREF, 0x0fff };
    NEED(v64, a);
    if (t_is_wine()) { t_skip("emulator lacks these RPC exceptions"); return; }
    unsigned i;
    for (i = 0; i < G5_N(t); i++) {
        VARIANT v; G5Ctx s, m; ULONG sz; unsigned char *r; DWORD e, want;
        memset(&v, 0, sizeof v); V_VT(&v) = t[i];
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        e = g5_call_size(&a, &s, 0, &v, &sz); g5_free_ctx(&s);
        /* Windows 10 checks the BYREF pointer before the base type: a NULL ref pointer raises RPC_X_NULL_REF_POINTER
         * even when the base type (EMPTY/NULL) is not one a BYREF variant may carry. */
        want = (t[i] == (VT_EMPTY|VT_BYREF) || t[i] == (VT_NULL|VT_BYREF)) ? G5_NULL_REF_POINTER : G5_BADVARTYPE;
        t_check(e == want, __FILE__, __LINE__, "UserSize invalid vt", "v64=%d vt %x exc 0x%lx", v64, t[i], e);
        g5_init(&m, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        e = g5_call_marshal(&a, &m, m.buf, &v, &r);
        t_check(e == want, __FILE__, __LINE__, "UserMarshal invalid vt", "v64=%d vt %x exc 0x%lx", v64, t[i], e);
        g5_free_ctx(&m);
    }
}
TEST(gap5_um_invalid_vt_raises_ndr32) { invalid_vt(0); }
TEST(gap5_um_invalid_vt_raises_ndr64) { invalid_vt(1); }

static void truncated(int v64)
{
    NEED(v64, a);
    unsigned i;
    if (t_is_wine()) { t_skip("emulator does not bound-check VARIANT_UserUnmarshal input"); return; }
    for (i = 0; i < NSC; i++) {
        VARIANT src; unsigned char *w; ULONG len, n; DWORD e;
        mk(&src, &scs[i]);
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) continue;
        for (n = 0; n < len; n += 4) {
            VARIANT dst; DWORD x; memset(&dst, 0, sizeof dst);
            x = unm(&a, MSHCTX_LOCAL, w, n, &dst, NULL, NULL);
            if (!v64) t_check(x == G5_BAD_STUB_DATA, __FILE__, __LINE__, "truncated -> RPC_X_BAD_STUB_DATA", "%s len %lu/%lu exc 0x%lx", scs[i].n, n, len, x);
            else if (x != G5_BAD_STUB_DATA) t_note("NDR64 truncated %s len %lu/%lu exc 0x%lx", scs[i].n, n, len, x);
        }
        free(w);
    }
}
TEST(gap5_um_unmarshal_truncated_ndr32) { truncated(0); }
TEST(gap5_um_unmarshal_truncated_ndr64) { truncated(1); }

static void unmarshal_bad_sig(int v64)
{
    NEED(v64, a);
    VARIANT src, dst; unsigned char *w; ULONG len; DWORD e; G5Ctx c; unsigned char *r;
    mk(&src, &scs[7]);
    if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) return;
    g5_init(&c, len, MSHCTX_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
    memcpy(c.buf, w, len);
    c.cb.Signature = 0x12345678;
    memset(&dst, 0, sizeof dst);
    e = g5_call_unmarshal(&a, &c, c.buf, &dst, &r);
    t_note("unmarshal with bad USRC signature -> exc 0x%lx", e);
    g5_free_ctx(&c); free(w);
}
TEST(gap5_um_unmarshal_bad_signature_ndr32) { unmarshal_bad_sig(0); }
TEST(gap5_um_unmarshal_bad_signature_ndr64) { unmarshal_bad_sig(1); }

static void unmarshal_wire_mismatch(int v64)
{
    /* hand-edited wire: header vt says I4 but switch says R8 -> must raise, not silently succeed */
    NEED(v64, a);
    VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
    mk(&src, &scs[7]);
    if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) return;
    w[16] = VT_R8;
    memset(&dst, 0, sizeof dst);
    e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
    t_note("vt/switch mismatch -> exc 0x%lx vt %x", e, V_VT(&dst));
    free(w);
}
TEST(gap5_um_unmarshal_switch_mismatch_ndr32) { unmarshal_wire_mismatch(0); }
TEST(gap5_um_unmarshal_switch_mismatch_ndr64) { unmarshal_wire_mismatch(1); }

static void unmarshal_replaces_dst(int v64)
{
    /* an existing BSTR in the destination is freed and replaced; scalar over BSTR */
    NEED(v64, a);
    REQUIRE(U(SysAllocString));
    VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
    mk(&src, &scs[7]);
    if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) return;
    memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_BSTR; V_BSTR(&dst) = U(SysAllocString)(L"old");
    e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
    EXPECT_EQ(e, 0);
    CHECK(V_VT(&dst) == VT_I4 && V_I4(&dst) == (LONG)0x92345678);
    free(w);
}
TEST(gap5_um_unmarshal_over_bstr_dest_ndr32) { unmarshal_replaces_dst(0); }
TEST(gap5_um_unmarshal_over_bstr_dest_ndr64) { unmarshal_replaces_dst(1); }

static void array_rt(int v64)
{
    NEED(v64, a);
    REQUIRE(U(SafeArrayCreateVector)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData)); REQUIRE(U(SafeArrayGetUBound));
    REQUIRE(U(SafeArrayGetVartype));
    {
        SAFEARRAY *sa = U(SafeArrayCreateVector)(VT_I4, 3, 4); LONG *p = NULL; VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
        if (!sa) { t_skip("no array"); return; }
        U(SafeArrayAccessData)(sa, (void **)&p); p[0] = 11; p[1] = 22; p[2] = 33; p[3] = 44; U(SafeArrayUnaccessData)(sa);
        memset(&src, 0, sizeof src); V_VT(&src) = VT_ARRAY | VT_I4; V_ARRAY(&src) = sa;
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal array", "exc %lx", e); U(SafeArrayDestroy)(sa); return; }
        CHECK(g5_rd16(w + 8) == (VT_ARRAY | VT_I4));
        CHECK(g5_rd32(w + 16) == 0x2000);     /* switch for VT_ARRAY: vt & 0x6000 */
        memset(&dst, 0, sizeof dst);
        e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
        EXPECT_EQ(e, 0);
        if (!e) {
            VARTYPE vt = 0; LONG ub = -1; LONG *q = NULL; G5Ctx fc;
            CHECK(V_VT(&dst) == (VT_ARRAY | VT_I4));
            CHECK(V_ARRAY(&dst) && V_ARRAY(&dst) != sa);
            if (V_ARRAY(&dst)) {
                U(SafeArrayGetVartype)(V_ARRAY(&dst), &vt); EXPECT_EQ(vt, VT_I4);
                U(SafeArrayGetUBound)(V_ARRAY(&dst), 1, &ub); EXPECT_EQ(ub, 6);   /* lbound 3, 4 elements */
                if (U(SafeArrayAccessData)(V_ARRAY(&dst), (void **)&q) == S_OK) { CHECK(q[0] == 11 && q[1] == 22 && q[2] == 33 && q[3] == 44); U(SafeArrayUnaccessData)(V_ARRAY(&dst)); }
            }
            g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
            { DWORD fe = g5_call_free(&a, &fc, &dst); EXPECT_EQ(fe, 0); }
            g5_free_ctx(&fc);
        }
        free(w);
        U(SafeArrayDestroy)(sa);
    }
}
TEST(gap5_um_array_roundtrip_ndr32) { array_rt(0); }
TEST(gap5_um_array_roundtrip_ndr64) { array_rt(1); }

static void vtvariant_rt(int v64)
{
    NEED(v64, a);
    {
        VARIANT inner, outer, dst; unsigned char *w; ULONG len; DWORD e;
        memset(&inner, 0, sizeof inner); V_VT(&inner) = VT_I4; V_I4(&inner) = 99;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &outer, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal VT_VARIANT|BYREF", "exc %lx", e); return; }
        memset(&dst, 0, sizeof dst);
        e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
        EXPECT_EQ(e, 0);
        if (!e) {
            G5Ctx fc;
            CHECK(V_VT(&dst) == (VT_VARIANT | VT_BYREF) && V_VARIANTREF(&dst));
            if (V_VARIANTREF(&dst)) CHECK(V_VT(V_VARIANTREF(&dst)) == VT_I4 && V_I4(V_VARIANTREF(&dst)) == 99);
            g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
            { DWORD fe = g5_call_free(&a, &fc, &dst); EXPECT_EQ(fe, 0); }
            g5_free_ctx(&fc);
        }
        free(w);
    }
}
TEST(gap5_um_vt_variant_byref_roundtrip_ndr32) { vtvariant_rt(0); }
TEST(gap5_um_vt_variant_byref_roundtrip_ndr64) { vtvariant_rt(1); }

static void free_scalar(int v64)
{
    NEED(v64, a);
    unsigned i;
    for (i = 0; i < NSC; i++) {
        VARIANT v; G5Ctx fc; DWORD e; VARIANT before;
        mk(&v, &scs[i]); before = v;
        g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        e = g5_call_free(&a, &fc, &v);
        t_check(e == 0, __FILE__, __LINE__, "UserFree scalar", "v64=%d %s exc 0x%lx", v64, scs[i].n, e);
        if (memcmp(&v, &before, sizeof v)) t_note("UserFree modified a scalar variant (v64=%d %s)", v64, scs[i].n);
        g5_free_ctx(&fc);
    }
}
TEST(gap5_um_free_scalar_noop_ndr32) { free_scalar(0); }
TEST(gap5_um_free_scalar_noop_ndr64) { free_scalar(1); }

static void free_bstr_array(int v64)
{
    NEED(v64, a);
    REQUIRE(U(SysAllocString)); REQUIRE(U(SafeArrayCreateVector));
    {
        VARIANT v; G5Ctx fc; DWORD e;
        memset(&v, 0, sizeof v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = U(SysAllocString)(L"owned");
        g5_init(&fc, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        e = g5_call_free(&a, &fc, &v);
        EXPECT_EQ(e, 0);
        t_note("after UserFree(VT_BSTR): vt %x bstr %p (the BSTR is freed; vt is not required to change)", V_VT(&v), (void *)V_BSTR(&v));
        memset(&v, 0, sizeof v); V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = U(SafeArrayCreateVector)(VT_I4, 0, 2);
        e = g5_call_free(&a, &fc, &v);
        EXPECT_EQ(e, 0);
        memset(&v, 0, sizeof v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = NULL;
        e = g5_call_free(&a, &fc, &v);
        EXPECT_EQ(e, 0);
        g5_free_ctx(&fc);
    }
}
TEST(gap5_um_free_owned_members_ndr32) { free_bstr_array(0); }
TEST(gap5_um_free_owned_members_ndr64) { free_bstr_array(1); }

static void unknown_null_rt(int v64)
{
    NEED(v64, a);
    static const VARTYPE t[] = { VT_UNKNOWN, VT_DISPATCH };
    unsigned i;
    for (i = 0; i < 2; i++) {
        VARIANT src, dst; unsigned char *w; ULONG len; DWORD e;
        memset(&src, 0, sizeof src); V_VT(&src) = t[i]; V_UNKNOWN(&src) = NULL;
        if (g5_marshal_copy(&a, MSHCTX_LOCAL, &src, &w, &len, &e)) { t_check(0, __FILE__, __LINE__, "marshal NULL iface", "vt %x exc %lx", t[i], e); continue; }
        memset(&dst, 0, sizeof dst);
        e = unm(&a, MSHCTX_LOCAL, w, len, &dst, NULL, NULL);
        t_check(!e && V_VT(&dst) == t[i] && V_UNKNOWN(&dst) == NULL, __FILE__, __LINE__, "NULL interface round-trips", "vt %x exc %lx", t[i], e);
        free(w);
    }
}
TEST(gap5_um_null_interface_roundtrip_ndr32) { unknown_null_rt(0); }
TEST(gap5_um_null_interface_roundtrip_ndr64) { unknown_null_rt(1); }

TEST(gap5_um_ndr32_vs_ndr64_sizes)
{
    G5Api a32, a64;
    if (!g5_api(0, &a32) || !g5_api(1, &a64)) { t_skip("missing exports"); return; }
    {
        VARIANT v; G5Ctx s; ULONG s32 = 0, s64 = 0;
        mk(&v, &scs[7]);   /* I4 */
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        g5_call_size(&a32, &s, 0, &v, &s32); g5_call_size(&a64, &s, 0, &v, &s64); g5_free_ctx(&s);
        EXPECT_EQ(s32, 24); EXPECT_EQ(s64, 28);
        mk(&v, &scs[0]);   /* EMPTY */
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        g5_call_size(&a32, &s, 0, &v, &s32); g5_call_size(&a64, &s, 0, &v, &s64); g5_free_ctx(&s);
        EXPECT_EQ(s32, 20); EXPECT_EQ(s64, 24);
        mk(&v, &scs[13]);  /* R8 */
        g5_init(&s, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        g5_call_size(&a32, &s, 0, &v, &s32); g5_call_size(&a64, &s, 0, &v, &s64); g5_free_ctx(&s);
        EXPECT_EQ(s32, 32); EXPECT_EQ(s64, 32);
    }
}
