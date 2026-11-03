/* gap_06 shared helpers. */
#ifndef GAP6_H
#define GAP6_H
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#define VariantInit(p) ((void)memset((p), 0, sizeof(VARIANT)))   /* no direct oleaut32 import */
/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice, wrong for calls with side effects) */
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); \
    t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)
static const GUID G6_IID_IUnknown = {0x00000000,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G6_IID_IDispatch = {0x00020400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G6_NIL;
#endif
