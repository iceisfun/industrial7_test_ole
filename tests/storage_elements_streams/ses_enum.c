/* IEnumSTATSTG from IStorage::EnumElements: ordering, Next/Skip/Reset/Clone, STATSTG contents. */
#include "ses.h"

static IEnumSTATSTG *en(IStorage *stg) { IEnumSTATSTG *e = NULL; if (IStorage_EnumElements(stg, 0, NULL, 0, &e) != S_OK) return NULL; return e; }
static void mk_set(IStorage *stg, const WCHAR *const *names, int n, int storages_from) {
    for (int i = 0; i < n; i++) {
        if (i >= storages_from) { IStorage *g = NULL; if (IStorage_CreateStorage(stg, names[i], MODE_RW, 0, 0, &g) == S_OK) IStorage_Release(g); }
        else ses_put(stg, names[i], "x", 1);
    }
}

DF_TEST(ses_en_empty_storage) {
    IEnumSTATSTG *e = en(stg); CHECK(e != NULL); if (!e) return;
    STATSTG st; ULONG f = 0xf00;
    memset(&st, 0xad, sizeof st);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_FALSE); EQ(f, 0);
    HR(IEnumSTATSTG_Reset(e), S_OK);
    HR(IEnumSTATSTG_Skip(e, 1), S_FALSE);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_arg_validation) {
    IEnumSTATSTG *e = (IEnumSTATSTG *)(ULONG_PTR)1;
    HRW(IStorage_EnumElements(stg, 1, NULL, 0, &e), STG_E_INVALIDPARAMETER);
    HRW(IStorage_EnumElements(stg, 0, (void *)(ULONG_PTR)1, 0, &e), STG_E_INVALIDPARAMETER);
    HRW(IStorage_EnumElements(stg, 0, NULL, 1, &e), STG_E_INVALIDPARAMETER);
    HRW(IStorage_EnumElements(stg, 0, NULL, 0, NULL), STG_E_INVALIDPOINTER);
}
DF_TEST(ses_en_single_stream_stat) {
    unsigned char w[5000]; memset(w, 3, sizeof w);
    ses_put(stg, L"Only", w, sizeof w);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f = 0xf00; memset(&st, 0xad, sizeof st);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EQ(f, 1);
    EQ(st.type, STGTY_STREAM); EQ(st.cbSize.QuadPart, 5000);
    CHECK(st.pwcsName != NULL);
    if (st.pwcsName) { EXPECT_WSTR(st.pwcsName, L"Only"); a->taskfree(st.pwcsName); }
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_FALSE); EQ(f, 0);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_storage_entry_stat) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"Sub", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    IStorage_SetClass(g, &SES_CLSID_A); ses_put(g, L"kid", "12345", 5); IStorage_Release(g);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f = 0; memset(&st, 0xad, sizeof st);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EQ(f, 1);
    EQ(st.type, STGTY_STORAGE);
    CHECK(IsEqualGUID(&st.clsid, &SES_CLSID_A));
    if (st.pwcsName) { EXPECT_WSTR(st.pwcsName, L"Sub"); a->taskfree(st.pwcsName); } else CHECK(st.pwcsName != NULL);
    t_note("enum storage entry cbSize=%llu grfMode=0x%lx", (unsigned long long)st.cbSize.QuadPart, st.grfMode);
    IEnumSTATSTG_Release(e);
}
/* directory order: shorter names first, then case-insensitive alphabetical */
DF_TEST(ses_en_order_length_then_name) {
    static const WCHAR *names[] = { L"b", L"a", L"C", L"ab", L"AA", L"abc", L"B2", L"zzzz", L"Abcd", L"x" };
    mk_set(stg, names, 10, 99);
    WCHAR l[16][40]; DWORD ty[16]; int n = ses_list(a, stg, l, ty, NULL, 16);
    EQ(n, 10);
    static const WCHAR *want[] = { L"a", L"b", L"C", L"x", L"AA", L"ab", L"B2", L"abc", L"Abcd", L"zzzz" };
    int bad = 0; for (int i = 0; i < n && i < 10; i++) if (lstrcmpW(l[i], want[i])) { bad++; t_note("pos %d got %ls want %ls", i, l[i], want[i]); }
    CHECKW(bad == 0);
}
DF_TEST(ses_en_order_independent_of_creation_order) {
    static const WCHAR *fwd[] = { L"aa", L"bb", L"cc", L"d", L"eee" };
    static const WCHAR *rev[] = { L"eee", L"d", L"cc", L"bb", L"aa" };
    mk_set(stg, rev, 5, 99);
    WCHAR l1[8][40]; int n1 = ses_list(a, stg, l1, NULL, NULL, 8);
    EQ(n1, 5);
    static const WCHAR *want[] = { L"d", L"aa", L"bb", L"cc", L"eee" };
    int bad = 0; for (int i = 0; i < n1 && i < 5; i++) if (lstrcmpW(l1[i], want[i])) bad++;
    CHECKW(bad == 0);
    (void)fwd;
}
DF_TEST(ses_en_order_case_insensitive) {
    static const WCHAR *names[] = { L"Bravo", L"alpha", L"CHARLIE", L"delta", L"ECHO5" };
    mk_set(stg, names, 5, 99);
    WCHAR l[8][40]; int n = ses_list(a, stg, l, NULL, NULL, 8);
    EQ(n, 5);
    /* all length 5 except Bravo/alpha/delta (5), CHARLIE (7), ECHO5 (5): 5-letter ones sorted case-insensitively */
    static const WCHAR *want[] = { L"alpha", L"Bravo", L"delta", L"ECHO5", L"CHARLIE" };
    int bad = 0; for (int i = 0; i < n && i < 5; i++) if (lstrcmpW(l[i], want[i])) bad++;
    CHECKW(bad == 0);
}
DF_TEST(ses_en_streams_and_storages_interleaved) {
    static const WCHAR *names[] = { L"s1", L"g1", L"s22", L"g22", L"s3" };
    ses_put(stg, names[0], "a", 1);
    IStorage *g = NULL; IStorage_CreateStorage(stg, names[1], MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    ses_put(stg, names[2], "bb", 2);
    IStorage_CreateStorage(stg, names[3], MODE_RW, 0, 0, &g); if (g) IStorage_Release(g);
    ses_put(stg, names[4], "ccc", 3);
    WCHAR l[8][40]; DWORD ty[8]; ULONGLONG sz[8]; int n = ses_list(a, stg, l, ty, sz, 8);
    EQ(n, 5);
    int streams = 0, stors = 0;
    for (int i = 0; i < n; i++) { if (ty[i] == STGTY_STREAM) streams++; else if (ty[i] == STGTY_STORAGE) stors++; }
    EQ(streams, 3); EQ(stors, 2);
    for (int i = 0; i < n; i++) if (!lstrcmpW(l[i], L"s22")) { EQ(ty[i], STGTY_STREAM); EQ(sz[i], 2); }
}
DF_TEST(ses_en_next_batch) {
    static const WCHAR *names[] = { L"a", L"b", L"c", L"d", L"e" };
    mk_set(stg, names, 5, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st[8]; ULONG f = 0xf00;
    HR(IEnumSTATSTG_Next(e, 2, st, &f), S_OK); EQ(f, 2);
    for (ULONG i = 0; i < f; i++) a->taskfree(st[i].pwcsName);
    HR(IEnumSTATSTG_Next(e, 8, st, &f), S_FALSE); EQ(f, 3); /* fewer than requested: S_FALSE with the partial count */
    for (ULONG i = 0; i < f; i++) { CHECK(st[i].pwcsName != NULL); a->taskfree(st[i].pwcsName); }
    HR(IEnumSTATSTG_Next(e, 1, st, &f), S_FALSE); EQ(f, 0);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_next_exact_remaining_is_ok) {
    static const WCHAR *names[] = { L"a", L"b", L"c" };
    mk_set(stg, names, 3, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st[4]; ULONG f = 0;
    HR(IEnumSTATSTG_Next(e, 3, st, &f), S_OK); EQ(f, 3);
    for (ULONG i = 0; i < f; i++) a->taskfree(st[i].pwcsName);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_next_null_fetched) {
    static const WCHAR *names[] = { L"a", L"b" };
    mk_set(stg, names, 2, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st[2];
    HR(IEnumSTATSTG_Next(e, 1, st, NULL), S_OK); a->taskfree(st[0].pwcsName);
    OBSERVE_HR(IEnumSTATSTG_Next(e, 2, st, NULL)); /* docs: NULL count only valid for celt == 1 */
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_next_zero_celt_observed) {
    static const WCHAR *names[] = { L"a" };
    mk_set(stg, names, 1, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f = 0xf00;
    OBSERVE_HR(IEnumSTATSTG_Next(e, 0, &st, &f));
    t_note("celt=0 fetched=%lu", f);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_skip_semantics) {
    static const WCHAR *names[] = { L"a", L"b", L"c", L"d", L"e" };
    mk_set(stg, names, 5, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f;
    HR(IEnumSTATSTG_Skip(e, 2), S_OK);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"c"); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Skip(e, 0), S_OK);
    HR(IEnumSTATSTG_Skip(e, 1), S_OK);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"e"); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Skip(e, 1), S_FALSE);
    HR(IEnumSTATSTG_Reset(e), S_OK);
    HR(IEnumSTATSTG_Skip(e, 6), S_FALSE); /* more than available */
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_FALSE); EQ(f, 0);
    HR(IEnumSTATSTG_Reset(e), S_OK);
    HR(IEnumSTATSTG_Skip(e, 5), S_OK);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_FALSE);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_reset_restarts) {
    static const WCHAR *names[] = { L"a", L"b", L"c" };
    mk_set(stg, names, 3, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f;
    for (int round = 0; round < 3; round++) {
        for (int i = 0; i < 3; i++) { HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); if (f == 1) { WCHAR w[2] = { (WCHAR)('a' + i), 0 }; EXPECT_WSTR(st.pwcsName, w); a->taskfree(st.pwcsName); } }
        HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_FALSE);
        HR(IEnumSTATSTG_Reset(e), S_OK);
    }
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_clone_position_and_independence) {
    static const WCHAR *names[] = { L"a", L"b", L"c", L"d" };
    mk_set(stg, names, 4, 99);
    IEnumSTATSTG *e = en(stg), *c = NULL; if (!e) return;
    STATSTG st; ULONG f;
    HR(IEnumSTATSTG_Skip(e, 1), S_OK);
    HR(IEnumSTATSTG_Clone(e, &c), S_OK); CHECK(c != NULL && c != e); if (!c) { IEnumSTATSTG_Release(e); return; }
    HR(IEnumSTATSTG_Next(c, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"b"); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Next(c, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"c"); a->taskfree(st.pwcsName);
    /* original not affected */
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"b"); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Reset(c), S_OK);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"c"); a->taskfree(st.pwcsName);
    IEnumSTATSTG_Release(c); IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_clone_null) {
    IEnumSTATSTG *e = en(stg); if (!e) return;
    HRW(IEnumSTATSTG_Clone(e, NULL), STG_E_INVALIDPOINTER);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_refcount_and_qi) {
    IEnumSTATSTG *e = en(stg); if (!e) return;
    EQ(IEnumSTATSTG_AddRef(e), 2); EQ(IEnumSTATSTG_Release(e), 1);
    void *p = NULL;
    HR(IEnumSTATSTG_QueryInterface(e, &IID_IEnumSTATSTG, &p), S_OK); if (p) IEnumSTATSTG_Release((IEnumSTATSTG *)p);
    p = NULL; HR(IEnumSTATSTG_QueryInterface(e, &IID_IUnknown, &p), S_OK); if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; HR(IEnumSTATSTG_QueryInterface(e, &IID_IStream, &p), E_NOINTERFACE); CHECK(p == NULL);
    EQ(IEnumSTATSTG_Release(e), 0);
}
DF_TEST(ses_en_enumerator_outlives_storage_ref) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    ses_put(g, L"one", "1", 1); ses_put(g, L"two", "22", 2);
    IEnumSTATSTG *e = en(g); IStorage_Release(g); if (!e) return;
    STATSTG st; ULONG f;
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"one"); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"two"); a->taskfree(st.pwcsName);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_two_enumerators_independent) {
    static const WCHAR *names[] = { L"a", L"b" };
    mk_set(stg, names, 2, 99);
    IEnumSTATSTG *e1 = en(stg), *e2 = en(stg); if (!e1 || !e2) return;
    STATSTG st; ULONG f;
    HR(IEnumSTATSTG_Next(e1, 1, &st, &f), S_OK); a->taskfree(st.pwcsName);
    HR(IEnumSTATSTG_Next(e2, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"a"); a->taskfree(st.pwcsName);
    IEnumSTATSTG_Release(e1); IEnumSTATSTG_Release(e2);
}
DF_TEST(ses_en_element_added_after_creation_observed) {
    static const WCHAR *names[] = { L"a", L"c" };
    mk_set(stg, names, 2, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    ses_put(stg, L"b", "x", 1); /* added behind/ahead of the cursor */
    int n = 0; STATSTG st; ULONG f;
    while (IEnumSTATSTG_Next(e, 1, &st, &f) == S_OK && f == 1) { n++; a->taskfree(st.pwcsName); if (n > 10) break; }
    t_note("enumerator created before CreateStream saw %d elements (snapshot=2, live=3)", n);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_element_removed_after_creation_observed) {
    static const WCHAR *names[] = { L"a", L"b", L"c" };
    mk_set(stg, names, 3, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    IStorage_DestroyElement(stg, L"b");
    int n = 0; STATSTG st; ULONG f;
    while (IEnumSTATSTG_Next(e, 1, &st, &f) == S_OK && f == 1) { n++; a->taskfree(st.pwcsName); if (n > 10) break; }
    t_note("enumerator created before DestroyElement saw %d elements (snapshot=3, live=2)", n);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_destroy_current_then_continue) {
    static const WCHAR *names[] = { L"a", L"b", L"c", L"d" };
    mk_set(stg, names, 4, 99);
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f;
    HR(IEnumSTATSTG_Next(e, 1, &st, &f), S_OK); EXPECT_WSTR(st.pwcsName, L"a"); a->taskfree(st.pwcsName);
    IStorage_DestroyElement(stg, L"a");
    HRESULT hr = IEnumSTATSTG_Next(e, 1, &st, &f);
    t_note("Next after destroying the just-returned element -> 0x%08lx", (unsigned long)hr);
    if (hr == S_OK) { t_note("next name %ls", st.pwcsName); a->taskfree(st.pwcsName); }
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_many_elements_sorted_300) {
    WCHAR nm[16]; int i, bad = 0;
    for (i = 0; i < 300; i++) { wsprintfW(nm, L"e%03d", (i * 7) % 300); ses_put(stg, nm, "x", 1); }
    IEnumSTATSTG *e = en(stg); if (!e) return;
    STATSTG st; ULONG f; int n = 0, prev = -1;
    while (IEnumSTATSTG_Next(e, 1, &st, &f) == S_OK && f == 1) {
        int v = _wtoi(st.pwcsName + 1); if (v != prev + 1) bad++; prev = v; n++; a->taskfree(st.pwcsName);
    }
    EQ(n, 300); EQ(bad, 0);
    IEnumSTATSTG_Release(e);
}
DF_TEST(ses_en_names_exact_roundtrip_unicode) {
    static const WCHAR *names[] = { L"été", L"中文", L"\x05Sum", L"with space" };
    mk_set(stg, names, 4, 99);
    WCHAR l[8][40]; int n = ses_list(a, stg, l, NULL, NULL, 8);
    EQ(n, 4);
    int found = 0;
    for (int i = 0; i < n; i++) for (int j = 0; j < 4; j++) if (!lstrcmpW(l[i], names[j])) found++;
    EQ(found, 4);
}
DF_TEST(ses_en_substorage_enum_isolated) {
    IStorage *g = NULL; HR(IStorage_CreateStorage(stg, L"S", MODE_RW, 0, 0, &g), S_OK); if (!g) return;
    ses_put(g, L"inner1", "1", 1); ses_put(g, L"inner2", "1", 1); ses_put(stg, L"outer", "o", 1);
    WCHAR l[8][40]; int n = ses_list(a, g, l, NULL, NULL, 8);
    EQ(n, 2);
    n = ses_list(a, stg, l, NULL, NULL, 8); EQ(n, 2); /* "S" + "outer" only: no recursion */
    IStorage_Release(g);
}
DF_TEST(ses_en_stream_sizes_in_enum) {
    static const unsigned sz[] = { 0, 1, 4095, 4096, 4097, 20000 };
    unsigned char *w = calloc(1, 20000); if (!w) return;
    WCHAR nm[8];
    for (int i = 0; i < 6; i++) { wsprintfW(nm, L"s%d", i); ses_put(stg, nm, w, sz[i]); }
    WCHAR l[8][40]; ULONGLONG s[8]; int n = ses_list(a, stg, l, NULL, s, 8);
    EQ(n, 6);
    for (int i = 0; i < n; i++) { int k = _wtoi(l[i] + 1); EQ(s[i], sz[k]); }
    free(w);
}
