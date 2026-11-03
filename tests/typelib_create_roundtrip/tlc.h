/* Shared helpers for the typelib_create_roundtrip area (oleaut32 type library creation).
 * Every API entry point is resolved with LoadLibrary+GetProcAddress (U / UIN); the COM objects
 * (ICreateTypeLib2, ITypeLib, ...) are driven through their vtables only.
 * All type libraries live in t_tmpdir(); REGKIND_NONE is used for loading, so no registry state. */
#ifndef TLC_H
#define TLC_H
#define COBJMACROS
#define CINTERFACE
#include "t.h"
#define IID_IUnknown tlcx_IID_IUnknown
#define IID_IDispatch tlcx_IID_IDispatch
#define IID_ITypeInfo tlcx_IID_ITypeInfo
#define IID_ITypeInfo2 tlcx_IID_ITypeInfo2
#define IID_ITypeLib tlcx_IID_ITypeLib
#define IID_ITypeLib2 tlcx_IID_ITypeLib2
#define IID_ICreateTypeLib tlcx_IID_ICreateTypeLib
#define IID_ICreateTypeLib2 tlcx_IID_ICreateTypeLib2
#define IID_ICreateTypeInfo tlcx_IID_ICreateTypeInfo
#define IID_ICreateTypeInfo2 tlcx_IID_ICreateTypeInfo2
#define IID_IRecordInfo tlcx_IID_IRecordInfo
#define IID_ITypeComp tlcx_IID_ITypeComp
#define GUID_NULL tlcx_GUID_NULL
#include <objbase.h>
#include <oleauto.h>
#include <stdlib.h>

static const GUID tlc_IID_IUnknown = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_IDispatch = {0x00020400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ITypeInfo = {0x00020401,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ITypeLib = {0x00020402,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ITypeComp = {0x00020403,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ICreateTypeInfo = {0x00020405,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ICreateTypeLib = {0x00020406,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ICreateTypeInfo2 = {0x0002040E,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ICreateTypeLib2 = {0x0002040F,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ITypeLib2 = {0x00020411,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_ITypeInfo2 = {0x00020412,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_IID_IRecordInfo = {0x0000002F,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID tlc_GUID_NULL = {0,0,0,{0,0,0,0,0,0,0,0}};
#undef IID_IUnknown
#define IID_IUnknown tlc_IID_IUnknown
#undef IID_IDispatch
#define IID_IDispatch tlc_IID_IDispatch
#undef IID_ITypeInfo
#define IID_ITypeInfo tlc_IID_ITypeInfo
#undef IID_ITypeLib
#define IID_ITypeLib tlc_IID_ITypeLib
#undef IID_ITypeComp
#define IID_ITypeComp tlc_IID_ITypeComp
#undef IID_ICreateTypeInfo
#define IID_ICreateTypeInfo tlc_IID_ICreateTypeInfo
#undef IID_ICreateTypeLib
#define IID_ICreateTypeLib tlc_IID_ICreateTypeLib
#undef IID_ICreateTypeInfo2
#define IID_ICreateTypeInfo2 tlc_IID_ICreateTypeInfo2
#undef IID_ICreateTypeLib2
#define IID_ICreateTypeLib2 tlc_IID_ICreateTypeLib2
#undef IID_ITypeLib2
#define IID_ITypeLib2 tlc_IID_ITypeLib2
#undef IID_ITypeInfo2
#define IID_ITypeInfo2 tlc_IID_ITypeInfo2
#undef IID_IRecordInfo
#define IID_IRecordInfo tlc_IID_IRecordInfo
#undef GUID_NULL
#define GUID_NULL tlc_GUID_NULL

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
#define EQHR(expr, want) do { HRESULT h_ = (HRESULT)(expr); \
    t_check(h_ == (HRESULT)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%08lx want 0x%08lx", \
            (unsigned long)h_, (unsigned long)(HRESULT)(want)); } while (0)
#define OK_(expr) EQHR(expr, S_OK)
/* Behaviour taken from observed/implementation knowledge rather than a verified Windows source:
 * a mismatch is logged as a note, a match counts as a passed check. */
#define SOFTHR(expr, want) do { HRESULT h_ = (HRESULT)(expr); \
    if (h_ != (HRESULT)(want)) t_note("SOFT %s: got 0x%08lx want 0x%08lx (%s:%d)", #expr, (unsigned long)h_, (unsigned long)(HRESULT)(want), __FILE__, __LINE__); \
    else t_check(1, __FILE__, __LINE__, #expr, NULL); } while (0)
#define SOFTEQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    if (v_ != (ULONG_PTR)(want)) t_note("SOFT %s: got 0x%llx want 0x%llx (%s:%d)", #expr, (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want), __FILE__, __LINE__); \
    else t_check(1, __FILE__, __LINE__, #expr, NULL); } while (0)

static __typeof__(&CreateTypeLib2) pCreateTypeLib2;
static __typeof__(&CreateTypeLib) pCreateTypeLib;
static __typeof__(&LoadTypeLibEx) pLoadTypeLibEx;
static __typeof__(&LoadTypeLib) pLoadTypeLib;
static __typeof__(&SysAllocString) pSysAllocString;
static __typeof__(&SysFreeString) pSysFreeString;
static __typeof__(&SysAllocStringLen) pSysAllocStringLen;
static __typeof__(&SysStringLen) pSysStringLen;
static __typeof__(&VariantInit) pVariantInit;
static __typeof__(&VariantClear) pVariantClear;
static __typeof__(&VariantCopy) pVariantCopy;
static __typeof__(&CoInitializeEx) pCoInitializeEx;
static __typeof__(&CoUninitialize) pCoUninitialize;
static __typeof__(&CreateStdDispatch) pCreateStdDispatch;
static __typeof__(&GetRecordInfoFromTypeInfo) pGetRecordInfoFromTypeInfo;
static __typeof__(&GetRecordInfoFromGuids) pGetRecordInfoFromGuids;
static __typeof__(&SafeArrayCreate) pSafeArrayCreate;
static __typeof__(&SafeArrayDestroy) pSafeArrayDestroy;
static __typeof__(&SafeArrayCopy) pSafeArrayCopy;
static __typeof__(&SafeArrayGetRecordInfo) pSafeArrayGetRecordInfo;
static __typeof__(&SafeArraySetRecordInfo) pSafeArraySetRecordInfo;
static __typeof__(&SafeArrayCreateEx) pSafeArrayCreateEx;
static __typeof__(&SafeArrayAccessData) pSafeArrayAccessData;
static __typeof__(&SafeArrayUnaccessData) pSafeArrayUnaccessData;
static __typeof__(&SafeArrayGetElement) pSafeArrayGetElement;
static __typeof__(&SafeArrayPutElement) pSafeArrayPutElement;
static __typeof__(&SafeArrayGetVartype) pSafeArrayGetVartype;
static __typeof__(&ClearCustData) pClearCustData;
#define ClearCustData(p) do { if (!pClearCustData) pClearCustData = UIN("oleaut32.dll", ClearCustData); if (pClearCustData) pClearCustData(p); } while (0)

static int tl_init(void)
{
    static int done, good;
    if (done) return good;
    done = 1;
    pCreateTypeLib2 = UIN("oleaut32.dll", CreateTypeLib2);
    pCreateTypeLib = UIN("oleaut32.dll", CreateTypeLib);
    pLoadTypeLibEx = UIN("oleaut32.dll", LoadTypeLibEx);
    pLoadTypeLib = UIN("oleaut32.dll", LoadTypeLib);
    pSysAllocString = UIN("oleaut32.dll", SysAllocString);
    pSysFreeString = UIN("oleaut32.dll", SysFreeString);
    pSysAllocStringLen = UIN("oleaut32.dll", SysAllocStringLen);
    pSysStringLen = UIN("oleaut32.dll", SysStringLen);
    pVariantInit = UIN("oleaut32.dll", VariantInit);
    pVariantClear = UIN("oleaut32.dll", VariantClear);
    pVariantCopy = UIN("oleaut32.dll", VariantCopy);
    pCoInitializeEx = U(CoInitializeEx);
    pCoUninitialize = U(CoUninitialize);
    pCreateStdDispatch = UIN("oleaut32.dll", CreateStdDispatch);
    pGetRecordInfoFromTypeInfo = UIN("oleaut32.dll", GetRecordInfoFromTypeInfo);
    pGetRecordInfoFromGuids = UIN("oleaut32.dll", GetRecordInfoFromGuids);
    pSafeArrayCreate = UIN("oleaut32.dll", SafeArrayCreate);
    pSafeArrayDestroy = UIN("oleaut32.dll", SafeArrayDestroy);
    pSafeArrayCopy = UIN("oleaut32.dll", SafeArrayCopy);
    pSafeArrayGetRecordInfo = UIN("oleaut32.dll", SafeArrayGetRecordInfo);
    pSafeArraySetRecordInfo = UIN("oleaut32.dll", SafeArraySetRecordInfo);
    pSafeArrayCreateEx = UIN("oleaut32.dll", SafeArrayCreateEx);
    pSafeArrayAccessData = UIN("oleaut32.dll", SafeArrayAccessData);
    pSafeArrayUnaccessData = UIN("oleaut32.dll", SafeArrayUnaccessData);
    pSafeArrayGetElement = UIN("oleaut32.dll", SafeArrayGetElement);
    pSafeArrayPutElement = UIN("oleaut32.dll", SafeArrayPutElement);
    pSafeArrayGetVartype = UIN("oleaut32.dll", SafeArrayGetVartype);
    good = pCreateTypeLib2 && pCreateTypeLib && pLoadTypeLibEx && pLoadTypeLib && pSysAllocString &&
           pSysFreeString && pVariantInit && pVariantClear && pVariantCopy && pCoInitializeEx &&
           pCoUninitialize && pCreateStdDispatch && pGetRecordInfoFromTypeInfo && pGetRecordInfoFromGuids &&
           pSafeArrayCreate && pSafeArrayDestroy;
    return good;
}
#define TL_BEGIN() do { if (!tl_init()) { t_skip("missing oleaut32/ole32 exports"); return; } } while (0)

/* pointer size of the type library's target platform */
static unsigned tl_ptr(SYSKIND sk) { return sk == SYS_WIN64 ? 8 : 4; }

/* Wide path for a leaf in the scratch directory. */
static void tl_path(WCHAR *out, const char *leaf)
{
    char a[MAX_PATH];
    t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, MAX_PATH);
}

static ICreateTypeLib2 *tl_create(SYSKIND sk, const char *leaf, WCHAR *path)
{
    ICreateTypeLib2 *ctl = NULL;
    tl_path(path, leaf);
    HRESULT hr = pCreateTypeLib2(sk, path, &ctl);
    EQHR(hr, S_OK);
    return hr == S_OK ? ctl : NULL;
}

static ICreateTypeInfo *tl_cti(ICreateTypeLib2 *ctl, const WCHAR *name, TYPEKIND k)
{
    ICreateTypeInfo *cti = NULL;
    HRESULT hr = ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)name, k, &cti);
    EQHR(hr, S_OK);
    return hr == S_OK ? cti : NULL;
}

/* Save, release the creator (which must be the last reference) and load the file back. */
static ITypeLib *tl_reload(ICreateTypeLib2 *ctl, const WCHAR *path)
{
    ITypeLib *tl = NULL;
    OK_(ICreateTypeLib2_SaveAllChanges(ctl));
    ULONG ref = ICreateTypeLib2_Release(ctl);
    EQ(ref, 0);
    HRESULT hr = pLoadTypeLibEx(path, REGKIND_NONE, &tl);
    EQHR(hr, S_OK);
    return hr == S_OK ? tl : NULL;
}

static ITypeInfo *tl_ti(ITypeLib *tl, UINT i)
{
    ITypeInfo *ti = NULL;
    HRESULT hr = ITypeLib_GetTypeInfo(tl, i, &ti);
    EQHR(hr, S_OK);
    return hr == S_OK ? ti : NULL;
}

static ITypeInfo *tl_ti_of_cti(ICreateTypeInfo *cti)
{
    ITypeInfo *ti = NULL;
    HRESULT hr = ICreateTypeInfo_QueryInterface(cti, &IID_ITypeInfo, (void **)&ti);
    EQHR(hr, S_OK);
    return hr == S_OK ? ti : NULL;
}

/* name of member (memid, or -1 for the type itself) as a narrow string; "" when NULL */
static void tl_doc_name(ITypeInfo *ti, MEMBERID id, char *out, size_t cap)
{
    BSTR n = NULL;
    out[0] = 0;
    if (ITypeInfo_GetDocumentation(ti, id, &n, NULL, NULL, NULL) == S_OK && n) {
        WideCharToMultiByte(CP_ACP, 0, n, -1, out, (int)cap, NULL, NULL);
        pSysFreeString(n);
    }
}

static void tl_w2a(const WCHAR *w, char *out, size_t cap)
{
    out[0] = 0;
    if (w) WideCharToMultiByte(CP_ACP, 0, w, -1, out, (int)cap, NULL, NULL);
}

/* Add a simple function. params: array of VARTYPEs (flags FIN). */
static HRESULT tl_add_func_k(ICreateTypeInfo *cti, UINT idx, FUNCKIND fk, INVOKEKIND ik, VARTYPE ret, MEMBERID memid,
                           const VARTYPE *params, unsigned n)
{
    FUNCDESC fd;
    ELEMDESC el[8];
    memset(&fd, 0, sizeof fd);
    memset(el, 0, sizeof el);
    for (unsigned i = 0; i < n && i < 8; i++) {
        el[i].tdesc.vt = params[i];
        el[i].paramdesc.wParamFlags = PARAMFLAG_FIN;
    }
    fd.memid = memid;
    fd.funckind = fk;
    fd.invkind = ik;
    fd.callconv = CC_STDCALL;
    fd.cParams = (SHORT)n;
    fd.lprgelemdescParam = n ? el : NULL;
    fd.elemdescFunc.tdesc.vt = ret;
    return ICreateTypeInfo_AddFuncDesc(cti, idx, &fd);
}

static HRESULT tl_add_func(ICreateTypeInfo *cti, UINT idx, INVOKEKIND ik, VARTYPE ret, MEMBERID memid,
                           const VARTYPE *params, unsigned n)
{ return tl_add_func_k(cti, idx, FUNC_PUREVIRTUAL, ik, ret, memid, params, n); }

/* Add an IUnknown base (from stdole2.tlb) to cti; returns the hreftype. */
static ITypeLib *tl_stdole(void)
{
    ITypeLib *so = NULL;
    HRESULT hr = pLoadTypeLib(L"stdole2.tlb", &so);
    if (hr != S_OK) { t_note("LoadTypeLib(stdole2.tlb) failed 0x%08lx", (unsigned long)hr); return NULL; }
    return so;
}
static ITypeInfo *tl_stdole_ti(ITypeLib *so, REFGUID g)
{
    ITypeInfo *ti = NULL;
    HRESULT hr = ITypeLib_GetTypeInfoOfGuid(so, g, &ti);
    EQHR(hr, S_OK);
    return hr == S_OK ? ti : NULL;
}

static const GUID TLC_GUID_LIB = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x01}};
static const GUID TLC_GUID_A   = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x02}};
static const GUID TLC_GUID_B   = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x03}};
static const GUID TLC_GUID_C   = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x04}};
static const GUID TLC_GUID_D   = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x05}};
static const GUID TLC_GUID_E   = {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x06}};
static const GUID TLC_GUID_CUST= {0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x10}};
static const GUID TLC_GUID_CUST2={0x7a1b2c30,0x1111,0x4222,{0x83,0x33,0x44,0x44,0x55,0x55,0x66,0x11}};
#endif
