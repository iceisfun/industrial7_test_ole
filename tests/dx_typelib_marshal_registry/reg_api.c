/* RegisterTypeLib / RegisterTypeLibForUser / UnRegisterTypeLib / UnRegisterTypeLibForUser registry contract. */
#include "reg.h"

static void spec3_4(TMLibSpec *s) { rg_spec(s, 3, 4, 0, &TM_IID_Mini1); }

TMTEST_BARE(dxtm_regfu_creates_keys)
{
    TMLibSpec s; WCHAR path[MAX_PATH], v[300]; HRESULT hr;
    spec3_4(&s);
    hr = rg_register(&s, "r1.tlb", 1, NULL, path);
    EQ(hr, S_OK);
    CHECK(rg_cu_exists(&s, NULL));
    CHECK(rg_cu_exists(&s, L"0"));
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    CHECK(rg_cu_exists(&s, L"FLAGS"));
    CHECK(rg_cu_exists(&s, L"HELPDIR"));
    EQ(rg_cu(&s, L"0\\win64", NULL, v, 300), 0);
    CHECK(tm_wieq(v, path));
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!rg_cu_exists(&s, NULL));
}

TMTEST_BARE(dxtm_regfu_value_types)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; WCHAR p[260];
    spec3_4(&s);
    EQ(rg_register(&s, "r2.tlb", 1, NULL, path), S_OK);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, L"0\\win64", p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, L"FLAGS", p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, L"HELPDIR", p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    tm_tl_path(tm_ur(), &s.libid, 3, 4, NULL, p, 260);
    EQ(tm_reg_value_type(tm_ur(), p, NULL, 0), REG_SZ);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_null_args)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL; HRESULT hr;
    spec3_4(&s);
    p = tm_mini_path("r3.tlb", &s);
    if (!p) { t_skip("cannot build typelib"); return; }
    hr = U(LoadTypeLibEx)(p, REGKIND_NONE, &tl);
    EQ(hr, S_OK);
    if (!tl) return;
    EQ(pRegisterTypeLibForUser(NULL, (OLECHAR *)p, NULL), E_INVALIDARG_);
    EQ(pRegisterTypeLibForUser(tl, NULL, NULL), E_INVALIDARG_);
    EQ(U(RegisterTypeLib)(NULL, (OLECHAR *)p, NULL), E_INVALIDARG_);
    EQ(U(RegisterTypeLib)(tl, NULL, NULL), E_INVALIDARG_);
    CHECK(!rg_cu_exists(&s, NULL));
    ITypeLib_Release(tl);
}

TMTEST_BARE(dxtm_regfu_path_length_limit)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL; WCHAR longp[400], v[400]; int base, i;
    spec3_4(&s);
    p = tm_mini_path("r4.tlb", &s);
    if (!p) { t_skip("cannot build typelib"); return; }
    if (FAILED(U(LoadTypeLibEx)(p, REGKIND_NONE, &tl))) { t_skip("load failed"); return; }
    /* A path of 259 characters is accepted, 260 is not (the registered path buffer is MAX_PATH). The file need not exist. */
    lstrcpyW(longp, L"C:\\olet_nonexistent_\\");
    base = lstrlenW(longp);
    for (i = base; i < 255; i++) longp[i] = L'x';
    longp[255] = L'.'; longp[256] = L't'; longp[257] = L'l'; longp[258] = L'b'; longp[259] = 0;
    EQ(lstrlenW(longp), 259);
    EQ(pRegisterTypeLibForUser(tl, longp, NULL), S_OK);
    EQ(rg_cu(&s, L"0\\win64", NULL, v, 400), 0);
    CHECKF(tm_wieq(v, longp), "stored path differs from the given 259-char path");
    /* unregistering needs to load the registered file, which does not exist */
    HRESULT hu = rg_unregister(&s, 1);
    CHECK(FAILED(hu));
    t_note("UnRegisterTypeLibForUser of a registration whose file is missing: 0x%lx", (long)hu);
    tm_sweep();
    longp[259] = L'x'; longp[260] = 0;
    EQ(lstrlenW(longp), 260);
    EQ(pRegisterTypeLibForUser(tl, longp, NULL), E_INVALIDARG_);
    CHECK(!rg_cu_exists(&s, NULL));
    ITypeLib_Release(tl);
}

TMTEST_BARE(dxtm_regfu_helpdir_explicit)
{
    TMLibSpec s; WCHAR path[MAX_PATH], v[300];
    spec3_4(&s);
    EQ(rg_register(&s, "r5.tlb", 1, L"C:\\olet\\help dir", path), S_OK);
    EQ(rg_cu(&s, L"HELPDIR", NULL, v, 300), 0);
    EXPECT_WSTR(v, L"C:\\olet\\help dir");
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_helpdir_default_from_path)
{
    TMLibSpec s; WCHAR path[MAX_PATH], v[300], dir[MAX_PATH]; WCHAR *sl; int n;
    spec3_4(&s);
    EQ(rg_register(&s, "r6.tlb", 1, NULL, path), S_OK);
    EQ(rg_cu(&s, L"HELPDIR", NULL, v, 300), 0);
    lstrcpyW(dir, path);
    sl = wcsrchr(dir, L'\\');
    if (sl) sl[1] = 0;
    n = lstrlenW(v);
    if (n && v[n - 1] == L'\\') t_note("HELPDIR keeps the trailing backslash");
    else t_note("HELPDIR has no trailing backslash");
    if (n && v[n - 1] != L'\\') { v[n] = L'\\'; v[n + 1] = 0; }
    CHECK(tm_wieq(v, dir));
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_helpdir_kept_on_reregister)
{
    TMLibSpec s; WCHAR path[MAX_PATH], v[300];
    spec3_4(&s);
    EQ(rg_register(&s, "r7.tlb", 1, L"C:\\olet\\explicit", path), S_OK);
    /* an existing HELPDIR value is not overwritten when no help dir is given */
    EQ(rg_register(&s, "r7.tlb", 1, NULL, path), S_OK);
    EQ(rg_cu(&s, L"HELPDIR", NULL, v, 300), 0);
    EXPECT_WSTR(v, L"C:\\olet\\explicit");
    /* an explicit one replaces it */
    EQ(rg_register(&s, "r7.tlb", 1, L"C:\\olet\\other", path), S_OK);
    EQ(rg_cu(&s, L"HELPDIR", NULL, v, 300), 0);
    EXPECT_WSTR(v, L"C:\\olet\\other");
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_reregister_updates_path)
{
    TMLibSpec s; WCHAR p1[MAX_PATH], p2[MAX_PATH], v[300]; BSTR q = NULL;
    spec3_4(&s);
    EQ(rg_register(&s, "r8a.tlb", 1, NULL, p1), S_OK);
    EQ(rg_register(&s, "r8b.tlb", 1, NULL, p2), S_OK);
    EQ(rg_cu(&s, L"0\\win64", NULL, v, 300), 0);
    CHECK(tm_wieq(v, p2));
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, p2));
    SysFreeString(q);
    EQ(rg_unregister(&s, 1), S_OK);
}

static const struct { WORD flags; const WCHAR *want; } flagcases[] = {
    { 0, L"0" }, { LIBFLAG_FRESTRICTED, L"1" }, { LIBFLAG_FCONTROL, L"2" }, { LIBFLAG_FHIDDEN, L"4" },
    { LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN, L"5" }, { LIBFLAG_FRESTRICTED | LIBFLAG_FCONTROL | LIBFLAG_FHIDDEN, L"7" },
};
TMTEST_BARE(dxtm_regfu_flags_value)
{
    int i;
    for (i = 0; i < (int)(sizeof flagcases / sizeof flagcases[0]); i++) {
        TMLibSpec s; WCHAR path[MAX_PATH], v[64]; char leaf[32];
        spec3_4(&s); s.libflags = flagcases[i].flags;
        sprintf(leaf, "rf%d.tlb", i);
        EQ(rg_register(&s, leaf, 1, NULL, path), S_OK);
        EQ(rg_cu(&s, L"FLAGS", NULL, v, 64), 0);
        CHECKF(tm_weq(v, flagcases[i].want), "flags 0x%x -> FLAGS \"%ls\" want \"%ls\"", flagcases[i].flags, v, flagcases[i].want);
        rg_unregister(&s, 1);
    }
}

TMTEST_BARE(dxtm_regfu_flags_diskimage_masked)
{
    /* LIBFLAG_FHASDISKIMAGE (8) is cleared from the FLAGS value (mask 0xfff7). */
    TMLibSpec s; WCHAR path[MAX_PATH], v[64];
    spec3_4(&s); s.libflags = LIBFLAG_FHIDDEN | LIBFLAG_FHASDISKIMAGE;
    EQ(rg_register(&s, "rf8.tlb", 1, NULL, path), S_OK);
    EQ(rg_cu(&s, L"FLAGS", NULL, v, 64), 0);
    EXPECT_WSTR(v, L"4");
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_version_default_value)
{
    /* The default value of the version key is the library doc string, else the library name, else "". */
    TMLibSpec s; WCHAR path[MAX_PATH], v[300];
    spec3_4(&s);
    EQ(rg_register(&s, "rd1.tlb", 1, NULL, path), S_OK);
    EQ(rg_cu(&s, NULL, NULL, v, 300), 0);
    EXPECT_WSTR(v, L"Mini doc string");
    rg_unregister(&s, 1);

    spec3_4(&s); s.doc = NULL;
    EQ(rg_register(&s, "rd2.tlb", 1, NULL, path), S_OK);
    EQ(rg_cu(&s, NULL, NULL, v, 300), 0);
    EXPECT_WSTR(v, L"OletMini");
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_version_key_is_hex)
{
    TMLibSpec s; WCHAR path[MAX_PATH], p[260], base[260];
    rg_spec(&s, 11, 26, 0, &TM_IID_Mini1);       /* 0xb.0x1a */
    EQ(rg_register(&s, "rv1.tlb", 1, NULL, path), S_OK);
    tm_tl_path(tm_ur(), &s.libid, 0xffff, 0xffff, NULL, base, 260);
    _snwprintf(p, 260, L"%ls\\b.1a", base);
    CHECK(tm_reg_key_exists(tm_ur(), p, 0));
    _snwprintf(p, 260, L"%ls\\11.26", base);
    CHECK(!tm_reg_key_exists(tm_ur(), p, 0));
    _snwprintf(p, 260, L"%ls\\B.1A", base);
    CHECK(tm_reg_key_exists(tm_ur(), p, 0));     /* key names compare case-insensitively */
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_regfu_lcid_key_is_lowercase_hex)
{
    TMLibSpec s; WCHAR path[MAX_PATH], p[260];
    rg_spec(&s, 3, 4, 0x40c, &TM_IID_Mini1);
    EQ(rg_register(&s, "rl1.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s, L"40c"));
    CHECK(!rg_cu_exists(&s, L"1036"));
    CHECK(rg_cu_exists(&s, L"40C"));      /* key names compare case-insensitively */
    CHECK(rg_cu_exists(&s, L"40c\\win64"));
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!rg_cu_exists(&s, NULL));
}

TMTEST_BARE(dxtm_regfu_syskind_subkeys)
{
    TMLibSpec s64, s32; WCHAR path[MAX_PATH];
    rg_spec(&s64, 3, 4, 0, &TM_IID_Mini1);
    rg_spec(&s32, 3, 4, 0, &TM_IID_Mini1); s32.syskind = SYS_WIN32;
    EQ(rg_register(&s32, "rs32.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s32, L"0\\win32"));
    CHECK(!rg_cu_exists(&s32, L"0\\win64"));
    EQ(rg_register(&s64, "rs64.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s32, L"0\\win32"));
    CHECK(rg_cu_exists(&s32, L"0\\win64"));
    /* removing one platform keeps the other */
    EQ(rg_unregister(&s32, 1), S_OK);
    CHECK(!rg_cu_exists(&s32, L"0\\win32"));
    CHECK(rg_cu_exists(&s32, L"0\\win64"));
    EQ(rg_unregister(&s64, 1), S_OK);
    CHECK(!rg_cu_exists(&s64, NULL));
}

TMTEST_BARE(dxtm_regfu_two_versions_side_by_side)
{
    TMLibSpec a, b; WCHAR path[MAX_PATH];
    rg_spec(&a, 3, 4, 0, &TM_IID_Mini1);
    rg_spec(&b, 5, 6, 0, &TM_IID_Mini2);
    EQ(rg_register(&a, "rt1.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&b, "rt2.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&a, NULL));
    CHECK(rg_cu_exists(&b, NULL));
    EQ(rg_unregister(&a, 1), S_OK);
    CHECK(!rg_cu_exists(&a, NULL));
    CHECK(rg_cu_exists(&b, L"0\\win64"));
    CHECK(tm_hkcu_has_lib(&TM_LIBID_Mini));
    EQ(rg_unregister(&b, 1), S_OK);
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Mini));
}

TMTEST_BARE(dxtm_regfu_two_lcids_side_by_side)
{
    TMLibSpec a, b; WCHAR path[MAX_PATH];
    rg_spec(&a, 3, 4, 0, &TM_IID_Mini1);
    rg_spec(&b, 3, 4, 0x407, &TM_IID_Mini1);
    EQ(rg_register(&a, "rq1.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&b, "rq2.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&a, L"0\\win64"));
    CHECK(rg_cu_exists(&b, L"407\\win64"));
    EQ(rg_unregister(&b, 1), S_OK);
    CHECK(!rg_cu_exists(&b, L"407"));
    CHECK(rg_cu_exists(&a, L"0\\win64"));
    EQ(rg_unregister(&a, 1), S_OK);
    CHECK(!rg_cu_exists(&a, NULL));
}

TMTEST_BARE(dxtm_regfu_hkcr_merged_view)
{
    TMLibSpec s; WCHAR path[MAX_PATH], p[260], v[300];
    spec3_4(&s);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
    EQ(rg_register(&s, "rh1.tlb", 1, NULL, path), S_OK);
    CHECK(tm_hkcr_has_lib(&TM_LIBID_Mini));
    tm_tl_path(HKEY_CLASSES_ROOT, &s.libid, 3, 4, L"0\\win64", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    CHECK(tm_wieq(v, path));
    tm_tl_path(HKEY_LOCAL_MACHINE, &s.libid, 3, 4, NULL, p, 260);
    if (!t_is_wine()) CHECK(!tm_reg_key_exists(HKEY_LOCAL_MACHINE, p, 0));      /* ForUser never touches HKLM */
    rg_unregister(&s, 1);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
}

TMTEST_BARE(dxtm_reg_plain_roundtrip)
{
    /* RegisterTypeLib picks the hive itself (HKCU for a non-elevated token); verify through the merged HKCR view. */
    TMLibSpec s; WCHAR path[MAX_PATH], v[300], p[260]; BSTR q = NULL; HRESULT hr;
    spec3_4(&s);
    hr = rg_register(&s, "rp1.tlb", 0, NULL, path);
    if (FAILED(hr)) { t_skip("RegisterTypeLib failed 0x%lx", (long)hr); return; }
    CHECK(tm_hkcr_has_lib(&TM_LIBID_Mini));
    tm_tl_path(HKEY_CLASSES_ROOT, &s.libid, 3, 4, L"0\\win64", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    CHECK(tm_wieq(v, path));
    t_note("RegisterTypeLib wrote to %s", tm_hkcu_has_lib(&TM_LIBID_Mini) ? "HKCU" : "HKLM");
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 3, 4, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, path));
    SysFreeString(q);
    EQ(rg_unregister(&s, 0), S_OK);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
    EQ(U(UnRegisterTypeLib)(&s.libid, 3, 4, 0, SYS_WIN64), TLB_REGACCESS);
}

TMTEST_BARE(dxtm_reg_plain_matches_foruser_layout)
{
    TMLibSpec s; WCHAR path[MAX_PATH], v[300], p[260];
    spec3_4(&s);
    if (FAILED(rg_register(&s, "rp2.tlb", 0, L"C:\\olet\\h", path))) { t_skip("RegisterTypeLib failed"); return; }
    tm_tl_path(HKEY_CLASSES_ROOT, &s.libid, 3, 4, L"HELPDIR", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    EXPECT_WSTR(v, L"C:\\olet\\h");
    tm_tl_path(HKEY_CLASSES_ROOT, &s.libid, 3, 4, L"FLAGS", p, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, p, NULL, v, 300, 0), 0);
    EXPECT_WSTR(v, L"0");
    CHECK(tm_hkcr_has_iface(&TM_IID_Mini1));
    rg_unregister(&s, 0);
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
}

/* ---------------------------------------------------------------- UnRegister */
TMTEST_BARE(dxtm_unreg_not_registered)
{
    EQ(pUnRegisterTypeLibForUser(&TM_LIBID_Mini, 3, 4, 0, SYS_WIN64), TLB_REGACCESS);
    EQ(U(UnRegisterTypeLib)(&TM_LIBID_Mini, 3, 4, 0, SYS_WIN64), TLB_REGACCESS);
    EQ(U(UnRegisterTypeLib)(&TM_LIBID_Other, 0, 0, 0, SYS_WIN32), TLB_REGACCESS);
}

TMTEST_BARE(dxtm_unreg_wrong_version_lcid_syskind)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    spec3_4(&s);
    EQ(rg_register(&s, "ru1.tlb", 1, NULL, path), S_OK);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 5, 0, SYS_WIN64), TLB_REGACCESS);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 4, 4, 0, SYS_WIN64), TLB_REGACCESS);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 4, 0x409, SYS_WIN64), TLB_REGACCESS);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 4, 0, SYS_WIN32), TLB_REGACCESS);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 4, 0, SYS_WIN16), TLB_REGACCESS);
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    CHECK(rg_hkcu_if(&TM_IID_Mini1));
    EQ(rg_unregister(&s, 1), S_OK);
}

TMTEST_BARE(dxtm_unreg_bad_syskind_is_invalidarg)
{
    /* SYS_MAC is not a registrable platform: with the version and lcid keys present the call is rejected as E_INVALIDARG. */
    TMLibSpec s; WCHAR path[MAX_PATH];
    spec3_4(&s);
    EQ(rg_register(&s, "ru2.tlb", 1, NULL, path), S_OK);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 4, 0, SYS_MAC), E_INVALIDARG_);
    EQ(pUnRegisterTypeLibForUser(&s.libid, 3, 4, 0, (SYSKIND)7), E_INVALIDARG_);
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    EQ(rg_unregister(&s, 1), S_OK);
}

TMTEST_BARE(dxtm_unreg_removes_everything)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    spec3_4(&s);
    EQ(rg_register(&s, "ru3.tlb", 1, NULL, path), S_OK);
    CHECK(rg_hkcu_if(&TM_IID_Mini1));
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Mini));
    CHECK(!rg_hkcu_if(&TM_IID_Mini1));
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
    /* a second unregister now fails */
    EQ(rg_unregister(&s, 1), TLB_REGACCESS);
}

TMTEST_BARE(dxtm_unreg_keeps_helpdir_flags_orphans_out)
{
    /* After the last platform key is removed the whole version key goes, even though FLAGS and HELPDIR remain as siblings. */
    TMLibSpec s; WCHAR path[MAX_PATH];
    spec3_4(&s);
    EQ(rg_register(&s, "ru4.tlb", 1, L"C:\\olet\\h", path), S_OK);
    CHECK(rg_cu_exists(&s, L"HELPDIR") && rg_cu_exists(&s, L"FLAGS"));
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!rg_cu_exists(&s, NULL));
    CHECK(!rg_cu_exists(&s, L"HELPDIR"));
}

TMTEST_BARE(dxtm_unreg_missing_file_fails_and_keeps_keys)
{
    /* UnRegisterTypeLib loads the registered file to learn which Interface keys to delete; without it the call fails. */
    TMLibSpec s; WCHAR path[MAX_PATH]; HRESULT hr;
    spec3_4(&s);
    EQ(rg_register(&s, "ru5.tlb", 1, NULL, path), S_OK);
    CHECK(DeleteFileW(path));
    hr = rg_unregister(&s, 1);
    CHECK(FAILED(hr));
    t_note("UnRegisterTypeLibForUser with the typelib file deleted: 0x%lx (TYPE_E_CANTLOADLIBRARY=0x%lx)", (long)hr, (long)TYPE_E_CANTLOADLIBRARY);
    CHECK(rg_cu_exists(&s, L"0\\win64"));
    CHECK(rg_hkcu_if(&TM_IID_Mini1));
    /* put the file back: now it works */
    EQ(tm_build_mini(path, &s), S_OK);
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!rg_cu_exists(&s, NULL));
}

TMTEST_BARE(dxtm_unreg_keeps_shared_interface_of_other_version)
{
    /* The same IID in two registered versions: removing one version must not delete the Interface key the other still uses. */
    TMLibSpec a, b; WCHAR path[MAX_PATH], v[300];
    rg_spec(&a, 1, 0, 0, &TM_IID_Mini3);
    rg_spec(&b, 2, 0, 0, &TM_IID_Mini3);
    EQ(rg_register(&a, "ruv1.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&b, "ruv2.tlb", 1, NULL, path), S_OK);
    /* the later registration owns the Interface\TypeLib Version value */
    EQ(rg_if_cu(&TM_IID_Mini3, L"TypeLib", L"Version", v, 300, 0, NULL), 1);
    EXPECT_WSTR(v, L"2.0");
    EQ(rg_unregister(&b, 1), S_OK);
    CHECK(rg_hkcu_if(&TM_IID_Mini3));
    EQ(rg_unregister(&a, 1), S_OK);
    CHECK(!rg_hkcu_if(&TM_IID_Mini3));
}

TMTEST_BARE(dxtm_regfu_idempotent)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; int i;
    spec3_4(&s);
    for (i = 0; i < 5; i++) EQ(rg_register(&s, "ri1.tlb", 1, NULL, path), S_OK);
    EQ(rg_unregister(&s, 1), S_OK);
    CHECK(!tm_hkcu_has_lib(&TM_LIBID_Mini));
}
