/* CoGetObjectContext, IComThreadingInfo, IContext, IContextCallback, CoCreateObjectInContext, service domains. */
#include "cx.h"

typedef struct CxIContextVtbl {
    HRESULT (WINAPI *QueryInterface)(void *, REFIID, void **);
    ULONG (WINAPI *AddRef)(void *);
    ULONG (WINAPI *Release)(void *);
    HRESULT (WINAPI *SetProperty)(void *, REFGUID, DWORD, IUnknown *);
    HRESULT (WINAPI *RemoveProperty)(void *, REFGUID);
    HRESULT (WINAPI *GetProperty)(void *, REFGUID, DWORD *, IUnknown **);
    HRESULT (WINAPI *EnumContextProps)(void *, void **);
} CxIContextVtbl;
typedef struct CxIContext { const CxIContextVtbl *lpVtbl; } CxIContext;

static ULONG rel(void *p) { return p ? IUnknown_Release((IUnknown *)p) : 0; }

/* ================= CoGetObjectContext ================= */
static void w_goc_uninit(void *p)
{
    void *pv = (void *)0xdead;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, &pv), CX_HR_NOTINIT);
    CXEQ(pv, 0);
    pv = (void *)0xdead;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IObjContext, &pv), CX_HR_NOTINIT);
    CXEQ(pv, 0);
    pv = (void *)0xdead;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_Bogus, &pv), CX_HR_NOTINIT);   /* state is checked before the IID */
    CXEQ(pv, 0);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_objctx_uninitialized)
{
    REQUIRE(U(CoGetObjectContext));
    cx_run(CXM_NONE, w_goc_uninit, NULL);
}

static const GUID *const supported_iids[] = { &CX_IID_IUnknown, &CX_IID_IContext, &CX_IID_IObjContext, &CX_IID_IContextCallback, &CX_IID_IComThreadingInfo, &CX_IID_IMarshal };
static const char *const supported_names[] = { "IUnknown", "IContext", "IObjContext", "IContextCallback", "IComThreadingInfo", "IMarshal" };
static void w_goc_supported(void *p)
{
    int i;
    for (i = 0; i < 6; i++) {
        void *pv = (void *)0xdead;
        HRESULT hr = U(CoGetObjectContext)(supported_iids[i], &pv);
        CHECKF(hr == S_OK && pv != NULL, "%s: hr 0x%08lx pv %p", supported_names[i], (unsigned long)hr, pv);
        rel(pv);
    }
}
TEST(ctxthr_objctx_supported_interfaces_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_goc_supported, NULL); }
TEST(ctxthr_objctx_supported_interfaces_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_goc_supported, NULL); }

static void w_goc_unsupported(void *p)
{
    static const GUID *const bad[] = { &CX_IID_Bogus, &CX_IID_IDispatch, &CX_IID_IClassFactory, &CX_IID_ICancelMethodCalls,
                                       &CX_IID_IStream, &CX_IID_IMessageFilter, &CX_IID_ISynchronize, &CX_IID_IServerSecurity };
    int i;
    for (i = 0; i < 8; i++) {
        void *pv = (void *)0xdead;
        CXEQ_HR(U(CoGetObjectContext)(bad[i], &pv), CX_HR_NOINTERFACE);
        CXEQ(pv, 0);
    }
}
TEST(ctxthr_objctx_unsupported_interfaces_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_goc_unsupported, NULL); }
TEST(ctxthr_objctx_unsupported_interfaces_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_goc_unsupported, NULL); }

/* COM identity: the context object has one IUnknown, and it is the context token. */
static void w_goc_identity(void *p)
{
    void *unk = NULL, *obj = NULL, *ctx = NULL, *cb = NULL, *info = NULL, *q = NULL;
    ULONG_PTR tok = 0;
    CXEQ_HR(U(CoGetContextToken)(&tok), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IUnknown, &unk), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IObjContext, &obj), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContext, &ctx), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, &cb), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, &info), S_OK);
    CXEQ(unk, tok);
    CXEQ(obj, tok);
    CXEQ(ctx, tok);
    /* QI from a secondary interface reaches the same identity */
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)cb, &CX_IID_IUnknown, &q), S_OK);
    CXEQ(q, tok); rel(q); q = NULL;
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)cb, &CX_IID_IObjContext, &q), S_OK);
    CXEQ(q, tok); rel(q); q = NULL;
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)info, &CX_IID_IUnknown, &q), S_OK);
    CXEQ(q, tok); rel(q); q = NULL;
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)info, &CX_IID_IContextCallback, &q), S_OK);
    CXEQ(q, cb); rel(q); q = NULL;                 /* same interface asked from another interface: same pointer */
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)obj, &CX_IID_IComThreadingInfo, &q), S_OK);
    CXEQ(q, info); rel(q); q = NULL;
    CXEQ_HR(IUnknown_QueryInterface((IUnknown *)obj, &CX_IID_Bogus, &q), S_OK + 0 == 0 ? CX_HR_NOINTERFACE : CX_HR_NOINTERFACE);
    CXEQ(q, 0);
    rel(unk); rel(obj); rel(ctx); rel(cb); rel(info);
}
TEST(ctxthr_objctx_identity_sta) { REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); cx_run(CXM_STA, w_goc_identity, NULL); }
TEST(ctxthr_objctx_identity_mta) { REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetContextToken)); cx_run(CXM_MTA, w_goc_identity, NULL); }

/* Repeated gets return the same interface pointer. */
static void w_goc_repeat(void *p)
{
    void *a = NULL, *b = NULL;
    ULONG r;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, &a), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, &b), S_OK);
    CXEQ(a, b);
    r = rel(b);
    t_note("Release of a second IComThreadingInfo reference returned %lu", r);
    r = rel(a);
    CXEQ(r, 0);                                    /* context refcounts count only external references */
}
TEST(ctxthr_objctx_repeat_same_pointer_and_refcount) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_goc_repeat, NULL); }

static void w_goc_addref(void *p)
{
    void *a = NULL;
    ULONG r1, r2, r3;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, &a), S_OK);
    r1 = IUnknown_AddRef((IUnknown *)a);
    r2 = IUnknown_AddRef((IUnknown *)a);
    CHECK(r2 == r1 + 1);
    r3 = IUnknown_Release((IUnknown *)a);
    CHECK(r3 == r1);
    r3 = IUnknown_Release((IUnknown *)a);
    rel(a);
}
TEST(ctxthr_objctx_addref_release_monotonic) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_goc_addref, NULL); }

TEST(ctxthr_objctx_distinct_per_sta)
{
    CxPark a, b;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&a, CXM_STA) || !cx_park_start(&b, CXM_STA)) { CHECK(!"park"); cx_park_stop(&a); cx_park_stop(&b); return; }
    CXEQ_HR(a.obj_hr, S_OK); CXEQ_HR(b.obj_hr, S_OK);
    CXNE(a.objctx, 0); CXNE(b.objctx, 0);
    CXNE(a.objctx, b.objctx);
    CXEQ(a.objctx, a.token);
    CXEQ(b.objctx, b.token);
    cx_park_stop(&a); cx_park_stop(&b);
}
TEST(ctxthr_objctx_shared_by_mta_threads)
{
    CxPark a, b;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&a, CXM_MTA) || !cx_park_start(&b, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&a); cx_park_stop(&b); return; }
    CXEQ_HR(a.obj_hr, S_OK); CXEQ_HR(b.obj_hr, S_OK);
    CXEQ(a.objctx, b.objctx);
    CXEQ(a.objctx, a.token);
    cx_park_stop(&a); cx_park_stop(&b);
}
static void w_goc_implicit(void *p)
{
    void *pv = (void *)0xdead; ULONG_PTR tok = 0;
    HRESULT hr = U(CoGetObjectContext)(&CX_IID_IObjContext, &pv);
    CXEQ_HR(hr, S_OK);
    if (!CX_FAILED(hr)) { U(CoGetContextToken)(&tok); CXEQ(pv, tok); rel(pv); }
    else CXEQ(pv, 0);
}
TEST(ctxthr_objctx_implicit_mta_thread)
{
    CxPark m;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    cx_run(CXM_NONE, w_goc_implicit, NULL);
    cx_park_stop(&m);
}

/* ================= IComThreadingInfo ================= */
static void w_cti_basic(void *p)
{
    int *want_sta = p;
    IUnknown *info = NULL;
    APTTYPE at = (APTTYPE)0x5a; THDTYPE tt = (THDTYPE)0x5a; GUID id, id2;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, (void **)&info), S_OK);
    IComThreadingInfo *ti = (IComThreadingInfo *)info;
    CXEQ_HR(IComThreadingInfo_GetCurrentApartmentType(ti, &at), S_OK);
    CXEQ_HR(IComThreadingInfo_GetCurrentThreadType(ti, &tt), S_OK);
    if (*want_sta) {
        CHECKF(cx_is_sta_type(at), "apt type %d", (int)at);
        CXEQ(tt, THDTYPE_PROCESSMESSAGES);
    } else {
        CXEQ(at, APTTYPE_MTA);
        CXEQ(tt, THDTYPE_BLOCKMESSAGES);
    }
    if (!t_is_wine()) {   /* Wine dereferences the NULL out pointers */
        CXEQ_HR(IComThreadingInfo_GetCurrentApartmentType(ti, NULL), E_INVALIDARG);
        CXEQ_HR(IComThreadingInfo_GetCurrentThreadType(ti, NULL), E_INVALIDARG);
        CXEQ_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, NULL), E_INVALIDARG);
    }
    CXEQ_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &id), S_OK);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&id2), S_OK);
    CHECK(IsEqualGUID(&id, &id2));
    rel(info);
}
TEST(ctxthr_threadinginfo_types_sta) { int sta = 1; REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_cti_basic, &sta); }
TEST(ctxthr_threadinginfo_types_mta) { int sta = 0; REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_cti_basic, &sta); }

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
static void w_cti_setid(void *p)
{
    IComThreadingInfo *ti = NULL;
    GUID orig, zero = CX_GUID_NULL, custom = CX_EXTENT_OLET, got = CX_IID_Bogus;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, (void **)&ti), S_OK);
    CXEQ_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &orig), S_OK);

    CXEQ_HR(IComThreadingInfo_SetCurrentLogicalThreadId(ti, &zero), S_OK);
    CXEQ_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &got), S_OK);
    CHECK(IsEqualGUID(&got, &zero));
    got = custom;
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&got), S_OK);
    CHECK(IsEqualGUID(&got, &zero));

    CXEQ_HR(IComThreadingInfo_SetCurrentLogicalThreadId(ti, &custom), S_OK);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&got), S_OK);
    CHECK(IsEqualGUID(&got, &custom));
    CXEQ_HR(IComThreadingInfo_GetCurrentLogicalThreadId(ti, &got), S_OK);
    CHECK(IsEqualGUID(&got, &custom));

    CXEQ_HR(IComThreadingInfo_SetCurrentLogicalThreadId(ti, &orig), S_OK);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&got), S_OK);
    CHECK(IsEqualGUID(&got, &orig));
    rel(ti);
}
TEST(ctxthr_threadinginfo_set_logical_thread_id_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_cti_setid, NULL); }
TEST(ctxthr_threadinginfo_set_logical_thread_id_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_cti_setid, NULL); }

/* The id lives in the thread, not in the context object: setting through an object affects the calling thread. */
typedef struct CtiShared { IComThreadingInfo *ti; GUID seen; GUID before_a; HRESULT apt_hr, thd_hr, set_hr, get_hr; APTTYPE apt; THDTYPE thd; } CtiShared;
static void w_cti_other_thread(void *p)
{
    CtiShared *s = p;
    GUID mine = CX_EXTENT_OLET, back;
    s->apt = (APTTYPE)0x5a; s->thd = (THDTYPE)0x5a;
    s->apt_hr = IComThreadingInfo_GetCurrentApartmentType(s->ti, &s->apt);
    s->thd_hr = IComThreadingInfo_GetCurrentThreadType(s->ti, &s->thd);
    s->set_hr = IComThreadingInfo_SetCurrentLogicalThreadId(s->ti, &mine);
    s->get_hr = IComThreadingInfo_GetCurrentLogicalThreadId(s->ti, &s->seen);
    U(CoGetCurrentLogicalThreadId)(&back);
    CHECK(IsEqualGUID(&back, &mine));
}
TEST(ctxthr_threadinginfo_object_reports_calling_thread)
{
    CxPark sta;
    CtiShared s;
    GUID after;
    HRESULT hr;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&sta, CXM_STA)) { CHECK(!"park"); cx_park_stop(&sta); return; }
    memset(&s, 0, sizeof s);
    s.ti = sta.info;
    cx_run(CXM_MTA, w_cti_other_thread, &s);
    CXEQ_HR(s.apt_hr, S_OK); CXEQ(s.apt, APTTYPE_MTA);        /* the *calling* thread's apartment, not the owner's */
    CXEQ_HR(s.thd_hr, S_OK); CXEQ(s.thd, THDTYPE_BLOCKMESSAGES);
    CXEQ_HR(s.set_hr, S_OK); CXEQ_HR(s.get_hr, S_OK);
    CHECK(IsEqualGUID(&s.seen, &sta.lid) == 0);               /* did not read the owner thread's id */
    /* the parked STA's own id is untouched: read it back through its object from a third thread */
    memset(&s, 0, sizeof s); s.ti = sta.info;
    cx_run(CXM_STA, w_cti_other_thread, &s);
    CXEQ_HR(s.apt_hr, S_OK); CHECK(cx_is_sta_type(s.apt));
    CXEQ(s.thd, THDTYPE_PROCESSMESSAGES);
    cx_park_stop(&sta);
    (void)after; (void)hr;
}
static void w_cti_nocom(void *p)
{
    CtiShared *s = p;
    s->apt = (APTTYPE)0x5a; s->thd = (THDTYPE)0x5a;
    s->apt_hr = IComThreadingInfo_GetCurrentApartmentType(s->ti, &s->apt);
    s->thd_hr = IComThreadingInfo_GetCurrentThreadType(s->ti, &s->thd);
    s->get_hr = IComThreadingInfo_GetCurrentLogicalThreadId(s->ti, &s->seen);
}
TEST(ctxthr_threadinginfo_object_on_uninitialized_thread)
{
    CxPark sta;
    CtiShared s;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&sta, CXM_STA)) { CHECK(!"park"); cx_park_stop(&sta); return; }
    memset(&s, 0, sizeof s); s.ti = sta.info;
    cx_run(CXM_NONE, w_cti_nocom, &s);
    CXEQ_HR(s.apt_hr, CX_HR_FAIL);                 /* no apartment on the calling thread: E_FAIL */
    CXEQ(s.apt, 0x5a);
    CXEQ_HR(s.thd_hr, CX_HR_FAIL);
    CXEQ(s.thd, 0x5a);
    t_note("GetCurrentLogicalThreadId on an uninitialized thread via a foreign context object: 0x%08lx", (unsigned long)s.get_hr);
    cx_park_stop(&sta);
}

/* ================= IContext ================= */
static void w_ictx(void *p)
{
    CxIContext *ctx = NULL;
    CxObj o; GUID prop = CX_EXTENT_OLET; DWORD flags = 0x77; IUnknown *got = (IUnknown *)0xdead; void *en = (void *)0xdead;
    HRESULT hr;
    cx_obj_init(&o, 0, 0);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContext, (void **)&ctx), S_OK);
    if (!ctx) return;
    CXEQ_HR(ctx->lpVtbl->SetProperty(ctx, &prop, 0, NULL), E_INVALIDARG);          /* NULL punk is rejected first */
    CXEQ_HR(ctx->lpVtbl->SetProperty(ctx, &prop, 0, cx_obj_unk(&o)), CX_HR_FAIL);   /* default context is frozen */
    CXEQ(o.ref, 1);                                                                 /* nothing retained */
    CXEQ_HR(ctx->lpVtbl->RemoveProperty(ctx, &prop), CX_HR_FAIL);
    hr = ctx->lpVtbl->GetProperty(ctx, &prop, &flags, &got);
    CXEQ_HR(hr, S_OK);                             /* absent property is not an error ... */
    CXEQ(got, 0);                                  /* ... it yields NULL */
    CXEQ(flags, 0x77);
    hr = ctx->lpVtbl->EnumContextProps(ctx, &en);
    t_note("IContext::EnumContextProps on an empty default context: 0x%08lx, enum=%p", (unsigned long)hr, en);
    if (!CX_FAILED(hr) && en) rel(en);
    rel(ctx);
}
TEST(ctxthr_icontext_default_context_properties_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_ictx, NULL); }
TEST(ctxthr_icontext_default_context_properties_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_ictx, NULL); }

/* ================= IContextCallback ================= */
typedef struct CbData {
    ComCallData cd;                  /* must be first: the callback receives this pointer */
    HRESULT ret;
    int called; ComCallData *seen_ptr; DWORD tid;
    HRESULT tok_hr, lid_hr, apt_hr, info_hr, info_apt_hr, info_thd_hr;
    ULONG_PTR token; GUID lid;
    APTTYPE type; APTTYPEQUALIFIER qual; APTTYPE info_apt; THDTYPE info_thd;
    DWORD seen_dispid; void *seen_user;
} CbData;
static HRESULT WINAPI cb_record(ComCallData *pd)
{
    CbData *d = (CbData *)pd;
    IComThreadingInfo *ti = NULL;
    d->called++;
    d->seen_ptr = pd; d->seen_dispid = pd->dwDispid; d->seen_user = pd->pUserDefined;
    d->tid = GetCurrentThreadId();
    d->tok_hr = U(CoGetContextToken)(&d->token);
    d->lid_hr = U(CoGetCurrentLogicalThreadId)(&d->lid);
    d->type = (APTTYPE)0x5a; d->qual = (APTTYPEQUALIFIER)0x5a;
    d->apt_hr = U(CoGetApartmentType)(&d->type, &d->qual);
    d->info_hr = U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, (void **)&ti);
    if (ti) {
        d->info_apt = (APTTYPE)0x5a; d->info_thd = (THDTYPE)0x5a;
        d->info_apt_hr = IComThreadingInfo_GetCurrentApartmentType(ti, &d->info_apt);
        d->info_thd_hr = IComThreadingInfo_GetCurrentThreadType(ti, &d->info_thd);
        IComThreadingInfo_Release(ti);
    }
    return d->ret;
}
static HRESULT do_cb(void *cb, CbData *d, const GUID *riid)
{
    HRESULT hr;
    memset(&d->called, 0, sizeof *d - offsetof(CbData, called));
    d->cd.dwDispid = 0x1234; d->cd.dwReserved = 0; d->cd.pUserDefined = d;
    hr = IContextCallback_ContextCallback((IContextCallback *)cb, cb_record, &d->cd, riid, 2, NULL);
    return hr;
}

static void w_cb_same_ctx(void *p)
{
    IContextCallback *cb = NULL;
    CbData d; HRESULT hr; ULONG_PTR tok = 0; GUID lid;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, (void **)&cb), S_OK);
    CXEQ_HR(U(CoGetContextToken)(&tok), S_OK);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&lid), S_OK);

    d.ret = S_FALSE;
    hr = do_cb(cb, &d, &CX_IID_IContextCallback);
    CXEQ_HR(hr, S_FALSE);                          /* the callback's HRESULT is the result */
    CXEQ(d.called, 1);
    CXEQ(d.tid, GetCurrentThreadId());             /* same context: runs inline */
    CXEQ(d.seen_ptr, &d.cd);                       /* same parameter block */
    CXEQ(d.seen_dispid, 0x1234);
    CXEQ(d.seen_user, &d);
    CXEQ_HR(d.tok_hr, S_OK); CXEQ(d.token, tok);
    CXEQ_HR(d.lid_hr, S_OK); CHECK(IsEqualGUID(&d.lid, &lid));

    d.ret = E_FAIL;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IContextCallback), E_FAIL);
    CXEQ(d.called, 1);
    d.ret = E_NOTIMPL;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IContextCallback), E_NOTIMPL);
    d.ret = S_OK;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IContextCallback), S_OK);
    CXEQ(d.called, 1);
    d.ret = (HRESULT)0x8abc1234;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IContextCallback), 0x8abc1234);
    rel(cb);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_ctxcb_same_context_runs_inline_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_cb_same_ctx, NULL); }
TEST(ctxthr_ctxcb_same_context_runs_inline_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_cb_same_ctx, NULL); }

static void w_cb_null_fn(void *p)
{
    IContextCallback *cb = NULL;
    ComCallData cd = { 1, 2, NULL };
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, (void **)&cb), S_OK);
    CXEQ_HR(IContextCallback_ContextCallback(cb, NULL, &cd, &CX_IID_IContextCallback, 2, NULL), E_INVALIDARG);
    CXEQ_HR(IContextCallback_ContextCallback(cb, NULL, NULL, &CX_IID_IEnterActivityWithNoLock, 0, NULL), E_INVALIDARG);
    rel(cb);
}
TEST(ctxthr_ctxcb_null_callback_e_invalidarg) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_cb_null_fn, NULL); cx_run(CXM_MTA, w_cb_null_fn, NULL); }

static void w_cb_null_param(void *p)
{
    /* the parameter block is opaque to COM: NULL is passed through */
    IContextCallback *cb = NULL;
    CbData d; HRESULT hr;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, (void **)&cb), S_OK);
    memset(&d, 0, sizeof d);
    d.ret = S_OK;
    hr = IContextCallback_ContextCallback(cb, cb_record, NULL, &CX_IID_IContextCallback, 2, NULL);
    t_note("ContextCallback with NULL pParam in the same context: 0x%08lx", (unsigned long)hr);
    rel(cb);
}
TEST(ctxthr_ctxcb_null_param_observed) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_cb_null_param, NULL); }

/* IID_IEnterActivityWithNoLock: the callback sees the finalizer logical thread id, which is restored afterwards. */
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
static void w_cb_enter_activity(void *p)
{
    IContextCallback *cb = NULL;
    CbData d; GUID before, after, mine = CX_EXTENT_OLET;
    IComThreadingInfo *ti = NULL;
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IContextCallback, (void **)&cb), S_OK);
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IComThreadingInfo, (void **)&ti), S_OK);
    IComThreadingInfo_SetCurrentLogicalThreadId(ti, &mine);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&before), S_OK);
    d.ret = S_FALSE;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IEnterActivityWithNoLock), S_FALSE);
    CXEQ(d.called, 1);
    CXEQ_HR(d.lid_hr, S_OK);
    CHECK(IsEqualGUID(&d.lid, &CX_IID_IEnterActivityWithNoLock));
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&after), S_OK);
    CHECK(IsEqualGUID(&after, &before));           /* restored */
    CHECK(IsEqualGUID(&after, &mine));
    /* failure of the callback does not leave the finalizer id behind either */
    d.ret = E_FAIL;
    CXEQ_HR(do_cb(cb, &d, &CX_IID_IEnterActivityWithNoLock), E_FAIL);
    CXEQ_HR(U(CoGetCurrentLogicalThreadId)(&after), S_OK);
    CHECK(IsEqualGUID(&after, &mine));
    rel(cb); rel(ti);
}
TEST(ctxthr_ctxcb_enter_activity_with_no_lock_logical_id_sta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_STA, w_cb_enter_activity, NULL); }
TEST(ctxthr_ctxcb_enter_activity_with_no_lock_logical_id_mta) { REQUIRE(U(CoGetObjectContext)); cx_run(CXM_MTA, w_cb_enter_activity, NULL); }

/* ---- cross-apartment callbacks ---- */
typedef struct XCb { void *cb; ULONG_PTR token; DWORD owner_tid; GUID caller_lid; CbData d; HRESULT hr; HRESULT hr_ea; CbData d_ea; DWORD caller_tid; } XCb;
static void w_xcb_call(void *p)
{
    XCb *x = p;
    x->caller_tid = GetCurrentThreadId();
    U(CoGetCurrentLogicalThreadId)(&x->caller_lid);
    x->d.ret = S_FALSE;
    x->hr = do_cb(x->cb, &x->d, &CX_IID_IContextCallback);
    x->d_ea.ret = S_FALSE;
    x->hr_ea = do_cb(x->cb, &x->d_ea, &CX_IID_IEnterActivityWithNoLock);
}
static void run_xcb_to_sta(DWORD caller_model, int strict_lid)
{
    CxPark a; XCb x;
    if (!cx_park_start(&a, CXM_STA)) { CHECK(!"park"); cx_park_stop(&a); return; }
    memset(&x, 0, sizeof x);
    x.cb = a.cb; x.token = a.token; x.owner_tid = a.tid;
    cx_run(caller_model, w_xcb_call, &x);
    CXEQ_HR(x.hr, S_FALSE);
    CXEQ(x.d.called, 1);
    CXEQ(x.d.tid, a.tid);                          /* an STA context is entered on its own thread */
    CXNE(x.d.tid, x.caller_tid);
    CXEQ_HR(x.d.tok_hr, S_OK); CXEQ(x.d.token, a.token);
    CXEQ_HR(x.d.apt_hr, S_OK); CHECK(cx_is_sta_type(x.d.type));
    if (strict_lid) { CXEQ_HR(x.d.lid_hr, S_OK); CHECK(IsEqualGUID(&x.d.lid, &x.caller_lid)); }
    else t_note("callback logical thread id equals the caller's: %d", IsEqualGUID(&x.d.lid, &x.caller_lid));
    CXEQ(x.d.seen_dispid, 0x1234);
    CXEQ_HR(x.hr_ea, S_FALSE);
    CXEQ(x.d_ea.tid, a.tid);
    CHECK(IsEqualGUID(&x.d_ea.lid, &CX_IID_IEnterActivityWithNoLock));
    cx_park_stop(&a);
}
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(ctxthr_ctxcb_sta_to_other_sta_runs_on_owner_thread)  { REQUIRE(U(CoGetObjectContext)); run_xcb_to_sta(CXM_STA, 1); }
TEST(ctxthr_ctxcb_mta_to_sta_runs_on_owner_thread)        { REQUIRE(U(CoGetObjectContext)); run_xcb_to_sta(CXM_MTA, 0); }

static void run_xcb_to_mta(DWORD caller_model)
{
    CxPark m; XCb x;
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    memset(&x, 0, sizeof x);
    x.cb = m.cb; x.token = m.token; x.owner_tid = m.tid;
    cx_run(caller_model, w_xcb_call, &x);
    CXEQ_HR(x.hr, S_FALSE);
    CXEQ(x.d.called, 1);
    CXEQ_HR(x.d.tok_hr, S_OK); CXEQ(x.d.token, m.token);       /* executed in the MTA default context */
    CXEQ_HR(x.d.apt_hr, S_OK); CXEQ(x.d.type, APTTYPE_MTA);
    CXEQ_HR(x.d.info_apt_hr, S_OK); CXEQ(x.d.info_apt, APTTYPE_MTA);
    t_note("callback into the MTA ran on %s thread", x.d.tid == x.caller_tid ? "the caller's" : x.d.tid == m.tid ? "the MTA owner's" : "another");
    CXEQ_HR(x.hr_ea, S_FALSE);
    cx_park_stop(&m);
}
TEST(ctxthr_ctxcb_sta_to_mta_executes_in_mta_context) { REQUIRE(U(CoGetObjectContext)); run_xcb_to_mta(CXM_STA); }
TEST(ctxthr_ctxcb_mta_to_mta_same_context_runs_on_caller)
{
    CxPark m; XCb x;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    memset(&x, 0, sizeof x);
    x.cb = m.cb;
    cx_run(CXM_MTA, w_xcb_call, &x);
    CXEQ_HR(x.hr, S_FALSE);
    CXEQ(x.d.tid, x.caller_tid);                   /* same context: no thread switch */
    CXEQ(x.d.token, m.token);
    cx_park_stop(&m);
}
TEST(ctxthr_ctxcb_implicit_mta_to_mta)
{
    CxPark m; XCb x;
    REQUIRE(U(CoGetObjectContext));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    memset(&x, 0, sizeof x);
    x.cb = m.cb;
    cx_run(CXM_NONE, w_xcb_call, &x);
    t_note("ContextCallback from an implicit-MTA thread: hr 0x%08lx, called %d", (unsigned long)x.hr, x.d.called);
    if (x.d.called) { CXEQ(x.d.token, m.token); CXEQ(x.d.tid, x.caller_tid); }
    cx_park_stop(&m);
}

/* ---- NTA ---- */
typedef struct NtaRun { DWORD model; HRESULT cb_hr, ctx_hr, got_hr, tok_hr, after_hr; ULONG_PTR nta_obj; CbData d; CbData d2;
    APTTYPE after_type; DWORD tid; int skip; ULONG_PTR outer_token; HRESULT hr2; } NtaRun;
static void w_nta(void *p)
{
    NtaRun *r = p;
    void *cb = NULL, *obj = NULL; APTTYPEQUALIFIER q;
    r->tid = GetCurrentThreadId();
    U(CoGetContextToken)(&r->outer_token);
    r->ctx_hr = U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IContextCallback, &cb);
    r->got_hr = U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IObjContext, &obj);
    r->nta_obj = (ULONG_PTR)obj;
    if (CX_FAILED(r->ctx_hr) || !cb) { r->skip = 1; rel(obj); return; }
    r->d.ret = S_FALSE;
    r->cb_hr = do_cb(cb, &r->d, &CX_IID_IContextCallback);
    r->d2.ret = S_OK;
    r->hr2 = do_cb(cb, &r->d2, &CX_IID_IEnterActivityWithNoLock);
    r->after_type = (APTTYPE)0x5a; q = 0;
    r->after_hr = U(CoGetApartmentType)(&r->after_type, &q);
    rel(cb); rel(obj);
}
static void run_nta(DWORD model, APTTYPEQUALIFIER expect_qual_a, APTTYPEQUALIFIER expect_qual_b, int is_sta)
{
    NtaRun r;
    CxPark m;
    int parked = 0;
    memset(&r, 0, sizeof r);
    if (model == CXM_NONE) { if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; } parked = 1; }
    if (!cx_run(model, w_nta, &r)) { if (parked) cx_park_stop(&m); return; }
    if (parked) cx_park_stop(&m);
    CXEQ_HR(r.ctx_hr, S_OK);
    CXEQ_HR(r.got_hr, S_OK);
    if (r.skip) return;
    CXEQ_HR(r.cb_hr, S_FALSE);
    CXEQ(r.d.called, 1);
    CXEQ(r.d.tid, r.tid);                          /* the NTA has no thread: the caller's thread executes */
    CXEQ_HR(r.d.tok_hr, S_OK);
    CXEQ(r.d.token, r.nta_obj);                    /* the context seen inside is the NTA default context */
    CXNE(r.d.token, r.outer_token);
    CXEQ_HR(r.d.apt_hr, S_OK);
    CXEQ(r.d.type, APTTYPE_NA);
    if (is_sta) CHECK(r.d.qual == expect_qual_a || r.d.qual == expect_qual_b);
    else CXEQ(r.d.qual, expect_qual_a);
    CXEQ_HR(r.d.info_apt_hr, S_OK);
    CXEQ(r.d.info_apt, APTTYPE_NA);
    CXEQ_HR(r.after_hr, model == CXM_NONE ? S_OK : S_OK);
    CXEQ_HR(r.hr2, S_OK);
    if (model == CXM_STA) CHECK(cx_is_sta_type(r.after_type));
    else CXEQ(r.after_type, APTTYPE_MTA);          /* back in the caller's own apartment */
    t_note("NTA callback: qualifier %d, thread type %d", (int)r.d.qual, (int)r.d.info_thd);
}
TEST(ctxthr_ctxcb_nta_from_sta) { REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetDefaultContext)); run_nta(CXM_STA, APTTYPEQUALIFIER_NA_ON_STA, APTTYPEQUALIFIER_NA_ON_MAINSTA, 1); }
TEST(ctxthr_ctxcb_nta_from_mta) { REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetDefaultContext)); run_nta(CXM_MTA, APTTYPEQUALIFIER_NA_ON_MTA, APTTYPEQUALIFIER_NA_ON_MTA, 0); }
TEST(ctxthr_ctxcb_nta_from_implicit_mta)
{
    NtaRun r; CxPark m;
    REQUIRE(U(CoGetObjectContext)); REQUIRE(U(CoGetDefaultContext));
    if (!cx_park_start(&m, CXM_MTA)) { CHECK(!"park"); cx_park_stop(&m); return; }
    memset(&r, 0, sizeof r);
    cx_run(CXM_NONE, w_nta, &r);
    cx_park_stop(&m);
    t_note("NTA from implicit MTA thread: ctx_hr 0x%08lx cb_hr 0x%08lx qual %d", (unsigned long)r.ctx_hr, (unsigned long)r.cb_hr, (int)r.d.qual);
    if (r.d.called) { CXEQ(r.d.type, APTTYPE_NA); CXEQ(r.d.qual, APTTYPEQUALIFIER_NA_ON_IMPLICIT_MTA); }
}

/* ================= CoCreateObjectInContext ================= */
static void w_coc_neg(void *p)
{
    CxObj punk, fake;
    void *cur = NULL, *out = (void *)0xdead;
    cx_obj_init(&punk, 0, 1); cx_obj_init(&fake, 0, 0);
    /* a context object that does not know the internal context interface is rejected */
    CXEQ_HR(UN(CoCreateObjectInContext)(cx_obj_unk(&punk), cx_obj_unk(&fake), &CX_IID_Bogus, &out), E_INVALIDARG);
    CXEQ(out, 0);
    CXEQ(fake.qi_calls > 0, 1);
    /* the current context is not a valid target */
    CXEQ_HR(U(CoGetObjectContext)(&CX_IID_IObjContext, &cur), S_OK);
    out = (void *)0xdead;
    CXEQ_HR(UN(CoCreateObjectInContext)(cx_obj_unk(&punk), (IUnknown *)cur, &CX_IID_Bogus, &out), E_INVALIDARG);
    CXEQ(out, 0);
    rel(cur);
}
TEST(ctxthr_create_in_context_invalid_context_sta) { REQUIRE(UN(CoCreateObjectInContext)); REQUIRE(U(CoGetObjectContext)); if (t_is_wine()) { t_skip("aborting stub under Wine"); return; } cx_run(CXM_STA, w_coc_neg, NULL); }
TEST(ctxthr_create_in_context_invalid_context_mta) { REQUIRE(UN(CoCreateObjectInContext)); REQUIRE(U(CoGetObjectContext)); if (t_is_wine()) { t_skip("aborting stub under Wine"); return; } cx_run(CXM_MTA, w_coc_neg, NULL); }

static void w_coc_pos(void *p)
{
    CxObj punk;
    void *nta = NULL, *out = (void *)0xdead;
    HRESULT hr;
    cx_obj_init(&punk, 0, 1);
    CXEQ_HR(U(CoGetDefaultContext)(APTTYPE_NA, &CX_IID_IObjContext, &nta), S_OK);
    hr = UN(CoCreateObjectInContext)(cx_obj_unk(&punk), (IUnknown *)nta, &CX_IID_IUnknown, &out);
    t_note("CoCreateObjectInContext(punk, NTA context, IUnknown): 0x%08lx out=%p", (unsigned long)hr, out);
    if (CX_FAILED(hr)) CXEQ(out, 0);
    else { CXNE(out, 0); rel(out); }
    rel(nta);
}
TEST(ctxthr_create_in_context_into_nta_observed) { REQUIRE(UN(CoCreateObjectInContext)); REQUIRE(U(CoGetDefaultContext)); if (t_is_wine()) { t_skip("aborting stub under Wine"); return; } cx_run(CXM_STA, w_coc_pos, NULL); }

/* ================= service domains ================= */
static void w_sd_args(void *p)
{
    CxObj o;
    cx_obj_init(&o, 0, 0);
    CXEQ_HR(UN(CoPushServiceDomain)(NULL), E_INVALIDARG);
    CXEQ_HR(UN(CoPopServiceDomain)(NULL), E_INVALIDARG);
}
TEST(ctxthr_service_domain_null_arguments)
{
    REQUIRE(UN(CoPushServiceDomain)); REQUIRE(UN(CoPopServiceDomain));
    if (t_is_wine()) { t_skip("CoPushServiceDomain is an aborting stub under Wine"); return; }
    cx_run(CXM_NONE, w_sd_args, NULL); cx_run(CXM_STA, w_sd_args, NULL); cx_run(CXM_MTA, w_sd_args, NULL);
}
static void w_sd_uninit(void *p)
{
    CxObj o; cx_obj_init(&o, 0, 0);
    CXEQ_HR(UN(CoPushServiceDomain)(cx_obj_unk(&o)), CX_HR_NOTINIT);   /* requires an initialized apartment */
    CXEQ(o.ref, 1);
}
TEST(ctxthr_service_domain_push_requires_apartment) { REQUIRE(UN(CoPushServiceDomain)); if (t_is_wine()) { t_skip("aborting stub under Wine"); return; } cx_run(CXM_NONE, w_sd_uninit, NULL); }
