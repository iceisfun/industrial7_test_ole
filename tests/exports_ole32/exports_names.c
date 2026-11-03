/* exports_ole32: name/ordinal sweep of Windows 10 x64 ole32.dll (resolve only, no calls).
 * Table: exp_table.h, generated from tools/ole32_exports.json (554 entries, ordinals 800..1353 contiguous,
 * 546 named + 8 ordinal-only: 800 801 803 804 820 821 822 823).
 */
#include "t.h"
#include "exp_table.h"
/* Wine's builtin ole32 has a different export set/ordinal numbering; the Windows 10 table is the authority,
 * so ordinal- and layout-based checks are skipped there and name checks are reported as notes. */
#define WINE_SKIP() do { if (t_is_wine()) { t_skip("Wine builtin ole32 exports differ from Windows 10"); return; } } while (0)

static FARPROC byname(const char *n) { return t_proc_in("ole32.dll", n); }
static FARPROC byord(unsigned o) { return t_proc_ord("ole32.dll", o); }

TEST(exports_ole32_module_loads) {
    HMODULE m = t_module("ole32.dll");
    CHECK(m != NULL);
    CHECK(t_module("combase.dll") != NULL);
    CHECK(GetModuleHandleA("ole32.dll") == m);
}
TEST(exports_ole32_table_shape) {
    EXPECT_EQ(EXP_N, 554);
    EXPECT_EQ(EXP_ORD_BASE, 800);
    EXPECT_EQ(EXP_ORD_MAX, 1353);
    EXPECT_EQ(EXP_NAMED, 546);
    for (unsigned i = 0; i < EXP_N; i++) EXPECT_EQ(g_exp[i].ord, 800 + i);
}

/* every named export resolves by name to non-NULL */
static void names_chunk(unsigned lo, unsigned hi) {
    if (!t_module("ole32.dll")) { t_skip("ole32"); return; }
    for (unsigned i = lo; i < hi && i < EXP_N; i++) {
        if (!g_exp[i].name) continue;
        SET_ERR(0);
        FARPROC p = byname(g_exp[i].name);
        if (t_is_wine()) { if (!p) t_note("wine lacks %s", g_exp[i].name); continue; }
        CHECKF(p != NULL, "ordinal %u %s not exported by name", g_exp[i].ord, g_exp[i].name);
    }
}
/* by ordinal (MAKEINTRESOURCE) for every entry, named or not */
static void ord_chunk(unsigned lo, unsigned hi) {
    WINE_SKIP();
    if (!t_module("ole32.dll")) { t_skip("ole32"); return; }
    for (unsigned i = lo; i < hi && i < EXP_N; i++) {
        FARPROC p = byord(g_exp[i].ord);
        CHECKF(p != NULL, "ordinal %u (%s) not resolvable by ordinal", g_exp[i].ord, g_exp[i].name ? g_exp[i].name : "no name");
    }
}
/* name and ordinal give the same address (stable name->ordinal mapping) */
static void agree_chunk(unsigned lo, unsigned hi) {
    WINE_SKIP();
    if (!t_module("ole32.dll")) { t_skip("ole32"); return; }
    for (unsigned i = lo; i < hi && i < EXP_N; i++) {
        if (!g_exp[i].name) continue;
        FARPROC a = byname(g_exp[i].name), b = byord(g_exp[i].ord);
        CHECKF(a != NULL && a == b, "%s: name %p vs ordinal %u -> %p", g_exp[i].name, a, g_exp[i].ord, b);
    }
}
#define CH(n) \
    TEST(exports_ole32_names_##n) { names_chunk((n) * 50, (n) * 50 + 50); } \
    TEST(exports_ole32_ordinals_##n) { ord_chunk((n) * 50, (n) * 50 + 50); } \
    TEST(exports_ole32_name_ordinal_agree_##n) { agree_chunk((n) * 50, (n) * 50 + 50); }
CH(0) CH(1) CH(2) CH(3) CH(4) CH(5) CH(6) CH(7) CH(8) CH(9) CH(10) CH(11)

/* distinct ordinals give distinct entry points unless they are legitimately aliases (forwarded to the same target) */
TEST(exports_ole32_ordinal_only_exports) {
    WINE_SKIP();
    static const unsigned o[] = { 800, 801, 803, 804, 820, 821, 822, 823 };
    for (unsigned i = 0; i < 8; i++) {
        FARPROC p = byord(o[i]);
        CHECKF(p != NULL, "ordinal-only export %u", o[i]);
        CHECKF(g_exp[o[i] - 800].name == NULL, "table says %u has no name", o[i]);
    }
}
#define ORDONLY(o) TEST(exports_ole32_ordonly_##o) { WINE_SKIP(); \
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; } \
    FARPROC p = GetProcAddress(m, MAKEINTRESOURCEA(o)); CHECK(p != NULL); \
    HMODULE h = NULL; \
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h) && h == m); }
ORDONLY(800) ORDONLY(801) ORDONLY(803) ORDONLY(804) ORDONLY(820) ORDONLY(821) ORDONLY(822) ORDONLY(823)

TEST(exports_ole32_ordinal_gaps_absent) {
    WINE_SKIP();
    /* 802 and 805..819 are named exports; 799 and 1354 are outside the table */
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    static const unsigned bad[] = { 1, 2, 100, 799, 1354, 1355, 2000, 4096, 65535 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, MAKEINTRESOURCEA(bad[i]));
        CHECKF(p == NULL, "ordinal %u must not resolve", bad[i]);
        DWORD e = GetLastError();
        if (e != ERROR_PROC_NOT_FOUND) t_note("ordinal %u: GetLastError=%lu (127 expected on most builds)", bad[i], e);
    }
}
TEST(exports_ole32_ordinal_zero) {
    WINE_SKIP();
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    SET_ERR(0);
    FARPROC p = GetProcAddress(m, MAKEINTRESOURCEA(0));
    CHECK(p == NULL);
    t_note("GetProcAddress(ole32, #0) last error %lu", GetLastError());
}

/* names that exist in combase.dll but are not exported by ole32.dll */
static void cbonly_chunk(unsigned lo, unsigned hi) {
    WINE_SKIP();
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    for (unsigned i = lo; i < hi && i < CBONLY_N; i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(m, g_cbonly[i]);
        CHECKF(p == NULL, "ole32 must not export %s (combase-only)", g_cbonly[i]);
        if (p) continue;
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
}
#define CBO(n) TEST(exports_ole32_not_exported_##n) { cbonly_chunk((n) * 150, (n) * 150 + 150); }
CBO(0) CBO(1) CBO(2) CBO(3) CBO(4) CBO(5) CBO(6) CBO(7) CBO(8) CBO(9) CBO(10) CBO(11) CBO(12)

/* A/W: ole32 exports no A/W pair at all (OleUI* etc. live in oledlg). Every name ending in A or W
 * is a unique entry whose counterpart is not exported. */
static int has(const char *n) { for (unsigned i = 0; i < EXP_N; i++) if (g_exp[i].name && !strcmp(g_exp[i].name, n)) return 1; return 0; }
TEST(exports_ole32_aw_counterparts_absent) {
    WINE_SKIP();
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    for (unsigned i = 0; i < EXP_N; i++) {
        const char *n = g_exp[i].name; if (!n) continue;
        size_t l = strlen(n); if (l < 3) continue;
        char c = n[l - 1];
        if ((c != 'A' && c != 'W') || (n[l - 2] >= 'A' && n[l - 2] <= 'Z')) continue; /* skip acronyms like ...IA */
        char o[200]; snprintf(o, sizeof o, "%s", n); o[l - 1] = (c == 'A') ? 'W' : 'A';
        if (has(o)) { CHECKF(GetProcAddress(m, n) != NULL && GetProcAddress(m, o) != NULL, "%s/%s", n, o); continue; }
        SET_ERR(0);
        CHECKF(GetProcAddress(m, o) == NULL, "%s: counterpart %s must not be exported", n, o);
    }
}
TEST(exports_ole32_aw_wow64_directory) {
    WINE_SKIP();
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    CHECK(GetProcAddress(m, "CoGetSystemWow64DirectoryW") != NULL);
    SET_ERR(0);
    CHECK(GetProcAddress(m, "CoGetSystemWow64DirectoryA") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
/* case sensitivity and near-miss names */
TEST(exports_ole32_name_case_sensitive) {
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    static const char *const bad[] = { "cocreateinstance", "COCREATEINSTANCE", "Cocreateinstance", "CoCreateInstance ", " CoCreateInstance",
        "CoCreateInstanceX", "CoCreateInstanc", "_CoCreateInstance", "CoCreateInstance@20", "", "ole32.CoCreateInstance" };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        CHECKF(GetProcAddress(m, bad[i]) == NULL, "\"%s\"", bad[i]);
    }
    CHECK(GetProcAddress(m, "CoCreateInstance") != NULL);
}
TEST(exports_ole32_getprocaddress_nonexistent) {
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    SET_ERR(0);
    CHECK(GetProcAddress(m, "NoSuchExport_olet_xyz") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
/* exported names with common historic decorations are not exported (stdcall decoration absent on x64) */
TEST(exports_ole32_no_decorated_names) {
    HMODULE m = t_module("ole32.dll"); if (!m) { t_skip("ole32"); return; }
    for (unsigned i = 0; i < EXP_N; i++) {
        if (!g_exp[i].name) continue;
        char b[256]; snprintf(b, sizeof b, "_%s@0", g_exp[i].name);
        CHECKF(GetProcAddress(m, b) == NULL, "%s", b);
        snprintf(b, sizeof b, "_%s", g_exp[i].name);
        CHECKF(GetProcAddress(m, b) == NULL, "%s", b);
    }
}
