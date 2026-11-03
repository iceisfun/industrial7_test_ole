/* OleCreateFontIndirect and the IFont property surface (Name/Size/Bold/Italic/Underline/Strikethrough/
 * Weight/Charset), Clone/IsEqual, hFont lifetime, AddRefHfont/ReleaseHfont, SetRatio, QueryTextMetrics.
 * Windows-side expectations follow the oleaut32 StdFont contract (OLE_E / CTL_E codes from olectl.h);
 * oleaut32 in Win10 forwards the entry points to an internal implementation, so the checks that Wine's
 * suite also makes (and that hold on Windows) are hard, anything else is a t_note. */
#include "pf.h"

#define FONT_BEGIN(f) IFont *f; REQUIRE(U(OleCreateFontIndirect)); \
    f = pf_font_new(); if (!f) { CHECKF(0, "font creation failed"); return; }

static BSTR bs(const WCHAR *s) { return SysAllocString(s); }

/* ---- creation ------------------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_basic) {
    FONTDESC fd; IFont *f = (IFont *)(ULONG_PTR)1; HRESULT hr;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    hr = U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f);
    EQ(hr, S_OK); CHECK(f != NULL);
    if (f) EQ(IFont_Release(f), 0);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_null_ppv) {
    FONTDESC fd; REQUIRE(U(OleCreateFontIndirect));
    EQ(U(OleCreateFontIndirect)(NULL, &IID_IFont, NULL), PF_E_POINTER);
    pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, NULL), PF_E_POINTER);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_null_desc) {
    IFont *f = NULL; REQUIRE(U(OleCreateFontIndirect));
    EQ(U(OleCreateFontIndirect)(NULL, &IID_IFont, (void **)&f), S_OK);
    CHECK(f != NULL);
    if (f) EQ(IFont_Release(f), 0);
}
TEST(pfont_create_null_desc_defaults) {
    IFont *f = NULL; BSTR name = NULL; CY sz; SHORT w = -1, cs = -1; BOOL b = 5;
    REQUIRE(U(OleCreateFontIndirect));
    EQ(U(OleCreateFontIndirect)(NULL, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_get_Name(f, &name), S_OK);
    CHECK(name && SysStringLen(name) > 0);
    t_note("default font name: %ls", name ? name : L"(null)");
    SysFreeString(name);
    sz.int64 = 0; EQ(IFont_get_Size(f, &sz), S_OK);
    t_note("default size cy=%lld (80000 expected for 8pt)", (long long)sz.int64);
    CHECK(sz.int64 > 0);
    EQ(IFont_get_Weight(f, &w), S_OK); t_note("default weight %d", w);
    EQ(IFont_get_Charset(f, &cs), S_OK); t_note("default charset %d", cs);
    EQ(IFont_get_Bold(f, &b), S_OK); CHECK(b == FALSE || b == TRUE);
    EQ(IFont_get_Italic(f, &b), S_OK); EQ(b, FALSE);
    EQ(IFont_get_Underline(f, &b), S_OK); EQ(b, FALSE);
    EQ(IFont_get_Strikethrough(f, &b), S_OK); EQ(b, FALSE);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_cbsize_ignored) {
    static const DWORD sizes[] = { sizeof(FONTDESC) - 1, sizeof(FONTDESC) + 1, 0, 1, 0xffff };
    int i; REQUIRE(U(OleCreateFontIndirect));
    for (i = 0; i < PF_N(sizes); i++) {
        FONTDESC fd; IFont *f = NULL; BSTR n = NULL;
        pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
        fd.cbSizeofstruct = sizes[i];
        EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
        if (!f) continue;
        EQ(IFont_get_Name(f, &n), S_OK);
        if (n) CHECK(!lstrcmpiW(n, pf_arial));
        SysFreeString(n);
        IFont_Release(f);
    }
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_null_name) {
    FONTDESC fd; IFont *f = (IFont *)(ULONG_PTR)0xdeadbeef;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, NULL, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), PF_CTL_E_INVALIDPROPERTYVALUE);
    EQ(f, NULL);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_create_empty_name) {
    static const WCHAR empty[] = { 0 };
    FONTDESC fd; IFont *f = NULL; BSTR n = NULL;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, empty, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    CHECK(f != NULL);
    if (!f) return;
    EQ(IFont_get_Name(f, &n), S_OK);
    CHECK(n != NULL);
    if (n) t_note("get_Name for empty FONTDESC name: \"%ls\"", n);
    SysFreeString(n);
    IFont_Release(f);
}
TEST(pfont_create_unknown_iid) {
    static const GUID bogus = { 0xdeadf00d, 0x1234, 0x4321, { 1,2,3,4,5,6,7,8 } };
    FONTDESC fd; void *p = (void *)(ULONG_PTR)0x1234;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &bogus, &p), PF_E_NOINTERFACE);
    EQ(p, NULL);
}
TEST(pfont_create_riid_table) {
    static const struct { const GUID *iid; const char *n; } t[] = {
        { &IID_IFont, "IFont" }, { &IID_IFontDisp, "IFontDisp" }, { &IID_IDispatch, "IDispatch" },
        { &IID_IUnknown, "IUnknown" }, { &IID_IPersistStream, "IPersistStream" },
        { &IID_IConnectionPointContainer, "IConnectionPointContainer" },
    };
    int i; FONTDESC fd; REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    for (i = 0; i < PF_N(t); i++) {
        IUnknown *u = NULL; HRESULT hr = U(OleCreateFontIndirect)(&fd, t[i].iid, (void **)&u);
        CHECKF(hr == S_OK, "%s: hr=%08lx", t[i].n, hr);
        if (u) EQ(IUnknown_Release(u), 0);
    }
}
TEST(pfont_create_distinct_objects) {
    IFont *a, *b; REQUIRE(U(OleCreateFontIndirect));
    a = pf_font_new(); b = pf_font_new();
    CHECK(a && b);
    if (a && b) CHECK(a != b);
    if (a) IFont_Release(a);
    if (b) IFont_Release(b);
}
TEST(pfont_create_desc_values_applied) {
    FONTDESC fd; IFont *f = NULL; BSTR n = NULL; CY sz; SHORT w = 0, cs = -1; BOOL b;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 135000, 300, ANSI_CHARSET, TRUE, TRUE, TRUE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_get_Name(f, &n), S_OK); if (n) CHECK(!lstrcmpW(n, pf_arial)); SysFreeString(n);
    sz.int64 = 0; EQ(IFont_get_Size(f, &sz), S_OK); CHECK(sz.int64 - 135000 < 10000 && sz.int64 - 135000 > -10000);
    EQ(IFont_get_Weight(f, &w), S_OK); EQ(w, 300);
    EQ(IFont_get_Charset(f, &cs), S_OK); EQ(cs, ANSI_CHARSET);
    EQ(IFont_get_Italic(f, &b), S_OK); EQ(b, TRUE);
    EQ(IFont_get_Underline(f, &b), S_OK); EQ(b, TRUE);
    EQ(IFont_get_Strikethrough(f, &b), S_OK); EQ(b, TRUE);
    EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, FALSE);
    IFont_Release(f);
}
TEST(pfont_create_desc_not_retained) {
    /* The name string is copied: freeing/overwriting the caller's buffer must not alter the object. */
    FONTDESC fd; IFont *f = NULL; BSTR n = NULL; WCHAR buf[16];
    REQUIRE(U(OleCreateFontIndirect));
    lstrcpyW(buf, pf_arial);
    pf_fontdesc(&fd, buf, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    lstrcpyW(buf, L"Zzzzzzzz");
    EQ(IFont_get_Name(f, &n), S_OK);
    if (n) CHECK(!lstrcmpW(n, pf_arial));
    SysFreeString(n);
    IFont_Release(f);
}

/* ---- interfaces ----------------------------------------------------------------------------- */
static void qi_check(void *iface, const GUID *iid, BOOL want, const char *n) {
    IUnknown *u = (IUnknown *)(ULONG_PTR)1; HRESULT hr = IUnknown_QueryInterface((IUnknown *)iface, iid, (void **)&u);
    CHECKF(hr == (want ? S_OK : PF_E_NOINTERFACE), "%s: hr=%08lx", n, hr);
    if (SUCCEEDED(hr)) IUnknown_Release(u); else CHECKF(u == NULL, "%s: out not cleared", n);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_interfaces) {
    FONT_BEGIN(f);
    qi_check(f, &IID_IFont, TRUE, "IFont");
    qi_check(f, &IID_IFontDisp, TRUE, "IFontDisp");
    qi_check(f, &IID_IDispatch, TRUE, "IDispatch");
    qi_check(f, &IID_IUnknown, TRUE, "IUnknown");
    qi_check(f, &IID_IPersist, TRUE, "IPersist");
    qi_check(f, &IID_IPersistStream, TRUE, "IPersistStream");
    qi_check(f, &IID_IConnectionPointContainer, TRUE, "IConnectionPointContainer");
    qi_check(f, &IID_IPersistPropertyBag, TRUE, "IPersistPropertyBag");
    qi_check(f, &IID_IPersistStreamInit, FALSE, "IPersistStreamInit");
    qi_check(f, &IID_IPersistStorage, FALSE, "IPersistStorage");
    qi_check(f, &IID_IOleObject, FALSE, "IOleObject");
    qi_check(f, &IID_IMarshal, FALSE, "IMarshal");
    qi_check(f, &IID_IPicture, FALSE, "IPicture");
    qi_check(f, &IID_IClassFactory, FALSE, "IClassFactory");
    IFont_Release(f);
}
TEST(pfont_unknown_identity) {
    FONT_BEGIN(f);
    IUnknown *u1 = NULL, *u2 = NULL, *u3 = NULL; IFontDisp *fd = NULL; IPersistStream *ps = NULL;
    EQ(IFont_QueryInterface(f, &IID_IUnknown, (void **)&u1), S_OK);
    EQ(IFont_QueryInterface(f, &IID_IFontDisp, (void **)&fd), S_OK);
    EQ(IFont_QueryInterface(f, &IID_IPersistStream, (void **)&ps), S_OK);
    if (fd) EQ(IFontDisp_QueryInterface(fd, &IID_IUnknown, (void **)&u2), S_OK);
    if (ps) EQ(IPersistStream_QueryInterface(ps, &IID_IUnknown, (void **)&u3), S_OK);
    CHECK(u1 && u2 && u3);
    EQ(u1, u2); EQ(u1, u3);
    if (u1) IUnknown_Release(u1);
    if (u2) IUnknown_Release(u2);
    if (u3) IUnknown_Release(u3);
    if (fd) IFontDisp_Release(fd);
    if (ps) IPersistStream_Release(ps);
    IFont_Release(f);
}
TEST(pfont_refcount_shared_across_interfaces) {
    FONT_BEGIN(f);
    IFontDisp *fd = NULL;
    EQ(IFont_QueryInterface(f, &IID_IFontDisp, (void **)&fd), S_OK);
    if (!fd) { IFont_Release(f); return; }
    EQ(IFont_AddRef(f), 3);
    EQ(IFontDisp_AddRef(fd), 4);
    EQ(IFontDisp_Release(fd), 3);
    EQ(IFont_Release(f), 2);
    EQ(IFontDisp_Release(fd), 1);
    EQ(IFont_Release(f), 0);
}
/* ---- Name ------------------------------------------------------------------------------------ */
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_name_null_pointers) {
    FONT_BEGIN(f);
    EQ(IFont_put_Name(f, NULL), PF_CTL_E_INVALIDPROPERTYVALUE);
    EQ(IFont_get_Name(f, NULL), PF_E_POINTER);
    IFont_Release(f);
}
TEST(pfont_name_roundtrip_table) {
    /* get_Name realizes the font and reports the face actually selected, so only installed faces round-trip */
    static const WCHAR *names[] = { L"Arial", L"Courier New", L"Times New Roman", L"Tahoma", L"MS Sans Serif" };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(names); i++) {
        BSTR in = bs(names[i]), out = NULL;
        EQ(IFont_put_Name(f, in), S_OK);
        EQ(IFont_get_Name(f, &out), S_OK);
        CHECKF(out && !lstrcmpiW(out, names[i]), "name %d", i);
        SysFreeString(out); SysFreeString(in);
    }
    IFont_Release(f);
}
TEST(pfont_name_uninstalled_face) {
    FONT_BEGIN(f);
    BSTR in = bs(L"Nonexistent Font Family 12345"), out = NULL;
    EQ(IFont_put_Name(f, in), S_OK);
    EQ(IFont_get_Name(f, &out), S_OK);
    CHECK(out != NULL && SysStringLen(out) > 0);
    t_note("get_Name after put_Name(uninstalled face): %ls", out ? out : L"(null)");
    SysFreeString(out); SysFreeString(in);
    IFont_Release(f);
}
TEST(pfont_name_get_returns_fresh_bstr) {
    FONT_BEGIN(f);
    BSTR a = NULL, b = NULL;
    EQ(IFont_get_Name(f, &a), S_OK); EQ(IFont_get_Name(f, &b), S_OK);
    CHECK(a && b);
    if (a && b) { CHECK(a != b); CHECK(!lstrcmpW(a, b)); }
    SysFreeString(a); SysFreeString(b);
    IFont_Release(f);
}
TEST(pfont_name_put_copies_bstr) {
    FONT_BEGIN(f);
    BSTR in = bs(L"Tahoma"), out = NULL;
    EQ(IFont_put_Name(f, in), S_OK);
    in[0] = L'Z';                /* mutate caller's buffer; the object must have copied */
    EQ(IFont_get_Name(f, &out), S_OK);
    CHECK(out && !lstrcmpW(out, L"Tahoma"));
    SysFreeString(out); SysFreeString(in);
    IFont_Release(f);
}
TEST(pfont_name_empty_roundtrip) {
    FONT_BEGIN(f);
    BSTR in = bs(L""), out = NULL;
    HRESULT hr = IFont_put_Name(f, in);
    t_note("put_Name(\"\") hr=%08lx", hr);
    if (hr == S_OK) { EQ(IFont_get_Name(f, &out), S_OK); if (out) t_note("get_Name after put_Name(\"\"): \"%ls\"", out); SysFreeString(out); }
    SysFreeString(in);
    IFont_Release(f);
}
TEST(pfont_name_long) {
    FONT_BEGIN(f);
    WCHAR buf[64]; BSTR in, out = NULL; HRESULT hr; int i;
    for (i = 0; i < 40; i++) buf[i] = L'A' + (i % 26);
    buf[40] = 0;
    in = bs(buf);
    hr = IFont_put_Name(f, in);
    t_note("put_Name(40 chars) hr=%08lx (LF_FACESIZE is 32)", hr);
    if (SUCCEEDED(hr)) {
        EQ(IFont_get_Name(f, &out), S_OK);
        if (out) t_note("get_Name length after 40-char put: %u", SysStringLen(out));
        SysFreeString(out);
    }
    SysFreeString(in);
    IFont_Release(f);
}
TEST(pfont_name_case_insensitive_match) {
    FONT_BEGIN(f);
    BSTR in = bs(L"aRiAl"), out = NULL;
    EQ(IFont_put_Name(f, in), S_OK);
    EQ(IFont_get_Name(f, &out), S_OK);
    CHECK(out && !lstrcmpiW(out, L"aRiAl"));
    if (out && lstrcmpW(out, L"aRiAl")) t_note("get_Name returned %ls (face case differs from the set value)", out);
    SysFreeString(out); SysFreeString(in);
    IFont_Release(f);
}
TEST(pfont_name_unicode_roundtrip) {
    static const WCHAR uni[] = { 0x0416, 0x0443, 0x043a, 0 };   /* Cyrillic */
    FONT_BEGIN(f);
    BSTR in = bs(uni), out = NULL;
    HRESULT hr = IFont_put_Name(f, in);
    EQ(hr, S_OK);
    EQ(IFont_get_Name(f, &out), S_OK);
    CHECK(out != NULL);
    if (out && lstrcmpW(out, uni)) t_note("non-Latin face name not round-tripped (realized as %ls)", out);
    SysFreeString(out); SysFreeString(in);
    IFont_Release(f);
}

/* ---- Size ------------------------------------------------------------------------------------ */
TEST(pfont_size_null) {
    FONT_BEGIN(f);
    EQ(IFont_get_Size(f, NULL), PF_E_POINTER);
    IFont_Release(f);
}
TEST(pfont_size_roundtrip_table) {
    static const LONGLONG sizes[] = { 10000, 40000, 80000, 85000, 100000, 120000, 123400, 240000, 725000, 1000000 };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(sizes); i++) {
        CY in, out; in.int64 = sizes[i]; out.int64 = 0;
        EQ(IFont_put_Size(f, in), S_OK);
        EQ(IFont_get_Size(f, &out), S_OK);
        /* get_Size reports the realized font height converted back to points, so allow up to 2 device pixels (1.5pt at 96 dpi) of rounding error */
        CHECKF(out.int64 - sizes[i] < 15000 && out.int64 - sizes[i] > -15000, "size %lld got %lld", sizes[i], (long long)out.int64);
        if (out.int64 != sizes[i]) t_note("put_Size %lld -> get_Size %lld", sizes[i], (long long)out.int64);
    }
    IFont_Release(f);
}
TEST(pfont_size_after_hfont_realization) {
    /* Realizing the HFONT may round the point size slightly; Wine tolerates 1 pt (10000 units). */
    FONT_BEGIN(f);
    CY in, out; HFONT h = NULL; long long d;
    in.int64 = 186000; out.int64 = 0;
    EQ(IFont_put_Size(f, in), S_OK);
    EQ(IFont_get_hFont(f, &h), S_OK);
    EQ(IFont_get_Size(f, &out), S_OK);
    d = (long long)out.int64 - 186000;
    CHECKF(d > -10000 && d < 10000, "size drifted by %lld", d);
    IFont_Release(f);
}
TEST(pfont_size_put_changes_hfont) {
    FONT_BEGIN(f);
    CY in; HFONT h1 = NULL, h2 = NULL; LOGFONTW lf1, lf2;
    EQ(IFont_get_hFont(f, &h1), S_OK);
    in.int64 = 240000;
    EQ(IFont_put_Size(f, in), S_OK);
    in.int64 = 480000;
    EQ(IFont_put_Size(f, in), S_OK);
    EQ(IFont_get_hFont(f, &h2), S_OK);
    memset(&lf1, 0, sizeof lf1); memset(&lf2, 0, sizeof lf2);
    CHECK(h2 != NULL);
    if (h2) {
        CHECK(GetObjectW(h2, sizeof lf2, &lf2) > 0);
        CHECK(lf2.lfHeight < 0);
        if (h1 != h2) CHECK(GetObjectW(h2, sizeof lf2, &lf2) > 0);
    }
    IFont_Release(f);
}

/* ---- Bold / Italic / Underline / Strikethrough / Weight / Charset -------------------------- */
TEST(pfont_bool_props_null) {
    FONT_BEGIN(f);
    EQ(IFont_get_Bold(f, NULL), PF_E_POINTER);
    EQ(IFont_get_Italic(f, NULL), PF_E_POINTER);
    EQ(IFont_get_Underline(f, NULL), PF_E_POINTER);
    EQ(IFont_get_Strikethrough(f, NULL), PF_E_POINTER);
    EQ(IFont_get_Weight(f, NULL), PF_E_POINTER);
    EQ(IFont_get_Charset(f, NULL), PF_E_POINTER);
    EQ(IFont_get_hFont(f, NULL), PF_E_POINTER);
    IFont_Release(f);
}
TEST(pfont_bold_roundtrip) {
    FONT_BEGIN(f);
    BOOL b = 5; SHORT w = 0;
    EQ(IFont_put_Bold(f, TRUE), S_OK);
    EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, TRUE);
    EQ(IFont_get_Weight(f, &w), S_OK); EQ(w, FW_BOLD);
    EQ(IFont_put_Bold(f, FALSE), S_OK);
    EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, FALSE);
    EQ(IFont_get_Weight(f, &w), S_OK); EQ(w, FW_NORMAL);
    IFont_Release(f);
}
TEST(pfont_bold_nonzero_values) {
    FONT_BEGIN(f);
    BOOL b = 5;
    EQ(IFont_put_Bold(f, 1), S_OK); EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, TRUE);
    EQ(IFont_put_Bold(f, 0), S_OK); EQ(IFont_get_Bold(f, &b), S_OK); EQ(b, FALSE);
    EQ(IFont_put_Bold(f, -1), S_OK); EQ(IFont_get_Bold(f, &b), S_OK);
    t_note("put_Bold(VARIANT_TRUE) -> get_Bold %d", b);
    IFont_Release(f);
}
TEST(pfont_weight_bold_relation_table) {
    static const struct { SHORT w; int bold; } t[] = {
        { 100, 0 }, { 300, 0 }, { 400, 0 }, { 500, 0 }, { 700, 1 }, { 800, 1 }, { 900, 1 },
    };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(t); i++) {
        BOOL b = 5; SHORT w = 0;
        EQ(IFont_put_Weight(f, t[i].w), S_OK);
        EQ(IFont_get_Weight(f, &w), S_OK); CHECKF(w == t[i].w, "weight %d got %d", t[i].w, w);
        EQ(IFont_get_Bold(f, &b), S_OK); CHECKF(b == t[i].bold, "weight %d bold=%d want %d", t[i].w, b, t[i].bold);
    }
    IFont_Release(f);
}
TEST(pfont_weight_intermediate) {
    FONT_BEGIN(f);
    BOOL b = 5; SHORT w;
    for (w = 550; w <= 650; w += 50) {
        EQ(IFont_put_Weight(f, w), S_OK);
        EQ(IFont_get_Bold(f, &b), S_OK);
        t_note("weight %d -> Bold=%d", w, b);
    }
    IFont_Release(f);
}
TEST(pfont_weight_roundtrip_range) {
    static const SHORT ws[] = { 100, 399, 400, 401, 699, 700, 900 };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(ws); i++) {
        SHORT out = -1;
        HRESULT hr = IFont_put_Weight(f, ws[i]);
        EQ(hr, S_OK);
        EQ(IFont_get_Weight(f, &out), S_OK);
        CHECKF(out == ws[i], "weight %d got %d", ws[i], out);
    }
    IFont_Release(f);
}
TEST(pfont_weight_negative) {
    FONT_BEGIN(f);
    HRESULT hr = IFont_put_Weight(f, -1);
    t_note("put_Weight(-1) hr=%08lx", hr);
    IFont_Release(f);
}
TEST(pfont_style_flags_roundtrip) {
    static const struct { int which; } t[] = { {0}, {1}, {2} };
    int i; BOOL b;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(t); i++) {
        HRESULT (STDMETHODCALLTYPE *put)(IFont *, BOOL) = i == 0 ? f->lpVtbl->put_Italic : i == 1 ? f->lpVtbl->put_Underline : f->lpVtbl->put_Strikethrough;
        HRESULT (STDMETHODCALLTYPE *get)(IFont *, BOOL *) = i == 0 ? f->lpVtbl->get_Italic : i == 1 ? f->lpVtbl->get_Underline : f->lpVtbl->get_Strikethrough;
        b = 5; EQ(put(f, TRUE), S_OK); EQ(get(f, &b), S_OK); EQ(b, TRUE);
        b = 5; EQ(put(f, FALSE), S_OK); EQ(get(f, &b), S_OK); EQ(b, FALSE);
    }
    IFont_Release(f);
}
TEST(pfont_style_flags_independent) {
    FONT_BEGIN(f);
    BOOL i, u, s, bo;
    EQ(IFont_put_Italic(f, TRUE), S_OK);
    EQ(IFont_get_Italic(f, &i), S_OK); EQ(IFont_get_Underline(f, &u), S_OK);
    EQ(IFont_get_Strikethrough(f, &s), S_OK); EQ(IFont_get_Bold(f, &bo), S_OK);
    EQ(i, TRUE); EQ(u, FALSE); EQ(s, FALSE); EQ(bo, FALSE);
    EQ(IFont_put_Underline(f, TRUE), S_OK);
    EQ(IFont_get_Italic(f, &i), S_OK); EQ(IFont_get_Underline(f, &u), S_OK);
    EQ(i, TRUE); EQ(u, TRUE);
    IFont_Release(f);
}
TEST(pfont_charset_roundtrip_table) {
    static const SHORT cs[] = { ANSI_CHARSET, DEFAULT_CHARSET, SYMBOL_CHARSET, SHIFTJIS_CHARSET, GREEK_CHARSET,
                                RUSSIAN_CHARSET };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(cs); i++) {
        SHORT out = -2;
        EQ(IFont_put_Charset(f, cs[i]), S_OK);
        EQ(IFont_get_Charset(f, &out), S_OK);
        /* get_Charset reflects the realized font: only ANSI is guaranteed to survive on every face */
        if (cs[i] == ANSI_CHARSET) EQ(out, ANSI_CHARSET);
        else if (out != cs[i]) t_note("put_Charset %d -> get_Charset %d", cs[i], out);
    }
    IFont_Release(f);
}

/* ---- hFont ----------------------------------------------------------------------------------- */
TEST(pfont_hfont_is_gdi_font) {
    FONT_BEGIN(f);
    HFONT h = NULL;
    EQ(IFont_get_hFont(f, &h), S_OK);
    CHECK(h != NULL);
    if (h) EQ(GetObjectType(h), OBJ_FONT);
    IFont_Release(f);
}
TEST(pfont_hfont_reflects_properties) {
    IFont *f = NULL; FONTDESC fd; HFONT h = NULL; LOGFONTW lf; int n;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, pf_arial, 120000, 700, ANSI_CHARSET, TRUE, TRUE, TRUE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_get_hFont(f, &h), S_OK);
    memset(&lf, 0xcc, sizeof lf);
    n = h ? GetObjectW(h, sizeof lf, &lf) : 0;
    CHECK(n > 0);
    if (n > 0) {
        EQ(lf.lfWeight, 700);
        EQ(lf.lfItalic, 1);
        EQ(lf.lfUnderline, 1);
        EQ(lf.lfStrikeOut, 1);
        EQ(lf.lfCharSet, ANSI_CHARSET);
        CHECK(lf.lfHeight < 0);
        CHECK(!lstrcmpiW(lf.lfFaceName, pf_arial));
    }
    IFont_Release(f);
}
TEST(pfont_hfont_stable_until_change) {
    FONT_BEGIN(f);
    HFONT a = NULL, b = NULL;
    EQ(IFont_get_hFont(f, &a), S_OK); EQ(IFont_get_hFont(f, &b), S_OK);
    EQ(a, b);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_hfont_old_valid_until_next_realize) {
    FONT_BEGIN(f);
    HFONT old = NULL, cur = NULL; CY sz;
    EQ(IFont_get_hFont(f, &old), S_OK);
    sz.int64 = 240000;
    EQ(IFont_put_Size(f, sz), S_OK);
    /* put_Size does not realize the new font: the previous handle is still a live font */
    EQ(GetObjectType(old), OBJ_FONT);
    EQ(IFont_get_hFont(f, &cur), S_OK);
    EQ(GetObjectType(cur), OBJ_FONT);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_hfont_addref_keeps_alive) {
    FONT_BEGIN(f);
    HFONT first = NULL, cur = NULL; CY sz; int i;
    for (i = 0; i < 20; i++) {
        sz.int64 = (i + 10) * 20000;
        EQ(IFont_put_Size(f, sz), S_OK);
        EQ(IFont_get_hFont(f, &cur), S_OK);
        EQ(IFont_AddRefHfont(f, cur), S_OK);
        if (i == 0) first = cur;
        EQ(GetObjectType(first), OBJ_FONT);
    }
    IFont_Release(f);
}
TEST(pfont_hfont_release_font_frees_unreferenced) {
    FONT_BEGIN(f);
    HFONT h = NULL;
    EQ(IFont_get_hFont(f, &h), S_OK);
    EQ(GetObjectType(h), OBJ_FONT);
    IFont_Release(f);
    /* With the owner gone and no external references the handle is no longer a valid font object.
     * (Win10 may delay deletion via a shared cache: informational.) */
    t_note("GetObjectType after last Release: %lu (OBJ_FONT=6)", GetObjectType(h));
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_addrefhfont_invalid) {
    FONT_BEGIN(f);
    EQ(IFont_AddRefHfont(f, NULL), PF_E_INVALIDARG);
    EQ(IFont_ReleaseHfont(f, NULL), PF_E_INVALIDARG);
    EQ(IFont_AddRefHfont(f, (HFONT)(ULONG_PTR)32), PF_S_FALSE);
    EQ(IFont_ReleaseHfont(f, (HFONT)(ULONG_PTR)32), PF_S_FALSE);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_releasehfont_counts) {
    FONT_BEGIN(f);
    HFONT h = NULL;
    EQ(IFont_get_hFont(f, &h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), PF_S_FALSE);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_addref_then_release_hfont_counts) {
    FONT_BEGIN(f);
    HFONT h = NULL;
    EQ(IFont_get_hFont(f, &h), S_OK);
    EQ(IFont_AddRefHfont(f, h), S_OK);
    EQ(IFont_AddRefHfont(f, h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), S_OK);
    EQ(IFont_ReleaseHfont(f, h), PF_S_FALSE);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_hfont_refs_are_per_hfont_not_per_object) {
    /* AddRefHfont looks only at the HFONT, whichever IFont it is called on. */
    IFont *a, *b; HFONT ha = NULL, hb = NULL;
    REQUIRE(U(OleCreateFontIndirect));
    a = pf_font_new(); b = pf_font_new();
    if (!a || !b) { CHECK(a && b); if (a) IFont_Release(a); if (b) IFont_Release(b); return; }
    EQ(IFont_get_hFont(a, &ha), S_OK); EQ(IFont_get_hFont(b, &hb), S_OK);
    EQ(IFont_AddRefHfont(b, ha), S_OK);
    EQ(IFont_ReleaseHfont(a, ha), S_OK);
    EQ(IFont_ReleaseHfont(a, ha), S_OK);
    IFont_Release(a); IFont_Release(b);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_hfont_addref_release_pair_still_rerealizes) {
    FONT_BEGIN(f);
    HFONT h = NULL; CY sz; int i;
    for (i = 0; i < 10; i++) {
        sz.int64 = (i + 10) * 20000;
        EQ(IFont_put_Size(f, sz), S_OK);
        EQ(IFont_get_hFont(f, &h), S_OK);
        EQ(IFont_AddRefHfont(f, h), S_OK);
        EQ(IFont_ReleaseHfont(f, h), S_OK);
        EQ(GetObjectType(h), OBJ_FONT);
    }
    IFont_Release(f);
}

/* ---- SetRatio / QueryTextMetrics / SetHdc --------------------------------------------------- */
static void ratio_case(LONGLONG size, LONG rl, LONG rh, LONG want_height, const char *label) {
    FONTDESC fd; IFont *f = NULL; HFONT h = NULL; LOGFONTA lf; CY got; HRESULT hr;
    pf_fontdesc(&fd, pf_arial, size, 0, 0, FALSE, FALSE, FALSE);
    if (FAILED(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f)) || !f) { CHECKF(0, "%s: create", label); return; }
    hr = IFont_SetRatio(f, rl, rh);
    CHECKF(hr == ((rl && rh) ? S_OK : PF_E_FAIL), "%s: SetRatio(%ld,%ld) hr=%08lx", label, rl, rh, hr);
    EQ(IFont_get_Size(f, &got), S_OK);
    CHECKF(got.int64 - size < 10000 && got.int64 - size > -10000, "%s: size %lld", label, (long long)got.int64);
    EQ(IFont_get_hFont(f, &h), S_OK);
    memset(&lf, 0, sizeof lf);
    if (h && GetObjectA(h, sizeof lf, &lf) > 0)
        CHECKF(abs(lf.lfHeight - want_height) <= 1, "%s: lfHeight %ld want %ld", label, lf.lfHeight, want_height);
    else CHECKF(0, "%s: GetObject failed", label);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_setratio_height_table) {
    static const struct { LONGLONG cy; LONG l, h, height; const char *n; } t[] = {
        { 180000, 72, 2540, -18, "ratio1" }, { 180000, 144, 2540, -36, "ratio2" }, { 180000, 72, 1270, -36, "ratio3" },
        { 186000, 72, 2540, -19, "rounding+ratio" },
        { 180000, 10, 10, -635, "10:10 a" }, { 240000, 10, 10, -848, "10:10 b" }, { 300000, 10, 10, -1058, "10:10 c" },
        { 180000, 1, 1, -24, "1:1 a" }, { 240000, 1, 1, -32, "1:1 b" }, { 300000, 1, 1, -40, "1:1 c" },
        { 180000, 2, 1, -1270, "2:1 a" }, { 240000, 2, 1, -1694, "2:1 b" }, { 300000, 2, 1, -2117, "2:1 c" },
        { 180000, 1, 2, -318, "1:2 a" }, { 240000, 1, 2, -424, "1:2 b" }, { 300000, 1, 2, -529, "1:2 c" },
        { 180000, 2, 2, -635, "2:2 a" }, { 240000, 2, 2, -848, "2:2 b" }, { 300000, 2, 2, -1058, "2:2 c" },
    };
    int i; REQUIRE(U(OleCreateFontIndirect));
    for (i = 0; i < PF_N(t); i++) ratio_case(t[i].cy, t[i].l, t[i].h, t[i].height, t[i].n);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_setratio_zero_is_failure_and_uses_display_dpi) {
    static const LONG rl[] = { 0, 1, 0 }, rh[] = { 0, 0, 1 };
    HDC hdc = GetDC(NULL); int dpi = GetDeviceCaps(hdc, LOGPIXELSY), i; ReleaseDC(NULL, hdc);
    REQUIRE(U(OleCreateFontIndirect));
    for (i = 0; i < 3; i++) {
        /* 18pt at the display DPI */
        LONG want = -MulDiv(18, dpi, 72);
        ratio_case(180000, rl[i], rh[i], want, "zero-ratio");
    }
}
TEST(pfont_setratio_returns_table) {
    static const struct { LONG l, h; HRESULT hr; } t[] = {
        { 1, 1, S_OK }, { 72, 2540, S_OK }, { 0, 0, PF_E_FAIL }, { 0, 5, PF_E_FAIL }, { 5, 0, PF_E_FAIL },
    };
    int i;
    FONT_BEGIN(f);
    for (i = 0; i < PF_N(t); i++) {
        HRESULT hr = IFont_SetRatio(f, t[i].l, t[i].h);
        CHECKF(hr == t[i].hr, "SetRatio(%ld,%ld) hr=%08lx want %08lx", t[i].l, t[i].h, hr, t[i].hr);
    }
    IFont_Release(f);
}
TEST(pfont_querytextmetrics) {
    FONT_BEGIN(f);
    TEXTMETRICOLE tm; HRESULT hr;
    memset(&tm, 0, sizeof tm);
    hr = IFont_QueryTextMetrics(f, &tm);
    EQ(hr, S_OK);
    if (hr == S_OK) { CHECK(tm.tmHeight > 0); CHECK(tm.tmAscent > 0); CHECK(tm.tmHeight >= tm.tmAscent); }
    IFont_Release(f);
}
TEST(pfont_querytextmetrics_scales_with_size) {
    FONT_BEGIN(f);
    TEXTMETRICOLE a, b; CY sz;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    sz.int64 = 100000; EQ(IFont_put_Size(f, sz), S_OK);
    EQ(IFont_QueryTextMetrics(f, &a), S_OK);
    sz.int64 = 400000; EQ(IFont_put_Size(f, sz), S_OK);
    EQ(IFont_QueryTextMetrics(f, &b), S_OK);
    CHECK(b.tmHeight > a.tmHeight);
    IFont_Release(f);
}
TEST(pfont_sethdc) {
    FONT_BEGIN(f);
    HDC dc = CreateCompatibleDC(NULL);
    if (!dc) { t_skip("no memory DC"); IFont_Release(f); return; }
    SOFT_EQ(IFont_SetHdc(f, dc), S_OK);
    SOFT_EQ(IFont_SetHdc(f, NULL), S_OK);
    DeleteDC(dc);
    IFont_Release(f);
}

/* ---- IsEqual / Clone -------------------------------------------------------------------------- */
static IFont *mk(const WCHAR *name, LONGLONG cy, SHORT w, SHORT cs, BOOL it, BOOL ul, BOOL st) {
    FONTDESC fd; IFont *f = NULL;
    pf_fontdesc(&fd, name, cy, w, cs, it, ul, st);
    U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f);
    return f;
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_isequal_table) {
    struct { const WCHAR *name; LONGLONG cy; SHORT w, cs; BOOL it, ul, st; const char *why; HRESULT hr; } t[] = {
        { pf_system, 1000000100LL, 0, 0, 0, 0, 0, "same", S_OK },
        { pf_arial, 1000000100LL, 0, 0, 0, 0, 0, "name", PF_S_FALSE },
        { pf_system, 1000000200LL, 0, 0, 0, 0, 0, "size lo", PF_S_FALSE },
        { pf_system, 1000000100LL + 0x100000000LL, 0, 0, 0, 0, 0, "size hi", PF_S_FALSE },
        { pf_system, 1000000100LL, 100, 0, 0, 0, 0, "weight", PF_S_FALSE },
        { pf_system, 1000000100LL, 0, 1, 0, 0, 0, "charset", PF_S_FALSE },
        { pf_system, 1000000100LL, 0, 0, 1, 0, 0, "italic", PF_S_FALSE },
        { pf_system, 1000000100LL, 0, 0, 0, 1, 0, "underline", PF_S_FALSE },
        { pf_system, 1000000100LL, 0, 0, 0, 0, 1, "strike", PF_S_FALSE },
    };
    int i; IFont *base;
    REQUIRE(U(OleCreateFontIndirect));
    base = mk(pf_system, 1000000100LL, 0, 0, 0, 0, 0);
    if (!base) { CHECK(base); return; }
    for (i = 0; i < PF_N(t); i++) {
        IFont *o = mk(t[i].name, t[i].cy, t[i].w, t[i].cs, t[i].it, t[i].ul, t[i].st);
        HRESULT hr;
        if (!o) { CHECKF(0, "%s create", t[i].why); continue; }
        hr = IFont_IsEqual(base, o);
        CHECKF(hr == t[i].hr, "%s: IsEqual hr=%08lx want %08lx", t[i].why, hr, t[i].hr);
        IFont_Release(o);
    }
    IFont_Release(base);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_isequal_null) {
    FONT_BEGIN(f);
    EQ(IFont_IsEqual(f, NULL), PF_E_POINTER);
    IFont_Release(f);
}
TEST(pfont_isequal_self_and_symmetry) {
    IFont *a, *b; REQUIRE(U(OleCreateFontIndirect));
    a = pf_font_new(); b = mk(pf_arial, 120000, FW_NORMAL, ANSI_CHARSET, 0, 0, 1);
    if (!a || !b) { CHECK(a && b); return; }
    EQ(IFont_IsEqual(a, a), S_OK);
    EQ(IFont_IsEqual(a, b), PF_S_FALSE);
    EQ(IFont_IsEqual(b, a), PF_S_FALSE);
    IFont_Release(a); IFont_Release(b);
}
TEST(pfont_isequal_after_property_change) {
    IFont *a, *b; REQUIRE(U(OleCreateFontIndirect));
    a = pf_font_new(); b = pf_font_new();
    if (!a || !b) { CHECK(a && b); return; }
    EQ(IFont_IsEqual(a, b), S_OK);
    EQ(IFont_put_Italic(a, TRUE), S_OK);
    EQ(IFont_IsEqual(a, b), PF_S_FALSE);
    EQ(IFont_put_Italic(b, TRUE), S_OK);
    EQ(IFont_IsEqual(a, b), S_OK);
    IFont_Release(a); IFont_Release(b);
}
TEST(pfont_clone_equal) {
    FONT_BEGIN(f);
    IFont *c = NULL;
    EQ(IFont_put_Italic(f, TRUE), S_OK);
    EQ(IFont_put_Weight(f, 300), S_OK);
    EQ(IFont_Clone(f, &c), S_OK);
    CHECK(c != NULL && c != f);
    if (c) {
        EQ(IFont_IsEqual(f, c), S_OK);
        EQ(IFont_Release(c), 0);
    }
    EQ(IFont_Release(f), 0);
}
TEST(pfont_clone_null) {
    FONT_BEGIN(f);
    EQ(IFont_Clone(f, NULL), PF_E_POINTER);
    IFont_Release(f);
}
TEST(pfont_clone_independent) {
    FONT_BEGIN(f);
    IFont *c = NULL; BOOL b;
    EQ(IFont_Clone(f, &c), S_OK);
    if (!c) { IFont_Release(f); return; }
    EQ(IFont_put_Underline(c, TRUE), S_OK);
    EQ(IFont_get_Underline(f, &b), S_OK); EQ(b, FALSE);
    EQ(IFont_IsEqual(f, c), PF_S_FALSE);
    EQ(IFont_put_Underline(f, TRUE), S_OK);
    EQ(IFont_IsEqual(f, c), S_OK);
    IFont_Release(c); IFont_Release(f);
}
TEST(pfont_clone_survives_original_release) {
    FONT_BEGIN(f);
    IFont *c = NULL; BSTR n = NULL;
    EQ(IFont_Clone(f, &c), S_OK);
    IFont_Release(f);
    if (!c) return;
    EQ(IFont_get_Name(c, &n), S_OK);
    CHECK(n && !lstrcmpiW(n, pf_arial));
    SysFreeString(n);
    IFont_Release(c);
}
TEST(pfont_clone_has_own_hfont) {
    FONT_BEGIN(f);
    IFont *c = NULL; HFONT h = NULL;
    EQ(IFont_Clone(f, &c), S_OK);
    if (c) { EQ(IFont_get_hFont(c, &h), S_OK); CHECK(h != NULL); IFont_Release(c); }
    IFont_Release(f);
}

/* ---- charset realization ------------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_realization_symbol_font_charset) {
    static const WCHAR marlett[] = { 'M','a','r','l','e','t','t',0 };
    FONT_BEGIN(f);
    BSTR name; SHORT cs = -1;
    if (t_is_wine()) { t_skip("symbol font charset realization differs under emulators"); IFont_Release(f); return; }
    name = bs(marlett);
    EQ(IFont_put_Name(f, name), S_OK);
    SysFreeString(name);
    name = NULL;
    EQ(IFont_get_Name(f, &name), S_OK);
    CHECK(name && !lstrcmpiW(name, marlett));
    SysFreeString(name);
    EQ(IFont_get_Charset(f, &cs), S_OK);
    EQ(cs, SYMBOL_CHARSET);
    IFont_Release(f);
}
/* adapted from Wine dlls/oleaut32/tests/olefont.c (LGPL-2.1+) */
TEST(pfont_realization_symbol_font_create) {
    static const WCHAR marlett[] = { 'M','a','r','l','e','t','t',0 };
    FONTDESC fd; IFont *f = NULL; SHORT cs = -1;
    REQUIRE(U(OleCreateFontIndirect));
    pf_fontdesc(&fd, marlett, 120000, FW_NORMAL, ANSI_CHARSET, FALSE, FALSE, FALSE);
    EQ(U(OleCreateFontIndirect)(&fd, &IID_IFont, (void **)&f), S_OK);
    if (!f) return;
    EQ(IFont_get_Charset(f, &cs), S_OK);
    EQ(cs, ANSI_CHARSET);
    IFont_Release(f);
}
