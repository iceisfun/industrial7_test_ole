/* Helpers of the dx_ole32_ext area. */
#ifndef EXT_H
#define EXT_H
#define COBJMACROS
#define CINTERFACE
#define INITGUID
#include "t.h"
#include <objbase.h>
#include <ole2.h>
#include <stdlib.h>
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); \
    t_check(v_ == (ULONG_PTR)(want), __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", \
            (unsigned long long)v_, (unsigned long long)(ULONG_PTR)(want)); } while (0)
/* HKCU\Software\Classes\CLSID\{clsid}[\sub] (create with an optional default value, or delete the whole class key) */
void x_clsid_text(const GUID *g, char out[40]);
int x_reg_class_sub(const GUID *g, const char *sub, const char *dflt);
void x_reg_class_delete(const GUID *g);
#endif
