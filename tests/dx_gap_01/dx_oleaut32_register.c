#include "dx1.h"
/* oleaut32!DllRegisterServer registers the ITypeInfo2/ITypeLib2 proxy-stub entries under HKCR\Interface, the standard
 * types (StdTypesRegisterServer / OcxDllRegisterServer) and stdole2.tlb. On Windows these keys already exist with exactly
 * these values, so the call is idempotent; it needs write access to HKLM\Software\Classes (administrator). */

static int get_default(const WCHAR *path, const WCHAR *name, WCHAR *out, DWORD cch)
{
    HKEY k; LONG r; DWORD sz = cch * 2, type = 0;
    r = RegOpenKeyExW(HKEY_CLASSES_ROOT, path, 0, KEY_READ, &k);
    if (r) return 0;
    r = RegQueryValueExW(k, name, NULL, &type, (BYTE *)out, &sz);
    RegCloseKey(k);
    return r == 0 && type == REG_SZ;
}

TEST(dx_gap1_oleaut32_dllregisterserver_interface_keys)
{
    typedef HRESULT (WINAPI *srv_fn)(void);
    srv_fn f = (srv_fn)t_proc_in("oleaut32.dll", "DllRegisterServer"); HRESULT hr; WCHAR v[260]; int i;
    static const struct { const WCHAR *key; const WCHAR *name; const WCHAR *val; } tab[] = {
        { L"Interface\\{00020412-0000-0000-C000-000000000046}", NULL, L"ITypeInfo2" },
        { L"Interface\\{00020412-0000-0000-C000-000000000046}\\NumMethods", NULL, L"32" },
        { L"Interface\\{00020412-0000-0000-C000-000000000046}\\ProxyStubClsid32", NULL, L"{00020420-0000-0000-C000-000000000046}" },
        { L"Interface\\{00020411-0000-0000-C000-000000000046}", NULL, L"ITypeLib2" },
        { L"Interface\\{00020411-0000-0000-C000-000000000046}\\NumMethods", NULL, L"16" },
        { L"Interface\\{00020411-0000-0000-C000-000000000046}\\ProxyStubClsid32", NULL, L"{00020420-0000-0000-C000-000000000046}" },
    };
    REQUIRE(f);
    if (t_is_wine()) { t_skip("registration is emulator specific"); return; }
    if (!dx_is_admin()) {
        hr = f(); t_note("not elevated: DllRegisterServer -> 0x%08lx", (unsigned long)hr);
        CHECK(FAILED(hr)); t_skip("needs administrator for the success path"); return;
    }
    hr = f(); EXPECT_EQ(hr, S_OK);
    hr = f(); EXPECT_EQ(hr, S_OK);                       /* idempotent */
    for (i = 0; i < 6; i++) {
        v[0] = 0;
        CHECKF(get_default(tab[i].key, tab[i].name, v, 260), "missing %ls", tab[i].key);
        CHECKF(!wcscmp(v, tab[i].val), "%ls = %ls (want %ls)", tab[i].key, v, tab[i].val);
    }
}

TEST(dx_gap1_oleaut32_dllregisterserver_registers_stdole2)
{
    typedef HRESULT (WINAPI *srv_fn)(void);
    srv_fn f = (srv_fn)t_proc_in("oleaut32.dll", "DllRegisterServer"); WCHAR v[300]; HKEY k;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("registration is emulator specific"); return; }
    if (!dx_is_admin()) { t_skip("needs administrator"); return; }
    EXPECT_EQ(f(), S_OK);
    CHECK(RegOpenKeyExW(HKEY_CLASSES_ROOT, L"TypeLib\\{00020430-0000-0000-C000-000000000046}\\2.0", 0, KEY_READ, &k) == 0);
    if (get_default(L"TypeLib\\{00020430-0000-0000-C000-000000000046}\\2.0\\0\\win64", NULL, v, 300)) {
        size_t n = wcslen(v);
        CHECKF(n >= 11 && !_wcsicmp(v + n - 11, L"stdole2.tlb"), "win64 path %ls", v);
    } else CHECK(0 && "stdole2 win64 entry missing");
    if (k) RegCloseKey(k);
}
