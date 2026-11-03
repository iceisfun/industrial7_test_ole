/* Additional combase.dll export-table properties observable through the loader. Resolve-only. */
#define EXP_WINE_TOLERANT
#include "exp_common.h"

#define NEED() HMODULE m = t_module(EXP_COMBASE); do { if (!m) { t_skip("combase.dll not loadable"); return; } } while (0)

static HMODULE owner_of(const void *addr) {
    HMODULE h = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &h)) return NULL;
    return h;
}

TEST(exports_combase_x_every_export_owned_by_combase) {
    /* the loader attributes the address of every export to combase.dll itself (none is a forwarder) */
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    for (unsigned o = e->Base; o < e->Base + e->NumberOfFunctions; o++) {
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o);
        CHECKF(p != NULL, "ordinal %u", o);
        if (p) CHECKF(owner_of(p) == m, "ordinal %u owner %p", o, (void *)owner_of(p));
    }
}
TEST(exports_combase_x_ole32_forwards_owned_by_combase) {
    HMODULE o = t_module("ole32.dll"), c = t_module(EXP_COMBASE);
    if (!o || !c) { t_skip("module missing"); return; }
    static const char *const n[] = { "CoInitializeEx", "CoUninitialize", "CoCreateInstance", "CoGetClassObject", "CoTaskMemAlloc", "CoTaskMemFree", "CoTaskMemRealloc", "StringFromGUID2", "CLSIDFromString", "CoMarshalInterface", "CoUnmarshalInterface" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        FARPROC p = GetProcAddress(o, n[i]);
        CHECKF(p != NULL, "ole32!%s", n[i]);
        if (p && !t_is_wine()) CHECKF(owner_of(p) == c, "ole32!%s owner %p want combase %p", n[i], (void *)owner_of(p), (void *)c);
    }
}
TEST(exports_combase_x_oleaut32_hwnd_forwards_owned_by_combase) {
    HMODULE a = t_module("oleaut32.dll"), c = t_module(EXP_COMBASE);
    if (!a || !c) { t_skip("module missing"); return; }
    for (unsigned i = 0; i < OA_FWD_NAME_N; i++) {
        FARPROC p = GetProcAddress(a, oa_fwd_name[i].name);
        CHECKF(p != NULL, "oleaut32!%s", oa_fwd_name[i].name);
        if (p && !t_is_wine()) CHECKF(owner_of(p) == c, "oleaut32!%s owner %p", oa_fwd_name[i].name, (void *)owner_of(p));
    }
}
TEST(exports_combase_x_getprocaddress_stable) {
    NEED();
    FARPROC a = GetProcAddress(m, "CoTaskMemAlloc");
    for (int i = 0; i < 1000; i++) CHECK(GetProcAddress(m, "CoTaskMemAlloc") == a);
    CHECK(a != NULL);
}
TEST(exports_combase_x_refcount_balanced_resolve) {
    /* resolving exports does not change the module's load count: one extra LoadLibrary/FreeLibrary pair is a no-op */
    NEED();
    for (int i = 0; i < 50; i++) { HMODULE h = LoadLibraryA("combase.dll"); CHECK(h == m); CHECK(GetProcAddress(h, "CoInitializeEx") != NULL); CHECK(FreeLibrary(h)); }
    CHECK(GetModuleHandleA("combase.dll") == m);
}
TEST(exports_combase_x_stubless_clients_distinct) {
    NEED();
    char n[64]; FARPROC prev[3000]; unsigned cnt = 0;
    for (unsigned k = 3; k < 3000; k++) {
        snprintf(n, sizeof n, "ObjectStublessClient%u", k);
        FARPROC p = GetProcAddress(m, n);
        if (!p) break;
        for (unsigned j = 0; j < cnt; j++) CHECKF(prev[j] != p, "%s aliases an earlier stub", n);
        prev[cnt++] = p;
    }
    CHECK(cnt >= 25);
}
TEST(exports_combase_x_com_server_entry_points_distinct) {
    NEED();
    FARPROC a = GetProcAddress(m, "DllGetClassObject"), b = GetProcAddress(m, "DllGetActivationFactory");
    CHECK(a != NULL && b != NULL);
    CHECK(a != b);
}
TEST(exports_combase_x_name_ordinal_lookup_roundtrip) {
    /* for every named export the loader's by-name and by-ordinal answers agree with the directory */
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    const WORD *no = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    for (DWORD i = 0; i < e->NumberOfNames; i++) {
        const char *n = (const char *)m + nm[i];
        FARPROC a = GetProcAddress(m, n), b = GetProcAddress(m, (LPCSTR)(ULONG_PTR)(no[i] + e->Base));
        CHECKF(a != NULL && a == b, "%s", n);
    }
}
TEST(exports_combase_x_export_dir_inside_image) {
    NEED();
    DWORD rva = 0, sz = 0;
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, &rva, &sz);
    if (!e) { t_skip("no export dir"); return; }
    SIZE_T img = exp_image_size(m);
    CHECK(rva + sz <= img);
    CHECK(e->AddressOfFunctions < img && e->AddressOfNames < img && e->AddressOfNameOrdinals < img && e->Name < img);
    CHECK(e->AddressOfFunctions + 4ull * e->NumberOfFunctions <= img);
    CHECK(e->AddressOfNames + 4ull * e->NumberOfNames <= img);
    CHECK(e->AddressOfNameOrdinals + 2ull * e->NumberOfNames <= img);
}
TEST(exports_combase_x_names_are_printable_ascii) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    for (DWORD i = 0; i < e->NumberOfNames; i++)
        for (const unsigned char *c = (const unsigned char *)m + nm[i]; *c; c++) CHECKF(*c > 0x20 && *c < 0x7f, "name #%lu has byte 0x%02x", (unsigned long)i, *c);
}
TEST(exports_combase_x_getmodulehandleex_pins_refcount) {
    NEED();
    HMODULE h = NULL;
    CHECK(GetModuleHandleExA(0, "combase.dll", &h));
    CHECK(h == m);
    if (h) CHECK(FreeLibrary(h));        /* balances the increment taken by flag 0 */
    h = NULL;
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "combase.dll", &h));
    CHECK(h == m);
}
