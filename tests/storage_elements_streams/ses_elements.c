/* IStorage element operations: CreateStream/CreateStorage/OpenStream/OpenStorage/DestroyElement/RenameElement,
 * name rules, STGM validation, error codes. DF_TEST variants run on an in-memory and on a file docfile. */
#include "ses.h"

static void fill_name(WCHAR *w, int n, WCHAR c) { for (int i = 0; i < n; i++) w[i] = c; w[n] = 0; }

/* ---- CreateStream argument validation ---- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_createstream_arg_errors) {
    IStream *s = (IStream *)(ULONG_PTR)1;
    HR(IStorage_CreateStream(stg, L"CONTENTS", MODE_RW, 1, 0, &s), STG_E_INVALIDPARAMETER);
    HR(IStorage_CreateStream(stg, L"CONTENTS", MODE_RW, 0, 1, &s), STG_E_INVALIDPARAMETER);
    HR(IStorage_CreateStream(stg, L"CONTENTS", MODE_RW, 0, 0, NULL), STG_E_INVALIDPOINTER);
    HR(IStorage_CreateStream(stg, NULL, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME);
    CHECK(!ses_exists_stream(stg, L"CONTENTS"));
}
DF_TEST(ses_el_createstream_out_ptr_nulled_on_failure) {
    IStream *s = (IStream *)(ULONG_PTR)1;
    HR(IStorage_CreateStream(stg, NULL, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME);
    CHECKW(s == NULL); /* docs: set to NULL when an error occurs */
    s = (IStream *)(ULONG_PTR)1;
    HR(IStorage_CreateStream(stg, L"x", STGM_READWRITE, 0, 0, &s), STG_E_INVALIDFLAG);
    CHECKW(s == NULL);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_createstream_invalid_modes) {
    static const DWORD bad[] = {
        STGM_READWRITE, STGM_READ, STGM_WRITE,
        STGM_SHARE_DENY_NONE | STGM_READWRITE, STGM_SHARE_DENY_NONE | STGM_READ,
    };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IStream *s = NULL;
        HRESULT hr = IStorage_CreateStream(stg, L"CONTENTS", bad[i], 0, 0, &s);
        CHECKF(hr == STG_E_INVALIDFLAG, "mode 0x%lx: hr=0x%08lx", bad[i], (unsigned long)hr);
        if (s) IStream_Release(s);
    }
    CHECK(!ses_exists_stream(stg, L"CONTENTS"));
}
DF_TEST(ses_el_createstream_unsupported_flags) {
    IStream *s = NULL;
    /* docs: STGM_DELETEONRELEASE and STGM_TRANSACTED are not supported for streams */
    OBSERVE_HR(IStorage_CreateStream(stg, L"a", MODE_RW | STGM_DELETEONRELEASE, 0, 0, &s)); if (s) { IStream_Release(s); s = NULL; IStorage_DestroyElement(stg, L"a"); }
    OBSERVE_HR(IStorage_CreateStream(stg, L"b", MODE_RW | STGM_TRANSACTED, 0, 0, &s)); if (s) { IStream_Release(s); s = NULL; IStorage_DestroyElement(stg, L"b"); }
}
DF_TEST(ses_el_createstream_name_length_limit) {
    WCHAR n[128]; IStream *s = NULL;
    fill_name(n, 31, L'a');
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), S_OK); if (s) IStream_Release(s);
    CHECK(ses_exists_stream(stg, n));
    fill_name(n, 32, L'b'); s = NULL;
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME); CHECK(s == NULL);
    fill_name(n, 33, L'c');
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME);
    fill_name(n, 100, L'd');
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME);
}
DF_TEST(ses_el_createstorage_name_length_limit) {
    WCHAR n[128]; IStorage *s = NULL;
    fill_name(n, 31, L'a');
    HR(IStorage_CreateStorage(stg, n, MODE_RW, 0, 0, &s), S_OK); if (s) IStorage_Release(s);
    CHECK(ses_exists_storage(stg, n));
    fill_name(n, 32, L'b'); s = NULL;
    HR(IStorage_CreateStorage(stg, n, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME); CHECK(s == NULL);
}
DF_TEST(ses_el_name_length_limit_counts_utf16_units) {
    WCHAR n[128]; IStream *s = NULL;
    fill_name(n, 31, 0x4e2d); /* CJK character: one UTF-16 unit each */
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), S_OK); if (s) IStream_Release(s);
    CHECK(ses_exists_stream(stg, n));
    fill_name(n, 32, 0x4e2d);
    HR(IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s), STG_E_INVALIDNAME);
}
DF_TEST(ses_el_empty_name_observed) {
    IStream *s = NULL;
    OBSERVE_HR(IStorage_CreateStream(stg, L"", MODE_RW, 0, 0, &s)); if (s) IStream_Release(s);
    IStorage *g = NULL;
    OBSERVE_HR(IStorage_CreateStorage(stg, L"", MODE_RW, 0, 0, &g)); if (g) IStorage_Release(g);
}
DF_TEST(ses_el_reserved_leading_chars_allowed) {
    /* the 0x01..0x1f leading characters are "reserved for OLE" but ordinary CreateStream accepts them
     * (OLE itself creates \001CompObj, \005SummaryInformation ...) */
    int bad = 0;
    for (WCHAR c = 1; c < 0x20; c++) {
        WCHAR n[8] = { c, 'X', 'y', 'z', 0 };
        IStream *s = NULL;
        if (IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s) != S_OK) bad++; else IStream_Release(s);
    }
    EQ(bad, 0);
    for (WCHAR c = 1; c < 0x20; c++) { WCHAR n[8] = { c, 'X', 'y', 'z', 0 }; if (!ses_exists_stream(stg, n)) bad++; }
    EQ(bad, 0);
}
DF_TEST(ses_el_reserved_leading_chars_storage) {
    int bad = 0;
    for (WCHAR c = 1; c < 0x20; c++) {
        WCHAR n[8] = { c, 'S', 't', 'g', 0 }; IStorage *s = NULL;
        if (IStorage_CreateStorage(stg, n, MODE_RW, 0, 0, &s) != S_OK) bad++; else IStorage_Release(s);
    }
    EQ(bad, 0);
}
DF_TEST(ses_el_special_chars_observed) {
    static const WCHAR chars[] = { L'/', L'\\', L':', L'!' };
    for (unsigned i = 0; i < 4; i++) {
        WCHAR n[8] = { 'a', chars[i], 'b', 0 }; IStream *s = NULL;
        HRESULT hr = IStorage_CreateStream(stg, n, MODE_RW, 0, 0, &s);
        t_note("CreateStream name with U+%04x -> 0x%08lx", chars[i], (unsigned long)hr);
        if (s) IStream_Release(s);
    }
}
DF_TEST(ses_el_valid_odd_names) {
    static const WCHAR *names[] = { L"a", L"A B", L"with space ", L" lead", L"dot.dot", L"a.b.c", L"été", L"中文", L"~$tmp", L"#%&()", L"CONTENTS", L"Ole10Native" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        IStream *s = NULL;
        HRESULT hr = IStorage_CreateStream(stg, names[i], MODE_RW, 0, 0, &s);
        CHECKF(hr == S_OK, "name #%u hr=0x%08lx", i, (unsigned long)hr);
        if (s) IStream_Release(s);
        CHECKF(ses_exists_stream(stg, names[i]), "name #%u missing after create", i);
    }
}
/* ---- create semantics ---- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_createstream_existing_fails) {
    IStream *s = ses_mkstream(stg, L"CONTENTS"); if (!s) return; IStream_Release(s);
    IStream *t = (IStream *)(ULONG_PTR)1;
    HR(IStorage_CreateStream(stg, L"CONTENTS", MODE_RW, 0, 0, &t), STG_E_FILEALREADYEXISTS);
    CHECKW(t == NULL);
    HR(IStorage_CreateStream(stg, L"CONTENTS", MODE_RW | STGM_FAILIFTHERE, 0, 0, &t), STG_E_FILEALREADYEXISTS);
}
DF_TEST(ses_el_createstream_name_case_insensitive) {
    IStream *s = ses_mkstream(stg, L"Mixed"); if (!s) return; IStream_Release(s), s = NULL;
    HR(IStorage_CreateStream(stg, L"MIXED", MODE_RW, 0, 0, &s), STG_E_FILEALREADYEXISTS);
    HR(IStorage_CreateStream(stg, L"mixed", MODE_RW, 0, 0, &s), STG_E_FILEALREADYEXISTS);
    HR(IStorage_OpenStream(stg, L"mIxEd", NULL, MODE_RD, 0, &s), S_OK); if (s) IStream_Release(s);
    HR(IStorage_DestroyElement(stg, L"MiXeD"), S_OK);
    CHECK(!ses_exists_stream(stg, L"Mixed"));
}
DF_TEST(ses_el_name_case_preserved_in_enum) {
    IStream *s = ses_mkstream(stg, L"CaSe_Name"); if (!s) return; IStream_Release(s);
    WCHAR l[4][40]; int n = ses_list(a, stg, l, NULL, NULL, 4);
    EQ(n, 1); if (n == 1) EXPECT_WSTR(l[0], L"CaSe_Name");
}
DF_TEST(ses_el_create_replace_existing_stream) {
    IStream *old = ses_mkstream(stg, L"R"), *nw = NULL; if (!old) return;
    ses_write_all(old, "old data", 8);
    HR(IStorage_CreateStream(stg, L"R", MODE_CREATE & ~STGM_CREATE | STGM_CREATE, 0, 0, &nw), S_OK);
    if (nw) { EQ(ses_stream_size(nw), 0); IStream_Release(nw); }
    char b[8] = {0};
    EQ(ses_get(stg, L"R", b, 8), 0);
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+): the replaced stream object is invalidated */
    HR(IStream_Write(old, "z", 1, NULL), STG_E_REVERTED);
    HR(ses_seek(old, 0, STREAM_SEEK_SET, NULL), STG_E_REVERTED);
    IStream_Release(old);
}
DF_TEST(ses_el_create_replace_with_storage_collision) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (g) IStorage_Release(g);
    IStream *s = NULL;
    HR(IStorage_CreateStream(stg, L"S", MODE_RW, 0, 0, &s), STG_E_FILEALREADYEXISTS);
    HR(IStorage_CreateStream(stg, L"T", MODE_RW, 0, 0, &s), S_OK); if (s) IStream_Release(s);
    g = NULL;
    HR(IStorage_CreateStorage(stg, L"T", MODE_RW, 0, 0, &g), STG_E_FILEALREADYEXISTS);
    HR(IStorage_CreateStorage(stg, L"s", MODE_RW, 0, 0, &g), STG_E_FILEALREADYEXISTS);
}
DF_TEST(ses_el_createstorage_arg_errors) {
    IStorage *g = (IStorage *)(ULONG_PTR)1;
    HRW(IStorage_CreateStorage(stg, L"S", MODE_RW, 1, 0, &g), STG_E_INVALIDPARAMETER);
    HRW(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 1, &g), STG_E_INVALIDPARAMETER);
    HRW(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, NULL), STG_E_INVALIDPOINTER);
    HR(IStorage_CreateStorage(stg, NULL, MODE_RW, 0, 0, &g), STG_E_INVALIDNAME);
    CHECK(!ses_exists_storage(stg, L"S"));
}
DF_TEST(ses_el_createstorage_invalid_modes) {
    static const DWORD bad[] = { STGM_READWRITE, STGM_READ, STGM_SHARE_DENY_NONE | STGM_READWRITE, MODE_RW | STGM_DELETEONRELEASE };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        IStorage *g = NULL; HRESULT hr = IStorage_CreateStorage(stg, L"S", bad[i], 0, 0, &g);
        /* Windows: DELETEONRELEASE is STG_E_INVALIDFUNCTION, the other bad modes STG_E_INVALIDFLAG */
        HRESULT want = (bad[i] & STGM_DELETEONRELEASE) ? STG_E_INVALIDFUNCTION : STG_E_INVALIDFLAG;
        CHECK(hr == want);
        if (hr != want) t_note("CreateStorage mode 0x%lx -> 0x%08lx", bad[i], (unsigned long)hr);
        if (g) { IStorage_Release(g); IStorage_DestroyElement(stg, L"S"); }
    }
}
DF_TEST(ses_el_createstorage_replace_semantics) {
    IStorage *g = NULL, *g2 = NULL; IStream *s;
    HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    s = ses_mkstream(g, L"inner"); if (s) IStream_Release(s);
    IStorage_Release(g); g = NULL;
    HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g2), STG_E_FILEALREADYEXISTS);
    HR(IStorage_CreateStorage(stg, L"S", MODE_RW | STGM_CREATE, 0, 0, &g2), S_OK);
    if (g2) { CHECK(!ses_exists_stream(g2, L"inner")); IStorage_Release(g2); } /* replaced storage starts empty */
}
DF_TEST(ses_el_same_names_in_different_storages) {
    IStorage *a1 = NULL, *b1 = NULL;
    HR(IStorage_CreateStorage(stg, L"A", MODE_RW, 0, 0, &a1), S_OK);
    HR(IStorage_CreateStorage(stg, L"B", MODE_RW, 0, 0, &b1), S_OK);
    if (!a1 || !b1) return;
    HR(ses_put(a1, L"same", "AAA", 3), S_OK); HR(ses_put(b1, L"same", "BBBB", 4), S_OK);
    HR(ses_put(stg, L"same", "R", 1), S_OK);
    char b[8] = {0};
    EQ(ses_get(a1, L"same", b, 8), 3); EXPECT_EQ(memcmp(b, "AAA", 3), 0);
    EQ(ses_get(b1, L"same", b, 8), 4); EXPECT_EQ(memcmp(b, "BBBB", 4), 0);
    EQ(ses_get(stg, L"same", b, 8), 1);
    IStorage_Release(a1); IStorage_Release(b1);
}
DF_TEST(ses_el_nested_storage_depth_20) {
    IStorage *cur[21]; WCHAR nm[8]; int i, bad = 0;
    cur[0] = stg; IStorage_AddRef(stg);
    for (i = 1; i <= 20; i++) { wsprintfW(nm, L"d%d", i); cur[i] = NULL; if (IStorage_CreateStorage(cur[i - 1], nm, MODE_RW, 0, 0, &cur[i]) != S_OK) { bad++; break; } }
    EQ(bad, 0);
    if (!bad) { HR(ses_put(cur[20], L"leaf", "deep", 4), S_OK); }
    for (i = 20; i >= 0; i--) if (cur[i]) IStorage_Release(cur[i]);
    /* reopen down the path (parents stay open until the leaf has been read) */
    IStorage *path[21]; memset(path, 0, sizeof path);
    path[0] = stg; IStorage_AddRef(stg);
    for (i = 1; i <= 20 && !bad; i++) {
        wsprintfW(nm, L"d%d", i);
        if (IStorage_OpenStorage(path[i - 1], nm, NULL, MODE_RW, NULL, 0, &path[i]) != S_OK) bad++;
    }
    EQ(bad, 0);
    if (!bad) { char b[8] = {0}; EQ(ses_get(path[20], L"leaf", b, 8), 4); EXPECT_EQ(memcmp(b, "deep", 4), 0); }
    for (i = 20; i >= 0; i--) if (path[i]) IStorage_Release(path[i]);
}
DF_TEST(ses_el_many_elements_600) {
    int bad = 0, i; WCHAR nm[16];
    for (i = 0; i < 600; i++) { wsprintfW(nm, L"stm%04d", i); char d[4] = { (char)i, (char)(i >> 8), 'x', 0 }; if (ses_put(stg, nm, d, 3) != S_OK) bad++; }
    EQ(bad, 0);
    for (i = 0; i < 600; i += 7) { wsprintfW(nm, L"stm%04d", i); char d[8]; if (ses_get(stg, nm, d, 8) != 3 || d[0] != (char)i || d[1] != (char)(i >> 8)) bad++; }
    EQ(bad, 0);
    for (i = 0; i < 600; i += 2) { wsprintfW(nm, L"stm%04d", i); if (IStorage_DestroyElement(stg, nm) != S_OK) bad++; }
    EQ(bad, 0);
    for (i = 0; i < 600; i++) { wsprintfW(nm, L"stm%04d", i); if (ses_exists_stream(stg, nm) != (i & 1)) bad++; }
    EQ(bad, 0);
}
/* ---- OpenStream / OpenStorage ---- */
DF_TEST(ses_el_openstream_not_found) {
    IStream *s = (IStream *)(ULONG_PTR)1;
    HR(IStorage_OpenStream(stg, L"nothere", NULL, MODE_RD, 0, &s), STG_E_FILENOTFOUND);
    CHECKW(s == NULL);
    HR(IStorage_OpenStream(stg, L"nothere", NULL, MODE_RW, 0, &s), STG_E_FILENOTFOUND);
}
DF_TEST(ses_el_openstorage_not_found) {
    IStorage *s = (IStorage *)(ULONG_PTR)1;
    HR(IStorage_OpenStorage(stg, L"nothere", NULL, MODE_RW, NULL, 0, &s), STG_E_FILENOTFOUND);
    CHECKW(s == NULL);
}
DF_TEST(ses_el_open_wrong_element_type) {
    IStream *x = ses_mkstream(stg, L"strm"); if (x) IStream_Release(x);
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"stor", MODE_RW, 0, 0, &g), S_OK); if (g) IStorage_Release(g);
    IStorage *os = NULL; IStream *s2 = NULL;
    /* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
    HR(IStorage_OpenStorage(stg, L"strm", NULL, MODE_RW, NULL, 0, &os), STG_E_FILENOTFOUND);
    HRW(IStorage_OpenStream(stg, L"stor", NULL, MODE_RW, 0, &s2), STG_E_FILENOTFOUND);
}
DF_TEST(ses_el_openstream_arg_errors) {
    IStream *x = ses_mkstream(stg, L"s"); if (x) IStream_Release(x);
    IStream *s = NULL;
    HRW(IStorage_OpenStream(stg, L"s", (void *)(ULONG_PTR)1, MODE_RW, 0, &s), STG_E_INVALIDPARAMETER);
    HRW(IStorage_OpenStream(stg, L"s", NULL, MODE_RW, 1, &s), STG_E_INVALIDPARAMETER);
    HRW(IStorage_OpenStream(stg, L"s", NULL, MODE_RW, 0, NULL), STG_E_INVALIDPOINTER);
    HRW(IStorage_OpenStream(stg, NULL, NULL, MODE_RW, 0, &s), STG_E_INVALIDNAME);
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_openstream_invalid_modes) {
    IStream *x = ses_mkstream(stg, L"s"); if (x) IStream_Release(x);
    IStream *s = NULL;
    HR(IStorage_OpenStream(stg, L"s", NULL, STGM_SHARE_DENY_NONE | STGM_READ, 0, &s), STG_E_INVALIDFLAG);
    HR(IStorage_OpenStream(stg, L"s", NULL, STGM_DELETEONRELEASE | MODE_RW, 0, &s), STG_E_INVALIDFUNCTION);
    HR(IStorage_OpenStream(stg, L"s", NULL, STGM_TRANSACTED | MODE_RW, 0, &s), STG_E_INVALIDFUNCTION);
    HRW(IStorage_OpenStream(stg, L"s", NULL, STGM_READWRITE, 0, &s), STG_E_INVALIDFLAG);
    HRW(IStorage_OpenStream(stg, L"s", NULL, STGM_READ, 0, &s), STG_E_INVALIDFLAG);
    HR(IStorage_OpenStream(stg, L"s", NULL, STGM_SHARE_DENY_WRITE | STGM_READ, 0, &s), STG_E_INVALIDFUNCTION); /* Windows: not INVALIDFLAG */
    CHECK(s == NULL);
}
DF_TEST(ses_el_openstream_valid_modes) {
    IStream *x = ses_mkstream(stg, L"s"); if (x) { ses_write_all(x, "abc", 3); IStream_Release(x); }
    static const DWORD good[] = { STGM_READ | STGM_SHARE_EXCLUSIVE, STGM_WRITE | STGM_SHARE_EXCLUSIVE, STGM_READWRITE | STGM_SHARE_EXCLUSIVE };
    for (unsigned i = 0; i < 3; i++) {
        IStream *s = NULL; HRESULT hr = IStorage_OpenStream(stg, L"s", NULL, good[i], 0, &s);
        CHECKF(hr == S_OK, "mode 0x%lx hr=0x%08lx", good[i], (unsigned long)hr);
        if (s) IStream_Release(s);
    }
}
DF_TEST(ses_el_open_stream_twice_denied) {
    IStream *x = ses_mkstream(stg, L"s"); if (!x) return;
    IStream *y = (IStream *)(ULONG_PTR)1;
    HRW(IStorage_OpenStream(stg, L"s", NULL, MODE_RW, 0, &y), STG_E_ACCESSDENIED);
    IStream_Release(x);
    y = NULL;
    HR(IStorage_OpenStream(stg, L"s", NULL, MODE_RW, 0, &y), S_OK); if (y) IStream_Release(y);
}
DF_TEST(ses_el_open_storage_twice_denied) {
    IStorage *x = NULL, *y = (IStorage *)(ULONG_PTR)1;
    HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &x), S_OK); if (!x) return;
    HRW(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &y), STG_E_ACCESSDENIED);
    IStorage_Release(x); y = NULL;
    HR(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &y), S_OK); if (y) IStorage_Release(y);
}
DF_TEST(ses_el_openstorage_invalid_modes) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (g) IStorage_Release(g);
    IStorage *o = NULL;
    OBSERVE_HR(IStorage_OpenStorage(stg, L"S", NULL, STGM_SHARE_DENY_NONE | STGM_READWRITE, NULL, 0, &o)); if (o) { IStorage_Release(o); o = NULL; }
    OBSERVE_HR(IStorage_OpenStorage(stg, L"S", NULL, STGM_READWRITE, NULL, 0, &o)); if (o) { IStorage_Release(o); o = NULL; }
    HRW(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW | STGM_DELETEONRELEASE, NULL, 0, &o), STG_E_INVALIDFUNCTION);
    CHECK(o == NULL);
}
DF_TEST(ses_el_openstorage_arg_errors) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (g) IStorage_Release(g);
    IStorage *o = NULL;
    HRW(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 1, &o), STG_E_INVALIDPARAMETER);
    HRW(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, NULL), STG_E_INVALIDPOINTER);
    HRW(IStorage_OpenStorage(stg, NULL, NULL, MODE_RW, NULL, 0, &o), STG_E_INVALIDNAME);
}
DF_TEST(ses_el_openstorage_content_persists) {
    IStorage *g = NULL, *o = NULL; unsigned char w[6000], r[6000];
    ses_fill(w, sizeof w, 51);
    HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    HR(ses_put(g, L"inner", w, sizeof w), S_OK);
    IStorage_Release(g);
    HR(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &o), S_OK); if (!o) return;
    EQ(ses_get(o, L"inner", r, sizeof r), 6000); CHECK(!memcmp(r, w, sizeof w));
    IStorage_Release(o);
}
/* ---- DestroyElement ---- */
DF_TEST(ses_el_destroy_stream_and_storage) {
    IStream *x = ses_mkstream(stg, L"s"); if (x) IStream_Release(x);
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"g", MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    HR(IStorage_DestroyElement(stg, L"s"), S_OK);
    HR(IStorage_DestroyElement(stg, L"g"), S_OK);
    CHECK(!ses_exists_stream(stg, L"s")); CHECK(!ses_exists_storage(stg, L"g"));
    WCHAR l[2][40]; EQ(ses_list(a, stg, l, NULL, NULL, 2), 0);
}
DF_TEST(ses_el_destroy_not_found) {
    HR(IStorage_DestroyElement(stg, L"missing"), STG_E_FILENOTFOUND);
    IStream *x = ses_mkstream(stg, L"s"); if (x) IStream_Release(x);
    HR(IStorage_DestroyElement(stg, L"s"), S_OK);
    HR(IStorage_DestroyElement(stg, L"s"), STG_E_FILENOTFOUND); /* second destroy */
}
DF_TEST(ses_el_destroy_null_name) {
    HRW(IStorage_DestroyElement(stg, NULL), STG_E_INVALIDNAME);
}
DF_TEST(ses_el_destroy_long_name_observed) {
    WCHAR n[128]; fill_name(n, 32, L'q');
    OBSERVE_HR(IStorage_DestroyElement(stg, n));
}
DF_TEST(ses_el_destroy_recursive_tree) {
    IStorage *g = NULL, *h = NULL;
    HR(IStorage_CreateStorage(stg, L"top", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    HR(IStorage_CreateStorage(g, L"mid", MODE_RW, 0, 0, &h), S_OK); if (!h) return;
    ses_put(h, L"leaf", "x", 1); ses_put(g, L"s2", "y", 1);
    IStorage_Release(h); IStorage_Release(g);
    HR(IStorage_DestroyElement(stg, L"top"), S_OK);
    CHECK(!ses_exists_storage(stg, L"top"));
    /* name can be reused and the new storage is empty */
    HR(IStorage_CreateStorage(stg, L"top", MODE_RW, 0, 0, &g), S_OK);
    if (g) { WCHAR l[2][40]; EQ(ses_list(a, g, l, NULL, NULL, 2), 0); IStorage_Release(g); }
}
DF_TEST(ses_el_destroy_then_recreate_stream) {
    ses_put(stg, L"s", "first", 5);
    HR(IStorage_DestroyElement(stg, L"s"), S_OK);
    HR(ses_put(stg, L"s", "2", 1), S_OK);
    char b[8] = {0}; EQ(ses_get(stg, L"s", b, 8), 1);
}
DF_TEST(ses_el_destroy_open_stream_observed) {
    IStream *x = ses_mkstream(stg, L"s"); if (!x) return;
    HRESULT hr = IStorage_DestroyElement(stg, L"s");
    t_note("DestroyElement on an open stream -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) { HRESULT w = IStream_Write(x, "a", 1, NULL); t_note("Write on destroyed stream -> 0x%08lx", (unsigned long)w); }
    IStream_Release(x);
}
DF_TEST(ses_el_destroy_open_storage_observed) {
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g); if (!g) return;
    HRESULT hr = IStorage_DestroyElement(stg, L"S");
    t_note("DestroyElement on an open substorage -> 0x%08lx", (unsigned long)hr);
    IStorage_Release(g);
}
DF_TEST(ses_el_destroy_storage_with_open_child_observed) {
    IStorage *g = NULL, *h = NULL;
    IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g); if (!g) return;
    IStorage_CreateStorage(g, L"T", MODE_RW, 0, 0, &h); if (!h) return;
    HRESULT hr = IStorage_DestroyElement(g, L"T");
    t_note("DestroyElement of child while opened -> 0x%08lx", (unsigned long)hr);
    IStorage_Release(h); IStorage_Release(g);
}
/* ---- RenameElement ---- */
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_rename_stream_basic) {
    HR(ses_put(stg, L"CONTENTS", "payload", 7), S_OK);
    HR(IStorage_RenameElement(stg, L"CONTENTS", L"ENTS"), S_OK);
    IStream *s = NULL;
    HR(IStorage_OpenStream(stg, L"CONTENTS", NULL, MODE_RW, 0, &s), STG_E_FILENOTFOUND);
    char b[16] = {0}; EQ(ses_get(stg, L"ENTS", b, 16), 7); EXPECT_EQ(memcmp(b, "payload", 7), 0);
}
DF_TEST(ses_el_rename_storage_keeps_children) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"PERMSTG", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    ses_put(g, L"c", "kid", 3); IStorage_Release(g);
    HR(IStorage_RenameElement(stg, L"PERMSTG", L"STG"), S_OK);
    IStorage *o = NULL;
    HR(IStorage_OpenStorage(stg, L"PERMSTG", NULL, MODE_RW, NULL, 0, &o), STG_E_FILENOTFOUND);
    HR(IStorage_OpenStorage(stg, L"STG", NULL, MODE_RW, NULL, 0, &o), S_OK);
    if (o) { char b[8] = {0}; EQ(ses_get(o, L"c", b, 8), 3); IStorage_Release(o); }
}
/* adapted from Wine dlls/ole32/tests/storage32.c (LGPL-2.1+) */
DF_TEST(ses_el_rename_name_too_long) {
    ses_put(stg, L"a", "1", 1);
    HRESULT hl = IStorage_RenameElement(stg, L"a", L"abcdef_abcdef_abcdef_abcdef_abcdef_abcdef_abcdef_abcdef");
    if (t_is_wine() && hl == S_OK) { t_skip("wine accepts over-long rename targets"); return; }
    HR(hl, STG_E_INVALIDNAME);
    WCHAR n[128]; fill_name(n, 32, L'z');
    HR(IStorage_RenameElement(stg, L"a", n), STG_E_INVALIDNAME);
    fill_name(n, 31, L'z');
    HR(IStorage_RenameElement(stg, L"a", n), S_OK);
    CHECK(ses_exists_stream(stg, n)); CHECK(!ses_exists_stream(stg, L"a"));
}
DF_TEST(ses_el_rename_target_exists) {
    ses_put(stg, L"a", "1", 1); ses_put(stg, L"b", "22", 2);
    /* Windows reports a name collision as STG_E_ACCESSDENIED, not STG_E_FILEALREADYEXISTS */
    HR(IStorage_RenameElement(stg, L"a", L"b"), STG_E_ACCESSDENIED);
    HR(IStorage_RenameElement(stg, L"a", L"B"), STG_E_ACCESSDENIED); /* case-insensitive collision */
    char r[4]; EQ(ses_get(stg, L"a", r, 4), 1); EQ(ses_get(stg, L"b", r, 4), 2);
    IStorage *g = NULL; IStorage_CreateStorage(stg, L"g", MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    HR(IStorage_RenameElement(stg, L"a", L"g"), STG_E_ACCESSDENIED); /* stream onto storage name */
    HR(IStorage_RenameElement(stg, L"g", L"a"), STG_E_ACCESSDENIED);
}
DF_TEST(ses_el_rename_source_missing) {
    HR(IStorage_RenameElement(stg, L"nothing", L"other"), STG_E_FILENOTFOUND);
}
DF_TEST(ses_el_rename_null_names) {
    ses_put(stg, L"a", "1", 1);
    if (t_is_wine()) { t_skip("wine faults on NULL names"); return; }
    HRW(IStorage_RenameElement(stg, NULL, L"x"), STG_E_INVALIDNAME);
    HRW(IStorage_RenameElement(stg, L"a", NULL), STG_E_INVALIDNAME);
    CHECK(ses_exists_stream(stg, L"a"));
}
DF_TEST(ses_el_rename_to_same_and_case_only_observed) {
    ses_put(stg, L"abc", "1", 1);
    OBSERVE_HR(IStorage_RenameElement(stg, L"abc", L"abc"));
    OBSERVE_HR(IStorage_RenameElement(stg, L"abc", L"ABC"));
}
DF_TEST(ses_el_rename_open_element_denied) {
    IStream *x = ses_mkstream(stg, L"a"); if (!x) return;
    HRW(IStorage_RenameElement(stg, L"a", L"b"), STG_E_ACCESSDENIED); /* docs: cannot rename while open */
    IStream_Release(x);
    HR(IStorage_RenameElement(stg, L"a", L"b"), S_OK);
}
DF_TEST(ses_el_rename_reserved_leading_char) {
    ses_put(stg, L"a", "1", 1);
    HR(IStorage_RenameElement(stg, L"a", L"\x05Summary"), S_OK);
    CHECK(ses_exists_stream(stg, L"\x05Summary"));
}
DF_TEST(ses_el_rename_chain) {
    ses_put(stg, L"n0", "chain", 5);
    int bad = 0; WCHAR a1[8], b1[8];
    for (int i = 0; i < 50; i++) { wsprintfW(a1, L"n%d", i); wsprintfW(b1, L"n%d", i + 1); if (IStorage_RenameElement(stg, a1, b1) != S_OK) bad++; }
    EQ(bad, 0);
    char b[8] = {0}; EQ(ses_get(stg, L"n50", b, 8), 5);
}
/* ---- IStorage interface identity ---- */
DF_TEST(ses_el_storage_qi_supported) {
    void *p = NULL;
    HR(IStorage_QueryInterface(stg, &IID_IStorage, &p), S_OK); CHECK(p == (void *)stg); if (p) IStorage_Release((IStorage *)p);
    p = NULL; HR(IStorage_QueryInterface(stg, &IID_IUnknown, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; HR(IStorage_QueryInterface(stg, &IID_IPropertySetStorage, &p), S_OK); CHECK(p != NULL); if (p) IUnknown_Release((IUnknown *)p);
}
DF_TEST(ses_el_storage_qi_unsupported) {
    static const IID *iids[] = { &IID_IStream, &IID_ISequentialStream, &IID_IPersist, &IID_IPersistStream, &IID_ILockBytes, &IID_IDispatch };
    for (unsigned i = 0; i < sizeof iids / sizeof *iids; i++) {
        void *p = (void *)(ULONG_PTR)1; HRESULT hr = IStorage_QueryInterface(stg, iids[i], &p);
        CHECKF(hr == E_NOINTERFACE, "iid #%u hr=0x%08lx", i, (unsigned long)hr);
        CHECKF(p == NULL, "iid #%u out ptr", i);
    }
}
DF_TEST(ses_el_substorage_qi) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    void *p = NULL;
    HR(IStorage_QueryInterface(g, &IID_IStorage, &p), S_OK); if (p) IStorage_Release((IStorage *)p);
    p = NULL; HR(IStorage_QueryInterface(g, &IID_IPropertySetStorage, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; HR(IStorage_QueryInterface(g, &IID_IStream, &p), E_NOINTERFACE);
    IStorage_Release(g);
}
DF_TEST(ses_el_storage_refcounts) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    EQ(IStorage_AddRef(g), 2); EQ(IStorage_AddRef(g), 3); EQ(IStorage_Release(g), 2); EQ(IStorage_Release(g), 1); EQ(IStorage_Release(g), 0);
}
DF_TEST(ses_el_substorage_outlives_parent_release) {
    IStorage *g = NULL, *p = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    IStorage_AddRef(stg); /* keep root alive for the harness */
    HR(ses_put(g, L"x", "ok", 2), S_OK);
    IStorage_Release(stg);
    HR(ses_put(g, L"y", "ok", 2), S_OK);
    IStorage_Release(g);
}
/* ---- SetClass / SetStateBits / Stat of storages ---- */
DF_TEST(ses_el_setclass_roundtrip_substorage) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    STATSTG st; HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &GUID_NULL));
    HR(IStorage_SetClass(g, &SES_CLSID_A), S_OK);
    HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A));
    HR(IStorage_SetClass(g, &GUID_NULL), S_OK);
    HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &GUID_NULL));
    IStorage_Release(g);
}
DF_TEST(ses_el_setclass_persists_after_reopen) {
    IStorage *g = NULL, *o = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    HR(IStorage_SetClass(g, &SES_CLSID_B), S_OK); IStorage_Release(g);
    HR(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &o), S_OK); if (!o) return;
    STATSTG st; HR(IStorage_Stat(o, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_B));
    IStorage_Release(o);
}
DF_TEST(ses_el_setclass_root) {
    STATSTG st;
    HR(IStorage_SetClass(stg, &SES_CLSID_A), S_OK);
    HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK); CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A));
}
DF_TEST(ses_el_setclass_null_pointer) {
    if (t_is_wine()) { t_skip("wine faults on NULL clsid"); return; }
    HRW(IStorage_SetClass(stg, NULL), STG_E_INVALIDPOINTER);
}
DF_TEST(ses_el_statebits_mask) {
    STATSTG st;
    HR(IStorage_SetStateBits(stg, 0xffffffff, 0xffffffff), S_OK);
    HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0xffffffff);
    HR(IStorage_SetStateBits(stg, 0, 0x0000ff00), S_OK);
    HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0xffff00ff);
    HR(IStorage_SetStateBits(stg, 0x12, 0x0000000f), S_OK);
    HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK); EQ(st.grfStateBits, 0xffff00f2);
}
DF_TEST(ses_el_statebits_substorage) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    STATSTG st;
    HR(IStorage_SetStateBits(g, 0x5a5a, 0xffff), S_OK);
    HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK); CHECKW(st.grfStateBits == 0x5a5a);
    IStorage_Release(g);
    IStorage *o = NULL; HR(IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &o), S_OK);
    if (o) { HR(IStorage_Stat(o, &st, STATFLAG_NONAME), S_OK); CHECKW(st.grfStateBits == 0x5a5a); IStorage_Release(o); }
}
DF_TEST(ses_el_stat_substorage_fields) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"SubName", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    STATSTG st; memset(&st, 0xcc, sizeof st);
    HR(IStorage_Stat(g, &st, STATFLAG_DEFAULT), S_OK);
    EQ(st.type, STGTY_STORAGE);
    CHECK(st.pwcsName != NULL);
    if (st.pwcsName) { EXPECT_WSTR(st.pwcsName, L"SubName"); a->taskfree(st.pwcsName); }
    t_note("substorage Stat grfMode=0x%lx cbSize=%llu ctime.hi=0x%lx", st.grfMode, (unsigned long long)st.cbSize.QuadPart, st.ctime.dwHighDateTime);
    HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK); CHECK(st.pwcsName == NULL);
    IStorage_Release(g);
}
DF_TEST(ses_el_stat_root_type) {
    STATSTG st; memset(&st, 0xcc, sizeof st);
    HR(IStorage_Stat(stg, &st, STATFLAG_NONAME), S_OK);
    EQ(st.type, STGTY_STORAGE); CHECK(st.pwcsName == NULL);
    t_note("root Stat grfMode=0x%lx", st.grfMode);
}
DF_TEST(ses_el_stat_null_out) {
    HRW(IStorage_Stat(stg, NULL, STATFLAG_NONAME), STG_E_INVALIDPOINTER);
}
DF_TEST(ses_el_setelementtimes_substorage) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    IStorage_Release(g);
    FILETIME ft; ft.dwHighDateTime = 0x01d00000; ft.dwLowDateTime = 0x12345678;
    HRESULT hr = IStorage_SetElementTimes(stg, L"S", &ft, &ft, &ft);
    t_note("SetElementTimes(substorage) -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) {
        IStorage *o = NULL; STATSTG st;
        if (IStorage_OpenStorage(stg, L"S", NULL, MODE_RW, NULL, 0, &o) == S_OK) {
            IStorage_Stat(o, &st, STATFLAG_NONAME);
            t_note("after SetElementTimes: ctime=%08lx%08lx mtime=%08lx%08lx", st.ctime.dwHighDateTime, st.ctime.dwLowDateTime, st.mtime.dwHighDateTime, st.mtime.dwLowDateTime);
            IStorage_Release(o);
        }
    }
}
DF_TEST(ses_el_setelementtimes_errors) {
    FILETIME ft = {1, 1};
    HRW(IStorage_SetElementTimes(stg, L"nothere", &ft, &ft, &ft), STG_E_FILENOTFOUND);
    /* a NULL name addresses the storage itself (documented), so Windows succeeds */
    HRW(IStorage_SetElementTimes(stg, NULL, &ft, &ft, &ft), S_OK);
}
DF_TEST(ses_el_setelementtimes_stream_observed) {
    ses_put(stg, L"s", "x", 1);
    FILETIME ft = {1, 1};
    OBSERVE_HR(IStorage_SetElementTimes(stg, L"s", &ft, &ft, &ft)); /* streams keep no timestamps */
}
DF_TEST(ses_el_substorage_times_after_create_observed) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    STATSTG st; HR(IStorage_Stat(g, &st, STATFLAG_NONAME), S_OK);
    t_note("fresh substorage ctime=%08lx%08lx mtime=%08lx%08lx", st.ctime.dwHighDateTime, st.ctime.dwLowDateTime, st.mtime.dwHighDateTime, st.mtime.dwLowDateTime);
    IStorage_Release(g);
}
