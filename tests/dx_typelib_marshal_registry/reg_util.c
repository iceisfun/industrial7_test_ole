/* Helpers shared by the registry tests. */
#include "tm.h"
#include "reg.h"

void rg_spec(TMLibSpec *s, WORD maj, WORD min, LCID lcid, const GUID *iid)
{
    tm_spec_init(s, &TM_LIBID_Mini, maj, min, lcid, iid);
    s->doc = L"Mini doc string";
    s->ifname = L"IOletMini1";
}

HRESULT rg_register(const TMLibSpec *s, const char *leaf, int foruser, const WCHAR *helpdir, WCHAR *path_out)
{
    const WCHAR *p = tm_mini_path(leaf, s); ITypeLib *tl = NULL; HRESULT hr;
    if (path_out) path_out[0] = 0;
    if (!p) return E_FAIL;
    if (path_out) lstrcpyW(path_out, p);
    hr = U(LoadTypeLibEx)(p, REGKIND_NONE, &tl);
    if (FAILED(hr)) return hr;
    if (foruser) hr = pRegisterTypeLibForUser(tl, (OLECHAR *)p, (OLECHAR *)helpdir);
    else hr = U(RegisterTypeLib)(tl, (OLECHAR *)p, (OLECHAR *)helpdir);
    ITypeLib_Release(tl);
    return hr;
}

HRESULT rg_unregister(const TMLibSpec *s, int foruser)
{
    if (foruser) return pUnRegisterTypeLibForUser(&s->libid, s->maj, s->min, s->lcid, s->syskind);
    return U(UnRegisterTypeLib)(&s->libid, s->maj, s->min, s->lcid, s->syskind);
}

/* Read a string from the HKCU copy of TypeLib\{libid}\maj.min\tail (tail may be NULL) */
LONG rg_cu(const TMLibSpec *s, const WCHAR *tail, const WCHAR *val, WCHAR *out, DWORD cch)
{
    WCHAR p[260];
    tm_tl_path(tm_ur(), &s->libid, s->maj, s->min, tail, p, 260);
    return tm_reg_sz(tm_ur(), p, val, out, cch, 0);
}
int rg_cu_exists(const TMLibSpec *s, const WCHAR *tail)
{
    WCHAR p[260];
    tm_tl_path(tm_ur(), &s->libid, s->maj, s->min, tail, p, 260);
    return tm_reg_key_exists(tm_ur(), p, 0);
}
int rg_if_cu(const GUID *iid, const WCHAR *tail, const WCHAR *val, WCHAR *out, DWORD cch, REGSAM wow, LONG *err)
{
    WCHAR p[260]; LONG e;
    tm_if_path(tm_ur(), iid, tail, p, 260);
    e = tm_reg_sz(tm_ur(), p, val, out, cch, wow);
    if (err) *err = e;
    return e == 0;
}
int rg_if_exists(HKEY root, const GUID *iid, const WCHAR *tail, REGSAM wow)
{
    WCHAR p[260];
    tm_if_path(root, iid, tail, p, 260);
    return tm_reg_key_exists(root, p, wow);
}
