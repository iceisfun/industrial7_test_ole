#include "g4_um.h"

int um_nalloc, um_nfree;
jmp_buf um_jb;
volatile int um_armed;
volatile DWORD um_code;
ULONG um_g_ul;
unsigned char *um_g_p;

static void *WINAPI um_alloc_cb(size_t n) { um_nalloc++; return U(CoTaskMemAlloc)(n); }
static void WINAPI um_free_cb(void *p) { um_nfree++; U(CoTaskMemFree)(p); }

void um_init(UMC *c, void *buf, ULONG len, ULONG mshctx)
{
    memset(c, 0, sizeof *c);
    c->rm.Buffer = buf;
    c->rm.BufferLength = len;
    c->sm.RpcMsg = &c->rm;
    c->sm.Buffer = buf;
    c->sm.pfnAllocate = um_alloc_cb;
    c->sm.pfnFree = um_free_cb;
    c->cb.Flags = MAKELONG(mshctx, 0x10);   /* NDR_LOCAL_DATA_REPRESENTATION */
    c->cb.pStubMsg = &c->sm;
    c->cb.Signature = UM_SIG;
    c->cb.CBType = buf ? USER_MARSHAL_CB_UNMARSHALL : USER_MARSHAL_CB_BUFFER_SIZE;
}

#define GUARD 64
unsigned char *um_buf_new(size_t n)
{
    unsigned char *p = malloc(n + 2 * GUARD);
    if (!p) return NULL;
    memset(p, 0xCC, n + 2 * GUARD);
    return p + GUARD;
}
void um_buf_free(unsigned char *p) { if (p) free(p - GUARD); }
int um_untouched(const unsigned char *p, size_t from, size_t to)
{
    size_t i;
    for (i = from; i < to; i++) if (p[i] != 0xCC) return 0;
    return 1;
}
int um_guard_ok(const unsigned char *p, size_t n)
{
    return um_untouched(p, (size_t)0 - 0, 0) && um_untouched(p - GUARD, 0, GUARD) && um_untouched(p, n, n + GUARD);
}

static LONG CALLBACK um_veh(PEXCEPTION_POINTERS ep)
{
    if (!um_armed) return EXCEPTION_CONTINUE_SEARCH;
    um_code = ep->ExceptionRecord->ExceptionCode;
    um_armed = 0;
    longjmp(um_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void um_veh_init(void)
{
    static int done;
    if (!done) { AddVectoredExceptionHandler(1, um_veh); done = 1; }
}

DWORD um_x_size(void *fn, ULONG *fl, ULONG off, void *obj, ULONG *out)
{
    DWORD c = 0;
    um_veh_init();
    um_g_ul = 0xdeadbeef;
    if (!setjmp(um_jb)) { um_armed = 1; um_g_ul = ((um_size_fn)fn)(fl, off, obj); um_armed = 0; }
    else c = um_code;
    um_armed = 0;
    if (out) *out = um_g_ul;
    return c;
}
DWORD um_x_mar(void *fn, ULONG *fl, unsigned char *b, void *obj, unsigned char **out)
{
    DWORD c = 0;
    um_veh_init();
    um_g_p = (unsigned char *)(ULONG_PTR)0xdeadbeef;
    if (!setjmp(um_jb)) { um_armed = 1; um_g_p = ((um_buf_fn)fn)(fl, b, obj); um_armed = 0; }
    else c = um_code;
    um_armed = 0;
    if (out) *out = um_g_p;
    return c;
}
DWORD um_x_free(void *fn, ULONG *fl, void *obj)
{
    DWORD c = 0;
    um_veh_init();
    if (!setjmp(um_jb)) { um_armed = 1; ((um_free_fn)fn)(fl, obj); um_armed = 0; }
    else c = um_code;
    um_armed = 0;
    return c;
}

const um_hfam um_hfams[] = {
    { "HWND", 1 }, { "HICON", 1 }, { "HMONITOR", 1 }, { "HDC", 1 },
    { "HMENU", 1 }, { "HACCEL", 1 }, { "HBRUSH", 1 }, { "HRGN", 0 },
};
const int um_nhfams = sizeof um_hfams / sizeof um_hfams[0];
