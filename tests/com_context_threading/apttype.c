/* CoGetApartmentType, CoGetContextToken, CoGetCurrentLogicalThreadId, CoGetDefaultContext. */
#include "cx.h"

#define APT_SENT ((APTTYPE)0x5a5a)
#define QUAL_SENT ((APTTYPEQUALIFIER)0x5a5a)

typedef struct AptRes { HRESULT hr; APTTYPE type; APTTYPEQUALIFIER qual; } AptRes;
static void apt_query(AptRes *r)
{
    r->type = APT_SENT; r->qual = QUAL_SENT;
    r->hr = U(CoGetApartmentType)(&r->type, &r->qual);
}

/* ---- argument validation ---- */
static void w_apt_null_args(void *p)
{
    APTTYPE t = APT_SENT; APTTYPEQUALIFIER q = QUAL_SENT;
    CXEQ_HR(U(CoGetApartmentType)(NULL, &q), E_INVALIDARG);
    CXEQ(q, QUAL_SENT);
    CXEQ_HR(U(CoGetApartmentType)(&t, NULL), E_INVALIDARG);
    CXEQ(t, APT_SENT);
    CXEQ_HR(U(CoGetApartmentType)(NULL, NULL), E_INVALIDARG);
}
/* adapted from Wine dlls/combase/tests/roapi.c (LGPL-2.1+) */
TEST(ctxthr_apt_null_args)
{
    REQUIRE(U(CoGetApartmentType));
    cx_run(CXM_NONE, w_apt_null_args, NULL);   /* argument check precedes any state check */
    cx_run(CXM_STA, w_apt_null_args, NULL);
    cx_run(CXM_MTA, w_apt_null_args, NULL);
}

/* ---- uninitialized thread ---- */
static void w_apt_uninit(void *p)
{
    AptRes r; apt_query(&r);
    CXEQ_HR(r.hr, CX_HR_NOTINIT);
    CXEQ(r.type, APTTYPE_CURRENT);
    CXEQ(r.qual, APTTYPEQUALIFIER_NONE);
}
TEST(ctxthr_apt_uninitialized_thread)
{
    REQUIRE(U(CoGetApartmentType));
    cx_run(CXM_NONE, w_apt_uninit, NULL);
}

/* ---- explicit STA / MTA ---- */
static void w_apt_sta(void *p)
{
    AptRes r; apt_query(&r);
    CXEQ_HR(r.hr, S_OK);
    CHECKF(cx_is_sta_type(r.type), "type %d", (int)r.type);
    CXEQ(r.qual, APTTYPEQUALIFIER_NONE);
    if (r.type == APTTYPE_MAINSTA) t_note("first STA of this process thread reports APTTYPE_MAINSTA");
}
static void w_apt_mta(void *p)
{
    AptRes r; apt_query(&r);
    CXEQ_HR(r.hr, S_OK);
    CXEQ(r.type, APTTYPE_MTA);
    CXEQ(r.qual, APTTYPEQUALIFIER_NONE);
}
TEST(ctxthr_apt_sta_thread)  { REQUIRE(U(CoGetApartmentType)); cx_run(CXM_STA, w_apt_sta, NULL); }
TEST(ctxthr_apt_mta_thread)  { REQUIRE(U(CoGetApartmentType)); cx_run(CXM_MTA, w_apt_mta, NULL); }

/* ---- implicit MTA: an uninitialized thread joins the MTA while any thread has it initialized ---- */
static void w_apt_implicit(void *p)
{
    AptRes r; apt_query(&r);
    CXEQ_HR(r.hr, S_OK);
    CXEQ(r.type, APTTYPE_MTA);
    CXEQ(r.qual, APTTYPEQUALIFIER_IMPLICIT_MTA);
}
TEST(ctxthr_apt_implicit_mta)
{
    CxPark mta;
    REQUIRE(U(CoGetApartmentType));
    if (!cx_park_start(&mta, CXM_MTA)) { cx_park_stop(&mta); CHECK(!"park MTA"); return; }
    cx_run(CXM_NONE, w_apt_implicit, NULL);
    cx_park_stop(&mta);
    /* MTA gone again: the same kind of thread is uninitialized */
    cx_run(CXM_NONE, w_apt_uninit, NULL);
}
/* a thread that initialized and then uninitialized falls back to the implicit MTA while another thread holds the MTA */
static void w_apt_after_uninit(void *p)
{
    AptRes r;
    CXEQ_HR(cx_init(CXM_MTA), S_OK);
    cx_uninit();
    apt_query(&r);
    CXEQ_HR(r.hr, S_OK);
    CXEQ(r.type, APTTYPE_MTA);
    CXEQ(r.qual, APTTYPEQUALIFIER_IMPLICIT_MTA);
}
TEST(ctxthr_apt_uninit_thread_falls_back_to_implicit_mta)
{
    CxPark mta;
    REQUIRE(U(CoGetApartmentType));
    if (!cx_park_start(&mta, CXM_MTA)) { cx_park_stop(&mta); CHECK(!"park MTA"); return; }
    cx_run(CXM_NONE, w_apt_after_uninit, NULL);
    cx_park_stop(&mta);
}
/* An STA thread is not affected by the MTA existing elsewhere. */
TEST(ctxthr_apt_sta_unaffected_by_mta)
{
    CxPark mta;
    REQUIRE(U(CoGetApartmentType));
    if (!cx_park_start(&mta, CXM_MTA)) { cx_park_stop(&mta); CHECK(!"park MTA"); return; }
    cx_run(CXM_STA, w_apt_sta, NULL);
    cx_park_stop(&mta);
}
/* The MTA stays alive while any thread holds it. */
TEST(ctxthr_apt_mta_refcounted_across_threads)
{
    CxPark a, b;
    REQUIRE(U(CoGetApartmentType));
    if (!cx_park_start(&a, CXM_MTA)) { cx_park_stop(&a); CHECK(!"park a"); return; }
    if (!cx_park_start(&b, CXM_MTA)) { cx_park_stop(&a); cx_park_stop(&b); CHECK(!"park b"); return; }
    cx_park_stop(&a);                              /* b still holds the MTA */
    cx_run(CXM_NONE, w_apt_implicit, NULL);
    cx_park_stop(&b);
    cx_run(CXM_NONE, w_apt_uninit, NULL);
}

/* ---- nested initialization on one thread ---- */
static void w_apt_nested(void *p)
{
    AptRes r;
    HRESULT hr = cx_init(CXM_STA);
    CXEQ_HR(hr, S_OK);
    hr = cx_init(CXM_STA);
    CXEQ_HR(hr, S_FALSE);                          /* second init of same model: S_FALSE */
    hr = cx_init(CXM_MTA);
    CXEQ_HR(hr, CX_HR_CHANGEDMODE);                /* conflicting model: RPC_E_CHANGED_MODE, no refcount taken */
    apt_query(&r);
    CXEQ_HR(r.hr, S_OK); CHECK(cx_is_sta_type(r.type));
    cx_uninit();                                   /* balances the S_FALSE init */
    apt_query(&r);
    CXEQ_HR(r.hr, S_OK); CHECK(cx_is_sta_type(r.type));
    cx_uninit();                                   /* balances the first init */
    apt_query(&r);
    CXEQ_HR(r.hr, CX_HR_NOTINIT);
    CXEQ(r.type, APTTYPE_CURRENT);
}
TEST(ctxthr_apt_nested_init_refcount)
{
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx));
    cx_run(CXM_NONE, w_apt_nested, NULL);
}
static void w_apt_nested_mta(void *p)
{
    AptRes r;
    CXEQ_HR(cx_init(CXM_MTA), S_OK);
    CXEQ_HR(cx_init(CXM_MTA), S_FALSE);
    CXEQ_HR(cx_init(CXM_STA), CX_HR_CHANGEDMODE);
    apt_query(&r); CXEQ_HR(r.hr, S_OK); CXEQ(r.type, APTTYPE_MTA); CXEQ(r.qual, APTTYPEQUALIFIER_NONE);
    cx_uninit(); cx_uninit();
    apt_query(&r); CXEQ_HR(r.hr, CX_HR_NOTINIT);
}
TEST(ctxthr_apt_nested_init_refcount_mta)
{
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx));
    cx_run(CXM_NONE, w_apt_nested_mta, NULL);
}

/* ---- repeated calls are stable ---- */
static void w_apt_stable(void *p)
{
    AptRes a, b;
    apt_query(&a); apt_query(&b);
    CXEQ_HR(a.hr, b.hr); CXEQ(a.type, b.type); CXEQ(a.qual, b.qual);
}
TEST(ctxthr_apt_repeated_calls_stable)
{
    REQUIRE(U(CoGetApartmentType));
    cx_run(CXM_STA, w_apt_stable, NULL);
    cx_run(CXM_MTA, w_apt_stable, NULL);
}

/* ---- fresh process: MAINSTA identity ---- */
static DWORD WINAPI wk_sta(LPVOID p)
{
    AptRes r;
    CXEQ_HR(cx_init(CXM_STA), S_OK);
    apt_query(&r);
    CXEQ_HR(r.hr, S_OK);
    CKEQ(r.type, APTTYPE_STA);                     /* main STA thread is alive: this is a plain STA */
    CKEQ(r.qual, APTTYPEQUALIFIER_NONE);
    cx_uninit();
    return 0;
}
static DWORD WINAPI wk_mta(LPVOID p)
{
    AptRes r;
    CXEQ_HR(cx_init(CXM_MTA), S_OK);
    apt_query(&r);
    CKEQ_HR(r.hr, S_OK); CKEQ(r.type, APTTYPE_MTA); CKEQ(r.qual, APTTYPEQUALIFIER_NONE);
    cx_uninit();
    return 0;
}
static void join_thread(LPTHREAD_START_ROUTINE f)
{
    HANDLE h = CreateThread(NULL, 0, f, NULL, 0, NULL);
    if (!h) { g_cx_cf++; return; }
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) g_cx_cf++;
    CloseHandle(h);
}
CHILD(ctxthr_apt_main_sta)
{
    AptRes r;
    g_cx_cf = 0;
    apt_query(&r);
    CKEQ_HR(r.hr, CX_HR_NOTINIT); CKEQ(r.type, APTTYPE_CURRENT);
    CKEQ_HR(cx_init(CXM_STA), S_OK);
    apt_query(&r);
    CKEQ_HR(r.hr, S_OK); CKEQ(r.type, APTTYPE_MAINSTA); CKEQ(r.qual, APTTYPEQUALIFIER_NONE);
    join_thread(wk_sta);
    join_thread(wk_mta);
    apt_query(&r);
    CKEQ(r.type, APTTYPE_MAINSTA);                 /* unaffected by the other threads */
    cx_uninit();
    apt_query(&r);
    CKEQ_HR(r.hr, CX_HR_NOTINIT); CKEQ(r.type, APTTYPE_CURRENT);
    return g_cx_cf;
}
TEST(ctxthr_apt_first_sta_is_main_sta)
{
    REQUIRE(U(CoGetApartmentType));
    CXEQ(t_run_child("ctxthr_apt_main_sta", "", 30000), 0);
}

/* ================= CoGetContextToken ================= */
static void w_tok_null(void *p) { CXEQ_HR(U(CoGetContextToken)(NULL), CX_HR_POINTER); }
TEST(ctxthr_token_null_pointer)
{
    REQUIRE(U(CoGetContextToken));
    cx_run(CXM_NONE, w_tok_null, NULL);            /* NULL is rejected before the initialization state is looked at */
    cx_run(CXM_STA, w_tok_null, NULL);
    cx_run(CXM_MTA, w_tok_null, NULL);
}
static void w_tok_uninit(void *p)
{
    ULONG_PTR tok = 0x1234;
    CXEQ_HR(U(CoGetContextToken)(&tok), CX_HR_NOTINIT);
    CXEQ(tok, 0x1234);                        /* output untouched on failure */
}
TEST(ctxthr_token_uninitialized)
{
    REQUIRE(U(CoGetContextToken));
    cx_run(CXM_NONE, w_tok_uninit, NULL);
}
static void w_tok_basic(void *p)
{
    ULONG_PTR a = 0, b = 0;
    CXEQ_HR(U(CoGetContextToken)(&a), S_OK);
    CXEQ_HR(U(CoGetContextToken)(&b), S_OK);
    CXNE(a, 0);
    CXEQ(a, b);
    CXEQ(a & 7, 0);                           /* a token is an object pointer: pointer-aligned */
}
TEST(ctxthr_token_stable_nonzero_sta) { REQUIRE(U(CoGetContextToken)); cx_run(CXM_STA, w_tok_basic, NULL); }
TEST(ctxthr_token_stable_nonzero_mta) { REQUIRE(U(CoGetContextToken)); cx_run(CXM_MTA, w_tok_basic, NULL); }

TEST(ctxthr_token_distinct_between_live_stas)
{
    CxPark a, b;
    REQUIRE(U(CoGetContextToken));
    if (!cx_park_start(&a, CXM_STA) || !cx_park_start(&b, CXM_STA)) { CHECK(!"park"); cx_park_stop(&a); cx_park_stop(&b); return; }
    CXEQ_HR(a.tok_hr, S_OK); CXEQ_HR(b.tok_hr, S_OK);
    CXNE(a.token, 0); CXNE(b.token, 0);
    CXNE(a.token, b.token);
    cx_park_stop(&a); cx_park_stop(&b);
}
TEST(ctxthr_token_shared_between_mta_threads)
{
    CxPark a, b;
    REQUIRE(U(CoGetContextToken));
    if (!cx_park_start(&a, CXM_MTA) || !cx_park_start(&b, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&a); cx_park_stop(&b); return; }
    CXEQ_HR(a.tok_hr, S_OK); CXEQ_HR(b.tok_hr, S_OK);
    CXNE(a.token, 0);
    CXEQ(a.token, b.token);                   /* one MTA, one default context */
    cx_park_stop(&a); cx_park_stop(&b);
}
TEST(ctxthr_token_sta_vs_mta_differ)
{
    CxPark a, b;
    REQUIRE(U(CoGetContextToken));
    if (!cx_park_start(&a, CXM_STA) || !cx_park_start(&b, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&a); cx_park_stop(&b); return; }
    CXNE(a.token, b.token);
    cx_park_stop(&a); cx_park_stop(&b);
}
static void w_tok_implicit(void *p)
{
    ULONG_PTR *expect = p, tok = 0;
    CXEQ_HR(U(CoGetContextToken)(&tok), S_OK);    /* an implicit MTA thread has a context: the MTA default context */
    CXEQ(tok, *expect);
}
TEST(ctxthr_token_implicit_mta_matches_mta)
{
    CxPark m;
    REQUIRE(U(CoGetContextToken));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    cx_run(CXM_NONE, w_tok_implicit, &m.token);
    cx_park_stop(&m);
}
static void w_tok_uninit_after_mta(void *p)
{
    ULONG_PTR tok = 0;
    CXEQ_HR(U(CoGetContextToken)(&tok), CX_HR_NOTINIT);
}
TEST(ctxthr_token_uninitialized_again_after_mta_gone)
{
    CxPark m;
    REQUIRE(U(CoGetContextToken));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    cx_park_stop(&m);
    cx_run(CXM_NONE, w_tok_uninit_after_mta, NULL);
}

/* ================= CoGetCurrentLogicalThreadId ================= */
static void w_lid_null(void *p) { CXEQ_HR(U(CoGetCurrentLogicalThreadId)(NULL), E_INVALIDARG); }
TEST(ctxthr_logical_thread_id_null)
{
    REQUIRE(U(CoGetCurrentLogicalThreadId));
    cx_run(CXM_STA, w_lid_null, NULL);
    cx_run(CXM_MTA, w_lid_null, NULL);
    cx_run(CXM_NONE, w_lid_null, NULL);
}
static void w_lid_basic(void *p)
{
    GUID a, b;
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&a), S_OK);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&b), S_OK);
    CHECK(IsEqualGUID(&a, &b));                    /* stable for the life of the thread */
    CHECK(!IsEqualGUID(&a, &CX_GUID_NULL));           /* generated on first use */
}
TEST(ctxthr_logical_thread_id_stable_sta) { REQUIRE(U(CoGetCurrentLogicalThreadId)); cx_run(CXM_STA, w_lid_basic, NULL); }
TEST(ctxthr_logical_thread_id_stable_mta) { REQUIRE(U(CoGetCurrentLogicalThreadId)); cx_run(CXM_MTA, w_lid_basic, NULL); }
static void w_lid_nocom(void *p)
{
    GUID a; HRESULT hr = U(CoGetCurrentLogicalThreadId)(&a);
    t_note("CoGetCurrentLogicalThreadId on a thread that never called CoInitialize: 0x%08lx", (unsigned long)hr);
}
TEST(ctxthr_logical_thread_id_without_com_init_observed) { REQUIRE(U(CoGetCurrentLogicalThreadId)); cx_run(CXM_NONE, w_lid_nocom, NULL); }

TEST(ctxthr_logical_thread_id_unique_per_thread)
{
    CxPark t[4]; int i, j, up = 0;
    REQUIRE(U(CoGetCurrentLogicalThreadId));
    for (i = 0; i < 4; i++) up += cx_park_start(&t[i], i & 1 ? CXM_MTA : CXM_STA);
    CHECK(up == 4);
    for (i = 0; i < 4; i++) {
        CXEQ_HR(t[i].lid_hr, S_OK);
        for (j = i + 1; j < 4; j++) CHECK(!IsEqualGUID(&t[i].lid, &t[j].lid));
    }
    for (i = 0; i < 4; i++) cx_park_stop(&t[i]);
}
static void w_lid_survives_uninit(void *p)
{
    GUID a, b;
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&a), S_OK);
    cx_uninit();                                   /* balanced by cx_run's own uninit: init again first */
    cx_init(CXM_STA);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&b), S_OK);
    t_note("logical thread id across uninit/init on the same thread: %s", IsEqualGUID(&a, &b) ? "preserved" : "regenerated");
}
TEST(ctxthr_logical_thread_id_across_reinit_observed) { REQUIRE(U(CoGetCurrentLogicalThreadId)); cx_run(CXM_STA, w_lid_survives_uninit, NULL); }

/* ================= CoGetDefaultContext ================= */
static void w_dc_sta_type(void *p)
{
    void *pv = (void *)0xdead;
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_STA, &CX_IID_IObjContext, &pv), E_INVALIDARG);
    CXEQ(pv, 0);                              /* output is cleared first */
}
TEST(ctxthr_defctx_apttype_sta_invalid)
{
    REQUIRE(U(CoGetDefaultContext));
    cx_run(CXM_NONE, w_dc_sta_type, NULL);         /* rejected before the apartment is consulted */
    cx_run(CXM_STA, w_dc_sta_type, NULL);
    cx_run(CXM_MTA, w_dc_sta_type, NULL);
}
static void w_dc_bad_type(void *p)
{
    static const int bad[] = { 4, 5, 99, -2, 0x7fffffff };
    int i;
    for (i = 0; i < 5; i++) {
        void *pv = (void *)0xdead;
        HRESULT hr = U(CoGetDefaultContext)((APTTYPE)bad[i], &CX_IID_IObjContext, &pv);
        CXEQ_HR(hr, E_INVALIDARG);
        CXEQ(pv, 0);
    }
}
TEST(ctxthr_defctx_unknown_apttype_invalid)
{
    REQUIRE(U(CoGetDefaultContext));
    cx_run(CXM_STA, w_dc_bad_type, NULL);
    cx_run(CXM_MTA, w_dc_bad_type, NULL);
}
static void w_dc_current_sta(void *p)
{
    void *dc = NULL, *unk = NULL; ULONG_PTR tok = 0;
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_CURRENT, &CX_IID_IObjContext, &dc), S_OK);
    CXEQ_HR(U(CoGetContextToken)(&tok), S_OK);
    CXNE(dc, 0);
    CXEQ((ULONG_PTR)dc, tok);                 /* a plain STA thread runs in its apartment's default context */
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_CURRENT, &CX_IID_IUnknown, &unk), S_OK);
    CXEQ((ULONG_PTR)unk, tok);                /* COM identity: IUnknown of the context object is the token */
    if (dc) IUnknown_Release((IUnknown *)dc);
    if (unk) IUnknown_Release((IUnknown *)unk);
}
TEST(ctxthr_defctx_current_equals_token_sta) { REQUIRE(U(CoGetDefaultContext)); REQUIRE(U(CoGetContextToken)); cx_run(CXM_STA, w_dc_current_sta, NULL); }
static void w_dc_current_mta(void *p)
{
    void *dc = NULL, *dc2 = NULL; ULONG_PTR tok = 0;
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_CURRENT, &CX_IID_IObjContext, &dc), S_OK);
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_MTA, &CX_IID_IObjContext, &dc2), S_OK);
    CXEQ_HR(U(CoGetContextToken)(&tok), S_OK);
    CXEQ((ULONG_PTR)dc, tok);
    CXEQ((ULONG_PTR)dc2, tok);
    if (dc) IUnknown_Release((IUnknown *)dc);
    if (dc2) IUnknown_Release((IUnknown *)dc2);
}
TEST(ctxthr_defctx_current_and_mta_equal_token_mta) { REQUIRE(U(CoGetDefaultContext)); REQUIRE(U(CoGetContextToken)); cx_run(CXM_MTA, w_dc_current_mta, NULL); }

static void w_dc_na(void *p)
{
    void *na = NULL, *na2 = NULL, *mta = NULL; ULONG_PTR tok = 0;
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IObjContext, &na), S_OK);
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IObjContext, &na2), S_OK);
    CXNE(na, 0);
    CXEQ(na, na2);                            /* one NTA default context per process */
    U(CoGetContextToken)(&tok);
    CXNE((ULONG_PTR)na, tok);
    if (U(CoGetDefaultContext)(APTTYPE_MTA, &CX_IID_IObjContext, &mta) == S_OK) {
        CXNE((ULONG_PTR)na, (ULONG_PTR)mta);
        IUnknown_Release((IUnknown *)mta);
    }
    if (na) IUnknown_Release((IUnknown *)na);
    if (na2) IUnknown_Release((IUnknown *)na2);
}
TEST(ctxthr_defctx_na_context_is_shared_and_distinct) { REQUIRE(U(CoGetDefaultContext)); cx_run(CXM_STA, w_dc_na, NULL); cx_run(CXM_MTA, w_dc_na, NULL); }

static void w_dc_bad_iid(void *p)
{
    void *pv = (void *)0xdead;
    static const APTTYPE t[] = { APTTYPE_CURRENT, APTTYPE_MTA, APTTYPE_NA };
    int i;
    for (i = 0; i < 3; i++) {
        pv = (void *)0xdead;
        CXEQ_HR(U(CoGetDefaultContext)(t[i], &CX_IID_Bogus, &pv), E_NOINTERFACE);
        CXEQ(pv, 0);
    }
}
TEST(ctxthr_defctx_unsupported_iid_e_nointerface) { REQUIRE(U(CoGetDefaultContext)); cx_run(CXM_STA, w_dc_bad_iid, NULL); cx_run(CXM_MTA, w_dc_bad_iid, NULL); }

static void w_dc_ifaces(void *p)
{
    static const GUID *ok[] = { &CX_IID_IUnknown, &CX_IID_IContext, &CX_IID_IObjContext, &CX_IID_IContextCallback, &CX_IID_IComThreadingInfo, &CX_IID_IMarshal };
    int i;
    for (i = 0; i < 6; i++) {
        void *pv = NULL;
        HRESULT hr = U(CoGetDefaultContext)(APTTYPE_CURRENT, ok[i], &pv);
        CHECKF(hr == S_OK && pv, "iid index %d hr 0x%08lx", i, (unsigned long)hr);
        if (pv) IUnknown_Release((IUnknown *)pv);
    }
}
TEST(ctxthr_defctx_supported_interfaces) { REQUIRE(U(CoGetDefaultContext)); cx_run(CXM_STA, w_dc_ifaces, NULL); cx_run(CXM_MTA, w_dc_ifaces, NULL); }

static void w_dc_mta_from_sta(void *p)
{
    void *pv = NULL; HRESULT hr = U(CoGetDefaultContext)(APTTYPE_MTA, &CX_IID_IObjContext, &pv);
    CXEQ_HR(hr, S_OK);
    if (pv) IUnknown_Release((IUnknown *)pv);
}
TEST(ctxthr_defctx_mta_reachable_from_sta)
{
    CxPark m;
    REQUIRE(U(CoGetDefaultContext));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    cx_run(CXM_STA, w_dc_mta_from_sta, NULL);
    cx_park_stop(&m);
}
static void w_dc_mta_no_mta(void *p)
{
    void *pv = (void *)0xdead; HRESULT hr = U(CoGetDefaultContext)(APTTYPE_MTA, &CX_IID_IObjContext, &pv);
    t_note("CoGetDefaultContext(APTTYPE_MTA) from an STA thread with no MTA in the process: 0x%08lx", (unsigned long)hr);
    if (CX_FAILED(hr)) CXEQ(pv, 0);
    if (!CX_FAILED(hr) && pv) IUnknown_Release((IUnknown *)pv);
}
TEST(ctxthr_defctx_mta_without_mta_observed) { REQUIRE(U(CoGetDefaultContext)); cx_run(CXM_STA, w_dc_mta_no_mta, NULL); }

static void w_dc_uninit_na(void *p)
{
    void *pv = (void *)0xdead; HRESULT hr = U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IObjContext, &pv);
    t_note("CoGetDefaultContext(APTTYPE_NA) on an uninitialized thread: 0x%08lx", (unsigned long)hr);
    if (CX_FAILED(hr)) CXEQ(pv, 0);
    else if (pv) IUnknown_Release((IUnknown *)pv);
}
TEST(ctxthr_defctx_uninitialized_observed) { REQUIRE(U(CoGetDefaultContext)); cx_run(CXM_NONE, w_dc_uninit_na, NULL); }

/* Child: MAINSTA default context needs a main STA to exist. */
static DWORD WINAPI wk_dc_mainsta(LPVOID p)
{
    void *pv = NULL; ULONG_PTR *main_tok = p;
    CKEQ_HR(cx_init(CXM_MTA), S_OK);
    CKEQ_HR(U(CoGetDefaultContext)(APTTYPE_MAINSTA, &CX_IID_IObjContext, &pv), S_OK);
    CKEQ((ULONG_PTR)pv, *main_tok);
    if (pv) IUnknown_Release((IUnknown *)pv);
    cx_uninit();
    return 0;
}
static DWORD WINAPI wk_dc_mta_only(LPVOID p)
{
    void *pv = (void *)0xdead;
    CKEQ_HR(cx_init(CXM_MTA), S_OK);               /* only an MTA exists: there is no main STA */
    CKEQ_HR(U(CoGetDefaultContext)(APTTYPE_MAINSTA, &CX_IID_IObjContext, &pv), CX_HR_NOTINIT);
    CKEQ(pv, 0);
    cx_uninit();
    return 0;
}
CHILD(ctxthr_defctx_mainsta)
{
    void *pv = NULL; ULONG_PTR tok = 0; HANDLE h;
    g_cx_cf = 0;
    join_thread(wk_dc_mta_only);
    CKEQ_HR(cx_init(CXM_STA), S_OK);               /* this thread becomes the main STA */
    CKEQ_HR(U(CoGetContextToken)(&tok), S_OK);
    h = CreateThread(NULL, 0, wk_dc_mainsta, &tok, 0, NULL);
    if (!h || WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) g_cx_cf++;
    if (h) CloseHandle(h);
    CKEQ_HR(U(CoGetDefaultContext)(APTTYPE_MAINSTA, &CX_IID_IObjContext, &pv), S_OK);
    CKEQ((ULONG_PTR)pv, tok);
    if (pv) IUnknown_Release((IUnknown *)pv);
    cx_uninit();
    return g_cx_cf;
}
TEST(ctxthr_defctx_mainsta_requires_main_sta)
{
    REQUIRE(U(CoGetDefaultContext));
    CXEQ(t_run_child("ctxthr_defctx_mainsta", "", 30000), 0);
}
