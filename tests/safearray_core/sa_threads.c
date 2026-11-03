/* Arrays are not apartment bound: they can be created on one thread and used/destroyed on another. */
#include "sa.h"

typedef struct { SAFEARRAY *psa; HRESULT hr; BSTR got; } XT;

static DWORD WINAPI worker_use_destroy(LPVOID arg)
{
    XT *x = arg; LONG i = 1;
    x->hr = pGetElement(x->psa, &i, &x->got);
    if (SUCCEEDED(x->hr)) {
        BSTR s = bstr_w(L"from-worker"); i = 0;
        x->hr = pPutElement(x->psa, &i, s);
        pSysFreeString(s);
    }
    return 0;
}

TEST(sa_threads_bstr_array_used_on_other_thread)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR s = bstr_w(L"main-thread"); LONG i = 1;
    EQHR(pPutElement(psa, &i, s), S_OK);
    pSysFreeString(s);
    XT x = { psa, E_FAIL, NULL };
    HANDLE h = CreateThread(NULL, 0, worker_use_destroy, &x, 0, NULL);
    CHECK(h != NULL);
    if (h) { EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0); CloseHandle(h); }
    EQHR(x.hr, S_OK);
    CHECK(x.got && !lstrcmpW(x.got, L"main-thread"));
    pSysFreeString(x.got);
    i = 0; BSTR o = NULL;
    EQHR(pGetElement(psa, &i, &o), S_OK);
    CHECK(o && !lstrcmpW(o, L"from-worker"));
    pSysFreeString(o);
    EQHR(pDestroy(psa), S_OK);
}

static DWORD WINAPI worker_destroy(LPVOID arg)
{
    XT *x = arg;
    x->hr = pDestroy(x->psa);
    return 0;
}

TEST(sa_threads_destroy_on_other_thread)
{
    SA_BEGIN();
    static const VARTYPE vts[] = { VT_BSTR, VT_VARIANT, VT_UNKNOWN, VT_I4 };
    for (unsigned k = 0; k < 4; k++) {
        SAFEARRAY *psa = sa1(vts[k], 0, 3);
        CHECK(psa != NULL);
        if (!psa) continue;
        TObj o; tobj_init(&o, 1);
        LONG i = 0;
        if (vts[k] == VT_BSTR) { BSTR s = bstr_w(L"abc"); pPutElement(psa, &i, s); pSysFreeString(s); }
        else if (vts[k] == VT_UNKNOWN) { pPutElement(psa, &i, OBJ(o)); }
        else if (vts[k] == VT_VARIANT) { VARIANT v; pVariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = bstr_w(L"v"); pPutElement(psa, &i, &v); pVariantClear(&v); }
        XT x = { psa, E_FAIL, NULL };
        HANDLE h = CreateThread(NULL, 0, worker_destroy, &x, 0, NULL);
        CHECK(h != NULL);
        if (h) { EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0); CloseHandle(h); }
        EQHR(x.hr, S_OK);
        EQ(o.ref, 1);
    }
}

typedef struct { int ok; } LoopT;
static DWORD WINAPI worker_loop(LPVOID arg)
{
    LoopT *l = arg; l->ok = 1;
    for (int n = 0; n < 2000; n++) {
        SAFEARRAY *psa = sa1(VT_BSTR, 0, 8);
        if (!psa) { l->ok = 0; break; }
        for (LONG i = 0; i < 8; i++) { BSTR s = bstr_w(L"loop"); if (FAILED(pPutElement(psa, &i, s))) l->ok = 0; pSysFreeString(s); }
        if (FAILED(pDestroy(psa))) l->ok = 0;
    }
    return 0;
}

TEST(sa_threads_concurrent_create_destroy)
{
    SA_BEGIN();
    LoopT l[4]; HANDLE h[4];
    for (int k = 0; k < 4; k++) h[k] = CreateThread(NULL, 0, worker_loop, &l[k], 0, NULL);
    for (int k = 0; k < 4; k++) {
        CHECK(h[k] != NULL);
        if (h[k]) { EQ(WaitForSingleObject(h[k], 20000), WAIT_OBJECT_0); CloseHandle(h[k]); EQ(l[k].ok, 1); }
    }
}
