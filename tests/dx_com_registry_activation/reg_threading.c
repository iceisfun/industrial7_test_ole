/* InprocServer32\ThreadingModel vs. the calling apartment: which thread/apartment hosts the instance and whether the
 * caller gets the object itself or a marshaled proxy. Each (model, caller kind) runs in a fresh child process so
 * that "the main STA" is well defined:
 *   sta0  caller is the first STA of the process (the main STA)
 *   sta1  a helper thread owns the main STA (pumping), the caller is a second STA
 *   mta0  caller is in the MTA, the process has no STA
 *   mta1  caller is in the MTA, a helper thread owns the main STA (pumping) */
#include "dxcommon.h"

#define NEED() do { DX_BEGIN(); if (!dx_selfload_ok()) { t_skip("exe cannot act as in-proc server"); return; } } while (0)

static HANDLE g_hq, g_hready; static DWORD g_htid;
static void pump(void) { MSG m; while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); } }
static DWORD WINAPI helper(LPVOID a) {
    U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED); g_htid = GetCurrentThreadId(); SetEvent(g_hready);
    for (;;) { DWORD w = MsgWaitForMultipleObjects(1, &g_hq, FALSE, 30000, QS_ALLINPUT); if (w != WAIT_OBJECT_0 + 1) break; pump(); }
    U(CoUninitialize)(); return 0;
}
#define STAAPT(a) ((a) == APTTYPE_STA || (a) == APTTYPE_MAINSTA)

CHILD(dxthr) {
    /* argv: dxthr <clsid> <model> <kind> res=.. */
    GUID c; const char *model, *kind; int bad = 0, sta, helperq; HANDLE ht = NULL; IUnknown *u = NULL; dx_info i; HRESULT hr;
    DWORD me = GetCurrentThreadId(); int direct;
    if (argc < 4 || !dx_parse_guid(argv[1], &c)) return 90;
    model = argv[2]; kind = argv[3]; sta = kind[0] == 's'; helperq = kind[3] == '1';
#define K(cond, msg) do { if (!(cond)) { dx_log(argc, argv, "%s/%s: %s (server tid=%lu apt=%d qual=%d direct=%d caller=%lu helper=%lu)", model, kind, msg, i.tid, i.apt, i.qual, direct, me, g_htid); bad++; } } while (0)
    memset(&i, 0, sizeof i); direct = -1;
    if (helperq) {
        g_hq = CreateEventA(NULL, TRUE, FALSE, NULL); g_hready = CreateEventA(NULL, TRUE, FALSE, NULL);
        ht = CreateThread(NULL, 0, helper, NULL, 0, NULL);
        if (WaitForSingleObject(g_hready, 10000) != WAIT_OBJECT_0) return 91;
    }
    hr = U(CoInitializeEx)(NULL, sta ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    if (FAILED(hr)) return 92;
    hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IPersist, (void **)&u);
    if (FAILED(hr) || !u) { dx_log(argc, argv, "%s/%s: CoCreateInstance hr=%08lx", model, kind, hr); bad++; }
    else {
        hr = dx_getinfo(u, &i);
        direct = dx_is_direct(u);
        K(SUCCEEDED(hr) && i.marker_ok, "GetClassID through the (proxy) pointer");
        if (!_stricmp(model, "none")) {
            if (!strcmp(kind, "sta0")) { K(direct, "legacy server in the main STA: direct pointer"); K(i.tid == me, "same thread"); K(STAAPT(i.apt), "STA"); }
            else if (!strcmp(kind, "sta1")) { K(!direct, "legacy server from a second STA: proxy"); K(i.tid == g_htid, "created in the main STA thread"); K(STAAPT(i.apt), "STA"); }
            else if (!strcmp(kind, "mta0")) { K(!direct, "legacy server from the MTA: proxy"); K(i.tid != me, "other thread"); K(STAAPT(i.apt), "hosted in an STA"); }
            else { K(!direct, "legacy server from the MTA: proxy"); K(i.tid != me, "other thread"); K(STAAPT(i.apt), "hosted in an STA"); if (i.tid == g_htid) dx_log(argc, argv, "none/mta1: hosted in the existing main STA"); else dx_log(argc, argv, "none/mta1: hosted in a new host STA"); }
        } else if (!_stricmp(model, "Apartment")) {
            if (sta) { K(direct, "apartment server in an STA caller: direct pointer"); K(i.tid == me, "same thread"); K(STAAPT(i.apt), "STA"); }
            else { K(!direct, "apartment server from the MTA: proxy"); K(i.tid != me, "other thread"); K(STAAPT(i.apt), "hosted in an STA"); }
        } else if (!_stricmp(model, "Free")) {
            if (sta) { K(!direct, "free-threaded server from an STA: proxy"); K(i.tid != me, "other thread"); K(i.apt == APTTYPE_MTA, "hosted in the MTA"); }
            else { K(direct, "free-threaded server in the MTA caller: direct pointer"); K(i.tid == me, "same thread"); K(i.apt == APTTYPE_MTA, "MTA"); }
        } else if (!_stricmp(model, "Both")) {
            K(direct, "both-threaded server: direct pointer in any apartment"); K(i.tid == me, "same thread");
            if (sta) K(STAAPT(i.apt), "STA"); else K(i.apt == APTTYPE_MTA, "MTA");
        } else if (!_stricmp(model, "Neutral")) {
            K(i.tid == me, "neutral apartment runs on the caller's thread"); if (i.apt != APTTYPE_NA) dx_log(argc, argv, "NOTE Neutral/%s: CoGetApartmentType inside the factory reported %d (APTTYPE_NA=2)", kind, i.apt);
        }
        u->lpVtbl->Release(u);
    }
    U(CoUninitialize)();
    if (helperq) { SetEvent(g_hq); WaitForSingleObject(ht, 10000); }
    return bad ? 1 : 0;
}

static void run_model(const char *model, const char *regmodel) {
    static const char *kinds[] = { "sta0", "sta1", "mta0", "mta1" };
    GUID c; char gs[40], args[200], log[2048];
    dx_reg_inproc(&c, regmodel, NULL, 0); dx_guidstr(&c, gs);
    for (int k = 0; k < 4; k++) {
        snprintf(args, sizeof args, "%s %s %s", gs, model, kinds[k]);
        DWORD code = dx_child("dxthr", args, 30000, log, sizeof log);
        CHECKF(code == 0, "model %s caller %s: exit %lx\n%s", model, kinds[k], code, log);
        if (code == 0 && log[0]) t_note("%s", log);
    }
}
TEST(dxthr_model_apartment) { REQUIRE(U(CoCreateInstance)); NEED(); run_model("Apartment", "Apartment"); }
TEST(dxthr_model_free) { REQUIRE(U(CoCreateInstance)); NEED(); run_model("Free", "Free"); }
TEST(dxthr_model_both) { REQUIRE(U(CoCreateInstance)); NEED(); run_model("Both", "Both"); }
TEST(dxthr_model_neutral) { REQUIRE(U(CoCreateInstance)); NEED(); run_model("Neutral", "Neutral"); }
TEST(dxthr_model_none) { REQUIRE(U(CoCreateInstance)); NEED(); run_model("none", NULL); }
TEST(dxthr_model_value_case_insensitive) {
    REQUIRE(U(CoCreateInstance)); NEED();
    /* the same expectations as the canonical spelling; a mismatch means the string compare is case-sensitive */
    GUID c; char gs[40], args[200], log[1024]; dx_reg_inproc(&c, "BOTH", NULL, 0); dx_guidstr(&c, gs);
    snprintf(args, sizeof args, "%s Both sta1", gs);
    DWORD code = dx_child("dxthr", args, 30000, log, sizeof log);
    t_note("ThreadingModel=\"BOTH\" treated like Both: %s (exit %lx) %s", code == 0 ? "yes" : "no", code, log);
}
TEST(dxthr_model_bogus_value) {
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; dx_reg_inproc(&c, "NoSuchModel", NULL, 0); dx_coinit(COINIT_APARTMENTTHREADED);
    IUnknown *u = NULL; HRESULT hr = U(CoCreateInstance)(&c, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void **)&u);
    t_note("ThreadingModel=\"NoSuchModel\": hr=%08lx (REGDB_E_BADTHREADINGMODEL=80040156)", hr);
    if (u) u->lpVtbl->Release(u);
    dx_couninit();
}
TEST(dxthr_apartment_server_proxy_calls_work) {
    /* from an MTA thread the apartment-model instance is reached through a proxy; calls (and Release) must work */
    REQUIRE(U(CoCreateInstance)); NEED();
    GUID c; char gs[40], args[200], log[512]; dx_reg_inproc(&c, "Apartment", NULL, 0); dx_guidstr(&c, gs);
    snprintf(args, sizeof args, "%s Apartment mta0", gs);
    DWORD code = dx_child("dxthr", args, 30000, log, sizeof log);
    CHECKF(code == 0, "exit %lx %s", code, log);
}
