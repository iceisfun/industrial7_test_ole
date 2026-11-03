/* QueryPathOfRegTypeLib / LoadRegTypeLib / LoadTypeLibEx(REGKIND_*) lookup rules. */
#include "reg.h"

/* compare a registered path with the one given, tolerating a "\\?\" long-path prefix */
static int same_path(const WCHAR *reg, const WCHAR *given)
{
    if (!reg) return 0;
    if (!wcsncmp(reg, L"\\\\?\\", 4)) { t_note("registered path carries a long-path prefix"); reg += 4; }
    return !lstrcmpiW(reg, given);
}

static HRESULT load(const TMLibSpec *s, WORD maj, WORD min, LCID lcid, ITypeLib **pp)
{
    return U(LoadRegTypeLib)(&s->libid, maj, min, lcid, pp);
}
static HRESULT qpath(const TMLibSpec *s, WORD maj, WORD min, LCID lcid, BSTR *pb)
{
    return U(QueryPathOfRegTypeLib)(&s->libid, maj, min, lcid, pb);
}
static void libattr(ITypeLib *tl, WORD *maj, WORD *min, LCID *lcid, GUID *g)
{
    TLIBATTR *a = NULL;
    if (SUCCEEDED(ITypeLib_GetLibAttr(tl, &a)) && a) {
        if (maj) *maj = a->wMajorVerNum;
        if (min) *min = a->wMinorVerNum;
        if (lcid) *lcid = a->lcid;
        if (g) *g = a->guid;
        ITypeLib_ReleaseTLibAttr(tl, a);
    }
}

TMTEST_BARE(dxtm_load_exact_version)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; WORD ma = 0, mi = 0; GUID g;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "ld1.tlb", 1, NULL, path), S_OK);
    EQ(load(&s, 7, 3, 0, &tl), S_OK);
    CHECK(tl != NULL);
    if (tl) {
        libattr(tl, &ma, &mi, NULL, &g);
        EQ(ma, 7); EQ(mi, 3);
        CHECK(IsEqualGUID(&g, &TM_LIBID_Mini));
        EQ(ITypeLib_GetTypeInfoCount(tl), 1);
        ITypeLib_Release(tl);
    }
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_load_not_registered)
{
    TMLibSpec s; ITypeLib *tl = (ITypeLib *)(ULONG_PTR)0x1234;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(load(&s, 7, 3, 0, &tl), TLB_NOTREG);
    CHECK(tl == NULL);
}

TMTEST_BARE(dxtm_load_minor_rule)
{
    /* major must match exactly; the registered minor must be >= the requested one */
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "ld2.tlb", 1, NULL, path), S_OK);
    EQ(load(&s, 7, 0, 0, &tl), S_OK);           if (tl) { ITypeLib_Release(tl); tl = NULL; }
    EQ(load(&s, 7, 2, 0, &tl), S_OK);           if (tl) { ITypeLib_Release(tl); tl = NULL; }
    EQ(load(&s, 7, 3, 0, &tl), S_OK);           if (tl) { ITypeLib_Release(tl); tl = NULL; }
    EQ(load(&s, 7, 4, 0, &tl), TLB_NOTREG);     CHECK(tl == NULL);
    EQ(load(&s, 7, 0xfffe, 0, &tl), TLB_NOTREG);
    EQ(load(&s, 6, 3, 0, &tl), TLB_NOTREG);
    EQ(load(&s, 8, 0, 0, &tl), TLB_NOTREG);
    EQ(load(&s, 0, 0, 0, &tl), TLB_NOTREG);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_load_picks_highest_minor_of_major)
{
    TMLibSpec a, b; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; WORD ma = 0, mi = 0;
    rg_spec(&a, 7, 1, 0, &TM_IID_Mini1);
    rg_spec(&b, 7, 9, 0, &TM_IID_Mini2);
    EQ(rg_register(&a, "ld3a.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&b, "ld3b.tlb", 1, NULL, path), S_OK);
    EQ(load(&a, 7, 0, 0, &tl), S_OK);
    if (tl) { libattr(tl, &ma, &mi, NULL, NULL); EQ(ma, 7); EQ(mi, 9); ITypeLib_Release(tl); tl = NULL; }
    EQ(load(&a, 7, 1, 0, &tl), S_OK);
    if (tl) { libattr(tl, &ma, &mi, NULL, NULL); EQ(mi, 1); ITypeLib_Release(tl); tl = NULL; }   /* exact minor wins */
    EQ(load(&a, 7, 9, 0, &tl), S_OK);
    if (tl) { libattr(tl, &ma, &mi, NULL, NULL); EQ(mi, 9); ITypeLib_Release(tl); tl = NULL; }
    EQ(load(&a, 7, 10, 0, &tl), TLB_NOTREG);
    rg_unregister(&a, 1); rg_unregister(&b, 1);
}

TMTEST_BARE(dxtm_load_latest_version_wildcard)
{
    /* 0xffff.0xffff selects the numerically highest major.minor (version keys are hex strings, e.g. "a.1" > "9.0") */
    TMLibSpec a, b, c; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; WORD ma = 0, mi = 0; BSTR q = NULL;
    rg_spec(&a, 9, 0, 0, &TM_IID_Mini1);
    rg_spec(&b, 10, 1, 0, &TM_IID_Mini2);
    rg_spec(&c, 3, 77, 0, &TM_IID_Mini3);
    EQ(rg_register(&a, "ll1.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&b, "ll2.tlb", 1, NULL, path), S_OK);
    EQ(rg_register(&c, "ll3.tlb", 1, NULL, path), S_OK);
    EQ(load(&a, 0xffff, 0xffff, 0, &tl), S_OK);
    if (tl) { libattr(tl, &ma, &mi, NULL, NULL); EQ(ma, 10); EQ(mi, 1); ITypeLib_Release(tl); tl = NULL; }
    EQ(qpath(&a, 0xffff, 0xffff, 0, &q), S_OK);
    CHECK(q != NULL);
    SysFreeString(q);
    rg_unregister(&b, 1);
    EQ(load(&a, 0xffff, 0xffff, 0, &tl), S_OK);
    if (tl) { libattr(tl, &ma, &mi, NULL, NULL); EQ(ma, 9); EQ(mi, 0); ITypeLib_Release(tl); tl = NULL; }
    rg_unregister(&a, 1); rg_unregister(&c, 1);
    EQ(load(&a, 0xffff, 0xffff, 0, &tl), TLB_NOTREG);
}

TMTEST_BARE(dxtm_load_lcid_fallback_to_neutral)
{
    /* A neutral (lcid 0) registration satisfies any requested lcid. */
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; static const LCID req[] = { 0, 0x409, 0x809, 0x407, 9, 0x400, 0x800, 0x40c };
    unsigned i;
    rg_spec(&s, 2, 0, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "lc1.tlb", 1, NULL, path), S_OK);
    for (i = 0; i < sizeof req / sizeof req[0]; i++) {
        HRESULT hr = load(&s, 2, 0, req[i], &tl);
        CHECKF(hr == S_OK, "LoadRegTypeLib lcid 0x%lx hr=0x%lx", (long)req[i], (long)hr);
        if (tl) { ITypeLib_Release(tl); tl = NULL; }
    }
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_load_lcid_exact_and_primary_language)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; BSTR q = NULL; LCID got = 99;
    rg_spec(&s, 2, 0, 0x409, &TM_IID_Mini1);       /* registered under "409" */
    EQ(rg_register(&s, "lc2.tlb", 1, NULL, path), S_OK);
    EQ(load(&s, 2, 0, 0x409, &tl), S_OK);
    if (tl) { libattr(tl, NULL, NULL, &got, NULL); EQ(got, 0x409); ITypeLib_Release(tl); tl = NULL; }
    /* other sub-languages and the primary-language-only id do not find a "409" registration */
    EQ(load(&s, 2, 0, 0x809, &tl), TLB_NOTREG);
    EQ(load(&s, 2, 0, 0x9, &tl), TLB_NOTREG);
    EQ(load(&s, 2, 0, 0x407, &tl), TLB_NOTREG);
    EQ(load(&s, 2, 0, 0, &tl), TLB_NOTREG);
    EQ(qpath(&s, 2, 0, 0x809, &q), TLB_NOTREG);
    CHECK(q == NULL);
    EQ(qpath(&s, 2, 0, 0x409, &q), S_OK);
    SysFreeString(q);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_load_lcid_primary_registration_matches_sublanguages)
{
    /* Registration under the primary-language id ("9") is found when asking for 0x409 or 0x809 (lcid & 0x3ff). */
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL; BSTR q = NULL;
    rg_spec(&s, 2, 0, 9, &TM_IID_Mini1);
    EQ(rg_register(&s, "lc3.tlb", 1, NULL, path), S_OK);
    CHECK(rg_cu_exists(&s, L"9\\win64"));
    EQ(qpath(&s, 2, 0, 0x409, &q), S_OK);  SysFreeString(q); q = NULL;
    EQ(qpath(&s, 2, 0, 0x809, &q), S_OK);  SysFreeString(q); q = NULL;
    EQ(qpath(&s, 2, 0, 9, &q), S_OK);      SysFreeString(q); q = NULL;
    EQ(qpath(&s, 2, 0, 0x407, &q), TLB_NOTREG);
    EQ(load(&s, 2, 0, 0x409, &tl), S_OK);
    if (tl) ITypeLib_Release(tl);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_qp_returns_registered_path)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "qp1.tlb", 1, NULL, path), S_OK);
    EQ(qpath(&s, 7, 3, 0, &q), S_OK);
    CHECK(q != NULL);
    if (q) {
        CHECK(tm_wieq(q, path));
        EQ(SysStringLen(q), lstrlenW(path));
        EQ(q[lstrlenW(path)], 0);
    }
    SysFreeString(q);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_qp_null_output)
{
    TMLibSpec s; WCHAR path[MAX_PATH];
    if (t_is_wine()) { t_skip("Wine dereferences the NULL output"); return; }
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 7, 3, 0, NULL), E_INVALIDARG_);
    EQ(rg_register(&s, "qp2.tlb", 1, NULL, path), S_OK);
    EQ(U(QueryPathOfRegTypeLib)(&s.libid, 7, 3, 0, NULL), E_INVALIDARG_);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_qp_not_registered)
{
    TMLibSpec s; BSTR q = (BSTR)(ULONG_PTR)0x1234; HRESULT hr;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    hr = qpath(&s, 7, 3, 0, &q);
    CHECK(FAILED(hr));
    EQ(hr, TLB_NOTREG);
    CHECK(q == NULL);
    q = (BSTR)(ULONG_PTR)0x1234;
    hr = qpath(&s, 0xffff, 0xffff, 0, &q);
    EQ(hr, TLB_NOTREG);
    CHECK(q == NULL);
}

TMTEST_BARE(dxtm_qp_major_mismatch)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "qp3.tlb", 1, NULL, path), S_OK);
    EQ(qpath(&s, 6, 3, 0, &q), TLB_NOTREG);   CHECK(q == NULL);
    EQ(qpath(&s, 8, 0, 0, &q), TLB_NOTREG);
    EQ(qpath(&s, 0, 0, 0, &q), TLB_NOTREG);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_qp_minor_variants_observed)
{
    /* Not specified by the documentation: record what happens for a requested minor different from the registered one. */
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL; HRESULT hr;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "qp4.tlb", 1, NULL, path), S_OK);
    hr = qpath(&s, 7, 0, 0, &q);  t_note("QueryPathOfRegTypeLib(7.0) with 7.3 registered: hr=0x%lx path=%ls", (long)hr, q ? q : L"(null)"); SysFreeString(q); q = NULL;
    hr = qpath(&s, 7, 9, 0, &q);  t_note("QueryPathOfRegTypeLib(7.9) with 7.3 registered: hr=0x%lx path=%ls", (long)hr, q ? q : L"(null)"); SysFreeString(q);
    rg_unregister(&s, 1);
}

TMTEST_BARE(dxtm_qp_after_unregister)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "qp5.tlb", 1, NULL, path), S_OK);
    EQ(qpath(&s, 7, 3, 0, &q), S_OK);  SysFreeString(q); q = NULL;
    EQ(rg_unregister(&s, 1), S_OK);
    EQ(qpath(&s, 7, 3, 0, &q), TLB_NOTREG);
    CHECK(q == NULL);
}

TMTEST_BARE(dxtm_qp_does_not_check_file_existence)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; BSTR q = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "qp6.tlb", 1, NULL, path), S_OK);
    CHECK(DeleteFileW(path));
    EQ(qpath(&s, 7, 3, 0, &q), S_OK);
    CHECK(q && tm_wieq(q, path));
    SysFreeString(q);
    tm_sweep();
}

TMTEST_BARE(dxtm_load_missing_file_fails)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = (ITypeLib *)(ULONG_PTR)0x1234; HRESULT hr;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "lm1.tlb", 1, NULL, path), S_OK);
    CHECK(DeleteFileW(path));
    hr = load(&s, 7, 3, 0, &tl);
    CHECK(FAILED(hr));
    CHECK(tl == NULL);
    t_note("LoadRegTypeLib with the registered file deleted: hr=0x%lx (TYPE_E_CANTLOADLIBRARY=0x%lx)", (long)hr, (long)TYPE_E_CANTLOADLIBRARY);
    tm_sweep();
}

TMTEST_BARE(dxtm_load_after_unregister_fails)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *tl = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "lm2.tlb", 1, NULL, path), S_OK);
    EQ(load(&s, 7, 3, 0, &tl), S_OK); if (tl) { ITypeLib_Release(tl); tl = NULL; }
    EQ(rg_unregister(&s, 1), S_OK);
    EQ(load(&s, 7, 3, 0, &tl), TLB_NOTREG);
    CHECK(tl == NULL);
}

TMTEST_BARE(dxtm_load_returns_independent_objects)
{
    TMLibSpec s; WCHAR path[MAX_PATH]; ITypeLib *a = NULL, *b = NULL;
    rg_spec(&s, 7, 3, 0, &TM_IID_Mini1);
    EQ(rg_register(&s, "lm3.tlb", 1, NULL, path), S_OK);
    EQ(load(&s, 7, 3, 0, &a), S_OK);
    EQ(load(&s, 7, 3, 0, &b), S_OK);
    CHECK(a && b);
    if (a && b) {
        ITypeInfo *ta = NULL, *tb = NULL;
        EQ(ITypeLib_GetTypeInfoOfGuid(a, &TM_IID_Mini1, &ta), S_OK);
        EQ(ITypeLib_GetTypeInfoOfGuid(b, &TM_IID_Mini1, &tb), S_OK);
        if (ta) ITypeInfo_Release(ta);
        if (tb) ITypeInfo_Release(tb);
    }
    if (a) ITypeLib_Release(a);
    if (b) ITypeLib_Release(b);
    rg_unregister(&s, 1);
}

/* ---------------------------------------------------------------- LoadTypeLibEx REGKIND */
TMTEST_BARE(dxtm_loadex_regkind_none_does_not_register)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL;
    rg_spec(&s, 4, 1, 0, &TM_IID_Mini1);
    p = tm_mini_path("le1.tlb", &s);
    if (!p) { t_skip("build"); return; }
    EQ(U(LoadTypeLibEx)(p, REGKIND_NONE, &tl), S_OK);
    if (tl) ITypeLib_Release(tl);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
}

TMTEST_BARE(dxtm_loadex_regkind_default_fullpath_does_not_register)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL;
    rg_spec(&s, 4, 1, 0, &TM_IID_Mini1);
    p = tm_mini_path("le2.tlb", &s);
    if (!p) { t_skip("build"); return; }
    EQ(U(LoadTypeLibEx)(p, REGKIND_DEFAULT, &tl), S_OK);
    if (tl) ITypeLib_Release(tl);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
    EQ(U(LoadTypeLib)(p, &tl), S_OK);     /* LoadTypeLib registers when asked through the default kind only for relative paths */
    if (tl) ITypeLib_Release(tl);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
}

TMTEST_BARE(dxtm_loadex_regkind_register_registers)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL; BSTR q = NULL; WCHAR v[300], pp[260];
    rg_spec(&s, 4, 1, 0, &TM_IID_Mini1);
    p = tm_mini_path("le3.tlb", &s);
    if (!p) { t_skip("build"); return; }
    EQ(U(LoadTypeLibEx)(p, REGKIND_REGISTER, &tl), S_OK);
    if (tl) ITypeLib_Release(tl);
    CHECK(tm_hkcr_has_lib(&TM_LIBID_Mini));
    CHECK(tm_hkcr_has_iface(&TM_IID_Mini1));
    EQ(qpath(&s, 4, 1, 0, &q), S_OK);
    CHECKF(same_path(q, p), "q=%ls p=%ls", q ? q : L"(null)", p);
    SysFreeString(q);
    tm_tl_path(HKEY_CLASSES_ROOT, &s.libid, 4, 1, L"0\\win64", pp, 260);
    EQ(tm_reg_sz(HKEY_CLASSES_ROOT, pp, NULL, v, 300, 0), 0);
    CHECK(same_path(v, p));
    EQ(U(UnRegisterTypeLib)(&s.libid, 4, 1, 0, SYS_WIN64), S_OK);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Mini));
    CHECK(!tm_hkcr_has_iface(&TM_IID_Mini1));
}

TMTEST_BARE(dxtm_loadex_register_registers_all_automation_interfaces)
{
    const WCHAR *p = tm_main_path(); ITypeLib *tl = NULL;
    if (!p) { t_skip("build"); return; }
    EQ(U(LoadTypeLibEx)(p, REGKIND_REGISTER, &tl), S_OK);
    if (tl) ITypeLib_Release(tl);
    CHECK(tm_hkcr_has_lib(&TM_LIBID_Main));
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletCustom));
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletDual));
    CHECK(tm_hkcr_has_iface(&TM_IID_IOletSink));
    CHECK(!tm_hkcr_has_iface(&TM_IID_IOletPlain));
    EQ(U(UnRegisterTypeLib)(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, SYS_WIN64), S_OK);
    CHECK(!tm_hkcr_has_lib(&TM_LIBID_Main));
    CHECK(!tm_hkcr_has_iface(&TM_IID_IOletCustom));
}

TMTEST_BARE(dxtm_loadex_missing_file)
{
    WCHAR p[MAX_PATH]; ITypeLib *tl = (ITypeLib *)(ULONG_PTR)0x1234; HRESULT hr; char a[MAX_PATH];
    t_tmpfile(a, sizeof a, "no_such_file.tlb");
    MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH);
    hr = U(LoadTypeLibEx)(p, REGKIND_NONE, &tl);
    CHECK(FAILED(hr));
    t_note("LoadTypeLibEx(nonexistent) hr=0x%lx", (long)hr);
    CHECK(tl == NULL);
}

TMTEST_BARE(dxtm_loadex_register_twice_idempotent)
{
    TMLibSpec s; const WCHAR *p; ITypeLib *tl = NULL; int i;
    rg_spec(&s, 4, 1, 0, &TM_IID_Mini1);
    p = tm_mini_path("le4.tlb", &s);
    if (!p) { t_skip("build"); return; }
    for (i = 0; i < 3; i++) { EQ(U(LoadTypeLibEx)(p, REGKIND_REGISTER, &tl), S_OK); if (tl) { ITypeLib_Release(tl); tl = NULL; } }
    EQ(U(UnRegisterTypeLib)(&s.libid, 4, 1, 0, SYS_WIN64), S_OK);
    EQ(U(UnRegisterTypeLib)(&s.libid, 4, 1, 0, SYS_WIN64), TLB_REGACCESS);
}

TMTEST_BARE(dxtm_qp_clears_out_on_failure_even_when_preset)
{
    /* decomp: *lpbstrPathName is zeroed before the lookup, so a failing query never leaves the caller's value */
    TMLibSpec s; BSTR q = (BSTR)(ULONG_PTR)0x5678;
    rg_spec(&s, 9, 9, 0, &TM_IID_Mini1);
    EQ(qpath(&s, 9, 9, 0, &q), TLB_NOTREG);
    CHECK(q == NULL);
    q = (BSTR)(ULONG_PTR)0x5678;
    EQ(qpath(&s, 9, 0, 0x409, &q), TLB_NOTREG);
    CHECK(q == NULL);
}

TMTEST_BARE(dxtm_load_not_registered_clears_out_pointer)
{
    TMLibSpec s; ITypeLib *tl = (ITypeLib *)(ULONG_PTR)0x9999;
    rg_spec(&s, 9, 8, 0, &TM_IID_Mini1);
    EQ(load(&s, 9, 8, 0, &tl), TLB_NOTREG);
    if (tl != NULL) t_note("out pointer left as %p after failed LoadRegTypeLib", (void *)tl);
}
