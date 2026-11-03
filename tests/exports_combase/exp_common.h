#ifndef EXP_COMMON_H
#define EXP_COMMON_H
#include "t.h"
#include "exp_data.h"

#define EXP_COMBASE "combase.dll"
#define EXP_NTABLE (sizeof(cb_all) / sizeof(cb_all[0]))

/* In-memory PE helpers over an already loaded module (trivial scaffolding; no DLL code is called). */
const IMAGE_NT_HEADERS *exp_nt(HMODULE m);
const IMAGE_EXPORT_DIRECTORY *exp_dir(HMODULE m, DWORD *dir_rva, DWORD *dir_size);
SIZE_T exp_image_size(HMODULE m);
int exp_in_image(HMODULE m, const void *p);
/* Biased ordinal of a name via the module's own export directory (0 when not exported by name). */
unsigned exp_dir_ordinal_of(HMODULE m, const char *name);
/* Function RVA for a biased ordinal (0 when out of range). */
DWORD exp_dir_rva_of_ordinal(HMODULE m, unsigned ord);
/* Expected-table lookups (from tools/combase_exports.json). */
const struct cbexp *exp_find_name(const char *name);
const struct cbexp *exp_find_ord(unsigned ord);

/* Table-driven checks. */
void exp_check_names(const char *const *names, size_t n);
void exp_check_ord_range(unsigned lo, unsigned hi);
void exp_check_fwd_range(HMODULE m, const struct fwd *tab, size_t lo, size_t hi, const char *dll);
#endif
