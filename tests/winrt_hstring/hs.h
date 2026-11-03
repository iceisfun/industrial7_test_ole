/* Shared helpers for the winrt_hstring area (combase WinRT string APIs).
 * Every entry point is resolved through LoadLibrary/GetProcAddress (U/UN), never linked. */
#ifndef HS_H
#define HS_H
#include "t.h"
#include <winstring.h>

#define HS_E_INVALIDARG   ((HRESULT)0x80070057)
#define HS_E_POINTER      ((HRESULT)0x80004003)
#define HS_E_BOUNDS       ((HRESULT)0x8000000B)
#define HS_E_NOTNULLTERM  ((HRESULT)0x80000017)  /* E_STRING_NOT_NULL_TERMINATED */
#define HS_E_LENGTH       ((HRESULT)0x80080011)  /* length too large for an HSTRING */
#define HS_E_OVERFLOW     ((HRESULT)0x80070216)  /* HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) */
#define HS_E_NOMEM        ((HRESULT)0x8007000E)
#define HS_SENTINEL       ((HSTRING)(ULONG_PTR)0x1234)

#define WCS   U(WindowsCreateString)
#define WCSR  U(WindowsCreateStringReference)
#define WDEL  U(WindowsDeleteString)
#define WDUP  U(WindowsDuplicateString)
#define WLEN  U(WindowsGetStringLen)
#define WRAW  U(WindowsGetStringRawBuffer)
#define WEMPTY U(WindowsIsStringEmpty)
#define WEMB  U(WindowsStringHasEmbeddedNull)
#define WCMP  U(WindowsCompareStringOrdinal)
#define WCAT  U(WindowsConcatString)
#define WSUB  U(WindowsSubstring)
#define WSUBL U(WindowsSubstringWithSpecifiedLength)
#define WREP  U(WindowsReplaceString)
#define WTRS  U(WindowsTrimStringStart)
#define WTRE  U(WindowsTrimStringEnd)
#define WPRE  U(WindowsPreallocateStringBuffer)
#define WPRO  U(WindowsPromoteStringBuffer)
#define WDELB U(WindowsDeleteStringBuffer)

/* require the whole basic API set */
#define REQUIRE_HS() do { \
    REQUIRE(WCS); REQUIRE(WCSR); REQUIRE(WDEL); REQUIRE(WDUP); REQUIRE(WLEN); REQUIRE(WRAW); \
    REQUIRE(WEMPTY); REQUIRE(WEMB); } while (0)
#define REQUIRE_HS_OPS() do { REQUIRE_HS(); REQUIRE(WCMP); REQUIRE(WCAT); REQUIRE(WSUB); REQUIRE(WSUBL); \
    REQUIRE(WREP); REQUIRE(WTRS); REQUIRE(WTRE); } while (0)
#define REQUIRE_HS_BUF() do { REQUIRE_HS(); REQUIRE(WPRE); REQUIRE(WPRO); REQUIRE(WDELB); } while (0)

/* HSTRING_User* (not in the SDK headers with C linkage we can rely on) */
UDECL(ULONG, HSTRING_UserSize,   (ULONG *, ULONG, HSTRING *));
UDECL(ULONG, HSTRING_UserSize64, (ULONG *, ULONG, HSTRING *));
UDECL(unsigned char *, HSTRING_UserMarshal,   (ULONG *, unsigned char *, HSTRING *));
UDECL(unsigned char *, HSTRING_UserMarshal64, (ULONG *, unsigned char *, HSTRING *));
UDECL(unsigned char *, HSTRING_UserUnmarshal,   (ULONG *, unsigned char *, HSTRING *));
UDECL(unsigned char *, HSTRING_UserUnmarshal64, (ULONG *, unsigned char *, HSTRING *));
UDECL(void, HSTRING_UserFree,   (ULONG *, HSTRING *));
UDECL(void, HSTRING_UserFree64, (ULONG *, HSTRING *));

/* create a heap string (hard-checks S_OK); NULL when n == 0 */
HSTRING hs_make(const WCHAR *s, UINT32 n);
HSTRING hs_makez(const WCHAR *s);            /* wcslen based */
/* verify an HSTRING completely: len, raw buffer content, NUL terminator, empty flag, embedded-NUL flag */
void hs_expect(HSTRING h, const WCHAR *content, UINT32 len, int has_null, const char *file, int line);
#define HS_EXPECT(h, content, len, nul) hs_expect((h), (content), (len), (nul), __FILE__, __LINE__)
#define HS_EXPECTZ(h, lit) hs_expect((h), (lit), (UINT32)(sizeof(lit) / sizeof(WCHAR) - 1), 0, __FILE__, __LINE__)
#define HS_EXPECT_NULL(h) hs_expect((h), L"", 0, 0, __FILE__, __LINE__)

/* run fn(ctx) and report the exception code it raised (0 = none). Only this thread, only while inside. */
typedef void (*hs_try_fn)(void *ctx);
DWORD hs_try(hs_try_fn fn, void *ctx);

/* USER_MARSHAL_CB scaffolding: a flags pointer for HSTRING_User* */
#include <rpc.h>
#include <rpcndr.h>
typedef struct hs_cb {
    USER_MARSHAL_CB umcb;
    MIDL_STUB_MESSAGE stub;
    RPC_MESSAGE rpc;
} hs_cb;
/* ctx = MSHCTX_*, buf/len describe the RPC buffer (stub.Buffer == buf) */
void hs_cb_init(hs_cb *cb, void *buf, ULONG len, ULONG flags, USER_MARSHAL_CB_TYPE type);
#endif
