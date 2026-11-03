/* Well-known combase exports: pinned resolution, typed through the SDK headers where available. */
#define EXP_WINE_TOLERANT
#include "exp_common.h"
#include <objbase.h>
#include <combaseapi.h>

static void names(const char *const *n, size_t c) { exp_check_names(n, c); }
#define CB(name) UIN("combase.dll", name)
#define HAVE(name) do { FARPROC p_ = (FARPROC)CB(name); CHECKF(p_ != NULL, "combase!" #name); \
    if (p_) { HMODULE m_ = t_module("combase.dll"); CHECKF(exp_in_image(m_, p_), "combase!" #name " outside image"); \
    CHECKF(exp_find_name(#name) != NULL, #name " missing from reference table"); } } while (0)

TEST(exports_combase_known_init) {
    HAVE(CoInitializeEx); HAVE(CoUninitialize); HAVE(CoInitializeSecurity); HAVE(CoIncrementMTAUsage);
    HAVE(CoDecrementMTAUsage); HAVE(CoGetApartmentType);
}
TEST(exports_combase_known_not_exported_CoInitialize) {
    /* CoInitialize (the legacy wrapper) lives in ole32, only CoInitializeEx is in combase. */
    HMODULE m = t_module("combase.dll"); if (!m) { t_skip("no combase"); return; }
    if (exp_find_name("CoInitialize")) { t_skip("reference table lists CoInitialize"); return; }
    SET_ERR(0); CHECK(GetProcAddress(m, "CoInitialize") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(exports_combase_known_taskmem) {
    HAVE(CoTaskMemAlloc); HAVE(CoTaskMemFree); HAVE(CoTaskMemRealloc); HAVE(CoGetMalloc);
}
TEST(exports_combase_known_creation) {
    HAVE(CoCreateInstance); HAVE(CoCreateInstanceEx); HAVE(CoGetClassObject); HAVE(CoCreateGuid);
    HAVE(CoRegisterClassObject); HAVE(CoRevokeClassObject); HAVE(CoResumeClassObjects); HAVE(CoSuspendClassObjects);
}
TEST(exports_combase_known_guid_strings) {
    HAVE(StringFromGUID2); HAVE(StringFromCLSID); HAVE(StringFromIID); HAVE(CLSIDFromString);
    HAVE(IIDFromString); HAVE(CLSIDFromProgID); HAVE(ProgIDFromCLSID); HAVE(CLSIDFromProgIDEx);
}
TEST(exports_combase_known_marshaling) {
    HAVE(CoMarshalInterface); HAVE(CoUnmarshalInterface); HAVE(CoReleaseMarshalData); HAVE(CoGetMarshalSizeMax);
    HAVE(CoMarshalInterThreadInterfaceInStream); HAVE(CoGetInterfaceAndReleaseStream); HAVE(CoGetStandardMarshal);
    HAVE(CoMarshalHresult); HAVE(CoUnmarshalHresult); HAVE(CoDisconnectObject); HAVE(CoLockObjectExternal);
    HAVE(CoCreateFreeThreadedMarshaler);
}
TEST(exports_combase_known_security) {
    HAVE(CoQueryProxyBlanket); HAVE(CoSetProxyBlanket); HAVE(CoCopyProxy); HAVE(CoQueryClientBlanket);
    HAVE(CoImpersonateClient); HAVE(CoRevertToSelf); HAVE(CoQueryAuthenticationServices);
}
TEST(exports_combase_known_streams_and_hglobal) {
    HAVE(CreateStreamOnHGlobal); HAVE(GetHGlobalFromStream);
}
TEST(exports_combase_known_propvariant) {
    HAVE(PropVariantClear); HAVE(PropVariantCopy); HAVE(FreePropVariantArray);
}
TEST(exports_combase_known_errorinfo) {
    HAVE(CreateErrorInfo); HAVE(GetErrorInfo); HAVE(SetErrorInfo);
}
TEST(exports_combase_known_errorinfo_co) {
    static const char *const n[] = { "CoGetErrorInfo", "CoSetErrorInfo", "CoCreateErrorInfo", "CoGetApartmentID" };
    names(n, 4);
}
TEST(exports_combase_known_call_control) {
    HAVE(CoEnableCallCancellation); HAVE(CoDisableCallCancellation); HAVE(CoCancelCall); HAVE(CoTestCancel);
    HAVE(CoSetCancelObject); HAVE(CoGetCancelObject); HAVE(CoGetCallContext); HAVE(CoSwitchCallContext);
    HAVE(CoWaitForMultipleHandles); HAVE(CoWaitForMultipleObjects);
}
TEST(exports_combase_known_context) {
    HAVE(CoGetObjectContext); HAVE(CoGetContextToken); HAVE(CoGetDefaultContext); 
    HAVE(CoGetCurrentLogicalThreadId); HAVE(CoGetCurrentProcess);
}
TEST(exports_combase_known_dll_entry_points) {
    /* combase also exports the standard COM server entry points. */
    HMODULE m = t_module("combase.dll"); if (!m) { t_skip("no combase"); return; }
    CHECK(GetProcAddress(m, "DllGetClassObject") != NULL);
    CHECK(GetProcAddress(m, "DllGetActivationFactory") != NULL);
    CHECK(GetProcAddress(m, "DllCanUnloadNow") == NULL || exp_find_name("DllCanUnloadNow"));
}

/* Names not in the SDK headers: plain string resolution. */
TEST(exports_combase_known_roapi) {
    static const char *const n[] = { "RoInitialize", "RoUninitialize", "RoGetActivationFactory", "RoActivateInstance",
        "RoRegisterActivationFactories", "RoRevokeActivationFactories", "RoGetAgileReference", "RoGetApartmentIdentifier",
        "RoRegisterForApartmentShutdown", "RoUnregisterForApartmentShutdown", "RoGetServerActivatableClasses", "RoGetActivatableClassRegistration" };
    names(n, sizeof n / sizeof *n);
}
TEST(exports_combase_known_ro_errors) {
    static const char *const n[] = { "RoOriginateError", "RoOriginateErrorW", "RoTransformError", "RoTransformErrorW", "RoClearError",
        "RoGetErrorReportingFlags", "RoSetErrorReportingFlags", "RoCaptureErrorContext", "RoFailFastWithErrorContext",
        "RoInspectCapturedStackBackTrace", "RoInspectThreadErrorInfo", "RoOriginateLanguageException", "RoReportUnhandledError",
        "RoReportFailedDelegate", "RoResolveRestrictedErrorInfoReference", "RoGetMatchingRestrictedErrorInfo",
        "GetRestrictedErrorInfo", "SetRestrictedErrorInfo", "IsErrorPropagationEnabled" };
    names(n, sizeof n / sizeof *n);
}
TEST(exports_combase_known_ro_parameterized) {
    static const char *const n[] = { "RoGetParameterizedTypeInstanceIID", "RoFreeParameterizedTypeExtra", "RoParameterizedTypeExtraGetTypeSignature" };
    names(n, sizeof n / sizeof *n);
}
TEST(exports_combase_known_hstring_api) {
    static const char *const n[] = { "WindowsCreateString", "WindowsCreateStringReference", "WindowsDeleteString", "WindowsDuplicateString",
        "WindowsGetStringLen", "WindowsGetStringRawBuffer", "WindowsIsStringEmpty", "WindowsStringHasEmbeddedNull",
        "WindowsCompareStringOrdinal", "WindowsConcatString", "WindowsSubstring", "WindowsSubstringWithSpecifiedLength",
        "WindowsReplaceString", "WindowsTrimStringStart", "WindowsTrimStringEnd", "WindowsPreallocateStringBuffer",
        "WindowsPromoteStringBuffer", "WindowsDeleteStringBuffer", "WindowsInspectString", "WindowsInspectString2" };
    names(n, sizeof n / sizeof *n);
}
TEST(exports_combase_known_internal_state) {
    static const char *const n[] = { "CleanupOleStateInAllTls", "CleanupTlsOleState", "CleanupComl2StateInAllTls", "CleanupTlsComl2State",
        "SetCleanupFlag", "ClearCleanupFlag", "UpdateDCOMSettings", "UpdateProcessTracing", "GetCatalogHelper", "GetHookInterface", "EnableHookObject", "DcomChannelSetHResult" };
    names(n, sizeof n / sizeof *n);
}
TEST(exports_combase_known_ndr) {
    static const char *const n[] = { "NdrProxyInitialize", "NdrProxyGetBuffer", "NdrProxySendReceive", "NdrProxyFreeBuffer", "NdrStubInitialize",
        "NdrStubGetBuffer", "NdrpClientCall3", "NdrpClientCall2" };
    int any = 0; for (size_t i = 0; i < sizeof n / sizeof *n; i++) any += exp_find_name(n[i]) != NULL;
    t_note("%d of %d probe Ndr names are exported by combase in the reference table", any, (int)(sizeof n / sizeof *n));
    for (size_t i = 0; i < sizeof n / sizeof *n; i++) if (exp_find_name(n[i])) names(&n[i], 1);
}
TEST(exports_combase_known_wire_marshal_misc) {
    static const char *const n[] = { "HkOleRegisterObject", "DllDebugObjectRPCHook", "GetFuncDescs", "ReleaseFuncDescs",
        "CoGetModuleType", "CoGetModuleArchitecture", "CoInitializeWOW", "CoUnloadingWOW", "CoRegisterRacActivationToken",
        "CoRevokeRacActivationToken", "CoPushServiceDomain", "CoPopServiceDomain", "CoRegisterDeviceCatalog", "CoRevokeDeviceCatalog" };
    names(n, sizeof n / sizeof *n);
}

/* Absent from combase (they live in ole32 / oleaut32 / rpcrt4): a pinned lookup must fail with ERROR_PROC_NOT_FOUND. */
static void absent(const char *const *n, size_t c) {
    HMODULE m = t_module("combase.dll"); if (!m) { t_skip("no combase"); return; }
    for (size_t i = 0; i < c; i++) {
        if (exp_find_name(n[i])) { t_note("%s is exported by combase in the reference table; skipped", n[i]); continue; }
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, n[i]);
        CHECKF(p == NULL, "combase unexpectedly exports %s", n[i]);
        if (!p) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
TEST(exports_combase_absent_ole_api) {
    static const char *const n[] = { "OleInitialize", "OleUninitialize", "OleSetClipboard", "OleGetClipboard", "OleFlushClipboard",
        "DoDragDrop", "RegisterDragDrop", "RevokeDragDrop", "OleCreate", "OleLoad", "OleSave", "OleRun", "OleLockRunning",
        "OleCreateMenuDescriptor", "OleDraw", "OleRegEnumVerbs" };
    absent(n, sizeof n / sizeof *n);
}
TEST(exports_combase_absent_storage_api) {
    static const char *const n[] = { "StgCreateDocfile", "StgOpenStorage", "StgCreateStorageEx", "StgOpenStorageEx", "StgIsStorageFile",
        "CreateILockBytesOnHGlobal", "GetHGlobalFromILockBytes", "ReadClassStg", "WriteClassStg", "ReadFmtUserTypeStg", "StgSetTimes" };
    absent(n, sizeof n / sizeof *n);
}
TEST(exports_combase_absent_moniker_rot_api) {
    static const char *const n[] = { "CreateFileMoniker", "CreateItemMoniker", "CreateBindCtx", "GetRunningObjectTable", "MkParseDisplayName",
        "MonikerCommonPrefixWith", "CreateGenericComposite", "BindMoniker", "CreateClassMoniker", "CreatePointerMoniker", "CreateAntiMoniker" };
    absent(n, sizeof n / sizeof *n);
}
TEST(exports_combase_absent_oleaut_api) {
    static const char *const n[] = { "SysAllocString", "SysFreeString", "VariantInit", "VariantClear", "SafeArrayCreate", "LoadTypeLib",
        "OleLoadPicture", "CreateTypeLib2", "BSTR_UserSize", "VarBstrFromDate" };
    absent(n, sizeof n / sizeof *n);
}
TEST(exports_combase_absent_rpc_api) {
    static const char *const n[] = { "RpcBindingFree", "RpcStringFreeW", "UuidCreate", "NdrClientCall2", "RpcServerListen",
        "RpcBindingFromStringBindingW", "NdrMesTypeDecode2", "I_RpcBindingInqSecurityContext" };
    absent(n, sizeof n / sizeof *n);
}
TEST(exports_combase_absent_legacy_co_api) {
    static const char *const n[] = { "CoInitialize", "CoBuildVersion", "CoGetObject", "CoGetTreatAsClassX", "CoTreatAsClass", "CoIsOle1Class",
        "CoGetClassObjectFromURL", "CoFileTimeToDosDateTime", "CoDosDateTimeToFileTime", "CoRegisterMallocSpyX", "CoLoadLibrary", "CoFreeLibrary", "CoFreeAllLibraries" };
    absent(n, sizeof n / sizeof *n);
}
