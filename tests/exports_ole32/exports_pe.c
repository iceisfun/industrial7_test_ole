/* exports_ole32: parse the in-memory export directory of the loaded ole32.dll and compare it with the
 * expected Windows 10 x64 table (names, ordinals, forwarder strings). Reads image memory only; no calls
 * into the DLL. */
#include "t.h"
#include "exp_table.h"

struct dir { const BYTE *base; const IMAGE_EXPORT_DIRECTORY *ed; DWORD rva, size; };
static int get_dir(struct dir *d) {
    HMODULE m = t_module("ole32.dll");
    if (!m) return 0;
    d->base = (const BYTE *)m;
    const IMAGE_DOS_HEADER *dos = (const void *)d->base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const IMAGE_NT_HEADERS64 *nt = (const void *)(d->base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return 0;
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    d->rva = dd->VirtualAddress; d->size = dd->Size;
    if (!d->rva) return 0;
    d->ed = (const void *)(d->base + d->rva);
    return 1;
}
#define NEED(d) struct dir d; if (t_is_wine()) { t_skip("Wine builtin ole32 has a different export layout"); return; } if (!get_dir(&d)) { t_skip("no export directory"); return; }

TEST(exports_ole32_pe_counts) {
    NEED(d);
    EXPECT_EQ(d.ed->Base, 800);
    EXPECT_EQ(d.ed->NumberOfFunctions, 554);
    EXPECT_EQ(d.ed->NumberOfNames, 546);
    const char *nm = (const char *)(d.base + d.ed->Name);
    CHECKF(!lstrcmpiA(nm, "ole32.dll"), "export dll name '%s'", nm);
}
TEST(exports_ole32_pe_names_sorted) {
    NEED(d);
    const DWORD *nn = (const void *)(d.base + d.ed->AddressOfNames);
    for (DWORD i = 1; i < d.ed->NumberOfNames; i++)
        CHECKF(strcmp((const char *)d.base + nn[i - 1], (const char *)d.base + nn[i]) < 0, "name table order at %lu", i);
}
/* name -> ordinal in the real export directory equals the expected table */
static void pe_name_chunk(unsigned lo, unsigned hi) {
    NEED(d);
    const DWORD *nn = (const void *)(d.base + d.ed->AddressOfNames);
    const WORD *no = (const void *)(d.base + d.ed->AddressOfNameOrdinals);
    for (DWORD i = lo; i < hi && i < d.ed->NumberOfNames; i++) {
        const char *n = (const char *)d.base + nn[i];
        unsigned ord = d.ed->Base + no[i];
        unsigned k;
        for (k = 0; k < EXP_N; k++) if (g_exp[k].name && !strcmp(g_exp[k].name, n)) break;
        CHECKF(k < EXP_N, "unexpected export name %s", n);
        if (k < EXP_N) CHECKF(g_exp[k].ord == ord, "%s: ordinal %u want %u", n, ord, g_exp[k].ord);
    }
}
#define PN(n) TEST(exports_ole32_pe_names_##n) { pe_name_chunk((n) * 140, (n) * 140 + 140); }
PN(0) PN(1) PN(2) PN(3)

/* function table: forwarder RVAs lie inside the export directory and hold the expected string; others do not */
static void pe_fn_chunk(unsigned lo, unsigned hi) {
    NEED(d);
    const DWORD *fn = (const void *)(d.base + d.ed->AddressOfFunctions);
    for (unsigned i = lo; i < hi && i < d.ed->NumberOfFunctions; i++) {
        const struct exp_ent *e = &g_exp[i];
        DWORD r = fn[i];
        int infwd = r >= d.rva && r < d.rva + d.size;
        if (e->fwd) {
            CHECKF(infwd, "ord %u %s expected to be a forwarder", e->ord, e->name ? e->name : "");
            if (infwd) CHECKF(!strcmp((const char *)d.base + r, e->fwd), "ord %u forwarder '%s' want '%s'", e->ord, (const char *)d.base + r, e->fwd);
        } else {
            CHECKF(!infwd && r != 0, "ord %u %s expected to be a code export", e->ord, e->name ? e->name : "");
        }
    }
}
#define PF(n) TEST(exports_ole32_pe_functions_##n) { pe_fn_chunk((n) * 140, (n) * 140 + 140); }
PF(0) PF(1) PF(2) PF(3)
