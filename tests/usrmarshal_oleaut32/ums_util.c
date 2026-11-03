#include "ums.h"

const ULONG ums_ctxs[3] = { MSHCTX_LOCAL, MSHCTX_DIFFERENTMACHINE, MSHCTX_NOSHAREDMEM };

/* ---- checks ------------------------------------------------------------------------------------------------ */
void ums_eq(long long got, long long want, const char *file, int line, const char *expr)
{
    t_check(got == want || ((unsigned)got == (unsigned)want && got >= -2147483648LL && got <= 4294967295LL &&
                            want >= -2147483648LL && want <= 4294967295LL),
            file, line, expr, "got 0x%llx want 0x%llx", (unsigned long long)got, (unsigned long long)want);
}

void ums_check_wire(const unsigned char *got, const unsigned char *exp, const unsigned char *mask, ULONG n,
                    const char *file, int line, const char *what)
{
    ULONG i;
    for (i = 0; i < n; i++) {
        if (mask && mask[i]) continue;
        if (got[i] != exp[i]) break;
    }
    if (i == n) { t_check(1, file, line, what, NULL); return; }
    t_check(0, file, line, what, "wire mismatch at +%lu: got %02x want %02x (len %lu)", i, got[i], exp[i], n);
}

/* ---- contexts ----------------------------------------------------------------------------------------------- */
void uctx_init(UCtx *c, ULONG len, ULONG mshctx, USER_MARSHAL_CB_TYPE type)
{
    memset(c, 0, sizeof(*c));
    if (len || type != USER_MARSHAL_CB_BUFFER_SIZE) {
        c->raw = malloc(len + UMS_CANARY + 32);
        c->buf = (unsigned char *)(((ULONG_PTR)c->raw + 15) & ~(ULONG_PTR)15);
        memset(c->buf, 0xcc, len + UMS_CANARY);
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
    c->umcb.Signature = UMS_SIG;
    c->umcb.CBType = type;
}

void uctx_free(UCtx *c) { free(c->raw); c->raw = c->buf = NULL; }

void uctx_reread(UCtx *c, ULONG len, ULONG mshctx)
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

void uctx_wire(UCtx *c, const void *wire, ULONG len, ULONG mshctx)
{
    uctx_init(c, len, mshctx, USER_MARSHAL_CB_UNMARSHALL);
    memcpy(c->buf, wire, len);
}

int uctx_canary_ok(const UCtx *c)
{
    ULONG i;
    if (!c->buf) return 1;
    for (i = 0; i < UMS_CANARY; i++) if (c->buf[c->len + i] != 0xcc) return 0;
    return 1;
}

/* ---- entry points -------------------------------------------------------------------------------------------- */
static const char *fam_name[3] = { "BSTR", "VARIANT", "LPSAFEARRAY" };
static const char *op_name[4] = { "_UserSize", "_UserMarshal", "_UserUnmarshal", "_UserFree" };
static FARPROC g_fp[3][2][4];
static char g_fp_done[3][2][4];

static FARPROC fp(int fam, int v64, int op)
{
    if (!g_fp_done[fam][v64][op]) {
        char n[64];
        snprintf(n, sizeof n, "%s%s%s", fam_name[fam], op_name[op], v64 ? "64" : "");
        g_fp[fam][v64][op] = t_proc_in("oleaut32.dll", n);
        g_fp_done[fam][v64][op] = 1;
    }
    return g_fp[fam][v64][op];
}

int u_have(int fam, int v64)
{
    int i;
    for (i = 0; i < 4; i++) if (!fp(fam, v64, i)) return 0;
    return 1;
}

typedef ULONG (WINAPI *size_fn)(ULONG *, ULONG, void *);
typedef unsigned char *(WINAPI *marsh_fn)(ULONG *, unsigned char *, void *);
typedef void (WINAPI *free_fn)(ULONG *, void *);

/* exception catching */
static jmp_buf g_jb;
static volatile LONG g_active;
static volatile DWORD g_tid;
static volatile DWORD g_code;
static LONG CALLBACK ums_veh(EXCEPTION_POINTERS *ep)
{
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_active || g_tid != GetCurrentThreadId()) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x4001000a || c == 0x406d1388 || c == 0x40010005 || c == 0x4001000b) return EXCEPTION_CONTINUE_SEARCH;
    g_code = c;
    g_active = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

typedef struct { int op, fam, v64; ULONG *flags; ULONG start; void *obj; unsigned char *at; ULONG rsize; unsigned char *rptr; } UCall;

static void do_call(UCall *k)
{
    FARPROC f = fp(k->fam, k->v64, k->op);
    switch (k->op) {
    case 0: k->rsize = ((size_fn)f)(k->flags, k->start, k->obj); break;
    case 1: case 2: k->rptr = ((marsh_fn)f)(k->flags, k->at, k->obj); break;
    case 3: ((free_fn)f)(k->flags, k->obj); break;
    }
}

static DWORD run_call(UCall *k)
{
    static PVOID h;
    if (!h) h = AddVectoredExceptionHandler(1, ums_veh);
    g_code = 0; g_tid = GetCurrentThreadId();
    if (!setjmp(g_jb)) {
        g_active = 1;
        do_call(k);
        g_active = 0;
        return 0;
    }
    return g_code;
}

static DWORD finish(UCall *k, DWORD *exc, const char *what)
{
    DWORD code = run_call(k);
    if (exc) *exc = code;
    else if (code) t_check(0, __FILE__, __LINE__, what, "unexpected exception 0x%lx", code);
    return code;
}

ULONG u_size(UCtx *c, int fam, int v64, ULONG start, void *obj, DWORD *exc)
{
    UCall k; memset(&k, 0, sizeof k);
    k.op = 0; k.fam = fam; k.v64 = v64; k.flags = &c->umcb.Flags; k.start = start; k.obj = obj;
    return finish(&k, exc, "UserSize") ? 0 : k.rsize;
}
unsigned char *u_marshal(UCtx *c, int fam, int v64, unsigned char *at, void *obj, DWORD *exc)
{
    UCall k; memset(&k, 0, sizeof k);
    k.op = 1; k.fam = fam; k.v64 = v64; k.flags = &c->umcb.Flags; k.at = at; k.obj = obj;
    return finish(&k, exc, "UserMarshal") ? NULL : k.rptr;
}
unsigned char *u_unmarshal(UCtx *c, int fam, int v64, unsigned char *at, void *obj, DWORD *exc)
{
    UCall k; memset(&k, 0, sizeof k);
    k.op = 2; k.fam = fam; k.v64 = v64; k.flags = &c->umcb.Flags; k.at = at; k.obj = obj;
    return finish(&k, exc, "UserUnmarshal") ? NULL : k.rptr;
}
void u_free(UCtx *c, int fam, int v64, void *obj, DWORD *exc)
{
    UCall k; memset(&k, 0, sizeof k);
    k.op = 3; k.fam = fam; k.v64 = v64; k.flags = &c->umcb.Flags; k.obj = obj;
    finish(&k, exc, "UserFree");
}

typedef unsigned char *(WINAPI *sa_m_fn)(ULONG *, unsigned char *, LPSAFEARRAY *);
typedef unsigned char *(WINAPI *sa_u_fn)(ULONG *, unsigned char *, LPSAFEARRAY *, GUID *);
static unsigned char *sa_exp(UCtx *c, const char *name, unsigned char *at, LPSAFEARRAY *p, DWORD *exc)
{
    static UCall k;
    FARPROC f = t_proc_in("oleaut32.dll", name);
    DWORD code;
    unsigned char *r = NULL;
    static PVOID h;
    if (!f) { if (exc) *exc = 0xfeedf00d; return NULL; }
    if (!h) h = AddVectoredExceptionHandler(1, ums_veh);
    g_code = 0; g_tid = GetCurrentThreadId();
    if (!setjmp(g_jb)) {
        g_active = 1;
        if (!strcmp(name, "LPSAFEARRAY_Marshal")) r = ((sa_m_fn)f)(&c->umcb.Flags, at, p);
        else r = ((sa_u_fn)f)(&c->umcb.Flags, at, p, NULL);
        g_active = 0;
        code = 0;
    } else { code = g_code; r = NULL; }
    if (exc) *exc = code;
    else if (code) t_check(0, __FILE__, __LINE__, name, "unexpected exception 0x%lx", code);
    return r;
}
unsigned char *u_sa_marshal_exp(UCtx *c, unsigned char *at, LPSAFEARRAY *p, DWORD *exc) { return sa_exp(c, "LPSAFEARRAY_Marshal", at, p, exc); }
unsigned char *u_sa_unmarshal_exp(UCtx *c, unsigned char *at, LPSAFEARRAY *p, DWORD *exc) { return sa_exp(c, "LPSAFEARRAY_Unmarshal", at, p, exc); }

/* ---- test object ------------------------------------------------------------------------------------------- */
static HRESULT WINAPI uo_qi(IUnknown *i, REFIID riid, void **ppv)
{
    static const GUID iid_unk = {0,0,0,{0xC0,0,0,0,0,0,0,0x46}};
    if (IsEqualIID(riid, &iid_unk)) { *ppv = i; IUnknown_AddRef(i); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI uo_addref(IUnknown *i) { return InterlockedIncrement(&((UObj *)i)->refs); }
static ULONG WINAPI uo_release(IUnknown *i)
{
    UObj *o = (UObj *)i; LONG r = InterlockedDecrement(&o->refs);
    if (!r) free(o);
    return r;
}
static IUnknownVtbl uo_vtbl = { uo_qi, uo_addref, uo_release };
UObj *uobj_new(void) { UObj *o = calloc(1, sizeof *o); o->iface.lpVtbl = &uo_vtbl; o->refs = 1; return o; }

void ums_vfill(VARIANT *v, VARTYPE vt, const void *payload, size_t n, unsigned short r1, unsigned short r2, unsigned short r3)
{
    memset(v, 0, sizeof *v);
    V_VT(v) = vt;
    v->wReserved1 = r1; v->wReserved2 = r2; v->wReserved3 = r3;
    if (payload && n) memcpy((unsigned char *)v + 8, payload, n);
}

/* ---- wire models ---------------------------------------------------------------------------------------------- */
#define W32(p, v) do { if (out) wr32(out + (p), (v)); } while (0)
#define W16(p, v) do { if (out) wr16(out + (p), (v)); } while (0)
#define W64(p, v) do { if (out) wr64(out + (p), (v)); } while (0)
#define MASK(p, n) do { if (mask) memset(mask + (p), 1, (n)); } while (0)

ULONG ums_model_bstr(int v64, BSTR b, ULONG pos, unsigned char *out, unsigned char *mask)
{
    ULONG bl = b ? SysStringByteLen(b) : 0;
    ULONG cnt = (bl + 1) >> 1;
    ULONG p = ALIGN_UP(pos, v64 ? 8 : 4);
    if (v64) {
        W64(p, cnt); W32(p + 8, b ? bl : 0xffffffffu); W32(p + 12, cnt);
        if (cnt && out) memcpy(out + p + 16, b, cnt * 2);
        return p + 16 + cnt * 2;
    }
    W32(p, cnt); W32(p + 4, b ? bl : 0xffffffffu); W32(p + 8, cnt);
    if (cnt && out) memcpy(out + p + 12, b, cnt * 2);
    return p + 12 + cnt * 2;
}

ULONG ums_model_variant(int v64, const VARIANT *v, ULONG pos, unsigned char *out, unsigned char *mask)
{
    ULONG p0 = ALIGN_UP(pos, 8), p, end, marker = v64 ? 8 : 4;
    VARTYPE vt = V_VT(v);
    VARTYPE sw = (vt & 0x2000) ? (vt & 0x6000) : vt;
    int byref = (sw & 0x4000) != 0;
    VARTYPE bt = sw & ~0x4000;
    const unsigned char *src = (const unsigned char *)v + 8;
    ULONG psz = 0; /* plain payload size */

    W32(p0 + 4, 0);
    W16(p0 + 8, vt); W16(p0 + 10, v->wReserved1); W16(p0 + 12, v->wReserved2); W16(p0 + 14, v->wReserved3);
    W32(p0 + 16, sw);
    if (byref) {
        if (!V_BYREF(v)) return 0;
        src = V_BYREF(v);
        p = p0 + (v64 ? 32 : 24);
    } else
        p = p0 + (v64 ? 24 : 20);

    switch (bt) {
    case VT_EMPTY: case VT_NULL: if (byref) return 0; end = p; break;
    case VT_I1: case VT_UI1: psz = 1; goto plain;
    case VT_I2: case VT_UI2: case VT_BOOL: psz = 2; goto plain;
    case VT_I4: case VT_UI4: case VT_R4: case VT_ERROR: case VT_INT: case VT_UINT: psz = 4; goto plain;
    case VT_R8: case VT_CY: case VT_DATE: psz = 8; marker = 8; p = ALIGN_UP(p, 8); goto plain;
    case VT_I8: case VT_UI8: psz = 8; p = ALIGN_UP(p, 8); goto plain;
    case VT_DECIMAL:
        if (!byref) src = (const unsigned char *)v;
        psz = 16; marker = 0x10; p = ALIGN_UP(p, 8);
    plain:
        if (out) memcpy(out + p, src, psz);
        end = p + psz;
        break;
    case VT_BSTR: {
        BSTR b = *(BSTR *)src;
        if (!b) { if (v64) { W64(p, 0); end = p + 8; } else { W32(p, 0); end = p + 4; } }
        else if (v64) { W64(p, (unsigned long long)(ULONG_PTR)b); MASK(p, 8); end = ums_model_bstr(1, b, p + 8, out, mask); }
        else { W32(p, (unsigned int)((ULONG_PTR)b | 1)); MASK(p, 4); end = ums_model_bstr(0, b, p + 4, out, mask); }
        break;
    }
    case VT_VARIANT:
        if (!byref) return 0;
        marker = 0x18;
        if (v64) { W64(p, 0x72657355); end = ums_model_variant(1, (const VARIANT *)src, p + 8, out, mask); }
        else { W32(p, 0x72657355); end = ums_model_variant(0, (const VARIANT *)src, p + 4, out, mask); }
        break;
    case VT_UNKNOWN: case VT_DISPATCH:
        if (*(void **)src) return 0;
        if (v64) { W64(p, 0); end = p + 8; } else { W32(p, 0); end = p + 4; }
        break;
    default:
        if (bt & 0x2000) {
            SAFEARRAY *psa = *(SAFEARRAY **)src;
            if (!psa) { if (v64) { W64(p, 0); end = p + 8; } else { W32(p, 0); end = p + 4; } }
            else {
                if (v64) { W64(p, (unsigned long long)(ULONG_PTR)psa); MASK(p, 8); end = ums_model_safearray(1, psa, p + 8, out, mask); }
                else { W32(p, (unsigned int)((ULONG_PTR)psa | 1)); MASK(p, 4); end = ums_model_safearray(0, psa, p + 4, out, mask); }
                if (!end) return 0;
            }
            break;
        }
        return 0;
    }
    if (byref) { if (v64) W64(p0 + 24, marker); else W32(p0 + 20, marker); }
    W32(p0, (end - p0 + 7) >> 3);
    return end;
}

ULONG ums_sftype(SAFEARRAY *psa)
{
    USHORT f = psa->fFeatures;
    if (f & 0x20) return 0x24;
    switch (psa->cbElements) {
    case 1: return 0x10;
    case 2: return 2;
    case 4: return 3;
    case 8:
        switch (f & 0xf40) {
        case 0x100: return 8;
        case 0x200: return 0xd;
        case 0x240: return 0x800d;
        case 0x400: return 9;
        case 0x440: return 0x800d;
        default: return 0x14;
        }
    case 0x18: if ((f & 0xf00) == 0x800) return 0xc; return 0;
    }
    return 0;
}

static ULONG sa_cells(SAFEARRAY *psa)
{
    ULONG n = 1; USHORT i;
    for (i = 0; i < psa->cDims; i++) n *= psa->rgsabound[i].cElements;
    return n;
}

ULONG ums_model_safearray(int v64, SAFEARRAY *psa, ULONG pos, unsigned char *out, unsigned char *mask)
{
    ULONG q = ALIGN_UP(pos, v64 ? 8 : 4), r, cells, sf, wel, d, e;
    VARTYPE vt = 0; ULONG lv;
    int i;
    if (!psa) { if (v64) { W64(q, 0); return q + 8; } W32(q, 0); return q + 4; }
    sf = ums_sftype(psa);
    if (!sf) return 0;
    cells = sa_cells(psa);
    if (psa->fFeatures & 0x80) SafeArrayGetVartype(psa, &vt);
    lv = psa->cLocks | ((psa->fFeatures & 0x80) ? ((ULONG)vt << 16) : 0);
    wel = psa->cbElements;
    if (sf == 8 || sf == 9 || sf == 0xd || sf == 0x800d) wel = 4;
    else if (sf == 0xc) wel = 0x10;
    if (sf == 0x24) return 0;
    if (v64) {
        W64(q, 1); W64(q + 8, psa->cDims); W16(q + 16, psa->cDims); W16(q + 18, psa->fFeatures); W32(q + 20, wel);
        W32(q + 24, lv); W32(q + 32, sf); W64(q + 40, cells); W64(q + 48, (unsigned long long)(ULONG_PTR)psa->pvData);
        MASK(q + 48, 8);
        r = q + 56;
    } else {
        W32(q, 1); W32(q + 4, psa->cDims); W16(q + 8, psa->cDims); W16(q + 10, psa->fFeatures); W32(q + 12, wel);
        W32(q + 16, lv); W32(q + 20, sf); W32(q + 24, cells); W32(q + 28, (unsigned int)(ULONG_PTR)psa->pvData);
        MASK(q + 28, 4);
        r = q + 32;
    }
    if (sf == 0x800d) {
        GUID g; if (FAILED(SafeArrayGetIID(psa, &g))) return 0;
        if (out) memcpy(out + r, &g, 16);
        r += 16; sf = 0xd;
    }
    for (i = psa->cDims - 1; i >= 0; i--) {
        W32(r, psa->rgsabound[i].cElements); W32(r + 4, psa->rgsabound[i].lLbound); r += 8;
    }
    if (v64) { W64(r, cells); d = r + 8; } else { W32(r, cells); d = r + 4; }
    if (sf == 0x10 || sf == 2 || sf == 3 || sf == 0x14) {
        if (sf == 0x14 && !v64) d = ALIGN_UP(d, 8);
        if (out) memcpy(out + d, psa->pvData, psa->cbElements * cells);
        return d + psa->cbElements * cells;
    }
    {
        ULONG step = v64 ? 8 : 4, k;
        unsigned char *data = psa->pvData;
        e = d + step * cells;
        for (k = 0; k < cells; k++) {
            if (sf == 8) {
                BSTR b = ((BSTR *)data)[k];
                if (v64) W64(d + 8 * k, b ? 1 : 0); else W32(d + 4 * k, b ? 1 : 0);
                if (b) e = ums_model_bstr(v64, b, e, out, mask);
            } else if (sf == 0xc) {
                if (v64) W64(d + 8 * k, 0x72657355); else W32(d + 4 * k, 0x72657355);
                e = ums_model_variant(v64, (VARIANT *)(data + k * sizeof(VARIANT)), e, out, mask);
                if (!e) return 0;
            } else if (sf == 9 || sf == 0xd) {
                if (((void **)data)[k]) return 0;
                if (v64) W64(d + 8 * k, 0); else W32(d + 4 * k, 0);
            } else
                return 0;
        }
        return e;
    }
}

ULONG ums_size_bstr(int v64, BSTR b, ULONG start)
{
    ULONG bl = b ? SysStringByteLen(b) : 0;
    return ALIGN_UP(start, v64 ? 8 : 4) + (v64 ? 16 : 12) + ALIGN_UP(bl, 2);
}

ULONG ums_size_safearray(int v64, SAFEARRAY *psa, ULONG start)
{
    static unsigned char scratch[1 << 16];
    ULONG s = ALIGN_UP(start, v64 ? 8 : 4) + (v64 ? 8 : 4), sf, cells, k;
    if (!psa) return s;
    s += v64 ? 0x2c : 0x28;
    sf = ums_sftype(psa);
    if (sf == 0x800d) { s += 0x10; sf = 0xd; }
    s += v64 ? psa->cDims * 8 : psa->cDims * 8 + 4;
    if (v64) { s = ALIGN_UP(s, 8); s += 8; }
    cells = sa_cells(psa);
    if (sf == 2 || sf == 3 || sf == 0x10 || sf == 0x14) {
        if (sf == 0x14) s = ALIGN_UP(s, 8);
        return s + psa->cbElements * cells;
    }
    s += (v64 ? 8 : 4) * cells;
    for (k = 0; k < cells; k++) {
        unsigned char *data = psa->pvData;
        if (sf == 8) { BSTR b = ((BSTR *)data)[k]; if (b) s = ums_size_bstr(v64, b, s); }
        else if (sf == 0xc) {
            ULONG e = ums_model_variant(v64, (VARIANT *)(data + k * sizeof(VARIANT)), s, scratch, NULL);
            if (e) s = e;
        }
    }
    return s;
}
