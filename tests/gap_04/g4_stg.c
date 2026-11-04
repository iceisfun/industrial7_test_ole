/* gap_04: STGMEDIUM_UserFree, STGMEDIUM_UserFree64, STGMEDIUM_UserUnmarshal64 (ole32 exports, implemented in combase).
 *
 * UserFree/UserFree64 (Win10 combase): NULL medium -> no-op. Otherwise by tymed:
 *   0           ReleaseStgMedium (releases pUnkForRelease if set)
 *   HGLOBAL/FILE-less handle kinds (1, 0x10, 0x20, 0x40): ReleaseStgMedium unless the marshal context is MSHCTX_INPROC (3), in
 *               which case the handle is left alone and only pUnkForRelease is released
 *   FILE (2)    lpszFileName freed with CoTaskMemFree, pUnkForRelease released
 *   STREAM/STORAGE (4/8) ReleaseStgMedium (releases the interface, or pUnkForRelease when set)
 *   anything else: RpcRaiseException(E_INVALIDARG = 0x80070057)
 * UserUnmarshal64 wire (8-byte aligned): DWORD tymed + pad, [QWORD u-marker if tymed != 0], QWORD pUnk marker, payload. */
#include "g4_um.h"

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)
#define TY_HGLOBAL 1
#define TY_FILE 2
#define TY_ISTREAM 4
#define TY_ISTORAGE 8
#define TY_GDI 16
#define TY_MFPICT 32
#define TY_ENHMF 64

static void *ffn(int w64) { return UM_FNIN("ole32.dll", w64 ? "STGMEDIUM_UserFree64" : "STGMEDIUM_UserFree"); }

/* ---- a counting IUnknown ---------------------------------------------------------------------------------------- */
typedef struct { const IUnknownVtbl *vt; volatile LONG refs, nrel; } mock;
static HRESULT STDMETHODCALLTYPE m_qi(IUnknown *p, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE m_ar(IUnknown *p) { return InterlockedIncrement(&((mock *)p)->refs); }
static ULONG STDMETHODCALLTYPE m_rel(IUnknown *p) { InterlockedIncrement(&((mock *)p)->nrel); return InterlockedDecrement(&((mock *)p)->refs); }
static const IUnknownVtbl mvt = { m_qi, m_ar, m_rel };
static void mock_init(mock *m, LONG refs) { m->vt = &mvt; m->refs = refs; m->nrel = 0; }

TEST(gap4_stgmedium_userfree_null_medium) {
    int w;
    for (w = 0; w < 2; w++) {
        void *f = ffn(w); UMC c;
        if (!f) { t_note("missing export: STGMEDIUM_UserFree%s", w ? "64" : ""); continue; }
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), NULL), 0);
    }
}
TEST(gap4_stgmedium_userfree_tymed_null_releases_punk) {
    int w; ULONG ctx;
    for (w = 0; w < 2; w++) for (ctx = C_LOCAL; ctx <= C_CONT; ctx++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; mock k;
        if (!f) continue;
        mock_init(&k, 2);
        memset(&m, 0, sizeof m); m.tymed = 0; m.pUnkForRelease = (IUnknown *)&k;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(f, UMF(&c), &m), 0);
        CHECKF(k.nrel == 1 && k.refs == 1, "w=%d ctx=%lu: Release called %ld times, refs %ld", w, ctx, (long)k.nrel, (long)k.refs);
    }
}
TEST(gap4_stgmedium_userfree_hglobal_freed_unless_inproc) {
    int w; ULONG ctx;
    for (w = 0; w < 2; w++) for (ctx = C_LOCAL; ctx <= C_CONT; ctx++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; HGLOBAL g; int inproc = ctx == C_INPROC;
        if (!f) continue;
        g = GlobalAlloc(GMEM_MOVEABLE, 64);
        memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(f, UMF(&c), &m), 0);
        SetLastError(0);
        if (inproc) { CHECKF(GlobalSize(g) == 64, "w=%d: INPROC context must not free the handle", w); GlobalFree(g); }
        else CHECKF(GlobalSize(g) == 0, "w=%d ctx=%lu: handle not freed", w, ctx);
    }
}
TEST(gap4_stgmedium_userfree_hglobal_with_punk) {
    /* with pUnkForRelease set, ReleaseStgMedium releases the unknown and leaves the handle to its owner */
    int w;
    for (w = 0; w < 2; w++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; HGLOBAL g; mock k;
        if (!f) continue;
        g = GlobalAlloc(GMEM_MOVEABLE, 64); mock_init(&k, 2);
        memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g; m.pUnkForRelease = (IUnknown *)&k;
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), &m), 0);
        CHECK(k.nrel == 1);
        SetLastError(0);
        CHECKF(GlobalSize(g) == 64, "w=%d: handle owned by pUnkForRelease must survive", w);
        GlobalFree(g);
    }
}
TEST(gap4_stgmedium_userfree_stream_releases_interface) {
    int w; ULONG ctx; static const ULONG tys[] = { TY_ISTREAM, TY_ISTORAGE };
    for (w = 0; w < 2; w++) for (ctx = C_LOCAL; ctx <= C_CONT; ctx++) {
        unsigned t;
        for (t = 0; t < 2; t++) {
            void *f = ffn(w); UMC c; STGMEDIUM m; mock k;
            if (!f) continue;
            mock_init(&k, 2);
            memset(&m, 0, sizeof m); m.tymed = tys[t]; m.pstm = (IStream *)&k;
            um_init(&c, NULL, 0, ctx);
            XEQ(um_x_free(f, UMF(&c), &m), 0);
            CHECKF(k.nrel == 1, "w=%d ctx=%lu tymed=%lu: Release x%ld", w, ctx, tys[t], (long)k.nrel);
        }
    }
}
TEST(gap4_stgmedium_userfree_stream_with_punk_releases_stream_and_punk) {
    int w;
    for (w = 0; w < 2; w++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; mock s, k;
        if (!f) continue;
        mock_init(&s, 2); mock_init(&k, 2);
        memset(&m, 0, sizeof m); m.tymed = TY_ISTREAM; m.pstm = (IStream *)&s; m.pUnkForRelease = (IUnknown *)&k;
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), &m), 0);
        /* Win10: STGMEDIUM_UserFree on an ISTREAM medium goes through ReleaseStgMedium and releases the stream as well as pUnkForRelease. */
        CHECKF(k.nrel == 1 && s.nrel == 1, "w=%d: punk %ld stream %ld", w, (long)k.nrel, (long)s.nrel);
    }
}
TEST(gap4_stgmedium_userfree_file_name_freed) {
    int w; ULONG ctx;
    REQUIRE(U(CoTaskMemAlloc));
    for (w = 0; w < 2; w++) for (ctx = C_LOCAL; ctx <= C_CONT; ctx++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; mock k; WCHAR *nm;
        if (!f) continue;
        nm = U(CoTaskMemAlloc)(32); lstrcpyW(nm, L"c:\\olet\\x.tmp"); mock_init(&k, 2);
        memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = nm; m.pUnkForRelease = (IUnknown *)&k;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(f, UMF(&c), &m), 0);                  /* the name block is released with CoTaskMemFree: no crash, no leak check possible */
        CHECKF(k.nrel == 1, "w=%d ctx=%lu: pUnkForRelease Release x%ld", w, ctx, (long)k.nrel);
    }
}
TEST(gap4_stgmedium_userfree_gdi_deleted_unless_inproc) {
    int w; ULONG ctx;
    for (w = 0; w < 2; w++) for (ctx = C_LOCAL; ctx <= C_CONT; ctx++) {
        void *f = ffn(w); UMC c; STGMEDIUM m; HBITMAP bm; int inproc = ctx == C_INPROC;
        if (!f) continue;
        bm = UIN("gdi32.dll", CreateBitmap)(4, 4, 1, 1, NULL); CHECK(bm != NULL); if (!bm) continue;
        memset(&m, 0, sizeof m); m.tymed = TY_GDI; m.hBitmap = bm;
        um_init(&c, NULL, 0, ctx);
        XEQ(um_x_free(f, UMF(&c), &m), 0);
        /* Win10: the bitmap is deleted for every context except INPROC; a second DeleteObject therefore fails (GetObjectType keeps
         * reporting OBJ_BITMAP for the stale handle, so it cannot be used to tell). */
        { BOOL del = UIN("gdi32.dll", DeleteObject)(bm);
          if (inproc) CHECKF(del, "w=%d: INPROC must not delete", w);
          else CHECKF(!del, "w=%d ctx=%lu: bitmap not deleted", w, ctx); }
    }
}
TEST(gap4_stgmedium_userfree_invalid_tymed) {
    static const DWORD bad[] = { 3, 5, 9, 12, 0x80, 0x100, 0x1000, 0xFFFFFFFF };
    int w; unsigned k;
    for (w = 0; w < 2; w++) for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        void *f = ffn(w); UMC c; STGMEDIUM m;
        if (!f) continue;
        memset(&m, 0, sizeof m); m.tymed = bad[k];
        um_init(&c, NULL, 0, C_LOCAL);
        XEQ(um_x_free(f, UMF(&c), &m), 0x80070057);          /* E_INVALIDARG raised through RpcRaiseException */
    }
}

/* ---- STGMEDIUM_UserUnmarshal64 -------------------------------------------------------------------------------------- */
static void *sfn64(const char *op) { char n[64]; snprintf(n, sizeof n, "STGMEDIUM_User%s64", op); return UM_FNIN("ole32.dll", n); }

TEST(gap4_stgmedium_unmarshal64_tymed_null) {
    void *fu = sfn64("Unmarshal"); int off;
    REQUIRE(fu);
    for (off = 0; off < 8; off += 3) {
        unsigned char *b = um_buf_new(64), *e; UMC c; STGMEDIUM o; unsigned a8 = UM_ALIGN((unsigned)off, 8);
        memset(b, 0, 64);
        memset(&o, 0xAB, sizeof o);
        *(DWORD *)(b + a8) = 0;                              /* tymed NULL */
        um_init(&c, b, 64, C_LOCAL);
        XEQ(um_x_mar(fu, UMF(&c), b + off, &o, &e), 0);
        CHECKF(e == b + a8 + 16, "end %+ld want %u", (long)(e - b), a8 + 16);
        /* Win10: the output is zeroed only when the marshal context says so; with this plain context a NULL-tymed medium leaves
         * pUnkForRelease as the caller had it (the 0xAB pattern) and only clears the union. */
        CHECKF(o.tymed == 0 && o.hGlobal == NULL, "tymed %lu u %p", o.tymed, o.hGlobal);
        CHECKF(o.pUnkForRelease == (IUnknown *)(ULONG_PTR)0xabababababababab, "pUnkForRelease %p (untouched expected)", o.pUnkForRelease);
        um_buf_free(b);
    }
}
static unsigned char *marshal64(UMC *c, unsigned char *b, ULONG cap, STGMEDIUM *m, ULONG ctx, unsigned char **end) {
    void *fm = sfn64("Marshal"); void *fs = sfn64("Size"); ULONG r;
    um_init(c, NULL, 0, ctx);
    if (um_x_size(fs, UMF(c), 0, m, &r)) return NULL;
    um_init(c, b, cap, ctx);
    if (um_x_mar(fm, UMF(c), b, m, end)) return NULL;
    return b;
}
TEST(gap4_stgmedium_unmarshal64_roundtrip_hglobal) {
    void *fu = sfn64("Unmarshal"), *ff = sfn64("Free"); static const ULONG sizes[] = { 1, 8, 100, 4000 };
    unsigned k; ULONG ctx;
    REQUIRE(fu); REQUIRE(sfn64("Marshal")); REQUIRE(sfn64("Size")); REQUIRE(ff);
    for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2)
        for (k = 0; k < sizeof sizes / sizeof *sizes; k++) {
            HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, sizes[k]); unsigned char *p, *b = um_buf_new(sizes[k] + 256), *e1 = NULL, *e2 = NULL; UMC c; STGMEDIUM m, o;
            p = GlobalLock(g); memset(p, 0x5a, sizes[k]); p[0] = 0x11; p[sizes[k] - 1] = 0x22; GlobalUnlock(g);
            memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
            memset(&o, 0, sizeof o);
            CHECK(marshal64(&c, b, sizes[k] + 256, &m, ctx, &e1) != NULL);
            if (!e1) { um_buf_free(b); GlobalFree(g); continue; }
            um_init(&c, b, (ULONG)(e1 - b), ctx);
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e2), 0);
            CHECK(e2 == e1);
            CHECKF(o.tymed == TY_HGLOBAL && o.hGlobal != NULL && o.hGlobal != g && o.pUnkForRelease == NULL, "size %lu ctx %lu", sizes[k], ctx);
            if (o.hGlobal) {
                CHECK(GlobalSize(o.hGlobal) >= sizes[k]);
                p = GlobalLock(o.hGlobal);
                /* the source fill is 0x5a with p[0]=0x11 then p[n-1]=0x22: for a 1-byte block the last write wins */
                CHECKF(p && p[0] == (sizes[k] == 1 ? 0x22 : 0x11) && p[sizes[k] - 1] == 0x22 && (sizes[k] < 3 || p[sizes[k] / 2] == 0x5a), "size %lu", sizes[k]);
                GlobalUnlock(o.hGlobal);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_free(ff, UMF(&c), &o), 0);
            }
            GlobalFree(g); um_buf_free(b);
        }
}
TEST(gap4_stgmedium_unmarshal64_roundtrip_file_name) {
    void *fu = sfn64("Unmarshal"), *ff = sfn64("Free"); static const WCHAR *names[] = { L"a", L"C:\\olet\\file.txt", L"\x00e9\x20ac name" };
    unsigned k;
    REQUIRE(fu); REQUIRE(sfn64("Marshal")); REQUIRE(sfn64("Size")); REQUIRE(ff); REQUIRE(U(CoTaskMemAlloc));
    for (k = 0; k < 3; k++) {
        WCHAR *nm = U(CoTaskMemAlloc)((lstrlenW(names[k]) + 1) * 2); unsigned char *b = um_buf_new(512), *e1 = NULL, *e2 = NULL; UMC c; STGMEDIUM m, o;
        lstrcpyW(nm, names[k]);
        memset(&m, 0, sizeof m); m.tymed = TY_FILE; m.lpszFileName = nm; memset(&o, 0, sizeof o);
        CHECK(marshal64(&c, b, 512, &m, C_LOCAL, &e1) != NULL);
        if (e1) {
            um_init(&c, b, (ULONG)(e1 - b), C_LOCAL);
            XEQ(um_x_mar(fu, UMF(&c), b, &o, &e2), 0);
            CHECK(e1 == e2);
            CHECKF(o.tymed == TY_FILE && o.lpszFileName && o.lpszFileName != nm && !lstrcmpW(o.lpszFileName, names[k]), "name round trip %u", k);
            um_init(&c, NULL, 0, C_LOCAL);
            XEQ(um_x_free(ff, UMF(&c), &o), 0);
        }
        U(CoTaskMemFree)(nm); um_buf_free(b);
    }
}
TEST(gap4_stgmedium_unmarshal64_truncated_buffer) {
    void *fu = sfn64("Unmarshal"); unsigned char *b = um_buf_new(256), *e1 = NULL; UMC c; STGMEDIUM m; HGLOBAL g; ULONG n, len; int bad = 0;
    REQUIRE(fu); REQUIRE(sfn64("Marshal")); REQUIRE(sfn64("Size"));
    g = GlobalAlloc(GMEM_MOVEABLE, 40);
    memset(&m, 0, sizeof m); m.tymed = TY_HGLOBAL; m.hGlobal = g;
    CHECK(marshal64(&c, b, 256, &m, C_LOCAL, &e1) != NULL);
    n = e1 ? (ULONG)(e1 - b) : 0;
    for (len = 0; len < n; len += 4) {                       /* every shorter buffer must be rejected, never over-read */
        STGMEDIUM o; unsigned char *e = NULL; DWORD x; unsigned char *tb = um_buf_new(len ? len : 1);
        if (len) memcpy(tb, b, len);
        memset(&o, 0, sizeof o);
        um_init(&c, tb, len, C_LOCAL);
        x = um_x_mar(fu, UMF(&c), tb, &o, &e);
        if (x == 0) { bad++; t_note("buffer length %lu of %lu accepted", len, n); if (o.tymed == TY_HGLOBAL && o.hGlobal) GlobalFree(o.hGlobal); }
        um_buf_free(tb);
    }
    EQ(bad, 0);
    GlobalFree(g); um_buf_free(b);
}
TEST(gap4_stgmedium_unmarshal64_invalid_tymed) {
    void *fu = sfn64("Unmarshal"); static const DWORD bad[] = { 3, 0x80, 0x100, 0xFFFFFFFF }; unsigned k;
    REQUIRE(fu);
    for (k = 0; k < sizeof bad / sizeof *bad; k++) {
        unsigned char *b = um_buf_new(64); UMC c; STGMEDIUM o; unsigned char *e = NULL; DWORD x;
        memset(b, 0, 64); *(DWORD *)b = bad[k]; *(ULONGLONG *)(b + 8) = 0x1000; *(ULONGLONG *)(b + 16) = 0;
        memset(&o, 0, sizeof o);
        um_init(&c, b, 64, C_LOCAL);
        x = um_x_mar(fu, UMF(&c), b, &o, &e);
        CHECKF(x != 0, "tymed 0x%lx accepted", bad[k]);
        t_note("Unmarshal64 with tymed 0x%lx -> exception 0x%lx", bad[k], x);
        um_buf_free(b);
    }
}
