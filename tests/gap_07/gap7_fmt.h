/* Oif procedure formats (implicit handle, interpreted) for gap_07, derived from the hand-built xif in rpc_server_client_ncalrpc.
 * ADD_PROC: int f(int a, int b)   WLEN_PROC: int f([in,string] wchar_t *s)   RAISE_PROC: void f(int code) */
#define ADD_PROC(M) \
    0x32,	/* FC_BIND_PRIMITIVE */ \
    0x48, \
    NdrFcLong(0x0), \
    NdrFcShort(M), \
    NdrFcShort(0x18),	/* stack size = 24 */ \
    NdrFcShort(0x10),	/* client buffer = 16 */ \
    NdrFcShort(0x8),	/* server buffer = 8 */ \
    0x44, \
    0x03,	/* 3 params */ \
    0x0a, \
    0x01, \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x48),	/* flags: in, base type */ \
    NdrFcShort(0x0),	/* stack offset = 0 */ \
    0x08,	/* FC_LONG */ \
    0x0, \
    NdrFcShort(0x48),	/* flags: in, base type */ \
    NdrFcShort(0x8),	/* stack offset = 8 */ \
    0x08,	/* FC_LONG */ \
    0x0, \
    NdrFcShort(0x70),	/* flags: out, return, base type */ \
    NdrFcShort(0x10),	/* stack offset = 16 */ \
    0x08,	/* FC_LONG */ \
    0x0,
#define WLEN_PROC(M) \
    0x32,	/* FC_BIND_PRIMITIVE */ \
    0x48, \
    NdrFcLong(0x0), \
    NdrFcShort(M), \
    NdrFcShort(0x10),	/* stack size = 16 */ \
    NdrFcShort(0x0),	/* client buffer = 0 */ \
    NdrFcShort(0x8),	/* server buffer = 8 */ \
    0x46, \
    0x02,	/* 2 params */ \
    0x0a, \
    0x01, \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x10b),	/* flags: must size, must free, in, simple ref */ \
    NdrFcShort(0x0),	/* stack offset = 0 */ \
    NdrFcShort(0x4),	/* type offset = 4 */ \
    NdrFcShort(0x70),	/* flags: out, return, base type */ \
    NdrFcShort(0x8),	/* stack offset = 8 */ \
    0x08,	/* FC_LONG */ \
    0x0,
#define RAISE_PROC(M) \
    0x32,	/* FC_BIND_PRIMITIVE */ \
    0x48, \
    NdrFcLong(0x0), \
    NdrFcShort(M), \
    NdrFcShort(0x8),	/* stack size = 8 */ \
    NdrFcShort(0x8),	/* client buffer = 8 */ \
    NdrFcShort(0x0),	/* server buffer = 0 */ \
    0x40, \
    0x01,	/* 1 params */ \
    0x0a, \
    0x01, \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x0), \
    NdrFcShort(0x48),	/* flags: in, base type */ \
    NdrFcShort(0x0),	/* stack offset = 0 */ \
    0x08,	/* FC_LONG */ \
    0x0,
