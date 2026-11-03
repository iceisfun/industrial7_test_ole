/* VARIANT_User* : VT_VARIANT|BYREF chains, VT_ARRAY variants and interface pointers (VT_UNKNOWN / VT_DISPATCH).
 *
 * Wire notes (NDR32 decomp):
 *  - VT_VARIANT|BYREF: marker 0x18 (sizeof(VARIANT) on x64) at +20, the dword 'User' (0x72657355) at +24, then the
 *    nested VARIANT image at the next 8-byte boundary (+32).
 *  - VT_ARRAY: at +20 a dword (low half of the SAFEARRAY* | 1, or 0), then the LPSAFEARRAY wire image (which starts
 *    with its own non-NULL dword) at +24; BYREF adds the marker, shifting everything by 4.
 *  - interfaces: dword (ptr|1) or 0, then {u32 size, u32 size, 'MEOW'... OBJREF} produced by standard marshaling. */
#include "ums.h"

#define NEEDV NEED_FAM(UF_VARIANT)

static ULONG vsize(int v, ULONG ctx, ULONG start, VARIANT *var, DWORD *exc)
{
    UCtx c; ULONG s;
    uctx_init(&c, 0, ctx, USER_MARSHAL_CB_BUFFER_SIZE);
    s = u_size(&c, UF_VARIANT, v, start, var, exc);
    uctx_free(&c);
    return s;
}

static void vfree(int v, VARIANT *var)
{
    UCtx c; uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE);
    u_free(&c, UF_VARIANT, v, var, NULL);
    uctx_free(&c);
}

/* size + marshal (+ optional model compare) + unmarshal into dst. Returns the size. */
static ULONG vcycle(int v, ULONG ctx, VARIANT *src, VARIANT *dst, int cmp_model, const char *what)
{
    UCtx c; ULONG size; unsigned char *m, *u, exp[2048], mask[2048];
    size = vsize(v, ctx, 0, src, NULL);
    uctx_init(&c, size, ctx, USER_MARSHAL_CB_MARSHALL);
    m = u_marshal(&c, UF_VARIANT, v, c.buf, src, NULL);
    t_check(m != NULL && m <= c.buf + size, __FILE__, __LINE__, what, "v64=%d marshal end %ld > UserSize %lu", v, (long)(m - c.buf), size);
    CHECK(uctx_canary_ok(&c));
    if (cmp_model) {
        ULONG me;
        memset(exp, 0xcc, sizeof exp); memset(mask, 0, sizeof mask);
        me = ums_model_variant(v, src, 0, exp, mask);
        t_check(me == (ULONG)(m - c.buf), __FILE__, __LINE__, what, "model end %lu marshal end %ld", me, (long)(m - c.buf));
        if (me <= sizeof exp) CHECK_WIRE(c.buf, exp, mask, me, what);
    }
    if (dst) {
        uctx_reread(&c, size, ctx);
        u = u_unmarshal(&c, UF_VARIANT, v, c.buf, dst, NULL);
        t_check(u == m, __FILE__, __LINE__, what, "unmarshal end %ld marshal end %ld", (long)(u - c.buf), (long)(m - c.buf));
    }
    uctx_free(&c);
    return size;
}

/* ---- VT_VARIANT | VT_BYREF ------------------------------------------------------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) */
TEST(ums_variant_byref_variant_r8)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT inner, outer, dst; IMalloc *mal = NULL; ULONG size; UCtx c; unsigned char *w;
        memset(&inner, 0, sizeof inner); V_VT(&inner) = VT_R8; V_R8(&inner) = 3.1415;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
        memset(&dst, 0, sizeof dst);
        size = vcycle(v, MSHCTX_DIFFERENTMACHINE, &outer, &dst, 1, "VARIANT byref R8");
        EQ(size, v ? 80 : 64);                       /* 32 header+marker+'User'+pad + 32 nested */
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &outer, NULL);
        w = c.buf;
        if (v) { EQ(rd64(w + 24), 0x18); EQ(rd64(w + 32), 0x72657355); }
        else   { EQ(rd32(w + 20), 0x18); EQ(rd32(w + 24), 0x72657355); EQ(rd32(w + 28), 0xcccccccc); }
        /* the nested image is a complete VARIANT image: its own clSize covers only itself */
        EQ(rd32(w + (v ? 40 : 32)), 4);              /* (32 + 7) / 8 for the nested R8 variant */
        EQ(rd16(w + (v ? 48 : 40)), VT_R8);
        uctx_free(&c);
        CHECK(V_VT(&dst) == (VT_VARIANT | VT_BYREF));
        CHECK(V_VARIANTREF(&dst) != NULL);
        if (V_VARIANTREF(&dst)) {
            CHECK(V_VT(V_VARIANTREF(&dst)) == VT_R8);
            CHECK(V_R8(V_VARIANTREF(&dst)) == 3.1415);
            if (CoGetMalloc(MEMCTX_TASK, &mal) == S_OK) { EQ(IMalloc_GetSize(mal, V_VARIANTREF(&dst)), sizeof(VARIANT)); IMalloc_Release(mal); }
        }
        vfree(v, &dst);
    }
}

TEST(ums_variant_byref_variant_chain)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT i4, mid, outer, dst;
        memset(&i4, 0, sizeof i4); V_VT(&i4) = VT_I4; V_I4(&i4) = -42;
        i4.wReserved1 = 0x1111;
        memset(&mid, 0, sizeof mid); V_VT(&mid) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&mid) = &i4;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &mid;
        memset(&dst, 0, sizeof dst);
        vcycle(v, MSHCTX_LOCAL, &outer, &dst, 1, "VARIANT byref chain");
        CHECK(V_VT(&dst) == (VT_VARIANT | VT_BYREF));
        if (V_VARIANTREF(&dst)) {
            VARIANT *m = V_VARIANTREF(&dst);
            CHECK(V_VT(m) == (VT_VARIANT | VT_BYREF));
            if (V_VARIANTREF(m)) {
                VARIANT *i = V_VARIANTREF(m);
                CHECK(V_VT(i) == VT_I4 && V_I4(i) == -42 && i->wReserved1 == 0x1111);
            }
        }
        vfree(v, &dst);
    }
}

TEST(ums_variant_byref_variant_holding_bstr_and_empty)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT inner, outer, dst;
        BSTR b = SysAllocString(L"nested bstr");
        memset(&inner, 0, sizeof inner); V_VT(&inner) = VT_BSTR; V_BSTR(&inner) = b;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
        memset(&dst, 0, sizeof dst);
        vcycle(v, MSHCTX_LOCAL, &outer, &dst, 1, "VARIANT byref BSTR");
        CHECK(V_VARIANTREF(&dst) && V_VT(V_VARIANTREF(&dst)) == VT_BSTR && V_BSTR(V_VARIANTREF(&dst)) && !lstrcmpW(V_BSTR(V_VARIANTREF(&dst)), L"nested bstr"));
        vfree(v, &dst);
        /* EMPTY inside */
        memset(&inner, 0, sizeof inner);
        memset(&dst, 0, sizeof dst);
        vcycle(v, MSHCTX_LOCAL, &outer, &dst, 1, "VARIANT byref EMPTY");
        CHECK(V_VARIANTREF(&dst) && V_VT(V_VARIANTREF(&dst)) == VT_EMPTY);
        vfree(v, &dst);
        SysFreeString(b);
    }
}

TEST(ums_variant_byref_variant_inner_not_byref_variant_raises)
{
    /* a VT_VARIANT (by value) can never be the inner value: size and marshal raise E_INVALIDARG */
    int v; NEEDV; WINE_SKIP("no validation");
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT inner, outer; DWORD exc = 0; UCtx c;
        memset(&inner, 0, sizeof inner); V_VT(&inner) = VT_VARIANT;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
        vsize(v, MSHCTX_LOCAL, 0, &outer, &exc);
        EQ(exc, E_INVALIDARG_X);
        uctx_init(&c, 256, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        exc = 0; u_marshal(&c, UF_VARIANT, v, c.buf, &outer, &exc);
        EQ(exc, E_INVALIDARG_X);
        uctx_free(&c);
    }
}

TEST(ums_variant_unmarshal_variant_byref_reuses_destination_memory)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT inner, outer, dst, target;
        memset(&inner, 0, sizeof inner); V_VT(&inner) = VT_I2; V_I2(&inner) = 321;
        memset(&outer, 0, sizeof outer); V_VT(&outer) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&outer) = &inner;
        memset(&target, 0, sizeof target); V_VT(&target) = VT_I4; V_I4(&target) = 1;
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&dst) = &target;
        vcycle(v, MSHCTX_LOCAL, &outer, &dst, 0, "reuse");
        CHECK(V_VARIANTREF(&dst) == &target);                    /* caller-provided variant is overwritten in place */
        CHECK(V_VT(&target) == VT_I2 && V_I2(&target) == 321);
    }
}

/* ---- VT_ARRAY variants -------------------------------------------------------------------------------------- */
static SAFEARRAY *r8_array(void)
{
    SAFEARRAYBOUND sab = { 10, 5 };
    SAFEARRAY *psa = SafeArrayCreate(VT_R8, 1, &sab);
    *(DWORD *)psa->pvData = 0xcafebabe;
    *((DWORD *)psa->pvData + 1) = 0xdeadbeef;
    return psa;
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) -- Wine accepts 152 (+8 / +16 for win64) */
TEST(ums_variant_array_size_and_wire)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAY *psa = r8_array(), *dsa; VARIANT src, dst; ULONG size, want;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UI4 | VT_ARRAY; V_ARRAY(&src) = psa;
        memset(&dst, 0, sizeof dst);
        size = vcycle(v, MSHCTX_DIFFERENTMACHINE, &src, &dst, 1, "ARRAY");
        want = ums_size_safearray(v, psa, v ? 32 : 24);
        if (!t_is_wine()) EQ(size, want);
        if (!v && !t_is_wine()) EQ(size, 160);
        CHECK(V_VT(&dst) == (VT_UI4 | VT_ARRAY));
        dsa = V_ARRAY(&dst);
        CHECK(dsa != NULL);
        if (dsa) {
            VARTYPE vt = 0;
            EQ(dsa->cDims, 1); EQ(dsa->rgsabound[0].lLbound, 5); EQ(dsa->rgsabound[0].cElements, 10);
            CHECK(SafeArrayGetVartype(dsa, &vt) == S_OK && vt == VT_R8);          /* the array keeps its own vartype */
            CHECK(!memcmp(dsa->pvData, psa->pvData, 80));
        }
        vfree(v, &dst);
        SafeArrayDestroy(psa);
    }
}

TEST(ums_variant_array_byref_size_and_wire)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAY *psa = r8_array(); VARIANT src, dst; ULONG size, want; IMalloc *mal = NULL;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UI4 | VT_ARRAY | VT_BYREF; V_ARRAYREF(&src) = &psa;
        memset(&dst, 0, sizeof dst);
        size = vcycle(v, MSHCTX_DIFFERENTMACHINE, &src, &dst, 1, "ARRAY byref");
        want = ums_size_safearray(v, psa, v ? 40 : 28);
        if (!t_is_wine()) EQ(size, want);
        if (!v && !t_is_wine()) EQ(size, 168);
        CHECK(V_VT(&dst) == (VT_UI4 | VT_ARRAY | VT_BYREF));
        if (V_ARRAYREF(&dst) && *V_ARRAYREF(&dst)) {
            SAFEARRAY *d = *V_ARRAYREF(&dst);
            EQ(d->cDims, 1); CHECK(!memcmp(d->pvData, psa->pvData, 80));
        } else CHECK(0);
        if (V_ARRAYREF(&dst) && CoGetMalloc(MEMCTX_TASK, &mal) == S_OK) { EQ(IMalloc_GetSize(mal, V_ARRAYREF(&dst)), sizeof(SAFEARRAY *)); IMalloc_Release(mal); }
        vfree(v, &dst);
        SafeArrayDestroy(psa);
    }
}

TEST(ums_variant_array_byref_reuses_static_array)
{
    /* a BYREF destination holding a FADF_STATIC array of the same shape keeps both descriptor and storage */
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAYBOUND sab = { 10, 5 };
        SAFEARRAY *psa = SafeArrayCreate(VT_R8, 1, &sab), *a2 = SafeArrayCreate(VT_I8, 1, &sab), *a2_copy = a2; VARIANT src, dst; void *mem;
        *(DWORD *)psa->pvData = 0xcafebabe;
        psa->fFeatures |= FADF_STATIC; a2->fFeatures |= FADF_STATIC;
        mem = a2->pvData;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UI4 | VT_ARRAY | VT_BYREF; V_ARRAYREF(&src) = &psa;
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_UI4 | VT_ARRAY | VT_BYREF; V_ARRAYREF(&dst) = &a2;
        vcycle(v, MSHCTX_LOCAL, &src, &dst, 0, "static reuse");
        CHECK(a2 == a2_copy);
        CHECK(a2->pvData == mem);
        CHECK(*(DWORD *)a2->pvData == 0xcafebabe);
        psa->fFeatures &= ~FADF_STATIC; a2->fFeatures &= ~FADF_STATIC;
        SafeArrayDestroy(psa); SafeArrayDestroy(a2);
    }
}

TEST(ums_variant_array_null_pointer)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT src, dst; ULONG size;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_I4 | VT_ARRAY; V_ARRAY(&src) = NULL;
        memset(&dst, 0, sizeof dst);
        size = vcycle(v, MSHCTX_LOCAL, &src, &dst, !t_is_wine(), "NULL ARRAY");
        if (!t_is_wine()) EQ(size, v ? 32 : 24);    /* header + the NULL dword/qword; no inner SAFEARRAY wire */
        CHECK(V_VT(&dst) == (VT_I4 | VT_ARRAY) && V_ARRAY(&dst) == NULL);
        vfree(v, &dst);
    }
}

TEST(ums_variant_array_switch_value_and_vt)
{
    /* the switch dword of an array variant keeps only the ARRAY/BYREF bits; the full vt stays in the vt field */
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAYBOUND sab = { 2, 0 };
        SAFEARRAY *psa = SafeArrayCreate(VT_I2, 1, &sab); VARIANT src; UCtx c; ULONG size;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_I2 | VT_ARRAY; V_ARRAY(&src) = psa;
        size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        EQ(rd16(c.buf + 8), VT_I2 | VT_ARRAY);
        EQ(rd32(c.buf + 16), VT_ARRAY);
        uctx_free(&c);
        V_VT(&src) = VT_I2 | VT_ARRAY | VT_BYREF; V_ARRAYREF(&src) = &psa;
        size = vsize(v, MSHCTX_LOCAL, 0, &src, NULL);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        EQ(rd16(c.buf + 8), VT_I2 | VT_ARRAY | VT_BYREF);
        EQ(rd32(c.buf + 16), VT_ARRAY | VT_BYREF);
        uctx_free(&c);
        SafeArrayDestroy(psa);
    }
}

TEST(ums_variant_array_free_destroys_array)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAYBOUND sab = { 3, 0 };
        VARIANT dst; SAFEARRAY *psa = SafeArrayCreate(VT_BSTR, 1, &sab);
        BSTR *e = psa->pvData; e[1] = SysAllocString(L"to be freed with the array");
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_BSTR | VT_ARRAY; V_ARRAY(&dst) = psa;
        vfree(v, &dst);
        if (!t_is_wine()) CHECK(V_ARRAY(&dst) == NULL);   /* LPSAFEARRAY_UserFree zeroes the pointer */
    }
}

/* ---- interface pointers ------------------------------------------------------------------------------------- */
TEST(ums_variant_interface_null)
{
    static const VARTYPE t[] = { VT_UNKNOWN, VT_DISPATCH };
    unsigned i; int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (i = 0; i < 2; i++) {
            VARIANT src, dst; ULONG size;
            memset(&src, 0, sizeof src); V_VT(&src) = t[i];
            memset(&dst, 0, sizeof dst);
            size = vcycle(v, MSHCTX_DIFFERENTMACHINE, &src, &dst, 1, "NULL iface");
            EQ(size, v ? 32 : 24);
            CHECK(V_VT(&dst) == t[i] && V_UNKNOWN(&dst) == NULL);
            /* BYREF with a NULL pointee */
            { IUnknown *nul = NULL; VARIANT r;
              memset(&r, 0, sizeof r); V_VT(&r) = t[i] | VT_BYREF; V_UNKNOWNREF(&r) = &nul;
              memset(&dst, 0, sizeof dst);
              size = vcycle(v, MSHCTX_DIFFERENTMACHINE, &r, &dst, 1, "NULL iface byref");
              EQ(size, v ? 40 : 28);
              CHECK(V_VT(&dst) == (t[i] | VT_BYREF) && V_UNKNOWNREF(&dst) && *V_UNKNOWNREF(&dst) == NULL);
              vfree(v, &dst); }
        }
}

TEST(ums_variant_interface_null_clears_destination_pointer)
{
    int v; NEEDV;
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        UObj *o = uobj_new(); VARIANT src, dst;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UNKNOWN;
        memset(&dst, 0, sizeof dst); V_VT(&dst) = VT_UNKNOWN; V_UNKNOWN(&dst) = UOBJ_UNK(o);
        IUnknown_AddRef(UOBJ_UNK(o));
        vcycle(v, MSHCTX_LOCAL, &src, &dst, 0, "iface clear");
        CHECK(V_VT(&dst) == VT_UNKNOWN && V_UNKNOWN(&dst) == NULL);
        EQ(o->refs, 1);                              /* the destination's reference was released */
        IUnknown_Release(UOBJ_UNK(o));
    }
}

/* adapted from Wine dlls/oleaut32/tests/usrmarshal.c (LGPL-2.1+) -- real object, standard marshaling */
TEST(ums_variant_interface_roundtrip_standard_marshaling)
{
    int v; NEEDV;
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) { t_skip("CoInitializeEx failed"); return; }
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        UObj *o = uobj_new(); VARIANT src, dst; UCtx c; ULONG size; unsigned char *m, *w;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UNKNOWN; V_UNKNOWN(&src) = UOBJ_UNK(o);
        size = vsize(v, MSHCTX_DIFFERENTMACHINE, 0, &src, NULL);
        CHECKF(size > 40, "size %lu", size);
        uctx_init(&c, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        t_check(m != NULL && m <= c.buf + size, __FILE__, __LINE__, "marshal within UserSize", "v64=%d", v);
        if (!t_is_wine()) EQ(o->refs, 2);            /* the table-strong marshal data holds a reference */
        w = c.buf;
        EQ(rd32(w), (unsigned)((m - c.buf) + 7) >> 3);
        EQ(rd16(w + 8), VT_UNKNOWN);
        EQ(rd32(w + 16), VT_UNKNOWN);
        if (!v) {
            if (!t_is_wine()) EQ(rd32(w + 20), (unsigned)(((ULONG_PTR)o + offsetof(UObj, iface)) | 1));
            else CHECK(rd32(w + 20) != 0);
            EQ(rd32(w + 24), (unsigned)(m - c.buf) - 0x20);
            EQ(rd32(w + 28), (unsigned)(m - c.buf) - 0x20);
            EQ(rd32(w + 32), 0x574f454d);            /* 'MEOW' OBJREF signature */
        }
        uctx_reread(&c, size, MSHCTX_DIFFERENTMACHINE);
        memset(&dst, 0, sizeof dst);
        EQ(u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, NULL), m);
        CHECK(V_VT(&dst) == VT_UNKNOWN);
        CHECK(V_UNKNOWN(&dst) == UOBJ_UNK(o));         /* same apartment: the very same pointer comes back */
        vfree(v, &dst);                                /* releases the reference taken by marshaling */
        t_check(o->refs == 1, __FILE__, __LINE__, "no leaked references", "v64=%d refs %ld", v, o->refs);
        uctx_free(&c);
        IUnknown_Release(UOBJ_UNK(o));
    }
    CoUninitialize();
}

TEST(ums_variant_interface_byref_roundtrip)
{
    int v; NEEDV;
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) { t_skip("CoInitializeEx failed"); return; }
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        UObj *o = uobj_new(); IUnknown *p = UOBJ_UNK(o); VARIANT src, dst; IMalloc *mal = NULL;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_UNKNOWN | VT_BYREF; V_UNKNOWNREF(&src) = &p;
        memset(&dst, 0, sizeof dst);
        vcycle(v, MSHCTX_DIFFERENTMACHINE, &src, &dst, 0, "iface byref");
        CHECK(V_VT(&dst) == (VT_UNKNOWN | VT_BYREF));
        CHECK(V_UNKNOWNREF(&dst) && *V_UNKNOWNREF(&dst) == p);
        if (V_UNKNOWNREF(&dst) && CoGetMalloc(MEMCTX_TASK, &mal) == S_OK) { EQ(IMalloc_GetSize(mal, V_UNKNOWNREF(&dst)), sizeof(void *)); IMalloc_Release(mal); }
        vfree(v, &dst);
        t_check(o->refs == 1, __FILE__, __LINE__, "no leaked references", "v64=%d refs %ld", v, o->refs);
        IUnknown_Release(UOBJ_UNK(o));
    }
    CoUninitialize();
}

TEST(ums_variant_interface_in_array_roundtrip)
{
    SAFEARRAYBOUND sab = { 4, 3 }; int v; NEEDV;
    if (CoInitializeEx(NULL, COINIT_APARTMENTTHREADED) != S_OK) { t_skip("CoInitializeEx failed"); return; }
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        UObj *o = uobj_new(); SAFEARRAY *src = SafeArrayCreate(VT_UNKNOWN, 1, &sab), *dst = NULL; IUnknown **e = src->pvData;
        UCtx z, c; ULONG size; unsigned char *m, *u; int i;
        e[0] = UOBJ_UNK(o); IUnknown_AddRef(e[0]);
        e[2] = UOBJ_UNK(o); IUnknown_AddRef(e[2]);
        uctx_init(&z, 0, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_DIFFERENTMACHINE, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        t_check(m != NULL && m <= c.buf + size, __FILE__, __LINE__, "marshal within UserSize", "v64=%d", v);
        uctx_reread(&c, size, MSHCTX_DIFFERENTMACHINE);
        u = u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL);
        CHECK(u == m);
        CHECK(dst != NULL);
        if (dst) {
            IUnknown **d = dst->pvData;
            for (i = 0; i < 4; i++) CHECK(d[i] == e[i]);
        }
        { UCtx f; uctx_init(&f, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&f, UF_SAFEARRAY, v, &dst, NULL); uctx_free(&f); }
        uctx_free(&c);
        SafeArrayDestroy(src);                          /* releases the two element references */
        if (!t_is_wine()) t_check(o->refs == 1, __FILE__, __LINE__, "no leaked references", "v64=%d refs %ld", v, o->refs);
        else if (o->refs != 1) { t_note("emulator leaks %ld references in interface arrays", o->refs - 1); while (o->refs > 1) IUnknown_Release(UOBJ_UNK(o)); }
        IUnknown_Release(UOBJ_UNK(o));
    }
    CoUninitialize();
}
