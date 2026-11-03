/* usrmarshal_ole32: cross-cutting behaviour of the helpers: USER_MARSHAL_CB validation (CUserMarshalInfo /
 * NdrGetUserMarshalInfo), monotonic sizing, shift invariance of the wire bytes, ole32 vs combase parity. */
#include "um.h"

typedef UINT (WINAPI *regcf_t)(LPCWSTR);

/* ---- USER_MARSHAL_CB validation ------------------------------------------------------------------------------- */
TEST(usr_cb_unmarshal_needs_buffer_cbtype)
{
    /* NdrGetUserMarshalInfo only fills Buffer/BufferSize for CBType MARSHALL (1) and UNMARSHALL (2); otherwise the
     * position check in CUserMarshalInfo fails with 0x6f8. */
    static const char *fns[] = { "HWND_UserUnmarshal", "HGLOBAL_UserUnmarshal", "CLIPFORMAT_UserUnmarshal", "SNB_UserUnmarshal", "STGMEDIUM_UserUnmarshal" };
    int i, t;
    for (i = 0; i < 5; i++) {
        void *f = UM_FN(fns[i]);
        NEED(f);
        for (t = 0; t <= 3; t++) {
            UMC c; unsigned char *b = um_buf_new(64), *e; char obj[64]; DWORD code;
            memset(obj, 0, sizeof obj);
            RD_U32(b, 0) = UM_TAG_H; RD_U32(b, 4) = 0x10;
            if (i == 1) { RD_U32(b, 0) = UM_TAG_R; RD_U32(b, 4) = 0; }
            if (i == 3) { RD_U32(b, 0) = 0; RD_U32(b, 4) = 0; RD_U32(b, 8) = 0; }
            if (i == 4) { RD_U32(b, 0) = 0; RD_U32(b, 4) = 0; }
            um_init(&c, b, 64, C_LOCAL);
            c.cb.CBType = (USER_MARSHAL_CB_TYPE)t;
            code = um_x_mar(f, UMF(&c), b, obj, &e);
            if (t == 1 || t == 2) { CHECKF(code == 0, "%s CBType %d raised %#lx", fns[i], t, code); }
            else { CHECKF(code == X_BADBUFPOS, "%s CBType %d: got %#lx want 0x6f8", fns[i], t, code); }
            um_buf_free(b);
        }
    }
}

TEST(usr_cb_clipfmt_named_marshal_validates_cb)
{
    void *fm = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal"), *fm6 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserMarshal64");
    regcf_t reg = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    WCHAR nm[64]; char tmp[64]; int i, n, w; UINT cf;
    REQUIRE(fm); REQUIRE(reg);
    n = snprintf(tmp, sizeof tmp, "olet%lu_cb_named", GetCurrentProcessId());
    for (i = 0; i <= n; i++) nm[i] = tmp[i];
    cf = reg(nm);
    for (w = 0; w < 2; w++) {
        void *f = w ? fm6 : fm; ULONG a = w ? 8 : 4;
        CLIPFORMAT c16 = (CLIPFORMAT)cf, plain = 13;
        UMC c; unsigned char *b = um_buf_new(300), *e;
        if (!f) continue;
        /* a bad signature makes the named form raise 0x6f8 ... */
        um_init(&c, b, 300, C_LOCAL); c.cb.Signature = 0x1234;
        XEQ(um_x_mar(f, UMF(&c), b, &c16, &e), X_BADBUFPOS);
        /* ... as does a sizing-pass CBType (no buffer) */
        um_init(&c, b, 300, C_LOCAL); c.cb.CBType = USER_MARSHAL_CB_BUFFER_SIZE;
        XEQ(um_x_mar(f, UMF(&c), b, &c16, &e), X_BADBUFPOS);
        /* a declared buffer that ends before the string area (aligned + 20 / + 40) too */
        um_init(&c, b, w ? 8 + 40 - 1 : 4 - 4 + 20 - 1, C_LOCAL);
        if (w) um_init(&c, b, 40 - 1, C_LOCAL);
        XEQ(um_x_mar(f, UMF(&c), b, &c16, &e), X_BADBUFPOS);
        /* the predefined-format form never consults the callback context */
        um_init(&c, b, 300, C_LOCAL); c.cb.Signature = 0x1234; c.cb.CBType = USER_MARSHAL_CB_BUFFER_SIZE;
        XEQ(um_x_mar(f, UMF(&c), b, &plain, &e), 0);
        CHECK(e == b + (w ? 12 : 8));
        /* and neither does the inproc form of a registered format */
        um_init(&c, b, 300, C_INPROC); c.cb.Signature = 0x1234;
        XEQ(um_x_mar(f, UMF(&c), b, &c16, &e), 0);
        um_buf_free(b);
        (void)a;
    }
}

TEST(usr_cb_size_ignores_callback_context)
{
    /* Size() helpers only look at the flags word: garbage in the rest of the USER_MARSHAL_CB is harmless. */
    static const char *fns[] = { "HWND_UserSize", "HGLOBAL_UserSize", "CLIPFORMAT_UserSize", "SNB_UserSize", "HMETAFILE_UserSize" };
    int i;
    for (i = 0; i < 5; i++) {
        void *f = UM_FN(fns[i]); ULONG r1, r2, flags = C_LOCAL | (0x10u << 16); char obj[16] = { 0 }; ULONG fl2[8];
        NEED(f);
        XEQ(um_x_size(f, &flags, 3, obj, &r1), 0);
        memset(fl2, 0xAA, sizeof fl2); fl2[0] = flags;
        XEQ(um_x_size(f, fl2, 3, obj, &r2), 0);
        CHECKF(r1 == r2, "%s: %lu vs %lu", fns[i], r1, r2);
    }
}

/* ---- sizing sanity ---------------------------------------------------------------------------------------------- */
TEST(usr_all_sizes_monotonic_and_aligned)
{
    static const char *fns[] = { "HWND_UserSize", "HICON_UserSize", "HGLOBAL_UserSize", "HGLOBAL_UserSize64", "CLIPFORMAT_UserSize", "CLIPFORMAT_UserSize64",
                                 "SNB_UserSize", "SNB_UserSize64", "STGMEDIUM_UserSize", "STGMEDIUM_UserSize64", "HBITMAP_UserSize", "HBITMAP_UserSize64",
                                 "HPALETTE_UserSize", "HENHMETAFILE_UserSize", "HMETAFILE_UserSize", "HMETAFILEPICT_UserSize" };
    int i;
    for (i = 0; i < 16; i++) {
        void *f = UM_FN(fns[i]); ULONG off, prev = 0, r; ULONG ctx;
        NEED(f);
        for (ctx = 0; ctx <= C_CONT; ctx += 3) {
            prev = 0;
            for (off = 0; off <= 40; off++) {
                UMC c; ULONG obj[8] = { 0 }; void *p = obj; ULONG r2;
                um_init(&c, NULL, 0, ctx);
                if (!strncmp(fns[i], "CLIPFORMAT", 10)) { ((WORD *)obj)[0] = 13; }
                XEQ(um_x_size(f, UMF(&c), off, p, &r), 0);
                CHECKF(r >= off, "%s ctx %lu off %lu -> %lu < off", fns[i], ctx, off, r);
                CHECKF(r >= prev, "%s ctx %lu: size not monotonic at off %lu (%lu < %lu)", fns[i], ctx, off, r, prev);
                prev = r;
                /* shifting the start by 8 shifts the result by exactly 8 (all alignments divide 8) */
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(f, UMF(&c), off + 8, p, &r2), 0);
                CHECKF(r2 == r + 8, "%s ctx %lu off %lu: +8 shift gave %lu vs %lu", fns[i], ctx, off, r2, r);
            }
        }
    }
}

TEST(usr_flags_data_representation_ignored)
{
    static const ULONG reps[] = { 0, 0x10, 0x1000, 0xFFFF };
    static const char *fns[] = { "HGLOBAL_UserSize", "CLIPFORMAT_UserSize", "HWND_UserSize", "SNB_UserSize" };
    int i, k;
    for (i = 0; i < 4; i++) {
        void *f = UM_FN(fns[i]); ULONG ref = 0, r; char obj[16] = { 0 };
        NEED(f);
        for (k = 0; k < 4; k++) {
            ULONG fl = C_LOCAL | (reps[k] << 16);
            XEQ(um_x_size(f, &fl, 1, obj, &r), 0);
            if (!k) ref = r;
            CHECKF(r == ref, "%s data rep %#lx changes the size", fns[i], reps[k]);
        }
    }
}

/* ---- shift invariance --------------------------------------------------------------------------------------------- */
typedef unsigned char *(*mk_fn)(UMC *c, unsigned char *b, int w);
static WCHAR g_snb_data[64];
static HGLOBAL g_hg;
static CLIPFORMAT g_cf;
static WCHAR **g_snbp;
static STGMEDIUM g_stg;
static HANDLE g_hwnd = (HANDLE)0x4488;

static unsigned char *mk_hg(UMC *c, unsigned char *b, int w) { unsigned char *e = NULL; HGLOBAL h = g_hg; um_x_mar(UM_FN(w ? "HGLOBAL_UserMarshal64" : "HGLOBAL_UserMarshal"), UMF(c), b, &h, &e); return e; }
static unsigned char *mk_cf(UMC *c, unsigned char *b, int w) { unsigned char *e = NULL; CLIPFORMAT cf = g_cf; um_x_mar(UM_FN(w ? "CLIPFORMAT_UserMarshal64" : "CLIPFORMAT_UserMarshal"), UMF(c), b, &cf, &e); return e; }
static unsigned char *mk_snb(UMC *c, unsigned char *b, int w) { unsigned char *e = NULL; WCHAR **s = g_snbp; um_x_mar(UM_FN(w ? "SNB_UserMarshal64" : "SNB_UserMarshal"), UMF(c), b, &s, &e); return e; }
static unsigned char *mk_stg(UMC *c, unsigned char *b, int w) { unsigned char *e = NULL; STGMEDIUM m = g_stg; um_x_mar(UM_FN(w ? "STGMEDIUM_UserMarshal64" : "STGMEDIUM_UserMarshal"), UMF(c), b, &m, &e); return e; }
static unsigned char *mk_hw(UMC *c, unsigned char *b, int w) { unsigned char *e = NULL; HANDLE h = g_hwnd; um_x_mar(UM_FN(w ? "HWND_UserMarshal64" : "HWND_UserMarshal"), UMF(c), b, &h, &e); return e; }

TEST(usr_wire_bytes_are_position_independent)
{
    static const struct { const char *name; mk_fn fn; } t[] = {
        { "HGLOBAL", mk_hg }, { "CLIPFORMAT", mk_cf }, { "SNB", mk_snb }, { "STGMEDIUM", mk_stg }, { "HWND", mk_hw },
    };
    regcf_t reg = (regcf_t)t_proc_in("user32.dll", "RegisterClipboardFormatW");
    const WCHAR *strs[] = { L"alpha", L"be" };
    int i, w; WCHAR nm[48]; char tmp[48]; int k, n;
    unsigned char *p;
    REQUIRE(reg);
    n = snprintf(tmp, sizeof tmp, "olet%lu_shift", GetCurrentProcessId());
    for (k = 0; k <= n; k++) nm[k] = tmp[k];
    g_cf = (CLIPFORMAT)reg(nm);
    g_hg = GlobalAlloc(GMEM_MOVEABLE, 50);
    p = GlobalLock(g_hg); for (k = 0; k < 50; k++) p[k] = (unsigned char)(k * 5 + 1); GlobalUnlock(g_hg);
    { size_t s0 = lstrlenW(strs[0]) + 1, s1 = lstrlenW(strs[1]) + 1;
      g_snbp = malloc(3 * sizeof(WCHAR *) + (s0 + s1) * 2);
      g_snbp[0] = (WCHAR *)(g_snbp + 3); lstrcpyW(g_snbp[0], strs[0]); g_snbp[1] = g_snbp[0] + s0; lstrcpyW(g_snbp[1], strs[1]); g_snbp[2] = NULL; }
    memset(&g_stg, 0, sizeof g_stg); g_stg.tymed = 1; g_stg.hGlobal = g_hg;
    for (i = 0; i < 5; i++)
        for (w = 0; w < 2; w++) {
            UMC c; unsigned char *b1 = um_buf_new(600), *b2 = um_buf_new(600), *e1, *e2;
            if (!UM_FN(w ? "HWND_UserMarshal64" : "HWND_UserMarshal") || !UM_FN(w ? "STGMEDIUM_UserMarshal64" : "STGMEDIUM_UserMarshal")) continue;
            if (w && (!UM_FN("SNB_UserMarshal64") || !UM_FN("HGLOBAL_UserMarshal64") || !UM_FN("CLIPFORMAT_UserMarshal64"))) continue;
            um_init(&c, b1, 600, C_LOCAL); e1 = t[i].fn(&c, b1 + 8, w);
            um_init(&c, b2, 600, C_LOCAL); e2 = t[i].fn(&c, b2 + 40, w);
            CHECKF(e1 && e2 && (e1 - b1) - 8 == (e2 - b2) - 40, "%s w=%d length depends on position", t[i].name, w);
            if (e1 && e2) CHECKF(!memcmp(b1 + 8, b2 + 40, e1 - (b1 + 8)), "%s w=%d bytes differ when shifted by 32", t[i].name, w);
            um_buf_free(b1); um_buf_free(b2);
        }
    GlobalFree(g_hg); free(g_snbp);
}

/* ---- ole32 vs combase ---------------------------------------------------------------------------------------------- */
TEST(usr_combase_parity_clipfmt_hglobal)
{
    static const char *ops[] = { "CLIPFORMAT_UserSize", "CLIPFORMAT_UserSize64", "HGLOBAL_UserSize", "HGLOBAL_UserSize64" };
    static const char *mar[] = { "CLIPFORMAT_UserMarshal", "CLIPFORMAT_UserMarshal64", "HGLOBAL_UserMarshal", "HGLOBAL_UserMarshal64" };
    int i; ULONG ctx;
    for (i = 0; i < 4; i++) {
        void *so = UM_FNIN("ole32.dll", ops[i]), *sc = UM_FNIN("combase.dll", ops[i]), *mo = UM_FNIN("ole32.dll", mar[i]), *mc = UM_FNIN("combase.dll", mar[i]);
        if (!so || !sc || !mo || !mc) { t_note("%s not exported by both modules", ops[i]); continue; }
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            UMC c; ULONG r1, r2; unsigned char *b1 = um_buf_new(300), *b2 = um_buf_new(300), *e1, *e2;
            HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, 33); CLIPFORMAT cf = 13; void *obj = i < 2 ? (void *)&cf : (void *)&hg;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(so, UMF(&c), 5, obj, &r1), 0); XEQ(um_x_size(sc, UMF(&c), 5, obj, &r2), 0);
            CHECKF(r1 == r2, "%s ctx %lu: ole32 %lu vs combase %lu", ops[i], ctx, r1, r2);
            um_init(&c, b1, 300, ctx); XEQ(um_x_mar(mo, UMF(&c), b1 + 5, obj, &e1), 0);
            um_init(&c, b2, 300, ctx); XEQ(um_x_mar(mc, UMF(&c), b2 + 5, obj, &e2), 0);
            CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 300));
            GlobalFree(hg); um_buf_free(b1); um_buf_free(b2);
        }
    }
}

TEST(usr_combase_parity_gdi)
{
    static const char *ops[] = { "HBITMAP_UserSize", "HPALETTE_UserSize" };
    static const char *mar[] = { "HBITMAP_UserMarshal", "HPALETTE_UserMarshal" };
    HBITMAP (WINAPI *cbm)(int, int, UINT, UINT, const void *) = (void *)t_proc_in("gdi32.dll", "CreateBitmap");
    HPALETTE (WINAPI *cpal)(const LOGPALETTE *) = (void *)t_proc_in("gdi32.dll", "CreatePalette");
    BOOL (WINAPI *del)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    LOGPALETTE lp[2]; HGDIOBJ objs[2]; int i; ULONG ctx;
    REQUIRE(cbm); REQUIRE(cpal); REQUIRE(del);
    memset(lp, 0, sizeof lp); lp[0].palVersion = 0x300; lp[0].palNumEntries = 1; lp[0].palPalEntry[0].peRed = 9;
    objs[0] = cbm(9, 9, 1, 1, NULL); objs[1] = cpal(lp);
    for (i = 0; i < 2; i++) {
        void *so = UM_FNIN("ole32.dll", ops[i]), *sc = UM_FNIN("combase.dll", ops[i]), *mo = UM_FNIN("ole32.dll", mar[i]), *mc = UM_FNIN("combase.dll", mar[i]);
        if (!so || !sc || !mo || !mc || !objs[i]) { t_note("%s unavailable in both modules", ops[i]); continue; }
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            UMC c; ULONG r1, r2; unsigned char *b1 = um_buf_new(300), *b2 = um_buf_new(300), *e1, *e2; HGDIOBJ o = objs[i];
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_size(so, UMF(&c), 1, &o, &r1), 0); XEQ(um_x_size(sc, UMF(&c), 1, &o, &r2), 0);
            CHECKF(r1 == r2, "%s ctx %lu: %lu vs %lu", ops[i], ctx, r1, r2);
            um_init(&c, b1, 300, ctx); XEQ(um_x_mar(mo, UMF(&c), b1 + 1, &o, &e1), 0);
            um_init(&c, b2, 300, ctx); XEQ(um_x_mar(mc, UMF(&c), b2 + 1, &o, &e2), 0);
            CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 300));
            um_buf_free(b1); um_buf_free(b2);
        }
    }
    if (objs[0]) del(objs[0]);
    if (objs[1]) del(objs[1]);
}

TEST(usr_hglobal64_inproc_roundtrip)
{
    void *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal64"), *fu = UM_FNIN("ole32.dll", "HGLOBAL_UserUnmarshal64");
    HGLOBAL h, o = NULL; UMC c; unsigned char *b = um_buf_new(64), *e1, *e2;
    REQUIRE(fm); REQUIRE(fu);
    h = GlobalAlloc(GMEM_MOVEABLE, 24);
    um_init(&c, b, 64, C_INPROC); XEQ(um_x_mar(fm, UMF(&c), b + 3, &h, &e1), 0);
    CHECK(e1 == b + 16);
    um_init(&c, b, 16, C_INPROC); XEQ(um_x_mar(fu, UMF(&c), b + 3, &o, &e2), 0);
    CHECK(o == h && e2 == e1);
    CHECK(GlobalSize(h) >= 24);
    GlobalFree(h);
    um_buf_free(b);
}

TEST(usr_hglobal_size_vs_marshal_gap)
{
    /* Size() always reserves at least as much as Marshal() writes; on Win10 x64 the surplus is exactly 8 for a
     * non-NULL block and 8 for NULL (Wine: "native allocates 4/8 bytes more than it needs"). */
    void *fs = UM_FNIN("ole32.dll", "HGLOBAL_UserSize"), *fm = UM_FNIN("ole32.dll", "HGLOBAL_UserMarshal");
    ULONG off, n; REQUIRE(fs); REQUIRE(fm);
    for (n = 0; n <= 17; n++) {
        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
        for (off = 0; off < 5; off++) {
            UMC c; ULONG sz; unsigned char *b = um_buf_new(200), *e;
            um_init(&c, NULL, 0, C_LOCAL); XEQ(um_x_size(fs, UMF(&c), off, &h, &sz), 0);
            um_init(&c, b, 200, C_LOCAL); XEQ(um_x_mar(fm, UMF(&c), b + off, &h, &e), 0);
            CHECKF(sz - (ULONG)(e - b) == 8, "n=%lu off %lu: surplus %ld", n, off, (long)sz - (long)(e - b));
            um_buf_free(b);
        }
        GlobalFree(h);
    }
}
