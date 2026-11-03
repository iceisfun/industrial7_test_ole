/* Interface\{iid} registrations written by RegisterTypeLib for the interfaces of the main test library. */
#include "reg.h"

static WCHAR g_clsid_psoa[40], g_clsid_psdisp[40], g_libid_str[40];
static void init_strs(void)
{
    tm_guid_str(&TM_CLSID_PSOAInterface, g_clsid_psoa);
    tm_guid_str(&TM_CLSID_PSDispatch, g_clsid_psdisp);
    tm_guid_str(&TM_LIBID_Main, g_libid_str);
}
static int rd(const GUID *iid, const WCHAR *tail, const WCHAR *val, WCHAR *out, REGSAM wow)
{
    return rg_if_cu(iid, tail, val, out, 300, wow, NULL);
}

static void check_automation_iface(const GUID *iid, const WCHAR *name)
{
    WCHAR v[300];
    init_strs();
    CHECKF(rg_if_exists(tm_ur(), iid, NULL, 0), "Interface key for %ls", name);
    CHECKF(rd(iid, NULL, NULL, v, 0), "default value of %ls", name);
    CHECKF(tm_weq(v, name), "default value \"%ls\" want \"%ls\"", v, name);
    CHECK(rd(iid, L"ProxyStubClsid32", NULL, v, 0));
    CHECKF(tm_weq(v, g_clsid_psoa), "ProxyStubClsid32 \"%ls\"", v);
    CHECK(rd(iid, L"TypeLib", NULL, v, 0));
    CHECKF(tm_weq(v, g_libid_str), "TypeLib default \"%ls\"", v);
    CHECK(rd(iid, L"TypeLib", L"Version", v, 0));
    CHECKF(tm_weq(v, L"b.1a"), "TypeLib Version \"%ls\"", v);
    CHECK(!rg_if_exists(tm_ur(), iid, L"ProxyStubClsid", 0));      /* only the 32-bit value is written */
    CHECK(!rg_if_exists(tm_ur(), iid, L"NumMethods", 0));
    CHECK(!rg_if_exists(tm_ur(), iid, L"BaseInterface", 0));
}

TMTEST(dxtm_regif_custom_oleautomation) { check_automation_iface(&TM_IID_IOletCustom, L"IOletCustom"); }
TMTEST(dxtm_regif_sink_oleautomation)   { check_automation_iface(&TM_IID_IOletSink, L"IOletSink"); }
TMTEST(dxtm_regif_dual_interface)       { check_automation_iface(&TM_IID_IOletDual, L"IOletDual"); }

TMTEST(dxtm_regif_pure_dispinterface_uses_psdispatch)
{
    WCHAR v[300];
    init_strs();
    CHECK(rg_if_exists(tm_ur(), &TM_IID_IOletDisp, NULL, 0));
    CHECK(rd(&TM_IID_IOletDisp, NULL, NULL, v, 0));
    EXPECT_WSTR(v, L"IOletDisp");
    CHECK(rd(&TM_IID_IOletDisp, L"ProxyStubClsid32", NULL, v, 0));
    CHECKF(tm_weq(v, g_clsid_psdisp), "ProxyStubClsid32 \"%ls\" want PSDispatch", v);
    CHECK(rd(&TM_IID_IOletDisp, L"TypeLib", NULL, v, 0));
    EXPECT_WSTR(v, g_libid_str);
}

TMTEST(dxtm_regif_plain_interface_not_registered)
{
    /* A TKIND_INTERFACE without oleautomation/dual flags gets no Interface registration at all. */
    CHECK(!rg_if_exists(tm_ur(), &TM_IID_IOletPlain, NULL, 0));
    CHECK(!tm_hkcr_has_iface(&TM_IID_IOletPlain));
}

TMTEST(dxtm_regif_proxy_flag_name_only)
{
    /* TYPEFLAG_FPROXY: the Interface key carries the name but neither ProxyStubClsid32 nor a TypeLib link. */
    WCHAR v[300];
    CHECK(rg_if_exists(tm_ur(), &TM_IID_IOletProxyFlag, NULL, 0));
    CHECK(rd(&TM_IID_IOletProxyFlag, NULL, NULL, v, 0));
    EXPECT_WSTR(v, L"IOletProxyFlag");
    CHECK(!rg_if_exists(tm_ur(), &TM_IID_IOletProxyFlag, L"ProxyStubClsid32", 0));
    CHECK(!rg_if_exists(tm_ur(), &TM_IID_IOletProxyFlag, L"TypeLib", 0));
}

TMTEST(dxtm_regif_non_interface_kinds_not_registered)
{
    WCHAR p[260];
    /* enum, records and coclass produce no Interface key */
    CHECK(!tm_hkcr_has_iface(&TM_GUID_OletPoint));
    CHECK(!tm_hkcr_has_iface(&TM_GUID_OletMix));
    CHECK(!tm_hkcr_has_iface(&TM_CLSID_OletCo));
    /* and RegisterTypeLib does not write CLSID entries for the coclass */
    {
        WCHAR g[40]; tm_guid_str(&TM_CLSID_OletCo, g);
        _snwprintf(p, 260, L"%lsCLSID\\%ls", tm_ur() == HKEY_CLASSES_ROOT ? L"" : L"Software\\Classes\\", g);
        CHECK(!tm_reg_key_exists(tm_ur(), p, 0));
        CHECK(!tm_reg_key_exists(HKEY_CLASSES_ROOT, L"CLSID\\{0b1d0020-7e57-4a11-9c11-01a1c0de0001}", 0));
    }
}

TMTEST(dxtm_regif_both_registry_views)
{
    /* The Interface keys are written through the 64-bit and the 32-bit registry view. */
    WCHAR v[300];
    CHECK(rd(&TM_IID_IOletCustom, L"ProxyStubClsid32", NULL, v, KEY_WOW64_64KEY));
    CHECK(rd(&TM_IID_IOletCustom, L"ProxyStubClsid32", NULL, v, KEY_WOW64_32KEY));
    CHECK(rd(&TM_IID_IOletDual, L"TypeLib", L"Version", v, KEY_WOW64_32KEY));
}

TMTEST(dxtm_regif_hkcr_merged_view)
{
    WCHAR p[260], v[300];
    tm_if_path(HKEY_CLASSES_ROOT, &TM_IID_IOletCustom, L"ProxyStubClsid32", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletDual));
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletSink));
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletDisp));
}

TMTEST(dxtm_regif_value_types_are_reg_sz)
{
    WCHAR p[260];
    tm_if_path(tm_ur(), &TM_IID_IOletDual, NULL, p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    tm_if_path(tm_ur(), &TM_IID_IOletDual, L"ProxyStubClsid32", p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    tm_if_path(tm_ur(), &TM_IID_IOletDual, L"TypeLib", p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    EQ(tm_reg_value_type(tm_ur(), p, L"Version", 0), REG_SZ);
}

TMTEST(dxtm_regif_main_typelib_keys)
{
    TMLibSpec s; WCHAR v[300], path[260];
    memset(&s, 0, sizeof s); s.libid = TM_LIBID_Main; s.maj = TM_MAIN_MAJ; s.min = TM_MAIN_MIN;
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    CHECK(rg_cu_exists(&s, L"FLAGS"));
    CHECK(rg_cu_exists(&s, L"HELPDIR"));
    EQ(rg_cu(&s, L"FLAGS", NULL, v, 300), 0);
    EXPECT_WSTR(v, L"5");     /* LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN */
    EQ(rg_cu(&s, NULL, NULL, v, 300), 0);
    EXPECT_WSTR(v, L"Olet typelib marshal test library");
    EQ(rg_cu(&s, L"0\\win64", NULL, v, 300), 0);
    CHECK(tm_wieq(v, tm_main_path()));
    (void)path;
}

TMTEST(dxtm_regif_preexisting_psclsid_is_replaced)
{
    /* Re-registering rewrites ProxyStubClsid32 even when somebody changed it. */
    WCHAR p[260], v[300], bogus[] = L"{0b1d0099-7e57-4a11-9c11-01a1c0de0001}"; HKEY k; HRESULT hr;
    init_strs();
    tm_if_path(tm_ur(), &TM_IID_IOletSink, L"ProxyStubClsid32", p, 260);
    CHECK(RegOpenKeyExW(tm_ur(), p, 0, KEY_WRITE, &k) == 0);
    RegSetValueExW(k, NULL, 0, REG_SZ, (BYTE *)bogus, sizeof bogus);
    RegCloseKey(k);
    CHECK(rd(&TM_IID_IOletSink, L"ProxyStubClsid32", NULL, v, 0));
    CHECK(tm_weq(v, bogus));
    hr = tm_register_main();
    EQ(hr, S_OK);
    CHECK(rd(&TM_IID_IOletSink, L"ProxyStubClsid32", NULL, v, 0));
    CHECKF(tm_weq(v, g_clsid_psoa), "ProxyStubClsid32 after re-register: %ls", v);
}

TMTEST(dxtm_regif_unregister_removes_interfaces)
{
    GUID ids[6];
    int i;
    ids[0] = TM_IID_IOletSink; ids[1] = TM_IID_IOletCustom; ids[2] = TM_IID_IOletDual; ids[3] = TM_IID_IOletDisp;
    ids[4] = TM_IID_IOletProxyFlag; ids[5] = TM_IID_IOletPlain;
    for (i = 0; i < 5; i++) CHECK(tm_hkcr_has_iface(&ids[i]));
    EQ(pUnRegisterTypeLibForUser(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, SYS_WIN64), S_OK);
    for (i = 0; i < 6; i++) CHECKF(!tm_hkcr_has_iface(&ids[i]), "iface %d still registered", i);
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Main));
}

TMTEST(dxtm_regif_unrelated_interface_untouched)
{
    /* Interface keys of other IIDs are neither created nor removed. */
    WCHAR p[260], g[40]; HKEY k; GUID other = {0x0b1d0099, 0x7e57, 0x4a11, {0x9c, 0x11, 0x01, 0xa1, 0xc0, 0xde, 0x00, 0x01}};
    tm_guid_str(&other, g);
    _snwprintf(p, 260, L"%lsInterface\\%ls", tm_ur() == HKEY_CLASSES_ROOT ? L"" : L"Software\\Classes\\", g);
    if (RegCreateKeyExW(tm_ur(), p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) { t_skip("cannot create key"); return; }
    RegCloseKey(k);
    EQ(pUnRegisterTypeLibForUser(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, SYS_WIN64), S_OK);
    CHECK(tm_reg_key_exists(tm_ur(), p, 0));
    RegDeleteKeyW(tm_ur(), p);
}
