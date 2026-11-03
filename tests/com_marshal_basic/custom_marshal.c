/* Custom IMarshal objects: call sequence, arguments, stream layout, unmarshal class resolution. */
#include "mb.h"
#include <stdlib.h>

#define SETUP() REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface)); REQUIRE(U(CoReleaseMarshalData)); \
    mb_obj_reset(1); mb_cm_reset(); MB_INIT_STA(); DWORD cookie_ = mb_cm_register(CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE); \
    if (!cookie_) t_note("class registration failed")
#define TEARDOWN() do { mb_cm_revoke(cookie_); MB_DONE(); } while (0)

static IStream *marshal_cm(DWORD ctx, DWORD fl, const GUID *iid, HRESULT *hr) {
    IStream *s = mb_stream();
    *hr = U(CoMarshalInterface)(s, iid, mb_cm_unk(), ctx, NULL, fl);
    return s;
}

TEST(mbasic_custom_call_sequence) {
    SETUP();
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    EQ(mb_ev_count(EV_GUC), 1);
    EQ(mb_ev_count(EV_MI), 1);
    EQ(mb_ev_count(EV_UI), 0);
    EQ(mb_ev_count(EV_RMD), 0);
    int g = mb_ev_index(EV_GUC, 0), m = mb_ev_index(EV_MI, 0), z = mb_ev_index(EV_GMSM, 0);
    CHECK(g >= 0 && m > g);
    EQ(mb_ev_count(EV_GMSM) <= 1, 1);
    if (z >= 0) CHECK(z > g && z < m);
    t_note("GetMarshalSizeMax called %d time(s) during CoMarshalInterface", mb_ev_count(EV_GMSM));
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_args_passed_through) {
    SETUP();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_INPROC };
    static const DWORD fls[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK, MSHLFLAGS_NOPING };
    static const GUID *iids[] = { &IID_IUnknown, &IID_IMarshal };
    for (int i = 0; i < 3; i++) for (int j = 0; j < 4; j++) for (int k = 0; k < 2; k++) {
        mb_ev_reset();
        HRESULT hr; IStream *s = marshal_cm(ctxs[i], fls[j], iids[k], &hr);
        EQ(hr, S_OK);
        int g = mb_ev_index(EV_GUC, 0), m = mb_ev_index(EV_MI, 0);
        if (g >= 0) { EQ(g_ev[g].ctx, ctxs[i]); EQ(g_ev[g].flags, fls[j]); CHECK(IsEqualGUID(&g_ev[g].iid, iids[k])); EQ(g_ev[g].dctx, 0); }
        else CHECK(0);
        if (m >= 0) { EQ(g_ev[m].ctx, ctxs[i]); EQ(g_ev[m].flags, fls[j]); CHECK(IsEqualGUID(&g_ev[m].iid, iids[k])); }
        else CHECK(0);
        if (g >= 0 && ctxs[i] == MSHCTX_INPROC && j == 0 && k == 0) t_note("pv passed to GetUnmarshalClass %s the original pointer", g_ev[g].pv == (void *)mb_cm_unk() ? "is" : "is not");
        IStream_Release(s);
    }
    TEARDOWN();
}
TEST(mbasic_custom_stream_layout) {
    SETUP();
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n);
    MBObjRef o;
    CHECK(mb_parse_objref(b, n, &o));
    EQ(o.signature, MB_OBJREF_SIG);
    EQ(o.flags, 4);                                  /* OBJREF_CUSTOM */
    CHECK(IsEqualGUID(&o.iid, &IID_IUnknown));
    CHECK(IsEqualGUID(&o.clsid, &MB_CLSID_Custom));  /* from GetUnmarshalClass */
    EQ(o.cbext, 0);
    EQ(n, 48 + g_cm_cfg.plen);                       /* header then exactly what MarshalInterface wrote */
    CHECK(o.cdata_len == g_cm_cfg.plen && !memcmp(o.cdata, g_cm_cfg.payload, g_cm_cfg.plen));
    t_note("OBJREF_CUSTOM size field = %lu (payload %lu)", o.csize, g_cm_cfg.plen);
    free(b); IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_stream_layout_large_payload) {
    SETUP();
    g_cm_cfg.plen = 256; for (int i = 0; i < 256; i++) g_cm_cfg.payload[i] = (BYTE)(i * 7 + 1);
    g_cm_cfg.size = 256;
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_LOCAL, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n);
    EQ(n, 48 + 256);
    CHECK(n == 304 && !memcmp(b + 48, g_cm_cfg.payload, 256));
    free(b); IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_stream_layout_empty_payload) {
    SETUP();
    g_cm_cfg.plen = 0; g_cm_cfg.size = 0;
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    EQ(mb_size(s), 48);
    EQ(mb_pos(s), 48);
    mb_rewind(s);
    IUnknown *u = NULL;
    EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, (void **)&u), S_OK);
    CHECK(u == mb_cm_unk());
    if (u) IUnknown_Release(u);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_via_registered_class) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    mb_ev_reset(); mb_rewind(s);
    IUnknown *u = NULL;
    hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, (void **)&u);
    EQ(hr, S_OK);
    EQ(mb_ev_count(EV_CTOR_CREATE), 1);                 /* class object asked to create the unmarshaler */
    EQ(mb_ev_count(EV_UI), 1);
    int u1 = mb_ev_index(EV_UI, 0);
    if (u1 >= 0) { EQ(g_ev[u1].which, 1); CHECK(IsEqualGUID(&g_ev[u1].iid, &IID_IUnknown)); }
    CHECK(u == mb_cm_unk());                            /* whatever UnmarshalInterface returned is handed out */
    EQ(mb_pos(s), 48 + g_cm_cfg.plen);                  /* payload consumed */
    EQ(mb_ev_count(EV_RMD), 0);                         /* unmarshal does not release marshal data */
    int c = mb_ev_index(EV_CTOR_CREATE, 0);
    if (c >= 0) t_note("factory CreateInstance riid: %s", IsEqualGUID(&g_ev[c].iid, &IID_IMarshal) ? "IID_IMarshal" : IsEqualGUID(&g_ev[c].iid, &IID_IUnknown) ? "IID_IUnknown" : "other");
    if (u) IUnknown_Release(u);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_requested_iid_forwarded) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IMarshal, &hr);
    EQ(hr, S_OK);
    mb_ev_reset(); mb_rewind(s);
    IMarshal *m = NULL;
    EQ(U(CoUnmarshalInterface)(s, &IID_IMarshal, (void **)&m), S_OK);
    int u1 = mb_ev_index(EV_UI, 0);
    if (u1 >= 0) CHECK(IsEqualGUID(&g_ev[u1].iid, &IID_IMarshal));
    CHECK(m == mb_cm_marshal());
    if (m) IMarshal_Release(m);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_unsupported_iid) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_rewind(s);
    void *p = (void *)1;
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, &p), MB_E_NOINTERFACE);   /* error from the custom UnmarshalInterface */
    EQ(p, 0);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_hr_propagates) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_rewind(s);
    g_cm_cfg.hr_ui = 0x80041234; void *p = (void *)1;
    EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, &p), 0x80041234);
    EQ(p, 0);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_twice_calls_twice) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_ev_reset();
    for (int i = 0; i < 2; i++) {
        mb_rewind(s); IUnknown *u = NULL;
        EQ(U(CoUnmarshalInterface)(s, &IID_IUnknown, (void **)&u), S_OK);
        if (u) IUnknown_Release(u);
    }
    EQ(mb_ev_count(EV_UI), 2);                           /* no once-only enforcement for custom marshaling */
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_unregistered_class_fails) {
    SETUP();
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    mb_cm_revoke(cookie_); cookie_ = 0;
    mb_ev_reset(); mb_rewind(s);
    void *p = (void *)1;
    hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    EQ(p, 0);
    EQ(mb_ev_count(EV_UI), 0);
    t_note("unregistered custom unmarshal class -> 0x%lx", (unsigned long)hr);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_factory_failure_propagates) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_rewind(s);
    g_cm_cfg.hr_factory = 0x80041235; void *p = (void *)1;
    hr = U(CoUnmarshalInterface)(s, &IID_IUnknown, &p);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("class factory failure 0x80041235 surfaces as 0x%lx", (unsigned long)hr);
    EQ(p, 0);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_release_marshal_data) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_ev_reset(); mb_rewind(s);
    EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(mb_ev_count(EV_RMD), 1);
    int r = mb_ev_index(EV_RMD, 0);
    if (r >= 0) EQ(g_ev[r].which, 1);                    /* called on the class-created unmarshaler */
    EQ(mb_pos(s), 48 + g_cm_cfg.plen);
    EQ(mb_ev_count(EV_UI), 0);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_release_marshal_data_failure) {
    SETUP();
    if (!cookie_) { t_skip("no class registration"); TEARDOWN(); return; }
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK); mb_rewind(s);
    g_cm_cfg.hr_rmd = 0x80041236;
    hr = U(CoReleaseMarshalData)(s);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("ReleaseMarshalData failure -> 0x%lx, stream pos %llu", (unsigned long)hr, mb_pos(s));
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_getunmarshalclass_failure) {
    SETUP();
    g_cm_cfg.hr_guc = 0x80041237;
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, 0x80041237);
    EQ(mb_ev_count(EV_MI), 0);
    EQ(mb_size(s), 0);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_marshalinterface_failure) {
    SETUP();
    g_cm_cfg.hr_mi = 0x80041238;
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, 0x80041238);
    EQ(mb_ev_count(EV_MI), 1);
    t_note("stream size after failed custom MarshalInterface: %llu", mb_size(s));
    IStream_Release(s); TEARDOWN();
}
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
TEST(mbasic_custom_stdmarshal_class_sequence) {
    SETUP();
    g_cm_cfg.clsid = MB_CLSID_StdMarshal;
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    EQ(mb_ev_count(EV_GUC), 1);                         /* Wine (Windows-verified): GetUnmarshalClass + MarshalInterface are called, no GetMarshalSizeMax */
    EQ(mb_ev_count(EV_MI), 1);
    EQ(mb_ev_count(EV_GMSM), 0);
    ULONG n; BYTE *b = mb_dump(s, &n); MBObjRef o;
    int ok = mb_parse_objref(b, n, &o);
    t_note("StdMarshal class + MarshalInterface writing %lu bytes: stream %lu bytes, parse=%d flags=%lu", g_cm_cfg.plen, n, ok, ok ? o.flags : 0);
    free(b);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_getmarshalsizemax) {
    SETUP();
    ULONG a = 0, b = 0;
    mb_ev_reset();
    EQ(U(CoGetMarshalSizeMax)(&a, &IID_IUnknown, mb_cm_unk(), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    EQ(mb_ev_count(EV_GMSM), 1);
    int z = mb_ev_index(EV_GMSM, 0);
    if (z >= 0) { EQ(g_ev[z].ctx, MSHCTX_INPROC); EQ(g_ev[z].flags, MSHLFLAGS_NORMAL); CHECK(IsEqualGUID(&g_ev[z].iid, &IID_IUnknown)); }
    CHECKF(a >= g_cm_cfg.size, "size %lu < custom size %lu", a, g_cm_cfg.size);
    t_note("CoGetMarshalSizeMax custom(size=%lu) = %lu", g_cm_cfg.size, a);
    g_cm_cfg.size = 116;
    EQ(U(CoGetMarshalSizeMax)(&b, &IID_IUnknown, mb_cm_unk(), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    EQ(b - a, 100);                                     /* total = fixed overhead + object's own maximum */
    TEARDOWN();
}
TEST(mbasic_custom_getmarshalsizemax_covers_actual) {
    SETUP();
    ULONG max = 0;
    EQ(U(CoGetMarshalSizeMax)(&max, &IID_IUnknown, mb_cm_unk(), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    CHECK(mb_size(s) <= max);
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_getmarshalsizemax_failure) {
    SETUP();
    g_cm_cfg.hr_gmsm = 0x80041239;
    ULONG a = 0x1234;
    HRESULT hr = U(CoGetMarshalSizeMax)(&a, &IID_IUnknown, mb_cm_unk(), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    CHECKF(FAILED(hr), "hr=0x%lx", (unsigned long)hr);
    t_note("GetMarshalSizeMax failure -> 0x%lx size=%lu", (unsigned long)hr, a);
    TEARDOWN();
}
TEST(mbasic_custom_disconnect_object) {
    SETUP();
    mb_ev_reset();
    EQ(U(CoDisconnectObject)(mb_cm_unk(), 0), S_OK);
    EQ(mb_ev_count(EV_DISC), 1);
    int d = mb_ev_index(EV_DISC, 0);
    if (d >= 0) { EQ(g_ev[d].which, 0); EQ(g_ev[d].ctx, 0); }    /* ctx field carries dwReserved */
    TEARDOWN();
}
TEST(mbasic_custom_disconnect_object_hr_propagates) {
    SETUP();
    g_cm_cfg.hr_disc = 0x8004123A;
    EQ(U(CoDisconnectObject)(mb_cm_unk(), 0), 0x8004123A);
    EQ(mb_ev_count(EV_DISC), 1);
    TEARDOWN();
}
TEST(mbasic_custom_disconnect_not_called_on_invalid_reserved) {
    SETUP();
    EQ(U(CoDisconnectObject)(mb_cm_unk(), 5), MB_E_INVALIDARG);
    EQ(mb_ev_count(EV_DISC), 0);
    TEARDOWN();
}
TEST(mbasic_custom_destcontext_object_with_differentmachine) {
    SETUP();
    static const IUnknownVtbl vt; (void)vt;
    IStream *s = mb_stream();
    /* pvDestContext is only legal for MSHCTX_DIFFERENTMACHINE; our object acts as an opaque destination */
    HRESULT hr = U(CoMarshalInterface)(s, &IID_IUnknown, mb_cm_unk(), MSHCTX_DIFFERENTMACHINE, mb_smi_unk(), MSHLFLAGS_NORMAL);
    t_note("DIFFERENTMACHINE with pvDestContext object -> 0x%lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) {
        int g = mb_ev_index(EV_GUC, 0);
        if (g >= 0) { EQ(g_ev[g].ctx, MSHCTX_DIFFERENTMACHINE); t_note("pvDestContext seen by IMarshal: %s", g_ev[g].dctx == (void *)mb_smi_unk() ? "original" : "different"); }
    }
    IStream_Release(s); TEARDOWN();
}
TEST(mbasic_custom_marshal_through_instrumented_stream) {
    SETUP();
    MBStm *w = mb_wstream_new(); if (!w) { CHECK(0); TEARDOWN(); return; }
    HRESULT hr = U(CoMarshalInterface)(&w->s, &IID_IUnknown, mb_cm_unk(), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    EQ(hr, S_OK);
    CHECK(w->writes >= 1);
    EQ(mb_size(&w->s), 48 + g_cm_cfg.plen);
    IStream_Release(&w->s); TEARDOWN();
}
TEST(mbasic_custom_unmarshal_short_header_stream) {
    SETUP();
    HRESULT hr; IStream *s = marshal_cm(MSHCTX_INPROC, MSHLFLAGS_NORMAL, &IID_IUnknown, &hr);
    EQ(hr, S_OK);
    ULONG n; BYTE *b = mb_dump(s, &n);
    IStream *t = mb_stream(); void *p = (void *)1;
    IStream_Write(t, b, 40, NULL); mb_rewind(t);               /* custom header cut mid-way */
    EQ(U(CoUnmarshalInterface)(t, &IID_IUnknown, &p), MB_STG_E_READFAULT);
    EQ(p, 0);
    IStream_Release(t); free(b); IStream_Release(s); TEARDOWN();
}
