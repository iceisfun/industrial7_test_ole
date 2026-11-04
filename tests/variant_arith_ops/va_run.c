#include "va.h"

static const char *opnames[] = {"VarAdd","VarSub","VarMul","VarDiv","VarIdiv","VarMod","VarPow","VarAnd","VarOr","VarXor","VarEqv","VarImp","VarCat","VarAbs","VarNot","VarNeg","VarInt","VarFix","VarRound"};
const char *va_opname(int op) { return opnames[op]; }

const char *va_vtname(unsigned vt)
{
    static char buf[4][32]; static int k;
    char *b = buf[k++ & 3];
    static const char *n[] = {"EMPTY","NULL","I2","I4","R4","R8","CY","DATE","BSTR","DISPATCH","ERROR","BOOL","VARIANT","UNKNOWN","DECIMAL","15","I1","UI1","UI2","UI4","I8","UI8","INT","UINT","VOID","HRESULT"};
    unsigned base = vt & 0xfff;
    if (base < sizeof(n)/sizeof(n[0])) sprintf(b, "%s%s%s%s", (vt & VT_ARRAY) ? "ARRAY|" : "", (vt & VT_BYREF) ? "BYREF|" : "", (vt & VT_VECTOR) ? "VEC|" : "", n[base]);
    else sprintf(b, "vt%u", vt);
    return b;
}

void va_make(VARIANT *v, const vcell *c)
{
    memset(v, 0, sizeof(*v));
    V_VT(v) = c->vt;
    switch (c->vt) {
    case VT_I1: V_I1(v) = (CHAR)c->i; break;
    case VT_UI1: V_UI1(v) = (BYTE)c->i; break;
    case VT_I2: V_I2(v) = (SHORT)c->i; break;
    case VT_UI2: V_UI2(v) = (USHORT)c->i; break;
    case VT_I4: case VT_INT: V_I4(v) = (LONG)c->i; break;
    case VT_UI4: case VT_UINT: V_UI4(v) = (ULONG)c->i; break;
    case VT_I8: V_I8(v) = c->i; break;
    case VT_UI8: V_UI8(v) = (ULONGLONG)c->i; break;
    case VT_BOOL: V_BOOL(v) = (VARIANT_BOOL)c->i; break;
    case VT_ERROR: V_ERROR(v) = (SCODE)c->i; break;
    case VT_R4: V_R4(v) = (float)c->d; break;
    case VT_R8: V_R8(v) = c->d; break;
    case VT_DATE: V_DATE(v) = c->d; break;
    case VT_CY: V_CY(v).int64 = c->i; break;
    case VT_DECIMAL: { double k = c->d; DECIMAL d; memset(&d, 0, sizeof(d));
        d.scale = (BYTE)((unsigned long long)k & 0xff); d.sign = (BYTE)(((unsigned long long)k >> 8) & 0xff); d.Hi32 = (ULONG)((unsigned long long)k >> 16); d.Lo64 = (ULONGLONG)c->i;
        V_DECIMAL(v) = d; V_VT(v) = VT_DECIMAL; break; }
    case VT_BSTR: V_BSTR(v) = c->s ? U(SysAllocString)(c->s) : NULL; break;
    case VT_EMPTY: case VT_NULL: break;
    default: V_I4(v) = (LONG)c->i; break;
    }
}
void va_free(VARIANT *v)
{
    if (V_VT(v) == VT_BSTR) U(SysFreeString)(V_BSTR(v));
    memset(v, 0, sizeof(*v));
}

static int eqd(double a, double b, double tol)
{
    if (a == b) return 1;
    return fabs(a - b) / (1.0 + fabs(a) + fabs(b)) < tol;
}

int va_same(const VARIANT *r, const VARIANT *e)
{
    if (V_VT(r) != V_VT(e)) return 0;
    switch (V_VT(e)) {
    case VT_EMPTY: case VT_NULL: return 1;
    case VT_BOOL: return V_BOOL(r) == V_BOOL(e);
    case VT_I1: return V_I1(r) == V_I1(e);
    case VT_UI1: return V_UI1(r) == V_UI1(e);
    case VT_I2: return V_I2(r) == V_I2(e);
    case VT_UI2: return V_UI2(r) == V_UI2(e);
    case VT_I4: case VT_INT: return V_I4(r) == V_I4(e);
    case VT_UI4: case VT_UINT: return V_UI4(r) == V_UI4(e);
    case VT_I8: return V_I8(r) == V_I8(e);
    case VT_UI8: return V_UI8(r) == V_UI8(e);
    case VT_ERROR: return V_ERROR(r) == V_ERROR(e);
    case VT_R4: return eqd(V_R4(r), V_R4(e), 1e-7);
    case VT_R8: return eqd(V_R8(r), V_R8(e), 1e-7);
    case VT_DATE: return eqd(V_DATE(r), V_DATE(e), 1e-7);
    case VT_CY: return V_CY(r).int64 == V_CY(e).int64;
    case VT_BSTR:
        if (!V_BSTR(r) || !V_BSTR(e)) return V_BSTR(r) == V_BSTR(e);
        return !lstrcmpW(V_BSTR(r), V_BSTR(e));
    case VT_DECIMAL: return !memcmp(&V_DECIMAL(r), &V_DECIMAL(e), sizeof(DECIMAL));
    default: return V_I4(r) == V_I4(e);
    }
}

int va_equal_cell(const VARIANT *v, const vcell *c)
{
    VARIANT e; int ok;
    va_make(&e, c); ok = va_same(v, &e); va_free(&e); return ok;
}

char *va_describe(const VARIANT *v, char *buf, size_t cap)
{
    switch (V_VT(v)) {
    case VT_I1: snprintf(buf, cap, "I1:%d", V_I1(v)); break;
    case VT_UI1: snprintf(buf, cap, "UI1:%u", V_UI1(v)); break;
    case VT_I2: snprintf(buf, cap, "I2:%d", V_I2(v)); break;
    case VT_UI2: snprintf(buf, cap, "UI2:%u", V_UI2(v)); break;
    case VT_I4: case VT_INT: snprintf(buf, cap, "%s:%ld", va_vtname(V_VT(v)), V_I4(v)); break;
    case VT_UI4: case VT_UINT: snprintf(buf, cap, "%s:%lu", va_vtname(V_VT(v)), V_UI4(v)); break;
    case VT_I8: snprintf(buf, cap, "I8:%lld", V_I8(v)); break;
    case VT_UI8: snprintf(buf, cap, "UI8:%llu", V_UI8(v)); break;
    case VT_BOOL: snprintf(buf, cap, "BOOL:%d", V_BOOL(v)); break;
    case VT_R4: snprintf(buf, cap, "R4:%.9g", V_R4(v)); break;
    case VT_R8: snprintf(buf, cap, "R8:%.17g", V_R8(v)); break;
    case VT_DATE: snprintf(buf, cap, "DATE:%.17g", V_DATE(v)); break;
    case VT_CY: snprintf(buf, cap, "CY:%lld", V_CY(v).int64); break;
    case VT_BSTR: snprintf(buf, cap, "BSTR:%s", V_BSTR(v) ? "(str)" : "NULL"); break;
    case VT_ERROR: snprintf(buf, cap, "ERROR:%lx", (long)V_ERROR(v)); break;
    default: snprintf(buf, cap, "%s", va_vtname(V_VT(v))); break;
    }
    return buf;
}

HRESULT va_bin(int op, VARIANT *l, VARIANT *r, VARIANT *res)
{
    switch (op) {
    case VA_ADD: return U(VarAdd)(l, r, res);
    case VA_SUB: return U(VarSub)(l, r, res);
    case VA_MUL: return U(VarMul)(l, r, res);
    case VA_DIV: return U(VarDiv)(l, r, res);
    case VA_IDIV: return U(VarIdiv)(l, r, res);
    case VA_MOD: return U(VarMod)(l, r, res);
    case VA_POW: return U(VarPow)(l, r, res);
    case VA_AND: return U(VarAnd)(l, r, res);
    case VA_OR: return U(VarOr)(l, r, res);
    case VA_XOR: return U(VarXor)(l, r, res);
    case VA_EQV: return U(VarEqv)(l, r, res);
    case VA_IMP: return U(VarImp)(l, r, res);
    case VA_CAT: return U(VarCat)(l, r, res);
    }
    return E_NOTIMPL;
}
HRESULT va_un(int op, VARIANT *v, int ndec, VARIANT *res)
{
    switch (op) {
    case VA_ABS: return U(VarAbs)(v, res);
    case VA_NOT: return U(VarNot)(v, res);
    case VA_NEG: return U(VarNeg)(v, res);
    case VA_INT: return U(VarInt)(v, res);
    case VA_FIX: return U(VarFix)(v, res);
    case VA_ROUND: return U(VarRound)(v, ndec, res);
    }
    return E_NOTIMPL;
}

void va_check_bin(int wl, int op, const vcell *lc, const vcell *rc, HRESULT hr, const vcell *ec)
{
    VARIANT l, r, res, l0, r0;
    HRESULT got;
    char b1[64], b2[64], b3[64];
    va_make(&l, lc); va_make(&r, rc);
    l0 = l; r0 = r;
    memset(&res, 0, sizeof(res));
    V_VT(&res) = 15; V_I4(&res) = 0;
    got = va_bin(op, &l, &r, &res);
    CHECKF(got == hr || (hr == HR_TYPEERR && (got == DISP_E_TYPEMISMATCH || got == DISP_E_BADVARTYPE)), "%s(%s,%s) [wine:%d] hr 0x%lx want 0x%lx", va_opname(op), va_describe(&l0, b1, 64), va_describe(&r0, b2, 64), wl, (long)got, (long)hr);
    if (got == hr || hr == HR_TYPEERR) {
        if (hr == S_OK)
            CHECKF(va_equal_cell(&res, ec), "%s(%s,%s) [wine:%d] result %s want vt %s", va_opname(op), b1, b2, wl, va_describe(&res, b3, 64), va_vtname(ec->vt));
        else
            CHECKF(VA_FAILRES_OK(V_VT(&res)), "%s(%s,%s) [wine:%d] failed result vt %s want EMPTY", va_opname(op), b1, b2, wl, va_vtname(V_VT(&res)));
    }
    CHECKF(!memcmp(&l, &l0, sizeof(l)), "%s [wine:%d] left operand modified", va_opname(op), wl);
    CHECKF(!memcmp(&r, &r0, sizeof(r)), "%s [wine:%d] right operand modified", va_opname(op), wl);
    if (got == S_OK) { if (V_VT(&res) == VT_BSTR) U(SysFreeString)(V_BSTR(&res)); }
    va_free(&l); va_free(&r);
}

void va_check_un(int wl, int op, const vcell *lc, int nd, HRESULT hr, const vcell *ec)
{
    VARIANT l, res, l0;
    HRESULT got;
    char b1[64], b3[64];
    va_make(&l, lc); l0 = l;
    memset(&res, 0, sizeof(res));
    V_VT(&res) = 15; V_I4(&res) = 0;
    got = va_un(op, &l, nd, &res);
    CHECKF(got == hr || (hr == HR_TYPEERR && (got == DISP_E_TYPEMISMATCH || got == DISP_E_BADVARTYPE)), "%s(%s) [wine:%d] hr 0x%lx want 0x%lx", va_opname(op), va_describe(&l0, b1, 64), wl, (long)got, (long)hr);
    if (got == hr || hr == HR_TYPEERR) {
        if (hr == S_OK)
            CHECKF(va_equal_cell(&res, ec), "%s(%s) [wine:%d] result %s want vt %s", va_opname(op), b1, wl, va_describe(&res, b3, 64), va_vtname(ec->vt));
        else
            CHECKF(VA_FAILRES_OK(V_VT(&res)), "%s(%s) [wine:%d] failed result vt %s want EMPTY", va_opname(op), b1, wl, va_vtname(V_VT(&res)));
    }
    CHECKF(!memcmp(&l, &l0, sizeof(l)), "%s [wine:%d] operand modified", va_opname(op), wl);
    if (got == S_OK && V_VT(&res) == VT_BSTR) U(SysFreeString)(V_BSTR(&res));
    va_free(&l);
}

void va_run_binary(int op, const vrow *rows, size_t n, int comm)
{
    size_t i;
    REQUIRE(U(SysAllocString));
    REQUIRE(op == VA_ADD ? (void*)U(VarAdd) : op == VA_SUB ? (void*)U(VarSub) : op == VA_MUL ? (void*)U(VarMul) : op == VA_DIV ? (void*)U(VarDiv) :
            op == VA_IDIV ? (void*)U(VarIdiv) : op == VA_MOD ? (void*)U(VarMod) : op == VA_POW ? (void*)U(VarPow) : op == VA_AND ? (void*)U(VarAnd) :
            op == VA_OR ? (void*)U(VarOr) : op == VA_XOR ? (void*)U(VarXor) : op == VA_EQV ? (void*)U(VarEqv) : (void*)U(VarImp));
    for (i = 0; i < n; i++) {
        va_check_bin(rows[i].wl, op, &rows[i].l, &rows[i].r, rows[i].hr, &rows[i].e);
        if (comm) va_check_bin(rows[i].wl, op, &rows[i].r, &rows[i].l, rows[i].hr, &rows[i].e);
    }
}
void va_run_unary(int op, const vrow *rows, size_t n, int comm)
{
    size_t i;
    REQUIRE(U(SysAllocString));
    REQUIRE(op == VA_ABS ? (void*)U(VarAbs) : op == VA_NOT ? (void*)U(VarNot) : op == VA_NEG ? (void*)U(VarNeg) : op == VA_INT ? (void*)U(VarInt) : (void*)U(VarFix));
    for (i = 0; i < n; i++) va_check_un(rows[i].wl, op, &rows[i].l, 0, rows[i].hr, &rows[i].e);
}
void va_run_round(int op, const vrow *rows, size_t n, int comm)
{
    size_t i;
    REQUIRE(U(SysAllocString)); REQUIRE(U(VarRound));
    for (i = 0; i < n; i++) va_check_un(rows[i].wl, VA_ROUND, &rows[i].l, rows[i].n, rows[i].hr, &rows[i].e);
}

HRESULT va_cmp(const vcell *lc, const vcell *rc, LCID lcid, ULONG flags, unsigned lf, unsigned rf)
{
    VARIANT l, r; HRESULT hr;
    va_make(&l, lc); va_make(&r, rc);
    V_VT(&l) |= lf; V_VT(&r) |= rf;
    hr = U(VarCmp)(&l, &r, lcid, flags);
    V_VT(&l) &= ~lf; V_VT(&r) &= ~rf;
    va_free(&l); va_free(&r);
    return hr;
}

static HRESULT swap_res(HRESULT h) { return h == VARCMP_LT ? VARCMP_GT : h == VARCMP_GT ? VARCMP_LT : h; }

void va_run_cmp(const vcmprow *rows, size_t n)
{
    size_t i;
    REQUIRE(U(VarCmp)); REQUIRE(U(SysAllocString));
    for (i = 0; i < n; i++) {
        const vcmprow *w = &rows[i];
        static const unsigned lf[4] = {0, VT_RESERVED, 0, VT_RESERVED}, rf[4] = {0, 0, VT_RESERVED, VT_RESERVED};
        int k;
        for (k = 0; k < 4; k++) {
            HRESULT h = va_cmp(&w->l, &w->r, LCID_US, 0, lf[k], rf[k]);
            CHECKF(h == w->res[k], "VarCmp(%s,%s) variant %d [wine:%d] got %ld want %ld", va_vtname(w->l.vt), va_vtname(w->r.vt), k, w->wl, (long)h, w->res[k]);
            /* VT_INT is only accepted as left operand on the swapped call */
            if (w->l.vt == VT_INT) continue;
            h = va_cmp(&w->r, &w->l, LCID_US, 0, rf[k], lf[k]);
            CHECKF(h == swap_res(w->res[k]), "VarCmp swapped(%s,%s) variant %d [wine:%d] got %ld want %ld", va_vtname(w->r.vt), va_vtname(w->l.vt), k, w->wl, (long)h, (long)swap_res(w->res[k]));
        }
    }
}
