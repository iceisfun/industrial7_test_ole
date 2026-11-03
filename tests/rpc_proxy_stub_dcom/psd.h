/* Shared helpers for the rpc_proxy_stub_dcom area (rpcrt4/ole32 COM proxy-stub runtime).
 *
 * Nothing here links the DLLs under test: every rpcrt4/ole32/combase/oleaut32 entry point is reached through
 * U()/UIN()/UN().  The interfaces are described with hand-built interpreter format strings (psd_fmt.h), so
 * the whole proxy/stub pair lives in the test process:
 *
 *   IOletOne : IUnknown   { HRESULT Add([in] long a, [in] long b, [out] long *r); }
 *   IOletTwo : IUnknown   { Echo(long, long*); Str([string] const WCHAR*, long*);
 *                           GetObj(REFIID, [iid_is] IUnknown**); PutObj(IOletOne*); }
 *
 * A loopback IRpcChannelBuffer feeds the proxy buffer straight into the stub buffer and records the traffic. */
#ifndef PSD_H
#define PSD_H
#define COBJMACROS
#define USE_STUBLESS_PROXY
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>
#include <rpcproxy.h>

/* ---- GUIDs (own copies: no libuuid is linked) ----------------------------------- */
extern const GUID psd_IID_IUnknown, psd_IID_IClassFactory, psd_IID_IPSFactoryBuffer, psd_IID_IRpcProxyBuffer,
    psd_IID_IRpcStubBuffer, psd_IID_IRpcChannelBuffer, psd_IID_IDispatch, psd_IID_ITypeInfo, psd_IID_ICallFactory,
    psd_IID_IMarshal, psd_IID_IStream, psd_IID_IReleaseMarshalBuffers, psd_IID_IRpcChannelBuffer2,
    psd_IID_IRpcChannelBuffer3, psd_IID_IMultiQI, psd_IID_IClientSecurity, psd_IID_IStdIdentity;
extern const GUID psd_IID_One, psd_IID_Two, psd_CLSID_PSF, psd_IID_Unknown1, psd_IID_Unknown2, psd_CLSID_Bogus;

/* ---- the two test interfaces (C vtables) ----------------------------------------- */
typedef struct IOletOne IOletOne;
typedef struct IOletTwo IOletTwo;
typedef struct IOletOneVtbl {
    HRESULT (WINAPI *QueryInterface)(IOletOne *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IOletOne *);
    ULONG (WINAPI *Release)(IOletOne *);
    HRESULT (WINAPI *Add)(IOletOne *, LONG a, LONG b, LONG *r);
} IOletOneVtbl;
struct IOletOne { const IOletOneVtbl *lpVtbl; };
typedef struct IOletTwoVtbl {
    HRESULT (WINAPI *QueryInterface)(IOletTwo *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IOletTwo *);
    ULONG (WINAPI *Release)(IOletTwo *);
    HRESULT (WINAPI *Echo)(IOletTwo *, LONG a, LONG *r);
    HRESULT (WINAPI *Str)(IOletTwo *, const WCHAR *s, LONG *len);
    HRESULT (WINAPI *GetObj)(IOletTwo *, REFIID riid, IUnknown **pp);
    HRESULT (WINAPI *PutObj)(IOletTwo *, IOletOne *p);
} IOletTwoVtbl;
struct IOletTwo { const IOletTwoVtbl *lpVtbl; };

/* ---- server object implementing IOletOne + IOletTwo ------------------------------- */
typedef struct PsdSrv {
    IOletOne one;
    IOletTwo two;
    LONG ref;
    LONG n_add, n_echo, n_str, n_getobj, n_putobj, n_qi;
    LONG last_a, last_b;
    LONG last_strlen;
    WCHAR last_str[64];
    IOletOne *last_put;       /* pointer received by PutObj (not AddRef'd) */
    DWORD last_tid;
    BOOL raise_in_add;        /* Add raises an exception when a == 0x7fffdead */
} PsdSrv;
PsdSrv *psd_srv_new(void);                       /* ref 1 */
#define PSD_SRV_UNK(s) ((IUnknown *)&(s)->one)

/* ---- configurable IUnknown mock (server object / outer unknown) ------------------------ */
typedef struct PsdMock {
    IUnknown iface;
    LONG ref, n_qi, n_addref, n_release;
    HRESULT qi_hr;            /* returned by QueryInterface */
    int qi_self;              /* success: *ppv = this (AddRef'd); otherwise *ppv = qi_out untouched/no AddRef */
    void *qi_out;
    GUID last_iid;
    int only;                 /* when set, QueryInterface answers only IUnknown and only_iid */
    GUID only_iid;
} PsdMock;
PsdMock *psd_mock_new(void);                      /* ref 1, QI succeeds with itself */
void psd_mock_free(PsdMock *m);

/* registers psd_psf as the PS class (class object + CoRegisterPSClsid for both IIDs); needs COM initialised */
int psd_register_ps(DWORD *cookie, IPSFactoryBuffer **psf);
void psd_unregister_ps(DWORD cookie, IPSFactoryBuffer *psf);

/* ---- loopback channel -------------------------------------------------------------- */
#define PSD_LOG 512
typedef struct PsdMsgLog {
    ULONG cb;                 /* cbBuffer at the time of the call */
    ULONG iMethod;
    ULONG rep;                /* dataRepresentation */
    ULONG rpcFlags;
    GUID iid;
    int server;               /* issued from inside a stub Invoke */
} PsdMsgLog;
typedef struct PsdChan {
    IRpcChannelBuffer iface;
    LONG ref;
    IRpcStubBuffer *stub;     /* SendReceive dispatches here (not AddRef'd); NULL: E_UNEXPECTED */
    HRESULT hr_getbuf, hr_sendrecv, hr_free, hr_isconn, hr_destctx, hr_qi;
    DWORD destctx;
    int n_qi, n_getbuf, n_sendrecv, n_freebuf, n_destctx, n_isconn;
    int depth;
    PsdMsgLog gb[8]; int ngb;
    BYTE req[PSD_LOG]; ULONG req_len; ULONG req_cb_alloc;
    BYTE rep[PSD_LOG]; ULONG rep_len;
    ULONG status;
    HRESULT last_invoke_hr;
    LONG live_buffers;
    BOOL fill_server_reply_ff;
    ULONG last_raw_imethod;
} PsdChan;
PsdChan *psd_chan_new(IRpcStubBuffer *stub);     /* ref 1 */

/* ---- proxy/stub file ------------------------------------------------------------------- */
int psd_init(void);                              /* builds the vtables; 0 when rpcrt4 exports are missing */
extern CStdPSFactoryBuffer psd_psf;
extern const ProxyFileInfo *psd_file_list[2];
extern ExtendedProxyFileInfo psd_pfi;
extern CInterfaceProxyVtbl *psd_proxy_vtbl[3];
extern CInterfaceStubVtbl *psd_stub_vtbl[3];
extern MIDL_STUB_DESC psd_stub_desc;
extern MIDL_SERVER_INFO psd_server_info1;
HRESULT psd_get_factory(IPSFactoryBuffer **ppsf);        /* NdrDllGetClassObject(CLSID_PSF) */

/* a fully wired proxy + stub + channel + server for IOletOne or IOletTwo */
typedef struct PsdRig {
    IPSFactoryBuffer *psf;
    IRpcProxyBuffer *pb;
    void *iface;               /* IOletOne* or IOletTwo* proxy interface */
    IRpcStubBuffer *sb;
    PsdChan *chan;
    PsdSrv *srv;
    const GUID *iid;
    int connected;             /* proxy buffer is connected to chan (rig_down disconnects it) */
} PsdRig;
int psd_rig_up(PsdRig *r, const GUID *iid, int connect);  /* 1 on success; connect: proxy Connect(chan) */
void psd_rig_down(PsdRig *r);

/* ---- COM init helpers ---------------------------------------------------------------------- */
#define PSD_STA() do { HRESULT hr_i = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); CHECKF(hr_i == S_OK, "CoInitializeEx hr=0x%lx", hr_i); } while (0)
#define PSD_MTA() do { HRESULT hr_i = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED); CHECKF(hr_i == S_OK, "CoInitializeEx hr=0x%lx", hr_i); } while (0)
#define PSD_DONE() U(CoUninitialize)()

/* single-evaluation compare of 32-bit-ish values */
static inline void psd_eq(long long got, long long want, const char *f, int line, const char *expr) {
    int ok = got == want || ((got >= -2147483648LL && got <= 4294967295LL) && (want >= -2147483648LL && want <= 4294967295LL) && (unsigned)got == (unsigned)want);
    t_check(ok, f, line, expr, "got 0x%llx want 0x%llx", (unsigned long long)got, (unsigned long long)want);
}
#define EQ(a, b) psd_eq((long long)(ULONG_PTR)(a), (long long)(ULONG_PTR)(b), __FILE__, __LINE__, #a " == " #b)
#define EQHR(a, b) psd_eq((long long)(a), (long long)(b), __FILE__, __LINE__, #a " == " #b)

/* ---- exception capture: runs fn(arg); *code = exception code raised inside (0 when none) ---- */
typedef void (*psd_fn)(void *);
int psd_catch(psd_fn fn, void *arg, DWORD *code);   /* 1 when an exception was caught */

#define PSD_E_NOTCONNECTED ((HRESULT)0x800401FD)
#endif
