/* Shared helpers for the bstr_core area (oleaut32 BSTR API). */
#ifndef BC_H
#define BC_H
#include "t.h"
#include <oleauto.h>
#include <wchar.h>

/* Functions missing from the mingw headers. */
UDECL(void, SysAddRefString, (BSTR));
UDECL(void, SysReleaseString, (BSTR));
UDECL(void, SetOaNoCache, (void));

/* Length prefix as stored by the allocator: DWORD at p[-1], in BYTES, not counting terminators. */
#define BLEN(p)  (((const DWORD *)(p))[-1])

#define VCMP_LT 0
#define VCMP_EQ 1
#define VCMP_GT 2
#define VCMP_NULL 3
#define BC_E_TYPEMISMATCH ((HRESULT)0x80020005)
#define BC_E_OVERFLOW     ((HRESULT)0x8002000A)

/* OLE-allocated BSTR must carry: prefix == 2*n bytes, n chars equal to w, wide NUL at [n]. */
static inline int bc_is_wstr(BSTR b, const WCHAR *w, UINT n)
{
    if (!b) return 0;
    if (BLEN(b) != n * 2) return 0;
    if (n && memcmp(b, w, n * 2)) return 0;
    return b[n] == 0;
}

/* true when b is exactly the NUL-terminated wide string w */
static inline int bc_eq_w(BSTR b, const WCHAR *w) { return b && !lstrcmpW(b, w) && bc_is_wstr(b, w, lstrlenW(w)); }

static inline void bc_fill(unsigned char *p, size_t n, unsigned seed)
{
    size_t i;
    for (i = 0; i < n; i++) { seed = seed * 1103515245u + 12345u; p[i] = (unsigned char)(seed >> 16); }
}

/* free through the module under test */
#define BC_FREE(b) do { if (U(SysFreeString)) U(SysFreeString)(b); } while (0)
#endif
