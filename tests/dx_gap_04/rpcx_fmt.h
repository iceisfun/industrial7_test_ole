/* NDR (Oif) procedure and type format strings for the hand-built "xif" test interface.
 * Produced once from tests/rpc_server_client_ncalrpc/xif.idl (kept in comments of rpcx.h)
 * and embedded as plain data; the corpus never depends on a MIDL toolchain.
 * Layout is the documented NDR format: proc header (implicit/explicit handle), Oif params, type descriptors. */
#define RX_PROC_FMT_SIZE 609
#define RX_TYPE_FMT_SIZE 117
static const unsigned char rx_proc_fmt[1 + RX_PROC_FMT_SIZE + 1] = { 0, 0,

/* 0 (procedure xif::p_void) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x0),	/* method 0 */
        NdrFcShort(0x0),	/* stack size = 0 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x40,
        0x00,	/* 0 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 26 (procedure xif::p_add) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x1),	/* method 1 */
        NdrFcShort(0x18),	/* stack size = 24 */
        NdrFcShort(0x10),	/* client buffer = 16 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x44,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 52 (parameter a) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 58 (parameter b) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 64 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 70 (procedure xif::p_wlen) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x2),	/* method 2 */
        NdrFcShort(0x10),	/* stack size = 16 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x46,
        0x02,	/* 2 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 96 (parameter s) */
        NdrFcShort(0x10b),	/* flags: must size, must free, in, simple ref */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x4),	/* type offset = 4 */
/* 102 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 108 (procedure xif::p_sum) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x3),	/* method 3 */
        NdrFcShort(0x18),	/* stack size = 24 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x46,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 134 (parameter n) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 140 (parameter a) */
        NdrFcShort(0x10b),	/* flags: must size, must free, in, simple ref */
        NdrFcShort(0x8),	/* stack offset = 8 */
        NdrFcShort(0x6),	/* type offset = 6 */
/* 146 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 152 (procedure xif::p_seq) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x4),	/* method 4 */
        NdrFcShort(0x10),	/* stack size = 16 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x41,
        0x02,	/* 2 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 178 (parameter n) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 184 (parameter a) */
        NdrFcShort(0x113),	/* flags: must size, must free, out, simple ref */
        NdrFcShort(0x8),	/* stack offset = 8 */
        NdrFcShort(0x16),	/* type offset = 22 */
/* 190 (procedure xif::p_bump) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x5),	/* method 5 */
        NdrFcShort(0x8),	/* stack size = 8 */
        NdrFcShort(0x18),	/* client buffer = 24 */
        NdrFcShort(0x18),	/* server buffer = 24 */
        0x40,
        0x01,	/* 1 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 216 (parameter s) */
        NdrFcShort(0x11a),	/* flags: must free, in, out, simple ref */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x26),	/* type offset = 38 */
/* 222 (procedure xif::p_raise) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x6),	/* method 6 */
        NdrFcShort(0x8),	/* stack size = 8 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x40,
        0x01,	/* 1 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 248 (parameter code) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 254 (procedure xif::p_sleep) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x7),	/* method 7 */
        NdrFcShort(0x8),	/* stack size = 8 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x40,
        0x01,	/* 1 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 280 (parameter ms) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 286 (procedure xif::p_ctx_open) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x8),	/* method 8 */
        NdrFcShort(0x10),	/* stack size = 16 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x18),	/* server buffer = 24 */
        0x40,
        0x02,	/* 2 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 312 (parameter h) */
        NdrFcShort(0x110),	/* flags: out, simple ref */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x38),	/* type offset = 56 */
/* 318 (parameter v) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 324 (procedure xif::p_ctx_get) */
        0x00,	/* explicit handle */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0x9),	/* method 9 */
        NdrFcShort(0x10),	/* stack size = 16 */
        0x30,	/* FC_BIND_CONTEXT */
        0x41,
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x00,
        0x00,	/* param 0 */
        NdrFcShort(0x18),	/* client buffer = 24 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x44,
        0x02,	/* 2 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 356 (parameter h) */
        NdrFcShort(0x8),	/* flags: in */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x3c),	/* type offset = 60 */
/* 362 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 368 (procedure xif::p_ctx_close) */
        0x00,	/* explicit handle */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xa),	/* method 10 */
        NdrFcShort(0x8),	/* stack size = 8 */
        0x30,	/* FC_BIND_CONTEXT */
        0xe0,
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x00,
        0x00,	/* param 0 */
        NdrFcShort(0x18),	/* client buffer = 24 */
        NdrFcShort(0x18),	/* server buffer = 24 */
        0x40,
        0x01,	/* 1 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 400 (parameter h) */
        NdrFcShort(0x118),	/* flags: in, out, simple ref */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x44),	/* type offset = 68 */
/* 406 (procedure xif::p_info) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xb),	/* method 11 */
        NdrFcShort(0x20),	/* stack size = 32 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x20),	/* server buffer = 32 */
        0x40,
        0x04,	/* 4 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 432 (parameter pid) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 438 (parameter level) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 444 (parameter islocal) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 450 (parameter authn) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x18),	/* stack offset = 24 */
        0x08,	/* FC_LONG */
        0x0,
/* 456 (procedure xif::p_echo) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xc),	/* method 12 */
        NdrFcShort(0x18),	/* stack size = 24 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x43,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 482 (parameter s) */
        NdrFcShort(0x10b),	/* flags: must size, must free, in, simple ref */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x5a),	/* type offset = 90 */
/* 488 (parameter n) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 494 (parameter o) */
        NdrFcShort(0x113),	/* flags: must size, must free, out, simple ref */
        NdrFcShort(0x10),	/* stack offset = 16 */
        NdrFcShort(0x5c),	/* type offset = 92 */
/* 500 (procedure xif::p_stop) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xd),	/* method 13 */
        NdrFcShort(0x0),	/* stack size = 0 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x0),	/* server buffer = 0 */
        0x40,
        0x00,	/* 0 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 526 (procedure xif::p_counter) */
        0x32,	/* FC_BIND_PRIMITIVE */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xe),	/* method 14 */
        NdrFcShort(0x8),	/* stack size = 8 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x44,
        0x01,	/* 1 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 552 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x08,	/* FC_LONG */
        0x0,
/* 558 (procedure xif::p_ctx_peek) */
        0x00,	/* explicit handle */
        0x48,
        NdrFcLong(0x0),
        NdrFcShort(0xf),	/* method 15 */
        NdrFcShort(0x18),	/* stack size = 24 */
        0x30,	/* FC_BIND_CONTEXT */
        0x41,
        NdrFcShort(0x0),	/* stack offset = 0 */
        0x00,
        0x00,	/* param 0 */
        NdrFcShort(0x18),	/* client buffer = 24 */
        NdrFcShort(0x10),	/* server buffer = 16 */
        0x44,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 590 (parameter h) */
        NdrFcShort(0x8),	/* flags: in */
        NdrFcShort(0x0),	/* stack offset = 0 */
        NdrFcShort(0x6c),	/* type offset = 108 */
/* 596 (parameter rundowns) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 602 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
        0x0};
static const unsigned char rx_type_fmt[2 + RX_TYPE_FMT_SIZE + 1] = { 0, 0,

        NdrFcShort(0x0),
/* 2 (wchar_t *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
/*  4 */
        0x25,	/* FC_C_WSTRING */
        0x5c,	/* FC_PAD */
/* 6 (int *) */
        0x1b,	/* FC_CARRAY */
        0x3,	/* 3 */
        NdrFcShort(0x4),	/* 4 */
        0x28,	/* Corr desc: parameter n, FC_LONG */
        0x0,	/* no operators */
        NdrFcShort(0x0),	/* offset = 0 */
        NdrFcShort(0x1),
        0x08,	/* FC_LONG */
        0x5b,	/* FC_END */
/* 18 */
        0x11, 0x0,	/* FC_RP */
        NdrFcShort(0xfff2),	/* Offset= -14 (6) */
/* 22 (int *) */
        0x1b,	/* FC_CARRAY */
        0x3,	/* 3 */
        NdrFcShort(0x4),	/* 4 */
        0x28,	/* Corr desc: parameter n, FC_LONG */
        0x0,	/* no operators */
        NdrFcShort(0x0),	/* offset = 0 */
        NdrFcShort(0x1),
        0x08,	/* FC_LONG */
        0x5b,	/* FC_END */
/* 34 */
        0x11, 0x0,	/* FC_RP */
        NdrFcShort(0xfff2),	/* Offset= -14 (22) */
/* 38 (S) */
        0x15,	/* FC_STRUCT */
        0x7,	/* 7 */
        NdrFcShort(0x10),	/* 16 */
        0x08,	/* FC_LONG */
        0x06,	/* FC_SHORT */
        0x3e,	/* FC_STRUCTPAD2 */
        0x0c,	/* FC_DOUBLE */
        0x5c,	/* FC_PAD */
        0x5b,	/* FC_END */
/* 48 (S *) */
        0x11, 0x0,		/* FC_RP */
        NdrFcShort(0xfff4),	/* Offset= -12 (38) */
/* 52 (CTX *) */
        0x11, 0x0,	/* FC_RP */
        NdrFcShort(0x2),	 /* Offset= 2 (56) */
/* 56 */
        0x30,	/* FC_BIND_CONTEXT */
        0xa0,	/* Context flags: out, via ptr, */
        0x0,	/* rundown routine */
        0, /* FIXME: param num */
/* 60 (CTX) */
        0x30,	/* FC_BIND_CONTEXT */
        0x41,	/* Context flags: can't be null, in, */
        0x0,	/* rundown routine */
        0, /* FIXME: param num */
/* 64 (CTX *) */
        0x11, 0x0,	/* FC_RP */
        NdrFcShort(0x2),	 /* Offset= 2 (68) */
/* 68 */
        0x30,	/* FC_BIND_CONTEXT */
        0xe0,	/* Context flags: out, in, via ptr, */
        0x0,	/* rundown routine */
        0, /* FIXME: param num */
/* 72 (int *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 76 (int *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 80 (int *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 84 (int *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 88 (wchar_t *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 90 */
        0x25,	/* FC_C_WSTRING */
        0x5c,	/* FC_PAD */
/* 92 (wchar_t *) */
        0x1b,	/* FC_CARRAY */
        0x1,	/* 1 */
        NdrFcShort(0x2),	/* 2 */
        0x28,	/* Corr desc: parameter n, FC_LONG */
        0x0,	/* no operators */
        NdrFcShort(0x8),	/* offset = 8 */
        NdrFcShort(0x1),
        0x05,	/* FC_WCHAR */
        0x5b,	/* FC_END */
/* 104 */
        0x11, 0x0,	/* FC_RP */
        NdrFcShort(0xfff2),	/* Offset= -14 (92) */
/* 108 (CTX) */
        0x30,	/* FC_BIND_CONTEXT */
        0x41,	/* Context flags: can't be null, in, */
        0x0,	/* rundown routine */
        0, /* FIXME: param num */
/* 112 (int *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
        0x0};
