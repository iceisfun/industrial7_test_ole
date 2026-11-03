/* Shared table-driven runner for VARIANT arithmetic/logic/compare tests (oleaut32).
 * Every call goes through GetProcAddress (U()). */
#ifndef VA_H
#define VA_H
#include "t.h"
#include <oleauto.h>
#include <float.h>
#include <math.h>

typedef struct { unsigned short vt; long long i; double d; const WCHAR *s; } vcell;
#define C_NONE        {VT_EMPTY,0,0,0}
#define C_EMPTY       {VT_EMPTY,0,0,0}
#define C_NULL        {VT_NULL,0,0,0}
#define C_RAW(v)      {(v),0,0,0}
#define C_RAWI(v,x)   {(v),(long long)(x),0,0}
#define C_I1(x)       {VT_I1,(long long)(x),0,0}
#define C_UI1(x)      {VT_UI1,(long long)(x),0,0}
#define C_I2(x)       {VT_I2,(long long)(x),0,0}
#define C_UI2(x)      {VT_UI2,(long long)(x),0,0}
#define C_I4(x)       {VT_I4,(long long)(x),0,0}
#define C_UI4(x)      {VT_UI4,(long long)(unsigned)(x),0,0}
#define C_I8(x)       {VT_I8,(long long)(x),0,0}
#define C_UI8(x)      {VT_UI8,(long long)(unsigned long long)(x),0,0}
#define C_INT(x)      {VT_INT,(long long)(x),0,0}
#define C_UINT(x)     {VT_UINT,(long long)(unsigned)(x),0,0}
#define C_BOOL(x)     {VT_BOOL,(long long)(short)(x),0,0}
#define C_ERROR(x)    {VT_ERROR,(long long)(x),0,0}
#define C_R4(x)       {VT_R4,0,(double)(float)(x),0}
#define C_R8(x)       {VT_R8,0,(double)(x),0}
#define C_DATE(x)     {VT_DATE,0,(double)(x),0}
#define C_CY(x)       {VT_CY,(long long)(x),0,0}
#define C_BSTR(w)     {VT_BSTR,0,0,(w)}
#define C_BSTRNULL    {VT_BSTR,0,0,0}
#define R4_MAX FLT_MAX
#define R4_MIN FLT_MIN
#define R8_MAX DBL_MAX
#define R8_MIN DBL_MIN
#define I1_MAX 0x7f
#define I1_MIN (-0x80)
#define UI1_MAX 0xff
#define UI1_MIN 0
#define I2_MAX 0x7fff
#define I2_MIN (-0x8000)
#define UI2_MAX 0xffff
#define UI2_MIN 0
#define I4_MAX 0x7fffffff
#define I4_MIN (-0x7fffffff-1)
#define UI4_MAX 0xffffffffu
#define UI4_MIN 0
#define I8_MAX 0x7fffffffffffffffLL
#define I8_MIN (-0x7fffffffffffffffLL-1)
#define UI8_MAX 0xffffffffffffffffULL
#define DATE_MAX 2958465
#define DATE_MIN -657434
#define VARIANT_TRUE_ ((VARIANT_BOOL)-1)

typedef struct { int wl; int n; vcell l, r, e; long hr; } vrow;
typedef struct { int wl; vcell l, r; long res[4]; } vcmprow;

enum { VA_ADD, VA_SUB, VA_MUL, VA_DIV, VA_IDIV, VA_MOD, VA_POW, VA_AND, VA_OR, VA_XOR, VA_EQV, VA_IMP, VA_CAT,
       VA_ABS, VA_NOT, VA_NEG, VA_INT, VA_FIX, VA_ROUND };

/* build a VARIANT from a cell (BSTR allocated: release with va_free) */
void va_make(VARIANT *v, const vcell *c);
void va_free(VARIANT *v);
int  va_same(const VARIANT *a, const VARIANT *b);          /* vt + value compare, bit-exact for ints/strings, tolerance for floats */
int  va_equal_cell(const VARIANT *v, const vcell *c);
const char *va_vtname(unsigned vt);
const char *va_opname(int op);
char *va_describe(const VARIANT *v, char *buf, size_t cap);

/* raw invocation (resolved through GetProcAddress) */
HRESULT va_bin(int op, VARIANT *l, VARIANT *r, VARIANT *res);
HRESULT va_un(int op, VARIANT *v, int ndec, VARIANT *res);

/* one binary call with full checking; returns hr */
void va_check_bin(int wl, int op, const vcell *l, const vcell *r, HRESULT hr, const vcell *e);
void va_check_un(int wl, int op, const vcell *l, int ndec, HRESULT hr, const vcell *e);

void va_run_binary(int op, const vrow *rows, size_t n, int commutative);
void va_run_unary(int op, const vrow *rows, size_t n, int commutative);
void va_run_round(int op, const vrow *rows, size_t n, int commutative);
void va_run_cmp(const vcmprow *rows, size_t n);

/* compare helpers: l,r are plain variant makers */
HRESULT va_cmp(const vcell *l, const vcell *r, LCID lcid, ULONG flags, unsigned lflag, unsigned rflag);
#define LCID_US MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT)
#endif
