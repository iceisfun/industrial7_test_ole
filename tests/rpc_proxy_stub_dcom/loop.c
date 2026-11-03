/* End-to-end proxy -> IRpcChannelBuffer -> stub -> server round trips through the loopback channel, checking the
 * NDR wire bytes the proxy marshals and the stub replies with. */
#include "psd.h"

#define RIGC(r, iid) PsdRig r; if (!psd_rig_up(&r, iid, 1)) { t_skip("cannot build rig"); psd_rig_down(&r); PSD_DONE(); return; }
#define ONE(r) ((IOletOne *)(r).iface)
#define TWO(r) ((IOletTwo *)(r).iface)

TEST(psdl_add_roundtrip_bytes) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    LONG out = 0x11111111;
    HRESULT hr = ONE(r)->lpVtbl->Add(ONE(r), 3, 4, &out);
    EQHR(hr, S_OK);
    EQ(out, 7);
    EQ(r.srv->n_add, 1); EQ(r.srv->last_a, 3); EQ(r.srv->last_b, 4);
    /* channel traffic */
    EQ(r.chan->n_getbuf, 2);                                 /* request buffer (client) + reply buffer (stub) */
    EQ(r.chan->n_sendrecv, 1);
    EQ(r.chan->n_freebuf, 1);                                /* the proxy frees the reply buffer */
    EQ(r.chan->live_buffers, 0);
    EQ(r.chan->n_isconn, 0);
    if (r.chan->ngb >= 2) {
        CHECK(!r.chan->gb[0].server && r.chan->gb[1].server);
        EQ(r.chan->gb[0].iMethod, 3);                        /* RPC_FLAGS_VALID_BIT is not passed to the channel */
        EQ(r.chan->gb[0].rep, 0x10);
        EQ(r.chan->gb[0].cb >= 8 && r.chan->gb[0].cb % 4 == 0, 1);
        CHECK(IsEqualGUID(&r.chan->gb[0].iid, &psd_IID_One));
        CHECK(IsEqualGUID(&r.chan->gb[1].iid, &psd_IID_One));
        EQ(r.chan->gb[1].rep, 0x10);
        EQ(r.chan->gb[1].cb >= 8, 1);
    }
    /* wire bytes: request = a, b (two little-endian longs); reply = [out] r then the HRESULT */
    static const BYTE req[8] = { 3,0,0,0, 4,0,0,0 };
    static const BYTE rep[8] = { 7,0,0,0, 0,0,0,0 };
    EQ(r.chan->req_len, 8);
    CHECK(!memcmp(r.chan->req, req, 8));
    EQ(r.chan->rep_len, 8);
    CHECK(!memcmp(r.chan->rep, rep, 8));
    EQHR(r.chan->last_invoke_hr, S_OK);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_add_values_table) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    static const struct { LONG a, b; } t[] = {
        {0,0}, {1,1}, {-1,1}, {-5,-6}, {0x7fffffff,0}, {(LONG)0x80000000,0}, {0x7fffffff,-1}, {(LONG)0x80000000,-1},
        {0x12345678,0x01010101}, {100000,-100000}, {(LONG)0xdeadbeef,(LONG)0xcafebabe}, {42,-42}, {7,0x7ffffff0},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        LONG out = 0x55555555;
        HRESULT hr = ONE(r)->lpVtbl->Add(ONE(r), t[i].a, t[i].b, &out);
        LONG want = (LONG)((ULONG)t[i].a + (ULONG)t[i].b);
        CHECKF(hr == S_OK && out == want, "Add(%ld,%ld): hr=0x%lx out=%ld want %ld", t[i].a, t[i].b, hr, out, want);
        CHECKF(r.chan->req_len == 8 && *(LONG *)r.chan->req == t[i].a && *(LONG *)(r.chan->req + 4) == t[i].b, "row %u request bytes", i);
        CHECKF(*(LONG *)r.chan->rep == want, "row %u reply bytes", i);
    }
    EQ(r.srv->n_add, (LONG)(sizeof t / sizeof *t));
    EQ(r.chan->live_buffers, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_add_repeat_many) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        LONG out = -1;
        if (ONE(r)->lpVtbl->Add(ONE(r), i, 3 * i, &out) != S_OK || out != 4 * i) bad++;
    }
    EQ(bad, 0);
    EQ(r.srv->n_add, 2000);
    EQ(r.chan->live_buffers, 0);
    EQ(r.chan->ref, 2);                                      /* no channel reference leaked by the calls */
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_server_failure_hresult) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    LONG out = 0x7777;
    HRESULT hr = ONE(r)->lpVtbl->Add(ONE(r), 1, 0x5a5a5a5a, &out);
    EQHR(hr, E_FAIL);                                        /* the server's HRESULT is the return value of the proxy call */
    EQ(r.srv->n_add, 1);
    EQ(r.chan->live_buffers, 0);
    EQ(r.chan->rep_len >= 4, 1);
    /* the reply always ends with the HRESULT */
    EQ(*(LONG *)(r.chan->rep + r.chan->rep_len - 4), E_FAIL);
    t_note("[out] long after a failing call: %ld (reply length %lu)", out, r.chan->rep_len);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_null_out_pointer) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    HRESULT hr = ONE(r)->lpVtbl->Add(ONE(r), 1, 2, NULL);
    EQHR(hr, 0x800706F4);                                    /* RPC_X_NULL_REF_POINTER (1780) mapped by NdrProxyErrorHandler */
    t_note("GetBuffer calls before the NULL [out] ref pointer was rejected: %d", r.chan->n_getbuf);
    EQ(r.srv->n_add, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_echo_two) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    LONG out = 0;
    EQHR(TWO(r)->lpVtbl->Echo(TWO(r), 0x1234abcd, &out), S_OK);
    EQ(out, 0x1234abcd);
    EQ(r.srv->n_echo, 1);
    if (r.chan->ngb) EQ(r.chan->gb[0].iMethod, 3);
    EQ(r.chan->req_len, 4);
    EQ(*(LONG *)r.chan->req, 0x1234abcd);
    EQ(r.chan->rep_len, 8);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_string_wire_format) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    LONG len = -1;
    EQHR(TWO(r)->lpVtbl->Str(TWO(r), L"hello", &len), S_OK);
    EQ(len, 5);
    EQ(r.srv->n_str, 1);
    CHECK(!lstrcmpW(r.srv->last_str, L"hello"));
    if (r.chan->ngb) EQ(r.chan->gb[0].iMethod, 4);
    /* [string] wchar_t*: conformant-varying array: max count, offset, actual count (all counting the NUL), then the characters */
    static const BYTE exp[] = { 6,0,0,0,  0,0,0,0,  6,0,0,0,  'h',0,'e',0,'l',0,'l',0,'o',0, 0,0 };
    EQ(r.chan->req_len, sizeof exp);
    CHECK(r.chan->req_len >= sizeof exp && !memcmp(r.chan->req, exp, sizeof exp));
    EQ(r.chan->rep_len, 8);
    EQ(*(LONG *)r.chan->rep, 5);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_string_sizes) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    static const int lens[] = { 0, 1, 2, 3, 7, 62, 63, 64, 100, 1000, 4000 };
    WCHAR *s = HeapAlloc(GetProcessHeap(), 0, 8192 * sizeof(WCHAR));
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        int n = lens[i];
        for (int k = 0; k < n; k++) s[k] = (WCHAR)('a' + k % 26);
        s[n] = 0;
        LONG len = -1;
        HRESULT hr = TWO(r)->lpVtbl->Str(TWO(r), s, &len);
        CHECKF(hr == S_OK && len == n, "len %d: hr=0x%lx got %ld", n, hr, len);
        CHECKF(r.srv->last_strlen == n, "server saw %ld chars want %d", r.srv->last_strlen, n);
        /* request: three counts + (n+1) wide characters */
        CHECKF(r.chan->req_len == 12 + 2 * (ULONG)(n + 1), "len %d: request %lu bytes", n, r.chan->req_len);
        if (r.chan->req_len >= 12) CHECKF(((LONG *)r.chan->req)[0] == n + 1 && ((LONG *)r.chan->req)[1] == 0 && ((LONG *)r.chan->req)[2] == n + 1, "len %d: counts", n);
        if (n < 63) CHECKF(!memcmp(r.srv->last_str, s, 2 * (size_t)n), "len %d: content", n);
    }
    HeapFree(GetProcessHeap(), 0, s);
    EQ(r.chan->live_buffers, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_string_unicode_content) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    static const WCHAR s[] = { 0x00e9, 0x4e2d, 0xd83d, 0xde00, 0x0001, 0xffff, 0 };   /* includes a surrogate pair */
    LONG len = 0;
    EQHR(TWO(r)->lpVtbl->Str(TWO(r), s, &len), S_OK);
    EQ(len, 6);
    CHECK(!memcmp(r.srv->last_str, s, sizeof s));
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_string_null_ref_pointer) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    LONG len = 0;
    EQHR(TWO(r)->lpVtbl->Str(TWO(r), NULL, &len), 0x800706F4);   /* [in] ref pointer is NULL -> RPC_X_NULL_REF_POINTER */
    EQ(r.chan->n_sendrecv, 0);
    EQ(r.srv->n_str, 0);
    EQHR(TWO(r)->lpVtbl->Str(TWO(r), L"x", NULL), 0x800706F4);
    EQ(r.srv->n_str, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_method_numbers) {
    PSD_STA(); RIGC(r, &psd_IID_Two);
    LONG v;
    r.chan->ngb = 0;
    TWO(r)->lpVtbl->Echo(TWO(r), 1, &v);
    TWO(r)->lpVtbl->Str(TWO(r), L"a", &v);
    r.chan->ngb = 0; /* keep the first two calls out of the log; log only the client GetBuffer of the next two */
    TWO(r)->lpVtbl->Echo(TWO(r), 2, &v);
    ULONG m1 = r.chan->ngb ? r.chan->gb[0].iMethod : 99;
    r.chan->ngb = 0;
    TWO(r)->lpVtbl->Str(TWO(r), L"bb", &v);
    ULONG m2 = r.chan->ngb ? r.chan->gb[0].iMethod : 99;
    EQ(m1, 3); EQ(m2, 4);
    EQ(r.srv->n_echo, 2); EQ(r.srv->n_str, 2);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_channel_getbuffer_failure_no_server_call) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    r.chan->hr_getbuf = E_OUTOFMEMORY;
    LONG out = 5;
    EQHR(ONE(r)->lpVtbl->Add(ONE(r), 1, 2, &out), E_OUTOFMEMORY);
    EQ(r.chan->n_sendrecv, 0);
    EQ(r.srv->n_add, 0);
    r.chan->hr_getbuf = S_OK;
    EQHR(ONE(r)->lpVtbl->Add(ONE(r), 1, 2, &out), S_OK);    /* the proxy is still usable afterwards */
    EQ(out, 3);
    EQ(r.chan->live_buffers, 0);
    psd_rig_down(&r); PSD_DONE();
}

TEST(psdl_channel_sendreceive_failure) {
    PSD_STA(); RIGC(r, &psd_IID_One);
    r.chan->hr_sendrecv = (HRESULT)0x80010108;               /* RPC_E_DISCONNECTED */
    LONG out = 5;
    EQHR(ONE(r)->lpVtbl->Add(ONE(r), 1, 2, &out), (HRESULT)0x80010108);
    EQ(r.chan->n_sendrecv, 1);
    EQ(r.srv->n_add, 0);
    t_note("channel buffers not freed after a failed SendReceive: %ld (FreeBuffer calls %d)", r.chan->live_buffers, r.chan->n_freebuf);
    r.chan->live_buffers = 0;
    r.chan->hr_sendrecv = S_OK;
    EQHR(ONE(r)->lpVtbl->Add(ONE(r), 4, 5, &out), S_OK);
    EQ(out, 9);
    psd_rig_down(&r); PSD_DONE();
}

/* ---- interface pointers: marshaled through the standard marshaler ---------------------------- */
static int ps_up(DWORD *cookie, IPSFactoryBuffer **psf) { return psd_register_ps(cookie, psf); }

TEST(psdl_putobj_marshals_interface_pointer) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !ps_up(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    {
        RIGC(r, &psd_IID_Two);
        PsdSrv *obj = psd_srv_new();
        EQHR(TWO(r)->lpVtbl->PutObj(TWO(r), &obj->one), S_OK);
        EQ(r.srv->n_putobj, 1);
        EQ(r.srv->last_put, &obj->one);                      /* unmarshaled in the same apartment: the original pointer */
        EQ(r.chan->n_destctx >= 1, 1);                       /* the destination context decides the marshaling flavour */
        /* the request carries an OBJREF ('MEOW') for the object */
        int found = 0;
        for (ULONG i = 0; i + 4 <= r.chan->req_len && i + 4 <= PSD_LOG; i++) if (*(DWORD *)(r.chan->req + i) == 0x574f454d) found = 1;
        CHECKF(found, "no OBJREF signature in %lu request bytes", r.chan->req_len);
        EQ(r.chan->rep_len, 4);                              /* only the HRESULT comes back */
        t_note("object refcount after PutObj: %ld; request %lu bytes", obj->ref, r.chan->req_len);
        psd_rig_down(&r);
        obj->one.lpVtbl->Release(&obj->one);
    }
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdl_putobj_null_pointer) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !ps_up(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    {
        RIGC(r, &psd_IID_Two);
        EQHR(TWO(r)->lpVtbl->PutObj(TWO(r), NULL), S_OK);    /* a NULL interface pointer is legal and marshals as NULL */
        EQ(r.srv->n_putobj, 1);
        CHECK(r.srv->last_put == NULL);
        EQ(r.chan->req_len >= 4, 1);
        t_note("request length for a NULL interface pointer: %lu", r.chan->req_len);
        psd_rig_down(&r);
    }
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdl_getobj_unmarshals_interface_pointer) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !ps_up(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    {
        RIGC(r, &psd_IID_Two);
        IUnknown *got = NULL;
        EQHR(TWO(r)->lpVtbl->GetObj(TWO(r), &psd_IID_One, &got), S_OK);
        EQ(r.srv->n_getobj, 1);
        EQ(got, (IUnknown *)&r.srv->one);                    /* same apartment: the server's own pointer comes back */
        if (got) IUnknown_Release(got);
        /* iid_is(riid): the requested IID selects the interface that is marshaled */
        got = NULL;
        EQHR(TWO(r)->lpVtbl->GetObj(TWO(r), &psd_IID_Two, &got), S_OK);
        EQ(got, (IUnknown *)&r.srv->two);
        if (got) IUnknown_Release(got);
        got = (IUnknown *)1;
        HRESULT hr = TWO(r)->lpVtbl->GetObj(TWO(r), &psd_IID_Unknown1, &got);
        EQHR(hr, E_NOINTERFACE);                             /* the server's HRESULT */
        t_note("[out] interface pointer after the failing GetObj: %p", got);
        if (got && got != (IUnknown *)1) IUnknown_Release(got);
        t_note("server refcount after the GetObj calls: %ld", r.srv->ref);
        psd_rig_down(&r);
    }
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}

TEST(psdl_getobj_destctx_sampled_per_call) {
    PSD_STA();
    DWORD ck; IPSFactoryBuffer *psf;
    if (!psd_init() || !ps_up(&ck, &psf)) { t_skip("cannot register PS class"); PSD_DONE(); return; }
    {
        RIGC(r, &psd_IID_Two);
        /* GetDestCtx is sampled when the call is initialised (proxy side) and when the stub is initialised */
        r.chan->destctx = MSHCTX_LOCAL;
        IUnknown *got = NULL;
        int d0 = r.chan->n_destctx;
        EQHR(TWO(r)->lpVtbl->GetObj(TWO(r), &psd_IID_One, &got), S_OK);
        EQ(r.chan->n_destctx - d0 >= 2, 1);
        if (got) IUnknown_Release(got);
        psd_rig_down(&r);
    }
    psd_unregister_ps(ck, psf);
    PSD_DONE();
}
