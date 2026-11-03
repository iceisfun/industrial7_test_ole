/* dx_gap_01: state-changing tests (credential store, ETW provider security, HKCU class registration, HKCR interface keys). */
#ifndef DX1_H
#define DX1_H
#include "t.h"
#include <wincred.h>
#include <objbase.h>
#include <sddl.h>
#include <wchar.h>

static inline FARPROC dx_cred(const char *n)
{
    if (t_is_wine()) return t_proc_in("advapi32.dll", n);
    return t_proc_in("sechost.dll", n);
}
#define CR(name) ((__typeof__(&name))dx_cred(#name))

#ifndef CRED_ENUMERATE_ALL_CREDENTIALS
#define CRED_ENUMERATE_ALL_CREDENTIALS 0x1
#endif
#define DX_BLOB_MAX 2560

static inline int dx_is_admin(void)
{
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY; PSID sid = NULL; BOOL m = FALSE;
    if (!AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &sid)) return 0;
    if (!CheckTokenMembership(NULL, sid, &m)) m = FALSE;
    FreeSid(sid);
    return m;
}
static inline void dx_wide(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
#endif
