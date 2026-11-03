/* Shared fixtures for the com_marshal_xproc area.
 *
 * Every COM/RPC entry point under test is reached through U()/UIN(); only interface
 * method calls through vtables (COBJMACROS) and trivial kernel32/user32 scaffolding are direct.
 * Test object: XpObj, an IClassFactory implementation (marshalable by the system-registered
 * proxy/stub for IClassFactory) whose methods record who called them.
 * Apartment host: XpHost, a thread that CoInitializes as STA/MTA, marshals an object into a
 * stream and then services calls (message pump for STA) until stopped.
 */
#ifndef XP_H
#define XP_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <objidl.h>
#include <oaidl.h>
#include <ocidl.h>
#include <callobj.h>

#ifndef EOAC_STATIC_CLOAKING
#define EOAC_STATIC_CLOAKING 0x20
#endif
#ifndef EOAC_DYNAMIC_CLOAKING
#define EOAC_DYNAMIC_CLOAKING 0x40
#endif

/* HRESULTs that the SDK headers may not all carry */
#define XP_RPC_E_CALL_COMPLETE      ((HRESULT)0x80010117)
#define XP_RPC_E_NO_CONTEXT         ((HRESULT)0x8001011E)
#define XP_RPC_E_CANCEL_DISABLED    ((HRESULT)0x80010140)
#define XP_RPC_E_WRONG_THREAD       ((HRESULT)0x8001010E)
#define XP_RPC_E_DISCONNECTED       ((HRESULT)0x80010108)
#define XP_RPC_E_CALL_REJECTED      ((HRESULT)0x80010001)
#define XP_RPC_E_CHANGED_MODE       ((HRESULT)0x80010106)
#define XP_CO_E_NOTINITIALIZED      ((HRESULT)0x800401F0)
#define XP_CO_E_OBJNOTCONNECTED     ((HRESULT)0x800401FD)
#define XP_CO_E_NOT_SUPPORTED       ((HRESULT)0x80004021)
#define XP_RPC_S_SERVER_UNAVAILABLE ((HRESULT)0x800706BA)
#define XP_RPC_S_CALL_FAILED        ((HRESULT)0x800706BE)
#define XP_CUSTOM_HR                ((HRESULT)0x8FFF1234)
#define XP_PREFIX "olet"

/* IIDs not provided as link-time objects (nothing here links uuid.lib): define our own. */
extern const GUID XP_IID_IClassFactory, XP_IID_IUnknown, XP_IID_IMarshal, XP_IID_IClientSecurity,
    XP_IID_IServerSecurity, XP_IID_IMultiQI, XP_IID_IStream, XP_IID_IStorage, XP_IID_IDispatch,
    XP_IID_IEnumString, XP_IID_IMessageFilter, XP_IID_IExternalConnection, XP_IID_IStdMarshalInfo,
    XP_IID_ICancelMethodCalls, XP_IID_ICallInterceptor, XP_IID_ICallFrameEvents, XP_IID_IPersist,
    XP_IID_ISequentialStream, XP_IID_IEnumUnknown, XP_IID_IEnumFORMATETC, XP_IID_IBindCtx,
    XP_IID_IMoniker, XP_IID_IParseDisplayName, XP_IID_IOleItemContainer, XP_IID_IMalloc,
    XP_IID_IProvideClassInfo, XP_IID_IRemUnknown, XP_IID_IUnknownBogus, XP_IID_IXpBogus2,
    XP_CLSID_StdMarshal, XP_CLSID_PSFactoryBuffer, XP_IID_IGlobalInterfaceTable, XP_CLSID_StdGlobalInterfaceTable,
    XP_IID_IPSFactoryBuffer, XP_IID_IRpcStubBuffer, XP_IID_IRpcProxyBuffer, XP_IID_ICallFrame,
    XP_IID_IEnumVARIANT;

#define XEQ(a, b) do { ULONG_PTR xa_ = (ULONG_PTR)(a); EXPECT_EQ(xa_, (b)); } while (0)
#define NEED(name) REQUIRE(U(name))
/* the common set every cross-apartment test needs */
#define XP_NEED_BASE() do { NEED(CoInitializeEx); NEED(CoUninitialize); NEED(CoMarshalInterface); \
    NEED(CoUnmarshalInterface); NEED(CoReleaseMarshalData); NEED(CoDisconnectObject); } while (0)

/* ---- the test object ---------------------------------------------------- */
typedef struct XpObj XpObj;
typedef void (*XpHook)(XpObj *o, int what, BOOL arg);
enum { XP_HOOK_LOCK = 1, XP_HOOK_CREATE = 2 };
struct XpObj {
    IClassFactory iface;
    volatile LONG ref;
    volatile LONG lockCalls, createCalls, qiCalls, extConnAdd, extConnRel;
    volatile LONG lastLock;          /* last fLock argument */
    volatile DWORD lastTid;          /* thread that executed the last method */
    volatile DWORD lastCallerTid;    /* CoGetCallerTID seen inside the last method */
    volatile LONG lastCallerTidHr;
    HRESULT lockHrTrue, lockHrFalse; /* defaults S_OK / S_FALSE */
    HRESULT createHr;                /* default S_OK: CreateInstance QIs itself */
    HRESULT probeHr;                 /* CreateInstance(IID_IXpBogus2) returns this, default S_OK */
    GUID lastCreateIid, lastQi;
    volatile LONG lastCreateOuterNull;
    XpHook hook;
    void *user;
    volatile LONG *destroyedFlag;    /* set to 1 when the object is freed */
    IUnknown *extra;                 /* when non-NULL, CreateInstance(IID_IUnknown) returns this instead of self */
    HRESULT hookHr;                  /* result recorded by hooks */
    IUnknown *ftm;                   /* free-threaded marshaler (aggregated), see xp_obj_enable_ftm */
    DWORD pad[4];
};
extern volatile LONG xp_live_objs;   /* XpObj currently alive */
XpObj *xp_obj_new(void);
#define XP_OBJ_UNK(o) ((IUnknown *)&(o)->iface)
#define XP_OBJ_CF(o)  (&(o)->iface)
XpObj *xp_obj_from(IUnknown *u);
/* aggregate a free-threaded marshaler: QueryInterface(IID_IMarshal) then returns the FTM */
BOOL xp_obj_enable_ftm(XpObj *o);

/* IExternalConnection variant: also implements IExternalConnection and records calls (for lock tests) */
XpObj *xp_obj_new_extconn(void);

/* ---- apartment host thread -------------------------------------------------- */
typedef void (*XpWorkFn)(void *arg);
typedef struct XpHost {
    HANDLE th; DWORD tid;
    DWORD model;                     /* COINIT_* */
    HANDLE hReady, hStop, hWork, hWorkDone, hExit;
    volatile LONG stopping;
    HRESULT initHr, marshalHr;
    IUnknown *obj; GUID iid; DWORD mshlflags, mshctx;
    IStream *stream;                 /* marshal data produced by the host */
    IMessageFilter *filter;          /* STA only: registered inside the host */
    XpWorkFn work; void *workArg;
    XpWorkFn onStop;                 /* run inside the host before CoUninitialize */
    void *onStopArg;
    int noMarshal;                   /* do not marshal anything */
    int uninitOnStop;                /* default 1 */
    int tooLong;
} XpHost;
/* Starts the host thread, waits (bounded) until it marshaled `obj`. Returns FALSE on any failure. */
BOOL xp_host_start(XpHost *h, DWORD model, IUnknown *obj, const GUID *iid, DWORD mshlflags);
BOOL xp_host_start_ex(XpHost *h, DWORD model, IUnknown *obj, const GUID *iid, DWORD mshlflags, IMessageFilter *filter);
/* Run fn(arg) inside the host apartment's thread and wait (<= 10 s). */
BOOL xp_host_run(XpHost *h, XpWorkFn fn, void *arg);
/* Ask the host to leave (runs onStop, CoUninitialize), join it. */
void xp_host_stop(XpHost *h);
/* Unmarshal the host's stream in the calling thread (rewinds first). Stream stays valid. */
HRESULT xp_host_unmarshal(XpHost *h, const GUID *iid, void **out);
/* Pump messages for up to ms (STA threads); returns when an event is signalled or time passes */
void xp_pump(DWORD ms);
BOOL xp_wait_pump(HANDLE ev, DWORD ms);   /* waits for ev while dispatching messages */

/* A tiny worker that runs fn(arg) in a fresh thread with the given COINIT model (or -1 = no CoInitialize)
 * and waits for it (<= timeout). Returns the thread's exit code (0xFFFFFFFF on timeout). */
DWORD xp_thread_run(int model, DWORD (*fn)(void *), void *arg, DWORD timeout_ms);

/* Marshal helpers */
IStream *xp_new_stream(void);
void xp_rewind(IStream *s);
ULONG xp_refcount(IUnknown *u);          /* AddRef then Release: refcount snapshot (+0) */
const char *xp_hrname(HRESULT hr);

/* ---- child process helpers ------------------------------------------------- */
/* name helper: "Local\\olet_xp_<parentpid>_<leaf>" */
void xp_evname(char *out, size_t cap, DWORD pid, const char *leaf);
HANDLE xp_event_create(DWORD pid, const char *leaf);     /* manual-reset, non-signalled */
void xp_parent_file(char *out, size_t cap, DWORD pid, const char *leaf);
BOOL xp_write_stream_to_file(IStream *s, const char *path);
HRESULT xp_stream_from_file(const char *path, IStream **out);
BOOL xp_stream_equal_bytes(IStream *a, IStream *b);

/* Server-side process: runs as CHILD(xp_server). args: <parentpid> <sta|mta> <flags-name>
 * See xp_child.c for the modes. */
typedef struct XpServer {
    PROCESS_INFORMATION pi; HANDLE hReady, hQuit, hDone;
    DWORD pid; char file[MAX_PATH * 2];
    IStream *stream;
} XpServer;
/* spawn the exe as an object server; mode string is passed to the child (see CHILD(xp_server)). */
BOOL xp_server_start(XpServer *s, const char *model, const char *mode, DWORD timeout);
HRESULT xp_server_unmarshal(XpServer *s, const GUID *iid, void **out);
void xp_server_stop(XpServer *s);    /* signals quit, waits, kills if needed */

/* ---- one-host fixture: main thread must already be CoInitialized ------------------ */
typedef struct XpFx {
    XpObj *o; XpHost h; IClassFactory *p;   /* p: proxy (or direct pointer) unmarshaled in the calling thread */
    volatile LONG gone;                     /* set when the object has been freed */
    HRESULT unmarshalHr; DWORD mainTid;
} XpFx;
/* hostModel COINIT_*; mshl MSHLFLAGS_*; unmarshal as IID_IClassFactory */
BOOL xp_fx_setup(XpFx *f, DWORD hostModel, DWORD mshl);
/* releases the proxy (if any), stops the host, then drops the test's own object reference */
void xp_fx_teardown(XpFx *f);
/* An IID that tells XpObj::CreateInstance to return exactly `hr` (works across processes). */
GUID xp_hr_iid(HRESULT hr);
#define XT(n) TEST(com_marshal_xproc_##n)

/* child-side report file: the child appends "key=hex" lines (in the parent's tmpdir), the parent reads them */
void xp_report(DWORD parentpid, const char *key, unsigned long long val);
BOOL xp_report_get(DWORD parentpid, const char *key, unsigned long long *val);
#endif
