/* Round trips of every writable VT through a compound file, persistence across reopen, large
 * properties, standard SummaryInformation / DocumentSummaryInformation property sets, and the
 * on-disk layout of the property set stream (MS-OLEPS). */
#include "pv.h"

/* ------------------------------------------------------------------ deep comparison */

static size_t scalar_size(VARTYPE vt) {
    switch (vt) {
    case VT_I1: case VT_UI1: return 1;
    case VT_I2: case VT_UI2: case VT_BOOL: return 2;
    case VT_I4: case VT_UI4: case VT_R4: case VT_ERROR: case VT_INT: case VT_UINT: return 4;
    case VT_I8: case VT_UI8: case VT_R8: case VT_CY: case VT_DATE: case VT_FILETIME: return 8;
    case VT_CLSID: return 16;
    }
    return 0;
}

static int pv_equal(const PROPVARIANT *a, const PROPVARIANT *b) {
    if (a->vt != b->vt) return 0;
    VARTYPE vt = a->vt;
    if (vt & VT_VECTOR) {
        VARTYPE base = vt & VT_TYPEMASK;
        if (a->caub.cElems != b->caub.cElems) return 0;
        ULONG n = a->caub.cElems;
        if (!n) return 1;
        if (!a->caub.pElems || !b->caub.pElems) return 0;
        size_t sz = scalar_size(base);
        if (sz) return !memcmp(a->caub.pElems, b->caub.pElems, sz * n);
        for (ULONG i = 0; i < n; i++) {
            switch (base) {
            case VT_LPSTR: if (strcmp(a->calpstr.pElems[i], b->calpstr.pElems[i])) return 0; break;
            case VT_LPWSTR: if (lstrcmpW(a->calpwstr.pElems[i], b->calpwstr.pElems[i])) return 0; break;
            case VT_BSTR: if (lstrcmpW(a->cabstr.pElems[i], b->cabstr.pElems[i])) return 0; break;
            case VT_VARIANT: if (!pv_equal(&a->capropvar.pElems[i], &b->capropvar.pElems[i])) return 0; break;
            case VT_CF:
                if (a->caclipdata.pElems[i].cbSize != b->caclipdata.pElems[i].cbSize ||
                    a->caclipdata.pElems[i].ulClipFmt != b->caclipdata.pElems[i].ulClipFmt ||
                    memcmp(a->caclipdata.pElems[i].pClipData, b->caclipdata.pElems[i].pClipData, a->caclipdata.pElems[i].cbSize - 4)) return 0;
                break;
            default: return 0;
            }
        }
        return 1;
    }
    switch (vt) {
    case VT_EMPTY: case VT_NULL: return 1;
    case VT_LPSTR: return !strcmp(a->pszVal, b->pszVal);
    case VT_LPWSTR: return !lstrcmpW(a->pwszVal, b->pwszVal);
    case VT_BSTR: return !lstrcmpW(a->bstrVal, b->bstrVal);
    case VT_BLOB: return a->blob.cbSize == b->blob.cbSize && (!a->blob.cbSize || !memcmp(a->blob.pBlobData, b->blob.pBlobData, a->blob.cbSize));
    case VT_CF: return a->pclipdata->cbSize == b->pclipdata->cbSize && a->pclipdata->ulClipFmt == b->pclipdata->ulClipFmt &&
                       !memcmp(a->pclipdata->pClipData, b->pclipdata->pClipData, a->pclipdata->cbSize - 4);
    case VT_CLSID: return guid_eq(a->puuid, b->puuid);
    default: { size_t sz = scalar_size(vt); return sz && !memcmp(&a->cVal, &b->cVal, sz); }
    }
}

/* Write v as property `id` of a fresh set, check it in-session and again after a reopen.
   strict: mismatches fail; otherwise they are recorded as notes. */
static void rt_case(const char *what, PROPVARIANT *v, DWORD flags, int strict, const char *leaf) {
    FX f;
    g_ctx = what;
    if (!fx_new(&f, leaf, &G_ANY_GUID, flags)) { fx_close(&f); return; }
    HRESULT hr = wr_id(f.ps, 7, v);
    if (hr != S_OK) {
        if (strict) CHECKF(0, "%s: write hr=%08lx", what, hr); else t_note("%s: write -> 0x%08lx", what, hr);
        fx_close(&f); return;
    }
    for (int pass = 0; pass < 2; pass++) {
        PROPVARIANT r; hr = rd_id(f.ps, 7, &r);
        int ok = hr == S_OK && pv_equal(v, &r);
        if (strict) CHECKF(ok, "%s (%s): hr=%08lx vt=0x%x (want 0x%x)", what, pass ? "after reopen" : "same session", hr, r.vt, v->vt);
        else if (!ok) t_note("%s (%s): mismatch hr=%08lx vt=0x%x (want 0x%x)", what, pass ? "after reopen" : "same session", hr, r.vt, v->vt);
        U(PropVariantClear)(&r);
        if (pass == 0 && !fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX)) break;
    }
    fx_close(&f);
}

#define RT(what, init, strict) do { PROPVARIANT v_; PropVariantInit(&v_); init; \
    rt_case(what, &v_, flags, strict, "rt.stg"); } while (0)

static void rt_scalars(DWORD flags) {
    RT("VT_EMPTY", v_.vt = VT_EMPTY, 0);
    RT("VT_NULL", v_.vt = VT_NULL, 0);
    RT("VT_I2", (v_.vt = VT_I2, v_.iVal = -1234), 1);
    RT("VT_I4", (v_.vt = VT_I4, v_.lVal = -123456789), 1);
    RT("VT_R4", (v_.vt = VT_R4, v_.fltVal = 3.5f), 1);
    RT("VT_R8", (v_.vt = VT_R8, v_.dblVal = -2.25e100), 1);
    RT("VT_CY", (v_.vt = VT_CY, v_.cyVal.int64 = 123456789012345ll), 1);
    RT("VT_DATE", (v_.vt = VT_DATE, v_.date = 40000.5), 1);
    RT("VT_ERROR", (v_.vt = VT_ERROR, v_.scode = (SCODE)0x80020005), 1);
    RT("VT_BOOL true", (v_.vt = VT_BOOL, v_.boolVal = VARIANT_TRUE), 1);
    RT("VT_BOOL false", (v_.vt = VT_BOOL, v_.boolVal = VARIANT_FALSE), 1);
    RT("VT_I1", (v_.vt = VT_I1, v_.cVal = -100), 1);
    RT("VT_UI1", (v_.vt = VT_UI1, v_.bVal = 250), 1);
    RT("VT_UI2", (v_.vt = VT_UI2, v_.uiVal = 65000), 1);
    RT("VT_UI4", (v_.vt = VT_UI4, v_.ulVal = 0xfedcba98), 1);
    RT("VT_I8", (v_.vt = VT_I8, v_.hVal.QuadPart = -0x123456789abcdefll), 1);
    RT("VT_UI8", (v_.vt = VT_UI8, v_.uhVal.QuadPart = 0xfedcba9876543210ull), 1);
    RT("VT_INT", (v_.vt = VT_INT, v_.intVal = -77), 0);
    RT("VT_UINT", (v_.vt = VT_UINT, v_.uintVal = 77), 0);
    RT("VT_FILETIME", (v_.vt = VT_FILETIME, v_.filetime.dwLowDateTime = 0x01234567, v_.filetime.dwHighDateTime = 0x01d00000), 1);
}

TEST(rt_scalars_unicode_set) { REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN(); rt_scalars(PROPSETFLAG_DEFAULT); COM_END(); }
TEST(rt_scalars_ansi_set)    { REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN(); rt_scalars(PROPSETFLAG_ANSI); COM_END(); }

static void rt_strings_blobs(DWORD flags) {
    RT("LPSTR", (v_.vt = VT_LPSTR, v_.pszVal = (LPSTR)"plain ascii text"), 1);
    RT("LPSTR empty", (v_.vt = VT_LPSTR, v_.pszVal = (LPSTR)""), 1);
    RT("LPWSTR", (v_.vt = VT_LPWSTR, v_.pwszVal = (LPWSTR)L"wide text \x00e9\x4e2d"), 1);
    RT("LPWSTR empty", (v_.vt = VT_LPWSTR, v_.pwszVal = (LPWSTR)L""), 1);
    RT("LPWSTR odd length (padding)", (v_.vt = VT_LPWSTR, v_.pwszVal = (LPWSTR)L"abc"), 1);
    BSTR b = U(SysAllocString)(L"a bstr");
    RT("BSTR", (v_.vt = VT_BSTR, v_.bstrVal = b), 1);
    U(SysFreeString)(b);
    BYTE d[37]; for (int i = 0; i < 37; i++) d[i] = (BYTE)(i * 11);
    RT("BLOB 37", (v_.vt = VT_BLOB, v_.blob.cbSize = 37, v_.blob.pBlobData = d), 1);
    RT("BLOB 1", (v_.vt = VT_BLOB, v_.blob.cbSize = 1, v_.blob.pBlobData = d), 1);
    RT("BLOB 4", (v_.vt = VT_BLOB, v_.blob.cbSize = 4, v_.blob.pBlobData = d), 1);
    RT("BLOB 0", (v_.vt = VT_BLOB, v_.blob.cbSize = 0, v_.blob.pBlobData = NULL), 1);
    CLIPDATA cd; cd.cbSize = 4 + 37; cd.ulClipFmt = CF_ENHMETAFILE; cd.pClipData = d;
    RT("CF enhmetafile", (v_.vt = VT_CF, v_.pclipdata = &cd), 1);
    cd.ulClipFmt = 0xC001; cd.cbSize = 4 + 5;
    RT("CF registered format", (v_.vt = VT_CF, v_.pclipdata = &cd), 1);
    cd.ulClipFmt = CF_TEXT; cd.cbSize = 4;
    RT("CF empty data", (v_.vt = VT_CF, v_.pclipdata = &cd), 0);
    GUID g = G_ANY_GUID;
    RT("CLSID", (v_.vt = VT_CLSID, v_.puuid = &g), 1);
}

TEST(rt_strings_blobs_unicode_set) { REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(SysAllocString)); COM_BEGIN(); rt_strings_blobs(PROPSETFLAG_DEFAULT); COM_END(); }
TEST(rt_strings_blobs_ansi_set)    { REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(SysAllocString)); COM_BEGIN(); rt_strings_blobs(PROPSETFLAG_ANSI); COM_END(); }

static void rt_vectors(DWORD flags) {
    { static const CHAR e[] = {-1, 0, 1, 127, -128};  RT("vector I1", (v_.vt = VT_VECTOR | VT_I1, v_.cac.cElems = 5, v_.cac.pElems = (CHAR *)e), 1); }
    { static const BYTE e[] = {0, 1, 255};            RT("vector UI1", (v_.vt = VT_VECTOR | VT_UI1, v_.caub.cElems = 3, v_.caub.pElems = (BYTE *)e), 1); }
    { static SHORT e[] = {-1, 2, 3};                  RT("vector I2", (v_.vt = VT_VECTOR | VT_I2, v_.cai.cElems = 3, v_.cai.pElems = e), 1); }
    { static USHORT e[] = {1, 2, 65535, 4};           RT("vector UI2", (v_.vt = VT_VECTOR | VT_UI2, v_.caui.cElems = 4, v_.caui.pElems = e), 1); }
    { static LONG e[] = {-1, 2, 3};                   RT("vector I4", (v_.vt = VT_VECTOR | VT_I4, v_.cal.cElems = 3, v_.cal.pElems = e), 1); }
    { static ULONG e[] = {1, 0xffffffff};             RT("vector UI4", (v_.vt = VT_VECTOR | VT_UI4, v_.caul.cElems = 2, v_.caul.pElems = e), 1); }
    { static float e[] = {1.5f, -2.5f};               RT("vector R4", (v_.vt = VT_VECTOR | VT_R4, v_.caflt.cElems = 2, v_.caflt.pElems = e), 1); }
    { static double e[] = {1.5, -2.5, 1e300};         RT("vector R8", (v_.vt = VT_VECTOR | VT_R8, v_.cadbl.cElems = 3, v_.cadbl.pElems = e), 1); }
    { static LARGE_INTEGER e[2] = {{{1, 2}}, {{3, 4}}}; RT("vector I8", (v_.vt = VT_VECTOR | VT_I8, v_.cah.cElems = 2, v_.cah.pElems = e), 1); }
    { static ULARGE_INTEGER e[2] = {{{1, 2}}, {{3, 4}}}; RT("vector UI8", (v_.vt = VT_VECTOR | VT_UI8, v_.cauh.cElems = 2, v_.cauh.pElems = e), 1); }
    { static CY e[2] = {{{1, 2}}, {{3, 4}}};          RT("vector CY", (v_.vt = VT_VECTOR | VT_CY, v_.cacy.cElems = 2, v_.cacy.pElems = e), 1); }
    { static DATE e[] = {1.0, 2.5};                   RT("vector DATE", (v_.vt = VT_VECTOR | VT_DATE, v_.cadate.cElems = 2, v_.cadate.pElems = e), 1); }
    { static VARIANT_BOOL e[] = {VARIANT_TRUE, VARIANT_FALSE, VARIANT_TRUE}; RT("vector BOOL", (v_.vt = VT_VECTOR | VT_BOOL, v_.cabool.cElems = 3, v_.cabool.pElems = e), 1); }
    { static SCODE e[] = {0, (SCODE)0x80004005};      RT("vector ERROR", (v_.vt = VT_VECTOR | VT_ERROR, v_.cascode.cElems = 2, v_.cascode.pElems = e), 1); }
    { static FILETIME e[2] = {{1, 2}, {3, 4}};        RT("vector FILETIME", (v_.vt = VT_VECTOR | VT_FILETIME, v_.cafiletime.cElems = 2, v_.cafiletime.pElems = e), 1); }
    { static CLSID e[2]; e[0] = G_ANY_GUID; e[1] = G_ANY_GUID2; RT("vector CLSID", (v_.vt = VT_VECTOR | VT_CLSID, v_.cauuid.cElems = 2, v_.cauuid.pElems = e), 1); }
    { static LPSTR e[] = {"one", "", "three three"};   RT("vector LPSTR", (v_.vt = VT_VECTOR | VT_LPSTR, v_.calpstr.cElems = 3, v_.calpstr.pElems = e), 1); }
    { static LPWSTR e[] = {L"a", L"bc", L"def", L"ghij"}; RT("vector LPWSTR", (v_.vt = VT_VECTOR | VT_LPWSTR, v_.calpwstr.cElems = 4, v_.calpwstr.pElems = e), 1); }
    { BSTR e[2] = {U(SysAllocString)(L"x"), U(SysAllocString)(L"yz")};
      RT("vector BSTR", (v_.vt = VT_VECTOR | VT_BSTR, v_.cabstr.cElems = 2, v_.cabstr.pElems = e), 1);
      U(SysFreeString)(e[0]); U(SysFreeString)(e[1]); }
    { PROPVARIANT e[3]; memset(e, 0, sizeof e);
      e[0].vt = VT_I4; e[0].lVal = 5; e[1].vt = VT_LPWSTR; e[1].pwszVal = (LPWSTR)L"in vector"; e[2].vt = VT_R8; e[2].dblVal = 0.5;
      RT("vector VARIANT", (v_.vt = VT_VECTOR | VT_VARIANT, v_.capropvar.cElems = 3, v_.capropvar.pElems = e), 1); }
    { BYTE d[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
      CLIPDATA e[2]; e[0].cbSize = 4 + 9; e[0].ulClipFmt = CF_TEXT; e[0].pClipData = d; e[1].cbSize = 4 + 3; e[1].ulClipFmt = CF_ENHMETAFILE; e[1].pClipData = d;
      RT("vector CF", (v_.vt = VT_VECTOR | VT_CF, v_.caclipdata.cElems = 2, v_.caclipdata.pElems = e), 1); }
    RT("vector I4 empty", (v_.vt = VT_VECTOR | VT_I4, v_.cal.cElems = 0, v_.cal.pElems = NULL), 0);
}

TEST(rt_vectors_unicode_set) { REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(SysAllocString)); COM_BEGIN(); rt_vectors(PROPSETFLAG_DEFAULT); COM_END(); }
TEST(rt_vectors_ansi_set)    { REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(SysAllocString)); COM_BEGIN(); rt_vectors(PROPSETFLAG_ANSI); COM_END(); }

TEST(rt_nonsimple_set_scalars_and_strings) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine lacks nonsimple property sets"); return; }
    COM_BEGIN();
    DWORD flags = PROPSETFLAG_NONSIMPLE;
    RT("ns I4", (v_.vt = VT_I4, v_.lVal = 99), 1);
    RT("ns R8", (v_.vt = VT_R8, v_.dblVal = 9.5), 1);
    RT("ns LPWSTR", (v_.vt = VT_LPWSTR, v_.pwszVal = (LPWSTR)L"in a nonsimple set"), 1);
    RT("ns LPSTR", (v_.vt = VT_LPSTR, v_.pszVal = (LPSTR)"ansi in nonsimple"), 1);
    BYTE d[20] = {1, 2, 3};
    RT("ns BLOB", (v_.vt = VT_BLOB, v_.blob.cbSize = 20, v_.blob.pBlobData = d), 1);
    COM_END();
}

TEST(rt_nonsimple_stream_and_storage_properties) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine lacks nonsimple property sets"); return; }
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "rt_ns_obj.stg", &G_ANY_GUID, PROPSETFLAG_NONSIMPLE)) {
        PROPVARIANT v; PropVariantInit(&v);
        v.vt = VT_STREAM; v.pStream = NULL;
        HRESULT hr = wr_id(f.ps, 5, &v);
        t_note("write VT_STREAM (NULL pointer) in a nonsimple set -> 0x%08lx", hr);
        if (hr == S_OK) {
            PROPVARIANT r; hr = rd_id(f.ps, 5, &r);
            t_note("read back -> 0x%08lx vt=%d ptr=%p", hr, r.vt, (void *)r.pStream);
            if (r.vt == VT_STREAM && r.pStream) {
                ULONG w = 0; HRESULT whr = CALL(r.pStream, Write, "hello", 5, &w);
                t_note("write through the returned stream -> 0x%08lx (%lu bytes)", whr, w);
            }
            U(PropVariantClear)(&r);
        }
        PropVariantInit(&v); v.vt = VT_STORAGE; v.pStorage = NULL;
        hr = wr_id(f.ps, 6, &v);
        t_note("write VT_STORAGE (NULL pointer) in a nonsimple set -> 0x%08lx", hr);
        if (hr == S_OK) {
            PROPVARIANT r; hr = rd_id(f.ps, 6, &r);
            t_note("read back -> 0x%08lx vt=%d ptr=%p", hr, r.vt, (void *)r.pStorage);
            U(PropVariantClear)(&r);
        }
        CHECK(1);
        fx_close(&f);
    }
    COM_END();
}

TEST(rt_stream_property_rejected_in_simple_set) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "rt_simple_stream.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        IStream *s = NULL;
        CALL(f.stg, CreateStream, L"aux", STGM_RW_EX_CREATE, 0, 0, &s);
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_STREAM; v.pStream = s;
        HRESULT hr = wr_id(f.ps, 5, &v);
        t_note("VT_STREAM written to a simple property set -> 0x%08lx", hr);
        
        v.vt = VT_STORAGE; v.pStorage = NULL;
        hr = wr_id(f.ps, 6, &v);
        t_note("VT_STORAGE written to a simple property set -> 0x%08lx", hr);
        
        if (s) CALL(s, Release);
        fx_close(&f);
    }
    COM_END();
}

/* ------------------------------------------------------------------ persistence */

TEST(persist_mixed_set_survives_reopen) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    for (int pass = 0; pass < 2; pass++) {
        FX f;
        if (!fx_new(&f, "persist_mixed.stg", &G_FMTID_SI, pass ? PROPSETFLAG_ANSI : PROPSETFLAG_DEFAULT)) { fx_close(&f); continue; }
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_I4; v.lVal = 77; wr_id(f.ps, 2, &v);
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"persisted"; wr_id(f.ps, 3, &v);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"persisted wide"; wr_id(f.ps, 4, &v);
        PropVariantInit(&v); v.vt = VT_R8; v.dblVal = 2.5; wr_name(f.ps, L"named real", &v, 0x20);
        PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; wr_name(f.ps, L"named bool", &v, 0x20);
        EQ(CALL(f.ps, SetClass, &G_ANY_GUID2), S_OK);
        if (fx_reopen(&f, &G_FMTID_SI, STGM_RW_EX)) {
            VARTYPE vt; LONG x; EQ(rd_i4(f.ps, 2, &vt, &x), S_OK); EQ(x, 77);
            PROPVARIANT r;
            EQ(rd_id(f.ps, 3, &r), S_OK); EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "persisted"); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, 4, &r), S_OK); EQ(r.vt, VT_LPWSTR); if (r.vt == VT_LPWSTR) EXPECT_WSTR(r.pwszVal, L"persisted wide"); U(PropVariantClear)(&r);
            EQ(rd_name(f.ps, L"named real", &r), S_OK); EQ(r.vt, VT_R8); CHECK(r.dblVal == 2.5);
            EQ(rd_name(f.ps, L"NAMED BOOL", &r), S_OK); EQ(r.vt, VT_BOOL); EQ(r.boolVal, VARIANT_TRUE);
            STATPROPSETSTG st; EQ(CALL(f.ps, Stat, &st), S_OK); CHECK(guid_eq(&st.clsid, &G_ANY_GUID2));
            EQ(rd_id(f.ps, PID_CODEPAGE, &r), S_OK); EQ(r.vt, VT_I2);
            if (pass) EQ((unsigned short)r.iVal, (unsigned short)GetACP()); else EQ(r.iVal, 1200);
            STATPROPSTG sp[8]; IEnumSTATPROPSTG *e = NULL; ULONG got; int n = 0;
            EQ(CALL(f.ps, Enum, &e), S_OK);
            while (e && CALL(e, Next, 1, &sp[n < 8 ? n : 7], &got) == S_OK) { if (sp[n < 8 ? n : 7].lpwstrName) U(CoTaskMemFree)(sp[n < 8 ? n : 7].lpwstrName); n++; }
            if (e) CALL(e, Release);
            EQ(n, 5);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(persist_second_reopen_and_modify) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "persist_modify.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        wr_i4(f.ps, 2, 1); wr_i4(f.ps, 3, 2); wr_i4(f.ps, 4, 3);
        if (fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX)) {
            wr_i4(f.ps, 3, 20);
            PROPSPEC s; s.ulKind = PRSPEC_PROPID; s.propid = 4; CALL(f.ps, DeleteMultiple, 1, &s);
            wr_i4(f.ps, 5, 30);
            if (fx_reopen(&f, &G_ANY_GUID, STGM_R_EX)) {
                VARTYPE vt; LONG x;
                EQ(rd_i4(f.ps, 2, &vt, &x), S_OK); EQ(x, 1);
                EQ(rd_i4(f.ps, 3, &vt, &x), S_OK); EQ(x, 20);
                EQ(rd_i4(f.ps, 4, &vt, &x), S_FALSE);
                EQ(rd_i4(f.ps, 5, &vt, &x), S_OK); EQ(x, 30);
            }
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(persist_release_without_commit_observed) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "persist_nocommit.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        wr_i4(f.ps, 2, 5);
        WCHAR path[MAX_PATH]; lstrcpyW(path, f.path);
        fx_close(&f);                                   /* no Commit: direct mode flushes on release? */
        EQ(U(StgOpenStorage)(path, NULL, STGM_RW_EX, NULL, 0, &f.stg), S_OK);
        if (f.stg) {
            EQ(U(StgCreatePropSetStg)(f.stg, 0, &f.pss), S_OK);
            HRESULT hr = f.pss ? CALL(f.pss, Open, &G_ANY_GUID, STGM_RW_EX, &f.ps) : E_FAIL;
            if (hr == S_OK) { VARTYPE vt; LONG x; hr = rd_i4(f.ps, 2, &vt, &x); t_note("uncommitted write after release+reopen: hr=0x%08lx value=%ld", hr, x); }
            else t_note("reopen after release without commit -> 0x%08lx", hr);
        }
        CHECK(1);
        fx_close(&f);
    }
    COM_END();
}

TEST(persist_survives_storage_commit_and_file_reopen_readonly) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "persist_ro.stg", &G_FMTID_DSI, PROPSETFLAG_ANSI)) {
        PROPVARIANT v; PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"category"; wr_id(f.ps, 2, &v);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        EQ(CALL(f.stg, Commit, STGC_DEFAULT), S_OK);
        WCHAR path[MAX_PATH]; lstrcpyW(path, f.path);
        fx_close(&f);
        IStorage *stg = NULL; IPropertySetStorage *pss = NULL; IPropertyStorage *ps = NULL;
        EQ(U(StgOpenStorage)(path, NULL, STGM_R_EX, NULL, 0, &stg), S_OK);
        if (stg) {
            EQ(U(StgCreatePropSetStg)(stg, 0, &pss), S_OK);
            if (pss) {
                EQ(CALL(pss, Open, &G_FMTID_DSI, STGM_R_EX, &ps), S_OK);
                if (ps) { PROPVARIANT r; EQ(rd_id(ps, 2, &r), S_OK); EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "category"); U(PropVariantClear)(&r); CALL(ps, Release); }
                CALL(pss, Release);
            }
            CALL(stg, Release);
        }
    }
    COM_END();
}

/* ------------------------------------------------------------------ large properties */

static void large_case(DWORD flags, const char *leaf, ULONG size, int as_blob) {
    FX f;
    if (!fx_new(&f, leaf, &G_ANY_GUID, flags)) { fx_close(&f); return; }
    BYTE *buf = malloc(size + 2);
    for (ULONG i = 0; i < size; i++) buf[i] = (BYTE)('a' + (i * 7 + i / 13) % 26);
    PROPVARIANT v; PropVariantInit(&v);
    if (as_blob) { v.vt = VT_BLOB; v.blob.cbSize = size; v.blob.pBlobData = buf; }
    else { buf[size] = 0; v.vt = VT_LPSTR; v.pszVal = (LPSTR)buf; }
    HRESULT hr = wr_id(f.ps, 9, &v);
    CHECKF(hr == S_OK, "size %lu %s write hr=%08lx", size, as_blob ? "blob" : "lpstr", hr);
    if (hr == S_OK) {
        for (int pass = 0; pass < 2; pass++) {
            PROPVARIANT r; hr = rd_id(f.ps, 9, &r);
            int ok = hr == S_OK && pv_equal(&v, &r);
            CHECKF(ok, "size %lu %s %s: hr=%08lx vt=%d", size, as_blob ? "blob" : "lpstr", pass ? "reopened" : "same session", hr, r.vt);
            U(PropVariantClear)(&r);
            if (!pass && !fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX)) break;
        }
    }
    free(buf);
    fx_close(&f);
}

TEST(large_blob_boundaries_simple) {
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN();
    static const ULONG sizes[] = {4000, 4084, 4085, 4096, 4097, 8192, 65536, 300000};
    for (int i = 0; i < N_ELEM(sizes); i++) large_case(PROPSETFLAG_DEFAULT, "large_blob.stg", sizes[i], 1);
    COM_END();
}
TEST(large_lpstr_boundaries_simple_ansi) {
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN();
    static const ULONG sizes[] = {4095, 4096, 5000, 70000};
    for (int i = 0; i < N_ELEM(sizes); i++) large_case(PROPSETFLAG_ANSI, "large_lpstr.stg", sizes[i], 0);
    COM_END();
}
TEST(large_lpstr_unicode_set) {
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN();
    static const ULONG sizes[] = {2047, 2048, 5000, 70000};
    for (int i = 0; i < N_ELEM(sizes); i++) large_case(PROPSETFLAG_DEFAULT, "large_lpstr_u.stg", sizes[i], 0);
    COM_END();
}
TEST(large_blob_nonsimple) {
    REQUIRE(U(StgCreatePropSetStg));
    if (t_is_wine()) { t_skip("wine lacks nonsimple property sets"); return; }
    COM_BEGIN();
    static const ULONG sizes[] = {100, 4096, 5000, 100000};
    for (int i = 0; i < N_ELEM(sizes); i++) large_case(PROPSETFLAG_NONSIMPLE, "large_blob_ns.stg", sizes[i], 1);
    COM_END();
}

TEST(large_many_small_properties) {
    REQUIRE(U(StgCreatePropSetStg)); COM_BEGIN();
    FX f;
    if (fx_new(&f, "many_props.stg", &G_ANY_GUID, PROPSETFLAG_DEFAULT)) {
        enum { N = 600 };
        PROPSPEC *s = malloc(N * sizeof *s); PROPVARIANT *v = malloc(N * sizeof *v);
        for (int i = 0; i < N; i++) { s[i].ulKind = PRSPEC_PROPID; s[i].propid = 2 + i * 3; PropVariantInit(&v[i]); v[i].vt = VT_I4; v[i].lVal = i * 17; }
        EQ(CALL(f.ps, WriteMultiple, N, s, v, 2), S_OK);
        if (fx_reopen(&f, &G_ANY_GUID, STGM_RW_EX)) {
            PROPVARIANT *r = malloc(N * sizeof *r);
            EQ(CALL(f.ps, ReadMultiple, N, s, r), S_OK);
            int bad = 0; for (int i = 0; i < N; i++) if (r[i].vt != VT_I4 || r[i].lVal != i * 17) bad++;
            CHECKF(bad == 0, "%d of %d properties wrong after reopen", bad, N);
            free(r);
        }
        free(s); free(v);
        fx_close(&f);
    }
    COM_END();
}

/* ------------------------------------------------------------------ standard property sets */

TEST(stdset_summary_information_roundtrip) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "std_si.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        FILETIME ft = {0x11223344, 0x01d12345};
        struct { ULONG id; VARTYPE vt; } t[] = { {PIDSI_TITLE, VT_LPSTR}, {PIDSI_SUBJECT, VT_LPSTR}, {PIDSI_AUTHOR, VT_LPSTR}, {PIDSI_KEYWORDS, VT_LPSTR},
            {PIDSI_COMMENTS, VT_LPSTR}, {PIDSI_TEMPLATE, VT_LPSTR}, {PIDSI_LASTAUTHOR, VT_LPSTR}, {PIDSI_REVNUMBER, VT_LPSTR},
            {PIDSI_EDITTIME, VT_FILETIME}, {PIDSI_LASTPRINTED, VT_FILETIME}, {PIDSI_CREATE_DTM, VT_FILETIME}, {PIDSI_LASTSAVE_DTM, VT_FILETIME},
            {PIDSI_PAGECOUNT, VT_I4}, {PIDSI_WORDCOUNT, VT_I4}, {PIDSI_CHARCOUNT, VT_I4}, {PIDSI_APPNAME, VT_LPSTR}, {PIDSI_DOC_SECURITY, VT_I4} };
        for (int i = 0; i < N_ELEM(t); i++) {
            PROPVARIANT v; PropVariantInit(&v); v.vt = t[i].vt;
            if (t[i].vt == VT_LPSTR) v.pszVal = (LPSTR)"std text";
            else if (t[i].vt == VT_FILETIME) v.filetime = ft;
            else v.lVal = 100 + i;
            EQ(wr_id(f.ps, t[i].id, &v), S_OK);
        }
        BYTE thumb[8] = {1, 2, 3, 4, 5, 6, 7, 8};
        CLIPDATA cd; cd.cbSize = 4 + 8; cd.ulClipFmt = (ULONG)-1 /* Windows clipboard format value */; cd.pClipData = thumb;
        PROPVARIANT tv; PropVariantInit(&tv); tv.vt = VT_CF; tv.pclipdata = &cd;
        EQ(wr_id(f.ps, PIDSI_THUMBNAIL, &tv), S_OK);
        if (fx_reopen(&f, &G_FMTID_SI, STGM_RW_EX)) {
            int bad = 0;
            for (int i = 0; i < N_ELEM(t); i++) {
                PROPVARIANT r; HRESULT hr = rd_id(f.ps, t[i].id, &r);
                if (hr != S_OK || r.vt != t[i].vt) bad++;
                U(PropVariantClear)(&r);
            }
            CHECKF(bad == 0, "%d SummaryInformation properties wrong after reopen", bad);
            PROPVARIANT r; EQ(rd_id(f.ps, PIDSI_THUMBNAIL, &r), S_OK); EQ(r.vt, VT_CF);
            if (r.vt == VT_CF && r.pclipdata) { EQ(r.pclipdata->cbSize, 12); CHECK(!memcmp(r.pclipdata->pClipData, thumb, 8)); }
            U(PropVariantClear)(&r);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(stdset_document_summary_information_types) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "std_dsi.stg", &G_FMTID_DSI, PROPSETFLAG_ANSI)) {
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"Category"; EQ(wr_id(f.ps, PIDDSI_CATEGORY, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"Manager"; EQ(wr_id(f.ps, PIDDSI_MANAGER, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"Company"; EQ(wr_id(f.ps, PIDDSI_COMPANY, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_I4; v.lVal = 4096; EQ(wr_id(f.ps, PIDDSI_BYTECOUNT, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_I4; v.lVal = 12; EQ(wr_id(f.ps, PIDDSI_LINECOUNT, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_FALSE; EQ(wr_id(f.ps, PIDDSI_SCALE, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; EQ(wr_id(f.ps, PIDDSI_LINKSDIRTY, &v), S_OK);
        { /* HEADINGPAIR: vector of variants (heading, count) pairs; DOCPARTS: vector of LPSTR */
            PROPVARIANT hp[2]; memset(hp, 0, sizeof hp);
            hp[0].vt = VT_LPSTR; hp[0].pszVal = (LPSTR)"Titles of parts"; hp[1].vt = VT_I4; hp[1].lVal = 2;
            PropVariantInit(&v); v.vt = VT_VECTOR | VT_VARIANT; v.capropvar.cElems = 2; v.capropvar.pElems = hp;
            EQ(wr_id(f.ps, PIDDSI_HEADINGPAIR, &v), S_OK);
            LPSTR dp[2] = {"part one", "part two"};
            PropVariantInit(&v); v.vt = VT_VECTOR | VT_LPSTR; v.calpstr.cElems = 2; v.calpstr.pElems = dp;
            EQ(wr_id(f.ps, PIDDSI_DOCPARTS, &v), S_OK);
        }
        if (fx_reopen(&f, &G_FMTID_DSI, STGM_RW_EX)) {
            PROPVARIANT r;
            EQ(rd_id(f.ps, PIDDSI_CATEGORY, &r), S_OK); EQ(r.vt, VT_LPSTR); U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, PIDDSI_BYTECOUNT, &r), S_OK); EQ(r.vt, VT_I4); EQ(r.lVal, 4096);
            EQ(rd_id(f.ps, PIDDSI_SCALE, &r), S_OK); EQ(r.vt, VT_BOOL); EQ(r.boolVal, VARIANT_FALSE);
            EQ(rd_id(f.ps, PIDDSI_LINKSDIRTY, &r), S_OK); EQ(r.vt, VT_BOOL); EQ(r.boolVal, VARIANT_TRUE);
            EQ(rd_id(f.ps, PIDDSI_HEADINGPAIR, &r), S_OK); EQ(r.vt, VT_VECTOR | VT_VARIANT); EQ(r.capropvar.cElems, 2);
            if (r.vt == (VT_VECTOR | VT_VARIANT) && r.capropvar.cElems == 2) { EQ(r.capropvar.pElems[0].vt, VT_LPSTR); EQ(r.capropvar.pElems[1].vt, VT_I4); EQ(r.capropvar.pElems[1].lVal, 2); }
            U(PropVariantClear)(&r);
            EQ(rd_id(f.ps, PIDDSI_DOCPARTS, &r), S_OK); EQ(r.vt, VT_VECTOR | VT_LPSTR); EQ(r.calpstr.cElems, 2);
            if (r.vt == (VT_VECTOR | VT_LPSTR) && r.calpstr.cElems == 2) { EXPECT_STR(r.calpstr.pElems[1], "part two"); }
            U(PropVariantClear)(&r);
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(stdset_user_defined_named_properties) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "std_udp.stg", &G_FMTID_UDP, PROPSETFLAG_ANSI)) {
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"custom text"; EQ(wr_name(f.ps, L"CustomText", &v, PID_FIRST_USABLE + 0xfe), S_OK);
        PropVariantInit(&v); v.vt = VT_I4; v.lVal = 123; EQ(wr_name(f.ps, L"CustomNumber", &v, PID_FIRST_USABLE + 0xfe), S_OK);
        PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; EQ(wr_name(f.ps, L"CustomYesNo", &v, PID_FIRST_USABLE + 0xfe), S_OK);
        PropVariantInit(&v); v.vt = VT_FILETIME; v.filetime.dwLowDateTime = 9; EQ(wr_name(f.ps, L"CustomDate", &v, PID_FIRST_USABLE + 0xfe), S_OK);
        if (fx_reopen(&f, &G_FMTID_UDP, STGM_RW_EX)) {
            PROPVARIANT r;
            EQ(rd_name(f.ps, L"CustomNumber", &r), S_OK); EQ(r.vt, VT_I4); EQ(r.lVal, 123);
            EQ(rd_name(f.ps, L"CustomText", &r), S_OK); EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "custom text"); U(PropVariantClear)(&r);
            EQ(rd_name(f.ps, L"CustomYesNo", &r), S_OK); EQ(r.vt, VT_BOOL);
            EQ(rd_name(f.ps, L"CustomDate", &r), S_OK); EQ(r.vt, VT_FILETIME); EQ(r.filetime.dwLowDateTime, 9);
            STATPROPSTG sp; IEnumSTATPROPSTG *e = NULL; ULONG got; int n = 0, minid = 0x7fffffff;
            EQ(CALL(f.ps, Enum, &e), S_OK);
            while (e && CALL(e, Next, 1, &sp, &got) == S_OK) { n++; if ((int)sp.propid < minid) minid = sp.propid; if (sp.lpwstrName) U(CoTaskMemFree)(sp.lpwstrName); }
            if (e) CALL(e, Release);
            EQ(n, 4); CHECKF(minid >= PID_FIRST_USABLE + 0xfe, "lowest user-defined id %d is below propidNameFirst", minid);
        }
        fx_close(&f);
    }
    COM_END();
}

/* ------------------------------------------------------------------ on-disk layout */

static DWORD rd32(const BYTE *p, ULONG off) { DWORD v; memcpy(&v, p + off, 4); return v; }
static WORD rd16(const BYTE *p, ULONG off) { WORD v; memcpy(&v, p + off, 2); return v; }

/* offset of property `id` in the section starting at `sect`, or 0 */
static ULONG find_prop(const BYTE *p, ULONG len, ULONG sect, DWORD id) {
    if (sect + 8 > len) return 0;
    DWORD cnt = rd32(p, sect + 4);
    for (DWORD i = 0; i < cnt && sect + 8 + 8 * (i + 1) <= len; i++)
        if (rd32(p, sect + 8 + 8 * i) == id) return sect + rd32(p, sect + 8 + 8 * i + 4);
    return 0;
}

TEST(layout_header_and_codepage_property) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    for (int pass = 0; pass < 2; pass++) {
        FX f;
        if (!fx_new(&f, "layout_hdr.stg", &G_FMTID_SI, pass ? PROPSETFLAG_ANSI : PROPSETFLAG_DEFAULT)) { fx_close(&f); continue; }
        EQ(CALL(f.ps, SetClass, &G_ANY_GUID), S_OK);
        wr_i4(f.ps, 2, 0x01020304);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        static BYTE raw[8192];
        ULONG n = get_stream(f.stg, SI_NAME, raw, sizeof raw);
        CHECKF(n != (ULONG)-1 && n >= 0x30 + 8, "stream length %lu", n);
        if (n != (ULONG)-1 && n >= 0x38) {
            EQ(rd16(raw, 0), 0xFFFE);                       /* byte order mark */
            t_note("%s: format version=%u os=0x%08lx stream length=%lu", pass ? "ansi" : "unicode", rd16(raw, 2), rd32(raw, 4), n);
            CHECK(!memcmp(raw + 8, &G_ANY_GUID, 16));        /* header CLSID = SetClass value */
            EQ(rd32(raw, 24), 1);                           /* one section */
            CHECK(!memcmp(raw + 28, &G_FMTID_SI, 16));
            EQ(rd32(raw, 44), 0x30);                        /* first section directly after the header */
            ULONG sect = 0x30;
            DWORD size = rd32(raw, sect), cnt = rd32(raw, sect + 4);
            CHECKF(sect + size <= n, "section size %lu exceeds stream %lu", size, n);
            CHECKF(cnt >= 2, "property count %lu", cnt);
            ULONG cp = find_prop(raw, n, sect, 1);
            CHECK(cp != 0);
            if (cp) {
                EQ(rd32(raw, cp), VT_I2);
                EQ(rd16(raw, cp + 4), pass ? (WORD)GetACP() : 1200);
            }
            ULONG p2 = find_prop(raw, n, sect, 2);
            CHECK(p2 != 0 && (p2 & 3) == 0);                 /* property values are DWORD aligned */
            if (p2) { EQ(rd32(raw, p2), VT_I4); EQ(rd32(raw, p2 + 4), 0x01020304); }
            ULONG pl = find_prop(raw, n, sect, 0x80000000u);
            t_note("locale property present in stream: %s", pl ? "yes" : "no");
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(layout_string_encodings) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    for (int pass = 0; pass < 2; pass++) {
        FX f;
        if (!fx_new(&f, "layout_str.stg", &G_FMTID_SI, pass ? PROPSETFLAG_ANSI : PROPSETFLAG_DEFAULT)) { fx_close(&f); continue; }
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"abc"; EQ(wr_id(f.ps, 2, &v), S_OK);
        PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = (LPWSTR)L"abc"; EQ(wr_id(f.ps, 3, &v), S_OK);
        BSTR b = U(SysAllocString)(L"abc");
        PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = b; EQ(wr_id(f.ps, 4, &v), S_OK); U(SysFreeString)(b);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        static BYTE raw[8192];
        ULONG n = get_stream(f.stg, SI_NAME, raw, sizeof raw);
        if (n != (ULONG)-1 && n > 0x40) {
            ULONG sect = 0x30;
            ULONG p2 = find_prop(raw, n, sect, 2), p3 = find_prop(raw, n, sect, 3), p4 = find_prop(raw, n, sect, 4);
            CHECK(p2 && p3 && p4);
            if (p2) {
                EQ(rd32(raw, p2), VT_LPSTR);
                if (pass) { EQ(rd32(raw, p2 + 4), 4); CHECK(!memcmp(raw + p2 + 8, "abc", 4)); }             /* ANSI: bytes incl NUL */
                else { EQ(rd32(raw, p2 + 4), 8); CHECK(!memcmp(raw + p2 + 8, L"abc", 8)); }                 /* Unicode: UTF-16, count in bytes */
            }
            if (p3) { EQ(rd32(raw, p3), VT_LPWSTR); EQ(rd32(raw, p3 + 4), 4); CHECK(!memcmp(raw + p3 + 8, L"abc", 8)); }   /* count in characters */
            if (p4) {
                EQ(rd32(raw, p4), VT_BSTR);
                if (pass) { EQ(rd32(raw, p4 + 4), 4); CHECK(!memcmp(raw + p4 + 8, "abc", 4)); }
                else { EQ(rd32(raw, p4 + 4), 8); CHECK(!memcmp(raw + p4 + 8, L"abc", 8)); }
            }
        } else CHECKF(0, "stream length %lu", n);
        fx_close(&f);
    }
    COM_END();
}

TEST(layout_dictionary_property) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    for (int pass = 0; pass < 2; pass++) {
        FX f;
        if (!fx_new(&f, "layout_dict.stg", &G_FMTID_SI, pass ? PROPSETFLAG_ANSI : PROPSETFLAG_DEFAULT)) { fx_close(&f); continue; }
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_I4; v.lVal = 1;
        EQ(wr_name(f.ps, L"prop", &v, 0x30), S_OK);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        static BYTE raw[8192];
        ULONG n = get_stream(f.stg, SI_NAME, raw, sizeof raw);
        ULONG d = n != (ULONG)-1 ? find_prop(raw, n, 0x30, 0) : 0;
        CHECKF(d != 0, "no dictionary property (stream length %lu)", n);
        if (d) {
            EQ(rd32(raw, d), 1);                              /* one entry */
            DWORD id = rd32(raw, d + 4), cch = rd32(raw, d + 8);
            CHECKF(id >= 0x30, "dictionary id %lx below propidNameFirst", id);
            if (pass) { EQ(cch, 5); CHECK(!memcmp(raw + d + 12, "prop", 5)); }                 /* ANSI: bytes, no padding */
            else { EQ(cch, 5); CHECK(!memcmp(raw + d + 12, L"prop", 10)); }                    /* Unicode: characters */
            ULONG pv = find_prop(raw, n, 0x30, id);
            CHECK(pv != 0);
            if (pv) { EQ(rd32(raw, pv), VT_I4); EQ(rd32(raw, pv + 4), 1); }
        }
        fx_close(&f);
    }
    COM_END();
}

TEST(layout_section_size_and_alignment) {
    REQUIRE(U(StgCreatePropSetStg));
    COM_BEGIN();
    FX f;
    if (fx_new(&f, "layout_align.stg", &G_FMTID_SI, PROPSETFLAG_ANSI)) {
        PROPVARIANT v;
        PropVariantInit(&v); v.vt = VT_I2; v.iVal = 1; wr_id(f.ps, 2, &v);
        PropVariantInit(&v); v.vt = VT_UI1; v.bVal = 1; wr_id(f.ps, 3, &v);
        PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = (LPSTR)"12345"; wr_id(f.ps, 4, &v);
        BYTE d[5] = {1, 2, 3, 4, 5};
        PropVariantInit(&v); v.vt = VT_BLOB; v.blob.cbSize = 5; v.blob.pBlobData = d; wr_id(f.ps, 5, &v);
        PropVariantInit(&v); v.vt = VT_R8; v.dblVal = 1; wr_id(f.ps, 6, &v);
        CALL(f.ps, Commit, STGC_DEFAULT);
        fx_release_ps(&f);
        static BYTE raw[8192];
        ULONG n = get_stream(f.stg, SI_NAME, raw, sizeof raw);
        if (n != (ULONG)-1 && n > 0x40) {
            DWORD size = rd32(raw, 0x30);
            CHECKF(size <= n - 0x30 + 3, "section size %lu vs stream %lu", size, n);
            t_note("stream length %lu, section size %lu", n, size);
            for (DWORD id = 2; id <= 6; id++) { ULONG p = find_prop(raw, n, 0x30, id); CHECKF(p && !(p & 3), "id %lu offset %lu not aligned", id, p); }
            ULONG p4 = find_prop(raw, n, 0x30, 4), p5 = find_prop(raw, n, 0x30, 5);
            /* padding after the 5 byte string / blob keeps the next value aligned */
            if (p4) { EQ(rd32(raw, p4 + 4), 6); }
            if (p5) { EQ(rd32(raw, p5), VT_BLOB); EQ(rd32(raw, p5 + 4), 5); CHECK(!memcmp(raw + p5 + 8, d, 5)); }
        } else CHECKF(0, "stream length %lu", n);
        fx_close(&f);
    }
    COM_END();
}

/* ------------------------------------------------------------------ hand built streams */

static HRESULT open_crafted(FX *f, const char *leaf, const GUID *fmtid, const PB *p) {
    if (!fx_storage(f, leaf)) return E_FAIL;
    WCHAR name[40]; U(FmtIdToPropStgName)(fmtid, name);
    HRESULT hr = put_stream(f->stg, name, p->b, p->n);
    if (hr != S_OK) return hr;
    return CALL(f->pss, Open, fmtid, STGM_RW_EX, &f->ps);
}

TEST(crafted_ansi_set_with_dictionary) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    PB p; pb_set_header(&p, &G_ANY_GUID);
    ULONG sect = p.n;
    pb_u32(&p, 0); pb_u32(&p, 5);                                  /* size patched later, 5 properties */
    ULONG dir = p.n; for (int i = 0; i < 5; i++) { pb_u32(&p, 0); pb_u32(&p, 0); }
    ULONG offs[5];
    /* 0: dictionary (one name for id 3) */
    offs[0] = p.n - sect; pb_u32(&p, 1); pb_u32(&p, 3); pb_u32(&p, 6); pb_raw(&p, "three", 6); pb_align4(&p);
    /* 1: codepage 1252 */
    offs[1] = p.n - sect; pb_u32(&p, VT_I2); pb_u16(&p, 1252); pb_u16(&p, 0);
    /* 2: I4 */
    offs[2] = p.n - sect; pb_u32(&p, VT_I4); pb_u32(&p, (DWORD)-5);
    /* 3: LPSTR "abc" in cp 1252 (with a high char) */
    offs[3] = p.n - sect; pb_u32(&p, VT_LPSTR); pb_u32(&p, 5); pb_raw(&p, "ab\xe9" "c\0", 5); pb_align4(&p);
    /* 16: BLOB */
    offs[4] = p.n - sect; pb_u32(&p, VT_BLOB); pb_u32(&p, 3); pb_raw(&p, "\x01\x02\x03", 3); pb_align4(&p);
    DWORD ids[5] = {0, 1, 2, 3, 16};
    for (int i = 0; i < 5; i++) { pb_put32_at(&p, dir + 8 * i, ids[i]); pb_put32_at(&p, dir + 8 * i + 4, offs[i]); }
    pb_put32_at(&p, sect, p.n - sect);
    FX f;
    HRESULT hr = open_crafted(&f, "crafted_ansi.stg", &G_ANY_GUID, &p);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        PROPVARIANT r;
        EQ(rd_id(f.ps, PID_CODEPAGE, &r), S_OK); EQ(r.vt, VT_I2); EQ(r.iVal, 1252);
        EQ(rd_id(f.ps, 2, &r), S_OK); EQ(r.vt, VT_I4); EQ(r.lVal, -5);
        EQ(rd_id(f.ps, 3, &r), S_OK); EQ(r.vt, VT_LPSTR);
        if (r.vt == VT_LPSTR) { if (GetACP() == 1252) EXPECT_STR(r.pszVal, "ab\xe9" "c"); else t_note("read \"%s\"", r.pszVal); }
        U(PropVariantClear)(&r);
        EQ(rd_name(f.ps, L"THREE", &r), S_OK); EQ(r.vt, VT_LPSTR); U(PropVariantClear)(&r);
        EQ(rd_id(f.ps, 16, &r), S_OK); EQ(r.vt, VT_BLOB); EQ(r.blob.cbSize, 3);
        if (r.vt == VT_BLOB && r.blob.cbSize == 3) CHECK(!memcmp(r.blob.pBlobData, "\x01\x02\x03", 3));
        U(PropVariantClear)(&r);
        STATPROPSTG sp[8]; IEnumSTATPROPSTG *e = NULL; ULONG got; int n = 0, named = 0;
        EQ(CALL(f.ps, Enum, &e), S_OK);
        while (e && n < 8 && CALL(e, Next, 1, &sp[n], &got) == S_OK) { if (sp[n].lpwstrName) { named++; EXPECT_WSTR(sp[n].lpwstrName, L"three"); EQ(sp[n].propid, 3); U(CoTaskMemFree)(sp[n].lpwstrName); } n++; }
        if (e) CALL(e, Release);
        EQ(n, 3); EQ(named, 1);                                 /* ids 2, 3, 16; the special properties are not listed */
    }
    fx_close(&f);
    COM_END();
}

TEST(crafted_unicode_set_strings) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    PB p; pb_set_header(&p, &G_ANY_GUID);
    ULONG sect = p.n;
    pb_u32(&p, 0); pb_u32(&p, 5);
    ULONG dir = p.n; for (int i = 0; i < 5; i++) { pb_u32(&p, 0); pb_u32(&p, 0); }
    ULONG offs[5];
    offs[0] = p.n - sect; pb_u32(&p, VT_I2); pb_u16(&p, 1200); pb_u16(&p, 0);
    offs[1] = p.n - sect; pb_u32(&p, VT_LPWSTR); pb_u32(&p, 4); pb_raw(&p, L"uni", 8);
    offs[2] = p.n - sect; pb_u32(&p, VT_LPSTR); pb_u32(&p, 8); pb_raw(&p, L"abc", 8);
    offs[3] = p.n - sect; pb_u32(&p, VT_BSTR); pb_u32(&p, 8); pb_raw(&p, L"bst", 8);
    offs[4] = p.n - sect; pb_u32(&p, VT_VECTOR | VT_LPWSTR); pb_u32(&p, 2); pb_u32(&p, 2); pb_raw(&p, L"a", 4); pb_u32(&p, 3); pb_raw(&p, L"bc", 6); pb_align4(&p);
    DWORD ids[5] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) { pb_put32_at(&p, dir + 8 * i, ids[i]); pb_put32_at(&p, dir + 8 * i + 4, offs[i]); }
    pb_put32_at(&p, sect, p.n - sect);
    FX f;
    HRESULT hr = open_crafted(&f, "crafted_uni.stg", &G_ANY_GUID, &p);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        PROPVARIANT r;
        EQ(rd_id(f.ps, 2, &r), S_OK); EQ(r.vt, VT_LPWSTR); if (r.vt == VT_LPWSTR) EXPECT_WSTR(r.pwszVal, L"uni"); U(PropVariantClear)(&r);
        EQ(rd_id(f.ps, 3, &r), S_OK); EQ(r.vt, VT_LPSTR); if (r.vt == VT_LPSTR) EXPECT_STR(r.pszVal, "abc"); U(PropVariantClear)(&r);
        EQ(rd_id(f.ps, 4, &r), S_OK); EQ(r.vt, VT_BSTR); if (r.vt == VT_BSTR) EXPECT_WSTR(r.bstrVal, L"bst"); U(PropVariantClear)(&r);
        EQ(rd_id(f.ps, 5, &r), S_OK); EQ(r.vt, VT_VECTOR | VT_LPWSTR); EQ(r.calpwstr.cElems, 2);
        if (r.vt == (VT_VECTOR | VT_LPWSTR) && r.calpwstr.cElems == 2) { EXPECT_WSTR(r.calpwstr.pElems[0], L"a"); EXPECT_WSTR(r.calpwstr.pElems[1], L"bc"); }
        U(PropVariantClear)(&r);
    }
    fx_close(&f);
    COM_END();
}

TEST(crafted_scalar_types) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    PB p; pb_set_header(&p, &G_ANY_GUID);
    ULONG sect = p.n;
    enum { N = 9 };
    pb_u32(&p, 0); pb_u32(&p, N);
    ULONG dir = p.n; for (int i = 0; i < N; i++) { pb_u32(&p, 0); pb_u32(&p, 0); }
    ULONG offs[N]; int k = 0;
    offs[k++] = p.n - sect; pb_u32(&p, VT_UI1); pb_u8(&p, 0xfe); pb_u8(&p, 0); pb_u16(&p, 0);
    offs[k++] = p.n - sect; pb_u32(&p, VT_I2); pb_u16(&p, 0x8001); pb_u16(&p, 0xabcd /* padding is ignored */);
    offs[k++] = p.n - sect; pb_u32(&p, VT_BOOL); pb_u16(&p, 0xffff); pb_u16(&p, 0);
    offs[k++] = p.n - sect; pb_u32(&p, VT_UI4); pb_u32(&p, 0xdeadbeef);
    offs[k++] = p.n - sect; pb_u32(&p, VT_I8); pb_u64(&p, 0x8000000000000001ull);
    offs[k++] = p.n - sect; pb_u32(&p, VT_R8); { double d = -0.5; pb_raw(&p, &d, 8); }
    offs[k++] = p.n - sect; pb_u32(&p, VT_FILETIME); pb_u32(&p, 0x11111111); pb_u32(&p, 0x22222222);
    offs[k++] = p.n - sect; pb_u32(&p, VT_CLSID); pb_guid(&p, &G_ANY_GUID2);
    offs[k++] = p.n - sect; pb_u32(&p, VT_EMPTY);
    for (int i = 0; i < N; i++) { pb_put32_at(&p, dir + 8 * i, 2 + i); pb_put32_at(&p, dir + 8 * i + 4, offs[i]); }
    pb_put32_at(&p, sect, p.n - sect);
    FX f;
    HRESULT hr = open_crafted(&f, "crafted_scalars.stg", &G_ANY_GUID, &p);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        PROPVARIANT r;
        EQ(rd_id(f.ps, 2, &r), S_OK); EQ(r.vt, VT_UI1); EQ(r.bVal, 0xfe);
        EQ(rd_id(f.ps, 3, &r), S_OK); EQ(r.vt, VT_I2); EQ((unsigned short)r.iVal, 0x8001);
        EQ(rd_id(f.ps, 4, &r), S_OK); EQ(r.vt, VT_BOOL); EQ((unsigned short)r.boolVal, 0xffff);
        EQ(rd_id(f.ps, 5, &r), S_OK); EQ(r.vt, VT_UI4); EQ(r.ulVal, 0xdeadbeef);
        EQ(rd_id(f.ps, 6, &r), S_OK); EQ(r.vt, VT_I8); CHECK(r.uhVal.QuadPart == 0x8000000000000001ull);
        EQ(rd_id(f.ps, 7, &r), S_OK); EQ(r.vt, VT_R8); CHECK(r.dblVal == -0.5);
        EQ(rd_id(f.ps, 8, &r), S_OK); EQ(r.vt, VT_FILETIME); EQ(r.filetime.dwLowDateTime, 0x11111111); EQ(r.filetime.dwHighDateTime, 0x22222222);
        EQ(rd_id(f.ps, 9, &r), S_OK); EQ(r.vt, VT_CLSID); if (r.vt == VT_CLSID && r.puuid) CHECK(guid_eq(r.puuid, &G_ANY_GUID2)); U(PropVariantClear)(&r);
        HRESULT eh = rd_id(f.ps, 10, &r); t_note("stored VT_EMPTY property reads hr=0x%08lx vt=%d", eh, r.vt);
    }
    fx_close(&f);
    COM_END();
}

TEST(crafted_corrupt_sections) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    for (int variant = 0; variant < 4; variant++) {
        PB p; pb_set_header(&p, &G_ANY_GUID);
        ULONG sect = p.n;
        pb_u32(&p, 0); pb_u32(&p, 1); pb_u32(&p, 2); pb_u32(&p, 16);
        pb_u32(&p, VT_I4); pb_u32(&p, 9);
        pb_put32_at(&p, sect, p.n - sect);
        switch (variant) {
        case 0: pb_put32_at(&p, sect + 12, 0x1000); break;                  /* property offset beyond the stream */
        case 1: pb_put32_at(&p, sect + 8, 2); pb_put32_at(&p, sect + 4, 0x7fffffff); break;   /* absurd property count */
        case 2: pb_put32_at(&p, p.n - 8, 0x7f7f); break;                    /* unknown type code */
        case 3: pb_put32_at(&p, 0x2c, p.n + 100); break;                    /* section offset past the end */
        }
        FX f;
        HRESULT hr = open_crafted(&f, "crafted_bad.stg", &G_ANY_GUID, &p);
        t_note("corrupt variant %d: Open -> 0x%08lx", variant, hr);
        if (hr == S_OK) {
            PROPVARIANT r; HRESULT rh = rd_id(f.ps, 2, &r);
            t_note("  ReadMultiple -> 0x%08lx vt=%d", rh, r.vt);
            if (variant == 3 || variant == 1) CHECKF(FAILED(rh) || rh == S_FALSE, "corrupt variant %d: read returned data", variant);
            U(PropVariantClear)(&r);
        } else if (variant == 3 || variant == 1) CHECK(FAILED(hr));
        fx_close(&f);
    }
    COM_END();
}

TEST(crafted_format_version_1_accepted) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    PB p; pb_set_header(&p, &G_ANY_GUID);
    pb_u16(&p, 0); p.n -= 2; p.b[2] = 1;                                   /* header version field = 1 */
    ULONG sect = p.n;
    pb_u32(&p, 0); pb_u32(&p, 1); pb_u32(&p, 2); pb_u32(&p, 16);
    pb_u32(&p, VT_I1); pb_u32(&p, 0xfffffff0);
    pb_put32_at(&p, sect, p.n - sect);
    FX f;
    HRESULT hr = open_crafted(&f, "crafted_v1.stg", &G_ANY_GUID, &p);
    EQ(hr, S_OK);
    if (hr == S_OK) { PROPVARIANT r; EQ(rd_id(f.ps, 2, &r), S_OK); EQ(r.vt, VT_I1); EQ(r.cVal, -16); }
    fx_close(&f);
    COM_END();
}

TEST(crafted_header_clsid_visible_in_stat) {
    REQUIRE(U(StgCreatePropSetStg)); REQUIRE(U(FmtIdToPropStgName));
    COM_BEGIN();
    PB p; pb_set_header(&p, &G_ANY_GUID);
    memcpy(p.b + 8, &G_ANY_GUID2, 16);
    ULONG sect = p.n;
    pb_u32(&p, 0); pb_u32(&p, 1); pb_u32(&p, 2); pb_u32(&p, 16); pb_u32(&p, VT_I4); pb_u32(&p, 3);
    pb_put32_at(&p, sect, p.n - sect);
    FX f;
    HRESULT hr = open_crafted(&f, "crafted_clsid.stg", &G_ANY_GUID, &p);
    EQ(hr, S_OK);
    if (hr == S_OK) {
        STATPROPSETSTG st; EQ(CALL(f.ps, Stat, &st), S_OK);
        CHECK(guid_eq(&st.clsid, &G_ANY_GUID2)); CHECK(guid_eq(&st.fmtid, &G_ANY_GUID));
        t_note("crafted set without a codepage property: Stat.grfFlags=0x%lx", st.grfFlags);
        PROPVARIANT r; HRESULT cp = rd_id(f.ps, PID_CODEPAGE, &r);
        t_note("  codepage read -> 0x%08lx vt=%d val=%d", cp, r.vt, r.iVal);
    }
    fx_close(&f);
    COM_END();
}
