/* Shared helpers for the usrmarshal_oleaut32 area (implemented in ums_util.c).
 *
 * The oleaut32 wire helpers (BSTR_User*, VARIANT_User*, LPSAFEARRAY_User*, and their *64 NDR64 siblings)
 * are called through GetProcAddress with a hand-built USER_MARSHAL_CB / MIDL_STUB_MESSAGE / RPC_MESSAGE.
 *
 * Behaviour notes taken from the Windows 10 decompilation (see the per-test comments):
 *  - NdrGetUserMarshalInfo (called by all *Unmarshal helpers) needs umcb.Signature == 'USRC', CBType 1 or 2,
 *    stub.RpcMsg->Buffer/BufferLength and stub.Buffer inside that range; the unmarshalers bound-check against
 *    [stub.Buffer, rpc.Buffer + rpc.BufferLength].
 *  - Malformed input raises an RPC exception (RpcRaiseException): RPC_X_BAD_STUB_DATA 0x6f7, etc.
 */
#ifndef UMS_H
#define UMS_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <rpc.h>
#include <rpcndr.h>
#include <setjmp.h>

#define UMS_SIG        0x55535243u   /* "USRC" */
#define RPC_X_BAD_STUB_DATA_   0x6f7u
#define RPC_X_NULL_REF_POINTER_ 0x6f4u
#define E_INVALIDARG_X   0x80070057u
#define E_OUTOFMEMORY_X  0x8007000eu
#define DISP_E_BADVARTYPE_X 0x80020008u
#define DISP_E_ARRAYISLOCKED_X 0x8002000du
#define DISP_E_BADCALLEE_X  0x80020010u
#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((ULONG)(a) - 1))
#define ARRAY_N(a)       (sizeof(a) / sizeof((a)[0]))

enum { UF_BSTR = 0, UF_VARIANT = 1, UF_SAFEARRAY = 2 };

/* One marshaling context: buffer + stub message + callback struct. */
typedef struct UCtx {
    RPC_MESSAGE rpc;
    MIDL_STUB_MESSAGE stub;
    USER_MARSHAL_CB umcb;
    unsigned char *raw;   /* allocation */
    unsigned char *buf;   /* 16-byte aligned start of the buffer */
    ULONG len;            /* usable length (bytes), canary follows */
} UCtx;

#define UMS_CANARY 64
/* Allocate a 0xCC-filled buffer of len bytes (NULL buffer when len==0 and type is size) and set up the structs. */
void uctx_init(UCtx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type);
void uctx_free(UCtx *c);
/* After a marshal: re-aim the same buffer at unmarshal. (len = bytes valid). */
void uctx_reread(UCtx *c, ULONG len, ULONG mshctx);
/* Copy raw wire bytes into a fresh aligned buffer (for hand-made wire data) ready to unmarshal. */
void uctx_wire(UCtx *c, const void *wire, ULONG len, ULONG mshctx);
/* True if the canary bytes after the buffer are untouched. */
int  uctx_canary_ok(const UCtx *c);

/* Resolved entry points. All return 0/NULL when the export is missing. */
int  u_have(int fam, int v64);
#define NEED_FAM(fam) do { if (!u_have(fam, 0)) { t_skip("missing user-marshal exports"); return; } } while (0)

/* tests of Windows-only behaviour (Wine lacks the oleaut32 shield checks / NULL tolerance) */
#define WINE_SKIP(why) do { if (t_is_wine()) { t_skip("emulator differs: " why); return; } } while (0)

/* Loop guard: NDR64 helpers are skipped (silently) when the module lacks them */
#define HAVE_V(fam) if (u_have(fam, v))

/* Exception-catching wrappers. exc == NULL: an exception is a test failure; otherwise *exc gets the code (0 = none). */
ULONG u_size(UCtx *c, int fam, int v64, ULONG start, void *obj, DWORD *exc);
unsigned char *u_marshal(UCtx *c, int fam, int v64, unsigned char *at, void *obj, DWORD *exc);
unsigned char *u_unmarshal(UCtx *c, int fam, int v64, unsigned char *at, void *obj, DWORD *exc);
void u_free(UCtx *c, int fam, int v64, void *obj, DWORD *exc);

/* LPSAFEARRAY_Marshal / LPSAFEARRAY_Unmarshal (the exported non-"User" names) */
unsigned char *u_sa_marshal_exp(UCtx *c, unsigned char *at, LPSAFEARRAY *p, DWORD *exc);
unsigned char *u_sa_unmarshal_exp(UCtx *c, unsigned char *at, LPSAFEARRAY *p, DWORD *exc);

/* Compare wire bytes against an expected image. mask[i] != 0 means "don't care". */
void ums_check_wire(const unsigned char *got, const unsigned char *exp, const unsigned char *mask, ULONG n,
                    const char *file, int line, const char *what);
#define CHECK_WIRE(g, e, m, n, what) ums_check_wire((g), (e), (m), (n), __FILE__, __LINE__, (what))

/* single-evaluation integer equality (EXPECT_EQ evaluates its arguments twice) */
void ums_eq(long long got, long long want, const char *file, int line, const char *expr);
#define EQ(a, b) ums_eq((long long)(a), (long long)(b), __FILE__, __LINE__, #a " == " #b)
#define EQU(a, b) ums_eq((long long)(unsigned long long)(ULONG)(a), (long long)(unsigned long long)(ULONG)(b), __FILE__, __LINE__, #a " == " #b)

/* unaligned little-endian readers/writers on wire images */
static inline unsigned int  rd32(const unsigned char *p) { unsigned int v; memcpy(&v, p, 4); return v; }
static inline unsigned short rd16(const unsigned char *p) { unsigned short v; memcpy(&v, p, 2); return v; }
static inline unsigned long long rd64(const unsigned char *p) { unsigned long long v; memcpy(&v, p, 8); return v; }
static inline void wr32(unsigned char *p, unsigned int v) { memcpy(p, &v, 4); }
static inline void wr16(unsigned char *p, unsigned short v) { memcpy(p, &v, 2); }
static inline void wr64(unsigned char *p, unsigned long long v) { memcpy(p, &v, 8); }

/* ---- expected-wire models (ums_util.c) ----------------------------------------------------------------------
 * Build the wire image the Windows 10 helpers produce for a VARIANT / SAFEARRAY marshaled at a buffer position
 * `base` that is 8-byte aligned (offset 0 of the image). `out` is pre-filled with 0xCC by the caller (padding is
 * never written by the helpers); mask bytes set to 1 mark pointer-derived values the model cannot predict.
 * Return the end offset, or 0 when the VT is not representable by the model. */
ULONG ums_model_bstr(int v64, BSTR b, ULONG pos, unsigned char *out, unsigned char *mask);
ULONG ums_model_variant(int v64, const VARIANT *v, ULONG pos, unsigned char *out, unsigned char *mask);
ULONG ums_model_safearray(int v64, SAFEARRAY *psa, ULONG pos, unsigned char *out, unsigned char *mask);
/* Predicted UserSize results (decomp formulas) */
ULONG ums_size_bstr(int v64, BSTR b, ULONG start);
ULONG ums_size_safearray(int v64, SAFEARRAY *psa, ULONG start);

/* SF_TYPE discriminant (GetSafeArrayDiscr with UDT support) */
ULONG ums_sftype(SAFEARRAY *psa);

/* Simple IUnknown test object (QI only IUnknown) */
typedef struct UObj { IUnknown iface; LONG refs; } UObj;
UObj *uobj_new(void);
#define UOBJ_UNK(o) (&(o)->iface)

/* VARIANT helpers */
void ums_vfill(VARIANT *v, VARTYPE vt, const void *payload, size_t n, unsigned short r1, unsigned short r2, unsigned short r3);

/* contexts used by table tests */
extern const ULONG ums_ctxs[3];
/* Every oleaut32/ole32 service goes through GetProcAddress as well (nothing is linked). */
#define SysAllocString(...) U(SysAllocString)(__VA_ARGS__)
#define SysAllocStringLen(...) U(SysAllocStringLen)(__VA_ARGS__)
#define SysAllocStringByteLen(...) U(SysAllocStringByteLen)(__VA_ARGS__)
#define SysFreeString(...) U(SysFreeString)(__VA_ARGS__)
#define SysStringLen(...) U(SysStringLen)(__VA_ARGS__)
#define SysStringByteLen(...) U(SysStringByteLen)(__VA_ARGS__)
#define SafeArrayCreate(...) U(SafeArrayCreate)(__VA_ARGS__)
#define SafeArrayCreateEx(...) U(SafeArrayCreateEx)(__VA_ARGS__)
#define SafeArrayDestroy(...) U(SafeArrayDestroy)(__VA_ARGS__)
#define SafeArrayAllocDescriptor(...) U(SafeArrayAllocDescriptor)(__VA_ARGS__)
#define SafeArrayAllocDescriptorEx(...) U(SafeArrayAllocDescriptorEx)(__VA_ARGS__)
#define SafeArrayAllocData(...) U(SafeArrayAllocData)(__VA_ARGS__)
#define SafeArrayDestroyData(...) U(SafeArrayDestroyData)(__VA_ARGS__)
#define SafeArrayDestroyDescriptor(...) U(SafeArrayDestroyDescriptor)(__VA_ARGS__)
#define SafeArrayAccessData(...) U(SafeArrayAccessData)(__VA_ARGS__)
#define SafeArrayUnaccessData(...) U(SafeArrayUnaccessData)(__VA_ARGS__)
#define SafeArrayLock(...) U(SafeArrayLock)(__VA_ARGS__)
#define SafeArrayUnlock(...) U(SafeArrayUnlock)(__VA_ARGS__)
#define SafeArrayGetElement(...) U(SafeArrayGetElement)(__VA_ARGS__)
#define SafeArrayPutElement(...) U(SafeArrayPutElement)(__VA_ARGS__)
#define SafeArrayGetDim(...) U(SafeArrayGetDim)(__VA_ARGS__)
#define SafeArrayGetLBound(...) U(SafeArrayGetLBound)(__VA_ARGS__)
#define SafeArrayGetUBound(...) U(SafeArrayGetUBound)(__VA_ARGS__)
#define SafeArrayGetVartype(...) U(SafeArrayGetVartype)(__VA_ARGS__)
#define SafeArrayGetIID(...) U(SafeArrayGetIID)(__VA_ARGS__)
#define SafeArraySetIID(...) U(SafeArraySetIID)(__VA_ARGS__)
#define SafeArrayGetElemsize(...) U(SafeArrayGetElemsize)(__VA_ARGS__)
#define SafeArrayCopy(...) U(SafeArrayCopy)(__VA_ARGS__)
#define VariantInit(...) U(VariantInit)(__VA_ARGS__)
#define VariantClear(...) U(VariantClear)(__VA_ARGS__)
#define VariantCopy(...) U(VariantCopy)(__VA_ARGS__)
#define CoTaskMemAlloc(...) U(CoTaskMemAlloc)(__VA_ARGS__)
#define CoTaskMemFree(...) U(CoTaskMemFree)(__VA_ARGS__)
#define CoInitializeEx(...) U(CoInitializeEx)(__VA_ARGS__)
#define CoUninitialize(...) U(CoUninitialize)(__VA_ARGS__)
#define CoGetMalloc(...) U(CoGetMalloc)(__VA_ARGS__)
#endif
