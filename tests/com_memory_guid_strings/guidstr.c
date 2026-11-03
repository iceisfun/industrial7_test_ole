#include "cmg.h"

typedef int (WINAPI *sfg2_fn)(REFGUID, LPOLESTR, int);
typedef HRESULT (WINAPI *sfx_fn)(REFGUID, LPOLESTR *);
typedef HRESULT (WINAPI *gfs_fn)(LPCOLESTR, GUID *);
#define SFG2(m) ((sfg2_fn)t_proc_in(m, "StringFromGUID2"))
#define SFCLSID(m) ((sfx_fn)t_proc_in(m, "StringFromCLSID"))
#define SFIID(m) ((sfx_fn)t_proc_in(m, "StringFromIID"))
#define CFS(m) ((gfs_fn)t_proc_in(m, "CLSIDFromString"))
#define IFS(m) ((gfs_fn)t_proc_in(m, "IIDFromString"))
#define NEED(f) do { if (!(f)) { t_skip("missing export"); return; } } while (0)

static const struct { const GUID *g; const WCHAR *s; } k_guids[] = {
    { &G_NULL, CMG_NULL_STR }, { &G_Ones, CMG_ONES_STR }, { &G_StdFont, CMG_STDFONT_STR },
    { &G_StdPicture, CMG_STDPIC_STR }, { &G_Sample, CMG_SAMPLE_STR },
    { &G_IUnknown, L"{00000000-0000-0000-C000-000000000046}" },
    { &G_IMalloc, L"{00000002-0000-0000-C000-000000000046}" },
    { &G_IStream, L"{0000000C-0000-0000-C000-000000000046}" },
};
#define NGUIDS (sizeof k_guids / sizeof *k_guids)

static void lower(WCHAR *s) { for (; *s; s++) if (*s >= 'A' && *s <= 'F') *s += 32; }
static unsigned rng_state = 0x1234567u;
static unsigned rnd(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }
static GUID rand_guid(void) { GUID g; unsigned char *b = (unsigned char *)&g; for (int i = 0; i < 16; i++) b[i] = (unsigned char)(rnd() >> 11); return g; }

/* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
TEST(guid_str2_known_values) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            WCHAR buf[64]; memset(buf, 0xCC, sizeof buf);
            EXPECT_EQ(SFG2(m)(k_guids[i].g, buf, 64), 39);
            EXPECT_WSTR(buf, k_guids[i].s);
            EXPECT_EQ(wcslen(buf), 38);
        }
    }
}
TEST(guid_str2_field_layout) {
    GUID g = {0x01020304, 0x0506, 0x0708, {0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10}};
    FOR_MODS(m) {
        NEED(SFG2(m));
        WCHAR buf[40];
        EXPECT_EQ(SFG2(m)(&g, buf, 40), 39);
        EXPECT_WSTR(buf, L"{01020304-0506-0708-090A-0B0C0D0E0F10}");   /* Data1-3 as integers, Data4 as bytes */
    }
}
TEST(guid_str2_uppercase_hex) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        for (int d = 0; d < 16; d++) {
            GUID g; memset(&g, d * 0x11, sizeof g);
            WCHAR buf[40];
            EXPECT_EQ(SFG2(m)(&g, buf, 40), 39);
            for (int i = 0; buf[i]; i++) {
                WCHAR c = buf[i];
                int ok = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '-' || c == '{' || c == '}';
                if (!ok) { CHECKF(0, "unexpected char 0x%x at %d for fill %x", c, i, d * 0x11); break; }
            }
        }
    }
}
TEST(guid_str2_punctuation_positions) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        GUID g = rand_guid(); WCHAR b[40];
        EXPECT_EQ(SFG2(m)(&g, b, 40), 39);
        CHECK(b[0] == '{' && b[9] == '-' && b[14] == '-' && b[19] == '-' && b[24] == '-' && b[37] == '}' && b[38] == 0);
    }
}
TEST(guid_str2_buffer_size_boundary) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        for (int cch = 1; cch <= 38; cch++) {           /* too small for 38 chars + NUL: nothing written */
            WCHAR b[64]; memset(b, 0xAB, sizeof b);
            EXPECT_EQ(SFG2(m)(&G_StdFont, b, cch), 0);
            int untouched = 1; for (int i = 0; i < 64; i++) if (b[i] != 0xABAB) untouched = 0;
            CHECKF(untouched, "buffer modified for cch=%d", cch);
        }
        for (int cch = 39; cch <= 45; cch++) {
            WCHAR b[64]; memset(b, 0xAB, sizeof b);
            EXPECT_EQ(SFG2(m)(&G_StdFont, b, cch), 39);
            EXPECT_WSTR(b, CMG_STDFONT_STR);
            CHECK(b[39] == 0xABAB);                      /* nothing written past the NUL */
        }
    }
}
TEST(guid_str2_zero_and_negative_size) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        WCHAR b[40]; memset(b, 0xAB, sizeof b);
        EXPECT_EQ(SFG2(m)(&G_StdFont, b, 0), 0);
        EXPECT_EQ(SFG2(m)(&G_StdFont, b, -1), 0);
        EXPECT_EQ(SFG2(m)(&G_StdFont, b, -39), 0);
        EXPECT_EQ(SFG2(m)(&G_StdFont, b, (int)0x80000000), 0);
        CHECK(b[0] == 0xABAB);
        EXPECT_EQ(SFG2(m)(&G_StdFont, NULL, 0), 0);
        EXPECT_EQ(SFG2(m)(&G_StdFont, NULL, 39), 0);
        EXPECT_EQ(SFG2(m)(&G_StdFont, NULL, 100), 0);
    }
}
TEST(guid_str2_null_guid_pointer) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        WCHAR b[40]; memset(b, 0xAB, sizeof b);
        SET_ERR(0xDEADBEEF);
        EXPECT_EQ(SFG2(m)(NULL, b, 40), 0);
        CHECK(b[0] == 0xABAB);
        EXPECT_EQ(GetLastError(), 0xDEADBEEF);             /* no last-error side effects */
        EXPECT_EQ(SFG2(m)(NULL, NULL, 0), 0);
    }
}
TEST(guid_str2_lasterror_untouched) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        WCHAR b[40];
        SET_ERR(0x31337);
        EXPECT_EQ(SFG2(m)(&G_Sample, b, 40), 39);
        EXPECT_EQ(GetLastError(), 0x31337);
        EXPECT_EQ(SFG2(m)(&G_Sample, b, 10), 0);
        EXPECT_EQ(GetLastError(), 0x31337);
    }
}
TEST(guid_str2_inplace_guid_buffer) {
    /* output buffer need not be aligned: write at odd 2-byte offsets */
    FOR_MODS(m) {
        NEED(SFG2(m));
        WCHAR big[80]; memset(big, 0, sizeof big);
        for (int off = 0; off < 4; off++) {
            EXPECT_EQ(SFG2(m)(&G_Sample, big + off, 80 - off), 39);
            EXPECT_WSTR(big + off, CMG_SAMPLE_STR);
        }
    }
}
TEST(guid_str2_byte_sweep) {
    FOR_MODS(m) {
        NEED(SFG2(m));
        static const WCHAR hex[] = L"0123456789ABCDEF";
        for (int idx = 0; idx < 16; idx++) for (int v = 0; v < 256; v++) {
            GUID g; memset(&g, 0, sizeof g); ((unsigned char *)&g)[idx] = (unsigned char)v;
            WCHAR b[40];
            int r = SFG2(m)(&g, b, 40);
            if (r != 39) { CHECK(r == 39); continue; }
            /* locate the hex pair of byte idx in the textual layout */
            static const int pos[16] = {7,5,3,1, 12,10, 17,15, 20,22, 25,27,29,31,33,35};  /* high-nibble index of each memory byte */
            int p = pos[idx];
            int ok = (b[p] == hex[v >> 4]) && (b[p + 1] == hex[v & 15]);
            if (!ok) CHECKF(0, "byte %d value %02x -> %ls", idx, v, b);
        }
    }
}

TEST(guid_str_clsid_known) {
    FOR_MODS(m) {
        NEED(SFCLSID(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            WCHAR *s = (WCHAR *)(ULONG_PTR)1;
            EXPECT_EQ(SFCLSID(m)(k_guids[i].g, &s), S_OK);
            CHECK(s != NULL && s != (WCHAR *)1);
            if (s && s != (WCHAR *)1) { EXPECT_WSTR(s, k_guids[i].s); CoTaskMemFree(s); }
        }
    }
}
TEST(guid_str_iid_known) {
    FOR_MODS(m) {
        NEED(SFIID(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            WCHAR *s = (WCHAR *)(ULONG_PTR)1;
            EXPECT_EQ(SFIID(m)(k_guids[i].g, &s), S_OK);
            CHECK(s != NULL && s != (WCHAR *)1);
            if (s && s != (WCHAR *)1) { EXPECT_WSTR(s, k_guids[i].s); CoTaskMemFree(s); }
        }
    }
}
TEST(guid_str_clsid_task_memory) {
    FOR_MODS(m) {
        NEED(SFCLSID(m));
        IMalloc *im = cmg_getmalloc(m);
        WCHAR *s = NULL;
        EXPECT_EQ(SFCLSID(m)(&G_StdFont, &s), S_OK);
        CHECK(s != NULL); if (!s) return;
        if (im) {
            EXPECT_EQ(im->lpVtbl->DidAlloc(im, s), 1);
            SIZE_T sz = im->lpVtbl->GetSize(im, s);
            if (sz != 78) t_note("%s StringFromCLSID block size %llu (expected 78 = 39 WCHARs)", m, (unsigned long long)sz);
        }
        CoTaskMemFree(s);
    }
}
TEST(guid_str_iid_task_memory) {
    FOR_MODS(m) {
        NEED(SFIID(m));
        IMalloc *im = cmg_getmalloc(m);
        WCHAR *s = NULL;
        EXPECT_EQ(SFIID(m)(&G_StdFont, &s), S_OK);
        CHECK(s != NULL); if (!s) return;
        if (im) EXPECT_EQ(im->lpVtbl->DidAlloc(im, s), 1);
        CoTaskMemFree(s);
    }
}
TEST(guid_str_clsid_distinct_blocks) {
    FOR_MODS(m) {
        NEED(SFCLSID(m));
        WCHAR *a = NULL, *b = NULL;
        SFCLSID(m)(&G_Sample, &a); SFCLSID(m)(&G_Sample, &b);
        CHECK(a && b && a != b);
        if (a && b) EXPECT_WSTR(a, b);
        CoTaskMemFree(a); CoTaskMemFree(b);
    }
}
TEST(guid_str_clsid_invalid_args) {
    FOR_MODS(m) {
        NEED(SFCLSID(m)); NEED(SFIID(m));
        WCHAR *s = (WCHAR *)(ULONG_PTR)0x5555;
        if (t_is_wine()) { t_skip("wine accepts a NULL clsid here"); return; }
        EXPECT_EQ(SFCLSID(m)(NULL, &s), CMG_E_INVALIDARG);
        CHECK(s == (WCHAR *)0x5555);                      /* output untouched on argument failure */
        EXPECT_EQ(SFCLSID(m)(&G_StdFont, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(SFCLSID(m)(NULL, NULL), CMG_E_INVALIDARG);
        s = (WCHAR *)0x5555;
        EXPECT_EQ(SFIID(m)(NULL, &s), CMG_E_INVALIDARG);
        CHECK(s == (WCHAR *)0x5555);
        EXPECT_EQ(SFIID(m)(&G_StdFont, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(SFIID(m)(NULL, NULL), CMG_E_INVALIDARG);
    }
}
TEST(guid_str_clsid_many_cycles) {
    FOR_MODS(m) {
        NEED(SFCLSID(m)); NEED(SFIID(m));
        for (int i = 0; i < 2000; i++) {
            GUID g = rand_guid(); WCHAR *a = NULL, *b = NULL, ref[40];
            SFG2(m)(&g, ref, 40);
            if (SFCLSID(m)(&g, &a) != S_OK || SFIID(m)(&g, &b) != S_OK || !a || !b || lstrcmpW(a, ref) || lstrcmpW(b, ref)) { CHECK(0); CoTaskMemFree(a); CoTaskMemFree(b); break; }
            CoTaskMemFree(a); CoTaskMemFree(b);
        }
        CHECK(1);
    }
}

/* ---- parsing ---- */
typedef struct { const WCHAR *s; HRESULT hr; const GUID *g; } parse_case;
#define BAD(x) { x, CMG_CLASSSTRING, NULL }

TEST(guid_clsidfromstring_valid) {
    FOR_MODS(m) {
        NEED(CFS(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            GUID g; memset(&g, 0xAB, sizeof g);
            EXPECT_EQ(CFS(m)(k_guids[i].s, &g), S_OK);
            CHECK(cmg_guid_eq(&g, k_guids[i].g));
        }
    }
}
TEST(guid_clsidfromstring_case_insensitive) {
    FOR_MODS(m) {
        NEED(CFS(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            WCHAR s[40]; lstrcpyW(s, k_guids[i].s); lower(s);
            GUID g; memset(&g, 0xAB, sizeof g);
            EXPECT_EQ(CFS(m)(s, &g), S_OK);
            CHECK(cmg_guid_eq(&g, k_guids[i].g));
        }
        GUID g;
        EXPECT_EQ(CFS(m)(L"{0bE35203-8f91-11Ce-9dE3-00aA004bB851}", &g), S_OK);   /* mixed */
        CHECK(cmg_guid_eq(&g, &G_StdFont));
    }
}
TEST(guid_clsidfromstring_null_string) {
    FOR_MODS(m) {
        NEED(CFS(m));
        GUID g; memset(&g, 0xAB, sizeof g);
        EXPECT_EQ(CFS(m)(NULL, &g), S_OK);
        CHECK(cmg_guid_eq(&g, &G_NULL));
    }
}
TEST(guid_clsidfromstring_null_out) {
    FOR_MODS(m) {
        NEED(CFS(m));
        EXPECT_EQ(CFS(m)(CMG_STDFONT_STR, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(CFS(m)(NULL, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(CFS(m)(L"", NULL), CMG_E_INVALIDARG);
    }
}
TEST(guid_clsidfromstring_empty) {
    FOR_MODS(m) {
        NEED(CFS(m));
        GUID g; memset(&g, 0xAB, sizeof g);
        EXPECT_EQ(CFS(m)(L"", &g), CMG_CLASSSTRING);
    }
}
TEST(guid_clsidfromstring_malformed_table) {
    static const parse_case t[] = {
        BAD(L"{"), BAD(L"{}"), BAD(L"}"), BAD(L"{0BE35203}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851"),            /* no closing brace */
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851} "),          /* trailing blank */
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}x"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}{0BE35203-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB85}"),            /* one digit short */
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB8511}"),          /* one digit long */
        BAD(L"{0BE3520-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE352033-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F9-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11C-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB85}"),
        BAD(L"{0BE35203_8F91-11CE-9DE3-00AA004BB851}"),           /* wrong separators */
        BAD(L"{0BE35203-8F91_11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE_9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3_00AA004BB851}"),
        BAD(L"{0BE35203 8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE352038F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F9111CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE300AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA-004BB851}"),          /* dash in the node part */
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB85G}"),           /* non-hex digits */
        BAD(L"{G0E35203-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB85 }"),
        BAD(L"{ 0BE35203-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851]"),           /* wrong closing bracket */
        BAD(L"{0xBE35203-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{-0BE3520-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{+0BE3520-8F91-11CE-9DE3-00AA004BB851}"),
        BAD(L"{\xFF10" L"BE35203-8F91-11CE-9DE3-00AA004BB851}"),   /* full-width digit is not a digit */
        BAD(L"{0BE35203-8F91-11CE-9DE3-00AA004BB851\xFF11}"),
    };
    FOR_MODS(m) {
        NEED(CFS(m));
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            GUID g;
            HRESULT hr = CFS(m)(t[i].s, &g);
            CHECKF(hr == t[i].hr, "case %zu \"%ls\": got 0x%08lx", i, t[i].s, (unsigned long)hr);
        }
    }
}
TEST(guid_clsidfromstring_no_braces) {
    /* without a leading '{' the string is treated as a ProgID, so a bare GUID fails with CO_E_CLASSSTRING */
    FOR_MODS(m) {
        NEED(CFS(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFS(m)(L"0BE35203-8F91-11CE-9DE3-00AA004BB851}", &g), CMG_CLASSSTRING);
        EXPECT_EQ(CFS(m)(L"0BE35203-8F91-11CE-9DE3-00AA004BB851", &g), CMG_CLASSSTRING);
        EXPECT_EQ(CFS(m)(L"(0BE35203-8F91-11CE-9DE3-00AA004BB851)", &g), CMG_CLASSSTRING);
        EXPECT_EQ(CFS(m)(L" {0BE35203-8F91-11CE-9DE3-00AA004BB851}", &g), CMG_CLASSSTRING);
        EXPECT_EQ(CFS(m)(L"[0BE35203-8F91-11CE-9DE3-00AA004BB851]", &g), CMG_CLASSSTRING);
    }
}
TEST(guid_clsidfromstring_progid_path) {
    /* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
    FOR_MODS(m) {
        NEED(CFS(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFS(m)(L"StdFont", &g), S_OK);          /* a registered ProgID is accepted */
        CHECK(cmg_guid_eq(&g, &G_StdFont));
        memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFS(m)(L"StdPicture", &g), S_OK);
        CHECK(cmg_guid_eq(&g, &G_StdPicture));
        memset(&g, 0xCC, sizeof g);
        EXPECT_EQ(CFS(m)(L"olet.NoSuchProgId.Surely", &g), CMG_CLASSSTRING);
        t_note("%s failed ProgID path leaves out GUID %s", m, cmg_guid_eq(&g, &G_NULL) ? "zeroed" : "untouched/partial");
    }
}
TEST(guid_clsidfromstring_partial_write_note) {
    /* a malformed but well-shaped string may have been partially parsed before it failed */
    FOR_MODS(m) {
        NEED(CFS(m));
        GUID g; memset(&g, 0xCC, sizeof g);
        HRESULT hr = CFS(m)(L"{0BE35203-8F91-11CE-9DE3-00AA004BB8G1}", &g);
        EXPECT_EQ(hr, CMG_CLASSSTRING);
        t_note("%s partial write: Data1=%08lx Data2=%04x", m, (unsigned long)g.Data1, g.Data2);
    }
}
TEST(guid_clsidfromstring_hex_digit_positions) {
    /* exactly [0-9A-Fa-f] are accepted in each digit slot */
    static const int slots[] = { 1, 8, 10, 13, 15, 18, 20, 23, 25, 36 };
    FOR_MODS(m) {
        NEED(CFS(m));
        for (size_t si = 0; si < sizeof slots / sizeof *slots; si++) {
            for (unsigned c = 1; c < 0x180; c++) {
                WCHAR s[40]; lstrcpyW(s, CMG_NULL_STR);
                s[slots[si]] = (WCHAR)c;
                GUID g; HRESULT hr = CFS(m)(s, &g);
                int isHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                if (isHex != (hr == S_OK)) { CHECKF(0, "slot %d char 0x%x hr 0x%08lx", slots[si], c, (unsigned long)hr); }
                if (!isHex && hr != CMG_CLASSSTRING) CHECKF(0, "slot %d char 0x%x hr 0x%08lx", slots[si], c, (unsigned long)hr);
            }
        }
        CHECK(1);
    }
}
TEST(guid_clsidfromstring_nonascii_hex_lookalikes) {
    static const WCHAR bad[] = { 0xFF10, 0xFF21, 0xFF41, 0x0660, 0x00B2, 0x2460, 0x0391, 0xFFFF, 0x8030 };
    FOR_MODS(m) {
        NEED(CFS(m));
        for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
            WCHAR s[40]; lstrcpyW(s, CMG_NULL_STR); s[1] = bad[i];
            GUID g; EXPECT_EQ(CFS(m)(s, &g), CMG_CLASSSTRING);
        }
    }
}
TEST(guid_clsidfromstring_embedded_terminator) {
    FOR_MODS(m) {
        NEED(CFS(m));
        for (int cut = 1; cut < 38; cut++) {
            WCHAR s[40]; lstrcpyW(s, CMG_STDFONT_STR); s[cut] = 0;
            GUID g; HRESULT hr = CFS(m)(s, &g);
            CHECKF(hr == CMG_CLASSSTRING, "cut %d hr 0x%08lx", cut, (unsigned long)hr);
        }
    }
}
TEST(guid_clsidfromstring_trailing_garbage_table) {
    FOR_MODS(m) {
        NEED(CFS(m));
        static const WCHAR tails[][4] = { L"A", L" ", L"\t", L"\n", L"}", L"{", L"0", L"-" };
        for (size_t i = 0; i < sizeof tails / sizeof *tails; i++) {
            WCHAR s[48]; lstrcpyW(s, CMG_STDFONT_STR); lstrcatW(s, tails[i]);
            GUID g; EXPECT_EQ(CFS(m)(s, &g), CMG_CLASSSTRING);
        }
    }
}
TEST(guid_clsidfromstring_roundtrip_random) {
    FOR_MODS(m) {
        NEED(CFS(m)); NEED(SFG2(m));
        for (int i = 0; i < 3000; i++) {
            GUID g = rand_guid(), o; WCHAR s[40];
            SFG2(m)(&g, s, 40);
            if (CFS(m)(s, &o) != S_OK || !cmg_guid_eq(&g, &o)) { CHECK(0); break; }
            lower(s);
            if (CFS(m)(s, &o) != S_OK || !cmg_guid_eq(&g, &o)) { CHECK(0); break; }
        }
        CHECK(1);
    }
}
TEST(guid_clsidfromstring_byte_sweep) {
    FOR_MODS(m) {
        NEED(CFS(m)); NEED(SFG2(m));
        for (int idx = 0; idx < 16; idx++) for (int v = 0; v < 256; v++) {
            GUID g, o; memset(&g, 0x5A, sizeof g); ((unsigned char *)&g)[idx] = (unsigned char)v;
            WCHAR s[40]; SFG2(m)(&g, s, 40);
            if (CFS(m)(s, &o) != S_OK || !cmg_guid_eq(&g, &o)) { CHECKF(0, "idx %d v %d", idx, v); }
        }
    }
}

/* IIDFromString: length gate (38) before parsing */
TEST(guid_iidfromstring_valid) {
    FOR_MODS(m) {
        NEED(IFS(m));
        for (size_t i = 0; i < NGUIDS; i++) {
            GUID g; memset(&g, 0xAB, sizeof g);
            EXPECT_EQ(IFS(m)(k_guids[i].s, &g), S_OK);
            CHECK(cmg_guid_eq(&g, k_guids[i].g));
            WCHAR s[40]; lstrcpyW(s, k_guids[i].s); lower(s);
            EXPECT_EQ(IFS(m)(s, &g), S_OK);
            CHECK(cmg_guid_eq(&g, k_guids[i].g));
        }
    }
}
TEST(guid_iidfromstring_null_string) {
    FOR_MODS(m) {
        NEED(IFS(m));
        GUID g; memset(&g, 0xAB, sizeof g);
        EXPECT_EQ(IFS(m)(NULL, &g), S_OK);
        CHECK(cmg_guid_eq(&g, &G_NULL));
    }
}
TEST(guid_iidfromstring_null_out) {
    FOR_MODS(m) {
        NEED(IFS(m));
        if (t_is_wine()) { t_skip("wine dereferences the NULL output"); return; }
        EXPECT_EQ(IFS(m)(CMG_STDFONT_STR, NULL), CMG_E_INVALIDARG);
        EXPECT_EQ(IFS(m)(NULL, NULL), CMG_E_INVALIDARG);
    }
}
TEST(guid_iidfromstring_wrong_length_is_invalidarg) {
    static const WCHAR *t[] = {
        L"", L"{", L"{}", L"StdFont", L"x",
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB85}",          /* 37 */
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB8511}",        /* 39 */
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB851} ",
        L" {0BE35203-8F91-11CE-9DE3-00AA004BB851}",
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}}",
        L"0BE35203-8F91-11CE-9DE3-00AA004BB851",           /* 36: no braces */
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}{0BE35203-8F91-11CE-9DE3-00AA004BB851}",
        L"This is a rather long string that is certainly not a GUID at all, no sir",
    };
    FOR_MODS(m) {
        NEED(IFS(m));
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            GUID g; memset(&g, 0xAB, sizeof g);
            HRESULT hr = IFS(m)(t[i], &g);
            CHECKF(hr == CMG_E_INVALIDARG, "case %zu \"%ls\": 0x%08lx", i, t[i], (unsigned long)hr);
            CHECKF(((unsigned char *)&g)[0] == 0xAB && ((unsigned char *)&g)[15] == 0xAB, "output modified for case %zu", i);
        }
    }
}
TEST(guid_iidfromstring_bad_content_is_iidstring) {
    /* exactly 38 characters but not a GUID */
    static const WCHAR *t[] = {
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB85G}",
        L"{G0E35203-8F91-11CE-9DE3-00AA004BB851}",
        L"{0BE35203_8F91-11CE-9DE3-00AA004BB851}",
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB851]",
        L"(0BE35203-8F91-11CE-9DE3-00AA004BB851)",
        L"x0BE35203-8F91-11CE-9DE3-00AA004BB851x",
        L"0BE35203-8F91-11CE-9DE3-00AA004BB8511{",
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB851 ",
        L" 0BE35203-8F91-11CE-9DE3-00AA004BB851}",
        L"{0BE35203-8F91-11CE-9DE3-00AA004B-851}",
        L"{0BE35203-8F91-11CE-9DE300-AA004BB851}",
        L"{0BE3520 -8F91-11CE-9DE3-00AA004BB851}",
        L"{0BE35203-8F91-11CE-9DE3-00AA004BB8\xFF11" L"1}",
        L"                                      ",
        L"{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{}",
    };
    FOR_MODS(m) {
        NEED(IFS(m));
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            GUID g; HRESULT hr = IFS(m)(t[i], &g);
            CHECKF(lstrlenW(t[i]) == 38, "case %zu has length %d", i, lstrlenW(t[i]));
            CHECKF(hr == CMG_IIDSTRING, "case %zu \"%ls\": 0x%08lx", i, t[i], (unsigned long)hr);
        }
    }
}
TEST(guid_iidfromstring_progid_not_accepted) {
    /* adapted from Wine dlls/ole32/tests/compobj.c (LGPL-2.1+) */
    FOR_MODS(m) {
        NEED(IFS(m));
        GUID g; memset(&g, 0xAB, sizeof g);
        EXPECT_EQ(IFS(m)(L"StdFont", &g), CMG_E_INVALIDARG);
        CHECK(g.Data1 == 0xABABABAB);
    }
}
TEST(guid_iidfromstring_hex_digit_positions) {
    static const int slots[] = { 1, 8, 10, 13, 15, 18, 20, 23, 25, 36 };
    FOR_MODS(m) {
        NEED(IFS(m));
        for (size_t si = 0; si < sizeof slots / sizeof *slots; si++) {
            for (unsigned c = 1; c < 0x180; c++) {
                WCHAR s[40]; lstrcpyW(s, CMG_NULL_STR); s[slots[si]] = (WCHAR)c;
                GUID g; HRESULT hr = IFS(m)(s, &g);
                int isHex = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                HRESULT want = isHex ? S_OK : CMG_IIDSTRING;
                if (hr != want) CHECKF(0, "slot %d char 0x%x hr 0x%08lx", slots[si], c, (unsigned long)hr);
            }
        }
        CHECK(1);
    }
}
TEST(guid_iidfromstring_roundtrip_random) {
    FOR_MODS(m) {
        NEED(IFS(m)); NEED(SFIID(m));
        for (int i = 0; i < 3000; i++) {
            GUID g = rand_guid(), o; WCHAR *s = NULL;
            if (SFIID(m)(&g, &s) != S_OK || !s) { CHECK(0); break; }
            HRESULT hr = IFS(m)(s, &o);
            CoTaskMemFree(s);
            if (hr != S_OK || !cmg_guid_eq(&g, &o)) { CHECK(0); break; }
        }
        CHECK(1);
    }
}
TEST(guid_iidfromstring_clsidfromstring_agree) {
    FOR_MODS(m) {
        NEED(IFS(m)); NEED(CFS(m));
        for (int i = 0; i < 500; i++) {
            GUID g = rand_guid(), a, b; WCHAR s[40];
            SFG2(m)(&g, s, 40);
            CHECK(IFS(m)(s, &a) == S_OK && CFS(m)(s, &b) == S_OK && cmg_guid_eq(&a, &b));
        }
    }
}

/* ---- IsEqualGUID export, cross-module formatting ---- */
UDECL(int, IsEqualGUID, (const GUID *, const GUID *));
TEST(guid_isequalguid_export) {
    REQUIRE(UN(IsEqualGUID));
    CHECK(UN(IsEqualGUID)(&G_StdFont, &G_StdFont));
    CHECK(!UN(IsEqualGUID)(&G_StdFont, &G_StdPicture));
    CHECK(UN(IsEqualGUID)(&G_NULL, &G_NULL));
    CHECK(!UN(IsEqualGUID)(&G_NULL, &G_Ones));
    GUID a = G_Sample, b = G_Sample;
    CHECK(UN(IsEqualGUID)(&a, &b));
    for (int i = 0; i < 16; i++) {                     /* every byte participates */
        GUID c = G_Sample; ((unsigned char *)&c)[i] ^= 0x01;
        CHECKF(!UN(IsEqualGUID)(&a, &c), "byte %d ignored", i);
        CHECKF(!UN(IsEqualGUID)(&c, &a), "byte %d ignored (swapped)", i);
    }
}
TEST(guid_isequalguid_return_is_boolean) {
    REQUIRE(UN(IsEqualGUID));
    int r1 = UN(IsEqualGUID)(&G_Sample, &G_Sample), r0 = UN(IsEqualGUID)(&G_Sample, &G_Ones);
    EXPECT_EQ(r1, 1); EXPECT_EQ(r0, 0);
}
TEST(guid_str2_vs_rpc_uuidtostring) {
    REQUIRE(U(UuidToStringW)); REQUIRE(U(RpcStringFreeW)); REQUIRE(SFG2("ole32.dll"));
    for (int i = 0; i < 200; i++) {
        GUID g = rand_guid(); RPC_WSTR rs = NULL; WCHAR s[40], t[40];
        EXPECT_EQ(U(UuidToStringW)(&g, &rs), RPC_S_OK);
        if (!rs) return;
        SFG2("ole32.dll")(&g, s, 40);
        lstrcpyW(t, (WCHAR *)rs);
        U(RpcStringFreeW)(&rs);
        WCHAR inner[40]; lstrcpyW(inner, s + 1); inner[36] = 0; lower(inner);
        CHECK(!lstrcmpW(inner, t));                    /* same digits: UuidToString is lowercase and brace-less */
    }
}
TEST(guid_str2_vs_rpc_uuidfromstring) {
    REQUIRE(U(UuidFromStringW)); REQUIRE(SFG2("ole32.dll"));
    for (int i = 0; i < 100; i++) {
        GUID g = rand_guid(), o; WCHAR s[40];
        SFG2("ole32.dll")(&g, s, 40); s[37] = 0;
        EXPECT_EQ(U(UuidFromStringW)((RPC_WSTR)(s + 1), &o), RPC_S_OK);
        CHECK(cmg_guid_eq(&g, &o));
    }
}
