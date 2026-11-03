/* NDR format strings for IOletOne/IOletTwo (x64, -Oicf style interpreter format, hand-reviewed). */
#define PSD_TYPE_FORMAT_SIZE 71
#define PSD_PROC_FORMAT_SIZE 221
typedef struct { short Pad; unsigned char Format[PSD_TYPE_FORMAT_SIZE]; } PSD_TYPE_FMT;
typedef struct { short Pad; unsigned char Format[PSD_PROC_FORMAT_SIZE]; } PSD_PROC_FMT;
static const PSD_PROC_FMT psd_proc_fmt =
{
    0,
    {
/* 0 (procedure IOletOne::Add) */
        0x33,	/* FC_AUTO_HANDLE */
        0x6c,
        NdrFcLong(0x0),
        NdrFcShort(0x3),	/* method 3 */
        NdrFcShort(0x28),	/* stack size = 40 */
        NdrFcShort(0x10),	/* client buffer = 16 */
        NdrFcShort(0x10),	/* server buffer = 16 */
        0x44,
        0x04,	/* 4 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 26 (parameter a) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 32 (parameter b) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 38 (parameter r) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x18),	/* stack offset = 24 */
        0x08,	/* FC_LONG */
        0x0,
/* 44 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x20),	/* stack offset = 32 */
        0x08,	/* FC_LONG */
        0x0,
/* 50 (procedure IOletTwo::Echo) */
        0x33,	/* FC_AUTO_HANDLE */
        0x6c,
        NdrFcLong(0x0),
        NdrFcShort(0x3),	/* method 3 */
        NdrFcShort(0x20),	/* stack size = 32 */
        NdrFcShort(0x8),	/* client buffer = 8 */
        NdrFcShort(0x10),	/* server buffer = 16 */
        0x44,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 76 (parameter a) */
        NdrFcShort(0x48),	/* flags: in, base type */
        NdrFcShort(0x8),	/* stack offset = 8 */
        0x08,	/* FC_LONG */
        0x0,
/* 82 (parameter r) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 88 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x18),	/* stack offset = 24 */
        0x08,	/* FC_LONG */
        0x0,
/* 94 (procedure IOletTwo::Str) */
        0x33,	/* FC_AUTO_HANDLE */
        0x6c,
        NdrFcLong(0x0),
        NdrFcShort(0x4),	/* method 4 */
        NdrFcShort(0x20),	/* stack size = 32 */
        NdrFcShort(0x0),	/* client buffer = 0 */
        NdrFcShort(0x10),	/* server buffer = 16 */
        0x46,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 120 (parameter s) */
        NdrFcShort(0x10b),	/* flags: must size, must free, in, simple ref */
        NdrFcShort(0x8),	/* stack offset = 8 */
        NdrFcShort(0xc),	/* type offset = 12 */
/* 126 (parameter len) */
        NdrFcShort(0x2150),	/* flags: out, base type, simple ref, srv size=8 */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
/* 132 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x18),	/* stack offset = 24 */
        0x08,	/* FC_LONG */
        0x0,
/* 138 (procedure IOletTwo::GetObj) */
        0x33,	/* FC_AUTO_HANDLE */
        0x6c,
        NdrFcLong(0x0),
        NdrFcShort(0x5),	/* method 5 */
        NdrFcShort(0x20),	/* stack size = 32 */
        NdrFcShort(0x14),	/* client buffer = 20 */
        NdrFcShort(0x8),	/* server buffer = 8 */
        0x45,
        0x03,	/* 3 params */
        0x0a,
        0x01,
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
        NdrFcShort(0x0),
/* 164 (parameter riid) */
        NdrFcShort(0x10a),	/* flags: must free, in, simple ref */
        NdrFcShort(0x8),	/* stack offset = 8 */
        NdrFcShort(0x18),	/* type offset = 24 */
/* 170 (parameter pp) */
        NdrFcShort(0x13),	/* flags: must size, must free, out */
        NdrFcShort(0x10),	/* stack offset = 16 */
        NdrFcShort(0x30),	/* type offset = 48 */
/* 176 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x18),	/* stack offset = 24 */
        0x08,	/* FC_LONG */
        0x0,
/* 182 (procedure IOletTwo::PutObj) */
        0x33,	/* FC_AUTO_HANDLE */
        0x6c,
        NdrFcLong(0x0),
        NdrFcShort(0x6),	/* method 6 */
        NdrFcShort(0x18),	/* stack size = 24 */
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
/* 208 (parameter p) */
        NdrFcShort(0xb),	/* flags: must size, must free, in */
        NdrFcShort(0x8),	/* stack offset = 8 */
        NdrFcShort(0x34),	/* type offset = 52 */
/* 214 (return value) */
        NdrFcShort(0x70),	/* flags: out, return, base type */
        NdrFcShort(0x10),	/* stack offset = 16 */
        0x08,	/* FC_LONG */
        0x0,
        0x0
    }
};
static const PSD_TYPE_FMT psd_type_fmt =
{
    0,
    {
        NdrFcShort(0x0),
/* 2 (LONG *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 6 (LONG *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 10 (const wchar_t *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
/* 12 */
        0x25,	/* FC_C_WSTRING */
        0x5c,	/* FC_PAD */
/* 14 (LONG *) */
        0x11, 0x8,	/* FC_RP [simple_pointer] */
        0x08,	/* FC_LONG */
        0x5c,	/* FC_PAD */
/* 18 (byte[8]) */
        0x1d,	/* FC_SMFARRAY */
        0x0,	/* 0 */
        NdrFcShort(0x8),	/* 8 */
        0x01,	/* FC_BYTE */
        0x5b,	/* FC_END */
/* 24 (IID) */
        0x15,	/* FC_STRUCT */
        0x3,	/* 3 */
        NdrFcShort(0x10),	/* 16 */
        0x08,	/* FC_LONG */
        0x06,	/* FC_SHORT */
        0x06,	/* FC_SHORT */
        0x4c,	/* FC_EMBEDDED_COMPLEX */
        0x0,
        NdrFcShort(0xfff1),	/* Offset= -15 (18) */
        0x5b,	/* FC_END */
/* 36 (REFIID) */
        0x11, 0x0,		/* FC_RP */
        NdrFcShort(0xfff2),	/* Offset= -14 (24) */
/* 40 (IUnknown *) */
        0x2f,  /* FC_IP */
        0x5c,  /* FC_PAD */
        0x2b,	/* Corr desc: parameter riid, FC_HYPER */
        0x0,	/* no operators */
        NdrFcShort(0x8),	/* offset = 8 */
        NdrFcShort(0x5),
/* 48 (IUnknown **) */
        0x11, 0x10,		/* FC_RP [pointer_deref] */
        NdrFcShort(0xfff6),	/* Offset= -10 (40) */
/* 52 (IOletOne *) */
        0x2f,	/* FC_IP */
        0x5a,	/* FC_CONSTANT_IID */
        NdrFcLong(0x6f7a0001),
        NdrFcShort(0x4a5b),
        NdrFcShort(0x4c6d),
        0x8e,
        0x7f,
        0x0a,
        0x1b,
        0x2c,
        0x3d,
        0x4e,
        0x5f,

        0x0
    }
};
