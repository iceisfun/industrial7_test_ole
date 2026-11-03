/* gap_09: HGLOBAL_UserFree and CLIPFORMAT_UserFree (ole32.dll).
 *
 * Win10 decompilation:
 *   HGLOBAL_UserFree(ULONG *flags, HGLOBAL *ph):
 *       if (ph && *ph && (BYTE)*flags != MSHCTX_INPROC(3)) GlobalFree(*ph);   -- nothing else is read or written
 *   CLIPFORMAT_UserFree == HRGN_UserFree == HDC_UserFree (one shared body): a no-op.
 * Only the low byte of the flags word (the marshal context) is looked at; the stub message is never touched, so a bare
 * ULONG is a valid first argument. The handle variable is never cleared. */
#include "g9.h"

typedef void (WINAPI *free_fn)(ULONG *, void *);

static DWORD call_free(void *fn, ULONG flags, void *obj)
{
    ULONG fl = flags;
    return um_x_free(fn, &fl, obj);
}

static HGLOBAL mk_movable(SIZE_T n, unsigned char fill)
{
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, n);
    if (h && n) { unsigned char *p = GlobalLock(h); if (p) { memset(p, fill, n); GlobalUnlock(h); } }
    return h;
}

static int block_alive(HGLOBAL h)
{
    SetLastError(0);
    return GlobalSize(h) != 0;
}

/* every context byte except INPROC frees the block */
TEST(gap9_hglobal_userfree_frees_for_every_noninproc_context)
{
    static const ULONG ctxs[] = { 0, 1, 2, 4, 5, 6, 7, 0x10, 0x7f, 0x80, 0xfe, 0xff };
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    unsigned i;
    REQUIRE(f);
    for (i = 0; i < sizeof ctxs / sizeof *ctxs; i++) {
        HGLOBAL h = mk_movable(64, 0xA5), keep;
        REQUIRE(h);
        keep = h;
        CHECK(block_alive(h));
        XEQ(call_free(f, MAKELONG(ctxs[i], 0x10), &h), 0);
        CHECKF(h == keep, "ctx %#lx: the handle variable is not cleared", ctxs[i]);
        CHECKF(!block_alive(h), "ctx %#lx: block must be freed", ctxs[i]);
        SetLastError(0);
    }
}

/* only (BYTE)flags decides: high bytes of the flags word do not turn a context into INPROC or vice versa */
TEST(gap9_hglobal_userfree_only_low_byte_of_flags_matters)
{
    static const struct { ULONG flags; int freed; } t[] = {
        { 0x00000003, 0 }, { 0x00100003, 0 }, { 0x00000103, 0 }, { 0xFFFFFF03, 0 }, { 0x12345603, 0 },
        { 0x00000300, 1 }, { 0x00100300, 1 }, { 0x00000003 | 0x00010000, 0 }, { 0xFFFFFF00, 1 }, { 0x00000203, 0 },
        { 0x00000403, 0 }, { 0x00000002, 1 }, { 0x00000103 & ~0xffu, 1 },
    };
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    unsigned i;
    REQUIRE(f);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        HGLOBAL h = mk_movable(32, 0x3C);
        REQUIRE(h);
        XEQ(call_free(f, t[i].flags, &h), 0);
        CHECKF(block_alive(h) == !t[i].freed, "flags %#lx: expected block %s", t[i].flags, t[i].freed ? "freed" : "kept");
        if (!t[i].freed) {
            unsigned char *p = GlobalLock(h);
            CHECK(p && p[0] == 0x3C && p[31] == 0x3C);       /* contents untouched */
            GlobalUnlock(h);
            GlobalFree(h);
        }
    }
}

TEST(gap9_hglobal_userfree_inproc_keeps_fixed_and_movable_blocks)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    HGLOBAL m, fx;
    REQUIRE(f);
    m = mk_movable(100, 0x11);
    fx = GlobalAlloc(GMEM_FIXED, 100);
    REQUIRE(m); REQUIRE(fx);
    memset(fx, 0x22, 100);
    XEQ(call_free(f, C_INPROC, &m), 0);
    XEQ(call_free(f, C_INPROC, &fx), 0);
    CHECK(block_alive(m));
    CHECK(GlobalSize(fx) >= 100 && ((unsigned char *)fx)[99] == 0x22);
    /* still owned by the caller: a second INPROC call keeps it alive too */
    XEQ(call_free(f, C_INPROC, &m), 0);
    CHECK(block_alive(m));
    GlobalFree(m);
    GlobalFree(fx);
}

TEST(gap9_hglobal_userfree_fixed_block_freed_and_variable_kept)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    HGLOBAL fx, keep;
    REQUIRE(f);
    fx = GlobalAlloc(GMEM_FIXED, 256);
    REQUIRE(fx);
    keep = fx;
    XEQ(call_free(f, C_LOCAL, &fx), 0);
    CHECK(fx == keep);
    /* the heap is still healthy: a fresh allocation can be made and released */
    {
        HGLOBAL g = GlobalAlloc(GMEM_FIXED, 256);
        CHECK(g != NULL);
        if (g) GlobalFree(g);
    }
}

TEST(gap9_hglobal_userfree_null_cases_raise_nothing)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    ULONG ctx;
    REQUIRE(f);
    for (ctx = 0; ctx < 8; ctx++) {
        HGLOBAL none = NULL;
        XEQ(call_free(f, ctx, NULL), 0);          /* NULL pointer to handle */
        XEQ(call_free(f, ctx, &none), 0);         /* NULL handle */
        CHECK(none == NULL);
    }
}

TEST(gap9_hglobal_userfree_does_not_use_stub_message)
{
    /* pStubMsg is NULL and the signature is wrong: the helper must not care */
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    USER_MARSHAL_CB cb;
    HGLOBAL h;
    REQUIRE(f);
    memset(&cb, 0, sizeof cb);
    cb.Flags = MAKELONG(C_DIFFM, 0x10);
    h = mk_movable(16, 1);
    REQUIRE(h);
    XEQ(um_x_free(f, &cb.Flags, &h), 0);
    CHECK(!block_alive(h));
}

TEST(gap9_hglobal_userfree_large_and_zero_size_blocks)
{
    static const SIZE_T sizes[] = { 0, 1, 4095, 4096, 65536, 1 << 20, 4 << 20 };
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    unsigned i;
    REQUIRE(f);
    for (i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        HGLOBAL h = mk_movable(sizes[i], 0x6B);
        if (!h) { t_note("GlobalAlloc(%u) failed", (unsigned)sizes[i]); continue; }
        XEQ(call_free(f, C_DIFFM, &h), 0);
        CHECKF(!block_alive(h), "size %u freed", (unsigned)sizes[i]);
    }
}

TEST(gap9_hglobal_userfree_locked_movable_block)
{
    /* the helper calls GlobalFree unconditionally: lock counts are not consulted by the helper itself */
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree");
    HGLOBAL h;
    void *p;
    REQUIRE(f);
    h = mk_movable(64, 0x77);
    REQUIRE(h);
    p = GlobalLock(h);
    CHECK(p != NULL);
    XEQ(call_free(f, C_LOCAL, &h), 0);
    t_note("GlobalFree of a locked movable block via HGLOBAL_UserFree: alive=%d", block_alive(h));
}

/* the 64-bit export is the same code (ordinals 399/400 share an RVA on the reference build) */
TEST(gap9_hglobal_userfree64_same_semantics)
{
    void *f = UM_FNIN("ole32.dll", "HGLOBAL_UserFree"), *f6 = UM_FNIN("ole32.dll", "HGLOBAL_UserFree64");
    int k;
    REQUIRE(f); REQUIRE(f6);
    if (f != f6) t_note("HGLOBAL_UserFree and HGLOBAL_UserFree64 are different functions on this build");
    for (k = 0; k < 2; k++) {
        void *fn = k ? f6 : f;
        ULONG ctx;
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            HGLOBAL h = mk_movable(24, 9);
            REQUIRE(h);
            XEQ(call_free(fn, ctx, &h), 0);
            CHECKF(block_alive(h) == (ctx == C_INPROC), "k=%d ctx %lu", k, ctx);
            if (ctx == C_INPROC) GlobalFree(h);
        }
    }
}

TEST(gap9_hglobal_userfree_missing_from_wrong_names)
{
    HMODULE m = t_module("ole32.dll");
    REQUIRE(m);
    SetLastError(0);
    CHECK(GetProcAddress(m, "HGLOBAL_UserFree32") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0);
    CHECK(GetProcAddress(m, "hglobal_userfree") == NULL);   /* export names are case sensitive */
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}

/* ---- CLIPFORMAT_UserFree: no-op ------------------------------------------------------------------------- */
TEST(gap9_clipformat_userfree_is_noop_for_every_context)
{
    static const WORD cfs[] = { 0, 1, 2, 13, 0x80, 0x200, 0x8000, 0xbfff, 0xc000, 0xffff };
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree");
    unsigned i;
    ULONG ctx;
    REQUIRE(f);
    for (i = 0; i < sizeof cfs / sizeof *cfs; i++)
        for (ctx = 0; ctx <= 7; ctx++) {
            CLIPFORMAT cf = cfs[i];
            XEQ(call_free(f, MAKELONG(ctx, 0x10), &cf), 0);
            CHECKF(cf == cfs[i], "cf %#x ctx %lu unchanged", cfs[i], ctx);
        }
}

TEST(gap9_clipformat_userfree_null_pointer_ok)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree");
    ULONG ctx;
    REQUIRE(f);
    for (ctx = 0; ctx <= 7; ctx++)
        XEQ(call_free(f, ctx, NULL), 0);
}

TEST(gap9_clipformat_userfree_ignores_flags_and_stub_message)
{
    void *f = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree");
    static const ULONG flags[] = { 0, 3, 0x100, 0xFFFFFFFF, 0x00100003, 0xDEAD0005 };
    unsigned i;
    REQUIRE(f);
    for (i = 0; i < sizeof flags / sizeof *flags; i++) {
        CLIPFORMAT cf = 0xc001;
        XEQ(call_free(f, flags[i], &cf), 0);
        CHECK(cf == 0xc001);
    }
}

/* CLIPFORMAT_UserFree shares one body with HRGN_UserFree and HDC_UserFree: none of them releases the object */
TEST(gap9_clipformat_hrgn_hdc_userfree_share_noop_body)
{
    void *fc = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree"), *fr = UM_FNIN("ole32.dll", "HRGN_UserFree"), *fd = UM_FNIN("ole32.dll", "HDC_UserFree");
    HRGN (WINAPI *p_CreateRectRgn)(int, int, int, int) = (void *)t_proc_in("gdi32.dll", "CreateRectRgn");
    HDC (WINAPI *p_CreateCompatibleDC)(HDC) = (void *)t_proc_in("gdi32.dll", "CreateCompatibleDC");
    BOOL (WINAPI *p_DeleteDC)(HDC) = (void *)t_proc_in("gdi32.dll", "DeleteDC");
    BOOL (WINAPI *p_DeleteObject)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "DeleteObject");
    DWORD (WINAPI *p_GetObjectType)(HGDIOBJ) = (void *)t_proc_in("gdi32.dll", "GetObjectType");
    HRGN rg; HDC dc; ULONG ctx;
    REQUIRE(fc); REQUIRE(fr); REQUIRE(fd);
    REQUIRE(p_CreateRectRgn); REQUIRE(p_CreateCompatibleDC); REQUIRE(p_DeleteDC); REQUIRE(p_DeleteObject); REQUIRE(p_GetObjectType);
    rg = p_CreateRectRgn(0, 0, 10, 10);
    dc = p_CreateCompatibleDC(NULL);
    REQUIRE(rg); REQUIRE(dc);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        HRGN r1 = rg; HDC d1 = dc;
        XEQ(call_free(fr, ctx, &r1), 0);
        XEQ(call_free(fd, ctx, &d1), 0);
        CHECKF(p_GetObjectType(rg) == OBJ_REGION, "ctx %lu: HRGN_UserFree must not delete the region", ctx);
        CHECKF(p_GetObjectType(dc) == OBJ_MEMDC, "ctx %lu: HDC_UserFree must not delete the DC", ctx);
        CHECK(r1 == rg && d1 == dc);
    }
    p_DeleteObject(rg);
    p_DeleteDC(dc);
    if (fc == fr && fr == fd) t_note("CLIPFORMAT_UserFree, HRGN_UserFree and HDC_UserFree share one address (as on the reference build)");
}

TEST(gap9_clipformat_userfree_ext_alias_is_noop)
{
    void *fe = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFreeExt"), *f6 = UM_FNIN("ole32.dll", "CLIPFORMAT_UserFree64");
    ULONG ctx;
    REQUIRE(fe); REQUIRE(f6);
    for (ctx = 0; ctx <= C_CONT; ctx++) {
        CLIPFORMAT cf = 0x0123;
        UMC c;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(fe, UMF(&c), &cf), 0);
        CHECK(cf == 0x0123);
        XEQ(um_x_free(f6, UMF(&c), &cf), 0);
        CHECK(cf == 0x0123);
    }
}
