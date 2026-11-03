/* exports_ole32: forwarder behaviour of Windows 10 x64 ole32.dll's export table (resolve only, no calls).
 *
 * On Windows 10 most Co and Ole worker exports of ole32.dll are forwarder entries (the export RVA points
 * inside the export directory and holds a "MODULE.Name" string). GetProcAddress follows the forwarder
 * and returns the address inside the final module. Forwarders are of three flavours:
 *   - "COMBASE.Name" or "COMBASE.#ord" - direct to combase.dll
 *   - "api-ms-win-core-com-*.Name" - API-set contracts, which on a Win10 desktop SKU are hosted by
 *     combase.dll (verified by the Win10 export table of combase.dll containing each target name)
 * The table below is generated from tools/ole32_exports.json (Name->Target when the names differ).
 */
/* BEGIN FWDTABLE (generated) */
/*
 * forwarded to api-ms-win-core-com-l1-1-0 (76):
 *   CLSIDFromProgID, CLSIDFromString, CoAddRefServerProcess, CoAllowUnmarshalerCLSID, CoCancelCall,
 *   CoCopyProxy, CoCreateFreeThreadedMarshaler, CoCreateGuid, CoCreateInstance, CoCreateInstanceEx,
 *   CoCreateInstanceFromApp, CoDecrementMTAUsage, CoDisableCallCancellation, CoDisconnectContext,
 *   CoDisconnectObject, CoEnableCallCancellation, CoFreeUnusedLibraries, CoFreeUnusedLibrariesEx,
 *   CoGetApartmentType, CoGetCallContext, CoGetCallerTID, CoGetCancelObject, CoGetClassObject,
 *   CoGetContextToken, CoGetCurrentLogicalThreadId, CoGetCurrentProcess, CoGetDefaultContext,
 *   CoGetInterfaceAndReleaseStream, CoGetMalloc, CoGetMarshalSizeMax, CoGetObjectContext,
 *   CoGetPSClsid, CoGetStandardMarshal, CoGetStdMarshalEx, CoGetTreatAsClass, CoImpersonateClient,
 *   CoIncrementMTAUsage, CoInitializeEx, CoInitializeSecurity, CoIsHandlerConnected,
 *   CoLockObjectExternal, CoMarshalHresult, CoMarshalInterThreadInterfaceInStream,
 *   CoMarshalInterface, CoQueryAuthenticationServices, CoQueryClientBlanket, CoQueryProxyBlanket,
 *   CoRegisterActivationFilter, CoRegisterClassObject, CoRegisterPSClsid, CoRegisterSurrogate,
 *   CoReleaseMarshalData, CoReleaseServerProcess, CoResumeClassObjects, CoRevertToSelf,
 *   CoRevokeClassObject, CoSetCancelObject, CoSetProxyBlanket, CoSuspendClassObjects,
 *   CoSwitchCallContext, CoTaskMemAlloc, CoTaskMemFree, CoTaskMemRealloc, CoTestCancel,
 *   CoUninitialize, CoUnmarshalHresult, CoUnmarshalInterface, CoWaitForMultipleHandles,
 *   CoWaitForMultipleObjects, CreateStreamOnHGlobal, GetHGlobalFromStream, IIDFromString,
 *   ProgIDFromCLSID, StringFromCLSID, StringFromGUID2, StringFromIID
 * forwarded to api-ms-win-core-com-l1-1-1 (1):
 *   RoGetAgileReference
 * forwarded to api-ms-win-core-com-l1-1-2 (2):
 *   CLSIDFromProgIDEx, CoFileTimeNow
 * forwarded to api-ms-win-core-com-midlproxystub-l1-1-0 (73):
 *   CStdAsyncStubBuffer2_Connect, CStdAsyncStubBuffer2_Disconnect, CStdAsyncStubBuffer2_Release,
 *   CStdAsyncStubBuffer_AddRef, CStdAsyncStubBuffer_Connect, CStdAsyncStubBuffer_Disconnect,
 *   CStdAsyncStubBuffer_Invoke, CStdAsyncStubBuffer_QueryInterface, CStdAsyncStubBuffer_Release,
 *   CStdStubBuffer2_Connect, CStdStubBuffer2_CountRefs, CStdStubBuffer2_Disconnect,
 *   CStdStubBuffer2_QueryInterface, NdrProxyForwardingFunction10, NdrProxyForwardingFunction11,
 *   NdrProxyForwardingFunction12, NdrProxyForwardingFunction13, NdrProxyForwardingFunction14,
 *   NdrProxyForwardingFunction15, NdrProxyForwardingFunction16, NdrProxyForwardingFunction17,
 *   NdrProxyForwardingFunction18, NdrProxyForwardingFunction19, NdrProxyForwardingFunction20,
 *   NdrProxyForwardingFunction21, NdrProxyForwardingFunction22, NdrProxyForwardingFunction23,
 *   NdrProxyForwardingFunction24, NdrProxyForwardingFunction25, NdrProxyForwardingFunction26,
 *   NdrProxyForwardingFunction27, NdrProxyForwardingFunction28, NdrProxyForwardingFunction29,
 *   NdrProxyForwardingFunction3, NdrProxyForwardingFunction30, NdrProxyForwardingFunction31,
 *   NdrProxyForwardingFunction32, NdrProxyForwardingFunction4, NdrProxyForwardingFunction5,
 *   NdrProxyForwardingFunction6, NdrProxyForwardingFunction7, NdrProxyForwardingFunction8,
 *   NdrProxyForwardingFunction9, ObjectStublessClient10, ObjectStublessClient11,
 *   ObjectStublessClient12, ObjectStublessClient13, ObjectStublessClient14, ObjectStublessClient15,
 *   ObjectStublessClient16, ObjectStublessClient17, ObjectStublessClient18, ObjectStublessClient19,
 *   ObjectStublessClient20, ObjectStublessClient21, ObjectStublessClient22, ObjectStublessClient23,
 *   ObjectStublessClient24, ObjectStublessClient25, ObjectStublessClient26, ObjectStublessClient27,
 *   ObjectStublessClient28, ObjectStublessClient29, ObjectStublessClient3, ObjectStublessClient30,
 *   ObjectStublessClient31, ObjectStublessClient32, ObjectStublessClient4, ObjectStublessClient5,
 *   ObjectStublessClient6, ObjectStublessClient7, ObjectStublessClient8, ObjectStublessClient9
 * forwarded to api-ms-win-core-com-private-l1-1-1 (35):
 *   CLSIDFromOle1Class, CoCreateObjectInContext, CoDeactivateObject, CoGetActivationState,
 *   CoGetApartmentID, CoGetCallState, CoGetClassVersion, CoGetComCatalog->GetCatalogHelper,
 *   CoGetInstanceFromFile, CoGetInstanceFromIStorage, CoGetModuleType, CoGetProcessIdentifier,
 *   CoGetSystemSecurityPermissions, CoInvalidateRemoteMachineBindings, CoPopServiceDomain,
 *   CoPushServiceDomain, CoReactivateObject, CoRegisterInitializeSpy, CoRegisterMallocSpy,
 *   CoRetireServer, CoRevokeInitializeSpy, CoRevokeMallocSpy, CoVrfCheckThreadState,
 *   CoVrfGetThreadState, CoVrfReleaseThreadState, CreateErrorInfo->CoCreateErrorInfo,
 *   DllDebugObjectRPCHook, EnableHookObject, GetErrorInfo->CoGetErrorInfo, GetHookInterface,
 *   HkOleRegisterObject, NdrOleInitializeExtension, SetErrorInfo->CoSetErrorInfo,
 *   UpdateDCOMSettings, UpdateProcessTracing
 * forwarded to combase (105):
 *   CLIPFORMAT_UserFree, CLIPFORMAT_UserFree64, CLIPFORMAT_UserMarshal, CLIPFORMAT_UserMarshal64,
 *   CLIPFORMAT_UserSize, CLIPFORMAT_UserSize64, CLIPFORMAT_UserUnmarshal,
 *   CLIPFORMAT_UserUnmarshal64, CoDecodeProxy, CoHandlePriorityEventsFromMessagePump->#111,
 *   CoRegisterChannelHook->#112, CoSetMessageDispatcher->#110, DcomChannelSetHResult,
 *   HACCEL_UserFree, HACCEL_UserFree64, HACCEL_UserMarshal, HACCEL_UserMarshal64, HACCEL_UserSize,
 *   HACCEL_UserSize64, HACCEL_UserUnmarshal, HACCEL_UserUnmarshal64, HBITMAP_UserFree,
 *   HBITMAP_UserFree64, HBITMAP_UserMarshal, HBITMAP_UserMarshal64, HBITMAP_UserSize,
 *   HBITMAP_UserSize64, HBITMAP_UserUnmarshal, HBITMAP_UserUnmarshal64, HBRUSH_UserFree,
 *   HBRUSH_UserFree64, HBRUSH_UserMarshal, HBRUSH_UserMarshal64, HBRUSH_UserSize,
 *   HBRUSH_UserSize64, HBRUSH_UserUnmarshal, HBRUSH_UserUnmarshal64, HDC_UserFree, HDC_UserFree64,
 *   HDC_UserMarshal, HDC_UserMarshal64, HDC_UserSize, HDC_UserSize64, HDC_UserUnmarshal,
 *   HDC_UserUnmarshal64, HGLOBAL_UserFree, HGLOBAL_UserFree64, HGLOBAL_UserMarshal,
 *   HGLOBAL_UserMarshal64, HGLOBAL_UserSize, HGLOBAL_UserSize64, HGLOBAL_UserUnmarshal,
 *   HGLOBAL_UserUnmarshal64, HICON_UserFree, HICON_UserFree64, HICON_UserMarshal,
 *   HICON_UserMarshal64, HICON_UserSize, HICON_UserSize64, HICON_UserUnmarshal,
 *   HICON_UserUnmarshal64, HMENU_UserFree, HMENU_UserFree64, HMENU_UserMarshal,
 *   HMENU_UserMarshal64, HMENU_UserSize, HMENU_UserSize64, HMENU_UserUnmarshal,
 *   HMENU_UserUnmarshal64, HMONITOR_UserFree, HMONITOR_UserFree64, HMONITOR_UserMarshal,
 *   HMONITOR_UserMarshal64, HMONITOR_UserSize, HMONITOR_UserSize64, HMONITOR_UserUnmarshal,
 *   HMONITOR_UserUnmarshal64, HPALETTE_UserFree, HPALETTE_UserFree64, HPALETTE_UserMarshal,
 *   HPALETTE_UserMarshal64, HPALETTE_UserSize, HPALETTE_UserSize64, HPALETTE_UserUnmarshal,
 *   HPALETTE_UserUnmarshal64, HRGN_UserFree, HRGN_UserMarshal, HRGN_UserSize, HRGN_UserUnmarshal,
 *   HWND_UserFree, HWND_UserFree64, HWND_UserMarshal, HWND_UserMarshal64, HWND_UserSize,
 *   HWND_UserSize64, HWND_UserUnmarshal, HWND_UserUnmarshal64, WdtpInterfacePointer_UserFree,
 *   WdtpInterfacePointer_UserFree64, WdtpInterfacePointer_UserMarshal,
 *   WdtpInterfacePointer_UserMarshal64, WdtpInterfacePointer_UserSize,
 *   WdtpInterfacePointer_UserSize64, WdtpInterfacePointer_UserUnmarshal,
 *   WdtpInterfacePointer_UserUnmarshal64
 */
/* END FWDTABLE */
#include "t.h"
#include "exp_table.h"
#define WINE_SKIP() do { if (t_is_wine()) { t_skip("Wine builtin ole32 does not forward like Windows 10"); return; } } while (0)

static int in_module(FARPROC p, HMODULE m) {
    HMODULE h = NULL;
    if (!p) return 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h)) return 0;
    return h == m;
}
static FARPROC byname(const char *mod, const char *n) { return t_proc_in(mod, n); }

enum { K_COMBASE_NAMED, K_COMBASE_ORD, K_APISET };
static int fwd_kind(const struct exp_ent *e) {
    if (_strnicmp(e->fwd, "COMBASE.", 8) == 0) return e->fwd[8] == '#' ? K_COMBASE_ORD : K_COMBASE_NAMED;
    return K_APISET;
}
/* target export name of a forwarder (NULL for "#ord") */
static const char *fwd_target(const struct exp_ent *e) {
    const char *dot = strchr(e->fwd, '.');
    return (dot && dot[1] != '#') ? dot + 1 : NULL;
}

/* forwarder entries resolve (by name and by ordinal) to the address in the target module, never inside ole32 */
static void fwd_chunk(int lo, int hi, int kind_mask) {
    WINE_SKIP();
    HMODULE ole = t_module("ole32.dll"), cb = t_module("combase.dll");
    if (!ole || !cb) { t_skip("module not loadable"); return; }
    int seen = 0;
    for (unsigned i = 0; i < EXP_N; i++) {
        const struct exp_ent *e = &g_exp[i];
        if (!e->fwd || !e->name) continue;
        if (!((1 << fwd_kind(e)) & kind_mask)) continue;
        if (seen++ < lo || seen > hi) continue;
        FARPROC p = byname("ole32.dll", e->name), q = t_proc_ord("ole32.dll", e->ord);
        CHECKF(p != NULL, "%s", e->name);
        CHECKF(p == q, "%s by-name vs ordinal %u", e->name, e->ord);
        CHECKF(!in_module(p, ole), "%s must resolve outside ole32 (forwarded)", e->name);
        const char *tn = fwd_target(e);
        if (tn) {
            FARPROC c = byname("combase.dll", tn);
            if (c) CHECKF(p == c, "%s: ole32 %p vs combase!%s %p", e->name, p, tn, c);
            else t_note("%s forwards to %s which combase does not export by that name", e->name, e->fwd);
        }
    }
}
#define FWDCHUNK(name, kinds, n) \
    TEST(exports_ole32_fwd_##name##_##n) { fwd_chunk((n) * 40, (n) * 40 + 40, kinds); }
#define FWDKINDS_COMBASE ((1 << K_COMBASE_NAMED) | (1 << K_COMBASE_ORD))
FWDCHUNK(combase, FWDKINDS_COMBASE, 0)
FWDCHUNK(combase, FWDKINDS_COMBASE, 1)
FWDCHUNK(combase, FWDKINDS_COMBASE, 2)
FWDCHUNK(apiset, (1 << K_APISET), 0)
FWDCHUNK(apiset, (1 << K_APISET), 1)
FWDCHUNK(apiset, (1 << K_APISET), 2)
FWDCHUNK(apiset, (1 << K_APISET), 3)
FWDCHUNK(apiset, (1 << K_APISET), 4)
FWDCHUNK(apiset, (1 << K_APISET), 5)

/* the three ordinal forwarders: ole32 name -> combase ordinal (#110..#112), no name on the combase side */
TEST(exports_ole32_fwd_by_ordinal_targets) {
    WINE_SKIP();
    static const struct { const char *name; unsigned cbord; } t[] = {
        { "CoSetMessageDispatcher", 110 }, { "CoRegisterChannelHook", 112 }, { "CoHandlePriorityEventsFromMessagePump", 111 } };
    for (unsigned i = 0; i < 3; i++) {
        FARPROC p = byname("ole32.dll", t[i].name), c = t_proc_ord("combase.dll", t[i].cbord);
        CHECKF(p != NULL, "%s", t[i].name);
        CHECKF(c != NULL, "combase #%u", t[i].cbord);
        CHECKF(p == c, "%s must equal combase!#%u", t[i].name, t[i].cbord);
    }
}

/* private forwarders whose target has a different name than the ole32 export */
TEST(exports_ole32_fwd_renamed_targets) {
    WINE_SKIP();
    static const struct { const char *name, *target; } t[] = {
        { "CoGetComCatalog", "GetCatalogHelper" }, { "CreateErrorInfo", "CoCreateErrorInfo" },
        { "GetErrorInfo", "CoGetErrorInfo" }, { "SetErrorInfo", "CoSetErrorInfo" } };
    for (unsigned i = 0; i < 4; i++) {
        FARPROC p = byname("ole32.dll", t[i].name), c = byname("combase.dll", t[i].target);
        CHECKF(p != NULL, "%s", t[i].name);
        if (!c) { t_note("combase lacks %s", t[i].target); continue; }
        CHECKF(p == c, "%s must equal combase!%s", t[i].name, t[i].target);
        /* the ole32 name is NOT a combase export under this name (SetErrorInfo is oleaut32's) */
    }
}

/* well-known forwarders spelled out explicitly (documentation for implementors) */
TEST(exports_ole32_fwd_well_known) {
    WINE_SKIP();
    static const char *const n[] = { "CoInitializeEx", "CoUninitialize", "CoCreateInstance", "CoCreateInstanceEx", "CoGetClassObject",
        "CoTaskMemAlloc", "CoTaskMemFree", "CoTaskMemRealloc", "CoMarshalInterface", "CoUnmarshalInterface", "CoGetMalloc",
        "CLSIDFromString", "StringFromGUID2", "CoRegisterClassObject", "CoRevokeClassObject", "CoGetObject",
        "CreateStreamOnHGlobal", "GetHGlobalFromStream", "CoCreateGuid", "IIDFromString", "StringFromCLSID", "ProgIDFromCLSID" };
    for (unsigned i = 0; i < sizeof n / sizeof n[0]; i++) {
        FARPROC p = byname("ole32.dll", n[i]), c = byname("combase.dll", n[i]);
        CHECKF(p != NULL, "ole32!%s", n[i]);
        if (!c) { t_note("combase!%s absent", n[i]); continue; }
        /* forwarded on Win10: identical address; if ole32 hosted its own copy this would differ */
        CHECKF(p == c, "ole32!%s expected to forward to combase", n[i]);
    }
}

/* exports implemented in ole32 itself (not forwarded) live inside the ole32 image and are not combase's */
static void own_chunk(int lo, int hi) {
    WINE_SKIP();
    HMODULE ole = t_module("ole32.dll");
    if (!ole) { t_skip("ole32"); return; }
    int seen = 0;
    for (unsigned i = 0; i < EXP_N; i++) {
        const struct exp_ent *e = &g_exp[i];
        if (e->fwd) continue;
        if (seen++ < lo || seen > hi) continue;
        FARPROC p = e->name ? byname("ole32.dll", e->name) : t_proc_ord("ole32.dll", e->ord);
        CHECKF(p != NULL, "ord %u %s", e->ord, e->name ? e->name : "(noname)");
        CHECKF(in_module(p, ole), "ord %u %s must be implemented inside ole32", e->ord, e->name ? e->name : "(noname)");
        if (e->name) {
            FARPROC c = byname("combase.dll", e->name);
            if (c) CHECKF(c != p, "%s: ole32 own export must not alias combase's", e->name);
        }
    }
}
#define OWNCHUNK(n) TEST(exports_ole32_own_##n) { own_chunk((n) * 45, (n) * 45 + 45); }
OWNCHUNK(0) OWNCHUNK(1) OWNCHUNK(2) OWNCHUNK(3) OWNCHUNK(4) OWNCHUNK(5)

TEST(exports_ole32_fwd_count) {
    WINE_SKIP();
    unsigned f = 0, own = 0;
    for (unsigned i = 0; i < EXP_N; i++) { if (g_exp[i].fwd) f++; else own++; }
    EXPECT_EQ(f, 292);
    EXPECT_EQ(own, 262);
}
