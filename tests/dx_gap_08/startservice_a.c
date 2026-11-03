/* dx_gap_08: STATE-CHANGING. StartServiceA against throw-away services named olet_g8_<pid>_<n>_svc.
 * Needs an elevated token (t_skip otherwise). Services point at a non-existent image so nothing ever runs.
 * Leftovers of earlier crashed runs are deleted on first use; every test deletes what it created. */
#include "t.h"
#include <winsvc.h>
#include <stdlib.h>

#define SH "sechost.dll"
static const WCHAR *BIN = L"C:\\olet_nonexistent_dir\\olet_g8.exe";
static LONG counter;
typedef struct { SC_HANDLE scm, svc; WCHAR name[96]; } FX;

static void sweep(SC_HANDLE scm)
{
    static int done; DWORD cap = 1 << 18, need = 0, cnt = 0, resume = 0; BYTE *buf; ENUM_SERVICE_STATUS_PROCESSW *e; DWORD i;
    if (done) return; done = 1;
    buf = malloc(cap);
    if (buf && UIN(SH, EnumServicesStatusExW)(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL, buf, cap, &need, &cnt, &resume, NULL)) {
        e = (void *)buf;
        for (i = 0; i < cnt; i++) {
            const WCHAR *n = e[i].lpServiceName; size_t l = lstrlenW(n);
            if (l > 9 && !wcsncmp(n, L"olet_g8_", 8) && !wcscmp(n + l - 4, L"_svc")) {
                SC_HANDLE h = UIN(SH, OpenServiceW)(scm, n, DELETE);
                if (h) { UIN(SH, DeleteService)(h); UIN(SH, CloseServiceHandle)(h); }
            }
        }
    }
    free(buf);
}
static int fx_begin(FX *f)
{
    memset(f, 0, sizeof *f);
    if (!UIN(SH, OpenSCManagerW) || !UIN(SH, CreateServiceW) || !UIN(SH, StartServiceA) || !UIN(SH, StartServiceW) || !UIN(SH, DeleteService)) { t_skip("missing service exports"); return 0; }
    if (t_is_wine()) { t_skip("emulated SCM"); return 0; }
    f->scm = UIN(SH, OpenSCManagerW)(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!f->scm) { t_skip("not elevated (err %lu)", GetLastError()); return 0; }
    sweep(f->scm);
    _snwprintf(f->name, 96, L"olet_g8_%lu_%ld_svc", GetCurrentProcessId(), InterlockedIncrement(&counter)); f->name[95] = 0;
    f->svc = UIN(SH, CreateServiceW)(f->scm, f->name, f->name, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
                                     SERVICE_ERROR_NORMAL, BIN, NULL, NULL, NULL, NULL, NULL);
    if (!f->svc) { DWORD e = GetLastError(); UIN(SH, CloseServiceHandle)(f->scm); f->scm = NULL; t_skip("CreateServiceW failed %lu", e); return 0; }
    return 1;
}
static void fx_end(FX *f)
{
    SC_HANDLE h;
    if (!f->scm) return;
    if (f->svc) { UIN(SH, DeleteService)(f->svc); UIN(SH, CloseServiceHandle)(f->svc); f->svc = NULL; }
    h = UIN(SH, OpenServiceW)(f->scm, f->name, DELETE);
    if (h) { UIN(SH, DeleteService)(h); UIN(SH, CloseServiceHandle)(h); }
    UIN(SH, CloseServiceHandle)(f->scm); f->scm = NULL;
}
static DWORD start_a(SC_HANDLE s, DWORD n, LPCSTR *v) { SET_ERR(0); return UIN(SH, StartServiceA)(s, n, v) ? 0 : GetLastError(); }
static DWORD start_w(SC_HANDLE s, DWORD n, LPCWSTR *v) { SET_ERR(0); return UIN(SH, StartServiceW)(s, n, v) ? 0 : GetLastError(); }

TEST(dx_gap8_starta_ansi_args_missing_image)
{
    FX f; LPCSTR argv[3] = { "first", "second argument", "" }; LPCWSTR wargv[3] = { L"first", L"second argument", L"" }; SERVICE_STATUS st;
    if (!fx_begin(&f)) return;
    EXPECT_EQ(start_a(f.svc, 0, NULL), ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(start_a(f.svc, 3, argv), ERROR_FILE_NOT_FOUND);       /* the ANSI vector is converted and passed on; the image is still missing */
    EXPECT_EQ(start_a(f.svc, 1, argv), ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(start_w(f.svc, 3, wargv), ERROR_FILE_NOT_FOUND);      /* parity with the wide variant */
    if (UIN(SH, QueryServiceStatus)(f.svc, &st)) EXPECT_EQ(st.dwCurrentState, SERVICE_STOPPED);
    fx_end(&f);
}

TEST(dx_gap8_starta_error_matrix_matches_w)
{
    FX f; SC_HANDLE ro; DWORD ea, ew; BOOL (WINAPI *chg)(SC_HANDLE, DWORD, DWORD, DWORD, LPCWSTR, LPCWSTR, LPDWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR);
    if (!fx_begin(&f)) return;
    chg = (void *)t_proc_in(SH, "ChangeServiceConfigW");
    if (!chg) { t_skip("no ChangeServiceConfigW"); fx_end(&f); return; }
    /* 1. enabled demand-start service, missing image */
    ea = start_a(f.svc, 0, NULL); ew = start_w(f.svc, 0, NULL);
    CHECKF(ea == ew && ea == ERROR_FILE_NOT_FOUND, "enabled: A=%lu W=%lu", ea, ew);
    /* 2. disabled */
    CHECK(chg(f.svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    ea = start_a(f.svc, 0, NULL); ew = start_w(f.svc, 0, NULL);
    CHECKF(ea == ew && ea == ERROR_SERVICE_DISABLED, "disabled: A=%lu W=%lu", ea, ew);
    CHECK(chg(f.svc, SERVICE_NO_CHANGE, SERVICE_DEMAND_START, SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL));
    /* 3. handle without SERVICE_START */
    ro = UIN(SH, OpenServiceW)(f.scm, f.name, SERVICE_QUERY_STATUS);
    CHECK(ro != NULL);
    if (ro) {
        ea = start_a(ro, 0, NULL); ew = start_w(ro, 0, NULL);
        CHECKF(ea == ew && ea == ERROR_ACCESS_DENIED, "no start right: A=%lu W=%lu", ea, ew);
        UIN(SH, CloseServiceHandle)(ro);
    }
    /* 4. the SCM handle is not a service handle; NULL is not a handle at all */
    ea = start_a(f.scm, 0, NULL); ew = start_w(f.scm, 0, NULL);
    CHECKF(ea == ew && ea == ERROR_INVALID_HANDLE, "scm handle: A=%lu W=%lu", ea, ew);
    ea = start_a(NULL, 0, NULL); ew = start_w(NULL, 0, NULL);
    CHECKF(ea == ew && ea == ERROR_INVALID_HANDLE, "NULL handle: A=%lu W=%lu", ea, ew);
    fx_end(&f);
}

TEST(dx_gap8_starta_deleted_service_marked_for_delete)
{
    FX f; DWORD e;
    if (!fx_begin(&f)) return;
    CHECK(UIN(SH, DeleteService)(f.svc));
    e = start_a(f.svc, 0, NULL);                                   /* the handle is still open, the service only marked for deletion */
    CHECKF(e == ERROR_SERVICE_MARKED_FOR_DELETE, "start after DeleteService -> %lu", e);
    e = start_w(f.svc, 0, NULL);
    CHECKF(e == ERROR_SERVICE_MARKED_FOR_DELETE, "StartServiceW after DeleteService -> %lu", e);
    fx_end(&f);
}
