/* gap_06: CreateTypeLib argument handling (decomp: szFile==NULL or ppctlib==NULL => E_INVALIDARG with *ppctlib untouched;
 * otherwise a new generic typelib object with refcount 1 is returned, nothing is written to disk until SaveAllChanges). */
#include "gap6.h"
#include <oleauto.h>

static const GUID G6_IID_ICreateTypeLib = {0x00020406,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G6_IID_ICreateTypeLib2 = {0x0002040F,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G6_IID_ITypeLib = {0x00020402,0,0,{0xC0,0,0,0,0,0,0,0x46}};

TEST(gap6_createtypelib_null_args) {
    REQUIRE(U(CreateTypeLib));
    if (t_is_wine()) { t_skip("wine does not validate NULL here"); return; }
    ICreateTypeLib *p = (ICreateTypeLib *)(ULONG_PTR)0x55;
    EQ(U(CreateTypeLib)(SYS_WIN32, NULL, &p), E_INVALIDARG);
    CHECK(p == (ICreateTypeLib *)(ULONG_PTR)0x55);        /* not written on this failure path */
    EQ(U(CreateTypeLib)(SYS_WIN32, L"x.tlb", NULL), E_INVALIDARG);
    EQ(U(CreateTypeLib)(SYS_WIN32, NULL, NULL), E_INVALIDARG);
}
TEST(gap6_createtypelib_returns_object_refcount_one) {
    REQUIRE(U(CreateTypeLib));
    WCHAR path[MAX_PATH]; wsprintfW(path, L"%hs\\gap6_ctl_%lu.tlb", t_tmpdir(), GetCurrentProcessId());
    ICreateTypeLib *p = NULL;
    HRESULT hr = U(CreateTypeLib)(SYS_WIN32, path, &p);
    EQ(hr, S_OK);
    CHECK(p != NULL); if (!p) return;
    EQ(p->lpVtbl->AddRef(p), 2);
    EQ(p->lpVtbl->Release(p), 1);
    EQ(GetFileAttributesW(path), INVALID_FILE_ATTRIBUTES);  /* nothing on disk before SaveAllChanges */
    EQ(p->lpVtbl->Release(p), 0);
    EQ(GetFileAttributesW(path), INVALID_FILE_ATTRIBUTES);
}
TEST(gap6_createtypelib_qi_interfaces) {
    REQUIRE(U(CreateTypeLib));
    ICreateTypeLib *p = NULL;
    if (U(CreateTypeLib)(SYS_WIN32, L"gap6_qi.tlb", &p) != S_OK || !p) { CHECK(0); return; }
    void *q = NULL;
    EQ(p->lpVtbl->QueryInterface(p, &G6_IID_IUnknown, &q), S_OK); if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    q = NULL;
    EQ(p->lpVtbl->QueryInterface(p, &G6_IID_ICreateTypeLib, &q), S_OK); if (q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    q = (void *)1;
    HRESULT hr = p->lpVtbl->QueryInterface(p, &G6_IID_ICreateTypeLib2, &q);
    t_note("ICreateTypeLib2 from CreateTypeLib object: 0x%lx", hr); if (hr == S_OK && q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    q = (void *)1;
    hr = p->lpVtbl->QueryInterface(p, &G6_IID_ITypeLib, &q);
    t_note("ITypeLib from CreateTypeLib object: 0x%lx", hr); if (hr == S_OK && q) ((IUnknown *)q)->lpVtbl->Release((IUnknown *)q);
    static const GUID bogus = {0xdeadbeef,1,2,{3,4,5,6,7,8,9,10}};
    q = (void *)1;
    EQ(p->lpVtbl->QueryInterface(p, &bogus, &q), E_NOINTERFACE);
    CHECK(q == NULL);
    p->lpVtbl->Release(p);
}
TEST(gap6_createtypelib_syskind_values_accepted) {
    REQUIRE(U(CreateTypeLib));
    static const SYSKIND k[] = { SYS_WIN16, SYS_WIN32, SYS_MAC, SYS_WIN64 };
    for (unsigned i = 0; i < 4; i++) {
        ICreateTypeLib *p = NULL;
        HRESULT hr = U(CreateTypeLib)(k[i], L"gap6_sk.tlb", &p);
        CHECKF(hr == S_OK && p, "syskind %d hr 0x%lx", k[i], hr);
        if (p) p->lpVtbl->Release(p);
    }
}
TEST(gap6_createtypelib_independent_objects) {
    REQUIRE(U(CreateTypeLib));
    ICreateTypeLib *a = NULL, *b = NULL;
    EQ(U(CreateTypeLib)(SYS_WIN32, L"gap6_a.tlb", &a), S_OK);
    EQ(U(CreateTypeLib)(SYS_WIN32, L"gap6_a.tlb", &b), S_OK);
    CHECK(a && b && a != b);
    if (a) a->lpVtbl->Release(a);
    if (b) b->lpVtbl->Release(b);
}
