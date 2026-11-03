/* CreateProxyFromTypeInfo / CreateStubFromTypeInfo: proxies and stubs generated at run time from an
 * oleautomation interface description built with CreateTypeLib2 (saved to a temp file and loaded back, REGKIND_NONE:
 * nothing is registered).  The interface re-uses IID psd_IID_One / psd_IID_Two so the PsdSrv server object can serve it. */
#include "psd.h"

static HRESULT (WINAPI *p_CreateTypeLib2)(SYSKIND, LPCOLESTR, ICreateTypeLib2 **);
static HRESULT (WINAPI *p_LoadTypeLibEx)(LPCOLESTR, REGKIND, ITypeLib **);
static HRESULT (WINAPI *p_LoadTypeLib)(LPCOLESTR, ITypeLib **);
static HRESULT (WINAPI *p_CreateProxy)(ITypeInfo *, IUnknown *, REFIID, IRpcProxyBuffer **, void **);
static HRESULT (WINAPI *p_CreateStub)(ITypeInfo *, REFIID, IUnknown *, IRpcStubBuffer **);

static int ti_init(void) {
    static int done, good;
    if (done) return good;
    done = 1;
    p_CreateTypeLib2 = (void *)t_proc_in("oleaut32.dll", "CreateTypeLib2");
    p_LoadTypeLibEx = (void *)t_proc_in("oleaut32.dll", "LoadTypeLibEx");
    p_LoadTypeLib = (void *)t_proc_in("oleaut32.dll", "LoadTypeLib");
    p_CreateProxy = (void *)t_proc_in("rpcrt4.dll", "CreateProxyFromTypeInfo");
    p_CreateStub = (void *)t_proc_in("rpcrt4.dll", "CreateStubFromTypeInfo");
    good = p_CreateTypeLib2 && p_LoadTypeLibEx && p_LoadTypeLib && p_CreateProxy && p_CreateStub && psd_init();
    return good;
}

typedef struct TiCtx { ITypeLib *tl; ITypeInfo *ti; } TiCtx;

/* kind 1: IOletOne { HRESULT Add([in] long a, [in] long b, [out] long *r); }
 * kind 2: IOletTwo { Echo(long, long*); Str([in] BSTR, long*); } (methods 3,4) */
static int ti_build(TiCtx *c, int kind, int oleautomation) {
    memset(c, 0, sizeof *c);
    char a[MAX_PATH]; WCHAR path[MAX_PATH];
    t_tmpfile(a, sizeof a, kind == 1 ? "psdone.tlb" : "psdtwo.tlb");
    MultiByteToWideChar(CP_ACP, 0, a, -1, path, MAX_PATH);
    ITypeLib *so = NULL;
    if (p_LoadTypeLib(L"stdole2.tlb", &so) != S_OK) { t_note("stdole2.tlb not loadable"); return 0; }
    ITypeInfo *unk = NULL;
    GUID iunk = {0,0,0,{0xC0,0,0,0,0,0,0,0x46}};
    if (ITypeLib_GetTypeInfoOfGuid(so, &iunk, &unk) != S_OK) { ITypeLib_Release(so); return 0; }
    ICreateTypeLib2 *ctl = NULL;
    if (p_CreateTypeLib2(SYS_WIN64, path, &ctl) != S_OK) { ITypeInfo_Release(unk); ITypeLib_Release(so); return 0; }
    ICreateTypeInfo *cti = NULL;
    HRESULT hr = ICreateTypeLib2_CreateTypeInfo(ctl, kind == 1 ? L"IPsdOne" : L"IPsdTwo", TKIND_INTERFACE, &cti);
    int ok = hr == S_OK;
    if (ok) {
        HREFTYPE href = 0;
        ICreateTypeInfo_SetGuid(cti, kind == 1 ? &psd_IID_One : &psd_IID_Two);
        ICreateTypeInfo_SetTypeFlags(cti, oleautomation ? TYPEFLAG_FOLEAUTOMATION : 0);
        ok = ICreateTypeInfo_AddRefTypeInfo(cti, unk, &href) == S_OK && ICreateTypeInfo_AddImplType(cti, 0, href) == S_OK;
        TYPEDESC lng = { { 0 }, VT_I4 };
        FUNCDESC fd; ELEMDESC el[3];
        memset(&fd, 0, sizeof fd); memset(el, 0, sizeof el);
        fd.memid = 0x60010000; fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
        fd.elemdescFunc.tdesc.vt = VT_HRESULT;
        fd.lprgelemdescParam = el;
        if (kind == 1) {
            el[0].tdesc.vt = VT_I4; el[0].paramdesc.wParamFlags = PARAMFLAG_FIN;
            el[1].tdesc.vt = VT_I4; el[1].paramdesc.wParamFlags = PARAMFLAG_FIN;
            el[2].tdesc.vt = VT_PTR; el[2].tdesc.lptdesc = &lng; el[2].paramdesc.wParamFlags = PARAMFLAG_FOUT;
            fd.cParams = 3;
            ok = ok && ICreateTypeInfo_AddFuncDesc(cti, 0, &fd) == S_OK;
        } else {
            el[0].tdesc.vt = VT_I4; el[0].paramdesc.wParamFlags = PARAMFLAG_FIN;
            el[1].tdesc.vt = VT_PTR; el[1].tdesc.lptdesc = &lng; el[1].paramdesc.wParamFlags = PARAMFLAG_FOUT;
            fd.cParams = 2;
            ok = ok && ICreateTypeInfo_AddFuncDesc(cti, 0, &fd) == S_OK;
            TYPEDESC lpw = { { 0 }, VT_BSTR };
            el[0].tdesc = lpw;
            fd.memid = 0x60010001;
            ok = ok && ICreateTypeInfo_AddFuncDesc(cti, 1, &fd) == S_OK;
        }
        ok = ok && ICreateTypeInfo_LayOut(cti) == S_OK;
        ICreateTypeInfo_Release(cti);
    }
    if (ok) ok = ICreateTypeLib2_SaveAllChanges(ctl) == S_OK;
    ICreateTypeLib2_Release(ctl);
    ITypeInfo_Release(unk); ITypeLib_Release(so);
    if (!ok) return 0;
    if (p_LoadTypeLibEx(path, REGKIND_NONE, &c->tl) != S_OK) return 0;
    if (ITypeLib_GetTypeInfoOfGuid(c->tl, kind == 1 ? &psd_IID_One : &psd_IID_Two, &c->ti) != S_OK) return 0;
    return 1;
}
static void ti_free(TiCtx *c) {
    if (c->ti) ITypeInfo_Release(c->ti);
    if (c->tl) ITypeLib_Release(c->tl);
    memset(c, 0, sizeof *c);
}
#define TI_BEGIN(c, kind, oa) TiCtx c; if (!ti_init()) { t_skip("missing exports"); return; } PSD_STA(); \
    if (!ti_build(&c, kind, oa)) { t_skip("cannot build the test type library"); ti_free(&c); PSD_DONE(); return; }

TEST(psdt_proxy_from_typeinfo_basic) {
    TI_BEGIN(c, 1, 1);
    IRpcProxyBuffer *pb = NULL; void *iface = NULL;
    HRESULT hr = p_CreateProxy(c.ti, NULL, &psd_IID_One, &pb, &iface);
    EQHR(hr, S_OK);
    if (hr == S_OK) {
        CHECK(pb != NULL && iface != NULL);
        void **vtbl = *(void ***)iface;
        for (int i = 0; i < 4; i++) CHECKF(vtbl[i] != NULL, "slot %d", i);
        EQ(IUnknown_AddRef((IUnknown *)iface), 3);           /* buffer + interface share a count */
        EQ(IUnknown_Release((IUnknown *)iface), 2);
        void *pv = NULL;
        EQHR(IRpcProxyBuffer_QueryInterface(pb, &psd_IID_One, &pv), S_OK);
        EQ(pv, iface);
        IUnknown_Release((IUnknown *)pv);
        pv = (void *)1;
        EQHR(IRpcProxyBuffer_QueryInterface(pb, &psd_IID_Two, &pv), E_NOINTERFACE);
        CHECK(pv == NULL);
        LONG out = 0;
        EQHR(((IOletOne *)iface)->lpVtbl->Add((IOletOne *)iface, 1, 2, &out), PSD_E_NOTCONNECTED);   /* not connected yet */
        IUnknown_Release((IUnknown *)iface);
        EQ(IRpcProxyBuffer_Release(pb), 0);
    }
    ti_free(&c); PSD_DONE();
}

TEST(psdt_stub_from_typeinfo_basic) {
    TI_BEGIN(c, 1, 1);
    PsdMock *srv = psd_mock_new();
    IRpcStubBuffer *sb = NULL;
    HRESULT hr = p_CreateStub(c.ti, &psd_IID_One, &srv->iface, &sb);
    EQHR(hr, S_OK);
    if (hr == S_OK && sb) {
        EQ(srv->n_qi, 1);
        CHECK(IsEqualGUID(&srv->last_iid, &psd_IID_One));
        EQ(IRpcStubBuffer_CountRefs(sb), 1);
        LONG rc0 = ((CStdStubBuffer *)sb)->RefCount;
        IRpcStubBuffer *r = IRpcStubBuffer_IsIIDSupported(sb, &psd_IID_One);
        EQ(r, sb);
        EQ(((CStdStubBuffer *)sb)->RefCount, rc0 + 1);       /* AddRef'd for the caller */
        if (r && ((CStdStubBuffer *)sb)->RefCount > rc0) IRpcStubBuffer_Release(r);
        CHECK(IRpcStubBuffer_IsIIDSupported(sb, &psd_IID_Two) == NULL);
        void *pv = NULL;
        EQHR(IRpcStubBuffer_QueryInterface(sb, &psd_IID_IRpcStubBuffer, &pv), S_OK);
        EQ(pv, sb);
        IRpcStubBuffer_Release(sb);
        IRpcStubBuffer_Disconnect(sb);
        EQ(srv->ref, 1);
        EQ(IRpcStubBuffer_Release(sb), 0);
    }
    psd_mock_free(srv);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_typeinfo_roundtrip_add) {
    TI_BEGIN(c, 1, 1);
    IRpcProxyBuffer *pb = NULL; void *iface = NULL; IRpcStubBuffer *sb = NULL;
    PsdSrv *srv = psd_srv_new();
    EQHR(p_CreateProxy(c.ti, NULL, &psd_IID_One, &pb, &iface), S_OK);
    EQHR(p_CreateStub(c.ti, &psd_IID_One, PSD_SRV_UNK(srv), &sb), S_OK);
    if (pb && sb) {
        PsdChan *ch = psd_chan_new(sb);
        EQHR(IRpcProxyBuffer_Connect(pb, &ch->iface), S_OK);
        LONG out = 0;
        EQHR(((IOletOne *)iface)->lpVtbl->Add((IOletOne *)iface, 30, 12, &out), S_OK);
        EQ(out, 42);
        EQ(srv->n_add, 1); EQ(srv->last_a, 30); EQ(srv->last_b, 12);
        if (ch->ngb) { EQ(ch->gb[0].iMethod, 3); CHECK(IsEqualGUID(&ch->gb[0].iid, &psd_IID_One)); EQ(ch->gb[0].rep, 0x10); }
        EQ(ch->req_len, 8);
        EQ(*(LONG *)ch->req, 30); EQ(*(LONG *)(ch->req + 4), 12);
        EQ(ch->rep_len, 8);
        EQ(*(LONG *)ch->rep, 42); EQ(*(LONG *)(ch->rep + 4), S_OK);
        /* negative / wrap-around values and the server's failure HRESULT */
        EQHR(((IOletOne *)iface)->lpVtbl->Add((IOletOne *)iface, -7, 3, &out), S_OK);
        EQ(out, -4);
        EQHR(((IOletOne *)iface)->lpVtbl->Add((IOletOne *)iface, 1, 0x5a5a5a5a, &out), E_FAIL);
        EQHR(((IOletOne *)iface)->lpVtbl->Add((IOletOne *)iface, 1, 2, NULL), 0x800706F4);
        EQ(ch->live_buffers, 0);
        IRpcProxyBuffer_Disconnect(pb);
        EQ(ch->ref, 1);
        HeapFree(GetProcessHeap(), 0, ch);
    }
    if (iface) IUnknown_Release((IUnknown *)iface);
    if (pb) IRpcProxyBuffer_Release(pb);
    if (sb) { IRpcStubBuffer_Disconnect(sb); IRpcStubBuffer_Release(sb); }
    srv->one.lpVtbl->Release(&srv->one);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_typeinfo_roundtrip_string) {
    TI_BEGIN(c, 2, 1);
    IRpcProxyBuffer *pb = NULL; void *iface = NULL; IRpcStubBuffer *sb = NULL;
    PsdSrv *srv = psd_srv_new();
    EQHR(p_CreateProxy(c.ti, NULL, &psd_IID_Two, &pb, &iface), S_OK);
    EQHR(p_CreateStub(c.ti, &psd_IID_Two, PSD_SRV_UNK(srv), &sb), S_OK);
    if (pb && sb) {
        PsdChan *ch = psd_chan_new(sb);
        EQHR(IRpcProxyBuffer_Connect(pb, &ch->iface), S_OK);
        IOletTwo *t = iface;
        LONG v = 0;
        EQHR(t->lpVtbl->Echo(t, 0x2468ace0, &v), S_OK);
        EQ(v, 0x2468ace0);
        if (ch->ngb) EQ(ch->gb[0].iMethod, 3);
        ch->ngb = 0;
        v = -1;
        BSTR bs = UIN("oleaut32.dll", SysAllocString) ? UIN("oleaut32.dll", SysAllocString)(L"typeinfo") : NULL;
        EQHR(t->lpVtbl->Str(t, bs, &v), S_OK);
        if (bs) UIN("oleaut32.dll", SysFreeString)(bs);
        EQ(v, 8);
        CHECK(!lstrcmpW(srv->last_str, L"typeinfo"));
        if (ch->ngb) EQ(ch->gb[0].iMethod, 4);
        IRpcProxyBuffer_Disconnect(pb);
        HeapFree(GetProcessHeap(), 0, ch);
    }
    if (iface) IUnknown_Release((IUnknown *)iface);
    if (pb) IRpcProxyBuffer_Release(pb);
    if (sb) { IRpcStubBuffer_Disconnect(sb); IRpcStubBuffer_Release(sb); }
    srv->one.lpVtbl->Release(&srv->one);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_stub_invoke_range_from_typeinfo) {
    if (t_is_wine()) { t_skip("wine dispatches out-of-range methods instead of raising"); return; }
    TI_BEGIN(c, 1, 1);
    PsdSrv *srv = psd_srv_new();
    IRpcStubBuffer *sb = NULL;
    EQHR(p_CreateStub(c.ti, &psd_IID_One, PSD_SRV_UNK(srv), &sb), S_OK);
    if (sb) {
        PsdChan *ch = psd_chan_new(sb);
        static const ULONG bad[] = { 0, 1, 2, 4, 100 };
        for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
            RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
            msg.dataRepresentation = 0x10; msg.iMethod = bad[i];
            void *args[3] = { sb, &msg, &ch->iface };
            DWORD code = 0;     /* any exception */
            int caught = 0;
            {
                /* reuse the catcher from stub.c semantics: invoke through psd_catch */
                extern void psd_invoke_thunk(void *);
                caught = psd_catch(psd_invoke_thunk, args, &code);
            }
            CHECKF(caught, "iMethod %lu did not raise", bad[i]);
            t_note("typeinfo stub Invoke(iMethod=%lu) raised 0x%lx", bad[i], code);
        }
        EQ(srv->n_add, 0);
        HeapFree(GetProcessHeap(), 0, ch);
        IRpcStubBuffer_Disconnect(sb); IRpcStubBuffer_Release(sb);
    }
    srv->one.lpVtbl->Release(&srv->one);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_proxy_from_typeinfo_outer_unknown) {
    TI_BEGIN(c, 1, 1);
    PsdMock *outer = psd_mock_new();
    outer->ref = 3;
    IRpcProxyBuffer *pb = NULL; void *iface = NULL;
    HRESULT hr = p_CreateProxy(c.ti, &outer->iface, &psd_IID_One, &pb, &iface);
    EQHR(hr, S_OK);
    if (hr == S_OK) {
        EQ(outer->ref, 4);
        EQ(IUnknown_Release((IUnknown *)iface), 3);
        EQ(outer->ref, 3);
        EQ(IRpcProxyBuffer_Release(pb), 0);
    }
    psd_mock_free(outer);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_proxy_from_typeinfo_wrong_iid) {
    TI_BEGIN(c, 1, 1);
    IRpcProxyBuffer *pb = (IRpcProxyBuffer *)1; void *iface = (void *)1;
    HRESULT hr = p_CreateProxy(c.ti, NULL, &psd_IID_Unknown1, &pb, &iface);
    t_note("CreateProxyFromTypeInfo(riid != typeinfo guid) = 0x%08lx pb=%p iface=%p", hr, pb, iface);
    if (hr == S_OK) { if (iface) IUnknown_Release((IUnknown *)iface); if (pb) IRpcProxyBuffer_Release(pb); }
    IRpcStubBuffer *sb = (IRpcStubBuffer *)1;
    PsdMock *srv = psd_mock_new();
    hr = p_CreateStub(c.ti, &psd_IID_Unknown1, &srv->iface, &sb);
    t_note("CreateStubFromTypeInfo(riid != typeinfo guid) = 0x%08lx sb=%p qi=%ld", hr, sb, srv->n_qi);
    if (hr == S_OK && sb) { IRpcStubBuffer_Disconnect(sb); IRpcStubBuffer_Release(sb); }
    psd_mock_free(srv);
    ti_free(&c); PSD_DONE();
}

TEST(psdt_proxy_from_typeinfo_not_oleautomation) {
    TI_BEGIN(c, 1, 0);
    IRpcProxyBuffer *pb = NULL; void *iface = NULL;
    HRESULT hr = p_CreateProxy(c.ti, NULL, &psd_IID_One, &pb, &iface);
    t_note("CreateProxyFromTypeInfo(interface without TYPEFLAG_FOLEAUTOMATION) = 0x%08lx", hr);
    if (hr == S_OK) { if (iface) IUnknown_Release((IUnknown *)iface); if (pb) IRpcProxyBuffer_Release(pb); }
    ti_free(&c); PSD_DONE();
}
