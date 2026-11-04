/* dx_gap_03: OaEnablePerUserTLibRegistration. The flag is process-global, so each variant runs in its own child process.
 * Decomp (oleaut32): RegisterTypeLib with the flag set and IsUserHiveOK() true registers through HKEY_CLASSES_ROOT, without
 * it through OpenClassesRootKeyExW. Both children register a throw-away type library with a pid-derived GUID and remove it;
 * only the single TypeLib\{guid} subtree they created is ever deleted. */
#define COBJMACROS
#include "t.h"
#include <oleauto.h>

#define GUID_PREFIX L"{0E7C0DE0-06A9-4E7C-B1C2-D3E4"

static void mkguid(GUID *g) {
    GUID t = {0x0e7c0de0, 0x6a9, 0x4e7c, {0xb1, 0xc2, 0xd3, 0xe4, 0, 0, 0, 0}};
    DWORD pid = GetCurrentProcessId();
    *g = t; memcpy(g->Data4 + 4, &pid, 4);
}
static int key_exists(HKEY root, const WCHAR *sub) {
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return 0;
    RegCloseKey(k); return 1;
}
/* delete one TypeLib\{guid} subtree below a "Software\Classes" parent; refuses anything but our own GUID prefix */
static void del_tlb(HKEY root, const WCHAR *parent, const WCHAR *gs) {
    if (wcsncmp(gs, GUID_PREFIX, lstrlenW(GUID_PREFIX)) != 0) return;
    WCHAR sub[200]; wsprintfW(sub, L"%s\\TypeLib\\%s", parent, gs);
    HKEY k;
    if (RegOpenKeyExW(root, sub, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) { RegCloseKey(k); RegDeleteTreeW(root, sub); }
}
static void sweep(HKEY root, const WCHAR *parent) {
    WCHAR tl[160]; wsprintfW(tl, L"%s\\TypeLib", parent);
    HKEY k;
    if (RegOpenKeyExW(root, tl, 0, KEY_READ | KEY_WOW64_64KEY, &k) != ERROR_SUCCESS) return;
    WCHAR names[64][64]; int n = 0;
    for (DWORD i = 0; n < 64; i++) {
        WCHAR nm[128]; DWORD len = 128;
        if (RegEnumKeyExW(k, i, nm, &len, NULL, NULL, NULL, NULL) != ERROR_SUCCESS) break;
        if (len < 64 && !wcsncmp(nm, GUID_PREFIX, lstrlenW(GUID_PREFIX))) lstrcpyW(names[n++], nm);
    }
    RegCloseKey(k);
    for (int i = 0; i < n; i++) del_tlb(root, parent, names[i]);
}


CHILD(dxg3_tlib_reg) {
    typedef void (WINAPI *oa_fn)(void);
    int per_user = argc > 1 && argv[1][0] == '1';
    oa_fn en = (oa_fn)t_proc("OaEnablePerUserTLibRegistration");
    if (per_user && !en) return 90;
    if (!U(CreateTypeLib2) || !U(LoadTypeLibEx) || !U(RegisterTypeLib) || !U(UnRegisterTypeLib) || !U(CoInitializeEx)) return 91;
    if (per_user) en();
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) return 92;
    GUID g; mkguid(&g);
    WCHAR gs[64]; U(StringFromGUID2)(&g, gs, 64);
    WCHAR path[300], sub[100];
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, path, 300);
    lstrcatW(path, L"olet_gap3_tlb.tlb");
    wsprintfW(sub, L"TypeLib\\%s", gs);
    int rc = 0, bits = 0;
    ICreateTypeLib2 *ctl = NULL; ITypeLib *tl = NULL;
    if (FAILED(U(CreateTypeLib2)(SYS_WIN64, path, &ctl))) { rc = 1; goto done; }
    ICreateTypeLib2_SetGuid(ctl, &g);
    ICreateTypeLib2_SetVersion(ctl, 1, 0);
    ICreateTypeLib2_SetName(ctl, L"OletGap3Lib");
    if (FAILED(ICreateTypeLib2_SaveAllChanges(ctl))) { rc = 2; goto done; }
    ICreateTypeLib2_Release(ctl); ctl = NULL;
    if (FAILED(U(LoadTypeLibEx)(path, REGKIND_NONE, &tl))) { rc = 3; goto done; }
    if (U(RegisterTypeLib)(tl, path, NULL) != S_OK) { rc = 4; goto done; }
    if (key_exists(HKEY_CURRENT_USER, (wsprintfW(path, L"Software\\Classes\\%s", sub), path))) bits |= 1;
    if (key_exists(HKEY_LOCAL_MACHINE, path)) bits |= 2;
    if (key_exists(HKEY_CLASSES_ROOT, sub)) bits |= 4; else rc = 5;
    /* Win10 registers the library under the LCID stored in the typelib (the creating thread's default, not 0), and
       UnRegisterTypeLib only finds it under that exact LCID: LANG_NEUTRAL gives TYPE_E_REGISTRYACCESS */
    LCID lcid = 0;
    { TLIBATTR *ta = NULL; if (SUCCEEDED(ITypeLib_GetLibAttr(tl, &ta)) && ta) { lcid = ta->lcid; ITypeLib_ReleaseTLibAttr(tl, ta); } }
    HRESULT uhr = U(UnRegisterTypeLib)(&g, 1, 0, lcid, SYS_WIN64);
    if (uhr != S_OK) rc = rc ? rc : 6;
    if (key_exists(HKEY_CLASSES_ROOT, sub)) rc = rc ? rc : 7;
done:
    if (tl) ITypeLib_Release(tl);
    if (ctl) ICreateTypeLib2_Release(ctl);
    del_tlb(HKEY_CURRENT_USER, L"Software\\Classes", gs);
    del_tlb(HKEY_LOCAL_MACHINE, L"Software\\Classes", gs);
    U(CoUninitialize)();
    return rc | (bits << 8);
}

static void run(int per_user) {
    sweep(HKEY_CURRENT_USER, L"Software\\Classes");
    sweep(HKEY_LOCAL_MACHINE, L"Software\\Classes");
    DWORD rc = t_run_child("dxg3_tlib_reg", per_user ? "1" : "0", 20000);
    CHECKF((rc & 0xff) == 0, "per-user=%d: child failed at stage %lu (exit 0x%lx)", per_user, (unsigned long)(rc & 0xff), (unsigned long)rc);
    t_note("per-user flag %d: registration visible in HKCU=%d HKLM=%d HKCR=%d", per_user, !!((rc >> 8) & 1), !!((rc >> 8) & 2), !!((rc >> 8) & 4));
    sweep(HKEY_CURRENT_USER, L"Software\\Classes");
    sweep(HKEY_LOCAL_MACHINE, L"Software\\Classes");
}
TEST(dxg3_oa_tlib_registration_default_mode) {
    if (t_is_wine()) { t_skip("wine: registry/typelib registration differs"); return; }
    run(0);
}
TEST(dxg3_oa_tlib_registration_per_user_mode) {
    if (t_is_wine()) { t_skip("wine: OaEnablePerUserTLibRegistration is not implemented"); return; }
    run(1);
}
