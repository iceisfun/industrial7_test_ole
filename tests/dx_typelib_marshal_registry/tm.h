/* Shared declarations for the dx_typelib_marshal_registry area.
 *
 * State-changing: registers a typelib built at run time (in t_tmpdir) under HKCU\Software\Classes (or the
 * hive RegisterTypeLib picks), then exercises typelib-driven standard marshaling (PSOAInterface /
 * PSDispatch) of interfaces implemented in this exe. All registry keys written belong to the fixed test
 * GUIDs below; they are swept before and after.
 */
#ifndef TM_H
#define TM_H
#define INITGUID
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>
#include <olectl.h>
#include <stdlib.h>
#include <wchar.h>

/* ------------------------------------------------------------------ GUIDs (fixed, test-owned) */
#define TMG(name, d1) static const GUID name = {d1, 0x7e57, 0x4a11, {0x9c, 0x11, 0x01, 0xa1, 0xc0, 0xde, 0x00, 0x01}}
TMG(TM_LIBID_Main,  0x0b1d0001);   /* main marshaling library */
TMG(TM_LIBID_Mini,  0x0b1d0002);   /* small libraries for registry tests */
TMG(TM_LIBID_Ver,   0x0b1d0003);   /* version-rule tests */
TMG(TM_LIBID_Lcid,  0x0b1d0004);   /* lcid-rule tests */
TMG(TM_LIBID_Other, 0x0b1d0005);
TMG(TM_IID_IOletSink,   0x0b1d0010);
TMG(TM_IID_IOletCustom, 0x0b1d0011);
TMG(TM_IID_IOletDual,   0x0b1d0012);
TMG(TM_IID_IOletDisp,   0x0b1d0013);   /* pure dispinterface */
TMG(TM_IID_IOletPlain,  0x0b1d0014);   /* TKIND_INTERFACE without oleautomation/dual */
TMG(TM_IID_IOletProxyFlag, 0x0b1d0015);/* oleautomation + TYPEFLAG_FPROXY */
TMG(TM_IID_IOletDualNE, 0x0b1d0016);   /* dual + nonextensible */
TMG(TM_CLSID_OletCo,    0x0b1d0020);
TMG(TM_GUID_OletPoint,  0x0b1d0021);
TMG(TM_GUID_OletMix,    0x0b1d0022);
TMG(TM_IID_Mini1,   0x0b1d0030);        /* interface of the Mini/Ver/Lcid libraries */
TMG(TM_IID_Mini2,   0x0b1d0031);
TMG(TM_IID_Mini3,   0x0b1d0032);
TMG(TM_IID_Mini4,   0x0b1d0033);
TMG(TM_IID_Mini5,   0x0b1d0034);
TMG(TM_IID_Mini6,   0x0b1d0035);
TMG(TM_IID_Mini7,   0x0b1d0036);
TMG(TM_IID_Mini8,   0x0b1d0037);
TMG(TM_IID_MiniDisp,0x0b1d0038);
TMG(TM_IID_MiniDual,0x0b1d0039);
TMG(TM_IID_MiniXu1, 0x0b1d003a);        /* cross-use: marshal-before/after registration tests */
TMG(TM_IID_MiniXu2, 0x0b1d003b);
TMG(TM_IID_MiniXu3, 0x0b1d003c);
TMG(TM_IID_MiniXu4, 0x0b1d003d);
static const GUID TM_IID_ISupportErrorInfo = {0xDF0B3D60, 0x548F, 0x101B, {0x8E, 0x65, 0x08, 0x00, 0x2B, 0x2B, 0xD1, 0x19}};
static const GUID TM_IID_NULL = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
static const GUID TM_CLSID_PSOAInterface = {0x00020424, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID TM_CLSID_PSDispatch    = {0x00020420, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID TM_CLSID_PSEnumVariant = {0x00020421, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};

UDECL(HRESULT, RegisterTypeLibForUser, (ITypeLib *, OLECHAR *, OLECHAR *));
UDECL(HRESULT, UnRegisterTypeLibForUser, (REFGUID, WORD, WORD, LCID, SYSKIND));
#define pRegisterTypeLibForUser UN(RegisterTypeLibForUser)
#define pUnRegisterTypeLibForUser UN(UnRegisterTypeLibForUser)

#define TM_MAIN_MAJ 11   /* 0xb: lets the tests tell hex from decimal in "maj.min" key names */
#define TM_MAIN_MIN 26   /* 0x1a */

/* ------------------------------------------------------------------ interfaces implemented in this exe */
typedef LONG OletColor;
enum { OLET_RED = 1, OLET_GREEN = 2, OLET_BLUE = 4 };
typedef struct OletPoint { LONG x, y; } OletPoint;
typedef struct OletMix { BSTR name; DOUBLE d; LONG id; OletPoint pt; } OletMix;

#define TM_CALL STDMETHODCALLTYPE
typedef struct IOletSink IOletSink;
struct IOletSinkVtbl {
    HRESULT (TM_CALL *QueryInterface)(IOletSink *, REFIID, void **);
    ULONG (TM_CALL *AddRef)(IOletSink *);
    ULONG (TM_CALL *Release)(IOletSink *);
    HRESULT (TM_CALL *Notify)(IOletSink *, LONG n, BSTR s);
    HRESULT (TM_CALL *Total)(IOletSink *, LONG *total);
};
struct IOletSink { const struct IOletSinkVtbl *lpVtbl; };

typedef struct IOletCustom IOletCustom;
struct IOletCustomVtbl {
    HRESULT (TM_CALL *QueryInterface)(IOletCustom *, REFIID, void **);
    ULONG (TM_CALL *AddRef)(IOletCustom *);
    ULONG (TM_CALL *Release)(IOletCustom *);
    HRESULT (TM_CALL *AddLong)(IOletCustom *, LONG a, LONG b, LONG *r);
    HRESULT (TM_CALL *ScalarsIn)(IOletCustom *, BYTE b, SHORT s, LONG l, LONGLONG h, FLOAT f, DOUBLE d, VARIANT_BOOL vb, DATE dt);
    HRESULT (TM_CALL *ScalarsOut)(IOletCustom *, BYTE *b, SHORT *s, LONG *l, LONGLONG *h, FLOAT *f, DOUBLE *d, VARIANT_BOOL *vb, DATE *dt);
    HRESULT (TM_CALL *EchoBstr)(IOletCustom *, BSTR in, BSTR *out);
    HRESULT (TM_CALL *BstrOut)(IOletCustom *, BSTR *out);
    HRESULT (TM_CALL *BstrInOut)(IOletCustom *, BSTR *io);
    HRESULT (TM_CALL *EchoVariant)(IOletCustom *, VARIANT v, VARIANT *out);
    HRESULT (TM_CALL *VariantInOut)(IOletCustom *, VARIANT *io);
    HRESULT (TM_CALL *SumArray)(IOletCustom *, SAFEARRAY *sa, LONG *sum);
    HRESULT (TM_CALL *MakeArray)(IOletCustom *, LONG n, SAFEARRAY **out);
    HRESULT (TM_CALL *EchoBstrArray)(IOletCustom *, SAFEARRAY *in, SAFEARRAY **out);
    HRESULT (TM_CALL *GetSink)(IOletCustom *, IOletSink **pp);
    HRESULT (TM_CALL *PutSink)(IOletCustom *, IOletSink *p);
    HRESULT (TM_CALL *EchoUnk)(IOletCustom *, IUnknown *in, IUnknown **out);
    HRESULT (TM_CALL *EchoPoint)(IOletCustom *, OletPoint p, OletPoint *out);
    HRESULT (TM_CALL *SwapPoint)(IOletCustom *, OletPoint *io);
    HRESULT (TM_CALL *EchoMix)(IOletCustom *, OletMix *in, OletMix *out);
    HRESULT (TM_CALL *EchoColor)(IOletCustom *, OletColor c, OletColor *out);
    HRESULT (TM_CALL *ReturnHr)(IOletCustom *, LONG hr);
    HRESULT (TM_CALL *FailWithOut)(IOletCustom *, LONG hr, BSTR *pb, LONG *pl, IOletSink **pp);
    HRESULT (TM_CALL *RaiseError)(IOletCustom *, BSTR desc, LONG hr);
    HRESULT (TM_CALL *Callback)(IOletCustom *, IOletSink *sink, LONG n, LONG *total);
    HRESULT (TM_CALL *ApartmentInfo)(IOletCustom *, LONG *tid, LONG *pid, LONG *apttype);
    HRESULT (TM_CALL *LongPtrs)(IOletCustom *, LONG *in, LONG *out, LONG *inout);
    HRESULT (TM_CALL *CurrencyEcho)(IOletCustom *, CY c, CY *r);
};
struct IOletCustom { const struct IOletCustomVtbl *lpVtbl; };
enum { OC_FIRST = 3, OC_AddLong = 3, OC_ScalarsIn, OC_ScalarsOut, OC_EchoBstr, OC_BstrOut, OC_BstrInOut, OC_EchoVariant,
       OC_VariantInOut, OC_SumArray, OC_MakeArray, OC_EchoBstrArray, OC_GetSink, OC_PutSink, OC_EchoUnk, OC_EchoPoint,
       OC_SwapPoint, OC_EchoMix, OC_EchoColor, OC_ReturnHr, OC_FailWithOut, OC_RaiseError, OC_Callback, OC_ApartmentInfo,
       OC_LongPtrs, OC_CurrencyEcho, OC_COUNT };

typedef struct IOletDual IOletDual;
struct IOletDualVtbl {
    HRESULT (TM_CALL *QueryInterface)(IOletDual *, REFIID, void **);
    ULONG (TM_CALL *AddRef)(IOletDual *);
    ULONG (TM_CALL *Release)(IOletDual *);
    HRESULT (TM_CALL *GetTypeInfoCount)(IOletDual *, UINT *);
    HRESULT (TM_CALL *GetTypeInfo)(IOletDual *, UINT, LCID, ITypeInfo **);
    HRESULT (TM_CALL *GetIDsOfNames)(IOletDual *, REFIID, LPOLESTR *, UINT, LCID, DISPID *);
    HRESULT (TM_CALL *Invoke)(IOletDual *, DISPID, REFIID, LCID, WORD, DISPPARAMS *, VARIANT *, EXCEPINFO *, UINT *);
    HRESULT (TM_CALL *Add)(IOletDual *, LONG a, LONG b, LONG *r);                 /* id 1 */
    HRESULT (TM_CALL *Concat)(IOletDual *, BSTR a, BSTR b, BSTR *r);              /* id 2 */
    HRESULT (TM_CALL *get_Value)(IOletDual *, VARIANT *r);                        /* id 3 propget */
    HRESULT (TM_CALL *put_Value)(IOletDual *, VARIANT v);                         /* id 3 propput */
    HRESULT (TM_CALL *get_Count)(IOletDual *, LONG *r);                           /* id 4 propget */
    HRESULT (TM_CALL *Fail)(IOletDual *, LONG hr);                                /* id 5 */
    HRESULT (TM_CALL *ItemName)(IOletDual *, LONG idx, BSTR *r);                  /* id 6 */
    HRESULT (TM_CALL *RaiseError)(IOletDual *, BSTR desc, LONG hr);               /* id 7 */
    HRESULT (TM_CALL *Sum)(IOletDual *, SAFEARRAY *sa, LONG *r);                  /* id 8 */
    HRESULT (TM_CALL *get__NewEnum)(IOletDual *, IUnknown **r);                   /* id -4 */
};
struct IOletDual { const struct IOletDualVtbl *lpVtbl; };
enum { OD_Add = 1, OD_Concat = 2, OD_Value = 3, OD_Count = 4, OD_Fail = 5, OD_ItemName = 6, OD_RaiseError = 7, OD_Sum = 8 };

typedef struct IOletMini IOletMini;
struct IOletMiniVtbl {
    HRESULT (TM_CALL *QueryInterface)(IOletMini *, REFIID, void **);
    ULONG (TM_CALL *AddRef)(IOletMini *);
    ULONG (TM_CALL *Release)(IOletMini *);
    HRESULT (TM_CALL *Ping)(IOletMini *, LONG x, LONG *r);    /* *r = x + 1000 */
};
struct IOletMini { const struct IOletMiniVtbl *lpVtbl; };

/* what the server side last received (valid after the synchronous call returned) */
typedef struct TMSrvLast {
    BYTE b; SHORT s; LONG l; LONGLONG h; FLOAT f; DOUBLE d; VARIANT_BOOL vb; CY cy; DATE dt;
    LONG nsink; LONG in_long; LONG inout_long_before; LONG point_x, point_y;
    WCHAR str[64]; ULONG strlen_; int str_was_null;
    USHORT vt; LONG vlong; LONG calls; DWORD tid;
} TMSrvLast;
extern TMSrvLast g_last;

/* server object constructors; all return an object with refcount 1 */
IOletCustom *tm_new_custom(void);
IOletSink *tm_new_sink(void);
IOletDual *tm_new_dual(void);
IOletMini *tm_new_mini(const GUID *iid);
LONG tm_live_objects(void);            /* live server objects (leak check) */
LONG tm_sink_total(IOletSink *s);      /* direct read (same apartment only) */
void tm_custom_set_peek(IOletCustom *c, IUnknown **hold);

/* ------------------------------------------------------------------ typelib builder (tm_lib.c) */
typedef struct TMLibSpec {
    GUID libid; WORD maj, min; LCID lcid; WORD libflags; SYSKIND syskind;
    const WCHAR *name; const WCHAR *doc; const WCHAR *helpfile;
    GUID iid; const WCHAR *ifname; WORD ifflags; TYPEKIND ifkind; int dual_func;   /* mini interface */
} TMLibSpec;
HRESULT tm_build_main(const WCHAR *path);
HRESULT tm_build_mini(const WCHAR *path, const TMLibSpec *spec);
void tm_spec_init(TMLibSpec *s, const GUID *libid, WORD maj, WORD min, LCID lcid, const GUID *iid);
const WCHAR *tm_invoke_path(void);     /* unrestricted unregistered copy of the main library, for ITypeInfo::Invoke */
const WCHAR *tm_main_path(void);       /* built on demand in t_tmpdir; NULL on failure */
const WCHAR *tm_mini_path(const char *leaf, const TMLibSpec *spec);   /* build a mini lib file; NULL on failure */

/* ------------------------------------------------------------------ fixture / registry helpers (tm_fix.c) */
int  tm_begin(void);                   /* sweep, probe HKCU write, register main lib (ForUser). 0 + t_skip when impossible */
int  tm_begin_bare(void);              /* sweep + probe only */
void tm_end(void);                     /* unregister + sweep */
void tm_sweep(void);
HRESULT tm_register_main(void);
#define TMTEST(n) static void body_##n(void); \
    TEST(n) { if (!tm_begin()) return; body_##n(); tm_end(); } \
    static void body_##n(void)
#define TMTEST_BARE(n) static void body_##n(void); \
    TEST(n) { if (!tm_begin_bare()) return; body_##n(); tm_end(); } \
    static void body_##n(void)

void tm_guid_str(const GUID *g, WCHAR out[40]);
void tm_ver_str(WORD maj, WORD min, WCHAR out[16]);
/* raw registry readers; root: HKEY_CURRENT_USER (Software\Classes\ prefix added) or HKEY_CLASSES_ROOT */
/* Where the *ForUser functions write: HKCU on Windows. Wine's implementation writes the merged class root instead. */
static inline HKEY tm_ur(void) { return t_is_wine() ? HKEY_CLASSES_ROOT : HKEY_CURRENT_USER; }
#define TM_HKCU HKEY_CURRENT_USER
#define TM_HKLM HKEY_LOCAL_MACHINE
#define TM_HKCR HKEY_CLASSES_ROOT
LONG tm_reg_sz(HKEY root, const WCHAR *path, const WCHAR *value, WCHAR *out, DWORD cch, REGSAM wow);   /* ERROR_* */
int  tm_reg_key_exists(HKEY root, const WCHAR *path, REGSAM wow);
int  tm_reg_subkey_count(HKEY root, const WCHAR *path, REGSAM wow);
int  tm_reg_value_type(HKEY root, const WCHAR *path, const WCHAR *value, REGSAM wow);    /* REG_* or -1 */
/* path helpers: root-relative paths (HKCU needs Software\Classes\ prefix, HKCR does not) */
void tm_tl_path(HKEY root, const GUID *libid, WORD maj, WORD min, const WCHAR *tail, WCHAR *out, size_t cch);   /* tail like L"0\\win64" or NULL */
void tm_if_path(HKEY root, const GUID *iid, const WCHAR *tail, WCHAR *out, size_t cch);
int  tm_hkcu_has_lib(const GUID *libid);
int  tm_hkcr_has_lib(const GUID *libid);
int  tm_hkcr_has_iface(const GUID *iid);

/* ------------------------------------------------------------------ apartments / threads (tm_run.c) */
typedef void (*tm_client_fn)(void *iface, void *ctx);
typedef struct TMCfg { DWORD srv_apt, cli_apt; const char *name; } TMCfg;
extern const TMCfg tm_cfgs[3];         /* srv STA/cli MTA, srv MTA/cli STA, srv STA/cli STA */
typedef IUnknown *(*tm_new_fn)(void);
/* Run `fn` in a fresh client thread (apartment cli_apt) against an object created on this thread. This thread is
 * (co)initialized with srv_apt for the duration. Returns 1 when the client finished, 0 on timeout/failure. */
int tm_run_cfg(const TMCfg *cfg, tm_new_fn mk, const GUID *iid, tm_client_fn fn, void *ctx);
/* Same, for every config; stops at the first failing run. */
int tm_run_all(tm_new_fn mk, const GUID *iid, tm_client_fn fn, void *ctx);
/* Run `fn` in a worker (apartment `apt`), with an object already marshaled by the caller into `stm`. */
int tm_run_thread(DWORD apt, IStream *stm, const GUID *iid, tm_client_fn fn, void *ctx, DWORD ms);
extern volatile LONG tm_client_hr;     /* HRESULT of CoGetInterfaceAndReleaseStream in the last run */
void tm_pump_wait(HANDLE h, DWORD ms); /* waits for h pumping messages */
IUnknown *tm_mk_custom(void);
IUnknown *tm_mk_dual(void);
IUnknown *tm_mk_sink(void);
BOOL tm_is_proxy_call(IOletCustom *c);  /* ApartmentInfo reports a different thread than the caller */

/* Every oleaut32 call goes through GetProcAddress: these shadow the SDK prototypes. */
#define SysAllocString(...) (U(SysAllocString)(__VA_ARGS__))
#define SysAllocStringLen(...) (U(SysAllocStringLen)(__VA_ARGS__))
#define SysFreeString(...) (U(SysFreeString)(__VA_ARGS__))
#define SysStringLen(...) (U(SysStringLen)(__VA_ARGS__))
#define VariantInit(...) (U(VariantInit)(__VA_ARGS__))
#define VariantClear(...) (U(VariantClear)(__VA_ARGS__))
#define VariantCopy(...) (U(VariantCopy)(__VA_ARGS__))
#define SafeArrayCreateVector(...) (U(SafeArrayCreateVector)(__VA_ARGS__))
#define SafeArrayPutElement(...) (U(SafeArrayPutElement)(__VA_ARGS__))
#define SafeArrayGetElement(...) (U(SafeArrayGetElement)(__VA_ARGS__))
#define SafeArrayGetLBound(...) (U(SafeArrayGetLBound)(__VA_ARGS__))
#define SafeArrayGetUBound(...) (U(SafeArrayGetUBound)(__VA_ARGS__))
#define SafeArrayAccessData(...) (U(SafeArrayAccessData)(__VA_ARGS__))
#define SafeArrayUnaccessData(...) (U(SafeArrayUnaccessData)(__VA_ARGS__))
#define SafeArrayCopy(...) (U(SafeArrayCopy)(__VA_ARGS__))
#define SafeArrayDestroy(...) (U(SafeArrayDestroy)(__VA_ARGS__))
#define SafeArrayGetDim(...) (U(SafeArrayGetDim)(__VA_ARGS__))
#define SafeArrayGetVartype(...) (U(SafeArrayGetVartype)(__VA_ARGS__))
#define SafeArrayCreate(...) (U(SafeArrayCreate)(__VA_ARGS__))
#define VariantChangeType(...) (U(VariantChangeType)(__VA_ARGS__))
#define GetErrorInfo(...) (U(GetErrorInfo)(__VA_ARGS__))
#define SetErrorInfo(...) (U(SetErrorInfo)(__VA_ARGS__))
#define CreateErrorInfo(...) (U(CreateErrorInfo)(__VA_ARGS__))
#define VariantTimeToSystemTime(...) (U(VariantTimeToSystemTime)(__VA_ARGS__))
#define SysStringByteLen(...) (U(SysStringByteLen)(__VA_ARGS__))
#define SysAllocStringByteLen(...) (U(SysAllocStringByteLen)(__VA_ARGS__))

/* misc */
static inline int tm_weq(const WCHAR *a, const WCHAR *b) { return a && b && !lstrcmpW(a, b); }
static inline int tm_wieq(const WCHAR *a, const WCHAR *b) { return a && b && !lstrcmpiW(a, b); }
BOOL tm_file_exists(const WCHAR *p);
#define HR(x) ((long)(x))
/* single-evaluation equality (EXPECT_EQ evaluates its operands twice, which breaks calls with side effects) */
static inline void tm_eq(long long got, long long want, const char *f, int line, const char *expr) {
    int ok = got == want || ((got >= -2147483648LL && got <= 4294967295LL) && (want >= -2147483648LL && want <= 4294967295LL) && (unsigned)got == (unsigned)want);
    t_check(ok, f, line, expr, "got 0x%llx want 0x%llx", (unsigned long long)got, (unsigned long long)want);
}
#define EQ(a, b) tm_eq((long long)(ULONG_PTR)(a), (long long)(ULONG_PTR)(b), __FILE__, __LINE__, #a " == " #b)
#endif
