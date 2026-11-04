/* combase.dll export directory structure and table-wide invariants. Resolve-only: no export is ever called. */
#define EXP_WINE_TOLERANT
#include "exp_common.h"

#define NAMED_COUNT 457
/* Build-specific expectations (one reference Win10 build): a mismatch is reported, not failed. */
#define NOTE_EQ(a, b) do { unsigned long long a_ = (unsigned long long)(a), b_ = (unsigned long long)(b); if (a_ != b_) t_note("build difference: " #a " = %llu, reference " #b " = %llu", a_, b_); } while (0)
#define NORD_ONLY (CB_NEXPORTS - NAMED_COUNT)

static HMODULE cb(void) { return t_module(EXP_COMBASE); }
#define NEED() HMODULE m = cb(); do { if (!m) { t_skip("combase.dll not loadable"); return; } } while (0)

TEST(exports_combase_module_loads) {
    NEED();
    CHECK(m != NULL);
    CHECK(((ULONG_PTR)m & 0xFFFF) == 0);
    EXPECT_EQ(GetModuleHandleA("combase.dll"), m);
}
TEST(exports_combase_module_handle_case_insensitive) {
    NEED();
    EXPECT_EQ(GetModuleHandleA("COMBASE.DLL"), m);
    EXPECT_EQ(GetModuleHandleA("CombaSE"), m);   /* extension optional */
    EXPECT_EQ(GetModuleHandleA("combase"), m);
}
TEST(exports_combase_load_twice_same_base) {
    NEED();
    HMODULE m2 = LoadLibraryA("combase.dll");
    EXPECT_EQ(m2, m);
    if (m2) FreeLibrary(m2);
    EXPECT_EQ(GetModuleHandleA("combase.dll"), m);   /* still loaded after balanced FreeLibrary */
}
TEST(exports_combase_module_path) {
    NEED();
    char p[MAX_PATH]; DWORD n = GetModuleFileNameA(m, p, sizeof p);
    CHECK(n > 0 && n < sizeof p);
    size_t l = strlen(p);
    CHECK(l > 12 && !_stricmp(p + l - 12, "\\combase.dll"));
    if (!strstr(p, "\\System32\\") && !strstr(p, "\\system32\\") && !strstr(p, "\\SYSTEM32\\")) t_note("combase.dll not under system32: %s", p);
}
TEST(exports_combase_ole32_pulls_in_combase) {
    HMODULE o = t_module("ole32.dll");
    if (!o) { t_skip("ole32 not loadable"); return; }
    /* ole32 forwards most of its COM surface to combase, so combase is loaded as a dependency. */
    CHECK(GetModuleHandleA("combase.dll") != NULL);
}
TEST(exports_combase_pe_headers) {
    NEED();
    const IMAGE_NT_HEADERS *nt = exp_nt(m);
    CHECK(nt != NULL);
    if (!nt) return;
    EXPECT_EQ(nt->FileHeader.Machine, IMAGE_FILE_MACHINE_AMD64);
    EXPECT_EQ(nt->OptionalHeader.Magic, IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_DLL);
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
    CHECK(nt->OptionalHeader.SizeOfImage > 0x100000);
    /* Win10 22H2: combase.dll is a console-subsystem image (3), not GUI (2) */
    EXPECT_EQ(nt->OptionalHeader.Subsystem, IMAGE_SUBSYSTEM_WINDOWS_CUI);
}
TEST(exports_combase_export_dir_present) {
    NEED();
    DWORD rva = 0, sz = 0;
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, &rva, &sz);
    CHECK(e != NULL);
    CHECK(rva != 0 && sz >= sizeof(IMAGE_EXPORT_DIRECTORY));
}
TEST(exports_combase_export_dir_name) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const char *n = (const char *)m + e->Name;
    CHECKF(!_stricmp(n, "combase.dll"), "export dir name \"%s\"", n);
}
TEST(exports_combase_export_dir_base_is_1) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    EXPECT_EQ(e->Base, 1);
}
TEST(exports_combase_export_dir_counts) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    NOTE_EQ(e->NumberOfFunctions, CB_NEXPORTS);
    NOTE_EQ(e->NumberOfNames, NAMED_COUNT);
    CHECK(e->NumberOfFunctions >= e->NumberOfNames);
    CHECK(e->NumberOfFunctions >= 400);
    NOTE_EQ(e->NumberOfFunctions - e->NumberOfNames, NORD_ONLY);
}
TEST(exports_combase_table_self_consistent) {
    /* The embedded reference table covers ordinals 1..N with no holes. */
    for (unsigned i = 0; i < EXP_NTABLE; i++) EXPECT_EQ(cb_all[i].ord, i + 1);
    unsigned named = 0;
    for (unsigned i = 0; i < EXP_NTABLE; i++) named += cb_all[i].name != NULL;
    EXPECT_EQ(named, NAMED_COUNT);
}
TEST(exports_combase_name_table_sorted) {
    /* The PE loader binary-searches AddressOfNames: it must be strictly sorted by ordinal byte value. */
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    for (DWORD i = 1; i < e->NumberOfNames; i++)
        CHECKF(strcmp((const char *)m + nm[i - 1], (const char *)m + nm[i]) < 0, "names out of order at %lu: %s / %s", (unsigned long)i, (const char *)m + nm[i - 1], (const char *)m + nm[i]);
}
TEST(exports_combase_name_ordinal_indices_in_range) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const WORD *o = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    for (DWORD i = 0; i < e->NumberOfNames; i++) CHECK(o[i] < e->NumberOfFunctions);
}
TEST(exports_combase_names_in_dir_match_reference) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    const WORD *o = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    for (DWORD i = 0; i < e->NumberOfNames; i++) {
        const char *n = (const char *)m + nm[i];
        const struct cbexp *x = exp_find_name(n);
        if (!x) { t_note("export name %s not in reference table (newer/older build?)", n); continue; }
        EXPECT_EQ(x->ord, o[i] + e->Base);
    }
}
TEST(exports_combase_no_duplicate_names) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    for (DWORD i = 1; i < e->NumberOfNames; i++) CHECK(strcmp((const char *)m + nm[i - 1], (const char *)m + nm[i]) != 0);
}
TEST(exports_combase_no_name_listed_twice_per_ordinal) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const WORD *o = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    static unsigned char seen[4096];
    memset(seen, 0, sizeof seen);
    for (DWORD i = 0; i < e->NumberOfNames; i++) { CHECK(o[i] < sizeof seen); CHECK(!seen[o[i]]); seen[o[i]] = 1; }
}
TEST(exports_combase_no_forwarders) {
    /* combase.dll is the implementation: no export entry may point into the export directory (forwarder). */
    NEED();
    DWORD drva = 0, dsz = 0;
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, &drva, &dsz);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *f = (const DWORD *)((const char *)m + e->AddressOfFunctions);
    unsigned fw = 0;
    for (DWORD i = 0; i < e->NumberOfFunctions; i++) if (f[i] >= drva && f[i] < drva + dsz) fw++;
    EXPECT_EQ(fw, 0);
}
TEST(exports_combase_no_empty_function_slots) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    const DWORD *f = (const DWORD *)((const char *)m + e->AddressOfFunctions);
    for (DWORD i = 0; i < e->NumberOfFunctions; i++) CHECKF(f[i] != 0, "ordinal %lu has no function", (unsigned long)(i + e->Base));
}
TEST(exports_combase_all_targets_in_executable_section) {
    NEED();
    const IMAGE_NT_HEADERS *nt = exp_nt(m);
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!nt || !e) { t_skip("no headers"); return; }
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    const DWORD *f = (const DWORD *)((const char *)m + e->AddressOfFunctions);
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    const WORD *no = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    for (DWORD i = 0; i < e->NumberOfFunctions; i++) {
        int ok = 0, anysec = 0, named = 0;
        for (DWORD q = 0; q < e->NumberOfNames; q++) if (no[q] == i) named = 1;
        (void)nm;
        for (WORD k = 0; k < nt->FileHeader.NumberOfSections; k++)
            if (f[i] >= s[k].VirtualAddress && f[i] < s[k].VirtualAddress + s[k].Misc.VirtualSize) {
                anysec = 1;
                if (s[k].Characteristics & IMAGE_SCN_MEM_EXECUTE) ok = 1;
            }
        /* Win10: a nameless ordinal (183) is a data export living in a non-executable section; every named export is code */
        if (!ok && !named && anysec) { t_note("ordinal %lu is a data export (rva 0x%x)", (unsigned long)(i + e->Base), (unsigned)f[i]); continue; }
        CHECKF(ok, "ordinal %lu rva 0x%x not in an executable section", (unsigned long)(i + e->Base), (unsigned)f[i]);
    }
}
TEST(exports_combase_getprocaddress_equals_dir_for_every_ordinal) {
    NEED();
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    for (unsigned o = e->Base; o < e->Base + e->NumberOfFunctions; o++) {
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o);
        CHECKF(p == (FARPROC)((const char *)m + exp_dir_rva_of_ordinal(m, o)), "ordinal %u", o);
    }
}
TEST(exports_combase_alias_groups_share_address) {
    /* Entries with the same RVA in the reference table (identical code folding / shared stubs) must resolve to one address, and distinct RVAs to distinct addresses. */
    NEED();
    for (unsigned i = 0; i < EXP_NTABLE; i++)
        for (unsigned j = i + 1; j < EXP_NTABLE; j++) {
            FARPROC a = GetProcAddress(m, (LPCSTR)(ULONG_PTR)cb_all[i].ord), b = GetProcAddress(m, (LPCSTR)(ULONG_PTR)cb_all[j].ord);
            int same_ref = cb_all[i].rva == cb_all[j].rva;
            if (same_ref != (a == b)) t_note("ordinals %u/%u: alias relation differs from reference (ref same=%d)", cb_all[i].ord, cb_all[j].ord, same_ref);
        }
}
TEST(exports_combase_alias_group_of_17_ordinals) {
    /* Reference build: one stub (rva 0x13700) is exported under 17 different ordinals. */
    NEED();
    unsigned rva = 0, cnt = 0;
    for (unsigned i = 0; i < EXP_NTABLE; i++) { unsigned c = 0; for (unsigned j = 0; j < EXP_NTABLE; j++) c += cb_all[j].rva == cb_all[i].rva; if (c > cnt) { cnt = c; rva = cb_all[i].rva; } }
    NOTE_EQ(cnt, 17);
    FARPROC first = NULL;
    for (unsigned i = 0; i < EXP_NTABLE; i++) if (cb_all[i].rva == rva) {
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)cb_all[i].ord);
        if (!first) first = p; else EXPECT_EQ(p, first);
    }
}
TEST(exports_combase_ordinal_only_count) {
    NEED();
    unsigned c = 0;
    for (unsigned i = 0; i < EXP_NTABLE; i++) c += cb_all[i].name == NULL;
    EXPECT_EQ(c, NORD_ONLY);
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (e) NOTE_EQ(e->NumberOfFunctions - e->NumberOfNames, c);
}
TEST(exports_combase_ordinal_only_not_resolvable_by_decorated_names) {
    NEED();
    char buf[64];
    for (unsigned i = 0; i < EXP_NTABLE; i++) {
        if (cb_all[i].name) continue;
        snprintf(buf, sizeof buf, "#%u", cb_all[i].ord);
        SET_ERR(0);
        CHECKF(GetProcAddress(m, buf) == NULL, "\"%s\" must not resolve as a name (only the loader's MAKEINTRESOURCE form works)", buf);
    }
}
TEST(exports_combase_stubless_client_run) {
    /* ObjectStublessClientN occupy consecutive ordinals starting at 2 (N=3 -> 2) and are spaced 0x10 apart. */
    NEED();
    char n[64]; unsigned prev = 0, cnt = 0;
    for (unsigned k = 3; k < 3000; k++) {
        snprintf(n, sizeof n, "ObjectStublessClient%u", k);
        const struct cbexp *e = exp_find_name(n);
        if (!e) break;
        cnt++;
        EXPECT_EQ(e->ord, k - 1);
        FARPROC p = GetProcAddress(m, n);
        CHECKF(p != NULL, "%s", n);
        if (prev) EXPECT_EQ((unsigned)((const char *)p - (const char *)m) - prev, 0x10);
        if (p) prev = (unsigned)((const char *)p - (const char *)m);
    }
    CHECKF(cnt >= 25, "only %u ObjectStublessClientN exports", cnt);
    t_note("ObjectStublessClient3..%u present", 2 + cnt);
}
TEST(exports_combase_stubless_client_ordinals_1_and_2_layout) {
    /* ordinal 1 is a nameless export; ordinal 2 is ObjectStublessClient3. */
    NEED();
    EXPECT_EQ(exp_dir_ordinal_of(m, "ObjectStublessClient3"), 2);
    CHECK(GetProcAddress(m, (LPCSTR)1) != NULL);
    CHECK(exp_find_ord(1)->name == NULL);
}

/* ---- per-family table checks: reference count == count in the real export directory ---- */
static void family(const char *prefix, unsigned want_min) {
    HMODULE m = cb();
    if (!m) { t_skip("combase.dll not loadable"); return; }
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) { t_skip("no export dir"); return; }
    size_t pl = strlen(prefix);
    unsigned refc = 0, dirc = 0;
    for (unsigned i = 0; i < EXP_NTABLE; i++) if (cb_all[i].name && !strncmp(cb_all[i].name, prefix, pl)) {
        refc++;
        CHECKF(GetProcAddress(m, cb_all[i].name) != NULL, "%s", cb_all[i].name);
    }
    const DWORD *nm = (const DWORD *)((const char *)m + e->AddressOfNames);
    for (DWORD i = 0; i < e->NumberOfNames; i++) dirc += !strncmp((const char *)m + nm[i], prefix, pl);
    NOTE_EQ(refc, dirc);
    CHECKF(dirc >= want_min, "family %s: %u exports in the export directory", prefix, dirc);
}
TEST(exports_combase_family_Co)        { family("Co", 100); }
TEST(exports_combase_family_CoGet)     { family("CoGet", 20); }
TEST(exports_combase_family_CoRegister){ family("CoRegister", 8); }
TEST(exports_combase_family_CoRevoke)  { family("CoRevoke", 5); }
TEST(exports_combase_family_CoCreate)  { family("CoCreate", 5); }
TEST(exports_combase_family_CoMarshal) { family("CoMarshal", 3); }
TEST(exports_combase_family_CoVrf)     { family("CoVrf", 3); }
TEST(exports_combase_family_CoTaskMem) { family("CoTaskMem", 3); }
TEST(exports_combase_family_Ro)        { family("Ro", 30); }
TEST(exports_combase_family_RoGet)     { family("RoGet", 6); }
TEST(exports_combase_family_RoReport)  { family("RoReport", 3); }
TEST(exports_combase_family_Windows)   { family("Windows", 20); }
TEST(exports_combase_family_WindowsCreate) { family("WindowsCreate", 2); }
TEST(exports_combase_family_Ndr)       { family("Ndr", 30); }
TEST(exports_combase_family_Internal)  { family("Internal", 50); }
TEST(exports_combase_family_Object)    { family("Object", 25); }
TEST(exports_combase_family_Wdtp)      { family("Wdtp", 8); }
TEST(exports_combase_family_C_stub)    { family("CStd", 8); }
TEST(exports_combase_family_CLIPFORMAT){ family("CLIPFORMAT_", 8); }
TEST(exports_combase_family_HSTRING_user) { family("HSTRING_User", 8); }
TEST(exports_combase_family_HWND_user) { family("HWND_User", 8); }
TEST(exports_combase_family_HGLOBAL_user) { family("HGLOBAL_User", 8); }
TEST(exports_combase_family_HBITMAP_user) { family("HBITMAP_User", 8); }
TEST(exports_combase_family_HDC_user)  { family("HDC_User", 8); }
TEST(exports_combase_family_HICON_user){ family("HICON_User", 8); }
TEST(exports_combase_family_HMENU_user){ family("HMENU_User", 8); }
TEST(exports_combase_family_HPALETTE_user){ family("HPALETTE_User", 8); }
TEST(exports_combase_family_HRGN_user) { family("HRGN_User", 8); }
TEST(exports_combase_family_HBRUSH_user){ family("HBRUSH_User", 8); }
TEST(exports_combase_family_HACCEL_user){ family("HACCEL_User", 8); }
TEST(exports_combase_family_HMONITOR_user){ family("HMONITOR_User", 8); }

/* The wire-marshal families come in Free/Marshal/Size/Unmarshal x {32,64} sets of 8 per handle type. */
static void usersuffixes(const char *type) {
    HMODULE m = cb();
    if (!m) { t_skip("combase.dll not loadable"); return; }
    static const char *sfx[] = { "Free", "Free64", "Marshal", "Marshal64", "Size", "Size64", "Unmarshal", "Unmarshal64" };
    char n[96];
    for (int i = 0; i < 8; i++) {
        snprintf(n, sizeof n, "%s_User%s", type, sfx[i]);
        CHECKF(GetProcAddress(m, n) != NULL, "%s", n);
    }
}
TEST(exports_combase_usermarshal_HSTRING)  { usersuffixes("HSTRING"); }
TEST(exports_combase_usermarshal_HWND)     { usersuffixes("HWND"); }
TEST(exports_combase_usermarshal_HGLOBAL)  { usersuffixes("HGLOBAL"); }
TEST(exports_combase_usermarshal_HBITMAP)  { usersuffixes("HBITMAP"); }
TEST(exports_combase_usermarshal_HDC)      { usersuffixes("HDC"); }
TEST(exports_combase_usermarshal_HICON)    { usersuffixes("HICON"); }
TEST(exports_combase_usermarshal_HMENU)    { usersuffixes("HMENU"); }
TEST(exports_combase_usermarshal_HPALETTE) { usersuffixes("HPALETTE"); }
TEST(exports_combase_usermarshal_HRGN)     { usersuffixes("HRGN"); }
TEST(exports_combase_usermarshal_HBRUSH)   { usersuffixes("HBRUSH"); }
TEST(exports_combase_usermarshal_HACCEL)   { usersuffixes("HACCEL"); }
TEST(exports_combase_usermarshal_HMONITOR) { usersuffixes("HMONITOR"); }
TEST(exports_combase_usermarshal_CLIPFORMAT) { usersuffixes("CLIPFORMAT"); }
