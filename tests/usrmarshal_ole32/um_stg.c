/* usrmarshal_ole32: SNB_User*, STGMEDIUM_User*, WdtpInterfacePointer_User*.
 *
 * SNB (string name block) 32-bit wire, from the Win10 decompilation:  DWORD C, DWORD count, DWORD C, WCHAR chars[C]
 *   where C = sum(len+1) over the strings; Size() = align4(off) + 12 + 2*C; 64-bit: QWORD C, DWORD count, DWORD C, chars at +16.
 * STGMEDIUM 32-bit wire:  DWORD tymed, [DWORD u-marker if tymed != 0], DWORD pUnkForRelease-marker, payload(tymed), [IUnknown].
 *   Payload is the matching helper called at (aligned offset + 12) (+8 for TYMED_NULL): HGLOBAL, string, interface, GDI object, ... */
#include "um.h"

static const GUID g_IID_IUnknown = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID g_IID_IStream  = { 0x0000000c, 0x0000, 0x0000, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };

/* ---- SNB ------------------------------------------------------------------------------------------------------ */
/* Build an SNB: n+1 pointers followed by the string data. */
static WCHAR **snb_make(const WCHAR *const *strs, int n)
{
    size_t chars = 0, i; WCHAR **p; WCHAR *d;
    for (i = 0; i < (size_t)n; i++) chars += lstrlenW(strs[i]) + 1;
    p = malloc((n + 1) * sizeof(WCHAR *) + chars * sizeof(WCHAR));
    d = (WCHAR *)(p + n + 1);
    for (i = 0; i < (size_t)n; i++) { p[i] = d; lstrcpyW(d, strs[i]); d += lstrlenW(strs[i]) + 1; }
    p[n] = NULL;
    return p;
}
static ULONG snb_chars(WCHAR **s)
{
    ULONG c = 0;
    if (s) for (; *s; s++) c += lstrlenW(*s) + 1;
    return c;
}

static const WCHAR *const k_snb_a[] = { L"string1", L"str2" };
static const WCHAR *const k_snb_b[] = { L"x" };
static const WCHAR *const k_snb_c[] = { L"", L"abc", L"" , L"longer string number four"};
static const struct { const WCHAR *const *s; int n; } k_snb[] = { { k_snb_a, 2 }, { k_snb_b, 1 }, { k_snb_c, 4 } };

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_SNB */
TEST(usr_snb_size)
{
    void *f = UM_FNIN("ole32.dll", "SNB_UserSize"), *f6 = UM_FNIN("ole32.dll", "SNB_UserSize64");
    ULONG ctx, off, r; int i;
    REQUIRE(f);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 9; off++) {
            UMC c; WCHAR **empty = NULL;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(f, UMF(&c), off, NULL, &r), 0); CHECK(r == off);                       /* NULL pointer: nothing */
            XEQ(um_x_size(f, UMF(&c), off, &empty, &r), 0);
            CHECKF(r == UM_ALIGN(off, 4) + 12, "empty SNB off %lu got %lu", off, r);               /* Wine: 12 at 0, 16 at 3 */
            for (i = 0; i < 3; i++) {
                WCHAR **snb = snb_make(k_snb[i].s, k_snb[i].n);
                XEQ(um_x_size(f, UMF(&c), off, &snb, &r), 0);
                CHECKF(r == UM_ALIGN(off, 4) + 12 + 2 * snb_chars(snb), "set %d off %lu got %lu", i, off, r);
                if (f6) {
                    ULONG r6;
                    XEQ(um_x_size(f6, UMF(&c), off, &snb, &r6), 0);
                    CHECKF(r6 >= UM_ALIGN(off, 8) + 16 + 2 * snb_chars(snb), "64: got %lu", r6);
                    if (r6 != UM_ALIGN(off, 8) + 16 + 2 * snb_chars(snb)) t_note("SNB_UserSize64 over-estimates: %lu vs exact %lu", r6, UM_ALIGN(off, 8) + 16 + 2 * snb_chars(snb));
                }
                free(snb);
            }
        }
}

TEST(usr_snb_marshal_wire)
{
    void *f = UM_FNIN("ole32.dll", "SNB_UserMarshal"), *f6 = UM_FNIN("ole32.dll", "SNB_UserMarshal64");
    ULONG off; int i, w;
    REQUIRE(f);
    for (w = 0; w < 2; w++) {
        void *fn = w ? f6 : f;
        if (!fn) continue;
        for (off = 0; off < 9; off += 3)
            for (i = -1; i < 3; i++) {
                UMC c; WCHAR **snb = i < 0 ? NULL : snb_make(k_snb[i].s, k_snb[i].n), **p = snb;
                ULONG C = snb_chars(snb), cnt = i < 0 ? 0 : k_snb[i].n, a = UM_ALIGN(off, w ? 8 : 4), hdr = w ? 16 : 12;
                unsigned char *b = um_buf_new(2 * C + 64), *e; ULONG k, pos = 0;
                um_init(&c, b, 2 * C + 64, C_LOCAL);
                XEQ(um_x_mar(fn, UMF(&c), b + off, &p, &e), 0);
                CHECKF(e == b + a + hdr + 2 * C, "w=%d set %d off %lu end %+ld", w, i, off, (long)(e - b));
                if (!w) { EXPECT_EQ(RD_U32(b, a), C); EXPECT_EQ(RD_U32(b, a + 4), cnt); EXPECT_EQ(RD_U32(b, a + 8), C); }
                else { CHECK(*(unsigned long long *)(b + a) == C); EXPECT_EQ(RD_U32(b, a + 8), cnt); EXPECT_EQ(RD_U32(b, a + 12), C); }
                for (k = 0; k < cnt; k++) {   /* strings are stored back to back, including their NULs */
                    const WCHAR *s = k_snb[i].s[k];
                    CHECK(!memcmp(b + a + hdr + 2 * pos, s, 2 * (lstrlenW(s) + 1)));
                    pos += lstrlenW(s) + 1;
                }
                CHECK(um_untouched(b, 0, a)); CHECK(um_guard_ok(b, 2 * C + 64));
                um_buf_free(b); free(snb);
            }
        {   /* NULL pointer: buffer returned as is */
            UMC c; unsigned char *b = um_buf_new(32), *e; um_init(&c, b, 32, C_LOCAL);
            XEQ(um_x_mar(fn, UMF(&c), b + 1, NULL, &e), 0); CHECK(e == b + 1);
        }
    }
}

TEST(usr_snb_roundtrip_alloc_free)
{
    void *fm = UM_FNIN("ole32.dll", "SNB_UserMarshal"), *fu = UM_FNIN("ole32.dll", "SNB_UserUnmarshal"), *ff = UM_FNIN("ole32.dll", "SNB_UserFree");
    int i, w;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    for (w = 0; w < 2; w++) {
        void *m = w ? UM_FNIN("ole32.dll", "SNB_UserMarshal64") : fm, *u = w ? UM_FNIN("ole32.dll", "SNB_UserUnmarshal64") : fu;
        void *fr = w ? UM_FNIN("ole32.dll", "SNB_UserFree64") : ff;
        NEED(m); NEED(u); NEED(fr);
        for (i = 0; i < 3; i++) {
            UMC c; WCHAR **snb = snb_make(k_snb[i].s, k_snb[i].n), **o = NULL, **q; unsigned char *b = um_buf_new(400), *e1, *e2;
            int a0, f0, k;
            um_init(&c, b, 400, C_LOCAL); XEQ(um_x_mar(m, UMF(&c), b + 1, &snb, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), C_LOCAL);
            a0 = um_nalloc;
            XEQ(um_x_mar(u, UMF(&c), b + 1, &o, &e2), 0);
            CHECK(e2 == e1);
            CHECKF(um_nalloc == a0 + 1, "one allocation through the stub's pfnAllocate (got %d)", um_nalloc - a0);
            CHECK(o != NULL);
            if (o) {
                for (k = 0, q = o; k < k_snb[i].n; k++, q++) { CHECK(*q != NULL && !lstrcmpW(*q, k_snb[i].s[k])); }
                CHECK(*q == NULL);                      /* terminating NULL pointer */
                f0 = um_nfree;
                um_init(&c, NULL, 0, C_LOCAL);
                XEQ(um_x_free(fr, UMF(&c), &o), 0);
                CHECKF(um_nfree == f0 + 1, "SNB_UserFree releases through pfnFree (got %d)", um_nfree - f0);
            }
            um_buf_free(b); free(snb);
        }
        {   /* empty block unmarshals to NULL, no allocation */
            UMC c; WCHAR **o = NULL, **empty = NULL; unsigned char *b = um_buf_new(64), *e1, *e2; int a0;
            um_init(&c, b, 64, C_LOCAL); XEQ(um_x_mar(m, UMF(&c), b, &empty, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), C_LOCAL); a0 = um_nalloc;
            XEQ(um_x_mar(u, UMF(&c), b, &o, &e2), 0);
            CHECK(o == NULL && e2 == e1 && a0 == um_nalloc);
            um_buf_free(b);
        }
    }
}

TEST(usr_snb_free_null)
{
    void *ff = UM_FNIN("ole32.dll", "SNB_UserFree"), *f6 = UM_FNIN("ole32.dll", "SNB_UserFree64");
    int k;
    REQUIRE(ff);
    for (k = 0; k < 2; k++) {
        void *f = k ? f6 : ff; UMC c; WCHAR **o = NULL; int f0 = um_nfree;
        if (!f) continue;
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), NULL), 0);
        XEQ(um_x_free(f, UMF(&c), &o), 0);
        CHECK(um_nfree == f0);
    }
}

TEST(usr_snb_unmarshal_errors)
{
    void *fu = UM_FNIN("ole32.dll", "SNB_UserUnmarshal");
    UMC c; WCHAR **o; unsigned char *b = um_buf_new(200), *e; ULONG len; int a0, f0;
    REQUIRE(fu);
    /* well formed: C=8 chars, 2 strings: "abc\0defg\0" would be 9; use "abc" "de" -> 4+3=7 */
    RD_U32(b, 0) = 7; RD_U32(b, 4) = 2; RD_U32(b, 8) = 7;
    memcpy(b + 12, L"abc\0de\0", 14);
    o = NULL; um_init(&c, b, 26, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(e == b + 26 && o && !lstrcmpW(o[0], L"abc") && !lstrcmpW(o[1], L"de") && !o[2]);
    if (o) U(CoTaskMemFree)(o);

    for (len = 0; len < 26; len++) {      /* any truncation is refused with RPC_X_BAD_STUB_DATA */
        o = NULL; um_init(&c, b, len, C_LOCAL);
        a0 = um_nalloc; f0 = um_nfree;
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
        CHECKF(o == NULL || len > 12, "len %lu: no result on failure", len);
        CHECKF(um_nalloc - a0 == um_nfree - f0, "len %lu: nothing leaked (alloc %d free %d)", len, um_nalloc - a0, um_nfree - f0);
    }
    RD_U32(b, 8) = 8;                        /* second C must equal the first */
    o = NULL; um_init(&c, b, 26, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 8) = 7; RD_U32(b, 4) = 8;      /* more strings than characters */
    um_init(&c, b, 26, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    RD_U32(b, 4) = 3;                        /* count says 3 strings but only 2 NUL-terminated ones exist */
    a0 = um_nalloc; f0 = um_nfree;
    um_init(&c, b, 26, C_LOCAL); XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), X_BADSTUB);
    CHECKF(um_nalloc - a0 == um_nfree - f0, "allocation made during parsing is released again (alloc %d free %d)", um_nalloc - a0, um_nfree - f0);
    RD_U32(b, 4) = 2; *(WCHAR *)(b + 12 + 12) = L'z';   /* last char not NUL */
    {   /* Win10 does not reject this: it raises STATUS_HEAP_CORRUPTION (0xc0000374) from the unterminated string walk */
        um_init(&c, b, 26, C_LOCAL);
        { DWORD x = um_x_mar(fu, UMF(&c), b, &o, &e); CHECKF(x == X_BADSTUB || x == 0xc0000374u, "last char not NUL: refused (got 0x%lx)", x);
          if (x != X_BADSTUB) t_note("SNB_UserUnmarshal with an unterminated last string raises 0x%lx instead of RPC_X_BAD_STUB_DATA", x); }
    }
    /* count == 0: Win10 returns just past the 12-byte header without consuming the characters */
    memcpy(b + 12, L"abc\0de\0", 14);
    RD_U32(b, 4) = 0; o = (WCHAR **)(ULONG_PTR)0; um_init(&c, b, 26, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(o == NULL && e == b + 12);
    um_buf_free(b);
}

/* ---- COM objects used for the interface-pointer members ----------------------------------------------------- */
typedef struct { IUnknown iface; LONG refs; LONG nadd, nrel; } TU;
static HRESULT WINAPI tu_qi(IUnknown *i, REFIID r, void **o)
{
    if (!memcmp(r, &g_IID_IUnknown, sizeof(GUID))) { *o = i; i->lpVtbl->AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI tu_addref(IUnknown *i) { TU *t = (TU *)i; t->nadd++; return InterlockedIncrement(&t->refs); }
static ULONG WINAPI tu_release(IUnknown *i) { TU *t = (TU *)i; t->nrel++; return InterlockedDecrement(&t->refs); }
static const IUnknownVtbl tu_vtbl = { tu_qi, tu_addref, tu_release };
static void tu_init(TU *t) { t->iface.lpVtbl = (IUnknownVtbl *)&tu_vtbl; t->refs = 1; t->nadd = t->nrel = 0; }

typedef struct { int ok; HRESULT hr; } comctx;
static comctx com_up(void)
{
    comctx c; c.hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED); c.ok = SUCCEEDED(c.hr); return c;
}
static void com_down(comctx c) { if (c.ok) U(CoUninitialize)(); }

/* Size of a standard marshal of obj for the given context, as reported by CoGetMarshalSizeMax. */
static ULONG marshal_max(IUnknown *obj, DWORD ctx)
{
    ULONG n = 0;
    HRESULT hr = U(CoGetMarshalSizeMax)(&n, &g_IID_IUnknown, obj, ctx, NULL, MSHLFLAGS_NORMAL);
    return SUCCEEDED(hr) ? n : 0xFFFFFFFF;
}

/* release marshal data that sits in a raw buffer */
static void drop_marshal_data(const unsigned char *data, ULONG n)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n); IStream *s = NULL; void *p;
    if (!h) return;
    p = GlobalLock(h); memcpy(p, data, n); GlobalUnlock(h);
    if (SUCCEEDED(U(CreateStreamOnHGlobal)(h, TRUE, &s))) {
        U(CoReleaseMarshalData)(s);
        s->lpVtbl->Release(s);
    } else GlobalFree(h);
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_WdtpInterfacePointer */
TEST(usr_wdtp_null_interface)
{
    void *fs = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserSize"), *fm = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserMarshal");
    typedef ULONG (WINAPI *wsz)(ULONG *, ULONG, ULONG, IUnknown *, const GUID *);
    typedef unsigned char *(WINAPI *wmar)(ULONG *, ULONG, unsigned char *, IUnknown *, const GUID *);
    ULONG ctx, off;
    REQUIRE(fs); REQUIRE(fm);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 9; off++) {
            UMC c; unsigned char *b = um_buf_new(32); ULONG r = 0xdead; unsigned char *e = NULL;
            um_init(&c, NULL, 0, ctx);
            if (!setjmp(um_jb)) { um_armed = 1; r = ((wsz)fs)(UMF(&c), ctx, off, NULL, &g_IID_IUnknown); um_armed = 0; }
            um_armed = 0;
            CHECKF(r == off, "NULL interface marshals nothing: size %lu off %lu", r, off);
            um_init(&c, b, 32, ctx);
            if (!setjmp(um_jb)) { um_armed = 1; e = ((wmar)fm)(UMF(&c), ctx, b + off, NULL, &g_IID_IUnknown); um_armed = 0; }
            um_armed = 0;
            CHECKF(e == b + off, "buffer returned unchanged (not even aligned)");
            CHECK(um_untouched(b, 0, 32));
            um_buf_free(b);
        }
}

TEST(usr_wdtp_marshal_wire)
{
    void *fs = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserSize"), *fm = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserMarshal");
    void *fu = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserUnmarshal"), *ffree = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserFree");
    typedef ULONG (WINAPI *wsz)(ULONG *, ULONG, ULONG, IUnknown *, const GUID *);
    typedef unsigned char *(WINAPI *wmar)(ULONG *, ULONG, unsigned char *, IUnknown *, const GUID *);
    typedef unsigned char *(WINAPI *wunm)(ULONG *, unsigned char *, IUnknown **, const GUID *);
    static const ULONG ctxs[] = { C_LOCAL, C_INPROC, C_DIFFM };
    int k; ULONG off;
    comctx cc;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ffree);
    cc = com_up();
    if (!cc.ok) { t_skip("CoInitializeEx failed %08lx", cc.hr); return; }
    for (k = 0; k < 3; k++)
        for (off = 0; off < 6; off += 5) {
            TU tu; IUnknown *unk, *out = NULL; UMC c; ULONG ctx = ctxs[k], max, sz = 0, n; unsigned char *b, *e = NULL, *e2 = NULL; ULONG a = UM_ALIGN(off, 4);
            tu_init(&tu); unk = &tu.iface;
            max = marshal_max(unk, ctx);
            CHECKF(max != 0xFFFFFFFF && max > 0, "CoGetMarshalSizeMax ctx %lu", ctx);
            if (max == 0xFFFFFFFF) continue;
            um_init(&c, NULL, 0, ctx);
            if (!setjmp(um_jb)) { um_armed = 1; sz = ((wsz)fs)(UMF(&c), ctx, off, unk, &g_IID_IUnknown); um_armed = 0; }
            um_armed = 0;
            CHECKF(sz == a + 8 + max, "ctx %lu off %lu size %lu want %lu", ctx, off, sz, a + 8 + max);
            b = um_buf_new(sz + 64);
            um_init(&c, b, sz + 64, ctx);
            if (!setjmp(um_jb)) { um_armed = 1; e = ((wmar)fm)(UMF(&c), ctx, b + off, unk, &g_IID_IUnknown); um_armed = 0; }
            um_armed = 0;
            CHECK(e != NULL && e <= b + sz);
            if (e) {
                n = RD_U32(b, a);
                CHECKF(RD_U32(b, a + 4) == n, "size appears twice");
                CHECKF(e == b + a + 8 + n, "end == aligned + 8 + marshalled bytes (ctx %lu)", ctx);
                CHECK(n > 0 && n <= max);
                CHECK(RD_U32(b, a + 8) == 0x574f454d);            /* OBJREF signature 'MEOW' */
                /* unmarshal it again in this process */
                um_init(&c, b, (ULONG)(e - b), ctx);
                if (!setjmp(um_jb)) { um_armed = 1; e2 = ((wunm)fu)(UMF(&c), b + off, &out, &g_IID_IUnknown); um_armed = 0; }
                um_armed = 0;
                CHECK(e2 == e);
                CHECKF(out != NULL, "interface unmarshalled");
                if (out) {
                    if (out != unk) t_note("ctx %lu: unmarshal returned a proxy/other pointer (%p vs %p)", ctx, out, unk);
                    ((void (WINAPI *)(IUnknown *))ffree)(out);           /* WdtpInterfacePointer_UserFree == Release */
                }
                else drop_marshal_data(b + a + 8, n);
            }
            um_buf_free(b);
        }
    com_down(cc);
}

TEST(usr_wdtp_free_calls_release)
{
    void *ff = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserFree"), *f6 = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserFree64");
    int k;
    REQUIRE(ff);
    for (k = 0; k < 2; k++) {
        void *f = k ? f6 : ff; TU tu;
        if (!f) continue;
        tu_init(&tu); tu.refs = 5;
        ((void (WINAPI *)(IUnknown *))f)(&tu.iface);
        CHECKF(tu.refs == 4 && tu.nrel == 1 && tu.nadd == 0, "exactly one Release (refs %ld, rel %ld)", tu.refs, tu.nrel);
        ((void (WINAPI *)(IUnknown *))f)(NULL);          /* NULL is accepted */
    }
}

TEST(usr_wdtp_size64)
{
    void *fs = UM_FNIN("ole32.dll", "WdtpInterfacePointer_UserSize64");
    typedef ULONG (WINAPI *wsz)(ULONG *, ULONG, ULONG, IUnknown *, const GUID *);
    comctx cc; ULONG off;
    REQUIRE(fs);
    cc = com_up();
    if (!cc.ok) { t_skip("CoInitializeEx failed"); return; }
    for (off = 0; off < 12; off += 3) {
        TU tu; UMC c; ULONG sz = 0, max;
        tu_init(&tu);
        max = marshal_max(&tu.iface, C_LOCAL);
        um_init(&c, NULL, 0, C_LOCAL);
        if (!setjmp(um_jb)) { um_armed = 1; sz = ((wsz)fs)(UMF(&c), C_LOCAL, off, &tu.iface, &g_IID_IUnknown); um_armed = 0; }
        um_armed = 0;
        CHECKF(sz == UM_ALIGN(off, 8) + 12 + max, "off %lu size %lu want %lu", off, sz, UM_ALIGN(off, 8) + 12 + max);
        sz = 0;
        if (!setjmp(um_jb)) { um_armed = 1; sz = ((wsz)fs)(UMF(&c), C_LOCAL, off, NULL, &g_IID_IUnknown); um_armed = 0; }
        um_armed = 0;
        CHECK(sz == off);
    }
    com_down(cc);
}

/* ---- STGMEDIUM -------------------------------------------------------------------------------------------------- */
#define TY_NULL 0
#define TY_HGLOBAL 1
#define TY_FILE 2
#define TY_ISTREAM 4
#define TY_ISTORAGE 8
#define TY_GDI 16
#define TY_MFPICT 32
#define TY_ENHMF 64

static void *sfn(const char *op, const char *sfx)
{
    char n[64]; snprintf(n, sizeof n, "STGMEDIUM_User%s%s", op, sfx); return UM_FNIN("ole32.dll", n);
}

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): marshal_STGMEDIUM / test_marshal_STGMEDIUM */
TEST(usr_stgmedium_null_ptr)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *fs6 = sfn("Size", "64"), *fm6 = sfn("Marshal", "64");
    int k; ULONG off, r;
    REQUIRE(fs); REQUIRE(fm);
    for (k = 0; k < 2; k++) {
        void *s = k ? fs6 : fs, *m = k ? fm6 : fm;
        if (!s || !m) continue;
        for (off = 0; off < 9; off++) {
            UMC c; unsigned char *b = um_buf_new(32), *e;
            um_init(&c, b, 32, C_LOCAL);
            XEQ(um_x_size(s, UMF(&c), off, NULL, &r), 0); CHECK(r == off);
            XEQ(um_x_mar(m, UMF(&c), b + off, NULL, &e), 0); CHECK(e == b + off);
            um_buf_free(b);
        }
    }
}

TEST(usr_stgmedium_tymed_null_and_empty_members)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *fs6 = sfn("Size", "64"), *fm6 = sfn("Marshal", "64");
    static const DWORD tys[] = { TY_NULL, TY_HGLOBAL, TY_FILE, TY_ISTREAM, TY_ISTORAGE, TY_GDI, TY_MFPICT, TY_ENHMF };
    int i; ULONG ctx, off, r;
    REQUIRE(fs); REQUIRE(fm);
    for (i = 0; i < 8; i++)
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 9; off += 4) {
                UMC c; STGMEDIUM m; unsigned char *b = um_buf_new(64), *e; ULONG a = UM_ALIGN(off, 4), a8 = UM_ALIGN(off, 8);
                ULONG want = a + (tys[i] == TY_NULL ? 8 : 12);
                memset(&m, 0, sizeof m); m.tymed = tys[i];          /* u == NULL, pUnkForRelease == NULL */
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
                CHECKF(r == want, "tymed %lu ctx %lu off %lu size %lu want %lu", tys[i], ctx, off, r, want);
                um_init(&c, b, 64, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &m, &e), 0);
                CHECKF(e == b + want, "tymed %lu end %+ld want %lu", tys[i], (long)(e - b), want);
                EXPECT_EQ(RD_U32(b, a), tys[i]);
                EXPECT_EQ(RD_U32(b, a + 4), 0);
                if (tys[i] != TY_NULL) EXPECT_EQ(RD_U32(b, a + 8), 0);
                CHECK(um_untouched(b, 0, a)); CHECK(um_untouched(b, want, 64));
                if (fs6 && fm6) {
                    ULONG want6 = a8 + (tys[i] == TY_NULL ? 16 : 24);
                    um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(fs6, UMF(&c), off, &m, &r), 0);
                    CHECKF(r == want6, "64: tymed %lu ctx %lu off %lu size %lu want %lu", tys[i], ctx, off, r, want6);
                    memset(b, 0xCC, 64); um_init(&c, b, 64, ctx);
                    XEQ(um_x_mar(fm6, UMF(&c), b + off, &m, &e), 0);
                    CHECKF(e == b + want6, "64: tymed %lu end %+ld want %lu", tys[i], (long)(e - b), want6);
                    EXPECT_EQ(RD_U32(b, a8), tys[i]);
                }
                um_buf_free(b);
            }
}

TEST(usr_stgmedium_hglobal_composition)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *hs = UM_FNIN("ole32.dll", "HGLOBAL_UserSize"), *hm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal");
    void *fs6 = sfn("Size", "64"), *fm6 = sfn("Marshal", "64"), *hs6 = UM_FNIN("ole32.dll", "HGLOBAL_UserSize64"), *hm6 = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal64");
    static const ULONG sizes[] = { 0, 1, 17, 100, 5000 };
    int i; ULONG ctx, off, r, rh;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(hs); REQUIRE(hm);
    for (i = 0; i < 5; i++) {
        HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizes[i]);
        if (!g) continue;
        if (sizes[i]) { unsigned char *p = GlobalLock(g); memset(p, 0x5A + i, sizes[i]); GlobalUnlock(g); }
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off += 5) {
                UMC c; STGMEDIUM m; unsigned char *b = um_buf_new(GlobalSize(g) + 128), *e, *e2, *b2 = um_buf_new(GlobalSize(g) + 128);
                ULONG a = UM_ALIGN(off, 4); HGLOBAL hg = g;
                memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
                XEQ(um_x_size(hs, UMF(&c), a + 12, &hg, &rh), 0);
                CHECKF(r == rh, "STGMEDIUM size == HGLOBAL size at +12: %lu vs %lu (ctx %lu)", r, rh, ctx);
                um_init(&c, b, GlobalSize(g) + 128, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &m, &e), 0);
                EXPECT_EQ(RD_U32(b, a), TY_HGLOBAL);
                EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)g);
                EXPECT_EQ(RD_U32(b, a + 8), 0);
                um_init(&c, b2, GlobalSize(g) + 128, ctx);
                XEQ(um_x_mar(hm, UMF(&c), b2 + a + 12, &hg, &e2), 0);
                CHECKF(e - b == e2 - b2, "payload ends where HGLOBAL_UserMarshal ends");
                CHECK(!memcmp(b + a + 12, b2 + a + 12, e2 - (b2 + a + 12)));
                if (fs6 && fm6 && hs6 && hm6) {
                    ULONG r6, rh6; unsigned char *e6, *e62, *b6 = um_buf_new(GlobalSize(g) + 128), *b62 = um_buf_new(GlobalSize(g) + 128);
                    ULONG a8 = UM_ALIGN(off, 8);
                    um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(fs6, UMF(&c), off, &m, &r6), 0);
                    XEQ(um_x_size(hs6, UMF(&c), a8 + 24, &hg, &rh6), 0);
                    CHECKF(r6 == rh6, "64: STGMEDIUM size == HGLOBAL64 size at +24: %lu vs %lu", r6, rh6);
                    um_init(&c, b6, GlobalSize(g) + 128, ctx);
                    XEQ(um_x_mar(fm6, UMF(&c), b6 + off, &m, &e6), 0);
                    um_init(&c, b62, GlobalSize(g) + 128, ctx);
                    XEQ(um_x_mar(hm6, UMF(&c), b62 + a8 + 24, &hg, &e62), 0);
                    CHECK(e6 - b6 == e62 - b62);
                    CHECK(!memcmp(b6 + a8 + 24, b62 + a8 + 24, e62 - (b62 + a8 + 24)));
                    um_buf_free(b6); um_buf_free(b62);
                }
                um_buf_free(b); um_buf_free(b2);
            }
        GlobalFree(g);
    }
}

TEST(usr_stgmedium_flag_bit8_forces_remote_form)
{
    /* flags bit 8 (0x100) with a non-remote context other than NOSHAREDMEM turns the context into
     * MSHCTX_DIFFERENTMACHINE (low byte rewritten to 2) for non-stream/storage media with a non-NULL u. */
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", "");
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 40);
    static const struct { ULONG in, out; } t[] = {
        { C_LOCAL, 2 }, { C_INPROC, 2 }, { C_CROSSX, 2 }, { C_NOSHM, 1 }, { C_DIFFM, 2 }, { C_CONT, 5 },
    };
    int i; STGMEDIUM m;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(g);
    memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
    for (i = 0; i < 6; i++) {
        UMC c; ULONG r, r2; unsigned char *b = um_buf_new(200), *e;
        um_init(&c, NULL, 0, t[i].in); c.cb.Flags |= 0x100;
        XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
        CHECKF((c.cb.Flags & 0xff) == t[i].out, "in ctx %lu: low byte now %lu want %lu", t[i].in, c.cb.Flags & 0xff, t[i].out);
        CHECK(c.cb.Flags & 0x100);
        um_init(&c, NULL, 0, t[i].out);
        XEQ(um_x_size(fs, UMF(&c), 0, &m, &r2), 0);
        CHECKF(r == r2, "in ctx %lu: sized like ctx %lu (%lu vs %lu)", t[i].in, t[i].out, r, r2);
        um_init(&c, b, 200, t[i].in); c.cb.Flags |= 0x100;
        XEQ(um_x_mar(fm, UMF(&c), b, &m, &e), 0);
        CHECK((c.cb.Flags & 0xff) == t[i].out);
        um_buf_free(b);
    }
    /* TYMED_ISTREAM is exempt, and a NULL u skips the rewrite entirely */
    {
        UMC c; ULONG r; STGMEDIUM n; memset(&n, 0, sizeof n); n.tymed = TY_HGLOBAL;
        um_init(&c, NULL, 0, C_INPROC); c.cb.Flags |= 0x100;
        XEQ(um_x_size(fs, UMF(&c), 0, &n, &r), 0);
        CHECK((c.cb.Flags & 0xff) == C_INPROC);
    }
    GlobalFree(g);
}

TEST(usr_stgmedium_file_name)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *fu = sfn("Unmarshal", ""), *ff = sfn("Free", "");
    void *fs6 = sfn("Size", "64"), *fm6 = sfn("Marshal", "64");
    static const WCHAR *names[] = { L"a", L"C:\\olet\\file.txt", L"a name with spaces and \x00e9\x20ac" };
    int i; ULONG ctx, off;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    for (i = 0; i < 3; i++) {
        WCHAR *nm = U(CoTaskMemAlloc)((lstrlenW(names[i]) + 1) * 2); ULONG n = lstrlenW(names[i]) + 1;
        lstrcpyW(nm, names[i]);
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 6; off += 5) {
                UMC c; STGMEDIUM m, o; unsigned char *b = um_buf_new(200), *e, *e2; ULONG a = UM_ALIGN(off, 4), r;
                memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = nm;
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(fs, UMF(&c), off, &m, &r), 0);
                CHECKF(r == a + 24 + 2 * n, "ctx %lu off %lu size %lu want %lu", ctx, off, r, a + 24 + 2 * n);
                um_init(&c, b, 200, ctx);
                XEQ(um_x_mar(fm, UMF(&c), b + off, &m, &e), 0);
                CHECKF(e == b + a + 24 + 2 * n, "end %+ld", (long)(e - b));
                EXPECT_EQ(RD_U32(b, a), TY_FILE);
                EXPECT_EQ(RD_U32(b, a + 4), (ULONG)(ULONG_PTR)nm);
                EXPECT_EQ(RD_U32(b, a + 8), 0);
                EXPECT_EQ(RD_U32(b, a + 12), n); EXPECT_EQ(RD_U32(b, a + 16), 0); EXPECT_EQ(RD_U32(b, a + 20), n);
                CHECK(!memcmp(b + a + 24, nm, 2 * n));
                CHECK(um_untouched(b, a + 24 + 2 * n, 200));
                memset(&o, 0, sizeof o);
                um_init(&c, b, (ULONG)(e - b), ctx);
                XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e2), 0);
                CHECK(e2 == e);
                CHECKF(o.tymed == TY_FILE && o.lpszFileName && !lstrcmpW(o.lpszFileName, nm) && o.pUnkForRelease == NULL, "round trip");
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_free(ff, UMF(&c), &o), 0);
                if (fs6 && fm6) {
                    ULONG a8 = UM_ALIGN(off, 8);
                    unsigned char *b6 = um_buf_new(300);
                    um_init(&c, NULL, 0, ctx); XEQ(um_x_size(fs6, UMF(&c), off, &m, &r), 0);
                    um_init(&c, b6, 300, ctx); XEQ(um_x_mar(fm6, UMF(&c), b6 + off, &m, &e), 0);
                    CHECKF(e == b6 + a8 + 24 + 8 * 3 + 2 * n && r >= (ULONG)(e - b6), "64: size %lu end %+ld", r, (long)(e - b6));
                    CHECK(*(unsigned long long *)(b6 + a8 + 24) == n);
                    CHECK(!memcmp(b6 + a8 + 24 + 24, nm, 2 * n));
                    um_buf_free(b6);
                }
                um_buf_free(b);
            }
        U(CoTaskMemFree)(nm);
    }
}

TEST(usr_stgmedium_hglobal_roundtrip_and_free)
{
    void *fm = sfn("Marshal", ""), *fu = sfn("Unmarshal", ""), *ff = sfn("Free", "");
    void *fm6 = sfn("Marshal", "64"), *fu6 = sfn("Unmarshal", "64"), *ff6 = sfn("Free", "64");
    int w; ULONG ctx;
    REQUIRE(fm); REQUIRE(fu); REQUIRE(ff);
    for (w = 0; w < 2; w++) {
        void *m_ = w ? fm6 : fm, *u_ = w ? fu6 : fu, *f_ = w ? ff6 : ff;
        NEED(m_); NEED(u_); NEED(f_);
        for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2) {
            HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 300);
            STGMEDIUM m, o; UMC c; unsigned char *b = um_buf_new(600), *e1, *e2; unsigned char *p;
            p = GlobalLock(g); memset(p, 0x77, 300); p[299] = 0x42; GlobalUnlock(g);
            memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
            memset(&o, 0, sizeof o);
            um_init(&c, b, 600, ctx); XEQ(um_x_mar(m_, UMF(&c), b + 2, &m, &e1), 0);
            um_init(&c, b, (ULONG)(e1 - b), ctx); XEQ(um_x_mar(u_, UMF(&c), b + 2, &o, &e2), 0);
            CHECK(e1 == e2);
            CHECKF(o.tymed == TY_HGLOBAL && o.hGlobal != NULL && o.hGlobal != g && o.pUnkForRelease == NULL, "tymed %lu h %p", o.tymed, o.hGlobal);
            if (o.hGlobal) {
                CHECK(GlobalSize(o.hGlobal) == GlobalSize(g));
                p = GlobalLock(o.hGlobal); CHECK(p && p[0] == 0x77 && p[299] == 0x42); GlobalUnlock(o.hGlobal);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_free(f_, UMF(&c), &o), 0);        /* ReleaseStgMedium: frees the HGLOBAL */
                SetLastError(0);
                CHECK(GlobalSize(o.hGlobal) == 0);
            }
            GlobalFree(g);
            um_buf_free(b);
        }
    }
}

TEST(usr_stgmedium_free_edge_cases)
{
    void *ff = sfn("Free", "");
    UMC c; STGMEDIUM m; static const DWORD bad[] = { 3, 0x80, 0x100, 12, 0xFFFFFFFF };
    int i;
    REQUIRE(ff);
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_free(ff, UMF(&c), NULL), 0);
    memset(&m, 0, sizeof m);                         /* TYMED_NULL: nothing to release */
    XEQ(um_x_free(ff, UMF(&c), &m), 0);
    for (i = 0; i < 5; i++) {                        /* unknown tymed: E_INVALIDARG */
        memset(&m, 0, sizeof m); m.tymed = bad[i];
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(ff, UMF(&c), &m), 0x80070057);
    }
}

TEST(usr_stgmedium_gdi_objects)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *fu = sfn("Unmarshal", ""), *ff = sfn("Free", "");
    void *bs = UM_FNIN("ole32.dll", "HBITMAP_UserSize"), *bm = UM_FNIN("ole32.dll", "HBITMAP_UserMarshal");
    HBITMAP (WINAPI *cb)(int, int, UINT, UINT, const void *) = (void *)t_proc_in("gdi32.dll", "CreateBitmap");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    HPEN (WINAPI *cpen)(int, int, COLORREF) = (void *)t_proc_in("gdi32.dll", "CreatePen");
    DWORD (WINAPI *gtype)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "GetObjectType");
    HBITMAP hb; HPEN pen; STGMEDIUM m, o; UMC c; ULONG r, rb; unsigned char *b, *b2, *e, *e2, *e3; ULONG ctx;
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff); REQUIRE(bs); REQUIRE(bm); REQUIRE(cb); REQUIRE(cpen);
    hb = cb(12, 7, 1, 8, NULL);
    pen = cpen(PS_SOLID, 1, 0);
    REQUIRE(hb); REQUIRE(pen);
    b = um_buf_new(1000); b2 = um_buf_new(1000);
    for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx++) {
        HBITMAP hh = hb;
        memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = hb;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0);
        XEQ(um_x_size(bs, UMF(&c), 16, &hh, &rb), 0);        /* 12 header + 4 object type */
        if (ctx != C_INPROC) CHECKF(r == rb, "size %lu vs HBITMAP at +16 %lu", r, rb);
        memset(b, 0xCC, 1000); um_init(&c, b, 1000, ctx);
        XEQ(um_x_mar(fm, UMF(&c), b, &m, &e), 0);
        EXPECT_EQ(RD_U32(b, 0), TY_GDI); EXPECT_EQ(RD_U32(b, 4), (ULONG)(ULONG_PTR)hb); EXPECT_EQ(RD_U32(b, 8), 0);
        EXPECT_EQ(RD_U32(b, 12), OBJ_BITMAP);
        memset(b2, 0xCC, 1000); um_init(&c, b2, 1000, ctx);
        XEQ(um_x_mar(bm, UMF(&c), b2 + 16, &hh, &e2), 0);
        CHECK(e - b == e2 - b2 && !memcmp(b + 16, b2 + 16, e2 - (b2 + 16)));
        memset(&o, 0, sizeof o);
        um_init(&c, b, (ULONG)(e - b), ctx);
        XEQ(um_x_mar(fu, UMF(&c), b, &o, &e3), 0);
        CHECK(e3 == e);
        CHECKF(o.tymed == TY_GDI && o.hBitmap != NULL && (!gtype || gtype(o.hBitmap) == OBJ_BITMAP), "gdi round trip");
        if (o.hBitmap) { um_init(&c, NULL, 0, ctx); XEQ(um_x_free(ff, UMF(&c), &o), 0); }
    }
    /* only bitmaps and palettes can travel as TYMED_GDI: a pen raises 0x80040069 from Size and from Marshal */
    memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = (HBITMAP)pen;
    um_init(&c, NULL, 0, C_LOCAL);
    XEQ(um_x_size(fs, UMF(&c), 0, &m, &r), 0x80040069);
    um_init(&c, b, 1000, C_LOCAL);
    XEQ(um_x_mar(fm, UMF(&c), b, &m, &e), 0x80040069);
    del(hb); del(pen);
    um_buf_free(b); um_buf_free(b2);
}

TEST(usr_stgmedium_unknown_tymed_marshals_header_only)
{
    /* sizes/marshals ignore tymed values they do not know (no exception): only header + markers */
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", "");
    static const DWORD tys[] = { 3, 0x80, 5 };
    int i;
    REQUIRE(fs); REQUIRE(fm);
    for (i = 0; i < 3; i++) {
        UMC c; STGMEDIUM m; ULONG r; unsigned char *b = um_buf_new(64), *e; DWORD code;
        memset(&m, 0, sizeof m); m.tymed = tys[i]; m.hGlobal = (HGLOBAL)(ULONG_PTR)0x1000;
        um_init(&c, NULL, 0, C_LOCAL);
        code = um_x_size(fs, UMF(&c), 0, &m, &r);
        t_note("STGMEDIUM_UserSize(tymed %lu, non-NULL u) -> exception %#lx size %lu", tys[i], code, r);
        um_init(&c, b, 64, C_LOCAL);
        code = um_x_mar(fm, UMF(&c), b, &m, &e);
        t_note("STGMEDIUM_UserMarshal(tymed %lu, non-NULL u) -> exception %#lx end %+ld", tys[i], code, code ? 0L : (long)(e - b));
        um_buf_free(b);
    }
}

TEST(usr_stgmedium_ext_aliases)
{
    void *fs = sfn("Size", ""), *fm = sfn("Marshal", ""), *fu = sfn("Unmarshal", ""), *ff = sfn("Free", "");
    void *es = sfn("Size", "Ext"), *em = sfn("Marshal", "Ext"), *eu = sfn("Unmarshal", "Ext"), *ef = sfn("Free", "Ext");
    UMC c; STGMEDIUM m, o; ULONG r1, r2; unsigned char *b1 = um_buf_new(300), *b2 = um_buf_new(300), *e1, *e2;
    WCHAR nm[] = L"olet-ext-name";
    REQUIRE(fs); REQUIRE(fm); REQUIRE(fu); REQUIRE(ff); REQUIRE(es); REQUIRE(em); REQUIRE(eu); REQUIRE(ef);
    memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = nm;
    um_init(&c, NULL, 0, C_DIFFM);
    XEQ(um_x_size(fs, UMF(&c), 3, &m, &r1), 0); XEQ(um_x_size(es, UMF(&c), 3, &m, &r2), 0);
    CHECK(r1 == r2);
    um_init(&c, b1, 300, C_DIFFM); XEQ(um_x_mar(fm, UMF(&c), b1 + 3, &m, &e1), 0);
    um_init(&c, b2, 300, C_DIFFM); XEQ(um_x_mar(em, UMF(&c), b2 + 3, &m, &e2), 0);
    CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 300));
    memset(&o, 0, sizeof o);
    um_init(&c, b2, (ULONG)(e2 - b2), C_DIFFM); XEQ(um_x_mar(eu, UMF(&c), b2 + 3, &o, &e1), 0);
    CHECK(o.tymed == TY_FILE && o.lpszFileName && !lstrcmpW(o.lpszFileName, nm));
    um_init(&c, NULL, 0, C_DIFFM); XEQ(um_x_free(ef, UMF(&c), &o), 0);
    um_buf_free(b1); um_buf_free(b2);
}

TEST(usr_stgmedium_unmarshal_errors)
{
    void *fu = sfn("Unmarshal", "");
    UMC c; STGMEDIUM o; unsigned char *b = um_buf_new(100), *e; ULONG len; DWORD code;
    REQUIRE(fu);
    RD_U32(b, 0) = TY_HGLOBAL; RD_U32(b, 4) = 0; RD_U32(b, 8) = 0;
    for (len = 0; len < 12; len++) {
        memset(&o, 0, sizeof o); um_init(&c, b, len, C_LOCAL);
        code = um_x_mar(fu, UMF(&c), b, &o, &e);
        CHECKF(code != 0, "truncated header (len %lu) must raise", len);
    }
    memset(&o, 0, sizeof o); um_init(&c, b, 12, C_LOCAL);
    XEQ(um_x_mar(fu, UMF(&c), b, &o, &e), 0);
    CHECK(e == b + 12 && o.tymed == TY_HGLOBAL && o.hGlobal == NULL && o.pUnkForRelease == NULL);
    RD_U32(b, 0) = 0x80;
    memset(&o, 0, sizeof o); um_init(&c, b, 12, C_LOCAL);
    code = um_x_mar(fu, UMF(&c), b, &o, &e);
    t_note("STGMEDIUM_UserUnmarshal(tymed 0x80, empty u) -> exception %#lx", code);
    um_buf_free(b);
}

/* ASYNC_STGMEDIUM / FLAG_STGMEDIUM helpers are declared in objidl.h but not exported by ole32.dll on Win10. */
TEST(usr_stgmedium_async_flag_variants_not_exported)
{
    static const char *names[] = { "ASYNC_STGMEDIUM_UserSize", "ASYNC_STGMEDIUM_UserMarshal", "ASYNC_STGMEDIUM_UserUnmarshal", "ASYNC_STGMEDIUM_UserFree",
                                   "FLAG_STGMEDIUM_UserSize", "FLAG_STGMEDIUM_UserMarshal", "FLAG_STGMEDIUM_UserUnmarshal", "FLAG_STGMEDIUM_UserFree" };
    int i; HMODULE m = t_module("ole32.dll");
    REQUIRE(m);
    for (i = 0; i < 8; i++) {
        SetLastError(0);
        if (GetProcAddress(m, names[i])) t_note("ole32.dll exports %s", names[i]);
        else EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
