/* Negative lookups against combase.dll: documented GetProcAddress failure behaviour. */
#include "exp_common.h"

#define NEED() HMODULE m = t_module("combase.dll"); do { if (!m) { t_skip("combase.dll not loadable"); return; } } while (0)

static void miss(HMODULE m, const char *n) {
    SET_ERR(0);
    FARPROC p = GetProcAddress(m, n);
    CHECKF(p == NULL, "\"%s\" unexpectedly resolved", n);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
static void miss_ord(HMODULE m, unsigned o) {
    SET_ERR(0);
    FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o);
    CHECKF(p == NULL, "ordinal %u unexpectedly resolved", o);
    /* ntdll LdrpGetProcedureAddress returns STATUS_ORDINAL_NOT_FOUND (0xC0000138 -> ERROR_INVALID_ORDINAL 182) for a bad
     * ordinal, while the loader maps name misses to STATUS_ENTRYPOINT_NOT_FOUND (-> 127). Accept both for ordinals. */
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_ORDINAL || e == ERROR_PROC_NOT_FOUND, "ordinal %u: GetLastError=%lu", o, e);
    if (e != ERROR_PROC_NOT_FOUND) t_note("ordinal miss last error = %lu", e);
}
TEST(exports_combase_neg_unknown_name) { NEED(); miss(m, "NoSuchExport_xyz"); miss(m, "CoNoSuchFunction"); miss(m, "RoNoSuchFunction"); }
TEST(exports_combase_neg_empty_name) { NEED(); miss(m, ""); }
TEST(exports_combase_neg_prefix_of_export) {
    /* a proper prefix / extension of a real name is not a match */
    NEED();
    miss(m, "CoInitializeE"); miss(m, "CoInitializeEx2"); miss(m, "CoInitializ"); miss(m, "CoTaskMemAllo"); miss(m, "RoInitializ");
    miss(m, "CoInitializeEx "); miss(m, " CoInitializeEx");
}
TEST(exports_combase_neg_case_sensitive) {
    NEED();
    miss(m, "coinitializeex"); miss(m, "COINITIALIZEEX"); miss(m, "cotaskmemalloc"); miss(m, "roinitialize"); miss(m, "WINDOWSCREATESTRING");
}
TEST(exports_combase_neg_case_flip_every_name) {
    /* flipping the case of the first letter of each export never yields another export unless the table says so */
    NEED();
    char b[160];
    for (unsigned i = 0; i < EXP_NTABLE; i++) {
        if (!cb_all[i].name) continue;
        strncpy(b, cb_all[i].name, sizeof b - 1); b[sizeof b - 1] = 0;
        b[0] = (b[0] >= 'A' && b[0] <= 'Z') ? b[0] + 32 : (b[0] >= 'a' && b[0] <= 'z' ? b[0] - 32 : b[0]);
        if (exp_find_name(b)) continue;
        SET_ERR(0);
        CHECKF(GetProcAddress(m, b) == NULL, "case-flipped %s resolved", b);
    }
}
TEST(exports_combase_neg_decorations) {
    NEED();
    miss(m, "CoInitializeExA"); miss(m, "CoInitializeExW"); miss(m, "_CoInitializeEx"); miss(m, "CoInitializeEx@8");
    miss(m, "CoInitializeEx@12"); miss(m, "_CoInitializeEx@8"); miss(m, "__imp_CoInitializeEx"); miss(m, "?CoInitializeEx@@YAJPEAXK@Z");
    miss(m, "CoTaskMemAllocA"); miss(m, "WindowsCreateStringW");
}
TEST(exports_combase_neg_qualified_names) {
    NEED();
    miss(m, "combase.CoInitializeEx"); miss(m, "combase.dll.CoInitializeEx"); miss(m, "combase.dll!CoInitializeEx"); miss(m, "ole32.CoInitializeEx");
}
TEST(exports_combase_neg_ordinal_beyond_last) {
    NEED();
    miss_ord(m, CB_NEXPORTS + 1); miss_ord(m, CB_NEXPORTS + 2); miss_ord(m, 1000); miss_ord(m, 0x1000); miss_ord(m, 0x7FFF); miss_ord(m, 0xFFFF);
}
TEST(exports_combase_neg_ordinal_just_past_end_boundary) {
    NEED();
    if (t_is_wine()) { t_skip("reference ordinal layout is Windows-specific"); return; }
    CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)CB_NEXPORTS) != NULL);
    miss_ord(m, CB_NEXPORTS + 1);
}
TEST(exports_combase_neg_ordinal_zero) {
    /* Ordinal 0 is below Base=1. The exact error is not pinned (loader dependent); the result must be NULL. */
    NEED();
    SET_ERR(0);
    FARPROC p = GetProcAddress(m, (LPCSTR)0);
    CHECK(p == NULL);
    DWORD e = GetLastError();
    /* LdrpGetProcedureAddress: ordinal 0 -> STATUS_INVALID_PARAMETER (87); older paths give 127/182 */
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_PROC_NOT_FOUND || e == ERROR_INVALID_ORDINAL, "last error %lu", e);
    t_note("GetProcAddress(combase, ordinal 0) last error = %lu", e);
}
TEST(exports_combase_neg_null_null) {
    SET_ERR(0);
    FARPROC p = GetProcAddress(NULL, NULL);
    CHECK(p == NULL);
    t_note("GetProcAddress(NULL,NULL) last error = %lu", GetLastError());
}
TEST(exports_combase_neg_null_module_searches_exe) {
    /* NULL module = the test executable, which exports none of combase's functions. */
    SET_ERR(0);
    FARPROC p = GetProcAddress(NULL, "CoInitializeEx");
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(exports_combase_neg_other_module_does_not_export) {
    /* names that exist in combase must not resolve through an unrelated module */
    HMODULE k = t_module("kernel32.dll"); if (!k) { t_skip("no kernel32"); return; }
    miss(k, "CoInitializeEx_notthere");
    static const char *const n[] = { "WindowsCreateString", "RoInitialize", "ObjectStublessClient3", "CoGetApartmentType" };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        FARPROC p = GetProcAddress(k, n[i]);
        if (p) t_note("kernel32 resolves %s (forwarder to api-ms/combase)", n[i]);
    }
}
TEST(exports_combase_neg_reference_names_stable_across_dirs) {
    /* every ordinal in [1,N] resolves; ordinal-only entries have no name in the dir (covered elsewhere), the table has no gap */
    NEED();
    if (t_is_wine()) { t_skip("reference ordinal layout is Windows-specific"); return; }
    for (unsigned o = 1; o <= CB_NEXPORTS; o++) CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)o) != NULL);
}
TEST(exports_combase_neg_long_name) {
    NEED();
    char b[1100]; memset(b, 'A', sizeof b - 1); b[sizeof b - 1] = 0;
    miss(m, b);
}
TEST(exports_combase_neg_embedded_look_alikes) {
    NEED();
    miss(m, "CoCreateInstance2"); miss(m, "CoCreateInstanceExx"); miss(m, "WindowsDeleteStrin"); miss(m, "RoGetActivationFactory2");
    miss(m, "ObjectStublessClient0"); miss(m, "ObjectStublessClient1"); miss(m, "ObjectStublessClient2"); /* N=0..2 are not exported */
}
TEST(exports_combase_neg_stubless_beyond_run) {
    NEED();
    if (t_is_wine()) { t_skip("reference table is Windows-specific"); return; }
    char n[64]; unsigned last = 0;
    for (unsigned k = 3; k < 3000; k++) { snprintf(n, sizeof n, "ObjectStublessClient%u", k); if (!exp_find_name(n)) break; last = k; }
    CHECK(last >= 27);
    snprintf(n, sizeof n, "ObjectStublessClient%u", last + 1);
    miss(m, n);
}
TEST(exports_combase_neg_loadlibrary_missing_sibling) {
    SET_ERR(0);
    HMODULE h = LoadLibraryA("combase_no_such_dll.dll");
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(exports_combase_neg_getmodulehandle_unloaded_name) {
    SET_ERR(0);
    CHECK(GetModuleHandleA("combase_no_such_dll.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
