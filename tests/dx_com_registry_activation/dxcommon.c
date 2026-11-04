#define INITGUID
#include "dxcommon.h"
#include <oleauto.h>
#include <shlwapi.h>

/* ------------------------------------------------------------------ registry helpers */
static REGSAM g_view;
/* Windows: HKCU\Software\Classes is merged into HKCR by the COM runtime. Wine's combase only reads HKLM\Software\Classes,
 * so for emulator validation (throw-away prefix) the same keys go to HKLM. The tests are written for HKCU. */
static HKEY g_root = HKEY_CURRENT_USER;
#define HKCU_ROOT g_root
static int g_ready = -1;
static char g_tracked[512][160];
static int g_ntracked;
static LONG g_serial;

void dx_view(REGSAM wow) { g_view = wow; }
HKEY dx_root(void) { return g_root; }
static void full(const char *sub, char *out, size_t cap) { snprintf(out, cap, "Software\\Classes\\%s", sub); }
static HKEY open_for_write(const char *sub) {
    char p[512]; HKEY k = NULL; full(sub, p, sizeof p);
    if (RegCreateKeyExA(HKCU_ROOT, p, 0, NULL, 0, KEY_ALL_ACCESS | g_view, NULL, &k, NULL)) return NULL;
    return k;
}
LONG dx_mkkey(const char *sub) { HKEY k = open_for_write(sub); if (!k) return ERROR_ACCESS_DENIED; RegCloseKey(k); return 0; }
LONG dx_set(const char *sub, const char *name, const char *val) {
    HKEY k = open_for_write(sub); LONG r; if (!k) return ERROR_ACCESS_DENIED;
    r = RegSetValueExA(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)strlen(val) + 1); RegCloseKey(k); return r;
}
LONG dx_setx(const char *sub, const char *name, const char *val) {
    HKEY k = open_for_write(sub); LONG r; if (!k) return ERROR_ACCESS_DENIED;
    r = RegSetValueExA(k, name, 0, REG_EXPAND_SZ, (const BYTE *)val, (DWORD)strlen(val) + 1); RegCloseKey(k); return r;
}
LONG dx_setdw(const char *sub, const char *name, DWORD v) {
    HKEY k = open_for_write(sub); LONG r; if (!k) return ERROR_ACCESS_DENIED;
    r = RegSetValueExA(k, name, 0, REG_DWORD, (const BYTE *)&v, 4); RegCloseKey(k); return r;
}
LONG dx_setw(const wchar_t *sub, const wchar_t *name, const wchar_t *val) {
    wchar_t p[512]; HKEY k = NULL; LONG r;
    snwprintf(p, 512, L"Software\\Classes\\%ls", sub);
    if (RegCreateKeyExW(HKCU_ROOT, p, 0, NULL, 0, KEY_ALL_ACCESS | g_view, NULL, &k, NULL)) return ERROR_ACCESS_DENIED;
    r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)(wcslen(val) + 1) * 2); RegCloseKey(k); return r;
}
int dx_get(const char *sub, const char *name, char *out, size_t cap) {
    char p[512]; HKEY k; DWORD n = (DWORD)cap, type; LONG r; full(sub, p, sizeof p);
    if (RegOpenKeyExA(HKCU_ROOT, p, 0, KEY_READ | g_view, &k)) return 1;
    r = RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &n); RegCloseKey(k);
    if (r) return 2;
    if (n < cap) out[n] = 0; else out[cap - 1] = 0;
    return 0;
}
int dx_exists(const char *sub) {
    char p[512]; HKEY k; full(sub, p, sizeof p);
    if (RegOpenKeyExA(HKCU_ROOT, p, 0, KEY_READ | g_view, &k)) return 0;
    RegCloseKey(k); return 1;
}
/* An elevated caller's writes through HKCR (CoTreatAsClass, OleSetAutoConvert) land in HKLM\\Software\\Classes, so a tracked olet
 * key is removed from the machine hive as well (best effort; only keys carrying the olet marker). */
void dx_del(const char *sub) {
    char p[512]; full(sub, p, sizeof p); RegDeleteTreeA(HKCU_ROOT, p);
    if (HKCU_ROOT == HKEY_CURRENT_USER && (strstr(sub, "7E57C0DE") || !strncmp(sub, "olet", 4))) RegDeleteTreeA(HKEY_LOCAL_MACHINE, p);
}
/* merged HKCR view: what the COM runtime reads and writes through */
int dx_get_merged(const char *sub, const char *name, char *out, size_t cap) {
    HKEY k; DWORD n = (DWORD)cap, type; LONG r;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, sub, 0, KEY_READ | g_view, &k)) return 1;
    r = RegQueryValueExA(k, name, NULL, &type, (BYTE *)out, &n); RegCloseKey(k);
    if (r) return 2;
    if (n < cap) out[n] = 0; else out[cap - 1] = 0;
    return 0;
}
int dx_exists_merged(const char *sub) {
    HKEY k; if (RegOpenKeyExA(HKEY_CLASSES_ROOT, sub, 0, KEY_READ | g_view, &k)) return 0;
    RegCloseKey(k); return 1;
}
void dx_track(const char *sub) {
    for (int i = 0; i < g_ntracked; i++) if (!strcmp(g_tracked[i], sub)) return;
    if (g_ntracked < 512) snprintf(g_tracked[g_ntracked++], 160, "%s", sub);
}
void dx_cleanup(void) {
    REGSAM saved = g_view;
    for (int v = 0; v < 3; v++) {
        g_view = v == 0 ? 0 : v == 1 ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
        for (int i = 0; i < g_ntracked; i++) dx_del(g_tracked[i]);
    }
    g_ntracked = 0; g_view = saved;
}

/* ------------------------------------------------------------------ guids */
void dx_newguid(GUID *g, WORD kind) {
    LONG n = InterlockedIncrement(&g_serial); DWORD pid = GetCurrentProcessId();
    g->Data1 = 0x7E57C0DE; g->Data2 = kind; g->Data3 = (WORD)n;
    memcpy(g->Data4, "olet", 4); memcpy(g->Data4 + 4, &pid, 4);
}
void dx_guidstr(const GUID *g, char *o) {
    snprintf(o, 40, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1],
             g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}
void dx_guidwstr(const GUID *g, wchar_t *o) { char a[40]; dx_guidstr(g, a); for (int i = 0; i < 40; i++) { o[i] = (unsigned char)a[i]; if (!a[i]) break; } }
int dx_parse_guid(const char *s, GUID *g) {
    unsigned a, b, c, d[8]; if (sscanf(s, "{%8x-%4x-%4x-%2x%2x-%2x%2x%2x%2x%2x%2x}", &a, &b, &c, d, d + 1, d + 2, d + 3, d + 4, d + 5, d + 6, d + 7) != 11) return 0;
    g->Data1 = a; g->Data2 = b; g->Data3 = c; for (int i = 0; i < 8; i++) g->Data4[i] = (BYTE)d[i]; return 1;
}
void dx_clsidkey(const GUID *g, char *out, size_t cap) { char s[40]; dx_guidstr(g, s); snprintf(out, cap, "CLSID\\%s", s); }
void dx_progid(char *out, size_t cap, const char *leaf) { snprintf(out, cap, "olet%lu.%s", GetCurrentProcessId(), leaf); }

/* ------------------------------------------------------------------ sweep */
static int g_sweep_self = 1;
static int pid_alive(DWORD pid) {
    HANDLE h; DWORD ec = 0; int alive;
    if (pid == GetCurrentProcessId()) return 1;
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;
    alive = GetExitCodeProcess(h, &ec) && ec == STILL_ACTIVE; CloseHandle(h); return alive;
}
static void sweep_guid_branch(const char *branch) {
    char p[256]; HKEY k; DWORD i; char name[128]; char kill[256][128]; int nk = 0;
    snprintf(p, sizeof p, "Software\\Classes\\%s", branch);
    if (RegOpenKeyExA(HKCU_ROOT, p, 0, KEY_READ | g_view, &k)) return;
    for (i = 0; ; i++) {
        DWORD n = sizeof name; GUID g;
        if (RegEnumKeyExA(k, i, name, &n, NULL, NULL, NULL, NULL)) break;
        if (!dx_parse_guid(name, &g) || g.Data1 != 0x7E57C0DE || memcmp(g.Data4, "olet", 4)) continue;
        DWORD pid; memcpy(&pid, g.Data4 + 4, 4);
        if ((g_sweep_self && pid == GetCurrentProcessId()) || !pid_alive(pid)) { if (nk < 256) strcpy(kill[nk++], name); }
    }
    RegCloseKey(k);
    for (int j = 0; j < nk; j++) { char s[300]; snprintf(s, sizeof s, "%s\\%s", branch, kill[j]); dx_del(s); }
}
static void sweep_progids(void) {
    HKEY k; DWORD i; char name[256]; char kill[256][256]; int nk = 0;
    if (RegOpenKeyExA(HKCU_ROOT, "Software\\Classes", 0, KEY_READ, &k)) return;
    for (i = 0; ; i++) {
        DWORD n = sizeof name;
        if (RegEnumKeyExA(k, i, name, &n, NULL, NULL, NULL, NULL)) break;
        if (strncmp(name, "olet", 4) || name[4] < '0' || name[4] > '9') continue;
        DWORD pid = (DWORD)strtoul(name + 4, NULL, 10);
        if ((g_sweep_self && pid == GetCurrentProcessId()) || !pid_alive(pid)) { if (nk < 256) strcpy(kill[nk++], name); }
    }
    RegCloseKey(k);
    for (int j = 0; j < nk; j++) dx_del(kill[j]);
}
void dx_sweep_stale(void);
static void sweep_all(void) {
    REGSAM saved = g_view;
    for (int v = 0; v < 3; v++) {
        g_view = v == 0 ? 0 : v == 1 ? KEY_WOW64_32KEY : KEY_WOW64_64KEY;
        sweep_guid_branch("CLSID"); sweep_guid_branch("Interface");
    }
    g_view = saved; sweep_progids();
}
void dx_sweep_stale(void) { g_sweep_self = 0; sweep_all(); g_sweep_self = 1; }
int dx_ready(void) {
    if (g_ready >= 0) return g_ready;
    HKEY k = NULL; char probe[96];
    g_ready = 0; if (t_is_wine()) g_root = HKEY_LOCAL_MACHINE;
    snprintf(probe, sizeof probe, "olet%lu.probe", GetCurrentProcessId());
    k = open_for_write(probe);
    if (k) {
        RegCloseKey(k); dx_del(probe); g_ready = 1; sweep_all(); atexit(dx_cleanup); atexit(sweep_all);
    }
    return g_ready;
}

/* ------------------------------------------------------------------ registration builders */
const char *dx_server_path(void) { return t_self_path(); }
int dx_selfload_ok(void) {
    static int ok = -1;
    if (ok < 0) {
        HMODULE h = LoadLibraryA(t_self_path());
        ok = h && h == GetModuleHandleA(NULL) && GetProcAddress(h, "DllGetClassObject") != NULL;
        if (h) FreeLibrary(h);
    }
    return ok;
}
void dx_reg_inproc(GUID *clsid, const char *model, char *key, size_t cap) {
    char k[128], sub[160], nm[40]; dx_newguid(clsid, DXK_NORMAL);
    dx_clsidkey(clsid, k, sizeof k); dx_track(k);
    snprintf(nm, sizeof nm, "olet_%u", clsid->Data3);
    dx_set(k, NULL, nm);
    snprintf(sub, sizeof sub, "%s\\InprocServer32", k);
    dx_set(sub, NULL, dx_server_path());
    if (model) dx_set(sub, "ThreadingModel", model);
    if (key) snprintf(key, cap, "%s", k);
}
static void qname(const GUID *c, char *o, size_t n) { snprintf(o, n, "Local\\olet_%lu_q_%u", GetCurrentProcessId(), c->Data3); }
void dx_reg_local(GUID *clsid, const char *apt, const char *regcls, char *key, size_t cap) {
    char k[128], sub[160], nm[40], cmd[1024], gs[40]; dx_newguid(clsid, DXK_NORMAL);
    dx_clsidkey(clsid, k, sizeof k); dx_track(k); dx_guidstr(clsid, gs);
    snprintf(nm, sizeof nm, "olet_%u", clsid->Data3); dx_set(k, NULL, nm);
    snprintf(sub, sizeof sub, "%s\\LocalServer32", k);
    snprintf(cmd, sizeof cmd, "\"%s\" --child dxlsrv %lu %s %s %s", dx_server_path(), GetCurrentProcessId(), apt, regcls, gs);
    dx_set(sub, NULL, cmd);
    if (key) snprintf(key, cap, "%s", k);
}
static HANDLE g_qev[64]; static unsigned g_qid[64]; static int g_nq;
void dx_local_prepare(const GUID *c) {
    char n[100]; qname(c, n, sizeof n);
    for (int i = 0; i < g_nq; i++) if (g_qid[i] == c->Data3 && g_qev[i]) { ResetEvent(g_qev[i]); return; }
    if (g_nq < 64) { g_qev[g_nq] = CreateEventA(NULL, TRUE, FALSE, n); g_qid[g_nq++] = c->Data3; }
}
void dx_local_stop(const GUID *c, DWORD pid) {
    HANDLE h = pid ? OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid) : NULL;
    for (int i = 0; i < g_nq; i++) if (g_qid[i] == c->Data3 && g_qev[i]) SetEvent(g_qev[i]);
    if (h) { if (WaitForSingleObject(h, 8000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 3000); } CloseHandle(h); }
}

/* ------------------------------------------------------------------ the server: instance + class factory */
volatile LONG dx_live, dx_nclassobj, dx_ncreate, dx_nlock;
volatile DWORD dx_ci_tid;
GUID dx_ci_iid, dx_gco_clsid;
IUnknown *volatile dx_ci_outer;
static volatile LONG g_embedding;
void dx_reset_counters(void) { dx_live = dx_nclassobj = dx_ncreate = dx_nlock = 0; dx_ci_tid = 0; dx_ci_outer = NULL; memset(&dx_ci_iid, 0, sizeof dx_ci_iid); memset(&dx_gco_clsid, 0, sizeof dx_gco_clsid); }

typedef struct DxObj {
    IPersist iface;           /* delegating IPersist */
    IUnknown inner;           /* non-delegating unknown (aggregation) */
    LONG ref; GUID id; IUnknown *outer; int local;
} DxObj;
static IPersistVtbl g_pvtbl;
static const IUnknownVtbl g_ivtbl;
static DxObj *from_iface(IPersist *p) { return (DxObj *)p; }
static DxObj *from_inner(IUnknown *p) { return (DxObj *)((char *)p - offsetof(DxObj, inner)); }

static HRESULT inner_qi(IUnknown *u, REFIID riid, void **ppv) {
    DxObj *o = from_inner(u); *ppv = NULL;
    if (IsEqualGUID(riid, &IID_IUnknown)) *ppv = &o->inner;
    else if (IsEqualGUID(riid, &IID_IPersist)) *ppv = &o->iface;
    else return E_NOINTERFACE;
    InterlockedIncrement(&o->ref); return S_OK;
}
static ULONG inner_addref(IUnknown *u) { return InterlockedIncrement(&from_inner(u)->ref); }
static ULONG inner_release(IUnknown *u) {
    DxObj *o = from_inner(u); LONG r = InterlockedDecrement(&o->ref);
    if (!r) { InterlockedDecrement(&dx_live); HeapFree(GetProcessHeap(), 0, o); }
    return r;
}
static const IUnknownVtbl g_ivtbl = { inner_qi, inner_addref, inner_release };
static HRESULT p_qi(IPersist *p, REFIID riid, void **ppv) { DxObj *o = from_iface(p); return o->outer ? o->outer->lpVtbl->QueryInterface(o->outer, riid, ppv) : inner_qi(&o->inner, riid, ppv); }
static ULONG p_addref(IPersist *p) { DxObj *o = from_iface(p); return o->outer ? o->outer->lpVtbl->AddRef(o->outer) : inner_addref(&o->inner); }
static ULONG p_release(IPersist *p) { DxObj *o = from_iface(p); return o->outer ? o->outer->lpVtbl->Release(o->outer) : inner_release(&o->inner); }
static HRESULT p_getclassid(IPersist *p, CLSID *id) { if (!id) return E_POINTER; *id = from_iface(p)->id; return S_OK; }
static IPersistVtbl g_pvtbl = { p_qi, p_addref, p_release, p_getclassid };

int dx_is_direct(IUnknown *p) { return p && *(void **)p == (void *)&g_pvtbl; }

typedef struct { IClassFactory f; LONG ref; GUID clsid; int local; } DxFact;
static HRESULT f_qi(IClassFactory *f, REFIID riid, void **ppv) {
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IClassFactory)) { *ppv = f; InterlockedIncrement(&((DxFact *)f)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG f_addref(IClassFactory *f) { return InterlockedIncrement(&((DxFact *)f)->ref); }
static ULONG f_release(IClassFactory *f) { DxFact *d = (DxFact *)f; LONG r = InterlockedDecrement(&d->ref); if (!r) HeapFree(GetProcessHeap(), 0, d); return r; }
static HRESULT f_create(IClassFactory *f, IUnknown *outer, REFIID riid, void **ppv) {
    DxFact *d = (DxFact *)f; DxObj *o; APTTYPE at = APTTYPE_CURRENT; APTTYPEQUALIFIER aq = APTTYPEQUALIFIER_NONE; DWORD pid = GetCurrentProcessId();
    HRESULT hr;
    *ppv = NULL;
    InterlockedIncrement(&dx_ncreate); dx_ci_tid = GetCurrentThreadId(); dx_ci_iid = *riid; dx_ci_outer = outer;
    if (d->clsid.Data2 == DXK_NOCREATE) return E_OUTOFMEMORY;
    if (outer && (d->clsid.Data2 != DXK_AGG || !IsEqualGUID(riid, &IID_IUnknown))) return CLASS_E_NOAGGREGATION;
    if (UIN("ole32.dll", CoGetApartmentType)) UIN("ole32.dll", CoGetApartmentType)(&at, &aq);
    o = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *o); if (!o) return E_OUTOFMEMORY;
    o->iface.lpVtbl = &g_pvtbl; o->inner.lpVtbl = (IUnknownVtbl *)&g_ivtbl; o->ref = 1; o->outer = outer; o->local = d->local;
    o->id.Data1 = GetCurrentThreadId(); o->id.Data2 = (WORD)(((int)at & 0xff) | (((int)aq & 0xff) << 8)); o->id.Data3 = d->clsid.Data3;
    o->id.Data4[0] = (BYTE)((g_embedding ? DXF_EMBEDDING : 0) | (d->local ? DXF_LOCAL : 0));
    memcpy(o->id.Data4 + 1, &pid, 4); o->id.Data4[7] = 'D';
    InterlockedIncrement(&dx_live);
    hr = inner_qi(&o->inner, riid, ppv);   /* ref 2 on success */
    inner_release(&o->inner);               /* drop creation ref */
    return hr;
}
static HRESULT f_lock(IClassFactory *f, BOOL l) { if (l) InterlockedIncrement(&dx_nlock); else InterlockedDecrement(&dx_nlock); return S_OK; }
static IClassFactoryVtbl g_fvtbl = { f_qi, f_addref, f_release, f_create, f_lock };
static DxFact *new_factory(const GUID *c, int local) {
    DxFact *d = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof *d); if (!d) return NULL;
    d->f.lpVtbl = &g_fvtbl; d->ref = 1; d->clsid = *c; d->local = local; return d;
}

__declspec(dllexport) HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv) {
    DxFact *d; HRESULT hr;
    InterlockedIncrement(&dx_nclassobj); dx_gco_clsid = *rclsid;
    if (!ppv) return E_POINTER;
    *ppv = NULL;
    if (rclsid->Data2 == DXK_NOCLASS) return CLASS_E_CLASSNOTAVAILABLE;
    d = new_factory(rclsid, 0); if (!d) return E_OUTOFMEMORY;
    hr = f_qi(&d->f, riid, ppv); f_release(&d->f); return hr;
}
__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }

int dx_getinfo(IUnknown *p, dx_info *o) {
    IPersist *ps = NULL; GUID id; HRESULT hr;
    memset(o, 0, sizeof *o);
    hr = p->lpVtbl->QueryInterface(p, &IID_IPersist, (void **)&ps);
    if (FAILED(hr)) return hr;
    hr = ps->lpVtbl->GetClassID(ps, &id);
    ps->lpVtbl->Release(ps);
    if (FAILED(hr)) return hr;
    o->tid = id.Data1; o->apt = (signed char)(id.Data2 & 0xff); o->qual = id.Data2 >> 8; o->idx = id.Data3; o->flags = id.Data4[0];
    memcpy(&o->pid, id.Data4 + 1, 4); o->marker_ok = id.Data4[7] == 'D';
    return S_OK;
}

HRESULT dx_coinit(DWORD m) { return U(CoInitializeEx)(NULL, m); }
void dx_couninit(void) { U(CoUninitialize)(); }

/* ------------------------------------------------------------------ child plumbing */
void dx_log(int argc, char **argv, const char *fmt, ...) {
    va_list a; char line[512]; DWORD w; HANDLE h;
    for (int i = 0; i < argc; i++) if (!strncmp(argv[i], "res=", 4)) {
        h = CreateFileA(argv[i] + 4, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) return;
        va_start(a, fmt); vsnprintf(line, sizeof line, fmt, a); va_end(a);
        WriteFile(h, line, (DWORD)strlen(line), &w, NULL); WriteFile(h, "\n", 1, &w, NULL); CloseHandle(h); return;
    }
}
DWORD dx_child(const char *name, const char *extra, DWORD timeout_ms, char *log, size_t cap) {
    static LONG n; char res[MAX_PATH], leaf[64], args[1024]; DWORD code, r = 0; HANDLE h;
    snprintf(leaf, sizeof leaf, "res%ld.txt", InterlockedIncrement(&n)); t_tmpfile(res, sizeof res, leaf); DeleteFileA(res);
    snprintf(args, sizeof args, "%s res=%s", extra ? extra : "", res);
    code = t_run_child(name, args, timeout_ms);
    if (log && cap) {
        log[0] = 0; h = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { ReadFile(h, log, (DWORD)cap - 1, &r, NULL); log[r] = 0; CloseHandle(h); }
    }
    return code;
}

/* ------------------------------------------------------------------ child: local server */
static void pump(DWORD ms) {
    MSG m; DWORD end = GetTickCount() + ms;
    while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); if (GetTickCount() > end) break; }
}
CHILD(dxlsrv) {
    /* argv: dxlsrv <parentpid> <sta|mta> <multi|single> <clsid> [-Embedding] [res=..] */
    GUID clsid; DWORD cookie = 0, regcls; HRESULT hr; HANDLE ev; char name[100]; DxFact *f; DWORD end = GetTickCount() + 25000;
    if (argc < 5 || !dx_parse_guid(argv[4], &clsid)) return 90;
    for (int i = 5; i < argc; i++) if (!_stricmp(argv[i], "-Embedding") || !_stricmp(argv[i], "/Embedding")) g_embedding = 1;
    if (!U(CoInitializeEx) || !U(CoRegisterClassObject)) return 91;
    hr = U(CoInitializeEx)(NULL, !strcmp(argv[2], "mta") ? COINIT_MULTITHREADED : COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return 92;
    snprintf(name, sizeof name, "Local\\olet_%s_q_%u", argv[1], clsid.Data3);
    ev = CreateEventA(NULL, TRUE, FALSE, name);
    f = new_factory(&clsid, 1); if (!f) return 93;
    regcls = !strcmp(argv[3], "single") ? REGCLS_SINGLEUSE : REGCLS_MULTIPLEUSE;
    hr = U(CoRegisterClassObject)(&clsid, (IUnknown *)&f->f, CLSCTX_LOCAL_SERVER, regcls, &cookie);
    if (FAILED(hr)) { dx_log(argc, argv, "CoRegisterClassObject failed %08lx", hr); return 94; }
    while (GetTickCount() < end) {
        DWORD w = MsgWaitForMultipleObjects(1, &ev, FALSE, 200, QS_ALLINPUT);
        if (w == WAIT_OBJECT_0) break;
        pump(50);
    }
    U(CoRevokeClassObject)(cookie);
    f_release(&f->f);
    pump(50);
    U(CoUninitialize)();
    return 0;
}

CHILD(dxexit) { return 0; }   /* a "server" that exits without registering its class */
