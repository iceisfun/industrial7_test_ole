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
/* The reference table is the Windows 10 export layout. Wine's combase has a different ordinal layout and lacks
 * exports, so files defining EXP_WINE_TOLERANT downgrade failing checks to a counted miss when running on Wine
 * (Windows is the authority; on Windows every check is hard). */
void exp_wine_miss(void);
unsigned exp_wine_misses(void);
#ifdef EXP_WINE_TOLERANT
#undef CHECK
#undef CHECKF
#undef EXPECT_EQ
#undef EXPECT_ERR
#define CHECK(c)        do { int ok_ = !!(c); if (!ok_ && t_is_wine()) exp_wine_miss(); else t_check(ok_, __FILE__, __LINE__, #c, NULL); } while (0)
#define CHECKF(c, ...)  do { int ok_ = !!(c); if (!ok_ && t_is_wine()) exp_wine_miss(); else t_check(ok_, __FILE__, __LINE__, #c, __VA_ARGS__); } while (0)
#define EXPECT_EQ(a, b) do { int ok_ = (ULONG_PTR)(a) == (ULONG_PTR)(b); if (!ok_ && t_is_wine()) exp_wine_miss(); else t_check(ok_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)(ULONG_PTR)(a), (unsigned long long)(ULONG_PTR)(b)); } while (0)
#define EXPECT_ERR(err) do { DWORD e_ = GetLastError(); int ok_ = e_ == (DWORD)(err); if (!ok_ && t_is_wine()) exp_wine_miss(); else t_check(ok_, __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#endif
#endif
