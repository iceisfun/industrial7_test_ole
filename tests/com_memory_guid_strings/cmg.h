/* shared helpers for area com_memory_guid_strings */
#ifndef CMG_H
#define CMG_H
#include "t.h"
#include <objbase.h>
#include <stdlib.h>
#include <wchar.h>

#define CMG_E_INVALIDARG   ((HRESULT)0x80070057)
#define CMG_E_OUTOFMEMORY  ((HRESULT)0x8007000E)
#define CMG_E_NOINTERFACE  ((HRESULT)0x80004002)
#define CMG_CLASSSTRING    ((HRESULT)0x800401F3)
#define CMG_IIDSTRING      ((HRESULT)0x800401F4)
#define CMG_CLASSNOTREG    ((HRESULT)0x80040154)

/* Modules that export the same entry point (ole32 forwards or reimplements combase). */
static const char *const CMG_MODS[] = { "ole32.dll", "combase.dll", NULL };
#define FOR_MODS(m) for (const char *const *mp_ = CMG_MODS; *mp_; mp_++) for (const char *m = *mp_, *once_ = (const char *)1; once_; once_ = NULL)

static const GUID G_NULL      = {0,0,0,{0,0,0,0,0,0,0,0}};
static const GUID G_IUnknown  = {0x00000000,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G_IMalloc   = {0x00000002,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G_IMallocSpy= {0x0000001D,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G_IStream   = {0x0000000C,0x0000,0x0000,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID G_StdFont   = {0x0BE35203,0x8F91,0x11CE,{0x9D,0xE3,0x00,0xAA,0x00,0x4B,0xB8,0x51}};
static const GUID G_StdPicture= {0x0BE35204,0x8F91,0x11CE,{0x9D,0xE3,0x00,0xAA,0x00,0x4B,0xB8,0x51}};
static const GUID G_Sample    = {0x12345678,0x9ABC,0xDEF0,{0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0}};
static const GUID G_Ones      = {0xFFFFFFFF,0xFFFF,0xFFFF,{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};

#define CMG_STDFONT_STR  L"{0BE35203-8F91-11CE-9DE3-00AA004BB851}"
#define CMG_STDPIC_STR   L"{0BE35204-8F91-11CE-9DE3-00AA004BB851}"
#define CMG_SAMPLE_STR   L"{12345678-9ABC-DEF0-1234-56789ABCDEF0}"
#define CMG_NULL_STR     L"{00000000-0000-0000-0000-000000000000}"
#define CMG_ONES_STR     L"{FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF}"

static inline int cmg_guid_eq(const GUID *a, const GUID *b) { return memcmp(a, b, sizeof(GUID)) == 0; }
static inline IMalloc *cmg_getmalloc(const char *mod) {
    typedef HRESULT (WINAPI *pfn)(DWORD, IMalloc **);
    pfn f = (pfn)t_proc_in(mod, "CoGetMalloc");
    IMalloc *m = NULL;
    if (!f || f(1, &m) != S_OK) return NULL;
    return m;
}
#endif
