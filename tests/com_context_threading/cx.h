/* Shared helpers for the com_context_threading area: GUIDs, thread runner, mock COM objects. */
#ifndef CX_H
#define CX_H
#include "t.h"
#define COBJMACROS
#include <ole2.h>
#include <objidl.h>
#include <ctxtcall.h>

/* GUIDs defined locally: the corpus never links uuid.lib. */
#define CXG(name, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
    static const GUID name = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }
CXG(CX_IID_IUnknown,        0x00000000, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IClassFactory,   0x00000001, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IMarshal,        0x00000003, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IMessageFilter,  0x00000016, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_ICancelMethodCalls, 0x00000029, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_ISynchronize,    0x00000030, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_ISynchronizeHandle, 0x00000031, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_ISynchronizeEvent,  0x00000032, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_ISynchronizeContainer, 0x00000033, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IContext,        0x000001c0, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IObjContext,     0x000001c6, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IComThreadingInfo, 0x000001ce, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IContextCallback,  0x000001da, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IMarshal2,       0x000001cf, 0, 0, 0xc0,0,0,0,0,0,0,0x46); /* unused placeholder */
CXG(CX_IID_IDispatch,       0x00020400, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IServerSecurity, 0x0000013e, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IStream,         0x0000000c, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_IID_IChannelHook,    0x1008c4a0, 0x7613, 0x11cf, 0x9a,0xf1,0x00,0x20,0xaf,0x6e,0x72,0xf4);
CXG(CX_IID_IEnterActivityWithNoLock, 0xd7174f82, 0x36b8, 0x4aa8, 0x80,0x0a,0xe9,0x63,0xab,0x2d,0xfa,0xb9);
CXG(CX_CLSID_StdEvent,         0x0000032b, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_CLSID_ManualResetEvent, 0x0000032c, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
CXG(CX_CLSID_SynchronizeContainer, 0x0000032d, 0, 0, 0xc0,0,0,0,0,0,0,0x46);
/* An IID no object implements, for negative QueryInterface tests. */
CXG(CX_IID_Bogus, 0x6f1a0c01, 0x3e5b, 0x4d27, 0x9a,0x10,0x4c,0x77,0x0b,0x2e,0x91,0xd3);
/* Our own EXTENTID for the channel hook tests. */
CXG(CX_EXTENT_OLET, 0x6f1a0c02, 0x3e5b, 0x4d27, 0x9a,0x10,0x4c,0x77,0x0b,0x2e,0x91,0xd4);

#define CX_HR_NOTINIT    ((HRESULT)0x800401F0L)
#define CX_HR_NOTSUPP    ((HRESULT)0x80004021L)
#define CX_HR_NOINTERFACE ((HRESULT)0x80004002L)
#define CX_HR_POINTER    ((HRESULT)0x80004003L)
#define CX_HR_FAIL       ((HRESULT)0x80004005L)
#define CX_HR_INVALIDARG ((HRESULT)0x80070057L)
#define CX_HR_OUTOFMEM   ((HRESULT)0x8007000EL)
#define CX_HR_CALLPENDING ((HRESULT)0x80010115L)
#define CX_HR_CALLCOMPLETE ((HRESULT)0x80010117L)
#define CX_HR_NOCONTEXT  ((HRESULT)0x8001011EL)
#define CX_HR_NOSYNC     ((HRESULT)0x80010120L)
#define CX_HR_CANCELDISABLED ((HRESULT)0x80010140L)
#define CX_HR_CALLREJECTED ((HRESULT)0x80010001L)
#define CX_HR_CALLCANCELED ((HRESULT)0x80010002L)
#define CX_HR_CHANGEDMODE ((HRESULT)0x80010106L)
#define CX_HR_RETRYLATER ((HRESULT)0x8001010AL)

#define UU(name) UIN("user32.dll", name)
static const GUID CX_GUID_NULL = { 0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
UDECL(HRESULT, CoCreateObjectInContext, (IUnknown *punk, IUnknown *ctx, REFIID riid, void **ppv));
UDECL(HRESULT, CoPushServiceDomain, (IUnknown *ctx));
UDECL(HRESULT, CoPopServiceDomain, (IUnknown **ctx));
#define CXM_NONE  0xFFFFFFFFu   /* thread does not call CoInitializeEx */
#define CXM_STA   COINIT_APARTMENTTHREADED
#define CXM_MTA   COINIT_MULTITHREADED

/* ---- COM entry points through GetProcAddress ---- */
HRESULT cx_init(DWORD model);       /* CoInitializeEx(NULL, model) */
void    cx_uninit(void);            /* CoUninitialize */
#define CX_FAILED(hr) ((HRESULT)(hr) < 0)

/* Run fn(arg) on a fresh thread. model is CXM_*; the thread calls CoInitializeEx before and CoUninitialize after.
 * Waits at most 10 s; returns 1 when fn completed, 0 on timeout (a failed CHECK is recorded). */
typedef void (*cx_fn)(void *);
int cx_run(DWORD model, cx_fn fn, void *arg);
/* Same, but returns the CoInitializeEx result in *init_hr (and does not run fn if it failed). */
int cx_run_hr(DWORD model, cx_fn fn, void *arg, HRESULT *init_hr);

/* Pump messages on the calling thread until h is signaled or ms elapse. Returns 1 if h signaled. */
int cx_pump_until(HANDLE h, DWORD ms);
void cx_pump_some(DWORD ms);          /* pump for about ms */
HANDLE cx_event(BOOL manual, BOOL initial);
DWORD cx_wait(HANDLE h, DWORD ms);

/* ---- mock: generic IUnknown / ICancelMethodCalls object ---- */
typedef struct CxObj {
    const void *lpVtbl;
    LONG ref;
    LONG qi_calls, cancel_calls, test_calls;
    ULONG last_cancel_secs;
    HRESULT cancel_ret, test_ret;
    DWORD cancel_tid;
    int qi_all;                      /* QI succeeds for every IID */
    int qi_cancel;                   /* QI succeeds for ICancelMethodCalls */
    HANDLE on_cancel;                /* event set when Cancel is called, may be NULL */
} CxObj;
void cx_obj_init(CxObj *o, int qi_cancel, int qi_all);
static inline IUnknown *cx_obj_unk(CxObj *o) { return (IUnknown *)o; }

/* ---- mock: IMessageFilter ---- */
typedef struct CxFilter {
    const void *lpVtbl;
    LONG ref;
    LONG incoming, retry, pending, qi_calls;
    DWORD last_calltype, last_caller, last_tick;
    INTERFACEINFO last_info; int have_info;
    DWORD ret_incoming;              /* SERVERCALL_* */
    DWORD ret_incoming_seq[8]; int seq_len;  /* if seq_len>0 use seq[n]; last repeated */
    DWORD ret_retry;
    DWORD last_reject_type, last_retry_callee;
    DWORD ret_pending;
    DWORD incoming_tid;
} CxFilter;
void cx_filter_init(CxFilter *f);

/* ---- mock: IClassFactory used as a trivially marshalable object ---- */
typedef struct CxCF {
    const void *lpVtbl;
    LONG ref;
    LONG lock_calls, create_calls;
    LONG locks;
    DWORD last_tid;
    HANDLE entered;                  /* signaled when LockServer entered, may be NULL */
    HANDLE release;                  /* if non-NULL, LockServer blocks on it (<= 9 s) */
    HRESULT lock_ret;
} CxCF;
void cx_cf_init(CxCF *c);

/* ---- a server thread that owns a CxCF in an apartment and pumps messages (STA) ---- */
typedef struct CxSrv {
    DWORD model;                     /* CXM_STA or CXM_MTA */
    CxFilter *filter;                /* registered on the server thread when STA, may be NULL */
    CxCF cf;
    HANDLE thread, ready, stop;
    DWORD tid;
    IStream *stream;
    HRESULT init_hr, marshal_hr, filter_hr;
    void (*on_start)(struct CxSrv *);  /* runs on the server thread after COM init, may be NULL */
    void *user;
} CxSrv;
int  cx_srv_start(CxSrv *s, DWORD model, CxFilter *filter);   /* returns 1 when ready */
IClassFactory *cx_srv_proxy(CxSrv *s);                         /* unmarshal in the calling thread (once) */
void cx_srv_stop(CxSrv *s);

/* ---- a parked thread: initializes COM, records its identity, then waits (pumping when STA) until stopped ---- */
typedef struct CxPark {
    DWORD model, tid;
    HANDLE thread, ready, stop;
    HRESULT init_hr, apt_hr, tok_hr, lid_hr;
    int type, qual;
    ULONG_PTR token;
    GUID lid;
    void *cb, *info, *objctx;   /* AddRef'd IContextCallback / IComThreadingInfo / IObjContext of the thread's context */
    HRESULT cb_hr, info_hr, obj_hr;
} CxPark;
int  cx_park_start(CxPark *p, DWORD model);   /* returns 1 when the thread is up and COM init succeeded (or CXM_NONE) */
void cx_park_stop(CxPark *p);

/* Child-process failure counting */
extern int g_cx_cf;
#define CK(c) do { int ok_ = !!(c); if (!ok_) g_cx_cf++; t_check(ok_, __FILE__, __LINE__, #c, NULL); } while (0)
#define CXEQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define CXNE(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#define CKEQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) g_cx_cf++; \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define CXEQ_HR(a, b) do { unsigned long a_ = (unsigned long)(a), b_ = (unsigned long)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%08lx want 0x%08lx", a_, b_); } while (0)
#define CKEQ_HR(a, b) do { unsigned long a_ = (unsigned long)(a), b_ = (unsigned long)(b); if (a_ != b_) g_cx_cf++; \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%08lx want 0x%08lx", a_, b_); } while (0)

static inline int cx_is_sta_type(int t) { return t == APTTYPE_STA || t == APTTYPE_MAINSTA; }
#endif
