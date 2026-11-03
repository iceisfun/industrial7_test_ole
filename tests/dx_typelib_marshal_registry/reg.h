#ifndef RG_H
#define RG_H
#include "tm.h"
void rg_spec(TMLibSpec *s, WORD maj, WORD min, LCID lcid, const GUID *iid);
HRESULT rg_register(const TMLibSpec *s, const char *leaf, int foruser, const WCHAR *helpdir, WCHAR *path_out);
HRESULT rg_unregister(const TMLibSpec *s, int foruser);
LONG rg_cu(const TMLibSpec *s, const WCHAR *tail, const WCHAR *val, WCHAR *out, DWORD cch);
int rg_cu_exists(const TMLibSpec *s, const WCHAR *tail);
int rg_if_cu(const GUID *iid, const WCHAR *tail, const WCHAR *val, WCHAR *out, DWORD cch, REGSAM wow, LONG *err);
int rg_if_exists(HKEY root, const GUID *iid, const WCHAR *tail, REGSAM wow);
#define E_INVALIDARG_ ((HRESULT)0x80070057L)
#define TLB_REGACCESS ((HRESULT)0x8002801CL)
#define TLB_NOTREG    ((HRESULT)0x8002801DL)

static inline int rg_hkcu_if(const GUID *iid) { return rg_if_exists(tm_ur(), iid, NULL, 0); }
#endif
