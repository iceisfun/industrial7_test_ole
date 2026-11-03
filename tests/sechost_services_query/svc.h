/* Shared helpers for the sechost_services_query area.
 * Service control manager client API: sechost.dll exports most of it (advapi32 forwards);
 * A-variants and a few W functions live only in advapi32, so S(Name) resolves sechost first,
 * then advapi32.  Nothing is linked: every call is LoadLibrary -> GetProcAddress. */
#ifndef SVC_H
#define SVC_H
#include "t.h"
#include <winsvc.h>

static FARPROC svc_p(const char *n) {
    FARPROC p = t_proc_in("sechost.dll", n);
    if (!p) p = t_proc_in("advapi32.dll", n);
    return p;
}
#define S(name) ((__typeof__(&name))svc_p(#name))

static SC_HANDLE scm_open(DWORD access) {
    __typeof__(&OpenSCManagerW) f = S(OpenSCManagerW);
    return f ? f(NULL, NULL, access) : NULL;
}
static SC_HANDLE svc_open(SC_HANDLE scm, const char *name, DWORD access) {
    WCHAR w[300];
    __typeof__(&OpenServiceW) f = S(OpenServiceW);
    if (!f || MultiByteToWideChar(CP_ACP, 0, name, -1, w, 300) <= 0) return NULL;
    return f(scm, w, access);
}
static void svc_close(SC_HANDLE h) {
    __typeof__(&CloseServiceHandle) f = S(CloseServiceHandle);
    if (h && f) f(h);
}
/* Opens SCM(CONNECT) + service; returns service handle or NULL (sets *scm). */
static SC_HANDLE svc_fixture(const char *name, DWORD access, SC_HANDLE *scm) {
    *scm = scm_open(SC_MANAGER_CONNECT);
    if (!*scm) return NULL;
    SC_HANDLE s = svc_open(*scm, name, access);
    if (!s) { svc_close(*scm); *scm = NULL; }
    return s;
}

/* Services that exist on every Windows 10 x64 install; first four are always running. */
#define PER_SERVICE(base, fn) \
    TEST(base##_RpcSs)        { fn("RpcSs"); } \
    TEST(base##_RpcEptMapper) { fn("RpcEptMapper"); } \
    TEST(base##_DcomLaunch)   { fn("DcomLaunch"); } \
    TEST(base##_EventLog)     { fn("EventLog"); } \
    TEST(base##_Winmgmt)      { fn("Winmgmt"); }

static void wide_to_narrow(char *dst, size_t cap, const WCHAR *w) {
    if (WideCharToMultiByte(CP_ACP, 0, w, -1, dst, (int)cap, NULL, NULL) <= 0) dst[0] = 0;
}
#endif
