/* Shared helpers for the safearray_core area (oleaut32 SAFEARRAY API).
 * Every oleaut32 entry point is resolved with LoadLibrary+GetProcAddress (UIN("oleaut32.dll", Name)).
 * Test objects (IUnknown, IRecordInfo) are hand-written so the test can observe how the API drives them. */
#ifndef SA_H
#define SA_H
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <stdlib.h>

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
#define EQHR(expr, want) do { HRESULT h_ = (HRESULT)(expr); \
    t_check(h_ == (HRESULT)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%08lx want 0x%08lx", \
            (unsigned long)h_, (unsigned long)(HRESULT)(want)); } while (0)

#ifndef FADF_CREATEVECTOR
#define FADF_CREATEVECTOR 0x2000   /* internal: vector with inline data */
#endif
#define FADF_DATADELETED 0x1000    /* internal */

/* ---- resolved entry points -------------------------------------------- */
static __typeof__(&SafeArrayCreate) pCreate;
static __typeof__(&SafeArrayCreateEx) pCreateEx;
static __typeof__(&SafeArrayCreateVector) pCreateVector;
static __typeof__(&SafeArrayCreateVectorEx) pCreateVectorEx;
static __typeof__(&SafeArrayAllocDescriptor) pAllocDescriptor;
static __typeof__(&SafeArrayAllocDescriptorEx) pAllocDescriptorEx;
static __typeof__(&SafeArrayAllocData) pAllocData;
static __typeof__(&SafeArrayDestroy) pDestroy;
static __typeof__(&SafeArrayDestroyData) pDestroyData;
static __typeof__(&SafeArrayDestroyDescriptor) pDestroyDescriptor;
static __typeof__(&SafeArrayGetDim) pGetDim;
static __typeof__(&SafeArrayGetElemsize) pGetElemsize;
static __typeof__(&SafeArrayGetUBound) pGetUBound;
static __typeof__(&SafeArrayGetLBound) pGetLBound;
static __typeof__(&SafeArrayLock) pLock;
static __typeof__(&SafeArrayUnlock) pUnlock;
static __typeof__(&SafeArrayAccessData) pAccessData;
static __typeof__(&SafeArrayUnaccessData) pUnaccessData;
static __typeof__(&SafeArrayPtrOfIndex) pPtrOfIndex;
static __typeof__(&SafeArrayGetElement) pGetElement;
static __typeof__(&SafeArrayPutElement) pPutElement;
static __typeof__(&SafeArrayCopy) pCopy;
static __typeof__(&SafeArrayCopyData) pCopyData;
static __typeof__(&SafeArrayRedim) pRedim;
static __typeof__(&SafeArraySetIID) pSetIID;
static __typeof__(&SafeArrayGetIID) pGetIID;
static __typeof__(&SafeArraySetRecordInfo) pSetRecordInfo;
static __typeof__(&SafeArrayGetRecordInfo) pGetRecordInfo;
static __typeof__(&SafeArrayGetVartype) pGetVartype;
static __typeof__(&SysAllocString) pSysAllocString;
static __typeof__(&SysAllocStringLen) pSysAllocStringLen;
static __typeof__(&SysAllocStringByteLen) pSysAllocStringByteLen;
static __typeof__(&SysFreeString) pSysFreeString;
static __typeof__(&SysStringLen) pSysStringLen;
static __typeof__(&SysStringByteLen) pSysStringByteLen;
static __typeof__(&VariantInit) pVariantInit;
static __typeof__(&VariantClear) pVariantClear;
static __typeof__(&VariantCopy) pVariantCopy;

static int sa_init(void)
{
    static int done = 0, ok = 0;
    if (done) return ok;
    done = 1;
#define R(var, name) var = UIN("oleaut32.dll", name); if (!var) return 0
    R(pCreate, SafeArrayCreate); R(pCreateEx, SafeArrayCreateEx);
    R(pCreateVector, SafeArrayCreateVector); R(pCreateVectorEx, SafeArrayCreateVectorEx);
    R(pAllocDescriptor, SafeArrayAllocDescriptor); R(pAllocDescriptorEx, SafeArrayAllocDescriptorEx);
    R(pAllocData, SafeArrayAllocData); R(pDestroy, SafeArrayDestroy);
    R(pDestroyData, SafeArrayDestroyData); R(pDestroyDescriptor, SafeArrayDestroyDescriptor);
    R(pGetDim, SafeArrayGetDim); R(pGetElemsize, SafeArrayGetElemsize);
    R(pGetUBound, SafeArrayGetUBound); R(pGetLBound, SafeArrayGetLBound);
    R(pLock, SafeArrayLock); R(pUnlock, SafeArrayUnlock);
    R(pAccessData, SafeArrayAccessData); R(pUnaccessData, SafeArrayUnaccessData);
    R(pPtrOfIndex, SafeArrayPtrOfIndex); R(pGetElement, SafeArrayGetElement);
    R(pPutElement, SafeArrayPutElement); R(pCopy, SafeArrayCopy); R(pCopyData, SafeArrayCopyData);
    R(pRedim, SafeArrayRedim); R(pSetIID, SafeArraySetIID); R(pGetIID, SafeArrayGetIID);
    R(pSetRecordInfo, SafeArraySetRecordInfo); R(pGetRecordInfo, SafeArrayGetRecordInfo);
    R(pGetVartype, SafeArrayGetVartype);
    R(pSysAllocString, SysAllocString); R(pSysAllocStringLen, SysAllocStringLen);
    R(pSysAllocStringByteLen, SysAllocStringByteLen); R(pSysFreeString, SysFreeString);
    R(pSysStringLen, SysStringLen); R(pSysStringByteLen, SysStringByteLen);
    R(pVariantInit, VariantInit); R(pVariantClear, VariantClear); R(pVariantCopy, VariantCopy);
#undef R
    ok = 1;
    return 1;
}
#define SA_BEGIN() do { if (!sa_init()) { t_skip("missing oleaut32 SafeArray/Sys*/Variant* export"); return; } } while (0)

/* ---- GUIDs (declared locally: no INITGUID, no libuuid) ------------------ */
static const GUID SA_IID_IUnknown  = {0x00000000,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID SA_IID_IDispatch = {0x00020400,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID SA_IID_Custom    = {0x6f1e2d3c,0x4b5a,0x4968,{0x87,0x76,0x65,0x54,0x43,0x32,0x21,0x10}};
static const GUID SA_IID_Custom2   = {0x0badf00d,0xcafe,0xd00d,{0xfe,0xed,0xfa,0xce,0xb0,0x0c,0x12,0x34}};

/* ---- descriptor hidden-area accessors (documented by layout: allocation = 16 bytes + descriptor) ---- */
#define SA_HIDDEN_VT(psa)   (*(DWORD *)((BYTE *)(psa) - 4))
#define SA_HIDDEN_IID(psa)  ((GUID *)((BYTE *)(psa) - 16))
#define SA_HIDDEN_REC(psa)  (*(void **)((BYTE *)(psa) - 8))

/* ---- counted IUnknown-shaped test object -------------------------------- */
typedef struct TObj { const IUnknownVtbl *lpVtbl; LONG ref; LONG nAddRef, nRelease; } TObj;
static HRESULT STDMETHODCALLTYPE tobj_qi(IUnknown *This, REFIID riid, void **ppv) { *ppv = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE tobj_addref(IUnknown *This) { TObj *o = (TObj *)This; o->nAddRef++; return (ULONG)++o->ref; }
static ULONG STDMETHODCALLTYPE tobj_release(IUnknown *This) { TObj *o = (TObj *)This; o->nRelease++; return (ULONG)--o->ref; }
static const IUnknownVtbl tobj_vtbl = { tobj_qi, tobj_addref, tobj_release };
static void tobj_init(TObj *o, LONG ref) { o->lpVtbl = &tobj_vtbl; o->ref = ref; o->nAddRef = o->nRelease = 0; }
#define OBJ(o) ((IUnknown *)&(o))

/* ---- counted IRecordInfo test object ------------------------------------- */
typedef struct TRec {
    const IRecordInfoVtbl *lpVtbl;
    LONG ref;
    ULONG size;
    LONG nGetSize, nInit, nClear, nCopy;
    void *copySrc, *copyDst;      /* last RecordCopy(pvExisting, pvNew) */
    HRESULT copyHr;
    BYTE clearMark;               /* RecordClear zero-fills when set */
} TRec;
static HRESULT STDMETHODCALLTYPE trec_qi(IRecordInfo *This, REFIID riid, void **ppv) { *ppv = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE trec_addref(IRecordInfo *This) { return (ULONG)++((TRec *)This)->ref; }
static ULONG STDMETHODCALLTYPE trec_release(IRecordInfo *This) { return (ULONG)--((TRec *)This)->ref; }
static HRESULT STDMETHODCALLTYPE trec_init(IRecordInfo *This, PVOID p) { TRec *r = (TRec *)This; r->nInit++; memset(p, 0, r->size); return S_OK; }
static HRESULT STDMETHODCALLTYPE trec_clear(IRecordInfo *This, PVOID p) { TRec *r = (TRec *)This; r->nClear++; if (r->clearMark) memset(p, 0, r->size); return S_OK; }
static HRESULT STDMETHODCALLTYPE trec_copy(IRecordInfo *This, PVOID src, PVOID dst)
{ TRec *r = (TRec *)This; r->nCopy++; r->copySrc = src; r->copyDst = dst; if (SUCCEEDED(r->copyHr)) memcpy(dst, src, r->size); return r->copyHr; }
static HRESULT STDMETHODCALLTYPE trec_guid(IRecordInfo *This, GUID *g) { *g = SA_IID_Custom2; return S_OK; }
static HRESULT STDMETHODCALLTYPE trec_name(IRecordInfo *This, BSTR *b) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_size(IRecordInfo *This, ULONG *s) { TRec *r = (TRec *)This; r->nGetSize++; *s = r->size; return S_OK; }
static HRESULT STDMETHODCALLTYPE trec_ti(IRecordInfo *This, ITypeInfo **p) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_gf(IRecordInfo *This, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_gfnc(IRecordInfo *This, PVOID d, LPCOLESTR n, VARIANT *v, PVOID *p) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_pf(IRecordInfo *This, ULONG f, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_pfnc(IRecordInfo *This, ULONG f, PVOID d, LPCOLESTR n, VARIANT *v) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_gfn(IRecordInfo *This, ULONG *c, BSTR *n) { return E_NOTIMPL; }
static WINBOOL STDMETHODCALLTYPE trec_imt(IRecordInfo *This, IRecordInfo *o) { return This == o; }
static PVOID STDMETHODCALLTYPE trec_rc(IRecordInfo *This) { return NULL; }
static HRESULT STDMETHODCALLTYPE trec_rcc(IRecordInfo *This, PVOID s, PVOID *d) { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE trec_rd(IRecordInfo *This, PVOID p) { return E_NOTIMPL; }
static const IRecordInfoVtbl trec_vtbl = {
    trec_qi, trec_addref, trec_release, trec_init, trec_clear, trec_copy, trec_guid, trec_name, trec_size,
    trec_ti, trec_gf, trec_gfnc, trec_pf, trec_pfnc, trec_gfn, trec_imt, trec_rc, trec_rcc, trec_rd };
static void trec_setup(TRec *r, ULONG size)
{ memset(r, 0, sizeof *r); r->lpVtbl = &trec_vtbl; r->ref = 1; r->size = size; r->copyHr = S_OK; }
#define REC(r) ((IRecordInfo *)&(r))

/* ---- small helpers -------------------------------------------------------- */
static SAFEARRAY *sa1(VARTYPE vt, LONG lb, ULONG n)
{ SAFEARRAYBOUND b; b.lLbound = lb; b.cElements = n; return pCreate(vt, 1, &b); }
static BSTR bstr_w(const WCHAR *s) { return pSysAllocString(s); }
static const VARTYPE SA_VALID_VTS[] = { VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BSTR, VT_DISPATCH, VT_ERROR,
    VT_BOOL, VT_VARIANT, VT_UNKNOWN, VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT, 37, 38 };
#define SA_NVALID (sizeof SA_VALID_VTS / sizeof SA_VALID_VTS[0])
static ULONG sa_expect_cb(VARTYPE vt)
{
    switch (vt) {
    case VT_I2: case VT_BOOL: case VT_UI2: return 2;
    case VT_I4: case VT_R4: case VT_ERROR: case VT_UI4: case VT_INT: case VT_UINT: return 4;
    case VT_R8: case VT_CY: case VT_DATE: case VT_BSTR: case VT_DISPATCH: case VT_UNKNOWN:
    case VT_I8: case VT_UI8: case 37: case 38: return 8;
    case VT_VARIANT: return 24;
    case VT_DECIMAL: return 16;
    case VT_I1: case VT_UI1: return 1;
    }
    return 0;
}
static USHORT sa_expect_features(VARTYPE vt)   /* as created by SafeArrayCreate / CreateEx */
{
    switch (vt) {
    case VT_BSTR: return FADF_HAVEVARTYPE | FADF_BSTR;
    case VT_VARIANT: return FADF_HAVEVARTYPE | FADF_VARIANT;
    case VT_UNKNOWN: return FADF_HAVEIID | FADF_UNKNOWN;
    case VT_DISPATCH: return FADF_HAVEIID | FADF_DISPATCH;
    }
    return FADF_HAVEVARTYPE;
}
#endif
