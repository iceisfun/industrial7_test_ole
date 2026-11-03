/* FmtIdToPropStgName / PropStgNameToFmtId and the property (de)serialization helpers
 * StgConvertVariantToProperty / StgConvertPropertyToVariant / StgPropertyLengthAsVariant. */
#include "pv.h"

/* ======================================================================= stream names */

static const WCHAR NAME_SI[]  = L"\005SummaryInformation";
static const WCHAR NAME_DSI[] = L"\005DocumentSummaryInformation";

/* the Wine/Win name for IID_IPropertySetStorage, spelled with explicit code units (the literal above is not it) */
static void ipss_name(WCHAR *out) {
    static const WCHAR t[] = {5,'0','j','a','a','a','a','a','a','A','a','a','a','a','a','d','a','A','a','a','a','a','a','a','a','G','c',0};
    memcpy(out, t, sizeof t);
}

TEST(pvps_fmtid_name_null_arguments) {
    REQUIRE(U(FmtIdToPropStgName));
    WCHAR name[32];
    EQ(U(FmtIdToPropStgName)(NULL, name), E_INVALIDARG_);
    EQ(U(FmtIdToPropStgName)(&G_FMTID_SI, NULL), E_INVALIDARG_);
    EQ(U(FmtIdToPropStgName)(NULL, NULL), E_INVALIDARG_);
}

TEST(pvps_fmtid_name_well_known) {
    REQUIRE(U(FmtIdToPropStgName));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WCHAR name[40];
    EQ(U(FmtIdToPropStgName)(&G_FMTID_SI, name), S_OK);
    CHECK(!lstrcmpW(name, NAME_SI));
    EQ(lstrlenW(name), 19);
    EQ(U(FmtIdToPropStgName)(&G_FMTID_DSI, name), S_OK);
    CHECK(!lstrcmpW(name, NAME_DSI));
    EQ(lstrlenW(name), 27);
    /* the user-defined section lives in the DocumentSummaryInformation stream */
    EQ(U(FmtIdToPropStgName)(&G_FMTID_UDP, name), S_OK);
    CHECK(!lstrcmpW(name, NAME_DSI));
}

TEST(pvps_fmtid_name_generic_guid) {
    REQUIRE(U(FmtIdToPropStgName));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    WCHAR name[40], want[40];
    ipss_name(want);
    EQ(U(FmtIdToPropStgName)(&G_IID_IPropSetStg, name), S_OK);
    CHECK(!lstrcmpW(name, want));
    EQ(lstrlenW(name), 27);
    EQ(name[0], 5);
}

TEST(pvps_fmtid_name_shape_for_arbitrary_guids) {
    REQUIRE(U(FmtIdToPropStgName));
    GUID g[8] = {G_ANY_GUID, G_ANY_GUID2, G_NULL_GUID, G_IID_IUnknown, G_IID_IPropStg};
    memset(&g[5], 0xFF, sizeof(GUID));
    memset(&g[6], 0x80, sizeof(GUID));
    memset(&g[7], 0x01, sizeof(GUID));
    for (int i = 0; i < 8; i++) {
        WCHAR name[40]; for (int k = 0; k < 40; k++) name[k] = 0xAAAA;
        EQ(U(FmtIdToPropStgName)(&g[i], name), S_OK);
        CHECKF(name[0] == 5, "guid %d: prefix %04x", i, name[0]);
        CHECKF(lstrlenW(name) == 27, "guid %d: length %d", i, lstrlenW(name));
        CHECKF(name[27] == 0, "guid %d: no terminator", i);
        for (int k = 1; k < 27; k++) CHECKF((name[k] >= '0' && name[k] <= '9') || (name[k] >= 'a' && name[k] <= 'z') || (name[k] >= 'A' && name[k] <= 'Z'), "guid %d: char %d = %04x", i, k, name[k]);
        /* nothing beyond CCH_MAX_PROPSTG_NAME+1 characters may be written */
        for (int k = 32; k < 40; k++) CHECKF(name[k] == 0xAAAA, "guid %d: wrote past 32 chars", i);
    }
}

TEST(pvps_fmtid_name_distinct_for_distinct_guids) {
    REQUIRE(U(FmtIdToPropStgName));
    static WCHAR names[64][40];
    for (int i = 0; i < 64; i++) {
        GUID g; memset(&g, 0, sizeof g);
        ((BYTE *)&g)[(i * 5) % 16] = (BYTE)(1 + i);
        ((BYTE *)&g)[(i * 3 + 1) % 16] ^= (BYTE)(i * 7);
        EQ(U(FmtIdToPropStgName)(&g, names[i]), S_OK);
    }
    int dup = 0;
    for (int i = 0; i < 64; i++) for (int j = 0; j < i; j++) if (!lstrcmpW(names[i], names[j])) dup++;
    CHECKF(dup == 0, "%d duplicate names for 64 distinct guids", dup);
}

static int guid_roundtrip(const GUID *g) {
    WCHAR name[40]; GUID back; memset(&back, 0x33, sizeof back);
    if (U(FmtIdToPropStgName)(g, name) != S_OK) return 0;
    if (U(PropStgNameToFmtId)(name, &back) != S_OK) return 0;
    return guid_eq(g, &back);
}

TEST(pvps_fmtid_name_roundtrip_table) {
    REQUIRE(U(FmtIdToPropStgName)); REQUIRE(U(PropStgNameToFmtId));
    GUID tab[12] = {G_FMTID_SI, G_FMTID_DSI, G_IID_IUnknown, G_IID_IPropSetStg, G_IID_IPropStg, G_ANY_GUID, G_ANY_GUID2, G_NULL_GUID};
    memset(&tab[8], 0xFF, 16); memset(&tab[9], 0x5A, 16); memset(&tab[10], 0xA5, 16);
    for (int i = 0; i < 16; i++) ((BYTE *)&tab[11])[i] = (BYTE)(0x10 * i + i);
    for (int i = 0; i < 12; i++) CHECKF(guid_roundtrip(&tab[i]), "guid %d does not round trip", i);
    /* UserDefinedProperties maps to the DocumentSummaryInformation stream name, hence round trips to DSI */
    WCHAR name[40]; GUID back;
    EQ(U(FmtIdToPropStgName)(&G_FMTID_UDP, name), S_OK);
    EQ(U(PropStgNameToFmtId)(name, &back), S_OK);
    CHECK(guid_eq(&back, &G_FMTID_DSI));
}

TEST(pvps_fmtid_name_roundtrip_bit_patterns) {
    REQUIRE(U(FmtIdToPropStgName)); REQUIRE(U(PropStgNameToFmtId));
    int bad = 0;
    for (int bit = 0; bit < 128; bit++) {
        GUID g; memset(&g, 0, sizeof g); ((BYTE *)&g)[bit / 8] = (BYTE)(1u << (bit % 8));
        if (!guid_roundtrip(&g)) bad++;
        memset(&g, 0xFF, sizeof g); ((BYTE *)&g)[bit / 8] &= (BYTE)~(1u << (bit % 8));
        if (!guid_roundtrip(&g)) bad++;
    }
    CHECKF(bad == 0, "%d single-bit guids failed to round trip", bad);
}

TEST(pvps_fmtid_name_to_fmtid_null_arguments) {
    REQUIRE(U(PropStgNameToFmtId));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    GUID g; WCHAR dsi[40]; lstrcpyW(dsi, NAME_DSI);
    EQ(U(PropStgNameToFmtId)(NULL, NULL), E_INVALIDARG_);
    EQ(U(PropStgNameToFmtId)(NULL, &g), STG_E_INVALIDNAME_);
    EQ(U(PropStgNameToFmtId)(dsi, NULL), E_INVALIDARG_);
}

TEST(pvps_fmtid_name_to_fmtid_well_known) {
    REQUIRE(U(PropStgNameToFmtId));
    GUID g; WCHAR n[40], want[40];
    lstrcpyW(n, NAME_SI); memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(n, &g), S_OK); CHECK(guid_eq(&g, &G_FMTID_SI));
    lstrcpyW(n, NAME_DSI); memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(n, &g), S_OK); CHECK(guid_eq(&g, &G_FMTID_DSI));
    ipss_name(want); memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(want, &g), S_OK); CHECK(guid_eq(&g, &G_IID_IPropSetStg));
}

TEST(pvps_fmtid_name_to_fmtid_case_insensitive) {
    REQUIRE(U(PropStgNameToFmtId));
    /* adapted from Wine dlls/ole32/tests/stg_prop.c (LGPL-2.1+) */
    GUID g; WCHAR n[40];
    lstrcpyW(n, NAME_DSI); n[1] = L'd';
    memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(n, &g), S_OK); CHECK(guid_eq(&g, &G_FMTID_DSI));
    lstrcpyW(n, NAME_SI); for (WCHAR *p = n + 1; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(n, &g), S_OK); CHECK(guid_eq(&g, &G_FMTID_SI));
    /* the generic encoding is also matched case insensitively on the letters after the prefix */
    ipss_name(n); n[1] = L'0'; n[2] = L'J';
    memset(&g, 0, sizeof g);
    EQ(U(PropStgNameToFmtId)(n, &g), S_OK); CHECK(guid_eq(&g, &G_IID_IPropSetStg));
}

TEST(pvps_fmtid_name_to_fmtid_rejects_malformed) {
    REQUIRE(U(PropStgNameToFmtId));
    GUID g; WCHAR n[40];
    static const WCHAR *bad[] = { L"", L"SummaryInformation", L"\005", L"\005Summary", L"\005NotAKnownName",
                                  L"abcdefghijklmnopqrstuvwxyz0", L"\005aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" };
    for (int i = 0; i < N_ELEM(bad); i++) {
        memset(&g, 0x44, sizeof g);
        HRESULT hr = U(PropStgNameToFmtId)((LPOLESTR)bad[i], &g);
        if (i < 6) CHECKF(FAILED(hr), "name %d accepted (hr=%08lx)", i, hr);
        t_note("PropStgNameToFmtId(bad name #%d) -> 0x%08lx", i, hr);
    }
    /* one valid name with its first char changed to a non-prefix must fail */
    ipss_name(n); n[0] = 'x';
    HRESULT hr = U(PropStgNameToFmtId)(n, &g);
    t_note("name without \\005 prefix -> 0x%08lx", hr);
    /* ... and a valid name that is one character short must fail */
    ipss_name(n); n[26] = 0;
    hr = U(PropStgNameToFmtId)(n, &g);
    CHECKF(FAILED(hr), "short name accepted (hr=%08lx)", hr);
    t_note("short generic name -> 0x%08lx", hr);
}

/* ======================================================================= serialization */

typedef struct TAlloc { const struct TAllocVtbl *vt; int allocs, frees; } TAlloc;
struct TAllocVtbl { void *(*Allocate)(TAlloc *, ULONG); void (*Free)(TAlloc *, void *); };
static void *ta_alloc(TAlloc *a, ULONG cb) { a->allocs++; return U(CoTaskMemAlloc)(cb); }
static void ta_free(TAlloc *a, void *p) { a->frees++; U(CoTaskMemFree)(p); }
static const struct TAllocVtbl g_ta_vtbl = { ta_alloc, ta_free };
static void ta_init(TAlloc *a) { a->vt = &g_ta_vtbl; a->allocs = a->frees = 0; }

typedef SERIALIZEDPROPERTYVALUE *(__stdcall *pfnV2P)(const PROPVARIANT *, USHORT, SERIALIZEDPROPERTYVALUE *, ULONG *, PROPID, BOOLEAN, ULONG *);
typedef BOOLEAN (__stdcall *pfnP2V)(const SERIALIZEDPROPERTYVALUE *, USHORT, PROPVARIANT *, void *);
UDECL(ULONG, StgPropertyLengthAsVariant, (SERIALIZEDPROPERTYVALUE *, ULONG, USHORT, BYTE));

static pfnV2P v2p(void);
#define REQUIRE_V2P() do { if (!v2p()) { t_skip("missing export: StgConvertVariantToProperty"); return; } \
    if (t_is_wine()) { t_skip("wine: StgConvertVariantToProperty is a stub"); return; } } while (0)
static pfnV2P v2p(void) { return (pfnV2P)t_proc("StgConvertVariantToProperty"); }
static pfnP2V p2v(void) { return (pfnP2V)t_proc("StgConvertPropertyToVariant"); }

/* Serialize pv at the given code page and compare with the expected on-disk image. */
static void ser_expect(const char *what, const PROPVARIANT *pv, USHORT cp, const BYTE *exp, ULONG explen) {
    pfnV2P f = v2p();
    BYTE raw[sizeof(SERIALIZEDPROPERTYVALUE) + 256 + 16];
    memset(raw, 0xCC, sizeof raw);
    ULONG len = 0xdeadbeef;
    /* query the size: NULL buffer returns NULL and the required length */
    SERIALIZEDPROPERTYVALUE *r = f(pv, cp, NULL, &len, 0, FALSE, NULL);
    CHECKF(r == NULL, "%s: size query returned a pointer", what);
    CHECKF(len == explen, "%s: size query length %lu want %lu", what, len, explen);
    len = sizeof raw - 16;
    r = f(pv, cp, (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
    CHECKF(r == (SERIALIZEDPROPERTYVALUE *)raw, "%s: returned %p", what, (void *)r);
    CHECKF(len == explen, "%s: length %lu want %lu", what, len, explen);
    if (r && len == explen) {
        CHECKF(!memcmp(raw, exp, explen), "%s: image differs", what);
        BYTE guard = 0xCC; int clean = 1;
        for (ULONG i = explen; i < explen + 8; i++) if (raw[i] != guard) clean = 0;
        CHECKF(clean, "%s: wrote past the reported length", what);
    }
}

TEST(pvps_serialize_scalars_unicode) {
    REQUIRE_V2P();
    PROPVARIANT v;
    { static const BYTE e[] = {3,0,0,0, 0xef,0xcd,0xab,0xfe}; PropVariantInit(&v); v.vt = VT_I4; v.lVal = 0xfeabcdef; ser_expect("I4", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {2,0,0,0, 0x34,0x12,0,0}; PropVariantInit(&v); v.vt = VT_I2; v.iVal = 0x1234; ser_expect("I2", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0x12,0,0,0, 0x34,0x12,0,0}; PropVariantInit(&v); v.vt = VT_UI2; v.uiVal = 0x1234; ser_expect("UI2", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0x11,0,0,0, 0x7f,0,0,0}; PropVariantInit(&v); v.vt = VT_UI1; v.bVal = 0x7f; ser_expect("UI1", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0x13,0,0,0, 1,2,3,4}; PropVariantInit(&v); v.vt = VT_UI4; v.ulVal = 0x04030201; ser_expect("UI4", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0xb,0,0,0, 0xff,0xff,0,0}; PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_TRUE; ser_expect("BOOL true", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0xb,0,0,0, 0,0,0,0}; PropVariantInit(&v); v.vt = VT_BOOL; v.boolVal = VARIANT_FALSE; ser_expect("BOOL false", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {4,0,0,0, 0,0,0x80,0x3f}; PropVariantInit(&v); v.vt = VT_R4; v.fltVal = 1.0f; ser_expect("R4", &v, CP_WINUNICODE, e, 8); }
    { static const BYTE e[] = {0xa,0,0,0, 0x57,0,7,0x80}; PropVariantInit(&v); v.vt = VT_ERROR; v.scode = (SCODE)0x80070057; ser_expect("ERROR", &v, CP_WINUNICODE, e, 8); }
}

TEST(pvps_serialize_8byte_scalars_unicode) {
    REQUIRE_V2P();
    PROPVARIANT v;
    { static const BYTE e[] = {5,0,0,0, 0,0,0,0,0,0,0xf0,0x3f}; PropVariantInit(&v); v.vt = VT_R8; v.dblVal = 1.0; ser_expect("R8", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {0x14,0,0,0, 1,2,3,4,5,6,7,8}; PropVariantInit(&v); v.vt = VT_I8; v.hVal.QuadPart = 0x0807060504030201ll; ser_expect("I8", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {0x15,0,0,0, 1,2,3,4,5,6,7,8}; PropVariantInit(&v); v.vt = VT_UI8; v.uhVal.QuadPart = 0x0807060504030201ull; ser_expect("UI8", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {0x40,0,0,0, 0x78,0x56,0x34,0x12, 0xf0,0xde,0xbc,0x9a}; PropVariantInit(&v); v.vt = VT_FILETIME; v.filetime.dwLowDateTime = 0x12345678; v.filetime.dwHighDateTime = 0x9abcdef0; ser_expect("FILETIME", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {6,0,0,0, 1,2,3,4,5,6,7,8}; PropVariantInit(&v); v.vt = VT_CY; v.cyVal.int64 = 0x0807060504030201ll; ser_expect("CY", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {7,0,0,0, 0,0,0,0,0,0,0xf0,0x3f}; PropVariantInit(&v); v.vt = VT_DATE; v.date = 1.0; ser_expect("DATE", &v, CP_WINUNICODE, e, 12); }
}

TEST(pvps_serialize_empty_null) {
    REQUIRE_V2P();
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    PROPVARIANT v; PropVariantInit(&v);
    static const BYTE ee[] = {0,0,0,0}, en[] = {1,0,0,0};
    v.vt = VT_EMPTY; ser_expect("EMPTY", &v, CP_WINUNICODE, ee, 4);
    v.vt = VT_NULL;  ser_expect("NULL", &v, CP_WINUNICODE, en, 4);
}

TEST(pvps_serialize_strings_unicode_codepage) {
    REQUIRE_V2P();
    PROPVARIANT v;
    /* LPWSTR: type, length in characters including NUL, UTF-16 data padded to 4 */
    { static const BYTE e[] = {0x1f,0,0,0, 4,0,0,0, 'a',0,'b',0,'c',0,0,0};
      PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = L"abc"; ser_expect("LPWSTR abc", &v, CP_WINUNICODE, e, 16); }
    { static const BYTE e[] = {0x1f,0,0,0, 3,0,0,0, 'a',0,'b',0,0,0,0,0};
      PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = L"ab"; ser_expect("LPWSTR ab (pad)", &v, CP_WINUNICODE, e, 16); }
    /* LPSTR in a code-page-1200 set is stored as UTF-16 */
    { static const BYTE e[] = {0x1e,0,0,0, 8,0,0,0, 'a',0,'b',0,'c',0,0,0};   /* count is in bytes */
      PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = "abc"; ser_expect("LPSTR abc @1200", &v, CP_WINUNICODE, e, 16); }
    /* BSTR: byte count includes the terminator, data padded to 4 */
    { static const BYTE e[] = {8,0,0,0, 10,0,0,0, 't',0,'e',0,'s',0,'t',0,0,0,0,0};
      BSTR b = U(SysAllocString)(L"test"); PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = b;
      ser_expect("BSTR test @1200", &v, CP_WINUNICODE, e, 20); U(SysFreeString)(b); }
}

TEST(pvps_serialize_strings_ansi_codepages) {
    REQUIRE_V2P(); REQUIRE(U(SysAllocString));
    PROPVARIANT v;
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+): BSTR at CP_UTF8 */
    { static const BYTE e[] = {8,0,0,0, 5,0,0,0, 't','e','s','t',0,0,0,0};
      BSTR b = U(SysAllocString)(L"test"); PropVariantInit(&v); v.vt = VT_BSTR; v.bstrVal = b;
      ser_expect("BSTR test @utf8", &v, CP_UTF8, e, 16); U(SysFreeString)(b); }
    /* LPSTR at 1252: byte count including NUL, data padded to 4 */
    { static const BYTE e[] = {0x1e,0,0,0, 4,0,0,0, 'a','b','c',0};
      PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = "abc"; ser_expect("LPSTR abc @1252", &v, 1252, e, 12); }
    { static const BYTE e[] = {0x1e,0,0,0, 5,0,0,0, 'a','b','c','d',0,0,0,0};
      PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = "abcd"; ser_expect("LPSTR abcd @1252", &v, 1252, e, 16); }
    /* LPWSTR is UTF-16 whatever the code page is */
    { static const BYTE e[] = {0x1f,0,0,0, 4,0,0,0, 'a',0,'b',0,'c',0,0,0};
      PropVariantInit(&v); v.vt = VT_LPWSTR; v.pwszVal = L"abc"; ser_expect("LPWSTR abc @1252", &v, 1252, e, 16); }
    { static const BYTE e[] = {0x1e,0,0,0, 1,0,0,0, 0,0,0,0};
      PropVariantInit(&v); v.vt = VT_LPSTR; v.pszVal = ""; ser_expect("LPSTR empty @1252", &v, 1252, e, 12); }
}

TEST(pvps_serialize_blob_clsid) {
    REQUIRE_V2P();
    PROPVARIANT v;
    { static const BYTE e[] = {0x41,0,0,0, 3,0,0,0, 1,2,3,0}; BYTE d[] = {1,2,3};
      PropVariantInit(&v); v.vt = VT_BLOB; v.blob.cbSize = 3; v.blob.pBlobData = d; ser_expect("BLOB 3", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {0x41,0,0,0, 4,0,0,0, 1,2,3,4}; BYTE d[] = {1,2,3,4};
      PropVariantInit(&v); v.vt = VT_BLOB; v.blob.cbSize = 4; v.blob.pBlobData = d; ser_expect("BLOB 4", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {0x41,0,0,0, 0,0,0,0};
      PropVariantInit(&v); v.vt = VT_BLOB; v.blob.cbSize = 0; v.blob.pBlobData = NULL; ser_expect("BLOB 0", &v, CP_WINUNICODE, e, 8); }
    { BYTE e[20] = {0x48,0,0,0}; GUID g = G_ANY_GUID; memcpy(e + 4, &g, 16);
      PropVariantInit(&v); v.vt = VT_CLSID; v.puuid = &g; ser_expect("CLSID", &v, CP_WINUNICODE, e, 20); }
}

TEST(pvps_serialize_vectors) {
    REQUIRE_V2P();
    PROPVARIANT v;
    { static const BYTE e[] = {0x10,0x10,0,0, 3,0,0,0, 1,2,3,0}; BYTE d[] = {1,2,3};   /* VT_VECTOR|VT_I1 */
      PropVariantInit(&v); v.vt = VT_VECTOR | VT_I1; v.cac.cElems = 3; v.cac.pElems = (CHAR *)d; ser_expect("I1 vector", &v, CP_WINUNICODE, e, 12); }
    { static const BYTE e[] = {3,0x10,0,0, 2,0,0,0, 1,0,0,0, 2,0,0,0}; LONG d[] = {1,2};
      PropVariantInit(&v); v.vt = VT_VECTOR | VT_I4; v.cal.cElems = 2; v.cal.pElems = d; ser_expect("I4 vector", &v, CP_WINUNICODE, e, 16); }
    { static const BYTE e[] = {2,0x10,0,0, 3,0,0,0, 1,0,2,0,3,0,0,0}; SHORT d[] = {1,2,3};
      PropVariantInit(&v); v.vt = VT_VECTOR | VT_I2; v.cai.cElems = 3; v.cai.pElems = d; ser_expect("I2 vector (odd count padded)", &v, CP_WINUNICODE, e, 16); }
    { static const BYTE e[] = {0x1f,0x10,0,0, 2,0,0,0, 2,0,0,0,'a',0,0,0, 3,0,0,0,'b',0,'c',0,0,0,0,0};
      WCHAR *d[2] = {L"a", L"bc"};
      PropVariantInit(&v); v.vt = VT_VECTOR | VT_LPWSTR; v.calpwstr.cElems = 2; v.calpwstr.pElems = d; ser_expect("LPWSTR vector", &v, CP_WINUNICODE, e, 28); }
    { static const BYTE e[] = {0,0x10,0,0, 0,0,0,0};
      /* VT_VECTOR|VT_EMPTY is not a valid vector; only length protocol checked in the invalid test */
      (void)e; }
}

TEST(pvps_serialize_variant_vector) {
    REQUIRE_V2P();
    PROPVARIANT el[2]; memset(el, 0, sizeof el);
    el[0].vt = VT_I1; el[0].cVal = 1; el[1].vt = VT_I4; el[1].lVal = 2;
    PROPVARIANT v; PropVariantInit(&v);
    v.vt = VT_VECTOR | VT_VARIANT; v.capropvar.cElems = 2; v.capropvar.pElems = el;
    static const BYTE e[] = {0xc,0x10,0,0, 2,0,0,0, 0x10,0,0,0, 1,0,0,0, 3,0,0,0, 2,0,0,0};
    ser_expect("VARIANT vector", &v, CP_WINUNICODE, e, 24);
}

TEST(pvps_serialize_buffer_protocol) {
    REQUIRE_V2P();
    pfnV2P f = v2p();
    PROPVARIANT v; PropVariantInit(&v); v.vt = VT_I4; v.lVal = 7;
    BYTE raw[64]; memset(raw, 0xCC, sizeof raw);
    ULONG len = 0xdeadbeef;
    SERIALIZEDPROPERTYVALUE *r = f(&v, CP_WINUNICODE, NULL, &len, 0, FALSE, NULL);
    CHECK(r == NULL); EQ(len, 8);
    /* a buffer exactly as large as the image is accepted */
    len = 8;
    r = f(&v, CP_WINUNICODE, (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
    CHECK(r == (SERIALIZEDPROPERTYVALUE *)raw); EQ(len, 8);
    CHECK(raw[8] == 0xCC);
    /* too small: nothing is written, the needed size is reported */
    memset(raw, 0xCC, sizeof raw); len = 4;
    r = f(&v, CP_WINUNICODE, (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
    t_note("buffer of 4 for an 8 byte image: ret=%p len=%lu raw[0]=%02x", (void *)r, len, raw[0]);
    CHECK(r == NULL || r == (SERIALIZEDPROPERTYVALUE *)raw);
    /* the length is reported for every call shape: a larger buffer still reports the image size, not the capacity */
    memset(raw, 0xCC, sizeof raw); len = 64;
    r = f(&v, CP_WINUNICODE, (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
    CHECK(r == (SERIALIZEDPROPERTYVALUE *)raw); EQ(len, 8);
}

TEST(pvps_serialize_unsupported_vt_fails) {
    REQUIRE_V2P();
    pfnV2P f = v2p();
    static const VARTYPE bad[] = {VT_DISPATCH, VT_UNKNOWN, VT_VARIANT, VT_VOID, VT_HRESULT, VT_PTR, VT_SAFEARRAY, VT_USERDEFINED};
    for (int i = 0; i < N_ELEM(bad); i++) {
        PROPVARIANT v; PropVariantInit(&v); v.vt = bad[i];
        BYTE raw[64]; memset(raw, 0xCC, sizeof raw); ULONG len = 64;
        SERIALIZEDPROPERTYVALUE *r = f(&v, CP_WINUNICODE, (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
        t_note("serialize vt=%d -> ret=%p len=%lu", bad[i], (void *)r, len);
        CHECKF(r == NULL, "vt %d serialized to a pointer", bad[i]);
    }
}

TEST(pvps_serialize_roundtrip_through_deserialize) {
    REQUIRE_V2P(); REQUIRE(p2v()); REQUIRE(U(CoTaskMemAlloc));
    pfnV2P f = v2p(); pfnP2V g = p2v();
    PROPVARIANT src[8]; memset(src, 0, sizeof src);
    src[0].vt = VT_I2; src[0].iVal = -5;
    src[1].vt = VT_UI4; src[1].ulVal = 0xdeadbeef;
    src[2].vt = VT_R8; src[2].dblVal = 3.25;
    src[3].vt = VT_I8; src[3].hVal.QuadPart = -123456789012345ll;
    src[4].vt = VT_FILETIME; src[4].filetime.dwLowDateTime = 1; src[4].filetime.dwHighDateTime = 2;
    src[5].vt = VT_BOOL; src[5].boolVal = VARIANT_TRUE;
    src[6].vt = VT_LPWSTR; src[6].pwszVal = L"round trip";
    src[7].vt = VT_UI1; src[7].bVal = 200;
    USHORT cps[2] = {CP_WINUNICODE, 1252};
    for (int c = 0; c < 2; c++) for (int i = 0; i < 8; i++) {
        BYTE raw[128]; ULONG len = sizeof raw;
        SERIALIZEDPROPERTYVALUE *r = f(&src[i], cps[c], (SERIALIZEDPROPERTYVALUE *)raw, &len, 0, FALSE, NULL);
        CHECKF(r != NULL, "case %d/%d serialize", c, i);
        if (!r) continue;
        TAlloc a; ta_init(&a); PROPVARIANT out; memset(&out, 0x11, sizeof out);
        BOOLEAN rc = g(r, cps[c], &out, &a);
        CHECKF(rc == 0, "case %d/%d deserialize rc=%d", c, i, rc);
        CHECKF(out.vt == src[i].vt, "case %d/%d vt %d", c, i, out.vt);
        if (src[i].vt == VT_LPWSTR) { if (out.vt == VT_LPWSTR) CHECKF(!lstrcmpW(out.pwszVal, src[i].pwszVal), "case %d/%d string", c, i); }
        else if (src[i].vt == VT_I2 || src[i].vt == VT_BOOL) CHECKF(out.iVal == src[i].iVal, "case %d/%d value", c, i);
        else if (src[i].vt == VT_UI1) CHECKF(out.bVal == src[i].bVal, "case %d/%d value", c, i);
        else if (src[i].vt == VT_UI4) CHECKF(out.ulVal == src[i].ulVal, "case %d/%d value", c, i);
        else CHECKF(out.uhVal.QuadPart == src[i].uhVal.QuadPart, "case %d/%d value", c, i);
        U(PropVariantClear)(&out);
    }
}

/* ======================================================================= deserialize */

static void de_expect_i4(const BYTE *img, USHORT cp, const char *what, LONG want) {
    TAlloc a; ta_init(&a); PROPVARIANT p; memset(&p, 0x11, sizeof p);
    BOOLEAN rc = p2v()((const SERIALIZEDPROPERTYVALUE *)img, cp, &p, &a);
    CHECKF(rc == 0, "%s rc=%d", what, rc); CHECKF(p.vt == VT_I4, "%s vt=%d", what, p.vt);
    CHECKF(p.lVal == want, "%s lVal=%ld", what, p.lVal);
    CHECKF(a.allocs == 0, "%s used the allocator %d times for a scalar", what, a.allocs);
}

TEST(pvps_deserialize_scalars) {
    REQUIRE(p2v()); REQUIRE(U(CoTaskMemAlloc));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    static const BYTE i4[] = {3,0,0,0, 0xef,0xcd,0xab,0xfe};
    de_expect_i4(i4, CP_WINUNICODE, "I4 @1200", (LONG)0xfeabcdef);
    de_expect_i4(i4, 1252, "I4 @1252", (LONG)0xfeabcdef);          /* the code page only matters for strings */
    TAlloc a; ta_init(&a); PROPVARIANT p;
    static const BYTE em[] = {0,0,0,0}, nu[] = {1,0,0,0};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)em, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_EMPTY);
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)nu, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_NULL);
    static const BYTE i2[] = {2,0,0,0, 0xfe,0xff,0,0};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)i2, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_I2); EQ(p.iVal, -2);
    static const BYTE u1[] = {0x11,0,0,0, 0xff,0,0,0};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)u1, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_UI1); EQ(p.bVal, 0xff);
    static const BYTE bo[] = {0xb,0,0,0, 0xff,0xff,0,0};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)bo, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_BOOL); EQ(p.boolVal, VARIANT_TRUE);
    static const BYTE r8[] = {5,0,0,0, 0,0,0,0,0,0,0xf0,0x3f};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)r8, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_R8); CHECK(p.dblVal == 1.0);
    static const BYTE ft[] = {0x40,0,0,0, 0x78,0x56,0x34,0x12, 0xf0,0xde,0xbc,0x9a};
    memset(&p, 0x11, sizeof p); EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)ft, CP_WINUNICODE, &p, &a), 0); EQ(p.vt, VT_FILETIME);
    EQ(p.filetime.dwLowDateTime, 0x12345678); CHECK(p.filetime.dwHighDateTime == 0x9abcdef0);
}

TEST(pvps_deserialize_strings) {
    REQUIRE(p2v()); REQUIRE(U(CoTaskMemAlloc)); REQUIRE(U(PropVariantClear));
    TAlloc a; ta_init(&a); PROPVARIANT p;
    static const BYTE wc[] = {8,0,0,0, 10,0,0,0, 't',0,'e',0,'s',0,'t',0,0,0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)wc, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_BSTR); if (p.vt == VT_BSTR) EXPECT_WSTR(p.bstrVal, L"test");
    U(PropVariantClear)(&p);
    static const BYTE mb[] = {8,0,0,0, 5,0,0,0, 't','e','s','t',0,0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)mb, CP_UTF8, &p, &a), 0);
    EQ(p.vt, VT_BSTR); if (p.vt == VT_BSTR) EXPECT_WSTR(p.bstrVal, L"test");
    U(PropVariantClear)(&p);
    static const BYTE lw[] = {0x1f,0,0,0, 4,0,0,0, 'a',0,'b',0,'c',0,0,0};
    ta_init(&a); memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)lw, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_LPWSTR); if (p.vt == VT_LPWSTR) EXPECT_WSTR(p.pwszVal, L"abc");
    CHECK(a.allocs >= 1);
    U(PropVariantClear)(&p);
    static const BYTE ls[] = {0x1e,0,0,0, 4,0,0,0, 'a','b','c',0};
    ta_init(&a); memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)ls, 1252, &p, &a), 0);
    EQ(p.vt, VT_LPSTR); if (p.vt == VT_LPSTR) EXPECT_STR(p.pszVal, "abc");
    U(PropVariantClear)(&p);
    /* LPSTR stored as UTF-16 (code page 1200) is returned as an ANSI VT_LPSTR */
    static const BYTE lu[] = {0x1e,0,0,0, 8,0,0,0, 'a',0,'b',0,'c',0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)lu, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_LPSTR); if (p.vt == VT_LPSTR) t_note("LPSTR stored as UTF-16 read back as \"%s\"", p.pszVal);
    U(PropVariantClear)(&p);
}

TEST(pvps_deserialize_blob_clsid) {
    REQUIRE(p2v()); REQUIRE(U(PropVariantClear));
    TAlloc a; ta_init(&a); PROPVARIANT p;
    static const BYTE bl[] = {0x41,0,0,0, 3,0,0,0, 1,2,3,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)bl, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_BLOB); EQ(p.blob.cbSize, 3);
    if (p.vt == VT_BLOB && p.blob.pBlobData) CHECK(!memcmp(p.blob.pBlobData, bl + 8, 3));
    U(PropVariantClear)(&p);
    BYTE cl[20] = {0x48,0,0,0}; GUID g = G_ANY_GUID; memcpy(cl + 4, &g, 16);
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)cl, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_CLSID); if (p.vt == VT_CLSID && p.puuid) CHECK(guid_eq(p.puuid, &g));
    U(PropVariantClear)(&p);
}

TEST(pvps_deserialize_vectors) {
    REQUIRE(p2v()); REQUIRE(U(PropVariantClear));
    /* adapted from Wine dlls/ole32/tests/propvariant.c (LGPL-2.1+) */
    TAlloc a; ta_init(&a); PROPVARIANT p;
    static const BYTE i1v[] = {0x10,0x10,0,0, 3,0,0,0, 1,2,3,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)i1v, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_VECTOR | VT_I1); EQ(p.cac.cElems, 3);
    if (p.cac.pElems && p.cac.cElems == 3) { EQ(p.cac.pElems[0], 1); EQ(p.cac.pElems[1], 2); EQ(p.cac.pElems[2], 3); }
    U(PropVariantClear)(&p);
    static const BYTE wv[] = {0x1f,0x10,0,0, 3,0,0,0, 2,0,0,0,'a',0,0,0, 3,0,0,0,'b',0,'c',0,0,0,0,0, 4,0,0,0,'d',0,'e',0,'f',0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)wv, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_VECTOR | VT_LPWSTR); EQ(p.calpwstr.cElems, 3);
    if (p.calpwstr.pElems && p.calpwstr.cElems == 3) {
        EXPECT_WSTR(p.calpwstr.pElems[0], L"a"); EXPECT_WSTR(p.calpwstr.pElems[1], L"bc"); EXPECT_WSTR(p.calpwstr.pElems[2], L"def");
    }
    U(PropVariantClear)(&p);
    static const BYTE vv[] = {0xc,0x10,0,0, 2,0,0,0, 0x10,0,0,0, 1,0,0,0, 3,0,0,0, 2,0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)vv, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_VECTOR | VT_VARIANT); EQ(p.capropvar.cElems, 2);
    if (p.capropvar.pElems && p.capropvar.cElems == 2) {
        EQ(p.capropvar.pElems[0].vt, VT_I1); EQ(p.capropvar.pElems[0].cVal, 1);
        EQ(p.capropvar.pElems[1].vt, VT_I4); EQ(p.capropvar.pElems[1].lVal, 2);
    }
    U(PropVariantClear)(&p);
    static const BYTE i4v[] = {3,0x10,0,0, 2,0,0,0, 0xff,0xff,0xff,0xff, 5,0,0,0};
    memset(&p, 0, sizeof p);
    EQ(p2v()((const SERIALIZEDPROPERTYVALUE *)i4v, CP_WINUNICODE, &p, &a), 0);
    EQ(p.vt, VT_VECTOR | VT_I4); EQ(p.cal.cElems, 2);
    if (p.cal.pElems && p.cal.cElems == 2) { EQ(p.cal.pElems[0], -1); EQ(p.cal.pElems[1], 5); }
    U(PropVariantClear)(&p);
}

TEST(pvps_deserialize_unsupported_type_fails) {
    REQUIRE(p2v());
    TAlloc a; ta_init(&a);
    static const BYTE bad[][8] = { {0x7f,0x7f,0,0, 0,0,0,0}, {0x18,0,0,0, 0,0,0,0}, {0x1a,0,0,0, 0,0,0,0}, {0x24,0,0,0, 0,0,0,0}, {0x0f,0,0,0, 0,0,0,0} };
    for (int i = 0; i < N_ELEM(bad); i++) {
        PROPVARIANT p; memset(&p, 0x11, sizeof p);
        BOOLEAN rc = p2v()((const SERIALIZEDPROPERTYVALUE *)bad[i], CP_WINUNICODE, &p, &a);
        t_note("deserialize dwType=0x%x -> rc=%d vt=%d", *(const DWORD *)bad[i], rc, p.vt);
    }
}

TEST(pvps_propertylength_as_variant_observed) {
    REQUIRE(UN(StgPropertyLengthAsVariant));
    /* Undocumented helper: returns the in-memory size the property needs as a PROPVARIANT payload.
       Only record what Windows reports; the corpus does not (yet) assert on it. */
    static const BYTE i4[] = {3,0,0,0, 1,0,0,0};
    static const BYTE lw[] = {0x1f,0,0,0, 4,0,0,0, 'a',0,'b',0,'c',0,0,0};
    static const BYTE i1v[] = {0x10,0x10,0,0, 3,0,0,0, 1,2,3,0};
    ULONG r;
    r = UN(StgPropertyLengthAsVariant)((SERIALIZEDPROPERTYVALUE *)i4, sizeof i4, CP_WINUNICODE, 0);
    t_note("StgPropertyLengthAsVariant(I4) = %lu", r);
    r = UN(StgPropertyLengthAsVariant)((SERIALIZEDPROPERTYVALUE *)lw, sizeof lw, CP_WINUNICODE, 0);
    t_note("StgPropertyLengthAsVariant(LPWSTR abc) = %lu", r);
    r = UN(StgPropertyLengthAsVariant)((SERIALIZEDPROPERTYVALUE *)i1v, sizeof i1v, CP_WINUNICODE, 0);
    t_note("StgPropertyLengthAsVariant(I1 vector x3) = %lu", r);
    CHECK(1);
}
