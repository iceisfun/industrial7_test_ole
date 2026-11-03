/* IRpcProxyBuffer and the proxy interface returned by IPSFactoryBuffer::CreateProxy (combase CStdProxyBuffer_*).
 * Facts from the Win10 combase decomp:
 *   Connect   : channel->QueryInterface(IID_IRpcChannelBuffer, &pch); on S_OK pch replaces (and releases) the old
 *               channel; on failure the QI hr is returned and nothing changes
 *   Disconnect: swaps the channel for NULL and releases it (safe when not connected)
 *   QI        : *ppv = NULL first; IUnknown/IRpcProxyBuffer -> the buffer; the proxy's IID -> the interface (AddRef);
 *               ICallFactory only for async IIDs; IReleaseMarshalBuffers when TableVersion >= 2
 *   a call on an unconnected proxy raises CO_E_OBJNOTCONNECTED, which NdrClientCall2 turns into the returned HRESULT */
#include "psd.h"

#define RIG(r, iid, conn) PsdRig r; if (!psd_rig_up(&r, iid, conn)) { t_skip("cannot build proxy rig"); psd_rig_down(&r); PSD_DONE(); return; }

TEST(psdp_create_refcounts) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    IUnknown *i = r.iface;
    EQ(IUnknown_AddRef(i), 3);                           /* proxy buffer and interface share one count */
    EQ(IRpcProxyBuffer_AddRef(r.pb), 4);
    EQ(IUnknown_Release(i), 3);
    EQ(IRpcProxyBuffer_Release(r.pb), 2);
    EQ(IUnknown_Release(i), 1);
    EQ(IRpcProxyBuffer_AddRef(r.pb), 2);
    EQ(IRpcProxyBuffer_Release(r.pb), 1);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_proxy_buffer_qi) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    const GUID *same[2] = { &psd_IID_IUnknown, &psd_IID_IRpcProxyBuffer };
    for (int i = 0; i < 2; i++) {
        void *pv = NULL;
        EQHR(IRpcProxyBuffer_QueryInterface(r.pb, same[i], &pv), S_OK);
        EQ(pv, r.pb);
        IRpcProxyBuffer_Release(r.pb);
    }
    void *pv = NULL;
    EQHR(IRpcProxyBuffer_QueryInterface(r.pb, &psd_IID_One, &pv), S_OK);
    EQ(pv, r.iface);                                      /* the proxy's own IID yields the interface pointer */
    IRpcProxyBuffer_Release(r.pb);
    const GUID *bad[6] = { &psd_IID_Two, &psd_IID_IDispatch, &psd_IID_IRpcStubBuffer, &psd_IID_IMarshal, &psd_IID_Unknown1, &psd_IID_ICallFactory };
    for (int i = 0; i < 6; i++) {
        pv = (void *)1;
        HRESULT hr = IRpcProxyBuffer_QueryInterface(r.pb, bad[i], &pv);
        CHECKF(hr == E_NOINTERFACE, "bad %d hr=0x%lx", i, hr);
        CHECK(pv == NULL);
    }
    EQ(IRpcProxyBuffer_AddRef(r.pb), 2);
    IRpcProxyBuffer_Release(r.pb);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_proxy_buffer_qi_release_marshal_buffers) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    void *pv = NULL;
    HRESULT hr = IRpcProxyBuffer_QueryInterface(r.pb, &psd_IID_IReleaseMarshalBuffers, &pv);
    EQHR(hr, S_OK);                                       /* TableVersion 2 in the ProxyFileInfo */
    if (hr == S_OK && pv) {
        CHECK(pv != (void *)r.pb);                        /* a separate interface on the same object */
        EQ(IRpcProxyBuffer_AddRef(r.pb), 4);              /* 2 after CreateProxy, +1 for the QI */
        EQ(IUnknown_Release((IUnknown *)pv), 3);          /* its Release forwards to the buffer */
        EQ(IRpcProxyBuffer_Release(r.pb), 2);
    }
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_interface_qi_identity) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    IUnknown *i = r.iface;
    void *pv = NULL;
    EQHR(IUnknown_QueryInterface(i, &psd_IID_IUnknown, &pv), S_OK);
    EQ(pv, r.pb);                                         /* identity IUnknown of the proxy object is the buffer */
    IUnknown_Release((IUnknown *)pv);
    pv = NULL;
    EQHR(IUnknown_QueryInterface(i, &psd_IID_One, &pv), S_OK);
    EQ(pv, r.iface);
    IUnknown_Release((IUnknown *)pv);
    pv = NULL;
    EQHR(IUnknown_QueryInterface(i, &psd_IID_IRpcProxyBuffer, &pv), S_OK);
    EQ(pv, r.pb);
    IUnknown_Release((IUnknown *)pv);
    pv = (void *)1;
    EQHR(IUnknown_QueryInterface(i, &psd_IID_Two, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    EQ(IUnknown_AddRef(i), 3); IUnknown_Release(i);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_outer_unknown_delegation) {
    PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    if (!psd_init() || psd_get_factory(&psf) != S_OK) { t_skip("factory"); PSD_DONE(); return; }
    PsdMock *outer = psd_mock_new();
    outer->qi_self = 0; outer->qi_hr = S_OK; outer->qi_out = (void *)0;
    IRpcProxyBuffer *pb = NULL; IUnknown *i = NULL;
    EQHR(IPSFactoryBuffer_CreateProxy(psf, &outer->iface, &psd_IID_One, &pb, (void **)&i), S_OK);
    if (pb && i) {
        LONG ar0 = outer->n_addref, rl0 = outer->n_release, q0 = outer->n_qi;
        IUnknown_AddRef(i); IUnknown_Release(i);
        EQ(outer->n_addref - ar0, 1); EQ(outer->n_release - rl0, 1);     /* interface IUnknown goes to the outer unknown */
        void *pv = NULL;
        IUnknown_QueryInterface(i, &psd_IID_IUnknown, &pv);
        EQ(outer->n_qi - q0, 1);                                         /* QueryInterface too */
        CHECK(IsEqualGUID(&outer->last_iid, &psd_IID_IUnknown));
        /* the proxy buffer keeps its own, non-delegating count */
        EQ(IRpcProxyBuffer_AddRef(pb), 2);
        EQ(IRpcProxyBuffer_Release(pb), 1);
        IUnknown_Release(i);   /* goes to the outer */
        EQ(IRpcProxyBuffer_Release(pb), 0);
    }
    psd_mock_free(outer);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdp_connect_uses_queryinterface) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    PsdChan *c = r.chan;
    EQ(c->ref, 1);
    EQHR(IRpcProxyBuffer_Connect(r.pb, &c->iface), S_OK); r.connected = 1;
    EQ(c->n_qi, 1);                                       /* the proxy QI()s the channel for IRpcChannelBuffer */
    EQ(c->ref, 2);                                        /* and keeps that reference */
    EQ(c->n_getbuf, 0); EQ(c->n_sendrecv, 0); EQ(c->n_isconn, 0);   /* nothing else is called at connect time */
    IRpcProxyBuffer_Disconnect(r.pb); r.connected = 0;
    EQ(c->ref, 1);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_connect_qi_failure) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    r.chan->hr_qi = 0x8abc0002;
    EQHR(IRpcProxyBuffer_Connect(r.pb, &r.chan->iface), 0x8abc0002);   /* QI failure is returned and nothing is connected */
    EQ(r.chan->ref, 1);
    LONG out = 77;
    HRESULT hr = ((IOletOne *)r.iface)->lpVtbl->Add((IOletOne *)r.iface, 1, 2, &out);
    EQHR(hr, PSD_E_NOTCONNECTED);
    r.chan->hr_qi = S_OK;
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_connect_replaces_channel) {
    PSD_STA(); RIG(r, &psd_IID_One, 1);
    PsdChan *c2 = psd_chan_new(r.sb);
    EQ(r.chan->ref, 2);
    EQHR(IRpcProxyBuffer_Connect(r.pb, &c2->iface), S_OK);
    EQ(r.chan->ref, 1);                                   /* the previous channel is released */
    EQ(c2->ref, 2);
    LONG out = 0;
    EQHR(((IOletOne *)r.iface)->lpVtbl->Add((IOletOne *)r.iface, 5, 6, &out), S_OK);
    EQ(out, 11);
    EQ(r.chan->n_getbuf, 0);                              /* calls use the new channel */
    EQ(c2->n_getbuf >= 1, 1);
    IRpcProxyBuffer_Disconnect(r.pb);
    EQ(c2->ref, 1);
    r.connected = 0;
    HeapFree(GetProcessHeap(), 0, c2);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_disconnect_semantics) {
    PSD_STA(); RIG(r, &psd_IID_One, 0);
    if (!t_is_wine()) IRpcProxyBuffer_Disconnect(r.pb);   /* not connected: harmless on Windows (wine dereferences NULL) */
    EQ(r.chan->ref, 1);
    EQHR(IRpcProxyBuffer_Connect(r.pb, &r.chan->iface), S_OK); r.connected = 1;
    IRpcProxyBuffer_Disconnect(r.pb);
    if (!t_is_wine()) IRpcProxyBuffer_Disconnect(r.pb);   /* idempotent on Windows */
    EQ(r.chan->ref, 1);
    LONG out = 99;
    HRESULT hr = ((IOletOne *)r.iface)->lpVtbl->Add((IOletOne *)r.iface, 1, 1, &out);
    EQHR(hr, PSD_E_NOTCONNECTED);                        /* CO_E_OBJNOTCONNECTED */
    EQ(r.chan->n_getbuf, 0);
    r.connected = 0;
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_call_unconnected) {
    PSD_STA(); RIG(r, &psd_IID_Two, 0);
    IOletTwo *t = r.iface;
    LONG v = 0; IUnknown *u = NULL;
    EQHR(t->lpVtbl->Echo(t, 1, &v), PSD_E_NOTCONNECTED);
    EQHR(t->lpVtbl->Str(t, L"x", &v), PSD_E_NOTCONNECTED);
    EQHR(t->lpVtbl->GetObj(t, &psd_IID_IUnknown, &u), PSD_E_NOTCONNECTED);
    EQHR(t->lpVtbl->PutObj(t, NULL), PSD_E_NOTCONNECTED);
    EQ(r.srv->n_echo + r.srv->n_str + r.srv->n_getobj + r.srv->n_putobj, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdp_final_release_disconnects) {
    PSD_STA();
    IPSFactoryBuffer *psf = NULL;
    if (!psd_init() || psd_get_factory(&psf) != S_OK) { t_skip("factory"); PSD_DONE(); return; }
    IRpcProxyBuffer *pb = NULL; IUnknown *i = NULL;
    EQHR(IPSFactoryBuffer_CreateProxy(psf, NULL, &psd_IID_One, &pb, (void **)&i), S_OK);
    PsdChan *c = psd_chan_new(NULL);
    EQHR(IRpcProxyBuffer_Connect(pb, &c->iface), S_OK);
    EQ(c->ref, 2);
    IUnknown_Release(i);
    EQ(c->ref, 2);
    EQ(IRpcProxyBuffer_Release(pb), 0);
    EQ(c->ref, 1);                                         /* destroying a connected proxy releases the channel */
    HeapFree(GetProcessHeap(), 0, c);
    IPSFactoryBuffer_Release(psf);
    PSD_DONE();
}

TEST(psdp_stubless_vtbl_slots) {
    PSD_STA(); RIG(r, &psd_IID_Two, 0);
    void **vtbl = *(void ***)r.iface;
    EQ(vtbl, psd_proxy_vtbl[1]->Vtbl);
    for (int i = 0; i < 7; i++) CHECKF(vtbl[i] != NULL && vtbl[i] != (void *)-1, "slot %d = %p", i, vtbl[i]);
    /* the three IUnknown slots are the rpcrt4 proxy helpers, the method slots are distinct thunks */
    t_note("IUnknown slots: %p %p %p (rpcrt4 helpers %p %p %p)", vtbl[0], vtbl[1], vtbl[2], t_proc_in("rpcrt4.dll", "IUnknown_QueryInterface_Proxy"),
           t_proc_in("rpcrt4.dll", "IUnknown_AddRef_Proxy"), t_proc_in("rpcrt4.dll", "IUnknown_Release_Proxy"));
    for (int i = 3; i < 7; i++) for (int j = i + 1; j < 7; j++) CHECKF(vtbl[i] != vtbl[j], "slots %d and %d share a thunk", i, j);
    for (int i = 3; i < 7; i++) {
        char nm[40]; wsprintfA(nm, "ObjectStublessClient%d", i);
        void *o = (void *)t_proc_in("combase.dll", nm), *o2 = (void *)t_proc_in("ole32.dll", nm);
        t_note("slot %d = %p; combase %s = %p, ole32 = %p", i, vtbl[i], nm, o, o2);
    }
    psd_rig_down(&r); PSD_DONE();
}
