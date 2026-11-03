/* Currency / Decimal / R8 arithmetic bodies adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+).
 * The statements are Wine's operand/result data; all calls go through GetProcAddress (U()) and the
 * expectations kept are those that hold on native Windows. */
#include "va.h"
#define ok(c, ...) t_check(!!(c), __FILE__, __LINE__, #c, __VA_ARGS__)
#define CY_MULTIPLIER 10000
#define _EXPECT_NO_OUT(res)     ok(hres == res, "expected " #res ", got hres=%#08lx\n", hres)
#define EXPECT_OVERFLOW _EXPECT_NO_OUT(DISP_E_OVERFLOW)
#define EXPECT_INVALID  _EXPECT_NO_OUT(E_INVALIDARG)
#define EXPECT_LT       _EXPECT_NO_OUT(VARCMP_LT)
#define EXPECT_GT       _EXPECT_NO_OUT(VARCMP_GT)
#define EXPECT_EQ       _EXPECT_NO_OUT(VARCMP_EQ)
#define EXPECTCY(x) \
  ok((hres == S_OK && out.int64 == (LONGLONG)(x*CY_MULTIPLIER)), \
     "expected " #x "*CY_MULTIPLIER, got (%8lx %8lx); hres=0x%08lx\n", out.Hi, out.Lo, hres)
#define EXPECTCY64(x,y) \
  ok(hres == S_OK && out.Hi == (LONG)x && out.Lo == y, \
     "expected " #x " " #y " (%lu,%lu), got (%lu,%lu); hres=0x%08lx\n", (ULONG)(x), (ULONG)(y), out.Hi, out.Lo, hres)
#define SETDEC(dec, scl, sgn, hi, lo) dec.scale = (BYTE)scl; dec.sign = (BYTE)sgn; dec.Hi32 = (ULONG)hi; dec.Lo64 = (ULONG64)lo
#define SETDEC64(dec, scl, sgn, hi, mid, lo) dec.scale = (BYTE)scl; dec.sign = (BYTE)sgn; dec.Hi32 = (ULONG)hi; dec.Mid32 = mid; dec.Lo32 = lo;
#define EXPECTDEC(scl, sgn, hi, lo) ok(hres == S_OK && \
  out.scale == (BYTE)(scl) && out.sign == (BYTE)(sgn) && \
  out.Hi32 == (ULONG)(hi) && out.Lo64 == (ULONG64)(lo), \
  "expected (%d,%d,%d,%I64x), got (%d,%d,%ld,(%lx %lx)) hres 0x%08lx\n", \
  scl, sgn, hi, (LONG64)(lo), out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres)
#define EXPECTDEC64(scl, sgn, hi, mid, lo) ok(hres == S_OK && \
  out.scale == (BYTE)(scl) && out.sign == (BYTE)(sgn) && \
  out.Hi32 == (ULONG)(hi) && out.Mid32 == (ULONG)(mid) && out.Lo32 == (ULONG)(lo), \
  "expected (%d,%d,%d,(%lx %lx)), got (%d,%d,%ld,(%lx %lx)) hres 0x%08lx\n", \
  scl, sgn, hi, (LONG)(mid), (LONG)(lo), out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres)
#define EXPECTDECZERO() ok(hres == S_OK && out.scale == 0 && \
  (out.sign == 0 || out.sign == 0x80) && out.Hi32 == 0 && out.Lo64 == 0, \
  "expected zero, got (%d,%d,%ld,(%lx %lx)) hres 0x%08lx\n", out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres)
#define CLEAR(x) memset(&(x), 0xBB, sizeof(x))
#define EXPECT_DBL_EXACT(x) ok(hres == S_OK && out == (double)(x), "expected %.17g, got %.17g; hres=%#08lx\n", (double)(x), out, hres)

/* ---- CY ---- */
#define MATHVARS1 HRESULT hres; double left = 0.0; CY cyLeft, out
#define MATHVARS2 MATHVARS1; double right = 0.0; CY cyRight
#define MATH1(func, l) left = (double)l; U(VarCyFromR8)(left, &cyLeft); hres = U(func)(cyLeft, &out)
#define MATH2(func, l, r) left = (double)l; right = (double)r; \
  U(VarCyFromR8)(left, &cyLeft); U(VarCyFromR8)(right, &cyRight); hres = U(func)(cyLeft, cyRight, &out)
#define MATHMULI4(l, r) left = l; right = r; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyMulI4)(cyLeft, right, &out)
#define MATHMULI8(l, r) left = l; right = r; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyMulI8)(cyLeft, right, &out)
#define MATHCMP(l, r) left = l; right = r; U(VarCyFromR8)(left, &cyLeft); U(VarCyFromR8)(right, &cyRight); hres = U(VarCyCmp)(cyLeft, cyRight)
#define MATHCMPR8(l, r) left = l; right = r; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyCmpR8)(cyLeft, right);
#define MATHRND(l, r) left = l; right = r; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyRound)(cyLeft, right, &out)
#define MATHFIX(l) left = l; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyFix)(cyLeft, &out)
#define MATHINT(l) left = l; U(VarCyFromR8)(left, &cyLeft); hres = U(VarCyInt)(cyLeft, &out)

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyAdd)
{
    REQUIRE(U(VarCyAdd));
    REQUIRE(U(VarCyFromR8));

  MATHVARS2;

  MATH2(VarCyAdd, 0.5, 0.5);   EXPECTCY(1);
  MATH2(VarCyAdd, 0.5, -0.4);  EXPECTCY(0.1);
  MATH2(VarCyAdd, 0.5, -0.6);  EXPECTCY(-0.1);
  MATH2(VarCyAdd, -0.5, -0.5); EXPECTCY(-1);
  MATH2(VarCyAdd, -922337203685476.0, -922337203685476.0); EXPECT_OVERFLOW;
  MATH2(VarCyAdd, -922337203685476.0, 922337203685476.0);  EXPECTCY(0);
  MATH2(VarCyAdd, 922337203685476.0, -922337203685476.0);  EXPECTCY(0);
  MATH2(VarCyAdd, 922337203685476.0, 922337203685476.0);   EXPECT_OVERFLOW;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyMul)
{
    REQUIRE(U(VarCyMul));
    REQUIRE(U(VarCyFromR8));

  MATHVARS2;

  MATH2(VarCyMul, 534443.0, 0.0); EXPECTCY(0);
  MATH2(VarCyMul, 0.5, 0.5);      EXPECTCY(0.25);
  MATH2(VarCyMul, 0.5, -0.4);     EXPECTCY(-0.2);
  MATH2(VarCyMul, 0.5, -0.6);     EXPECTCY(-0.3);
  MATH2(VarCyMul, -0.5, -0.5);    EXPECTCY(0.25);
  MATH2(VarCyMul, 922337203685476.0, 20000); EXPECT_OVERFLOW;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCySub)
{
    REQUIRE(U(VarCySub));
    REQUIRE(U(VarCyFromR8));

  MATHVARS2;

  MATH2(VarCySub, 0.5, 0.5);   EXPECTCY(0);
  MATH2(VarCySub, 0.5, -0.4);  EXPECTCY(0.9);
  MATH2(VarCySub, 0.5, -0.6);  EXPECTCY(1.1);
  MATH2(VarCySub, -0.5, -0.5); EXPECTCY(0);
  MATH2(VarCySub, -922337203685476.0, -922337203685476.0); EXPECTCY(0);
  MATH2(VarCySub, -922337203685476.0, 922337203685476.0);  EXPECT_OVERFLOW;
  MATH2(VarCySub, 922337203685476.0, -922337203685476.0);  EXPECT_OVERFLOW;
  MATH2(VarCySub, 922337203685476.0, 922337203685476.0);   EXPECTCY(0);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyAbs)
{
    REQUIRE(U(VarCyAbs));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;

  MATH1(VarCyAbs, 0.5);  EXPECTCY(0.5);
  MATH1(VarCyAbs, -0.5); EXPECTCY(0.5);
  MATH1(VarCyAbs, 922337203685476.0);  EXPECTCY64(2147483647ul,4294951488ul);
  MATH1(VarCyAbs, -922337203685476.0); EXPECTCY64(2147483647ul,4294951488ul);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyNeg)
{
    REQUIRE(U(VarCyNeg));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;

  MATH1(VarCyNeg, 0.5); EXPECTCY(-0.5);
  MATH1(VarCyNeg, -0.5); EXPECTCY(0.5);
  MATH1(VarCyNeg, 922337203685476.0);  EXPECTCY64(2147483648ul,15808);
  MATH1(VarCyNeg, -922337203685476.0); EXPECTCY64(2147483647ul,4294951488ul);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyMulI4)
{
    REQUIRE(U(VarCyMulI4));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;
  LONG right;

  MATHMULI4(534443.0, 0); EXPECTCY(0);
  MATHMULI4(0.5, 1);      EXPECTCY(0.5);
  MATHMULI4(0.5, 2);      EXPECTCY(1);
  MATHMULI4(922337203685476.0, 1); EXPECTCY64(2147483647ul,4294951488ul);
  MATHMULI4(922337203685476.0, 2); EXPECT_OVERFLOW;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyMulI8)
{
    REQUIRE(U(VarCyMulI8));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;
  LONG64 right;

  MATHMULI8(534443.0, 0); EXPECTCY(0);
  MATHMULI8(0.5, 1);      EXPECTCY(0.5);
  MATHMULI8(0.5, 2);      EXPECTCY(1);
  MATHMULI8(922337203685476.0, 1); EXPECTCY64(2147483647ul,4294951488ul);
  MATHMULI8(922337203685476.0, 2); EXPECT_OVERFLOW;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyCmp)
{
    REQUIRE(U(VarCyCmp));
    REQUIRE(U(VarCyFromR8));

  HRESULT hres;
  double left = 0.0, right = 0.0;
  CY cyLeft, cyRight;

  MATHCMP(-1.0, -1.0); EXPECT_EQ;
  MATHCMP(-1.0, 0.0);  EXPECT_LT;
  MATHCMP(-1.0, 1.0);  EXPECT_LT;
  MATHCMP(-1.0, 2.0);  EXPECT_LT;
  MATHCMP(0.0, 1.0);   EXPECT_LT;
  MATHCMP(0.0, 0.0);   EXPECT_EQ;
  MATHCMP(0.0, -1.0);  EXPECT_GT;
  MATHCMP(1.0, -1.0);  EXPECT_GT;
  MATHCMP(1.0, 0.0);   EXPECT_GT;
  MATHCMP(1.0, 1.0);   EXPECT_EQ;
  MATHCMP(1.0, 2.0);   EXPECT_LT;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyCmpR8)
{
    REQUIRE(U(VarCyCmpR8));
    REQUIRE(U(VarCyFromR8));

  HRESULT hres;
  double left = 0.0;
  CY cyLeft;
  double right;

  MATHCMPR8(-1.0, -1.0); EXPECT_EQ;
  MATHCMPR8(-1.0, 0.0);  EXPECT_LT;
  MATHCMPR8(-1.0, 1.0);  EXPECT_LT;
  MATHCMPR8(-1.0, 2.0);  EXPECT_LT;
  MATHCMPR8(0.0, 1.0);   EXPECT_LT;
  MATHCMPR8(0.0, 0.0);   EXPECT_EQ;
  MATHCMPR8(0.0, -1.0);  EXPECT_GT;
  MATHCMPR8(1.0, -1.0);  EXPECT_GT;
  MATHCMPR8(1.0, 0.0);   EXPECT_GT;
  MATHCMPR8(1.0, 1.0);   EXPECT_EQ;
  MATHCMPR8(1.0, 2.0);   EXPECT_LT;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyRound)
{
    REQUIRE(U(VarCyRound));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;
  int right;

  MATHRND(0.5432, 5);  EXPECTCY(0.5432);
  MATHRND(0.5432, 4);  EXPECTCY(0.5432);
  MATHRND(0.5432, 3);  EXPECTCY(0.543);
  MATHRND(0.5432, 2);  EXPECTCY(0.54);
  MATHRND(0.5432, 1);  EXPECTCY(0.5);
  MATHRND(0.5532, 0);  EXPECTCY(1);
  MATHRND(0.5532, -1); EXPECT_INVALID;

  MATHRND(0.5568, 5);  EXPECTCY(0.5568);
  MATHRND(0.5568, 4);  EXPECTCY(0.5568);
  MATHRND(0.5568, 3);  EXPECTCY(0.557);
  MATHRND(0.5568, 2);  EXPECTCY(0.56);
  MATHRND(0.5568, 1);  EXPECTCY(0.6);
  MATHRND(0.5568, 0);  EXPECTCY(1);
  MATHRND(0.5568, -1); EXPECT_INVALID;

  MATHRND(0.4999, 0); EXPECTCY(0);
  MATHRND(0.5000, 0); EXPECTCY(0);
  MATHRND(0.5001, 0); EXPECTCY(1);
  MATHRND(1.4999, 0); EXPECTCY(1);
  MATHRND(1.5000, 0); EXPECTCY(2);
  MATHRND(1.5001, 0); EXPECTCY(2);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyFix)
{
    REQUIRE(U(VarCyFix));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;

  MATHFIX(-1.0001); EXPECTCY(-1);
  MATHFIX(-1.4999); EXPECTCY(-1);
  MATHFIX(-1.5001); EXPECTCY(-1);
  MATHFIX(-1.9999); EXPECTCY(-1);
  MATHFIX(-0.0001); EXPECTCY(0);
  MATHFIX(-0.4999); EXPECTCY(0);
  MATHFIX(-0.5001); EXPECTCY(0);
  MATHFIX(-0.9999); EXPECTCY(0);
  MATHFIX(0.0001);  EXPECTCY(0);
  MATHFIX(0.4999);  EXPECTCY(0);
  MATHFIX(0.5001);  EXPECTCY(0);
  MATHFIX(0.9999);  EXPECTCY(0);
  MATHFIX(1.0001);  EXPECTCY(1);
  MATHFIX(1.4999);  EXPECTCY(1);
  MATHFIX(1.5001);  EXPECTCY(1);
  MATHFIX(1.9999);  EXPECTCY(1);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarCyInt)
{
    REQUIRE(U(VarCyInt));
    REQUIRE(U(VarCyFromR8));

  MATHVARS1;

  MATHINT(-1.0001); EXPECTCY(-2);
  MATHINT(-1.4999); EXPECTCY(-2);
  MATHINT(-1.5001); EXPECTCY(-2);
  MATHINT(-1.9999); EXPECTCY(-2);
  MATHINT(-0.0001); EXPECTCY(-1);
  MATHINT(-0.4999); EXPECTCY(-1);
  MATHINT(-0.5001); EXPECTCY(-1);
  MATHINT(-0.9999); EXPECTCY(-1);
  MATHINT(0.0001);  EXPECTCY(0);
  MATHINT(0.4999);  EXPECTCY(0);
  MATHINT(0.5001);  EXPECTCY(0);
  MATHINT(0.9999);  EXPECTCY(0);
  MATHINT(1.0001);  EXPECTCY(1);
  MATHINT(1.4999);  EXPECTCY(1);
  MATHINT(1.5001);  EXPECTCY(1);
  MATHINT(1.9999);  EXPECTCY(1);
}

/* ---- DECIMAL ---- */
#undef MATHVARS1
#undef MATHVARS2
#undef MATH1
#undef MATH2
#undef MATHRND
#define MATHVARS1 HRESULT hres; DECIMAL l, out
#define MATHVARS2 MATHVARS1; DECIMAL r
#define MATH1(func) hres = U(func)(&l, &out)
#define MATH2(func) hres = U(func)(&l, &r, &out)
#define MATH3(func) hres = U(func)(&l, r)

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecAbs)
{
    REQUIRE(U(VarDecAbs));

  MATHVARS1;

  SETDEC(l,0,0x80,0,1);  MATH1(VarDecAbs); EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0,0,0);     MATH1(VarDecAbs); EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0x80,0,0);  MATH1(VarDecAbs); EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,1);     MATH1(VarDecAbs); EXPECTDEC(0,0,0,1);

  /* Doesn't check for invalid input */
  SETDEC(l,0,0x7f,0,1);  MATH1(VarDecAbs); EXPECTDEC(0,0x7f,0,1);
  SETDEC(l,0,0x80,29,1); MATH1(VarDecAbs); EXPECTDEC(0,0,29,1);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecNeg)
{
    REQUIRE(U(VarDecNeg));

  MATHVARS1;

  SETDEC(l,0,0x80,0,1); MATH1(VarDecNeg); EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0,0,0);    MATH1(VarDecNeg); EXPECTDEC(0,0x80,0,0); /* '-0'! */
  SETDEC(l,0,0x80,0,0); MATH1(VarDecNeg); EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,1);    MATH1(VarDecNeg); EXPECTDEC(0,0x80,0,1);

  /* Doesn't check for invalid input */
  SETDEC(l,0,0x7f,0,1);  MATH1(VarDecNeg); EXPECTDEC(0,0xff,0,1);
  SETDEC(l,0,0x80,29,1); MATH1(VarDecNeg); EXPECTDEC(0,0,29,1);
  SETDEC(l,0,0,29,1);    MATH1(VarDecNeg); EXPECTDEC(0,0x80,29,1);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecAdd)
{
    REQUIRE(U(VarDecAdd));

  MATHVARS2;

  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,0);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd); EXPECTDEC(0,0x80,0,1);
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,1);

  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,0);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,2);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd); EXPECTDECZERO();
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0x80,0,2); MATH2(VarDecAdd); EXPECTDEC(0,0x80,0,1);

  SETDEC(l,0,0x80,0,0); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0x80,0,1); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDECZERO();
  SETDEC(l,0,0x80,0,1); SETDEC(r,0,0,0,2);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0x80,0,1); SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd); EXPECTDEC(0,0x80,0,2);
  SETDEC(l,0,0x80,0,2); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0x80,0,1);

  SETDEC(l,0,0,0,0xffffffff); SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd); EXPECTDEC(0,0,0,0xfffffffe);
  SETDEC(l,0,0,0,0xffffffff); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,(ULONG64)1 << 32);
  SETDEC(l,0,0,0,0xffffffff); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0,(ULONG64)1 << 32);

  SETDEC64(l,0,0,0,0xffffffff,0); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC64(0,0,0,0xffffffff,1);
  SETDEC64(l,0,0,0,0xffffffff,0); SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0,0xfffffffe,0xffffffff);

  SETDEC64(l,0,0,0,0xffffffff,0xffffffff); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,1,0);
  SETDEC64(l,0,0,0,0xffffffff,0xffffffff); SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0,0xffffffff,0xfffffffe);

  SETDEC(l,0,0,0xffffffff,0); SETDEC(r,0,0,0,1);    MATH2(VarDecAdd); EXPECTDEC(0,0,0xffffffff,1);
  SETDEC(l,0,0,0xffffffff,0); SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0xfffffffe,0xffffffff,0xffffffff);

  SETDEC64(l,0,0,0xffffffff,0xffffffff,0xffffffff);SETDEC(r,0,0x80,0,1); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0xffffffff,0xffffffff,0xfffffffe);
  SETDEC64(l,0,0,0xffffffff,0xffffffff,0xffffffff);SETDEC(r,0,0,0,1); MATH2(VarDecAdd);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);

  SETDEC64(l,1,0,0xffffffff,0xffffffff,0xffffffff);SETDEC(r,1,0,0,1); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0x19999999,0x99999999,0x9999999A);

  SETDEC64(l,0,0,0xe22ea493,0xb30310a7,0x70000000);SETDEC64(r,0,0,0xe22ea493,0xb30310a7,0x70000000); MATH2(VarDecAdd);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);

  SETDEC64(l,1,0,0xe22ea493,0xb30310a7,0x70000000);SETDEC64(r,1,0,0xe22ea493,0xb30310a7,0x70000000); MATH2(VarDecAdd);
  EXPECTDEC64(0,0,0x2d3c8750,0xbd670354,0xb0000000);

  SETDEC(l,3,128,0,123456); SETDEC64(r,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF);
  MATH2(VarDecAdd); EXPECTDEC64(0,0,-1,0xFFFFFFFF,0xFFFFFF84);

  SETDEC(l,3,0,0,123456); SETDEC64(r,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF); MATH2(VarDecAdd);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);

  SETDEC(l,4,0,0,123456); SETDEC64(r,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF); MATH2(VarDecAdd);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);

  SETDEC(l,5,0,0,123456); SETDEC64(r,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF); MATH2(VarDecAdd);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);

  SETDEC(l,6,0,0,123456); SETDEC64(r,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF);
  MATH2(VarDecAdd); EXPECTDEC64(0,0,-1,0xFFFFFFFF,0xFFFFFFFF);

  SETDEC(l,3,128,0,123456); SETDEC64(r,0,0,0x19999999,0x99999999,0x99999999);
  MATH2(VarDecAdd); EXPECTDEC64(1,0,-1,0xFFFFFFFF,0xFFFFFB27);

  SETDEC(l,3,128,0,123567); SETDEC64(r,0,0,0x19999999,0x99999999,0x99999999);
  MATH2(VarDecAdd); EXPECTDEC64(1,0,-1,0xFFFFFFFF,0xFFFFFB26);

  /* Promotes to the highest scale, so here the results are in the scale of 2 */
  SETDEC(l,2,0,0,0);   SETDEC(r,0,0,0,0); MATH2(VarDecAdd); EXPECTDEC(2,0,0,0);
  SETDEC(l,2,0,0,100); SETDEC(r,0,0,0,1); MATH2(VarDecAdd); EXPECTDEC(2,0,0,200);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecSub)
{
    REQUIRE(U(VarDecSub));

  MATHVARS2;

  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,0);    MATH2(VarDecSub); EXPECTDECZERO();
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,1);    MATH2(VarDecSub); EXPECTDEC(0,0x80,0,1);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,1);    MATH2(VarDecSub); EXPECTDECZERO();
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0x80,0,1); MATH2(VarDecSub); EXPECTDEC(0,0,0,2);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecMul)
{
    REQUIRE(U(VarDecMul));

  MATHVARS2;
  
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,0);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,0);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,1);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,1);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,1);
  SETDEC(l,0,0,0,45000);SETDEC(r,0,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,90000);
  SETDEC(l,0,0,0,2);    SETDEC(r,0,0,0,45000);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,90000);

  SETDEC(l,0,0x80,0,2); SETDEC(r,0,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(0,0x80,0,4);
  SETDEC(l,0,0,0,2);    SETDEC(r,0,0x80,0,2);  MATH2(VarDecMul);   EXPECTDEC(0,0x80,0,4);
  SETDEC(l,0,0x80,0,2); SETDEC(r,0,0x80,0,2);  MATH2(VarDecMul);   EXPECTDEC(0,0,0,4);

  SETDEC(l,4,0,0,2);    SETDEC(r,0,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(4,0,0,4);
  SETDEC(l,0,0,0,2);    SETDEC(r,3,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(3,0,0,4);
  SETDEC(l,4,0,0,2);    SETDEC(r,3,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(7,0,0,4);
  /* this last one shows that native oleaut32 does *not* gratuitously seize opportunities
     to reduce the scale if possible - the canonical result for the expected value is (6,0,0,1)
   */
  SETDEC(l,4,0,0,5);    SETDEC(r,3,0,0,2);  MATH2(VarDecMul);   EXPECTDEC(7,0,0,10);
  
  SETDEC64(l,0,0,0,0xFFFFFFFF,0xFFFFFFFF);    SETDEC(r,0,0,0,2);  MATH2(VarDecMul);   EXPECTDEC64(0,0,1,0xFFFFFFFF,0xFFFFFFFE);
  SETDEC(l,0,0,0,2);    SETDEC64(r,0,0,0,0xFFFFFFFF,0xFFFFFFFF);  MATH2(VarDecMul);   EXPECTDEC64(0,0,1,0xFFFFFFFF,0xFFFFFFFE);
  SETDEC(l,0,0,1,1);    SETDEC(r,0,0,0,0x80000000);  MATH2(VarDecMul);   EXPECTDEC(0,0,0x80000000,0x80000000);
  SETDEC(l,0,0,0,0x80000000);    SETDEC(r,0,0,1,1);  MATH2(VarDecMul);   EXPECTDEC(0,0,0x80000000,0x80000000);
  
  /* near-overflow, used as a reference */
  SETDEC64(l,0,0,0,0xFFFFFFFF,0xFFFFFFFF);    SETDEC(r,0,0,0,2000000000);  MATH2(VarDecMul);EXPECTDEC64(0,0,1999999999,0xFFFFFFFF,0x88CA6C00);
  /* actual overflow - right operand is 10 times the previous value */
  SETDEC64(l,0,0,0,0xFFFFFFFF,0xFFFFFFFF);    SETDEC64(r,0,0,0,4,0xA817C800);  MATH2(VarDecMul);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);
  /* here, native oleaut32 has an opportunity to avert the overflow, by reducing the scale of the result  */
  SETDEC64(l,1,0,0,0xFFFFFFFF,0xFFFFFFFF);    SETDEC64(r,0,0,0,4,0xA817C800);  MATH2(VarDecMul);EXPECTDEC64(0,0,1999999999,0xFFFFFFFF,0x88CA6C00);

  /* near-overflow, used as a reference */
  SETDEC64(l,0,0,1,0xFFFFFFFF,0xFFFFFFFE);    SETDEC(r,0,0,0,1000000000);  MATH2(VarDecMul);EXPECTDEC64(0,0,1999999999,0xFFFFFFFF,0x88CA6C00);
  /* actual overflow - right operand is 10 times the previous value */
  SETDEC64(l,0,0,1,0xFFFFFFFF,0xFFFFFFFE);    SETDEC64(r,0,0,0,2,0x540BE400);  MATH2(VarDecMul);
  ok(hres == DISP_E_OVERFLOW,"Expected overflow, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);
  /* here, native oleaut32 has an opportunity to avert the overflow, by reducing the scale of the result  */
  SETDEC64(l,1,0,1,0xFFFFFFFF,0xFFFFFFFE);    SETDEC64(r,0,0,0,2,0x540BE400);  MATH2(VarDecMul);EXPECTDEC64(0,0,1999999999,0xFFFFFFFF,0x88CA6C00);
  
  /* this one shows that native oleaut32 is willing to lose significant digits in order to avert an overflow */
  SETDEC64(l,2,0,0,0xFFFFFFFF,0xFFFFFFFF);    SETDEC64(r,0,0,0,9,0x502F9001);  MATH2(VarDecMul);EXPECTDEC64(1,0,0xee6b2800,0x19999998,0xab2e719a);
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecDiv)
{
    REQUIRE(U(VarDecDiv));

  MATHVARS2;
  
  /* identity divisions */
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,1);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,0);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,1);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,1);
  SETDEC(l,1,0,0,1);    SETDEC(r,0,0,0,1);  MATH2(VarDecDiv);   EXPECTDEC(1,0,0,1);

  /* exact divisions */  
  SETDEC(l,0,0,0,45);    SETDEC(r,0,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,5);
  SETDEC(l,1,0,0,45);    SETDEC(r,0,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(1,0,0,5);
  SETDEC(l,0,0,0,45);    SETDEC(r,1,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,50);
  SETDEC(l,1,0,0,45);    SETDEC(r,2,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,50);
  /* these last three results suggest that native oleaut32 scales both operands down to zero
     before the division, but does not always try to scale the result, even if it is possible -
     analogous to multiplication behavior.
   */
  SETDEC(l,1,0,0,45);    SETDEC(r,1,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(0,0,0,5);
  SETDEC(l,2,0,0,450);    SETDEC(r,1,0,0,9);  MATH2(VarDecDiv);
  if (out.scale == 1) EXPECTDEC(1,0,0,50);
  else EXPECTDEC(0,0,0,5);

  /* inexact divisions */
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,180700362,0x14b700cb,0x05555555);
  SETDEC(l,1,0,0,1);    SETDEC(r,0,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,18070036,0x35458014,0x4d555555);
  SETDEC(l,0,0,0,1);    SETDEC(r,1,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,1807003620,0xcf2607ee,0x35555555);
  SETDEC(l,1,0,0,1);    SETDEC(r,2,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,1807003620,0xcf2607ee,0x35555555);
  SETDEC(l,1,0,0,1);    SETDEC(r,1,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,180700362,0x14b700cb,0x05555555);
  SETDEC(l,2,0,0,10);    SETDEC(r,1,0,0,3);  MATH2(VarDecDiv);  EXPECTDEC64(28,0,180700362,0x14b700cb,0x05555555);

  /* this one shows that native oleaut32 rounds up the result */
  SETDEC(l,0,0,0,2);    SETDEC(r,0,0,0,3);  MATH2(VarDecDiv);   EXPECTDEC64(28,0,361400724,0x296e0196,0x0aaaaaab);
  
  /* sign tests */
  SETDEC(l,0,0x80,0,45);    SETDEC(r,0,0,0,9);  MATH2(VarDecDiv);   EXPECTDEC(0,0x80,0,5);
  SETDEC(l,0,0,0,45);       SETDEC(r,0,0x80,0,9);  MATH2(VarDecDiv);EXPECTDEC(0,0x80,0,5);
  SETDEC(l,0,0x80,0,45);    SETDEC(r,0,0x80,0,9);  MATH2(VarDecDiv);EXPECTDEC(0,0,0,5);
  
  /* oddballs */
  SETDEC(l,0,0,0,0);    SETDEC(r,0,0,0,0);  MATH2(VarDecDiv);/* indeterminate */
  ok(hres == DISP_E_DIVBYZERO,"Expected division-by-zero, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);
  SETDEC(l,0,0,0,1);    SETDEC(r,0,0,0,0);  MATH2(VarDecDiv);/* division by zero */
  ok(hres == DISP_E_DIVBYZERO,"Expected division-by-zero, got (%d,%d,%ld,(%8lx,%8lx)x) hres 0x%08lx\n",
     out.scale, out.sign, out.Hi32, out.Mid32, out.Lo32, hres);
  
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecCmp)
{
    REQUIRE(U(VarDecCmp));

  MATHVARS1;

  SETDEC(l,0,0,0,1); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,0,0,1); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,0,1); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(l,0,0,0,1); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,0,1); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,0,1); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,1); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(l,0,0,0,0); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,0,0,0); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,0,0,0); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(l,0,0,0,0); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,0,0); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,0,0,0); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(l,0,DECIMAL_NEG,0,0); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(l,0,0,-1,-1); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,-1,-1); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,-1,-1); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_EQ;

  SETDEC(l,0,0,-1,-1); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,-1,-1); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(l,0,0,-1,-1); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,0,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,0,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,-1,-1); SETDEC(out,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_EQ;


  SETDEC(out,0,0,0,1); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,0,0,1); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,0,1); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(out,0,0,0,1); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,0,1); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,0,1); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,0,1); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(out,0,0,0,0); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,0,0,0); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,0,0,0); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(out,0,0,0,0); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,0,0); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,0,0,0); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_EQ;
  SETDEC(out,0,DECIMAL_NEG,0,0); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(out,0,0,-1,-1); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,-1,-1); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,-1,-1); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_EQ;

  SETDEC(out,0,0,-1,-1); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,-1,-1); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_LT;
  SETDEC(out,0,0,-1,-1); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_LT;

  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,0,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,0,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,0,-1,-1); MATH1(VarDecCmp); EXPECT_GT;

  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,DECIMAL_NEG,0,1); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,DECIMAL_NEG,0,0); MATH1(VarDecCmp); EXPECT_GT;
  SETDEC(out,0,DECIMAL_NEG,-1,-1); SETDEC(l,0,DECIMAL_NEG,-1,-1); MATH1(VarDecCmp); EXPECT_EQ;

  SETDEC(l,3,0,0,123456); SETDEC64(out,0,0,0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF);
  MATH1(VarDecCmp); EXPECT_LT;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecCmpR8)
{
    REQUIRE(U(VarDecCmpR8));

  HRESULT hres;
  DECIMAL l;
  double r;

  SETDEC(l,0,0,0,1); r = 0.0;  MATH3(VarDecCmpR8); EXPECT_GT;
  SETDEC(l,0,0,0,1); r = 0.1;  MATH3(VarDecCmpR8); EXPECT_GT;
  SETDEC(l,0,0,0,1); r = -0.1; MATH3(VarDecCmpR8); EXPECT_GT;

  SETDEC(l,0,DECIMAL_NEG,0,1); r = 0.0;  MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,1); r = 0.1;  MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,1); r = -0.1; MATH3(VarDecCmpR8); EXPECT_LT;

  SETDEC(l,0,0,0,0); r = 0.0;  MATH3(VarDecCmpR8); EXPECT_EQ;
  SETDEC(l,0,0,0,0); r = 0.1;  MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,0,0,0); r = -0.1; MATH3(VarDecCmpR8); EXPECT_GT;

  SETDEC(l,0,DECIMAL_NEG,0,0); r = 0.0;  MATH3(VarDecCmpR8); EXPECT_EQ;
  SETDEC(l,0,DECIMAL_NEG,0,0); r = 0.1;  MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,0); r = -0.1; MATH3(VarDecCmpR8); EXPECT_GT;

  SETDEC(l,0,0,0,1);             r = DECIMAL_NEG; MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,DECIMAL_NEG,0,0);   r = DECIMAL_NEG; MATH3(VarDecCmpR8); EXPECT_LT;
  SETDEC(l,0,0,-1,-1);           r = DECIMAL_NEG; MATH3(VarDecCmpR8); EXPECT_GT;
  SETDEC(l,0,DECIMAL_NEG,-1,-1); r = DECIMAL_NEG; MATH3(VarDecCmpR8); EXPECT_LT;
}

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarDecRound)
{
    REQUIRE(U(VarDecRound));

    HRESULT hres;
    DECIMAL l, out;

    CLEAR(out); SETDEC(l, 0, 0, 0, 1); hres = U(VarDecRound)(&l, 3, &out); EXPECTDEC(0, 0, 0, 1);

    CLEAR(out); SETDEC(l, 0, 0, 0, 1); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC(0, 0, 0, 1);
    CLEAR(out); SETDEC(l, 1, 0, 0, 1); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC(0, 0, 0, 0);
    CLEAR(out); SETDEC(l, 1, 0, 0, 1); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, 0, 0, 1);
    CLEAR(out); SETDEC(l, 2, 0, 0, 11); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, 0, 0, 1);
    CLEAR(out); SETDEC(l, 2, 0, 0, 15); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, 0, 0, 2);
    CLEAR(out); SETDEC(l, 6, 0, 0, 550001); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, 0, 0, 6);

    CLEAR(out); SETDEC(l, 0, DECIMAL_NEG, 0, 1); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC(0, DECIMAL_NEG, 0, 1);
    CLEAR(out); SETDEC(l, 1, DECIMAL_NEG, 0, 1); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC(0, DECIMAL_NEG, 0, 0);
    CLEAR(out); SETDEC(l, 1, DECIMAL_NEG, 0, 1); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, DECIMAL_NEG, 0, 1);
    CLEAR(out); SETDEC(l, 2, DECIMAL_NEG, 0, 11); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, DECIMAL_NEG, 0, 1);
    CLEAR(out); SETDEC(l, 2, DECIMAL_NEG, 0, 15); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, DECIMAL_NEG, 0, 2);
    CLEAR(out); SETDEC(l, 6, DECIMAL_NEG, 0, 550001); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, DECIMAL_NEG, 0, 6);

    CLEAR(out); SETDEC64(l, 0, 0, 0xffffffff, 0xffffffff, 0xffffffff); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC64(0, 0, 0xffffffff, 0xffffffff, 0xffffffff);
    CLEAR(out); SETDEC64(l, 28, 0, 0xffffffff, 0xffffffff, 0xffffffff); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC64(0, 0, 0, 0, 8);
    CLEAR(out); SETDEC64(l, 0, DECIMAL_NEG, 0xffffffff, 0xffffffff, 0xffffffff); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC64(0, DECIMAL_NEG, 0xffffffff, 0xffffffff, 0xffffffff);
    CLEAR(out); SETDEC64(l, 28, DECIMAL_NEG, 0xffffffff, 0xffffffff, 0xffffffff); hres = U(VarDecRound)(&l, 0, &out); EXPECTDEC64(0, DECIMAL_NEG, 0, 0, 8);

    CLEAR(out); SETDEC(l, 2, 0, 0, 0); hres = U(VarDecRound)(&l, 1, &out); EXPECTDEC(1, 0, 0, 0);
}

/* ---- VarR8Round ---- */
#undef MATHRND
#define MATHRND(l, r) left = l; right = r; hres = U(VarR8Round)(left, right, &out)
#define EXPECT(x) EXPECT_DBL_EXACT(x)

/* adapted from Wine dlls/oleaut32/tests/vartype.c (LGPL-2.1+) */
TEST(variant_arith_ops_lift_VarR8Round)
{
    REQUIRE(U(VarR8Round));

  HRESULT hres;
  double left = 0.0, out;
  int right;

  MATHRND(0.5432, 5);  EXPECT(0.5432);
  MATHRND(0.5432, 4);  EXPECT(0.5432);
  MATHRND(0.5432, 3);  EXPECT(0.543);
  MATHRND(0.5432, 2);  EXPECT(0.54);
  MATHRND(0.5432, 1);  EXPECT(0.5);
  MATHRND(0.5532, 0);  EXPECT(1);
  MATHRND(0.5532, -1); EXPECT_INVALID;

  MATHRND(0.5568, 5);  EXPECT(0.5568);
  MATHRND(0.5568, 4);  EXPECT(0.5568);
  MATHRND(0.5568, 3);  EXPECT(0.557);
  MATHRND(0.5568, 2);  EXPECT(0.56);
  MATHRND(0.5568, 1);  EXPECT(0.6);
  MATHRND(0.5568, 0);  EXPECT(1);
  MATHRND(0.5568, -1); EXPECT_INVALID;

  MATHRND(0.4999, 0); EXPECT(0);
  MATHRND(0.5000, 0); EXPECT(0);
  MATHRND(0.5001, 0); EXPECT(1);
  MATHRND(1.4999, 0); EXPECT(1);
  MATHRND(1.5000, 0); EXPECT(2);
  MATHRND(1.5001, 0); EXPECT(2);

  MATHRND(-0.4999, 0); EXPECT(0);
  MATHRND(-0.5000, 0); EXPECT(0);
  MATHRND(-0.5001, 0); EXPECT(-1);
  MATHRND(-1.4999, 0); EXPECT(-1);
  MATHRND(-1.5000, 0); EXPECT(-2);
  MATHRND(-1.5001, 0); EXPECT(-2);
}
