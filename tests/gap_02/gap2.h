/* gap_02: shared helpers.
 *
 * Wire helpers (HWND/HDC/HMENU/HACCEL/LPSAFEARRAY _User{Size,Marshal,Unmarshal,Free}[64]) are invoked by function
 * pointer with a hand built USER_MARSHAL_CB (its first member, Flags, is what the helper receives) pointing at a
 * MIDL_STUB_MESSAGE / RPC_MESSAGE. RPC failures are RpcRaiseException calls; g2_call turns them into a return code
 * (a vectored handler + longjmp) so a test can assert the exact exception value.
 *
 * Flags low byte = MSHCTX_*. REMOTE_CALL() is true for 2 (DIFFERENTMACHINE) and 5 (container) only. */
#ifndef GAP2_H
#define GAP2_H
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <rpc.h>
#include <rpcndr.h>
#include <setjmp.h>

#define G2_SIG        0x55535243u      /* USER_MARSHAL_CB_SIGNATURE 'USRC' */
#define G2_TAG_H      0x48746457u      /* 'WdtH' */
#define C_LOCAL   0
#define C_NOSHM   1
#define C_DIFFM   2
#define C_INPROC  3
#define C_CROSSX  4
#define C_CONT    5
#define X_BADTAG     0x6c5u
#define X_BADSTUB    0x6f7u
#define X_BADBUFPOS  0x6f8u
#define X_ARRAYLOCKED 0x8002000du
#define G2_ALIGN(x, a) (((x) + (a) - 1) & ~((ULONG)(a) - 1))
#define ARRAY_N(a) (sizeof(a) / sizeof((a)[0]))

typedef struct G2Ctx {
    RPC_MESSAGE rpc;
    MIDL_STUB_MESSAGE stub;
    USER_MARSHAL_CB umcb;
    unsigned char *raw, *buf;
    ULONG len;
} G2Ctx;

/* buf is NULL when len == 0 and type is BUFFER_SIZE. The buffer is 16-aligned, 0xCC filled, with 64 canary bytes either side. */
void g2_init(G2Ctx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type);
void g2_free(G2Ctx *c);
void g2_reread(G2Ctx *c, ULONG len, ULONG mshctx);   /* re-aim the same buffer at unmarshal with len valid bytes */
int  g2_canary_ok(const G2Ctx *c);                   /* 64 bytes before and after intact */
int  g2_untouched(const G2Ctx *c, ULONG from, ULONG to);
#define G2F(c) (&(c)->umcb.Flags)

typedef ULONG (WINAPI *g2_size_fn)(ULONG *, ULONG, void *);
typedef unsigned char *(WINAPI *g2_buf_fn)(ULONG *, unsigned char *, void *);
typedef void (WINAPI *g2_free_fn)(ULONG *, void *);

/* each returns 0 or the raised exception code */
DWORD g2_x_size(const void *fn, ULONG *fl, ULONG off, void *obj, ULONG *out);
DWORD g2_x_buf(const void *fn, ULONG *fl, unsigned char *at, void *obj, unsigned char **out);
DWORD g2_x_free(const void *fn, ULONG *fl, void *obj);

#define XEQ(call, want) do { DWORD x__ = (call); t_check(x__ == (DWORD)(want), __FILE__, __LINE__, #call " == " #want, "got 0x%lx want 0x%lx", x__, (DWORD)(want)); } while (0)
#define EQ64(a, b) t_check((unsigned long long)(a) == (unsigned long long)(b), __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)(a), (unsigned long long)(b))

static inline unsigned int  g2_rd32(const unsigned char *p) { unsigned int v; memcpy(&v, p, 4); return v; }
static inline unsigned short g2_rd16(const unsigned char *p) { unsigned short v; memcpy(&v, p, 2); return v; }
static inline unsigned long long g2_rd64(const unsigned char *p) { unsigned long long v; memcpy(&v, p, 8); return v; }
static inline void g2_wr32(unsigned char *p, unsigned int v) { memcpy(p, &v, 4); }
static inline void g2_wr16(unsigned char *p, unsigned short v) { memcpy(p, &v, 2); }
static inline void g2_wr64(unsigned char *p, unsigned long long v) { memcpy(p, &v, 8); }
#endif
