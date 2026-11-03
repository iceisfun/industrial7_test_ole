/* usrmarshal_ole32: shared helpers for the *_User{Size,Marshal,Unmarshal,Free}[64] wire helpers.
 *
 * Calling convention of every helper (exported by ole32.dll, implemented in combase.dll on Win10):
 *   ULONG  Size   (ULONG *flags, ULONG offset, T *obj)
 *   BYTE * Marshal(ULONG *flags, BYTE *buf,    T *obj)
 *   BYTE * Unmarshal(ULONG *flags, BYTE *buf,  T *obj)
 *   void   Free   (ULONG *flags, T *obj)
 * where `flags` is really the first member of a USER_MARSHAL_CB that points at a MIDL_STUB_MESSAGE/RPC_MESSAGE.
 * Failures are signalled by RpcRaiseException, which the helpers below turn into a returned exception code. */
#ifndef UM_H
#define UM_H
#include "t.h"
#include <objbase.h>
#include <rpc.h>
#include <rpcndr.h>
#include <setjmp.h>

#define UM_SIG       0x55535243u      /* USER_MARSHAL_CB_SIGNATURE */
#define UM_TAG_R     0x52746457u      /* 'WdtR' WDT_REMOTE_CALL   */
#define UM_TAG_H     0x48746457u      /* 'WdtH' WDT_INPROC_CALL   */
#define UM_TAG_P     0x50746457u      /* 'WdtP' WDT_INPROC64_CALL */

/* context byte of the flags word (MSHCTX_*) */
#define C_LOCAL   0
#define C_NOSHM   1
#define C_DIFFM   2
#define C_INPROC  3
#define C_CROSSX  4
#define C_CONT    5

/* RPC exception codes observed in the decompilation */
#define X_BADTAG     0x6c5u   /* wrong wire discriminant / remote handle */
#define X_BADCONF    0x6c6u   /* inconsistent conformant-string header */
#define X_BADSTUB    0x6f7u   /* buffer too short / malformed */
#define X_BADBUFPOS  0x6f8u   /* supplied pointer outside the RPC buffer */

typedef ULONG (WINAPI *um_size_fn)(ULONG *, ULONG, void *);
typedef unsigned char *(WINAPI *um_buf_fn)(ULONG *, unsigned char *, void *);
typedef void (WINAPI *um_free_fn)(ULONG *, void *);

typedef struct {
    USER_MARSHAL_CB cb;
    MIDL_STUB_MESSAGE sm;
    RPC_MESSAGE rm;
} UMC;

extern int um_nalloc, um_nfree;
extern jmp_buf um_jb;
extern volatile int um_armed;
extern volatile DWORD um_code;
extern ULONG um_g_ul;
extern unsigned char *um_g_p;

/* Fill an NDR-style callback context. buf==NULL -> sizing pass (CBType BUFFER_SIZE), else CBType UNMARSHALL. */
void um_init(UMC *c, void *buf, ULONG len, ULONG mshctx);
#define UMF(c) (&(c)->cb.Flags)

/* Guarded buffer: n usable bytes between 64 canary bytes on each side, everything 0xCC. */
unsigned char *um_buf_new(size_t n);
void um_buf_free(unsigned char *p);
int  um_untouched(const unsigned char *p, size_t from, size_t to);   /* all bytes in [from,to) still 0xCC */
int  um_guard_ok(const unsigned char *p, size_t n);                  /* canaries before/after intact */

/* Each returns 0 or the RPC exception code that was raised. */
DWORD um_x_size(void *fn, ULONG *fl, ULONG off, void *obj, ULONG *out);
DWORD um_x_mar(void *fn, ULONG *fl, unsigned char *b, void *obj, unsigned char **out);
DWORD um_x_free(void *fn, ULONG *fl, void *obj);
#define um_x_unm um_x_mar

#define RD_U32(b, o) (*(ULONG *)((b) + (o)))
#define XEQ(call, want) do { DWORD x__ = (call); t_check(x__ == (DWORD)(want), __FILE__, __LINE__, #call " == " #want, "got 0x%lx want 0x%lx", x__, (DWORD)(want)); } while (0)
#define NEED(x) if (!(x)) { t_note("missing export: " #x); continue; }
#define UM_ALIGN(x, a) (((x) + (a) - 1) & ~((ULONG)(a) - 1))
#define UM_FN(name) ((void *)t_proc(name))
#define UM_FNIN(dll, name) ((void *)t_proc_in(dll, name))

/* Simple "handle" families that share one implementation. */
typedef struct { const char *pfx; int has64; } um_hfam;
extern const um_hfam um_hfams[];
extern const int um_nhfams;
#endif
