#include "gap2.h"

void g2_init(G2Ctx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type)
{
    memset(c, 0, sizeof *c);
    if (len || type != USER_MARSHAL_CB_BUFFER_SIZE) {
        c->raw = malloc(len + 256);
        memset(c->raw, 0xcc, len + 256);
        c->buf = (unsigned char *)(((ULONG_PTR)c->raw + 64 + 15) & ~(ULONG_PTR)15);
    }
    c->len = len;
    c->rpc.Buffer = c->buf;
    c->rpc.BufferLength = len;
    c->stub.RpcMsg = &c->rpc;
    c->stub.Buffer = c->buf;
    c->stub.BufferStart = c->buf;
    c->stub.BufferEnd = c->buf ? c->buf + len : NULL;
    c->stub.BufferLength = len;
    c->umcb.Flags = MAKELONG(mshctx, NDR_LOCAL_DATA_REPRESENTATION);
    c->umcb.pStubMsg = &c->stub;
    c->umcb.Signature = G2_SIG;
    c->umcb.CBType = type;
}
void g2_free(G2Ctx *c) { free(c->raw); c->raw = c->buf = NULL; }
void g2_reread(G2Ctx *c, ULONG len, ULONG mshctx)
{
    c->len = len;
    c->rpc.Buffer = c->buf;
    c->rpc.BufferLength = len;
    c->stub.Buffer = c->buf;
    c->stub.BufferStart = c->buf;
    c->stub.BufferEnd = c->buf + len;
    c->stub.BufferLength = len;
    c->umcb.Flags = MAKELONG(mshctx, NDR_LOCAL_DATA_REPRESENTATION);
    c->umcb.CBType = USER_MARSHAL_CB_UNMARSHALL;
}
int g2_canary_ok(const G2Ctx *c)
{
    ULONG i;
    if (!c->buf) return 1;
    for (i = 1; i <= 64; i++) if (c->buf[-(long)i] != 0xcc) return 0;
    for (i = 0; i < 64; i++) if (c->buf[c->len + i] != 0xcc) return 0;
    return 1;
}
int g2_untouched(const G2Ctx *c, ULONG from, ULONG to)
{
    ULONG i;
    for (i = from; i < to; i++) if (c->buf[i] != 0xcc) return 0;
    return 1;
}

static jmp_buf g_jb;
static volatile LONG g_armed;
static volatile DWORD g_tid, g_code;
static LONG CALLBACK g2_veh(EXCEPTION_POINTERS *ep)
{
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_armed || g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000a || c == 0x406d1388 || c == 0x40010005 || c == 0x4001000b) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c;
    g_armed = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
static void arm_init(void)
{
    static PVOID h;
    if (!h) h = AddVectoredExceptionHandler(1, g2_veh);
    g_code = 0; g_tid = GetCurrentThreadId();
}
static ULONG s_ul; static unsigned char *s_p;
DWORD g2_x_size(const void *fn, ULONG *fl, ULONG off, void *obj, ULONG *out)
{
    DWORD c = 0;
    arm_init(); s_ul = 0xdeadbeef;
    if (!setjmp(g_jb)) { g_armed = 1; s_ul = ((g2_size_fn)fn)(fl, off, obj); g_armed = 0; }
    else c = g_code;
    g_armed = 0;
    if (out) *out = s_ul;
    return c;
}
DWORD g2_x_buf(const void *fn, ULONG *fl, unsigned char *at, void *obj, unsigned char **out)
{
    DWORD c = 0;
    arm_init(); s_p = (unsigned char *)(ULONG_PTR)0xdeadbeef;
    if (!setjmp(g_jb)) { g_armed = 1; s_p = ((g2_buf_fn)fn)(fl, at, obj); g_armed = 0; }
    else c = g_code;
    g_armed = 0;
    if (out) *out = s_p;
    return c;
}
DWORD g2_x_free(const void *fn, ULONG *fl, void *obj)
{
    DWORD c = 0;
    arm_init();
    if (!setjmp(g_jb)) { g_armed = 1; ((g2_free_fn)fn)(fl, obj); g_armed = 0; }
    else c = g_code;
    g_armed = 0;
    return c;
}
