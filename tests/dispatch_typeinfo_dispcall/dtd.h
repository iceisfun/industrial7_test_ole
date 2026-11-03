/* dispatch_typeinfo_dispcall: shared helpers.
 * Windows 10 x64 oleaut32 (decomp-verified) automation plumbing:
 *   CreateDispTypeInfo -> CDispTypeInfo (coclass wrapper + interface typeinfo), DispInvoke / DispGetIDsOfNames
 *   (thin wrappers over ITypeInfo::Invoke / GetIDsOfNames), DispCallFunc (DispCallFuncAmd64), CreateStdDispatch
 *   (CStdDisp), DispGetParam (IndexOfParam + VariantChangeTypeEx), LHashValOfNameSys[A], error info (combase).
 * Everything is called through GetProcAddress pointers pinned to oleaut32.dll (or combase.dll). */
#ifndef DTD_H
#define DTD_H
#include "t.h"
#define COBJMACROS
#include <ole2.h>
#include <oleauto.h>
#include <ocidl.h>

/* t.h's EXPECT_EQ evaluates its arguments twice; calls with side effects (Release, Get*) need single evaluation. */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)

/* Nothing from ole32/oleaut32/uuid is linked: IIDs are local copies, and BSTR/VARIANT helpers go through GetProcAddress. */
extern const GUID DI_IUnknown, DI_IDispatch, DI_ITypeInfo, DI_ITypeInfo2, DI_IErrorInfo, DI_ICreateErrorInfo,
                  DI_ISupportErrorInfo, DI_IMarshal, DI_NULL, DI_IClassFactory;
BSTR  bs_alloc(const WCHAR *s);
BSTR  bs_allocn(const WCHAR *s, UINT n);
void  bs_free(BSTR b);
UINT  bs_len(BSTR b);
void  vinit(VARIANT *v);
HRESULT vclear(VARIANT *v);
HRESULT vchange(VARIANT *dst, VARIANT *src, VARTYPE vt);
int   bs_eq(BSTR b, const WCHAR *s);   /* lstrcmpW-like equality, NULL-safe */

#define OA(name)  UIN("oleaut32.dll", name)
#define OL(name)  UIN("ole32.dll", name)

/* Error codes spelled numerically so expectations are visible. */
#define E_DISP_UNKNOWNIFACE   ((HRESULT)0x80020001)
#define E_DISP_MEMBERNOTFOUND ((HRESULT)0x80020003)
#define E_DISP_PARAMNOTFOUND  ((HRESULT)0x80020004)
#define E_DISP_TYPEMISMATCH   ((HRESULT)0x80020005)
#define E_DISP_UNKNOWNNAME    ((HRESULT)0x80020006)
#define E_DISP_BADVARTYPE     ((HRESULT)0x80020008)
#define E_DISP_OVERFLOW       ((HRESULT)0x8002000A)
#define E_DISP_BADINDEX       ((HRESULT)0x8002000B)
#define E_DISP_BADPARAMCOUNT  ((HRESULT)0x8002000E)
#define E_DISP_PARAMNOTOPT    ((HRESULT)0x8002000F)
#define E_DISP_NOTACOLLECTION ((HRESULT)0x80020011)
#define HR_E_NOINTERFACE      ((HRESULT)0x80004002)
#define HR_E_NOTIMPL          ((HRESULT)0x80004001)
#define HR_E_POINTER          ((HRESULT)0x80004003)
#define HR_E_FAIL             ((HRESULT)0x80004005)
#define HR_E_UNEXPECTED       ((HRESULT)0x8000FFFF)
#define HR_E_INVALIDARG       ((HRESULT)0x80070057)

/* ---- the demo object: a plain C object with a hand-built vtable -------- */
enum {
    DD_ADD = 0x101, DD_SUB, DD_SCALE, DD_CONCAT, DD_VALUE, DD_VARVT, DD_DEREF, DD_PACK, DD_OBJ,
    DD_MUL64, DD_HALF, DD_CHILD, DD_ITEM, DD_COUNT
};
/* vtable slot of each method (slots 0..2 are IUnknown) */
enum { DV_ADD = 3, DV_SUB, DV_SCALE, DV_CONCAT, DV_SETVALUE, DV_GETVALUE, DV_VARVT, DV_DEREF, DV_PACK, DV_SETOBJ,
       DV_MUL64, DV_HALF, DV_CHILD, DV_ITEM, DV_COUNT, DV_NSLOTS };

typedef struct Demo {
    const void *const *vtbl;
    LONG ref;
    LONG value;
    LONG calls;        /* number of method calls reached */
    void *obj;         /* last pointer given to SetObj */
    IDispatch *child;  /* returned (AddRef'd) by Child getter */
} Demo;
extern const void *const Demo_vtbl[DV_NSLOTS];
void Demo_init(Demo *d);

/* A counting IDispatch that records the last Invoke it saw. */
typedef struct Mock {
    const IDispatchVtbl *lpVtbl;
    LONG ref;
    DISPID last_dispid; WORD last_flags; UINT last_cargs, last_cnamed; LCID last_lcid;
    LONG last_arg0_i4; int invoked;
    GUID last_riid;
} Mock;
void Mock_init(Mock *m);

/* Interface data describing Demo; the INTERFACEDATA is static and lives forever. */
INTERFACEDATA *demo_idata(void);
extern const UINT demo_nmeth;
ITypeInfo *demo_typeinfo(LCID lcid);      /* CreateDispTypeInfo(demo_idata()) or NULL (+ failed CHECK) */
IDispatch *demo_stddisp(Demo *d, ITypeInfo *ti, IUnknown **unk_out); /* CreateStdDispatch + QI IDispatch */

/* DISPPARAMS builders */
static inline DISPPARAMS dp_make(VARIANT *args, UINT n, DISPID *named, UINT nnamed)
{ DISPPARAMS d; d.rgvarg = args; d.cArgs = n; d.rgdispidNamedArgs = named; d.cNamedArgs = nnamed; return d; }
static inline void v_i4(VARIANT *v, LONG x) { vinit(v); V_VT(v) = VT_I4; V_I4(v) = x; }
static inline void v_r8(VARIANT *v, double x) { vinit(v); V_VT(v) = VT_R8; V_R8(v) = x; }

/* Run a static (not real) vtable-slot call through the demo object's vtable for ground truth. */
#define DEMO_SLOT(d, idx) ((*(void *const *const *)(d))[idx])
#endif
