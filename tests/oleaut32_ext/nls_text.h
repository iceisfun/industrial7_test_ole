/* oleaut32_ext: LOCALE_USE_NLS text of VarBstrFrom<number> and VariantChangeType(Ex)(VARIANT_USE_NLS). Expectations are the Windows 10 22H2
 * results; en-US rows use LCID 0x409 and are exact with LOCALE_NOUSEROVERRIDE (the stock number-info record of en-US with that flag has
 * 3 fraction digits and the negative order "- n"), and with the user locale only when it is en-US. */
#ifndef NLS_TEXT_H
#define NLS_TEXT_H
#include "t.h"
#include <oleauto.h>
#include <wchar.h>
#define NLS_FLAG   0x10000000UL   /* LOCALE_USE_NLS == VARIANT_USE_NLS for the VarBstrFrom* dwFlags */
#define NLS_NUO    (NLS_FLAG | LOCALE_NOUSEROVERRIDE)
#define NLS_USER_EN() (GetUserDefaultLCID() == 0x409)
static CY nls_cy(long long v) { CY c; c.int64 = v; return c; }
static DECIMAL nls_dec(BYTE scale, BYTE sign, ULONG hi, ULONGLONG lo) { DECIMAL d; memset(&d, 0, sizeof d); d.scale = scale; d.sign = sign; d.Hi32 = hi; d.Lo64 = lo; return d; }
/* run a VarBstrFrom call (the expression assigns b_), check hr and text */
#define NLS_EXPECT(call, want_hr, want_text) do { BSTR b_ = NULL; HRESULT h_ = (call); \
    t_check(h_ == (HRESULT)(want_hr), __FILE__, __LINE__, #call, "hr got 0x%08lx want 0x%08lx", (unsigned long)h_, (unsigned long)(want_hr)); \
    if ((want_text) != NULL) t_check(b_ != NULL && !lstrcmpW(b_, (want_text)), __FILE__, __LINE__, #call, "text got \"%ls\"", b_ ? b_ : L"<null>"); \
    else t_check(b_ == NULL, __FILE__, __LINE__, #call, "BSTR written on failure"); \
    if (b_) U(SysFreeString)(b_); } while (0)
#endif
