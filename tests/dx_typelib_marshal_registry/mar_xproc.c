/* Typelib-driven marshaling to a separate process: the child unmarshals a stream written by the parent. */
#include "mar.h"
#define M(c, name, ...) ((c)->lpVtbl->name((c), __VA_ARGS__))
#define M0(c, name) ((c)->lpVtbl->name((c)))

static int g_cfails;
#define XC(cond) do { if (!(cond)) { printf("  CHILD FAIL line %d: %s\n", __LINE__, #cond); g_cfails++; } } while (0)
#define XCH(hr_, want) do { HRESULT h_ = (hr_); if (h_ != (HRESULT)(want)) { printf("  CHILD FAIL line %d: %s = 0x%lx want 0x%lx\n", __LINE__, #hr_, (long)h_, (long)(HRESULT)(want)); g_cfails++; } } while (0)

static BSTR bigstr(UINT n) { BSTR b = SysAllocStringLen(NULL, n); UINT i; for (i = 0; i < n; i++) b[i] = (WCHAR)(1 + i % 3000); return b; }

static void scen_basic(IOletCustom *c)
{
    LONG r = 0, t = 0; BSTR out = NULL, in; IOletSink *s = NULL; SAFEARRAY *sa; VARIANT v, o; OletColor col = 0;
    LONG tid = 0, pid = 0, apt = 0;
    XCH(M(c, AddLong, 40, 2, &r), S_OK); XC(r == 42);
    XCH(M(c, ApartmentInfo, &tid, &pid, &apt), S_OK);
    XC((DWORD)pid != GetCurrentProcessId());
    in = bigstr(100000);
    XCH(M(c, EchoBstr, in, &out), S_OK);
    XC(out && SysStringLen(out) == 100000 && !memcmp(out, in, 200000));
    SysFreeString(in); SysFreeString(out);
    sa = SafeArrayCreateVector(VT_I4, 0, 50);
    { LONG i; for (i = 0; i < 50; i++) SafeArrayPutElement(sa, &i, &i); }
    XCH(M(c, SumArray, sa, &r), S_OK); XC(r == 1225);
    SafeArrayDestroy(sa);
    VariantInit(&v); VariantInit(&o); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"xproc");
    XCH(M(c, EchoVariant, v, &o), S_OK); XC(V_VT(&o) == VT_BSTR && V_BSTR(&o) && !lstrcmpW(V_BSTR(&o), L"xproc"));
    VariantClear(&v); VariantClear(&o);
    XCH(M(c, EchoColor, OLET_BLUE, &col), S_OK); XC(col == 8);
    XCH(M(c, GetSink, &s), S_OK);
    XC(s != NULL);
    if (s) {
        XCH(s->lpVtbl->Total(s, &t), S_OK); XC(t == 100);
        XCH(s->lpVtbl->Notify(s, 3, NULL), S_OK);
        XCH(s->lpVtbl->Total(s, &t), S_OK); XC(t == 103);
        s->lpVtbl->Release(s);
    }
    XCH(M(c, ReturnHr, (LONG)0x8abc1234), (LONG)0x8abc1234);
    XCH(M(c, ReturnHr, S_FALSE), S_FALSE);
}

static void scen_callback(IOletCustom *c)
{
    IOletSink *mine = tm_new_sink(); LONG total = 0;
    XCH(M(c, Callback, mine, 4, &total), S_OK);       /* parent process calls back into this process */
    XC(total == 4);
    XCH(M(c, Callback, mine, 6, &total), S_OK);
    XC(total == 10 && tm_sink_total(mine) == 10);
    mine->lpVtbl->Release(mine);
}

static void scen_errinfo(IOletCustom *c)
{
    BSTR d = SysAllocString(L"cross process error"); IErrorInfo *ei = NULL; BSTR got = NULL; GUID g; DWORD hc = 0;
    SetErrorInfo(0, NULL);
    XCH(M(c, RaiseError, d, E_UNEXPECTED), E_UNEXPECTED);
    XCH(GetErrorInfo(0, &ei), S_OK);
    XC(ei != NULL);
    if (ei) {
        XCH(IErrorInfo_GetDescription(ei, &got), S_OK); XC(got && !lstrcmpW(got, L"cross process error"));
        XCH(IErrorInfo_GetGUID(ei, &g), S_OK); XC(IsEqualGUID(&g, &TM_IID_IOletCustom));
        XCH(IErrorInfo_GetHelpContext(ei, &hc), S_OK); XC(hc == 4242);
        SysFreeString(got); IErrorInfo_Release(ei);
    }
    SysFreeString(d);
}

static void scen_dual(IOletDual *d)
{
    IDispatch *disp = NULL; VARIANT args[2], res; DISPPARAMS dp; LONG r = 0; IUnknown *u = NULL; IEnumVARIANT *e = NULL; VARIANT v; ULONG f = 0;
    XCH(M(d, Add, 1, 2, &r), S_OK); XC(r == 3);
    XCH(IUnknown_QueryInterface((IUnknown *)d, &IID_IDispatch, (void **)&disp), S_OK);
    if (disp) {
        VariantInit(&args[0]); VariantInit(&args[1]); VariantInit(&res);
        V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 100; V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 23;
        dp.rgvarg = args; dp.cArgs = 2; dp.cNamedArgs = 0; dp.rgdispidNamedArgs = NULL;
        XCH(IDispatch_Invoke(disp, 1, &TM_IID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL), S_OK);
        XC(V_VT(&res) == VT_I4 && V_I4(&res) == 123);
        IDispatch_Release(disp);
    }
    XCH(M(d, get__NewEnum, &u), S_OK);
    if (u) {
        XCH(IUnknown_QueryInterface(u, &IID_IEnumVARIANT, (void **)&e), S_OK);
        if (e) {
            int i;
            for (i = 1; i <= 3; i++) { VariantInit(&v); XCH(IEnumVARIANT_Next(e, 1, &v, &f), S_OK); XC(V_I4(&v) == 10 * i); }
            IEnumVARIANT_Release(e);
        }
        IUnknown_Release(u);
    }
}

static void scen_hold(IOletCustom *c)
{
    /* take extra references and drop them again; the last Release (done by process exit path of the caller) frees the server object */
    LONG r = 0; int i;
    for (i = 0; i < 5; i++) { M0(c, AddRef); }
    XCH(M(c, AddLong, 1, 1, &r), S_OK);
    for (i = 0; i < 5; i++) { M0(c, Release); }
}

CHILD(tm_xp)
{
    /* argv: tm_xp <scenario> <S|M> <stream-file> <iid-kind> */
    const char *scen = argc > 1 ? argv[1] : "basic"; const char *apt = argc > 2 ? argv[2] : "M"; const char *file = argc > 3 ? argv[3] : "";
    HRESULT hr; HANDLE h; DWORD n = 0, got = 0; HGLOBAL hg; void *mem; IStream *stm = NULL; void *itf = NULL; const GUID *iid;
    iid = (scen[0] == 'd') ? &TM_IID_IOletDual : &TM_IID_IOletCustom;
    if (argc > 4) { WCHAR w[64]; static GUID gi; MultiByteToWideChar(CP_ACP, 0, argv[4], -1, w, 64); if (U(CLSIDFromString)(w, &gi) == S_OK) iid = &gi; }
    hr = U(CoInitializeEx)(NULL, apt[0] == 'S' ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    if (FAILED(hr)) return 90;
    h = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 91;
    n = GetFileSize(h, NULL);
    hg = GlobalAlloc(GMEM_MOVEABLE, n); mem = GlobalLock(hg);
    ReadFile(h, mem, n, &got, NULL); GlobalUnlock(hg); CloseHandle(h);
    if (got != n) return 92;
    hr = U(CreateStreamOnHGlobal)(hg, TRUE, &stm);
    if (FAILED(hr)) return 93;
    hr = U(CoUnmarshalInterface)(stm, iid, &itf);
    IStream_Release(stm);
    if (!strcmp(scen, "noreg")) {
        printf("  CHILD unmarshal without registration: hr=0x%lx\n", (long)hr);
        if (itf) IUnknown_Release((IUnknown *)itf);
        U(CoUninitialize)();
        return FAILED(hr) ? 0 : 1;
    }
    if (FAILED(hr) || !itf) { printf("  CHILD unmarshal failed hr=0x%lx\n", (long)hr); U(CoUninitialize)(); return 94; }
    if (!strcmp(scen, "basic")) scen_basic(itf);
    else if (!strcmp(scen, "callback")) scen_callback(itf);
    else if (!strcmp(scen, "errinfo")) scen_errinfo(itf);
    else if (!strcmp(scen, "dual")) scen_dual(itf);
    else if (!strcmp(scen, "hold")) scen_hold(itf);
    IUnknown_Release((IUnknown *)itf);
    U(CoUninitialize)();
    return g_cfails > 80 ? 80 : g_cfails;
}

static DWORD xp_run(const char *scen, const char *apt, tm_new_fn mk, const GUID *iid, int unregister_before_child, int check_release);
static const GUID *g_child_iid, *g_mini_iid;
static IUnknown *mk_mini_g(void) { return (IUnknown *)tm_new_mini(g_mini_iid); }
/* a fresh process unmarshals a marshaled IOletMini for iid: returns 0 when its unmarshal failed, 1 when it worked */
DWORD tm_xp_fresh_unmarshal(const GUID *iid)
{
    DWORD c; g_mini_iid = iid; g_child_iid = iid;
    c = xp_run("noreg", "M", mk_mini_g, iid, 0, 0);
    g_child_iid = NULL;
    return c;
}
/* returns the child's exit code, or -1 when the parent side failed */
static DWORD xp_run(const char *scen, const char *apt, tm_new_fn mk, const GUID *iid, int unregister_before_child, int check_release)
{
    HRESULT hr; IUnknown *obj; IStream *stm = NULL; HGLOBAL hg = NULL; void *mem; SIZE_T sz; char file[MAX_PATH], extra[MAX_PATH + 64]; HANDLE f; DWORD wr, code = (DWORD)-1;
    hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { CHECKF(0, "CoInitializeEx(MTA) 0x%lx", (long)hr); return code; }
    obj = mk();
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &stm), S_OK);
    hr = U(CoMarshalInterface)(stm, iid, obj, MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    EQ(hr, S_OK);
    if (SUCCEEDED(hr)) {
        U(GetHGlobalFromStream)(stm, &hg);
        sz = GlobalSize(hg); mem = GlobalLock(hg);
        t_tmpfile(file, sizeof file, "xp.stm");
        f = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) { WriteFile(f, mem, (DWORD)sz, &wr, NULL); CloseHandle(f); }
        GlobalUnlock(hg);
        if (unregister_before_child) tm_end();
        snprintf(extra, sizeof extra, "%s %s \"%s\"", scen, apt, file);
        if (g_child_iid) { WCHAR w[40]; size_t l = strlen(extra); tm_guid_str(g_child_iid, w); snprintf(extra + l, sizeof extra - l, " %ls", w); }
        code = t_run_child("tm_xp", extra, 30000);
        DeleteFileA(file);
    }
    IStream_Release(stm);
    if (check_release) {
        int i;
        IUnknown_Release(obj);
        for (i = 0; i < 150 && tm_live_objects() != 0; i++) Sleep(20);
        CHECKF(tm_live_objects() == 0, "server objects alive after the child released everything: %ld", (long)tm_live_objects());
    } else IUnknown_Release(obj);
    U(CoUninitialize)();
    return code;
}

static void run_scen(const char *scen, tm_new_fn mk, const GUID *iid)
{
    static const char *apts[2] = { "M", "S" }; int i;
    for (i = 0; i < 2; i++) {
        DWORD code = xp_run(scen, apts[i], mk, iid, 0, 1);
        CHECKF(code == 0, "child scenario %s (apartment %s) exit code %lu (0xDEAD0001 = timeout)", scen, apts[i], (unsigned long)code);
    }
}

TMTEST(dxtm_xproc_basic_calls) { run_scen("basic", tm_mk_custom, &TM_IID_IOletCustom); }
TMTEST(dxtm_xproc_callback_into_client) { run_scen("callback", tm_mk_custom, &TM_IID_IOletCustom); }
TMTEST(dxtm_xproc_error_info) { run_scen("errinfo", tm_mk_custom, &TM_IID_IOletCustom); }
TMTEST(dxtm_xproc_dual_dispatch_and_enum) { run_scen("dual", tm_mk_dual, &TM_IID_IOletDual); }
TMTEST(dxtm_xproc_reference_counting) { run_scen("hold", tm_mk_custom, &TM_IID_IOletCustom); }

TMTEST(dxtm_xproc_unmarshal_fails_without_registration)
{
    DWORD code = xp_run("noreg", "M", tm_mk_custom, &TM_IID_IOletCustom, 1, 0);
    CHECKF(code == 0, "child exit code %lu: unmarshaling an oleautomation interface must fail once its typelib registration is gone", (unsigned long)code);
    tm_leakcheck();
}
