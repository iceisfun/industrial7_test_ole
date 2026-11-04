/* gap_09: VARIANT_UserSize64 (oleaut32.dll), derived from the Win10 decompilation.
 *
 *   size(flags, off, pv):  pv == NULL -> off (not even aligned)
 *     r = align8(off) + 0x14; r = align8(r)        => header ends at align8(off) + 24
 *     sw = (vt & VT_ARRAY) ? (vt & 0x6000) : vt     (an array VARIANT ignores every other type bit)
 *     !(sw & VT_BYREF): sw == VT_VARIANT -> raise E_INVALIDARG (0x80070057)
 *     VT_BYREF: pointer (pvarVal) NULL -> raise RPC_X_NULL_REF_POINTER (0x6f4); r += 8; sw &= ~VT_BYREF
 *     then by sw (r = header end, plus 8 for BYREF):
 *       VT_EMPTY/VT_NULL        +0 (raise DISP_E_BADVARTYPE 0x80020008 when BYREF)
 *       I2, BOOL, UI2           +2       I4, R4, ERROR, UI4, INT, UINT, INT_PTR, UINT_PTR   +4
 *       I1, UI1                 +1       R8, CY, DATE, I8, UI8                               +8 (8-aligned)
 *       DECIMAL                 +16 (8-aligned)
 *       BSTR                    +8, then BSTR_UserSize64(r) when the BSTR pointer is non-NULL
 *       UNKNOWN, DISPATCH       +8, then WdtpInterfacePointer_UserSize64 when the pointer is non-NULL
 *       ARRAY (0x2000)          +8, then LPSAFEARRAY_UserSize64(r) when the SAFEARRAY* is non-NULL
 *       VARIANT (BYREF only)    +8, then VARIANT_UserSize64(r, inner)
 *       RECORD (0x24)           raises RPC_X_NULL_REF_POINTER when pRecInfo is NULL (by value)
 *       anything else           raise DISP_E_BADVARTYPE (0x80020008) */
#include "g9.h"

#define DISP_E_BADVARTYPE_ 0x80020008u
#define E_INVALIDARG_      0x80070057u
#define X_NULLREF          0x6f4u

typedef BSTR (WINAPI *sysalloc_t)(const OLECHAR *);
typedef void (WINAPI *sysfree_t)(BSTR);
typedef SAFEARRAY *(WINAPI *savec_t)(VARTYPE, LONG, ULONG);
typedef HRESULT (WINAPI *sadestroy_t)(SAFEARRAY *);

static void *fv64(void) { return UM_FNIN("oleaut32.dll", "VARIANT_UserSize64"); }

static DWORD vsize(void *f, ULONG ctx, ULONG off, VARIANT *v, ULONG *out)
{
    UMC c;
    um_init(&c, NULL, 0, ctx);
    return um_x_size(f, UMF(&c), off, v, out);
}

static void setv(VARIANT *v, VARTYPE vt)
{
    memset(v, 0, sizeof *v);
    V_VT(v) = vt;
}

TEST(gap9_variant_usersize64_header_is_24_after_8_alignment)
{
    void *f = fv64();
    ULONG off, r;
    REQUIRE(f);
    for (off = 0; off < 40; off++) {
        VARIANT v;
        setv(&v, VT_EMPTY);
        XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
        CHECKF(r == UM_ALIGN(off, 8) + 24, "off %lu got %lu", off, r);
        setv(&v, VT_NULL);
        XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
        CHECKF(r == UM_ALIGN(off, 8) + 24, "VT_NULL off %lu got %lu", off, r);
    }
}

TEST(gap9_variant_usersize64_null_pointer_returns_offset_unaligned)
{
    void *f = fv64();
    ULONG off, r, ctx;
    REQUIRE(f);
    for (ctx = 0; ctx <= C_CONT; ctx++)
        for (off = 0; off < 20; off++) {
            XEQ(vsize(f, ctx, off, NULL, &r), 0);
            CHECKF(r == off, "ctx %lu off %lu got %lu", ctx, off, r);
        }
}

TEST(gap9_variant_usersize64_scalar_payload_table)
{
    static const struct { VARTYPE vt; ULONG add; } t[] = {
        { VT_EMPTY, 0 }, { VT_NULL, 0 }, { VT_I2, 2 }, { VT_I4, 4 }, { VT_R4, 4 }, { VT_R8, 8 }, { VT_CY, 8 }, { VT_DATE, 8 },
        { VT_ERROR, 4 }, { VT_BOOL, 2 }, { VT_DECIMAL, 16 }, { VT_I1, 1 }, { VT_UI1, 1 }, { VT_UI2, 2 }, { VT_UI4, 4 },
        { VT_I8, 8 }, { VT_UI8, 8 }, { VT_INT, 4 }, { VT_UINT, 4 }, { VT_INT_PTR, 4 }, { VT_UINT_PTR, 4 },
        { VT_BSTR, 8 }, { VT_DISPATCH, 8 }, { VT_UNKNOWN, 8 },     /* NULL pointers: only the 8-byte marker */
    };
    void *f = fv64();
    unsigned i; ULONG off, r;
    REQUIRE(f);
    for (i = 0; i < sizeof t / sizeof *t; i++)
        for (off = 0; off < 18; off += 5) {
            VARIANT v;
            setv(&v, t[i].vt);
            XEQ(vsize(f, C_DIFFM, off, &v, &r), 0);
            CHECKF(r == UM_ALIGN(off, 8) + 24 + t[i].add, "vt %u off %lu got %lu want %lu", t[i].vt, off, r, UM_ALIGN(off, 8) + 24 + t[i].add);
        }
}

TEST(gap9_variant_usersize64_scalar_size_is_context_independent)
{
    static const VARTYPE vts[] = { VT_I2, VT_I4, VT_R8, VT_DECIMAL, VT_BSTR, VT_ERROR };
    static const ULONG hi[] = { 0, 0x100, 0x00100000, 0xFFFFFF00u };
    void *f = fv64();
    unsigned i, k; ULONG ctx, r0, r;
    REQUIRE(f);
    for (i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v; setv(&v, vts[i]);
        XEQ(vsize(f, C_LOCAL, 3, &v, &r0), 0);
        for (ctx = 0; ctx <= C_CONT; ctx++) {
            XEQ(vsize(f, ctx, 3, &v, &r), 0);
            CHECKF(r == r0, "vt %u ctx %lu: %lu vs %lu", vts[i], ctx, r, r0);
            for (k = 0; k < sizeof hi / sizeof *hi; k++) {
                UMC c; um_init(&c, NULL, 0, ctx); c.cb.Flags = (c.cb.Flags & 0xff) | hi[k];
                XEQ(um_x_size(f, UMF(&c), 3, &v, &r), 0);
                CHECKF(r == r0, "vt %u flags %#lx: %lu vs %lu", vts[i], c.cb.Flags, r, r0);
            }
        }
    }
}

TEST(gap9_variant_usersize64_reserved_fields_and_payload_values_do_not_matter)
{
    void *f = fv64();
    VARIANT a, b; ULONG ra, rb;
    REQUIRE(f);
    setv(&a, VT_I4); setv(&b, VT_I4);
    a.wReserved1 = 1; a.wReserved2 = 2; a.wReserved3 = 3; a.lVal = 0x7fffffff;
    b.lVal = 0;
    XEQ(vsize(f, C_LOCAL, 0, &a, &ra), 0);
    XEQ(vsize(f, C_LOCAL, 0, &b, &rb), 0);
    CHECK(ra == rb && ra == 28);
}

TEST(gap9_variant_usersize64_byref_scalar_adds_pointer_marker)
{
    static const struct { VARTYPE vt; ULONG add; } t[] = {
        { VT_I2, 2 }, { VT_I4, 4 }, { VT_R4, 4 }, { VT_R8, 8 }, { VT_CY, 8 }, { VT_DATE, 8 }, { VT_ERROR, 4 }, { VT_BOOL, 2 }, { VT_DECIMAL, 16 },
        { VT_I1, 1 }, { VT_UI1, 1 }, { VT_UI2, 2 }, { VT_UI4, 4 }, { VT_I8, 8 }, { VT_UI8, 8 }, { VT_INT, 4 }, { VT_UINT, 4 },
    };
    void *f = fv64();
    unsigned i; ULONG off, r;
    unsigned char storage[16] = { 0 };
    REQUIRE(f);
    for (i = 0; i < sizeof t / sizeof *t; i++)
        for (off = 0; off < 18; off += 5) {
            VARIANT v;
            setv(&v, (VARTYPE)(t[i].vt | VT_BYREF));
            v.pvarVal = (VARIANT *)storage;
            XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
            CHECKF(r == UM_ALIGN(off, 8) + 24 + 8 + t[i].add, "byref vt %u off %lu got %lu", t[i].vt, off, r);
        }
}

TEST(gap9_variant_usersize64_byref_null_pointer_raises_null_ref)
{
    static const VARTYPE vts[] = { VT_I2, VT_I4, VT_R8, VT_BSTR, VT_UNKNOWN, VT_DISPATCH, VT_VARIANT, VT_DECIMAL, VT_I1, VT_ARRAY | VT_I4 };
    void *f = fv64();
    unsigned i; ULONG r;
    REQUIRE(f);
    for (i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT v;
        setv(&v, (VARTYPE)(vts[i] | VT_BYREF));
        XEQ(vsize(f, C_LOCAL, 0, &v, &r), X_NULLREF);
    }
}

TEST(gap9_variant_usersize64_byref_empty_or_null_raises_badvartype)
{
    static const VARTYPE vts[] = { VT_EMPTY | VT_BYREF, VT_NULL | VT_BYREF };
    void *f = fv64();
    unsigned i; ULONG r; unsigned char storage[16] = { 0 };
    REQUIRE(f);
    for (i = 0; i < 2; i++) {
        VARIANT v;
        setv(&v, vts[i]); v.pvarVal = (VARIANT *)storage;
        XEQ(vsize(f, C_LOCAL, 0, &v, &r), DISP_E_BADVARTYPE_);
    }
}

TEST(gap9_variant_usersize64_by_value_variant_type_is_invalid_arg)
{
    void *f = fv64();
    VARIANT v; ULONG r, off;
    REQUIRE(f);
    for (off = 0; off < 9; off += 4) {
        setv(&v, VT_VARIANT);
        XEQ(vsize(f, C_LOCAL, off, &v, &r), E_INVALIDARG_);
    }
}

TEST(gap9_variant_usersize64_unknown_types_raise_badvartype)
{
    static const VARTYPE bad[] = {
        15, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 39, 40, 41, 63, 64, 65, 66, 127,
        VT_VECTOR | VT_I4, VT_VECTOR | VT_UI1, VT_VECTOR, 0x8000, 0x8003, 0xffff & ~0x6000 & ~0x2000, 0x0100, 0x0fff,
        VT_ILLEGAL & 0xfffe,
    };
    void *f = fv64();
    unsigned i; ULONG r;
    REQUIRE(f);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        VARIANT v;
        if ((bad[i] & VT_ARRAY) || (bad[i] & VT_BYREF)) continue;
        setv(&v, bad[i]);
        XEQ(vsize(f, C_LOCAL, 0, &v, &r), DISP_E_BADVARTYPE_);
    }
}

TEST(gap9_variant_usersize64_byref_unknown_types_raise_badvartype)
{
    static const VARTYPE bad[] = { 15, 24, 25, 26, 27, 28, 29, 30, 31, 32, 35, 63, 64, 0x1003, 0x8003, 0x0fff };
    void *f = fv64();
    unsigned i; ULONG r; unsigned char storage[16] = { 0 };
    REQUIRE(f);
    for (i = 0; i < sizeof bad / sizeof *bad; i++) {
        VARIANT v;
        setv(&v, (VARTYPE)(bad[i] | VT_BYREF)); v.pvarVal = (VARIANT *)storage;
        XEQ(vsize(f, C_LOCAL, 0, &v, &r), DISP_E_BADVARTYPE_);
    }
}

TEST(gap9_variant_usersize64_array_flag_hides_element_type)
{
    /* with VT_ARRAY set only bits 0x6000 are kept, so any low bits behave identically (NULL array -> 8-byte marker) */
    static const VARTYPE low[] = { 0, 1, 2, 3, 8, 12, 13, 0x0f, 0x24, 0x100, 0x800, 0x1000, 0x1fff, 0x0ff0, VT_I4 | VT_VECTOR };
    void *f = fv64();
    unsigned i; ULONG r, off;
    REQUIRE(f);
    for (i = 0; i < sizeof low / sizeof *low; i++)
        for (off = 0; off < 10; off += 3) {
            VARIANT v;
            setv(&v, (VARTYPE)(VT_ARRAY | low[i]));
            v.parray = NULL;
            XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
            CHECKF(r == UM_ALIGN(off, 8) + 24 + 8, "vt %#x off %lu got %lu", V_VT(&v), off, r);
        }
}

TEST(gap9_variant_usersize64_array_composes_with_safearray_size)
{
    void *f = fv64(), *sf = UM_FNIN("oleaut32.dll", "LPSAFEARRAY_UserSize64");
    savec_t cv = (savec_t)t_proc_in("oleaut32.dll", "SafeArrayCreateVector");
    sadestroy_t dv = (sadestroy_t)t_proc_in("oleaut32.dll", "SafeArrayDestroy");
    static const struct { VARTYPE vt; ULONG n; } t[] = { { VT_I4, 1 }, { VT_I4, 10 }, { VT_UI1, 7 }, { VT_R8, 3 }, { VT_I2, 0 } };
    unsigned i; ULONG off, ctx;
    REQUIRE(f); REQUIRE(sf); REQUIRE(cv); REQUIRE(dv);
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        SAFEARRAY *sa = cv(t[i].vt, 0, t[i].n);
        REQUIRE(sa);
        for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2)
            for (off = 0; off < 10; off += 3) {
                VARIANT v, vr; ULONG r, rr, want, ws; LPSAFEARRAY ref = sa;
                setv(&v, (VARTYPE)(VT_ARRAY | t[i].vt)); v.parray = sa;
                XEQ(vsize(f, ctx, off, &v, &r), 0);
                {   UMC c; um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(sf, UMF(&c), UM_ALIGN(off, 8) + 24 + 8, &ref, &ws), 0); }
                CHECKF(r == ws, "by value: vt %u n %lu ctx %lu off %lu got %lu want %lu", t[i].vt, t[i].n, ctx, off, r, ws);
                setv(&vr, (VARTYPE)(VT_ARRAY | VT_BYREF | t[i].vt)); vr.pparray = &ref;
                XEQ(vsize(f, ctx, off, &vr, &rr), 0);
                {   UMC c; um_init(&c, NULL, 0, ctx);
                    XEQ(um_x_size(sf, UMF(&c), UM_ALIGN(off, 8) + 24 + 8 + 8, &ref, &want), 0); }
                CHECKF(rr == want, "by ref: vt %u n %lu ctx %lu off %lu got %lu want %lu", t[i].vt, t[i].n, ctx, off, rr, want);
                CHECK(r > UM_ALIGN(off, 8) + 24 + 8);
            }
        dv(sa);
    }
}

TEST(gap9_variant_usersize64_byref_null_array_pointer)
{
    void *f = fv64();
    VARIANT v; LPSAFEARRAY none = NULL; ULONG r, off;
    REQUIRE(f);
    for (off = 0; off < 10; off += 3) {
        setv(&v, VT_ARRAY | VT_BYREF | VT_I4); v.pparray = &none;
        XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
        CHECKF(r == UM_ALIGN(off, 8) + 24 + 8 + 8, "off %lu got %lu", off, r);
    }
}

TEST(gap9_variant_usersize64_bstr_composes_with_bstr_size)
{
    void *f = fv64(), *bf = UM_FNIN("oleaut32.dll", "BSTR_UserSize64");
    sysalloc_t sa = (sysalloc_t)t_proc_in("oleaut32.dll", "SysAllocString");
    sysfree_t sfree = (sysfree_t)t_proc_in("oleaut32.dll", "SysFreeString");
    static const WCHAR *strs[] = { L"", L"a", L"abc", L"0123456789abcdef", L"unicode \x00e9\x20ac\x4e2d" };
    unsigned i; ULONG off, ctx;
    REQUIRE(f); REQUIRE(bf); REQUIRE(sa); REQUIRE(sfree);
    for (i = 0; i < sizeof strs / sizeof *strs; i++) {
        BSTR s = sa(strs[i]);
        REQUIRE(s);
        for (ctx = C_LOCAL; ctx <= C_DIFFM; ctx += 2)
            for (off = 0; off < 10; off += 3) {
                VARIANT v; ULONG r, want; BSTR ref = s; UMC c;
                setv(&v, VT_BSTR); v.bstrVal = s;
                XEQ(vsize(f, ctx, off, &v, &r), 0);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(bf, UMF(&c), UM_ALIGN(off, 8) + 24 + 8, &ref, &want), 0);
                CHECKF(r == want, "by value: str %u ctx %lu off %lu got %lu want %lu", i, ctx, off, r, want);
                setv(&v, VT_BSTR | VT_BYREF); v.pbstrVal = &ref;
                XEQ(vsize(f, ctx, off, &v, &r), 0);
                um_init(&c, NULL, 0, ctx);
                XEQ(um_x_size(bf, UMF(&c), UM_ALIGN(off, 8) + 24 + 8 + 8, &ref, &want), 0);
                CHECKF(r == want, "by ref: str %u ctx %lu off %lu got %lu want %lu", i, ctx, off, r, want);
            }
        sfree(s);
    }
}

TEST(gap9_variant_usersize64_byref_null_bstr_is_marker_only)
{
    void *f = fv64();
    VARIANT v; BSTR none = NULL; ULONG r;
    REQUIRE(f);
    setv(&v, VT_BSTR | VT_BYREF); v.pbstrVal = &none;
    XEQ(vsize(f, C_LOCAL, 0, &v, &r), 0);
    CHECK(r == 24 + 8 + 8);
}

TEST(gap9_variant_usersize64_byref_null_interface_is_marker_only)
{
    static const VARTYPE vts[] = { VT_UNKNOWN, VT_DISPATCH };
    void *f = fv64();
    unsigned i; ULONG r;
    REQUIRE(f);
    for (i = 0; i < 2; i++) {
        VARIANT v; IUnknown *none = NULL;
        setv(&v, (VARTYPE)(vts[i] | VT_BYREF)); v.ppunkVal = &none;
        XEQ(vsize(f, C_LOCAL, 0, &v, &r), 0);
        CHECKF(r == 24 + 8 + 8, "vt %u got %lu", vts[i], r);
    }
}

TEST(gap9_variant_usersize64_byref_variant_recurses)
{
    static const VARTYPE inner[] = { VT_EMPTY, VT_I2, VT_I4, VT_R8, VT_DECIMAL, VT_BSTR, VT_UNKNOWN };
    void *f = fv64();
    unsigned i; ULONG off;
    REQUIRE(f);
    for (i = 0; i < sizeof inner / sizeof *inner; i++)
        for (off = 0; off < 10; off += 3) {
            VARIANT in, v; ULONG r, ri;
            setv(&in, inner[i]);
            setv(&v, VT_VARIANT | VT_BYREF); v.pvarVal = &in;
            XEQ(vsize(f, C_LOCAL, off, &v, &r), 0);
            XEQ(vsize(f, C_LOCAL, UM_ALIGN(off, 8) + 24 + 8 + 8, &in, &ri), 0);   /* Win10: one extra 8-byte slot before the referenced variant */
            CHECKF(r == ri, "inner vt %u off %lu: %lu vs %lu", inner[i], off, r, ri);
        }
}

TEST(gap9_variant_usersize64_byref_variant_chain)
{
    void *f = fv64();
    VARIANT leaf, mid, top; ULONG r, rl;
    REQUIRE(f);
    setv(&leaf, VT_I8);
    setv(&mid, VT_VARIANT | VT_BYREF); mid.pvarVal = &leaf;
    setv(&top, VT_VARIANT | VT_BYREF); top.pvarVal = &mid;
    XEQ(vsize(f, C_LOCAL, 0, &leaf, &rl), 0);
    CHECK(rl == 24 + 8);
    XEQ(vsize(f, C_LOCAL, 0, &mid, &r), 0);
    CHECKF(r == 24 + 8 + 8 + (24 + 8), "one level: %lu", r);                   /* mid header + ptr, then leaf at offset 32 */
    XEQ(vsize(f, C_LOCAL, 0, &top, &r), 0);
    CHECKF(r == 24 + 8 + 8 + (24 + 8 + 8 + (24 + 8)), "two levels: %lu", r);
}

TEST(gap9_variant_usersize64_byref_variant_inner_by_value_variant_raises)
{
    void *f = fv64();
    VARIANT in, v; ULONG r;
    REQUIRE(f);
    setv(&in, VT_VARIANT);
    setv(&v, VT_VARIANT | VT_BYREF); v.pvarVal = &in;
    XEQ(vsize(f, C_LOCAL, 0, &v, &r), E_INVALIDARG_);
}

TEST(gap9_variant_usersize64_byref_variant_inner_bad_type_raises)
{
    void *f = fv64();
    VARIANT in, v; ULONG r;
    REQUIRE(f);
    setv(&in, 0x1f);
    setv(&v, VT_VARIANT | VT_BYREF); v.pvarVal = &in;
    XEQ(vsize(f, C_LOCAL, 0, &v, &r), DISP_E_BADVARTYPE_);
}

TEST(gap9_variant_usersize64_record_without_recinfo_raises_null_ref)
{
    void *f = fv64();
    VARIANT v; ULONG r;
    REQUIRE(f);
    setv(&v, VT_RECORD);
    v.pvRecord = NULL; v.pRecInfo = NULL;
    XEQ(vsize(f, C_LOCAL, 0, &v, &r), X_NULLREF);
}

TEST(gap9_variant_usersize64_offset_overflow_raises)
{
    /* the running offset is added with overflow checking: a start near 2^32 cannot take a header */
    static const ULONG offs[] = { 0xFFFFFFF0u, 0xFFFFFFF8u, 0xFFFFFFFFu, 0xFFFFFFE9u };
    void *f = fv64();
    unsigned i; ULONG r;
    REQUIRE(f);
    for (i = 0; i < sizeof offs / sizeof *offs; i++) {
        VARIANT v; DWORD code;
        setv(&v, VT_I4);
        code = vsize(f, C_LOCAL, offs[i], &v, &r);
        CHECKF(code != 0, "start %#lx: header + payload overflows 32 bits and must raise (got size %#lx)", offs[i], r);
        if (code) t_note("start %#lx raised %#lx", offs[i], code);
    }
}
