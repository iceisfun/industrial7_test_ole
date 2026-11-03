/* gap_01: shared helpers. Everything is resolved through GetProcAddress; nothing here links the DLL under test. */
#ifndef G1_H
#define G1_H
#include "t.h"
#include <wincred.h>
#include <objbase.h>
#include <wchar.h>

/* Cred* live in sechost.dll on Win10 (advapi32 forwards). Emulators may only have them in advapi32. */
static inline FARPROC g1_cred(const char *n)
{
    FARPROC p;
    if (t_is_wine()) return t_proc_in("advapi32.dll", n);   /* wine's sechost exports abort-stubs */
    return t_proc_in("sechost.dll", n);
}
#define CR(name) ((__typeof__(&name))g1_cred(#name))

/* the mingw header still says CRED_TYPE_MAXIMUM == 5; Win10 SDK says 7 */
#define G1_CRED_TYPE_MAXIMUM 7
#define G1_BLOB_MAX 2560

#ifndef CRED_ENUMERATE_ALL_CREDENTIALS
#define CRED_ENUMERATE_ALL_CREDENTIALS 0x1
#endif
#define G1_NO_SUCH_LOGON_SESSION 1312

/* Tests that reach the credential service call this after a failure that should be a logical error:
 * returns nonzero (and notes) when the environment has no usable credential set. */
static inline int g1_no_cred_session(DWORD e)
{
    if (e == ERROR_NO_SUCH_LOGON_SESSION) { t_note("credential set unavailable in this logon session (1312)"); return 1; }
    return 0;
}

static inline void g1_wide(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
#endif
