/* variant_core: shared helpers for the oleaut32 VARIANT basics tests.
 * Every oleaut32 entry point is resolved through U() (LoadLibrary + GetProcAddress); the
 * wrappers below cache the pointer so table-driven tests stay fast. */
#ifndef VC_H
#define VC_H
#include "t.h"
#include <oaidl.h>
#include <oleauto.h>
#include <math.h>

#define HR_EQ(a, b) do { LONG hr_a_ = (LONG)(a), hr_b_ = (LONG)(b); \
    t_check(hr_a_ == hr_b_, __FILE__, __LINE__, #a " == " #b, "got 0x%08lx want 0x%08lx", (unsigned long)(ULONG)hr_a_, (unsigned long)(ULONG)hr_b_); } while (0)
#define VC_BADVT    ((HRESULT)0x80020008L)   /* DISP_E_BADVARTYPE */
#define VC_MISMATCH ((HRESULT)0x80020005L)   /* DISP_E_TYPEMISMATCH */
#define VC_OVERFLOW ((HRESULT)0x8002000AL)   /* DISP_E_OVERFLOW */
#define VC_LOCKED   ((HRESULT)0x8002000DL)   /* DISP_E_ARRAYISLOCKED */
#define VC_INVALID  ((HRESULT)0x80070057L)   /* E_INVALIDARG */
#define VC_LCID_US  MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)
#define VC_LCID_DE  MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN), SORT_DEFAULT)

/* cached typed wrappers; callers must have REQUIREd the export first (VC_REQ) */
#define VCW(ret, name, decl, args) \
    static inline ret w_##name decl { static __typeof__(&name) vcp_; if (!vcp_) vcp_ = U(name); return vcp_ args; }
VCW(void,    VariantInit,      (VARIANT *v), (v))
VCW(HRESULT, VariantClear,     (VARIANT *v), (v))
VCW(HRESULT, VariantCopy,      (VARIANT *d, VARIANT *s), (d, s))
VCW(HRESULT, VariantCopyInd,   (VARIANT *d, VARIANT *s), (d, s))
VCW(HRESULT, VariantChangeType,   (VARIANT *d, VARIANT *s, USHORT f, VARTYPE vt), (d, s, f, vt))
VCW(HRESULT, VariantChangeTypeEx, (VARIANT *d, VARIANT *s, LCID l, USHORT f, VARTYPE vt), (d, s, l, f, vt))
VCW(BSTR,    SysAllocString,   (const OLECHAR *s), (s))
VCW(BSTR,    SysAllocStringLen,(const OLECHAR *s, UINT n), (s, n))
VCW(BSTR,    SysAllocStringByteLen,(LPCSTR s, UINT n), (s, n))
VCW(void,    SysFreeString,    (BSTR b), (b))
VCW(UINT,    SysStringLen,     (BSTR b), (b))
VCW(UINT,    SysStringByteLen, (BSTR b), (b))
VCW(SAFEARRAY *, SafeArrayCreate, (VARTYPE vt, UINT n, SAFEARRAYBOUND *b), (vt, n, b))
VCW(SAFEARRAY *, SafeArrayCreateVector, (VARTYPE vt, LONG lb, ULONG n), (vt, lb, n))
VCW(HRESULT, SafeArrayDestroy, (SAFEARRAY *a), (a))
VCW(HRESULT, SafeArrayLock,    (SAFEARRAY *a), (a))
VCW(HRESULT, SafeArrayUnlock,  (SAFEARRAY *a), (a))
VCW(HRESULT, SafeArrayPutElement, (SAFEARRAY *a, LONG *i, void *p), (a, i, p))
VCW(HRESULT, SafeArrayGetElement, (SAFEARRAY *a, LONG *i, void *p), (a, i, p))
VCW(HRESULT, SafeArrayAccessData, (SAFEARRAY *a, void **pp), (a, pp))
VCW(HRESULT, SafeArrayUnaccessData, (SAFEARRAY *a), (a))
VCW(HRESULT, SafeArrayGetLBound, (SAFEARRAY *a, UINT d, LONG *p), (a, d, p))
VCW(HRESULT, SafeArrayGetUBound, (SAFEARRAY *a, UINT d, LONG *p), (a, d, p))
VCW(UINT,    SafeArrayGetDim,  (SAFEARRAY *a), (a))

#define VC_REQ() do { REQUIRE(U(VariantInit)); REQUIRE(U(VariantClear)); REQUIRE(U(VariantCopy)); \
    REQUIRE(U(VariantCopyInd)); REQUIRE(U(VariantChangeTypeEx)); REQUIRE(U(SysAllocString)); \
    REQUIRE(U(SysFreeString)); REQUIRE(U(SysStringLen)); } while (0)

static inline int vc_guid_eq(const GUID *a, const GUID *b) { return !memcmp(a, b, sizeof(GUID)); }
static const GUID vc_IID_Unknown  = {0x00000000, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID vc_IID_Dispatch = {0x00020400, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID vc_GUID_Null    = {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

/* ---- IUnknown mock (does not support IDispatch) -------------------------- */
typedef struct { const IUnknownVtbl *lpVtbl; LONG ref, nqi, nadd, nrel; } MU;
static HRESULT WINAPI mu_qi(IUnknown *i, REFIID riid, void **pp) {
    MU *m = (MU *)i; m->nqi++;
    if (vc_guid_eq(riid, &vc_IID_Unknown)) { *pp = i; m->ref++; m->nadd++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI mu_addref(IUnknown *i) { MU *m = (MU *)i; m->nadd++; return ++m->ref; }
static ULONG WINAPI mu_release(IUnknown *i) { MU *m = (MU *)i; m->nrel++; return --m->ref; }
static const IUnknownVtbl mu_vtbl = { mu_qi, mu_addref, mu_release };
static void mu_init(MU *m) { memset(m, 0, sizeof *m); m->lpVtbl = &mu_vtbl; m->ref = 1; }

/* ---- IDispatch mock: Invoke(DISPID_VALUE) returns a configurable VARIANT -- */
typedef struct {
    const IDispatchVtbl *lpVtbl; LONG ref, nqi, nadd, nrel, ninvoke;
    DISPID last_dispid; WORD last_flags; LCID last_lcid; UINT last_cargs; int last_riid_null; int last_has_result;
    HRESULT invoke_hr; VARIANT result;
} MD;
static HRESULT WINAPI md_qi(IDispatch *i, REFIID riid, void **pp) {
    MD *m = (MD *)i; m->nqi++;
    if (vc_guid_eq(riid, &vc_IID_Unknown) || vc_guid_eq(riid, &vc_IID_Dispatch)) { *pp = i; m->ref++; m->nadd++; return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI md_addref(IDispatch *i) { MD *m = (MD *)i; m->nadd++; return ++m->ref; }
static ULONG WINAPI md_release(IDispatch *i) { MD *m = (MD *)i; m->nrel++; return --m->ref; }
static HRESULT WINAPI md_gtic(IDispatch *i, UINT *p) { *p = 0; return S_OK; }
static HRESULT WINAPI md_gti(IDispatch *i, UINT n, LCID l, ITypeInfo **p) { *p = NULL; return E_NOTIMPL; }
static HRESULT WINAPI md_gids(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI md_invoke(IDispatch *i, DISPID id, REFIID riid, LCID lcid, WORD fl, DISPPARAMS *dp, VARIANT *res, EXCEPINFO *ei, UINT *ae) {
    MD *m = (MD *)i;
    m->ninvoke++; m->last_dispid = id; m->last_flags = fl; m->last_lcid = lcid;
    m->last_cargs = dp ? dp->cArgs : 0xffff; m->last_riid_null = riid && vc_guid_eq(riid, &vc_GUID_Null);
    m->last_has_result = res != NULL;
    if (m->invoke_hr < 0) return m->invoke_hr;
    if (res) w_VariantCopy(res, &m->result);
    return m->invoke_hr;
}
static const IDispatchVtbl md_vtbl = { md_qi, md_addref, md_release, md_gtic, md_gti, md_gids, md_invoke };
static void md_init(MD *m) { memset(m, 0, sizeof *m); m->lpVtbl = &md_vtbl; m->ref = 1; w_VariantInit(&m->result); }

/* ---- IRecordInfo mock ------------------------------------------------------ */
typedef struct { const IRecordInfoVtbl *lpVtbl; LONG ref, nadd, nrel, nclear, ncopy, nsize, ninit; ULONG size; PVOID last_clear, last_copy_src, last_copy_dst; } MR;
static HRESULT WINAPI mr_qi(IRecordInfo *i, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI mr_addref(IRecordInfo *i) { MR *m = (MR *)i; m->nadd++; return ++m->ref; }
static ULONG WINAPI mr_release(IRecordInfo *i) { MR *m = (MR *)i; m->nrel++; return --m->ref; }
static HRESULT WINAPI mr_init(IRecordInfo *i, PVOID p) { ((MR *)i)->ninit++; memset(p, 0, ((MR *)i)->size); return S_OK; }
static HRESULT WINAPI mr_clear(IRecordInfo *i, PVOID p) { MR *m = (MR *)i; m->nclear++; m->last_clear = p; return S_OK; }
static HRESULT WINAPI mr_copy(IRecordInfo *i, PVOID s, PVOID d) { MR *m = (MR *)i; m->ncopy++; m->last_copy_src = s; m->last_copy_dst = d; memcpy(d, s, m->size); return S_OK; }
static HRESULT WINAPI mr_guid(IRecordInfo *i, GUID *g) { *g = vc_GUID_Null; return S_OK; }
static HRESULT WINAPI mr_name(IRecordInfo *i, BSTR *b) { *b = NULL; return E_NOTIMPL; }
static HRESULT WINAPI mr_size(IRecordInfo *i, ULONG *p) { MR *m = (MR *)i; m->nsize++; *p = m->size; return S_OK; }
static HRESULT WINAPI mr_ti(IRecordInfo *i, ITypeInfo **p) { *p = NULL; return E_NOTIMPL; }
static HRESULT WINAPI mr_getf(IRecordInfo *i, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT WINAPI mr_getfnc(IRecordInfo *i, PVOID d, LPCOLESTR n, VARIANT *v, PVOID *c) { return E_NOTIMPL; }
static HRESULT WINAPI mr_putf(IRecordInfo *i, ULONG f, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT WINAPI mr_putfnc(IRecordInfo *i, ULONG f, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT WINAPI mr_names(IRecordInfo *i, ULONG *c, BSTR *n) { *c = 0; return S_OK; }
static WINBOOL WINAPI mr_match(IRecordInfo *i, IRecordInfo *o) { return i == o; }
static PVOID WINAPI mr_create(IRecordInfo *i) { return NULL; }
static HRESULT WINAPI mr_createcopy(IRecordInfo *i, PVOID s, PVOID *d) { return E_NOTIMPL; }
static HRESULT WINAPI mr_destroy(IRecordInfo *i, PVOID p) { return E_NOTIMPL; }
static const IRecordInfoVtbl mr_vtbl = { mr_qi, mr_addref, mr_release, mr_init, mr_clear, mr_copy, mr_guid, mr_name, mr_size, mr_ti,
    mr_getf, mr_getfnc, mr_putf, mr_putfnc, mr_names, mr_match, mr_create, mr_createcopy, mr_destroy };
static void mr_setup(MR *m, ULONG size) { memset(m, 0, sizeof *m); m->lpVtbl = &mr_vtbl; m->ref = 1; m->size = size; }

/* ---- misc ------------------------------------------------------------------- */
static inline int vc_dbl_eq(double a, double b) { return fabs(a - b) <= 1e-9 * (fabs(b) > 1 ? fabs(b) : 1); }
static inline int vc_is_zero24(const VARIANT *v) { static const char z[sizeof(VARIANT)]; return !memcmp(v, z, sizeof(VARIANT)); }

/* Wine/Windows-validated validity rule for VariantClear / VariantCopy (src) of a given vt value */
static inline int vc_valid_vt(unsigned vt) {
    unsigned base = vt & ~0xF000u, extra = vt & 0xF000u;
    /* vt is (base | extra); base here includes anything below 0x1000 */
    if (!(base <= 31 || base == 36 || base == 72)) return 0;
    if (base == 15 || (base >= 24 && base <= 31)) return 0;
    if ((extra & (VT_BYREF | VT_ARRAY)) && base <= VT_NULL) return 0;
    if (!(extra == 0 || extra == VT_BYREF || extra == VT_ARRAY || extra == (VT_ARRAY | VT_BYREF))) return 0;
    return 1;
}
#endif
