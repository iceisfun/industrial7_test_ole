/* Shared helpers of the ole32_ext area: ole32 entry points that the other areas only check for presence
 * (Ole32DllGetClassObject, OleInitializeWOW, the clipboard-broker ordinals, GetObjectFromRotByPath, CleanROTForApartment,
 * DestroyRunningObjectTable, CoIsOle1Class, CoCheckElevationEnabled, StgOpenStorageOnHandle, the internal drop-target
 * registration ordinals). Every expectation was measured on Windows 10 22H2; version-dependent facts are t_note()s. */
#ifndef EXT_H
#define EXT_H
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <oleauto.h>
#include <ocidl.h>
#include <stdlib.h>

/* the DLLs are never linked: route the SDK names through the module lookup */
#define CoInitializeEx U(CoInitializeEx)
#define CoUninitialize U(CoUninitialize)
#define CoTaskMemFree U(CoTaskMemFree)
#define OleInitialize U(OleInitialize)
#define OleUninitialize U(OleUninitialize)
#define GetRunningObjectTable U(GetRunningObjectTable)
#define CreateFileMoniker U(CreateFileMoniker)
#define StgCreateDocfile U(StgCreateDocfile)
#define StgOpenStorage U(StgOpenStorage)

#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)

#define X_E_INVALIDARG        ((HRESULT)0x80070057)
#define X_E_HANDLE            ((HRESULT)0x80070006)
#define X_E_NOINTERFACE       ((HRESULT)0x80004002)
#define X_E_OUTOFMEMORY       ((HRESULT)0x8007000E)
#define X_CO_E_NOTINITIALIZED ((HRESULT)0x800401F0)
#define X_MK_E_UNAVAILABLE    ((HRESULT)0x800401E3)
#define X_RPC_E_CHANGED_MODE  ((HRESULT)0x80010106)
#define X_RPC_E_WRONG_THREAD  ((HRESULT)0x8001010E)
#define X_STG_E_INVALIDFLAG   ((HRESULT)0x800300FF)
#define X_STG_E_INVALIDPOINTER ((HRESULT)0x80030009)
#define X_DRAGDROP_E_NOTREGISTERED ((HRESULT)0x80040100)
#define X_DRAGDROP_E_ALREADYREGISTERED ((HRESULT)0x80040101)
#define X_DRAGDROP_E_INVALIDHWND ((HRESULT)0x80040102)

/* APTTYPE of the calling thread, or 0xffff when it has no apartment */
unsigned x_apt_type(void);

/* run fn(arg) on a fresh thread (no COM state of the harness thread leaks in) and wait for it */
typedef void (*x_thread_fn)(void *);
void x_on_thread(x_thread_fn fn, void *arg);

/* a small ref-counted COM object with only IUnknown */
typedef struct { IUnknownVtbl *v; LONG refs; } XObj;
void x_obj_init(XObj *o);

/* an IDropTarget whose methods do nothing */
IDropTarget *x_drop_target(void);

#endif
