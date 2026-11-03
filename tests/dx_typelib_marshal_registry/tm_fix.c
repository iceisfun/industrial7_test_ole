/* Fixture, sweeping and raw registry helpers. */
#include "tm.h"

static const GUID *const g_libs[] = { &TM_LIBID_Main, &TM_LIBID_Mini, &TM_LIBID_Ver, &TM_LIBID_Lcid, &TM_LIBID_Other, NULL };
static const GUID *const g_ifaces[] = {
    &TM_IID_IOletSink, &TM_IID_IOletCustom, &TM_IID_IOletDual, &TM_IID_IOletDisp, &TM_IID_IOletPlain, &TM_IID_IOletProxyFlag,
    &TM_IID_IOletDualNE, &TM_IID_Mini1, &TM_IID_Mini2, &TM_IID_Mini3, &TM_IID_Mini4, &TM_IID_Mini5, &TM_IID_Mini6,
    &TM_IID_Mini7, &TM_IID_Mini8, &TM_IID_MiniDisp, &TM_IID_MiniDual, &TM_IID_MiniXu1, &TM_IID_MiniXu2, &TM_IID_MiniXu3,
    &TM_IID_MiniXu4, NULL };

void tm_guid_str(const GUID *g, WCHAR out[40])
{
    _snwprintf(out, 40, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3, g->Data4[0],
               g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
    out[39] = 0;
}
void tm_ver_str(WORD maj, WORD min, WCHAR out[16]) { _snwprintf(out, 16, L"%x.%x", maj, min); out[15] = 0; }

BOOL tm_file_exists(const WCHAR *p) { DWORD a = GetFileAttributesW(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }

static const WCHAR *prefix_for(HKEY root) { return root == HKEY_CLASSES_ROOT ? L"" : L"Software\\Classes\\"; }

void tm_tl_path(HKEY root, const GUID *libid, WORD maj, WORD min, const WCHAR *tail, WCHAR *out, size_t cch)
{
    WCHAR g[40], v[16];
    tm_guid_str(libid, g);
    if (maj == 0xffff && min == 0xffff) _snwprintf(out, cch, L"%lsTypeLib\\%ls", prefix_for(root), g);
    else {
        tm_ver_str(maj, min, v);
        _snwprintf(out, cch, L"%lsTypeLib\\%ls\\%ls%ls%ls", prefix_for(root), g, v, tail ? L"\\" : L"", tail ? tail : L"");
    }
    out[cch - 1] = 0;
}
void tm_if_path(HKEY root, const GUID *iid, const WCHAR *tail, WCHAR *out, size_t cch)
{
    WCHAR g[40];
    tm_guid_str(iid, g);
    _snwprintf(out, cch, L"%lsInterface\\%ls%ls%ls", prefix_for(root), g, tail ? L"\\" : L"", tail ? tail : L"");
    out[cch - 1] = 0;
}

LONG tm_reg_sz(HKEY root, const WCHAR *path, const WCHAR *value, WCHAR *out, DWORD cch, REGSAM wow)
{
    HKEY k; DWORD type = 0, cb = (cch - 1) * sizeof(WCHAR); LONG r;
    if (cch) out[0] = 0;
    r = RegOpenKeyExW(root, path, 0, KEY_READ | wow, &k);
    if (r) return r;
    r = RegQueryValueExW(k, value, NULL, &type, (BYTE *)out, &cb);
    if (!r && type != REG_SZ && type != REG_EXPAND_SZ) r = ERROR_INVALID_DATATYPE;
    if (!r) out[cb / sizeof(WCHAR)] = 0;
    RegCloseKey(k);
    return r;
}
int tm_reg_key_exists(HKEY root, const WCHAR *path, REGSAM wow)
{
    HKEY k; if (RegOpenKeyExW(root, path, 0, KEY_READ | wow, &k)) return 0; RegCloseKey(k); return 1;
}
int tm_reg_subkey_count(HKEY root, const WCHAR *path, REGSAM wow)
{
    HKEY k; DWORD n = 0;
    if (RegOpenKeyExW(root, path, 0, KEY_READ | wow, &k)) return -1;
    RegQueryInfoKeyW(k, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    RegCloseKey(k);
    return (int)n;
}
int tm_reg_value_type(HKEY root, const WCHAR *path, const WCHAR *value, REGSAM wow)
{
    HKEY k; DWORD t = 0; LONG r;
    if (RegOpenKeyExW(root, path, 0, KEY_READ | wow, &k)) return -1;
    r = RegQueryValueExW(k, value, NULL, &t, NULL, NULL);
    RegCloseKey(k);
    return r ? -1 : (int)t;
}
int tm_hkcu_has_lib(const GUID *libid) { WCHAR p[200]; tm_tl_path(tm_ur(), libid, 0xffff, 0xffff, NULL, p, 200); return tm_reg_key_exists(tm_ur(), p, 0); }
int tm_hkcr_has_lib(const GUID *libid) { WCHAR p[200]; tm_tl_path(HKEY_CLASSES_ROOT, libid, 0xffff, 0xffff, NULL, p, 200); return tm_reg_key_exists(HKEY_CLASSES_ROOT, p, 0); }
int tm_hkcr_has_iface(const GUID *iid) { WCHAR p[200]; tm_if_path(HKEY_CLASSES_ROOT, iid, NULL, p, 200); return tm_reg_key_exists(HKEY_CLASSES_ROOT, p, 0); }

static void del_tree(HKEY root, const WCHAR *path)
{
    REGSAM w[3] = { 0, KEY_WOW64_32KEY, KEY_WOW64_64KEY };
    int i;
    for (i = 0; i < 3; i++) {
        HKEY k;
        if (RegOpenKeyExW(root, path, 0, DELETE | KEY_READ | KEY_WRITE | w[i], &k) == 0) {
            RegCloseKey(k);
            RegDeleteTreeW(root, path);   /* default view */
            if (w[i]) {   /* explicit views: reopen the parent in that view is not possible here; best effort via RegDeleteKeyEx */ }
        }
    }
}
/* delete a subtree in a given view through the parent key */
static void del_tree_view(HKEY root, const WCHAR *parent, const WCHAR *child, REGSAM wow)
{
    HKEY k;
    if (RegOpenKeyExW(root, parent, 0, KEY_READ | KEY_WRITE | DELETE | wow, &k)) return;
    RegDeleteTreeW(k, child);
    RegDeleteKeyExW(k, child, wow, 0);
    RegCloseKey(k);
}

void tm_sweep(void)
{
    HKEY roots[2] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    REGSAM views[2] = { KEY_WOW64_64KEY, KEY_WOW64_32KEY };
    int r, v, i;
    for (r = 0; r < 2; r++)
        for (v = 0; v < 2; v++) {
            for (i = 0; g_libs[i]; i++) { WCHAR g[40]; tm_guid_str(g_libs[i], g); del_tree_view(roots[r], L"Software\\Classes\\TypeLib", g, views[v]); }
            for (i = 0; g_ifaces[i]; i++) { WCHAR g[40]; tm_guid_str(g_ifaces[i], g); del_tree_view(roots[r], L"Software\\Classes\\Interface", g, views[v]); }
        }
}

static int g_atexit;
static void sweep_atexit(void) { tm_sweep(); }

/* Probe: can this process create keys under HKCU\Software\Classes at all? */
static int probe_hkcu(void)
{
    HKEY k; LONG r;
    r = RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\OletProbe_dx_typelib", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    if (r) return 0;
    RegCloseKey(k);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\OletProbe_dx_typelib");
    return 1;
}

int tm_begin_bare(void)
{
    if (!g_atexit) { g_atexit = 1; atexit(sweep_atexit); }
    if (!U(RegisterTypeLib) || !U(LoadRegTypeLib)) { t_skip("oleaut32 typelib registration exports missing"); return 0; }
    if (!probe_hkcu()) { t_skip("cannot write HKCU\\Software\\Classes"); return 0; }
    tm_sweep();
    return 1;
}

HRESULT tm_register_main(void)
{
    ITypeLib *tl = NULL; HRESULT hr; const WCHAR *p = tm_main_path();
    if (!p) return E_FAIL;
    hr = U(LoadTypeLibEx)(p, REGKIND_NONE, &tl);
    if (FAILED(hr)) return hr;
    hr = pRegisterTypeLibForUser(tl, (OLECHAR *)p, NULL);
    ITypeLib_Release(tl);
    return hr;
}

int tm_begin(void)
{
    HRESULT hr;
    if (!tm_begin_bare()) return 0;
    if (!tm_main_path()) { t_skip("cannot build the test typelib"); return 0; }
    hr = tm_register_main();
    if (FAILED(hr)) { t_skip("RegisterTypeLibForUser failed hr=0x%lx", (long)hr); tm_sweep(); return 0; }
    return 1;
}

void tm_end(void)
{
    if (pUnRegisterTypeLibForUser) pUnRegisterTypeLibForUser((GUID *)&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, SYS_WIN64);
    tm_sweep();
}
