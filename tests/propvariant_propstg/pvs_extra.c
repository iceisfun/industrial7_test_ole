/* Additional coverage: independence of sets, name+id mixing, unbuffered mode, value fidelity. */
#include "pv.h"

#define WITH_PS(leaf, flags) \
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN(); FX f; \
    if (fx_new(&f, leaf, &G_ANY_GUID, flags)) {
#define END_PS() } fx_close(&f); COM_END();

TEST(pvps_x_two_sets_are_independent) {
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN();
    FX f;
    if (fx_new(&f, "x_two_sets.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        IPropertyStorage *p2 = NULL;
        EQ(CALL(f.pss, Create, &G_ANY_GUID2, NULL, PROPSETFLAG_DEFAULT, STGM_RW_EX_CREATE, &p2), S_OK);
        if (p2) {
            EQ(wr_i4(f.ps, 7, 111), S_OK);
            EQ(wr_i4(p2, 7, 222), S_OK);
            VARTYPE vt; LONG x;
            EQ(rd_i4(f.ps, 7, &vt, &x), S_OK); EQ(x, 111);
            EQ(rd_i4(p2, 7, &vt, &x), S_OK); EQ(x, 222);
            PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = 7;
            EQ(CALL(p2, DeleteMultiple, 1, &s), S_OK);
            EQ(rd_i4(f.ps, 7, &vt, &x), S_OK); EQ(x, 111);
            EQ(rd_i4(p2, 7, &vt, &x), S_FALSE); EQ(vt, VT_EMPTY);
            CALL(p2, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_x_i8_ui8_r8_filetime_clsid_roundtrip) {
    WITH_PS("x_wide_scalars.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v[5]; memset(v, 0, sizeof v);
        v[0].vt = VT_I8; v[0].hVal.QuadPart = -0x123456789ALL;
        v[1].vt = VT_UI8; v[1].uhVal.QuadPart = 0xFEDCBA9876543210ULL;
        v[2].vt = VT_R8; v[2].dblVal = -1234.5625;
        v[3].vt = VT_FILETIME; v[3].filetime.dwLowDateTime = 0x89ABCDEF; v[3].filetime.dwHighDateTime = 0x01234567;
        GUID g = G_ANY_GUID2; v[4].vt = VT_CLSID; v[4].puuid = &g;
        for (int i = 0; i < 5; i++) EQ(wr_id(f.ps, 20 + i, &v[i]), S_OK);
        EQ(fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX), 1);
        if (f.ps) {
            PROPVARIANT r;
            EQ(rd_id(f.ps, 20, &r), S_OK); EQ(r.vt, VT_I8); CHECK(r.hVal.QuadPart == v[0].hVal.QuadPart); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, 21, &r), S_OK); EQ(r.vt, VT_UI8); CHECK(r.uhVal.QuadPart == v[1].uhVal.QuadPart); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, 22, &r), S_OK); EQ(r.vt, VT_R8); CHECK(r.dblVal == -1234.5625); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, 23, &r), S_OK); EQ(r.vt, VT_FILETIME);
            CHECK(r.filetime.dwLowDateTime == 0x89ABCDEF && r.filetime.dwHighDateTime == 0x01234567); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, 24, &r), S_OK); EQ(r.vt, VT_CLSID); CHECK(r.puuid && guid_eq(r.puuid, &G_ANY_GUID2)); U(PropVariantClear)(&r);
        }
    END_PS()
}

TEST(pvps_x_name_and_id_same_property) {
    WITH_PS("x_name_id.stg", PROPSETFLAG_DEFAULT)
        EQ(wr_name_i4(f.ps, L"Alpha", 5, 100), S_OK);
        PROPID id = 0;
        EQ(name_to_id(f.ps, L"Alpha", &id), S_OK);
        CHECK(id >= PID_FIRST_USABLE);
        VARTYPE vt; LONG x = 0;
        EQ(rd_i4(f.ps, id, &vt, &x), S_OK); EQ(vt, VT_I4); EQ(x, 5);
        EQ(wr_i4(f.ps, id, 6), S_OK);
        PROPVARIANT r; EQ(rd_name(f.ps, L"Alpha", &r), S_OK); EQ(r.vt, VT_I4); EQ(r.lVal, 6);
        U(PropVariantClear)(&r);
    END_PS()
}

TEST(pvps_x_unbuffered_flag_roundtrip) {
    WITH_PS("x_unbuf.stg", PROPSETFLAG_UNBUFFERED)
        EQ(wr_i4(f.ps, 9, 1234), S_OK);
        EQ(CALL(f.ps, Commit, STGC_DEFAULT), S_OK);
        VARTYPE vt; LONG x;
        EQ(rd_i4(f.ps, 9, &vt, &x), S_OK); EQ(x, 1234);
        STATPROPSETSTG st; memset(&st, 0, sizeof st);
        EQ(CALL(f.ps, Stat, &st), S_OK);
        CHECK(guid_eq(&st.fmtid, &G_ANY_GUID));
        t_note("Stat grfFlags=0x%lx for UNBUFFERED set", (unsigned long)st.grfFlags);
    END_PS()
}

TEST(pvps_x_overwrite_with_larger_value_persists) {
    WITH_PS("x_grow.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"a";
        EQ(wr_id(f.ps, 30, &v), S_OK);
        EQ(wr_i4(f.ps, 31, 77), S_OK);
        static WCHAR big[600]; for (int i = 0; i < 599; i++) big[i] = L'z'; big[599] = 0;
        v.pwszVal = big; EQ(wr_id(f.ps, 30, &v), S_OK);
        EQ(fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX), 1);
        if (f.ps) {
            PROPVARIANT r;
            EQ(rd_id(f.ps, 30, &r), S_OK); EQ(r.vt, VT_LPWSTR);
            CHECK(r.pwszVal && lstrlenW(r.pwszVal) == 599); U(PropVariantClear)(&r);
            VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 31, &vt, &x), S_OK); EQ(x, 77);
        }
    END_PS()
}

TEST(pvps_x_delete_all_then_rewrite) {
    WITH_PS("x_delall.stg", PROPSETFLAG_DEFAULT)
        PROPSPEC s[3]; for (int i = 0; i < 3; i++) { s[i].ulKind = PRSPEC_PROPID; s[i].propid = 40 + i; EQ(wr_i4(f.ps, 40 + i, i), S_OK); }
        EQ(CALL(f.ps, DeleteMultiple, 3, s), S_OK);
        PROPVARIANT out[3]; for (int i = 0; i < 3; i++) PropVariantInit(&out[i]);
        EQ(CALL(f.ps, ReadMultiple, 3, s, out), S_FALSE);
        for (int i = 0; i < 3; i++) EQ(out[i].vt, VT_EMPTY);
        EQ(wr_i4(f.ps, 41, 99), S_OK);
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 41, &vt, &x), S_OK); EQ(x, 99);
        EQ(rd_i4(f.ps, 40, &vt, &x), S_FALSE);
    END_PS()
}

TEST(pvps_x_stat_reports_flags_and_fmtid_after_reopen) {
    WITH_PS("x_stat_reopen.stg", PROPSETFLAG_ANSI)
        EQ(wr_i4(f.ps, 3, 1), S_OK);
        EQ(fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX), 1);
        if (f.ps) {
            STATPROPSETSTG st; memset(&st, 0, sizeof st);
            EQ(CALL(f.ps, Stat, &st), S_OK);
            CHECK(guid_eq(&st.fmtid, &G_ANY_GUID));
            t_note("Stat grfFlags=0x%lx after reopen of ANSI set", (unsigned long)st.grfFlags);
            PROPVARIANT r; EQ(rd_id(f.ps, PID_CODEPAGE, &r), S_OK); EQ(r.vt, VT_I2);
            CHECK(r.iVal != 1200);
        }
    END_PS()
}

TEST(pvps_x_fmtid_stream_name_prefix) {
    REQUIRE(U(FmtIdToPropStgName));
    WCHAR n[64];
    EQ(U(FmtIdToPropStgName)(&G_ANY_GUID, n), S_OK);
    EQ(n[0], 5);
    EQ(U(FmtIdToPropStgName)(&G_ANY_GUID, n), S_OK);
    WCHAR m[64]; EQ(U(FmtIdToPropStgName)(&G_ANY_GUID2, m), S_OK);
    CHECK(lstrcmpW(n, m) != 0);
}
