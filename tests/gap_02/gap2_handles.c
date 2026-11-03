/* gap_02: HDC / HMENU / HWND / HACCEL user-marshal helpers (all _UserSize/_UserMarshal/_UserUnmarshal/_UserFree and
 * their *64 siblings), looked up by literal export name in ole32.dll and combase.dll (and oleaut32.dll for HWND).
 *
 * Win10 decomp (HICON_UserSize / HRGN_UserMarshal64 / HRGN_UserUnmarshal64 / HRGN_UserFree; the code is folded so every
 * handle family and both bit-widths run the same body):
 *   Size:      NULL obj -> returns offset unchanged; else ((off + 3) & ~3) + 8. A non-zero handle with REMOTE_CALL(flags)
 *              (MSHCTX low byte 2 or 5) raises RPC exception 0x6c5.
 *   Marshal:   NULL obj -> returns buffer unchanged; remote+non-zero raises 0x6c5; else align4, writes 'WdtH' then the LOW 32 bits
 *              of the handle; returns aligned+8. Padding bytes are not written.
 *   Unmarshal: aligned = (buf+3)&~3; BufferSize check first (too short -> 0x6f7), then the tag (!= 'WdtH' -> 0x6c5), then the 32-bit
 *              value is SIGN-extended into the 64-bit handle which is stored BEFORE the remote check (non-zero + remote -> 0x6c5
 *              after *obj was written).
 *   Free:      empty function (compiled to a bare 'ret'): never touches flags or the object. */
#include "gap2.h"

typedef struct { const char *fam; const char *mod; int w64; const char *nsize, *nmar, *nunm, *nfree;
                 const void *size, *mar, *unm, *fre; } hvar;

typedef struct { const char *fam; const char *mod; const char *names[2][4]; } hrow;
static const hrow rows[] = {
 { "HDC", "ole32.dll", { { "HDC_UserSize", "HDC_UserMarshal", "HDC_UserUnmarshal", "HDC_UserFree" },
                         { "HDC_UserSize64", "HDC_UserMarshal64", "HDC_UserUnmarshal64", "HDC_UserFree64" } } },
 { "HDC", "combase.dll", { { "HDC_UserSize", "HDC_UserMarshal", "HDC_UserUnmarshal", "HDC_UserFree" },
                         { "HDC_UserSize64", "HDC_UserMarshal64", "HDC_UserUnmarshal64", "HDC_UserFree64" } } },
 { "HMENU", "ole32.dll", { { "HMENU_UserSize", "HMENU_UserMarshal", "HMENU_UserUnmarshal", "HMENU_UserFree" },
                         { "HMENU_UserSize64", "HMENU_UserMarshal64", "HMENU_UserUnmarshal64", "HMENU_UserFree64" } } },
 { "HMENU", "combase.dll", { { "HMENU_UserSize", "HMENU_UserMarshal", "HMENU_UserUnmarshal", "HMENU_UserFree" },
                         { "HMENU_UserSize64", "HMENU_UserMarshal64", "HMENU_UserUnmarshal64", "HMENU_UserFree64" } } },
 { "HWND", "ole32.dll", { { "HWND_UserSize", "HWND_UserMarshal", "HWND_UserUnmarshal", "HWND_UserFree" },
                         { "HWND_UserSize64", "HWND_UserMarshal64", "HWND_UserUnmarshal64", "HWND_UserFree64" } } },
 { "HWND", "combase.dll", { { "HWND_UserSize", "HWND_UserMarshal", "HWND_UserUnmarshal", "HWND_UserFree" },
                         { "HWND_UserSize64", "HWND_UserMarshal64", "HWND_UserUnmarshal64", "HWND_UserFree64" } } },
 { "HWND", "oleaut32.dll", { { "HWND_UserSize", "HWND_UserMarshal", "HWND_UserUnmarshal", "HWND_UserFree" },
                         { "HWND_UserSize64", "HWND_UserMarshal64", "HWND_UserUnmarshal64", "HWND_UserFree64" } } },
 { "HACCEL", "ole32.dll", { { "HACCEL_UserSize", "HACCEL_UserMarshal", "HACCEL_UserUnmarshal", "HACCEL_UserFree" },
                         { "HACCEL_UserSize64", "HACCEL_UserMarshal64", "HACCEL_UserUnmarshal64", "HACCEL_UserFree64" } } },
 { "HACCEL", "combase.dll", { { "HACCEL_UserSize", "HACCEL_UserMarshal", "HACCEL_UserUnmarshal", "HACCEL_UserFree" },
                         { "HACCEL_UserSize64", "HACCEL_UserMarshal64", "HACCEL_UserUnmarshal64", "HACCEL_UserFree64" } } },
};

static hvar vars[ARRAY_N(rows) * 2];
static int nvars;
static void build(void)
{
    unsigned i; int w;
    if (nvars) return;
    for (i = 0; i < ARRAY_N(rows); i++)
        for (w = 0; w < 2; w++) {
            hvar *v = &vars[nvars++];
            v->fam = rows[i].fam; v->mod = rows[i].mod; v->w64 = w;
            v->nsize = rows[i].names[w][0]; v->nmar = rows[i].names[w][1];
            v->nunm = rows[i].names[w][2]; v->nfree = rows[i].names[w][3];
            v->size = t_proc_in(v->mod, v->nsize); v->mar = t_proc_in(v->mod, v->nmar);
            v->unm = t_proc_in(v->mod, v->nunm); v->fre = t_proc_in(v->mod, v->nfree);
        }
}
static int complete(const hvar *v) { return v->size && v->mar && v->unm && v->fre; }
#define FOR_VARS(v) build(); for (hvar *v = vars; v < vars + nvars; v++) if (complete(v))
#define LBL(v) (v)->fam, (v)->mod, (v)->w64 ? "64" : ""

/* handle values: zero, small, typical, 32-bit sign bit, and 64-bit values whose low halves differ from the whole */
static const ULONG_PTR hvals[] = { 0, 1, 0x00123456, 0x7fffffff, 0x80000000, 0xffffffff };
static const ULONG nonremote[] = { C_LOCAL, C_NOSHM, C_INPROC, C_CROSSX };
static const ULONG remote[] = { C_DIFFM, C_CONT };

/* adapted from Wine dlls/ole32/tests/usrmarshal.c (LGPL-2.1+): test_marshal_HWND/HDC existence checks, widened to every export */
TEST(gap2_handle_exports_resolve)
{
    unsigned i; int w, k;
    for (i = 0; i < ARRAY_N(rows); i++)
        for (w = 0; w < 2; w++)
            for (k = 0; k < 4; k++) {
                const void *f = t_proc_in(rows[i].mod, rows[i].names[w][k]);
                if (!f && !strcmp(rows[i].mod, "oleaut32.dll")) { t_note("oleaut32.dll does not export %s", rows[i].names[w][k]); continue; }
                if (!f && t_is_wine()) { t_note("emulator lacks %s!%s", rows[i].mod, rows[i].names[w][k]); continue; }
                CHECKF(f != NULL, "%s!%s missing", rows[i].mod, rows[i].names[w][k]);
            }
}

TEST(gap2_handle_export_identity_notes)
{
    FOR_VARS(v) {
        const hvar *o = NULL; hvar *q;
        for (q = vars; q < vars + nvars; q++)
            if (q != v && q->fam == v->fam && q->w64 == v->w64 && !strcmp(q->mod, "ole32.dll")) o = q;
        if (o && complete(o) && strcmp(v->mod, "ole32.dll"))
            t_note("%s%s: ole32 size/unmarshal %s combase", v->fam, v->w64 ? "64" : "", o->size == v->size ? "==" : "!=");
    }
}

/* ---- Size ---- */
TEST(gap2_handle_size_null_obj)
{
    if (t_is_wine()) { t_skip("emulator crashes on a NULL object pointer"); return; }
    FOR_VARS(v) {
        ULONG off, ctx, r;
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 18; off++) {
                G2Ctx c; g2_init(&c, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
                XEQ(g2_x_size(v->size, G2F(&c), off, NULL, &r), 0);
                CHECKF(r == off, "%s %s %s ctx %lu off %lu got %lu", v->fam, v->mod, v->nsize, ctx, off, r);
            }
    }
}
TEST(gap2_handle_size_aligns_to_four_plus_eight)
{
    FOR_VARS(v) {
        unsigned i; ULONG off, r;
        for (i = 0; i < ARRAY_N(hvals); i++)
            for (off = 0; off < 40; off++) {
                G2Ctx c; HANDLE h = (HANDLE)hvals[i];
                g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
                XEQ(g2_x_size(v->size, G2F(&c), off, &h, &r), 0);
                if (hvals[i] == 0) { CHECKF(r == G2_ALIGN(off, 4) + 8, "%s %s %s null handle still sizes: off %lu got %lu", v->fam, v->mod, v->nsize, off, r); }
                else CHECKF(r == G2_ALIGN(off, 4) + 8, "%s %s %s off %lu got %lu", v->fam, v->mod, v->nsize, off, r);
            }
    }
}
TEST(gap2_handle_size_ignores_64bit_width)
{
    /* the 64 variants are not 8-aligned and not 16 bytes: 4-aligned, 8 bytes, like the 32 ones */
    FOR_VARS(v) {
        ULONG r; G2Ctx c; HANDLE h = (HANDLE)0x1234;
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        XEQ(g2_x_size(v->size, G2F(&c), 1, &h, &r), 0);
        EQ64(r, 12);
        XEQ(g2_x_size(v->size, G2F(&c), 4, &h, &r), 0);
        EQ64(r, 12);
        XEQ(g2_x_size(v->size, G2F(&c), 5, &h, &r), 0);
        EQ64(r, 16);
        g2_free(&c);
    }
}
TEST(gap2_handle_size_context_matrix)
{
    if (t_is_wine()) { t_skip("emulator has no REMOTE_CALL check"); return; }
    FOR_VARS(v) {
        unsigned k; ULONG r;
        for (k = 0; k < ARRAY_N(nonremote); k++) {
            G2Ctx c; HANDLE h = (HANDLE)0x4321;
            g2_init(&c, 0, nonremote[k], USER_MARSHAL_CB_BUFFER_SIZE);
            XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), 0);
            CHECKF(r == 8, "%s %s ctx %lu: %lu", v->fam, v->nsize, nonremote[k], r);
        }
        for (k = 0; k < ARRAY_N(remote); k++) {
            G2Ctx c; HANDLE h = (HANDLE)0x4321, nul = NULL;
            g2_init(&c, 0, remote[k], USER_MARSHAL_CB_BUFFER_SIZE);
            XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), X_BADTAG);
            XEQ(g2_x_size(v->size, G2F(&c), 0, &nul, &r), 0);    /* NULL handle is fine in a remote context */
            CHECKF(r == 8, "%s %s remote null: %lu", v->fam, v->nsize, r);
        }
    }
}
TEST(gap2_handle_size_remote_wide_handle)
{
    if (t_is_wine()) { t_skip("emulator has no REMOTE_CALL check"); return; }
    /* the remote test looks at the full 64-bit handle: 0x1_0000_0000 is non-zero although its low half is zero */
    FOR_VARS(v) {
        G2Ctx c; HANDLE h = (HANDLE)(ULONG_PTR)0x100000000ull; ULONG r;
        g2_init(&c, 0, C_DIFFM, USER_MARSHAL_CB_BUFFER_SIZE);
        XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), X_BADTAG);
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), 0);
        EQ64(r, 8);
    }
}
TEST(gap2_handle_size_flags_high_bits_ignored)
{
    if (t_is_wine()) { t_skip("emulator has no REMOTE_CALL check"); return; }
    FOR_VARS(v) {
        G2Ctx c; HANDLE h = (HANDLE)0x42; ULONG r;
        g2_init(&c, 0, C_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        c.umcb.Flags = 0x0100 | (0x10u << 16) | C_LOCAL;
        XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), 0); EQ64(r, 8);
        c.umcb.Flags = 0xff00 | (0x00u << 16) | C_DIFFM;
        XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), X_BADTAG);
        c.umcb.Flags = 0x0100 | C_CONT;
        XEQ(g2_x_size(v->size, G2F(&c), 0, &h, &r), X_BADTAG);
        c.umcb.Flags = 0x0200 | C_CROSSX;
        XEQ(g2_x_size(v->size, G2F(&c), 3, &h, &r), 0); EQ64(r, 12);
    }
}

/* ---- Marshal ---- */
TEST(gap2_handle_marshal_wire_layout)
{
    FOR_VARS(v) {
        unsigned i; ULONG off;
        for (i = 0; i < ARRAY_N(hvals); i++)
            for (off = 0; off < 6; off++) {
                G2Ctx c; HANDLE h = (HANDLE)hvals[i]; unsigned char *end; ULONG a = G2_ALIGN(off, 4);
                if (off && (ULONG_PTR)0) {}
                g2_init(&c, 32, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
                XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, &h, &end), 0);
                CHECKF(end == c.buf + a + 8, "%s %s off %lu end %+ld", v->fam, v->nmar, off, (long)(end - c.buf));
                EQ64(g2_rd32(c.buf + a), G2_TAG_H);
                EQ64(g2_rd32(c.buf + a + 4), (ULONG)hvals[i]);
                CHECK(g2_untouched(&c, 0, a));
                CHECK(g2_untouched(&c, a + 8, 32));
                CHECK(g2_canary_ok(&c));
                g2_free(&c);
            }
    }
}
TEST(gap2_handle_marshal_writes_low_32_bits)
{
    FOR_VARS(v) {
        static const unsigned long long hv[] = { 0x1234567890ull, 0xffffffff80000001ull, 0x100000000ull, 0xdeadbeefcafef00dull };
        unsigned i;
        for (i = 0; i < ARRAY_N(hv); i++) {
            G2Ctx c; HANDLE h = (HANDLE)(ULONG_PTR)hv[i]; unsigned char *end;
            g2_init(&c, 16, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &h, &end), 0);
            EQ64(g2_rd32(c.buf + 4), (ULONG)hv[i]);
            CHECK(h == (HANDLE)(ULONG_PTR)hv[i]);      /* input untouched */
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_marshal_null_obj)
{
    if (t_is_wine()) { t_skip("emulator crashes on a NULL object pointer"); return; }
    FOR_VARS(v) {
        ULONG off, ctx;
        for (ctx = 0; ctx <= C_CONT; ctx++)
            for (off = 0; off < 4; off++) {
                G2Ctx c; unsigned char *end;
                g2_init(&c, 16, ctx, USER_MARSHAL_CB_MARSHALL);
                XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, NULL, &end), 0);
                CHECKF(end == c.buf + off, "%s %s: NULL obj must return the buffer unchanged, got %+ld", v->fam, v->nmar, (long)(end - c.buf - off));
                CHECK(g2_untouched(&c, 0, 16));
                g2_free(&c);
            }
    }
}
TEST(gap2_handle_marshal_remote_context)
{
    if (t_is_wine()) { t_skip("emulator has no REMOTE_CALL check"); return; }
    FOR_VARS(v) {
        unsigned k;
        for (k = 0; k < ARRAY_N(remote); k++) {
            G2Ctx c; HANDLE h = (HANDLE)0x77, nul = NULL; unsigned char *end;
            g2_init(&c, 16, remote[k], USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &h, &end), X_BADTAG);
            CHECK(g2_untouched(&c, 0, 16));              /* the check precedes any write */
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &nul, &end), 0);
            CHECK(end == c.buf + 8);
            EQ64(g2_rd32(c.buf), G2_TAG_H);
            EQ64(g2_rd32(c.buf + 4), 0);
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_marshal_all_local_contexts)
{
    FOR_VARS(v) {
        unsigned k;
        for (k = 0; k < ARRAY_N(nonremote); k++) {
            G2Ctx c; HANDLE h = (HANDLE)0x1020; unsigned char *end;
            g2_init(&c, 16, nonremote[k], USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf, &h, &end), 0);
            CHECK(end == c.buf + 8);
            EQ64(g2_rd32(c.buf + 4), 0x1020);
            g2_free(&c);
        }
    }
}

/* ---- Unmarshal ---- */
TEST(gap2_handle_unmarshal_roundtrip)
{
    FOR_VARS(v) {
        unsigned i, k;
        for (k = 0; k < ARRAY_N(nonremote); k++)
            for (i = 0; i < 4; i++) {
                G2Ctx c; HANDLE h = (HANDLE)hvals[i], o = (HANDLE)0x5555; unsigned char *e1, *e2;
                g2_init(&c, 32, nonremote[k], USER_MARSHAL_CB_MARSHALL);
                XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + 1, &h, &e1), 0);
                g2_reread(&c, (ULONG)(e1 - c.buf), nonremote[k]);
                XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 1, &o, &e2), 0);
                CHECK(e2 == e1);
                CHECKF(o == h, "%s %s: %p != %p", v->fam, v->nunm, o, h);
                g2_free(&c);
            }
    }
}
TEST(gap2_handle_unmarshal_sign_extends)
{
    static const struct { ULONG wire; ULONG_PTR want; } t[] = {
        { 0x80000001u, (ULONG_PTR)0xFFFFFFFF80000001ull }, { 0xFFFFFFFFu, (ULONG_PTR)-1 }, { 0x7FFFFFFFu, 0x7FFFFFFF },
        { 0x00010002u, 0x10002 }, { 0x80000000u, (ULONG_PTR)0xFFFFFFFF80000000ull }, { 1u, 1 },
    };
    FOR_VARS(v) {
        unsigned k;
        for (k = 0; k < ARRAY_N(t); k++) {
            G2Ctx c; HANDLE o = NULL; unsigned char *e;
            g2_init(&c, 8, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
            g2_wr32(c.buf, G2_TAG_H); g2_wr32(c.buf + 4, t[k].wire);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), 0);
            CHECKF((ULONG_PTR)o == t[k].want, "%s %s wire %08lx got %p", v->fam, v->nunm, t[k].wire, o);
            CHECK(e == c.buf + 8);
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_unmarshal_bad_tag)
{
    if (t_is_wine()) { t_skip("emulator raises a different code"); return; }
    static const ULONG tags[] = { 0, 0x52746457u /* WdtR */, 0x50746457u /* WdtP */, 0x48746458u, 0xFFFFFFFFu, 1, 0x57647448u /* byte swapped */ };
    FOR_VARS(v) {
        unsigned t;
        for (t = 0; t < ARRAY_N(tags); t++) {
            G2Ctx c; HANDLE o = (HANDLE)9; unsigned char *e;
            g2_init(&c, 8, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
            g2_wr32(c.buf, tags[t]); g2_wr32(c.buf + 4, 5);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), X_BADTAG);
            CHECKF(o == (HANDLE)9, "%s %s tag %08lx: output must stay untouched", v->fam, v->nunm, tags[t]);
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_unmarshal_remote_nonzero_raises_after_store)
{
    if (t_is_wine()) { t_skip("emulator has no REMOTE_CALL check"); return; }
    FOR_VARS(v) {
        unsigned k;
        for (k = 0; k < ARRAY_N(remote); k++) {
            G2Ctx c; HANDLE o = (HANDLE)9; unsigned char *e;
            g2_init(&c, 8, remote[k], USER_MARSHAL_CB_UNMARSHALL);
            g2_wr32(c.buf, G2_TAG_H); g2_wr32(c.buf + 4, 0x1234);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), X_BADTAG);
            CHECKF(o == (HANDLE)0x1234, "%s %s: decomp stores the handle before the remote check, o=%p", v->fam, v->nunm, o);
            g2_wr32(c.buf + 4, 0);
            o = (HANDLE)9;
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), 0);
            CHECK(o == NULL); CHECK(e == c.buf + 8);
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_unmarshal_truncated)
{
    if (t_is_wine()) { t_skip("emulator does not bound-check unmarshal buffers"); return; }
    FOR_VARS(v) {
        ULONG len;
        for (len = 0; len <= 10; len++) {
            G2Ctx c; HANDLE o = NULL; unsigned char *e;
            g2_init(&c, 16, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
            g2_wr32(c.buf, G2_TAG_H); g2_wr32(c.buf + 4, 5);
            g2_reread(&c, len, C_LOCAL);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), len < 8 ? X_BADSTUB : 0);
            if (len >= 8) CHECK(o == (HANDLE)5); else CHECK(o == NULL);
            g2_free(&c);
        }
        {   /* the alignment padding counts: supplied=buf+1 aligns to buf+4 and needs 12 bytes */
            G2Ctx c; HANDLE o = NULL; unsigned char *e;
            g2_init(&c, 32, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
            g2_wr32(c.buf + 4, G2_TAG_H); g2_wr32(c.buf + 8, 5);
            g2_reread(&c, 11, C_LOCAL);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 1, &o, &e), X_BADSTUB);
            g2_reread(&c, 12, C_LOCAL);
            XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 1, &o, &e), 0);
            CHECK(e == c.buf + 12); CHECK(o == (HANDLE)5);
            g2_free(&c);
        }
    }
}
TEST(gap2_handle_unmarshal_size_check_precedes_tag_check)
{
    if (t_is_wine()) { t_skip("emulator does not bound-check unmarshal buffers"); return; }
    FOR_VARS(v) {
        G2Ctx c; HANDLE o = (HANDLE)9; unsigned char *e;
        g2_init(&c, 16, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        g2_wr32(c.buf, 0xdeadbeef); g2_wr32(c.buf + 4, 5);
        g2_reread(&c, 4, C_LOCAL);                                    /* too short AND bad tag */
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), X_BADSTUB);
        g2_reread(&c, 8, C_LOCAL);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), X_BADTAG);
        CHECK(o == (HANDLE)9);
        g2_free(&c);
    }
}
TEST(gap2_handle_unmarshal_supplied_pointer_range)
{
    if (t_is_wine()) { t_skip("emulator does not bound-check unmarshal buffers"); return; }
    FOR_VARS(v) {
        G2Ctx c; HANDLE o = NULL; unsigned char *e;
        g2_init(&c, 64, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        g2_wr32(c.buf + 16, G2_TAG_H); g2_wr32(c.buf + 20, 5);
        c.rpc.Buffer = c.buf + 8; c.rpc.BufferLength = 16;           /* declared range [buf+8, buf+24] */
        c.stub.Buffer = c.buf + 8;
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 7, &o, &e), X_BADBUFPOS);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 25, &o, &e), X_BADBUFPOS);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 24, &o, &e), X_BADSTUB);
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf + 16, &o, &e), 0);
        CHECK(o == (HANDLE)5);
        g2_free(&c);
    }
}
TEST(gap2_handle_unmarshal_bad_signature_disables_bounds)
{
    if (t_is_wine()) { t_skip("emulator does not use NdrGetUserMarshalInfo"); return; }
    /* NdrGetUserMarshalInfo fails with a bad signature: CUserMarshalInfo then reports an unbounded buffer */
    FOR_VARS(v) {
        G2Ctx c; HANDLE o = NULL; unsigned char *e;
        g2_init(&c, 16, C_LOCAL, USER_MARSHAL_CB_UNMARSHALL);
        g2_wr32(c.buf, G2_TAG_H); g2_wr32(c.buf + 4, 6);
        g2_reread(&c, 0, C_LOCAL);
        c.umcb.Signature = 0;
        XEQ(g2_x_buf(v->unm, G2F(&c), c.buf, &o, &e), 0);
        CHECK(o == (HANDLE)6); CHECK(e == c.buf + 8);
        g2_free(&c);
    }
}

/* ---- Free ---- */
TEST(gap2_handle_free_is_noop)
{
    FOR_VARS(v) {
        ULONG ctx;
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            G2Ctx c; HANDLE h = (HANDLE)0x1234;
            g2_init(&c, 0, ctx, USER_MARSHAL_CB_FREE);
            XEQ(g2_x_free(v->fre, G2F(&c), &h), 0);
            CHECKF(h == (HANDLE)0x1234, "%s %s: Free must not modify the handle", v->fam, v->nfree);
            h = NULL;
            XEQ(g2_x_free(v->fre, G2F(&c), &h), 0);
            CHECK(h == NULL);
        }
    }
}
TEST(gap2_handle_free_ignores_its_arguments)
{
    /* the body is a bare 'ret' (folded with the compiler's empty delete): NULL flags and NULL object are not dereferenced */
    FOR_VARS(v) {
        XEQ(g2_x_free(v->fre, NULL, NULL), 0);
    }
}

/* ---- cross checks ---- */
TEST(gap2_handle_families_share_wire_format)
{
    /* marshal with one family, unmarshal with any other: same tag, same layout */
    build();
    {
        hvar *a, *b;
        for (a = vars; a < vars + nvars; a++) {
            if (!complete(a)) continue;
            for (b = vars; b < vars + nvars; b++) {
                G2Ctx c; HANDLE h = (HANDLE)0x00abcdef, o = NULL; unsigned char *e1, *e2;
                if (!complete(b)) continue;
                g2_init(&c, 32, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
                XEQ(g2_x_buf(a->mar, G2F(&c), c.buf + 2, &h, &e1), 0);
                g2_reread(&c, (ULONG)(e1 - c.buf), C_LOCAL);
                XEQ(g2_x_buf(b->unm, G2F(&c), c.buf + 2, &o, &e2), 0);
                CHECKF(o == h && e1 == e2, "%s%s/%s -> %s%s/%s", a->fam, a->w64 ? "64" : "", a->mod, b->fam, b->w64 ? "64" : "", b->mod);
                g2_free(&c);
            }
        }
    }
}
TEST(gap2_handle_size_equals_marshal_footprint)
{
    FOR_VARS(v) {
        ULONG off;
        for (off = 0; off < 9; off++) {
            G2Ctx z, c; HANDLE h = (HANDLE)0x8; ULONG sz; unsigned char *e;
            g2_init(&z, 0, C_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            XEQ(g2_x_size(v->size, G2F(&z), off, &h, &sz), 0);
            g2_init(&c, sz, C_LOCAL, USER_MARSHAL_CB_MARSHALL);
            XEQ(g2_x_buf(v->mar, G2F(&c), c.buf + off, &h, &e), 0);
            CHECKF((ULONG)(e - c.buf) == sz, "%s %s off %lu size %lu wrote to %ld", v->fam, v->nsize, off, sz, (long)(e - c.buf));
            CHECK(g2_canary_ok(&c));
            g2_free(&c);
        }
    }
}
