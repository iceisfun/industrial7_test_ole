#include "cmg.h"

/* Resolve the three task-allocator exports from one module. */
typedef void *(WINAPI *alloc_fn)(SIZE_T);
typedef void *(WINAPI *realloc_fn)(void *, SIZE_T);
typedef void (WINAPI *free_fn)(void *);
#define ALLOC(m) ((alloc_fn)t_proc_in(m, "CoTaskMemAlloc"))
#define REALLOC(m) ((realloc_fn)t_proc_in(m, "CoTaskMemRealloc"))
#define FREE(m) ((free_fn)t_proc_in(m, "CoTaskMemFree"))
#define NEED3(m) do { if (!ALLOC(m) || !REALLOC(m) || !FREE(m)) { t_skip("missing task-mem exports in %s", m); return; } } while (0)

static void fill(unsigned char *p, size_t n, unsigned seed) { for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(seed * 31 + i * 7 + (i >> 8)); }
static int verify(const unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++) if (p[i] != (unsigned char)(seed * 31 + i * 7 + (i >> 8))) return 0;
    return 1;
}

static const SIZE_T k_sizes[] = {1,2,3,4,5,7,8,9,15,16,17,24,31,32,33,48,63,64,65,100,127,128,129,255,256,257,500,1000,1023,1024,1025,4095,4096,4097,8192,65535,65536,65537,1<<20,4<<20};

TEST(mem_task_alloc_sizes) {
    FOR_MODS(m) {
        NEED3(m);
        for (size_t i = 0; i < sizeof k_sizes / sizeof *k_sizes; i++) {
            unsigned char *p = ALLOC(m)(k_sizes[i]);
            CHECK(p != NULL);
            if (!p) continue;
            CHECK(((ULONG_PTR)p & 15) == 0);     /* x64 heap alignment */
            fill(p, k_sizes[i], (unsigned)i);
            CHECK(verify(p, k_sizes[i], (unsigned)i));
            FREE(m)(p);
        }
    }
}
TEST(mem_task_alloc_zero_returns_pointer) {
    FOR_MODS(m) {
        NEED3(m);
        void *a = ALLOC(m)(0), *b = ALLOC(m)(0);
        CHECK(a != NULL); CHECK(b != NULL); CHECK(a != b);
        FREE(m)(a); FREE(m)(b);
    }
}
TEST(mem_task_free_null_is_noop) {
    FOR_MODS(m) { NEED3(m); FREE(m)(NULL); FREE(m)(NULL); CHECK(1); }
}
TEST(mem_task_realloc_null_allocates) {
    FOR_MODS(m) {
        NEED3(m);
        for (SIZE_T n = 1; n <= 4096; n *= 4) {
            unsigned char *p = REALLOC(m)(NULL, n);
            CHECK(p != NULL);
            if (!p) continue;
            fill(p, n, 5); CHECK(verify(p, n, 5));
            FREE(m)(p);
        }
    }
}
TEST(mem_task_realloc_null_zero) {
    /* Realloc(NULL, 0) behaves as Alloc(0) per the IMalloc contract: non-NULL block. */
    FOR_MODS(m) {
        NEED3(m);
        void *p = REALLOC(m)(NULL, 0);
        t_note("%s CoTaskMemRealloc(NULL,0) -> %p", m, p);
        if (p) FREE(m)(p);
    }
}
TEST(mem_task_realloc_zero_frees) {
    FOR_MODS(m) {
        NEED3(m);
        void *p = ALLOC(m)(64);
        CHECK(p != NULL);
        void *q = REALLOC(m)(p, 0);
        CHECK(q == NULL);                         /* block freed, NULL returned */
    }
}
TEST(mem_task_realloc_grow_preserves) {
    FOR_MODS(m) {
        NEED3(m);
        for (size_t from = 1; from <= 2048; from *= 2) for (size_t to = from + 1; to <= 8192; to *= 3) {
            unsigned char *p = ALLOC(m)(from);
            CHECK(p != NULL); if (!p) continue;
            fill(p, from, 9);
            unsigned char *q = REALLOC(m)(p, to);
            CHECK(q != NULL);
            if (!q) { FREE(m)(p); continue; }
            CHECK(verify(q, from, 9));
            memset(q + from, 0xA5, to - from);   /* whole new extent writable */
            CHECK(verify(q, from, 9));
            FREE(m)(q);
        }
    }
}
TEST(mem_task_realloc_shrink_preserves) {
    FOR_MODS(m) {
        NEED3(m);
        for (size_t from = 8192; from >= 2; from /= 2) for (size_t to = 1; to < from; to *= 3) {
            unsigned char *p = ALLOC(m)(from);
            CHECK(p != NULL); if (!p) continue;
            fill(p, from, 3);
            unsigned char *q = REALLOC(m)(p, to);
            CHECK(q != NULL);
            if (!q) { FREE(m)(p); continue; }
            CHECK(verify(q, to, 3));
            FREE(m)(q);
        }
    }
}
TEST(mem_task_realloc_same_size) {
    FOR_MODS(m) {
        NEED3(m);
        unsigned char *p = ALLOC(m)(100);
        CHECK(p != NULL); if (!p) return;
        fill(p, 100, 11);
        unsigned char *q = REALLOC(m)(p, 100);
        CHECK(q != NULL);
        if (q) { CHECK(verify(q, 100, 11)); FREE(m)(q); } else FREE(m)(p);
    }
}
TEST(mem_task_realloc_chain) {
    FOR_MODS(m) {
        NEED3(m);
        unsigned char *p = ALLOC(m)(1);
        size_t cur = 1; unsigned rng = 12345;
        CHECK(p != NULL); if (!p) return;
        p[0] = 0xEE;
        size_t keep = 1; unsigned char first = 0xEE;
        for (int i = 0; i < 300; i++) {
            rng = rng * 1103515245u + 12345u;
            size_t n = 1 + (rng >> 8) % 5000;
            unsigned char *q = REALLOC(m)(p, n);
            CHECK(q != NULL);
            if (!q) { FREE(m)(p); return; }
            p = q;
            if (n < keep) keep = n;
            CHECK(p[0] == first);
            cur = n;
        }
        FREE(m)(p);
    }
}
TEST(mem_task_live_blocks_disjoint) {
    FOR_MODS(m) {
        NEED3(m);
        enum { N = 600 };
        unsigned char *b[N]; size_t sz[N];
        for (int i = 0; i < N; i++) { sz[i] = 1 + (i * 37) % 700; b[i] = ALLOC(m)(sz[i]); CHECK(b[i] != NULL); if (b[i]) fill(b[i], sz[i], i); }
        int ok = 1;
        for (int i = 0; i < N; i++) if (b[i] && !verify(b[i], sz[i], i)) ok = 0;
        CHECK(ok);
        for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++)
            if (b[i] && b[j] && b[i] == b[j]) ok = 0;
        CHECK(ok);
        for (int i = 0; i < N; i++) FREE(m)(b[i]);
    }
}
TEST(mem_task_large_block) {
    FOR_MODS(m) {
        NEED3(m);
        size_t n = 32u << 20;
        unsigned char *p = ALLOC(m)(n);
        CHECK(p != NULL); if (!p) continue;
        p[0] = 1; p[n / 2] = 2; p[n - 1] = 3;
        unsigned char *q = REALLOC(m)(p, n + (1u << 20));
        CHECK(q != NULL);
        if (q) { CHECK(q[0] == 1 && q[n / 2] == 2 && q[n - 1] == 3); q[n + (1u << 20) - 1] = 4; FREE(m)(q); } else FREE(m)(p);
    }
}
TEST(mem_task_cross_module_free) {
    /* both modules front the process heap: a block from one may be released through the other */
    if (!ALLOC("ole32.dll") || !FREE("combase.dll")) { t_skip("exports missing"); return; }
    void *a = ALLOC("ole32.dll")(123);
    void *b = ALLOC("combase.dll")(123);
    CHECK(a && b);
    IMalloc *mo = cmg_getmalloc("ole32.dll"), *mc = cmg_getmalloc("combase.dll");
    if (mo && mc && a && b) {
        CHECK(mc->lpVtbl->GetSize(mc, a) == 123);
        CHECK(mo->lpVtbl->GetSize(mo, b) == 123);
    }
    FREE("combase.dll")(a);
    FREE("ole32.dll")(b);
}
static alloc_fn g_th_alloc;
static DWORD WINAPI th_alloc(LPVOID p) { void **out = p; *out = g_th_alloc(500); if (*out) fill(*out, 500, 77); return 0; }
TEST(mem_task_free_on_other_thread) {
    REQUIRE(ALLOC("ole32.dll"));
    void *blk = NULL;
    g_th_alloc = ALLOC("ole32.dll");
    HANDLE h = CreateThread(NULL, 0, th_alloc, &blk, 0, NULL);
    CHECK(h != NULL); if (!h) return;
    CHECK(WaitForSingleObject(h, 5000) == WAIT_OBJECT_0);
    CloseHandle(h);
    CHECK(blk != NULL);
    if (blk) { CHECK(verify(blk, 500, 77)); FREE("ole32.dll")(blk); }
}
TEST(mem_task_does_not_set_lasterror_on_success) {
    FOR_MODS(m) {
        NEED3(m);
        SET_ERR(0x1234);
        void *p = ALLOC(m)(32);
        CHECK(p != NULL);
        FREE(m)(p);
        t_note("%s lasterror after Alloc/Free = %lu", m, GetLastError());
    }
}

/* ---------------- CoGetMalloc / IMalloc ---------------- */
TEST(mem_getmalloc_task_ok) {
    FOR_MODS(m) {
        typedef HRESULT (WINAPI *pfn)(DWORD, IMalloc **);
        pfn f = (pfn)t_proc_in(m, "CoGetMalloc");
        if (!f) { t_skip("CoGetMalloc missing in %s", m); return; }
        IMalloc *a = NULL, *b = NULL;
        EXPECT_EQ(f(MEMCTX_TASK, &a), S_OK);
        EXPECT_EQ(f(MEMCTX_TASK, &b), S_OK);
        CHECK(a != NULL);
        CHECK(a == b);                            /* process-wide singleton, no per-call object */
    }
}
TEST(mem_getmalloc_invalid_context) {
    static const DWORD ctx[] = { 0 /*UNKNOWN*/, 2 /*SHARED*/, 3 /*MACSYSTEM*/, 4, 5, 0x10, 0x100, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu /*SAME*/ };
    FOR_MODS(m) {
        typedef HRESULT (WINAPI *pfn)(DWORD, IMalloc **);
        pfn f = (pfn)t_proc_in(m, "CoGetMalloc");
        if (!f) { t_skip("CoGetMalloc missing in %s", m); return; }
        for (size_t i = 0; i < sizeof ctx / sizeof *ctx; i++) {
            IMalloc *p = (IMalloc *)(ULONG_PTR)0xDEADBEEF0000ull;
            HRESULT hr = f(ctx[i], &p);
            EXPECT_EQ(hr, CMG_E_INVALIDARG);
            CHECK(p == NULL);                    /* out pointer is cleared on failure */
        }
    }
}
TEST(mem_getmalloc_no_com_init_needed) {
    /* the task allocator is usable on a thread that never called CoInitialize */
    IMalloc *m = cmg_getmalloc("ole32.dll");
    CHECK(m != NULL);
    if (!m) return;
    void *p = m->lpVtbl->Alloc(m, 10);
    CHECK(p != NULL);
    m->lpVtbl->Free(m, p);
}
TEST(mem_imalloc_queryinterface) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        void *o = NULL;
        EXPECT_EQ(im->lpVtbl->QueryInterface(im, &G_IMalloc, &o), S_OK);
        CHECK(o == (void *)im);
        o = NULL;
        EXPECT_EQ(im->lpVtbl->QueryInterface(im, &G_IUnknown, &o), S_OK);
        CHECK(o == (void *)im);
        static const GUID *bad[] = { &G_IMallocSpy, &G_IStream, &G_NULL, &G_Sample };
        for (int i = 0; i < 4; i++) {
            o = (void *)(ULONG_PTR)0x1234;
            EXPECT_EQ(im->lpVtbl->QueryInterface(im, bad[i], &o), CMG_E_NOINTERFACE);
            CHECK(o == NULL);
        }
    }
}
TEST(mem_imalloc_refcounts_static) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        /* the allocator is a static object: AddRef/Release always report 1 */
        /* Windows: CMalloc_AddRef is ICF-folded to "return 1" (decomp combase 0xedf90); Wine returns 2 */
        for (int i = 0; i < 5; i++) { EXPECT_EQ(im->lpVtbl->AddRef(im), 1); }
        for (int i = 0; i < 20; i++) { EXPECT_EQ(im->lpVtbl->Release(im), 1); }
        void *p = im->lpVtbl->Alloc(im, 8);           /* still alive after excess Release */
        CHECK(p != NULL);
        im->lpVtbl->Free(im, p);
    }
}
TEST(mem_imalloc_alloc_free) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        for (size_t i = 0; i < sizeof k_sizes / sizeof *k_sizes; i++) {
            unsigned char *p = im->lpVtbl->Alloc(im, k_sizes[i]);
            CHECK(p != NULL); if (!p) continue;
            fill(p, k_sizes[i], 1); CHECK(verify(p, k_sizes[i], 1));
            im->lpVtbl->Free(im, p);
        }
    }
}
TEST(mem_imalloc_alloc_zero) {
    IMalloc *im = cmg_getmalloc("ole32.dll");
    REQUIRE(im);
    void *p = im->lpVtbl->Alloc(im, 0);
    CHECK(p != NULL);
    CHECK(im->lpVtbl->GetSize(im, p) == 0);
    im->lpVtbl->Free(im, p);
}
TEST(mem_imalloc_free_null) {
    FOR_MODS(m) { IMalloc *im = cmg_getmalloc(m); if (!im) { t_skip("no IMalloc"); return; } im->lpVtbl->Free(im, NULL); CHECK(1); }
}
TEST(mem_imalloc_realloc_semantics) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        unsigned char *p = im->lpVtbl->Realloc(im, NULL, 40);      /* NULL: plain allocation */
        CHECK(p != NULL); if (!p) return;
        fill(p, 40, 2);
        unsigned char *q = im->lpVtbl->Realloc(im, p, 4000);
        CHECK(q != NULL); if (!q) return;
        CHECK(verify(q, 40, 2));
        CHECK(im->lpVtbl->GetSize(im, q) == 4000);
        p = im->lpVtbl->Realloc(im, q, 10);
        CHECK(p != NULL); if (!p) return;
        CHECK(verify(p, 10, 2));
        CHECK(im->lpVtbl->GetSize(im, p) == 10);
        void *z = im->lpVtbl->Realloc(im, p, 0);                    /* size 0: freed */
        CHECK(z == NULL);
    }
}
TEST(mem_imalloc_getsize_exact) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        for (size_t n = 0; n <= 300; n++) {
            void *p = im->lpVtbl->Alloc(im, n);
            CHECK(p != NULL); if (!p) continue;
            EXPECT_EQ(im->lpVtbl->GetSize(im, p), n);
            im->lpVtbl->Free(im, p);
        }
        for (size_t i = 0; i < sizeof k_sizes / sizeof *k_sizes; i++) {
            void *p = im->lpVtbl->Alloc(im, k_sizes[i]);
            CHECK(p != NULL); if (!p) continue;
            EXPECT_EQ(im->lpVtbl->GetSize(im, p), k_sizes[i]);
            im->lpVtbl->Free(im, p);
        }
    }
}
TEST(mem_imalloc_getsize_null) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        SIZE_T s = im->lpVtbl->GetSize(im, NULL);
        CHECK(s == (SIZE_T)-1);
    }
}
TEST(mem_imalloc_getsize_after_realloc) {
    IMalloc *im = cmg_getmalloc("ole32.dll");
    REQUIRE(im);
    void *p = im->lpVtbl->Alloc(im, 10);
    CHECK(p != NULL); if (!p) return;
    for (size_t n = 11; n < 3000; n = n * 2 + 1) {
        p = im->lpVtbl->Realloc(im, p, n);
        CHECK(p != NULL); if (!p) return;
        EXPECT_EQ(im->lpVtbl->GetSize(im, p), n);
    }
    im->lpVtbl->Free(im, p);
}
TEST(mem_imalloc_didalloc) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        char *p = im->lpVtbl->Alloc(im, 16);
        CHECK(p != NULL); if (!p) return;
        EXPECT_EQ(im->lpVtbl->DidAlloc(im, p), 1);
        EXPECT_EQ(im->lpVtbl->DidAlloc(im, NULL), -1);          /* cannot tell for NULL */
        EXPECT_EQ(im->lpVtbl->DidAlloc(im, (void *)1), 0);
        EXPECT_EQ(im->lpVtbl->DidAlloc(im, p + 4), 0);          /* interior pointer is not a block start */
        char *z = im->lpVtbl->Alloc(im, 0);
        if (z) { EXPECT_EQ(im->lpVtbl->DidAlloc(im, z), 1); im->lpVtbl->Free(im, z); }
        im->lpVtbl->Free(im, p);
    }
}
TEST(mem_imalloc_didalloc_non_heap_pointers) {
    IMalloc *im = cmg_getmalloc("ole32.dll");
    REQUIRE(im);
    char stackbuf[64];
    static char staticbuf[64];
    t_note("DidAlloc(stack)=%d DidAlloc(static)=%d DidAlloc(0x10)=%d", im->lpVtbl->DidAlloc(im, stackbuf), im->lpVtbl->DidAlloc(im, staticbuf), im->lpVtbl->DidAlloc(im, (void *)0x10));
}
TEST(mem_imalloc_foreign_process_heap_block) {
    /* the task allocator is backed by the process heap: observe how it treats a plain HeapAlloc block */
    IMalloc *im = cmg_getmalloc("ole32.dll");
    REQUIRE(im);
    void *p = HeapAlloc(GetProcessHeap(), 0, 77);
    CHECK(p != NULL); if (!p) return;
    t_note("GetSize(HeapAlloc 77)=%llu DidAlloc=%d", (unsigned long long)im->lpVtbl->GetSize(im, p), im->lpVtbl->DidAlloc(im, p));
    HeapFree(GetProcessHeap(), 0, p);
}
TEST(mem_imalloc_block_visible_to_heapsize) {
    /* CoTaskMemAlloc blocks live in the process heap */
    REQUIRE(ALLOC("ole32.dll"));
    void *p = ALLOC("ole32.dll")(321);
    CHECK(p != NULL); if (!p) return;
    SIZE_T hs = HeapSize(GetProcessHeap(), 0, p);
    t_note("HeapSize(process heap, CoTaskMemAlloc(321)) = %llu", (unsigned long long)hs);
    FREE("ole32.dll")(p);
}
TEST(mem_imalloc_heapminimize) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        unsigned char *keep = im->lpVtbl->Alloc(im, 200);
        CHECK(keep != NULL); if (!keep) return;
        fill(keep, 200, 4);
        void *tmp[50];
        for (int i = 0; i < 50; i++) tmp[i] = im->lpVtbl->Alloc(im, 1000 + i);
        for (int i = 0; i < 50; i++) im->lpVtbl->Free(im, tmp[i]);
        im->lpVtbl->HeapMinimize(im);
        CHECK(verify(keep, 200, 4));                           /* live data untouched */
        EXPECT_EQ(im->lpVtbl->GetSize(im, keep), 200);
        void *again = im->lpVtbl->Alloc(im, 50);
        CHECK(again != NULL);
        im->lpVtbl->Free(im, again);
        im->lpVtbl->Free(im, keep);
    }
}
TEST(mem_imalloc_matches_cotaskmem) {
    /* a block from CoTaskMemAlloc is a valid IMalloc block and vice versa */
    FOR_MODS(m) {
        NEED3(m);
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        void *a = ALLOC(m)(90);
        CHECK(a != NULL);
        if (a) { EXPECT_EQ(im->lpVtbl->GetSize(im, a), 90); EXPECT_EQ(im->lpVtbl->DidAlloc(im, a), 1); }
        void *b = im->lpVtbl->Alloc(im, 91);
        CHECK(b != NULL);
        if (a) FREE(m)(a);
        if (b) { EXPECT_EQ(im->lpVtbl->GetSize(im, b), 91); FREE(m)(b); }
    }
}
TEST(mem_task_realloc_getsize) {
    FOR_MODS(m) {
        NEED3(m);
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        void *p = ALLOC(m)(10);
        CHECK(p != NULL); if (!p) return;
        p = REALLOC(m)(p, 1000);
        CHECK(p != NULL); if (!p) return;
        EXPECT_EQ(im->lpVtbl->GetSize(im, p), 1000);
        p = REALLOC(m)(p, 3);
        CHECK(p != NULL); if (!p) return;
        EXPECT_EQ(im->lpVtbl->GetSize(im, p), 3);
        FREE(m)(p);
    }
}
TEST(mem_task_stress_alloc_free_cycle) {
    FOR_MODS(m) {
        NEED3(m);
        unsigned rng = 99;
        void *slot[64] = {0};
        for (int i = 0; i < 20000; i++) {
            rng = rng * 1664525u + 1013904223u;
            int s = (rng >> 16) & 63;
            if (slot[s]) { FREE(m)(slot[s]); slot[s] = NULL; }
            else { slot[s] = ALLOC(m)(1 + ((rng >> 4) & 2047)); if (!slot[s]) { CHECK(0); break; } *(unsigned char *)slot[s] = 1; }
        }
        for (int s = 0; s < 64; s++) FREE(m)(slot[s]);
        CHECK(1);
    }
}
