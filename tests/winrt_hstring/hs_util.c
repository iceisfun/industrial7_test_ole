#include "hs.h"
#include <setjmp.h>

HSTRING hs_make(const WCHAR *s, UINT32 n)
{
    HSTRING h = HS_SENTINEL;
    HRESULT hr = WCS(s, n, &h);
    t_check(hr == S_OK, __FILE__, __LINE__, "hs_make S_OK", "hr=0x%08lx", (unsigned long)hr);
    if (n == 0) return NULL;
    return h;
}
HSTRING hs_makez(const WCHAR *s) { return hs_make(s, (UINT32)lstrlenW(s)); }

void hs_expect(HSTRING h, const WCHAR *content, UINT32 len, int has_null, const char *file, int line)
{
    BOOL nul = 0xdead;
    UINT32 outlen = 0xdeadbeef;
    const WCHAR *p, *p2;
    t_check(WEMPTY(h) == (len == 0), file, line, "WindowsIsStringEmpty", "len=%u", len);
    t_check(WEMB(h, &nul) == S_OK, file, line, "WindowsStringHasEmbeddedNull S_OK", NULL);
    t_check(nul == (BOOL)has_null, file, line, "HasEmbeddedNull value", "got %d want %d", nul, has_null);
    t_check(WLEN(h) == len, file, line, "WindowsGetStringLen", "got %u want %u", WLEN(h), len);
    p = WRAW(h, &outlen);
    t_check(p != NULL, file, line, "raw buffer non-NULL", NULL);
    t_check(outlen == len, file, line, "raw buffer length out", "got %u want %u", outlen, len);
    p2 = WRAW(h, NULL);
    t_check(p2 == p, file, line, "raw buffer stable with/without length", NULL);
    if (p) {
        t_check(p[len] == 0, file, line, "raw buffer NUL terminated", NULL);
        t_check(memcmp(p, content, len * sizeof(WCHAR)) == 0, file, line, "string content", NULL);
    }
}

/* ---- exception catching ---- */
static jmp_buf g_jb;
static volatile LONG g_active;
static volatile DWORD g_tid;
static volatile DWORD g_code;
static LONG CALLBACK hs_veh(EXCEPTION_POINTERS *ep)
{
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_active || g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000a || c == 0x406d1388 || c == 0x40010005) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c;
    g_active = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
DWORD hs_try(hs_try_fn fn, void *ctx)
{
    static PVOID h;
    if (!h) h = AddVectoredExceptionHandler(1, hs_veh);
    g_code = 0; g_tid = GetCurrentThreadId();
    if (!setjmp(g_jb)) {
        g_active = 1;
        fn(ctx);
        g_active = 0;
        return 0;
    }
    return g_code;
}

void hs_cb_init(hs_cb *cb, void *buf, ULONG len, ULONG flags, USER_MARSHAL_CB_TYPE type)
{
    memset(cb, 0, sizeof(*cb));
    cb->rpc.Buffer = buf;
    cb->rpc.BufferLength = len;
    cb->stub.RpcMsg = &cb->rpc;
    cb->stub.Buffer = buf;
    cb->stub.BufferStart = buf;
    cb->stub.BufferEnd = (unsigned char *)buf + len;
    cb->stub.BufferLength = len;
    cb->umcb.Flags = flags;
    cb->umcb.pStubMsg = &cb->stub;
    cb->umcb.Signature = 0x55535243; /* "USRC" */
    cb->umcb.CBType = type;
}
