/* rpcrt4 UUID <-> string conversion and RpcStringFree.
 * Decomp facts used: RPC_UUID::ConvertFromString accepts exactly 8-4-4-4-4-12 hex digits (either case), '-' at
 * fixed offsets, NUL at offset 36, anything else -> RPC_S_INVALID_STRING_UUID (1705). A NULL string yields the nil
 * uuid and success. The output uuid is only written on success. UuidToString emits lowercase. */
#include "rus.h"

static const UUID g_u[] = {
    { 0x00000000, 0x0000, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x01020304, 0x0506, 0x0708, {0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10} },
    { 0xffffffff, 0xffff, 0xffff, {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff} },
    { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60} },
    { 0x00000046, 0x0000, 0x0000, {0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46} },
    { 0xdeadbeef, 0xcafe,  0xf00d, {0xab,0xcd,0xef,0x01,0x23,0x45,0x67,0x89} },
    { 0x12312312, 0x1231, 0x1231, {0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xff} },
    { 0x00000001, 0x0000, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
};
static const char *const g_s[] = {
    "00000000-0000-0000-0000-000000000000",
    "01020304-0506-0708-090a-0b0c0d0e0f10",
    "ffffffff-ffff-ffff-ffff-ffffffffffff",
    "8a885d04-1ceb-11c9-9fe8-08002b104860",
    "00000046-0000-0000-c000-000000000046",
    "deadbeef-cafe-f00d-abcd-ef0123456789",
    "12312312-1231-1231-1234-56789abcdeff",
    "00000001-0000-0000-0000-000000000000",
};

/* ---------------------------------------------------------------- ToString */

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+) */
TEST(rus_uuidtostring_a_table)
{
    NEED(UuidToStringA); NEED(RpcStringFreeA);
    for (int i = 0; i < NSTR(g_u); i++) {
        UUID u = g_u[i]; unsigned char *str = NULL;
        EXPECT_EQ(p_UuidToStringA(&u, &str), RPC_S_OK);
        CHECK(str != NULL);
        if (!str) continue;
        EXPECT_STR((char *)str, g_s[i]);
        EXPECT_EQ(strlen((char *)str), 36);
        EXPECT_EQ(p_RpcStringFreeA(&str), RPC_S_OK);
        CHECK(str == NULL);
    }
}

TEST(rus_uuidtostring_w_table)
{
    NEED(UuidToStringW); NEED(RpcStringFreeW);
    for (int i = 0; i < NSTR(g_u); i++) {
        UUID u = g_u[i]; RPC_WSTR str = NULL;
        EXPECT_EQ(p_UuidToStringW(&u, &str), RPC_S_OK);
        CHECK(str != NULL);
        if (!str) continue;
        CHECK(rus_wide_eq_ascii(str, g_s[i]));
        EXPECT_EQ(lstrlenW(str), 36);
        EXPECT_EQ(p_RpcStringFreeW(&str), RPC_S_OK);
        CHECK(str == NULL);
    }
}

TEST(rus_uuidtostring_lowercase_only)
{
    NEED(UuidToStringA); NEED(UuidToStringW); NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    UUID u = g_u[5]; /* all hex letters present */
    unsigned char *a = NULL; RPC_WSTR w = NULL;
    EXPECT_EQ(p_UuidToStringA(&u, &a), RPC_S_OK);
    EXPECT_EQ(p_UuidToStringW(&u, &w), RPC_S_OK);
    int upper = 0;
    for (int i = 0; a && a[i]; i++) if (a[i] >= 'A' && a[i] <= 'Z') upper++;
    for (int i = 0; w && w[i]; i++) if (w[i] >= 'A' && w[i] <= 'Z') upper++;
    EXPECT_EQ(upper, 0);
    p_RpcStringFreeA(&a); p_RpcStringFreeW(&w);
}

TEST(rus_uuidtostring_format_positions)
{
    NEED(UuidToStringW); NEED(RpcStringFreeW);
    UUID u = g_u[1]; RPC_WSTR w = NULL;
    EXPECT_EQ(p_UuidToStringW(&u, &w), RPC_S_OK);
    if (!w) return;
    /* dashes at 8, 13, 18, 23; no braces; NUL at 36 */
    for (int i = 0; i < 36; i++) {
        int dash = (i == 8 || i == 13 || i == 18 || i == 23);
        CHECKF((w[i] == L'-') == dash, "pos %d char %u", i, (unsigned)w[i]);
    }
    EXPECT_EQ(w[36], 0);
    p_RpcStringFreeW(&w);
}

TEST(rus_uuidtostring_field_order)
{
    NEED(UuidToStringA); NEED(RpcStringFreeA);
    /* Data1 prints big-endian as one integer, Data2/Data3 likewise, Data4 byte by byte */
    UUID u = { 0xa1b2c3d4, 0xe5f6, 0x0718, {0x29,0x3a,0x4b,0x5c,0x6d,0x7e,0x8f,0x90} };
    unsigned char *s = NULL;
    EXPECT_EQ(p_UuidToStringA(&u, &s), RPC_S_OK);
    if (s) EXPECT_STR((char *)s, "a1b2c3d4-e5f6-0718-293a-4b5c6d7e8f90");
    p_RpcStringFreeA(&s);
}

TEST(rus_uuidtostring_a_matches_w_for_random)
{
    NEED(UuidToStringA); NEED(UuidToStringW); NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    unsigned seed = 99;
    for (int i = 0; i < 200; i++) {
        UUID u; unsigned char *b = (unsigned char *)&u;
        for (int k = 0; k < 16; k++) b[k] = (unsigned char)rus_rand(&seed);
        unsigned char *a = NULL; RPC_WSTR w = NULL;
        EXPECT_EQ(p_UuidToStringA(&u, &a), RPC_S_OK);
        EXPECT_EQ(p_UuidToStringW(&u, &w), RPC_S_OK);
        if (a && w) {
            int same = 1;
            for (int k = 0; k <= 36; k++) if ((unsigned char)a[k] != w[k]) same = 0;
            CHECK(same);
        }
        p_RpcStringFreeA(&a); p_RpcStringFreeW(&w);
    }
}

TEST(rus_uuidtostring_each_call_allocates_new)
{
    NEED(UuidToStringA); NEED(RpcStringFreeA);
    UUID u = g_u[3]; unsigned char *a = NULL, *b = NULL;
    EXPECT_EQ(p_UuidToStringA(&u, &a), RPC_S_OK);
    EXPECT_EQ(p_UuidToStringA(&u, &b), RPC_S_OK);
    CHECK(a != b);
    if (a && b) EXPECT_STR((char *)a, (char *)b);
    p_RpcStringFreeA(&a); p_RpcStringFreeA(&b);
}

/* -------------------------------------------------------------- FromString */

TEST(rus_uuidfromstring_a_table)
{
    NEED(UuidFromStringA);
    for (int i = 0; i < NSTR(g_u); i++) {
        UUID u; memset(&u, 0x5a, sizeof u);
        EXPECT_EQ(p_UuidFromStringA((unsigned char *)g_s[i], &u), RPC_S_OK);
        CHECK(rus_uuid_eq(&u, &g_u[i]));
    }
}

TEST(rus_uuidfromstring_w_table)
{
    NEED(UuidFromStringW);
    for (int i = 0; i < NSTR(g_u); i++) {
        WCHAR ws[64]; UUID u; memset(&u, 0x5a, sizeof u);
        rus_widen(ws, 64, g_s[i]);
        EXPECT_EQ(p_UuidFromStringW(ws, &u), RPC_S_OK);
        CHECK(rus_uuid_eq(&u, &g_u[i]));
    }
}

TEST(rus_uuidfromstring_case_insensitive)
{
    NEED(UuidFromStringA); NEED(UuidFromStringW);
    const char *forms[] = {
        "DEADBEEF-CAFE-F00D-ABCD-EF0123456789",
        "deadbeef-cafe-f00d-abcd-ef0123456789",
        "DeadBeef-CaFe-F00d-AbCd-Ef0123456789",
        "deadBEEF-cafe-F00D-abcd-EF0123456789",
    };
    for (int i = 0; i < NSTR(forms); i++) {
        UUID a, b; WCHAR w[64]; rus_widen(w, 64, forms[i]);
        EXPECT_EQ(p_UuidFromStringA((unsigned char *)forms[i], &a), RPC_S_OK);
        EXPECT_EQ(p_UuidFromStringW(w, &b), RPC_S_OK);
        CHECK(rus_uuid_eq(&a, &g_u[5]));
        CHECK(rus_uuid_eq(&b, &g_u[5]));
    }
}

TEST(rus_uuidfromstring_null_string_gives_nil)
{
    NEED(UuidFromStringA); NEED(UuidFromStringW);
    UUID a, b;
    memset(&a, 0xee, sizeof a); memset(&b, 0xee, sizeof b);
    EXPECT_EQ(p_UuidFromStringA(NULL, &a), RPC_S_OK);
    EXPECT_EQ(p_UuidFromStringW(NULL, &b), RPC_S_OK);
    unsigned char zero[16] = {0};
    CHECK(memcmp(&a, zero, 16) == 0);
    CHECK(memcmp(&b, zero, 16) == 0);
}

/* invalid inputs; the destination must be left alone on failure (decomp writes it only after success) */
static const char *const g_bad[] = {
    "",                                          /* empty is not NULL */
    "{8a885d04-1ceb-11c9-9fe8-08002b104860}",    /* braces */
    "{8a885d04-1ceb-11c9-9fe8-08002b104860",
    "8a885d04-1ceb-11c9-9fe8-08002b104860}",
    "(8a885d04-1ceb-11c9-9fe8-08002b104860)",
    "8a885d04-1ceb-11c9-9fe8-08002b10486",       /* 35 chars */
    "8a885d04-1ceb-11c9-9fe8-08002b10486000",    /* 38 chars */
    "8a885d04-1ceb-11c9-9fe8-08002b1048600",     /* 37 chars */
    "8a885d041ceb11c99fe808002b104860",          /* no dashes */
    "8a885d04-1ceb-11c99-fe8-08002b104860",      /* dash shifted */
    "8a885d0-41ceb-11c9-9fe8-08002b104860",
    "8a885d04_1ceb_11c9_9fe8_08002b104860",      /* wrong separator */
    "8a885d04 1ceb 11c9 9fe8 08002b104860",
    " 8a885d04-1ceb-11c9-9fe8-08002b104860",     /* leading space */
    "8a885d04-1ceb-11c9-9fe8-08002b104860 ",     /* trailing space */
    "8a885d04-1ceb-11c9-9fe8-08002b104860\n",
    "8a885d04-1ceb-11c9-9fe8-08002b10486g",      /* non hex at end */
    "g8a885d04-1ceb-11c9-9fe8-08002b10486",
    "0x8a885d04-1ceb-11c9-9fe8-08002b104860",
    "8a885d04-1ceb-11c9-9fe8-08002b10486\x80",
    "-8a885d04-1ceb-11c9-9fe8-08002b104860",
    "8a885d04-1ceb-11c9-9fe8--08002b104860",
    "zzzzzzzz-zzzz-zzzz-zzzz-zzzzzzzzzzzz",
    "--------------------------------------",
    "8A885D04-1CEB-11C9-9FE8-08002B104860-",
    "8a885d04-1ceb-11c9-9fe8-08002b104860-0000",
    "00000000-0000-0000-0000-00000000000",       /* nil minus one digit */
    "00000000000000000000000000000000000000",
    "ncalrpc",
};

TEST(rus_uuidfromstring_invalid_a)
{
    NEED(UuidFromStringA);
    for (int i = 0; i < NSTR(g_bad); i++) {
        UUID u; memset(&u, 0x77, sizeof u);
        RPC_STATUS st = p_UuidFromStringA((unsigned char *)g_bad[i], &u);
        CHECKF(st == RPC_S_INVALID_STRING_UUID, "[%d] \"%s\" -> %lu", i, g_bad[i], (unsigned long)st);
        unsigned char same[16]; memset(same, 0x77, 16);
        CHECKF(memcmp(&u, same, 16) == 0, "[%d] output was modified on failure", i);
    }
}

TEST(rus_uuidfromstring_invalid_w)
{
    NEED(UuidFromStringW);
    for (int i = 0; i < NSTR(g_bad); i++) {
        WCHAR ws[128]; UUID u; memset(&u, 0x77, sizeof u);
        rus_widen(ws, 128, g_bad[i]);
        /* rus_widen zero extends bytes, which is what we want for the 0x80 case: U+0080 is not a hex digit */
        RPC_STATUS st = p_UuidFromStringW(ws, &u);
        CHECKF(st == RPC_S_INVALID_STRING_UUID, "[%d] \"%s\" -> %lu", i, g_bad[i], (unsigned long)st);
        unsigned char same[16]; memset(same, 0x77, 16);
        CHECKF(memcmp(&u, same, 16) == 0, "[%d] output was modified on failure", i);
    }
}

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+): one bad character at every offset */
TEST(rus_uuidfromstring_single_char_corruption_a)
{
    NEED(UuidFromStringA);
    for (int s = 0; s < NSTR(g_s); s++) {
        for (int pos = 0; pos < 36; pos++) {
            char buf[40]; UUID u;
            strcpy(buf, g_s[s]);
            buf[pos] = 'g';
            EXPECT_EQ(p_UuidFromStringA((unsigned char *)buf, &u), RUS_INVALID_STRING_UUID);
            /* a dash where a digit belongs, a digit where a dash belongs */
            strcpy(buf, g_s[s]);
            int dashpos = (pos == 8 || pos == 13 || pos == 18 || pos == 23);
            buf[pos] = dashpos ? '0' : '-';
            EXPECT_EQ(p_UuidFromStringA((unsigned char *)buf, &u), RUS_INVALID_STRING_UUID);
        }
    }
}

TEST(rus_uuidfromstring_single_char_corruption_w)
{
    NEED(UuidFromStringW);
    for (int s = 0; s < NSTR(g_s); s++) {
        for (int pos = 0; pos < 36; pos++) {
            WCHAR buf[40]; UUID u;
            rus_widen(buf, 40, g_s[s]);
            buf[pos] = L'g';
            EXPECT_EQ(p_UuidFromStringW(buf, &u), RUS_INVALID_STRING_UUID);
            rus_widen(buf, 40, g_s[s]);
            int dashpos = (pos == 8 || pos == 13 || pos == 18 || pos == 23);
            buf[pos] = dashpos ? L'0' : L'-';
            EXPECT_EQ(p_UuidFromStringW(buf, &u), RUS_INVALID_STRING_UUID);
        }
    }
}

TEST(rus_uuidfromstring_truncations_and_extensions)
{
    NEED(UuidFromStringA); NEED(UuidFromStringW);
    for (int len = 0; len < 36; len++) {
        char a[48]; WCHAR w[48]; UUID u;
        memcpy(a, g_s[3], len); a[len] = 0;
        rus_widen(w, 48, a);
        EXPECT_EQ(p_UuidFromStringA((unsigned char *)a, &u), RUS_INVALID_STRING_UUID);
        EXPECT_EQ(p_UuidFromStringW(w, &u), RUS_INVALID_STRING_UUID);
    }
    /* one extra trailing character of any kind */
    const char extra[] = "0a-_ {}\\\t";
    for (int i = 0; extra[i]; i++) {
        char a[48]; WCHAR w[48]; UUID u;
        strcpy(a, g_s[3]); a[36] = extra[i]; a[37] = 0;
        rus_widen(w, 48, a);
        EXPECT_EQ(p_UuidFromStringA((unsigned char *)a, &u), RUS_INVALID_STRING_UUID);
        EXPECT_EQ(p_UuidFromStringW(w, &u), RUS_INVALID_STRING_UUID);
    }
}

TEST(rus_uuidfromstring_w_non_ascii_digits_rejected)
{
    NEED(UuidFromStringW);
    /* the digit test is a strict ASCII range check: full width digits, Arabic-Indic digits and
     * Latin letters with the high bits set are not hex digits */
    WCHAR bad[] = { 0xFF10, 0x0660, 0x0130, 0x0161, 0x1030, 0x0230, 0x0141 };
    for (int i = 0; i < NSTR(bad); i++) {
        for (int pos = 0; pos < 36; pos += 5) {
            WCHAR buf[40]; UUID u;
            rus_widen(buf, 40, g_s[1]);
            if (buf[pos] == L'-') continue;
            buf[pos] = bad[i];
            EXPECT_EQ(p_UuidFromStringW(buf, &u), RUS_INVALID_STRING_UUID);
        }
    }
}

TEST(rus_uuidfromstring_a_high_bit_bytes_rejected)
{
    NEED(UuidFromStringA);
    for (int c = 0x80; c <= 0xff; c += 7) {
        char buf[40]; UUID u;
        strcpy(buf, g_s[1]);
        buf[2] = (char)c;
        EXPECT_EQ(p_UuidFromStringA((unsigned char *)buf, &u), RUS_INVALID_STRING_UUID);
    }
}

TEST(rus_uuidfromstring_exact_hex_boundaries)
{
    NEED(UuidFromStringA);
    /* every one of the 256 byte values at a digit position: only 0-9 a-f A-F succeed */
    for (int c = 1; c < 256; c++) {
        char buf[40]; UUID u;
        strcpy(buf, g_s[1]);
        buf[0] = (char)c;
        int hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        RPC_STATUS st = p_UuidFromStringA((unsigned char *)buf, &u);
        /* bytes >= 0x80 pass through the ANSI->Unicode conversion first; none of them can map to an ASCII hex digit */
        CHECKF(hex ? st == RPC_S_OK : st == RPC_S_INVALID_STRING_UUID, "byte 0x%02x -> %lu", c, (unsigned long)st);
    }
}

TEST(rus_uuidfromstring_leading_zero_and_all_digits)
{
    NEED(UuidFromStringA);
    UUID u;
    EXPECT_EQ(p_UuidFromStringA((unsigned char *)"00000001-0002-0003-0405-060708090a0b", &u), RPC_S_OK);
    CHECK(u.Data1 == 1 && u.Data2 == 2 && u.Data3 == 3 && u.Data4[0] == 4 && u.Data4[1] == 5 && u.Data4[7] == 0x0b);
    EXPECT_EQ(p_UuidFromStringA((unsigned char *)"99999999-9999-9999-9999-999999999999", &u), RPC_S_OK);
    CHECK(u.Data1 == 0x99999999u && u.Data4[7] == 0x99);
    EXPECT_EQ(p_UuidFromStringA((unsigned char *)"AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE", &u), RPC_S_OK);
    CHECK(u.Data1 == 0xaaaaaaaau && u.Data2 == 0xbbbb && u.Data3 == 0xcccc && u.Data4[0] == 0xdd && u.Data4[7] == 0xee);
}

/* ----------------------------------------------------------- round trips */

TEST(rus_uuid_string_roundtrip_random_a)
{
    NEED(UuidToStringA); NEED(UuidFromStringA); NEED(RpcStringFreeA);
    unsigned seed = 0xBEEF;
    for (int i = 0; i < 300; i++) {
        UUID u, r; unsigned char *b = (unsigned char *)&u, *s = NULL;
        for (int k = 0; k < 16; k++) b[k] = (unsigned char)rus_rand(&seed);
        EXPECT_EQ(p_UuidToStringA(&u, &s), RPC_S_OK);
        EXPECT_EQ(p_UuidFromStringA(s, &r), RPC_S_OK);
        CHECK(rus_uuid_eq(&u, &r));
        p_RpcStringFreeA(&s);
    }
}

TEST(rus_uuid_string_roundtrip_random_w)
{
    NEED(UuidToStringW); NEED(UuidFromStringW); NEED(RpcStringFreeW);
    unsigned seed = 0xFEED;
    for (int i = 0; i < 300; i++) {
        UUID u, r; unsigned char *b = (unsigned char *)&u; RPC_WSTR s = NULL;
        for (int k = 0; k < 16; k++) b[k] = (unsigned char)rus_rand(&seed);
        EXPECT_EQ(p_UuidToStringW(&u, &s), RPC_S_OK);
        EXPECT_EQ(p_UuidFromStringW(s, &r), RPC_S_OK);
        CHECK(rus_uuid_eq(&u, &r));
        p_RpcStringFreeW(&s);
    }
}

TEST(rus_uuid_string_cross_a_to_w)
{
    NEED(UuidToStringA); NEED(UuidFromStringW); NEED(RpcStringFreeA);
    for (int i = 0; i < NSTR(g_u); i++) {
        UUID u = g_u[i], r; unsigned char *s = NULL; WCHAR w[64];
        EXPECT_EQ(p_UuidToStringA(&u, &s), RPC_S_OK);
        if (!s) continue;
        rus_widen(w, 64, (char *)s);
        EXPECT_EQ(p_UuidFromStringW(w, &r), RPC_S_OK);
        CHECK(rus_uuid_eq(&u, &r));
        p_RpcStringFreeA(&s);
    }
}

TEST(rus_uuid_create_to_string_version_digit)
{
    NEED(UuidCreate); NEED(UuidToStringA); NEED(RpcStringFreeA);
    for (int i = 0; i < 40; i++) {
        UUID u; unsigned char *s = NULL;
        p_UuidCreate(&u);
        EXPECT_EQ(p_UuidToStringA(&u, &s), RPC_S_OK);
        if (!s) continue;
        EXPECT_EQ(s[14], '4');                                   /* version nibble at offset 14 */
        CHECK(s[19] == '8' || s[19] == '9' || s[19] == 'a' || s[19] == 'b'); /* variant nibble at 19 */
        p_RpcStringFreeA(&s);
    }
}

/* -------------------------------------------------------------- StringFree */

TEST(rus_rpcstringfree_null_pointer_value_is_ok)
{
    NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    unsigned char *a = NULL; RPC_WSTR w = NULL;
    EXPECT_EQ(p_RpcStringFreeA(&a), RPC_S_OK);
    EXPECT_EQ(p_RpcStringFreeW(&w), RPC_S_OK);
    CHECK(a == NULL && w == NULL);
}

TEST(rus_rpcstringfree_null_pointer_to_pointer)
{
    NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    /* decomp: String == NULL -> ERROR_INVALID_PARAMETER */
    if (t_is_wine()) { t_skip("wine dereferences the NULL pointer"); return; }
    EXPECT_EQ(p_RpcStringFreeW(NULL), RUS_INVALID_PARAMETER);
    EXPECT_EQ(p_RpcStringFreeA(NULL), RUS_INVALID_PARAMETER);
}

TEST(rus_rpcstringfree_resets_pointer_and_double_free_is_harmless)
{
    NEED(UuidToStringW); NEED(RpcStringFreeW);
    UUID u = g_u[3]; RPC_WSTR s = NULL;
    EXPECT_EQ(p_UuidToStringW(&u, &s), RPC_S_OK);
    CHECK(s != NULL);
    EXPECT_EQ(p_RpcStringFreeW(&s), RPC_S_OK);
    CHECK(s == NULL);
    EXPECT_EQ(p_RpcStringFreeW(&s), RPC_S_OK); /* now frees NULL */
    CHECK(s == NULL);
}

TEST(rus_rpcstringfree_a_and_w_are_interchangeable)
{
    NEED(UuidToStringA); NEED(UuidToStringW); NEED(RpcStringFreeA); NEED(RpcStringFreeW);
    UUID u = g_u[1]; unsigned char *a = NULL; RPC_WSTR w = NULL;
    EXPECT_EQ(p_UuidToStringA(&u, &a), RPC_S_OK);
    EXPECT_EQ(p_UuidToStringW(&u, &w), RPC_S_OK);
    /* both free the same allocator: free each with the other flavour */
    EXPECT_EQ(p_RpcStringFreeW((RPC_WSTR *)&a), RPC_S_OK);
    EXPECT_EQ(p_RpcStringFreeA((unsigned char **)&w), RPC_S_OK);
    CHECK(a == NULL && w == NULL);
}

TEST(rus_rpcstringfree_many_cycles)
{
    NEED(UuidToStringA); NEED(RpcStringFreeA);
    UUID u = g_u[2];
    for (int i = 0; i < 2000; i++) {
        unsigned char *s = NULL;
        if (p_UuidToStringA(&u, &s) != RPC_S_OK) { CHECK(0); break; }
        if (p_RpcStringFreeA(&s) != RPC_S_OK) { CHECK(0); break; }
    }
    CHECK(1);
}

TEST(rus_uuid_nil_roundtrip_via_create_nil)
{
    NEED(UuidCreateNil); NEED(UuidToStringA); NEED(UuidFromStringA); NEED(RpcStringFreeA); NEED(UuidIsNil);
    UUID u, r; unsigned char *s = NULL; RPC_STATUS st;
    p_UuidCreateNil(&u);
    EXPECT_EQ(p_UuidToStringA(&u, &s), RPC_S_OK);
    if (s) EXPECT_STR((char *)s, "00000000-0000-0000-0000-000000000000");
    memset(&r, 1, sizeof r);
    EXPECT_EQ(p_UuidFromStringA(s, &r), RPC_S_OK);
    EXPECT_EQ(p_UuidIsNil(&r, &st), 1);
    p_RpcStringFreeA(&s);
}
