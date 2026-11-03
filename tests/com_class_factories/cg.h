/* Shared scaffolding for the com_class_factories area. All COM entry points are resolved with
 * GetProcAddress (U()); no import library for ole32/combase/oleaut32/rpcrt4 is linked, so every
 * IID/CLSID used here is defined locally (cg_*). */
#ifndef CG_H
#define CG_H
#include "t.h"
#include <objbase.h>
#include <ole2.h>

/* ---- GUIDs (local, because uuid.lib is not linked) ---- */
#define CGG(name, a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) static const GUID name = {a,b,c,{d0,d1,d2,d3,d4,d5,d6,d7}}
CGG(cg_IID_IUnknown,       0x00000000,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IClassFactory,  0x00000001,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IMarshal,       0x00000003,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IStorage,       0x0000000b,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IStream,        0x0000000c,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IMoniker,       0x0000000f,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IBindCtx,       0x0000000e,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IPersist,       0x0000010c,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IPersistFile,   0x0000010b,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IPersistStorage,0x0000010a,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IPersistStream, 0x00000109,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IMultiQI,       0x00000020,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IForegroundTransfer,0x00000145,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IGlobalInterfaceTable,0x00000146,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IGlobalOptions, 0x0000015b,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IServerSecurity,0x0000013e,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_ICancelMethodCalls,0x00000029,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IDispatch,      0x00020400,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_IID_IClassFactory2, 0xb196b28f,0xbab4,0x101a,0xb6,0x9c,0x00,0xaa,0x00,0x34,0x1d,0x07);
CGG(cg_IID_IPSFactoryBuffer,0xd5f569d0,0x593b,0x101a,0xb5,0x69,0x08,0x00,0x2b,0x2d,0xbf,0x7a);
/* in-box classes */
CGG(cg_CLSID_StdMarshal,   0x00000017,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_FileMoniker,  0x00000303,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_ItemMoniker,  0x00000304,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_AntiMoniker,  0x00000305,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_PointerMoniker,0x00000306,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_CompositeMoniker,0x00000309,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_ClassMoniker, 0x0000031a,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_PSFactoryBuffer,0x00000320,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_StdGlobalInterfaceTable,0x00000323,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_ObjrefMoniker,0x00000327,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_InProcFreeMarshaler,0x0000033a,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_GlobalOptions,0x0000034b,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_StdFont,      0x0be35203,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);
CGG(cg_CLSID_StdPicture,   0x0be35204,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);
CGG(cg_CLSID_PSDispatch,   0x00020420,0,0,0xC0,0,0,0,0,0,0,0x46);
CGG(cg_CLSID_PSOAInterface,0x00020424,0,0,0xC0,0,0,0,0,0,0,0x46);
/* private test classes: random GUIDs that are not registered anywhere (variant byte last varies) */
CGG(cg_TC0, 0x6f1c9a00,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x00,0x00);
CGG(cg_TC1, 0x6f1c9a01,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x00,0x01);
CGG(cg_TC2, 0x6f1c9a02,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x00,0x02);
CGG(cg_TC3, 0x6f1c9a03,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x00,0x03);
CGG(cg_TCUNREG, 0x6f1c9aff,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x00,0xff);
/* a private interface the test object does NOT implement */
CGG(cg_IID_IPrivate, 0x6f1c9b00,0x3c1d,0x4b7e,0x9a,0x10,0x6f,0x6c,0x74,0x65,0x01,0x00);

/* CLSCTX values not (all) in the mingw headers */
#define CG_CLSCTX_NO_CODE_DOWNLOAD   0x400
#define CG_CLSCTX_NO_CUSTOM_MARSHAL  0x1000
#define CG_CLSCTX_ENABLE_CODE_DOWNLOAD 0x2000
#define CG_CLSCTX_NO_FAILURE_LOG     0x4000
#define CG_CLSCTX_DISABLE_AAA        0x8000
#define CG_CLSCTX_ENABLE_AAA         0x10000
#define CG_CLSCTX_FROM_DEFAULT_CONTEXT 0x20000
#define CG_CLSCTX_ACTIVATE_32_BIT_SERVER 0x40000
#define CG_CLSCTX_ACTIVATE_64_BIT_SERVER 0x80000
#define CG_CLSCTX_ENABLE_CLOAKING    0x100000
#define CG_CLSCTX_APPCONTAINER       0x400000
#define CG_CLSCTX_ACTIVATE_AAA_AS_IU 0x800000
#define CG_CLSCTX_ACTIVATE_ARM32_SERVER 0x2000000
#define CG_REGCLS_AGILE 0x10

#define CG_E_NOTINIT      ((HRESULT)0x800401F0)
#define CG_E_CLASSNOTREG  ((HRESULT)0x80040154)
#define CG_E_NOAGG        ((HRESULT)0x80040110)
#define CG_E_NOINTERFACE  ((HRESULT)0x80004002)
#define CG_E_INVALIDARG   ((HRESULT)0x80070057)
#define CG_E_CALLCOMPLETE ((HRESULT)0x80010117)
#define CG_E_WRONGTHREAD  ((HRESULT)0x8001010E)
#define CG_E_NOCONTEXT    ((HRESULT)0x8001011E)
#define CG_S_NOTALLINTERFACES ((HRESULT)0x00080012)

/* ---- resolved entry points (cg_api() fills them; returns 0 and t_skips when a core one is missing) ---- */
#define CG_CORE(X) X(CoInitializeEx) X(CoUninitialize) X(CoRegisterClassObject) X(CoRevokeClassObject) \
    X(CoGetClassObject) X(CoCreateInstance) X(CoCreateInstanceEx)
#define CG_DECL(n) extern __typeof__(&n) p##n;
CG_CORE(CG_DECL)
int cg_api(void);

/* ---- test class factory / object ---- */
typedef struct CGObj {
    IUnknown inner; IPersistFile pf; IPersistStorage ps;
    LONG ref; IUnknown *outer; GUID clsid;
    WCHAR file[MAX_PATH]; DWORD fmode; int fileLoaded; int stgLoaded; IStorage *stg; DWORD stgmode;
    HRESULT pfLoadHr, psLoadHr;
} CGObj;
typedef struct CGFactory {
    IClassFactory iface; LONG ref;
    LONG creates, locks, lockCalls, unlockCalls, qis;
    int aggregable;          /* 0: any pUnkOuter -> CLASS_E_NOAGGREGATION */
    int strictOuterIid;      /* outer && riid != IUnknown -> CLASS_E_NOAGGREGATION */
    HRESULT createHr;        /* forced failure when FAILED */
    GUID clsid; GUID lastIid; IUnknown *lastOuter; CGObj *lastObj;
    IUnknown *ftm;           /* optional free-threaded marshaler aggregated into the factory */
    HRESULT pfLoadHr, psLoadHr;
} CGFactory;
void cg_factory_init(CGFactory *f, const GUID *clsid);
extern LONG cg_live_objects;
ULONG cg_release(IUnknown *p);
/* initialise COM on this thread and register f; returns hr, *cookie set */
HRESULT cg_register(CGFactory *f, DWORD clsctx, DWORD regcls, DWORD *cookie);
/* run fn on a new thread, bounded 10 s wait; returns its exit code (0xDEAD0001 on timeout) */
DWORD cg_run_thread(LPTHREAD_START_ROUTINE fn, void *arg);
/* fail-fast helper for child processes: returns the line number of the first failing check */
/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice) */
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define CK(c) do { if (!(c)) return __LINE__; } while (0)
#endif
