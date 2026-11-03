/* Shared helpers for the gap5 VARIANT_User* tests (oleaut32 wire marshaling of VARIANT, NDR32 + NDR64 flavours). */
#ifndef GAP5_UM_H
#define GAP5_UM_H
#define COBJMACROS
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#include <rpc.h>
#include <rpcndr.h>
#include <setjmp.h>

#define G5_SIG 0x55535243u /* 'USRC' */
#define G5_BAD_STUB_DATA 0x6f7u
#define G5_NULL_REF_POINTER 0x6f4u
#define G5_BADVARTYPE 0x80020008u
#define G5_N(a) (sizeof(a) / sizeof((a)[0]))

typedef struct G5Ctx {
    RPC_MESSAGE rpc;
    MIDL_STUB_MESSAGE stub;
    USER_MARSHAL_CB cb;
    unsigned char *raw, *buf;
    ULONG len;
} G5Ctx;

typedef ULONG (WINAPI *g5_size_fn)(ULONG *, ULONG, VARIANT *);
typedef unsigned char *(WINAPI *g5_wire_fn)(ULONG *, unsigned char *, VARIANT *);
typedef void (WINAPI *g5_free_fn)(ULONG *, VARIANT *);

typedef struct G5Api { g5_size_fn size; g5_wire_fn marshal, unmarshal; g5_free_fn free; } G5Api;
/* v64 = 0: VARIANT_User*, 1: VARIANT_User*64.  Returns 0 when any export is missing. */
int g5_api(int v64, G5Api *a);

void g5_init(G5Ctx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type);
void g5_free_ctx(G5Ctx *c);
void g5_reread(G5Ctx *c, ULONG len, ULONG mshctx);
int g5_canary_ok(const G5Ctx *c);

/* each helper returns the SEH-ish exception code raised (0 = none) */
DWORD g5_call_size(const G5Api *a, G5Ctx *c, ULONG start, VARIANT *v, ULONG *out);
DWORD g5_call_marshal(const G5Api *a, G5Ctx *c, unsigned char *at, VARIANT *v, unsigned char **out);
DWORD g5_call_unmarshal(const G5Api *a, G5Ctx *c, unsigned char *at, VARIANT *v, unsigned char **out);
DWORD g5_call_free(const G5Api *a, G5Ctx *c, VARIANT *v);

static const ULONG g5_ctxs[3] = { MSHCTX_LOCAL, MSHCTX_DIFFERENTMACHINE, MSHCTX_NOSHAREDMEM };
static inline ULONG g5_rd32(const unsigned char *p) { ULONG x; memcpy(&x, p, 4); return x; }
static inline unsigned g5_rd16(const unsigned char *p) { unsigned short x; memcpy(&x, p, 2); return x; }

/* Marshal v into a fresh buffer; fills *wire (malloc'd copy, caller frees) and *len. Returns 0 ok. */
int g5_marshal_copy(const G5Api *a, ULONG mshctx, VARIANT *v, unsigned char **wire, ULONG *len, DWORD *exc);
#endif
