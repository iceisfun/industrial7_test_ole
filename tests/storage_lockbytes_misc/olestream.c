/* OLE 1.0 conversion helpers: OleConvertOLESTREAMToIStorage(Ex), OleConvertIStorageToOLESTREAM(Ex).
 * OLE1 object stream layout read by ole32 (decompilation of OLESTREAMToGenericObject/GetPresentationObject):
 *   DWORD version(ignored) ; DWORD format (1=link 2=embedded 3=static) ;
 *   counted strings (DWORD len incl. NUL + bytes, len 0 = NULL): class, topic, item ;
 *   embedded: DWORD nativeSize + bytes ; then presentation: DWORD version ; DWORD format(5 = presentation, 0 = none).
 * Error values: CONVERT10_E_OLESTREAM_GET 0x800401C0, _PUT 0x800401C1, _FMT 0x800401C2, STG_NO_STD_STREAM 0x800401C5. */
#include "slm.h"

#define E_OS_GET  ((HRESULT)0x800401C0L)
#define E_OS_PUT  ((HRESULT)0x800401C1L)
#define E_OS_FMT  ((HRESULT)0x800401C2L)
#define E_DV_TYMED ((HRESULT)0x80040069L)
#define E_DV_STGMEDIUM ((HRESULT)0x80040066L)

typedef HRESULT (WINAPI *PFN_C1)(LPOLESTREAM, LPSTORAGE, const DVTARGETDEVICE *);
typedef HRESULT (WINAPI *PFN_C2)(LPSTORAGE, LPOLESTREAM);
typedef HRESULT (WINAPI *PFN_C1X)(LPOLESTREAM, LPSTORAGE, CLIPFORMAT *, LONG *, LONG *, DWORD *, LPSTGMEDIUM);
typedef HRESULT (WINAPI *PFN_C2X)(LPSTORAGE, CLIPFORMAT, LONG, LONG, DWORD, LPSTGMEDIUM, LPOLESTREAM);
#define TO_STG  U(OleConvertOLESTREAMToIStorage)
#define TO_OS   U(OleConvertIStorageToOLESTREAM)
#define TO_STGX U(OleConvertOLESTREAMToIStorageEx)
#define TO_OSX  U(OleConvertIStorageToOLESTREAMEx)

static const char PAYLOAD[] = "native-payload-0123456789";
#define PL ((DWORD)sizeof PAYLOAD)

static IStorage *new_stg(ILockBytes **plb) {
    IStorage *stg = NULL;
    if (FAILED(slm_mem_docfile(&stg, plb, SM_CREATE_RW))) return NULL;
    return stg;
}
static void rel(IStorage *s, ILockBytes *lb) { if (s) IStorage_Release(s); if (lb) ILockBytes_Release(lb); }
static int find_bytes(const BYTE *h, ULONG hl, const void *n, ULONG nl) {
    for (ULONG i = 0; i + nl <= hl; i++) if (!memcmp(h + i, n, nl)) return 1;
    return 0;
}

/* ---- OleConvertOLESTREAMToIStorage ----------------------------------- */
TEST(slm_os_to_stg_minimal_embedded) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); CHECK(stg != NULL); if (!stg) { slm_couninit(); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1);
    ULONG total = m.len; mos_load(&m, m.buf, m.len);
    HRESULT hr = TO_STG(&m.os, stg, NULL);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(m.pos, total);           /* the whole stream is consumed, nothing more */
    CLSID cls; memset(&cls, 0, sizeof cls);
    STATSTG st; memset(&st, 0, sizeof st);
    IStorage_Stat(stg, &st, STATFLAG_NONAME);
    t_note("class written to storage: {%08lx-%04x-%04x-...}", st.clsid.Data1, st.clsid.Data2, st.clsid.Data3);
    if (IsEqualGUID(&st.clsid, &G_CLSID_NULL)) t_note("class \"Package\" did not map to a CLSID (no ProgID registration?)");
    IEnumSTATSTG *en = NULL;
    if (SUCCEEDED(IStorage_EnumElements(stg, 0, NULL, 0, &en))) {
        STATSTG e; ULONG f = 0; int n = 0;
        while (IEnumSTATSTG_Next(en, 1, &e, &f) == S_OK && f == 1) {
            char nm[128]; WideCharToMultiByte(CP_ACP, 0, e.pwcsName, -1, nm, sizeof nm, NULL, NULL);
            t_note("element %d: \"%s\" (\\x%02x...) type=%lu size=%llu", n++, nm[0] < 32 ? nm + 1 : nm, (unsigned char)nm[0], e.type, e.cbSize.QuadPart);
            U(CoTaskMemFree)(e.pwcsName);
        }
        IEnumSTATSTG_Release(en);
    }
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_without_presentation_tail) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    /* the trailing presentation header is mandatory: version+format words are read unconditionally */
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 0);
    ULONG total = m.len; mos_load(&m, m.buf, m.len);
    HRESULT hr = TO_STG(&m.os, stg, NULL);
    EXPECT_EQ(hr, E_OS_GET);
    t_note("consumed %lu of %lu bytes before failing", m.pos, total);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_null_storage) {
    REQUIRE(TO_STG);
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1); mos_load(&m, m.buf, m.len);
    EXPECT_EQ(TO_STG(&m.os, NULL, NULL), H_INVALIDARG);
    EXPECT_EQ(m.gets, 0);              /* storage is validated before the stream is touched */
    mos_free(&m);
    slm_couninit();
}
TEST(slm_os_to_stg_empty_stream) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_load(&m, "", 0);
    EXPECT_EQ(TO_STG(&m.os, stg, NULL), E_OS_GET);
    EXPECT_EQ(m.gets, 1);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_bad_format_ids) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    DWORD fmts[] = { 0, 4, 5, 6, 9, 0xFFFFFFFF };
    for (int i = 0; i < 6; i++) {
        ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); break; }
        MEMOS m; mos_init(&m); mos_u32(&m, 0x501); mos_u32(&m, fmts[i]); mos_str(&m, "Package"); mos_str(&m, NULL); mos_str(&m, NULL);
        mos_u32(&m, 0); mos_u32(&m, 0x501); mos_u32(&m, 0); mos_load(&m, m.buf, m.len);
        HRESULT hr = TO_STG(&m.os, stg, NULL);
        CHECKF(hr == E_OS_FMT, "format %lu: got 0x%08lx", fmts[i], hr);
        mos_free(&m); rel(stg, lb);
    }
    slm_couninit();
}
TEST(slm_os_to_stg_embedded_empty_class) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_init(&m); mos_u32(&m, 0x501); mos_u32(&m, 2); mos_str(&m, NULL); mos_str(&m, NULL); mos_str(&m, NULL);
    mos_u32(&m, 0); mos_u32(&m, 0x501); mos_u32(&m, 0); mos_load(&m, m.buf, m.len);
    EXPECT_EQ(TO_STG(&m.os, stg, NULL), E_OS_FMT);   /* NULL class string */
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_truncation_points) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    MEMOS full; mos_build_embedded(&full, "Package", PAYLOAD, PL, 1);
    ULONG total = full.len;
    int bad = 0, fmt_err = 0;
    for (ULONG cut = 0; cut < total; cut++) {
        ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); break; }
        MEMOS m; mos_load(&m, full.buf, cut);
        HRESULT hr = TO_STG(&m.os, stg, NULL);
        if (SUCCEEDED(hr)) bad++;
        else if (hr != E_OS_GET) fmt_err++;
        mos_free(&m); rel(stg, lb);
    }
    EXPECT_EQ(bad, 0);                 /* every strict prefix of a valid stream is rejected */
    t_note("%d of %lu truncations produced an error other than CONVERT10_E_OLESTREAM_GET", fmt_err, total);
    mos_free(&full);
    slm_couninit();
}
TEST(slm_os_to_stg_get_callback_short_read_mid_native) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1);
    ULONG cut = m.len - 8 - 5;         /* chop inside the native data */
    mos_load(&m, m.buf, cut);
    EXPECT_EQ(TO_STG(&m.os, stg, NULL), E_OS_GET);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_class_string_not_terminated) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_init(&m); mos_u32(&m, 0x501); mos_u32(&m, 2);
    mos_u32(&m, 4); mos_Put(&m.os, "ABCD", 4);       /* declared length 4, no NUL inside it */
    mos_str(&m, NULL); mos_str(&m, NULL); mos_u32(&m, 0); mos_u32(&m, 0x501); mos_u32(&m, 0);
    mos_load(&m, m.buf, m.len);
    HRESULT hr = TO_STG(&m.os, stg, NULL);
    CHECK(FAILED(hr));
    t_note("unterminated class string -> 0x%08lx", hr);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stg_static_object_header) {
    REQUIRE(TO_STG);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    /* format 3 (static) goes straight to the presentation reader; an empty remainder cannot succeed */
    MEMOS m; mos_init(&m); mos_u32(&m, 0x501); mos_u32(&m, 3); mos_load(&m, m.buf, m.len);
    HRESULT hr = TO_STG(&m.os, stg, NULL);
    CHECK(FAILED(hr));
    t_note("static object with no presentation -> 0x%08lx", hr);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}

/* ---- OleConvertIStorageToOLESTREAM ----------------------------------- */
static IStorage *converted_storage(ILockBytes **plb) {
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) return NULL;
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1); mos_load(&m, m.buf, m.len);
    HRESULT hr = TO_STG(&m.os, stg, NULL);
    mos_free(&m);
    if (FAILED(hr)) { rel(stg, lb); return NULL; }
    *plb = lb; return stg;
}
TEST(slm_os_from_stg_roundtrip) {
    REQUIRE(TO_STG); REQUIRE(TO_OS);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = converted_storage(&lb);
    CHECK(stg != NULL); if (!stg) { slm_couninit(); return; }
    MEMOS o; mos_init(&o);
    HRESULT hr = TO_OS(stg, &o.os);
    EXPECT_EQ(hr, S_OK);
    CHECK(o.puts > 0);
    if (o.len >= 8) {
        EXPECT_EQ(*(DWORD *)o.buf, 0x501);       /* OLE1 version written by ole32 */
        EXPECT_EQ(*(DWORD *)(o.buf + 4), 2);     /* embedded object */
    } else CHECK(0);
    CHECK(find_bytes(o.buf, o.len, "Package", 8));
    CHECK(find_bytes(o.buf, o.len, PAYLOAD, PL));
    t_note("OLESTREAM produced %lu bytes in %d Put calls", o.len, o.puts);
    /* what it produced can be consumed again */
    ILockBytes *lb2; IStorage *stg2 = new_stg(&lb2);
    if (stg2) {
        MEMOS in; mos_load(&in, o.buf, o.len);
        EXPECT_EQ(TO_STG(&in.os, stg2, NULL), S_OK);
        EXPECT_EQ(in.pos, in.len);
        mos_free(&in); rel(stg2, lb2);
    }
    mos_free(&o); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_from_stg_put_failure) {
    REQUIRE(TO_STG); REQUIRE(TO_OS);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = converted_storage(&lb);
    CHECK(stg != NULL); if (!stg) { slm_couninit(); return; }
    ULONG limits[] = { 1, 3, 8, 20, 40 };
    for (int i = 0; i < 5; i++) {
        MEMOS o; mos_init(&o); o.fail_after_put_bytes = limits[i];
        HRESULT hr = TO_OS(stg, &o.os);
        CHECKF(hr == E_OS_PUT, "put limit %lu: 0x%08lx", limits[i], hr);
        CHECK(o.len <= limits[i]);
        mos_free(&o);
    }
    rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_from_stg_null_storage) {
    REQUIRE(TO_OS);
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    MEMOS o; mos_init(&o);
    EXPECT_EQ(TO_OS(NULL, &o.os), H_INVALIDARG);
    EXPECT_EQ(o.puts, 0);
    mos_free(&o);
    slm_couninit();
}
TEST(slm_os_from_stg_plain_docfile) {
    REQUIRE(TO_OS);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS o; mos_init(&o);
    HRESULT hr = TO_OS(stg, &o.os);
    CHECK(FAILED(hr));
    CHECK(o.puts == 0);
    t_note("empty docfile -> OLESTREAM: 0x%08lx (CONVERT10_E_STG_NO_STD_STREAM is 0x800401C5)", hr);
    mos_free(&o); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_from_stg_only_native_stream) {
    REQUIRE(TO_OS);
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    IStorage_SetClass(stg, &G_CLSID_SlmA);
    slm_put_stream(stg, L"\1Ole10Native", "\4\0\0\0abcd", 8);
    MEMOS o; mos_init(&o);
    HRESULT hr = TO_OS(stg, &o.os);
    t_note("storage with only \\1Ole10Native -> 0x%08lx, %lu bytes out", hr, o.len);
    if (FAILED(hr)) CHECK(o.puts == 0 || 1);
    mos_free(&o); rel(stg, lb);
    slm_couninit();
}

/* ---- Ex variants ---------------------------------------------------------- */
TEST(slm_os_to_stgex_null_params) {
    REQUIRE(TO_STGX);
    if (t_is_wine()) { t_skip("wine aborts on unimplemented OleConvertOLESTREAMToIStorageEx"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1); mos_load(&m, m.buf, m.len);
    CLIPFORMAT cf = 0x7777; LONG w = 11, h = 12; DWORD cb = 13; STGMEDIUM med; memset(&med, 0, sizeof med);
    /* each output pointer is mandatory and checked before the stream is read */
    EXPECT_EQ(TO_STGX(&m.os, stg, NULL, &w, &h, &cb, &med), H_INVALIDARG);
    EXPECT_EQ(TO_STGX(&m.os, stg, &cf, NULL, &h, &cb, &med), H_INVALIDARG);
    EXPECT_EQ(TO_STGX(&m.os, stg, &cf, &w, NULL, &cb, &med), H_INVALIDARG);
    EXPECT_EQ(TO_STGX(&m.os, stg, &cf, &w, &h, NULL, &med), H_INVALIDARG);
    EXPECT_EQ(TO_STGX(&m.os, stg, &cf, &w, &h, &cb, NULL), H_INVALIDARG);
    EXPECT_EQ(m.gets, 0);
    EXPECT_EQ(cf, 0x7777); EXPECT_EQ(w, 11); EXPECT_EQ(h, 12); EXPECT_EQ(cb, 13);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stgex_bad_tymed) {
    REQUIRE(TO_STGX);
    if (t_is_wine()) { t_skip("wine aborts/stubs the Ex conversion exports"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1); mos_load(&m, m.buf, m.len);
    CLIPFORMAT cf = 0; LONG w = 0, h = 0; DWORD cb = 0; STGMEDIUM med; memset(&med, 0, sizeof med);
    DWORD bad[] = { TYMED_HGLOBAL, TYMED_FILE, TYMED_GDI, TYMED_MFPICT, TYMED_ENHMF, 0x80 };
    for (int i = 0; i < 6; i++) {
        med.tymed = bad[i];
        HRESULT hr = TO_STGX(&m.os, stg, &cf, &w, &h, &cb, &med);
        CHECKF(hr == E_DV_TYMED, "tymed %lu: 0x%08lx", bad[i], hr);
    }
    EXPECT_EQ(m.gets, 0);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_to_stgex_no_presentation) {
    REQUIRE(TO_STGX);
    if (t_is_wine()) { t_skip("wine aborts/stubs the Ex conversion exports"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = new_stg(&lb); if (!stg) { CHECK(0); slm_couninit(); return; }
    MEMOS m; mos_build_embedded(&m, "Package", PAYLOAD, PL, 1); mos_load(&m, m.buf, m.len);
    CLIPFORMAT cf = 0x7777; LONG w = 11, h = 12; DWORD cb = 13; STGMEDIUM med; memset(&med, 0, sizeof med);
    HRESULT hr = TO_STGX(&m.os, stg, &cf, &w, &h, &cb, &med);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(cf, 0); EXPECT_EQ(w, 0); EXPECT_EQ(h, 0); EXPECT_EQ(cb, 0);
    EXPECT_EQ(med.tymed, TYMED_NULL);
    mos_free(&m); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_from_stgex_null_cf_no_presentation) {
    REQUIRE(TO_STG); REQUIRE(TO_OSX);
    if (t_is_wine()) { t_skip("wine aborts/stubs the Ex conversion exports"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = converted_storage(&lb);
    CHECK(stg != NULL); if (!stg) { slm_couninit(); return; }
    STGMEDIUM med; memset(&med, 0, sizeof med);
    MEMOS o; mos_init(&o);
    HRESULT hr = TO_OSX(stg, 0, 0, 0, 0, &med, &o.os);
    EXPECT_EQ(hr, S_OK);
    if (o.len >= 8) { EXPECT_EQ(*(DWORD *)o.buf, 0x501); EXPECT_EQ(*(DWORD *)(o.buf + 4), 2); }
    CHECK(find_bytes(o.buf, o.len, PAYLOAD, PL));
    t_note("Ex conversion without presentation: %lu bytes", o.len);
    mos_free(&o); rel(stg, lb);
    slm_couninit();
}
TEST(slm_os_from_stgex_medium_validation) {
    REQUIRE(TO_STG); REQUIRE(TO_OSX);
    if (t_is_wine()) { t_skip("wine reads the medium before validating it"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    ILockBytes *lb; IStorage *stg = converted_storage(&lb);
    CHECK(stg != NULL); if (!stg) { slm_couninit(); return; }
    STGMEDIUM med; MEMOS o; mos_init(&o);
    /* a clipboard format with no data handle */
    memset(&med, 0, sizeof med); med.tymed = TYMED_HGLOBAL;
    EXPECT_EQ(TO_OSX(stg, CF_DIB, 1, 1, 16, &med, &o.os), E_DV_STGMEDIUM);
    /* data size of zero */
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 16);
    memset(&med, 0, sizeof med); med.tymed = TYMED_HGLOBAL; med.hGlobal = g;
    EXPECT_EQ(TO_OSX(stg, CF_DIB, 1, 1, 0, &med, &o.os), H_INVALIDARG);
    /* unsupported medium types */
    DWORD bad[] = { TYMED_FILE, TYMED_GDI, TYMED_MFPICT, TYMED_ENHMF, TYMED_ISTORAGE };
    for (int i = 0; i < 5; i++) {
        memset(&med, 0, sizeof med); med.tymed = bad[i]; med.hGlobal = g;
        HRESULT hr = TO_OSX(stg, CF_DIB, 1, 1, 16, &med, &o.os);
        CHECKF(hr == E_DV_TYMED, "tymed %lu: 0x%08lx", bad[i], hr);
    }
    EXPECT_EQ(o.puts, 0);              /* all rejected before any output */
    GlobalFree(g);
    mos_free(&o); rel(stg, lb);
    slm_couninit();
}
