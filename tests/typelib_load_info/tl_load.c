/* LoadTypeLib / LoadTypeLibEx / LoadRegTypeLib / QueryPathOfRegTypeLib / UnRegisterTypeLib (read paths). */
#include "tl.h"

#define SENTINEL ((ITypeLib *)(ULONG_PTR)0x1badf00d)

static void w_from_tmp(const char *leaf, WCHAR *out, int cap)
{
    char a[MAX_PATH];
    t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, cap);
}

static void write_file(const WCHAR *path, const void *data, DWORD len)
{
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD w; if (len) WriteFile(h, data, len, &w, NULL);
        CloseHandle(h);
    }
}

static int is_load_failure_code(HRESULT hr)
{
    return hr == TYPE_E_CANTLOADLIBRARY || hr == TYPE_E_IOERROR || hr == TYPE_E_UNSUPFORMAT ||
           hr == TYPE_E_INVDATAREAD || hr == STG_E_FILENOTFOUND || hr == STG_E_PATHNOTFOUND ||
           hr == STG_E_INVALIDHEADER || hr == STG_E_FILEALREADYEXISTS || hr == STG_E_ACCESSDENIED;
}

/* ---- argument validation (decomp: LoadTypeLibEx) ---- */

TEST(tlli_loadex_null_file)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    ITypeLib *tl = SENTINEL;
    TEQ(f(NULL, REGKIND_NONE, &tl), E_INVALIDARG);
    CHECK(tl == SENTINEL);                       /* out param is untouched on argument failure */
    tl = SENTINEL;
    TEQ(f(NULL, REGKIND_DEFAULT, &tl), E_INVALIDARG);
    CHECK(tl == SENTINEL);
    tl = SENTINEL;
    TEQ(f(NULL, REGKIND_REGISTER, &tl), E_INVALIDARG);
    CHECK(tl == SENTINEL);
}

TEST(tlli_loadex_null_out)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    TEQ(f(p, REGKIND_NONE, NULL), E_INVALIDARG);
    TEQ(f(NULL, REGKIND_NONE, NULL), E_INVALIDARG);
    TEQ(f(L"", REGKIND_NONE, NULL), E_INVALIDARG);
}

TEST(tlli_load_null_args)
{
    HRESULT (WINAPI *f)(LPCOLESTR, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLib);
    REQUIRE(f);
    ITypeLib *tl = SENTINEL;
    TEQ(f(NULL, NULL), E_INVALIDARG);
    TEQ(f(NULL, &tl), E_INVALIDARG);
    CHECK(tl == SENTINEL);
    TEQ(f(L"stdole2.tlb", NULL), E_INVALIDARG);
}

TEST(tlli_loadex_bad_regkind)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    /* after masking off LOAD_TLB_AS_32BIT/64BIT (0x20/0x40) only 0,1,2 are valid */
    static const int bad[] = { 3, 4, 5, 6, 7, 8, 0x10, 0x13, 0x80, 0x100 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(p, (REGKIND)bad[i], &tl);
        if (!t_is_wine()) { CHECKF(hr == E_INVALIDARG, "regkind 0x%x -> 0x%08lx", bad[i], (unsigned long)hr);
        CHECKF(tl == SENTINEL, "regkind 0x%x touched out param", bad[i]); }
        if (SUCCEEDED(hr) && tl && tl != SENTINEL) ITypeLib_Release(tl);
    }
}

TEST(tlli_loadex_regkind_arch_flags)
{
    /* LOAD_TLB_AS_32BIT (0x20) / LOAD_TLB_AS_64BIT (0x40) are masked out of the REGKIND test. */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    static const int flags[] = { 0x40, 0x20, 0x60 };
    for (unsigned i = 0; i < 3; i++) {
        ITypeLib *tl = NULL;
        HRESULT hr = f(p, (REGKIND)(REGKIND_NONE | flags[i]), &tl);
        if (SUCCEEDED(hr)) {
            CHECK(tl != NULL);
            TLIBATTR *a = NULL;
            CHECK(SUCCEEDED(ITypeLib_GetLibAttr(tl, &a)) && a);
            if (a) { CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle)); ITypeLib_ReleaseTLibAttr(tl, a); }
            ITypeLib_Release(tl);
        } else {
            t_note("LoadTypeLibEx(stdole2, NONE|0x%x) -> 0x%08lx", flags[i], (unsigned long)hr);
            CHECK(tl == NULL);
            CHECK(hr != E_INVALIDARG);          /* flag bits are never an argument error */
        }
    }
}

/* ---- failing loads ---- */

TEST(tlli_loadex_empty_string)
{
    /* decomp: *szFile == 0 -> 0x80030002 (STG_E_FILENOTFOUND), out param already set to NULL */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    static const REGKIND k[] = { REGKIND_NONE, REGKIND_DEFAULT, REGKIND_REGISTER };
    for (int i = 0; i < 3; i++) {
        ITypeLib *tl = SENTINEL;
        WCHECK(f(L"", k[i], &tl) == STG_E_FILENOTFOUND);
        WCHECK(tl == NULL);
    }
    ITypeLib *tl = SENTINEL;
    HRESULT (WINAPI *g)(LPCOLESTR, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLib);
    if (g) { WCHECK(g(L"", &tl) == STG_E_FILENOTFOUND); WCHECK(tl == NULL); }
}

TEST(tlli_loadex_missing_absolute)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; w_from_tmp("olet_no_such.tlb", p, MAX_PATH);
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(p, REGKIND_NONE, &tl);
    CHECK(FAILED(hr));
    CHECK(tl == NULL);
    CHECKF(hr == TYPE_E_CANTLOADLIBRARY, "got 0x%08lx", (unsigned long)hr);   /* decomp: FindTypeLib failure -> 0x80029C4A */
    t_note("missing absolute path -> 0x%08lx", (unsigned long)hr);
}

TEST(tlli_loadex_missing_relative)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    static const WCHAR *names[] = { L"olet_no_such_lib_xyz.tlb", L"olet_no_such_lib_xyz", L"olet_no_such\\dir\\x.tlb", L"olet_nolib_xyz.dll" };
    for (int i = 0; i < 4; i++) {
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(names[i], REGKIND_NONE, &tl);
        CHECKF(FAILED(hr), "name %d hr=0x%08lx", i, (unsigned long)hr);
        CHECK(tl == NULL);
        CHECKF(is_load_failure_code(hr), "name %d hr=0x%08lx", i, (unsigned long)hr);
        t_note("missing relative %d -> 0x%08lx", i, (unsigned long)hr);
    }
}

TEST(tlli_loadex_directory)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR dir[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, dir, MAX_PATH);
    int n = lstrlenW(dir);
    if (n > 3 && dir[n - 1] == L'\\') dir[n - 1] = 0;
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(dir, REGKIND_NONE, &tl);
    CHECK(FAILED(hr));
    CHECK(tl == NULL);
    CHECKF(is_load_failure_code(hr), "hr=0x%08lx", (unsigned long)hr);
    t_note("directory -> 0x%08lx", (unsigned long)hr);
    GetSystemDirectoryW(dir, MAX_PATH);
    tl = SENTINEL;
    hr = f(dir, REGKIND_NONE, &tl);
    CHECK(FAILED(hr)); CHECK(tl == NULL);
}

TEST(tlli_loadex_zero_length_file)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; w_from_tmp("olet_empty.tlb", p, MAX_PATH);
    write_file(p, NULL, 0);
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(p, REGKIND_NONE, &tl);
    CHECK(FAILED(hr)); CHECK(tl == NULL);
    CHECKF(is_load_failure_code(hr), "hr=0x%08lx", (unsigned long)hr);
    t_note("zero length file -> 0x%08lx", (unsigned long)hr);
    DeleteFileW(p);
}

TEST(tlli_loadex_garbage_files)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    static const char text[] = "This is not a type library. Plain text content for olet.\r\n";
    BYTE zeros[4096], mz[512], magic[512];
    memset(zeros, 0, sizeof zeros);
    memset(mz, 0, sizeof mz); mz[0] = 'M'; mz[1] = 'Z';
    memset(magic, 0, sizeof magic); memcpy(magic, "MSFT", 4);   /* MSFT magic but truncated header */
    struct { const char *leaf; const void *d; DWORD n; } cases[] = {
        { "olet_garb_text.tlb", text, sizeof text - 1 },
        { "olet_garb_zero.tlb", zeros, sizeof zeros },
        { "olet_garb_mz.tlb", mz, sizeof mz },
        { "olet_garb_msft.tlb", magic, 8 },
        { "olet_garb_one.tlb", "x", 1 },
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        if (t_is_wine() && i == 3) continue;   /* wine crashes on a truncated MSFT header */
        WCHAR p[MAX_PATH]; w_from_tmp(cases[i].leaf, p, MAX_PATH);
        write_file(p, cases[i].d, cases[i].n);
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(p, REGKIND_NONE, &tl);
        CHECKF(FAILED(hr), "case %u hr=0x%08lx", i, (unsigned long)hr);
        CHECKF(tl == NULL, "case %u", i);
        CHECKF(is_load_failure_code(hr), "case %u hr=0x%08lx", i, (unsigned long)hr);
        t_note("garbage case %u (%s) -> 0x%08lx", i, cases[i].leaf, (unsigned long)hr);
        DeleteFileW(p);
    }
}

TEST(tlli_loadex_pe_without_typelib)
{
    /* a PE image with no TYPELIB resource: TYPE_E_CANTLOADLIBRARY */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    static const WCHAR *dlls[] = { L"kernel32.dll", L"ntdll.dll", L"advapi32.dll" };
    for (int i = 0; i < 3; i++) {
        WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, dlls[i]);
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(p, REGKIND_NONE, &tl);
        CHECKF(hr == TYPE_E_CANTLOADLIBRARY, "%d: hr=0x%08lx", i, (unsigned long)hr);
        CHECK(tl == NULL);
    }
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(L"kernel32.dll", REGKIND_NONE, &tl);     /* found via search path */
    CHECK(hr == TYPE_E_CANTLOADLIBRARY); CHECK(tl == NULL);
}

TEST(tlli_loadex_resource_index_missing)
{
    /* "file\N" selects TYPELIB resource N; a PE with no such resource cannot be loaded */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR base[MAX_PATH], p[MAX_PATH];
    tl_sysfile(base, MAX_PATH, L"kernel32.dll");
    static const WCHAR *idx[] = { L"\\1", L"\\2", L"\\99", L"\\0", L"\\x" };
    for (int i = 0; i < 5; i++) {
        lstrcpyW(p, base); lstrcatW(p, idx[i]);
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(p, REGKIND_NONE, &tl);
        CHECKF(FAILED(hr), "index %d hr=0x%08lx", i, (unsigned long)hr);
        CHECK(tl == NULL);
        t_note("kernel32.dll%ls -> 0x%08lx", idx[i], (unsigned long)hr);
    }
}

TEST(tlli_loadex_resource_index_in_oleaut32)
{
    /* Real Windows ships TYPELIB resources in several system DLLs; record what is observed. */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    static const WCHAR *dlls[] = { L"oleaut32.dll", L"ole32.dll", L"shell32.dll", L"msvbvm60.dll", L"mshtml.tlb" };
    for (int d = 0; d < 5; d++) {
        for (int i = 0; i <= 2; i++) {
            WCHAR p[MAX_PATH], s[16];
            tl_sysfile(p, MAX_PATH, dlls[d]);
            if (i) { wsprintfW(s, L"\\%d", i); lstrcatW(p, s); }
            ITypeLib *tl = SENTINEL;
            HRESULT hr = f(p, REGKIND_NONE, &tl);
            if (SUCCEEDED(hr)) {
                CHECK(tl != NULL && tl != SENTINEL);
                TLIBATTR *a = NULL;
                if (tl && SUCCEEDED(ITypeLib_GetLibAttr(tl, &a))) {
                    t_note("%ls idx %d: loads, libid {%08lx-...} v%u.%u syskind %d", dlls[d], i, a->guid.Data1, a->wMajorVerNum, a->wMinorVerNum, a->syskind);
                    ITypeLib_ReleaseTLibAttr(tl, a);
                }
                if (tl) ITypeLib_Release(tl);
            } else {
                CHECK(tl == NULL);
            }
        }
    }
}

/* ---- stdole2.tlb loads ---- */

TEST(tlli_stdole2_absolute_all_regkinds_none)
{
    ITypeLib *tl = NULL;
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    if (GetFileAttributesW(p) == INVALID_FILE_ATTRIBUTES) { t_note("no stdole2.tlb in system dir"); }
    HRESULT hr = tl_open_file(p, &tl);
    TEQ(hr, S_OK);
    CHECK(tl != NULL);
    if (tl) {
        TEQ(ITypeLib_Release(tl) < 100, 1);
    }
}

TEST(tlli_stdole2_filename_only)
{
    HRESULT (WINAPI *f)(LPCOLESTR, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLib);
    REQUIRE(f);
    ITypeLib *tl = NULL;
    HRESULT hr = f(L"stdole2.tlb", &tl);         /* found through the search path (System32) */
    TEQ(hr, S_OK);
    CHECK(tl != NULL);
    if (tl) {
        TLIBATTR *a = NULL;
        CHECK(SUCCEEDED(ITypeLib_GetLibAttr(tl, &a)));
        if (a) { CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle)); ITypeLib_ReleaseTLibAttr(tl, a); }
        ITypeLib_Release(tl);
    }
}

TEST(tlli_stdole2_case_insensitive_name)
{
    HRESULT (WINAPI *f)(LPCOLESTR, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLib);
    REQUIRE(f);
    ITypeLib *tl = NULL;
    HRESULT hr = f(L"STDOLE2.TLB", &tl);
    TEQ(hr, S_OK);
    if (tl) ITypeLib_Release(tl);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"STDOLE2.tlb");
    tl = NULL;
    hr = f(p, &tl);
    TEQ(hr, S_OK);
    if (tl) ITypeLib_Release(tl);
}

TEST(tlli_stdole2_repeated_loads)
{
    ITypeLib *a = NULL, *b = NULL;
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    TEQ(tl_open_file(p, &a), S_OK);
    TEQ(tl_open_file(p, &b), S_OK);
    REQUIRE(a && b);
    t_note("two loads of same absolute path: same object = %d", a == b);
    TEQ(ITypeLib_GetTypeInfoCount(a), ITypeLib_GetTypeInfoCount(b));
    /* lifetime: releasing one leaves the other usable */
    ITypeLib_Release(a);
    TLIBATTR *at = NULL;
    CHECK(SUCCEEDED(ITypeLib_GetLibAttr(b, &at)));
    if (at) ITypeLib_ReleaseTLibAttr(b, at);
    ITypeLib_Release(b);
}

TEST(tlli_stdole2_loadtypelib_default_is_readonly_for_registered)
{
    /* LoadTypeLib == LoadTypeLibEx(REGKIND_DEFAULT) per decomp; result is equivalent to REGKIND_NONE */
    HRESULT (WINAPI *f)(LPCOLESTR, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLib);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    ITypeLib *a = NULL, *b = NULL;
    TEQ(f(p, &a), S_OK);
    TEQ(tl_open_file(p, &b), S_OK);
    REQUIRE(a && b);
    TEQ(ITypeLib_GetTypeInfoCount(a), ITypeLib_GetTypeInfoCount(b));
    TLIBATTR *x = NULL, *y = NULL;
    CHECK(SUCCEEDED(ITypeLib_GetLibAttr(a, &x)) && SUCCEEDED(ITypeLib_GetLibAttr(b, &y)));
    if (x && y) {
        CHECK(!memcmp(x, y, sizeof *x));
    }
    if (x) ITypeLib_ReleaseTLibAttr(a, x);
    if (y) ITypeLib_ReleaseTLibAttr(b, y);
    ITypeLib_Release(a); ITypeLib_Release(b);
}

TEST(tlli_stdole32_loads_on_64bit)
{
    /* oleaut32 redirects/loads stdole32.tlb on 64-bit Windows (decomp: stdole32.tlb special case) */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; tl_sysfile(p, MAX_PATH, L"stdole32.tlb");
    if (GetFileAttributesW(p) == INVALID_FILE_ATTRIBUTES) { t_skip("stdole32.tlb not present"); return; }
    ITypeLib *tl = NULL;
    HRESULT hr = f(p, REGKIND_NONE, &tl);
    CHECKF(hr == S_OK, "hr=0x%08lx", (unsigned long)hr);
    if (tl) {
        TLIBATTR *a = NULL;
        if (SUCCEEDED(ITypeLib_GetLibAttr(tl, &a))) {
            WCHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle));
            WCHECK(a->wMajorVerNum == 2);
            t_note("stdole32.tlb: minor %u syskind %d lcid %lu flags 0x%x", a->wMinorVerNum, a->syskind, a->lcid, a->wLibFlags);
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        ITypeLib_Release(tl);
    }
}

/* ---- registry-based lookup (read-only) ---- */

TEST(tlli_queryreg_stdole)
{
    HRESULT (WINAPI *f)(const GUID *, USHORT, USHORT, LCID, LPBSTR) = UIN("oleaut32.dll", QueryPathOfRegTypeLib);
    REQUIRE(f);
    BSTR path = (BSTR)SENTINEL;
    HRESULT hr = f(&TL_LIBID_StdOle, 2, 0, 0, &path);
    TEQ(hr, S_OK);
    if (hr == S_OK) {
        CHECK(path != NULL && path != (BSTR)SENTINEL);
        if (path && path != (BSTR)SENTINEL) {
            int n = lstrlenW(path);
            CHECK(n > 11);
            WCHECK(n >= 11 && !lstrcmpiW(path + n - 11, L"stdole2.tlb"));
            CHECK(GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES);
            { UINT (WINAPI *sl)(BSTR) = (void *)t_proc_in("oleaut32.dll", "SysStringLen"); if (sl) CHECK(sl(path) == (UINT)n); }
            tl_freebstr(path);
        }
    }
    path = (BSTR)SENTINEL;
    hr = f(&TL_LIBID_StdOle, 0xffff, 0xffff, 0, &path);     /* "any version": highest registered */
    TEQ(hr, S_OK);
    if (hr == S_OK && path && path != (BSTR)SENTINEL) tl_freebstr(path);
    path = (BSTR)SENTINEL;
    hr = f(&TL_LIBID_StdOle, 2, 0, 0x409, &path);           /* neutral registration serves en-US */
    t_note("QueryPath stdole 2.0 lcid 0x409 -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK && path && path != (BSTR)SENTINEL) tl_freebstr(path);
}

TEST(tlli_queryreg_null_out)
{
    HRESULT (WINAPI *f)(const GUID *, USHORT, USHORT, LCID, LPBSTR) = UIN("oleaut32.dll", QueryPathOfRegTypeLib);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    TEQ(f(&TL_LIBID_StdOle, 2, 0, 0, NULL), E_INVALIDARG);
}

TEST(tlli_queryreg_unregistered)
{
    HRESULT (WINAPI *f)(const GUID *, USHORT, USHORT, LCID, LPBSTR) = UIN("oleaut32.dll", QueryPathOfRegTypeLib);
    REQUIRE(f);
    BSTR path = (BSTR)SENTINEL;
    TEQ(f(&TL_GUID_NONE, 1, 0, 0, &path), TYPE_E_LIBNOTREGISTERED);
    WCHECK(path == NULL);                          /* decomp: *lpbstrPathName = 0 before the lookup */
    path = (BSTR)SENTINEL;
    TEQ(f(&TL_GUID_NONE, 0xffff, 0xffff, 0, &path), TYPE_E_LIBNOTREGISTERED);
    WCHECK(path == NULL);
}

TEST(tlli_queryreg_version_mismatch)
{
    HRESULT (WINAPI *f)(const GUID *, USHORT, USHORT, LCID, LPBSTR) = UIN("oleaut32.dll", QueryPathOfRegTypeLib);
    REQUIRE(f);
    static const struct { USHORT ma, mi; } bad[] = { {1,0}, {3,0}, {2,1}, {2,7}, {2,100}, {1,5} , {0,0} };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        BSTR path = (BSTR)SENTINEL;
        HRESULT hr = f(&TL_LIBID_StdOle, bad[i].ma, bad[i].mi, 0, &path);
        if (!(t_is_wine() && bad[i].ma < 2)) CHECKF(hr == TYPE_E_LIBNOTREGISTERED, "%u.%u -> 0x%08lx", bad[i].ma, bad[i].mi, (unsigned long)hr);
        if (SUCCEEDED(hr) && path && path != (BSTR)SENTINEL) tl_freebstr(path);
        else WCHECK(path == NULL);
    }
}

TEST(tlli_loadreg_stdole)
{
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, ITypeLib **) = UIN("oleaut32.dll", LoadRegTypeLib);
    REQUIRE(f);
    static const LCID lcids[] = { 0, 0x409, 0x800, 0x400, 0x0009 };
    for (unsigned i = 0; i < sizeof lcids / sizeof lcids[0]; i++) {
        ITypeLib *tl = NULL;
        HRESULT hr = f(&TL_LIBID_StdOle, 2, 0, lcids[i], &tl);
        CHECKF(hr == S_OK, "lcid 0x%lx -> 0x%08lx", lcids[i], (unsigned long)hr);
        if (tl) {
            TLIBATTR *a = NULL;
            CHECK(SUCCEEDED(ITypeLib_GetLibAttr(tl, &a)));
            if (a) {
                CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle));
                CHECK(a->wMajorVerNum == 2 && a->wMinorVerNum >= 0);
                ITypeLib_ReleaseTLibAttr(tl, a);
            }
            ITypeLib_Release(tl);
        }
    }
}

TEST(tlli_loadreg_any_version)
{
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, ITypeLib **) = UIN("oleaut32.dll", LoadRegTypeLib);
    REQUIRE(f);
    ITypeLib *tl = NULL;
    HRESULT hr = f(&TL_LIBID_StdOle, 0xffff, 0xffff, 0, &tl);
    t_note("LoadRegTypeLib(stdole, ffff.ffff) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && tl) {
        TLIBATTR *a = NULL;
        if (SUCCEEDED(ITypeLib_GetLibAttr(tl, &a))) {
            CHECK(a->wMajorVerNum == 2);
            ITypeLib_ReleaseTLibAttr(tl, a);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlli_loadreg_unregistered)
{
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, ITypeLib **) = UIN("oleaut32.dll", LoadRegTypeLib);
    REQUIRE(f);
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(&TL_GUID_NONE, 1, 0, 0, &tl);
    TEQ(hr, TYPE_E_LIBNOTREGISTERED);
    CHECK(tl == NULL);                            /* set to NULL up front by LoadRegTypeLib_WithSyskind */
}

TEST(tlli_loadreg_version_mismatch)
{
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, ITypeLib **) = UIN("oleaut32.dll", LoadRegTypeLib);
    REQUIRE(f);
    static const struct { WORD ma, mi; } bad[] = { {1,0}, {3,0}, {2,1}, {2,50}, {0,0} };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(&TL_LIBID_StdOle, bad[i].ma, bad[i].mi, 0, &tl);
        if (t_is_wine() && bad[i].ma < 2) { if (tl) ITypeLib_Release(tl); continue; }
        CHECKF(hr == TYPE_E_LIBNOTREGISTERED, "%u.%u -> 0x%08lx", bad[i].ma, bad[i].mi, (unsigned long)hr);
        CHECKF(tl == NULL, "%u.%u", bad[i].ma, bad[i].mi);
        if (SUCCEEDED(hr) && tl) ITypeLib_Release(tl);
    }
}

TEST(tlli_loadreg_wrong_lcid)
{
    /* a registration for neutral lcid 0 is served for any lcid; a specific-lcid-only lib is not (not testable
       read-only here), so only record the outcome for an odd lcid */
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, ITypeLib **) = UIN("oleaut32.dll", LoadRegTypeLib);
    REQUIRE(f);
    ITypeLib *tl = NULL;
    HRESULT hr = f(&TL_LIBID_StdOle, 2, 0, 0x0407, &tl);
    t_note("LoadRegTypeLib(stdole, de-DE) -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && tl) ITypeLib_Release(tl);
}

TEST(tlli_unregister_unregistered)
{
    /* nothing to delete: registry access error, no side effect */
    HRESULT (WINAPI *f)(const GUID *, WORD, WORD, LCID, SYSKIND) = UIN("oleaut32.dll", UnRegisterTypeLib);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine returns E_INVALIDARG"); return; }
    TEQ(f(&TL_GUID_NONE, 1, 0, 0, SYS_WIN64), TYPE_E_REGISTRYACCESS);
    TEQ(f(&TL_GUID_NONE, 1, 0, 0, SYS_WIN32), TYPE_E_REGISTRYACCESS);
    TEQ(f(&TL_GUID_NONE, 7, 3, 0x409, SYS_WIN64), TYPE_E_REGISTRYACCESS);
}
