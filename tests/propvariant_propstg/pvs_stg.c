/* Property set storage level: StgCreatePropSetStg / StgCreatePropStg / StgOpenPropStg,
 * IPropertySetStorage (Create/Open/Delete/Enum), IPropertyStorage object-level methods
 * (Stat, SetClass, SetTimes, Commit, Revert, Enum) and the streams they create. */
#include "pv.h"

static int stream_exists(IStorage *stg, const WCHAR *name) {
    IStream *s = NULL;
    HRESULT hr = CALL(stg, OpenStream, name, NULL, STGM_R_EX, 0, &s);
    if (s) CALL(s, Release);
    return hr == S_OK;
}
static int storage_exists(IStorage *stg, const WCHAR *name) {
    IStorage *s = NULL;
    HRESULT hr = CALL(stg, OpenStorage, name, NULL, STGM_R_EX, NULL, 0, &s);
    if (s) CALL(s, Release);
    return hr == S_OK;
}

/* ============================================================ StgCreatePropSetStg */

TEST(pvps_pss_create_propsetstg_ok_and_qi) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_qi.stg")) {
        CHECK(f.pss != NULL);
        IUnknown *u = NULL; void *p = NULL;
        EQ(CALL(f.pss, QueryInterface, &G_IID_IUnknown, (void **)&u), S_OK);
        if (u) u->lpVtbl->Release(u);
        EQ(CALL(f.pss, QueryInterface, &G_IID_IPropSetStg, &p), S_OK);
        if (p) CALL((IPropertySetStorage *)p, Release);
        p = (void *)1;
        HRESULT hr = CALL(f.pss, QueryInterface, &G_IID_IPropStg, &p);
        EQ(hr, E_NOINTERFACE); CHECK(p == NULL);
        ULONG a = CALL(f.pss, AddRef), r = CALL(f.pss, Release);
        EQ(r, a - 1);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_propsetstg_bad_arguments) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("not run under wine"); return; }
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_badarg.stg")) {
        IPropertySetStorage *p = (IPropertySetStorage *)1;
        HRESULT hr = U(StgCreatePropSetStg)(NULL, 0, &p);
        CHECKF(FAILED(hr), "NULL storage accepted hr=%08lx", hr);
        t_note("StgCreatePropSetStg(NULL storage) = 0x%08lx", hr);
        hr = U(StgCreatePropSetStg)(f.stg, 0, NULL);
        CHECKF(FAILED(hr), "NULL out pointer accepted hr=%08lx", hr);
        t_note("StgCreatePropSetStg(NULL out) = 0x%08lx", hr);
        p = NULL;
        hr = U(StgCreatePropSetStg)(f.stg, 1, &p);
        CHECKF(hr == S_OK, "reserved=1 (ignored by Windows) hr=%08lx", hr);
        t_note("StgCreatePropSetStg(reserved=1) = 0x%08lx", hr);
        if (SUCCEEDED(hr) && p) CALL(p, Release);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_two_propsetstg_same_storage) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_two.stg")) {
        IPropertySetStorage *p2 = NULL;
        EQ(U(StgCreatePropSetStg)(f.stg, 0, &p2), S_OK);
        CHECK(p2 != NULL);
        IPropertyStorage *a = NULL;
        EQ(CALL(f.pss, Create, &G_FMTID_SI, NULL, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &a), S_OK);
        if (a) {
            wr_i4(a, PID_FIRST_USABLE, 5);
            CALL(a, Commit, STGC_DEFAULT);
            CALL(a, Release);
        }
        IPropertyStorage *b = NULL;
        /* the second wrapper sees the stream the first one created */
        HRESULT hr = p2 ? CALL(p2, Open, &G_FMTID_SI, STGM_RW_EX, &b) : E_FAIL;
        EQ(hr, S_OK);
        if (b) { VARTYPE vt; LONG v; EQ(rd_i4(b, PID_FIRST_USABLE, &vt, &v), S_OK); EQ(vt, VT_I4); EQ(v, 5); CALL(b, Release); }
        if (p2) CALL(p2, Release);
        fx_close(&f);
    }
    COM_END();
}

/* ============================================================ IPropertySetStorage::Create */

TEST(pvps_pss_create_makes_named_stream) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    struct { const GUID *id; const char *n; } t[] = { {&G_FMTID_SI, "SI"}, {&G_FMTID_DSI, "DSI"}, {&G_ANY_GUID, "any"}, {&G_IID_IPropSetStg, "ipss"} };
    for (int i = 0; i < 4; i++) {
        FX f;
        if (!fx_new(&f, "pss_named.stg", t[i].id, PROPSETFLAG_DEFAULT)) { fx_close(&f); continue; }
        WCHAR name[40];
        EQ(U(FmtIdToPropStgName)(t[i].id, name), S_OK);
        CALL(f.ps, Commit, STGC_DEFAULT);
        CHECKF(stream_exists(f.stg, name) || (fx_release_ps(&f), stream_exists(f.stg, name)), "%s: stream missing", t[i].n);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_nonsimple_makes_storage_with_contents) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_nonsimple.stg", &G_ANY_GUID, PROPSETFLAG_NONSIMPLE)) {
        WCHAR name[40];
        EQ(U(FmtIdToPropStgName)(&G_ANY_GUID, name), S_OK);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        CHECK(storage_exists(f.stg, name));
        CHECK(!stream_exists(f.stg, name));
        IStorage *sub = NULL;
        if (CALL(f.stg, OpenStorage, name, NULL, STGM_R_EX, NULL, 0, &sub) == S_OK) {
            IStream *c = NULL;
            EQ(CALL(sub, OpenStream, L"CONTENTS", NULL, STGM_R_EX, 0, &c), S_OK);   /* the property data lives in "CONTENTS" */
            if (c) CALL(c, Release);
            CALL(sub, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_fail_if_there_without_create_flag) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_failifthere.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        wr_i4(f.ps, PID_FIRST_USABLE, 11);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        IPropertyStorage *p = NULL;
        HRESULT hr = CALL(f.pss, Create, &G_ANY_GUID, NULL, PROPSETFLAG_DEFAULT, STGM_RW_EX, &p);   /* implied STGM_FAILIFTHERE */
        CHECKF(FAILED(hr), "Create over an existing set succeeded hr=%08lx", hr);
        t_note("Create existing without STGM_CREATE -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && p) CALL(p, Release);
        /* the original content must be intact */
        p = NULL;
        EQ(CALL(f.pss, Open, &G_ANY_GUID, STGM_RW_EX, &p), S_OK);
        if (p) { VARTYPE vt; LONG v; EQ(rd_i4(p, PID_FIRST_USABLE, &vt, &v), S_OK); EQ(v, 11); CALL(p, Release); }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_with_create_flag_replaces) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_replace.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        wr_i4(f.ps, PID_FIRST_USABLE, 11);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        EQ(CALL(f.pss, Create, &G_ANY_GUID, NULL, PROPSETFLAG_DEFAULT, STGM_RW_EX_CREATE, &f.ps), S_OK);
        if (f.ps) {
            PROPVARIANT v; HRESULT hr = rd_id(f.ps, PID_FIRST_USABLE, &v);
            EQ(hr, S_FALSE); EQ(v.vt, VT_EMPTY);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_clsid_and_stat) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_stat.stg")) {
        EQ(CALL(f.pss, Create, &G_FMTID_SI, &G_IID_IUnknown, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &f.ps), S_OK);
        if (f.ps) {
            STATPROPSETSTG st; memset(&st, 0xEE, sizeof st);
            EQ(CALL(f.ps, Stat, &st), S_OK);
            CHECK(guid_eq(&st.fmtid, &G_FMTID_SI));
            CHECK(guid_eq(&st.clsid, &G_IID_IUnknown));
            CHECK(st.grfFlags & PROPSETFLAG_ANSI);
            CHECK(!(st.grfFlags & PROPSETFLAG_NONSIMPLE));
            t_note("Stat: grfFlags=%lx mtime=%08lx:%08lx ctime=%08lx:%08lx atime=%08lx:%08lx", st.grfFlags,
                   st.mtime.dwHighDateTime, st.mtime.dwLowDateTime, st.ctime.dwHighDateTime, st.ctime.dwLowDateTime,
                   st.atime.dwHighDateTime, st.atime.dwLowDateTime);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_null_clsid_stat_zero) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_nullclsid.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        STATPROPSETSTG st; memset(&st, 0xEE, sizeof st);
        EQ(CALL(f.ps, Stat, &st), S_OK);
        CHECK(guid_eq(&st.clsid, &G_NULL_GUID));     /* "May be NULL, in which case it is set to all zeroes" */
        CHECK(guid_eq(&st.fmtid, &G_ANY_GUID));
        CHECK(!(st.grfFlags & PROPSETFLAG_ANSI));
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_stat_flags_nonsimple) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_statflags.stg", &G_ANY_GUID, PROPSETFLAG_NONSIMPLE | PROPSETFLAG_ANSI)) {
        STATPROPSETSTG st; memset(&st, 0, sizeof st);
        EQ(CALL(f.ps, Stat, &st), S_OK);
        CHECK(st.grfFlags & PROPSETFLAG_NONSIMPLE);
        CHECK(st.grfFlags & PROPSETFLAG_ANSI);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_flag_combinations_succeed) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    static const DWORD fl[] = { 0, PROPSETFLAG_ANSI, PROPSETFLAG_NONSIMPLE, PROPSETFLAG_UNBUFFERED, PROPSETFLAG_CASE_SENSITIVE,
        PROPSETFLAG_ANSI | PROPSETFLAG_CASE_SENSITIVE, PROPSETFLAG_NONSIMPLE | PROPSETFLAG_UNBUFFERED, PROPSETFLAG_NONSIMPLE | PROPSETFLAG_CASE_SENSITIVE };
    for (int i = 0; i < N_ELEM(fl); i++) {
        FX f;
        if (!fx_storage(&f, "pss_flags.stg")) { fx_close(&f); continue; }
        if (t_is_wine() && (fl[i] & PROPSETFLAG_NONSIMPLE)) { fx_close(&f); continue; }
        HRESULT hr = CALL(f.pss, Create, &G_ANY_GUID, NULL, fl[i], STGM_RW_EX_CREATE, &f.ps);
        if (fl[i] & PROPSETFLAG_UNBUFFERED) {
            /* Windows 10: IPropertySetStorage::Create rejects PROPSETFLAG_UNBUFFERED */
            CHECKF(hr == STG_E_INVALIDFLAG, "flags 0x%lx hr=%08lx", fl[i], hr);
            t_note("Create(flags 0x%lx) -> 0x%08lx", fl[i], hr);
            if (SUCCEEDED(hr) && f.ps) { CALL(f.ps, Release); f.ps = NULL; }
            fx_close(&f); continue;
        }
        CHECKF(hr == S_OK, "flags 0x%lx hr=%08lx", fl[i], hr);
        if (hr == S_OK) {
            hr = wr_i4(f.ps, PID_FIRST_USABLE, 3);
            CHECKF(hr == S_OK, "flags 0x%lx write hr=%08lx", fl[i], hr);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_invalid_flag_bits) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("not run under wine"); return; }
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_invflag.stg")) {
        IPropertyStorage *p = NULL;
        HRESULT hr = CALL(f.pss, Create, &G_ANY_GUID, NULL, 0x80, STGM_RW_EX_CREATE, &p);
        t_note("Create(grfFlags=0x80) -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && p) CALL(p, Release);
        p = (IPropertyStorage *)1;
        hr = CALL(f.pss, Create, &G_ANY_GUID, NULL, 0, STGM_RW_EX_CREATE, NULL);
        CHECKF(FAILED(hr), "NULL out pointer accepted hr=%08lx", hr);
        t_note("Create(NULL out) -> 0x%08lx", hr);
        hr = CALL(f.pss, Create, NULL, NULL, 0, STGM_RW_EX_CREATE, &p);
        CHECKF(FAILED(hr), "NULL fmtid accepted hr=%08lx", hr);
        t_note("Create(NULL fmtid) -> 0x%08lx", hr);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_in_readonly_storage_fails) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_ro.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        wr_i4(f.ps, PID_FIRST_USABLE, 1);
        fx_close(&f);
        IStorage *stg = NULL; IPropertySetStorage *pss = NULL; IPropertyStorage *ps = NULL;
        WCHAR path[MAX_PATH]; w_path(path, "pss_ro.stg");
        EQ(U(StgOpenStorage)(path, NULL, STGM_R_EX, NULL, 0, &stg), S_OK);
        if (stg) {
            EQ(U(StgCreatePropSetStg)(stg, 0, &pss), S_OK);
            if (pss) {
                HRESULT hr = CALL(pss, Create, &G_ANY_GUID, NULL, 0, STGM_RW_EX_CREATE, &ps);
                CHECKF(FAILED(hr), "Create in a read-only storage succeeded hr=%08lx", hr);
                t_note("Create in read-only storage -> 0x%08lx", hr);
                if (SUCCEEDED(hr) && ps) CALL(ps, Release);
                ps = NULL;
                EQ(CALL(pss, Open, &G_FMTID_SI, STGM_R_EX, &ps), S_OK);    /* reading is fine */
                if (ps) {
                    VARTYPE vt; LONG v; EQ(rd_i4(ps, PID_FIRST_USABLE, &vt, &v), S_OK); EQ(v, 1);
                    hr = wr_i4(ps, PID_FIRST_USABLE, 2);
                    EQ(hr, STG_E_ACCESSDENIED);
                    CALL(ps, Release);
                }
                CALL(pss, Release);
            }
            CALL(stg, Release);
        }
    }
    COM_END();
}

/* ============================================================ Open / Delete */

TEST(pvps_pss_open_missing_fails_not_found) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_openmissing.stg")) {
        IPropertyStorage *p = (IPropertyStorage *)1;
        HRESULT hr = CALL(f.pss, Open, &G_FMTID_SI, STGM_RW_EX, &p);
        EQ(hr, STG_E_FILENOTFOUND_);
        CHECK(p == NULL || hr != S_OK);
        hr = CALL(f.pss, Open, &G_ANY_GUID, STGM_RW_EX, &p);
        EQ(hr, STG_E_FILENOTFOUND_);
        hr = CALL(f.pss, Open, &G_FMTID_DSI, STGM_RW_EX, &p);
        EQ(hr, STG_E_FILENOTFOUND_);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_open_readonly_blocks_write_ops) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_openro.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        wr_i4(f.ps, PID_FIRST_USABLE, 7);
        fx_release_ps(&f);
        EQ(CALL(f.pss, Open, &G_ANY_GUID, STGM_R_EX, &f.ps), S_OK);
        if (f.ps) {
            PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = PID_FIRST_USABLE;
            PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I4; v.lVal = 8;
            EQ(CALL(f.ps, WriteMultiple, 1, &s, &v, 2), STG_E_ACCESSDENIED);
            EQ(CALL(f.ps, DeleteMultiple, 1, &s), STG_E_ACCESSDENIED);
            LPOLESTR nm = (LPOLESTR)L"x";
            PROPID id = 2;
            EQ(CALL(f.ps, WritePropertyNames, 1, &id, &nm), STG_E_ACCESSDENIED);
            EQ(CALL(f.ps, DeletePropertyNames, 1, &id), STG_E_ACCESSDENIED);
            EQ(CALL(f.ps, SetClass, &G_ANY_GUID), STG_E_ACCESSDENIED);
            VARTYPE vt; LONG val; EQ(rd_i4(f.ps, PID_FIRST_USABLE, &vt, &val), S_OK); EQ(val, 7);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_delete_removes_set) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_delete.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        WCHAR name[40]; U(FmtIdToPropStgName)(&G_ANY_GUID, name);
        wr_i4(f.ps, PID_FIRST_USABLE, 1);
        fx_release_ps(&f);
        CHECK(stream_exists(f.stg, name));
        EQ(CALL(f.pss, Delete, &G_ANY_GUID), S_OK);
        CHECK(!stream_exists(f.stg, name));
        IPropertyStorage *p = NULL;
        EQ(CALL(f.pss, Open, &G_ANY_GUID, STGM_RW_EX, &p), STG_E_FILENOTFOUND_);
        /* delete a second time: "specifying a property set that does not exist returns an error" */
        HRESULT hr = CALL(f.pss, Delete, &G_ANY_GUID);
        EQ(hr, STG_E_FILENOTFOUND_);
        /* and it can be created again */
        EQ(CALL(f.pss, Create, &G_ANY_GUID, NULL, 0, STGM_RW_EX_CREATE, &p), S_OK);
        if (p) CALL(p, Release);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_create_without_create_flag_on_missing_set) {
    REQUIRE(U(StgCreatePropSetStg));
    /* documented: STGM_FAILIFTHERE is the default; with no existing set the call simply creates it
       (wine insists on STGM_CREATE here) */
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_nocreate.stg")) {
        HRESULT hr = CALL(f.pss, Create, &G_ANY_GUID, NULL, PROPSETFLAG_DEFAULT, STGM_RW_EX, &f.ps);
        t_note("Create without STGM_CREATE on a missing set -> 0x%08lx", hr);
        if (hr == S_OK && f.ps) EQ(wr_i4(f.ps, PID_FIRST_USABLE, 1), S_OK);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_delete_nonsimple_removes_storage) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_delete_ns.stg", &G_ANY_GUID, PROPSETFLAG_NONSIMPLE)) {
        WCHAR name[40]; U(FmtIdToPropStgName)(&G_ANY_GUID, name);
        fx_release_ps(&f);
        CHECK(storage_exists(f.stg, name));
        EQ(CALL(f.pss, Delete, &G_ANY_GUID), S_OK);
        CHECK(!storage_exists(f.stg, name));
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_delete_missing_set_fails) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_delete_missing.stg")) {
        EQ(CALL(f.pss, Delete, &G_FMTID_SI), STG_E_FILENOTFOUND_);
        EQ(CALL(f.pss, Delete, &G_ANY_GUID2), STG_E_FILENOTFOUND_);
        fx_close(&f);
    }
    COM_END();
}

/* ============================================================ Enum of property sets */

static int enum_sets(IPropertySetStorage *pss, GUID *out, DWORD *flags, int cap) {
    IEnumSTATPROPSETSTG *e = NULL; int n = 0;
    if (CALL(pss, Enum, &e) != S_OK || !e) return -1;
    for (;;) {
        STATPROPSETSTG st; ULONG got = 0;
        HRESULT hr = CALL(e, Next, 1, &st, &got);
        if (hr != S_OK || got != 1) break;
        if (n < cap) { out[n] = st.fmtid; if (flags) flags[n] = st.grfFlags; }
        n++;
    }
    CALL(e, Release);
    return n;
}

TEST(pvps_pss_enum_null_argument) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_enum_null.stg")) {
        EQ(CALL(f.pss, Enum, NULL), E_INVALIDARG_);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_enum_empty_storage) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_enum_empty.stg")) {
        IEnumSTATPROPSETSTG *e = NULL;
        EQ(CALL(f.pss, Enum, &e), S_OK);
        if (e) {
            STATPROPSETSTG st; ULONG got = 0xff;
            HRESULT hr = CALL(e, Next, 1, &st, &got);
            EQ(hr, S_FALSE); EQ(got, 0);
            EQ(CALL(e, Reset), S_OK);
            CALL(e, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_enum_lists_created_sets) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_enum_list.stg")) {
        IPropertyStorage *p[3] = {0};
        EQ(CALL(f.pss, Create, &G_FMTID_SI, NULL, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &p[0]), S_OK);
        EQ(CALL(f.pss, Create, &G_ANY_GUID, NULL, PROPSETFLAG_DEFAULT, STGM_RW_EX_CREATE, &p[1]), S_OK);
        int want = 3;
        if (t_is_wine()) want = 2;          /* wine cannot create nonsimple sets */
        else EQ(CALL(f.pss, Create, &G_ANY_GUID2, NULL, PROPSETFLAG_NONSIMPLE, STGM_RW_EX_CREATE, &p[2]), S_OK);
        for (int i = 0; i < 3; i++) if (p[i]) { wr_i4(p[i], PID_FIRST_USABLE, i); CALL(p[i], Release); }
        GUID ids[8]; DWORD fl[8];
        int n = enum_sets(f.pss, ids, fl, 8);
        EQ(n, want);
        int seen[3] = {0, 0, 0};
        for (int i = 0; i < n && i < 8; i++) {
            if (guid_eq(&ids[i], &G_FMTID_SI)) { seen[0]++; t_note("enum SI grfFlags=0x%lx", fl[i]); }
            else if (guid_eq(&ids[i], &G_ANY_GUID)) { seen[1]++; }
            else if (guid_eq(&ids[i], &G_ANY_GUID2)) { seen[2]++; t_note("enum nonsimple grfFlags=0x%lx", fl[i]); }
        }
        EQ(seen[0], 1); EQ(seen[1], 1); EQ(seen[2], want == 3);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_enum_ignores_plain_streams) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_enum_plain.stg")) {
        EQ(put_stream(f.stg, L"just a stream", "xyz", 3), S_OK);
        EQ(put_stream(f.stg, L"\005NotAPropertySetName", "xyz", 3), S_OK);
        IPropertyStorage *p = NULL;
        EQ(CALL(f.pss, Create, &G_ANY_GUID, NULL, 0, STGM_RW_EX_CREATE, &p), S_OK);
        if (p) CALL(p, Release);
        GUID ids[8];
        int n = enum_sets(f.pss, ids, NULL, 8);
        t_note("property sets enumerated with two foreign streams present: %d", n);
        int found = 0; for (int i = 0; i < n && i < 8; i++) if (guid_eq(&ids[i], &G_ANY_GUID)) found++;
        EQ(found, 1);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_enum_next_skip_reset_semantics) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+); the Windows-side results of the todo_wine lines are kept */
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_enum_skip.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        IEnumSTATPROPSETSTG *e = NULL; STATPROPSETSTG st; ULONG got = 0;
        EQ(CALL(f.pss, Enum, &e), S_OK);
        if (e) {
            EQ(CALL(e, Next, 1, &st, &got), S_OK); EQ(got, 1); CHECK(guid_eq(&st.fmtid, &G_FMTID_SI));
            EQ(CALL(e, Next, 1, &st, &got), S_FALSE);
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Skip, 2), S_FALSE);                       /* more than available */
            EQ(CALL(e, Next, 1, &st, &got), S_FALSE);
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Skip, 1), S_OK);
            EQ(CALL(e, Next, 1, &st, &got), S_FALSE);
            EQ(CALL(e, Reset), S_OK);
            HRESULT hr = CALL(e, Skip, 0);
            t_note("Skip(0) -> 0x%08lx", hr);
            EQ(CALL(e, Reset), S_OK);
            EQ(CALL(e, Next, 1, &st, &got), S_OK);               /* Reset really rewinds */
            IEnumSTATPROPSETSTG *c = NULL;
            hr = CALL(e, Clone, &c);
            t_note("Clone -> 0x%08lx", hr);
            if (SUCCEEDED(hr) && c) CALL(c, Release);
            CALL(e, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_enum_next_celt_zero_and_null_fetched) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pss_enum_celt.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        IEnumSTATPROPSETSTG *e = NULL; STATPROPSETSTG st[4];
        EQ(CALL(f.pss, Enum, &e), S_OK);
        if (e) {
            /* pceltFetched may be NULL when celt == 1 (COM convention) */
            HRESULT hr = CALL(e, Next, 1, st, NULL);
            t_note("Next(1, NULL fetched) -> 0x%08lx", hr);
            CALL(e, Reset);
            ULONG got = 77;
            hr = CALL(e, Next, 4, st, &got);                     /* ask for more than exist */
            EQ(hr, S_FALSE); EQ(got, 1);
            CALL(e, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

/* ============================================================ DocumentSummaryInformation: two sections */

TEST(pvps_pss_docsummary_userdefined_share_stream) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_dsi_ud.stg")) {
        IPropertyStorage *ud = NULL, *ds = NULL;
        EQ(CALL(f.pss, Create, &G_FMTID_UDP, &G_IID_IUnknown, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &ud), S_OK);
        if (ud) { EQ(wr_i4(ud, 0x10, 10), S_OK); CALL(ud, Release); }
        EQ(CALL(f.pss, Open, &G_FMTID_DSI, STGM_RW_EX, &ds), S_OK);          /* sibling section opens although never created */
        if (ds) {
            PROPVARIANT v; PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = 11;
            EQ(wr_id(ds, 0x10, &v), S_OK);
            CALL(ds, Release);
        }
        GUID ids[8];
        int n = enum_sets(f.pss, ids, NULL, 8);
        EQ(n, 1);                                                         /* one stream -> one enumerated set */
        if (n >= 1) CHECK(guid_eq(&ids[0], &G_FMTID_DSI));
        /* both sections keep their own property 0x10 */
        EQ(CALL(f.pss, Open, &G_FMTID_DSI, STGM_R_EX, &ds), S_OK);
        if (ds) { PROPVARIANT v; EQ(rd_id(ds, 0x10, &v), S_OK); EQ(v.vt, VT_UI4); EQ(v.ulVal, 11); CALL(ds, Release); }
        EQ(CALL(f.pss, Open, &G_FMTID_UDP, STGM_R_EX, &ud), S_OK);
        if (ud) { PROPVARIANT v; EQ(rd_id(ud, 0x10, &v), S_OK); EQ(v.vt, VT_I4); EQ(v.lVal, 10); CALL(ud, Release); }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_docsummary_userdefined_stat_clsid_independent) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_dsi_ud_stat.stg")) {
        IPropertyStorage *ds = NULL, *ud = NULL;
        EQ(CALL(f.pss, Create, &G_FMTID_DSI, NULL, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &ds), S_OK);
        if (ds) {
            STATPROPSETSTG st; EQ(CALL(ds, Stat, &st), S_OK);
            CHECK(guid_eq(&st.fmtid, &G_FMTID_DSI));
            CALL(ds, Release);
        }
        /* Windows 10: creating the DocumentSummaryInformation set does not create the user-defined section */
        HRESULT hu = CALL(f.pss, Open, &G_FMTID_UDP, STGM_RW_EX, &ud);
        t_note("Open(UDP) after Create(DSI) -> 0x%08lx", hu);
        CHECK(hu == STG_E_FILENOTFOUND);
        if (ud) { CALL(ud, Release); ud = NULL; }
        EQ(CALL(f.pss, Create, &G_FMTID_UDP, NULL, PROPSETFLAG_ANSI, STGM_RW_EX_CREATE, &ud), S_OK);
        if (ud) {
            STATPROPSETSTG st; EQ(CALL(ud, Stat, &st), S_OK);
            CHECK(guid_eq(&st.fmtid, &G_FMTID_UDP));
            CALL(ud, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pss_docsummary_raw_two_section_stream) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+): a hand-built two-section stream */
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "pss_dsi_raw.stg")) {
        PB p; p.n = 0;
        pb_u16(&p, 0xFFFE); pb_u16(&p, 0); pb_u32(&p, 0x2000a00); pb_guid(&p, &G_IID_IUnknown); pb_u32(&p, 2);
        pb_guid(&p, &G_FMTID_DSI); ULONG off1 = p.n; pb_u32(&p, 0);
        pb_guid(&p, &G_FMTID_UDP); ULONG off2 = p.n; pb_u32(&p, 0);
        pb_put32_at(&p, off1, p.n);
        pb_u32(&p, 8 + 8 + 8); pb_u32(&p, 1); pb_u32(&p, 0x10); pb_u32(&p, 16); pb_u32(&p, VT_UI4); pb_u32(&p, 11);
        pb_put32_at(&p, off2, p.n);
        pb_u32(&p, 8 + 8 + 8); pb_u32(&p, 1); pb_u32(&p, 0x10); pb_u32(&p, 16); pb_u32(&p, VT_I4); pb_u32(&p, 10);
        EQ(put_stream(f.stg, DSI_NAME, p.b, p.n), S_OK);
        IPropertyStorage *ds = NULL, *ud = NULL;
        EQ(CALL(f.pss, Open, &G_FMTID_DSI, STGM_R_EX, &ds), S_OK);
        if (ds) {
            PROPVARIANT v; EQ(rd_id(ds, 0x10, &v), S_OK); EQ(v.vt, VT_UI4); EQ(v.ulVal, 11);
            IEnumSTATPROPSTG *e = NULL; STATPROPSTG sp; ULONG got = 0;
            EQ(CALL(ds, Enum, &e), S_OK);
            if (e) {
                memset(&sp, 0, sizeof sp);
                EQ(CALL(e, Next, 1, &sp, &got), S_OK); EQ(sp.propid, 0x10); EQ(sp.vt, VT_UI4);
                EQ(CALL(e, Next, 1, &sp, &got), S_FALSE);
                CALL(e, Release);
            }
            CALL(ds, Release);
        }
        EQ(CALL(f.pss, Open, &G_FMTID_UDP, STGM_R_EX, &ud), S_OK);
        if (ud) {
            PROPVARIANT v; EQ(rd_id(ud, 0x10, &v), S_OK); EQ(v.vt, VT_I4); EQ(v.lVal, 10);
            IEnumSTATPROPSTG *e = NULL; STATPROPSTG sp; ULONG got = 0;
            EQ(CALL(ud, Enum, &e), S_OK);
            if (e) {
                memset(&sp, 0, sizeof sp);
                EQ(CALL(e, Next, 1, &sp, &got), S_OK); EQ(sp.propid, 0x10); EQ(sp.vt, VT_I4);
                CALL(e, Release);
            }
            CALL(ud, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

/* ============================================================ IPropertyStorage object level */

TEST(pvps_pst_qi_addref_release) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_qi.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        IUnknown *u = NULL; void *p = NULL;
        EQ(CALL(f.ps, QueryInterface, &G_IID_IUnknown, (void **)&u), S_OK);
        if (u) u->lpVtbl->Release(u);
        EQ(CALL(f.ps, QueryInterface, &G_IID_IPropStg, &p), S_OK);
        if (p) CALL((IPropertyStorage *)p, Release);
        p = (void *)1;
        EQ(CALL(f.ps, QueryInterface, &G_IID_IPropSetStg, &p), E_NOINTERFACE);
        CHECK(p == NULL);
        ULONG a = CALL(f.ps, AddRef), r = CALL(f.ps, Release);
        EQ(r, a - 1);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_stat_null_pointer) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("not run under wine"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_statnull.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        HRESULT hr = CALL(f.ps, Stat, NULL);
        CHECKF(FAILED(hr), "Stat(NULL) hr=%08lx", hr);
        t_note("Stat(NULL) -> 0x%08lx", hr);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_setclass_stat_and_persist) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_setclass.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        STATPROPSETSTG st;
        EQ(CALL(f.ps, SetClass, &G_ANY_GUID2), S_OK);
        EQ(CALL(f.ps, Stat, &st), S_OK); CHECK(guid_eq(&st.clsid, &G_ANY_GUID2));
        EQ(CALL(f.ps, Commit, STGC_DEFAULT), S_OK);
        if (fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX)) {
            EQ(CALL(f.ps, Stat, &st), S_OK); CHECK(guid_eq(&st.clsid, &G_ANY_GUID2));
            EQ(CALL(f.ps, SetClass, &G_NULL_GUID), S_OK);
            EQ(CALL(f.ps, Stat, &st), S_OK); CHECK(guid_eq(&st.clsid, &G_NULL_GUID));
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_setclass_null_pointer) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("not run under wine"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_setclass_null.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        HRESULT hr = CALL(f.ps, SetClass, NULL);
        CHECKF(FAILED(hr), "SetClass(NULL) hr=%08lx", hr);
        t_note("SetClass(NULL) -> 0x%08lx", hr);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_settimes_roundtrip_in_stat) {
    REQUIRE(U(StgCreatePropSetStg));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+): SetTimes succeeds on Windows (todo_wine there) */
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_settimes.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        FILETIME m = {1, 0}, c = {0x11111111, 0x01d00000}, a = {0x22222222, 0x01d00001};
        EQ(CALL(f.ps, SetTimes, NULL, NULL, &m), S_OK);
        EQ(CALL(f.ps, SetTimes, &c, &a, &m), S_OK);
        EQ(CALL(f.ps, SetTimes, NULL, NULL, NULL), S_OK);
        STATPROPSETSTG st; memset(&st, 0, sizeof st);
        EQ(CALL(f.ps, Stat, &st), S_OK);
        t_note("after SetTimes: ctime=%08lx:%08lx atime=%08lx:%08lx mtime=%08lx:%08lx",
               st.ctime.dwHighDateTime, st.ctime.dwLowDateTime, st.atime.dwHighDateTime, st.atime.dwLowDateTime,
               st.mtime.dwHighDateTime, st.mtime.dwLowDateTime);
        t_note("mtime explicitly set value reflected in Stat: %d", st.mtime.dwLowDateTime == 1 && st.mtime.dwHighDateTime == 0);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_commit_revert_success_codes) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_commit.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        EQ(CALL(f.ps, Commit, STGC_DEFAULT), S_OK);
        EQ(CALL(f.ps, Revert), S_OK);
        wr_i4(f.ps, PID_FIRST_USABLE, 1);
        EQ(CALL(f.ps, Commit, STGC_DEFAULT), S_OK);
        EQ(CALL(f.ps, Commit, STGC_OVERWRITE), S_OK);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_pst_revert_discards_uncommitted_in_transacted_nonsimple) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "pst_revert.stg", &G_ANY_GUID, PROPSETFLAG_NONSIMPLE)) {
        wr_i4(f.ps, PID_FIRST_USABLE, 1);
        EQ(CALL(f.ps, Commit, STGC_DEFAULT), S_OK);
        fx_release_ps(&f);
        EQ(CALL(f.pss, Open, &G_ANY_GUID, STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_TRANSACTED, &f.ps), S_OK);
        if (f.ps) {
            wr_i4(f.ps, PID_FIRST_USABLE, 2);
            EQ(CALL(f.ps, Revert), S_OK);
            VARTYPE vt; LONG v; EQ(rd_i4(f.ps, PID_FIRST_USABLE, &vt, &v), S_OK);
            EQ(vt, VT_I4);
            t_note("value after Revert of an uncommitted write in a transacted property set: %ld", v);
            EQ(v, 1);
        }
        fx_close(&f);
    }
    COM_END();
}

/* ============================================================ StgCreatePropStg / StgOpenPropStg on streams and storages */

TEST(pvps_stgprop_create_on_stream_roundtrip) {
    REQUIRE(U(StgCreatePropStg)); REQUIRE(U(StgOpenPropStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_stream.stg")) {
        IStream *s = NULL; IPropertyStorage *ps = NULL;
        EQ(CALL(f.stg, CreateStream, SI_NAME, STGM_RW_EX_CREATE, 0, 0, &s), S_OK);
        if (s) {
            EQ(U(StgCreatePropStg)((IUnknown *)s, &G_FMTID_SI, NULL, PROPSETFLAG_ANSI, 0, &ps), S_OK);
            if (ps) { EQ(wr_i4(ps, PID_FIRST_USABLE, 42), S_OK); EQ(CALL(ps, Commit, STGC_DEFAULT), S_OK); CALL(ps, Release); }
            ps = NULL;
            EQ(U(StgOpenPropStg)((IUnknown *)s, &G_FMTID_SI, PROPSETFLAG_DEFAULT, 0, &ps), S_OK);
            if (ps) { VARTYPE vt; LONG v; EQ(rd_i4(ps, PID_FIRST_USABLE, &vt, &v), S_OK); EQ(vt, VT_I4); EQ(v, 42); CALL(ps, Release); }
            CALL(s, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_stgprop_create_on_storage_is_nonsimple) {
    REQUIRE(U(StgCreatePropStg)); REQUIRE(U(StgOpenPropStg));
    if (t_is_wine()) { t_skip("wine does not implement PROPSETFLAG_NONSIMPLE"); return; }
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_storage.stg")) {
        IStorage *sub = NULL; IPropertyStorage *ps = NULL;
        EQ(CALL(f.stg, CreateStorage, L"ns", STGM_RW_EX_CREATE, 0, 0, &sub), S_OK);
        if (sub) {
            HRESULT hr = U(StgCreatePropStg)((IUnknown *)sub, &G_ANY_GUID, NULL, PROPSETFLAG_NONSIMPLE, 0, &ps);
            EQ(hr, S_OK);
            if (ps) {
                EQ(wr_i4(ps, PID_FIRST_USABLE, 9), S_OK);
                STATPROPSETSTG st; EQ(CALL(ps, Stat, &st), S_OK); CHECK(st.grfFlags & PROPSETFLAG_NONSIMPLE);
                CALL(ps, Commit, STGC_DEFAULT); CALL(ps, Release);
            }
            IStream *c = NULL;
            EQ(CALL(sub, OpenStream, L"CONTENTS", NULL, STGM_R_EX, 0, &c), S_OK);
            if (c) CALL(c, Release);
            CALL(sub, Release);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_stgprop_bad_arguments) {
    REQUIRE(U(StgCreatePropStg)); REQUIRE(U(StgOpenPropStg));
    if (t_is_wine()) { t_skip("not run under wine"); return; }
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_bad.stg")) {
        IStream *s = NULL; IPropertyStorage *ps = NULL;
        CALL(f.stg, CreateStream, SI_NAME, STGM_RW_EX_CREATE, 0, 0, &s);
        HRESULT hr;
        hr = U(StgCreatePropStg)(NULL, &G_FMTID_SI, NULL, 0, 0, &ps);
        CHECKF(FAILED(hr), "NULL pUnk accepted hr=%08lx", hr); t_note("StgCreatePropStg(NULL pUnk) -> 0x%08lx", hr);
        if (s) {
            hr = U(StgCreatePropStg)((IUnknown *)s, NULL, NULL, 0, 0, &ps);
            CHECKF(FAILED(hr), "NULL fmtid accepted hr=%08lx", hr); t_note("StgCreatePropStg(NULL fmtid) -> 0x%08lx", hr);
            hr = U(StgCreatePropStg)((IUnknown *)s, &G_FMTID_SI, NULL, 0, 0, NULL);
            CHECKF(FAILED(hr), "NULL out accepted hr=%08lx", hr); t_note("StgCreatePropStg(NULL out) -> 0x%08lx", hr);
            ps = NULL;
            hr = U(StgCreatePropStg)((IUnknown *)s, &G_FMTID_SI, NULL, 0, 1, &ps);
            CHECKF(hr == S_OK, "reserved=1 (ignored by Windows) hr=%08lx", hr); t_note("StgCreatePropStg(reserved=1) -> 0x%08lx", hr);
            if (SUCCEEDED(hr) && ps) { CALL(ps, Release); ps = NULL; }
            hr = U(StgOpenPropStg)((IUnknown *)s, &G_FMTID_SI, 0, 1, &ps);
            CHECKF(hr == S_OK, "Open reserved=1 (ignored by Windows) hr=%08lx", hr); t_note("StgOpenPropStg(reserved=1) -> 0x%08lx", hr);
            if (SUCCEEDED(hr) && ps) { CALL(ps, Release); ps = NULL; }
        }
        /* an object that is neither IStream nor IStorage */
        TUnk u; tu_init(&u);
        hr = U(StgCreatePropStg)((IUnknown *)&u, &G_FMTID_SI, NULL, 0, 0, &ps);
        CHECKF(FAILED(hr), "foreign IUnknown accepted hr=%08lx", hr); t_note("StgCreatePropStg(foreign IUnknown) -> 0x%08lx", hr);
        hr = U(StgOpenPropStg)((IUnknown *)&u, &G_FMTID_SI, 0, 0, &ps);
        CHECKF(FAILED(hr), "Open foreign IUnknown accepted hr=%08lx", hr); t_note("StgOpenPropStg(foreign IUnknown) -> 0x%08lx", hr);
        if (s) CALL(s, Release);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_stgprop_open_rejects_garbage_header) {
    REQUIRE(U(StgOpenPropStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_garbage.stg")) {
        BYTE junk[128]; memset(junk, 0x5A, sizeof junk);
        EQ(put_stream(f.stg, SI_NAME, junk, sizeof junk), S_OK);
        IStream *s = NULL; IPropertyStorage *ps = (IPropertyStorage *)1;
        EQ(CALL(f.stg, OpenStream, SI_NAME, NULL, STGM_RW_EX, 0, &s), S_OK);
        if (s) {
            HRESULT hr = U(StgOpenPropStg)((IUnknown *)s, &G_FMTID_SI, PROPSETFLAG_DEFAULT, 0, &ps);
            CHECKF(FAILED(hr), "garbage stream accepted hr=%08lx", hr);
            t_note("StgOpenPropStg(garbage header) -> 0x%08lx", hr);
            if (SUCCEEDED(hr) && ps) CALL(ps, Release);
            CALL(s, Release);
        }
        /* same through IPropertySetStorage::Open */
        IPropertyStorage *p2 = NULL;
        HRESULT hr = CALL(f.pss, Open, &G_FMTID_SI, STGM_RW_EX, &p2);
        CHECKF(FAILED(hr), "Open on garbage stream accepted hr=%08lx", hr);
        t_note("IPropertySetStorage::Open(garbage) -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && p2) CALL(p2, Release);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_stgprop_open_unknown_byte_order_or_version) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_version.stg")) {
        PB p; pb_set_header(&p, &G_ANY_GUID);
        pb_u32(&p, 8 + 8 + 8); pb_u32(&p, 1); pb_u32(&p, 2); pb_u32(&p, 16); pb_u32(&p, VT_I4); pb_u32(&p, 77);
        WCHAR name[40]; U(FmtIdToPropStgName)(&G_ANY_GUID, name);
        /* a good stream first */
        EQ(put_stream(f.stg, name, p.b, p.n), S_OK);
        IPropertyStorage *ps = NULL;
        HRESULT hr = CALL(f.pss, Open, &G_ANY_GUID, STGM_R_EX, &ps);
        EQ(hr, S_OK);
        if (ps) { VARTYPE vt; LONG v; EQ(rd_i4(ps, 2, &vt, &v), S_OK); EQ(vt, VT_I4); EQ(v, 77); CALL(ps, Release); }
        /* corrupt the byte-order mark */
        PB bad = p; bad.b[0] = 0x00; bad.b[1] = 0x00;
        EQ(put_stream(f.stg, name, bad.b, bad.n), S_OK);
        hr = CALL(f.pss, Open, &G_ANY_GUID, STGM_R_EX, &ps);
        CHECKF(FAILED(hr), "bad byte order accepted hr=%08lx", hr);
        t_note("Open with byte order 0x0000 -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && ps) CALL(ps, Release);
        /* a section count of 0 */
        bad = p; bad.b[24] = 0;
        EQ(put_stream(f.stg, name, bad.b, bad.n), S_OK);
        hr = CALL(f.pss, Open, &G_ANY_GUID, STGM_R_EX, &ps);
        t_note("Open with zero sections -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && ps) CALL(ps, Release);
        /* truncated stream */
        EQ(put_stream(f.stg, name, p.b, 30), S_OK);
        hr = CALL(f.pss, Open, &G_ANY_GUID, STGM_R_EX, &ps);
        CHECKF(FAILED(hr), "truncated header accepted hr=%08lx", hr);
        t_note("Open with truncated stream -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && ps) CALL(ps, Release);
        fx_close(&f);
    }
    COM_END();
}

TEST(pvps_stgprop_open_empty_stream_observed) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_storage(&f, "stgprop_empty.stg")) {
        EQ(put_stream(f.stg, SI_NAME, "", 0), S_OK);
        IPropertyStorage *ps = NULL;
        HRESULT hr = CALL(f.pss, Open, &G_FMTID_SI, STGM_RW_EX, &ps);
        t_note("Open on a zero-length SummaryInformation stream -> 0x%08lx", hr);
        if (SUCCEEDED(hr) && ps) {
            PROPVARIANT v; HRESULT r = rd_id(ps, PID_CODEPAGE, &v);
            t_note("  codepage read -> 0x%08lx vt=%d", r, v.vt);
            U(PropVariantClear)(&v);
            CALL(ps, Release);
        }
        CHECK(1);
        fx_close(&f);
    }
    COM_END();
}
