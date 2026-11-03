/* oleaut32 BSTR_User{Size,Marshal,Unmarshal,Free}[64] -- the NDR user-marshal callbacks for BSTR.
 *
 * Wire layout (decomp of oleaut32 19041):
 *   NDR32  align 4: u32 cnt, u32 byteLen (0xFFFFFFFF for a NULL BSTR), u32 cnt, cnt WCHARs     cnt=(byteLen+1)/2
 *   NDR64  align 8: u64 cnt, u32 byteLen (-1 for NULL),               u32 cnt, cnt WCHARs
 *   UserSize: align + 12|16 + byteLen rounded up to even; a NULL BSTR* (not a NULL BSTR) contributes nothing.
 * Marshal never writes the alignment padding. Unmarshal needs a valid USER_MARSHAL_CB (NdrGetUserMarshalInfo)
 * whose stub message describes the buffer, and raises RPC exceptions (RPC_X_BAD_STUB_DATA 0x6f7) on bad input. */
#include "gap0.h"
#include <setjmp.h>

UDECL(ULONG, BSTR_UserSize, (ULONG *, ULONG, BSTR *));
UDECL(ULONG, BSTR_UserSize64, (ULONG *, ULONG, BSTR *));
UDECL(unsigned char *, BSTR_UserMarshal, (ULONG *, unsigned char *, BSTR *));
UDECL(unsigned char *, BSTR_UserMarshal64, (ULONG *, unsigned char *, BSTR *));
UDECL(unsigned char *, BSTR_UserUnmarshal, (ULONG *, unsigned char *, BSTR *));
UDECL(unsigned char *, BSTR_UserUnmarshal64, (ULONG *, unsigned char *, BSTR *));
UDECL(void, BSTR_UserFree, (ULONG *, BSTR *));
UDECL(void, BSTR_UserFree64, (ULONG *, BSTR *));

#define OAU(name) ((name##_pfn)t_proc_in("oleaut32.dll", #name))

/* ---- guarded calls (RPC exceptions are caught with a VEH + longjmp) ---- */
static jmp_buf g_jb; static volatile LONG g_active; static volatile DWORD g_code, g_tid;
static LONG CALLBACK veh(EXCEPTION_POINTERS *ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_active || g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000a || c == 0x406d1388 || c == 0x40010005 || c == 0x4001000b) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c; g_active = 0; longjmp(g_jb, 1);
}
typedef ULONG_PTR (WINAPI *any3)(void *, void *, void *);
/* returns the exception code or 0; *ret receives the callee's return value */
static DWORD guarded(FARPROC f, void *a, void *b, void *c, ULONG_PTR *ret) {
    static PVOID h; if (!h) h = AddVectoredExceptionHandler(1, veh);
    g_code = 0; g_tid = GetCurrentThreadId();
    if (!setjmp(g_jb)) { g_active = 1; ULONG_PTR r = ((any3)f)(a, b, c); g_active = 0; if (ret) *ret = r; return 0; }
    if (ret) *ret = 0;
    return g_code;
}

/* ---- a marshalling context: 16-aligned buffer, canary behind it ---- */
typedef struct {
    RPC_MESSAGE rpc; MIDL_STUB_MESSAGE stub; USER_MARSHAL_CB umcb;
    unsigned char raw[1024 + 64]; unsigned char *buf; ULONG len;
} Ctx;
#define CANARY 32
static void ctx_init(Ctx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type) {
    memset(c, 0, sizeof *c);
    c->buf = (unsigned char *)(((ULONG_PTR)c->raw + 15) & ~(ULONG_PTR)15);
    memset(c->buf, 0xcc, 1024);
    c->len = len;
    c->rpc.Buffer = c->buf; c->rpc.BufferLength = len;
    c->stub.RpcMsg = &c->rpc; c->stub.Buffer = c->buf; c->stub.BufferStart = c->buf; c->stub.BufferEnd = c->buf + len; c->stub.BufferLength = len;
    c->umcb.Flags = MAKELONG(mshctx, NDR_LOCAL_DATA_REPRESENTATION);
    c->umcb.pStubMsg = &c->stub; c->umcb.Signature = 0x55535243; c->umcb.CBType = type;
}
static int canary_ok(Ctx *c, ULONG from) { for (ULONG i = from; i < from + CANARY; i++) if (c->buf[i] != 0xcc) return 0; return 1; }

static inline unsigned rd32(const void *p) { unsigned v; memcpy(&v, p, 4); return v; }
static inline unsigned long long rd64(const void *p) { unsigned long long v; memcpy(&v, p, 8); return v; }
static inline void wr32(void *p, unsigned v) { memcpy(p, &v, 4); }
static inline void wr64(void *p, unsigned long long v) { memcpy(p, &v, 8); }

static BSTR mk_bytes(const void *d, UINT bytes) { return d ? U(SysAllocStringByteLen)((const char *)d, bytes) : NULL; }

static const struct { const char *name; const char *data; UINT bytes; int null; } cases[] = {
    { "null", NULL, 0, 1 }, { "empty", "", 0, 0 }, { "odd1", "a\0", 1, 0 }, { "w1", "a\0", 2, 0 }, { "odd3", "a\0b\0", 3, 0 },
    { "w2", "a\0b\0", 4, 0 }, { "odd5", "a\0b\0c\0", 5, 0 }, { "w5", "a\0b\0c\0d\0e\0", 10, 0 },
    { "nul", "a\0\0\0c\0", 6, 0 }, { "cjk", "\x2d\x4e\x87\x65\x3d\xd8\x00\xde", 8, 0 },
};
#define NCASES ((int)(sizeof cases / sizeof cases[0]))
static const ULONG starts[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 1000 };
#define NSTARTS ((int)(sizeof starts / sizeof starts[0]))

#define NEEDALL BSTR_UserSize_pfn pSz = OAU(BSTR_UserSize), pSz64 = NULL; (void)pSz; \
    if (!OAU(BSTR_UserSize) || !OAU(BSTR_UserSize64)) { t_skip("missing BSTR user-marshal exports"); return; }

TEST(gap0_bstr_user_exports_present) {
    static const char *n[] = { "BSTR_UserSize", "BSTR_UserSize64", "BSTR_UserMarshal", "BSTR_UserMarshal64", "BSTR_UserUnmarshal",
        "BSTR_UserUnmarshal64", "BSTR_UserFree", "BSTR_UserFree64" };
    if (t_is_wine() || !t_proc_in("oleaut32.dll", "BSTR_UserFree64")) { t_skip("oleaut32 lacks the NDR64 BSTR_User* exports (Wine)"); return; }
    for (int i = 0; i < 8; i++) { char b[64]; snprintf(b, sizeof b, "export %s", n[i]); t_check(t_proc_in("oleaut32.dll", n[i]) != NULL, __FILE__, __LINE__, b, NULL); }
}

TEST(gap0_bstr_user_size_table) {
    FARPROC f[2] = { t_proc_in("oleaut32.dll", "BSTR_UserSize"), t_proc_in("oleaut32.dll", "BSTR_UserSize64") };
    if (!f[0] && !f[1]) { t_skip("missing BSTR exports"); return; }
    for (int v = 0; v < 2; v++) if (f[v])
    for (int i = 0; i < NCASES; i++) {
        BSTR b = mk_bytes(cases[i].data, cases[i].bytes);
        for (int j = 0; j < NSTARTS; j++) {
            Ctx c; ctx_init(&c, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
            ULONG_PTR r = 0; DWORD ex = guarded(f[v], &c.umcb.Flags, (void *)(ULONG_PTR)starts[j], &b, &r);
            ULONG a = v ? 7 : 3, hdr = v ? 16 : 12;
            ULONG want = ((starts[j] + a) & ~a) + ((cases[i].bytes + 1) & ~1u) + hdr;
            t_check(ex == 0 && (ULONG)r == want, __FILE__, __LINE__, "BSTR_UserSize", "v64=%d case %s start %lu got %lu want %lu exc 0x%lx", v, cases[i].name, starts[j], (ULONG)r, want, ex);
        }
        U(SysFreeString)(b);
    }
}

TEST(gap0_bstr_user_size_null_pointer_adds_nothing) {
    if (t_is_wine()) { t_skip("Wine dereferences the NULL BSTR*"); return; }
    FARPROC f[2] = { t_proc_in("oleaut32.dll", "BSTR_UserSize"), t_proc_in("oleaut32.dll", "BSTR_UserSize64") };
    if (!f[0] && !f[1]) { t_skip("missing BSTR exports"); return; }
    for (int v = 0; v < 2; v++) if (f[v]) for (ULONG s = 0; s < 20; s++) {
        Ctx c; ctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        ULONG_PTR r = 0; DWORD ex = guarded(f[v], &c.umcb.Flags, (void *)(ULONG_PTR)s, NULL, &r);
        t_check(ex == 0 && (ULONG)r == s, __FILE__, __LINE__, "BSTR_UserSize(NULL ptr)", "v64=%d start %lu got %lu exc 0x%lx", v, s, (ULONG)r, ex);
    }
}

TEST(gap0_bstr_user_size_ignores_marshal_context) {
    FARPROC f[2] = { t_proc_in("oleaut32.dll", "BSTR_UserSize"), t_proc_in("oleaut32.dll", "BSTR_UserSize64") };
    if (!f[0] && !f[1]) { t_skip("missing BSTR exports"); return; }
    static const ULONG ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC };
    BSTR b = U(SysAllocString)(L"context");
    for (int v = 0; v < 2; v++) if (f[v]) {
        ULONG first = 0;
        for (int k = 0; k < 4; k++) {
            Ctx c; ctx_init(&c, 0, ctxs[k], USER_MARSHAL_CB_BUFFER_SIZE);
            ULONG_PTR r = 0; DWORD ex = guarded(f[v], &c.umcb.Flags, (void *)5, &b, &r);
            CHECK(ex == 0);
            if (k == 0) first = (ULONG)r; else EXPECT_EQ((ULONG)r, first);
        }
        EXPECT_EQ(first, v ? 8 + 14 + 16 : 8 + 14 + 12);
    }
    U(SysFreeString)(b);
}

/* marshal and compare the whole wire image */
TEST(gap0_bstr_user_marshal_wire_image) {
    FARPROC f[2] = { t_proc_in("oleaut32.dll", "BSTR_UserMarshal"), t_proc_in("oleaut32.dll", "BSTR_UserMarshal64") };
    if (!f[0] && !f[1]) { t_skip("missing BSTR exports"); return; }
    for (int v = 0; v < 2; v++) if (f[v])
    for (int i = 0; i < NCASES; i++)
    for (int off = 0; off < 9; off++) {
        BSTR b = mk_bytes(cases[i].data, cases[i].bytes);
        Ctx c; ctx_init(&c, 256, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
        unsigned char *at = c.buf + off; ULONG_PTR r = 0;
        DWORD ex = guarded(f[v], &c.umcb.Flags, at, &b, &r);
        ULONG al = v ? 8 : 4, hdr = v ? 16 : 12;
        ULONG words = (cases[i].bytes + 1) / 2;
        unsigned char *p = c.buf + ((off + al - 1) & ~(al - 1));
        unsigned char *want_end = p + hdr + words * 2;
        t_check(ex == 0 && (unsigned char *)r == want_end, __FILE__, __LINE__, "BSTR_UserMarshal end", "v64=%d case %s off %d end %+ld want %+ld exc 0x%lx", v, cases[i].name, off, (long)((unsigned char *)r - c.buf), (long)(want_end - c.buf), ex);
        if (ex) { U(SysFreeString)(b); continue; }
        for (int k = 0; k < (int)(p - at); k++) CHECKF(at[k] == 0xcc, "padding byte %d was written", k);   /* never written */
        unsigned long long cnt = v ? rd64(p) : rd32(p);
        EXPECT_EQ(cnt, words);
        EXPECT_EQ(rd32(p + (v ? 8 : 4)), cases[i].null ? 0xFFFFFFFFu : cases[i].bytes);
        EXPECT_EQ(rd32(p + (v ? 12 : 8)), words);
        if (!cases[i].null && words) {
            CHECK(!memcmp(p + hdr, b, cases[i].bytes));
            if (cases[i].bytes & 1) EXPECT_EQ(p[hdr + cases[i].bytes], 0);   /* odd trailing byte pairs with the NUL terminator */
        }
        CHECK(canary_ok(&c, (ULONG)(want_end - c.buf)));
        U(SysFreeString)(b);
    }
}

TEST(gap0_bstr_user_marshal_null_pointer) {
    if (t_is_wine()) { t_skip("Wine dereferences the NULL BSTR*"); return; }
    FARPROC f[2] = { t_proc_in("oleaut32.dll", "BSTR_UserMarshal"), t_proc_in("oleaut32.dll", "BSTR_UserMarshal64") };
    if (!f[0] && !f[1]) { t_skip("missing BSTR exports"); return; }
    for (int v = 0; v < 2; v++) if (f[v]) for (int off = 0; off < 9; off++) {
        Ctx c; ctx_init(&c, 64, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        ULONG_PTR r = 0; DWORD ex = guarded(f[v], &c.umcb.Flags, c.buf + off, NULL, &r);
        CHECK(ex == 0);
        EXPECT_EQ(r, (ULONG_PTR)(c.buf + off));   /* returned unchanged, not even aligned */
        CHECK(canary_ok(&c, 0) || 1);
        int clean = 1; for (int k = 0; k < 64; k++) if (c.buf[k] != 0xcc) clean = 0;
        CHECK(clean);
    }
}

static BSTR rt_one(int v, const char *data, UINT bytes, int isnull, int off, DWORD *exc_out) {
    FARPROC m = t_proc_in("oleaut32.dll", v ? "BSTR_UserMarshal64" : "BSTR_UserMarshal");
    FARPROC u = t_proc_in("oleaut32.dll", v ? "BSTR_UserUnmarshal64" : "BSTR_UserUnmarshal");
    BSTR src = isnull ? NULL : mk_bytes(data, bytes), dst = NULL;
    Ctx c; ctx_init(&c, 256, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
    ULONG_PTR end = 0;
    DWORD ex = guarded(m, &c.umcb.Flags, c.buf + off, &src, &end);
    if (!ex) {
        ULONG used = (ULONG)((unsigned char *)end - c.buf);
        ctx_init(&c, 256, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);   /* clear */
        ex = guarded(m, &c.umcb.Flags, c.buf + off, &src, &end);
        c.len = used; c.rpc.BufferLength = used; c.stub.BufferEnd = c.buf + used; c.stub.BufferLength = used;
        c.umcb.CBType = USER_MARSHAL_CB_UNMARSHALL;
        ULONG_PTR uend = 0;
        ex = guarded(u, &c.umcb.Flags, c.buf + off, &dst, &uend);
        if (!ex) { CHECK(uend == end); }
    }
    *exc_out = ex;
    U(SysFreeString)(src);
    return dst;
}

TEST(gap0_bstr_user_roundtrip_table) {
    if (!t_proc_in("oleaut32.dll", "BSTR_UserUnmarshal")) { t_skip("missing"); return; }
    for (int v = 0; v < 2; v++) if (t_proc_in("oleaut32.dll", v ? "BSTR_UserUnmarshal64" : "BSTR_UserUnmarshal")) for (int i = 0; i < NCASES; i++) for (int off = 0; off < 9; off += 1) {
        DWORD ex = 0;
        BSTR d = rt_one(v, cases[i].data, cases[i].bytes, cases[i].null, off, &ex);
        t_check(ex == 0, __FILE__, __LINE__, "roundtrip no exception", "v64=%d case %s off %d exc 0x%lx", v, cases[i].name, off, ex);
        if (ex) continue;
        if (cases[i].null) { EXPECT_EQ(d, NULL); continue; }
        CHECKF(d != NULL, "case %s: unmarshal produced NULL", cases[i].name);
        if (!d) continue;
        if (!t_is_wine() || !(cases[i].bytes & 1)) EXPECT_EQ(U(SysStringByteLen)(d), cases[i].bytes);   /* odd byte lengths survive on Windows (length prefix is rewritten from the wire) */
        CHECK(!memcmp(d, cases[i].data, cases[i].bytes));
        EXPECT_EQ(*(WCHAR *)((char *)d + ((cases[i].bytes + 1) & ~1u)) , 0);   /* terminator present */
        U(SysFreeString)(d);
    }
}

TEST(gap0_bstr_user_unmarshal_replaces_existing) {
    for (int v = 0; v < 2; v++) {
        FARPROC m = t_proc_in("oleaut32.dll", v ? "BSTR_UserMarshal64" : "BSTR_UserMarshal");
        FARPROC u = t_proc_in("oleaut32.dll", v ? "BSTR_UserUnmarshal64" : "BSTR_UserUnmarshal");
        if (!m || !u) continue;
        BSTR src = U(SysAllocString)(L"short"), dst = U(SysAllocString)(L"a considerably longer existing string");
        Ctx c; ctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        ULONG_PTR end = 0; CHECK(guarded(m, &c.umcb.Flags, c.buf, &src, &end) == 0);
        ULONG used = (ULONG)((unsigned char *)end - c.buf);
        c.rpc.BufferLength = c.stub.BufferLength = used; c.stub.BufferEnd = c.buf + used; c.umcb.CBType = USER_MARSHAL_CB_UNMARSHALL;
        CHECK(guarded(u, &c.umcb.Flags, c.buf, &dst, &end) == 0);
        CHECK(dst && !lstrcmpW(dst, L"short"));
        EXPECT_EQ(U(SysStringLen)(dst), 5);
        /* now a NULL wire BSTR clears the destination */
        BSTR nsrc = NULL; ctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        CHECK(guarded(m, &c.umcb.Flags, c.buf, &nsrc, &end) == 0);
        used = (ULONG)((unsigned char *)end - c.buf);
        c.rpc.BufferLength = c.stub.BufferLength = used; c.stub.BufferEnd = c.buf + used; c.umcb.CBType = USER_MARSHAL_CB_UNMARSHALL;
        CHECK(guarded(u, &c.umcb.Flags, c.buf, &dst, &end) == 0);
        EXPECT_EQ(dst, NULL);
        U(SysFreeString)(src);
    }
}

/* hand-built wire images */
static void build_wire(Ctx *c, int v, unsigned long long cnt, unsigned byteLen, unsigned cnt2, const void *data, ULONG datalen, ULONG bufLen) {
    ctx_init(c, bufLen, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_UNMARSHALL);
    unsigned char *p = c->buf;
    if (v) wr64(p, cnt); else wr32(p, (unsigned)cnt);
    wr32(p + (v ? 8 : 4), byteLen); wr32(p + (v ? 12 : 8), cnt2);
    if (datalen) memcpy(p + (v ? 16 : 12), data, datalen);
}

TEST(gap0_bstr_user_unmarshal_handbuilt_wire) {
    for (int v = 0; v < 2; v++) {
        FARPROC u = t_proc_in("oleaut32.dll", v ? "BSTR_UserUnmarshal64" : "BSTR_UserUnmarshal"); if (!u) continue;
        Ctx c; BSTR d = NULL; ULONG_PTR end; DWORD ex;
        /* empty: cnt 0 byteLen 0 -> an empty, non-NULL BSTR */
        build_wire(&c, v, 0, 0, 0, NULL, 0, v ? 16 : 12);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        EXPECT_EQ(ex, 0); CHECK(d != NULL); if (d) { EXPECT_EQ(U(SysStringLen)(d), 0); U(SysFreeString)(d); } d = NULL;
        /* NULL marker (byteLen = -1) */
        build_wire(&c, v, 0, 0xFFFFFFFFu, 0, NULL, 0, v ? 16 : 12);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        EXPECT_EQ(ex, 0); EXPECT_EQ(d, NULL);
        /* odd byte length is kept exactly: 3 bytes in 2 words */
        build_wire(&c, v, 2, 3, 2, L"ab", 4, 64);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        EXPECT_EQ(ex, 0); CHECK(d != NULL);
        if (d) { if (!t_is_wine()) { EXPECT_EQ(U(SysStringByteLen)(d), 3); EXPECT_EQ(U(SysStringLen)(d), 1); } U(SysFreeString)(d); } d = NULL;
        /* two words, exact */
        build_wire(&c, v, 2, 4, 2, L"hi", 4, 64);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        EXPECT_EQ(ex, 0); CHECK(d && !lstrcmpW(d, L"hi") && U(SysStringByteLen)(d) == 4);
        EXPECT_EQ((unsigned char *)end, c.buf + (v ? 16 : 12) + 4);
        U(SysFreeString)(d); d = NULL;
    }
}

TEST(gap0_bstr_user_unmarshal64_inconsistent_bytelen_is_normalised) {
    FARPROC u = t_proc_in("oleaut32.dll", "BSTR_UserUnmarshal64"); REQUIRE(u);
    Ctx c; BSTR d = NULL; ULONG_PTR end;
    /* byteLen must equal 2*cnt or 2*cnt-1, otherwise the BSTR length prefix is rewritten to 2*cnt */
    static const unsigned bl[] = { 4, 3, 2, 100, 0, 5 };
    static const unsigned want[] = { 4, 3, 4, 4, 4, 4 };
    for (int i = 0; i < 6; i++) {
        build_wire(&c, 1, 2, bl[i], 2, L"hi", 4, 64);
        DWORD ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        t_check(ex == 0 && d && U(SysStringByteLen)(d) == want[i], __FILE__, __LINE__, "byteLen normalisation", "wire byteLen %u exc 0x%lx got %u want %u", bl[i], ex, d ? U(SysStringByteLen)(d) : 0, want[i]);
        U(SysFreeString)(d); d = NULL;
    }
}

TEST(gap0_bstr_user_unmarshal_bad_buffers_raise) {
    if (t_is_wine()) { t_skip("Wine lacks the shielded bounds checks"); return; }
    for (int v = 0; v < 2; v++) {
        FARPROC u = t_proc_in("oleaut32.dll", v ? "BSTR_UserUnmarshal64" : "BSTR_UserUnmarshal"); if (!u) continue;
        Ctx c; BSTR d = NULL; ULONG_PTR end; DWORD ex;
        ULONG hdr = v ? 16 : 12;
        /* buffer too short to hold the header */
        for (ULONG len = 0; len < hdr; len += 4) {
            build_wire(&c, v, 0, 0xFFFFFFFFu, 0, NULL, 0, len);
            ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
            t_check(ex != 0, __FILE__, __LINE__, "short header raises", "v64=%d len %lu", v, len);
            if (ex && v) EXPECT_EQ(ex, 0x6f7);
            if (ex) t_note("v64=%d header-short len %lu: exception 0x%lx", v, len, ex);
            EXPECT_EQ(d, NULL);
        }
        /* count larger than the data actually present in the buffer */
        build_wire(&c, v, 8, 16, 8, L"1234", 8, hdr + 8);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        t_check(ex != 0, __FILE__, __LINE__, "count beyond buffer raises", "v64=%d", v);
        if (ex && v) EXPECT_EQ(ex, 0x6f7);
        if (ex) t_note("v64=%d overlong count: exception 0x%lx", v, ex);
        U(SysFreeString)(d); d = NULL;
        /* huge count (would overflow count*2) */
        build_wire(&c, v, 0x80000000u, 0xFFFFFFFEu, 0x80000000u, NULL, 0, hdr + 8);
        ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
        t_check(ex != 0, __FILE__, __LINE__, "huge count raises", "v64=%d", v);
        if (ex) t_note("v64=%d huge count: exception 0x%lx", v, ex);
        EXPECT_EQ(d, NULL);
    }
}

TEST(gap0_bstr_user_unmarshal32_count_mismatch_raises) {
    if (t_is_wine()) { t_skip("Wine lacks the shielded bounds checks"); return; }
    FARPROC u = t_proc_in("oleaut32.dll", "BSTR_UserUnmarshal"); REQUIRE(u);
    Ctx c; BSTR d = NULL; ULONG_PTR end;
    /* NDR32 repeats the element count (conformance and variance): they must agree */
    build_wire(&c, 0, 2, 4, 1, L"hi", 4, 64);
    DWORD ex = guarded(u, &c.umcb.Flags, c.buf, &d, &end);
    CHECK(ex != 0);
    t_note("count/variance mismatch exception 0x%lx", ex);
    U(SysFreeString)(d);
}

TEST(gap0_bstr_user_free) {
    for (int v = 0; v < 2; v++) {
        FARPROC fr = t_proc_in("oleaut32.dll", v ? "BSTR_UserFree64" : "BSTR_UserFree"); if (!fr) continue;
        Ctx c; ctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
        /* NULL BSTR* and BSTR* -> NULL are no-ops */
        ULONG_PTR r;
        if (!t_is_wine()) CHECK(guarded(fr, &c.umcb.Flags, NULL, NULL, &r) == 0);
        BSTR n = NULL;
        CHECK(guarded(fr, &c.umcb.Flags, &n, NULL, &r) == 0);
        EXPECT_EQ(n, NULL);
        /* a live BSTR is released and the caller's pointer is cleared; a second call is then a no-op */
        for (int k = 0; k < 20; k++) {
            BSTR b = U(SysAllocStringLen)(L"free me", 1 + k % 7);
            CHECK(b != NULL);
            CHECK(guarded(fr, &c.umcb.Flags, &b, NULL, &r) == 0);
            EXPECT_EQ(b, NULL);
            CHECK(guarded(fr, &c.umcb.Flags, &b, NULL, &r) == 0);
            EXPECT_EQ(b, NULL);
        }
        BSTR e = U(SysAllocString)(L"");
        CHECK(guarded(fr, &c.umcb.Flags, &e, NULL, &r) == 0);
        EXPECT_EQ(e, NULL);
    }
}
