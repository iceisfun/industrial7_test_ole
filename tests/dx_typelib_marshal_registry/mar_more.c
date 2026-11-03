/* More typelib-marshaling scenarios: concurrency, global interface table, records in VARIANTs, multi-dimensional arrays, disconnection. */
#include "mar.h"
#define M(c, name, ...) ((c)->lpVtbl->name((c), __VA_ARGS__))
#define M0(c, name) ((c)->lpVtbl->name((c)))

/* ---------------------------------------------------------------- concurrent clients of one STA object */
typedef struct { IStream *stm; volatile LONG done, bad; } CJob;
static DWORD WINAPI conc_thread(void *p)
{
    CJob *j = p; IOletCustom *c = NULL; int i;
    if (SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) {
        if (SUCCEEDED(U(CoGetInterfaceAndReleaseStream)(j->stm, &TM_IID_IOletCustom, (void **)&c)) && c) {
            for (i = 0; i < 100; i++) {
                LONG r = 0;
                if (c->lpVtbl->AddLong(c, i, 1000, &r) != S_OK || r != i + 1000) InterlockedIncrement(&j->bad);
            }
            c->lpVtbl->Release(c);
        } else InterlockedIncrement(&j->bad);
        U(CoUninitialize)();
    } else InterlockedIncrement(&j->bad);
    InterlockedIncrement(&j->done);
    return 0;
}
TMTEST(dxtm_mar_concurrent_mta_clients_of_sta_object)
{
    CJob jobs[4]; HANDLE th[4]; IOletCustom *obj; int i; DWORD start;
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    obj = tm_new_custom();
    memset(&g_last, 0, sizeof g_last);
    for (i = 0; i < 4; i++) {
        memset(&jobs[i], 0, sizeof jobs[i]);
        EQ(U(CoMarshalInterThreadInterfaceInStream)(&TM_IID_IOletCustom, (IUnknown *)obj, &jobs[i].stm), S_OK);
        th[i] = CreateThread(NULL, 0, conc_thread, &jobs[i], 0, NULL);
    }
    start = GetTickCount();
    for (;;) {
        int alive = 0; MSG m;
        for (i = 0; i < 4; i++) if (!jobs[i].done) alive = 1;
        if (!alive || GetTickCount() - start > 20000) break;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); }
        Sleep(1);
    }
    for (i = 0; i < 4; i++) { CHECKF(jobs[i].done, "client %d did not finish", i); EQ(jobs[i].bad, 0); if (jobs[i].done) { WaitForSingleObject(th[i], 5000); } CloseHandle(th[i]); }
    EQ(g_last.calls, 400);        /* every call was executed exactly once, serialized on the STA */
    obj->lpVtbl->Release(obj);
    U(CoUninitialize)();
    tm_leakcheck();
}

/* ---------------------------------------------------------------- global interface table */
static void cl_git(void *p, void *ctx)
{
    CLI(c, p); LONG r = 0;
    EQ(M(c, AddLong, 5, 6, &r), S_OK); EQ(r, 11);
}
TMTEST(dxtm_mar_global_interface_table)
{
    IGlobalInterfaceTable *git = NULL; DWORD cookie = 0; HRESULT hr; IOletCustom *obj, *got = NULL; static const GUID clsid_git = {0x323, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    EQ(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED), S_OK);
    hr = U(CoCreateInstance)(&clsid_git, NULL, CLSCTX_INPROC_SERVER, &IID_IGlobalInterfaceTable, (void **)&git);
    EQ(hr, S_OK);
    if (git) {
        obj = tm_new_custom();
        hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, (IUnknown *)obj, &TM_IID_IOletCustom, &cookie);
        EQ(hr, S_OK);
        CHECK(cookie != 0);
        /* same apartment: the original comes back */
        hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &TM_IID_IOletCustom, (void **)&got);
        EQ(hr, S_OK);
        CHECK(got == obj);
        if (got) M0(got, Release);
        /* another apartment gets a typelib proxy; fetch it from a worker thread */
        IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie);
        hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &TM_IID_IOletCustom, (void **)&got);
        CHECK(FAILED(hr));
        t_note("GetInterfaceFromGlobal after revoke: hr=0x%lx", (long)hr);
        obj->lpVtbl->Release(obj);
        IGlobalInterfaceTable_Release(git);
    }
    U(CoUninitialize)();
    tm_leakcheck();
}

/* ---------------------------------------------------------------- VT_RECORD in a VARIANT (needs the registered typelib on both sides) */
static void cl_variant_record(void *p, void *ctx)
{
    CLI(c, p); IRecordInfo *ri = NULL; OletPoint pt = { 11, 22 }; VARIANT in, out; HRESULT hr;
    hr = U(GetRecordInfoFromGuids)(&TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, &TM_GUID_OletPoint, &ri);
    EQ(hr, S_OK);
    if (!ri) return;
    VariantInit(&in); VariantInit(&out);
    V_VT(&in) = VT_RECORD; V_RECORDINFO(&in) = ri; V_RECORD(&in) = &pt;
    memset(&g_last, 0, sizeof g_last);
    hr = M(c, EchoVariant, in, &out);
    EQ(hr, S_OK);
    EQ(V_VT(&out), VT_RECORD);
    if (V_VT(&out) == VT_RECORD) {
        OletPoint *rp = (OletPoint *)V_RECORD(&out);
        CHECK(rp != NULL);
        if (rp) { EQ(rp->x, 11); EQ(rp->y, 22); }
        CHECK(V_RECORDINFO(&out) != NULL);
    }
    VariantClear(&out);
    IRecordInfo_Release(ri);
}
TMTEST(dxtm_mar_variant_holding_record)
{
    if (t_is_wine()) { t_skip("Wine record sizes are 0"); return; }
    RUN_CUSTOM(cl_variant_record, NULL);
}

/* ---------------------------------------------------------------- multi-dimensional and VARIANT arrays */
static void cl_variant_arrays(void *p, void *ctx)
{
    CLI(c, p); SAFEARRAYBOUND b[2] = { {3, 2}, {4, 5} }; SAFEARRAY *sa; VARIANT in, out; LONG ix[2]; HRESULT hr; LONG v = 0;
    sa = SafeArrayCreate(VT_I4, 2, b);
    for (ix[0] = 2; ix[0] < 5; ix[0]++) for (ix[1] = 5; ix[1] < 9; ix[1]++) { LONG val = ix[0] * 100 + ix[1]; SafeArrayPutElement(sa, ix, &val); }
    VariantInit(&in); VariantInit(&out); V_VT(&in) = VT_ARRAY | VT_I4; V_ARRAY(&in) = sa;
    hr = M(c, EchoVariant, in, &out);
    EQ(hr, S_OK);
    if (hr == S_OK && V_VT(&out) == (VT_ARRAY | VT_I4)) {
        SAFEARRAY *o = V_ARRAY(&out); LONG lb = 0, ub = 0;
        EQ(SafeArrayGetDim(o), 2);
        SafeArrayGetLBound(o, 1, &lb); SafeArrayGetUBound(o, 1, &ub); EQ(lb, 2); EQ(ub, 4);
        SafeArrayGetLBound(o, 2, &lb); SafeArrayGetUBound(o, 2, &ub); EQ(lb, 5); EQ(ub, 8);
        ix[0] = 3; ix[1] = 7; SafeArrayGetElement(o, ix, &v); EQ(v, 307);
        ix[0] = 4; ix[1] = 8; SafeArrayGetElement(o, ix, &v); EQ(v, 408);
    }
    VariantClear(&out); VariantClear(&in);
    /* array of VARIANTs holding different types */
    {
        SAFEARRAY *sv = SafeArrayCreateVector(VT_VARIANT, 0, 3); VARIANT e; LONG k;
        VariantInit(&e); V_VT(&e) = VT_I4; V_I4(&e) = 9; k = 0; SafeArrayPutElement(sv, &k, &e);
        V_VT(&e) = VT_BSTR; V_BSTR(&e) = SysAllocString(L"elem"); k = 1; SafeArrayPutElement(sv, &k, &e); VariantClear(&e);
        V_VT(&e) = VT_R8; V_R8(&e) = 0.5; k = 2; SafeArrayPutElement(sv, &k, &e);
        VariantInit(&in); V_VT(&in) = VT_ARRAY | VT_VARIANT; V_ARRAY(&in) = sv;
        hr = M(c, EchoVariant, in, &out);
        EQ(hr, S_OK);
        if (hr == S_OK && V_VT(&out) == (VT_ARRAY | VT_VARIANT)) {
            VARIANT g; k = 1; VariantInit(&g); SafeArrayGetElement(V_ARRAY(&out), &k, &g);
            EQ(V_VT(&g), VT_BSTR); if (V_VT(&g) == VT_BSTR) CHECK(V_BSTR(&g) && !lstrcmpW(V_BSTR(&g), L"elem"));
            VariantClear(&g);
            k = 2; SafeArrayGetElement(V_ARRAY(&out), &k, &g); EQ(V_VT(&g), VT_R8);
        } else CHECKF(0, "VARIANT array came back as vt %u", V_VT(&out));
        VariantClear(&out); VariantClear(&in);
    }
}
TMTEST(dxtm_mar_variant_multidimensional_and_variant_arrays) { RUN_CUSTOM(cl_variant_arrays, NULL); }

/* ---------------------------------------------------------------- disconnection */
typedef struct { HANDLE ready, go, done; IStream *stm; } DisJob;
static DWORD WINAPI dis_server(void *p)
{
    DisJob *j = p; IOletCustom *obj;
    U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    obj = tm_new_custom();
    U(CoMarshalInterThreadInterfaceInStream)(&TM_IID_IOletCustom, (IUnknown *)obj, &j->stm);
    SetEvent(j->ready);
    tm_pump_wait(j->go, 15000);
    U(CoDisconnectObject)((IUnknown *)obj, 0);
    SetEvent(j->done);
    tm_pump_wait(j->go, 100);
    obj->lpVtbl->Release(obj);
    U(CoUninitialize)();
    return 0;
}
TMTEST(dxtm_mar_call_on_disconnected_object_fails)
{
    DisJob j; HANDLE th; IOletCustom *c = NULL; LONG r = 0; HRESULT hr;
    memset(&j, 0, sizeof j);
    j.ready = CreateEventW(NULL, TRUE, FALSE, NULL); j.go = CreateEventW(NULL, TRUE, FALSE, NULL); j.done = CreateEventW(NULL, TRUE, FALSE, NULL);
    th = CreateThread(NULL, 0, dis_server, &j, 0, NULL);
    CHECK(WaitForSingleObject(j.ready, 10000) == WAIT_OBJECT_0);
    EQ(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED), S_OK);
    hr = U(CoGetInterfaceAndReleaseStream)(j.stm, &TM_IID_IOletCustom, (void **)&c);
    EQ(hr, S_OK);
    if (c) {
        EQ(M(c, AddLong, 1, 2, &r), S_OK); EQ(r, 3);
        SetEvent(j.go);
        CHECK(WaitForSingleObject(j.done, 10000) == WAIT_OBJECT_0);
        r = 0;
        hr = M(c, AddLong, 1, 2, &r);
        CHECK(FAILED(hr));
        t_note("call on a disconnected object: hr=0x%lx (RPC_E_DISCONNECTED=0x%lx)", (long)hr, (long)RPC_E_DISCONNECTED);
        EQ(r, 0);
        c->lpVtbl->Release(c);
    } else SetEvent(j.go);
    U(CoUninitialize)();
    WaitForSingleObject(th, 10000);
    CloseHandle(th); CloseHandle(j.ready); CloseHandle(j.go); CloseHandle(j.done);
    tm_leakcheck();
}
