#include "cmg.h"

typedef HRESULT (WINAPI *cfp_fn)(LPCOLESTR, CLSID *);
typedef HRESULT (WINAPI *pfc_fn)(REFCLSID, LPOLESTR *);
typedef HRESULT (WINAPI *cfo_fn)(LPOLESTR, LPCLSID, BOOL);
#define CFP(m) ((cfp_fn)t_proc_in(m, "CLSIDFromProgID"))
#define CFPEX(m) ((cfp_fn)t_proc_in(m, "CLSIDFromProgIDEx"))
#define PFC(m) ((pfc_fn)t_proc_in(m, "ProgIDFromCLSID"))
#define NEED(f) do { if (!(f)) { t_skip("missing export"); return; } } while (0)

static const GUID G_Missing = {0x6F6C6574, 0x0001, 0x4000, {0x80, 0x00, 0x6F, 0x6C, 0x65, 0x74, 0x00, 0x01}};

/* ---------------- CLSIDFromProgID / Ex ---------------- */
/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(progid_clsidfromprogid_builtin) {
    static const struct { const WCHAR *p; const GUID *g; } t[] = {
        { L"StdFont", &G_StdFont }, { L"StdPicture", &G_StdPicture },
    };
    FOR_MODS(m) {
        NEED(CFP(m));
        for (size_t i = 0; i < 2; i++) {
            GUID g; memset(&g, 0xCC, sizeof g);
            EXPECT_EQ(CFP(m)(t[i].p, &g), S_OK);
            CHECK(cmg_guid_eq(&g, t[i].g));
        }
    }
}
TEST(progid_clsidfromprogidex_builtin) {
    static const struct { const WCHAR *p; const GUID *g; } t[] = {
        { L"StdFont", &G_StdFont }, { L"StdPicture", &G_StdPicture },
    };
    FOR_MODS(m) {
        NEED(CFPEX(m));
        for (size_t i = 0; i < 2; i++) {
            GUID g; memset(&g, 0xCC, sizeof g);
            EXPECT_EQ(CFPEX(m)(t[i].p, &g), S_OK);
            CHECK(cmg_guid_eq(&g, t[i].g));
        }
    }
}
TEST(progid_clsidfromprogid_null_args) {
    FOR_MODS(m) {
        NEED(CFP(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFP(m)(NULL, &g), CMG_E_INVALIDARG);
        CHECK(((unsigned char *)&g)[0] == 0xCC);       /* output not touched */
        EXPECT_EQ(CFP(m)(L"StdFont", NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(CFP(m)(L"olet.NoSuch", NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(CFP(m)(NULL, NULL), CMG_E_INVALIDARG);
    }
}
TEST(progid_clsidfromprogid_empty_string) {
    FOR_MODS(m) {
        NEED(CFP(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFP(m)(L"", &g), CMG_CLASSSTRING);
        CHECK(cmg_guid_eq(&g, &G_NULL));               /* output zeroed before lookup */
    }
}
TEST(progid_clsidfromprogid_unregistered) {
    static const WCHAR *t[] = { L"olet.NoSuchProgId", L"olet.NoSuchProgId.1", L"x", L"StdFontX", L"StdFon", L"Std Font", L" StdFont", L"StdFont ", L"olet no.such" };
    FOR_MODS(m) {
        NEED(CFP(m));
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            GUID g; memset(&g, 0xCC, sizeof g);
            HRESULT hr = CFP(m)(t[i], &g);
            CHECKF(hr == CMG_CLASSSTRING, "\"%ls\" -> 0x%08lx", t[i], (unsigned long)hr);
            CHECKF(cmg_guid_eq(&g, &G_NULL), "\"%ls\": output not zeroed", t[i]);
        }
    }
}
TEST(progid_clsidfromprogidex_failure) {
    FOR_MODS(m) {
        NEED(CFPEX(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        HRESULT hr = CFPEX(m)(L"olet.NoSuchProgId.Ex", &g);
        CHECK(FAILED(hr));
        if (hr != CMG_CLASSSTRING) t_note("%s CLSIDFromProgIDEx(unregistered) = 0x%08lx", m, (unsigned long)hr);
        EXPECT_EQ(CFPEX(m)(NULL, &g), CMG_E_INVALIDARG);
        EXPECT_EQ(CFPEX(m)(L"StdFont", NULL), CMG_E_INVALIDARG);
    }
}
TEST(progid_clsidfromprogid_case_note) {
    FOR_MODS(m) {
        NEED(CFP(m));
        GUID g; memset(&g, 0, sizeof g);
        HRESULT hr = CFP(m)(L"stdfont", &g);
        t_note("%s CLSIDFromProgID(\"stdfont\") = 0x%08lx match=%d", m, (unsigned long)hr, cmg_guid_eq(&g, &G_StdFont));
        hr = CFP(m)(L"STDPICTURE", &g);
        t_note("%s CLSIDFromProgID(\"STDPICTURE\") = 0x%08lx match=%d", m, (unsigned long)hr, cmg_guid_eq(&g, &G_StdPicture));
    }
}
TEST(progid_clsidfromprogid_guid_string_note) {
    FOR_MODS(m) {
        NEED(CFP(m));
        GUID g; memset(&g, 0, sizeof g);
        HRESULT hr = CFP(m)(CMG_STDFONT_STR, &g);
        t_note("%s CLSIDFromProgID(\"{StdFont GUID}\") = 0x%08lx", m, (unsigned long)hr);
    }
}
TEST(progid_clsidfromprogid_long_names) {
    FOR_MODS(m) {
        NEED(CFP(m));
        WCHAR big[600];
        for (int len = 100; len <= 500; len += 200) {
            for (int i = 0; i < len; i++) big[i] = 'a' + (i % 26);
            big[len] = 0;
            GUID g;
            HRESULT hr = CFP(m)(big, &g);
            CHECKF(FAILED(hr), "len %d -> 0x%08lx", len, (unsigned long)hr);
            if (hr != CMG_CLASSSTRING) t_note("%s long progid len %d -> 0x%08lx", m, len, (unsigned long)hr);
        }
    }
}
TEST(progid_clsidfromprogid_common_registered_note) {
    static const WCHAR *t[] = { L"Scripting.FileSystemObject", L"Scripting.Dictionary", L"WScript.Shell", L"Shell.Application", L"htmlfile", L"MSXML2.DOMDocument", L"StdFont", L"StdPicture", L"Excel.Application" };
    FOR_MODS(m) {
        NEED(CFP(m));
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            GUID g; memset(&g, 0, sizeof g);
            HRESULT hr = CFP(m)(t[i], &g);
            WCHAR s[40]; lstrcpyW(s, L"-");
            if (SUCCEEDED(hr)) U(StringFromGUID2)(&g, s, 40);
            t_note("%s CLSIDFromProgID(%ls) = 0x%08lx %ls", m, t[i], (unsigned long)hr, s);
        }
    }
}

/* ---------------- ProgIDFromCLSID ---------------- */
TEST(progid_progidfromclsid_builtin) {
    static const struct { const GUID *g; const WCHAR *p; } t[] = { { &G_StdFont, L"StdFont" }, { &G_StdPicture, L"StdPicture" } };
    FOR_MODS(m) {
        NEED(PFC(m));
        for (size_t i = 0; i < 2; i++) {
            WCHAR *p = (WCHAR *)(ULONG_PTR)0x77;
            HRESULT hr = PFC(m)(t[i].g, &p);
            EXPECT_EQ(hr, S_OK);
            if (hr == S_OK && p) {
                CHECK(!lstrcmpiW(p, t[i].p));
                if (lstrcmpW(p, t[i].p)) t_note("%s ProgID differs in case: %ls", m, p);
                CoTaskMemFree(p);
            }
        }
    }
}
TEST(progid_progidfromclsid_task_memory) {
    FOR_MODS(m) {
        NEED(PFC(m));
        IMalloc *im = cmg_getmalloc(m);
        WCHAR *p = NULL;
        EXPECT_EQ(PFC(m)(&G_StdFont, &p), S_OK);
        CHECK(p != NULL); if (!p) return;
        if (im) { EXPECT_EQ(im->lpVtbl->DidAlloc(im, p), 1); SIZE_T sz = im->lpVtbl->GetSize(im, p); if (sz != (lstrlenW(p) + 1) * sizeof(WCHAR)) t_note("%s ProgIDFromCLSID block size %llu for %d chars", m, (unsigned long long)sz, lstrlenW(p)); }
        CoTaskMemFree(p);
    }
}
TEST(progid_progidfromclsid_not_registered) {
    FOR_MODS(m) {
        NEED(PFC(m));
        WCHAR *p = (WCHAR *)(ULONG_PTR)0xDEAD0;
        EXPECT_EQ(PFC(m)(&G_Missing, &p), CMG_CLASSNOTREG);
        CHECK(p == NULL);                               /* cleared before lookup */
        p = (WCHAR *)(ULONG_PTR)0xDEAD0;
        EXPECT_EQ(PFC(m)(&G_Ones, &p), CMG_CLASSNOTREG);
        CHECK(p == NULL);
    }
}
TEST(progid_progidfromclsid_null_clsid_value) {
    FOR_MODS(m) {
        NEED(PFC(m));
        WCHAR *p = (WCHAR *)(ULONG_PTR)0xDEAD0;
        HRESULT hr = PFC(m)(&G_NULL, &p);
        CHECK(FAILED(hr));
        CHECK(p == NULL);
        if (hr != CMG_CLASSNOTREG) t_note("%s ProgIDFromCLSID(CLSID_NULL) = 0x%08lx", m, (unsigned long)hr);
    }
}
TEST(progid_progidfromclsid_null_args) {
    FOR_MODS(m) {
        NEED(PFC(m));
        WCHAR *p = (WCHAR *)(ULONG_PTR)0xDEAD0;
        EXPECT_EQ(PFC(m)(NULL, &p), CMG_E_INVALIDARG);
        CHECK(p == (WCHAR *)0xDEAD0);
        EXPECT_EQ(PFC(m)(&G_StdFont, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(PFC(m)(NULL, NULL), CMG_E_INVALIDARG);
    }
}
TEST(progid_progidfromclsid_roundtrip) {
    static const GUID *t[] = { &G_StdFont, &G_StdPicture };
    FOR_MODS(m) {
        NEED(PFC(m)); NEED(CFP(m));
        for (int i = 0; i < 2; i++) {
            WCHAR *p = NULL; GUID g;
            EXPECT_EQ(PFC(m)(t[i], &p), S_OK);
            if (!p) continue;
            memset(&g, 0, sizeof g);
            EXPECT_EQ(CFP(m)(p, &g), S_OK);
            CHECK(cmg_guid_eq(&g, t[i]));
            CoTaskMemFree(p);
        }
    }
}
TEST(progid_progidfromclsid_repeatable) {
    FOR_MODS(m) {
        NEED(PFC(m));
        WCHAR *first = NULL;
        EXPECT_EQ(PFC(m)(&G_StdFont, &first), S_OK);
        for (int i = 0; i < 200; i++) {
            WCHAR *p = NULL;
            if (PFC(m)(&G_StdFont, &p) != S_OK || !p || !first || lstrcmpW(p, first)) { CHECK(0); CoTaskMemFree(p); break; }
            CoTaskMemFree(p);
        }
        CoTaskMemFree(first);
    }
}
TEST(progid_progidfromclsid_after_com_init) {
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    HRESULT hi = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    CHECK(SUCCEEDED(hi));
    if (FAILED(hi)) return;
    FOR_MODS(m) {
        NEED(PFC(m)); NEED(CFP(m));
        WCHAR *p = NULL; GUID g;
        EXPECT_EQ(PFC(m)(&G_StdFont, &p), S_OK);
        CoTaskMemFree(p);
        EXPECT_EQ(CFP(m)(L"StdFont", &g), S_OK);
        CHECK(cmg_guid_eq(&g, &G_StdFont));
    }
    U(CoUninitialize)();
}
TEST(progid_progidfromclsid_mta_thread) {
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    HRESULT hi = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    CHECK(SUCCEEDED(hi));
    if (FAILED(hi)) return;
    WCHAR *p = NULL;
    EXPECT_EQ(PFC("ole32.dll")(&G_StdPicture, &p), S_OK);
    CoTaskMemFree(p);
    U(CoUninitialize)();
}

/* ---------------- CLSIDFromOle1Class / CoIsOle1Class ---------------- */
TEST(progid_clsidfromole1class_args) {
    FOR_MODS(m) {
        cfo_fn f = (cfo_fn)t_proc_in(m, "CLSIDFromOle1Class");
        NEED(f);
        GUID g; memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(f(NULL, &g, FALSE), CMG_E_INVALIDARG);
        EXPECT_EQ(f(L"StdFont", NULL, FALSE), CMG_E_INVALIDARG);
        EXPECT_EQ(f(NULL, NULL, FALSE), CMG_E_INVALIDARG);
        memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(f(L"", &g, FALSE), CMG_CLASSSTRING);
        CHECK(cmg_guid_eq(&g, &G_NULL));
    }
}
TEST(progid_clsidfromole1class_unknown) {
    FOR_MODS(m) {
        cfo_fn f = (cfo_fn)t_proc_in(m, "CLSIDFromOle1Class");
        NEED(f);
        GUID g;
        HRESULT hr = f(L"olet.NoSuchOle1Class", &g, FALSE);
        CHECK(FAILED(hr));
        t_note("%s CLSIDFromOle1Class(unknown) = 0x%08lx", m, (unsigned long)hr);
    }
}
TEST(progid_isole1class) {
    UDECL(BOOL, CoIsOle1Class, (REFCLSID));
    REQUIRE(UN(CoIsOle1Class));
    EXPECT_EQ(UN(CoIsOle1Class)(NULL), 0);
    EXPECT_EQ(UN(CoIsOle1Class)(&G_StdFont), 0);
    EXPECT_EQ(UN(CoIsOle1Class)(&G_StdPicture), 0);
    EXPECT_EQ(UN(CoIsOle1Class)(&G_Missing), 0);
    EXPECT_EQ(UN(CoIsOle1Class)(&G_NULL), 0);
    GUID g = {0x00030000, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};   /* OLE1 range, unregistered */
    g.Data1 = 0x0003FFF1; EXPECT_EQ(UN(CoIsOle1Class)(&g), 0);
    g.Data1 = 0x0004FFF1; EXPECT_EQ(UN(CoIsOle1Class)(&g), 0);
}

/* ---------------- CoCreateGuid ---------------- */
typedef HRESULT (WINAPI *ccg_fn)(GUID *);
#define CCG(m) ((ccg_fn)t_proc_in(m, "CoCreateGuid"))

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(guid_cocreateguid_null) {
    FOR_MODS(m) { NEED(CCG(m)); EXPECT_EQ(CCG(m)(NULL), CMG_E_INVALIDARG); }
}
TEST(guid_cocreateguid_ok_and_nonnull) {
    FOR_MODS(m) {
        NEED(CCG(m));
        for (int i = 0; i < 50; i++) {
            GUID g; memset(&g, 0, sizeof g);
            EXPECT_EQ(CCG(m)(&g), S_OK);
            CHECK(!cmg_guid_eq(&g, &G_NULL));
            CHECK(!cmg_guid_eq(&g, &G_Ones));
        }
    }
}
TEST(guid_cocreateguid_variant_version_bits) {
    FOR_MODS(m) {
        NEED(CCG(m));
        for (int i = 0; i < 2000; i++) {
            GUID g; CCG(m)(&g);
            if ((g.Data3 >> 12) != 4 || (g.Data4[0] & 0xC0) != 0x80) { CHECKF(0, "guid %d: Data3=%04x Data4[0]=%02x", i, g.Data3, g.Data4[0]); break; }
        }
        CHECK(1);
    }
}
static int cmp_guid(const void *a, const void *b) { return memcmp(a, b, sizeof(GUID)); }
TEST(guid_cocreateguid_unique) {
    FOR_MODS(m) {
        NEED(CCG(m));
        enum { N = 20000 };
        GUID *v = malloc(N * sizeof(GUID));
        CHECK(v != NULL); if (!v) return;
        for (int i = 0; i < N; i++) CCG(m)(&v[i]);
        qsort(v, N, sizeof(GUID), cmp_guid);
        int dup = 0; for (int i = 1; i < N; i++) if (!memcmp(&v[i], &v[i - 1], sizeof(GUID))) dup++;
        EXPECT_EQ(dup, 0);
        free(v);
    }
}
TEST(guid_cocreateguid_unique_across_modules) {
    REQUIRE(CCG("ole32.dll")); REQUIRE(CCG("combase.dll"));
    enum { N = 3000 };
    GUID *v = malloc(2 * N * sizeof(GUID));
    CHECK(v != NULL); if (!v) return;
    for (int i = 0; i < N; i++) { CCG("ole32.dll")(&v[2 * i]); CCG("combase.dll")(&v[2 * i + 1]); }
    qsort(v, 2 * N, sizeof(GUID), cmp_guid);
    int dup = 0; for (int i = 1; i < 2 * N; i++) if (!memcmp(&v[i], &v[i - 1], sizeof(GUID))) dup++;
    EXPECT_EQ(dup, 0);
    free(v);
}
TEST(guid_cocreateguid_not_sequential) {
    FOR_MODS(m) {
        NEED(CCG(m));
        GUID a, b; CCG(m)(&a); CCG(m)(&b);
        CHECK(!cmg_guid_eq(&a, &b));
        /* random (version 4): consecutive values do not share the 6-byte node tail */
        int same_tail = !memcmp(a.Data4 + 2, b.Data4 + 2, 6);
        CHECK(!same_tail);
    }
}
TEST(guid_cocreateguid_bit_entropy) {
    /* over many GUIDs every non-fixed bit takes both values (random, not a counter) */
    FOR_MODS(m) {
        NEED(CCG(m));
        unsigned char seen0[16] = {0}, seen1[16] = {0};
        for (int i = 0; i < 400; i++) {
            GUID g; CCG(m)(&g); unsigned char *b = (unsigned char *)&g;
            for (int k = 0; k < 16; k++) { seen1[k] |= b[k]; seen0[k] |= (unsigned char)~b[k]; }
        }
        unsigned char fixed[16] = {0}; fixed[6] = 0xF0; fixed[7] = 0x00 /*Data3 hi byte is byte 7*/; fixed[8] = 0xC0;
        for (int k = 0; k < 16; k++) {
            unsigned char need = 0xFF;
            if (k == 7) need = 0x0F;               /* top nibble fixed to 4 */
            if (k == 8) need = 0x3F;               /* variant bits fixed */
            CHECKF((seen1[k] & need) == need && (seen0[k] & need) == need, "byte %d not random: seen1=%02x seen0=%02x", k, seen1[k], seen0[k]);
        }
    }
}
static GUID thr_guids[4][2500]; static ccg_fn thr_fn;
static DWORD WINAPI th_guids(LPVOID p) { int i = (int)(ULONG_PTR)p; for (int k = 0; k < 2500; k++) thr_fn(&thr_guids[i][k]); return 0; }
TEST(guid_cocreateguid_threads_unique) {
    REQUIRE(CCG("ole32.dll"));
    if (t_is_wine()) { t_skip("crashes inside wine UuidCreate when called concurrently"); return; }
    thr_fn = CCG("ole32.dll");                    /* resolve once: the harness lookup cache is not thread-safe */
    HANDLE h[4];
    for (int i = 0; i < 4; i++) { h[i] = CreateThread(NULL, 0, th_guids, (LPVOID)(ULONG_PTR)i, 0, NULL); CHECK(h[i] != NULL); }
    for (int i = 0; i < 4; i++) if (h[i]) { CHECK(WaitForSingleObject(h[i], 10000) == WAIT_OBJECT_0); CloseHandle(h[i]); }
    /* sort a heap copy: qsort over the static 2-D array walks off the end of the image on Windows' msvcrt */
    GUID *all = malloc(4 * 2500 * sizeof(GUID)); CHECK(all != NULL); if (!all) return;
    memcpy(all, thr_guids, 4 * 2500 * sizeof(GUID));
    qsort(all, 4 * 2500, sizeof(GUID), cmp_guid);
    int dup = 0; for (int i = 1; i < 4 * 2500; i++) if (!memcmp(&all[i], &all[i - 1], sizeof(GUID))) dup++;
    free(all);
    EXPECT_EQ(dup, 0);
}
TEST(guid_cocreateguid_string_roundtrip) {
    FOR_MODS(m) {
        NEED(CCG(m));
        for (int i = 0; i < 300; i++) {
            GUID g, o; WCHAR s[40]; CCG(m)(&g);
            U(StringFromGUID2)(&g, s, 40);
            U(CLSIDFromString)(s, &o);
            if (!cmg_guid_eq(&g, &o)) { CHECK(0); break; }
        }
        CHECK(1);
    }
}
TEST(guid_cocreateguid_vs_uuidcreate) {
    REQUIRE(U(UuidCreate)); REQUIRE(CCG("ole32.dll"));
    for (int i = 0; i < 200; i++) {
        GUID a, b;
        RPC_STATUS st = U(UuidCreate)((UUID *)&a);
        CHECK(st == RPC_S_OK || st == RPC_S_UUID_LOCAL_ONLY);
        CCG("ole32.dll")(&b);
        CHECK((a.Data3 >> 12) == 4 && (a.Data4[0] & 0xC0) == 0x80);
        CHECK((b.Data3 >> 12) == 4 && (b.Data4[0] & 0xC0) == 0x80);
        CHECK(!cmg_guid_eq(&a, &b));
    }
}

/* ---------------- CoGetCurrentProcess / CoGetCallerTID / CoSetState ---------------- */
typedef DWORD (WINAPI *gcp_fn)(void);
typedef HRESULT (WINAPI *gct_fn)(DWORD *);
TEST(guid_cogetcurrentprocess_stable_per_thread) {
    FOR_MODS(m) {
        gcp_fn f = (gcp_fn)t_proc_in(m, "CoGetCurrentProcess");
        NEED(f);
        DWORD a = f();
        CHECK(a != 0);
        for (int i = 0; i < 100; i++) if (f() != a) { CHECK(0); break; }
    }
}
static gcp_fn g_gcp; static DWORD g_tid_vals[2]; static HANDLE g_ev_go, g_ev_ready[2];
static DWORD WINAPI th_cp(LPVOID p) {
    int i = (int)(ULONG_PTR)p; gcp_fn f = g_gcp;
    g_tid_vals[i] = f(); SetEvent(g_ev_ready[i]); WaitForSingleObject(g_ev_go, 5000); return 0;
}
TEST(guid_cogetcurrentprocess_distinct_threads) {
    gcp_fn f = (gcp_fn)t_proc_in("ole32.dll", "CoGetCurrentProcess");
    REQUIRE(f);
    g_gcp = f;
    g_ev_go = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 2; i++) g_ev_ready[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE h[2];
    for (int i = 0; i < 2; i++) h[i] = CreateThread(NULL, 0, th_cp, (LPVOID)(ULONG_PTR)i, 0, NULL);
    WaitForSingleObject(g_ev_ready[0], 5000); WaitForSingleObject(g_ev_ready[1], 5000);
    DWORD me = f();
    SetEvent(g_ev_go);
    for (int i = 0; i < 2; i++) { WaitForSingleObject(h[i], 5000); CloseHandle(h[i]); CloseHandle(g_ev_ready[i]); }
    CloseHandle(g_ev_go);
    CHECK(g_tid_vals[0] != 0 && g_tid_vals[1] != 0 && me != 0);
    CHECK(g_tid_vals[0] != g_tid_vals[1]);
    CHECK(me != g_tid_vals[0] && me != g_tid_vals[1]);
}
TEST(guid_cogetcallertid_null) {
    FOR_MODS(m) {
        gct_fn f = (gct_fn)t_proc_in(m, "CoGetCallerTID");
        NEED(f);
        EXPECT_EQ(f(NULL), CMG_E_INVALIDARG);
    }
}
TEST(guid_cogetcallertid_outside_call) {
    FOR_MODS(m) {
        gct_fn f = (gct_fn)t_proc_in(m, "CoGetCallerTID");
        NEED(f);
        HRESULT hi = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
        DWORD tid = 0xFFFFFFFF;
        HRESULT hr = f(&tid);
        t_note("%s CoGetCallerTID outside any call: hr=0x%08lx tid=%lu", m, (unsigned long)hr, (unsigned long)tid);
        CHECK(hr == S_OK || hr == S_FALSE);
        if (SUCCEEDED(hi)) U(CoUninitialize)();
    }
}
TEST(guid_cosetstate_returns_ok) {
    UDECL(HRESULT, CoSetState, (IUnknown *));
    REQUIRE(UN(CoSetState));
    EXPECT_EQ(UN(CoSetState)(NULL), S_OK);              /* ole32 export is a stub returning 0 */
}
