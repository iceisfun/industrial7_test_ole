/* Shared helpers for dx_sechost_service_lifecycle.
 * STATE-CHANGING: creates throw-away services named olet_<pid>_<n>_svc. Needs an elevated token;
 * every test t_skip()s when the SCM cannot be opened with SC_MANAGER_ALL_ACCESS.
 * Existing services are only ever opened read-only (QUERY_*) by these tests. */
#ifndef SVC_H
#define SVC_H
#include "t.h"
#include <winsvc.h>
#include <stdlib.h>
#include <sddl.h>

#ifndef SERVICE_CONFIG_DELAYED_AUTO_START_INFO
#define SERVICE_CONFIG_DELAYED_AUTO_START_INFO 3
#endif
#ifndef SERVICE_CONFIG_FAILURE_ACTIONS_FLAG
#define SERVICE_CONFIG_FAILURE_ACTIONS_FLAG 4
#define SERVICE_CONFIG_SERVICE_SID_INFO 5
#define SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO 6
#define SERVICE_CONFIG_PRESHUTDOWN_INFO 7
#endif

/* sechost exports most SCM entry points on Win10; advapi32 holds the rest (LockServiceDatabase ...). */
static FARPROC svc_proc(const char *n) {
    FARPROC p = t_proc_in("sechost.dll", n);
    if (!p) p = t_proc_in("advapi32.dll", n);
    return p;
}
#define S(name) ((__typeof__(&name))svc_proc(#name))
/* strictly sechost */
#define SH(name) ((__typeof__(&name))t_proc_in("sechost.dll", #name))

typedef struct {
    SC_HANDLE scm, svc;
    WCHAR name[96], disp[128];
    char  nameA[96];
    int   deleted;
} fx_t;

static const WCHAR *FX_BIN = L"C:\\olet_nonexistent_dir\\olet_svc.exe";

static LONG fx_counter;

static SC_HANDLE fx_scm(DWORD acc) { return S(OpenSCManagerW)(NULL, NULL, acc); }

/* Delete every leftover olet_*_svc from earlier (crashed) runs. */
static void fx_sweep(void) {
    static int done;
    if (done) return;
    done = 1;
    SC_HANDLE scm = fx_scm(SC_MANAGER_ALL_ACCESS);
    if (!scm) return;
    DWORD cap = 1 << 18, need = 0, cnt = 0, resume = 0;
    BYTE *buf = malloc(cap);
    if (buf && S(EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, cap, &need, &cnt, &resume, NULL)) {
        ENUM_SERVICE_STATUS_PROCESSW *e = (void *)buf;
        for (DWORD i = 0; i < cnt; i++) {
            const WCHAR *n = e[i].lpServiceName; size_t l = lstrlenW(n);
            if (l > 9 && !wcsncmp(n, L"olet_", 5) && !wcscmp(n + l - 4, L"_svc")) {
                SC_HANDLE h = S(OpenServiceW)(scm, n, DELETE);
                if (h) { S(DeleteService)(h); S(CloseServiceHandle)(h); }
            }
        }
    }
    free(buf);
    S(CloseServiceHandle)(scm);
}

/* Returns 1 when ready. create: also create the default service (demand start, own process). */
static int fx_begin(fx_t *f, int create) {
    memset(f, 0, sizeof *f);
    if (!S(OpenSCManagerW) || !S(CreateServiceW) || !S(OpenServiceW) || !S(DeleteService) || !S(CloseServiceHandle)) {
        t_skip("missing service exports"); return 0;
    }
    f->scm = fx_scm(SC_MANAGER_ALL_ACCESS);
    if (!f->scm) { t_skip("not elevated / SCM not available (err %lu)", GetLastError()); return 0; }
    fx_sweep();
    LONG n = InterlockedIncrement(&fx_counter);
    DWORD pid = GetCurrentProcessId();
    wsprintfW(f->name, L"olet_%lu_%ld_svc", pid, n);
    wsprintfW(f->disp, L"olet display %lu %ld", pid, n);
    wsprintfA(f->nameA, "olet_%lu_%ld_svc", pid, n);
    if (create) {
        f->svc = S(CreateServiceW)(f->scm, f->name, f->disp, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                                   SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, NULL, NULL, NULL);
        if (!f->svc) { DWORD e = GetLastError(); S(CloseServiceHandle)(f->scm); f->scm = NULL; t_skip("CreateServiceW failed %lu", e); return 0; }
    }
    return 1;
}

static void fx_end(fx_t *f) {
    if (!f->scm) return;
    if (f->svc) {
        SERVICE_STATUS ss;
        if (S(ControlService)) S(ControlService)(f->svc, SERVICE_CONTROL_STOP, &ss);
        if (S(DeleteService)) S(DeleteService)(f->svc);
        S(CloseServiceHandle)(f->svc); f->svc = NULL;
    }
    SC_HANDLE h = S(OpenServiceW)(f->scm, f->name, DELETE);
    if (h) { S(DeleteService)(h); S(CloseServiceHandle)(h); }
    S(CloseServiceHandle)(f->scm); f->scm = NULL;
}

/* Create an extra service in the fixture's namespace with a suffix. */
static SC_HANDLE fx_create2(fx_t *f, const WCHAR *suffix, DWORD start, const WCHAR *deps, WCHAR *outname) {
    WCHAR nm[160], dn[160];
    wsprintfW(nm, L"%s_%s_svc", f->name, suffix);  /* ends with _svc: swept too */
    wsprintfW(dn, L"%s %s", f->disp, suffix);
    if (outname) lstrcpyW(outname, nm);
    return S(CreateServiceW)(f->scm, nm, dn, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, start,
                             SERVICE_ERROR_NORMAL, FX_BIN, NULL, NULL, deps, NULL, NULL);
}
static void fx_del(fx_t *f, const WCHAR *nm, SC_HANDLE h) {
    if (h) { S(DeleteService)(h); S(CloseServiceHandle)(h); }
    SC_HANDLE o = S(OpenServiceW)(f->scm, nm, DELETE);
    if (o) { S(DeleteService)(o); S(CloseServiceHandle)(o); }
}

static LONG reg_open_svc(const WCHAR *name, HKEY *k) {
    WCHAR p[260]; wsprintfW(p, L"SYSTEM\\CurrentControlSet\\Services\\%s", name);
    return RegOpenKeyExW(HKEY_LOCAL_MACHINE, p, 0, KEY_READ, k);
}
static int reg_dword(const WCHAR *name, const WCHAR *val, DWORD *out) {
    HKEY k; if (reg_open_svc(name, &k)) return 0;
    DWORD t = 0, sz = 4; LONG r = RegQueryValueExW(k, val, NULL, &t, (BYTE *)out, &sz);
    RegCloseKey(k); return r == 0 && t == REG_DWORD;
}
static int reg_str(const WCHAR *name, const WCHAR *val, WCHAR *out, DWORD cch, DWORD *type) {
    HKEY k; if (reg_open_svc(name, &k)) return 0;
    DWORD t = 0, sz = cch * 2; LONG r = RegQueryValueExW(k, val, NULL, &t, (BYTE *)out, &sz);
    RegCloseKey(k); if (type) *type = t; return r == 0;
}
static int reg_exists(const WCHAR *name) { HKEY k; if (reg_open_svc(name, &k)) return 0; RegCloseKey(k); return 1; }

static BYTE *qcfg(SC_HANDLE s, QUERY_SERVICE_CONFIGW **c) {
    DWORD need = 0; BYTE *b = NULL;
    S(QueryServiceConfigW)(s, NULL, 0, &need);
    if (!need) return NULL;
    b = malloc(need);
    if (!S(QueryServiceConfigW)(s, (void *)b, need, &need)) { free(b); return NULL; }
    *c = (void *)b; return b;
}
static BYTE *qcfg2(SC_HANDLE s, DWORD lvl) {
    DWORD need = 0;
    S(QueryServiceConfig2W)(s, lvl, NULL, 0, &need);
    if (!need) return NULL;
    BYTE *b = calloc(1, need);
    if (!S(QueryServiceConfig2W)(s, lvl, b, need, &need)) { free(b); return NULL; }
    return b;
}
#endif
