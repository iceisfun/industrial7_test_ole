/* variant_core: VariantChangeType / VariantChangeTypeEx -- structure, matrix, flags, values */
#include "vc.h"
#include "vc_matrix.h"

static HRESULT conv(VARIANT *dst, VARIANT *src, VARTYPE vt, USHORT fl) {
    return w_VariantChangeTypeEx(dst, src, VC_LCID_US, (USHORT)(fl | VARIANT_NOUSEROVERRIDE), vt);
}
static void set_num(VARIANT *v, VARTYPE vt, double d) {
    w_VariantInit(v); V_VT(v) = vt;
    switch (vt) {
    case VT_I1: V_I1(v) = (signed char)d; break;
    case VT_UI1: V_UI1(v) = (BYTE)d; break;
    case VT_I2: V_I2(v) = (SHORT)d; break;
    case VT_UI2: V_UI2(v) = (USHORT)d; break;
    case VT_I4: V_I4(v) = (LONG)d; break;
    case VT_UI4: V_UI4(v) = (ULONG)d; break;
    case VT_INT: V_INT(v) = (int)d; break;
    case VT_UINT: V_UINT(v) = (UINT)d; break;
    case VT_I8: V_I8(v) = (LONGLONG)d; break;
    case VT_UI8: V_UI8(v) = (ULONGLONG)d; break;
    case VT_R4: V_R4(v) = (float)d; break;
    case VT_R8: V_R8(v) = d; break;
    case VT_DATE: V_DATE(v) = d; break;
    case VT_BOOL: V_BOOL(v) = d ? VARIANT_TRUE : VARIANT_FALSE; break;
    case VT_CY: V_CY(v).int64 = (LONGLONG)(d * 10000.0 + (d < 0 ? -0.5 : 0.5)); break;
    default: break;
    }
}
static double get_num(const VARIANT *v) {
    switch (V_VT(v)) {
    case VT_I1: return V_I1(v);
    case VT_UI1: return V_UI1(v);
    case VT_I2: return V_I2(v);
    case VT_UI2: return V_UI2(v);
    case VT_I4: return V_I4(v);
    case VT_UI4: return V_UI4(v);
    case VT_INT: return V_INT(v);
    case VT_UINT: return V_UINT(v);
    case VT_I8: return (double)V_I8(v);
    case VT_UI8: return (double)V_UI8(v);
    case VT_R4: return V_R4(v);
    case VT_R8: return V_R8(v);
    case VT_DATE: return V_DATE(v);
    case VT_BOOL: return V_BOOL(v);
    case VT_CY: return (double)V_CY(v).int64 / 10000.0;
    default: return -1e300;
    }
}
static void set_bstr(VARIANT *v, const WCHAR *s) { w_VariantInit(v); V_VT(v) = VT_BSTR; V_BSTR(v) = w_SysAllocString(s); }
static void mark_dest(VARIANT *d) { w_VariantInit(d); V_VT(d) = VT_I4; V_I4(d) = 0x5151; }
static int dest_untouched(const VARIANT *d) { return V_VT(d) == VT_I4 && V_I4(d) == 0x5151; }

/* ---------------------------------------------------------------- argument validation */
TEST(vc_change_null_arguments) {
    VC_REQ();
    if (t_is_wine()) { t_skip("wine dereferences NULL arguments"); return; }
    VARIANT v; set_num(&v, VT_I4, 1);
    HR_EQ(w_VariantChangeTypeEx(NULL, &v, VC_LCID_US, 0, VT_I4), VC_INVALID);
    HR_EQ(w_VariantChangeTypeEx(&v, NULL, VC_LCID_US, 0, VT_I4), VC_INVALID);
    HR_EQ(w_VariantChangeTypeEx(NULL, NULL, VC_LCID_US, 0, VT_I4), VC_INVALID);
    HR_EQ(w_VariantChangeType(NULL, &v, 0, VT_I4), VC_INVALID);
    HR_EQ(w_VariantChangeType(&v, NULL, 0, VT_I4), VC_INVALID);
    EXPECT_EQ(V_VT(&v), VT_I4); EXPECT_EQ(V_I4(&v), 1);
}
TEST(vc_change_invalid_target_vt) {
    VC_REQ();
    static const unsigned bad[] = { 15, 24, 25, 26, 27, 28, 29, 30, 31, 37, 64, 65, 72, 0x0fff, 0x1000 | VT_I4, 0x8000 | VT_I4, 0x1000, 0x8000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        VARIANT s, d; set_num(&s, VT_I4, 1); mark_dest(&d);
        HRESULT hr = w_VariantChangeTypeEx(&d, &s, VC_LCID_US, 0, (VARTYPE)bad[i]);
        CHECKF(hr == VC_BADVT, "target vt 0x%x hr 0x%08lx", bad[i], (unsigned long)hr);
        CHECKF(dest_untouched(&d), "target vt 0x%x touched dest", bad[i]);
    }
}
TEST(vc_change_invalid_target_vt_independent_of_source) {
    VC_REQ();
    static const VARTYPE srcs[] = { VT_EMPTY, VT_NULL, VT_I2, VT_BSTR, VT_ERROR };
    for (unsigned i = 0; i < sizeof srcs / sizeof *srcs; i++) {
        VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = srcs[i]; mark_dest(&d);
        HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, 0, 15), VC_BADVT);
        HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, 0, VT_VOID), VC_BADVT);
    }
}
TEST(vc_change_wrapper_matches_ex_with_neutral_lcid) {
    VC_REQ(); REQUIRE(U(VariantChangeType));
    VARIANT s, d1, d2; set_num(&s, VT_I4, 77); w_VariantInit(&d1); w_VariantInit(&d2);
    HR_EQ(w_VariantChangeType(&d1, &s, 0, VT_BSTR), S_OK);
    HR_EQ(w_VariantChangeTypeEx(&d2, &s, 0x400, 0, VT_BSTR), S_OK);
    EXPECT_WSTR(V_BSTR(&d1), L"77"); EXPECT_WSTR(V_BSTR(&d2), L"77");
    w_VariantClear(&d1); w_VariantClear(&d2);
}

/* ---------------------------------------------------------------- the conversion matrix */
static void fill_src(VARIANT *s, unsigned svt, MU *u, MD *d) {
    w_VariantInit(s);
    switch (svt) {
    case VT_EMPTY: break;
    case VT_NULL: V_VT(s) = VT_NULL; break;
    case VT_BSTR: V_VT(s) = VT_BSTR; V_BSTR(s) = w_SysAllocString(L"1"); break;
    case VT_ERROR: V_VT(s) = VT_ERROR; V_ERROR(s) = 0x80020004; break;
    case VT_DECIMAL: V_VT(s) = VT_DECIMAL; V_DECIMAL(s).Lo64 = 1; break;
    case VT_CY: V_VT(s) = VT_CY; V_CY(s).int64 = 10000; break;
    case VT_BOOL: V_VT(s) = VT_BOOL; V_BOOL(s) = VARIANT_TRUE; break;
    case VT_DISPATCH: V_VT(s) = VT_DISPATCH; V_DISPATCH(s) = (IDispatch *)d; break;
    case VT_UNKNOWN: V_VT(s) = VT_UNKNOWN; V_UNKNOWN(s) = (IUnknown *)u; break;
    default: set_num(s, (VARTYPE)svt, 1); break;
    }
}
/* src row x dst column over vt 0..23 (without the object types, tested separately).
 * A clear bit in the table means DISP_E_TYPEMISMATCH with the destination untouched; a set bit means success. */
TEST(vc_change_matrix_numeric_and_string_sources) {
    VC_REQ();
    static const unsigned srcs[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 14, 16, 17, 18, 19, 20, 21, 22, 23 };
    int bad = 0;
    for (unsigned si = 0; si < sizeof srcs / sizeof *srcs; si++) {
        unsigned svt = srcs[si];
        for (unsigned dvt = 0; dvt < 24; dvt++) {
            if (dvt == 15) continue;   /* invalid vt: DISP_E_BADVARTYPE, covered elsewhere */
            if (svt == VT_BSTR && dvt == VT_DATE) continue; /* "1" as a date is a parser question, not a table question */
            VARIANT s, d; fill_src(&s, svt, NULL, NULL); mark_dest(&d);
            HRESULT hr = conv(&d, &s, (VARTYPE)dvt, 0);
            int ok = (vc_ct_ok[svt] >> dvt) & 1;
            if (ok) {
                if (hr != S_OK || V_VT(&d) != dvt) { bad++; CHECKF(0, "convert %u -> %u: hr 0x%08lx vt %u (want S_OK)", svt, dvt, (unsigned long)hr, V_VT(&d)); }
                w_VariantClear(&d);
            } else {
                if (hr != VC_MISMATCH || !dest_untouched(&d)) { bad++; CHECKF(0, "convert %u -> %u: hr 0x%08lx (want TYPEMISMATCH, dest untouched)", svt, dvt, (unsigned long)hr); }
            }
            w_VariantClear(&s);
        }
    }
    CHECK(bad == 0);
}
TEST(vc_change_matrix_to_unusable_targets_is_mismatch) {
    VC_REQ();
    /* targets VT_VARIANT(12), VT_RECORD(36): valid vts for which no conversion exists */
    static const unsigned srcs[] = { 0, 2, 3, 5, 8, 11 };
    for (unsigned si = 0; si < sizeof srcs / sizeof *srcs; si++) {
        VARIANT s, d; fill_src(&s, srcs[si], NULL, NULL); mark_dest(&d);
        HRESULT hr = conv(&d, &s, VT_VARIANT, 0);
        CHECKF(hr == VC_MISMATCH && dest_untouched(&d), "src %u -> VT_VARIANT hr 0x%08lx", srcs[si], (unsigned long)hr);
        hr = conv(&d, &s, VT_RECORD, 0);
        CHECKF(hr == VC_MISMATCH && dest_untouched(&d), "src %u -> VT_RECORD hr 0x%08lx", srcs[si], (unsigned long)hr);
        w_VariantClear(&s);
    }
}
TEST(vc_change_null_source_only_converts_to_null) {
    VC_REQ();
    VARIANT s, d; fill_src(&s, VT_NULL, NULL, NULL); mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_NULL, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_NULL);
    mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_EMPTY, 0), VC_MISMATCH); CHECK(dest_untouched(&d));
    HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_BOOL, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_R8, 0), VC_MISMATCH);
}
TEST(vc_change_error_source_only_converts_to_error) {
    VC_REQ();
    VARIANT s, d; fill_src(&s, VT_ERROR, NULL, NULL); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_ERROR, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_ERROR); CHECK(V_ERROR(&d) == 0x80020004);
    mark_dest(&d);
    static const VARTYPE dsts[] = { VT_I2, VT_I4, VT_BSTR, VT_BOOL, VT_R8, VT_EMPTY, VT_NULL, VT_UI1 };
    for (unsigned i = 0; i < sizeof dsts / sizeof *dsts; i++) { HR_EQ(conv(&d, &s, dsts[i], 0), VC_MISMATCH); CHECK(dest_untouched(&d)); }
}
TEST(vc_change_empty_source_defaults) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s);
    static const VARTYPE nums[] = { VT_I2, VT_I4, VT_R4, VT_R8, VT_DATE, VT_UI1, VT_I1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT, VT_BOOL, VT_CY };
    for (unsigned i = 0; i < sizeof nums / sizeof *nums; i++) {
        w_VariantInit(&d);
        HR_EQ(conv(&d, &s, nums[i], 0), S_OK);
        EXPECT_EQ(V_VT(&d), nums[i]);
        CHECKF(get_num(&d) == 0.0, "empty -> vt %u not zero", nums[i]);
    }
    w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR); CHECK(V_BSTR(&d) != NULL);
    if (V_BSTR(&d)) EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 0);
    w_VariantClear(&d);
    w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DECIMAL); CHECK(V_DECIMAL(&d).Lo64 == 0 && V_DECIMAL(&d).Hi32 == 0 && V_DECIMAL(&d).scale == 0 && V_DECIMAL(&d).sign == 0);
    HR_EQ(conv(&d, &s, VT_NULL, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_NULL);
    HR_EQ(conv(&d, &s, VT_EMPTY, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_EMPTY);
    mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_ERROR, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_UNKNOWN, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_DISPATCH, 0), VC_MISMATCH);
}
TEST(vc_change_int_source_cannot_become_int64) {
    /* quirk visible in the Win10 dispatch table: VT_INT -> VT_I8/VT_UI8 has no handler; VT_UINT -> VT_I8/UI8 has */
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_INT, 5); mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_I8, 0), VC_MISMATCH); CHECK(dest_untouched(&d));
    HR_EQ(conv(&d, &s, VT_UI8, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 5);
    set_num(&s, VT_UINT, 5);
    HR_EQ(conv(&d, &s, VT_I8, 0), S_OK); CHECK(V_I8(&d) == 5);
    HR_EQ(conv(&d, &s, VT_UI8, 0), S_OK); CHECK(V_UI8(&d) == 5);
}

/* ---------------------------------------------------------------- same type, in place */
TEST(vc_change_same_type_copies) {
    VC_REQ();
    static const unsigned vts[] = { 2, 3, 4, 5, 6, 7, 11, 14, 16, 17, 18, 19, 20, 21, 22, 23 };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT s, d; fill_src(&s, vts[i], NULL, NULL); mark_dest(&d);
        HR_EQ(conv(&d, &s, (VARTYPE)vts[i], 0), S_OK);
        EXPECT_EQ(V_VT(&d), vts[i]);
        CHECKF(!memcmp(&V_I8(&d), &V_I8(&s), 8) || vts[i] == 14, "vt %u payload differs", vts[i]);
    }
}
TEST(vc_change_same_type_bstr_deep_copy) {
    VC_REQ();
    VARIANT s, d; set_bstr(&s, L"same"); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR); CHECK(V_BSTR(&d) != V_BSTR(&s)); EXPECT_WSTR(V_BSTR(&d), L"same");
    EXPECT_WSTR(V_BSTR(&s), L"same"); EXPECT_EQ(V_VT(&s), VT_BSTR);
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_change_same_type_bstr_preserves_embedded_nul) {
    VC_REQ();
    static const OLECHAR src[] = { 'a', 0, 'b' };
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_BSTR; V_BSTR(&s) = w_SysAllocStringLen(src, 3); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK);
    EXPECT_EQ(w_SysStringLen(V_BSTR(&d)), 3);
    w_VariantClear(&s); w_VariantClear(&d);
}
TEST(vc_change_in_place_numeric) {
    VC_REQ();
    VARIANT v; set_num(&v, VT_I4, 41);
    HR_EQ(conv(&v, &v, VT_R8, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_R8); CHECK(V_R8(&v) == 41.0);
    HR_EQ(conv(&v, &v, VT_I2, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_I2); EXPECT_EQ(V_I2(&v), 41);
    HR_EQ(conv(&v, &v, VT_BSTR, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_BSTR); EXPECT_WSTR(V_BSTR(&v), L"41");
    HR_EQ(conv(&v, &v, VT_UI1, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_UI1); EXPECT_EQ(V_UI1(&v), 41);
    HR_EQ(conv(&v, &v, VT_BOOL, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_BOOL); EXPECT_EQ(V_BOOL(&v), VARIANT_TRUE);
    HR_EQ(conv(&v, &v, VT_NULL, 0), S_OK); EXPECT_EQ(V_VT(&v), VT_NULL);
}
TEST(vc_change_in_place_failure_keeps_source) {
    VC_REQ();
    VARIANT v; set_bstr(&v, L"not a number");
    HR_EQ(conv(&v, &v, VT_I4, 0), VC_MISMATCH);
    EXPECT_EQ(V_VT(&v), VT_BSTR); EXPECT_WSTR(V_BSTR(&v), L"not a number");
    w_VariantClear(&v);
    set_num(&v, VT_I4, 100000);
    HR_EQ(conv(&v, &v, VT_I2, 0), VC_OVERFLOW);
    EXPECT_EQ(V_VT(&v), VT_I4); EXPECT_EQ(V_I4(&v), 100000);
}
TEST(vc_change_destination_released_on_success_only) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT s, d; set_num(&s, VT_I4, 3);
    w_VariantInit(&d); V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = (IUnknown *)&u;
    HR_EQ(conv(&d, &s, VT_I2, 0), S_OK);
    EXPECT_EQ(u.nrel, 1); EXPECT_EQ(V_VT(&d), VT_I2); EXPECT_EQ(V_I2(&d), 3);
    /* failing conversion must not release the destination */
    MU u2; mu_init(&u2);
    set_num(&s, VT_I4, 70000);
    V_VT(&d) = VT_UNKNOWN; V_UNKNOWN(&d) = (IUnknown *)&u2;
    HR_EQ(conv(&d, &s, VT_I2, 0), VC_OVERFLOW);
    EXPECT_EQ(u2.nrel, 0); EXPECT_EQ(V_VT(&d), VT_UNKNOWN);
}
TEST(vc_change_does_not_modify_source) {
    VC_REQ();
    VARIANT s, ref, d; set_num(&s, VT_R8, 2.5); ref = s; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK);
    CHECK(!memcmp(&s, &ref, sizeof s));
    set_bstr(&s, L"12"); ref = s;
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 12);
    CHECK(!memcmp(&s, &ref, sizeof s));
    w_VariantClear(&s);
}

/* ---------------------------------------------------------------- numeric values */
typedef struct { VARTYPE svt; double sv; VARTYPE dvt; HRESULT hr; double want; } numcase;
TEST(vc_change_numeric_rounding_and_range_table) {
    VC_REQ();
    static const numcase t[] = {
        /* R8 -> I4: round half to even */
        { VT_R8, 1.5, VT_I4, S_OK, 2 }, { VT_R8, 2.5, VT_I4, S_OK, 2 }, { VT_R8, 3.5, VT_I4, S_OK, 4 }, { VT_R8, 0.5, VT_I4, S_OK, 0 },
        { VT_R8, -0.5, VT_I4, S_OK, 0 }, { VT_R8, -1.5, VT_I4, S_OK, -2 }, { VT_R8, -2.5, VT_I4, S_OK, -2 }, { VT_R8, 0.6, VT_I4, S_OK, 1 },
        { VT_R8, -0.6, VT_I4, S_OK, -1 }, { VT_R8, 0.4, VT_I4, S_OK, 0 }, { VT_R8, 2147483647.0, VT_I4, S_OK, 2147483647.0 },
        { VT_R8, 2147483647.4, VT_I4, S_OK, 2147483647.0 }, { VT_R8, 2147483648.0, VT_I4, VC_OVERFLOW, 0 },
        { VT_R8, -2147483648.0, VT_I4, S_OK, -2147483648.0 }, { VT_R8, -2147483649.0, VT_I4, VC_OVERFLOW, 0 },
        /* I2 */
        { VT_R8, 32767.0, VT_I2, S_OK, 32767 }, { VT_R8, 32768.0, VT_I2, VC_OVERFLOW, 0 }, { VT_R8, -32768.0, VT_I2, S_OK, -32768 },
        { VT_R8, -32769.0, VT_I2, VC_OVERFLOW, 0 }, { VT_R8, 1.5, VT_I2, S_OK, 2 }, { VT_R8, 2.5, VT_I2, S_OK, 2 },
        /* I1 / UI1 */
        { VT_R8, 127.0, VT_I1, S_OK, 127 }, { VT_R8, 128.0, VT_I1, VC_OVERFLOW, 0 }, { VT_R8, -128.0, VT_I1, S_OK, -128 }, { VT_R8, -129.0, VT_I1, VC_OVERFLOW, 0 },
        { VT_R8, 255.0, VT_UI1, S_OK, 255 }, { VT_R8, 255.4, VT_UI1, S_OK, 255 }, { VT_R8, 256.0, VT_UI1, VC_OVERFLOW, 0 },
        { VT_R8, -1.0, VT_UI1, VC_OVERFLOW, 0 }, { VT_R8, -0.4, VT_UI1, S_OK, 0 }, { VT_R8, 1.5, VT_UI1, S_OK, 2 },
        /* UI2 / UI4 */
        { VT_R8, 65535.0, VT_UI2, S_OK, 65535 }, { VT_R8, 65536.0, VT_UI2, VC_OVERFLOW, 0 }, { VT_R8, -1.0, VT_UI2, VC_OVERFLOW, 0 },
        { VT_R8, 4294967295.0, VT_UI4, S_OK, 4294967295.0 }, { VT_R8, 4294967296.0, VT_UI4, VC_OVERFLOW, 0 }, { VT_R8, -1.0, VT_UI4, VC_OVERFLOW, 0 },
        /* R4 sources */
        { VT_R4, 1.5, VT_I4, S_OK, 2 }, { VT_R4, 2.5, VT_I4, S_OK, 2 }, { VT_R4, -2.5, VT_I4, S_OK, -2 }, { VT_R4, 65536.0, VT_I2, VC_OVERFLOW, 0 },
        /* wider -> narrower integer */
        { VT_I4, 32767, VT_I2, S_OK, 32767 }, { VT_I4, 32768, VT_I2, VC_OVERFLOW, 0 }, { VT_I4, -32769, VT_I2, VC_OVERFLOW, 0 },
        { VT_I4, 255, VT_UI1, S_OK, 255 }, { VT_I4, 256, VT_UI1, VC_OVERFLOW, 0 }, { VT_I4, -1, VT_UI1, VC_OVERFLOW, 0 },
        { VT_I4, 128, VT_I1, VC_OVERFLOW, 0 }, { VT_I4, -128, VT_I1, S_OK, -128 },
        { VT_I4, 65535, VT_UI2, S_OK, 65535 }, { VT_I4, 65536, VT_UI2, VC_OVERFLOW, 0 }, { VT_I4, -1, VT_UI2, VC_OVERFLOW, 0 },
        { VT_I4, -1, VT_UI8, VC_OVERFLOW, 0 }, { VT_I4, 5, VT_UI8, S_OK, 5 }, { VT_I4, -5, VT_I8, S_OK, -5 },
        { VT_I2, 255, VT_UI1, S_OK, 255 }, { VT_I2, 256, VT_UI1, VC_OVERFLOW, 0 }, { VT_I2, -1, VT_UI1, VC_OVERFLOW, 0 },
        { VT_I2, 128, VT_I1, VC_OVERFLOW, 0 },  { VT_UI2, 32767, VT_I2, S_OK, 32767 },
         { VT_UI4, 2147483647.0, VT_I4, S_OK, 2147483647.0 },
         { VT_UI1, 127, VT_I1, S_OK, 127 },
         { VT_I1, 5, VT_UI1, S_OK, 5 }, { VT_I1, -1, VT_UI4, VC_OVERFLOW, 0 },
        /* widening */
        { VT_I1, -5, VT_I2, S_OK, -5 }, { VT_I1, -5, VT_I4, S_OK, -5 }, { VT_I1, -5, VT_R8, S_OK, -5 }, { VT_I2, -5, VT_I4, S_OK, -5 },
        { VT_UI1, 200, VT_I2, S_OK, 200 }, { VT_UI1, 200, VT_UI4, S_OK, 200 }, { VT_UI2, 60000, VT_I4, S_OK, 60000 },
        { VT_UI4, 4000000000.0, VT_R8, S_OK, 4000000000.0 }, { VT_I4, -123456, VT_R8, S_OK, -123456 }, { VT_I4, 16777217, VT_R8, S_OK, 16777217 },
        /* floats */
        { VT_R8, 1.5, VT_R4, S_OK, 1.5 }, { VT_R8, 3.5e38, VT_R4, VC_OVERFLOW, 0 }, { VT_R8, -3.5e38, VT_R4, VC_OVERFLOW, 0 },
        { VT_R4, 0.25, VT_R8, S_OK, 0.25 }, { VT_R8, 36526.5, VT_DATE, S_OK, 36526.5 }, { VT_DATE, 36526.5, VT_R8, S_OK, 36526.5 },
        { VT_DATE, 1.6, VT_I4, S_OK, 2 }, { VT_DATE, 2.5, VT_I4, S_OK, 2 },
        /* currency */
        { VT_I4, 3, VT_CY, S_OK, 3 }, { VT_R8, 1.5, VT_CY, S_OK, 1.5 }, { VT_R8, 1e15, VT_CY, VC_OVERFLOW, 0 },
        { VT_CY, 1.5, VT_I4, S_OK, 2 }, { VT_CY, 2.5, VT_I4, S_OK, 2 }, { VT_CY, 3.5, VT_I4, S_OK, 4 }, { VT_CY, -1.5, VT_I4, S_OK, -2 },
        { VT_CY, 0.5, VT_I4, S_OK, 0 }, { VT_CY, 1.2345, VT_R8, S_OK, 1.2345 }, { VT_CY, 300.0, VT_UI1, VC_OVERFLOW, 0 },
        { VT_CY, 1.0, VT_BOOL, S_OK, -1 },
        /* booleans */
        { VT_I4, 2, VT_BOOL, S_OK, -1 }, { VT_I4, 0, VT_BOOL, S_OK, 0 }, { VT_I4, -7, VT_BOOL, S_OK, -1 }, { VT_R8, 0.5, VT_BOOL, S_OK, -1 },
        { VT_R8, 0.0, VT_BOOL, S_OK, 0 }, { VT_UI1, 255, VT_BOOL, S_OK, -1 }, { VT_I8, 4294967296.0, VT_BOOL, S_OK, -1 },
        { VT_BOOL, -1, VT_I2, S_OK, -1 }, { VT_BOOL, -1, VT_I4, S_OK, -1 }, { VT_BOOL, -1, VT_R8, S_OK, -1 }, { VT_BOOL, 0, VT_I4, S_OK, 0 },
        { VT_BOOL, -1, VT_I8, S_OK, -1 }, { VT_BOOL, -1, VT_I1, S_OK, -1 }, { VT_BOOL, -1, VT_UI2, S_OK, 65535 },
        { VT_BOOL, -1, VT_UI4, S_OK, 4294967295.0 }, { VT_BOOL, -1, VT_DATE, S_OK, -1 },
        /* 64 bit */
        { VT_I8, 4294967296.0, VT_I4, VC_OVERFLOW, 0 }, { VT_I8, 2147483647.0, VT_I4, S_OK, 2147483647.0 }, 
        { VT_I8, 1e15, VT_R8, S_OK, 1e15 }, { VT_UI8, 1e15, VT_R8, S_OK, 1e15 }, { VT_UI8, 4294967296.0, VT_UI4, VC_OVERFLOW, 0 },
        { VT_UI8, 4294967295.0, VT_UI4, S_OK, 4294967295.0 },  { VT_I8, 255, VT_UI1, S_OK, 255 },
        { VT_UI8, 255, VT_I1, VC_OVERFLOW, 0 }, { VT_I8, 1e15, VT_I8, S_OK, 1e15 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; set_num(&s, t[i].svt, t[i].sv); mark_dest(&d);
        HRESULT hr = conv(&d, &s, t[i].dvt, 0);
        if (hr != t[i].hr) { CHECKF(0, "case %u: vt%u(%g) -> vt%u hr 0x%08lx want 0x%08lx", i, t[i].svt, t[i].sv, t[i].dvt, (unsigned long)hr, (unsigned long)t[i].hr); continue; }
        if (hr == S_OK) {
            CHECKF(V_VT(&d) == t[i].dvt && vc_dbl_eq(get_num(&d), t[i].want), "case %u: vt%u(%g) -> vt%u got %g (vt %u) want %g", i, t[i].svt, t[i].sv, t[i].dvt, get_num(&d), V_VT(&d), t[i].want);
        } else {
            CHECKF(dest_untouched(&d), "case %u: dest modified on failure", i);
        }
    }
}
TEST(vc_change_same_width_signed_unsigned_observations) {
    /* In the Win10 dispatch table VT_I2->VT_UI2 and VT_I4->VT_UI4 share the plain-copy handler of the same type;
     * report what this system does, the semantics are not hard-checked. */
    VC_REQ();
    VARIANT s, d;
    set_num(&s, VT_I2, -1); w_VariantInit(&d);
    HRESULT hr = conv(&d, &s, VT_UI2, 0);
    t_note("I2(-1)->UI2 hr=0x%08lx value=%g", (unsigned long)hr, hr == S_OK ? get_num(&d) : 0.0);
    set_num(&s, VT_I4, -1);
    hr = conv(&d, &s, VT_UI4, 0);
    t_note("I4(-1)->UI4 hr=0x%08lx value=%g", (unsigned long)hr, hr == S_OK ? get_num(&d) : 0.0);
    set_num(&s, VT_UI2, 40000); hr = conv(&d, &s, VT_I2, 0); t_note("UI2(40000)->I2 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_UI1, 128); hr = conv(&d, &s, VT_I1, 0); t_note("UI1(128)->I1 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_I1, -1); hr = conv(&d, &s, VT_UI1, 0); t_note("I1(-1)->UI1 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_UI4, 2147483648.0); hr = conv(&d, &s, VT_I4, 0); t_note("UI4(2^31)->I4 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_I8, 65536); hr = conv(&d, &s, VT_UI2, 0); t_note("I8(65536)->UI2 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_I8, -1); hr = conv(&d, &s, VT_UI8, 0); t_note("I8(-1)->UI8 hr=0x%08lx", (unsigned long)hr);
    set_num(&s, VT_UI4, 4000000000.0);
    hr = conv(&d, &s, VT_INT, 0);
    t_note("UI4(4e9)->INT hr=0x%08lx", (unsigned long)hr);
    CHECK(1);
}
TEST(vc_change_int64_precision) {
    VC_REQ();
    VARIANT s, d;
    w_VariantInit(&s); V_VT(&s) = VT_I8; V_I8(&s) = 9007199254740993LL; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_I8, 0), S_OK); CHECK(V_I8(&d) == 9007199254740993LL);
    HR_EQ(conv(&d, &s, VT_UI8, 0), S_OK); CHECK(V_UI8(&d) == 9007199254740993ULL);
    V_I8(&s) = (LONGLONG)0x8000000000000000ULL;
    { HRESULT h = conv(&d, &s, VT_UI8, 0); t_note("I8 min -> UI8 hr=0x%08lx", (unsigned long)h); }
    HR_EQ(conv(&d, &s, VT_I8, 0), S_OK); CHECK(V_I8(&d) == (LONGLONG)0x8000000000000000ULL);
    V_VT(&s) = VT_UI8; V_UI8(&s) = 0xFFFFFFFFFFFFFFFFULL;
    { HRESULT h = conv(&d, &s, VT_I8, 0); t_note("UI8 max -> I8 hr=0x%08lx", (unsigned long)h); }
    HR_EQ(conv(&d, &s, VT_UI8, 0), S_OK); CHECK(V_UI8(&d) == 0xFFFFFFFFFFFFFFFFULL);
    HR_EQ(conv(&d, &s, VT_R8, 0), S_OK); CHECK(V_R8(&d) == 18446744073709551616.0);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"18446744073709551615");
    w_VariantClear(&d);
    V_VT(&s) = VT_I8; V_I8(&s) = (LONGLONG)0x8000000000000000ULL;
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"-9223372036854775808");
    w_VariantClear(&d);
}

/* ---------------------------------------------------------------- to BSTR */
typedef struct { VARTYPE svt; double sv; const WCHAR *want; } strcase;
TEST(vc_change_numbers_to_bstr_en_us) {
    VC_REQ();
    static const strcase t[] = {
        { VT_I4, 0, L"0" }, { VT_I4, -42, L"-42" }, { VT_I4, 2147483647.0, L"2147483647" }, { VT_I4, -2147483648.0, L"-2147483648" },
        { VT_I2, -32768, L"-32768" }, { VT_I1, -128, L"-128" }, { VT_UI1, 255, L"255" }, { VT_UI2, 65535, L"65535" }, { VT_UI4, 4294967295.0, L"4294967295" },
        { VT_INT, -5, L"-5" }, { VT_UINT, 5, L"5" },
        { VT_R8, 1.5, L"1.5" }, { VT_R8, 0.1, L"0.1" }, { VT_R8, -2.25, L"-2.25" }, { VT_R8, 123456789.0, L"123456789" }, { VT_R8, 100.0, L"100" },
        { VT_R8, 0.0, L"0" }, 
        { VT_R4, 1.5, L"1.5" }, { VT_R4, 0.25, L"0.25" }, { VT_R4, 100.0, L"100" },
        { VT_CY, 1.5, L"1.5" }, { VT_CY, 1234.5678, L"1234.5678" }, { VT_CY, -3.0, L"-3" }, { VT_CY, 0.0, L"0" },
        { VT_BOOL, -1, L"-1" }, { VT_BOOL, 0, L"0" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; set_num(&s, t[i].svt, t[i].sv); w_VariantInit(&d);
        HRESULT hr = conv(&d, &s, VT_BSTR, 0);
        if (hr != S_OK) { CHECKF(0, "case %u hr 0x%08lx", i, (unsigned long)hr); continue; }
        CHECKF(V_VT(&d) == VT_BSTR && V_BSTR(&d) && !lstrcmpW(V_BSTR(&d), t[i].want), "case %u (vt%u %g): got \"%ls\" want \"%ls\"", i, t[i].svt, t[i].sv, V_BSTR(&d) ? V_BSTR(&d) : L"(null)", t[i].want);
        w_VariantClear(&d);
    }
}
TEST(vc_change_r8_to_bstr_precision_notes) {
    VC_REQ();
    static const double vals[] = { 1.0 / 3.0, 1234567890123456.0, 123456789012345678.0, 1e-10, 1e-5, 1e15, 1e20, 5e-324, 0.30000000000000004 };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        VARIANT s, d; set_num(&s, VT_R8, vals[i]); w_VariantInit(&d);
        HRESULT hr = conv(&d, &s, VT_BSTR, 0);
        t_note("R8 %.17g -> hr=0x%08lx \"%ls\"", vals[i], (unsigned long)hr, hr == S_OK ? V_BSTR(&d) : L"");
        if (hr == S_OK) {
            double back = wcstod(V_BSTR(&d), NULL);
            CHECKF(fabs(back - vals[i]) <= fabs(vals[i]) * 1e-14, "round trip %g -> %g", vals[i], back);
            w_VariantClear(&d);
        }
    }
}
TEST(vc_change_r8_to_bstr_german_decimal_comma) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_R8, 1.5); w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE, VT_BSTR), S_OK);
    EXPECT_WSTR(V_BSTR(&d), L"1,5");
    w_VariantClear(&d);
    set_num(&s, VT_CY, 1.5);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE, VT_BSTR), S_OK);
    EXPECT_WSTR(V_BSTR(&d), L"1,5");
    w_VariantClear(&d);
}
TEST(vc_change_date_to_bstr_en_us) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_DATE, 36526.0); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"1/1/2000"); w_VariantClear(&d);
    set_num(&s, VT_DATE, 36526.5);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"1/1/2000 12:00:00 PM"); w_VariantClear(&d);
    set_num(&s, VT_DATE, 0.75);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"6:00:00 PM"); w_VariantClear(&d);
}
TEST(vc_change_decimal_to_bstr) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DECIMAL; V_DECIMAL(&s).scale = 2; V_DECIMAL(&s).sign = 0; V_DECIMAL(&s).Hi32 = 0; V_DECIMAL(&s).Lo64 = 150;
    w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"1.5"); w_VariantClear(&d);
    V_DECIMAL(&s).sign = DECIMAL_NEG; V_DECIMAL(&s).scale = 0; V_DECIMAL(&s).Lo64 = 42;
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"-42"); w_VariantClear(&d);
}

/* ---------------------------------------------------------------- to DECIMAL / CY */
TEST(vc_change_to_decimal_layout) {
    VC_REQ();
    VARIANT s, d;
    set_num(&s, VT_I4, -5); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DECIMAL);
    EXPECT_EQ(V_DECIMAL(&d).sign, DECIMAL_NEG); EXPECT_EQ(V_DECIMAL(&d).scale, 0); EXPECT_EQ(V_DECIMAL(&d).Hi32, 0); CHECK(V_DECIMAL(&d).Lo64 == 5);
    set_num(&s, VT_UI8, 0); V_UI8(&s) = 0xFFFFFFFFFFFFFFFFULL;
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK); CHECK(V_DECIMAL(&d).Lo64 == 0xFFFFFFFFFFFFFFFFULL); EXPECT_EQ(V_DECIMAL(&d).Hi32, 0); EXPECT_EQ(V_DECIMAL(&d).sign, 0);
    set_num(&s, VT_R8, 1.5);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK); EXPECT_EQ(V_DECIMAL(&d).scale, 1); CHECK(V_DECIMAL(&d).Lo64 == 15);
    set_num(&s, VT_R8, 0.25);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK); EXPECT_EQ(V_DECIMAL(&d).scale, 2); CHECK(V_DECIMAL(&d).Lo64 == 25);
    set_num(&s, VT_CY, 12.3456);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK); EXPECT_EQ(V_DECIMAL(&d).scale, 4); CHECK(V_DECIMAL(&d).Lo64 == 123456);
    set_num(&s, VT_BOOL, 1);
    HR_EQ(conv(&d, &s, VT_DECIMAL, 0), S_OK); CHECK(V_DECIMAL(&d).Lo64 == 1); t_note("BOOL TRUE -> DECIMAL sign=0x%x (VarDecFromBool gives 0x80)", V_DECIMAL(&d).sign);
}
TEST(vc_change_from_decimal) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DECIMAL;
    static const struct { BYTE scale, sign; ULONGLONG lo; VARTYPE dvt; HRESULT hr; double want; } t[] = {
        { 1, 0, 15, VT_I4, S_OK, 2 }, { 1, 0, 25, VT_I4, S_OK, 2 }, { 1, 0, 35, VT_I4, S_OK, 4 }, { 1, DECIMAL_NEG, 15, VT_I4, S_OK, -2 },
        { 0, 0, 255, VT_UI1, S_OK, 255 }, { 0, 0, 256, VT_UI1, VC_OVERFLOW, 0 }, { 0, DECIMAL_NEG, 1, VT_UI1, VC_OVERFLOW, 0 },
        { 0, 0, 32768, VT_I2, VC_OVERFLOW, 0 }, { 0, DECIMAL_NEG, 32768, VT_I2, S_OK, -32768 },
        { 2, 0, 150, VT_R8, S_OK, 1.5 }, { 0, 0, 7, VT_BOOL, S_OK, -1 }, { 0, 0, 0, VT_BOOL, S_OK, 0 },
        { 4, 0, 123456, VT_CY, S_OK, 12.3456 }, { 0, 0, 1000000000000000ULL, VT_CY, VC_OVERFLOW, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        V_DECIMAL(&s).scale = t[i].scale; V_DECIMAL(&s).sign = t[i].sign; V_DECIMAL(&s).Hi32 = 0; V_DECIMAL(&s).Lo64 = t[i].lo;
        mark_dest(&d);
        HRESULT hr = conv(&d, &s, t[i].dvt, 0);
        if (hr != t[i].hr) { CHECKF(0, "case %u hr 0x%08lx want 0x%08lx", i, (unsigned long)hr, (unsigned long)t[i].hr); continue; }
        if (hr == S_OK) CHECKF(V_VT(&d) == t[i].dvt && vc_dbl_eq(get_num(&d), t[i].want), "case %u got %g want %g", i, get_num(&d), t[i].want);
    }
}
TEST(vc_change_invalid_decimal_source) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DECIMAL; V_DECIMAL(&s).scale = 29; V_DECIMAL(&s).Lo64 = 1; mark_dest(&d);
    HRESULT hr = conv(&d, &s, VT_I4, 0);
    t_note("DECIMAL scale 29 -> I4 hr=0x%08lx", (unsigned long)hr);
    CHECK(hr < 0 && dest_untouched(&d));
    V_DECIMAL(&s).scale = 0; V_DECIMAL(&s).sign = 1;
    hr = conv(&d, &s, VT_I4, 0);
    CHECK(hr < 0 && dest_untouched(&d));
}

/* ---------------------------------------------------------------- strings to values */
typedef struct { const WCHAR *s; VARTYPE dvt; HRESULT hr; double want; } s2n;
TEST(vc_change_bstr_to_number_table) {
    VC_REQ();
    static const s2n t[] = {
        { L"123", VT_I4, S_OK, 123 }, { L"-5", VT_I4, S_OK, -5 }, { L"+5", VT_I4, S_OK, 5 }, { L"0", VT_I4, S_OK, 0 },
        { L"1.5", VT_I4, S_OK, 2 }, { L"2.5", VT_I4, S_OK, 2 }, { L"-2.5", VT_I4, S_OK, -2 }, { L"0.5", VT_I4, S_OK, 0 },
        { L"1e3", VT_I4, S_OK, 1000 }, { L"1E3", VT_I4, S_OK, 1000 }, { L"12.5e1", VT_I4, S_OK, 125 },
        { L"2147483647", VT_I4, S_OK, 2147483647.0 }, { L"2147483648", VT_I4, VC_OVERFLOW, 0 }, { L"-2147483648", VT_I4, S_OK, -2147483648.0 },
        { L"99999999999999999999", VT_I4, VC_OVERFLOW, 0 },
        { L"abc", VT_I4, VC_MISMATCH, 0 }, { L"", VT_I4, VC_MISMATCH, 0 }, { L"12abc", VT_I4, VC_MISMATCH, 0 }, { L"1.5.2", VT_I4, VC_MISMATCH, 0 },
        { L"--1", VT_I4, VC_MISMATCH, 0 }, { L"1 2", VT_I4, VC_MISMATCH, 0 }, { L"e5", VT_I4, VC_MISMATCH, 0 }, { L".", VT_I4, VC_MISMATCH, 0 },
        { L"32767", VT_I2, S_OK, 32767 }, { L"32768", VT_I2, VC_OVERFLOW, 0 }, { L"-32769", VT_I2, VC_OVERFLOW, 0 },
        { L"255", VT_UI1, S_OK, 255 }, { L"256", VT_UI1, VC_OVERFLOW, 0 }, { L"-1", VT_UI1, VC_OVERFLOW, 0 },
        { L"127", VT_I1, S_OK, 127 }, { L"128", VT_I1, VC_OVERFLOW, 0 },
        { L"65535", VT_UI2, S_OK, 65535 }, { L"65536", VT_UI2, VC_OVERFLOW, 0 },
        { L"4294967295", VT_UI4, S_OK, 4294967295.0 }, { L"4294967296", VT_UI4, VC_OVERFLOW, 0 }, { L"-1", VT_UI4, VC_OVERFLOW, 0 },
        { L"1.25", VT_R8, S_OK, 1.25 }, { L".5", VT_R8, S_OK, 0.5 }, { L"-.5", VT_R8, S_OK, -0.5 }, { L"1e-2", VT_R8, S_OK, 0.01 },
        { L"1.5", VT_R4, S_OK, 1.5 }, { L"1e39", VT_R4, VC_OVERFLOW, 0 }, { L"abc", VT_R8, VC_MISMATCH, 0 }, { L"1e999", VT_R8, VC_OVERFLOW, 0 },
        { L"1.5", VT_CY, S_OK, 1.5 }, { L"-3", VT_CY, S_OK, -3 },
        { L"1", VT_BOOL, S_OK, -1 }, { L"0", VT_BOOL, S_OK, 0 }, { L"-1", VT_BOOL, S_OK, -1 }, { L"2", VT_BOOL, S_OK, -1 },
        { L"True", VT_BOOL, S_OK, -1 }, { L"False", VT_BOOL, S_OK, 0 }, { L"#TRUE#", VT_BOOL, S_OK, -1 }, { L"#FALSE#", VT_BOOL, S_OK, 0 },
        { L"abc", VT_BOOL, VC_MISMATCH, 0 }, { L"", VT_BOOL, VC_MISMATCH, 0 },
        { L"1/2/2000", VT_DATE, S_OK, 36527.0 }, { L"1/2/2000 12:00:00 PM", VT_DATE, S_OK, 36527.5 }, { L"garbage", VT_DATE, VC_MISMATCH, 0 },
        { L"18446744073709551615", VT_UI8, S_OK, 18446744073709551615.0 }, { L"-1", VT_UI8, VC_OVERFLOW, 0 },
        { L"9223372036854775807", VT_I8, S_OK, 9223372036854775807.0 }, { L"9223372036854775808", VT_I8, VC_OVERFLOW, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; set_bstr(&s, t[i].s); mark_dest(&d);
        HRESULT hr = conv(&d, &s, t[i].dvt, 0);
        if (hr != t[i].hr) { CHECKF(0, "case %u: \"%ls\" -> vt%u hr 0x%08lx want 0x%08lx", i, t[i].s, t[i].dvt, (unsigned long)hr, (unsigned long)t[i].hr); }
        else if (hr == S_OK) CHECKF(V_VT(&d) == t[i].dvt && vc_dbl_eq(get_num(&d), t[i].want), "case %u: \"%ls\" -> vt%u got %g want %g", i, t[i].s, t[i].dvt, get_num(&d), t[i].want);
        else CHECKF(dest_untouched(&d), "case %u dest modified on failure", i);
        w_VariantClear(&s);
    }
}
TEST(vc_change_bstr_to_number_hex_octal_prefix) {
    VC_REQ();
    static const struct { const WCHAR *s; double want; } t[] = { { L"&H10", 16 }, { L"&HFF", 255 }, { L"&O17", 15 }, { L"&H7FFF", 32767 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; set_bstr(&s, t[i].s); w_VariantInit(&d);
        HRESULT hr = conv(&d, &s, VT_I4, 0);
        t_note("\"%ls\" -> I4 hr=0x%08lx value=%g", t[i].s, (unsigned long)hr, hr == S_OK ? get_num(&d) : 0.0);
        if (hr == S_OK) CHECKF(get_num(&d) == t[i].want, "\"%ls\" got %g", t[i].s, get_num(&d));
        w_VariantClear(&s);
    }
}
TEST(vc_change_bstr_to_number_german_locale) {
    VC_REQ();
    VARIANT s, d; set_bstr(&s, L"1,5"); w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE, VT_R8), S_OK); CHECK(V_R8(&d) == 1.5);
    w_VariantClear(&s); set_bstr(&s, L"1.5");
    d.vt = VT_EMPTY;
    HRESULT hr = w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE, VT_R8);
    t_note("\"1.5\" in de-DE -> R8 hr=0x%08lx value=%g", (unsigned long)hr, hr == S_OK ? V_R8(&d) : 0.0);
    w_VariantClear(&s);
}
TEST(vc_change_bstr_null_pointer_source) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_BSTR; mark_dest(&d);
    HRESULT hr = conv(&d, &s, VT_I4, 0);
    t_note("NULL BSTR -> I4 hr=0x%08lx", (unsigned long)hr);
    CHECK(hr < 0);
    hr = conv(&d, &s, VT_BSTR, 0);
    HR_EQ(hr, S_OK); EXPECT_EQ(V_VT(&d), VT_BSTR);
    w_VariantClear(&d);
}
TEST(vc_change_bstr_roundtrip_numbers) {
    VC_REQ();
    static const double vals[] = { 0, 1, -1, 3.14159, -273.15, 1e10, 1234567.125 };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        VARIANT s, b, d; set_num(&s, VT_R8, vals[i]); w_VariantInit(&b); w_VariantInit(&d);
        HR_EQ(conv(&b, &s, VT_BSTR, 0), S_OK);
        HR_EQ(conv(&d, &b, VT_R8, 0), S_OK);
        CHECKF(vc_dbl_eq(V_R8(&d), vals[i]), "roundtrip %g -> %g", vals[i], V_R8(&d));
        w_VariantClear(&b);
    }
}

/* ---------------------------------------------------------------- booleans */
TEST(vc_change_bool_to_bstr_flags) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&d);
    set_num(&s, VT_BOOL, 1);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"-1"); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_ALPHABOOL), S_OK); EXPECT_WSTR(V_BSTR(&d), L"True"); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_LOCALBOOL), S_OK); EXPECT_WSTR(V_BSTR(&d), L"True"); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_ALPHABOOL | VARIANT_LOCALBOOL), S_OK); EXPECT_WSTR(V_BSTR(&d), L"True"); w_VariantClear(&d);
    set_num(&s, VT_BOOL, 0);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"0"); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_ALPHABOOL), S_OK); EXPECT_WSTR(V_BSTR(&d), L"False"); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_LOCALBOOL), S_OK); EXPECT_WSTR(V_BSTR(&d), L"False"); w_VariantClear(&d);
}
TEST(vc_change_bool_to_bstr_localized_german) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_BOOL, 1); w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE | VARIANT_LOCALBOOL, VT_BSTR), S_OK);
    t_note("BOOL TRUE LOCALBOOL de-DE -> \"%ls\"", V_BSTR(&d));
    CHECK(lstrcmpW(V_BSTR(&d), L"-1") != 0);
    w_VariantClear(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, VARIANT_NOUSEROVERRIDE | VARIANT_ALPHABOOL, VT_BSTR), S_OK);
    EXPECT_WSTR(V_BSTR(&d), L"True"); /* ALPHABOOL without LOCALBOOL stays English */
    w_VariantClear(&d);
}
TEST(vc_change_bool_to_non_bstr_ignores_alpha_flags) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_BOOL, 1); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_I4, VARIANT_ALPHABOOL | VARIANT_LOCALBOOL), S_OK); EXPECT_EQ(V_I4(&d), -1);
    HR_EQ(conv(&d, &s, VT_R8, VARIANT_ALPHABOOL), S_OK); CHECK(V_R8(&d) == -1.0);
}
TEST(vc_change_bool_true_widening_values) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_BOOL, 1); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_UI1, 0), S_OK);
    t_note("BOOL TRUE -> UI1: value=%g", get_num(&d));
    HR_EQ(conv(&d, &s, VT_UI2, 0), S_OK); EXPECT_EQ(V_UI2(&d), 0xFFFF);
    HR_EQ(conv(&d, &s, VT_UI4, 0), S_OK); EXPECT_EQ(V_UI4(&d), 0xFFFFFFFFu);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), -1);
    HR_EQ(conv(&d, &s, VT_I1, 0), S_OK); EXPECT_EQ(V_I1(&d), -1);
}

/* ---------------------------------------------------------------- byref sources */
TEST(vc_change_byref_scalar_sources) {
    VC_REQ();
    LONG i4 = 41; double r8 = 2.5; SHORT i2 = -3; VARIANT s, d;
    w_VariantInit(&s); V_VT(&s) = VT_I4 | VT_BYREF; V_I4REF(&s) = &i4; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_R8, 0), S_OK); CHECK(V_R8(&d) == 41.0);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"41"); w_VariantClear(&d);
    V_VT(&s) = VT_R8 | VT_BYREF; V_R8REF(&s) = &r8;
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 2);
    V_VT(&s) = VT_I2 | VT_BYREF; V_I2REF(&s) = &i2;
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), -3);
    EXPECT_EQ(V_VT(&s), VT_I2 | VT_BYREF); /* source untouched */
}
TEST(vc_change_byref_same_type_result_is_not_byref) {
    VC_REQ();
    LONG i4 = 9; VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_I4 | VT_BYREF; V_I4REF(&s) = &i4; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_I4); EXPECT_EQ(V_I4(&d), 9);
}
TEST(vc_change_byref_bstr_source) {
    VC_REQ();
    BSTR b = w_SysAllocString(L"250"); VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_BSTR | VT_BYREF; V_BSTRREF(&s) = &b; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_UI1, 0), S_OK); EXPECT_EQ(V_UI1(&d), 250);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"250"); CHECK(V_BSTR(&d) != b);
    w_VariantClear(&d); w_SysFreeString(b);
}
TEST(vc_change_byref_variant_source) {
    VC_REQ();
    VARIANT inner, s, d; set_num(&inner, VT_I2, 12); w_VariantInit(&s); V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &inner; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 12);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"12"); w_VariantClear(&d);
    /* inner is itself a byref */
    LONG i4 = 77; V_VT(&inner) = VT_I4 | VT_BYREF; V_I4REF(&inner) = &i4;
    HR_EQ(conv(&d, &s, VT_R8, 0), S_OK); CHECK(V_R8(&d) == 77.0);
    /* inner is NULL / empty */
    w_VariantInit(&inner);
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 0);
    set_num(&inner, VT_I4, 1); V_VT(&inner) = VT_NULL;
    mark_dest(&d); HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
}
TEST(vc_change_byref_variant_to_byref_variant_fails) {
    VC_REQ();
    VARIANT a, b, s, d; set_num(&a, VT_I4, 1);
    w_VariantInit(&b); V_VT(&b) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&b) = &a;
    w_VariantInit(&s); V_VT(&s) = VT_VARIANT | VT_BYREF; V_VARIANTREF(&s) = &b; mark_dest(&d);
    HRESULT hr = conv(&d, &s, VT_I4, 0);
    t_note("VARIANT|BYREF -> VARIANT|BYREF -> I4: hr=0x%08lx", (unsigned long)hr);
    CHECK(hr < 0); CHECK(dest_untouched(&d));
}
TEST(vc_change_byref_null_pointer) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); mark_dest(&d);
    V_VT(&s) = VT_I4 | VT_BYREF; HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    V_VT(&s) = VT_BSTR | VT_BYREF; HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    V_VT(&s) = VT_VARIANT | VT_BYREF; HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    CHECK(dest_untouched(&d));
}
TEST(vc_change_byref_in_place) {
    VC_REQ();
    LONG i4 = 1234; VARIANT v; w_VariantInit(&v); V_VT(&v) = VT_I4 | VT_BYREF; V_I4REF(&v) = &i4;
    HR_EQ(conv(&v, &v, VT_R8, 0), S_OK);
    EXPECT_EQ(V_VT(&v), VT_R8); CHECK(V_R8(&v) == 1234.0); EXPECT_EQ(i4, 1234);
}

/* ---------------------------------------------------------------- IDispatch default value / objects */
TEST(vc_change_dispatch_default_value_property) {
    VC_REQ();
    static const VARTYPE dsts[] = { VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BSTR, VT_BOOL, VT_DECIMAL, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
    for (unsigned i = 0; i < sizeof dsts / sizeof *dsts; i++) {
        MD m; md_init(&m); V_VT(&m.result) = VT_I4; V_I4(&m.result) = 42; m.invoke_hr = S_OK;
        VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; mark_dest(&d);
        HRESULT hr = conv(&d, &s, dsts[i], 0);
        CHECKF(hr == S_OK && V_VT(&d) == dsts[i], "dispatch -> vt%u hr 0x%08lx vt %u", dsts[i], (unsigned long)hr, V_VT(&d));
        EXPECT_EQ(m.ninvoke, 1);
        if (hr == S_OK && dsts[i] != VT_BSTR && dsts[i] != VT_DECIMAL && dsts[i] != VT_BOOL) CHECKF(vc_dbl_eq(get_num(&d), 42.0), "dispatch -> vt%u value %g", dsts[i], get_num(&d));
        if (hr == S_OK) w_VariantClear(&d);
        CHECK(m.ref == 1);
    }
}
TEST(vc_change_dispatch_invoke_arguments) {
    VC_REQ();
    MD m; md_init(&m); V_VT(&m.result) = VT_I4; V_I4(&m.result) = 5;
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_DE, 0, VT_I4), S_OK);
    EXPECT_EQ(m.ninvoke, 1);
    EXPECT_EQ(m.last_dispid, DISPID_VALUE);
    CHECK(m.last_flags & DISPATCH_PROPERTYGET);
    EXPECT_EQ(m.last_cargs, 0);
    CHECK(m.last_riid_null);
    EXPECT_EQ(m.last_has_result, 1);
    EXPECT_EQ(m.last_lcid, VC_LCID_DE);
    EXPECT_EQ(V_I4(&d), 5);
}
TEST(vc_change_dispatch_novalueprop_flag) {
    VC_REQ();
    MD m; md_init(&m); V_VT(&m.result) = VT_I4; V_I4(&m.result) = 5;
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_I4, VARIANT_NOVALUEPROP), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_BSTR, VARIANT_NOVALUEPROP), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_R8, VARIANT_NOVALUEPROP | VARIANT_ALPHABOOL), VC_MISMATCH);
    EXPECT_EQ(m.ninvoke, 0);
    CHECK(dest_untouched(&d));
    /* NOVALUEPROP does not prevent same-type / EMPTY / NULL results */
    HR_EQ(conv(&d, &s, VT_DISPATCH, VARIANT_NOVALUEPROP), S_OK); EXPECT_EQ(V_VT(&d), VT_DISPATCH); w_VariantClear(&d);
    HR_EQ(conv(&d, &s, VT_EMPTY, VARIANT_NOVALUEPROP), S_OK);
    HR_EQ(conv(&d, &s, VT_NULL, VARIANT_NOVALUEPROP), S_OK);
    EXPECT_EQ(m.ninvoke, 0);
}
TEST(vc_change_dispatch_invoke_failure_is_type_mismatch) {
    VC_REQ();
    static const HRESULT fails[] = { E_FAIL, DISP_E_MEMBERNOTFOUND, E_NOTIMPL, DISP_E_EXCEPTION };
    for (unsigned i = 0; i < sizeof fails / sizeof *fails; i++) {
        MD m; md_init(&m); m.invoke_hr = fails[i];
        VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; mark_dest(&d);
        HRESULT hr = conv(&d, &s, VT_I4, 0);
        CHECKF(hr == VC_MISMATCH, "invoke hr 0x%08lx -> 0x%08lx", (unsigned long)fails[i], (unsigned long)hr);
        EXPECT_EQ(m.ninvoke, 1); CHECK(dest_untouched(&d));
    }
}
TEST(vc_change_dispatch_default_value_types) {
    VC_REQ();
    MD m; md_init(&m); m.invoke_hr = S_OK;
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; w_VariantInit(&d);
    V_VT(&m.result) = VT_BSTR; V_BSTR(&m.result) = w_SysAllocString(L"123");
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 123);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), S_OK); EXPECT_WSTR(V_BSTR(&d), L"123"); w_VariantClear(&d);
    w_VariantClear(&m.result);
    V_VT(&m.result) = VT_R8; V_R8(&m.result) = 2.5;
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 2);
    V_VT(&m.result) = VT_BOOL; V_BOOL(&m.result) = VARIANT_TRUE;
    HR_EQ(conv(&d, &s, VT_I2, 0), S_OK); EXPECT_EQ(V_I2(&d), -1);
    V_VT(&m.result) = VT_EMPTY;
    HR_EQ(conv(&d, &s, VT_I4, 0), S_OK); EXPECT_EQ(V_I4(&d), 0);
    V_VT(&m.result) = VT_NULL;
    mark_dest(&d); HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    V_VT(&m.result) = VT_ERROR; V_ERROR(&m.result) = 0x80020004;
    HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    V_VT(&m.result) = VT_UNKNOWN; V_UNKNOWN(&m.result) = NULL;
    HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    CHECK(dest_untouched(&d));
}
TEST(vc_change_dispatch_null_pointer) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_DISPATCH; mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_I4, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_BSTR, 0), VC_MISMATCH);
    CHECK(dest_untouched(&d));
}
TEST(vc_change_dispatch_unusable_targets_do_not_invoke) {
    VC_REQ();
    MD m; md_init(&m); V_VT(&m.result) = VT_I4; V_I4(&m.result) = 5;
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_ERROR, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_VARIANT, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_RECORD, 0), VC_MISMATCH);
    EXPECT_EQ(m.ninvoke, 0);
}
TEST(vc_change_dispatch_to_dispatch_and_unknown) {
    VC_REQ();
    MD m; md_init(&m);
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_DISPATCH, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DISPATCH); CHECK(V_DISPATCH(&d) == (IDispatch *)&m);
    EXPECT_EQ(m.ref, 2);
    w_VariantClear(&d); EXPECT_EQ(m.ref, 1);
    HR_EQ(conv(&d, &s, VT_UNKNOWN, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == (IUnknown *)&m);
    EXPECT_EQ(m.ref, 2);
    w_VariantClear(&d); EXPECT_EQ(m.ref, 1);
    EXPECT_EQ(m.ninvoke, 0);
}
TEST(vc_change_dispatch_to_empty_and_null) {
    VC_REQ();
    MD m; md_init(&m);
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_DISPATCH; V_DISPATCH(&s) = (IDispatch *)&m; mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_EMPTY, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_EMPTY);
    HR_EQ(conv(&d, &s, VT_NULL, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_NULL);
    EXPECT_EQ(m.ref, 1); EXPECT_EQ(m.ninvoke, 0);
}
TEST(vc_change_unknown_to_dispatch_queries_interface) {
    VC_REQ();
    MD m; md_init(&m);
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_UNKNOWN; V_UNKNOWN(&s) = (IUnknown *)&m; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_DISPATCH, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_DISPATCH); CHECK(V_DISPATCH(&d) == (IDispatch *)&m);
    EXPECT_EQ(m.nqi, 1);
    w_VariantClear(&d);
    EXPECT_EQ(m.ref, 1);
    /* identity */
    HR_EQ(conv(&d, &s, VT_UNKNOWN, 0), S_OK); CHECK(V_UNKNOWN(&d) == (IUnknown *)&m);
    w_VariantClear(&d); EXPECT_EQ(m.ref, 1);
}
TEST(vc_change_unknown_without_idispatch_fails) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_UNKNOWN; V_UNKNOWN(&s) = (IUnknown *)&u; mark_dest(&d);
    HRESULT hr = conv(&d, &s, VT_DISPATCH, 0);
    t_note("IUnknown without IDispatch -> VT_DISPATCH hr=0x%08lx", (unsigned long)hr);
    CHECK(hr < 0); CHECK(dest_untouched(&d));
    EXPECT_EQ(u.nqi, 1); EXPECT_EQ(u.ref, 1);
}
TEST(vc_change_unknown_other_targets_mismatch) {
    VC_REQ();
    MU u; mu_init(&u);
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_UNKNOWN; V_UNKNOWN(&s) = (IUnknown *)&u; mark_dest(&d);
    static const VARTYPE dsts[] = { VT_I2, VT_I4, VT_R8, VT_BSTR, VT_BOOL, VT_CY, VT_DATE, VT_DECIMAL, VT_UI1, VT_ERROR };
    for (unsigned i = 0; i < sizeof dsts / sizeof *dsts; i++) { HR_EQ(conv(&d, &s, dsts[i], 0), VC_MISMATCH); }
    CHECK(dest_untouched(&d));
    HR_EQ(conv(&d, &s, VT_EMPTY, 0), S_OK); HR_EQ(conv(&d, &s, VT_NULL, 0), S_OK);
    EXPECT_EQ(u.nadd + u.nrel, 0);
}
TEST(vc_change_unknown_null_pointer) {
    VC_REQ();
    VARIANT s, d; memset(&s, 0, sizeof s); V_VT(&s) = VT_UNKNOWN; w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_UNKNOWN, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_UNKNOWN); CHECK(V_UNKNOWN(&d) == NULL);
    HR_EQ(conv(&d, &s, VT_DISPATCH, 0), S_OK); EXPECT_EQ(V_VT(&d), VT_DISPATCH); CHECK(V_DISPATCH(&d) == NULL);
}
TEST(vc_change_numbers_to_object_types_mismatch) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_I4, 1); mark_dest(&d);
    HR_EQ(conv(&d, &s, VT_UNKNOWN, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_DISPATCH, 0), VC_MISMATCH);
    HR_EQ(conv(&d, &s, VT_ERROR, 0), VC_MISMATCH);
    CHECK(dest_untouched(&d));
}

/* ---------------------------------------------------------------- BSTR <-> byte vector */
TEST(vc_change_bstr_to_byte_array_and_back) {
    VC_REQ(); REQUIRE(U(SafeArrayGetUBound)); REQUIRE(U(SafeArrayAccessData)); REQUIRE(U(SafeArrayUnaccessData));
    VARIANT s, d, e; set_bstr(&s, L"AB"); w_VariantInit(&d); w_VariantInit(&e);
    HRESULT hr = conv(&d, &s, VT_ARRAY | VT_UI1, 0);
    HR_EQ(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_UI1);
        LONG lb = -1, ub = -1; void *p = NULL;
        HR_EQ(w_SafeArrayGetLBound(V_ARRAY(&d), 1, &lb), S_OK); HR_EQ(w_SafeArrayGetUBound(V_ARRAY(&d), 1, &ub), S_OK);
        EXPECT_EQ(lb, 0); EXPECT_EQ(ub, 3);
        HR_EQ(w_SafeArrayAccessData(V_ARRAY(&d), &p), S_OK);
        CHECK(p && !memcmp(p, L"AB", 4));
        w_SafeArrayUnaccessData(V_ARRAY(&d));
        HR_EQ(conv(&e, &d, VT_BSTR, 0), S_OK);
        EXPECT_EQ(V_VT(&e), VT_BSTR); EXPECT_WSTR(V_BSTR(&e), L"AB"); EXPECT_EQ(w_SysStringLen(V_BSTR(&e)), 2);
        w_VariantClear(&e); w_VariantClear(&d);
    }
    w_VariantClear(&s);
}
TEST(vc_change_byte_array_same_type_deep_copies) {
    VC_REQ(); REQUIRE(U(SafeArrayCreateVector));
    VARIANT s, d; w_VariantInit(&s); V_VT(&s) = VT_ARRAY | VT_UI1; V_ARRAY(&s) = w_SafeArrayCreateVector(VT_UI1, 0, 8); w_VariantInit(&d);
    HR_EQ(conv(&d, &s, VT_ARRAY | VT_UI1, 0), S_OK);
    EXPECT_EQ(V_VT(&d), VT_ARRAY | VT_UI1); CHECK(V_ARRAY(&d) != V_ARRAY(&s) && V_ARRAY(&d) != NULL);
    w_VariantClear(&s); w_VariantClear(&d);
}

/* ---------------------------------------------------------------- misc flags */
TEST(vc_change_flags_nouseroverride_has_no_effect_on_invariant_values) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_R8, 1.5); w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, 0, VT_BSTR), S_OK); EXPECT_WSTR(V_BSTR(&d), L"1.5"); w_VariantClear(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, VARIANT_NOUSEROVERRIDE, VT_BSTR), S_OK); EXPECT_WSTR(V_BSTR(&d), L"1.5"); w_VariantClear(&d);
    set_num(&s, VT_I4, 12);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, VC_LCID_US, VARIANT_NOUSEROVERRIDE, VT_I2), S_OK); EXPECT_EQ(V_I2(&d), 12);
}
TEST(vc_change_unknown_flag_bits_are_ignored) {
    VC_REQ();
    static const USHORT fl[] = { 0x08, 0x20, 0x40, 0x80, 0x100, 0x8000 };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        VARIANT s, d; set_num(&s, VT_I4, 12); w_VariantInit(&d);
        HRESULT hr = w_VariantChangeTypeEx(&d, &s, VC_LCID_US, fl[i], VT_I2);
        t_note("flag 0x%x I4->I2 hr=0x%08lx", fl[i], (unsigned long)hr);
        CHECK(hr == S_OK || hr < 0);
    }
}
TEST(vc_change_lcid_user_default_zero_works) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_I4, 12); w_VariantInit(&d);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, 0, 0, VT_I2), S_OK); EXPECT_EQ(V_I2(&d), 12);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, LOCALE_USER_DEFAULT, 0, VT_R8), S_OK);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, LOCALE_SYSTEM_DEFAULT, 0, VT_R8), S_OK);
    HR_EQ(w_VariantChangeTypeEx(&d, &s, LOCALE_NEUTRAL, 0, VT_R8), S_OK);
}
TEST(vc_change_dest_with_illegal_vt_is_error) {
    VC_REQ();
    VARIANT s, d; set_num(&s, VT_I4, 12); memset(&d, 0, sizeof d); V_VT(&d) = 31;
    HRESULT hr = conv(&d, &s, VT_I2, 0);
    t_note("dest vt 31 -> hr=0x%08lx", (unsigned long)hr);
    CHECK(hr < 0);
    EXPECT_EQ(V_VT(&d), 31);
}
