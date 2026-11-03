#define EXP_WINE_TOLERANT
#include "exp_common.h"

const IMAGE_NT_HEADERS *exp_nt(HMODULE m) {
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)m;
    if (!m || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const char *)m + d->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE ? nt : NULL;
}
const IMAGE_EXPORT_DIRECTORY *exp_dir(HMODULE m, DWORD *rva, DWORD *size) {
    const IMAGE_NT_HEADERS *nt = exp_nt(m);
    if (!nt) return NULL;
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return NULL;
    if (rva) *rva = dd->VirtualAddress;
    if (size) *size = dd->Size;
    return (const IMAGE_EXPORT_DIRECTORY *)((const char *)m + dd->VirtualAddress);
}
SIZE_T exp_image_size(HMODULE m) { const IMAGE_NT_HEADERS *nt = exp_nt(m); return nt ? nt->OptionalHeader.SizeOfImage : 0; }
int exp_in_image(HMODULE m, const void *p) {
    const char *b = (const char *)m, *q = (const char *)p;
    return q >= b && q < b + exp_image_size(m);
}
unsigned exp_dir_ordinal_of(HMODULE m, const char *name) {
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e) return 0;
    const DWORD *names = (const DWORD *)((const char *)m + e->AddressOfNames);
    const WORD *ords = (const WORD *)((const char *)m + e->AddressOfNameOrdinals);
    for (DWORD i = 0; i < e->NumberOfNames; i++)
        if (!strcmp((const char *)m + names[i], name)) return ords[i] + e->Base;
    return 0;
}
DWORD exp_dir_rva_of_ordinal(HMODULE m, unsigned ord) {
    const IMAGE_EXPORT_DIRECTORY *e = exp_dir(m, NULL, NULL);
    if (!e || ord < e->Base || ord - e->Base >= e->NumberOfFunctions) return 0;
    return ((const DWORD *)((const char *)m + e->AddressOfFunctions))[ord - e->Base];
}
const struct cbexp *exp_find_name(const char *name) {
    for (size_t i = 0; i < EXP_NTABLE; i++) if (cb_all[i].name && !strcmp(cb_all[i].name, name)) return &cb_all[i];
    return NULL;
}
const struct cbexp *exp_find_ord(unsigned ord) {
    for (size_t i = 0; i < EXP_NTABLE; i++) if (cb_all[i].ord == ord) return &cb_all[i];
    return NULL;
}

void exp_check_names(const char *const *names, size_t n) {
    HMODULE m = t_module(EXP_COMBASE);
    if (!m) { t_skip("combase.dll not loadable"); return; }
    for (size_t i = 0; i < n; i++) {
        const char *nm = names[i];
        const struct cbexp *e = exp_find_name(nm);
        FARPROC p = GetProcAddress(m, nm);
        CHECKF(p != NULL, "%s does not resolve by name", nm);
        if (!p) continue;
        CHECKF(exp_in_image(m, p), "%s resolves outside combase image (%p)", nm, (void *)p);
        FARPROC q = t_proc_in(EXP_COMBASE, nm);
        CHECKF(q == p, "%s: t_proc_in differs from GetProcAddress", nm);
        if (e) {
            FARPROC po = GetProcAddress(m, (LPCSTR)(ULONG_PTR)e->ord);
            CHECKF(po == p, "%s: by-ordinal %u gives %p, by-name %p", nm, e->ord, (void *)po, (void *)p);
            unsigned dord = exp_dir_ordinal_of(m, nm);
            CHECKF(dord == e->ord, "%s: export dir ordinal %u want %u", nm, dord, e->ord);
            if ((const char *)p != (const char *)m + e->rva) t_note("%s: rva 0x%x differs from reference 0x%x (build difference?)", nm, (unsigned)((const char *)p - (const char *)m), e->rva);
        }
    }
}

void exp_check_ord_range(unsigned lo, unsigned hi) {
    HMODULE m = t_module(EXP_COMBASE);
    if (!m) { t_skip("combase.dll not loadable"); return; }
    for (unsigned o = lo; o <= hi; o++) {
        const struct cbexp *e = exp_find_ord(o);
        if (!e) { CHECKF(0, "ordinal %u missing in reference table", o); continue; }
        FARPROC p = GetProcAddress(m, (LPCSTR)(ULONG_PTR)o);
        CHECKF(p != NULL, "ordinal %u (%s) does not resolve", o, e->name ? e->name : "<noname>");
        if (!p) continue;
        CHECKF(exp_in_image(m, p), "ordinal %u resolves outside image", o);
        DWORD rva = exp_dir_rva_of_ordinal(m, o);
        CHECKF(rva != 0 && (const char *)p == (const char *)m + rva, "ordinal %u: address != base+dir rva", o);
        if (e->name) {
            FARPROC pn = GetProcAddress(m, e->name);
            CHECKF(pn == p, "ordinal %u: name %s resolves to %p, ordinal to %p", o, e->name, (void *)pn, (void *)p);
        } else {
            /* ordinal-only (private) export: must not be reachable under any name in the export dir */
            const IMAGE_EXPORT_DIRECTORY *d = exp_dir(m, NULL, NULL);
            const WORD *ords = (const WORD *)((const char *)m + d->AddressOfNameOrdinals);
            int named = 0;
            for (DWORD i = 0; i < d->NumberOfNames; i++) if ((unsigned)ords[i] + d->Base == o) named = 1;
            CHECKF(!named, "ordinal %u should be ordinal-only", o);
        }
        if ((const char *)p != (const char *)m + e->rva) t_note("ordinal %u: rva 0x%x differs from reference 0x%x", o, (unsigned)((const char *)p - (const char *)m), e->rva);
    }
}

void exp_check_fwd_range(HMODULE m, const struct fwd *tab, size_t lo, size_t hi, const char *dll) {
    HMODULE cb = t_module(EXP_COMBASE);
    if (!m || !cb) { t_skip("module not loadable"); return; }
    DWORD drva = 0, dsize = 0;
    exp_dir(m, &drva, &dsize);
    for (size_t i = lo; i < hi; i++) {
        const struct fwd *f = &tab[i];
        FARPROC pf = GetProcAddress(m, f->name);
        CHECKF(pf != NULL, "%s!%s does not resolve", dll, f->name);
        if (!pf) continue;
        FARPROC pt;
        if (f->target) pt = GetProcAddress(cb, f->target);
        else pt = GetProcAddress(cb, (LPCSTR)(ULONG_PTR)f->tord);
        CHECKF(pt != NULL, "combase target of %s not resolvable", f->name);
        CHECKF(pf == pt, "%s!%s -> %p but combase target -> %p (forwarder must resolve to the target's address)", dll, f->name, (void *)pf, (void *)pt);
        CHECKF(!exp_in_image(m, pf), "%s!%s resolved inside %s itself (not forwarded)", dll, f->name, dll);
        if (!t_is_wine()) {
            unsigned o = exp_dir_ordinal_of(m, f->name);
            DWORD rva = o ? exp_dir_rva_of_ordinal(m, o) : 0;
            int isfwd = rva >= drva && rva < drva + dsize;
            CHECKF(isfwd, "%s!%s export entry is not a forwarder string", dll, f->name);
            if (isfwd && f->target) {
                const char *s = (const char *)m + rva, *dot = strchr(s, '.');
                CHECKF(dot && !strcmp(dot + 1, f->target), "%s!%s forwarder string \"%s\" does not end with .%s", dll, f->name, s, f->target);
            } else if (isfwd) {
                const char *s = (const char *)m + rva, *dot = strchr(s, '.');
                char want[16]; snprintf(want, sizeof want, "#%u", f->tord);
                CHECKF(dot && !strcmp(dot + 1, want), "%s!%s forwarder string \"%s\" want ordinal %s", dll, f->name, s, want);
            }
        }
    }
}

static unsigned g_miss;
void exp_wine_miss(void) { g_miss++; }
unsigned exp_wine_misses(void) { return g_miss; }
TEST(exports_combase_zz_wine_tolerated_summary) {
    if (t_is_wine()) t_note("%u reference-table expectations not met by this Wine (tolerated)", exp_wine_misses());
}
