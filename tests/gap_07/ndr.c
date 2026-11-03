/* NdrConvert (data-representation conversion), NdrInterfacePointerFree/Unmarshall, NdrConformantString round trips.
 * All calls go through U(); stub messages are built by hand with the documented structure. */
#include "gap7.h"
#include <ndrtypes.h>

#define X_BAD_STUB      0x6f7   /* RPC_X_BAD_STUB_DATA */
#define X_INTERNAL      0x6e6   /* RPC_S_INTERNAL_ERROR */
#define X_INVALID_BOUND 0x6c6   /* RPC_X_INVALID_BOUND */

typedef struct {
    RPC_MESSAGE rpc; MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC desc;
    unsigned char *raw, *buf; ULONG cap;
} mctx;

static void *__RPC_API m_alloc(size_t n) { return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }
static void __RPC_API m_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

/* A bare message over a 16-byte aligned private copy of data (+ `skew` bytes of leading offset). */
static void m_make(mctx *c, const void *data, ULONG len, ULONG skew, ULONG rep, int client)
{
    memset(c, 0, sizeof *c);
    c->desc.pfnAllocate = m_alloc; c->desc.pfnFree = m_free; c->desc.Version = 0x20000; c->desc.MIDLVersion = 0x50100a4; c->desc.mFlags = 1;
    c->cap = len + skew + 64;
    c->raw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, c->cap + 16);
    c->buf = (unsigned char *)(((ULONG_PTR)c->raw + 15) & ~(ULONG_PTR)15);
    if (len) memcpy(c->buf + skew, data, len);
    c->rpc.DataRepresentation = rep;
    c->rpc.Buffer = c->buf; c->rpc.BufferLength = skew + len;
    c->sm.RpcMsg = &c->rpc; c->sm.StubDesc = &c->desc;
    c->sm.IsClient = (unsigned char)client;
    c->sm.pfnAllocate = m_alloc; c->sm.pfnFree = m_free;
    c->sm.BufferStart = c->buf; c->sm.Buffer = c->buf + skew; c->sm.BufferEnd = c->buf + skew + len;
}
static void m_done(mctx *c) { if (c->raw) HeapFree(GetProcessHeap(), 0, c->raw); c->raw = NULL; }

/* ======================= NdrConvert ======================= */
typedef struct { mctx *c; const unsigned char *fmt; } cv_arg;
static void do_convert(void *p) { cv_arg *a = p; U(NdrConvert)(&a->c->sm, (PFORMAT_STRING)a->fmt); }

/* returns exception code or 0 */
static DWORD convert(mctx *c, const unsigned char *fmt) { cv_arg a; a.c = c; a.fmt = fmt; return g7_try_here(do_convert, &a); }

TEST(gap7_ndrconvert_big_endian_swaps_each_basetype_width)
{
    static const struct { unsigned char type; unsigned width; } t[] = {
        { FC_WCHAR, 2 }, { FC_SHORT, 2 }, { FC_USHORT, 2 }, { FC_ENUM16, 2 },
        { FC_LONG, 4 }, { FC_ULONG, 4 }, { FC_ENUM32, 4 }, { FC_ERROR_STATUS_T, 4 },
        { FC_HYPER, 8 }, { FC_DOUBLE, 8 }, { FC_FLOAT, 4 },
        { FC_BYTE, 1 }, { FC_SMALL, 1 }, { FC_USMALL, 1 }, { FC_CHAR, 1 }, { FC_IGNORE, 0 } };
    unsigned i, k;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < NELEM(t); i++) {
        static const unsigned char data[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
        unsigned char fmt[4]; mctx c; DWORD x; unsigned char want[16];
        fmt[0] = FC_IN_PARAM_BASETYPE; fmt[1] = t[i].type; fmt[2] = FC_END; fmt[3] = 0;
        m_make(&c, data, 16, 0, 0x0000, 0);               /* server side, big-endian integers, ASCII, IEEE */
        memcpy(want, data, 16);
        for (k = 0; k < t[i].width / 2; k++) { want[k] = data[t[i].width - 1 - k]; want[t[i].width - 1 - k] = data[k]; }
        x = convert(&c, fmt);
        CHECKF(x == 0, "type 0x%02x raised 0x%lx", t[i].type, x);
        CHECKF(!memcmp(c.buf, want, 16), "type 0x%02x: buffer %02x %02x %02x %02x %02x %02x %02x %02x", t[i].type,
               c.buf[0], c.buf[1], c.buf[2], c.buf[3], c.buf[4], c.buf[5], c.buf[6], c.buf[7]);
        CHECKF(c.sm.Buffer == c.buf, "type 0x%02x: Buffer must be restored", t[i].type);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_little_endian_low_word_is_noop_regardless_of_high_word)
{
    static const ULONG reps[] = { 0x0010, 0xFFFF0010, 0x12340010 };
    static const unsigned char data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static const unsigned char fmt[] = { FC_IN_PARAM_BASETYPE, FC_LONG, FC_IN_PARAM_BASETYPE, FC_HYPER, FC_END };
    unsigned i;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < NELEM(reps); i++) {
        mctx c;
        m_make(&c, data, 8, 0, reps[i], 0);
        EQ(convert(&c, fmt), 0);
        CHECK(!memcmp(c.buf, data, 8));
        CHECK(c.sm.Buffer == c.buf);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_walks_consecutive_parameters_with_alignment)
{
    /* wire: short at 0, long aligned to 4, hyper aligned to 8, then a trailing byte */
    unsigned char data[32], want[32]; static const unsigned char fmt[] = {
        FC_IN_PARAM_BASETYPE, FC_SHORT, FC_IN_PARAM_BASETYPE, FC_LONG, FC_IN_PARAM_BASETYPE, FC_HYPER, FC_IN_PARAM_BASETYPE, FC_BYTE, FC_END };
    mctx c; unsigned i;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < 32; i++) data[i] = (unsigned char)(0x10 + i);
    memcpy(want, data, 32);
    want[0] = data[1]; want[1] = data[0];
    for (i = 0; i < 4; i++) want[4 + i] = data[7 - i];
    for (i = 0; i < 8; i++) want[8 + i] = data[15 - i];
    m_make(&c, data, 32, 0, 0x0000, 0);
    EQ(convert(&c, fmt), 0);
    CHECK(!memcmp(c.buf, want, 32));
    CHECK(c.sm.Buffer == c.buf);
    m_done(&c);
}
TEST(gap7_ndrconvert_respects_buffer_alignment_when_message_starts_misaligned)
{
    static const unsigned char data[8] = { 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x11, 0x22 };
    static const unsigned char fmt[] = { FC_IN_PARAM_BASETYPE, FC_LONG, FC_END };
    unsigned skew;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (skew = 0; skew < 4; skew++) {
        mctx c; unsigned char *aligned; unsigned char before[16];
        m_make(&c, data, 8, skew, 0x0000, 0);
        aligned = (unsigned char *)(((ULONG_PTR)(c.buf + skew) + 3) & ~(ULONG_PTR)3);
        memcpy(before, aligned, 4);
        EQ(convert(&c, fmt), 0);
        CHECKF(aligned[0] == before[3] && aligned[1] == before[2] && aligned[2] == before[1] && aligned[3] == before[0], "skew %u", skew);
        CHECKF(c.sm.Buffer == c.buf + skew, "skew %u: Buffer restored to the pre-call position", skew);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_client_and_server_convert_different_parameter_kinds)
{
    /* FC_IN_PARAM_BASETYPE is converted by the server only; FC_RETURN_PARAM_BASETYPE by the client only */
    static const unsigned char data[4] = { 1, 2, 3, 4 }, rev[4] = { 4, 3, 2, 1 };
    unsigned char in_fmt[] = { FC_IN_PARAM_BASETYPE, FC_LONG, FC_END }, ret_fmt[] = { FC_RETURN_PARAM_BASETYPE, FC_LONG, FC_END };
    mctx c;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    m_make(&c, data, 4, 0, 0x0000, 0); EQ(convert(&c, in_fmt), 0);  CHECK(!memcmp(c.buf, rev, 4)); m_done(&c);
    m_make(&c, data, 4, 0, 0x0000, 1); EQ(convert(&c, in_fmt), 0);  CHECK(!memcmp(c.buf, data, 4)); m_done(&c);
    m_make(&c, data, 4, 0, 0x0000, 1); EQ(convert(&c, ret_fmt), 0); CHECK(!memcmp(c.buf, rev, 4)); m_done(&c);
    m_make(&c, data, 4, 0, 0x0000, 0); EQ(convert(&c, ret_fmt), 0); CHECK(!memcmp(c.buf, data, 4)); m_done(&c);
}
TEST(gap7_ndrconvert_ebcdic_char_is_translated)
{
    static const struct { unsigned char e, a; } t[] = { { 0xC1, 'A' }, { 0xC2, 'B' }, { 0xF0, '0' }, { 0xF9, '9' }, { 0x40, ' ' }, { 0x81, 'a' } };
    static const unsigned char fmt[] = { FC_IN_PARAM_BASETYPE, FC_CHAR, FC_END };
    unsigned i;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < NELEM(t); i++) {
        mctx c;
        m_make(&c, &t[i].e, 1, 0, 0x0011, 0);               /* little-endian integers + EBCDIC characters */
        EQ(convert(&c, fmt), 0);
        CHECKF(c.buf[0] == t[i].a, "0x%02x -> 0x%02x want 0x%02x", t[i].e, c.buf[0], t[i].a);
        m_done(&c);
        m_make(&c, &t[i].e, 1, 0, 0x0010, 0);               /* ASCII rep: untouched */
        EQ(convert(&c, fmt), 0);
        CHECK(c.buf[0] == t[i].e);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_invalid_basetype_raises_internal_error)
{
    static const unsigned char bad[] = { 0x00, 0x11, 0x20, 0x35, 0x37, 0x80, 0xff };
    unsigned i;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < NELEM(bad); i++) {
        static const unsigned char data[16] = { 0 };
        unsigned char fmt[4]; mctx c;
        fmt[0] = FC_IN_PARAM_BASETYPE; fmt[1] = bad[i]; fmt[2] = FC_END; fmt[3] = 0;
        m_make(&c, data, 16, 0, 0x0000, 0);
        EQ(convert(&c, fmt), X_INTERNAL);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_truncated_buffer_raises_bad_stub_data)
{
    static const struct { unsigned char type; unsigned have; } t[] = { { FC_SHORT, 1 }, { FC_LONG, 3 }, { FC_LONG, 0 }, { FC_HYPER, 7 }, { FC_DOUBLE, 4 }, { FC_BYTE, 0 } };
    unsigned i;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    for (i = 0; i < NELEM(t); i++) {
        static const unsigned char data[16] = { 1, 2, 3, 4, 5, 6, 7, 8 };
        unsigned char fmt[4]; mctx c;
        fmt[0] = FC_IN_PARAM_BASETYPE; fmt[1] = t[i].type; fmt[2] = FC_END; fmt[3] = 0;
        m_make(&c, data, t[i].have, 0, 0x0000, 0);
        EQ(convert(&c, fmt), X_BAD_STUB);
        m_done(&c);
    }
}
TEST(gap7_ndrconvert_empty_format_changes_nothing)
{
    static const unsigned char data[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, fmt[] = { FC_END, 0, 0, 0 };
    mctx c;
    REQUIRE(U(NdrConvert));
    if (t_is_wine()) { t_skip("wine NdrConvert is a stub"); return; }
    m_make(&c, data, 8, 0, 0x0000, 0);
    EQ(convert(&c, fmt), 0);
    CHECK(!memcmp(c.buf, data, 8)); CHECK(c.sm.Buffer == c.buf);
    m_done(&c);
}

/* ======================= NdrInterfacePointerFree / Unmarshall ======================= */
typedef struct { void **vtbl; } fake_unk;
static volatile LONG f_qi, f_addref, f_release; static void *volatile f_release_this;
static HRESULT STDMETHODCALLTYPE fq(void *t, const IID *i, void **o) { InterlockedIncrement(&f_qi); return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE fa(void *t) { InterlockedIncrement(&f_addref); return 2; }
static ULONG STDMETHODCALLTYPE fr(void *t) { InterlockedIncrement(&f_release); f_release_this = t; return 1; }
static void *fake_vtbl[3] = { (void *)fq, (void *)fa, (void *)fr };
static fake_unk fake_obj = { fake_vtbl };
static void fake_reset(void) { f_qi = f_addref = f_release = 0; f_release_this = NULL; }

typedef struct { mctx *c; unsigned char *mem; unsigned char *fmt; } ip_arg;
static void do_ip_free(void *p) { ip_arg *a = p; U(NdrInterfacePointerFree)(&a->c->sm, a->mem, (PFORMAT_STRING)a->fmt); }
TEST(gap7_ndrinterfacepointerfree_calls_release_once)
{
    mctx c; ip_arg a; unsigned char fmt[4] = { FC_IP, FC_CONSTANT_IID, 0, 0 };
    REQUIRE(U(NdrInterfacePointerFree));
    if (t_is_wine()) { t_skip("not validated under wine"); return; }
    m_make(&c, NULL, 0, 0, 0x10, 0);
    a.c = &c; a.fmt = fmt;
    fake_reset(); a.mem = (unsigned char *)&fake_obj;
    EQ(g7_try_here(do_ip_free, &a), 0);
    EQ(f_release, 1); EQ(f_addref, 0); EQ(f_qi, 0);                  /* IUnknown slot 2 only */
    CHECK(f_release_this == &fake_obj);
    EQ(g7_try_here(do_ip_free, &a), 0);
    EQ(f_release, 2);                                                /* no state: every call releases */
    m_done(&c);
}
TEST(gap7_ndrinterfacepointerfree_null_is_ignored)
{
    mctx c; ip_arg a; unsigned char fmt[4] = { FC_IP, FC_CONSTANT_IID, 0, 0 };
    REQUIRE(U(NdrInterfacePointerFree));
    if (t_is_wine()) { t_skip("not validated under wine"); return; }
    m_make(&c, NULL, 0, 0, 0x10, 0);
    a.c = &c; a.fmt = fmt; a.mem = NULL; fake_reset();
    EQ(g7_try_here(do_ip_free, &a), 0);
    EQ(f_release, 0);
    m_done(&c);
}

typedef struct { mctx *c; unsigned char **pp; unsigned char *fmt; unsigned char must; unsigned char *ret; } ipu_arg;
static void do_ip_unm(void *p) { ipu_arg *a = p; a->ret = U(NdrInterfacePointerUnmarshall)(&a->c->sm, a->pp, (PFORMAT_STRING)a->fmt, a->must); }
TEST(gap7_ndrinterfacepointerunmarshall_null_wire_flag)
{
    static const unsigned char wire[8] = { 0, 0, 0, 0, 0xEE, 0xEE, 0xEE, 0xEE };
    unsigned char fmt[4] = { FC_IP, FC_CONSTANT_IID, 0, 0 }; unsigned skew, client;
    REQUIRE(U(NdrInterfacePointerUnmarshall));
    if (t_is_wine()) { t_skip("not validated under wine"); return; }
    for (client = 0; client < 2; client++)
        for (skew = 0; skew < 4; skew++) {
            mctx c; ipu_arg a; unsigned char *mem = (unsigned char *)(ULONG_PTR)0xdeadbeef; unsigned char *aligned;
            m_make(&c, wire, 8, skew, 0x10, client);
            aligned = (unsigned char *)(((ULONG_PTR)(c.buf + skew) + 3) & ~(ULONG_PTR)3);
            memset(c.buf + skew, 0, 8); memcpy(aligned, wire, 8);
            c.sm.BufferEnd = aligned + 8;
            a.c = &c; a.pp = &mem; a.fmt = fmt; a.must = 0; a.ret = (unsigned char *)1; fake_reset();
            if (client) mem = NULL;                            /* a client frees a non-NULL previous value: start clean */
            EQ(g7_try_here(do_ip_unm, &a), 0);
            CHECKF(mem == NULL, "client %u skew %u: *ppMemory %p", client, skew, mem);
            CHECK(a.ret == NULL);
            CHECKF(c.sm.Buffer == aligned + 4, "client %u skew %u: Buffer advanced by exactly the 4 byte flag", client, skew);
            m_done(&c);
        }
}
TEST(gap7_ndrinterfacepointerunmarshall_client_releases_previous_server_does_not)
{
    static const unsigned char wire[4] = { 0, 0, 0, 0 };
    unsigned char fmt[4] = { FC_IP, FC_CONSTANT_IID, 0, 0 }; unsigned client;
    REQUIRE(U(NdrInterfacePointerUnmarshall));
    if (t_is_wine()) { t_skip("not validated under wine"); return; }
    for (client = 0; client < 2; client++) {
        mctx c; ipu_arg a; unsigned char *mem = (unsigned char *)&fake_obj;
        m_make(&c, wire, 4, 0, 0x10, client);
        a.c = &c; a.pp = &mem; a.fmt = fmt; a.must = 0; fake_reset();
        EQ(g7_try_here(do_ip_unm, &a), 0);
        CHECK(mem == NULL);
        EQ(f_release, client ? 1 : 0);
        if (client) CHECK(f_release_this == &fake_obj);
        m_done(&c);
    }
}
TEST(gap7_ndrinterfacepointerunmarshall_short_buffer_raises_bad_stub_data)
{
    static const unsigned char wire[8] = { 0 };
    unsigned char fmt[4] = { FC_IP, FC_CONSTANT_IID, 0, 0 }; unsigned have;
    REQUIRE(U(NdrInterfacePointerUnmarshall));
    if (t_is_wine()) { t_skip("not validated under wine"); return; }
    for (have = 0; have < 4; have++) {
        mctx c; ipu_arg a; unsigned char *mem = (unsigned char *)(ULONG_PTR)0xdeadbeef;
        m_make(&c, wire, have, 0, 0x10, 0);
        a.c = &c; a.pp = &mem; a.fmt = fmt; a.must = 0;
        EQ(g7_try_here(do_ip_unm, &a), X_BAD_STUB);
        CHECKF(mem == NULL, "have %u: *ppMemory is cleared before the length check", have);
        m_done(&c);
    }
}

/* ======================= conformant string marshal / unmarshal round trips ======================= */
typedef struct { mctx *c; unsigned char *mem; unsigned char *fmt; unsigned char **pp; unsigned char must; } cs_arg;
static void do_cs_buf(void *p) { cs_arg *a = p; U(NdrConformantStringBufferSize)(&a->c->sm, a->mem, (PFORMAT_STRING)a->fmt); }
static void do_cs_marshal(void *p) { cs_arg *a = p; U(NdrConformantStringMarshall)(&a->c->sm, a->mem, (PFORMAT_STRING)a->fmt); }
static void do_cs_unmarshal(void *p) { cs_arg *a = p; U(NdrConformantStringUnmarshall)(&a->c->sm, a->pp, (PFORMAT_STRING)a->fmt, a->must); }

static void string_roundtrip(int wide)
{
    static const unsigned char fmt_c[] = { FC_C_CSTRING, FC_PAD, 0, 0 }, fmt_w[] = { FC_C_WSTRING, FC_PAD, 0, 0 };
    const unsigned char *fmt = wide ? fmt_w : fmt_c;
    unsigned len, skew;
    for (len = 0; len <= 70; len += (len < 9 ? 1 : 13))
        for (skew = 0; skew < 4; skew++) {
            union { char a[128]; WCHAR w[128]; } s; mctx c; cs_arg a; unsigned i, used, expect_data; unsigned char *start, *out = NULL;
            if (wide) { for (i = 0; i < len; i++) s.w[i] = (WCHAR)(0x41 + i % 26 + (i % 5 == 0 ? 0x100 : 0)); s.w[len] = 0; }
            else { for (i = 0; i < len; i++) s.a[i] = (char)(0x41 + i % 26); s.a[len] = 0; }
            expect_data = (len + 1) * (wide ? 2 : 1);
            /* 1. BufferSize */
            m_make(&c, NULL, 0, 0, 0x10, 1);
            c.sm.BufferLength = skew;
            a.c = &c; a.mem = (unsigned char *)&s; a.fmt = (unsigned char *)fmt;
            EQ(g7_try_here(do_cs_buf, &a), 0);
            CHECKF(c.sm.BufferLength >= skew + 12 + expect_data, "len %u: BufferLength %lu", len, (unsigned long)c.sm.BufferLength);
            {   ULONG need = c.sm.BufferLength; m_done(&c);
                /* 2. Marshal into a buffer of exactly that size */
                m_make(&c, NULL, 0, 0, 0x10, 1);
                HeapFree(GetProcessHeap(), 0, c.raw); c.raw = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, need + 64);
                c.buf = (unsigned char *)(((ULONG_PTR)c.raw + 15) & ~(ULONG_PTR)15);
                memset(c.buf, 0xCD, need + 32);
                c.sm.BufferStart = c.buf; c.sm.Buffer = c.buf + skew; c.sm.BufferEnd = c.buf + need; c.sm.BufferLength = need; c.rpc.Buffer = c.buf;
                start = c.sm.Buffer;
                a.c = &c;
                EQ(g7_try_here(do_cs_marshal, &a), 0);
                used = (unsigned)(c.sm.Buffer - start);
                CHECKF(c.sm.Buffer <= c.sm.BufferEnd, "len %u skew %u: marshal overran BufferLength", len, skew);
                CHECKF(used >= 12 + expect_data, "len %u skew %u: used %u", len, skew, used);
                {   unsigned char *al = (unsigned char *)(((ULONG_PTR)start + 3) & ~(ULONG_PTR)3); unsigned *h = (unsigned *)al;
                    CHECKF(h[0] == len + 1 && h[1] == 0 && h[2] == len + 1, "len %u: header %u %u %u", len, h[0], h[1], h[2]);
                    CHECK(!memcmp(al + 12, &s, expect_data));
                }
                /* 3. Unmarshal as the server (points into the buffer) and as a client with must-alloc */
                c.sm.IsClient = 0; c.sm.Buffer = start; c.sm.BufferEnd = c.buf + need; c.rpc.BufferLength = need;
                a.pp = &out; a.must = 0; out = NULL;
                EQ(g7_try_here(do_cs_unmarshal, &a), 0);
                CHECKF(out != NULL && !memcmp(out, &s, expect_data), "len %u skew %u: server unmarshal", len, skew);
                CHECKF(c.sm.Buffer >= start + 12 + expect_data && c.sm.Buffer <= c.sm.BufferEnd, "len %u skew %u: Buffer after unmarshal", len, skew);
                c.sm.IsClient = 1; c.sm.Buffer = start; out = NULL; a.must = 1;
                EQ(g7_try_here(do_cs_unmarshal, &a), 0);
                CHECKF(out != NULL && (out < c.buf || out >= c.buf + need + 64), "len %u: must-alloc result must be a fresh copy", len);
                CHECK(out && !memcmp(out, &s, expect_data));
                if (out) HeapFree(GetProcessHeap(), 0, out);
                m_done(&c);
            }
        }
}
TEST(gap7_ndrconformantstring_char_roundtrip_all_alignments)
{
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrConformantStringMarshall)); REQUIRE(U(NdrConformantStringUnmarshall));
    string_roundtrip(0);
}
TEST(gap7_ndrconformantstring_wchar_roundtrip_all_alignments)
{
    REQUIRE(U(NdrConformantStringBufferSize)); REQUIRE(U(NdrConformantStringMarshall)); REQUIRE(U(NdrConformantStringUnmarshall));
    string_roundtrip(1);
}
TEST(gap7_ndrconformantstring_unmarshal_rejects_nonzero_offset_and_inconsistent_counts)
{
    static const unsigned char fmt[] = { FC_C_CSTRING, FC_PAD, 0, 0 };
    static const struct { unsigned max, off, act; DWORD want; } t[] = {
        { 8, 1, 4, X_INVALID_BOUND },       /* offset must be zero */
        { 8, 0, 9, X_INVALID_BOUND },       /* actual count above max count */
        { 3, 0, 4, X_INVALID_BOUND },
        { 8, 0, 4, X_INVALID_BOUND },       /* unsized string: max count must equal actual count */
        { 8, 0, 0, X_INVALID_BOUND },       /* actual count 0 is never valid */
        { 4, 0, 4, 0 },
        { 1, 0, 1, 0 },
        { 2, 0, 2, 0 },
    };
    unsigned i;
    REQUIRE(U(NdrConformantStringUnmarshall));
    for (i = 0; i < NELEM(t); i++) {
        unsigned char wire[32]; mctx c; cs_arg a; unsigned char *out = NULL; DWORD x;
        memset(wire, 'x', sizeof wire);
        memcpy(wire, &t[i].max, 4); memcpy(wire + 4, &t[i].off, 4); memcpy(wire + 8, &t[i].act, 4);
        if (t[i].act) { memcpy(wire + 12, "abc", 3); wire[12 + t[i].act - 1] = 0; }
        m_make(&c, wire, sizeof wire, 0, 0x10, 0);
        a.c = &c; a.pp = &out; a.fmt = (unsigned char *)fmt; a.must = 1;
        x = g7_try_here(do_cs_unmarshal, &a);
        CHECKF(x == t[i].want, "max %u off %u act %u: raised 0x%lx want 0x%lx", t[i].max, t[i].off, t[i].act, x, t[i].want);
        if (out && !x) HeapFree(GetProcessHeap(), 0, out);
        m_done(&c);
    }
}
