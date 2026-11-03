#include "g1.h"
#include <setjmp.h>
/* HACCEL_UserSize/Marshal/Free (+64 variants), exported by ole32.dll and implemented in combase.dll. On Win10 the
 * HACCEL helpers share one body with HWND/HMENU/...: the wire form is a 4-aligned { DWORD 'WdtH'; DWORD low32(handle) }
 * (8 bytes). A non-NULL handle with a remote context (low byte of the flags is MSHCTX_DIFFERENTMACHINE=2 or
 * MSHCTX_CROSSCONTAINER=5... i.e. REMOTE_CALL) raises RPC exception 0x6c5. Free is an empty function.
 * Only the flags word of USER_MARSHAL_CB is consulted by Size/Marshal, so a bare ULONG is passed as "flags". */

#define TAG_H 0x48746457u
#define X_BADTAG 0x6c5u

typedef ULONG (WINAPI *sz_fn)(ULONG *, ULONG, HACCEL *);
typedef unsigned char *(WINAPI *mar_fn)(ULONG *, unsigned char *, HACCEL *);
typedef void (WINAPI *free_fn)(ULONG *, HACCEL *);

static jmp_buf jb; static volatile int armed; static volatile DWORD xcode;
static LONG CALLBACK veh(PEXCEPTION_POINTERS ep)
{
    if (!armed) return EXCEPTION_CONTINUE_SEARCH;
    xcode = ep->ExceptionRecord->ExceptionCode; armed = 0; longjmp(jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void veh_init(void) { static int d; if (!d) { AddVectoredExceptionHandler(1, veh); d = 1; } }

static DWORD do_size(void *fn, ULONG ctx, ULONG off, HACCEL *h, ULONG *out)
{
    ULONG fl = MAKELONG(ctx, 0x10), r = 0xdeadbeef; DWORD x = 0; veh_init();
    if (!setjmp(jb)) { armed = 1; r = ((sz_fn)fn)(&fl, off, h); armed = 0; } else x = xcode;
    armed = 0; *out = r; return x;
}
static DWORD do_mar(void *fn, ULONG ctx, unsigned char *b, HACCEL *h, unsigned char **out)
{
    ULONG fl = MAKELONG(ctx, 0x10); unsigned char *r = (unsigned char *)(ULONG_PTR)0xdeadbeef; DWORD x = 0; veh_init();
    if (!setjmp(jb)) { armed = 1; r = ((mar_fn)fn)(&fl, b, h); armed = 0; } else x = xcode;
    armed = 0; *out = r; return x;
}
static DWORD do_free(void *fn, ULONG ctx, HACCEL *h)
{
    ULONG fl = MAKELONG(ctx, 0x10); DWORD x = 0; veh_init();
    if (!setjmp(jb)) { armed = 1; ((free_fn)fn)(&fl, h); armed = 0; } else x = xcode;
    armed = 0; return x;
}

#define FN(n) ((void *)t_proc_in("ole32.dll", n))
static void *fsize[2], *fmar[2], *ffree[2];
static int load(void)
{
    fsize[0] = FN("HACCEL_UserSize"); fsize[1] = FN("HACCEL_UserSize64");
    fmar[0] = FN("HACCEL_UserMarshal"); fmar[1] = FN("HACCEL_UserMarshal64");
    ffree[0] = FN("HACCEL_UserFree"); ffree[1] = FN("HACCEL_UserFree64");
    if (!fsize[0] || !fsize[1] || !fmar[0] || !fmar[1] || !ffree[0] || !ffree[1]) { t_skip("missing HACCEL helper"); return 0; }
    return 1;
}
static const ULONG nonremote[] = { 0, 1, 3, 4 };
static const ULONG remote[] = { 2, 5 };
static const ULONG_PTR hv[] = { 0, 1, 0x00123456, 0x7fffffff, 0x80000000, 0xffffffff };

TEST(gap1_haccel_exports)
{
    int i;
    const char *n[] = { "HACCEL_UserSize", "HACCEL_UserSize64", "HACCEL_UserMarshal", "HACCEL_UserMarshal64", "HACCEL_UserFree", "HACCEL_UserFree64" };
    for (i = 0; i < 6; i++) { if (t_is_wine() && !t_proc_in("ole32.dll", n[i])) { t_note("%s missing (emulator)", n[i]); continue; } CHECKF(t_proc_in("ole32.dll", n[i]) != NULL, "ole32!%s", n[i]); }
}

TEST(gap1_haccel_size_aligns_to_four_plus_eight)
{
    int w; unsigned c, v; ULONG off, r;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 4; c++)
            for (v = 0; v < 6; v++)
                for (off = 0; off < 20; off++) {
                    HACCEL h = (HACCEL)hv[v];
                    EXPECT_EQ(do_size(fsize[w], nonremote[c], off, &h, &r), 0);
                    CHECKF(r == ((off + 3) & ~3u) + 8, "w%d ctx %lu h %p off %lu -> %lu", w, nonremote[c], h, off, r);
                }
}

TEST(gap1_haccel_size_null_object_adds_nothing)
{
    int w; unsigned c; ULONG off, r;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 6; c++)
            for (off = 0; off < 12; off++) {
                ULONG ctx = c < 4 ? nonremote[c] : remote[c - 4];
                EXPECT_EQ(do_size(fsize[w], ctx, off, NULL, &r), 0);
                CHECKF(r == off, "w%d ctx %lu off %lu -> %lu", w, ctx, off, r);
            }
}

#define RD32(p, o) (*(ULONG *)((p) + (o)))

TEST(gap1_haccel_remote_context_rules)
{
    int w; unsigned c; ULONG r; unsigned char *e, *b = malloc(64);
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 2; c++) {
            HACCEL h = (HACCEL)(ULONG_PTR)0x1234, nul = NULL;
            memset(b, 0xcc, 64);
            /* a non-NULL handle cannot be sent to another machine / container */
            EXPECT_EQ(do_size(fsize[w], remote[c], 0, &h, &r), X_BADTAG);
            EXPECT_EQ(do_mar(fmar[w], remote[c], b, &h, &e), X_BADTAG);
            CHECK(b[0] == 0xcc && b[7] == 0xcc);       /* nothing written */
            /* a NULL handle can */
            EXPECT_EQ(do_size(fsize[w], remote[c], 0, &nul, &r), 0); EXPECT_EQ(r, 8);
            EXPECT_EQ(do_mar(fmar[w], remote[c], b, &nul, &e), 0); CHECK(e == b + 8);
            EXPECT_EQ(RD32(b, 0), TAG_H); EXPECT_EQ(RD32(b, 4), 0);
        }
    free(b);
}

TEST(gap1_haccel_marshal_wire_format)
{
    int w; unsigned c, v; ULONG pad;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 4; c++)
            for (v = 0; v < 6; v++)
                for (pad = 0; pad < 4; pad++) {
                    unsigned char *raw = malloc(48), *b, *e; HACCEL h = (HACCEL)hv[v];
                    memset(raw, 0xcc, 48); b = raw + 16;
                    EXPECT_EQ(do_mar(fmar[w], nonremote[c], b + pad, &h, &e), 0);
                    {   /* data starts at the next 4-byte boundary of the supplied pointer */
                        ULONG_PTR a = ((ULONG_PTR)(b + pad) + 3) & ~(ULONG_PTR)3;
                        unsigned char *d = (unsigned char *)a;
                        CHECKF(e == d + 8, "w%d ctx %lu pad %lu: end %p start %p", w, nonremote[c], pad, e, d);
                        EXPECT_EQ(RD32(d, 0), TAG_H);
                        EXPECT_EQ(RD32(d, 4), (ULONG)(ULONG_PTR)h);       /* low 32 bits of the handle value */
                        CHECK(d[8] == 0xcc && d[-1 + (d > b + pad ? 0 : 1)] == 0xcc);
                    }
                    free(raw);
                }
}

TEST(gap1_haccel_marshal_truncates_to_32_bits)
{
    HACCEL h = (HACCEL)(ULONG_PTR)0x1122334455667788ull; unsigned char b[16]; unsigned char *e; int w;
    if (!load()) return;
    for (w = 0; w < 2; w++) {
        memset(b, 0, sizeof b);
        EXPECT_EQ(do_mar(fmar[w], 0, b, &h, &e), 0);
        EXPECT_EQ(RD32(b, 0), TAG_H); EXPECT_EQ(RD32(b, 4), 0x55667788);
    }
}

TEST(gap1_haccel_marshal_null_object_is_noop)
{
    unsigned char *raw = malloc(32), *e; int w; unsigned c;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 4; c++) {
            memset(raw, 0xcc, 32);
            EXPECT_EQ(do_mar(fmar[w], nonremote[c], raw + 1, NULL, &e), 0);
            CHECK(e == raw + 1);                      /* returned unchanged, even if unaligned */
            { int i, ok = 1; for (i = 0; i < 32; i++) if (raw[i] != 0xcc) ok = 0; CHECK(ok); }
        }
    free(raw);
}

TEST(gap1_haccel_size_then_marshal_consistent)
{
    /* sizing two objects back to back, then marshaling them, fills exactly the sized region */
    int w; ULONG off; HACCEL a = (HACCEL)(ULONG_PTR)0x11, b = (HACCEL)(ULONG_PTR)0x22;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (off = 0; off < 4; off++) {
            ULONG s1, s2; unsigned char *buf = malloc(64), *p, *e1, *e2;
            memset(buf, 0xcc, 64);
            EXPECT_EQ(do_size(fsize[w], 0, off, &a, &s1), 0);
            EXPECT_EQ(do_size(fsize[w], 0, s1, &b, &s2), 0);
            CHECKF(s2 == ((off + 3) & ~3u) + 16, "off %lu total %lu", off, s2);
            p = buf + 8;  /* 8-aligned base; offset emulated by bumping the pointer */
            EXPECT_EQ(do_mar(fmar[w], 0, p + off, &a, &e1), 0);
            EXPECT_EQ(do_mar(fmar[w], 0, e1, &b, &e2), 0);
            CHECK(e2 - (p + off) == (long)(s2 - off));
            EXPECT_EQ(RD32(e1 - 8, 4), 0x11); EXPECT_EQ(RD32(e2 - 8, 4), 0x22);
            free(buf);
        }
}

TEST(gap1_haccel_free_is_noop)
{
    int w; unsigned c;
    if (!load()) return;
    for (w = 0; w < 2; w++)
        for (c = 0; c < 6; c++) {
            HACCEL h = (HACCEL)(ULONG_PTR)0x5150;
            ULONG ctx = c < 4 ? nonremote[c] : remote[c - 4];
            EXPECT_EQ(do_free(ffree[w], ctx, &h), 0);
            EXPECT_EQ(h, 0x5150);                       /* the handle is not touched (no DestroyAcceleratorTable) */
        }
}

TEST(gap1_haccel_accelerator_table_handle_roundtrip_value)
{
    /* a real accelerator table handle: its value survives the wire form and the table stays usable after Free */
    ACCEL ac[2] = { { FVIRTKEY, 'A', 100 }, { FVIRTKEY | FCONTROL, 'B', 101 } }; HACCEL h, h2 = NULL; unsigned char b[16], *e; int w;
    if (!load()) return;
    h = CreateAcceleratorTableW(ac, 2);
    if (!h) { t_skip("cannot create accelerator table"); return; }
    for (w = 0; w < 2; w++) {
        memset(b, 0, sizeof b);
        EXPECT_EQ(do_mar(fmar[w], 0, b, &h, &e), 0);
        EXPECT_EQ(RD32(b, 4), (ULONG)(ULONG_PTR)h);
        EXPECT_EQ(do_free(ffree[w], 0, &h), 0);
        CHECK(CopyAcceleratorTableW(h, NULL, 0) == 2);
    }
    (void)h2;
    DestroyAcceleratorTable(h);
}

TEST(gap1_haccel_same_as_hwnd_and_hmenu_helpers)
{
    /* HACCEL, HWND and HMENU share one wire form on Win10 */
    static const char *const fam[] = { "HWND", "HMENU", "HICON", "HBRUSH" };
    unsigned i; int w;
    if (!load()) return;
    for (i = 0; i < 4; i++)
        for (w = 0; w < 2; w++) {
            char nm[48]; void *fs, *fm; ULONG r1, r2; unsigned char b1[16], b2[16], *e1, *e2; HACCEL h = (HACCEL)(ULONG_PTR)0x4242;
            snprintf(nm, sizeof nm, "%s_UserSize%s", fam[i], w ? "64" : ""); fs = FN(nm);
            snprintf(nm, sizeof nm, "%s_UserMarshal%s", fam[i], w ? "64" : ""); fm = FN(nm);
            if (!fs || !fm) { t_note("%s helpers missing", fam[i]); continue; }
            do_size(fsize[w], 0, 3, &h, &r1); do_size(fs, 0, 3, &h, &r2); CHECKF(r1 == r2, "%s size %lu vs %lu", fam[i], r1, r2);
            memset(b1, 0xcc, 16); memset(b2, 0xcc, 16);
            do_mar(fmar[w], 0, b1, &h, &e1); do_mar(fm, 0, b2, &h, &e2);
            CHECKF(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 16), "%s marshal differs", fam[i]);
        }
}

TEST(gap1_haccel_ole32_combase_parity)
{
    int w; HACCEL h = (HACCEL)(ULONG_PTR)0x9999;
    if (!load()) return;
    for (w = 0; w < 2; w++) {
        void *cs = t_proc_in("combase.dll", w ? "HACCEL_UserSize64" : "HACCEL_UserSize");
        void *cm = t_proc_in("combase.dll", w ? "HACCEL_UserMarshal64" : "HACCEL_UserMarshal");
        ULONG r1, r2; unsigned char b1[16], b2[16], *e1, *e2;
        if (!cs || !cm) { t_note("combase does not export the HACCEL helpers"); continue; }
        do_size(fsize[w], 0, 1, &h, &r1); do_size(cs, 0, 1, &h, &r2); EXPECT_EQ(r1, r2);
        memset(b1, 0, 16); memset(b2, 0, 16);
        do_mar(fmar[w], 0, b1, &h, &e1); do_mar(cm, 0, b2, &h, &e2);
        CHECK(e1 - b1 == e2 - b2 && !memcmp(b1, b2, 16));
    }
}
