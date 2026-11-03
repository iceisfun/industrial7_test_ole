/* Shared helpers for area sechost_security_sid_sd.
 * sechost.dll exports (also forwarded from advapi32 on Win10) are pinned with UIN("sechost.dll", X).
 * A-variants and privilege lookups live in advapi32 only: UIN("advapi32.dll", X). */
#ifndef SSH_H
#define SSH_H
#include "t.h"
#include <sddl.h>
#include <aclapi.h>
#include <ntsecapi.h>
#include <wincred.h>
#include <wchar.h>

#define SH "sechost.dll"
#define AD "advapi32.dll"
#define ERR_INVALID_SID 1337
#define ERR_INVALID_ACL 1336

/* Soft error check: the value is logged, not failed (used where the code path is not decomp-verified). */
#define NOTE_ERR(want) do { DWORD e_ = GetLastError(); if (e_ != (DWORD)(want)) t_note("%s:%d GetLastError %lu, expected %lu", __FILE__, __LINE__, e_, (DWORD)(want)); } while (0)

/* Build a SID in caller storage. */
typedef struct { BYTE buf[SECURITY_MAX_SID_SIZE]; } SIDBUF;
static inline PSID mk_sid(SIDBUF *b, unsigned long long auth, int n, ...)
{
    PISID s = (PISID)b->buf;
    va_list ap; int i;
    memset(b, 0, sizeof *b);
    s->Revision = 1; s->SubAuthorityCount = (BYTE)n;
    for (i = 0; i < 6; i++) s->IdentifierAuthority.Value[i] = (BYTE)(auth >> (8 * (5 - i)));
    va_start(ap, n);
    for (i = 0; i < n; i++) s->SubAuthority[i] = va_arg(ap, DWORD);
    va_end(ap);
    return s;
}
static inline int is_english_ui(void) { return (GetUserDefaultUILanguage() & 0xff) == LANG_ENGLISH; }

static inline void narrow(const WCHAR *w, char *out, size_t cap)
{
    size_t i = 0; if (w) for (; w[i] && i + 1 < cap; i++) out[i] = (char)w[i]; out[i] = 0;
}
static inline void widen(const char *a, WCHAR *out, size_t cap)
{
    size_t i = 0; for (; a[i] && i + 1 < cap; i++) out[i] = (unsigned char)a[i]; out[i] = 0;
}

/* Convert string -> SD via sechost (W), returns the SD (LocalAlloc) or NULL. */
static inline PSECURITY_DESCRIPTOR sd_from_str(const char *s, ULONG *size)
{
    WCHAR w[1024]; PSECURITY_DESCRIPTOR sd = NULL; ULONG sz = 0;
    widen(s, w, 1024);
    if (!UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW)(w, SDDL_REVISION_1, &sd, &sz)) return NULL;
    if (size) *size = sz;
    return sd;
}
/* SD -> string via sechost; returns static buffer ("" on failure, with *ok = 0). */
static inline const char *sd_to_str(PSECURITY_DESCRIPTOR sd, SECURITY_INFORMATION si, int *ok)
{
    static char out[8][2048]; static int k; char *dst = out[k++ & 7];
    WCHAR *w = NULL; ULONG len = 0;
    dst[0] = 0;
    if (ok) *ok = 0;
    if (!UIN(SH, ConvertSecurityDescriptorToStringSecurityDescriptorW)(sd, SDDL_REVISION_1, si, &w, &len)) return dst;
    narrow(w, dst, 2048); LocalFree(w);
    if (ok) *ok = 1;
    return dst;
}
#define ALL_SI (OWNER_SECURITY_INFORMATION|GROUP_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION|SACL_SECURITY_INFORMATION)
#endif
