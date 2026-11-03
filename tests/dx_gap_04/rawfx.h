/* raw-interface server fixture shared by the call and security tests */
#ifndef RAWFX_H
#define RAWFX_H
#include "rpcx.h"
typedef struct { rx_raw r; WCHAR ep[64]; RPC_BINDING_HANDLE h; UUID u; } rawfx;

static int raw_up_ex(rawfx *f, unsigned n, unsigned short major, unsigned short minor, unsigned flags, RPC_IF_CALLBACK_FN *cb) {
    memset(f, 0, sizeof *f);
    rx_raw_uuid(&f->u, n); rx_raw_init(&f->r, &f->u, major, minor); rx_ep(f->ep, 64);
    rx_server_reset();
    if (rx_use_ep(f->ep) != RPC_S_OK) return 0;
    if (U(RpcServerRegisterIfEx)((RPC_IF_HANDLE)&f->r.si, NULL, NULL, flags, RPC_C_LISTEN_MAX_CALLS_DEFAULT, cb) != RPC_S_OK) return 0;
    if (!(flags & RPC_IF_AUTOLISTEN) && U(RpcServerListen)(1, 20, TRUE) != RPC_S_OK) return 0;
    f->h = rx_bind(f->ep);
    return f->h != NULL;
}
static void raw_down(rawfx *f) { rx_unbind(f->h); f->h = NULL; rx_server_down((RPC_IF_HANDLE)&f->r.si, NULL); }
#define RUP(f, n, ma, mi) do { if (!raw_up(&f, n, ma, mi)) { CHECKF(0, "raw server fixture failed"); raw_down(&f); return; } } while (0)


static int raw_up(rawfx *f, unsigned n, unsigned short major, unsigned short minor) { return raw_up_ex(f, n, major, minor, 0, NULL); }
#define RUPX(f, n, ma, mi, flags, cb) do { if (!raw_up_ex(&f, n, ma, mi, flags, cb)) { CHECKF(0, "raw server fixture failed"); raw_down(&f); return; } } while (0)
#endif
