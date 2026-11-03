#include "gap5_um.h"

static const char *const opn[4] = { "VARIANT_UserSize", "VARIANT_UserMarshal", "VARIANT_UserUnmarshal", "VARIANT_UserFree" };

int g5_api(int v64, G5Api *a)
{
    char n[64]; FARPROC f[4]; int i;
    for (i = 0; i < 4; i++) {
        snprintf(n, sizeof n, "%s%s", opn[i], v64 ? "64" : "");
        f[i] = t_proc_in("oleaut32.dll", n);
        if (!f[i]) return 0;
    }
    a->size = (g5_size_fn)f[0]; a->marshal = (g5_wire_fn)f[1]; a->unmarshal = (g5_wire_fn)f[2]; a->free = (g5_free_fn)f[3];
    return 1;
}

void g5_init(G5Ctx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type)
{
    memset(c, 0, sizeof *c);
    if (len || type != USER_MARSHAL_CB_BUFFER_SIZE) {
        c->raw = malloc(len + 64 + 32);
        c->buf = (unsigned char *)(((ULONG_PTR)c->raw + 15) & ~(ULONG_PTR)15);
        memset(c->buf, 0xcc, len + 64);
    }
    c->len = len;
    c->rpc.Buffer = c->buf; c->rpc.BufferLength = len;
    c->stub.RpcMsg = &c->rpc; c->stub.Buffer = c->buf; c->stub.BufferStart = c->buf;
    c->stub.BufferEnd = c->buf ? c->buf + len : NULL; c->stub.BufferLength = len;
    c->cb.Flags = MAKELONG(mshctx, NDR_LOCAL_DATA_REPRESENTATION);
    c->cb.pStubMsg = &c->stub; c->cb.Signature = G5_SIG; c->cb.CBType = type;
}
void g5_free_ctx(G5Ctx *c) { free(c->raw); c->raw = c->buf = NULL; }
void g5_reread(G5Ctx *c, ULONG len, ULONG mshctx)
{
    c->len = len;
    c->rpc.Buffer = c->buf; c->rpc.BufferLength = len;
    c->stub.Buffer = c->buf; c->stub.BufferStart = c->buf; c->stub.BufferEnd = c->buf + len; c->stub.BufferLength = len;
    c->cb.Flags = MAKELONG(mshctx, NDR_LOCAL_DATA_REPRESENTATION);
    c->cb.CBType = USER_MARSHAL_CB_UNMARSHALL;
}
int g5_canary_ok(const G5Ctx *c)
{
    ULONG i;
    if (!c->buf) return 1;
    for (i = 0; i < 64; i++) if (c->buf[c->len + i] != 0xcc) return 0;
    return 1;
}

static jmp_buf g_jb;
static volatile LONG g_active;
static volatile DWORD g_tid, g_code;
static LONG CALLBACK veh(EXCEPTION_POINTERS *ep)
{
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_active || g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000a || c == 0x406d1388 || c == 0x40010005 || c == 0x4001000b) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c; g_active = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

#define GUARD_BEGIN do { static PVOID h_; if (!h_) h_ = AddVectoredExceptionHandler(1, veh); \
    g_code = 0; g_tid = GetCurrentThreadId(); if (!setjmp(g_jb)) { g_active = 1;
#define GUARD_END g_active = 0; } else return g_code; } while (0); return 0

DWORD g5_call_size(const G5Api *a, G5Ctx *c, ULONG start, VARIANT *v, ULONG *out)
{ GUARD_BEGIN *out = a->size(&c->cb.Flags, start, v); GUARD_END; }
DWORD g5_call_marshal(const G5Api *a, G5Ctx *c, unsigned char *at, VARIANT *v, unsigned char **out)
{ GUARD_BEGIN *out = a->marshal(&c->cb.Flags, at, v); GUARD_END; }
DWORD g5_call_unmarshal(const G5Api *a, G5Ctx *c, unsigned char *at, VARIANT *v, unsigned char **out)
{ GUARD_BEGIN *out = a->unmarshal(&c->cb.Flags, at, v); GUARD_END; }
DWORD g5_call_free(const G5Api *a, G5Ctx *c, VARIANT *v)
{ GUARD_BEGIN a->free(&c->cb.Flags, v); GUARD_END; }

int g5_marshal_copy(const G5Api *a, ULONG mshctx, VARIANT *v, unsigned char **wire, ULONG *len, DWORD *exc)
{
    G5Ctx s, m; ULONG sz = 0; unsigned char *end = NULL; DWORD e;
    *wire = NULL; *len = 0;
    g5_init(&s, 0, mshctx, USER_MARSHAL_CB_BUFFER_SIZE);
    e = g5_call_size(a, &s, 0, v, &sz);
    g5_free_ctx(&s);
    if (e) { if (exc) *exc = e; return -1; }
    g5_init(&m, sz, mshctx, USER_MARSHAL_CB_MARSHALL);
    e = g5_call_marshal(a, &m, m.buf, v, &end);
    if (!e && (!end || end < m.buf || (ULONG)(end - m.buf) > sz)) e = 0xfeed0001;
    if (e) { g5_free_ctx(&m); if (exc) *exc = e; return -1; }
    *len = (ULONG)(end - m.buf);
    *wire = malloc(*len + 1);
    memcpy(*wire, m.buf, *len);
    if (!g5_canary_ok(&m)) { if (exc) *exc = 0xfeed0002; g5_free_ctx(&m); return -1; }
    g5_free_ctx(&m);
    if (exc) *exc = 0;
    return 0;
}
