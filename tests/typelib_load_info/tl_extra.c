/* File-copy based load tests (relative paths, corruption), object lifetime, worker thread use. */
#include "tl.h"

#define SENTINEL ((ITypeLib *)(ULONG_PTR)0x1badf00d)

static BOOL copy_stdole(WCHAR *dst, int cap, const char *leaf)
{
    char a[MAX_PATH]; WCHAR src[MAX_PATH];
    t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, dst, cap);
    tl_sysfile(src, MAX_PATH, L"stdole2.tlb");
    return CopyFileW(src, dst, FALSE);
}

static BYTE *slurp(const WCHAR *p, DWORD *n)
{
    HANDLE h = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(h, NULL), r = 0;
    BYTE *b = (BYTE *)malloc(sz ? sz : 1);
    if (!b || !ReadFile(h, b, sz, &r, NULL) || r != sz) { CloseHandle(h); free(b); return NULL; }
    CloseHandle(h); *n = sz; return b;
}

static void spit(const WCHAR *p, const BYTE *b, DWORD n)
{
    HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, b, n, &w, NULL); CloseHandle(h); }
}

TEST(tlli_x_copy_loads_with_same_content)
{
    WCHAR p[MAX_PATH];
    if (!copy_stdole(p, MAX_PATH, "olet_copy1.tlb")) { t_skip("cannot copy stdole2.tlb"); return; }
    ITypeLib *a = NULL, *b = tl_open_stdole();
    TEQ(tl_open_file(p, &a), S_OK);
    REQUIRE(a && b);
    TEQ(ITypeLib_GetTypeInfoCount(a), ITypeLib_GetTypeInfoCount(b));
    CHECK(a != b);                                    /* different path, different object */
    for (UINT i = 0; i < ITypeLib_GetTypeInfoCount(a); i++) {
        BSTR n1 = NULL, n2 = NULL;
        ITypeLib_GetDocumentation(a, (INT)i, &n1, NULL, NULL, NULL);
        ITypeLib_GetDocumentation(b, (INT)i, &n2, NULL, NULL, NULL);
        CHECK(n1 && n2 && !lstrcmpW(n1, n2));
        tl_freebstr(n1); tl_freebstr(n2);
    }
    ITypeLib_Release(a); ITypeLib_Release(b);
    DeleteFileW(p);
}

TEST(tlli_x_relative_path_resolution)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH], old[MAX_PATH], dir[MAX_PATH];
    if (!copy_stdole(p, MAX_PATH, "olet_rel.tlb")) { t_skip("cannot copy stdole2.tlb"); return; }
    GetCurrentDirectoryW(MAX_PATH, old);
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, dir, MAX_PATH);
    if (!SetCurrentDirectoryW(dir)) { t_skip("cannot chdir"); DeleteFileW(p); return; }
    static const WCHAR *names[] = { L"olet_rel.tlb", L".\\olet_rel.tlb", L"OLET_REL.TLB" };
    for (int i = 0; i < 3; i++) {
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(names[i], REGKIND_NONE, &tl);
        CHECKF(hr == S_OK, "%ls hr=0x%08lx", names[i], (unsigned long)hr);
        if (hr == S_OK && tl) {
            TLIBATTR *a = NULL;
            if (SUCCEEDED(ITypeLib_GetLibAttr(tl, &a))) { CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle)); ITypeLib_ReleaseTLibAttr(tl, a); }
            ITypeLib_Release(tl);
        }
    }
    /* a name that exists nowhere in the cwd or search path */
    ITypeLib *tl = SENTINEL;
    CHECK(FAILED(f(L"olet_not_here.tlb", REGKIND_NONE, &tl)));
    CHECK(tl == NULL);
    SetCurrentDirectoryW(old);
    DeleteFileW(p);
}

TEST(tlli_x_forward_slashes_and_trailing)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH], q[MAX_PATH];
    tl_sysfile(p, MAX_PATH, L"stdole2.tlb");
    lstrcpyW(q, p);
    for (WCHAR *c = q; *c; c++) if (*c == L'\\') *c = L'/';
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(q, REGKIND_NONE, &tl);
    t_note("forward-slash path -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && tl) ITypeLib_Release(tl);
    lstrcpyW(q, p); lstrcatW(q, L"\\1");                 /* file\1: resource index on a non-PE file */
    tl = SENTINEL;
    hr = f(q, REGKIND_NONE, &tl);
    t_note("stdole2.tlb\\1 -> 0x%08lx", (unsigned long)hr);
    if (SUCCEEDED(hr) && tl && tl != SENTINEL) ITypeLib_Release(tl); else CHECK(tl == NULL || tl == SENTINEL);
}

TEST(tlli_x_truncated_and_corrupt_copies)
{
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR src[MAX_PATH], p[MAX_PATH]; char a[MAX_PATH];
    tl_sysfile(src, MAX_PATH, L"stdole2.tlb");
    DWORD n = 0; BYTE *orig = slurp(src, &n);
    if (!orig || n < 512) { t_skip("cannot read stdole2.tlb"); free(orig); return; }
    t_tmpfile(a, sizeof a, "olet_corrupt.tlb");
    MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH);
    const DWORD cuts[] = { 4, 16, 64, 100, 0x54, n / 4, n / 2, n - 1 };
    for (unsigned i = 0; i < sizeof cuts / sizeof cuts[0]; i++) {
        if (t_is_wine()) continue;   /* wine's MSFT reader crashes on truncated files */
        spit(p, orig, cuts[i]);
        ITypeLib *tl = SENTINEL;
        HRESULT hr = f(p, REGKIND_NONE, &tl);
        /* Windows 10 reports either TYPE_E_CANTLOADLIBRARY (0x80029c4a) or 0x80028ca2 for the same truncated file, from boot to
           boot, so only a code outside that pair is worth printing */
        if ((unsigned long)hr == 0x80029c4aUL || (unsigned long)hr == 0x80028ca2UL)
            t_note("truncated to %lu bytes -> a typelib load error (CANTLOADLIBRARY or 0x80028ca2)", (unsigned long)cuts[i]);
        else
            t_note("truncated to %lu bytes -> 0x%08lx", (unsigned long)cuts[i], (unsigned long)hr);
        if (cuts[i] < 0x54) {
            CHECKF(FAILED(hr), "truncated to %lu loaded", (unsigned long)cuts[i]);
            CHECK(tl == NULL);
        }
        if (SUCCEEDED(hr) && tl && tl != SENTINEL) ITypeLib_Release(tl);
        DeleteFileW(p);
    }
    /* destroyed magic number */
    BYTE *copy = (BYTE *)malloc(n);
    memcpy(copy, orig, n);
    copy[0] = 'X';
    spit(p, copy, n);
    ITypeLib *tl = SENTINEL;
    HRESULT hr = f(p, REGKIND_NONE, &tl);
    CHECKF(FAILED(hr), "bad magic loaded: 0x%08lx", (unsigned long)hr);
    CHECK(tl == NULL);
    t_note("bad magic -> 0x%08lx", (unsigned long)hr);
    DeleteFileW(p);
    free(copy); free(orig);
}

TEST(tlli_x_failed_load_leaves_nothing_cached)
{
    /* after a failed load of a path, creating a valid file there and loading again must succeed */
    HRESULT (WINAPI *f)(LPCOLESTR, REGKIND, ITypeLib **) = UIN("oleaut32.dll", LoadTypeLibEx);
    REQUIRE(f);
    WCHAR p[MAX_PATH]; char a[MAX_PATH];
    t_tmpfile(a, sizeof a, "olet_later.tlb");
    MultiByteToWideChar(CP_ACP, 0, a, -1, p, MAX_PATH);
    ITypeLib *tl = SENTINEL;
    CHECK(FAILED(f(p, REGKIND_NONE, &tl)));
    CHECK(tl == NULL);
    WCHAR src[MAX_PATH]; tl_sysfile(src, MAX_PATH, L"stdole2.tlb");
    if (!CopyFileW(src, p, FALSE)) { t_skip("cannot copy"); return; }
    tl = SENTINEL;
    TEQ(f(p, REGKIND_NONE, &tl), S_OK);
    if (tl && tl != SENTINEL) ITypeLib_Release(tl);
    DeleteFileW(p);
}

TEST(tlli_x_typeinfo_keeps_library_alive)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = tl_get_named(tl, L"IDispatch");
    REQUIRE(ti);
    ITypeLib_Release(tl);                       /* our only library reference is gone */
    ITypeLib *c = NULL; UINT idx = 0x7777;
    TEQ(ITypeInfo_GetContainingTypeLib(ti, &c, &idx), S_OK);
    CHECK(c != NULL);
    if (c) {
        TLIBATTR *a = NULL;
        TEQ(ITypeLib_GetLibAttr(c, &a), S_OK);
        if (a) { CHECK(tl_guid_eq(&a->guid, &TL_LIBID_StdOle)); ITypeLib_ReleaseTLibAttr(c, a); }
        ITypeLib_Release(c);
    }
    TYPEATTR *ta = NULL;
    TEQ(ITypeInfo_GetTypeAttr(ti, &ta), S_OK);
    if (ta) { CHECK(tl_guid_eq(&ta->guid, &TL_IID_IDispatch)); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    ITypeInfo_Release(ti);
}

TEST(tlli_x_typeinfo_refcount_protocol)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = tl_get_named(tl, L"IUnknown");
    REQUIRE(ti);
    ULONG a = ITypeInfo_AddRef(ti), b = ITypeInfo_AddRef(ti);
    TEQ(b, a + 1);
    TEQ(ITypeInfo_Release(ti), a);
    TEQ(ITypeInfo_Release(ti), a - 1);
    ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

struct walk { int ok; UINT types; };
static DWORD WINAPI walk_thread(LPVOID arg)
{
    struct walk *w = (struct walk *)arg;
    ITypeLib *tl = tl_open_stdole();
    if (!tl) return 1;
    w->types = ITypeLib_GetTypeInfoCount(tl);
    w->ok = 1;
    for (UINT i = 0; i < w->types; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) { w->ok = 0; continue; }
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) ITypeInfo_ReleaseTypeAttr(ti, ta); else w->ok = 0;
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
    return 0;
}

TEST(tlli_x_concurrent_walks)
{
    enum { N = 4 };
    HANDLE h[N]; struct walk w[N];
    memset(w, 0, sizeof w);
    int started = 0;
    for (int i = 0; i < N; i++) {
        h[i] = CreateThread(NULL, 0, walk_thread, &w[i], 0, NULL);
        if (h[i]) started++;
    }
    for (int i = 0; i < N; i++) {
        if (!h[i]) continue;
        DWORD r = WaitForSingleObject(h[i], 10000);
        CHECK(r == WAIT_OBJECT_0);
        if (r != WAIT_OBJECT_0) TerminateThread(h[i], 1);
        CloseHandle(h[i]);
    }
    CHECK(started == N);
    for (int i = 0; i < N; i++) { CHECKF(w[i].ok, "thread %d", i); CHECK(w[i].types == w[0].types); }
}

TEST(tlli_x_getdocumentation_name_matches_typeinfo_for_members)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    ITypeInfo *ti = tl_get_named(tl, L"IDispatch");
    REQUIRE(ti);
    static const struct { MEMBERID id; const WCHAR *n; } m[] = {
        { 0x60010000, L"GetTypeInfoCount" }, { 0x60010001, L"GetTypeInfo" }, { 0x60010002, L"GetIDsOfNames" }, { 0x60010003, L"Invoke" },
        { 0x60000000, L"QueryInterface" }, { 0x60000001, L"AddRef" }, { 0x60000002, L"Release" },
    };
    for (unsigned i = 0; i < 7; i++) {
        BSTR n = NULL;
        HRESULT hr = ITypeInfo_GetDocumentation(ti, m[i].id, &n, NULL, NULL, NULL);
        /* inherited IUnknown members are not documented via the derived interface's lookup */
        if (i < 4) { TEQ(hr, S_OK); CHECK(n && tl_weq(n, m[i].n)); }
        else t_note("IDispatch GetDocumentation(inherited %ls) -> 0x%08lx", m[i].n, (unsigned long)hr);
        tl_freebstr(n);
    }
    ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

/* ---- whole-library function description invariants ---- */

static int vt_ok(VARTYPE vt)
{
    VARTYPE b = vt & VT_TYPEMASK;
    return b <= VT_CLSID || b == VT_USERDEFINED || b == VT_PTR || b == VT_SAFEARRAY || b == VT_CARRAY || b == VT_HRESULT || b == VT_INT_PTR || b == VT_UINT_PTR;
}

TEST(tlli_x_all_funcs_invariants)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl), total = 0;
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        if (FAILED(ITypeInfo_GetTypeAttr(ti, &ta))) { ITypeInfo_Release(ti); continue; }
        for (UINT f = 0; f < ta->cFuncs; f++) {
            FUNCDESC *fd = NULL; BSTR nm[32]; UINT c = 0;
            if (FAILED(ITypeInfo_GetFuncDesc(ti, f, &fd)) || !fd) { CHECK(0); continue; }
            total++;
            CHECKF(fd->cParams >= 0 && fd->cParams < 31, "type %u func %u cParams %d", i, f, fd->cParams);
            CHECKF(fd->cParamsOpt == 0 || fd->cParamsOpt == -1 || (fd->cParamsOpt > 0 && fd->cParamsOpt <= fd->cParams), "cParamsOpt %d", fd->cParamsOpt);
            CHECK(fd->invkind == INVOKE_FUNC || fd->invkind == INVOKE_PROPERTYGET || fd->invkind == INVOKE_PROPERTYPUT || fd->invkind == INVOKE_PROPERTYPUTREF);
            CHECK(fd->funckind >= FUNC_VIRTUAL && fd->funckind <= FUNC_DISPATCH);
            CHECK(vt_ok(fd->elemdescFunc.tdesc.vt));
            if (ta->typekind == TKIND_INTERFACE) {
                CHECK(fd->funckind == FUNC_PUREVIRTUAL);
                CHECK(fd->callconv == CC_STDCALL);
            }
            if (ta->typekind == TKIND_DISPATCH) CHECK(fd->funckind == FUNC_DISPATCH);
            if (ta->typekind == TKIND_MODULE) CHECK(fd->funckind == FUNC_STATIC);
            for (SHORT p = 0; p < fd->cParams && fd->lprgelemdescParam; p++) {
                CHECKF(vt_ok(fd->lprgelemdescParam[p].tdesc.vt), "type %u func %u param %d vt %d", i, f, p, fd->lprgelemdescParam[p].tdesc.vt);
                if (fd->lprgelemdescParam[p].tdesc.vt == VT_PTR) CHECK(fd->lprgelemdescParam[p].tdesc.lptdesc != NULL);
                if (fd->lprgelemdescParam[p].tdesc.vt == VT_CARRAY) CHECK(fd->lprgelemdescParam[p].tdesc.lpadesc != NULL);
            }
            memset(nm, 0, sizeof nm);
            if (SUCCEEDED(ITypeInfo_GetNames(ti, fd->memid, nm, 32, &c))) {
                CHECKF(c >= 1 && c <= (UINT)fd->cParams + 1, "type %u func %u names %u params %d", i, f, c, fd->cParams);
                for (UINT k = 0; k < c; k++) tl_freebstr(nm[k]);
            } else CHECK(0);
            ITypeInfo_ReleaseFuncDesc(ti, fd);
        }
        ITypeInfo_ReleaseTypeAttr(ti, ta);
        ITypeInfo_Release(ti);
    }
    CHECK(total > 40);
    t_note("walked %u function descriptions", total);
    ITypeLib_Release(tl);
}

TEST(tlli_x_all_vars_invariants)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl), total = 0;
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        if (FAILED(ITypeInfo_GetTypeAttr(ti, &ta))) { ITypeInfo_Release(ti); continue; }
        for (UINT v = 0; v < ta->cVars; v++) {
            VARDESC *vd = NULL; BSTR nm = NULL; UINT c = 0;
            if (FAILED(ITypeInfo_GetVarDesc(ti, v, &vd)) || !vd) { CHECK(0); continue; }
            total++;
            CHECK(vd->varkind >= VAR_PERINSTANCE && vd->varkind <= VAR_DISPATCH);
            if (ta->typekind == TKIND_ENUM) { CHECK(vd->varkind == VAR_CONST); CHECK(vd->lpvarValue != NULL); }
            if (ta->typekind == TKIND_RECORD) CHECK(vd->varkind == VAR_PERINSTANCE && (ULONG)vd->oInst < ta->cbSizeInstance);
            CHECK(vt_ok(vd->elemdescVar.tdesc.vt));
            if (SUCCEEDED(ITypeInfo_GetNames(ti, vd->memid, &nm, 1, &c))) { TEQ(c, 1); CHECK(nm && nm[0]); tl_freebstr(nm); } else CHECK(0);
            ITypeInfo_ReleaseVarDesc(ti, vd);
        }
        ITypeInfo_ReleaseTypeAttr(ti, ta);
        ITypeInfo_Release(ti);
    }
    CHECK(total >= 3 + 4 + 4);
    ITypeLib_Release(tl);
}

TEST(tlli_x_memids_unique_within_type)
{
    ITypeLib *tl; TL_REQUIRE_STDOLE(tl);
    UINT n = ITypeLib_GetTypeInfoCount(tl);
    for (UINT i = 0; i < n; i++) {
        ITypeInfo *ti = NULL; TYPEATTR *ta = NULL;
        if (FAILED(ITypeLib_GetTypeInfo(tl, i, &ti)) || !ti) continue;
        if (SUCCEEDED(ITypeInfo_GetTypeAttr(ti, &ta))) {
            for (UINT a = 0; a < ta->cFuncs; a++) for (UINT b = a + 1; b < ta->cFuncs; b++) {
                FUNCDESC *fa = NULL, *fb = NULL;
                ITypeInfo_GetFuncDesc(ti, a, &fa); ITypeInfo_GetFuncDesc(ti, b, &fb);
                if (fa && fb && fa->memid == fb->memid)
                    CHECKF(fa->invkind != fb->invkind, "type %u: funcs %u and %u share memid 0x%lx and invkind", i, a, b, fa->memid);
                if (fa) ITypeInfo_ReleaseFuncDesc(ti, fa);
                if (fb) ITypeInfo_ReleaseFuncDesc(ti, fb);
            }
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}
