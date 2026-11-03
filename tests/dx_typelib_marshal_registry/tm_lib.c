/* Run-time construction of the test type libraries with CreateTypeLib2 / ICreateTypeLib2 / ICreateTypeInfo. */
#include "tm.h"

#define MAXTD 512
typedef struct { const char *name; ICreateTypeInfo *cti; ITypeInfo *ti; } TLEnt;
typedef struct {
    ICreateTypeLib2 *lib; ICreateTypeInfo *cur;
    TLEnt ents[24]; int nent;
    TYPEDESC pool[MAXTD]; int npool;
    ITypeInfo *t_unk, *t_disp; ITypeLib *stdole;
} Ctx;

static TYPEDESC *td_new(Ctx *c) { TYPEDESC *t = &c->pool[c->npool++]; memset(t, 0, sizeof *t); return t; }

static HREFTYPE href_for(Ctx *c, ITypeInfo *ti)
{
    HREFTYPE h = 0;
    HRESULT hr = ICreateTypeInfo_AddRefTypeInfo(c->cur, ti, &h);
    if (FAILED(hr)) t_note("AddRefTypeInfo failed 0x%lx", (long)hr);
    return h;
}
static ITypeInfo *ent_ti(Ctx *c, const char *name)
{
    int i;
    for (i = 0; i < c->nent; i++) if (!strcmp(c->ents[i].name, name)) return c->ents[i].ti;
    return NULL;
}

/* "i4", "bstr*", "sa.i4", "sa.bstr*", "ref.OletPoint", "ref.IOletSink**" */
static void td_parse(Ctx *c, const char *s, TYPEDESC *out)
{
    char base[48]; int stars = 0, n = (int)strlen(s), i; VARTYPE vt = VT_EMPTY;
    TYPEDESC *inner = NULL;
    while (n > 0 && s[n - 1] == '*') { stars++; n--; }
    memcpy(base, s, n); base[n] = 0;
    memset(out, 0, sizeof *out);
    if (!strncmp(base, "sa.", 3)) {
        TYPEDESC *e = td_new(c);
        td_parse(c, base + 3, e);
        out->vt = VT_SAFEARRAY; out->lptdesc = e;
    } else if (!strncmp(base, "ref.", 4)) {
        ITypeInfo *ti = ent_ti(c, base + 4);
        out->vt = VT_USERDEFINED; out->hreftype = ti ? href_for(c, ti) : 0;
    } else {
        static const struct { const char *n; VARTYPE vt; } m[] = {
            {"i1", VT_I1}, {"ui1", VT_UI1}, {"i2", VT_I2}, {"ui2", VT_UI2}, {"i4", VT_I4}, {"ui4", VT_UI4}, {"i8", VT_I8},
            {"ui8", VT_UI8}, {"r4", VT_R4}, {"r8", VT_R8}, {"bool", VT_BOOL}, {"cy", VT_CY}, {"date", VT_DATE},
            {"bstr", VT_BSTR}, {"var", VT_VARIANT}, {"unk", VT_UNKNOWN}, {"disp", VT_DISPATCH}, {"int", VT_INT},
            {"hr", VT_HRESULT}, {"void", VT_VOID}, {NULL, 0} };
        for (i = 0; m[i].n; i++) if (!strcmp(m[i].n, base)) vt = m[i].vt;
        out->vt = vt;
    }
    (void)inner;
    while (stars--) {
        TYPEDESC *cp = td_new(c);
        *cp = *out;
        out->vt = VT_PTR; out->lptdesc = cp;
    }
}

typedef struct { const char *name; MEMBERID id; INVOKEKIND ik; WORD fflags; const char *params; } FN;

static WCHAR *wide(const char *s) { static WCHAR buf[16][64]; static int k; WCHAR *w = buf[k++ & 15]; int i; for (i = 0; s[i] && i < 63; i++) w[i] = (WCHAR)s[i]; w[i] = 0; return w; }

static HRESULT add_funcs(Ctx *c, const FN *fns, int n, FUNCKIND fk)
{
    int i, p;
    for (i = 0; i < n; i++) {
        ELEMDESC el[12]; FUNCDESC fd; char buf[512]; char *tok, *save; WCHAR *names[13]; int np = 0; HRESULT hr;
        strcpy(buf, fns[i].params);
        memset(el, 0, sizeof el); memset(&fd, 0, sizeof fd);
        names[0] = wide(fns[i].name);
        for (tok = strtok_r(buf, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
            char *f1 = strchr(tok, ':'), *f2;
            if (!f1) break;
            *f1++ = 0; f2 = strchr(f1, ':'); *f2++ = 0;
            names[np + 1] = wide(tok);
            td_parse(c, f2, &el[np].tdesc);
            el[np].idldesc.wIDLFlags = 0;
            if (strstr(f1, "in")) el[np].idldesc.wIDLFlags |= IDLFLAG_FIN;
            if (strstr(f1, "out")) el[np].idldesc.wIDLFlags |= IDLFLAG_FOUT;
            if (strstr(f1, "ret")) el[np].idldesc.wIDLFlags |= IDLFLAG_FRETVAL;
            np++;
        }
        fd.memid = fns[i].id; fd.funckind = fk; fd.invkind = fns[i].ik; fd.callconv = CC_STDCALL;
        fd.cParams = (SHORT)np; fd.lprgelemdescParam = np ? el : NULL; fd.wFuncFlags = fns[i].fflags;
        fd.elemdescFunc.tdesc.vt = VT_HRESULT;
        hr = ICreateTypeInfo_AddFuncDesc(c->cur, i, &fd);
        if (FAILED(hr)) { t_note("AddFuncDesc(%s) failed 0x%lx", fns[i].name, (long)hr); return hr; }
        hr = ICreateTypeInfo_SetFuncAndParamNames(c->cur, i, names, (fns[i].ik & (INVOKE_PROPERTYPUT | INVOKE_PROPERTYPUTREF)) ? np : np + 1);
        if (FAILED(hr)) { t_note("SetFuncAndParamNames(%s) failed 0x%lx", fns[i].name, (long)hr); return hr; }
        (void)p;
    }
    return S_OK;
}

static HRESULT start_type(Ctx *c, const char *name, TYPEKIND k, const GUID *g, WORD flags)
{
    ICreateTypeInfo *cti = NULL; ITypeInfo *ti = NULL; HRESULT hr;
    hr = ICreateTypeLib2_CreateTypeInfo(c->lib, wide(name), k, &cti);
    if (FAILED(hr)) return hr;
    hr = ICreateTypeInfo_QueryInterface(cti, &IID_ITypeInfo, (void **)&ti);
    if (FAILED(hr)) return hr;
    c->ents[c->nent].name = name; c->ents[c->nent].cti = cti; c->ents[c->nent].ti = ti; c->nent++;
    c->cur = cti;
    if (g) { hr = ICreateTypeInfo_SetGuid(cti, g); if (FAILED(hr)) return hr; }
    if (flags) { hr = ICreateTypeInfo_SetTypeFlags(cti, flags); if (FAILED(hr)) return hr; }
    return S_OK;
}

static HRESULT open_ctx(Ctx *c, const WCHAR *path, SYSKIND sk)
{
    WCHAR sys[MAX_PATH]; HRESULT hr;
    memset(c, 0, sizeof *c);
    GetSystemDirectoryW(sys, MAX_PATH);
    lstrcatW(sys, L"\\stdole2.tlb");
    hr = U(LoadTypeLibEx)(sys, REGKIND_NONE, &c->stdole);
    if (FAILED(hr)) return hr;
    hr = ITypeLib_GetTypeInfoOfGuid(c->stdole, &IID_IUnknown, &c->t_unk);
    if (FAILED(hr)) return hr;
    hr = ITypeLib_GetTypeInfoOfGuid(c->stdole, &IID_IDispatch, &c->t_disp);
    if (FAILED(hr)) return hr;
    return U(CreateTypeLib2)(sk, path, &c->lib);
}
static void close_ctx(Ctx *c)
{
    int i;
    for (i = 0; i < c->nent; i++) { ITypeInfo_Release(c->ents[i].ti); ICreateTypeInfo_Release(c->ents[i].cti); }
    if (c->lib) ICreateTypeLib2_Release(c->lib);
    if (c->t_unk) ITypeInfo_Release(c->t_unk);
    if (c->t_disp) ITypeInfo_Release(c->t_disp);
    if (c->stdole) ITypeLib_Release(c->stdole);
}
static HRESULT set_base(Ctx *c, ITypeInfo *base)
{
    HREFTYPE h = href_for(c, base);
    return ICreateTypeInfo_AddImplType(c->cur, 0, h);
}
#define CK(x) do { HRESULT h_ = (x); if (FAILED(h_)) { t_note("%s failed 0x%lx (%s:%d)", #x, (long)h_, __FILE__, __LINE__); hr = h_; goto done; } } while (0)

static HRESULT lib_header(Ctx *c, const GUID *libid, WORD maj, WORD min, LCID lcid, WORD flags, const WCHAR *name, const WCHAR *doc, const WCHAR *help)
{
    HRESULT hr;
    CK(ICreateTypeLib2_SetGuid(c->lib, libid));
    CK(ICreateTypeLib2_SetVersion(c->lib, maj, min));
    CK(ICreateTypeLib2_SetLcid(c->lib, lcid));
    CK(ICreateTypeLib2_SetLibFlags(c->lib, flags));
    if (name) CK(ICreateTypeLib2_SetName(c->lib, (LPOLESTR)name));
    if (doc) CK(ICreateTypeLib2_SetDocString(c->lib, (LPOLESTR)doc));
    if (help) CK(ICreateTypeLib2_SetHelpFileName(c->lib, (LPOLESTR)help));
    hr = S_OK;
done:
    return hr;
}

/* --------------------------------------------------------------------- main library */
static const FN fn_sink[] = {
    { "Notify", 1, INVOKE_FUNC, 0, "n:in:i4 s:in:bstr" },
    { "Total", 2, INVOKE_FUNC, 0, "t:out,ret:i4*" },
};
static const FN fn_custom[] = {
    { "AddLong", 1, INVOKE_FUNC, 0, "a:in:i4 b:in:i4 r:out,ret:i4*" },
    { "ScalarsIn", 2, INVOKE_FUNC, 0, "b:in:ui1 s:in:i2 l:in:i4 h:in:i8 f:in:r4 d:in:r8 vb:in:bool cy:in:cy dt:in:date" },
    { "ScalarsOut", 3, INVOKE_FUNC, 0, "b:out:ui1* s:out:i2* l:out:i4* h:out:i8* f:out:r4* d:out:r8* vb:out:bool* cy:out:cy* dt:out:date*" },
    { "EchoBstr", 4, INVOKE_FUNC, 0, "s:in:bstr r:out,ret:bstr*" },
    { "BstrOut", 5, INVOKE_FUNC, 0, "s:out:bstr*" },
    { "BstrInOut", 6, INVOKE_FUNC, 0, "s:in,out:bstr*" },
    { "EchoVariant", 7, INVOKE_FUNC, 0, "v:in:var r:out,ret:var*" },
    { "VariantInOut", 8, INVOKE_FUNC, 0, "v:in,out:var*" },
    { "SumArray", 9, INVOKE_FUNC, 0, "sa:in:sa.i4 sum:out,ret:i4*" },
    { "MakeArray", 10, INVOKE_FUNC, 0, "n:in:i4 sa:out,ret:sa.i4*" },
    { "EchoBstrArray", 11, INVOKE_FUNC, 0, "in:in:sa.bstr out:out,ret:sa.bstr*" },
    { "GetSink", 12, INVOKE_FUNC, 0, "p:out,ret:ref.IOletSink**" },
    { "PutSink", 13, INVOKE_FUNC, 0, "p:in:ref.IOletSink*" },
    { "EchoUnk", 14, INVOKE_FUNC, 0, "in:in:unk out:out,ret:unk*" },
    { "EchoPoint", 15, INVOKE_FUNC, 0, "p:in:ref.OletPoint r:out,ret:ref.OletPoint*" },
    { "SwapPoint", 16, INVOKE_FUNC, 0, "p:in,out:ref.OletPoint*" },
    { "EchoMix", 17, INVOKE_FUNC, 0, "m:in:ref.OletMix* r:out:ref.OletMix*" },
    { "EchoColor", 18, INVOKE_FUNC, 0, "c:in:ref.OletColor r:out,ret:ref.OletColor*" },
    { "ReturnHr", 19, INVOKE_FUNC, 0, "hr:in:i4" },
    { "FailWithOut", 20, INVOKE_FUNC, 0, "hr:in:i4 pb:out:bstr* pl:out:i4* pp:out:ref.IOletSink**" },
    { "RaiseError", 21, INVOKE_FUNC, 0, "desc:in:bstr hr:in:i4" },
    { "Callback", 22, INVOKE_FUNC, 0, "sink:in:ref.IOletSink* n:in:i4 total:out,ret:i4*" },
    { "ApartmentInfo", 23, INVOKE_FUNC, 0, "tid:out:i4* pid:out:i4* apt:out:i4*" },
    { "LongPtrs", 24, INVOKE_FUNC, 0, "in:in:i4* out:out:i4* inout:in,out:i4*" },
};
static const FN fn_dual[] = {
    { "Add", 1, INVOKE_FUNC, 0, "a:in:i4 b:in:i4 r:out,ret:i4*" },
    { "Concat", 2, INVOKE_FUNC, 0, "a:in:bstr b:in:bstr r:out,ret:bstr*" },
    { "Value", 3, INVOKE_PROPERTYGET, 0, "r:out,ret:var*" },
    { "Value", 3, INVOKE_PROPERTYPUT, 0, "v:in:var" },
    { "Count", 4, INVOKE_PROPERTYGET, 0, "r:out,ret:i4*" },
    { "Fail", 5, INVOKE_FUNC, 0, "hr:in:i4" },
    { "ItemName", 6, INVOKE_FUNC, 0, "idx:in:i4 r:out,ret:bstr*" },
    { "RaiseError", 7, INVOKE_FUNC, 0, "desc:in:bstr hr:in:i4" },
    { "Sum", 8, INVOKE_FUNC, 0, "sa:in:sa.i4 r:out,ret:i4*" },
    { "_NewEnum", -4, INVOKE_PROPERTYGET, FUNCFLAG_FRESTRICTED, "r:out,ret:unk*" },
};
static const FN fn_disp[] = {
    { "Echo", 1, INVOKE_FUNC, 0, "s:in:bstr r:out,ret:bstr*" },
    { "Twice", 2, INVOKE_FUNC, 0, "n:in:i4 r:out,ret:i4*" },
};
static const FN fn_plain[] = { { "Ping", 1, INVOKE_FUNC, 0, "x:in:i4" } };
static const FN fn_proxyflag[] = { { "Ping", 1, INVOKE_FUNC, 0, "x:in:i4" } };

HRESULT tm_build_main(const WCHAR *path)
{
    Ctx *c = calloc(1, sizeof *c); HRESULT hr; VARIANT v; ICreateTypeInfo *cti; int i;
    if (!c) return E_OUTOFMEMORY;
    hr = open_ctx(c, path, SYS_WIN64);
    if (FAILED(hr)) { t_note("open_ctx failed 0x%lx", (long)hr); goto done; }
    CK(lib_header(c, &TM_LIBID_Main, TM_MAIN_MAJ, TM_MAIN_MIN, 0, LIBFLAG_FRESTRICTED | LIBFLAG_FHIDDEN, L"OletLib", L"Olet typelib marshal test library", NULL));

    /* enum OletColor */
    CK(start_type(c, "OletColor", TKIND_ENUM, NULL, 0)); cti = c->cur;
    {
        static const struct { const char *n; int v; } e[] = { {"OLET_RED", 1}, {"OLET_GREEN", 2}, {"OLET_BLUE", 4} };
        for (i = 0; i < 3; i++) {
            VARDESC vd; memset(&vd, 0, sizeof vd);
            VariantInit(&v); V_VT(&v) = VT_I4; V_I4(&v) = e[i].v;
            vd.memid = MEMBERID_NIL; vd.varkind = VAR_CONST; vd.elemdescVar.tdesc.vt = VT_INT; vd.lpvarValue = &v;
            CK(ICreateTypeInfo_AddVarDesc(cti, i, &vd));
            CK(ICreateTypeInfo_SetVarName(cti, i, wide(e[i].n)));
        }
        CK(ICreateTypeInfo_LayOut(cti));
    }
    /* record OletPoint */
    CK(start_type(c, "OletPoint", TKIND_RECORD, &TM_GUID_OletPoint, 0)); cti = c->cur;
    {
        static const char *fn[] = { "x", "y" };
        for (i = 0; i < 2; i++) {
            VARDESC vd; memset(&vd, 0, sizeof vd);
            vd.memid = i; vd.varkind = VAR_PERINSTANCE; vd.elemdescVar.tdesc.vt = VT_I4; vd.oInst = i * 4;
            CK(ICreateTypeInfo_AddVarDesc(cti, i, &vd));
            CK(ICreateTypeInfo_SetVarName(cti, i, wide(fn[i])));
        }
        CK(ICreateTypeInfo_SetAlignment(cti, 4));
        CK(ICreateTypeInfo_LayOut(cti));
    }
    /* record OletMix { BSTR name; double d; long id; OletPoint pt; } */
    CK(start_type(c, "OletMix", TKIND_RECORD, &TM_GUID_OletMix, 0)); cti = c->cur;
    {
        static const char *fnm[] = { "name", "d", "id", "pt" };
        static const int off[] = { 0, 8, 16, 20 };
        for (i = 0; i < 4; i++) {
            VARDESC vd; memset(&vd, 0, sizeof vd);
            vd.memid = i; vd.varkind = VAR_PERINSTANCE; vd.oInst = off[i];
            if (i == 0) vd.elemdescVar.tdesc.vt = VT_BSTR;
            else if (i == 1) vd.elemdescVar.tdesc.vt = VT_R8;
            else if (i == 2) vd.elemdescVar.tdesc.vt = VT_I4;
            else { vd.elemdescVar.tdesc.vt = VT_USERDEFINED; vd.elemdescVar.tdesc.hreftype = href_for(c, ent_ti(c, "OletPoint")); }
            CK(ICreateTypeInfo_AddVarDesc(cti, i, &vd));
            CK(ICreateTypeInfo_SetVarName(cti, i, wide(fnm[i])));
        }
        CK(ICreateTypeInfo_SetAlignment(cti, 8));
        CK(ICreateTypeInfo_LayOut(cti));
    }
    /* IOletSink : IUnknown */
    CK(start_type(c, "IOletSink", TKIND_INTERFACE, &TM_IID_IOletSink, TYPEFLAG_FOLEAUTOMATION));
    CK(set_base(c, c->t_unk)); CK(add_funcs(c, fn_sink, 2, FUNC_PUREVIRTUAL)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* IOletCustom : IUnknown */
    CK(start_type(c, "IOletCustom", TKIND_INTERFACE, &TM_IID_IOletCustom, TYPEFLAG_FOLEAUTOMATION));
    CK(set_base(c, c->t_unk)); CK(add_funcs(c, fn_custom, (int)(sizeof fn_custom / sizeof fn_custom[0]), FUNC_PUREVIRTUAL)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* IOletDual : IDispatch (dual) */
    CK(start_type(c, "IOletDual", TKIND_INTERFACE, &TM_IID_IOletDual, TYPEFLAG_FDUAL | TYPEFLAG_FOLEAUTOMATION));
    CK(set_base(c, c->t_disp)); CK(add_funcs(c, fn_dual, (int)(sizeof fn_dual / sizeof fn_dual[0]), FUNC_PUREVIRTUAL)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* IOletDisp : pure dispinterface */
    CK(start_type(c, "IOletDisp", TKIND_DISPATCH, &TM_IID_IOletDisp, 0));
    CK(set_base(c, c->t_disp)); CK(add_funcs(c, fn_disp, 2, FUNC_DISPATCH)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* IOletPlain : IUnknown, no automation flags */
    CK(start_type(c, "IOletPlain", TKIND_INTERFACE, &TM_IID_IOletPlain, 0));
    CK(set_base(c, c->t_unk)); CK(add_funcs(c, fn_plain, 1, FUNC_PUREVIRTUAL)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* IOletProxyFlag : oleautomation + proxy */
    CK(start_type(c, "IOletProxyFlag", TKIND_INTERFACE, &TM_IID_IOletProxyFlag, TYPEFLAG_FOLEAUTOMATION | TYPEFLAG_FPROXY));
    CK(set_base(c, c->t_unk)); CK(add_funcs(c, fn_proxyflag, 1, FUNC_PUREVIRTUAL)); CK(ICreateTypeInfo_LayOut(c->cur));
    /* coclass */
    CK(start_type(c, "OletCo", TKIND_COCLASS, &TM_CLSID_OletCo, 0)); cti = c->cur;
    CK(ICreateTypeInfo_AddImplType(cti, 0, href_for(c, ent_ti(c, "IOletCustom"))));
    CK(ICreateTypeInfo_SetImplTypeFlags(cti, 0, IMPLTYPEFLAG_FDEFAULT));
    CK(ICreateTypeInfo_AddImplType(cti, 1, href_for(c, ent_ti(c, "IOletDual"))));
    CK(ICreateTypeInfo_LayOut(cti));
    CK(ICreateTypeLib2_SaveAllChanges(c->lib));
done:
    close_ctx(c);
    free(c);
    return hr;
}

/* --------------------------------------------------------------------- mini libraries */
void tm_spec_init(TMLibSpec *s, const GUID *libid, WORD maj, WORD min, LCID lcid, const GUID *iid)
{
    memset(s, 0, sizeof *s);
    s->libid = *libid; s->maj = maj; s->min = min; s->lcid = lcid; s->syskind = SYS_WIN64;
    s->name = L"OletMini"; s->iid = *iid; s->ifname = L"IOletMini1"; s->ifflags = TYPEFLAG_FOLEAUTOMATION; s->ifkind = TKIND_INTERFACE;
}

HRESULT tm_build_mini(const WCHAR *path, const TMLibSpec *sp)
{
    Ctx *c = calloc(1, sizeof *c); HRESULT hr; static const FN ping[] = { { "Ping", 1, INVOKE_FUNC, 0, "x:in:i4 r:out,ret:i4*" } };
    static const FN dping[] = { { "Ping", 1, INVOKE_FUNC, 0, "x:in:i4 r:out,ret:i4*" } };
    char nm[64]; int i;
    if (!c) return E_OUTOFMEMORY;
    hr = open_ctx(c, path, sp->syskind);
    if (FAILED(hr)) { t_note("open_ctx(mini) failed 0x%lx", (long)hr); goto done; }
    CK(lib_header(c, &sp->libid, sp->maj, sp->min, sp->lcid, sp->libflags, sp->name, sp->doc, sp->helpfile));
    if (sp->ifname) {
        for (i = 0; sp->ifname[i] && i < 60; i++) nm[i] = (char)sp->ifname[i];
        nm[i] = 0;
        {
            char *keep = _strdup(nm);
            CK(start_type(c, keep, sp->ifkind, &sp->iid, sp->ifflags));
            CK(set_base(c, (sp->dual_func || sp->ifkind == TKIND_DISPATCH) ? c->t_disp : c->t_unk));
            CK(add_funcs(c, ping, 1, sp->ifkind == TKIND_DISPATCH ? FUNC_DISPATCH : FUNC_PUREVIRTUAL));
            CK(ICreateTypeInfo_LayOut(c->cur));
        }
    }
    CK(ICreateTypeLib2_SaveAllChanges(c->lib));
done:
    close_ctx(c);
    free(c);
    return hr;
}

/* --------------------------------------------------------------------- on-demand files */
static WCHAR g_main[MAX_PATH]; static int g_main_state;   /* 0 unbuilt, 1 ok, -1 failed */
const WCHAR *tm_main_path(void)
{
    if (g_main_state == 0) {
        char a[MAX_PATH]; HRESULT hr;
        t_tmpfile(a, sizeof a, "olet_main.tlb");
        MultiByteToWideChar(CP_ACP, 0, a, -1, g_main, MAX_PATH);
        hr = tm_build_main(g_main);
        if (FAILED(hr)) { t_note("tm_build_main failed 0x%lx", (long)hr); g_main_state = -1; DeleteFileW(g_main); }
        else g_main_state = 1;
    }
    return g_main_state > 0 ? g_main : NULL;
}

const WCHAR *tm_mini_path(const char *leaf, const TMLibSpec *spec)
{
    static WCHAR out[8][MAX_PATH]; static int k; WCHAR *w = out[k++ & 7]; char a[MAX_PATH]; HRESULT hr;
    t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    hr = tm_build_mini(w, spec);
    if (FAILED(hr)) { t_note("tm_build_mini(%s) failed 0x%lx", leaf, (long)hr); return NULL; }
    return w;
}
