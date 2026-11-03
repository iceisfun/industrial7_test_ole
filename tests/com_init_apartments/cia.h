/* Shared helpers for the com_init_apartments area.
 * Everything under test is reached through U()/UIN()/UN() (LoadLibrary + GetProcAddress). */
#ifndef CIA_H
#define CIA_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <objidl.h>
#include <ctxtcall.h>
#include <rpcdce.h>

/* ---- constants (kept local: the corpus never links libuuid / the DLL) ---- */
#define CIA_E_NOINTERFACE   ((HRESULT)0x80004002L)
#define CIA_E_NOTIMPL       ((HRESULT)0x80004001L)
#define CIA_E_POINTER       ((HRESULT)0x80004003L)
#define CIA_E_INVALIDARG    ((HRESULT)0x80070057L)
#define CIA_NOTINIT         ((HRESULT)0x800401F0L)   /* CO_E_NOTINITIALIZED */
#define CIA_CHANGED_MODE    ((HRESULT)0x80010106L)   /* RPC_E_CHANGED_MODE */
#define CIA_TOO_LATE        ((HRESULT)0x80010119L)   /* RPC_E_TOO_LATE */

/* APTTYPE / APTTYPEQUALIFIER numeric values (objidlbase.h) */
#define CIA_APT_CURRENT   (-1)
#define CIA_APT_STA       0
#define CIA_APT_MTA       1
#define CIA_APT_NA        2
#define CIA_APT_MAINSTA   3
#define CIA_Q_NONE                 0
#define CIA_Q_IMPLICIT_MTA         1
#define CIA_Q_NA_ON_MTA            2
#define CIA_Q_NA_ON_STA            3
#define CIA_Q_NA_ON_IMPLICIT_MTA   4
#define CIA_Q_NA_ON_MAINSTA        5
#define CIA_Q_APPLICATION_STA      6

#define CIA_COINIT_MTA   0x0
#define CIA_COINIT_STA   0x2
#define CIA_COINIT_NOOLE1DDE 0x4
#define CIA_COINIT_SPEED 0x8

static const GUID CIA_GUID_NULL = {0,0,0,{0,0,0,0,0,0,0,0}};
static const GUID CIA_IID_IUnknown        = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IInitializeSpy  = {0x00000034,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IObjContext     = {0x000001c6,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IComThreadingInfo = {0x000001ce,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IContextCallback  = {0x000001da,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IStream         = {0x0000000c,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CIA_IID_IClassFactory   = {0x00000001,0,0,{0xC0,0,0,0,0,0,0,0x46}};

UDECL(HRESULT, CoGetApartmentID, (int aptType, DWORD *pdwAptId));
UDECL(HRESULT, CoInitializeWOW, (void *a, void *b));

/* ---- small wrappers over the resolved entry points ---------------------- */
#define cInitEx(pv, fl)   (U(CoInitializeEx)((pv), (fl)))
#define cInit(pv)         (U(CoInitialize)(pv))
#define cUninit()         (U(CoUninitialize)())
#define cOleInit(pv)      (U(OleInitialize)(pv))
#define cOleUninit()      (U(OleUninitialize)())

/* CoGetApartmentType: returns hr, outputs in *t / *q. */
static inline HRESULT cia_apt(int *t, int *q)
{
    APTTYPE tt = (APTTYPE)0x7777; APTTYPEQUALIFIER qq = (APTTYPEQUALIFIER)0x7777;
    HRESULT hr = U(CoGetApartmentType)(&tt, &qq);
    if (t) *t = (int)tt;
    if (q) *q = (int)qq;
    return hr;
}
static inline int cia_inited(void) { int t, q; return cia_apt(&t, &q) == S_OK; }
/* hr of CoGetApartmentType only */
static inline HRESULT cia_apt_hr(void) { int t, q; return cia_apt(&t, &q); }

/* Uninitialize until the calling thread reports CO_E_NOTINITIALIZED (bounded). */
static inline int cia_drain(void)
{
    int n = 0;
    while (n < 2000 && cia_inited()) { cUninit(); n++; }
    return n;
}

/* Check type + qualifier of the calling thread. */
#define CIA_CHECK_APT(want_hr, want_t, want_q) do { int t_ = 0x7777, q_ = 0x7777; \
    HRESULT h_ = cia_apt(&t_, &q_); \
    t_check(h_ == (HRESULT)(want_hr), __FILE__, __LINE__, "CoGetApartmentType hr", "got 0x%08lx want 0x%08lx", (unsigned long)h_, (unsigned long)(HRESULT)(want_hr)); \
    t_check(t_ == (want_t), __FILE__, __LINE__, "apt type", "got %d want %d", t_, (int)(want_t)); \
    t_check(q_ == (want_q), __FILE__, __LINE__, "apt qualifier", "got %d want %d", q_, (int)(want_q)); } while (0)
#define CIA_CHECK_NOTINIT() CIA_CHECK_APT(CIA_NOTINIT, CIA_APT_CURRENT, CIA_Q_NONE)

#define EXPECT_HR(expr, want) do { HRESULT h__ = (expr); \
    t_check(h__ == (HRESULT)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%08lx want 0x%08lx", (unsigned long)h__, (unsigned long)(HRESULT)(want)); } while (0)

/* ---- worker threads ------------------------------------------------------ */
typedef void (*cia_fn)(void *);

typedef struct {
    cia_fn fn; void *arg; HANDLE done;
} cia_once;

static DWORD WINAPI cia_once_proc(void *p)
{
    cia_once *o = (cia_once *)p;
    o->fn(o->arg);
    SetEvent(o->done);
    return 0;
}

/* Run fn(arg) on a fresh thread and wait (bounded).  Returns 1 when it completed. */
static inline int cia_run(cia_fn fn, void *arg)
{
    cia_once o; HANDLE th; int ok;
    o.fn = fn; o.arg = arg; o.done = CreateEventA(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, cia_once_proc, &o, 0, NULL);
    if (!th) { CloseHandle(o.done); t_check(0, __FILE__, __LINE__, "CreateThread", NULL); return 0; }
    ok = WaitForSingleObject(o.done, 10000) == WAIT_OBJECT_0;
    t_check(ok, __FILE__, __LINE__, "worker finished within 10s", NULL);
    if (ok) WaitForSingleObject(th, 5000); /* else: leak the thread rather than free the stack it uses */
    CloseHandle(th);
    if (ok) CloseHandle(o.done);
    return ok;
}

/* Persistent worker: executes commands posted from the owning thread, one at a time. */
typedef struct {
    HANDLE th, evCmd, evDone;
    cia_fn fn; void *arg;
    volatile LONG quit;
} cia_w;

static DWORD WINAPI cia_w_proc(void *p)
{
    cia_w *w = (cia_w *)p;
    for (;;) {
        if (WaitForSingleObject(w->evCmd, 20000) != WAIT_OBJECT_0) break;
        if (w->quit) break;
        w->fn(w->arg);
        SetEvent(w->evDone);
    }
    return 0;
}
static inline int cia_w_start(cia_w *w)
{
    memset(w, 0, sizeof *w);
    w->evCmd = CreateEventA(NULL, FALSE, FALSE, NULL);
    w->evDone = CreateEventA(NULL, FALSE, FALSE, NULL);
    w->th = CreateThread(NULL, 0, cia_w_proc, w, 0, NULL);
    t_check(w->th != NULL, __FILE__, __LINE__, "CreateThread", NULL);
    return w->th != NULL;
}
static inline int cia_w_call(cia_w *w, cia_fn fn, void *arg)
{
    int ok;
    w->fn = fn; w->arg = arg;
    SetEvent(w->evCmd);
    ok = WaitForSingleObject(w->evDone, 10000) == WAIT_OBJECT_0;
    t_check(ok, __FILE__, __LINE__, "worker command finished within 10s", NULL);
    return ok;
}
static inline void cia_w_stop(cia_w *w)
{
    w->quit = 1; SetEvent(w->evCmd);
    if (WaitForSingleObject(w->th, 10000) != WAIT_OBJECT_0) {
        t_check(0, __FILE__, __LINE__, "worker exited", NULL);
        TerminateThread(w->th, 0);
    }
    CloseHandle(w->th); CloseHandle(w->evCmd); CloseHandle(w->evDone);
}

/* canned worker commands */
static void cia_cmd_init_sta(void *a)  { *(HRESULT *)a = cInitEx(NULL, CIA_COINIT_STA); }
static void cia_cmd_init_mta(void *a)  { *(HRESULT *)a = cInitEx(NULL, CIA_COINIT_MTA); }
static void cia_cmd_uninit(void *a)    { cUninit(); (void)a; }
static void cia_cmd_drain(void *a)     { *(int *)a = cia_drain(); }
struct cia_apt_res { HRESULT hr; int t, q; };
static void cia_cmd_apt(void *a)       { struct cia_apt_res *r = (struct cia_apt_res *)a; r->hr = cia_apt(&r->t, &r->q); }

/* ---- IInitializeSpy implementation --------------------------------------- */
enum { CIA_EV_PREINIT = 1, CIA_EV_POSTINIT, CIA_EV_PREUNINIT, CIA_EV_POSTUNINIT };
typedef struct { int id, ev; DWORD a, b, c; DWORD tid; } cia_ev;
#define CIA_LOGMAX 256
static cia_ev cia_log[CIA_LOGMAX];
static int cia_nlog;
static inline void cia_log_reset(void) { cia_nlog = 0; }
static inline void cia_log_add(int id, int ev, DWORD a, DWORD b, DWORD c)
{
    if (cia_nlog < CIA_LOGMAX) { cia_ev *e = &cia_log[cia_nlog++]; e->id = id; e->ev = ev; e->a = a; e->b = b; e->c = c; e->tid = GetCurrentThreadId(); }
}
static inline int cia_log_count(int id, int ev)
{
    int i, n = 0;
    for (i = 0; i < cia_nlog; i++) if ((id < 0 || cia_log[i].id == id) && cia_log[i].ev == ev) n++;
    return n;
}

typedef struct cia_spy {
    const IInitializeSpyVtbl *lpVtbl;
    LONG ref, nqi, naddref, nrelease;
    int id;
    HRESULT pre_ret;           /* returned from PreInitialize / PreUninitialize */
    int override_post;         /* when set, PostInitialize returns post_ret instead of hrCoInit */
    HRESULT post_ret;
    int fail_qi;               /* QueryInterface(IID_IInitializeSpy) returns E_NOINTERFACE */
    int revoke_in_preuninit;   /* revoke own registration from PreUninitialize */
    ULARGE_INTEGER cookie;
    HRESULT revoke_hr;
    GUID last_qi;
} cia_spy;

static HRESULT STDMETHODCALLTYPE cia_spy_QI(IInitializeSpy *i, REFIID riid, void **ppv)
{
    cia_spy *s = (cia_spy *)i;
    s->nqi++; s->last_qi = *riid;
    if (s->fail_qi) { *ppv = NULL; return E_NOINTERFACE; }
    if (IsEqualGUID(riid, &CIA_IID_IInitializeSpy) || IsEqualGUID(riid, &CIA_IID_IUnknown)) {
        *ppv = i; InterlockedIncrement(&s->ref); s->naddref++; return S_OK;
    }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE cia_spy_AddRef(IInitializeSpy *i)  { cia_spy *s = (cia_spy *)i; s->naddref++; return InterlockedIncrement(&s->ref); }
static ULONG STDMETHODCALLTYPE cia_spy_Release(IInitializeSpy *i) { cia_spy *s = (cia_spy *)i; s->nrelease++; return InterlockedDecrement(&s->ref); }
static HRESULT STDMETHODCALLTYPE cia_spy_PreInit(IInitializeSpy *i, DWORD coinit, DWORD refs)
{ cia_spy *s = (cia_spy *)i; cia_log_add(s->id, CIA_EV_PREINIT, coinit, refs, 0); return s->pre_ret; }
static HRESULT STDMETHODCALLTYPE cia_spy_PostInit(IInitializeSpy *i, HRESULT hr, DWORD coinit, DWORD refs)
{ cia_spy *s = (cia_spy *)i; cia_log_add(s->id, CIA_EV_POSTINIT, (DWORD)hr, coinit, refs); return s->override_post ? s->post_ret : hr; }
static HRESULT STDMETHODCALLTYPE cia_spy_PreUninit(IInitializeSpy *i, DWORD refs)
{
    cia_spy *s = (cia_spy *)i;
    cia_log_add(s->id, CIA_EV_PREUNINIT, refs, 0, 0);
    if (s->revoke_in_preuninit) { s->revoke_in_preuninit = 0; s->revoke_hr = U(CoRevokeInitializeSpy)(s->cookie); }
    return s->pre_ret;
}
static HRESULT STDMETHODCALLTYPE cia_spy_PostUninit(IInitializeSpy *i, DWORD refs)
{ cia_spy *s = (cia_spy *)i; cia_log_add(s->id, CIA_EV_POSTUNINIT, refs, 0, 0); return E_NOTIMPL; }

static const IInitializeSpyVtbl cia_spy_vtbl = {
    cia_spy_QI, cia_spy_AddRef, cia_spy_Release,
    cia_spy_PreInit, cia_spy_PostInit, cia_spy_PreUninit, cia_spy_PostUninit
};
static inline void cia_spy_init(cia_spy *s, int id)
{
    memset(s, 0, sizeof *s);
    s->lpVtbl = &cia_spy_vtbl; s->ref = 1; s->id = id; s->pre_ret = S_OK;
}
#define CIA_SPYP(s) ((IInitializeSpy *)(s))

/* Parent/child helper: child exit code = line number of the first failed check (0 = all passed). */
#define CCHK(c) do { if (!(c) && !bad) bad = __LINE__; } while (0)
#define CCHK_HR(e, want) do { HRESULT h__ = (e); if (h__ != (HRESULT)(want) && !bad) bad = __LINE__; } while (0)

#endif
