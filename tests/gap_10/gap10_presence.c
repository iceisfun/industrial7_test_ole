/* gap10_presence: every internal export of the five modules that other areas leave uncovered resolves via
 * GetProcAddress; ole32 forwarders land on the combase code; exports that share one RVA in the Win10
 * export directory (identical-code-folded stubs) resolve to one address. */
#include "gap10.h"
#include "gap10_tables.h"

#define N(a) (sizeof(a) / sizeof((a)[0]))

static void presence(const char *dll, const pe *t, size_t n) {
    if (!t_module(dll)) { t_skip("%s not loadable", dll); return; }
    for (size_t i = 0; i < n; i++) {
        FARPROC p = t_proc_in(dll, t[i].name);
        PRESENT(p != NULL, "%s!%s does not resolve", dll, t[i].name);
        if (p && !t[i].fwd) {
            /* a non-forwarded export must resolve inside the module image */
            MEMORY_BASIC_INFORMATION mbi;
            HMODULE m = t_module(dll);
            if (VirtualQuery((void *)p, &mbi, sizeof mbi))
                PRESENT(mbi.AllocationBase == (void *)m, "%s!%s resolves outside the module (%p)", dll, t[i].name, (void *)p);
        }
    }
}
TEST(gap10_presence_ole32) { presence("ole32.dll", tbl_ole32, N(tbl_ole32)); }
TEST(gap10_presence_combase) { presence("combase.dll", tbl_combase, N(tbl_combase)); }
TEST(gap10_presence_oleaut32) { presence("oleaut32.dll", tbl_oleaut32, N(tbl_oleaut32)); }
TEST(gap10_presence_rpcrt4) { presence("rpcrt4.dll", tbl_rpcrt4, N(tbl_rpcrt4)); }
TEST(gap10_presence_sechost) { presence("sechost.dll", tbl_sechost, N(tbl_sechost)); }

/* The count of exports is part of the contract: nothing in the table may be missing on a genuine Windows 10. */
TEST(gap10_table_sizes) {
    CHECK(N(tbl_ole32) == 128);
    CHECK(N(tbl_combase) == 127);
    CHECK(N(tbl_oleaut32) == 2);
    CHECK(N(tbl_rpcrt4) == 206);
    CHECK(N(tbl_sechost) == 65);
}

/* ole32 forwards these to combase (directly "COMBASE.x" or through an api-ms-win-core-com-private contract
 * hosted by combase): GetProcAddress(ole32) must return the code inside combase. */
TEST(gap10_ole32_forwarders_land_in_combase) {
    HMODULE cb = t_module("combase.dll");
    if (!t_module("ole32.dll") || !cb) { t_skip("modules"); return; }
    int nfwd = 0;
    for (size_t i = 0; i < N(tbl_ole32); i++) {
        const pe *e = &tbl_ole32[i];
        if (!e->fwd) continue;
        nfwd++;
        const char *dot = strchr(e->fwd, '.');
        if (!dot) { CHECKF(0, "%s: malformed forwarder %s", e->name, e->fwd); continue; }
        FARPROC a = t_proc_in("ole32.dll", e->name), b;
        if (dot[1] == '#') b = t_proc_ord("combase.dll", (unsigned)atoi(dot + 2));
        else b = t_proc_in("combase.dll", dot + 1);
        if (!a || !b) { PRESENT(a && b, "%s -> %s: ole32=%p combase=%p", e->name, e->fwd, (void *)a, (void *)b); continue; }
        PRESENT(a == b, "ole32!%s (%s) = %p, combase target = %p", e->name, e->fwd, (void *)a, (void *)b);
    }
    CHECK(nfwd >= 40);
}
TEST(gap10_ole32_forwarders_are_not_local_code) {
    HMODULE o = t_module("ole32.dll");
    if (!o) { t_skip("ole32"); return; }
    for (size_t i = 0; i < N(tbl_ole32); i++) {
        const pe *e = &tbl_ole32[i];
        if (!e->fwd) continue;
        FARPROC a = t_proc_in("ole32.dll", e->name);
        if (!a) continue;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)a, &mbi, sizeof mbi))
            PRESENT(mbi.AllocationBase != (void *)o, "ole32!%s is forwarded (%s) but resolved inside ole32 (%p)", e->name, e->fwd, (void *)a);
    }
}
/* Names that exist in both ole32 and combase unforwarded (same name, different code) are not forwarders. */
TEST(gap10_ole32_non_forwarders_are_local) {
    HMODULE o = t_module("ole32.dll");
    if (!o) { t_skip("ole32"); return; }
    for (size_t i = 0; i < N(tbl_ole32); i++) {
        const pe *e = &tbl_ole32[i];
        if (e->fwd) continue;
        FARPROC a = t_proc_in("ole32.dll", e->name);
        if (!a) continue;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)a, &mbi, sizeof mbi))
            PRESENT(mbi.AllocationBase == (void *)o, "ole32!%s should be local code, got %p", e->name, (void *)a);
    }
}

static void groups(const char *dll, const char *const (*g)[12], size_t n) {
    if (!t_module(dll)) { t_skip("%s", dll); return; }
    for (size_t i = 0; i < n; i++) {
        FARPROC first = t_proc_in(dll, g[i][0]);
        for (int j = 1; g[i][j]; j++) {
            FARPROC p = t_proc_in(dll, g[i][j]);
            if (!first || !p) { PRESENT(first && p, "%s: %s / %s unresolved", dll, g[i][0], g[i][j]); continue; }
            PRESENT(p == first, "%s: %s (%p) and %s (%p) share one RVA in the export directory", dll, g[i][0], (void *)first, g[i][j], (void *)p);
        }
    }
}
TEST(gap10_same_rva_groups_ole32) { groups("ole32.dll", grp_ole32, N(grp_ole32)); }
TEST(gap10_same_rva_groups_combase) { groups("combase.dll", grp_combase, N(grp_combase)); }
TEST(gap10_same_rva_groups_rpcrt4) { groups("rpcrt4.dll", grp_rpcrt4, N(grp_rpcrt4)); }

/* The 9 no-op/stub exports of combase that share RVA 0x13780: ret 0 */
TEST(gap10_combase_noop_ret_group_is_one_address) {
    static const char *const n[] = { "HBRUSH_UserFree", "HBRUSH_UserFree64", "HICON_UserFree", "HICON_UserFree64", "HMONITOR_UserFree",
        "HMONITOR_UserFree64", "HRGN_UserFree64", "InternalFreeObjRef", "InternalSetOleThunkWowPtr" };
    FARPROC first = t_proc_in("combase.dll", n[0]);
    if (!first) { t_skip("missing export"); return; }
    for (size_t i = 1; i < N(n); i++) { FARPROC p = t_proc_in("combase.dll", n[i]); PRESENT(p == first, "%s = %p want %p", n[i], (void *)p, (void *)first); }
}
/* ole32 also exports the user-marshal handle families of combase: same module-level contract as the other areas */
TEST(gap10_ole32_exports_hbrush_family) {
    static const char *const n[] = { "HBRUSH_UserFree", "HBRUSH_UserMarshal", "HBRUSH_UserSize", "HBRUSH_UserUnmarshal" };
    for (size_t i = 0; i < N(n); i++) {
        FARPROC o = t_proc_in("ole32.dll", n[i]), c = t_proc_in("combase.dll", n[i]);
        if (!o || !c) { PRESENT(o && c, "%s", n[i]); continue; }
        PRESENT(o == c, "ole32!%s (%p) is a forwarder to combase (%p)", n[i], (void *)o, (void *)c);
    }
}

/* rpcrt4 exports the ANSI-less names I_RpcBindingInqDynamicEndpoint and I_RpcNsBindingSetEntryName as forwarders to its own W entry. */
TEST(gap10_rpcrt4_self_forwarders) {
    static const char *const pr[][2] = { { "I_RpcBindingInqDynamicEndpoint", "I_RpcBindingInqDynamicEndpointW" },
                                         { "I_RpcNsBindingSetEntryName", "I_RpcNsBindingSetEntryNameW" } };
    for (size_t i = 0; i < 2; i++) {
        FARPROC a = t_proc_in("rpcrt4.dll", pr[i][0]), b = t_proc_in("rpcrt4.dll", pr[i][1]);
        if (!a || !b) { PRESENT(a && b, "%s/%s", pr[i][0], pr[i][1]); continue; }
        PRESENT(a == b, "%s (%p) should forward to %s (%p)", pr[i][0], (void *)a, pr[i][1], (void *)b);
    }
}
