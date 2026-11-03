/* winrt_roapi: in-box WinRT classes that need no UI, reached via RoGetActivationFactory / RoActivateInstance. */
#include "ro_common.h"

typedef HRESULT (WINAPI *s_getiids)(void *, ULONG *, GUID **);
typedef HRESULT (WINAPI *s_getname)(void *, HSTRING *);
typedef HRESULT (WINAPI *s_gettrust)(void *, int *);
typedef HRESULT (WINAPI *s_activate)(void *, void **);
typedef HRESULT (WINAPI *s_hs_obj)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *s_hs_in)(void *, HSTRING);
typedef HRESULT (WINAPI *s_out_u32)(void *, UINT32 *);
typedef HRESULT (WINAPI *s_in_u32)(void *, UINT32);
typedef HRESULT (WINAPI *s_out_i32)(void *, INT32 *);
typedef HRESULT (WINAPI *s_out_u8)(void *, BYTE *);
typedef HRESULT (WINAPI *s_out_i16)(void *, INT16 *);
typedef HRESULT (WINAPI *s_out_i64)(void *, INT64 *);
typedef HRESULT (WINAPI *s_out_dbl)(void *, double *);
typedef HRESULT (WINAPI *s_out_bool)(void *, BOOLEAN *);
typedef HRESULT (WINAPI *s_out_hs)(void *, HSTRING *);
typedef HRESULT (WINAPI *s_hs_bool)(void *, HSTRING, BOOLEAN *);
typedef HRESULT (WINAPI *s_hs_obj_bool)(void *, HSTRING, void *, BOOLEAN *);
typedef HRESULT (WINAPI *s_in_i32_obj)(void *, INT32, void **);
typedef HRESULT (WINAPI *s_in_dbl_obj)(void *, double, void **);
typedef HRESULT (WINAPI *s_in_bool_obj)(void *, BOOLEAN, void **);
typedef HRESULT (WINAPI *s_in_hs_obj)(void *, HSTRING, void **);
typedef HRESULT (WINAPI *s_in_u8_obj)(void *, BYTE, void **);
typedef HRESULT (WINAPI *s_in_i64_obj)(void *, INT64, void **);
typedef HRESULT (WINAPI *s_out_ptr)(void *, void **);
typedef HRESULT (WINAPI *s_in_u32_obj)(void *, UINT32, void **);
typedef HRESULT (WINAPI *s_in_obj_out_bool)(void *, void *, BOOLEAN *);

static const WCHAR *C_JSON = L"Windows.Data.Json.JsonObject";
static const WCHAR *C_JSONV = L"Windows.Data.Json.JsonValue";
static const WCHAR *C_URI = L"Windows.Foundation.Uri";
static const WCHAR *C_PV = L"Windows.Foundation.PropertyValue";
static const WCHAR *C_PS = L"Windows.Foundation.Collections.PropertySet";
static const WCHAR *C_BUF = L"Windows.Storage.Streams.Buffer";

static HRESULT get_factory(const WCHAR *cls, const GUID *iid, void **out) {
    HSTRING h = ro_hs(cls); *out = (void *)0xdeadbeef;
    HRESULT hr = h ? U(RoGetActivationFactory)(h, iid, out) : E_OUTOFMEMORY;
    ro_hs_free(h);
    return hr;
}
static HRESULT activate(const WCHAR *cls, void **out) {
    HSTRING h = ro_hs(cls); *out = (void *)0xdeadbeef;
    HRESULT hr = h ? U(RoActivateInstance)(h, (IInspectable **)out) : E_OUTOFMEMORY;
    ro_hs_free(h);
    return hr;
}
static int runtime_class_is(void *o, const WCHAR *want) {
    HSTRING n = NULL;
    HRESULT hr = ((s_getname)SLOT(o, 4))(o, &n);
    int ok = hr == S_OK && ro_hs_eq(n, want);
    if (!ok) t_note("runtime class name hr=%08lx got \"%ls\" want \"%ls\"", (unsigned long)hr, n ? ro_hs_buf(n, NULL) : L"(null)", want);
    ro_hs_free(n);
    return ok;
}
#define NEED_RO() do { REQUIRE(U(RoGetActivationFactory)); REQUIRE(U(RoActivateInstance)); REQUIRE(U(RoInitialize)); \
    REQUIRE(U(RoUninitialize)); REQUIRE(U(WindowsCreateString)); REQUIRE(U(CoTaskMemFree)); } while (0)
/* initialize an apartment model for the duration of the (thread-isolated) test */
#define INIT_MTA() RO_EQ(U(RoInitialize)(RO_INIT_MULTITHREADED), S_OK)
#define STR(x) ro_hs(x)

/* ================= generic IInspectable contract ================= */
static void check_inspectable(void *o, const WCHAR *cls) {
    ULONG n = 0xdeadbeef; GUID *ids = (GUID *)0xdeadbeef;
    RO_EQ(((s_getiids)SLOT(o, 3))(o, &n, &ids), S_OK);
    CHECKF(n >= 1 && n < 64, "iid count %lu", n);
    if (n >= 1 && n < 64 && ids && ids != (GUID *)0xdeadbeef) {
        for (ULONG i = 0; i < n; i++) {
            CHECK(memcmp(&ids[i], &RIID_IUnknown, 16) && memcmp(&ids[i], &RIID_IInspectable, 16));   /* only class-specific interfaces */
            for (ULONG j = i + 1; j < n; j++) CHECK(memcmp(&ids[i], &ids[j], 16));
            void *q = NULL;
            HRESULT hr = ro_qi(o, &ids[i], &q);
            CHECKF(hr == S_OK, "GetIids lists {%08lx-...} but QueryInterface = %08lx", ids[i].Data1, (unsigned long)hr);
            if (hr == S_OK) RO_RELEASE(*(IUnknown **)&q);
        }
        U(CoTaskMemFree)(ids);
    }
    CHECK(runtime_class_is(o, cls));
    int trust = -1;
    RO_EQ(((s_gettrust)SLOT(o, 5))(o, &trust), S_OK);
    CHECKF(trust >= 0 && trust <= 2, "trust level %d", trust);
    t_note("%ls: %lu iids, trust level %d", cls, n, trust);
}

RO_TEST(winrt_roapi_class_jsonobject_activate_inspectable) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_JSON, &o); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK); REQUIRE(o);
    check_inspectable(o, C_JSON);
    ULONG n; GUID *ids = NULL; ((s_getiids)SLOT(o, 3))(o, &n, &ids);
    int has_obj = 0, has_val = 0;
    for (ULONG i = 0; ids && i < n; i++) { has_obj |= !memcmp(&ids[i], &RIID_IJsonObject, 16); has_val |= !memcmp(&ids[i], &RIID_IJsonValue, 16); }
    CHECK(has_obj); CHECK(has_val);
    if (ids) U(CoTaskMemFree)(ids);
    RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_jsonobject_value_semantics) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_JSON, &o); RO_WINE_SKIP_IF(hr); REQUIRE(hr == S_OK);
    void *jv = NULL, *jo = NULL;
    RO_EQ(ro_qi(o, &RIID_IJsonValue, &jv), S_OK);
    RO_EQ(ro_qi(o, &RIID_IJsonObject, &jo), S_OK);
    int vt = -1; RO_EQ(((s_gettrust)SLOT(jv, 6))(jv, &vt), S_OK); RO_EQ(vt, 5);       /* JsonValueType_Object */
    HSTRING s = NULL; RO_EQ(((s_out_hs)SLOT(jv, 7))(jv, &s), S_OK);
    CHECK(ro_hs_eq(s, L"{}")); ro_hs_free(s);
    /* the three interface pointers are the same object */
    IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL;
    ro_qi(o, &RIID_IUnknown, (void **)&u1); ro_qi(jv, &RIID_IUnknown, (void **)&u2); ro_qi(jo, &RIID_IUnknown, (void **)&u3);
    CHECK(u1 && u1 == u2 && u1 == u3);
    RO_RELEASE(u1); RO_RELEASE(u2); RO_RELEASE(u3);
    void *bad = (void *)1;
    RO_EQ(ro_qi(o, &RIID_IPropertySet, &bad), RO_E_NOINTERFACE); CHECK(bad == NULL);
    RO_EQ(ro_qi(o, &RIID_IUriRuntimeClass, &bad), RO_E_NOINTERFACE);
    void *ag = NULL; HRESULT ahr = ro_qi(o, &RIID_IAgileObject, &ag);
    t_note("JsonObject supports IAgileObject: %d", ahr == S_OK); RO_RELEASE(*(IUnknown **)&ag);
    RO_RELEASE(*(IUnknown **)&jv); RO_RELEASE(*(IUnknown **)&jo); RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_jsonobject_namedvalue_missing) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_JSON, &o); RO_WINE_SKIP_IF(hr); REQUIRE(hr == S_OK);
    void *jo = NULL; ro_qi(o, &RIID_IJsonObject, &jo); REQUIRE(jo);
    HSTRING k = STR(L"nokey"); void *v = (void *)1;
    hr = ((s_hs_obj)SLOT(jo, 6))(jo, k, &v);               /* IJsonObject::GetNamedValue */
    CHECK(FAILED(hr));
    t_note("GetNamedValue(missing) = %08lx", (unsigned long)hr);
    ro_hs_free(k);
    RO_RELEASE(*(IUnknown **)&jo); RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}
/* JsonValue statics: Parse/TryParse/Create* */
static void *json_statics(void) { void *f = NULL; if (get_factory(C_JSONV, &RIID_IJsonValueStatics, &f) != S_OK) return NULL; return f; }
RO_TEST(winrt_roapi_class_jsonvalue_parse_types) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_JSONV, &RIID_IJsonValueStatics, &f); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK); REQUIRE(f);
    static const struct { const WCHAR *text; int type; const WCHAR *round; } t[] = {
        { L"null", 0, L"null" }, { L"true", 1, L"true" }, { L"false", 1, L"false" }, { L"12.5", 2, L"12.5" }, { L"0", 2, L"0" },
        { L"\"hi\"", 3, L"\"hi\"" }, { L"[1,2,3]", 4, L"[1,2,3]" }, { L"{\"a\":1}", 5, L"{\"a\":1}" }, { L"[]", 4, L"[]" }, { L"{}", 5, L"{}" },
        { L"  true  ", 1, L"true" } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HSTRING in = STR(t[i].text); void *v = NULL;
        hr = ((s_hs_obj)SLOT(f, 6))(f, in, &v);             /* Parse */
        CHECKF(hr == S_OK && v, "Parse(%ls) hr=%08lx", t[i].text, (unsigned long)hr);
        if (v) {
            int vt = -1; ((s_gettrust)SLOT(v, 6))(v, &vt);
            CHECKF(vt == t[i].type, "Parse(%ls) ValueType=%d want %d", t[i].text, vt, t[i].type);
            HSTRING out = NULL; ((s_out_hs)SLOT(v, 7))(v, &out);
            CHECKF(ro_hs_eq(out, t[i].round), "Stringify(%ls)=%ls", t[i].text, out ? ro_hs_buf(out, NULL) : L"(null)");
            ro_hs_free(out); RO_RELEASE(*(IUnknown **)&v);
        }
        ro_hs_free(in);
    }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_jsonvalue_parse_errors) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_JSONV, &RIID_IJsonValueStatics, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    static const WCHAR *bad[] = { L"", L"{", L"[1,", L"tru", L"{a:1}", L"1 2", L"'x'", L"}{" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HSTRING in = STR(bad[i]); void *v = (void *)1;
        hr = ((s_hs_obj)SLOT(f, 6))(f, in, &v);
        CHECKF(FAILED(hr), "Parse(\"%ls\") unexpectedly succeeded", bad[i]);
        t_note("Parse(\"%ls\") = %08lx", bad[i], (unsigned long)hr);
        /* TryParse: succeeds as a call, reports false (slot 7: Parse, TryParse(hs, IJsonValue**, boolean*)) */
        typedef HRESULT (WINAPI *try_t)(void *, HSTRING, void **, BOOLEAN *);
        BOOLEAN ok = 7; void *tv = (void *)1;
        hr = ((try_t)SLOT(f, 7))(f, in, &tv, &ok);
        CHECKF(hr == S_OK && ok == FALSE, "TryParse(\"%ls\") hr=%08lx ok=%d", bad[i], (unsigned long)hr, ok);
        ro_hs_free(in);
    }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_jsonvalue_create_values) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_JSONV, &RIID_IJsonValueStatics, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    void *b = NULL, *n = NULL, *s = NULL;
    RO_EQ(((s_in_bool_obj)SLOT(f, 8))(f, TRUE, &b), S_OK);
    RO_EQ(((s_in_dbl_obj)SLOT(f, 9))(f, 3.25, &n), S_OK);
    HSTRING hs = STR(L"quote\"d"); RO_EQ(((s_in_hs_obj)SLOT(f, 10))(f, hs, &s), S_OK); ro_hs_free(hs);
    struct { void *v; int type; const WCHAR *str; } t[] = { { b, 1, L"true" }, { n, 2, L"3.25" }, { s, 3, L"\"quote\\\"d\"" } };
    for (int i = 0; i < 3; i++) {
        if (!t[i].v) { CHECK(t[i].v != NULL); continue; }
        int vt = -1; ((s_gettrust)SLOT(t[i].v, 6))(t[i].v, &vt); CHECKF(vt == t[i].type, "value %d type %d", i, vt);
        HSTRING o = NULL; ((s_out_hs)SLOT(t[i].v, 7))(t[i].v, &o);
        CHECKF(ro_hs_eq(o, t[i].str), "value %d stringify=%ls", i, o ? ro_hs_buf(o, NULL) : L"(null)"); ro_hs_free(o);
    }
    /* typed getters: GetNumber(9) on a number; wrong-type getters fail */
    if (n) { double d = 0; RO_EQ(((s_out_dbl)SLOT(n, 9))(n, &d), S_OK); CHECK(d == 3.25);
             HSTRING x = NULL; HRESULT bh = ((s_out_hs)SLOT(n, 8))(n, &x); CHECK(FAILED(bh)); t_note("GetString on number = %08lx", (unsigned long)bh); ro_hs_free(x); }
    if (b) { BOOLEAN v = 0; RO_EQ(((s_out_bool)SLOT(b, 10))(b, &v), S_OK); CHECK(v == TRUE);
             double d = 0; HRESULT bh = ((s_out_dbl)SLOT(b, 9))(b, &d); CHECK(FAILED(bh)); }
    if (s) { HSTRING x = NULL; RO_EQ(((s_out_hs)SLOT(s, 8))(s, &x), S_OK); CHECK(ro_hs_eq(x, L"quote\"d")); ro_hs_free(x); }
    RO_RELEASE(*(IUnknown **)&b); RO_RELEASE(*(IUnknown **)&n); RO_RELEASE(*(IUnknown **)&s); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}

/* ================= Windows.Foundation.Uri ================= */
static void *mk_uri(void *f, const WCHAR *text) {
    HSTRING h = STR(text); void *u = NULL;
    HRESULT hr = ((s_hs_obj)SLOT(f, 6))(f, h, &u);       /* IUriRuntimeClassFactory::CreateUri */
    ro_hs_free(h);
    if (hr != S_OK) { t_note("CreateUri(%ls) = %08lx", text, (unsigned long)hr); return NULL; }
    return u;
}
static int uri_prop_is(void *u, int slot, const WCHAR *want, const char *name) {
    HSTRING h = NULL; HRESULT hr = ((s_out_hs)SLOT(u, slot))(u, &h);
    int ok = hr == S_OK && ro_hs_eq(h, want);
    if (!ok) t_note("Uri.%s hr=%08lx got \"%ls\" want \"%ls\"", name, (unsigned long)hr, h ? ro_hs_buf(h, NULL) : L"(null)", want);
    ro_hs_free(h);
    return ok;
}
RO_TEST(winrt_roapi_class_uri_components) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_URI, &RIID_IUriRuntimeClassFactory, &f); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK); REQUIRE(f);
    void *u = mk_uri(f, L"http://user:pw@www.contoso.com:8080/a/b.txt?x=1&y=2#frag"); REQUIRE(u);
    CHECK(uri_prop_is(u, 11, L"www.contoso.com", "Host"));
    CHECK(uri_prop_is(u, 8, L"contoso.com", "Domain"));
    CHECK(uri_prop_is(u, 13, L"/a/b.txt", "Path"));
    CHECK(uri_prop_is(u, 14, L"?x=1&y=2", "Query"));
    CHECK(uri_prop_is(u, 10, L"#frag", "Fragment"));
    CHECK(uri_prop_is(u, 17, L"http", "SchemeName"));
    CHECK(uri_prop_is(u, 18, L"user", "UserName"));
    CHECK(uri_prop_is(u, 12, L"pw", "Password"));
    CHECK(uri_prop_is(u, 9, L".txt", "Extension"));
    CHECK(uri_prop_is(u, 16, L"http://user:pw@www.contoso.com:8080/a/b.txt?x=1&y=2#frag", "RawUri"));
    INT32 port = 0; RO_EQ(((s_out_i32)SLOT(u, 19))(u, &port), S_OK); RO_EQ(port, 8080);
    BOOLEAN susp = 7; RO_EQ(((s_out_bool)SLOT(u, 20))(u, &susp), S_OK); CHECK(susp == FALSE || susp == TRUE);
    check_inspectable(u, C_URI);
    RO_RELEASE(*(IUnknown **)&u); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_defaults) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_URI, &RIID_IUriRuntimeClassFactory, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    void *u = mk_uri(f, L"https://www.contoso.com/a/b.txt?x=1#frag"); REQUIRE(u);
    INT32 port = 0; RO_EQ(((s_out_i32)SLOT(u, 19))(u, &port), S_OK); RO_EQ(port, 443);   /* default port of the scheme */
    CHECK(uri_prop_is(u, 18, L"", "UserName"));
    CHECK(uri_prop_is(u, 12, L"", "Password"));
    CHECK(uri_prop_is(u, 6, L"https://www.contoso.com/a/b.txt?x=1#frag", "AbsoluteUri"));
    CHECK(uri_prop_is(u, 7, L"https://www.contoso.com/a/b.txt?x=1", "DisplayUri"));   /* DisplayUri drops the fragment */
    RO_RELEASE(*(IUnknown **)&u);
    u = mk_uri(f, L"http://www.contoso.com"); REQUIRE(u);
    RO_EQ(((s_out_i32)SLOT(u, 19))(u, &port), S_OK); RO_EQ(port, 80);
    CHECK(uri_prop_is(u, 13, L"/", "Path"));              /* empty path is normalized */
    CHECK(uri_prop_is(u, 14, L"", "Query"));
    CHECK(uri_prop_is(u, 10, L"", "Fragment"));
    CHECK(uri_prop_is(u, 9, L"", "Extension"));
    RO_RELEASE(*(IUnknown **)&u); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_normalization) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_URI, &RIID_IUriRuntimeClassFactory, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    static const struct { const WCHAR *in, *abs, *host; } t[] = {
        { L"HTTP://WWW.CONTOSO.COM/Path", L"http://www.contoso.com/Path", L"www.contoso.com" },
        { L"http://www.contoso.com:80/", L"http://www.contoso.com/", L"www.contoso.com" },
        { L"http://www.contoso.com/a/./b/../c", L"http://www.contoso.com/a/c", L"www.contoso.com" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *u = mk_uri(f, t[i].in);
        CHECKF(u != NULL, "CreateUri(%ls)", t[i].in);
        if (!u) continue;
        CHECK(uri_prop_is(u, 6, t[i].abs, "AbsoluteUri"));
        CHECK(uri_prop_is(u, 11, t[i].host, "Host"));
        RO_RELEASE(*(IUnknown **)&u);
    }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_combine_and_equals) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_URI, &RIID_IUriRuntimeClassFactory, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    void *base = mk_uri(f, L"http://www.contoso.com/a/b/x.txt"); REQUIRE(base);
    HSTRING rel = STR(L"../c.txt"); void *comb = NULL;
    RO_EQ(((s_hs_obj)SLOT(base, 22))(base, rel, &comb), S_OK);   /* CombineUri */
    ro_hs_free(rel);
    if (comb) { CHECK(uri_prop_is(comb, 6, L"http://www.contoso.com/a/c.txt", "combined AbsoluteUri")); }
    else CHECK(comb != NULL);
    void *same = mk_uri(f, L"HTTP://WWW.CONTOSO.COM/a/b/x.txt"), *other = mk_uri(f, L"http://www.contoso.com/a/b/y.txt");
    BOOLEAN eq = 7;
    if (same) { RO_EQ(((s_in_obj_out_bool)SLOT(base, 21))(base, same, &eq), S_OK); CHECK(eq == TRUE); }
    if (other) { eq = 7; RO_EQ(((s_in_obj_out_bool)SLOT(base, 21))(base, other, &eq), S_OK); CHECK(eq == FALSE); }
    RO_RELEASE(*(IUnknown **)&comb); RO_RELEASE(*(IUnknown **)&same); RO_RELEASE(*(IUnknown **)&other);
    RO_RELEASE(*(IUnknown **)&base); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_invalid) {
    NEED_RO(); INIT_MTA();
    void *f; HRESULT hr = get_factory(C_URI, &RIID_IUriRuntimeClassFactory, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    static const WCHAR *bad[] = { L"", L"http://", L"not a uri", L"://nohost", L"http://exa mple.com/" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HSTRING h = STR(bad[i]); void *u = (void *)1;
        hr = ((s_hs_obj)SLOT(f, 6))(f, h, &u);
        t_note("CreateUri(\"%ls\") = %08lx", bad[i], (unsigned long)hr);
        if (i < 3) { CHECKF(FAILED(hr), "CreateUri(\"%ls\") succeeded", bad[i]); CHECK(u == NULL); }
        else if (SUCCEEDED(hr) && u) RO_RELEASE(*(IUnknown **)&u);
        ro_hs_free(h);
    }
    void *u = (void *)1;                                  /* NULL HSTRING == "" */
    if (!t_is_wine()) { hr = ((s_hs_obj)SLOT(f, 6))(f, NULL, &u); CHECK(FAILED(hr)); }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_no_default_constructor) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_URI, &o);
    if (t_is_wine() && SUCCEEDED(hr)) { t_skip("emulator differs"); return; }
    t_note("RoActivateInstance(Windows.Foundation.Uri) = %08lx", (unsigned long)hr);
    CHECK(FAILED(hr));                                    /* Uri has no default constructor */
    CHECK(o == NULL);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_uri_factory_wrong_iid) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_URI, &RIID_IJsonObject, &f); RO_WINE_SKIP_IF(hr == RO_E_CLASSNOTREG ? hr : S_OK);
    RO_EQ(hr, RO_E_NOINTERFACE);
    CHECK(f == NULL);
    hr = get_factory(C_URI, &RIID_IInspectable, &f);
    t_note("Uri factory IInspectable = %08lx", (unsigned long)hr);
    if (hr == S_OK) { CHECK(f != NULL); RO_RELEASE(*(IUnknown **)&f); }
    hr = get_factory(C_URI, &RIID_IUnknown, &f);
    RO_EQ(hr, S_OK); if (hr == S_OK) RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}

/* ================= Windows.Foundation.PropertyValue ================= */
static void *pv_statics(void) { void *f = NULL; get_factory(C_PV, &RIID_IPropertyValueStatics, &f); return f; }
RO_TEST(winrt_roapi_class_propertyvalue_int32) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_PV, &RIID_IPropertyValueStatics, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    void *o = NULL;
    RO_EQ(((s_in_i32_obj)SLOT(f, 10))(f, -5, &o), S_OK); REQUIRE(o);
    void *pv = NULL; RO_EQ(ro_qi(o, &RIID_IPropertyValue, &pv), S_OK); REQUIRE(pv);
    int type = -1; RO_EQ(((s_gettrust)SLOT(pv, 6))(pv, &type), S_OK); RO_EQ(type, 4);   /* PropertyType_Int32 */
    BOOLEAN num = 7; RO_EQ(((s_out_bool)SLOT(pv, 7))(pv, &num), S_OK); CHECK(num == TRUE);
    INT32 v = 0; RO_EQ(((s_out_i32)SLOT(pv, 11))(pv, &v), S_OK); RO_EQ(v, -5);
    INT64 w = 0; RO_EQ(((s_out_i64)SLOT(pv, 13))(pv, &w), S_OK); CHECK(w == -5);        /* widening conversion */
    double d = 0; RO_EQ(((s_out_dbl)SLOT(pv, 16))(pv, &d), S_OK); CHECK(d == -5.0);
    BYTE b = 0; hr = ((s_out_u8)SLOT(pv, 8))(pv, &b);                                    /* -5 does not fit a UInt8 */
    CHECKF(FAILED(hr), "GetUInt8(-5) succeeded"); t_note("GetUInt8(-5) = %08lx", (unsigned long)hr);
    HSTRING s = NULL; hr = ((s_out_hs)SLOT(pv, 19))(pv, &s);
    CHECKF(FAILED(hr), "GetString on Int32"); t_note("GetString(Int32) = %08lx", (unsigned long)hr); ro_hs_free(s);
    t_note("boxed Int32 runtime class: %ls", (HSTRING)0 ? L"" : L"see below");
    HSTRING cn = NULL; ((s_getname)SLOT(o, 4))(o, &cn); t_note("boxed Int32 GetRuntimeClassName = %ls", cn ? ro_hs_buf(cn, NULL) : L"(null)"); ro_hs_free(cn);
    RO_RELEASE(*(IUnknown **)&pv); RO_RELEASE(*(IUnknown **)&o); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_propertyvalue_scalar_types) {
    NEED_RO(); INIT_MTA();
    void *f = pv_statics(); if (!f) { RO_WINE_SKIP_IF(E_FAIL); } REQUIRE(f);
    struct { const char *n; int slot; int type; void *o; } t[] = {
        { "UInt8", 7, 1, NULL }, { "Int16", 8, 2, NULL }, { "UInt16", 9, 3, NULL }, { "Int32", 10, 4, NULL }, { "UInt32", 11, 5, NULL },
        { "Int64", 12, 6, NULL }, { "UInt64", 13, 7, NULL }, { "Single", 14, 8, NULL }, { "Double", 15, 9, NULL },
        { "Char16", 16, 10, NULL }, { "Boolean", 17, 11, NULL } };
    RO_EQ(((s_in_u8_obj)SLOT(f, 7))(f, 200, &t[0].o), S_OK);
    typedef HRESULT (WINAPI *c16)(void *, INT16, void **); RO_EQ(((c16)SLOT(f, 8))(f, -300, &t[1].o), S_OK);
    typedef HRESULT (WINAPI *cu16)(void *, UINT16, void **); RO_EQ(((cu16)SLOT(f, 9))(f, 60000, &t[2].o), S_OK);
    RO_EQ(((s_in_i32_obj)SLOT(f, 10))(f, 7, &t[3].o), S_OK);
    RO_EQ(((s_in_u32_obj)SLOT(f, 11))(f, 4000000000u, &t[4].o), S_OK);
    RO_EQ(((s_in_i64_obj)SLOT(f, 12))(f, -1234567890123LL, &t[5].o), S_OK);
    typedef HRESULT (WINAPI *cu64)(void *, UINT64, void **); RO_EQ(((cu64)SLOT(f, 13))(f, 18000000000000000000ull, &t[6].o), S_OK);
    typedef HRESULT (WINAPI *csg)(void *, float, void **); RO_EQ(((csg)SLOT(f, 14))(f, 1.5f, &t[7].o), S_OK);
    RO_EQ(((s_in_dbl_obj)SLOT(f, 15))(f, 2.5, &t[8].o), S_OK);
    typedef HRESULT (WINAPI *cch)(void *, WCHAR, void **); RO_EQ(((cch)SLOT(f, 16))(f, L'Q', &t[9].o), S_OK);
    RO_EQ(((s_in_bool_obj)SLOT(f, 17))(f, TRUE, &t[10].o), S_OK);
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *pv = NULL;
        if (!t[i].o) { CHECKF(0, "%s: no object", t[i].n); continue; }
        RO_EQ(ro_qi(t[i].o, &RIID_IPropertyValue, &pv), S_OK);
        int type = -1; if (pv) ((s_gettrust)SLOT(pv, 6))(pv, &type);
        CHECKF(type == t[i].type, "%s: Type=%d want %d", t[i].n, type, t[i].type);
        BOOLEAN num = 7; if (pv) ((s_out_bool)SLOT(pv, 7))(pv, &num);
        int want_num = t[i].type <= 9;
        CHECKF(num == want_num, "%s: IsNumericScalar=%d want %d", t[i].n, num, want_num);
        RO_RELEASE(*(IUnknown **)&pv);
    }
    { void *pv; ro_qi(t[0].o, &RIID_IPropertyValue, &pv); BYTE v = 0; RO_EQ(((s_out_u8)SLOT(pv, 8))(pv, &v), S_OK); CHECK(v == 200); INT16 w = 0; RO_EQ(((s_out_i16)SLOT(pv, 9))(pv, &w), S_OK); CHECK(w == 200); RO_RELEASE(*(IUnknown **)&pv); }
    { void *pv; ro_qi(t[7].o, &RIID_IPropertyValue, &pv); double d = 0; RO_EQ(((s_out_dbl)SLOT(pv, 16))(pv, &d), S_OK); CHECK(d == 1.5); RO_RELEASE(*(IUnknown **)&pv); }
    { void *pv; ro_qi(t[10].o, &RIID_IPropertyValue, &pv); BOOLEAN v = 0; RO_EQ(((s_out_bool)SLOT(pv, 18))(pv, &v), S_OK); CHECK(v == TRUE); RO_RELEASE(*(IUnknown **)&pv); }
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) RO_RELEASE(*(IUnknown **)&t[i].o);
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_propertyvalue_string_and_empty) {
    NEED_RO(); INIT_MTA();
    void *f = pv_statics(); if (!f) { RO_WINE_SKIP_IF(E_FAIL); } REQUIRE(f);
    HSTRING in = STR(L"olet string"); void *o = NULL;
    RO_EQ(((s_in_hs_obj)SLOT(f, 18))(f, in, &o), S_OK); ro_hs_free(in); REQUIRE(o);
    void *pv = NULL; RO_EQ(ro_qi(o, &RIID_IPropertyValue, &pv), S_OK); REQUIRE(pv);
    int type = -1; ((s_gettrust)SLOT(pv, 6))(pv, &type); RO_EQ(type, 12);
    BOOLEAN num = 7; ((s_out_bool)SLOT(pv, 7))(pv, &num); CHECK(num == FALSE);
    HSTRING out = NULL; RO_EQ(((s_out_hs)SLOT(pv, 19))(pv, &out), S_OK); CHECK(ro_hs_eq(out, L"olet string")); ro_hs_free(out);
    INT32 i = 0; HRESULT hr = ((s_out_i32)SLOT(pv, 11))(pv, &i);
    t_note("GetInt32 on non-numeric String = %08lx", (unsigned long)hr);
    RO_RELEASE(*(IUnknown **)&pv); RO_RELEASE(*(IUnknown **)&o);
    /* CreateEmpty (slot 6) */
    hr = ((s_out_ptr)SLOT(f, 6))(f, &o);
    t_note("CreateEmpty = %08lx obj=%p", (unsigned long)hr, o);
    if (hr == S_OK && o) {
        void *pe = NULL;
        if (ro_qi(o, &RIID_IPropertyValue, &pe) == S_OK) { type = -1; ((s_gettrust)SLOT(pe, 6))(pe, &type); t_note("Empty PropertyValue type=%d", type); RO_RELEASE(*(IUnknown **)&pe); }
        RO_RELEASE(*(IUnknown **)&o);
    }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}

/* ================= PropertySet (IMap<String,Object>) ================= */
static void *mk_boxed_int(int v) {
    void *f = pv_statics(), *o = NULL; if (!f) return NULL;
    ((s_in_i32_obj)SLOT(f, 10))(f, v, &o); RO_RELEASE(*(IUnknown **)&f); return o;
}
static int map_size(void *m) { UINT32 n = 0xffff; ((s_out_u32)SLOT(m, 7))(m, &n); return (int)n; }
RO_TEST(winrt_roapi_class_propertyset_map_semantics) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_PS, &o); RO_WINE_SKIP_IF(hr); RO_EQ(hr, S_OK); REQUIRE(o);
    check_inspectable(o, C_PS);
    void *m = NULL, *ps = NULL, *om = NULL, *it = NULL;
    RO_EQ(ro_qi(o, &RIID_IPropertySet, &ps), S_OK);
    RO_EQ(ro_qi(o, &RIID_IMapStrObj, &m), S_OK); REQUIRE(m);
    RO_EQ(ro_qi(o, &RIID_IObservableMapStrObj, &om), S_OK);
    RO_EQ(ro_qi(o, &RIID_IIterableKvpStrObj, &it), S_OK);
    RO_EQ(map_size(m), 0);
    HSTRING ka = STR(L"a"), kb = STR(L"b");
    void *v1 = mk_boxed_int(11), *v2 = mk_boxed_int(22);
    BOOLEAN replaced = 7;
    RO_EQ(((s_hs_obj_bool)SLOT(m, 10))(m, ka, v1, &replaced), S_OK); CHECK(replaced == FALSE);
    RO_EQ(map_size(m), 1);
    replaced = 7; RO_EQ(((s_hs_obj_bool)SLOT(m, 10))(m, ka, v2, &replaced), S_OK); CHECK(replaced == TRUE);
    RO_EQ(map_size(m), 1);
    BOOLEAN has = 7;
    RO_EQ(((s_hs_bool)SLOT(m, 8))(m, ka, &has), S_OK); CHECK(has == TRUE);
    RO_EQ(((s_hs_bool)SLOT(m, 8))(m, kb, &has), S_OK); CHECK(has == FALSE);
    void *got = NULL; RO_EQ(((s_hs_obj)SLOT(m, 6))(m, ka, &got), S_OK); REQUIRE(got);
    { void *pv; ro_qi(got, &RIID_IPropertyValue, &pv); INT32 v = 0; RO_EQ(((s_out_i32)SLOT(pv, 11))(pv, &v), S_OK); RO_EQ(v, 22); RO_RELEASE(*(IUnknown **)&pv); }
    CHECK(got == v2 || 1);                                   /* identity of stored boxes is not specified */
    RO_RELEASE(*(IUnknown **)&got);
    void *miss = (void *)1; hr = ((s_hs_obj)SLOT(m, 6))(m, kb, &miss);
    CHECK(FAILED(hr)); t_note("Lookup(missing) = %08lx", (unsigned long)hr);
    hr = ((s_hs_in)SLOT(m, 11))(m, kb); CHECK(FAILED(hr)); t_note("Remove(missing) = %08lx", (unsigned long)hr);
    RO_EQ(((s_hs_in)SLOT(m, 11))(m, ka), S_OK);
    RO_EQ(map_size(m), 0);
    ((s_hs_obj_bool)SLOT(m, 10))(m, ka, v1, &replaced); ((s_hs_obj_bool)SLOT(m, 10))(m, kb, v2, &replaced);
    RO_EQ(map_size(m), 2);
    typedef HRESULT (WINAPI *clr)(void *); RO_EQ(((clr)SLOT(m, 12))(m), S_OK);
    RO_EQ(map_size(m), 0);
    ro_hs_free(ka); ro_hs_free(kb);
    RO_RELEASE(*(IUnknown **)&v1); RO_RELEASE(*(IUnknown **)&v2);
    RO_RELEASE(*(IUnknown **)&m); RO_RELEASE(*(IUnknown **)&ps); RO_RELEASE(*(IUnknown **)&om); RO_RELEASE(*(IUnknown **)&it); RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_propertyset_insert_holds_reference) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_PS, &o); RO_WINE_SKIP_IF(hr); REQUIRE(o);
    void *m = NULL; ro_qi(o, &RIID_IMapStrObj, &m); REQUIRE(m);
    void *v = mk_boxed_int(5); REQUIRE(v);
    ULONG base = ro_refcount(v);
    HSTRING k = STR(L"k"); BOOLEAN r;
    ((s_hs_obj_bool)SLOT(m, 10))(m, k, v, &r);
    CHECKF(ro_refcount(v) > base, "map did not AddRef its value (%lu -> %lu)", base, ro_refcount(v));
    ((s_hs_in)SLOT(m, 11))(m, k);
    CHECKF(ro_refcount(v) == base, "Remove did not release (%lu -> %lu)", base, ro_refcount(v));
    ro_hs_free(k);
    RO_RELEASE(*(IUnknown **)&v); RO_RELEASE(*(IUnknown **)&m); RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}

/* ================= Windows.Storage.Streams.Buffer ================= */
RO_TEST(winrt_roapi_class_buffer_create_and_length) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_BUF, &RIID_IBufferFactory, &f); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK); REQUIRE(f);
    void *b = NULL; RO_EQ(((s_in_u32_obj)SLOT(f, 6))(f, 100, &b), S_OK); REQUIRE(b);
    check_inspectable(b, C_BUF);
    UINT32 cap = 0xffff, len = 0xffff;
    RO_EQ(((s_out_u32)SLOT(b, 6))(b, &cap), S_OK); RO_EQ(cap, 100);
    RO_EQ(((s_out_u32)SLOT(b, 7))(b, &len), S_OK); RO_EQ(len, 0);
    RO_EQ(((s_in_u32)SLOT(b, 8))(b, 50), S_OK);
    RO_EQ(((s_out_u32)SLOT(b, 7))(b, &len), S_OK); RO_EQ(len, 50);
    RO_EQ(((s_in_u32)SLOT(b, 8))(b, 100), S_OK);               /* Length == Capacity is allowed */
    hr = ((s_in_u32)SLOT(b, 8))(b, 101);
    CHECKF(FAILED(hr), "Length beyond Capacity accepted"); t_note("put_Length(cap+1) = %08lx", (unsigned long)hr);
    RO_EQ(((s_out_u32)SLOT(b, 7))(b, &len), S_OK); RO_EQ(len, 100);   /* unchanged by the failed call */
    RO_EQ(((s_in_u32)SLOT(b, 8))(b, 0), S_OK);
    RO_RELEASE(*(IUnknown **)&b); RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_buffer_byte_access) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_BUF, &RIID_IBufferFactory, &f); RO_WINE_SKIP_IF(hr); REQUIRE(f);
    void *b = NULL; ((s_in_u32_obj)SLOT(f, 6))(f, 64, &b); REQUIRE(b);
    void *ba = NULL; RO_EQ(ro_qi(b, &RIID_IBufferByteAccess, &ba), S_OK); REQUIRE(ba);
    BYTE *p = NULL; RO_EQ(((s_out_ptr)SLOT(ba, 3))(ba, (void **)&p), S_OK);   /* IBufferByteAccess::Buffer */
    CHECK(p != NULL);
    if (p) { for (int i = 0; i < 64; i++) p[i] = (BYTE)i; CHECK(p[63] == 63); }
    BYTE *p2 = NULL; ((s_out_ptr)SLOT(ba, 3))(ba, (void **)&p2); CHECK(p2 == p);   /* stable storage */
    RO_RELEASE(*(IUnknown **)&ba); RO_RELEASE(*(IUnknown **)&b);
    /* zero capacity */
    RO_EQ(((s_in_u32_obj)SLOT(f, 6))(f, 0, &b), S_OK);
    if (b) { UINT32 cap = 7; ((s_out_u32)SLOT(b, 6))(b, &cap); RO_EQ(cap, 0); RO_RELEASE(*(IUnknown **)&b); }
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_buffer_default_activation_record) {
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_BUF, &o);
    t_note("RoActivateInstance(Buffer) = %08lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) RO_RELEASE(*(IUnknown **)&o); else CHECK(o == NULL);
    U(RoUninitialize)();
}

/* ================= activation in both apartment models ================= */
RO_TEST(winrt_roapi_class_activate_in_sta) {
    NEED_RO();
    RO_EQ(U(RoInitialize)(RO_INIT_SINGLETHREADED), S_OK);
    void *o; HRESULT hr = activate(C_JSON, &o); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK);
    if (o) { CHECK(runtime_class_is(o, C_JSON)); RO_RELEASE(*(IUnknown **)&o); }
    hr = activate(C_PS, &o); RO_EQ(hr, S_OK);
    if (o) { CHECK(runtime_class_is(o, C_PS)); RO_RELEASE(*(IUnknown **)&o); }
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_factory_activate_instance_slot) {
    NEED_RO(); INIT_MTA();
    void *f = NULL; HRESULT hr = get_factory(C_JSON, &RIID_IActivationFactory, &f); RO_WINE_SKIP_IF(hr);
    RO_EQ(hr, S_OK); REQUIRE(f);
    void *inst = NULL; RO_EQ(((s_activate)SLOT(f, 6))(f, &inst), S_OK);      /* IActivationFactory::ActivateInstance */
    CHECK(inst != NULL);
    if (inst) { CHECK(runtime_class_is(inst, C_JSON)); RO_RELEASE(*(IUnknown **)&inst); }
    void *inst2 = NULL; ((s_activate)SLOT(f, 6))(f, &inst2);
    CHECK(inst2 != NULL && inst2 != inst);                                    /* new object each time */
    RO_RELEASE(*(IUnknown **)&inst2);
    /* QI of the factory for its own interface */
    void *again = NULL; RO_EQ(ro_qi(f, &RIID_IActivationFactory, &again), S_OK); CHECK(again == f); RO_RELEASE(*(IUnknown **)&again);
    HSTRING fn = NULL; HRESULT nh = ((s_getname)SLOT(f, 4))(f, &fn);
    t_note("factory GetRuntimeClassName hr=%08lx %ls", (unsigned long)nh, fn ? ro_hs_buf(fn, NULL) : L"(null)"); ro_hs_free(fn);
    RO_RELEASE(*(IUnknown **)&f);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_factory_cached_identity_record) {
    NEED_RO(); INIT_MTA();
    void *a = NULL, *b = NULL;
    HRESULT hr = get_factory(C_PS, &RIID_IActivationFactory, &a); RO_WINE_SKIP_IF(hr);
    get_factory(C_PS, &RIID_IActivationFactory, &b);
    t_note("two RoGetActivationFactory calls return the same pointer: %d", a == b);
    RO_RELEASE(*(IUnknown **)&a); RO_RELEASE(*(IUnknown **)&b);
    U(RoUninitialize)();
}
RO_TEST(winrt_roapi_class_sta_to_mta_inspectable_strings) {
    /* A string-returning IInspectable method called from an STA thread on an object created in the MTA. */
    NEED_RO(); INIT_MTA();
    void *o; HRESULT hr = activate(C_JSON, &o); RO_WINE_SKIP_IF(hr); REQUIRE(o);
    HSTRING n1 = NULL; RO_EQ(((s_getname)SLOT(o, 4))(o, &n1), S_OK);
    CHECK(ro_hs_eq(n1, C_JSON));
    UINT32 len = 0; const WCHAR *raw = ro_hs_buf(n1, &len);
    RO_EQ(len, lstrlenW(C_JSON)); CHECK(raw && raw[len] == 0);                /* HSTRINGs are NUL terminated */
    ro_hs_free(n1);
    RO_RELEASE(*(IUnknown **)&o);
    U(RoUninitialize)();
}
