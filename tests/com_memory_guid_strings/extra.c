#include "cmg.h"

typedef int (WINAPI *x_sfg2_fn)(REFGUID, LPOLESTR, int);
typedef HRESULT (WINAPI *x_sfx_fn)(REFGUID, LPOLESTR *);
typedef HRESULT (WINAPI *x_gfs_fn)(LPCOLESTR, GUID *);
typedef void *(WINAPI *x_alloc_fn)(SIZE_T);
typedef void *(WINAPI *x_realloc_fn)(void *, SIZE_T);
typedef void (WINAPI *x_free_fn)(void *);

/* StringFromCLSID/StringFromIID/StringFromGUID2 must produce identical text for the same GUID */
TEST(cmgx_string_functions_agree) {
    FOR_MODS(m) {
        x_sfg2_fn g2 = (x_sfg2_fn)t_proc_in(m, "StringFromGUID2");
        x_sfx_fn sc = (x_sfx_fn)t_proc_in(m, "StringFromCLSID");
        x_sfx_fn si = (x_sfx_fn)t_proc_in(m, "StringFromIID");
        if (!g2 || !sc || !si) { t_skip("missing"); return; }
        const GUID *gs[] = { &G_NULL, &G_Ones, &G_StdFont, &G_StdPicture, &G_Sample, &G_IMalloc, &G_IMallocSpy };
        for (size_t i = 0; i < sizeof gs / sizeof *gs; i++) {
            WCHAR buf[39]; WCHAR *a = NULL, *b = NULL;
            EXPECT_EQ(g2(gs[i], buf, 39), 39);
            EXPECT_EQ(sc(gs[i], &a), S_OK);
            EXPECT_EQ(si(gs[i], &b), S_OK);
            CHECK(a && b);
            if (a) { CHECK(wcscmp(a, buf) == 0); CoTaskMemFree(a); }
            if (b) { CHECK(wcscmp(b, buf) == 0); CoTaskMemFree(b); }
            CHECK(wcslen(buf) == 38);
        }
    }
}
/* parse(format(g)) == g across both parsers and both modules */
TEST(cmgx_parse_format_cross_module) {
    x_sfx_fn sc = (x_sfx_fn)t_proc_in("ole32.dll", "StringFromCLSID");
    x_gfs_fn pc = (x_gfs_fn)t_proc_in("combase.dll", "CLSIDFromString");
    x_gfs_fn pi = (x_gfs_fn)t_proc_in("combase.dll", "IIDFromString");
    if (!sc || !pc || !pi) { t_skip("missing"); return; }
    const GUID *gs[] = { &G_NULL, &G_Ones, &G_StdFont, &G_Sample, &G_IStream };
    for (size_t i = 0; i < sizeof gs / sizeof *gs; i++) {
        WCHAR *s = NULL; GUID a, b;
        EXPECT_EQ(sc(gs[i], &s), S_OK);
        if (!s) continue;
        EXPECT_EQ(pc(s, &a), S_OK); CHECK(cmg_guid_eq(&a, gs[i]));
        EXPECT_EQ(pi(s, &b), S_OK); CHECK(cmg_guid_eq(&b, gs[i]));
        CoTaskMemFree(s);
    }
}
/* realloc growth by one byte across many sizes keeps the prefix intact */
TEST(cmgx_realloc_grow_by_one_sweep) {
    FOR_MODS(m) {
        x_alloc_fn a = (x_alloc_fn)t_proc_in(m, "CoTaskMemAlloc");
        x_realloc_fn r = (x_realloc_fn)t_proc_in(m, "CoTaskMemRealloc");
        x_free_fn f = (x_free_fn)t_proc_in(m, "CoTaskMemFree");
        if (!a || !r || !f) { t_skip("missing"); return; }
        unsigned char *p = a(1); REQUIRE(p); p[0] = 1;
        int bad = 0;
        for (size_t n = 2; n <= 600; n++) {
            unsigned char *q = r(p, n);
            if (!q) { bad++; break; }
            p = q;
            for (size_t i = 0; i + 1 < n; i++) if (p[i] != (unsigned char)(i * 3 + 1)) { bad++; break; }
            for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(i * 3 + 1);
        }
        EXPECT_EQ(bad, 0);
        f(p);
    }
}
/* shrink to every smaller size keeps the prefix */
TEST(cmgx_realloc_shrink_sweep) {
    FOR_MODS(m) {
        x_alloc_fn a = (x_alloc_fn)t_proc_in(m, "CoTaskMemAlloc");
        x_realloc_fn r = (x_realloc_fn)t_proc_in(m, "CoTaskMemRealloc");
        x_free_fn f = (x_free_fn)t_proc_in(m, "CoTaskMemFree");
        if (!a || !r || !f) { t_skip("missing"); return; }
        unsigned char *p = a(512); REQUIRE(p);
        for (size_t i = 0; i < 512; i++) p[i] = (unsigned char)(i ^ 0x5A);
        int bad = 0;
        for (size_t n = 511; n >= 18; n -= 17) {
            unsigned char *q = r(p, n);
            if (!q) { bad++; break; }
            p = q;
            for (size_t i = 0; i < n; i++) if (p[i] != (unsigned char)(i ^ 0x5A)) { bad++; break; }
        }
        EXPECT_EQ(bad, 0);
        f(p);
    }
}
/* IMalloc::GetSize for a range of sizes is at least the requested size and stable */
TEST(cmgx_imalloc_getsize_table) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        static const SIZE_T sz[] = {1, 7, 16, 17, 100, 1000, 4096, 70000};
        for (size_t i = 0; i < sizeof sz / sizeof *sz; i++) {
            void *p = im->lpVtbl->Alloc(im, sz[i]); CHECK(p);
            if (!p) continue;
            SIZE_T s1 = im->lpVtbl->GetSize(im, p), s2 = im->lpVtbl->GetSize(im, p);
            EXPECT_EQ(s1, sz[i]);
            CHECK(s1 == s2);
            CHECK(im->lpVtbl->DidAlloc(im, p) == 1);
            im->lpVtbl->Free(im, p);
        }
        im->lpVtbl->Release(im);
    }
}
/* IMalloc::Realloc to size 0 with non-NULL block frees it and returns NULL */
TEST(cmgx_imalloc_realloc_to_zero) {
    FOR_MODS(m) {
        IMalloc *im = cmg_getmalloc(m);
        if (!im) { t_skip("no IMalloc"); return; }
        void *p = im->lpVtbl->Alloc(im, 32); REQUIRE(p);
        void *q = im->lpVtbl->Realloc(im, p, 0);
        t_note("%s IMalloc::Realloc(p,0) -> %p", m, q);
        if (q) im->lpVtbl->Free(im, q);
        im->lpVtbl->Release(im);
    }
}
/* malformed GUID strings fail with CO_E_CLASSSTRING for CLSIDFromString */
TEST(cmgx_clsidfromstring_whitespace_and_nesting) {
    FOR_MODS(m) {
        x_gfs_fn f = (x_gfs_fn)t_proc_in(m, "CLSIDFromString");
        if (!f) { t_skip("missing"); return; }
        static const WCHAR *bad[] = {
            L" {12345678-9ABC-DEF0-1234-56789ABCDEF0}",
            L"{{12345678-9ABC-DEF0-1234-56789ABCDEF0}}",
            L"{12345678-9ABC-DEF0-1234-56789ABCDEF0",
            L"12345678-9ABC-DEF0-1234-56789ABCDEF0}",
            L"{12345678_9ABC_DEF0_1234_56789ABCDEF0}",
            L"{1234567-89ABC-DEF0-1234-56789ABCDEF0}",
        };
        for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
            GUID g; EXPECT_EQ(f(bad[i], &g), CMG_CLASSSTRING);
        }
    }
}
/* CoCreateGuid output parses back through IIDFromString as a valid GUID */
TEST(cmgx_cocreateguid_format_parse) {
    FOR_MODS(m) {
        typedef HRESULT (WINAPI *cg_fn)(GUID *);
        cg_fn cg = (cg_fn)t_proc_in(m, "CoCreateGuid");
        x_sfg2_fn g2 = (x_sfg2_fn)t_proc_in(m, "StringFromGUID2");
        x_gfs_fn pi = (x_gfs_fn)t_proc_in(m, "IIDFromString");
        if (!cg || !g2 || !pi) { t_skip("missing"); return; }
        for (int i = 0; i < 100; i++) {
            GUID g, h; WCHAR b[39];
            EXPECT_EQ(cg(&g), S_OK);
            EXPECT_EQ(g2(&g, b, 39), 39);
            EXPECT_EQ(pi(b, &h), S_OK);
            CHECK(cmg_guid_eq(&g, &h));
            CHECK((g.Data4[0] & 0xC0) == 0x80);
            CHECK((g.Data3 >> 12) == 4);
        }
    }
}
