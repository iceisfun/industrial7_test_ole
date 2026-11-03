/* Shared helpers for the variant_format_parse area (oleaut32 text formatting / parsing).
 * Everything is reached through GetProcAddress (U()/UN()). Expectations are en-US (LCID 0x409) unless a test says otherwise.
 * VarFormat() always uses GetUserDefaultLCID() (decomp-verified: it is VarTokenizeFormatString + VarFormatFromTokens with the
 * user default LCID and a 300 byte token buffer), so table rows are driven through the LCID-taking pair with 0x409 (hard
 * check) and additionally through VarFormat() when the user locale looks like en-US (hard check, else skipped). */
#ifndef VFP_H
#define VFP_H
#include "t.h"
#include <oleauto.h>
#include <wchar.h>
#include <stdlib.h>
#include <math.h>

/* VariantInit is a trivial memset; do not link oleaut32 */
#define VariantInit(p) memset((p), 0, sizeof(VARIANT))
#define VFP_EN_US  MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)
#define VFP_DE_DE  MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN), SORT_DEFAULT)
#define VFP_ES_ES  MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_SPANISH), SORT_DEFAULT)
#define VFP_NOUSER LOCALE_NOUSEROVERRIDE
#define VFP_E_TYPEMISMATCH ((HRESULT)0x80020005)
#define VFP_E_OVERFLOW     ((HRESULT)0x8002000A)
#define VFP_E_BUFTOOSMALL  ((HRESULT)0x80020013)
#define VFP_E_INVALIDARG   ((HRESULT)0x80070057)

/* ---- small utilities ---------------------------------------------------- */
/* narrow ASCII -> wide in a rotating static buffer (valid for the next few calls) */
static inline const WCHAR *vfp_w(const char *s)
{
    static WCHAR ring[8][512];
    static unsigned n;
    WCHAR *d = ring[n++ & 7];
    int i = 0;
    if (!s) return NULL;
    while (s[i] && i < 511) { d[i] = (unsigned char)s[i]; i++; }
    d[i] = 0;
    return d;
}
/* wide -> narrow (non-ASCII chars become '?'); NULL -> "<null>" */
static inline const char *vfp_n(const WCHAR *w)
{
    static char ring[8][512];
    static unsigned n;
    char *d = ring[n++ & 7];
    int i = 0;
    if (!w) return "<null>";
    while (w[i] && i < 511) { d[i] = w[i] < 128 ? (char)w[i] : '?'; i++; }
    d[i] = 0;
    return d;
}

/* BSTR returned by the DLL: length prefix must agree with the NUL terminated text */
static inline int vfp_bstr_ok(BSTR b)
{
    if (!b) return 0;
    return ((const DWORD *)b)[-1] == (DWORD)lstrlenW(b) * 2;
}

/* The user locale resembles en-US closely enough for VarFormat() rows (LCID-less entry point). */
static inline int vfp_user_is_en_us(void)
{
    char b[32];
    if (PRIMARYLANGID(LANGIDFROMLCID(GetUserDefaultLCID())) != LANG_ENGLISH) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, b, sizeof b) || strcmp(b, ".")) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, b, sizeof b) || strcmp(b, ",")) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_IDIGITS, b, sizeof b) || strcmp(b, "2")) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_STIMEFORMAT, b, sizeof b) || strcmp(b, "h:mm:ss tt")) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, b, sizeof b) || strcmp(b, "M/d/yyyy")) return 0;
    if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SCURRENCY, b, sizeof b) || strcmp(b, "$")) return 0;
    return 1;
}

/* ---- variant builders --------------------------------------------------- */
static inline VARIANT vfp_r8(double d)   { VARIANT v; VariantInit(&v); V_VT(&v) = VT_R8;   V_R8(&v) = d; return v; }
static inline VARIANT vfp_r4(float d)    { VARIANT v; VariantInit(&v); V_VT(&v) = VT_R4;   V_R4(&v) = d; return v; }
static inline VARIANT vfp_i4(LONG d)     { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I4;   V_I4(&v) = d; return v; }
static inline VARIANT vfp_i2(SHORT d)    { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I2;   V_I2(&v) = d; return v; }
static inline VARIANT vfp_ui1(BYTE d)    { VARIANT v; VariantInit(&v); V_VT(&v) = VT_UI1;  V_UI1(&v) = d; return v; }
static inline VARIANT vfp_i8(LONG64 d)   { VARIANT v; VariantInit(&v); V_VT(&v) = VT_I8;   V_I8(&v) = d; return v; }
static inline VARIANT vfp_bool(int t)    { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BOOL; V_BOOL(&v) = t ? VARIANT_TRUE : VARIANT_FALSE; return v; }
static inline VARIANT vfp_date(double d) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_DATE; V_DATE(&v) = d; return v; }
static inline VARIANT vfp_cy(LONG64 units) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_CY; V_CY(&v).int64 = units; return v; }
static inline VARIANT vfp_null(void)     { VARIANT v; VariantInit(&v); V_VT(&v) = VT_NULL; return v; }
static inline VARIANT vfp_empty(void)    { VARIANT v; VariantInit(&v); return v; }
/* VT_BSTR; caller must VariantClear (through U(VariantClear)) */
static inline VARIANT vfp_bstr(const char *s) { VARIANT v; VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = U(SysAllocString) ? U(SysAllocString)(vfp_w(s)) : NULL; return v; }
static inline void vfp_clear(VARIANT *v) { if (V_VT(v) == VT_BSTR && V_BSTR(v)) { U(SysFreeString)(V_BSTR(v)); V_BSTR(v) = NULL; } V_VT(v) = VT_EMPTY; }

/* ---- the two formatting paths ------------------------------------------ */
#define VFP_TOKBUF 300
/* Pinned path: VarTokenizeFormatString + VarFormatFromTokens with lcid. */
static inline HRESULT vfp_format_tok(VARIANT *v, const char *fmt, int fd, int fw, ULONG flags, LCID lcid, BSTR *out)
{
    BYTE tok[VFP_TOKBUF];
    HRESULT hr;
    memset(tok, 0, sizeof tok);
    hr = U(VarTokenizeFormatString)((LPOLESTR)vfp_w(fmt), tok, sizeof tok, fd, fw, lcid, NULL);
    if (FAILED(hr)) return hr;
    return U(VarFormatFromTokens)(v, (LPOLESTR)vfp_w(fmt), tok, flags, out, lcid);
}

/* Compare one row. exp == NULL means "S_OK and a NULL BSTR". Both paths are checked; VarFormat only when user locale is en-US.
 * soft != 0 makes a mismatch a t_note (observation not verified against the Windows decomp/docs). */
static inline void vfp_row(const char *file, int line, VARIANT *v, const char *fmt, int fd, int fw, const char *exp, int soft)
{
    BSTR out = (BSTR)0x1;
    HRESULT hr;
    int ok;
    const char *got;

    hr = vfp_format_tok(v, fmt, fd, fw, 0, VFP_EN_US, &out);
    got = (hr == S_OK) ? vfp_n(out) : "<fail>";
    ok = (hr == S_OK) && (exp ? (out && vfp_bstr_ok(out) && !strcmp(got, exp)) : out == NULL);
    if (soft) { if (!ok) t_note("%s:%d fmt \"%s\" (vt %d) want \"%s\" got hr=0x%lx \"%s\"", file, line, fmt ? fmt : "<null>", V_VT(v), exp ? exp : "<null>", (unsigned long)hr, got); }
    else t_check(ok, file, line, "VarFormatFromTokens(0x409)", "fmt \"%s\" (vt %d fd %d fw %d) want \"%s\" got hr=0x%lx \"%s\"", fmt ? fmt : "<null>", V_VT(v), fd, fw, exp ? exp : "<null>", (unsigned long)hr, got);
    if (hr == S_OK && out && out != (BSTR)0x1) U(SysFreeString)(out);

    if (!vfp_user_is_en_us()) return;
    out = NULL;
    hr = U(VarFormat)(v, (LPOLESTR)vfp_w(fmt), fd, fw, 0, &out);
    got = (hr == S_OK) ? vfp_n(out) : "<fail>";
    ok = (hr == S_OK) && (exp ? (out && vfp_bstr_ok(out) && !strcmp(got, exp)) : out == NULL);
    if (soft) { if (!ok) t_note("%s:%d VarFormat fmt \"%s\" want \"%s\" got hr=0x%lx \"%s\"", file, line, fmt ? fmt : "<null>", exp ? exp : "<null>", (unsigned long)hr, got); }
    else t_check(ok, file, line, "VarFormat", "fmt \"%s\" (vt %d fd %d fw %d) want \"%s\" got hr=0x%lx \"%s\"", fmt ? fmt : "<null>", V_VT(v), fd, fw, exp ? exp : "<null>", (unsigned long)hr, got);
    if (hr == S_OK && out) U(SysFreeString)(out);
}
#define VFP_ROW(v, fmt, fd, fw, exp)       vfp_row(__FILE__, __LINE__, &(v), fmt, fd, fw, exp, 0)
#define VFP_ROW_SOFT(v, fmt, fd, fw, exp)  vfp_row(__FILE__, __LINE__, &(v), fmt, fd, fw, exp, 1)

/* table-row variants used by table-driven tests: line is the table line of the row */
#define VFP_NEED_FORMAT() do { REQUIRE(U(VarFormat)); REQUIRE(U(VarTokenizeFormatString)); REQUIRE(U(VarFormatFromTokens)); REQUIRE(U(SysFreeString)); REQUIRE(U(SysAllocString)); } while (0)

#endif
