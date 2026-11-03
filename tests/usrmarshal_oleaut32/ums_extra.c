/* Arithmetic-overflow protection, purity of the Size/Marshal helpers, length sweeps and repeated unmarshaling.
 *
 * Safe_Length_Align / ULongAdd_RpcRaiseIfFailed / ULongMult_RpcRaiseIfFailed (decomp) raise RpcRaiseException(0x80010010)
 * ("-0x7ffefff0") when a 32-bit offset computation wraps. These are pure arithmetic checks on the start offset passed to
 * *_UserSize, so they are deterministic and need no memory. */
#include "ums.h"

#define OVF 0x80010010u

static ULONG ssize(int fam, int v, ULONG start, void *obj, DWORD *exc)
{
    UCtx c; ULONG s;
    uctx_init(&c, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
    s = u_size(&c, fam, v, start, obj, exc);
    uctx_free(&c);
    return s;
}

TEST(ums_bstr_size_offset_overflow)
{
    /* NULL BSTR adds align + 12 (NDR32) / 16 (NDR64) */
    static const struct { int v; ULONG start; int raises; ULONG want; } t[] = {
        { 0, 0xFFFFFFF0u, 0, 0xFFFFFFFCu }, { 0, 0xFFFFFFF1u, 1, 0 }, { 0, 0xFFFFFFF4u, 1, 0 }, { 0, 0xFFFFFFFCu, 1, 0 }, { 0, 0xFFFFFFFFu, 1, 0 },
        { 0, 0xFFFFFFE0u, 0, 0xFFFFFFECu },
        { 1, 0xFFFFFFE8u, 0, 0xFFFFFFF8u }, { 1, 0xFFFFFFE9u, 1, 0 }, { 1, 0xFFFFFFF0u, 1, 0 }, { 1, 0xFFFFFFFFu, 1, 0 },
    };
    unsigned i; NEED_FAM(UF_BSTR); WINE_SKIP("no overflow checks");
    for (i = 0; i < ARRAY_N(t); i++) {
        BSTR nb = NULL; DWORD exc = 0; ULONG got;
        if (!u_have(UF_BSTR, t[i].v)) continue;
        got = ssize(UF_BSTR, t[i].v, t[i].start, &nb, &exc);
        if (t[i].raises) t_check(exc == OVF, __FILE__, __LINE__, "BSTR_UserSize overflow", "v64=%d start %08lx exc 0x%lx", t[i].v, t[i].start, exc);
        else t_check(exc == 0 && got == t[i].want, __FILE__, __LINE__, "BSTR_UserSize near wrap", "v64=%d start %08lx exc 0x%lx got %08lx want %08lx", t[i].v, t[i].start, exc, got, t[i].want);
    }
}

TEST(ums_variant_size_offset_overflow)
{
    static const struct { int v; VARTYPE vt; ULONG start; int raises; ULONG want; } t[] = {
        { 0, VT_EMPTY, 0xFFFFFFF8u, 1, 0 },              /* align8 ok, +20 wraps */
        { 0, VT_EMPTY, 0xFFFFFFE8u, 0, 0xFFFFFFFCu },
        { 0, VT_EMPTY, 0xFFFFFFE0u, 0, 0xFFFFFFF4u },
        { 0, VT_I4,    0xFFFFFFE0u, 0, 0xFFFFFFF8u },
        { 0, VT_I4,    0xFFFFFFE8u, 1, 0 },              /* 0xFFFFFFFC + 4 wraps */
        { 0, VT_I8,    0xFFFFFFE0u, 1, 0 },              /* 0xFFFFFFF4 aligned to 0xFFFFFFF8, + 8 wraps */
        { 0, VT_I1,    0xFFFFFFE8u, 0, 0xFFFFFFFDu },
        { 0, VT_EMPTY, 0xFFFFFFF9u, 1, 0 },              /* the initial align8 itself wraps */
        { 1, VT_EMPTY, 0xFFFFFFE0u, 0, 0xFFFFFFF8u },
        { 1, VT_EMPTY, 0xFFFFFFE8u, 1, 0 },              /* +20 = 0xFFFFFFFC, align8 wraps */
        { 1, VT_I4,    0xFFFFFFE0u, 1, 0 },              /* 0xFFFFFFF8 + 4 ok?  align8(+20)=0xFFFFFFF8 then +4 = 0xFFFFFFFC */
    };
    unsigned i; NEED_FAM(UF_VARIANT); WINE_SKIP("no overflow checks");
    for (i = 0; i < ARRAY_N(t); i++) {
        VARIANT var; DWORD exc = 0; ULONG got;
        if (!u_have(UF_VARIANT, t[i].v)) continue;
        memset(&var, 0, sizeof var); V_VT(&var) = t[i].vt;
        if (t[i].v == 1 && t[i].vt == VT_I4 && t[i].start == 0xFFFFFFE0u) { got = ssize(UF_VARIANT, 1, t[i].start, &var, &exc); t_check(exc == 0 && got == 0xFFFFFFFCu, __FILE__, __LINE__, "VARIANT_UserSize64 near wrap", "exc 0x%lx got %08lx", exc, got); continue; }
        got = ssize(UF_VARIANT, t[i].v, t[i].start, &var, &exc);
        if (t[i].raises) t_check(exc == OVF, __FILE__, __LINE__, "VARIANT_UserSize overflow", "v64=%d vt %x start %08lx exc 0x%lx", t[i].v, t[i].vt, t[i].start, exc);
        else t_check(exc == 0 && got == t[i].want, __FILE__, __LINE__, "VARIANT_UserSize near wrap", "v64=%d vt %x start %08lx exc 0x%lx got %08lx want %08lx", t[i].v, t[i].vt, t[i].start, exc, got, t[i].want);
    }
}

TEST(ums_sa_size_offset_overflow)
{
    static const struct { int v; ULONG start; int raises; ULONG want; } t[] = {
        { 0, 0xFFFFFFF8u, 0, 0xFFFFFFFCu }, { 0, 0xFFFFFFFCu, 1, 0 }, { 0, 0xFFFFFFFDu, 1, 0 }, { 0, 0xFFFFFFFFu, 1, 0 },
        { 1, 0xFFFFFFF0u, 0, 0xFFFFFFF8u }, { 1, 0xFFFFFFF1u, 1, 0 }, { 1, 0xFFFFFFF8u, 1, 0 },
    };
    unsigned i; NEED_FAM(UF_SAFEARRAY); WINE_SKIP("no overflow checks");
    for (i = 0; i < ARRAY_N(t); i++) {
        SAFEARRAY *nul = NULL; DWORD exc = 0; ULONG got;
        if (!u_have(UF_SAFEARRAY, t[i].v)) continue;
        got = ssize(UF_SAFEARRAY, t[i].v, t[i].start, &nul, &exc);
        if (t[i].raises) t_check(exc == OVF, __FILE__, __LINE__, "LPSAFEARRAY_UserSize overflow", "v64=%d start %08lx exc 0x%lx", t[i].v, t[i].start, exc);
        else t_check(exc == 0 && got == t[i].want, __FILE__, __LINE__, "LPSAFEARRAY_UserSize near wrap", "v64=%d start %08lx exc 0x%lx got %08lx", t[i].v, t[i].start, exc, got);
    }
}

TEST(ums_sa_size_cell_count_overflow)
{
    /* descriptors without storage (pvData NULL): UserSize only inspects dims, bounds and cbElements */
    static const struct { const char *name; USHORT dims; ULONG cb; ULONG el[3]; USHORT feat; } t[] = {
        { "65536x65536 words", 2, 2, { 0x10000, 0x10000, 0 }, 0 },
        { "2^31 words", 1, 2, { 0x80000000u, 0, 0 }, 0 },
        { "0xFFFFFFFF words", 1, 2, { 0xFFFFFFFFu, 0, 0 }, 0 },
        { "2^30 qwords (bytes overflow)", 1, 8, { 0x40000000u, 0, 0 }, 0 },
        { "2^30 dwords x4 (bytes overflow)", 2, 4, { 0x8000, 0x8000, 0 }, 0 },
        { "3 dims 2^11 each", 3, 8, { 0x800, 0x800, 0x800 }, 0 },
    };
    unsigned i; int v; NEED_FAM(UF_SAFEARRAY); WINE_SKIP("no overflow checks");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (i = 0; i < ARRAY_N(t); i++) {
            SAFEARRAY *psa = NULL; DWORD exc = 0; USHORT d;
            if (SafeArrayAllocDescriptor(t[i].dims, &psa) != S_OK) { t_note("AllocDescriptor failed"); continue; }
            psa->cbElements = t[i].cb;
            for (d = 0; d < t[i].dims; d++) { psa->rgsabound[d].cElements = t[i].el[d]; psa->rgsabound[d].lLbound = 0; }
            ssize(UF_SAFEARRAY, v, 0, &psa, &exc);
            t_check(exc == OVF, __FILE__, __LINE__, "cell/byte count overflow raises", "v64=%d %s exc 0x%lx", v, t[i].name, exc);
            SafeArrayDestroyDescriptor(psa);
        }
}

TEST(ums_sa_size_zero_cells_with_huge_other_dimension)
{
    /* a 0 anywhere makes the array empty; no overflow even if the product of the others would wrap */
    int v; NEED_FAM(UF_SAFEARRAY); WINE_SKIP("different checks");
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAY *psa = NULL; DWORD exc = 0xabc; ULONG got, want;
        if (SafeArrayAllocDescriptor(2, &psa) != S_OK) continue;
        psa->cbElements = 4;
        psa->rgsabound[0].cElements = 0;           /* first checked dimension is the empty one */
        psa->rgsabound[0].lLbound = 0;
        psa->rgsabound[1].cElements = 0xFFFFFFFFu;
        psa->rgsabound[1].lLbound = 0;
        got = ssize(UF_SAFEARRAY, v, 0, &psa, &exc);
        want = ums_size_safearray(v, psa, 0);
        t_check(exc == 0, __FILE__, __LINE__, "empty array sizes", "v64=%d exc 0x%lx", v, exc);
        (void)got; (void)want;
        SafeArrayDestroyDescriptor(psa);
    }
}

/* ---- purity ------------------------------------------------------------------------------------------------ */
TEST(ums_marshal_does_not_modify_source)
{
    int v; NEED_FAM(UF_VARIANT);
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        SAFEARRAYBOUND sab = { 6, 1 };
        SAFEARRAY *psa = SafeArrayCreate(VT_BSTR, 1, &sab); BSTR *e = psa->pvData; BSTR bs = SysAllocString(L"pure");
        VARIANT arr, str, br, copy[3]; UCtx z, c; ULONG size; unsigned i; VARIANT *vs[3]; int cmp;
        BSTR sav[6];
        e[0] = SysAllocString(L"e0"); e[3] = SysAllocString(L"e3");
        memset(&arr, 0, sizeof arr); V_VT(&arr) = VT_BSTR | VT_ARRAY; V_ARRAY(&arr) = psa;
        memset(&str, 0, sizeof str); V_VT(&str) = VT_BSTR; V_BSTR(&str) = bs;
        memset(&br, 0, sizeof br); V_VT(&br) = VT_BSTR | VT_BYREF; V_BSTRREF(&br) = &bs;
        vs[0] = &arr; vs[1] = &str; vs[2] = &br;
        for (i = 0; i < 3; i++) copy[i] = *vs[i];
        for (i = 0; i < 6; i++) sav[i] = e[i];
        for (i = 0; i < 3; i++) {
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_VARIANT, v, 0, vs[i], NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            u_marshal(&c, UF_VARIANT, v, c.buf, vs[i], NULL);
            uctx_free(&c);
            cmp = memcmp(&copy[i], vs[i], sizeof(VARIANT));
            t_check(cmp == 0, __FILE__, __LINE__, "VARIANT unchanged by Size+Marshal", "v64=%d #%u", v, i);
        }
        for (i = 0; i < 6; i++) CHECK(e[i] == sav[i]);
        EQ(psa->cLocks, 0);
        EQ(psa->cDims, 1);
        SysFreeString(bs); SafeArrayDestroy(psa);
    }
}

TEST(ums_unmarshal_twice_into_same_destination)
{
    int v; NEED_FAM(UF_VARIANT);
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT) {
        VARIANT src, dst; BSTR b = SysAllocString(L"twice"); UCtx z, c; ULONG size; unsigned char *m; int round;
        memset(&src, 0, sizeof src); V_VT(&src) = VT_BSTR; V_BSTR(&src) = b;
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_VARIANT, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_VARIANT, v, c.buf, &src, NULL);
        memset(&dst, 0, sizeof dst);
        for (round = 0; round < 3; round++) {
            uctx_reread(&c, size, MSHCTX_LOCAL);
            EQ(u_unmarshal(&c, UF_VARIANT, v, c.buf, &dst, NULL), m);
            CHECK(V_VT(&dst) == VT_BSTR && V_BSTR(&dst) && !lstrcmpW(V_BSTR(&dst), L"twice"));
        }
        SysFreeString(V_BSTR(&dst));
        uctx_free(&c); SysFreeString(b);
    }
}

TEST(ums_unmarshal_sa_twice_into_same_destination)
{
    int v; NEED_FAM(UF_SAFEARRAY);
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY) {
        SAFEARRAYBOUND sab = { 5, 1 };
        SAFEARRAY *src = SafeArrayCreate(VT_BSTR, 1, &sab), *dst = NULL, *first; UCtx z, c; ULONG size; unsigned char *m; int round;
        ((BSTR *)src->pvData)[2] = SysAllocString(L"again");
        uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
        size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
        uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
        m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
        first = NULL;
        for (round = 0; round < 3; round++) {
            uctx_reread(&c, size, MSHCTX_LOCAL);
            EQ(u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL), m);
            CHECK(dst != NULL);
            if (round == 0) first = dst;
            else CHECK(dst == first);                           /* identical shape: the descriptor is reused */
            if (dst) CHECK(((BSTR *)dst->pvData)[2] && !lstrcmpW(((BSTR *)dst->pvData)[2], L"again"));
        }
        uctx_free(&c);
        { UCtx f; uctx_init(&f, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&f, UF_SAFEARRAY, v, &dst, NULL); uctx_free(&f); }
        SafeArrayDestroy(src);
    }
}

/* ---- sweeps ------------------------------------------------------------------------------------------------ */
TEST(ums_bstr_roundtrip_every_length)
{
    int v; ULONG n; NEED_FAM(UF_BSTR);
    for (v = 0; v < 2; v++) HAVE_V(UF_BSTR)
        for (n = 0; n <= 300; n++) {
            WCHAR w[320]; ULONG i, bytes = n; BSTR b, r = NULL; UCtx c; ULONG size; unsigned char *m;
            for (i = 0; i < 320; i++) w[i] = (WCHAR)(0x41 + (i * 13) % 50);
            b = SysAllocStringByteLen((const char *)w, bytes);          /* every byte length, odd ones included */
            size = ssize(UF_BSTR, v, 0, &b, NULL);
            if (size != ums_size_bstr(v, b, 0)) { CHECK(0); }
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            m = u_marshal(&c, UF_BSTR, v, c.buf, &b, NULL);
            if (m != c.buf + size) t_check(0, __FILE__, __LINE__, "marshal end", "v64=%d bytes %lu", v, n);
            uctx_reread(&c, size, MSHCTX_LOCAL);
            u_unmarshal(&c, UF_BSTR, v, c.buf, &r, NULL);
            if (!r || (!(t_is_wine() && (n & 1)) && SysStringByteLen(r) != n) || memcmp(r, w, n & ~1u))
                t_check(0, __FILE__, __LINE__, "roundtrip", "v64=%d bytes %lu", v, n);
            SysFreeString(r); SysFreeString(b); uctx_free(&c);
        }
}

TEST(ums_sa_roundtrip_every_small_length)
{
    static const VARTYPE vts[] = { VT_I2, VT_I4, VT_R8, VT_BSTR, VT_VARIANT };
    int v; unsigned k; LONG n; NEED_FAM(UF_SAFEARRAY);
    for (v = 0; v < 2; v++) HAVE_V(UF_SAFEARRAY)
        for (k = 0; k < ARRAY_N(vts); k++) {
            if (t_is_wine() && (vts[k] == VT_BSTR || vts[k] == VT_VARIANT)) continue;
            for (n = 0; n <= 40; n++) {
                SAFEARRAYBOUND sab; SAFEARRAY *src, *dst = NULL; UCtx z, c; ULONG size, i; unsigned char *m, *u; int ok = 1;
                sab.cElements = n; sab.lLbound = (n % 5) - 2;
                src = SafeArrayCreate(vts[k], 1, &sab);
                if (!src) { t_check(0, __FILE__, __LINE__, "SafeArrayCreate", "vt %u n %ld", vts[k], n); continue; }
                for (i = 0; i < (ULONG)n; i++) {
                    if (vts[k] == VT_BSTR) { WCHAR ch = (WCHAR)('a' + i % 26); ((BSTR *)src->pvData)[i] = (i % 3) ? SysAllocStringLen(&ch, 1) : NULL; }
                    else if (vts[k] == VT_VARIANT) { VARIANT *e = &((VARIANT *)src->pvData)[i]; V_VT(e) = VT_I4; V_I4(e) = i * 3; }
                    else memset((unsigned char *)src->pvData + i * src->cbElements, (int)(i + 1), src->cbElements);
                }
                uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
                size = u_size(&z, UF_SAFEARRAY, v, 0, &src, NULL); uctx_free(&z);
                if (!t_is_wine() && size != ums_size_safearray(v, src, 0)) ok = 0;
                uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
                m = u_marshal(&c, UF_SAFEARRAY, v, c.buf, &src, NULL);
                if (m > c.buf + size) ok = 0;
                uctx_reread(&c, size, MSHCTX_LOCAL);
                u = u_unmarshal(&c, UF_SAFEARRAY, v, c.buf, &dst, NULL);
                if (u != m || !dst || dst->cDims != 1 || dst->rgsabound[0].cElements != (ULONG)n || dst->rgsabound[0].lLbound != sab.lLbound) ok = 0;
                if (ok && dst && n) {
                    if (vts[k] == VT_BSTR) { for (i = 0; i < (ULONG)n; i++) { BSTR a = ((BSTR *)src->pvData)[i], b = ((BSTR *)dst->pvData)[i]; if ((a == NULL) != (b == NULL) || (a && lstrcmpW(a, b))) ok = 0; } }
                    else if (vts[k] == VT_VARIANT) { for (i = 0; i < (ULONG)n; i++) { VARIANT *a = &((VARIANT *)src->pvData)[i], *b = &((VARIANT *)dst->pvData)[i]; if (V_VT(a) != V_VT(b) || V_I4(a) != V_I4(b)) ok = 0; } }
                    else if (memcmp(src->pvData, dst->pvData, n * src->cbElements)) ok = 0;
                }
                t_check(ok, __FILE__, __LINE__, "small-length array roundtrip", "v64=%d vt %u n %ld", v, vts[k], n);
                if (dst) { UCtx f; uctx_init(&f, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_FREE); u_free(&f, UF_SAFEARRAY, v, &dst, NULL); uctx_free(&f); }
                uctx_free(&c);
                SafeArrayDestroy(src);
            }
        }
}

TEST(ums_variant_unmarshal_truncated_byref_and_bstr_variants)
{
    int v = 0; unsigned i; NEED_FAM(UF_VARIANT); WINE_SKIP("no shield bounds checks");
    {
        short s = 5; double d = 2.0; BSTR b = SysAllocString(L"truncate"); VARIANT vs[4]; UCtx z, c;
        memset(vs, 0, sizeof vs);
        V_VT(&vs[0]) = VT_I2 | VT_BYREF; V_I2REF(&vs[0]) = &s;
        V_VT(&vs[1]) = VT_R8 | VT_BYREF; V_R8REF(&vs[1]) = &d;
        V_VT(&vs[2]) = VT_BSTR; V_BSTR(&vs[2]) = b;
        V_VT(&vs[3]) = VT_BSTR | VT_BYREF; V_BSTRREF(&vs[3]) = &b;
        for (i = 0; i < 4; i++) {
            ULONG size, n; unsigned char *wire, *m;
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_VARIANT, v, 0, &vs[i], NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            m = u_marshal(&c, UF_VARIANT, v, c.buf, &vs[i], NULL);
            wire = malloc(size); memcpy(wire, c.buf, size); uctx_free(&c);
            for (n = 0; n < size; n++) {
                UCtx d; VARIANT dst; DWORD exc = 0;
                memset(&dst, 0, sizeof dst);
                uctx_wire(&d, wire, n, MSHCTX_LOCAL);
                u_unmarshal(&d, UF_VARIANT, v, d.buf, &dst, &exc);
                t_check(exc == RPC_X_BAD_STUB_DATA_, __FILE__, __LINE__, "truncated -> 0x6f7", "variant #%u len %lu/%lu exc 0x%lx", i, n, size, exc);
                uctx_free(&d);
            }
            free(wire);
        }
        SysFreeString(b);
    }
}

TEST(ums_variant_32bit_start_not_8aligned_unmarshal_skips_pad)
{
    /* the unmarshaler consumes the padding implied by its own pointer: end = aligned start + image size */
    int v; ULONG off; NEED_FAM(UF_VARIANT);
    for (v = 0; v < 2; v++) HAVE_V(UF_VARIANT)
        for (off = 0; off < 8; off++) {
            VARIANT src, dst; UCtx z, c; ULONG size; unsigned char *m, *u;
            memset(&src, 0, sizeof src); V_VT(&src) = VT_I8; V_I8(&src) = 0x0102030405060708LL;
            uctx_init(&z, 0, MSHCTX_LOCAL, USER_MARSHAL_CB_BUFFER_SIZE);
            size = u_size(&z, UF_VARIANT, v, off, &src, NULL); uctx_free(&z);
            uctx_init(&c, size, MSHCTX_LOCAL, USER_MARSHAL_CB_MARSHALL);
            m = u_marshal(&c, UF_VARIANT, v, c.buf + off, &src, NULL);
            EQ(m - c.buf, size);
            uctx_reread(&c, size, MSHCTX_LOCAL);
            memset(&dst, 0, sizeof dst);
            u = u_unmarshal(&c, UF_VARIANT, v, c.buf + off, &dst, NULL);
            EQ(u - c.buf, size);
            CHECK(V_VT(&dst) == VT_I8 && V_I8(&dst) == 0x0102030405060708LL);
            uctx_free(&c);
        }
}
