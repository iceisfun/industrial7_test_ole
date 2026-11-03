/* Result-type promotion matrices: every pair of operand types from the supported set is run through
 * VarAdd/VarSub/VarMul/VarDiv/VarIdiv/VarPow and the result vt (and numeric value for the sample
 * operands 2 / "2" / VARIANT_TRUE / EMPTY) is checked against the promotion model.
 * Model adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) type loops; failure paths are
 * checked with the decompiled oleaut32 failure protocol (result vt is VT_EMPTY after any failure). */
#include "va.h"

static const unsigned short VTS[] = { VT_EMPTY, VT_NULL, VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BSTR, VT_BOOL,
    VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
#define NVTS (sizeof(VTS)/sizeof(VTS[0]))

static void sample(unsigned vt, vcell *c, double *val)
{
    static const WCHAR two[] = {'2',0};
    memset(c, 0, sizeof(*c)); c->vt = (unsigned short)vt; *val = 2;
    switch (vt) {
    case VT_EMPTY: case VT_NULL: *val = 0; break;
    case VT_BOOL: c->i = -1; *val = -1; break;
    case VT_R4: case VT_R8: case VT_DATE: c->d = 2; break;
    case VT_CY: c->i = 20000; break;
    case VT_BSTR: c->s = two; break;
    case VT_DECIMAL: c->i = 2; c->d = 0; break;
    default: c->i = 2; break;
    }
}

static double to_double(const VARIANT *v, int *ok)
{
    *ok = 1;
    switch (V_VT(v)) {
    case VT_I1: return V_I1(v); case VT_UI1: return V_UI1(v); case VT_I2: return V_I2(v); case VT_UI2: return V_UI2(v);
    case VT_I4: case VT_INT: return V_I4(v); case VT_UI4: case VT_UINT: return V_UI4(v);
    case VT_I8: return (double)V_I8(v); case VT_UI8: return (double)V_UI8(v);
    case VT_BOOL: return V_BOOL(v); case VT_R4: return V_R4(v); case VT_R8: return V_R8(v); case VT_DATE: return V_DATE(v);
    case VT_CY: return V_CY(v).int64 / 10000.0;
    case VT_DECIMAL: { static const double p10[] = {1,10,100,1000,1e4,1e5,1e6,1e7,1e8,1e9};
        double m = (double)V_DECIMAL(v).Lo64 + (double)V_DECIMAL(v).Hi32 * 18446744073709551616.0;
        return (V_DECIMAL(v).sign ? -m : m) / (V_DECIMAL(v).scale < 10 ? p10[V_DECIMAL(v).scale] : 1e9); }
    }
    *ok = 0; return 0;
}

enum { M_ADD, M_SUB, M_MUL, M_DIV, M_IDIV, M_POW };
#define HAS(a) (l == (a) || r == (a))
/* returns 0 = success with *rv, else failure kind: 1 = BADVARTYPE, 2 = TYPEMISMATCH, 3 = either of the divide failures */
static int model(int op, unsigned l, unsigned r, unsigned *rv)
{
    *rv = VT_EMPTY;
    if (op == M_ADD || op == M_SUB || op == M_MUL) {
        if (HAS(VT_I1) || HAS(VT_UI2) || HAS(VT_UI4) || HAS(VT_UI8) || HAS(VT_INT) || HAS(VT_UINT)) return 1;
        if (HAS(VT_NULL)) { *rv = VT_NULL; return 0; }
        if (HAS(VT_DECIMAL)) { *rv = VT_DECIMAL; return 0; }
        if (op == M_MUL) {
            if (HAS(VT_R8) || HAS(VT_BSTR) || HAS(VT_DATE)) *rv = VT_R8;
            else if (HAS(VT_R4)) *rv = (HAS(VT_I4) || HAS(VT_I8) || HAS(VT_CY)) ? VT_R8 : VT_R4;
            else if (HAS(VT_CY)) *rv = VT_CY;
            else if (HAS(VT_I8)) *rv = VT_I8;
            else if (HAS(VT_I4)) *rv = VT_I4;
            else if (HAS(VT_I2) || HAS(VT_BOOL) || (l == VT_EMPTY && r == VT_EMPTY)) *rv = VT_I2;
            else if (HAS(VT_UI1)) *rv = VT_UI1;
            return 0;
        }
        if (op == M_SUB && ((l == VT_EMPTY && r == VT_BSTR) || (l == VT_DATE && r == VT_DATE) || (l == VT_BSTR && r == VT_EMPTY) || (l == VT_BSTR && r == VT_BSTR)))
            { *rv = VT_R8; return 0; }
        if (HAS(VT_DATE)) *rv = VT_DATE;
        else if (HAS(VT_CY)) *rv = VT_CY;
        else if (HAS(VT_R8)) *rv = VT_R8;
        else if (HAS(VT_BSTR)) *rv = (op == M_ADD && (l == VT_BSTR && r == VT_BSTR || l == VT_EMPTY || r == VT_EMPTY)) ? VT_BSTR : VT_R8;
        else if (HAS(VT_R4)) *rv = (HAS(VT_I4) || HAS(VT_I8)) ? VT_R8 : VT_R4;
        else if (HAS(VT_I8)) *rv = VT_I8;
        else if (HAS(VT_I4)) *rv = VT_I4;
        else if (HAS(VT_I2) || HAS(VT_BOOL) || (l == VT_EMPTY && r == VT_EMPTY)) *rv = VT_I2;
        else if (HAS(VT_UI1)) *rv = VT_UI1;
        return 0;
    }
    if (op == M_DIV) {
        if (HAS(VT_I1) || HAS(VT_UI2) || HAS(VT_UI4) || HAS(VT_UI8) || HAS(VT_INT) || HAS(VT_UINT)) return -1; /* not modelled */
        if (r != VT_EMPTY) {
            if (HAS(VT_NULL)) *rv = VT_NULL;
            else if (HAS(VT_DECIMAL)) *rv = VT_DECIMAL;
            else if (HAS(VT_I8) || HAS(VT_CY) || HAS(VT_DATE) || HAS(VT_I4) || HAS(VT_BSTR) || HAS(VT_I2) || HAS(VT_BOOL) || HAS(VT_R8) || HAS(VT_UI1)) {
                if ((l == VT_UI1 && r == VT_R4) || (l == VT_R4 && r == VT_UI1)) *rv = VT_R4;
                else if ((l == VT_R4 && (r == VT_BOOL || r == VT_I2)) || (r == VT_R4 && (l == VT_BOOL || l == VT_I2))) *rv = VT_R4;
                else *rv = VT_R8;
            } else if (HAS(VT_R4)) *rv = VT_R4;
            return 0;
        }
        if (l == VT_NULL) { *rv = VT_NULL; return 0; }
        return 3;
    }
    if (op == M_IDIV) {
        if ((l == VT_I8 && r == VT_INT) || (l == VT_INT && r == VT_I8) || (r == VT_EMPTY && l != VT_NULL)) return 3;
        if (HAS(VT_NULL)) *rv = VT_NULL;
        else if (HAS(VT_I8)) *rv = VT_I8;
        else if (HAS(VT_I4) || HAS(VT_INT) || HAS(VT_UINT) || HAS(VT_UI8) || HAS(VT_UI4) || HAS(VT_UI2) || HAS(VT_I1) || HAS(VT_BSTR) || HAS(VT_DATE) ||
                 HAS(VT_CY) || HAS(VT_DECIMAL) || HAS(VT_R8) || HAS(VT_R4)) *rv = VT_I4;
        else if (HAS(VT_I2) || HAS(VT_BOOL) || l == VT_EMPTY) *rv = VT_I2;
        else if (HAS(VT_UI1)) *rv = VT_UI1;
        else return 3;
        return 0;
    }
    /* M_POW */
    if (HAS(VT_NULL)) *rv = VT_NULL; else *rv = VT_R8;
    return 0;
}

static const char *mname[] = {"VarAdd","VarSub","VarMul","VarDiv","VarIdiv","VarPow"};
static const int vaop[] = {VA_ADD, VA_SUB, VA_MUL, VA_DIV, VA_IDIV, VA_POW};

static void run_matrix(int m)
{
    size_t a, b;
    for (a = 0; a < NVTS; a++) for (b = 0; b < NVTS; b++) {
        unsigned l = VTS[a], r = VTS[b], rv;
        vcell lc, rc; double lval, rval;
        VARIANT L, R, res; HRESULT hr; int kind;
        kind = model(m, l, r, &rv);
        if (kind < 0) continue;
        sample(l, &lc, &lval); sample(r, &rc, &rval);
        /* Wine's VarIdiv raises an integer divide exception for a zero (EMPTY) divisor; Windows returns DISP_E_DIVBYZERO */
        if (t_is_wine() && m == M_IDIV && r == VT_EMPTY && l != VT_NULL) continue;
        va_make(&L, &lc); va_make(&R, &rc);
        memset(&res, 0, sizeof(res)); V_VT(&res) = 15;
        hr = va_bin(vaop[m], &L, &R, &res);
        if (kind == 0) {
            int ok; double got, want = 0; int check = 1;
            CHECKF(hr == S_OK, "%s(%s,%s) hr 0x%lx want S_OK", mname[m], va_vtname(l), va_vtname(r), (long)hr);
            if (hr != S_OK) { va_free(&L); va_free(&R); continue; }
            CHECKF(V_VT(&res) == rv, "%s(%s,%s) result vt %s want %s", mname[m], va_vtname(l), va_vtname(r), va_vtname(V_VT(&res)), va_vtname(rv));
            if (rv == VT_UI1 && m == M_SUB && lval - rval < 0) {
                t_note("%s(%s,%s): unsigned byte underflow result not asserted", mname[m], va_vtname(l), va_vtname(r));
                check = 0;
            }
            if (HAS(VT_BOOL) && HAS(VT_DECIMAL)) {
                t_note("%s(%s,%s): BOOL TRUE coerced to DECIMAL (-1 expected) value not asserted", mname[m], va_vtname(l), va_vtname(r));
                check = 0;
            }
            switch (m) {
            case M_ADD: want = lval + rval; break; case M_SUB: want = lval - rval; break; case M_MUL: want = lval * rval; break;
            case M_DIV: want = rval != 0 ? lval / rval : 0; break;
            case M_IDIV: want = (rv != VT_NULL && rval != 0) ? (double)((long long)lval / (long long)rval) : 0; break;
            case M_POW: if (lval == 0 && rval < 0) check = 0; else want = pow(lval, rval); break;
            }
            if (m == M_ADD && rv == VT_BSTR) {
                /* "2"+"2" -> "22"; EMPTY+"2" -> "2"; "2"+EMPTY -> "2" */
                const WCHAR *w = V_BSTR(&res); const WCHAR *exp = (l == VT_BSTR && r == VT_BSTR) ? L"22" : L"2";
                CHECKF(w && !lstrcmpW(w, exp), "%s(%s,%s) BSTR result wrong", mname[m], va_vtname(l), va_vtname(r));
            } else if (rv != VT_NULL && check) {
                got = to_double(&res, &ok);
                if (m == M_SUB && rv == VT_R8 && l == VT_DATE) { /* DATE-DATE is plain double */ }
                CHECKF(ok && fabs(got - want) < 1e-9, "%s(%s,%s) value %g want %g", mname[m], va_vtname(l), va_vtname(r), got, want);
            }
            if (V_VT(&res) == VT_BSTR) U(SysFreeString)(V_BSTR(&res));
        } else {
            int okh;
            if (kind == 1) okh = (hr == DISP_E_BADVARTYPE);
            else if (kind == 2) okh = (hr == DISP_E_TYPEMISMATCH);
            else okh = (hr == DISP_E_BADVARTYPE || hr == DISP_E_TYPEMISMATCH || hr == DISP_E_DIVBYZERO || hr == DISP_E_OVERFLOW);
            if (m <= M_MUL && kind == 1) okh = (hr == DISP_E_BADVARTYPE || hr == DISP_E_TYPEMISMATCH);
            CHECKF(okh, "%s(%s,%s) failure hr 0x%lx", mname[m], va_vtname(l), va_vtname(r), (long)hr);
            CHECKF(V_VT(&res) == VT_EMPTY, "%s(%s,%s) failed result vt %s want EMPTY", mname[m], va_vtname(l), va_vtname(r), va_vtname(V_VT(&res)));
        }
        va_free(&L); va_free(&R);
    }
}

TEST(variant_arith_ops_matrix_add) { REQUIRE(U(VarAdd)); REQUIRE(U(SysAllocString)); run_matrix(M_ADD); }
TEST(variant_arith_ops_matrix_sub) { REQUIRE(U(VarSub)); REQUIRE(U(SysAllocString)); run_matrix(M_SUB); }
TEST(variant_arith_ops_matrix_mul) { REQUIRE(U(VarMul)); REQUIRE(U(SysAllocString)); run_matrix(M_MUL); }
TEST(variant_arith_ops_matrix_div) { REQUIRE(U(VarDiv)); REQUIRE(U(SysAllocString)); run_matrix(M_DIV); }
TEST(variant_arith_ops_matrix_idiv) { REQUIRE(U(VarIdiv)); REQUIRE(U(SysAllocString)); run_matrix(M_IDIV); }
TEST(variant_arith_ops_matrix_pow) { REQUIRE(U(VarPow)); REQUIRE(U(SysAllocString)); run_matrix(M_POW); }
