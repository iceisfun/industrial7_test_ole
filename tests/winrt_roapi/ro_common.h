/* Shared helpers for the winrt_roapi area. Everything resolves through t.h (LoadLibrary/GetProcAddress). */
#ifndef RO_COMMON_H
#define RO_COMMON_H
#include "t.h"
#include <objbase.h>
#include <inspectable.h>
#include <activation.h>
#include <roapi.h>
#include <winstring.h>

#define RO_GUID(n, a,b,c,d0,d1,d2,d3,d4,d5,d6,d7) \
    static const GUID n = { a, b, c, { d0,d1,d2,d3,d4,d5,d6,d7 } }
RO_GUID(RIID_NULLG, 0,0,0,0,0,0,0,0,0,0,0);
RO_GUID(RIID_IUnknown,           0x00000000,0x0000,0x0000,0xC0,0,0,0,0,0,0,0x46);
RO_GUID(RIID_IInspectable,       0xAF86E2E0,0xB12D,0x4C6A,0x9C,0x5A,0xD7,0xAA,0x65,0x10,0x1E,0x90);
RO_GUID(RIID_IActivationFactory, 0x00000035,0x0000,0x0000,0xC0,0,0,0,0,0,0,0x46);
RO_GUID(RIID_IAgileObject,       0x94EA2B94,0xE9CC,0x49E0,0xC0,0xFF,0xEE,0x64,0xCA,0x8F,0x5B,0x90);
RO_GUID(RIID_IAgileReference,    0xC03F6A43,0x65A4,0x9818,0x98,0x7E,0xE0,0xB8,0x10,0xD2,0xA6,0xF2);
RO_GUID(RIID_IErrorInfo,         0x1CF2B120,0x547D,0x101B,0x8E,0x65,0x08,0x00,0x2B,0x2B,0xD1,0x19);
RO_GUID(RIID_ICreateErrorInfo,   0x22F03340,0x547D,0x101B,0x8E,0x65,0x08,0x00,0x2B,0x2B,0xD1,0x19);
RO_GUID(RIID_ISupportErrorInfo,  0xDF0B3D60,0x548F,0x101B,0x8E,0x65,0x08,0x00,0x2B,0x2B,0xD1,0x19);
RO_GUID(RIID_IMarshal,           0x00000003,0x0000,0x0000,0xC0,0,0,0,0,0,0,0x46);
RO_GUID(RIID_IRestrictedErrorInfo,0x82BA7092,0x4C88,0x427D,0xA7,0xBC,0x16,0xDD,0x93,0xFE,0xB6,0x7E);
RO_GUID(RIID_IUriRuntimeClass,   0x9E365E57,0x48B2,0x4160,0x95,0x6F,0xC7,0x38,0x51,0x20,0xBB,0xFC);
RO_GUID(RIID_IUriRuntimeClassFactory,0x44A9796F,0x723E,0x4FDF,0xA2,0x18,0x03,0x3E,0x75,0xB0,0xC0,0x84);
RO_GUID(RIID_IPropertyValue,     0x4BD682DD,0x7554,0x40E9,0x9A,0x9B,0x82,0x65,0x4E,0xDE,0x7E,0x62);
RO_GUID(RIID_IPropertyValueStatics,0x629BDBC8,0xD932,0x4FF4,0x96,0xB9,0x8D,0x96,0xC5,0xC1,0xE8,0x58);
RO_GUID(RIID_IPropertySet,       0x8A43ED9F,0xF4E6,0x4421,0xAC,0xF9,0x1D,0xAB,0x29,0x86,0x82,0x0C);
RO_GUID(RIID_IBuffer,            0x905A0FE0,0xBC53,0x11DF,0x8C,0x49,0x00,0x1E,0x4F,0xC6,0x86,0xDA);
RO_GUID(RIID_IBufferFactory,     0x71AF914D,0xC10F,0x484B,0xBC,0x50,0x14,0xBC,0x62,0x3B,0x3A,0x27);
RO_GUID(RIID_IJsonObject,        0x064E24DD,0x29C2,0x4F83,0x9A,0xC1,0x9E,0xE1,0x15,0x78,0xBE,0xB3);
RO_GUID(RIID_IJsonValue,         0xA3219ECB,0xF0B3,0x4DCD,0xBE,0xEE,0x19,0xD4,0x8C,0xD3,0xED,0x1E);

#define RO_E_NOTINIT      ((HRESULT)0x800401F0L)   /* CO_E_NOTINITIALIZED */
#define RO_E_CLASSNOTREG  ((HRESULT)0x80040154L)   /* REGDB_E_CLASSNOTREG */
#define RO_E_CHANGED_MODE ((HRESULT)0x80010106L)   /* RPC_E_CHANGED_MODE */
#define RO_E_INVALIDARG   ((HRESULT)0x80070057L)
#define RO_E_POINTER      ((HRESULT)0x80004003L)
#define RO_E_NOINTERFACE  ((HRESULT)0x80004002L)
#define RO_E_API_UNAVAIL  ((HRESULT)0x80073DE1L)   /* HRESULT_FROM_WIN32(ERROR_API_UNAVAILABLE) */

/* ---- functions missing from / mis-declared in the mingw headers ---------- */
UDECL(BOOL,    RoOriginateError,   (HRESULT, HSTRING));
UDECL(BOOL,    RoOriginateErrorW,  (HRESULT, UINT32, PCWSTR));
UDECL(BOOL,    RoOriginateLanguageException, (HRESULT, HSTRING, IUnknown *));
UDECL(HRESULT, RoGetErrorReportingFlags, (UINT32 *));
UDECL(HRESULT, RoSetErrorReportingFlags, (UINT32));
UDECL(BOOL,    RoTransformError,   (HRESULT, HRESULT, HSTRING));
UDECL(BOOL,    RoTransformErrorW,  (HRESULT, HRESULT, UINT32, PCWSTR));
UDECL(HRESULT, RoCaptureErrorContext, (HRESULT));
UDECL(void,    RoClearError,       (void));
UDECL(HRESULT, GetRestrictedErrorInfo, (void **));
UDECL(HRESULT, SetRestrictedErrorInfo, (void *));
UDECL(HRESULT, RoGetMatchingRestrictedErrorInfo, (HRESULT, void **));
UDECL(HRESULT, RoGetAgileReference, (UINT, REFIID, IUnknown *, void **));
UDECL(HRESULT, RoGetApartmentIdentifier, (UINT64 *));

/* ---- minimal C vtables of the interfaces we call ------------------------- */
typedef struct IRErrInfo { const struct IRErrInfoVtbl *lpVtbl; } IRErrInfo;
struct IRErrInfoVtbl {
    HRESULT (WINAPI *QueryInterface)(IRErrInfo *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IRErrInfo *);
    ULONG (WINAPI *Release)(IRErrInfo *);
    HRESULT (WINAPI *GetErrorDetails)(IRErrInfo *, BSTR *desc, HRESULT *err, BSTR *restricted, BSTR *sid);
    HRESULT (WINAPI *GetReference)(IRErrInfo *, BSTR *ref);
};
typedef struct IAgileRef { const struct IAgileRefVtbl *lpVtbl; } IAgileRef;
struct IAgileRefVtbl {
    HRESULT (WINAPI *QueryInterface)(IAgileRef *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IAgileRef *);
    ULONG (WINAPI *Release)(IAgileRef *);
    HRESULT (WINAPI *Resolve)(IAgileRef *, REFIID, void **);
};
/* Generic IInspectable-derived object: slots 0..5 are IUnknown+IInspectable, S[n] gives raw slot n. */
typedef struct RoObj { void **vtbl; } RoObj;
#define SLOT(o, n) (((RoObj *)(o))->vtbl[n])
#define RO_UNK(o) ((IUnknown *)(o))
#define RO_RELEASE(o) do { if (o) { RO_UNK(o)->lpVtbl->Release(RO_UNK(o)); (o) = NULL; } } while (0)
typedef HRESULT (WINAPI *slot_hs_out)(void *, HSTRING *);
typedef HRESULT (WINAPI *slot_i32_out)(void *, INT32 *);
typedef HRESULT (WINAPI *slot_u32_out)(void *, UINT32 *);
typedef HRESULT (WINAPI *slot_obj_out)(void *, void **);
typedef HRESULT (WINAPI *slot_hs_in_obj_out)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *slot_i32_in_obj_out)(void *, INT32, void **);
typedef HRESULT (WINAPI *slot_u32_in_obj_out)(void *, UINT32, void **);

static inline HRESULT ro_qi(void *o, const GUID *iid, void **out) {
    return RO_UNK(o)->lpVtbl->QueryInterface(RO_UNK(o), iid, out);
}
static inline ULONG ro_refcount(void *o) {
    RO_UNK(o)->lpVtbl->AddRef(RO_UNK(o));
    return RO_UNK(o)->lpVtbl->Release(RO_UNK(o));
}
/* HSTRING helpers (resolved from combase) */
static inline HSTRING ro_hs(const WCHAR *s) {
    HSTRING h = NULL;
    HRESULT (WINAPI *f)(PCNZWCH, UINT32, HSTRING *) = (void *)U(WindowsCreateString);
    if (!f || FAILED(f(s, (UINT32)lstrlenW(s), &h))) return NULL;
    return h;
}
static inline void ro_hs_free(HSTRING h) { if (h && U(WindowsDeleteString)) U(WindowsDeleteString)(h); }
static inline const WCHAR *ro_hs_buf(HSTRING h, UINT32 *len) { return U(WindowsGetStringRawBuffer)(h, len); }
static inline int ro_hs_eq(HSTRING h, const WCHAR *s) {
    UINT32 n = 0; const WCHAR *b = h ? ro_hs_buf(h, &n) : L"";
    return b && n == (UINT32)lstrlenW(s) && !memcmp(b, s, n * sizeof(WCHAR));
}
/* Skip (under Wine) when the in-box WinRT class is not available there. */
#define RO_WINE_SKIP_IF(hr) do { if (FAILED(hr) && t_is_wine()) { t_skip("class not implemented by emulator (hr=%08lx)", (unsigned long)(hr)); return; } } while (0)

/* Run a block in a fresh thread with COM state of the callee's choosing, bounded wait. */
typedef DWORD (WINAPI *ro_thread_fn)(void *);
static inline DWORD ro_run_thread(ro_thread_fn fn, void *arg, DWORD ms) {
    HANDLE h = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!h) return 0xDEAD0000;
    DWORD w = WaitForSingleObject(h, ms), code = 0xDEAD0001;
    if (w == WAIT_OBJECT_0) GetExitCodeThread(h, &code); else TerminateThread(h, 0xDEAD0001);
    CloseHandle(h);
    return code;
}
/* Single-evaluation equality check (EXPECT_EQ in t.h evaluates its arguments twice). */
#define RO_EQ(a, b) do { long long ra_ = (long long)(a), rb_ = (long long)(b); \
    t_check(ra_ == rb_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)ra_, (unsigned long long)rb_); } while (0)
/* RO_TEST: a TEST whose body runs on a fresh thread, so each test starts with no COM state. */
#define RO_TEST(n) \
    static void rb_##n(void); \
    static DWORD WINAPI rt_##n(void *p) { rb_##n(); return 0; } \
    TEST(n) { DWORD r = ro_run_thread(rt_##n, NULL, 30000); CHECKF(r == 0, "test thread rc=%lx", r); } \
    static void rb_##n(void)
static inline HRESULT ro_apt(APTTYPE *t, APTTYPEQUALIFIER *q) {
    APTTYPE a = (APTTYPE)-99; APTTYPEQUALIFIER b = (APTTYPEQUALIFIER)-99;
    HRESULT h = U(CoGetApartmentType)(&a, &b);
    if (t) *t = a; if (q) *q = b; return h;
}
#define RO_WINE_UNIMPL(n) do { if (t_is_wine()) { t_skip("%s is an aborting stub in the emulator", n); return; } } while (0)
#endif
