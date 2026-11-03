/* IPropertyStorage: ReadMultiple / WriteMultiple / DeleteMultiple, property names (dictionary),
 * special properties (codepage, locale, behavior), Enum -> IEnumSTATPROPSTG. */
#include "pv.h"

#define WITH_PS(leaf, flags) \
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN(); FX f; \
    if (fx_new(&f, leaf, &G_ANY_GUID, flags)) {
#define END_PS() } fx_close(&f); COM_END();

static const PROPVARIANT *pvi4(PROPVARIANT *v, LONG x) { PropVariantInit(v); v->vt = VT_I4; v->lVal = x; return v; }

/* ============================================================ argument handling */

TEST(pvps_props_zero_count_and_null_arrays) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_args.stg", PROPSETFLAG_ANSI)
        EQ(CALL(f.ps, WriteMultiple, 0, NULL, NULL, 0), S_OK);
        EQ(CALL(f.ps, WriteMultiple, 1, NULL, NULL, 0), E_INVALIDARG_);
        EQ(CALL(f.ps, ReadMultiple, 0, NULL, NULL), S_FALSE);
        EQ(CALL(f.ps, ReadMultiple, 1, NULL, NULL), E_INVALIDARG_);
        EQ(CALL(f.ps, DeleteMultiple, 0, NULL), S_OK);
        EQ(CALL(f.ps, DeleteMultiple, 1, NULL), E_INVALIDARG_);
        PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = 5; PROPVARIANT v; PropVariantInit(&v);
        EQ(CALL(f.ps, WriteMultiple, 1, &s, NULL, 0), E_INVALIDARG_);
        EQ(CALL(f.ps, ReadMultiple, 1, &s, NULL), E_INVALIDARG_);
        EQ(CALL(f.ps, WriteMultiple, 1, NULL, &v, 0), E_INVALIDARG_);
    END_PS()
}

TEST(pvps_props_names_null_arrays) {
    WITH_PS("props_names_args.stg", PROPSETFLAG_DEFAULT)
        PROPID id = 5; LPOLESTR nm = (LPOLESTR)L"x";
        EQ(CALL(f.ps, WritePropertyNames, 0, NULL, NULL), S_OK);
        EQ(CALL(f.ps, DeletePropertyNames, 0, NULL), S_OK);
        HRESULT hr = CALL(f.ps, WritePropertyNames, 1, NULL, &nm); CHECKF(FAILED(hr), "WritePropertyNames(NULL ids) hr=%08lx", hr);
        t_note("WritePropertyNames(1, NULL ids) -> 0x%08lx", hr);
        hr = CALL(f.ps, WritePropertyNames, 1, &id, NULL); CHECKF(FAILED(hr), "WritePropertyNames(NULL names) hr=%08lx", hr);
        t_note("WritePropertyNames(1, ids, NULL) -> 0x%08lx", hr);
        hr = CALL(f.ps, DeletePropertyNames, 1, NULL); CHECKF(FAILED(hr), "DeletePropertyNames(NULL) hr=%08lx", hr);
        t_note("DeletePropertyNames(1, NULL) -> 0x%08lx", hr);
        LPOLESTR out[1] = {0};
        hr = CALL(f.ps, ReadPropertyNames, 1, NULL, out); CHECKF(FAILED(hr), "ReadPropertyNames(NULL ids) hr=%08lx", hr);
        t_note("ReadPropertyNames(1, NULL ids) -> 0x%08lx", hr);
        hr = CALL(f.ps, ReadPropertyNames, 1, &id, NULL); CHECKF(FAILED(hr), "ReadPropertyNames(NULL out) hr=%08lx", hr);
        t_note("ReadPropertyNames(1, ids, NULL) -> 0x%08lx", hr);
    END_PS()
}

TEST(pvps_props_invalid_propspec_kind) {
    WITH_PS("props_badkind.stg", PROPSETFLAG_DEFAULT)
        PROPSPEC s; s.ulKind = 7; s.propid = 5;
        PROPVARIANT v; pvi4(&v, 1);
        HRESULT hr = CALL(f.ps, WriteMultiple, 1, &s, &v, 2);
        t_note("WriteMultiple(ulKind=7) -> 0x%08lx", hr);
        PROPVARIANT r; PropVariantInit(&r);
        hr = CALL(f.ps, ReadMultiple, 1, &s, &r);
        t_note("ReadMultiple(ulKind=7) -> 0x%08lx", hr);
    END_PS()
}

/* ============================================================ special / reserved ids */

TEST(pvps_props_write_dictionary_id_rejected) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_dict.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_id(f.ps, PID_DICTIONARY, &v), STG_E_INVALIDPARAMETER_);
        /* ... and by name with propidNameFirst inside the reserved range */
        PROPSPEC s; s.ulKind = PRSPEC_LPWSTR; s.lpwstr = (LPOLESTR)L"prop";
        EQ(CALL(f.ps, WriteMultiple, 1, &s, &v, PID_DICTIONARY), STG_E_INVALIDPARAMETER_);
        EQ(CALL(f.ps, WriteMultiple, 1, &s, &v, PID_CODEPAGE), STG_E_INVALIDPARAMETER_);
        /* nothing was created by the failed calls */
        PROPVARIANT r; EQ(rd_name(f.ps, L"prop", &r), S_FALSE);
    END_PS()
}

TEST(pvps_props_delete_dictionary_id_rejected) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+): "contrary to what the docs say, you can't delete the dictionary" */
    WITH_PS("props_deldict.stg", PROPSETFLAG_ANSI)
        PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = PID_DICTIONARY;
        EQ(CALL(f.ps, DeleteMultiple, 1, &s), STG_E_INVALIDPARAMETER_);
    END_PS()
}

TEST(pvps_props_write_behavior_id) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_behavior.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_id(f.ps, PID_BEHAVIOR, &v), STG_E_INVALIDPARAMETER_);       /* VT_I4 is not the type of the behavior property */
        PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = 1;
        HRESULT hr = wr_id(f.ps, PID_BEHAVIOR, &v);
        t_note("write PID_BEHAVIOR as VT_UI4 1 in a default set -> 0x%08lx", hr);
    END_PS()
}

TEST(pvps_props_behavior_property_in_case_sensitive_set) {
    WITH_PS("props_behavior_cs.stg", PROPSETFLAG_CASE_SENSITIVE)
        PROPVARIANT v; HRESULT hr = rd_id(f.ps, PID_BEHAVIOR, &v);
        t_note("PID_BEHAVIOR read in a case-sensitive set -> 0x%08lx vt=%d val=%lu", hr, v.vt, v.ulVal);
        U(PropVariantClear)(&v);
        PROPVARIANT w; PropVariantInit(&w);
        hr = rd_id(f.ps, 0, &w);
        t_note("PID_DICTIONARY read -> 0x%08lx vt=%d", hr, w.vt);
        U(PropVariantClear)(&w);
        CHECK(1);
    END_PS()
}

TEST(pvps_props_pid_illegal_is_ignored) {
    WITH_PS("props_illegal.stg", PROPSETFLAG_ANSI)
        PROPSPEC s[2]; PROPVARIANT v[2];
        s[0].ulKind = PRSPEC_PROPID; s[0].propid = PID_ILLEGAL; pvi4(&v[0], 99);
        s[1].ulKind = PRSPEC_PROPID; s[1].propid = 7;           pvi4(&v[1], 7);
        EQ(CALL(f.ps, WriteMultiple, 2, s, v, 2), S_OK);        /* documented: PID_ILLEGAL entries are ignored */
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 7, &vt, &x), S_OK); EQ(x, 7);
        PROPVARIANT r; HRESULT hr = rd_id(f.ps, PID_ILLEGAL, &r);
        t_note("read of PID_ILLEGAL -> 0x%08lx vt=%d", hr, r.vt);
        PROPSPEC d; d.ulKind = PRSPEC_PROPID; d.propid = PID_ILLEGAL;
        HRESULT dh = CALL(f.ps, DeleteMultiple, 1, &d);
        t_note("DeleteMultiple(PID_ILLEGAL) -> 0x%08lx", dh);
        EQ(rd_i4(f.ps, 7, &vt, &x), S_OK);
    END_PS()
}

TEST(pvps_props_readonly_id_range_write) {
    WITH_PS("props_ro_range.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        HRESULT hr = wr_id(f.ps, PID_MODIFY_TIME_, &v);
        t_note("write PID_MODIFY_TIME (0x80000001) -> 0x%08lx", hr);
        hr = wr_id(f.ps, 0x80000002u, &v);
        t_note("write PID_SECURITY (0x80000002) -> 0x%08lx", hr);
        hr = wr_id(f.ps, 0xC0000000u, &v);
        t_note("write id 0xC0000000 -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

/* ============================================================ basic read / write / delete */

TEST(pvps_props_write_read_by_id) {
    WITH_PS("props_rw_id.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_id(f.ps, PID_FIRST_USABLE, &v), S_OK);
        VARTYPE vt; LONG x;
        EQ(rd_i4(f.ps, PID_FIRST_USABLE, &vt, &x), S_OK);
        EQ(vt, VT_I4); EQ(x, 1);
    END_PS()
}

TEST(pvps_props_overwrite_changes_type) {
    WITH_PS("props_overwrite.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_id(f.ps, 5, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"now a string";
        EQ(wr_id(f.ps, 5, &v), S_OK);       /* "even when the old and new types for the property value are different" */
        PROPVARIANT r; EQ(rd_id(f.ps, 5, &r), S_OK);
        EQ(r.vt, VT_LPWSTR); if (r.vt == VT_LPWSTR) EXPECT_WSTR(r.pwszVal, L"now a string");
        U(PropVariantClear)(&r);
        PropVariantInit(&v); v.vt = VT_R8; v.dblVal = 2.5;
        EQ(wr_id(f.ps, 5, &v), S_OK);
        EQ(rd_id(f.ps, 5, &r), S_OK); EQ(r.vt, VT_R8); CHECK(r.dblVal == 2.5);
    END_PS()
}

TEST(pvps_props_missing_property_reads_empty_s_false) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_missing.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; memset(&v, 0x77, sizeof v); v.vt = VT_BSTR;
        PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = 0xdead;
        EQ(CALL(f.ps, ReadMultiple, 1, &s, &v), S_FALSE);
        EQ(v.vt, VT_EMPTY);
        /* by name too */
        EQ(rd_name(f.ps, L"non-existing", &v), S_FALSE); EQ(v.vt, VT_EMPTY);
    END_PS()
}

TEST(pvps_props_partial_read_is_s_ok_and_marks_missing_empty) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_partial.stg", PROPSETFLAG_ANSI)
        PROPVARIANT w; pvi4(&w, 1); EQ(wr_id(f.ps, PID_FIRST_USABLE, &w), S_OK);
        PROPSPEC s[2]; PROPVARIANT v[2];
        s[0].ulKind = PRSPEC_PROPID; s[0].propid = PID_FIRST_USABLE;
        s[1].ulKind = PRSPEC_PROPID; s[1].propid = 0xdead;
        v[0].vt = VT_BSTR; v[1].vt = VT_BSTR;
        EQ(CALL(f.ps, ReadMultiple, 2, s, v), S_OK);           /* at least one found */
        EQ(v[0].vt, VT_I4); EQ(v[0].lVal, 1); EQ(v[1].vt, VT_EMPTY);
        /* order of the array is the order of the request */
        s[0].propid = 0xdead; s[1].propid = PID_FIRST_USABLE;
        EQ(CALL(f.ps, ReadMultiple, 2, s, v), S_OK);
        EQ(v[0].vt, VT_EMPTY); EQ(v[1].vt, VT_I4);
        /* two missing */
        s[1].propid = 0xbeef;
        EQ(CALL(f.ps, ReadMultiple, 2, s, v), S_FALSE);
        EQ(v[0].vt, VT_EMPTY); EQ(v[1].vt, VT_EMPTY);
    END_PS()
}

TEST(pvps_props_read_same_id_twice_in_one_call) {
    WITH_PS("props_dupread.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT w; pvi4(&w, 31); wr_id(f.ps, 9, &w);
        PROPSPEC s[2]; PROPVARIANT v[2];
        s[0].ulKind = PRSPEC_PROPID; s[0].propid = 9; s[1] = s[0];
        EQ(CALL(f.ps, ReadMultiple, 2, s, v), S_OK);
        EQ(v[0].vt, VT_I4); EQ(v[1].vt, VT_I4); EQ(v[0].lVal, 31); EQ(v[1].lVal, 31);
    END_PS()
}

TEST(pvps_props_batch_write_read_mixed_kinds) {
    WITH_PS("props_batch.stg", PROPSETFLAG_DEFAULT)
        PROPSPEC s[4]; PROPVARIANT v[4];
        s[0].ulKind = PRSPEC_PROPID; s[0].propid = 10; pvi4(&v[0], 100);
        s[1].ulKind = PRSPEC_LPWSTR; s[1].lpwstr = (LPOLESTR)L"second"; PropVariantInit(&v[1]); v[1].vt = VT_R8; v[1].dblVal = 1.5;
        s[2].ulKind = PRSPEC_PROPID; s[2].propid = 11; PropVariantInit(&v[2]); v[2].vt = VT_LPWSTR; v[2].pwszVal = (LPWSTR)L"third";
        s[3].ulKind = PRSPEC_LPWSTR; s[3].lpwstr = (LPOLESTR)L"fourth"; PropVariantInit(&v[3]); v[3].vt = VT_BOOL; v[3].boolVal = VARIANT_TRUE;
        EQ(CALL(f.ps, WriteMultiple, 4, s, v, 0x100), S_OK);
        PROPVARIANT r[4]; EQ(CALL(f.ps, ReadMultiple, 4, s, r), S_OK);
        EQ(r[0].vt, VT_I4); EQ(r[0].lVal, 100);
        EQ(r[1].vt, VT_R8); CHECK(r[1].dblVal == 1.5);
        EQ(r[2].vt, VT_LPWSTR); if (r[2].vt == VT_LPWSTR) EXPECT_WSTR(r[2].pwszVal, L"third");
        EQ(r[3].vt, VT_BOOL); EQ(r[3].boolVal, VARIANT_TRUE);
        EQ(U(FreePropVariantArray)(4, r), S_OK);
    END_PS()
}

TEST(pvps_props_write_does_not_take_ownership) {
    WITH_PS("props_ownership.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = tm_strA("owned by caller");
        EQ(wr_id(f.ps, 3, &v), S_OK);
        EQ(U(PropVariantClear)(&v), S_OK);        /* releasing the source must not affect the stored copy */
        PROPVARIANT r; EQ(rd_id(f.ps, 3, &r), S_OK);
        EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "owned by caller");
        U(PropVariantClear)(&r);
        /* BSTR source: modifying the source afterwards does not change the property */
        BSTR b = U(SysAllocString)(L"abc");
        PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = b;
        EQ(wr_id(f.ps, 4, &v), S_OK);
        b[0] = L'Z';
        EQ(rd_id(f.ps, 4, &r), S_OK);
        EQ(r.vt, VT_BSTR); if (r.vt == VT_BSTR) EXPECT_WSTR(r.bstrVal, L"abc");
        U(PropVariantClear)(&r); U(SysFreeString)(b);
    END_PS()
}

TEST(pvps_props_read_returns_independent_memory) {
    WITH_PS("props_indep.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT w; PropVariantInit(&w); w.vt = VT_LPWSTR; w.pwszVal = (LPWSTR)L"shared?";
        EQ(wr_id(f.ps, 3, &w), S_OK);
        PROPVARIANT a, b; EQ(rd_id(f.ps, 3, &a), S_OK); EQ(rd_id(f.ps, 3, &b), S_OK);
        CHECK(a.pwszVal != b.pwszVal);
        a.pwszVal[0] = L'X';                      /* scribbling over one result must not change later reads */
        PROPVARIANT c; EQ(rd_id(f.ps, 3, &c), S_OK);
        EXPECT_WSTR(c.pwszVal, L"shared?");
        EXPECT_WSTR(b.pwszVal, L"shared?");
        U(PropVariantClear)(&a); U(PropVariantClear)(&b); U(PropVariantClear)(&c);
    END_PS()
}

TEST(pvps_props_delete_existing_and_missing) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("props_delete.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        wr_id(f.ps, PID_FIRST_USABLE, &v);
        PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = PID_FIRST_USABLE;
        EQ(CALL(f.ps, DeleteMultiple, 1, &s), S_OK);
        PROPVARIANT r; EQ(CALL(f.ps, ReadMultiple, 1, &s, &r), S_FALSE);
        HRESULT hr = CALL(f.ps, DeleteMultiple, 1, &s);
        t_note("delete of an already-deleted property -> 0x%08lx", hr);
        CHECK(hr == S_OK || hr == S_FALSE);
    END_PS()
}

TEST(pvps_props_delete_by_name) {
    WITH_PS("props_delname.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"victim", &v, 2), S_OK);
        EQ(wr_name(f.ps, L"survivor", &v, 2), S_OK);
        PROPSPEC s; s.ulKind = PRSPEC_LPWSTR; s.lpwstr = (LPOLESTR)L"victim";
        EQ(CALL(f.ps, DeleteMultiple, 1, &s), S_OK);
        PROPVARIANT r; EQ(rd_name(f.ps, L"victim", &r), S_FALSE);
        EQ(rd_name(f.ps, L"survivor", &r), S_OK); EQ(r.vt, VT_I4);
    END_PS()
}

TEST(pvps_props_delete_batch_then_enum_empty) {
    WITH_PS("props_delbatch.stg", PROPSETFLAG_DEFAULT)
        PROPSPEC s[10];
        for (int i = 0; i < 10; i++) { PROPVARIANT v; pvi4(&v, i); wr_id(f.ps, 20 + i, &v); s[i].ulKind = PRSPEC_PROPID; s[i].propid = 20 + i; }
        EQ(CALL(f.ps, DeleteMultiple, 10, s), S_OK);
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st; ULONG got = 0xff;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        if (e) { EQ(CALL(e, Next, 1, &st, &got), S_FALSE); EQ(got, 0); CALL(e, Release); }
    END_PS()
}

TEST(pvps_props_sparse_and_extreme_ids) {
    WITH_PS("props_ids.stg", PROPSETFLAG_DEFAULT)
        static const ULONG ids[] = {2, 3, 100, 0xFFF, 0x10000, 0x7FFFFFFE, 0x7FFFFFFF};
        for (int i = 0; i < N_ELEM(ids); i++) { PROPVARIANT v; pvi4(&v, (LONG)ids[i] ^ 0x55); HRESULT hr = wr_id(f.ps, ids[i], &v); CHECKF(hr == S_OK, "write id %lx hr=%08lx", ids[i], hr); }
        for (int i = 0; i < N_ELEM(ids); i++) { VARTYPE vt; LONG x; HRESULT hr = rd_i4(f.ps, ids[i], &vt, &x); CHECKF(hr == S_OK && vt == VT_I4 && x == (LONG)(ids[i] ^ 0x55), "id %lx hr=%08lx vt=%d", ids[i], hr, vt); }
    END_PS()
}

/* ============================================================ unsupported types */

TEST(pvps_props_write_invalid_vt_rejected) {
    WITH_PS("props_badvt.stg", PROPSETFLAG_DEFAULT)
        static const VARTYPE bad[] = {15, VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_CARRAY, VT_USERDEFINED, 32, 40, 63, 0x7f};
        for (int i = 0; i < N_ELEM(bad); i++) {
            PROPVARIANT v; memset(&v, 0, sizeof v); v.vt = bad[i];
            HRESULT hr = wr_id(f.ps, 6, &v);
            CHECKF(FAILED(hr), "vt %d accepted hr=%08lx", bad[i], hr);
            t_note("WriteMultiple(vt=%d) -> 0x%08lx", bad[i], hr);
            PROPVARIANT r; EQ(rd_id(f.ps, 6, &r), S_FALSE);        /* nothing was stored */
        }
    END_PS()
}

TEST(pvps_props_write_object_types_in_simple_set) {
    WITH_PS("props_objvt.stg", PROPSETFLAG_DEFAULT)
        TUnk u; tu_init(&u);
        PROPVARIANT v; PropVariantInit(&v);
        v.vt = VT_UNKNOWN; v.punkVal = (IUnknown *)&u;
        HRESULT hr = wr_id(f.ps, 6, &v);
        t_note("WriteMultiple(VT_UNKNOWN) -> 0x%08lx (ref now %ld)", hr, u.ref);
        v.vt = VT_DISPATCH; v.pdispVal = (IDispatch *)&u;
        hr = wr_id(f.ps, 7, &v);
        t_note("WriteMultiple(VT_DISPATCH) -> 0x%08lx", hr);
        v.vt = VT_STORAGE; v.pStorage = NULL;
        hr = wr_id(f.ps, 8, &v);
        t_note("WriteMultiple(VT_STORAGE NULL) in a simple set -> 0x%08lx", hr);
        v.vt = VT_STREAM; v.pStream = NULL;
        hr = wr_id(f.ps, 9, &v);
        t_note("WriteMultiple(VT_STREAM NULL) in a simple set -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

TEST(pvps_props_write_byref_array_types) {
    WITH_PS("props_byref.stg", PROPSETFLAG_DEFAULT)
        LONG x = 5; PROPVARIANT v; PropVariantInit(&v);
        v.vt = VT_BYREF | VT_I4; v.plVal = &x;
        HRESULT hr = wr_id(f.ps, 6, &v);
        t_note("WriteMultiple(VT_BYREF|VT_I4) -> 0x%08lx", hr);
        SAFEARRAYBOUND b = {2, 0};
        SAFEARRAY *sa = U(SafeArrayCreate)(VT_I4, 1, &b);
        v.vt = VT_ARRAY | VT_I4; v.parray = sa;
        hr = wr_id(f.ps, 7, &v);
        t_note("WriteMultiple(VT_ARRAY|VT_I4) -> 0x%08lx", hr);
        if (sa) U(SafeArrayDestroy)(sa);
        CHECK(1);
    END_PS()
}

/* ============================================================ property names (dictionary) */

TEST(pvps_names_write_by_name_allocates_ids_from_first) {
    WITH_PS("names_alloc.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        PROPID id1 = 0, id2 = 0, id3 = 0;
        EQ(wr_name(f.ps, L"alpha", &v, 0x100), S_OK);
        EQ(wr_name(f.ps, L"beta", &v, 0x100), S_OK);
        EQ(wr_name(f.ps, L"gamma", &v, 0x2000), S_OK);
        /* name_to_id scans ids 2..65; use the full dictionary through Enum instead */
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st; ULONG got;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        int n = 0;
        while (e && CALL(e, Next, 1, &st, &got) == S_OK) {
            n++;
            if (st.lpwstrName) {
                if (!lstrcmpW(st.lpwstrName, L"alpha")) id1 = st.propid;
                if (!lstrcmpW(st.lpwstrName, L"beta")) id2 = st.propid;
                if (!lstrcmpW(st.lpwstrName, L"gamma")) id3 = st.propid;
                U(CoTaskMemFree)(st.lpwstrName);
            }
        }
        if (e) CALL(e, Release);
        EQ(n, 3);
        CHECKF(id1 >= 0x100 && id2 >= 0x100, "ids %lx %lx below propidNameFirst", id1, id2);
        CHECKF(id3 >= 0x2000, "id %lx below 0x2000", id3);
        CHECK(id1 != id2); CHECK(id1 != id3);
        t_note("allocated ids: alpha=%lx beta=%lx gamma=%lx", id1, id2, id3);
    END_PS()
}

TEST(pvps_names_first_id_is_exactly_propid_name_first_when_free) {
    WITH_PS("names_exact.stg", PROPSETFLAG_ANSI)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"prop", &v, PID_FIRST_USABLE), S_OK);
        PROPID id = 0; HRESULT hr = name_to_id(f.ps, L"prop", &id);
        EQ(hr, S_OK);
        t_note("first name with propidNameFirst=2 got id %lu", id);
        CHECK(id >= PID_FIRST_USABLE);
    END_PS()
}

TEST(pvps_names_allocation_skips_ids_in_use) {
    WITH_PS("names_skip.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_id(f.ps, 2, &v), S_OK); EQ(wr_id(f.ps, 3, &v), S_OK);
        EQ(wr_name(f.ps, L"named", &v, 2), S_OK);
        PROPID id = 0; EQ(name_to_id(f.ps, L"named", &id), S_OK);
        CHECKF(id >= 4, "named property got in-use id %lu", id);
        VARTYPE vt; LONG x;
        EQ(rd_i4(f.ps, 2, &vt, &x), S_OK); EQ(rd_i4(f.ps, 3, &vt, &x), S_OK);
    END_PS()
}

TEST(pvps_names_rewrite_keeps_id_and_replaces_value) {
    WITH_PS("names_rewrite.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"stable", &v, 2), S_OK);
        PROPID a = 0, b = 0; EQ(name_to_id(f.ps, L"stable", &a), S_OK);
        pvi4(&v, 2);
        EQ(wr_name(f.ps, L"stable", &v, 0x500), S_OK);        /* propidNameFirst only matters for new names */
        EQ(name_to_id(f.ps, L"stable", &b), S_OK);
        EQ(b, a);
        PROPVARIANT r; EQ(rd_name(f.ps, L"stable", &r), S_OK); EQ(r.lVal, 2);
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, a, &vt, &x), S_OK); EQ(x, 2);
    END_PS()
}

TEST(pvps_names_case_insensitive_by_default) {
    WITH_PS("names_ci.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"MixedCase", &v, 2), S_OK);
        PROPVARIANT r;
        EQ(rd_name(f.ps, L"mixedcase", &r), S_OK); EQ(r.vt, VT_I4);
        EQ(rd_name(f.ps, L"MIXEDCASE", &r), S_OK); EQ(r.vt, VT_I4);
        pvi4(&v, 2);
        EQ(wr_name(f.ps, L"MIXEDCASE", &v, 2), S_OK);      /* same property */
        EQ(rd_name(f.ps, L"MixedCase", &r), S_OK); EQ(r.lVal, 2);
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st; ULONG got; int n = 0;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        while (e && CALL(e, Next, 1, &st, &got) == S_OK) { n++; if (st.lpwstrName) { t_note("stored name: %ls", st.lpwstrName); U(CoTaskMemFree)(st.lpwstrName); } }
        if (e) CALL(e, Release);
        EQ(n, 1);
    END_PS()
}

TEST(pvps_names_case_sensitive_set_distinguishes_case) {
    WITH_PS("names_cs.stg", PROPSETFLAG_CASE_SENSITIVE)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"Name", &v, 2), S_OK);
        pvi4(&v, 2);
        EQ(wr_name(f.ps, L"name", &v, 2), S_OK);
        PROPVARIANT r;
        EQ(rd_name(f.ps, L"Name", &r), S_OK); EQ(r.lVal, 1);
        EQ(rd_name(f.ps, L"name", &r), S_OK); EQ(r.lVal, 2);
        EQ(rd_name(f.ps, L"NAME", &r), S_FALSE);
    END_PS()
}

TEST(pvps_names_read_missing_name) {
    WITH_PS("names_missing.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT r; memset(&r, 0x33, sizeof r);
        EQ(rd_name(f.ps, L"nothing", &r), S_FALSE); EQ(r.vt, VT_EMPTY);
    END_PS()
}

TEST(pvps_names_write_property_names_basic) {
    WITH_PS("names_wpn.stg", PROPSETFLAG_DEFAULT)
        /* "It is explicitly valid to define a name for a property ID that is not currently present" */
        PROPID id = 77; LPOLESTR nm = (LPOLESTR)L"seventy-seven";
        EQ(CALL(f.ps, WritePropertyNames, 1, &id, &nm), S_OK);
        LPOLESTR out[1] = {0};
        EQ(CALL(f.ps, ReadPropertyNames, 1, &id, out), S_OK);
        CHECK(out[0] != NULL); if (out[0]) { EXPECT_WSTR(out[0], L"seventy-seven"); U(CoTaskMemFree)(out[0]); }
        PROPVARIANT r; EQ(rd_id(f.ps, 77, &r), S_FALSE);                  /* the property itself still does not exist */
        PROPVARIANT v; pvi4(&v, 5);
        EQ(wr_name(f.ps, L"SEVENTY-SEVEN", &v, 2), S_OK);                 /* the name is now bound to 77 */
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 77, &vt, &x), S_OK); EQ(x, 5);
    END_PS()
}

TEST(pvps_names_read_property_names_unnamed_is_null_s_false) {
    WITH_PS("names_rpn.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1); wr_id(f.ps, 9, &v);
        PROPID ids[2] = {9, 10}; LPOLESTR out[2] = {(LPOLESTR)1, (LPOLESTR)1};
        HRESULT hr = CALL(f.ps, ReadPropertyNames, 2, ids, out);
        EQ(hr, S_FALSE);                       /* no names found */
        CHECK(out[0] == NULL); CHECK(out[1] == NULL);
        PROPID id = 9; LPOLESTR nm = (LPOLESTR)L"nine";
        EQ(CALL(f.ps, WritePropertyNames, 1, &id, &nm), S_OK);
        out[0] = out[1] = (LPOLESTR)1;
        hr = CALL(f.ps, ReadPropertyNames, 2, ids, out);
        EQ(hr, S_OK);                          /* at least one found */
        CHECK(out[0] != NULL && out[1] == NULL);
        if (out[0]) { EXPECT_WSTR(out[0], L"nine"); U(CoTaskMemFree)(out[0]); }
    END_PS()
}

TEST(pvps_names_name_is_unique_remap) {
    WITH_PS("names_remap.stg", PROPSETFLAG_DEFAULT)
        PROPID a = 20, b = 21; LPOLESTR nm = (LPOLESTR)L"shared";
        EQ(CALL(f.ps, WritePropertyNames, 1, &a, &nm), S_OK);
        EQ(CALL(f.ps, WritePropertyNames, 1, &b, &nm), S_OK);   /* "the original mapping is deleted" */
        PROPID ids[2] = {20, 21}; LPOLESTR out[2] = {0};
        HRESULT hr = CALL(f.ps, ReadPropertyNames, 2, ids, out);
        EQ(hr, S_OK);
        CHECK(out[0] == NULL); CHECK(out[1] != NULL);
        if (out[1]) { EXPECT_WSTR(out[1], L"shared"); U(CoTaskMemFree)(out[1]); }
        if (out[0]) U(CoTaskMemFree)(out[0]);
        /* case-insensitive match counts as the same name */
        PROPID c = 22; LPOLESTR nm2 = (LPOLESTR)L"SHARED";
        EQ(CALL(f.ps, WritePropertyNames, 1, &c, &nm2), S_OK);
        PROPID ids2[2] = {21, 22}; LPOLESTR o2[2] = {0};
        hr = CALL(f.ps, ReadPropertyNames, 2, ids2, o2);
        EQ(hr, S_OK);
        CHECK(o2[0] == NULL); CHECK(o2[1] != NULL);
        if (o2[1]) { EXPECT_WSTR(o2[1], L"SHARED"); U(CoTaskMemFree)(o2[1]); }     /* case is preserved */
        if (o2[0]) U(CoTaskMemFree)(o2[0]);
    END_PS()
}

TEST(pvps_names_rename_property_id) {
    WITH_PS("names_rename.stg", PROPSETFLAG_DEFAULT)
        PROPID a = 30; LPOLESTR n1 = (LPOLESTR)L"first-name", n2 = (LPOLESTR)L"second-name";
        EQ(CALL(f.ps, WritePropertyNames, 1, &a, &n1), S_OK);
        EQ(CALL(f.ps, WritePropertyNames, 1, &a, &n2), S_OK);
        LPOLESTR out[1] = {0};
        EQ(CALL(f.ps, ReadPropertyNames, 1, &a, out), S_OK);
        if (out[0]) { EXPECT_WSTR(out[0], L"second-name"); U(CoTaskMemFree)(out[0]); } else CHECK(0);
        PROPVARIANT v; EQ(rd_name(f.ps, L"first-name", &v), S_FALSE);
        PROPVARIANT w; pvi4(&w, 3); wr_id(f.ps, 30, &w);
        EQ(rd_name(f.ps, L"second-name", &v), S_OK); EQ(v.lVal, 3);
    END_PS()
}

TEST(pvps_names_delete_property_names) {
    WITH_PS("names_dpn.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"kept", &v, 2), S_OK);
        EQ(wr_name(f.ps, L"dropped", &v, 2), S_OK);
        PROPID kid = 0, did = 0;
        EQ(name_to_id(f.ps, L"kept", &kid), S_OK); EQ(name_to_id(f.ps, L"dropped", &did), S_OK);
        EQ(CALL(f.ps, DeletePropertyNames, 1, &did), S_OK);
        PROPVARIANT r;
        EQ(rd_name(f.ps, L"dropped", &r), S_FALSE);                    /* the name is gone ... */
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, did, &vt, &x), S_OK);       /* ... the property is not */
        EQ(rd_name(f.ps, L"kept", &r), S_OK);
        /* silently ignore ids without a name / nonexistent ids (documented) */
        PROPID none[2] = {0x3333, 0x4444};
        EQ(CALL(f.ps, DeletePropertyNames, 2, none), S_OK);
    END_PS()
}

TEST(pvps_names_delete_property_names_zero_clears_all) {
    WITH_PS("names_dpn0.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        wr_name(f.ps, L"one", &v, 2); wr_name(f.ps, L"two", &v, 2); wr_name(f.ps, L"three", &v, 2);
        PROPID zero = 0;
        EQ(CALL(f.ps, DeletePropertyNames, 1, &zero), S_OK);       /* documented: deleting id 0 deletes every name */
        PROPVARIANT r;
        EQ(rd_name(f.ps, L"one", &r), S_FALSE); EQ(rd_name(f.ps, L"two", &r), S_FALSE); EQ(rd_name(f.ps, L"three", &r), S_FALSE);
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st; ULONG got; int n = 0, named = 0;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        while (e && CALL(e, Next, 1, &st, &got) == S_OK) { n++; if (st.lpwstrName) { named++; U(CoTaskMemFree)(st.lpwstrName); } }
        if (e) CALL(e, Release);
        EQ(n, 3); EQ(named, 0);                                    /* the values survive */
    END_PS()
}

TEST(pvps_names_pid_illegal_ignored_in_write_property_names) {
    WITH_PS("names_illegal.stg", PROPSETFLAG_DEFAULT)
        PROPID ids[2] = {PID_ILLEGAL, 40}; LPOLESTR nm[2] = {(LPOLESTR)L"ignored", (LPOLESTR)L"used"};
        EQ(CALL(f.ps, WritePropertyNames, 2, ids, nm), S_OK);
        PROPVARIANT r;
        EQ(rd_name(f.ps, L"ignored", &r), S_FALSE);
        PROPVARIANT w; pvi4(&w, 1); EQ(wr_name(f.ps, L"used", &w, 2), S_OK);
        VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 40, &vt, &x), S_OK);
    END_PS()
}

TEST(pvps_names_write_property_names_reserved_ids) {
    WITH_PS("names_resid.stg", PROPSETFLAG_DEFAULT)
        LPOLESTR nm = (LPOLESTR)L"x";
        PROPID ids[] = {PID_DICTIONARY, PID_CODEPAGE, PID_LOCALE_, PID_BEHAVIOR};
        for (int i = 0; i < 4; i++) {
            HRESULT hr = CALL(f.ps, WritePropertyNames, 1, &ids[i], &nm);
            t_note("WritePropertyNames(id=0x%lx) -> 0x%08lx", ids[i], hr);
        }
        CHECK(1);
    END_PS()
}

TEST(pvps_names_length_limits) {
    WITH_PS("names_len.stg", PROPSETFLAG_DEFAULT)
        WCHAR n128[130], n129[131];
        for (int i = 0; i < 128; i++) n128[i] = L'a' + (i % 26);
        n128[128] = 0;
        memcpy(n129, n128, sizeof n128); n129[128] = L'z'; n129[129] = 0;
        PROPVARIANT v; pvi4(&v, 1);
        HRESULT hr = wr_name(f.ps, n128, &v, 2);
        t_note("128-char name -> 0x%08lx", hr);
        PROPVARIANT r; if (hr == S_OK) { EQ(rd_name(f.ps, n128, &r), S_OK); }
        hr = wr_name(f.ps, n129, &v, 2);
        t_note("129-char name -> 0x%08lx", hr);
        hr = wr_name(f.ps, L"", &v, 2);
        t_note("empty name -> 0x%08lx", hr);
        WCHAR res[4] = {1, 'a', 0, 0};
        hr = wr_name(f.ps, res, &v, 2);
        t_note("name starting with U+0001 -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

TEST(pvps_names_unicode_names_roundtrip) {
    WITH_PS("names_uni.stg", PROPSETFLAG_DEFAULT)
        static const WCHAR nm[] = {0x00e9, 0x00fc, 0x4e2d, 0x6587, ' ', 'x', 0};
        PROPVARIANT v; pvi4(&v, 12);
        EQ(wr_name(f.ps, nm, &v, 2), S_OK);
        PROPVARIANT r; EQ(rd_name(f.ps, nm, &r), S_OK); EQ(r.lVal, 12);
        PROPID id; EQ(name_to_id(f.ps, nm, &id), S_OK);
    END_PS()
}

TEST(pvps_names_property_with_name_keeps_name_on_id_write) {
    WITH_PS("names_idwrite.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1);
        EQ(wr_name(f.ps, L"labelled", &v, 2), S_OK);
        PROPID id = 0; EQ(name_to_id(f.ps, L"labelled", &id), S_OK);
        pvi4(&v, 2);
        EQ(wr_id(f.ps, id, &v), S_OK);
        PROPVARIANT r; EQ(rd_name(f.ps, L"labelled", &r), S_OK); EQ(r.lVal, 2);
    END_PS()
}

TEST(pvps_names_many_names) {
    WITH_PS("names_many.stg", PROPSETFLAG_DEFAULT)
        for (int i = 0; i < 150; i++) {
            WCHAR nm[32]; wsprintfW(nm, L"name_%03d", i);
            PROPVARIANT v; pvi4(&v, i);
            HRESULT hr = wr_name(f.ps, nm, &v, 2);
            if (hr != S_OK) { CHECKF(0, "name %d hr=%08lx", i, hr); break; }
        }
        int bad = 0;
        for (int i = 0; i < 150; i++) {
            WCHAR nm[32]; wsprintfW(nm, L"NAME_%03d", i);
            PROPVARIANT r; if (rd_name(f.ps, nm, &r) != S_OK || r.vt != VT_I4 || r.lVal != i) bad++;
        }
        CHECKF(bad == 0, "%d of 150 names did not read back", bad);
    END_PS()
}

/* ============================================================ codepage / locale */

TEST(pvps_special_codepage_defaults) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "special_cp_default.stg", &G_FMTID_SI, PROPSETFLAG_DEFAULT)) {
        PROPVARIANT v; EQ(rd_id(f.ps, PID_CODEPAGE, &v), S_OK);
        EQ(v.vt, VT_I2); EQ(v.iVal, 1200);                 /* Unicode unless PROPSETFLAG_ANSI */
        fx_close(&f);
    }
    if (fx_new(&f, "special_cp_ansi.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        PROPVARIANT v; EQ(rd_id(f.ps, PID_CODEPAGE, &v), S_OK);
        EQ(v.vt, VT_I2);
        EQ((unsigned short)v.iVal, (unsigned short)GetACP());   /* "set to the current system default" */
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_special_codepage_change_on_empty_set) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("special_cp_change.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I2; v.iVal = 1252;
        EQ(wr_id(f.ps, PID_CODEPAGE, &v), S_OK);
        PROPVARIANT r; EQ(rd_id(f.ps, PID_CODEPAGE, &r), S_OK); EQ(r.vt, VT_I2); EQ(r.iVal, 1252);
        v.iVal = 1200;
        EQ(wr_id(f.ps, PID_CODEPAGE, &v), S_OK);
        EQ(rd_id(f.ps, PID_CODEPAGE, &r), S_OK); EQ(r.vt, VT_I2); EQ(r.iVal, 1200);
        /* once a real property exists the code page is frozen */
        EQ(wr_i4(f.ps, PID_FIRST_USABLE, 1), S_OK);
        EQ(wr_id(f.ps, PID_CODEPAGE, &v), STG_E_INVALIDPARAMETER_);
        v.iVal = 1252;
        EQ(wr_id(f.ps, PID_CODEPAGE, &v), STG_E_INVALIDPARAMETER_);
    END_PS()
}

TEST(pvps_special_codepage_frozen_by_name_only_set) {
    WITH_PS("special_cp_names.stg", PROPSETFLAG_DEFAULT)
        /* documented: a set is "non-empty" also when it has names in its dictionary */
        PROPID id = 30; LPOLESTR nm = (LPOLESTR)L"label";
        EQ(CALL(f.ps, WritePropertyNames, 1, &id, &nm), S_OK);
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I2; v.iVal = 1252;
        HRESULT hr = wr_id(f.ps, PID_CODEPAGE, &v);
        t_note("codepage change on a set that only has a dictionary entry -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

TEST(pvps_special_codepage_wrong_type_and_unknown_page) {
    WITH_PS("special_cp_type.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1252);
        HRESULT hr = wr_id(f.ps, PID_CODEPAGE, &v);
        t_note("codepage as VT_I4 -> 0x%08lx", hr);
        PropVariantInit(&v); v.vt = VT_UI2; v.uiVal = 1252;
        hr = wr_id(f.ps, PID_CODEPAGE, &v);
        t_note("codepage as VT_UI2 -> 0x%08lx", hr);
        PropVariantInit(&v); v.vt = VT_I2; v.iVal = 31337;
        hr = wr_id(f.ps, PID_CODEPAGE, &v);
        t_note("codepage 31337 -> 0x%08lx", hr);
        PropVariantInit(&v); v.vt = VT_I2; v.iVal = 65001 & 0x7fff;
        hr = wr_id(f.ps, PID_CODEPAGE, &v);
        t_note("codepage 0x7e... -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

TEST(pvps_special_codepage_not_deletable_when_default) {
    WITH_PS("special_cp_del.stg", PROPSETFLAG_DEFAULT)
        PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = PID_CODEPAGE;
        HRESULT hr = CALL(f.ps, DeleteMultiple, 1, &s);
        t_note("DeleteMultiple(PID_CODEPAGE) -> 0x%08lx", hr);
        PROPVARIANT r; hr = rd_id(f.ps, PID_CODEPAGE, &r);
        t_note("codepage after delete: hr=0x%08lx vt=%d val=%d", hr, r.vt, r.iVal);
        CHECK(1);
    END_PS()
}

TEST(pvps_special_locale_property) {
    WITH_PS("special_locale.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; HRESULT hr = rd_id(f.ps, PID_LOCALE_, &v);
        t_note("PID_LOCALE read -> 0x%08lx vt=%d value=0x%lx (user default LCID 0x%lx)", hr, v.vt, v.ulVal, GetUserDefaultLCID());
        if (hr == S_OK && v.vt != VT_UI4) t_note("  unexpected locale vt %d", v.vt);
        U(PropVariantClear)(&v);
        PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = 0x0409;
        hr = wr_id(f.ps, PID_LOCALE_, &v);
        t_note("write PID_LOCALE on an empty set -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

TEST(pvps_special_modify_time_property) {
    WITH_PS("special_mtime.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; HRESULT hr = rd_id(f.ps, PID_MODIFY_TIME_, &v);
        t_note("PID_MODIFY_TIME read -> 0x%08lx vt=%d", hr, v.vt);
        U(PropVariantClear)(&v);
        CHECK(1);
    END_PS()
}

/* ============================================================ ANSI vs Unicode string storage */

TEST(pvps_strings_lpwstr_roundtrip_in_ansi_and_unicode_sets) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    for (int pass = 0; pass < 2; pass++) {
        FX f;
        if (fx_new(&f, "strings_w.stg", &G_ANY_GUID, pass ? PROPSETFLAG_ANSI : PROPSETFLAG_DEFAULT)) {
            PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"wide text";
            EQ(wr_id(f.ps, 5, &v), S_OK);
            PROPVARIANT r; EQ(rd_id(f.ps, 5, &r), S_OK);
            EQ(r.vt, VT_LPWSTR); if (r.vt == VT_LPWSTR) EXPECT_WSTR(r.pwszVal, L"wide text");
            U(PropVariantClear)(&r);
            WCHAR uni[] = {'U', 0x00e9, 0x4e2d, 0x20ac, 0};
            v.pwszVal = uni;
            EQ(wr_id(f.ps, 6, &v), S_OK);
            EQ(rd_id(f.ps, 6, &r), S_OK);
            EQ(r.vt, VT_LPWSTR); if (r.vt == VT_LPWSTR) EXPECT_WSTR(r.pwszVal, uni);     /* LPWSTR is always UTF-16 on disk */
            U(PropVariantClear)(&r);
            fx_close(&f);
        }
    }
    COM_END();
}

TEST(pvps_strings_lpstr_in_unicode_set) {
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WITH_PS("strings_a_in_w.stg", PROPSETFLAG_DEFAULT)
        char aval[] = "hi";
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = aval;
        EQ(wr_id(f.ps, PID_FIRST_USABLE, &v), S_OK);
        PROPVARIANT r; EQ(rd_id(f.ps, PID_FIRST_USABLE, &r), S_OK);
        EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "hi");
        U(PropVariantClear)(&r);
        if (GetACP() == 1252) {
            /* a wide string passed as VT_LPSTR is interpreted with the system code page, not the set's */
            static const WCHAR wval[] = {'h', 'i', 0};
            v.pszVal = (LPSTR)wval;
            EQ(wr_id(f.ps, PID_FIRST_USABLE, &v), S_OK);
            EQ(rd_id(f.ps, PID_FIRST_USABLE, &r), S_OK);
            EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "h");
            U(PropVariantClear)(&r);
        } else t_note("ACP %u: skipping the wide-string-as-LPSTR probe", GetACP());
    END_PS()
}

TEST(pvps_strings_lpstr_high_chars_in_ansi_set) {
    WITH_PS("strings_a_high.stg", PROPSETFLAG_ANSI)
        char aval[] = "caf\xe9 \x80";
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = aval;
        EQ(wr_id(f.ps, 5, &v), S_OK);
        PROPVARIANT r; EQ(rd_id(f.ps, 5, &r), S_OK);
        EQ(r.vt, VT_LPSTR);
        if (r.vt == VT_LPSTR) { if (GetACP() == 1252) EXPECT_STR(r.pszVal, aval); else t_note("ACP %u read back \"%s\"", GetACP(), r.pszVal); }
        U(PropVariantClear)(&r);
    END_PS()
}

TEST(pvps_strings_bstr_roundtrip_and_empty) {
    REQUIRE(U(SysAllocString));
    WITH_PS("strings_bstr.stg", PROPSETFLAG_DEFAULT)
        BSTR b = U(SysAllocString)(L"bstr value");
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = b;
        EQ(wr_id(f.ps, 5, &v), S_OK);
        PROPVARIANT r; EQ(rd_id(f.ps, 5, &r), S_OK);
        EQ(r.vt, VT_BSTR); if (r.vt == VT_BSTR) EXPECT_WSTR(r.bstrVal, L"bstr value");
        U(PropVariantClear)(&r); U(SysFreeString)(b);
        if (t_is_wine()) { t_skip("wine aborts on NULL string pointers"); return; }
        v.bstrVal = NULL;                           /* a NULL BSTR is the empty string */
        HRESULT hr = wr_id(f.ps, 6, &v);
        t_note("write of a NULL BSTR -> 0x%08lx", hr);
        if (hr == S_OK) { EQ(rd_id(f.ps, 6, &r), S_OK); t_note("  reads back vt=%d ptr=%p", r.vt, (void *)r.bstrVal); U(PropVariantClear)(&r); }
        CHECK(1);
    END_PS()
}

TEST(pvps_strings_empty_lpstr_lpwstr) {
    WITH_PS("strings_empty.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"";
        EQ(wr_id(f.ps, 5, &v), S_OK);
        PROPVARIANT r; EQ(rd_id(f.ps, 5, &r), S_OK);
        EQ(r.vt, VT_LPSTR); CHECK(r.pszVal != NULL && r.pszVal[0] == 0); U(PropVariantClear)(&r);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"";
        EQ(wr_id(f.ps, 6, &v), S_OK);
        EQ(rd_id(f.ps, 6, &r), S_OK);
        EQ(r.vt, VT_LPWSTR); CHECK(r.pwszVal != NULL && r.pwszVal[0] == 0); U(PropVariantClear)(&r);
        /* NULL pointers (wine asserts) */
        if (t_is_wine()) { t_skip("wine aborts on NULL string pointers"); return; }
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = NULL;
        HRESULT hr = wr_id(f.ps, 7, &v);
        t_note("write of VT_LPSTR NULL -> 0x%08lx", hr);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = NULL;
        hr = wr_id(f.ps, 8, &v);
        t_note("write of VT_LPWSTR NULL -> 0x%08lx", hr);
        CHECK(1);
    END_PS()
}

/* ============================================================ Enum (IEnumSTATPROPSTG) */

static int enum_props(IPropertyStorage *ps, STATPROPSTG *out, int cap) {
    IEnumSTATPROPSTG *e = NULL; int n = 0;
    if (CALL(ps, Enum, &e) != S_OK || !e) return -1;
    for (;;) {
        STATPROPSTG st; ULONG got = 0;
        memset(&st, 0, sizeof st);
        if (CALL(e, Next, 1, &st, &got) != S_OK || got != 1) break;
        if (n < cap) out[n] = st; else if (st.lpwstrName) U(CoTaskMemFree)(st.lpwstrName);
        n++;
    }
    CALL(e, Release);
    return n;
}
static void free_stats(STATPROPSTG *s, int n) { for (int i = 0; i < n; i++) if (s[i].lpwstrName) U(CoTaskMemFree)(s[i].lpwstrName); }

TEST(pvps_enum_props_excludes_special_properties) {
    WITH_PS("enum_special.stg", PROPSETFLAG_ANSI)
        STATPROPSTG st[8];
        EQ(enum_props(f.ps, st, 8), 0);                  /* codepage / locale / dictionary are not enumerated */
        wr_i4(f.ps, 0x10, 1);
        int n = enum_props(f.ps, st, 8);
        EQ(n, 1);
        if (n == 1) { EQ(st[0].propid, 0x10); EQ(st[0].vt, VT_I4); CHECK(st[0].lpwstrName == NULL); }
        free_stats(st, n > 8 ? 8 : n);
    END_PS()
}

TEST(pvps_enum_props_reports_names_and_types) {
    WITH_PS("enum_types.stg", PROPSETFLAG_DEFAULT)
        PROPVARIANT v; pvi4(&v, 1); wr_id(f.ps, 5, &v);
        PropVariantInit(&v); v.vt = VT_R8; v.dblVal = 1.0; wr_name(f.ps, L"floaty", &v, 0x40);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"s"; wr_id(f.ps, 7, &v);
        STATPROPSTG st[8]; int n = enum_props(f.ps, st, 8);
        EQ(n, 3);
        int s5 = 0, sf = 0, s7 = 0;
        for (int i = 0; i < n && i < 8; i++) {
            if (st[i].propid == 5) { s5++; EQ(st[i].vt, VT_I4); CHECK(st[i].lpwstrName == NULL); }
            else if (st[i].propid == 7) { s7++; EQ(st[i].vt, VT_LPWSTR); CHECK(st[i].lpwstrName == NULL); }
            else if (st[i].lpwstrName && !lstrcmpW(st[i].lpwstrName, L"floaty")) { sf++; EQ(st[i].vt, VT_R8); CHECK(st[i].propid >= 0x40); }
        }
        EQ(s5, 1); EQ(s7, 1); EQ(sf, 1);
        free_stats(st, n > 8 ? 8 : n);
    END_PS()
}

TEST(pvps_enum_props_is_a_snapshot_or_live_observed) {
    WITH_PS("enum_snap.stg", PROPSETFLAG_DEFAULT)
        wr_i4(f.ps, 5, 1);
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st; ULONG got = 0;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        wr_i4(f.ps, 6, 2);                       /* added after the enumerator was created */
        int n = 0;
        while (e && CALL(e, Next, 1, &st, &got) == S_OK) n++;
        t_note("enumerator created before a write returned %d properties (snapshot=1, live=2)", n);
        CHECK(n == 1 || n == 2);
        if (e) CALL(e, Release);
    END_PS()
}

TEST(pvps_enum_props_skip_reset_next) {
    WITH_PS("enum_skip.stg", PROPSETFLAG_DEFAULT)
        for (int i = 0; i < 5; i++) wr_i4(f.ps, 10 + i, i);
        IEnumSTATPROPSTG *e = NULL; STATPROPSTG st[8]; ULONG got = 0;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        if (e) {
            EQ(CALL(e, Skip, 2), S_OK);
            EQ(CALL(e, Next, 8, st, &got), S_FALSE); EQ(got, 3);          /* fewer than asked: S_FALSE */
            free_stats(st, got > 8 ? 8 : got);
            EQ(CALL(e, Next, 1, st, &got), S_FALSE); EQ(got, 0);
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Next, 5, st, &got), S_OK); EQ(got, 5);             /* exactly as many as asked: S_OK */
            free_stats(st, got > 8 ? 8 : got);
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Skip, 6), S_FALSE);
            IEnumSTATPROPSTG *c = NULL;
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Skip, 1), S_OK);
            HRESULT hr = CALL(e, Clone, &c);
            t_note("Clone -> 0x%08lx", hr);
            if (hr == S_OK && c) {
                EQ(CALL(c, Next, 8, st, &got), S_FALSE);
                t_note("clone continues from the current position: %lu left", got);
                free_stats(st, got > 8 ? 8 : got);
                CALL(c, Release);
            }
            CALL(e, Release);
        }
    END_PS()
}

TEST(pvps_enum_props_order_is_stable_and_complete) {
    WITH_PS("enum_order.stg", PROPSETFLAG_DEFAULT)
        static STATPROPSTG st[300];
        for (int i = 0; i < 200; i++) wr_i4(f.ps, 2 + ((i * 37) % 997), i);
        int n = enum_props(f.ps, st, 300);
        int distinct = 0; { static ULONG seen[1000]; memset(seen, 0, sizeof seen);
            for (int i = 0; i < n && i < 300; i++) if (st[i].propid < 1000 && !seen[st[i].propid]++) distinct++; }
        EQ(n, 200); EQ(distinct, 200);
        free_stats(st, n > 300 ? 300 : n);
    END_PS()
}

TEST(pvps_enum_props_null_argument) {
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    WITH_PS("enum_null.stg", PROPSETFLAG_DEFAULT)
        HRESULT hr = CALL(f.ps, Enum, NULL);
        CHECKF(FAILED(hr), "Enum(NULL) hr=%08lx", hr);
        t_note("IPropertyStorage::Enum(NULL) -> 0x%08lx", hr);
    END_PS()
}

TEST(pvps_enum_props_released_storage_still_usable) {
    WITH_PS("enum_after.stg", PROPSETFLAG_DEFAULT)
        wr_i4(f.ps, 5, 1);
        IEnumSTATPROPSTG *e = NULL; EQ(CALL(f.ps, Enum, &e), S_OK);
        ULONG a = e ? CALL(e, AddRef) : 0, r = e ? CALL(e, Release) : 0; EQ(r, a - 1);
        if (e) CALL(e, Release);
        EQ(wr_i4(f.ps, 6, 2), S_OK);
    END_PS()
}
