/* variant_core: additional conversion semantics (value conversions with documented, locale-independent results) */
#include "vc.h"

static HRESULT cv(VARIANT *d, VARIANT *s, VARTYPE vt) { return w_VariantChangeTypeEx(d, s, VC_LCID_US, 0, vt); }

TEST(vc_extra_r8_to_int_banker_rounding) {
    VC_REQ();
    static const struct { double in; LONG out; } t[] = {
        { 0.5, 0 }, { 1.5, 2 }, { 2.5, 2 }, { 3.5, 4 }, { -0.5, 0 }, { -1.5, -2 }, { -2.5, -2 }, { 2.4999, 2 }, { 2.5001, 3 }, { -2.5001, -3 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
        V_VT(&s) = VT_R8; V_R8(&s) = t[i].in;
        HR_EQ(cv(&d, &s, VT_I4), S_OK);
        CHECKF(V_VT(&d) == VT_I4 && V_I4(&d) == t[i].out, "R8 %g -> I4 %ld want %ld", t[i].in, (long)V_I4(&d), (long)t[i].out);
    }
}

TEST(vc_extra_r8_to_small_ints_overflow) {
    VC_REQ();
    static const struct { double in; VARTYPE vt; HRESULT hr; } t[] = {
        { 32767.4, VT_I2, S_OK }, { 32767.5, VT_I2, VC_OVERFLOW }, { -32768.0, VT_I2, S_OK }, { -32768.6, VT_I2, VC_OVERFLOW },
        { 255.4, VT_UI1, S_OK }, { 255.6, VT_UI1, VC_OVERFLOW }, { -0.6, VT_UI1, VC_OVERFLOW }, { 65535.0, VT_UI2, S_OK }, { 65536.0, VT_UI2, VC_OVERFLOW },
        { 127.0, VT_I1, S_OK }, { 128.0, VT_I1, VC_OVERFLOW }, { 2147483647.0, VT_I4, S_OK }, { 2147483648.0, VT_I4, VC_OVERFLOW },
        { 4294967295.0, VT_UI4, S_OK }, { 4294967296.0, VT_UI4, VC_OVERFLOW }, { -1.0, VT_UI4, VC_OVERFLOW },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
        V_VT(&s) = VT_R8; V_R8(&s) = t[i].in;
        HRESULT hr = cv(&d, &s, t[i].vt);
        CHECKF(hr == t[i].hr, "R8 %g -> vt %u: hr 0x%08lx want 0x%08lx", t[i].in, t[i].vt, (unsigned long)hr, (unsigned long)t[i].hr);
        if (hr == S_OK) w_VariantClear(&d);
    }
}

TEST(vc_extra_cy_conversions) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
    V_VT(&s) = VT_CY; V_CY(&s).int64 = 12345;     /* 1.2345 */
    HR_EQ(cv(&d, &s, VT_R8), S_OK); CHECK(vc_dbl_eq(V_R8(&d), 1.2345));
    HR_EQ(cv(&d, &s, VT_I4), S_OK); EXPECT_EQ(V_I4(&d), 1);
    V_CY(&s).int64 = 25000;                        /* 2.5 -> banker's rounding */
    HR_EQ(cv(&d, &s, VT_I4), S_OK); EXPECT_EQ(V_I4(&d), 2);
    V_CY(&s).int64 = 35000;
    HR_EQ(cv(&d, &s, VT_I4), S_OK); EXPECT_EQ(V_I4(&d), 4);
    V_VT(&s) = VT_R8; V_R8(&s) = 3.0;
    HR_EQ(cv(&d, &s, VT_CY), S_OK); CHECK(V_CY(&d).int64 == 30000);
    V_R8(&s) = 1e15;                               /* CY max is ~9.22e14 */
    HR_EQ(cv(&d, &s, VT_CY), VC_OVERFLOW);
    V_VT(&s) = VT_I4; V_I4(&s) = -7;
    HR_EQ(cv(&d, &s, VT_CY), S_OK); CHECK(V_CY(&d).int64 == -70000);
}

TEST(vc_extra_date_r8_identity) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
    V_VT(&s) = VT_DATE; V_DATE(&s) = 36526.75;
    HR_EQ(cv(&d, &s, VT_R8), S_OK); CHECK(V_R8(&d) == 36526.75);
    V_VT(&s) = VT_R8; V_R8(&s) = -1.25;
    HR_EQ(cv(&d, &s, VT_DATE), S_OK); EXPECT_EQ(V_VT(&d), VT_DATE); CHECK(V_DATE(&d) == -1.25);
    V_VT(&s) = VT_I4; V_I4(&s) = 5;
    HR_EQ(cv(&d, &s, VT_DATE), S_OK); CHECK(V_DATE(&d) == 5.0);
}

TEST(vc_extra_bool_from_numbers) {
    VC_REQ();
    static const struct { double in; VARIANT_BOOL out; } t[] = { {0, 0}, {1, -1}, {-1, -1}, {2, -1}, {0.4, -1}, {-3.5, -1}, {1e10, -1} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
        V_VT(&s) = VT_R8; V_R8(&s) = t[i].in;
        HR_EQ(cv(&d, &s, VT_BOOL), S_OK);
        CHECKF(V_VT(&d) == VT_BOOL && V_BOOL(&d) == t[i].out, "R8 %g -> BOOL %d want %d", t[i].in, V_BOOL(&d), t[i].out);
    }
    VARIANT s, d; w_VariantInit(&d);
    V_VT(&s) = VT_I8; V_I8(&s) = 0x100000000LL;
    HR_EQ(cv(&d, &s, VT_BOOL), S_OK); EXPECT_EQ(V_BOOL(&d), VARIANT_TRUE);
}

TEST(vc_extra_empty_and_null_to_bstr) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
    HR_EQ(cv(&d, &s, VT_BSTR), S_OK);
    EXPECT_EQ(V_VT(&d), VT_BSTR);
    CHECK(V_BSTR(&d) != NULL && w_SysStringLen(V_BSTR(&d)) == 0);
    w_VariantClear(&d);
    V_VT(&s) = VT_NULL;
    HR_EQ(cv(&d, &s, VT_BSTR), VC_MISMATCH);
    V_VT(&s) = VT_ERROR; V_ERROR(&s) = 5;
    HR_EQ(cv(&d, &s, VT_BSTR), VC_MISMATCH);
}

TEST(vc_extra_ui1_to_i1_and_wider_signed_values) {
    VC_REQ();
    VARIANT s, d; w_VariantInit(&s); w_VariantInit(&d);
    V_VT(&s) = VT_UI1; V_UI1(&s) = 200;
    HR_EQ(cv(&d, &s, VT_I2), S_OK); EXPECT_EQ(V_I2(&d), 200);
    HR_EQ(cv(&d, &s, VT_UI8), S_OK); CHECK(V_UI8(&d) == 200);
    V_VT(&s) = VT_I2; V_I2(&s) = -300;
    HR_EQ(cv(&d, &s, VT_I1), VC_OVERFLOW);
    HR_EQ(cv(&d, &s, VT_UI1), VC_OVERFLOW);
    HR_EQ(cv(&d, &s, VT_I8), S_OK); CHECK(V_I8(&d) == -300);
}

TEST(vc_extra_copy_preserves_all_scalar_payload_bits) {
    VC_REQ();
    static const VARTYPE vts[] = { VT_I2, VT_I4, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BOOL, VT_ERROR, VT_I1, VT_UI1, VT_UI2, VT_UI4, VT_I8, VT_UI8, VT_INT, VT_UINT };
    for (unsigned i = 0; i < sizeof vts / sizeof *vts; i++) {
        VARIANT s, d; memset(&s, 0, sizeof s); memset(&d, 0xAA, sizeof d); w_VariantInit(&d);
        V_VT(&s) = vts[i]; V_UI8(&s) = 0x8123456789ABCDEFULL;
        HR_EQ(w_VariantCopy(&d, &s), S_OK);
        CHECKF(V_VT(&d) == vts[i] && V_UI8(&d) == V_UI8(&s), "copy of vt %u", vts[i]);
    }
}
