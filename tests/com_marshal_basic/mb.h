/* Shared helpers for the com_marshal_basic area (implemented in mb_util.c). */
#ifndef MB_H
#define MB_H
#define INITGUID
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <ole2.h>

/* ---- GUIDs not provided by the headers ---------------------------------- */
extern const GUID MB_CLSID_Custom;        /* our custom-marshal unmarshaler class (registered with CoRegisterClassObject) */
extern const GUID MB_CLSID_Handler;       /* class id returned by our IStdMarshalInfo */
extern const GUID MB_IID_Bogus;           /* an interface nobody registered */
extern const GUID MB_CLSID_StdMarshal;    /* 00000017-0000-0000-C000-000000000046 */
extern const GUID MB_CLSID_StdGIT;        /* 00000323-0000-0000-C000-000000000046 */
extern const GUID MB_CLSID_InProcFTM;     /* 0000033A-0000-0000-C000-000000000046 */

/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice). 32-bit values compare modulo sign extension. */
static inline void mb_eq(long long got, long long want, const char *f, int line, const char *expr) {
    int ok = got == want || ((got >= -2147483648LL && got <= 4294967295LL) && (want >= -2147483648LL && want <= 4294967295LL) && (unsigned)got == (unsigned)want);
    t_check(ok, f, line, expr, "got 0x%llx want 0x%llx", (unsigned long long)got, (unsigned long long)want);
}
#define EQ(a, b) mb_eq((long long)(ULONG_PTR)(a), (long long)(ULONG_PTR)(b), __FILE__, __LINE__, #a " == " #b)

/* ---- COM init helpers ----------------------------------------------------- */
#define MB_INIT_STA() do { HRESULT hr_i = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); CHECKF(hr_i == S_OK, "CoInitializeEx STA hr=0x%lx", hr_i); } while (0)
#define MB_INIT_MTA() do { HRESULT hr_i = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED); CHECKF(hr_i == S_OK, "CoInitializeEx MTA hr=0x%lx", hr_i); } while (0)
#define MB_DONE()     U(CoUninitialize)()

/* ---- test object: IClassFactory + IExternalConnection ---------------------- */
typedef struct MBObj {
    IClassFactory cf;
    IExternalConnection ec;
    LONG ref;            /* starts at 1 (the static owner reference) */
    LONG locks;          /* IClassFactory::LockServer(TRUE) - LockServer(FALSE) */
    LONG ext;            /* strong external connections */
    LONG ext_weak;       /* weak external connections */
    LONG add_conn, rel_conn;
    BOOL last_close;     /* fLastReleaseCloses seen in the last ReleaseConnection */
    BOOL with_ec;        /* expose IExternalConnection through QueryInterface */
    DWORD last_tid;      /* thread that executed the last LockServer/CreateInstance */
    LONG ci_calls;
    IUnknown *marshal_inner; /* if set, QueryInterface(IID_IMarshal) is forwarded here (aggregation of the free-threaded marshaler) */
    DWORD qi_extconn;    /* number of QI(IID_IExternalConnection) calls */
} MBObj;
extern MBObj g_obj[2];
void mb_obj_reset(int with_ec);                 /* resets both objects (ref back to 1) */
#define MB_UNK(i) ((IUnknown *)&g_obj[i].cf)
#define MB_CF(i)  (&g_obj[i].cf)

/* ---- instrumented stream wrapper around a CreateStreamOnHGlobal stream ------- */
typedef struct MBStm {
    IStream s;
    IStream *inner;
    LONG ref;
    LONG reads, writes, seeks, stats, clones, other;
    ULONG last_read_cb, last_write_cb;
    BOOL destroyed;
    BOOL fail_read;      /* make Read return STG_E_ACCESSDENIED */
    BOOL fail_write;
    ULONG short_read_max; /* if nonzero, Read returns at most this many bytes */
} MBStm;
MBStm *mb_wstream_new(void);                     /* ref 1, wraps a fresh HGLOBAL stream; caller Release()s */

/* ---- custom IMarshal ---------------------------------------------------------- */
enum { EV_GUC = 1, EV_GMSM, EV_MI, EV_UI, EV_RMD, EV_DISC, EV_SMI_GCFH, EV_CTOR_CREATE };
typedef struct MBEv { int kind; int which; GUID iid; DWORD ctx; DWORD flags; void *pv; void *dctx; } MBEv;
extern MBEv g_ev[128];
extern int g_nev;
void mb_ev_reset(void);
int  mb_ev_count(int kind);
int  mb_ev_index(int kind, int nth);             /* index of nth (0-based) event of kind, or -1 */

typedef struct MBCustomCfg {
    GUID clsid;                  /* returned by GetUnmarshalClass */
    ULONG size;                  /* returned by GetMarshalSizeMax */
    BYTE payload[256];
    ULONG plen;                  /* bytes written by MarshalInterface */
    HRESULT hr_guc, hr_gmsm, hr_mi, hr_ui, hr_rmd, hr_disc;
    HRESULT hr_factory;          /* hr of class factory CreateInstance */
    void *unmarshaled_obj;       /* what UnmarshalInterface hands back (IUnknown) */
    int fail_riid_check;
} MBCustomCfg;
extern MBCustomCfg g_cm_cfg;
void mb_cm_reset(void);          /* default: class MB_CLSID_Custom, size 16, 16 byte payload "0123456789ABCDEF", all S_OK */
IUnknown *mb_cm_unk(void);       /* the static custom-marshal object, as IUnknown (exposes IMarshal) */
IMarshal *mb_cm_marshal(void);
/* class registration for the unmarshaler class (CoRegisterClassObject); returns cookie, 0 on failure */
DWORD mb_cm_register(DWORD clsctx, DWORD regcls);
void mb_cm_revoke(DWORD cookie);

/* IStdMarshalInfo object (marshals as handler for CLSID MB_CLSID_Handler) */
IUnknown *mb_smi_unk(void);
extern DWORD g_smi_last_ctx; extern int g_smi_calls; extern HRESULT g_smi_hr; extern GUID g_smi_clsid;

/* ---- stream/bytes helpers ----------------------------------------------------- */
IStream *mb_stream(void);                                  /* new HGLOBAL stream, delete on release */
void mb_rewind(IStream *s);
ULONGLONG mb_pos(IStream *s);
ULONGLONG mb_size(IStream *s);
BYTE *mb_dump(IStream *s, ULONG *len);                     /* malloc'd copy of stream contents (caller free) */

/* ---- OBJREF parsing from raw bytes ---------------------------------------------- */
static const GUID MB_ZERO = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
#define MB_OBJREF_SIG 0x574f454d
typedef struct MBObjRef {
    ULONG signature, flags;
    GUID iid;
    /* standard / handler */
    ULONG sflags, crefs;
    ULONGLONG oxid, oid;
    GUID ipid;
    GUID clsid;                 /* handler / custom */
    USHORT nentries, secoff;
    const BYTE *dsa;            /* first entry */
    ULONG cbext, csize;         /* custom */
    const BYTE *cdata; ULONG cdata_len;
    ULONG consumed;             /* bytes consumed by the header+dsa (custom: header only) */
} MBObjRef;
int mb_parse_objref(const BYTE *b, ULONG n, MBObjRef *o);  /* 1 on success */

/* ---- threads ---------------------------------------------------------------- */
typedef DWORD (CALLBACK *MBThreadFn)(void *);
HANDLE mb_thread(MBThreadFn fn, void *arg);
BOOL mb_join(HANDLE h, DWORD ms, DWORD *exitcode);        /* pumps messages while waiting; closes the handle; kills on timeout */
BOOL mb_wait_pump(HANDLE h, DWORD ms);                    /* wait for event/thread while pumping messages */

#define MB_WIN_LOCAL_TIMEOUT 10000

/* convenience */
#define MB_E_INVALIDARG ((HRESULT)0x80070057)
#define MB_E_NOINTERFACE ((HRESULT)0x80004002)
#define MB_E_NOTIMPL ((HRESULT)0x80004001)
#define MB_E_UNEXPECTED ((HRESULT)0x8000FFFF)
#define MB_RPC_E_INVALID_OBJREF ((HRESULT)0x8001011D)
#define MB_STG_E_READFAULT ((HRESULT)0x8003001E)
#define MB_CO_E_NOTINITIALIZED ((HRESULT)0x800401F0)
#define MB_REGDB_E_IIDNOTREG ((HRESULT)0x80040155)
#define MB_REGDB_E_CLASSNOTREG ((HRESULT)0x80040154)
#define MB_RPC_E_DISCONNECTED ((HRESULT)0x80010108)
#define MB_CO_E_OBJNOTCONNECTED ((HRESULT)0x800401FD)
#define MB_CO_E_OBJNOTREG ((HRESULT)0x800401FE)
#endif
