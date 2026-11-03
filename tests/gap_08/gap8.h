/* gap_08: depth tests for thinly covered public exports (combase WinRT, sechost, ole32 helpers).
 * Everything is resolved through LoadLibrary/GetProcAddress (t.h). */
#ifndef GAP8_H
#define GAP8_H
#include "t.h"
#include <objbase.h>
#include <winstring.h>

#define G8_E_FAIL        ((HRESULT)0x80004005)
#define G8_E_INVALIDARG  ((HRESULT)0x80070057)
#define G8_E_UNEXPECTED  ((HRESULT)0x8000FFFF)
#define G8_E_NOINTERFACE ((HRESULT)0x80004002)
#define G8_E_ACCESSDENIED ((HRESULT)0x80070005)
#define G8_E_BOUNDS      ((HRESULT)0x8000000B)
#define G8_E_NOTINIT     ((HRESULT)0x800401F0)

/* single-evaluation equality (EXPECT_EQ evaluates its arguments twice) */
#define G8_EQ(a, b) do { long long ga_ = (long long)(a), gb_ = (long long)(b); \
    t_check(ga_ == gb_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)ga_, (unsigned long long)gb_); } while (0)

/* run fn on a fresh thread (clean COM state), bounded wait */
typedef DWORD (WINAPI *g8_thread_fn)(void *);
DWORD g8_run_thread(g8_thread_fn fn, void *arg, DWORD ms);

/* SHA-1 (for the WinRT parameterized-type IID derivation) */
void g8_sha1(const unsigned char *data, size_t n, unsigned char out[20]);
/* IID of a WinRT type signature (RFC 4122 v5 under the WinRT namespace) */
void g8_signature_iid(const char *sig, GUID *out);
void g8_guid_str(const GUID *g, char *out); /* "{xxxxxxxx-...}" lower case */
/* require an export, and skip under an emulator that aborts in a stub for it */
#define G8_REQ_NW(ptr) do { if (!(ptr)) { t_skip("missing export: " #ptr); return; } \
    if (t_is_wine()) { t_skip("emulator stub aborts: " #ptr); return; } } while (0)
#endif
