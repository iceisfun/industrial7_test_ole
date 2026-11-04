/* Shared helpers for the export-table sweeps (resolve-only; the DLLs are never linked). */
#ifndef EXP_H
#define EXP_H
#include "t.h"
#include <oleauto.h>

typedef struct { const char *name; unsigned ord; unsigned rva; const char *fwd; } exp_ent;

static inline const IMAGE_NT_HEADERS64 *exp_nt(HMODULE m) {
    const IMAGE_DOS_HEADER *d = (const IMAGE_DOS_HEADER *)m;
    return (const IMAGE_NT_HEADERS64 *)((const char *)m + d->e_lfanew);
}
static inline int exp_in_image(HMODULE m, FARPROC p) {
    const char *b = (const char *)m, *q = (const char *)p;
    return q >= b && q < b + exp_nt(m)->OptionalHeader.SizeOfImage;
}
static inline const IMAGE_EXPORT_DIRECTORY *exp_dir(HMODULE m, DWORD *size) {
    const IMAGE_DATA_DIRECTORY *dd = &exp_nt(m)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (size) *size = dd->Size;
    return dd->VirtualAddress ? (const IMAGE_EXPORT_DIRECTORY *)((const char *)m + dd->VirtualAddress) : NULL;
}
/* Resolve a forwarder string "DLL.Name" through the target module. */
static inline FARPROC exp_fwd_target(const char *fwd) {
    char dll[64]; const char *dot = strchr(fwd, '.');
    if (!dot || (size_t)(dot - fwd) > 40) return NULL;
    memcpy(dll, fwd, dot - fwd); dll[dot - fwd] = 0; strcat(dll, ".dll");
    return t_proc_in(dll, dot + 1);
}
/* Name + ordinal resolution for a slice of an export table. */
static inline void exp_check_range(const char *dll, const exp_ent *t, int from, int to) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    for (int i = from; i < to; i++) {
        const exp_ent *e = &t[i];
        FARPROC byord = t_proc_ord(dll, e->ord);
        CHECKF(byord != NULL, "%s ordinal %u does not resolve", dll, e->ord);
        if (!e->name) { 
            if (byord && !e->fwd) CHECKF(exp_in_image(m, byord), "ordinal-only %u outside image", e->ord);
            continue;
        }
        FARPROC byname = t_proc_in(dll, e->name);
        CHECKF(byname != NULL, "%s!%s does not resolve", dll, e->name);
        CHECKF(byname == byord, "%s!%s (ord %u): name and ordinal resolve differently", dll, e->name, e->ord);
        if (!byname) continue;
        if (e->fwd) {
            FARPROC tgt = exp_fwd_target(e->fwd);
            CHECKF(tgt != NULL, "forward target %s unresolved", e->fwd);
            CHECKF(byname == tgt, "%s!%s should equal forward target %s", dll, e->name, e->fwd);
            CHECKF(!exp_in_image(m, byname) || strncmp(e->fwd, "RPCRT4.", 7) == 0 || strncmp(e->fwd, "sechost.", 8) == 0, "%s forwarder resolved inside own image", e->name);
        } else {
            CHECKF(exp_in_image(m, byname), "%s!%s outside image", dll, e->name);
            if ((unsigned)((const char *)byname - (const char *)m) != e->rva) t_note("%s!%s rva 0x%x differs from reference build rva 0x%x (build dependent)", dll, e->name, (unsigned)((const char *)byname - (const char *)m), e->rva);
        }
    }
}
/* Exports sharing an RVA in the reference build (aliases) normally resolve identically; build dependent, so notes only. */
static inline void exp_check_alias_groups(const char *dll, const exp_ent *t, int n) {
    int groups = 0;
    for (int i = 0; i < n; i++) {
        if (!t[i].name || t[i].fwd) continue;
        FARPROC a = t_proc_in(dll, t[i].name);
        CHECKF(a != NULL, "%s!%s does not resolve", dll, t[i].name);
        for (int j = i + 1; j < n; j++) {
            if (!t[j].name || t[j].fwd || t[i].rva != t[j].rva) continue;
            groups++;
            FARPROC b = t_proc_in(dll, t[j].name);
            CHECKF(a == b || a == NULL || b == NULL, "%s: %s and %s are aliases in the reference build and must resolve identically", dll, t[i].name, t[j].name);
        }
    }
    t_note("%s: %d alias pairs in reference build", dll, groups);
}
/* Export directory consistency vs the table. */
static inline void exp_check_dir(const char *dll, const exp_ent *t, int n, unsigned base, unsigned maxord, int named) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    DWORD sz; const IMAGE_EXPORT_DIRECTORY *d = exp_dir(m, &sz);
    CHECK(d != NULL);
    if (!d) return;
    if (d->Base != base) t_note("%s: export Base %lu, reference %u", dll, (unsigned long)d->Base, base);
    if (d->NumberOfFunctions != maxord - base + 1) t_note("%s: NumberOfFunctions %lu, reference %u", dll, (unsigned long)d->NumberOfFunctions, maxord - base + 1);
    if (d->NumberOfNames != (DWORD)named) t_note("%s: NumberOfNames %lu, reference %d", dll, (unsigned long)d->NumberOfNames, named);
    CHECK(d->NumberOfFunctions >= d->NumberOfNames);
    CHECK(d->NumberOfNames <= d->NumberOfFunctions);
    /* names sorted ascending (binary-search requirement of the loader) */
    const DWORD *nm = (const DWORD *)((const char *)m + d->AddressOfNames);
    for (DWORD i = 1; i < d->NumberOfNames; i++)
        CHECKF(strcmp((const char *)m + nm[i - 1], (const char *)m + nm[i]) < 0, "export names not strictly sorted at %lu", (unsigned long)i);
    /* forwarders: RVA lies inside the export directory range and the string matches */
    const DWORD *fn = (const DWORD *)((const char *)m + d->AddressOfFunctions);
    DWORD dstart = exp_nt(m)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    for (int i = 0; i < n; i++) {
        if (t[i].ord < d->Base || t[i].ord - d->Base >= d->NumberOfFunctions) { CHECKF(0, "%s ord %u outside export directory range", dll, t[i].ord); continue; }
        DWORD r = fn[t[i].ord - d->Base];
        int isfwd = r >= dstart && r < dstart + sz;
        CHECKF(isfwd == (t[i].fwd != NULL), "%s ord %u forwarder status", dll, t[i].ord);
        if (isfwd && t[i].fwd && strcmp((const char *)m + r, t[i].fwd)) t_note("%s ord %u forwarder string \"%s\", reference \"%s\"", dll, t[i].ord, (const char *)m + r, t[i].fwd);

    }
}
/* ordinals in [base,max] that are not in the table must not resolve */
static inline void exp_check_gaps(const char *dll, const exp_ent *t, int n, unsigned base, unsigned maxord) {
    static unsigned char seen[4096];
    memset(seen, 0, sizeof seen);
    for (int i = 0; i < n; i++) seen[t[i].ord - base] = 1;
    int checked = 0;
    for (unsigned o = base; o <= maxord && checked < 400; o++) {
        if (seen[o - base]) continue;
        SetLastError(0);
        FARPROC p = t_proc_ord(dll, o);
        CHECKF(p == NULL, "%s unexpected export at gap ordinal %u", dll, o);
        /* kernelbase GetProcAddress: ordinal path maps STATUS_ORDINAL_NOT_FOUND (0xC0000138) -> ERROR_INVALID_ORDINAL;
           an in-range gap has RVA 0 so ntdll yields the module base, which GetProcAddress converts to the same status */
        EXPECT_ERR(ERROR_INVALID_ORDINAL);
        checked++;
    }
    t_note("%s: %d gap ordinals checked", dll, checked);
}
static inline void exp_check_negatives(const char *dll, const char *any_name, unsigned maxord) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    SetLastError(0); CHECK(GetProcAddress(m, "NoSuchExport_olet") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0); CHECK(GetProcAddress(m, "") == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SetLastError(0); CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)(maxord + 1)) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
    SetLastError(0); CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0xFFFF) == NULL); EXPECT_ERR(ERROR_INVALID_ORDINAL);
    SetLastError(0); CHECK(GetProcAddress(m, (LPCSTR)(ULONG_PTR)0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); /* Win10: ordinal 0 is rejected before lookup */
    SetLastError(0); CHECK(GetProcAddress(m, NULL) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* lookup is case sensitive, exact length */
    char buf[256]; size_t l = strlen(any_name);
    memcpy(buf, any_name, l + 1);
    for (size_t i = 0; i < l; i++) if (buf[i] >= 'a' && buf[i] <= 'z') { buf[i] -= 32; break; } else if (buf[i] >= 'A' && buf[i] <= 'Z') { buf[i] += 32; break; }
    if (strcmp(buf, any_name)) { SetLastError(0); CHECK(GetProcAddress(m, buf) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND); }
    memcpy(buf, any_name, l); buf[l] = 'x'; buf[l + 1] = 0;
    SetLastError(0); CHECK(GetProcAddress(m, buf) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    if (l > 1) { memcpy(buf, any_name, l - 1); buf[l - 1] = 0; SetLastError(0); CHECK(GetProcAddress(m, buf) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND); }
    /* repeated resolution is stable */
    FARPROC a = GetProcAddress(m, any_name), b = GetProcAddress(m, any_name);
    CHECK(a != NULL); CHECK(a == b);
    /* LoadLibrary twice returns the same handle; case-insensitive module name */
    char up[64]; snprintf(up, sizeof up, "%s", dll); for (char *c = up; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    HMODULE m2 = LoadLibraryA(up);
    CHECK(m2 == m);
    if (m2) FreeLibrary(m2);
}

/* ---- additional structural sweeps ---- */
typedef LONG (NTAPI *exp_LdrGPA)(HMODULE, const void *, ULONG, PVOID *);
typedef struct { USHORT Length, MaximumLength; PCHAR Buffer; } exp_astr;

/* name -> AddressOfNameOrdinals -> AddressOfFunctions must agree with GetProcAddress (the loader's lookup algorithm) */
static inline void exp_check_name_ordinal_tables(const char *dll) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    DWORD sz; const IMAGE_EXPORT_DIRECTORY *d = exp_dir(m, &sz);
    CHECK(d != NULL); if (!d) return;
    const DWORD *nm = (const DWORD *)((const char *)m + d->AddressOfNames);
    const WORD *no = (const WORD *)((const char *)m + d->AddressOfNameOrdinals);
    const DWORD *fn = (const DWORD *)((const char *)m + d->AddressOfFunctions);
    DWORD dstart = exp_nt(m)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    for (DWORD i = 0; i < d->NumberOfNames; i++) {
        const char *name = (const char *)m + nm[i];
        CHECKF(no[i] < d->NumberOfFunctions, "%s name index %lu ordinal index out of range", dll, (unsigned long)i);
        if (no[i] >= d->NumberOfFunctions) continue;
        DWORD r = fn[no[i]];
        CHECKF(r != 0, "%s!%s has a name but a zero RVA", dll, name);
        if (r >= dstart && r < dstart + sz) continue; /* forwarder: string, not code */
        FARPROC p = GetProcAddress(m, name);
        CHECKF(p == (FARPROC)((const char *)m + r), "%s!%s GetProcAddress disagrees with export directory", dll, name);
        FARPROC q = GetProcAddress(m, (LPCSTR)(ULONG_PTR)(no[i] + d->Base));
        CHECKF(q == p, "%s!%s ordinal %lu disagrees with name lookup", dll, name, (unsigned long)(no[i] + d->Base));
    }
}
/* every table entry: directory slot RVA equals pointer offset (non-forwarders) */
static inline void exp_check_rva_slots(const char *dll, const exp_ent *t, int n) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    DWORD sz; const IMAGE_EXPORT_DIRECTORY *d = exp_dir(m, &sz);
    CHECK(d != NULL); if (!d) return;
    const DWORD *fn = (const DWORD *)((const char *)m + d->AddressOfFunctions);
    DWORD dstart = exp_nt(m)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    for (int i = 0; i < n; i++) {
        if (t[i].ord < d->Base || t[i].ord - d->Base >= d->NumberOfFunctions) continue;
        DWORD r = fn[t[i].ord - d->Base];
        CHECKF(r != 0, "%s ord %u has zero RVA", dll, t[i].ord);
        if (r >= dstart && r < dstart + sz) continue;
        FARPROC p = t_proc_ord(dll, t[i].ord);
        CHECKF(p == (FARPROC)((const char *)m + r), "%s ord %u pointer != base+RVA", dll, t[i].ord);
    }
}
/* non-forwarded exports point into committed executable image memory (or data for data exports: note only) */
static inline void exp_check_exec_pages(const char *dll, const exp_ent *t, int n) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    int nonexec = 0;
    for (int i = 0; i < n; i++) {
        if (t[i].fwd) continue;
        FARPROC p = t_proc_ord(dll, t[i].ord);
        if (!p) continue;
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery((void *)p, &mbi, sizeof mbi) == sizeof mbi);
        CHECK(mbi.Type == MEM_IMAGE);
        CHECK(mbi.State == MEM_COMMIT);
        CHECK(mbi.AllocationBase == (void *)m);
        if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) { nonexec++; t_note("%s ord %u (%s) not in executable page (data export?)", dll, t[i].ord, t[i].name ? t[i].name : "-"); }
    }
    t_note("%s: %d non-executable non-forwarded exports", dll, nonexec);
}
/* ntdll's loader entry point agrees with GetProcAddress, with the documented NTSTATUS values for failures */
static inline void exp_check_ldr(const char *dll, const exp_ent *t, int n, unsigned maxord) {
    HMODULE m = t_module(dll);
    exp_LdrGPA ldr = (exp_LdrGPA)t_proc_in("ntdll.dll", "LdrGetProcedureAddress");
    if (!m || !ldr) { t_skip("cannot resolve loader entry"); return; }
    int lim = 0;
    for (int i = 0; i < n && lim < 120; i++) {
        PVOID out = NULL; LONG st;
        if (t[i].name) {
            exp_astr s = { (USHORT)strlen(t[i].name), (USHORT)strlen(t[i].name) + 1, (PCHAR)t[i].name };
            st = ldr(m, &s, 0, &out);
            CHECKF(st >= 0, "Ldr %s!%s status 0x%lx", dll, t[i].name, (unsigned long)st);
            CHECKF(out == (PVOID)GetProcAddress(m, t[i].name), "Ldr %s!%s differs from GetProcAddress", dll, t[i].name);
        }
        out = NULL;
        st = ldr(m, NULL, t[i].ord, &out);
        CHECKF(st >= 0, "Ldr %s ord %u status 0x%lx", dll, t[i].ord, (unsigned long)st);
        CHECKF(out == (PVOID)t_proc_ord(dll, t[i].ord), "Ldr %s ord %u differs from GetProcAddress", dll, t[i].ord);
        lim++;
    }
    /* unknown name: STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139) on Win10 22H2 */
    { PVOID out = NULL; const char *nm = "NoSuchExport_olet"; exp_astr s = { (USHORT)strlen(nm), (USHORT)strlen(nm) + 1, (PCHAR)nm };
      LONG st = ldr(m, &s, 0, &out); CHECKF((ULONG)st == 0xC0000139u, "unknown name status 0x%lx", (unsigned long)st); }
    /* ordinal beyond the table: STATUS_ORDINAL_NOT_FOUND (0xC0000138) */
    { PVOID out = NULL; LONG st = ldr(m, NULL, maxord + 1, &out); CHECK((ULONG)st == 0xC0000138u); }
    /* neither name nor ordinal: STATUS_INVALID_PARAMETER (0xC000000D) */
    { PVOID out = NULL; LONG st = ldr(m, NULL, 0, &out); CHECK((ULONG)st == 0xC000000Du); }
}
/* table sanity: sorted by ordinal, names well-formed */
static inline void exp_check_table_shape(const char *dll, const exp_ent *t, int n) {
    for (int i = 0; i < n; i++) {
        if (i) CHECKF(t[i].ord > t[i - 1].ord, "%s table not ordinal-ascending at %d", dll, i);
        if (t[i].name) {
            CHECK(t[i].name[0] != 0);
            for (const char *c = t[i].name; *c; c++) CHECKF((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') || *c == '_' || *c == '@' || *c == '?' || *c == '$', "%s odd char in %s", dll, t[i].name);
        }
    }
}
/* handle-level lookups: GetModuleHandleEx variants agree with the cached handle */
static inline void exp_check_handles(const char *dll, const char *name) {
    HMODULE m = t_module(dll);
    if (!m) { t_skip("cannot load %s", dll); return; }
    CHECK(GetModuleHandleA(dll) == m);
    HMODULE h = NULL;
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, dll, &h)); CHECK(h == m);
    FARPROC p = GetProcAddress(m, name); h = NULL;
    CHECK(p != NULL);
    CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h)); CHECK(h == m);
    char path[MAX_PATH]; DWORD n = GetModuleFileNameA(m, path, sizeof path);
    CHECK(n > 0 && n < sizeof path);
    size_t l = strlen(dll); CHECK(n >= l && _stricmp(path + n - l, dll) == 0);
    /* the executable has no such export */
    SetLastError(0); CHECK(GetProcAddress(NULL, name) == NULL); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    /* DOS / NT signatures of the image */
    const IMAGE_DOS_HEADER *dh = (const IMAGE_DOS_HEADER *)m;
    CHECK(dh->e_magic == IMAGE_DOS_SIGNATURE);
    CHECK(exp_nt(m)->Signature == IMAGE_NT_SIGNATURE);
    CHECK(exp_nt(m)->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
    CHECK(exp_nt(m)->FileHeader.Characteristics & IMAGE_FILE_DLL);
    CHECK(exp_nt(m)->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
}
#endif
