/* A 30-method interface (vtable slots 3..32): every ObjectStublessClientN thunk, and the stub dispatch of method N.
 * Generated pattern: HRESULT M<N>(long a, long *r) { *r = a * 3 + N; return S_OK; }  (proc format cloned from IOletTwo::Echo). */
#include "psd.h"

static const GUID IID_Big = {0x6f7a0005, 0x4a5b, 0x4c6d, {0x8e, 0x7f, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f}};
#define NMETH 30
#define ECHO_OFF 50
#define ECHO_LEN 44

typedef struct BigObj { const void *const *vtbl; LONG ref; LONG calls[33]; } BigObj;
static BigObj *bigfrom(void *p) { return (BigObj *)p; }
static HRESULT WINAPI big_qi(void *p, REFIID riid, void **ppv) {
    if (IsEqualGUID(riid, &psd_IID_IUnknown) || IsEqualGUID(riid, &IID_Big)) { *ppv = p; InterlockedIncrement(&bigfrom(p)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI big_addref(void *p) { return InterlockedIncrement(&bigfrom(p)->ref); }
static ULONG WINAPI big_release(void *p) { return InterlockedDecrement(&bigfrom(p)->ref); }
static HRESULT WINAPI big_m3(void *p, LONG a, LONG *r) { bigfrom(p)->calls[3]++; *r = a * 3 + 3; return S_OK; }
static HRESULT WINAPI big_m4(void *p, LONG a, LONG *r) { bigfrom(p)->calls[4]++; *r = a * 3 + 4; return S_OK; }
static HRESULT WINAPI big_m5(void *p, LONG a, LONG *r) { bigfrom(p)->calls[5]++; *r = a * 3 + 5; return S_OK; }
static HRESULT WINAPI big_m6(void *p, LONG a, LONG *r) { bigfrom(p)->calls[6]++; *r = a * 3 + 6; return S_OK; }
static HRESULT WINAPI big_m7(void *p, LONG a, LONG *r) { bigfrom(p)->calls[7]++; *r = a * 3 + 7; return S_OK; }
static HRESULT WINAPI big_m8(void *p, LONG a, LONG *r) { bigfrom(p)->calls[8]++; *r = a * 3 + 8; return S_OK; }
static HRESULT WINAPI big_m9(void *p, LONG a, LONG *r) { bigfrom(p)->calls[9]++; *r = a * 3 + 9; return S_OK; }
static HRESULT WINAPI big_m10(void *p, LONG a, LONG *r) { bigfrom(p)->calls[10]++; *r = a * 3 + 10; return S_OK; }
static HRESULT WINAPI big_m11(void *p, LONG a, LONG *r) { bigfrom(p)->calls[11]++; *r = a * 3 + 11; return S_OK; }
static HRESULT WINAPI big_m12(void *p, LONG a, LONG *r) { bigfrom(p)->calls[12]++; *r = a * 3 + 12; return S_OK; }
static HRESULT WINAPI big_m13(void *p, LONG a, LONG *r) { bigfrom(p)->calls[13]++; *r = a * 3 + 13; return S_OK; }
static HRESULT WINAPI big_m14(void *p, LONG a, LONG *r) { bigfrom(p)->calls[14]++; *r = a * 3 + 14; return S_OK; }
static HRESULT WINAPI big_m15(void *p, LONG a, LONG *r) { bigfrom(p)->calls[15]++; *r = a * 3 + 15; return S_OK; }
static HRESULT WINAPI big_m16(void *p, LONG a, LONG *r) { bigfrom(p)->calls[16]++; *r = a * 3 + 16; return S_OK; }
static HRESULT WINAPI big_m17(void *p, LONG a, LONG *r) { bigfrom(p)->calls[17]++; *r = a * 3 + 17; return S_OK; }
static HRESULT WINAPI big_m18(void *p, LONG a, LONG *r) { bigfrom(p)->calls[18]++; *r = a * 3 + 18; return S_OK; }
static HRESULT WINAPI big_m19(void *p, LONG a, LONG *r) { bigfrom(p)->calls[19]++; *r = a * 3 + 19; return S_OK; }
static HRESULT WINAPI big_m20(void *p, LONG a, LONG *r) { bigfrom(p)->calls[20]++; *r = a * 3 + 20; return S_OK; }
static HRESULT WINAPI big_m21(void *p, LONG a, LONG *r) { bigfrom(p)->calls[21]++; *r = a * 3 + 21; return S_OK; }
static HRESULT WINAPI big_m22(void *p, LONG a, LONG *r) { bigfrom(p)->calls[22]++; *r = a * 3 + 22; return S_OK; }
static HRESULT WINAPI big_m23(void *p, LONG a, LONG *r) { bigfrom(p)->calls[23]++; *r = a * 3 + 23; return S_OK; }
static HRESULT WINAPI big_m24(void *p, LONG a, LONG *r) { bigfrom(p)->calls[24]++; *r = a * 3 + 24; return S_OK; }
static HRESULT WINAPI big_m25(void *p, LONG a, LONG *r) { bigfrom(p)->calls[25]++; *r = a * 3 + 25; return S_OK; }
static HRESULT WINAPI big_m26(void *p, LONG a, LONG *r) { bigfrom(p)->calls[26]++; *r = a * 3 + 26; return S_OK; }
static HRESULT WINAPI big_m27(void *p, LONG a, LONG *r) { bigfrom(p)->calls[27]++; *r = a * 3 + 27; return S_OK; }
static HRESULT WINAPI big_m28(void *p, LONG a, LONG *r) { bigfrom(p)->calls[28]++; *r = a * 3 + 28; return S_OK; }
static HRESULT WINAPI big_m29(void *p, LONG a, LONG *r) { bigfrom(p)->calls[29]++; *r = a * 3 + 29; return S_OK; }
static HRESULT WINAPI big_m30(void *p, LONG a, LONG *r) { bigfrom(p)->calls[30]++; *r = a * 3 + 30; return S_OK; }
static HRESULT WINAPI big_m31(void *p, LONG a, LONG *r) { bigfrom(p)->calls[31]++; *r = a * 3 + 31; return S_OK; }
static HRESULT WINAPI big_m32(void *p, LONG a, LONG *r) { bigfrom(p)->calls[32]++; *r = a * 3 + 32; return S_OK; }
static const void *const big_vtbl[33] = { (void *)big_qi, (void *)big_addref, (void *)big_release,
    (void *)big_m3,
    (void *)big_m4,
    (void *)big_m5,
    (void *)big_m6,
    (void *)big_m7,
    (void *)big_m8,
    (void *)big_m9,
    (void *)big_m10,
    (void *)big_m11,
    (void *)big_m12,
    (void *)big_m13,
    (void *)big_m14,
    (void *)big_m15,
    (void *)big_m16,
    (void *)big_m17,
    (void *)big_m18,
    (void *)big_m19,
    (void *)big_m20,
    (void *)big_m21,
    (void *)big_m22,
    (void *)big_m23,
    (void *)big_m24,
    (void *)big_m25,
    (void *)big_m26,
    (void *)big_m27,
    (void *)big_m28,
    (void *)big_m29,
    (void *)big_m30,
    (void *)big_m31,
    (void *)big_m32
};

/* ---- proxy/stub file for IOletBig ---------------------------------------------------------------- */
static CStdPSFactoryBuffer big_psf;
static CINTERFACE_PROXY_VTABLE(33) big_pv;
static CInterfaceStubVtbl big_sv;
static CInterfaceProxyVtbl *big_pvl[2];
static CInterfaceStubVtbl *big_svl[2];
static MIDL_STUBLESS_PROXY_INFO big_pinfo;
static MIDL_SERVER_INFO big_sinfo;
static unsigned short big_offs[NMETH];
static BYTE big_proc[ECHO_LEN * NMETH];
static const char *big_names[2] = { "IOletBig", 0 };
static int WINAPI big_lookup(const IID *iid, int *idx) { if (IsEqualGUID(iid, &IID_Big)) { *idx = 0; return 1; } return 0; }
static ExtendedProxyFileInfo big_pfi = { (const PCInterfaceProxyVtblList *)big_pvl, (const PCInterfaceStubVtblList *)big_svl,
    (const PCInterfaceName *)big_names, 0, big_lookup, 1, 2, 0, 0, 0, 0 };
static const ProxyFileInfo *big_list[2] = { &big_pfi, 0 };
static int big_ready;

static ULONG WINAPI big_stub_release(IRpcStubBuffer *This) {
    return UIN("rpcrt4.dll", NdrCStdStubBuffer_Release)(This, (IPSFactoryBuffer *)&big_psf);
}
static int big_init(void) {
    if (big_ready) return big_ready > 0;
    big_ready = -1;
    if (!psd_init()) return 0;
    extern const unsigned char *psd_echo_template(void);
    const unsigned char *echo = psd_echo_template();
    for (int i = 0; i < NMETH; i++) {
        BYTE *d = big_proc + i * ECHO_LEN;
        memcpy(d, echo, ECHO_LEN);
        d[6] = (BYTE)(i + 3); d[7] = 0;                      /* proc number */
        big_offs[i] = (unsigned short)(i * ECHO_LEN);
    }
    void *qi = (void *)t_proc_in("rpcrt4.dll", "IUnknown_QueryInterface_Proxy");
    void *ar = (void *)t_proc_in("rpcrt4.dll", "IUnknown_AddRef_Proxy");
    void *rl = (void *)t_proc_in("rpcrt4.dll", "IUnknown_Release_Proxy");
    big_pinfo.pStubDesc = &psd_stub_desc; big_pinfo.ProcFormatString = big_proc; big_pinfo.FormatStringOffset = &big_offs[-3];
    big_pv.header.pStublessProxyInfo = &big_pinfo; big_pv.header.piid = &IID_Big;
    big_pv.Vtbl[0] = qi; big_pv.Vtbl[1] = ar; big_pv.Vtbl[2] = rl;
    for (int i = 3; i < 33; i++) big_pv.Vtbl[i] = (void *)-1;
    big_sinfo.pStubDesc = &psd_stub_desc; big_sinfo.ProcString = big_proc; big_sinfo.FmtStringOffset = &big_offs[-3];
    big_sv.header.piid = &IID_Big; big_sv.header.pServerInfo = &big_sinfo; big_sv.header.DispatchTableCount = 33; big_sv.header.pDispatchTable = 0;
    big_sv.Vtbl = psd_stub_vtbl[0]->Vtbl;
    big_sv.Vtbl.Release = big_stub_release;
    big_pvl[0] = (CInterfaceProxyVtbl *)&big_pv; big_pvl[1] = 0;
    big_svl[0] = &big_sv; big_svl[1] = 0;
    big_ready = 1;
    return 1;
}

typedef struct BigRig { IPSFactoryBuffer *psf; IRpcProxyBuffer *pb; void **iface; IRpcStubBuffer *sb; PsdChan *chan; BigObj obj; } BigRig;
static int big_up(BigRig *r) {
    memset(r, 0, sizeof *r);
    r->obj.vtbl = big_vtbl; r->obj.ref = 1;
    if (!big_init()) return 0;
    if (UIN("rpcrt4.dll", NdrDllGetClassObject)(&IID_Big, &psd_IID_IPSFactoryBuffer, (void **)&r->psf, big_list, NULL, &big_psf) != S_OK) return 0;
    if (IPSFactoryBuffer_CreateStub(r->psf, &IID_Big, (IUnknown *)&r->obj, &r->sb) != S_OK) return 0;
    if (IPSFactoryBuffer_CreateProxy(r->psf, NULL, &IID_Big, &r->pb, (void **)&r->iface) != S_OK) return 0;
    r->chan = psd_chan_new(r->sb);
    return IRpcProxyBuffer_Connect(r->pb, &r->chan->iface) == S_OK;
}
static void big_down(BigRig *r) {
    if (r->pb) IRpcProxyBuffer_Disconnect(r->pb);
    if (r->iface) ((IUnknown *)r->iface)->lpVtbl->Release((IUnknown *)r->iface);
    if (r->pb) IRpcProxyBuffer_Release(r->pb);
    if (r->sb) { IRpcStubBuffer_Disconnect(r->sb); IRpcStubBuffer_Release(r->sb); }
    if (r->psf) IPSFactoryBuffer_Release(r->psf);
    if (r->chan) HeapFree(GetProcessHeap(), 0, r->chan);
}
typedef HRESULT (WINAPI *meth_fn)(void *, LONG, LONG *);

TEST(psdb_all_method_slots_dispatch) {
    PSD_STA();
    BigRig r;
    if (!big_up(&r)) { t_skip("cannot build the 30-method rig"); big_down(&r); PSD_DONE(); return; }
    void **vtbl = *r.iface;
    int bad = 0;
    for (int n = 3; n <= 32; n++) {
        LONG out = -1;
        r.chan->ngb = 0;
        HRESULT hr = ((meth_fn)vtbl[n])(r.iface, 100 + n, &out);
        if (hr != S_OK || out != (100 + n) * 3 + n) { bad++; t_check(0, __FILE__, __LINE__, "slot call", "slot %d: hr=0x%lx out=%ld want %d", n, hr, out, (100 + n) * 3 + n); }
        if (r.chan->ngb < 1 || (r.chan->gb[0].iMethod & 0x7fff) != (ULONG)n) { bad++; t_check(0, __FILE__, __LINE__, "iMethod", "slot %d reached the channel as iMethod %lu", n, r.chan->ngb ? r.chan->gb[0].iMethod : 9999); }
        if (r.obj.calls[n] != 1) { bad++; t_check(0, __FILE__, __LINE__, "server", "slot %d: server method ran %ld times", n, r.obj.calls[n]); }
    }
    EQ(bad, 0);
    EQ(r.chan->live_buffers, 0);
    big_down(&r);
    PSD_DONE();
}

TEST(psdb_slots_are_stubless_thunks) {
    PSD_STA();
    BigRig r;
    if (!big_up(&r)) { t_skip("cannot build the 30-method rig"); big_down(&r); PSD_DONE(); return; }
    void **vtbl = *r.iface;
    EQ(vtbl, big_pv.Vtbl);
    int same = 0, distinct = 0;
    for (int n = 3; n <= 32; n++) {
        CHECKF(vtbl[n] != NULL && vtbl[n] != (void *)-1, "slot %d = %p", n, vtbl[n]);
        for (int m = n + 1; m <= 32; m++) if (vtbl[n] != vtbl[m]) distinct++;
        char nm[40]; wsprintfA(nm, "ObjectStublessClient%d", n);
        if (vtbl[n] == (void *)t_proc_in("combase.dll", nm)) same++;
    }
    EQ(distinct, 30 * 29 / 2);                              /* all 30 method slots are different thunks */
    t_note("%d of 30 proxy slots equal the combase ObjectStublessClientN exports", same);
    big_down(&r);
    PSD_DONE();
}

TEST(psdb_stub_dispatch_table_range) {
    PSD_STA();
    BigRig r;
    if (!big_up(&r)) { t_skip("cannot build the 30-method rig"); big_down(&r); PSD_DONE(); return; }
    EQ(big_sv.header.DispatchTableCount, 33);
    /* method 33 is one past the end: out of range for the stub */
    if (!t_is_wine()) {
        extern void psd_invoke_thunk(void *);
        RPCOLEMESSAGE msg; memset(&msg, 0, sizeof msg);
        msg.iMethod = 33; msg.dataRepresentation = 0x10;
        void *args[3] = { r.sb, &msg, &r.chan->iface };
        DWORD code = 0x6d1;
        CHECK(psd_catch(psd_invoke_thunk, args, &code));
        EQ(code, 0x6d1);
    }
    big_down(&r);
    PSD_DONE();
}

TEST(psdb_repeat_each_slot) {
    PSD_STA();
    BigRig r;
    if (!big_up(&r)) { t_skip("cannot build the 30-method rig"); big_down(&r); PSD_DONE(); return; }
    void **vtbl = *r.iface;
    int bad = 0;
    for (int it = 0; it < 20; it++)
        for (int n = 32; n >= 3; n--) {
            LONG out = 0;
            if (((meth_fn)vtbl[n])(r.iface, it - 10, &out) != S_OK || out != (it - 10) * 3 + n) bad++;
        }
    EQ(bad, 0);
    for (int n = 3; n <= 32; n++) EQ(r.obj.calls[n], 20);
    big_down(&r);
    PSD_DONE();
}
