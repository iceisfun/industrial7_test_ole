/* Export surface of the COM proxy/stub runtime: which module exports what, what is a forwarder, and where the
 * ObjectStublessClientN / NdrProxyForwardingFunctionN families live.  ole32.dll forwards most of them through
 * api-ms-win-core-com-midlproxystub-l1-1-0 to combase.dll (ordinals/forwarder strings from the Win10 export tables). */
#include "psd.h"

/* an export Wine does not provide is only noted under Wine; on Windows it is a failure */
#define EXPCHK(c, ...) do { if (t_is_wine() && !(c)) t_note(__VA_ARGS__); else CHECKF(c, __VA_ARGS__); } while (0)

/* return the forwarder string of an export (NULL if not a forwarder or not exported) */
static const char *export_forwarder(HMODULE m, const char *name, int *exported) {
    *exported = 0;
    if (!m) return NULL;
    BYTE *b = (BYTE *)m;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)b;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return NULL;
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(b + dd->VirtualAddress);
    DWORD *names = (DWORD *)(b + ed->AddressOfNames), *funcs = (DWORD *)(b + ed->AddressOfFunctions);
    WORD *ords = (WORD *)(b + ed->AddressOfNameOrdinals);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        if (strcmp((const char *)(b + names[i]), name)) continue;
        *exported = 1;
        DWORD rva = funcs[ords[i]];
        if (rva >= dd->VirtualAddress && rva < dd->VirtualAddress + dd->Size) return (const char *)(b + rva);
        return NULL;
    }
    return NULL;
}

static const char *const rpcrt4_names[] = {
    "NdrProxyInitialize", "NdrProxyGetBuffer", "NdrProxySendReceive", "NdrProxyFreeBuffer", "NdrProxyErrorHandler",
    "NdrStubInitialize", "NdrStubInitializeMarshall", "NdrStubGetBuffer", "NdrStubForwardingFunction", "NdrStubCall2", "NdrStubCall3",
    "NdrDllGetClassObject", "NdrDllCanUnloadNow", "NdrDllRegisterProxy", "NdrDllUnregisterProxy",
    "CStdStubBuffer_QueryInterface", "CStdStubBuffer_AddRef", "NdrCStdStubBuffer_Release", "NdrCStdStubBuffer2_Release",
    "CStdStubBuffer_Connect", "CStdStubBuffer_Disconnect", "CStdStubBuffer_Invoke", "CStdStubBuffer_IsIIDSupported",
    "CStdStubBuffer_CountRefs", "CStdStubBuffer_DebugServerQueryInterface", "CStdStubBuffer_DebugServerRelease",
    "IUnknown_QueryInterface_Proxy", "IUnknown_AddRef_Proxy", "IUnknown_Release_Proxy",
    "CreateProxyFromTypeInfo", "CreateStubFromTypeInfo", "NdrOleAllocate", "NdrOleFree", "NdrpCreateProxy", "NdrpCreateStub",
    "NdrDcomAsyncClientCall", "NdrDcomAsyncStubCall", "Ndr64DcomAsyncClientCall", "Ndr64DcomAsyncStubCall",
    "NdrClientCall2", "NdrClientCall3", "NdrServerCall2", "NdrServerInitialize", "NdrServerInitializeNew", "NdrClientInitializeNew",
    "NdrInterfacePointerBufferSize", "NdrInterfacePointerMarshall", "NdrInterfacePointerUnmarshall", "NdrInterfacePointerMemorySize", "NdrInterfacePointerFree",
    "NdrGetDcomProtocolVersion", "NdrConformantStringMarshall", "NdrConformantStringUnmarshall",
};
TEST(psde_rpcrt4_exports) {
    HMODULE m = t_module("rpcrt4.dll");
    CHECK(m != NULL);
    for (unsigned i = 0; i < sizeof rpcrt4_names / sizeof *rpcrt4_names; i++)
        EXPCHK(m && GetProcAddress(m, rpcrt4_names[i]) != NULL, "rpcrt4 does not export %s", rpcrt4_names[i]);
}

static const char *const combase_names[] = {
    "CStdStubBuffer_QueryInterface", "CStdStubBuffer_AddRef", "CStdStubBuffer_Connect", "CStdStubBuffer_Disconnect",
    "CStdStubBuffer_Invoke", "CStdStubBuffer_IsIIDSupported", "CStdStubBuffer_CountRefs",
    "CStdStubBuffer_DebugServerQueryInterface", "CStdStubBuffer_DebugServerRelease",
    "NdrCStdStubBuffer_Release", "NdrCStdStubBuffer2_Release",
    "CStdStubBuffer2_Connect", "CStdStubBuffer2_Disconnect", "CStdStubBuffer2_CountRefs", "CStdStubBuffer2_QueryInterface",
    "CStdAsyncStubBuffer_QueryInterface", "CStdAsyncStubBuffer_AddRef", "CStdAsyncStubBuffer_Release",
    "CStdAsyncStubBuffer_Connect", "CStdAsyncStubBuffer_Disconnect", "CStdAsyncStubBuffer_Invoke",
    "CStdAsyncStubBuffer2_Connect", "CStdAsyncStubBuffer2_Disconnect", "CStdAsyncStubBuffer2_Release",
    "NdrOleDllGetClassObject", "NdrOleInitializeExtension", "NdrExtStubInitialize", "NdrpFindInterface",
    "CoGetPSClsid", "CoRegisterPSClsid",
};
TEST(psde_combase_exports) {
    HMODULE m = t_module("combase.dll");
    if (!m) { t_skip("no combase.dll"); return; }
    for (unsigned i = 0; i < sizeof combase_names / sizeof *combase_names; i++)
        EXPCHK(GetProcAddress(m, combase_names[i]) != NULL, "combase does not export %s", combase_names[i]);
}

static const char *const ole32_names[] = {
    "CStdStubBuffer2_Connect", "CStdStubBuffer2_Disconnect", "CStdStubBuffer2_CountRefs", "CStdStubBuffer2_QueryInterface",
    "CStdAsyncStubBuffer_QueryInterface", "CStdAsyncStubBuffer_AddRef", "CStdAsyncStubBuffer_Release",
    "CStdAsyncStubBuffer_Connect", "CStdAsyncStubBuffer_Disconnect", "CStdAsyncStubBuffer_Invoke",
    "CStdAsyncStubBuffer2_Connect", "CStdAsyncStubBuffer2_Disconnect", "CStdAsyncStubBuffer2_Release",
    "ComPs_NdrDllCanUnloadNow", "ComPs_NdrDllGetClassObject", "ComPs_NdrDllRegisterProxy", "ComPs_NdrDllUnregisterProxy",
    "NdrOleInitializeExtension", "CoGetPSClsid", "CoRegisterPSClsid",
};
TEST(psde_ole32_exports) {
    HMODULE m = t_module("ole32.dll");
    CHECK(m != NULL);
    for (unsigned i = 0; i < sizeof ole32_names / sizeof *ole32_names; i++)
        EXPCHK(m && GetProcAddress(m, ole32_names[i]) != NULL, "ole32 does not export %s", ole32_names[i]);
}

TEST(psde_stubless_families_exported) {
    HMODULE o = t_module("ole32.dll"), c = t_module("combase.dll");
    char nm[48];
    for (int n = 3; n <= 32; n++) {
        wsprintfA(nm, "ObjectStublessClient%d", n);
        EXPCHK(o && GetProcAddress(o, nm), "ole32 lacks %s", nm);
        EXPCHK(c && GetProcAddress(c, nm), "combase lacks %s", nm);
        wsprintfA(nm, "NdrProxyForwardingFunction%d", n);
        EXPCHK(o && GetProcAddress(o, nm), "ole32 lacks %s", nm);
        EXPCHK(c && GetProcAddress(c, nm), "combase lacks %s", nm);
    }
    /* the family starts at slot 3 (0..2 are IUnknown): there is no ObjectStublessClient0/1/2 or 33 */
    for (int n = 0; n <= 2; n++) {
        wsprintfA(nm, "ObjectStublessClient%d", n);
        SET_ERR(0);
        CHECKF(!(o && GetProcAddress(o, nm)), "ole32 exports %s", nm);
        wsprintfA(nm, "NdrProxyForwardingFunction%d", n);
        CHECKF(!(o && GetProcAddress(o, nm)), "ole32 exports %s", nm);
    }
    CHECK(!(o && GetProcAddress(o, "ObjectStublessClient33")));
    CHECK(!(c && GetProcAddress(c, "ObjectStublessClient33")));
    CHECK(!(o && GetProcAddress(o, "NdrProxyForwardingFunction33")));
}

TEST(psde_ole32_forwarders) {
    HMODULE o = t_module("ole32.dll"), c = t_module("combase.dll");
    if (t_is_wine()) { t_skip("wine's ole32 implements these itself"); return; }
    char nm[48];
    static const char *const fwd_static[] = { "CStdStubBuffer2_Connect", "CStdStubBuffer2_Disconnect", "CStdStubBuffer2_CountRefs", "CStdStubBuffer2_QueryInterface" };
    for (unsigned i = 0; i < 4; i++) {
        int ex;
        const char *f = export_forwarder(o, fwd_static[i], &ex);
        CHECKF(ex && f != NULL, "%s: exported=%d forwarder=%s", fwd_static[i], ex, f ? f : "(none)");
        if (f) {
            char want[96]; wsprintfA(want, "api-ms-win-core-com-midlproxystub-l1-1-0.%s", fwd_static[i]);
            CHECKF(!strcmp(f, want), "%s forwards to \"%s\"", fwd_static[i], f);
        }
        CHECKF(GetProcAddress(o, fwd_static[i]) == GetProcAddress(c, fwd_static[i]), "%s: ole32 and combase resolve differently", fwd_static[i]);
    }
    for (int n = 3; n <= 32; n++) {
        const char *fam[2] = { "ObjectStublessClient%d", "NdrProxyForwardingFunction%d" };
        for (int k = 0; k < 2; k++) {
            wsprintfA(nm, fam[k], n);
            int ex;
            const char *f = export_forwarder(o, nm, &ex);
            char want[96]; wsprintfA(want, "api-ms-win-core-com-midlproxystub-l1-1-0.%s", nm);
            CHECKF(ex && f && !strcmp(f, want), "%s: forwarder \"%s\"", nm, f ? f : "(none)");
            CHECKF(GetProcAddress(o, nm) == GetProcAddress(c, nm), "%s: ole32 and combase resolve differently", nm);
        }
    }
    /* ComPs_* and NdrOleInitializeExtension are real ole32 code, not forwarders */
    static const char *const own[] = { "ComPs_NdrDllGetClassObject", "ComPs_NdrDllRegisterProxy", "ComPs_NdrDllUnregisterProxy", "ComPs_NdrDllCanUnloadNow" };
    for (unsigned i = 0; i < 4; i++) {
        int ex;
        const char *f = export_forwarder(o, own[i], &ex);
        CHECKF(ex && f == NULL, "%s should be implemented in ole32 (forwarder=%s)", own[i], f ? f : "none");
    }
}

TEST(psde_stdstubbuffer_in_combase_forwarded_by_rpcrt4_thunks) {
    HMODULE r = t_module("rpcrt4.dll"), c = t_module("combase.dll");
    static const char *const n[] = { "CStdStubBuffer_QueryInterface", "CStdStubBuffer_AddRef", "CStdStubBuffer_Connect", "CStdStubBuffer_Disconnect",
        "CStdStubBuffer_Invoke", "CStdStubBuffer_IsIIDSupported", "CStdStubBuffer_CountRefs", "NdrCStdStubBuffer_Release" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        FARPROC a = GetProcAddress(r, n[i]), b = GetProcAddress(c, n[i]);
        EXPCHK(a && b, "%s missing", n[i]);
        /* rpcrt4 owns a small trampoline into the ole routines; combase owns the implementation: distinct code */
        t_note("%s: rpcrt4 %p combase %p", n[i], a, b);
        int ex;
        CHECKF(export_forwarder(r, n[i], &ex) == NULL, "%s should not be an export forwarder in rpcrt4", n[i]);
    }
}

TEST(psde_stdstub_slot_distinct_addresses) {
    HMODULE c = t_module("combase.dll");
    char nm[48];
    FARPROC prev = NULL;
    int distinct = 0;
    for (int n = 3; n <= 32; n++) {
        wsprintfA(nm, "ObjectStublessClient%d", n);
        FARPROC p = GetProcAddress(c, nm);
        if (p && p != prev) distinct++;
        prev = p;
    }
    EQ(distinct, 30);                                       /* 30 different thunks */
    /* each thunk is ordered in memory by its slot number (they are laid out back to back) */
    FARPROC a = GetProcAddress(c, "ObjectStublessClient3"), b = GetProcAddress(c, "ObjectStublessClient4"), d = GetProcAddress(c, "ObjectStublessClient32");
    if (a && b && d && !t_is_wine()) CHECK((ULONG_PTR)a < (ULONG_PTR)b && (ULONG_PTR)b < (ULONG_PTR)d);
}

TEST(psde_ndr_stub_call_surface) {
    /* The generated-stub entry points exist and are not forwarders to a missing module. */
    HMODULE r = t_module("rpcrt4.dll");
    static const char *const n[] = { "NdrStubCall2", "NdrStubCall3", "NdrServerCall2", "NdrServerCallAll", "NdrServerCallNdr64",
        "NdrAsyncServerCall", "Ndr64AsyncServerCallAll", "NdrDcomAsyncStubCall", "Ndr64DcomAsyncStubCall", "NdrDcomAsyncClientCall", "Ndr64DcomAsyncClientCall" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        FARPROC p = GetProcAddress(r, n[i]);
        EXPCHK(p != NULL, "rpcrt4!%s", n[i]);
        if (p) {
            MEMORY_BASIC_INFORMATION mbi;
            CHECKF(VirtualQuery(p, &mbi, sizeof mbi) && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)), "%s is not executable code", n[i]);
        }
    }
}

/* ---- ComPs_* : ole32's private flavour of the Ndr proxy-file entry points ---------------------------- */
TEST(psde_comps_can_unload_now_always_false) {
    typedef HRESULT (WINAPI *fn_t)(void *);
    fn_t f = (fn_t)t_proc_in("ole32.dll", "ComPs_NdrDllCanUnloadNow");
    if (!f) { t_skip("no ComPs_NdrDllCanUnloadNow"); return; }
    if (t_is_wine()) { t_skip("wine: different semantics"); return; }
    /* the Win10 body is `return 1` (it shares code with IsValidPtrIn/CoQueryReleaseObject): never S_OK */
    EQHR(f(NULL), S_FALSE);
    CStdPSFactoryBuffer empty; memset(&empty, 0, sizeof empty);
    EQHR(f(&empty), S_FALSE);
    EQHR(f((void *)1), S_FALSE);                            /* argument is ignored */
}

TEST(psde_comps_getclassobject) {
    typedef HRESULT (WINAPI *fn_t)(REFCLSID, REFIID, void **, const ProxyFileInfo **, const CLSID *, CStdPSFactoryBuffer *);
    fn_t f = (fn_t)t_proc_in("ole32.dll", "ComPs_NdrDllGetClassObject");
    if (!f || !psd_init()) { t_skip("no ComPs_NdrDllGetClassObject"); return; }
    if (t_is_wine()) { t_skip("wine's ole32 has no ComPs_ entry points"); return; }
    PSD_STA();
    void *pv = (void *)1;
    HRESULT hr = f(&psd_CLSID_Bogus, &psd_IID_IClassFactory, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    EQHR(hr, CLASS_E_CLASSNOTAVAILABLE);
    CHECK(pv == NULL || pv == (void *)1);
    EQ(psd_psf.RefCount, 0);
    pv = NULL;
    hr = f(&psd_CLSID_PSF, &psd_IID_IClassFactory, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    EQHR(hr, S_OK);                                          /* an IClassFactory wrapper around the PS factory */
    CHECK(pv != NULL);
    if (hr == S_OK && pv) {
        CHECK(pv != (void *)&psd_psf);
        IUnknown_Release((IUnknown *)pv);
    }
    t_note("factory RefCount after releasing the ComPs wrapper: %ld", (long)psd_psf.RefCount);
    while (psd_psf.RefCount > 0) IPSFactoryBuffer_Release((IPSFactoryBuffer *)&psd_psf);
    pv = NULL;
    hr = f(&psd_IID_One, &psd_IID_IUnknown, &pv, psd_file_list, NULL, &psd_psf);
    t_note("ComPs_NdrDllGetClassObject(by IID, riid=IUnknown) = 0x%08lx", hr);
    if (hr == S_OK && pv) IUnknown_Release((IUnknown *)pv);
    pv = NULL;
    hr = f(&psd_CLSID_PSF, &psd_IID_IPSFactoryBuffer, &pv, psd_file_list, &psd_CLSID_PSF, &psd_psf);
    t_note("ComPs_NdrDllGetClassObject(riid=IPSFactoryBuffer) = 0x%08lx pv=%p", hr, pv);
    if (hr == S_OK && pv) IUnknown_Release((IUnknown *)pv);
    PSD_DONE();
}
