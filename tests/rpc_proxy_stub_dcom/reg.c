/* NdrDllRegisterProxy / NdrDllUnregisterProxy.  The argument-validation tests touch nothing.  The registration
 * test redirects HKEY_CLASSES_ROOT to a private key under HKCU for this process (RegOverridePredefKey), registers
 * the hand-built proxy file, inspects what was written and unregisters again; the private key is swept at the start
 * and the end.  Skipped when the override or the key cannot be created. */
#include "psd.h"
#include "modules.h"

static void guid_str(const GUID *g, char *out) {
    wsprintfA(out, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1],
              g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}

TEST(psdg_register_invalid_arguments) {
    if (!UIN("rpcrt4.dll", NdrDllRegisterProxy) || !psd_init()) { t_skip("no NdrDllRegisterProxy"); return; }
    PSD_STA();
    HMODULE hmod = GetModuleHandleA(NULL);
    HRESULT hr = UIN("rpcrt4.dll", NdrDllRegisterProxy)(NULL, NULL, NULL);
    EQHR(hr, E_HANDLE);
    const ProxyFileInfo *nolist = NULL;
    hr = UIN("rpcrt4.dll", NdrDllRegisterProxy)(hmod, &nolist, NULL);       /* empty file list */
    EQHR(hr, E_NOINTERFACE);
    static const CInterfaceProxyVtbl *no_proxies[1] = { NULL };
    static const CInterfaceStubVtbl *no_stubs[1] = { NULL };
    static ProxyFileInfo empty = { (const PCInterfaceProxyVtblList *)no_proxies, (const PCInterfaceStubVtblList *)no_stubs, NULL, NULL, NULL, 0, 1, NULL, 0, 0, 0 };
    static const ProxyFileInfo *empty_list[2] = { &empty, NULL };
    hr = UIN("rpcrt4.dll", NdrDllRegisterProxy)(hmod, empty_list, NULL);    /* a file with no interfaces */
    EQHR(hr, E_NOINTERFACE);
    PSD_DONE();
}

static int key_exists(HKEY root, const char *sub) {
    HKEY k;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    RegCloseKey(k);
    return 1;
}
static int get_default(HKEY root, const char *sub, char *out, DWORD cap) {
    HKEY k;
    out[0] = 0;
    if (RegOpenKeyExA(root, sub, 0, KEY_READ, &k) != ERROR_SUCCESS) return 0;
    DWORD ty = 0, n = cap;
    LONG r = RegQueryValueExA(k, NULL, NULL, &ty, (BYTE *)out, &n);
    RegCloseKey(k);
    return r == ERROR_SUCCESS && ty == REG_SZ;
}

TEST(psdg_register_unregister_hkcu_sandbox) {
    if (!UIN("rpcrt4.dll", NdrDllRegisterProxy) || !UIN("rpcrt4.dll", NdrDllUnregisterProxy) || !psd_init()) { t_skip("no register exports"); return; }
    PSD_STA();
    char sandbox[96];
    wsprintfA(sandbox, "Software\\%s_psd_%lu", T_TMP_PREFIX, GetCurrentProcessId());
    RegDeleteTreeA(HKEY_CURRENT_USER, sandbox);                 /* sweep leftovers */
    HKEY sb = NULL;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, sandbox, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &sb, NULL) != ERROR_SUCCESS) { t_skip("cannot create HKCU sandbox key"); PSD_DONE(); return; }
    LONG ov = RegOverridePredefKey(HKEY_CLASSES_ROOT, sb);
    if (ov != ERROR_SUCCESS) {
        t_skip("RegOverridePredefKey(HKCR) failed: %ld", ov);
        RegCloseKey(sb); RegDeleteTreeA(HKEY_CURRENT_USER, sandbox); PSD_DONE(); return;
    }
    HMODULE hmod = GetModuleHandleA(NULL);
    char self[MAX_PATH]; GetModuleFileNameA(hmod, self, sizeof self);
    char clsid[64], iid1[64], iid2[64], path[512], val[512];
    guid_str(&psd_CLSID_PSF, clsid); guid_str(&psd_IID_One, iid1); guid_str(&psd_IID_Two, iid2);

    HRESULT hr = UIN("rpcrt4.dll", NdrDllRegisterProxy)(hmod, psd_file_list, &psd_CLSID_PSF);
    if (hr == E_ACCESSDENIED) {
        t_skip("registration denied (0x%08lx)", hr);
    } else {
        EQHR(hr, S_OK);
        if (hr == S_OK) {
            int in_sb = key_exists(sb, "Interface");
            int in_hkcu = key_exists(HKEY_CURRENT_USER, "Software\\Classes\\Interface");
            char ifkey[160];
            wsprintfA(ifkey, "Interface\\%s\\ProxyStubClsid32", iid1);
            HKEY root = sb;
            char pre[64] = "";
            if (!key_exists(sb, ifkey)) {
                wsprintfA(path, "Software\\Classes\\%s", ifkey);
                if (key_exists(HKEY_CURRENT_USER, path)) { root = HKEY_CURRENT_USER; strcpy(pre, "Software\\Classes\\"); }
                else if (key_exists(HKEY_LOCAL_MACHINE, path)) { root = HKEY_LOCAL_MACHINE; strcpy(pre, "Software\\Classes\\"); }
                else root = NULL;
            }
            t_note("registration landed in: %s (sandbox has Interface=%d, HKCU Classes has Interface=%d)",
                   root == sb ? "override key" : root == HKEY_CURRENT_USER ? "HKCU\\Software\\Classes" : root ? "HKLM\\Software\\Classes" : "(not found)", in_sb, in_hkcu);
            CHECK(root != NULL);
            if (root) {
                wsprintfA(path, "%sInterface\\%s\\ProxyStubClsid32", pre, iid1);
                CHECK(get_default(root, path, val, sizeof val));
                CHECKF(!lstrcmpiA(val, clsid), "IOletOne ProxyStubClsid32 = \"%s\" want \"%s\"", val, clsid);
                wsprintfA(path, "%sInterface\\%s\\ProxyStubClsid32", pre, iid2);
                CHECK(get_default(root, path, val, sizeof val));
                CHECKF(!lstrcmpiA(val, clsid), "IOletTwo ProxyStubClsid32 = \"%s\" want \"%s\"", val, clsid);
                wsprintfA(path, "%sInterface\\%s", pre, iid1);
                if (get_default(root, path, val, sizeof val)) t_note("Interface\\IOletOne default = \"%s\"", val);
                wsprintfA(path, "%sInterface\\%s\\NumMethods", pre, iid1);
                if (get_default(root, path, val, sizeof val)) t_note("Interface\\IOletOne\\NumMethods = \"%s\"", val);
                wsprintfA(path, "%sInterface\\%s\\NumMethods", pre, iid2);
                if (get_default(root, path, val, sizeof val)) t_note("Interface\\IOletTwo\\NumMethods = \"%s\"", val);
                wsprintfA(path, "%sCLSID\\%s\\InprocServer32", pre, clsid);
                CHECK(get_default(root, path, val, sizeof val));
                CHECKF(!lstrcmpiA(val, self), "InprocServer32 = \"%s\" want \"%s\"", val, self);
                HKEY k;
                if (RegOpenKeyExA(root, path, 0, KEY_READ, &k) == ERROR_SUCCESS) {
                    DWORD n = sizeof val, ty;
                    if (RegQueryValueExA(k, "ThreadingModel", NULL, &ty, (BYTE *)val, &n) == ERROR_SUCCESS) t_note("ThreadingModel = \"%s\"", val);
                    RegCloseKey(k);
                }
            }
            hr = UIN("rpcrt4.dll", NdrDllUnregisterProxy)(hmod, psd_file_list, &psd_CLSID_PSF);
            EQHR(hr, S_OK);
            if (root) {
                wsprintfA(path, "%sInterface\\%s\\ProxyStubClsid32", pre, iid1);
                CHECK(!key_exists(root, path));
                wsprintfA(path, "%sInterface\\%s\\ProxyStubClsid32", pre, iid2);
                CHECK(!key_exists(root, path));
                wsprintfA(path, "%sCLSID\\%s\\InprocServer32", pre, clsid);
                CHECK(!key_exists(root, path));
            }
        }
    }
    /* make sure nothing is left behind wherever it landed */
    UIN("rpcrt4.dll", NdrDllUnregisterProxy)(hmod, psd_file_list, &psd_CLSID_PSF);
    RegOverridePredefKey(HKEY_CLASSES_ROOT, NULL);
    RegCloseKey(sb);
    RegDeleteTreeA(HKEY_CURRENT_USER, sandbox);
    PSD_DONE();
}
