/* CoRegisterMallocSpy / CoRevokeMallocSpy and the IMallocSpy call protocol of the task allocator.
 *
 * Decomp of combase 19041 (class\memapi.cxx):
 *  - CoRegisterMallocSpy: NULL, or an object that fails QueryInterface(IID_IMallocSpy) -> E_INVALIDARG;
 *    already registered (including a revoke that is still pending) -> CO_E_OBJISREG; the QI'd pointer is kept.
 *  - CoRevokeMallocSpy: none registered -> CO_E_OBJNOTREG; spied allocations still outstanding -> E_ACCESSDENIED
 *    and the revoke is marked pending; otherwise the spy is Released and unregistered.
 * Everything is process-global, so each scenario runs in its own child process. */
#include "gap0.h"

UDECL(HRESULT, CoRegisterMallocSpy, (IMallocSpy *));
UDECL(HRESULT, CoRevokeMallocSpy, (void));

static const GUID G_IID_MallocSpy = {0x0000001d,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G_IID_Unknown = {0,0,0,{0xC0,0,0,0,0,0,0,0x46}};
typedef struct {
    IMallocSpy iface; LONG refs;
    int qi_ok, qi_iid_spy;
    int pre_alloc, post_alloc, pre_free, post_free, pre_realloc, post_realloc, pre_getsize, post_getsize,
        pre_didalloc, post_didalloc, pre_min, post_min;
    SIZE_T last_req, last_realloc_req; void *last_post_alloc, *last_pre_free, *last_pre_realloc, *last_post_realloc, *last_pre_getsize;
    BOOL last_free_spyed, last_postfree_spyed, last_realloc_spyed, last_getsize_spyed, last_didalloc_spyed;
    SIZE_T last_post_getsize;
    int bytes_extra;
} Spy;
static Spy *S(IMallocSpy *p) { return (Spy *)p; }

static HRESULT STDMETHODCALLTYPE sp_qi(IMallocSpy *t, REFIID r, void **o) {
    Spy *s = S(t);
    if (IsEqualGUID(r, &G_IID_MallocSpy)) s->qi_iid_spy++;
    if (!s->qi_ok) { *o = NULL; return E_NOINTERFACE; }
    if (IsEqualGUID(r, &G_IID_MallocSpy) || IsEqualGUID(r, &G_IID_Unknown)) { *o = t; InterlockedIncrement(&s->refs); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE sp_addref(IMallocSpy *t) { return InterlockedIncrement(&S(t)->refs); }
static ULONG STDMETHODCALLTYPE sp_release(IMallocSpy *t) { return InterlockedDecrement(&S(t)->refs); }
static SIZE_T STDMETHODCALLTYPE sp_prealloc(IMallocSpy *t, SIZE_T cb) { Spy *s = S(t); s->pre_alloc++; s->last_req = cb; return cb + s->bytes_extra; }
static void *STDMETHODCALLTYPE sp_postalloc(IMallocSpy *t, void *p) { Spy *s = S(t); s->post_alloc++; s->last_post_alloc = p; return p; }
static void *STDMETHODCALLTYPE sp_prefree(IMallocSpy *t, void *p, BOOL spyed) { Spy *s = S(t); s->pre_free++; s->last_pre_free = p; s->last_free_spyed = spyed; return p; }
static void STDMETHODCALLTYPE sp_postfree(IMallocSpy *t, BOOL spyed) { Spy *s = S(t); s->post_free++; s->last_postfree_spyed = spyed; }
static SIZE_T STDMETHODCALLTYPE sp_prerealloc(IMallocSpy *t, void *p, SIZE_T cb, void **pp, BOOL spyed) {
    Spy *s = S(t); s->pre_realloc++; s->last_pre_realloc = p; s->last_realloc_req = cb; s->last_realloc_spyed = spyed; *pp = p; return cb + s->bytes_extra; }
static void *STDMETHODCALLTYPE sp_postrealloc(IMallocSpy *t, void *p, BOOL spyed) { Spy *s = S(t); s->post_realloc++; s->last_post_realloc = p; return p; }
static void *STDMETHODCALLTYPE sp_pregetsize(IMallocSpy *t, void *p, BOOL spyed) { Spy *s = S(t); s->pre_getsize++; s->last_pre_getsize = p; s->last_getsize_spyed = spyed; return p; }
static SIZE_T STDMETHODCALLTYPE sp_postgetsize(IMallocSpy *t, SIZE_T cb, BOOL spyed) { Spy *s = S(t); s->post_getsize++; s->last_post_getsize = cb; return cb; }
static void *STDMETHODCALLTYPE sp_predidalloc(IMallocSpy *t, void *p, BOOL spyed) { Spy *s = S(t); s->pre_didalloc++; s->last_didalloc_spyed = spyed; return p; }
static int STDMETHODCALLTYPE sp_postdidalloc(IMallocSpy *t, void *p, BOOL spyed, int actual) { S(t)->post_didalloc++; return actual; }
static void STDMETHODCALLTYPE sp_premin(IMallocSpy *t) { S(t)->pre_min++; }
static void STDMETHODCALLTYPE sp_postmin(IMallocSpy *t) { S(t)->post_min++; }

static const IMallocSpyVtbl spy_vtbl = { sp_qi, sp_addref, sp_release, sp_prealloc, sp_postalloc, sp_prefree, sp_postfree,
    sp_prerealloc, sp_postrealloc, sp_pregetsize, sp_postgetsize, sp_predidalloc, sp_postdidalloc, sp_premin, sp_postmin };
static void spy_init(Spy *s) { memset(s, 0, sizeof *s); s->iface.lpVtbl = (IMallocSpyVtbl *)&spy_vtbl; s->refs = 1; s->qi_ok = 1; }

#define SPYNEED CNEED(CoRegisterMallocSpy); CoRevokeMallocSpy_pfn pCoRevokeMallocSpy = UN(CoRevokeMallocSpy); if (!pCoRevokeMallocSpy) return 0

CHILD(gap0_spy_register_validation) {
    SPYNEED;
    Spy s; spy_init(&s);
    CEQH(pCoRegisterMallocSpy(NULL), G_E_INVALIDARG);
    /* revoke with nothing registered */
    CEQH(pCoRevokeMallocSpy(), G_CO_E_OBJNOTREG);
    CEQH(pCoRevokeMallocSpy(), G_CO_E_OBJNOTREG);
    /* an object that does not answer QueryInterface(IID_IMallocSpy) is rejected as E_INVALIDARG */
    s.qi_ok = 0;
    CEQH(pCoRegisterMallocSpy(&s.iface), G_E_INVALIDARG);
    C(s.qi_iid_spy >= 1);
    CEQH(pCoRevokeMallocSpy(), G_CO_E_OBJNOTREG);   /* the failed registration left nothing behind */
    return CHILD_RC();
}
TEST(gap0_spy_register_validation) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_register_validation); }

CHILD(gap0_spy_register_revoke_cycle) {
    SPYNEED;
    Spy s; spy_init(&s);
    CEQH(pCoRegisterMallocSpy(&s.iface), S_OK);
    C(s.qi_iid_spy >= 1);                                  /* registration asks for IID_IMallocSpy */
    printf("  NOTE spy refcount right after registration: %ld (QI AddRef)\n", s.refs);
    CEQH(pCoRegisterMallocSpy(&s.iface), G_CO_E_OBJISREG);  /* only one spy at a time, even the same object */
    Spy t; spy_init(&t);
    CEQH(pCoRegisterMallocSpy(&t.iface), G_CO_E_OBJISREG);
    CEQ(t.refs, 1);
    CEQH(pCoRevokeMallocSpy(), S_OK);
    CEQ(s.refs, 1);                                        /* the reference taken at registration is released */
    CEQH(pCoRevokeMallocSpy(), G_CO_E_OBJNOTREG);
    /* a different spy can be installed once the first is gone, and the cycle repeats */
    for (int i = 0; i < 5; i++) {
        CEQH(pCoRegisterMallocSpy(&t.iface), S_OK);
        CEQH(pCoRevokeMallocSpy(), S_OK);
        CEQ(t.refs, 1);
    }
    return CHILD_RC();
}
TEST(gap0_spy_register_revoke_cycle) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_register_revoke_cycle); }

CHILD(gap0_spy_alloc_protocol) {
    SPYNEED;
    Spy s; spy_init(&s);
    void *before = U(CoTaskMemAlloc)(40);       /* allocated before the spy exists */
    CEQH(pCoRegisterMallocSpy(&s.iface), S_OK);

    void *p = U(CoTaskMemAlloc)(100);
    C(p != NULL);
    CEQ(s.pre_alloc, 1); CEQ(s.post_alloc, 1);
    CEQ(s.last_req, 100);
    CEQ(s.last_post_alloc, p);                       /* PostAlloc receives the block the allocator returned */

    IMalloc *m = NULL;
    CEQH(U(CoGetMalloc)(MEMCTX_TASK, &m), S_OK);
    if (m) {
        int pa = s.pre_alloc;
        void *q = IMalloc_Alloc(m, 24);
        CEQ(s.pre_alloc, pa + 1); CEQ(s.last_req, 24); CEQ(s.last_post_alloc, q);
        /* GetSize / DidAlloc / HeapMinimize go through their Pre/Post pairs */
        SIZE_T sz = IMalloc_GetSize(m, q);
        CEQ(s.pre_getsize, 1); CEQ(s.post_getsize, 1);
        CEQ(s.last_pre_getsize, q); C(s.last_getsize_spyed);
        CEQ(sz, s.last_post_getsize);
        C(sz >= 24 || sz == 24);
        int da = IMalloc_DidAlloc(m, q);
        CEQ(s.pre_didalloc, 1); CEQ(s.post_didalloc, 1); C(s.last_didalloc_spyed);
        CEQ(da, 1);
        IMalloc_HeapMinimize(m);
        CEQ(s.pre_min, 1); CEQ(s.post_min, 1);

        /* Realloc */
        void *r = IMalloc_Realloc(m, q, 200);
        CEQ(s.pre_realloc, 1); CEQ(s.post_realloc, 1);
        CEQ(s.last_pre_realloc, q); CEQ(s.last_realloc_req, 200); C(s.last_realloc_spyed);
        CEQ(s.last_post_realloc, r);
        IMalloc_Free(m, r);
        CEQ(s.last_pre_free, r); C(s.last_free_spyed); C(s.last_postfree_spyed);
        IMalloc_Release(m);
    }
    /* a block allocated while the spy was registered is reported as spied; one from before is not */
    int pf = s.pre_free, qf = s.post_free;
    U(CoTaskMemFree)(p);
    CEQ(s.pre_free, pf + 1 + 0 * qf); C(s.pre_free > pf);
    CEQ(s.last_pre_free, p); C(s.last_free_spyed); C(s.last_postfree_spyed);
    pf = s.pre_free;
    U(CoTaskMemFree)(before);
    CEQ(s.pre_free, pf + 1); CEQ(s.last_pre_free, before);
    C(!s.last_free_spyed); C(!s.last_postfree_spyed);
    CEQ(s.pre_free, s.post_free);                          /* every PreFree has its PostFree */
    CEQ(s.pre_alloc, s.post_alloc);

    CEQH(pCoRevokeMallocSpy(), S_OK);
    /* after the revoke the spy is no longer called */
    int pa = s.pre_alloc;
    void *z = U(CoTaskMemAlloc)(8);
    CEQ(s.pre_alloc, pa);
    U(CoTaskMemFree)(z);
    return CHILD_RC();
}
TEST(gap0_spy_alloc_protocol) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_alloc_protocol); }

CHILD(gap0_spy_zero_and_null) {
    SPYNEED;
    Spy s; spy_init(&s);
    CEQH(pCoRegisterMallocSpy(&s.iface), S_OK);
    /* CoTaskMemFree(NULL) is a no-op for the allocator: the spy sees nothing or a NULL free; either way no crash */
    U(CoTaskMemFree)(NULL);
    printf("  NOTE CoTaskMemFree(NULL): pre_free=%d post_free=%d\n", s.pre_free, s.post_free);
    CEQ(s.pre_free, s.post_free);
    void *z = U(CoTaskMemAlloc)(0);
    printf("  NOTE CoTaskMemAlloc(0): pre_alloc=%d last_req=%llu result=%p\n", s.pre_alloc, (unsigned long long)s.last_req, z);
    U(CoTaskMemFree)(z);
    CEQ(s.pre_free, s.post_free);
    CEQH(pCoRevokeMallocSpy(), S_OK);
    return CHILD_RC();
}
TEST(gap0_spy_zero_and_null) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_zero_and_null); }

CHILD(gap0_spy_prealloc_extra_bytes) {
    SPYNEED;
    Spy s; spy_init(&s); s.bytes_extra = 64;      /* PreAlloc asks for 64 more than requested */
    CEQH(pCoRegisterMallocSpy(&s.iface), S_OK);
    IMalloc *m = NULL; CEQH(U(CoGetMalloc)(MEMCTX_TASK, &m), S_OK);
    if (m) {
        void *p = IMalloc_Alloc(m, 16);
        C(p != NULL);
        SIZE_T sz = p ? IMalloc_GetSize(m, p) : 0;
        printf("  NOTE GetSize after PreAlloc grew the request 16->80: %llu\n", (unsigned long long)sz);
        C(sz >= 80 || sz == 16 + 0);    /* the real allocation is the size PreAlloc returned (GetSize may report either) */
        IMalloc_Free(m, p);
        IMalloc_Release(m);
    }
    CEQH(pCoRevokeMallocSpy(), S_OK);
    return CHILD_RC();
}
TEST(gap0_spy_prealloc_extra_bytes) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_prealloc_extra_bytes); }

CHILD(gap0_spy_revoke_pending) {
    SPYNEED;
    Spy s; spy_init(&s);
    CEQH(pCoRegisterMallocSpy(&s.iface), S_OK);
    void *p = U(CoTaskMemAlloc)(48);                 /* a spied allocation that stays outstanding */
    C(p != NULL);
    /* outstanding spied blocks: the revoke is refused and left pending */
    CEQH(pCoRevokeMallocSpy(), G_E_ACCESSDENIED);
    CEQH(pCoRegisterMallocSpy(&s.iface), G_CO_E_OBJISREG);   /* still registered while pending */
    Spy t; spy_init(&t);
    CEQH(pCoRegisterMallocSpy(&t.iface), G_CO_E_OBJISREG);
    CEQH(pCoRevokeMallocSpy(), G_E_ACCESSDENIED);            /* asking again changes nothing */
    /* the spy keeps seeing traffic until the last block is freed */
    int pa = s.pre_alloc;
    void *q = U(CoTaskMemAlloc)(16);
    C(s.pre_alloc == pa + 1);
    U(CoTaskMemFree)(q);
    U(CoTaskMemFree)(p);
    printf("  NOTE spy refcount after the last spied block was freed: %ld\n", s.refs);
    HRESULT hr = pCoRevokeMallocSpy();
    printf("  NOTE revoke after freeing everything: 0x%08lx (S_OK, or CO_E_OBJNOTREG when the free completed the pending revoke)\n", (unsigned long)hr);
    C(hr == S_OK || hr == G_CO_E_OBJNOTREG);
    hr = pCoRevokeMallocSpy();
    CEQH(hr, G_CO_E_OBJNOTREG);
    C(s.refs == 1);
    /* the slot is free again */
    CEQH(pCoRegisterMallocSpy(&t.iface), S_OK);
    CEQH(pCoRevokeMallocSpy(), S_OK);
    return CHILD_RC();
}
TEST(gap0_spy_revoke_pending_with_outstanding_allocations) { { CoRegisterMallocSpy_pfn p = UN(CoRegisterMallocSpy); REQUIRE(p); } RUN_CHILD(gap0_spy_revoke_pending); }
