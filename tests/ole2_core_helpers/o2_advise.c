/* CreateDataAdviseHolder / IDataAdviseHolder and CreateOleAdviseHolder / IOleAdviseHolder.
 * Expectations are derived from the Win10 ole32 decompilation of CDAHolder and COAHolder. */
#include "olh.h"

static HRESULT mk_dah(IDataAdviseHolder **pp) { *pp = NULL; return U(CreateDataAdviseHolder)(pp); }
static HRESULT mk_oah(IOleAdviseHolder **pp) { *pp = NULL; return U(CreateOleAdviseHolder)(pp); }
static FORMATETC fe_make(CLIPFORMAT cf, DWORD aspect, LONG idx, DWORD tymed) {
    FORMATETC f; f.cfFormat = cf; f.ptd = NULL; f.dwAspect = aspect; f.lindex = idx; f.tymed = tymed; return f;
}
#define FE_TEXT() fe_make(CF_TEXT, DVASPECT_CONTENT, -1, TYMED_HGLOBAL)

/* ================================================================================================
 * IDataAdviseHolder
 * ============================================================================================== */
TEST(o2dah_create_null) {
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(U(CreateDataAdviseHolder)(NULL), O2_E_INVALIDARG);
}
TEST(o2dah_create_and_release) {
    IDataAdviseHolder *h; HRESULT hr;
    REQUIRE(U(CreateDataAdviseHolder));
    hr = mk_dah(&h);
    EQ(hr, S_OK); CHECK(h != NULL);
    EQ(IDataAdviseHolder_AddRef(h), 2);
    EQ(IDataAdviseHolder_Release(h), 1);
    EQ(IDataAdviseHolder_Release(h), 0);
}
TEST(o2dah_create_distinct_instances) {
    IDataAdviseHolder *a, *b;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&a), S_OK); EQ(mk_dah(&b), S_OK);
    CHECK(a != b);
    IDataAdviseHolder_Release(a); IDataAdviseHolder_Release(b);
}
TEST(o2dah_queryinterface) {
    IDataAdviseHolder *h; void *p; HRESULT hr;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    p = (void *)1; hr = IDataAdviseHolder_QueryInterface(h, &IID_IUnknown, &p);
    EQ(hr, S_OK); EQ(p, h); IUnknown_Release((IUnknown *)p);
    p = (void *)1; hr = IDataAdviseHolder_QueryInterface(h, &IID_IDataAdviseHolder, &p);
    EQ(hr, S_OK); EQ(p, h); IUnknown_Release((IUnknown *)p);
    p = (void *)1; hr = IDataAdviseHolder_QueryInterface(h, &IID_IOleAdviseHolder, &p);
    EQ(hr, O2_E_NOINTERFACE); EQ(p, NULL);
    p = (void *)1; hr = IDataAdviseHolder_QueryInterface(h, &IID_IDataObject, &p);
    EQ(hr, O2_E_NOINTERFACE); EQ(p, NULL);
    p = (void *)1; hr = IDataAdviseHolder_QueryInterface(h, &O2_IID_BOGUS, &p);
    EQ(hr, O2_E_NOINTERFACE); EQ(p, NULL);
    EQ(IDataAdviseHolder_Release(h), 0);
}
TEST(o2dah_advise_connection_ids_increase) {
    IDataAdviseHolder *h; O2Sink s[3]; DWORD c[3]; int i;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    for (i = 0; i < 3; i++) {
        FORMATETC f = FE_TEXT(); os_init(&s[i]); c[i] = 0xCCCC;
        EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s[i].i, &c[i]), S_OK);
    }
    EQ(c[0], 1); EQ(c[1], 2); EQ(c[2], 3);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_connection_ids_not_reused) {
    IDataAdviseHolder *h; O2Sink a, b, c; DWORD ca, cb, cc; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    os_init(&a); os_init(&b); os_init(&c);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &a.i, &ca), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &b.i, &cb), S_OK);
    EQ(IDataAdviseHolder_Unadvise(h, ca), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &c.i, &cc), S_OK);
    EQ(ca, 1); EQ(cb, 2); EQ(cc, 3);          /* unlike COAHolder, ids come from a running counter */
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_advise_null_connection_pointer_ok) {
    IDataAdviseHolder *h; O2Sink s; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, NULL), S_OK);
    EQ(s.ref, 2);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_advise_null_sink) {
    IDataAdviseHolder *h; DWORD c = 0xCCCC; FORMATETC f = FE_TEXT();
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, NULL, &c), O2_E_INVALIDARG);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_advise_lindex_validation) {
    static const struct { DWORD aspect; LONG idx; HRESULT hr; } c[] = {
        { DVASPECT_CONTENT, -1, S_OK }, { DVASPECT_ICON, -1, S_OK }, { DVASPECT_CONTENT, 0, DV_E_LINDEX },
        { DVASPECT_CONTENT, 1, DV_E_LINDEX }, { DVASPECT_THUMBNAIL, 5, DV_E_LINDEX }, { DVASPECT_ICON, -2, DV_E_LINDEX },
        { DVASPECT_DOCPRINT, 0, S_OK },
    };
    int i;
    REQUIRE(U(CreateDataAdviseHolder));
    for (i = 0; i < O2_N(c); i++) {
        IDataAdviseHolder *h; O2Sink s; FORMATETC f = fe_make(CF_TEXT, c[i].aspect, c[i].idx, TYMED_HGLOBAL); DWORD con = 0xCCCC; HRESULT hr;
        EQ(mk_dah(&h), S_OK); os_init(&s);
        hr = IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &con);
        CHECKF(hr == c[i].hr, "aspect %lu lindex %ld: got 0x%08lx want 0x%08lx", (unsigned long)c[i].aspect, (long)c[i].idx, (unsigned long)hr, (unsigned long)c[i].hr);
        if (FAILED(c[i].hr)) { EQ(s.ref, 1); }
        IDataAdviseHolder_Release(h);
    }
}
TEST(o2dah_sink_refcounting) {
    IDataAdviseHolder *h; O2Sink s; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);
    EQ(s.ref, 2);
    EQ(IDataAdviseHolder_Unadvise(h, c), S_OK);
    EQ(s.ref, 1);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);   /* same sink twice: two references */
    EQ(s.ref, 3);
    IDataAdviseHolder_Release(h);                                  /* destroying the holder releases the sinks */
    EQ(s.ref, 1);
}
TEST(o2dah_unadvise_errors) {
    IDataAdviseHolder *h; O2Sink s; DWORD c; FORMATETC f = FE_TEXT(); HRESULT hr;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s);
    EQ(IDataAdviseHolder_Unadvise(h, 0), O2_OLE_E_NOCONNECTION);        /* nothing advised yet */
    EQ(IDataAdviseHolder_Unadvise(h, 1), O2_OLE_E_NOCONNECTION);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_Unadvise(h, c + 100), O2_OLE_E_NOCONNECTION);
    EQ(IDataAdviseHolder_Unadvise(h, 0xFFFFFFFF), O2_OLE_E_NOCONNECTION);
    EQ(IDataAdviseHolder_Unadvise(h, c), S_OK);
    EQ(IDataAdviseHolder_Unadvise(h, c), O2_OLE_E_NOCONNECTION);        /* twice */
    hr = IDataAdviseHolder_Unadvise(h, 0);
    t_note("IDataAdviseHolder::Unadvise(0) after an advise slot table exists -> 0x%08lx", (unsigned long)hr);
    IDataAdviseHolder_Release(h);
}

/* ---- PRIMEFIRST ----------------------------------------------------------------------------- */
TEST(o2dah_primefirst_delivers_immediately) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, ADVF_PRIMEFIRST, &s.i, &c), S_OK);
    EQ(c, 1);
    EQ(d.getdata, 1);
    EQ(s.ondata, 1);
    EQ(s.last_tymed, TYMED_HGLOBAL);
    EQ(s.last_fe.cfFormat, CF_TEXT);
    EQ(s.ref, 2);
    IDataAdviseHolder_Release(h);
    EQ(d.ref, 1);
}
TEST(o2dah_primefirst_clears_flag_in_stored_statdata) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT(); IEnumSTATDATA *e; STATDATA sd; ULONG got;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, ADVF_PRIMEFIRST | ADVF_DATAONSTOP, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    memset(&sd, 0, sizeof sd); got = 99;
    EQ(IEnumSTATDATA_Next(e, 1, &sd, &got), S_OK);
    EQ(got, 1);
    EQ(sd.advf, ADVF_DATAONSTOP);                /* PRIMEFIRST is consumed by Advise */
    EQ(sd.dwConnection, c);
    EQ(sd.pAdvSink, &s.i);
    IAdviseSink_Release(sd.pAdvSink);
    IEnumSTATDATA_Release(e);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_primefirst_nodata) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, ADVF_PRIMEFIRST | ADVF_NODATA, &s.i, &c), S_OK);
    EQ(d.getdata, 0);              /* ADVF_NODATA: the data object is not asked for data */
    EQ(s.ondata, 1);
    EQ(s.last_tymed, TYMED_NULL);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_primefirst_getdata_failure_still_registers) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c = 0xCCCC; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    d.getdata_hr = E_FAIL;
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, ADVF_PRIMEFIRST, &s.i, &c), S_OK);
    EQ(d.getdata, 1);
    EQ(s.ondata, 0);               /* GetData failed: no notification */
    EQ(c, 1);
    EQ(s.ref, 2);                  /* but the advise is registered */
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_primefirst_onlyonce_not_registered) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c = 0xCCCC; FORMATETC f = FE_TEXT(); IEnumSTATDATA *e; STATDATA sd; ULONG got = 99;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, ADVF_PRIMEFIRST | ADVF_ONLYONCE, &s.i, &c), S_OK);
    EQ(s.ondata, 1);
    EQ(c, 0);                      /* no connection was created */
    EQ(s.ref, 1);
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    EQ(IEnumSTATDATA_Next(e, 1, &sd, &got), S_FALSE);
    EQ(got, 0);
    IEnumSTATDATA_Release(e);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_primefirst_without_dataobject) {
    IDataAdviseHolder *h; O2Sink s; DWORD c = 0xCCCC; FORMATETC f = FE_TEXT();
    if (t_is_wine()) { t_skip("emulator dereferences the NULL data object"); return; }
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, ADVF_PRIMEFIRST, &s.i, &c), S_OK);
    EQ(s.ondata, 0);
    EQ(c, 1);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_advise_without_primefirst_does_not_call_getdata) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, 0, &s.i, &c), S_OK);
    EQ(d.getdata, 0); EQ(s.ondata, 0);
    IDataAdviseHolder_Release(h);
}

/* ---- SendOnDataChange ------------------------------------------------------------------------- */
TEST(o2dah_senddatachange_null_data) {
    IDataAdviseHolder *h;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, NULL, 0, 0), O2_E_INVALIDARG);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_no_advises) {
    IDataAdviseHolder *h; O2Data d;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "x");
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 0);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_delivers_data) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, 0, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 1); EQ(s.ondata, 1);
    EQ(s.last_tymed, TYMED_HGLOBAL);
    EQ(s.last_fe.cfFormat, CF_TEXT); EQ(s.last_fe.dwAspect, DVASPECT_CONTENT); EQ(s.last_fe.lindex, -1);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 2); EQ(s.ondata, 2);
    IDataAdviseHolder_Release(h);
    EQ(d.ref, 1);
}
TEST(o2dah_senddatachange_same_format_single_getdata) {
    IDataAdviseHolder *h; O2Sink a, b; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&a); os_init(&b); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, 0, &a.i, &c), S_OK);
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f, 0, &b.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 1);              /* one GetData per distinct FORMATETC */
    EQ(a.ondata, 1); EQ(b.ondata, 1);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_distinct_formats) {
    IDataAdviseHolder *h; O2Sink a, b; O2Data d; DWORD c; FORMATETC f1 = FE_TEXT(), f2 = fe_make(CF_UNICODETEXT, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&a); os_init(&b); od_init(&d);
    od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "t"); od_add(&d, CF_UNICODETEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "u");
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f1, 0, &a.i, &c), S_OK);
    EQ(IDataAdviseHolder_Advise(h, &d.i, &f2, 0, &b.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 2);
    EQ(a.ondata, 1); EQ(a.last_fe.cfFormat, CF_TEXT);
    EQ(b.ondata, 1); EQ(b.last_fe.cfFormat, CF_UNICODETEXT);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_nodata_sink) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, ADVF_NODATA, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(d.getdata, 0);
    EQ(s.ondata, 1);
    EQ(s.last_tymed, TYMED_NULL);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_onlyonce_removed_after_send) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, ADVF_ONLYONCE, &s.i, &c), S_OK);
    EQ(c, 1); EQ(s.ref, 2);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(s.ondata, 1);
    EQ(s.ref, 1);                  /* advise dropped and sink released */
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(s.ondata, 1);
    EQ(IDataAdviseHolder_Unadvise(h, c), O2_OLE_E_NOCONNECTION);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_nodata_onlyonce) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, ADVF_NODATA | ADVF_ONLYONCE, &s.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(s.ondata, 1);
    EQ(s.ref, 1);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_dataonstop_gating) {
    IDataAdviseHolder *h; O2Sink normal, stop; O2Data d; DWORD c; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&normal); os_init(&stop); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &normal.i, &c), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, ADVF_DATAONSTOP, &stop.i, &c), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(normal.ondata, 1); EQ(stop.ondata, 0);          /* a plain change notification skips DATAONSTOP sinks */
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, ADVF_DATAONSTOP), S_OK);
    EQ(normal.ondata, 1); EQ(stop.ondata, 1);          /* the stop notification reaches only DATAONSTOP sinks */
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_unadvised_sink_not_called) {
    IDataAdviseHolder *h; O2Sink a, b; O2Data d; DWORD ca, cb; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&a); os_init(&b); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &a.i, &ca), S_OK);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &b.i, &cb), S_OK);
    EQ(IDataAdviseHolder_Unadvise(h, ca), S_OK);
    EQ(IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0), S_OK);
    EQ(a.ondata, 0); EQ(b.ondata, 1);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_senddatachange_getdata_failure) {
    IDataAdviseHolder *h; O2Sink s; O2Data d; DWORD c; FORMATETC f = FE_TEXT(); HRESULT hr;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s); od_init(&d); od_add(&d, CF_TEXT, TYMED_HGLOBAL, DVASPECT_CONTENT, "hello");
    d.getdata_hr = E_FAIL;
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);
    hr = IDataAdviseHolder_SendOnDataChange(h, &d.i, 0, 0);
    EQ(hr, S_OK);
    EQ(d.getdata, 1);
    t_note("sink notified despite GetData failure: ondata=%ld tymed=%lu", (long)s.ondata, (unsigned long)s.last_tymed);
    IDataAdviseHolder_Release(h);
}

/* ---- EnumAdvise ------------------------------------------------------------------------------- */
TEST(o2dah_enumadvise_null) {
    IDataAdviseHolder *h;
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    EQ(IDataAdviseHolder_EnumAdvise(h, NULL), O2_E_INVALIDARG);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_enumadvise_empty) {
    IDataAdviseHolder *h; IEnumSTATDATA *e; STATDATA sd; ULONG got = 99;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    CHECK(e != NULL);
    EQ(IEnumSTATDATA_Next(e, 1, &sd, &got), S_FALSE);
    EQ(got, 0);
    IEnumSTATDATA_Release(e);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_enumadvise_contents_and_navigation) {
    IDataAdviseHolder *h; O2Sink s[3]; DWORD c[3]; IEnumSTATDATA *e, *e2; STATDATA sd[4]; ULONG got; int i;
    static const DWORD advfs[3] = { 0, ADVF_NODATA, ADVF_DATAONSTOP | ADVF_ONLYONCE };
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    for (i = 0; i < 3; i++) {
        FORMATETC f = fe_make(CF_TEXT + i, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
        os_init(&s[i]); EQ(IDataAdviseHolder_Advise(h, NULL, &f, advfs[i], &s[i].i, &c[i]), S_OK);
    }
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    memset(sd, 0, sizeof sd); got = 99;
    EQ(IEnumSTATDATA_Next(e, 4, sd, &got), S_FALSE);
    EQ(got, 3);
    for (i = 0; i < 3; i++) {
        EQ(sd[i].dwConnection, c[i]);
        EQ(sd[i].advf, advfs[i]);
        EQ(sd[i].pAdvSink, &s[i].i);
        EQ(sd[i].formatetc.cfFormat, CF_TEXT + i);
        EQ(sd[i].formatetc.dwAspect, DVASPECT_CONTENT);
        EQ(sd[i].formatetc.lindex, -1);
        EQ(sd[i].formatetc.tymed, TYMED_HGLOBAL);
        CHECK(s[i].ref >= 3);  /* holder + the AddRef'd STATDATA copy */
        IAdviseSink_Release(sd[i].pAdvSink);
    }
    EQ(IEnumSTATDATA_Reset(e), S_OK);
    EQ(IEnumSTATDATA_Skip(e, 2), S_OK);
    got = 99; EQ(IEnumSTATDATA_Next(e, 1, sd, &got), S_OK);
    EQ(got, 1); EQ(sd[0].dwConnection, c[2]); IAdviseSink_Release(sd[0].pAdvSink);
    got = 99; EQ(IEnumSTATDATA_Next(e, 1, sd, &got), S_FALSE); EQ(got, 0);
    EQ(IEnumSTATDATA_Reset(e), S_OK);
    EQ(IEnumSTATDATA_Clone(e, &e2), S_OK);
    got = 99; EQ(IEnumSTATDATA_Next(e2, 1, sd, &got), S_OK);
    EQ(sd[0].dwConnection, c[0]); IAdviseSink_Release(sd[0].pAdvSink);
    IEnumSTATDATA_Release(e2);
    IEnumSTATDATA_Release(e);
    /* Unadvise: the enumeration skips the hole */
    EQ(IDataAdviseHolder_Unadvise(h, c[1]), S_OK);
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    got = 99; EQ(IEnumSTATDATA_Next(e, 4, sd, &got), S_FALSE);
    EQ(got, 2);
    EQ(sd[0].dwConnection, c[0]); EQ(sd[1].dwConnection, c[2]);
    IAdviseSink_Release(sd[0].pAdvSink); IAdviseSink_Release(sd[1].pAdvSink);
    IEnumSTATDATA_Release(e);
    IDataAdviseHolder_Release(h);
    for (i = 0; i < 3; i++) EQ(s[i].ref, 1);
}
TEST(o2dah_advise_copies_formatetc) {
    IDataAdviseHolder *h; O2Sink s; DWORD c; FORMATETC f = FE_TEXT(); IEnumSTATDATA *e; STATDATA sd; ULONG got;
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK); os_init(&s);
    EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s.i, &c), S_OK);
    f.cfFormat = CF_BITMAP; f.tymed = TYMED_GDI;       /* caller reuses its structure */
    EQ(IDataAdviseHolder_EnumAdvise(h, &e), S_OK);
    EQ(IEnumSTATDATA_Next(e, 1, &sd, &got), S_OK);
    EQ(sd.formatetc.cfFormat, CF_TEXT); EQ(sd.formatetc.tymed, TYMED_HGLOBAL);
    IAdviseSink_Release(sd.pAdvSink);
    IEnumSTATDATA_Release(e);
    IDataAdviseHolder_Release(h);
}
TEST(o2dah_many_advises_grow_table) {
    IDataAdviseHolder *h; O2Sink s[23]; DWORD c, last = 0; int i; FORMATETC f = FE_TEXT();
    REQUIRE(U(CreateDataAdviseHolder));
    EQ(mk_dah(&h), S_OK);
    for (i = 0; i < O2_N(s); i++) { os_init(&s[i]); EQ(IDataAdviseHolder_Advise(h, NULL, &f, 0, &s[i].i, &c), S_OK); if (c != (DWORD)(i + 1)) CHECKF(0, "id %lu at %d", (unsigned long)c, i); last = c; }
    EQ(last, 23);
    for (i = 0; i < O2_N(s); i++) EQ(IDataAdviseHolder_Unadvise(h, (DWORD)(i + 1)), S_OK);
    IDataAdviseHolder_Release(h);
    for (i = 0; i < O2_N(s); i++) EQ(s[i].ref, 1);
}

/* ================================================================================================
 * IOleAdviseHolder
 * ============================================================================================== */
TEST(o2oah_create_null) {
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(U(CreateOleAdviseHolder)(NULL), O2_E_INVALIDARG);
}
TEST(o2oah_create_and_release) {
    IOleAdviseHolder *h;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); CHECK(h != NULL);
    EQ(IOleAdviseHolder_AddRef(h), 2);
    EQ(IOleAdviseHolder_Release(h), 1);
    EQ(IOleAdviseHolder_Release(h), 0);
}
TEST(o2oah_queryinterface) {
    IOleAdviseHolder *h; void *p; HRESULT hr;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK);
    p = (void *)1; hr = IOleAdviseHolder_QueryInterface(h, &IID_IUnknown, &p); EQ(hr, S_OK); EQ(p, h); IUnknown_Release((IUnknown *)p);
    p = (void *)1; hr = IOleAdviseHolder_QueryInterface(h, &IID_IOleAdviseHolder, &p); EQ(hr, S_OK); EQ(p, h); IUnknown_Release((IUnknown *)p);
    p = (void *)1; hr = IOleAdviseHolder_QueryInterface(h, &IID_IDataAdviseHolder, &p); EQ(hr, O2_E_NOINTERFACE); EQ(p, NULL);
    p = (void *)1; hr = IOleAdviseHolder_QueryInterface(h, &IID_IAdviseSink, &p); EQ(hr, O2_E_NOINTERFACE); EQ(p, NULL);
    EQ(IOleAdviseHolder_Release(h), 0);
}
TEST(o2oah_advise_null_sink) {
    IOleAdviseHolder *h; DWORD c = 0xCCCC;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK);
    EQ(IOleAdviseHolder_Advise(h, NULL, &c), O2_E_INVALIDARG);
    EQ(c, 0xCCCC);                 /* rejected before the output is touched */
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_advise_ids_one_based_slots) {
    IOleAdviseHolder *h; O2Sink s[3]; DWORD c[3]; int i;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK);
    for (i = 0; i < 3; i++) { os_init(&s[i]); c[i] = 0xCCCC; EQ(IOleAdviseHolder_Advise(h, &s[i].i, &c[i]), S_OK); }
    EQ(c[0], 1); EQ(c[1], 2); EQ(c[2], 3);
    for (i = 0; i < 3; i++) EQ(s[i].ref, 2);
    IOleAdviseHolder_Release(h);
    for (i = 0; i < 3; i++) EQ(s[i].ref, 1);
}
TEST(o2oah_advise_reuses_freed_slot) {
    IOleAdviseHolder *h; O2Sink a, b, c, d; DWORD ca, cb, cc, cd;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&a); os_init(&b); os_init(&c); os_init(&d);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &ca), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &b.i, &cb), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &c.i, &cc), S_OK);
    EQ(IOleAdviseHolder_Unadvise(h, cb), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &d.i, &cd), S_OK);
    EQ(cd, cb);                    /* the lowest free slot (id = slot + 1) is reused */
    EQ(IOleAdviseHolder_Unadvise(h, ca), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &ca), S_OK);
    EQ(ca, 1);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_advise_null_connection_pointer_ok) {
    IOleAdviseHolder *h; O2Sink s;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&s);
    EQ(IOleAdviseHolder_Advise(h, &s.i, NULL), S_OK);
    EQ(s.ref, 2);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_unadvise_errors) {
    IOleAdviseHolder *h; O2Sink s; DWORD c;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&s);
    EQ(IOleAdviseHolder_Unadvise(h, 0), O2_OLE_E_NOCONNECTION);
    EQ(IOleAdviseHolder_Unadvise(h, 1), O2_OLE_E_NOCONNECTION);
    EQ(IOleAdviseHolder_Advise(h, &s.i, &c), S_OK);
    EQ(IOleAdviseHolder_Unadvise(h, 0), O2_OLE_E_NOCONNECTION);          /* ids are 1-based */
    EQ(IOleAdviseHolder_Unadvise(h, 0xFFFFFFFF), O2_OLE_E_NOCONNECTION);
    EQ(IOleAdviseHolder_Unadvise(h, 0x80000001), O2_OLE_E_NOCONNECTION);
    EQ(IOleAdviseHolder_Unadvise(h, c + 50), O2_OLE_E_NOCONNECTION);
    EQ(s.ref, 2);
    EQ(IOleAdviseHolder_Unadvise(h, c), S_OK);
    EQ(s.ref, 1);
    EQ(IOleAdviseHolder_Unadvise(h, c), O2_OLE_E_NOCONNECTION);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_enumadvise) {
    IOleAdviseHolder *h; IEnumSTATDATA *e = (IEnumSTATDATA *)1; HRESULT hr;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK);
    hr = IOleAdviseHolder_EnumAdvise(h, &e);
    EQ(hr, O2_E_NOTIMPL);
    EQ(e, NULL);
    if (!t_is_wine()) EQ(IOleAdviseHolder_EnumAdvise(h, NULL), O2_E_INVALIDARG);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_sendonsave_and_close) {
    IOleAdviseHolder *h; O2Sink a, b; DWORD c;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&a); os_init(&b);
    EQ(IOleAdviseHolder_SendOnSave(h), S_OK);       /* no sinks yet */
    EQ(IOleAdviseHolder_SendOnClose(h), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &c), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &b.i, &c), S_OK);
    EQ(IOleAdviseHolder_SendOnSave(h), S_OK);
    EQ(a.onsave, 1); EQ(b.onsave, 1); EQ(a.onclose, 0); EQ(b.onclose, 0);
    EQ(IOleAdviseHolder_SendOnClose(h), S_OK);
    EQ(a.onclose, 1); EQ(b.onclose, 1); EQ(a.onsave, 1);
    EQ(IOleAdviseHolder_SendOnSave(h), S_OK);
    EQ(a.onsave, 2);
    EQ(a.ondata + a.onview + a.onrename, 0);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_send_after_unadvise) {
    IOleAdviseHolder *h; O2Sink a, b; DWORD ca, cb;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&a); os_init(&b);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &ca), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &b.i, &cb), S_OK);
    EQ(IOleAdviseHolder_Unadvise(h, ca), S_OK);
    EQ(IOleAdviseHolder_SendOnClose(h), S_OK);
    EQ(a.onclose, 0); EQ(b.onclose, 1);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_same_sink_twice_notified_twice) {
    IOleAdviseHolder *h; O2Sink a; DWORD c1, c2;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&a);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &c1), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &c2), S_OK);
    CHECK(c1 != c2);
    EQ(a.ref, 3);
    EQ(IOleAdviseHolder_SendOnSave(h), S_OK);
    EQ(a.onsave, 2);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_sendonrename) {
    IOleAdviseHolder *h; O2Sink a, b; DWORD c; IMoniker *mk = NULL; HRESULT hr;
    REQUIRE(U(CreateOleAdviseHolder)); REQUIRE(U(CreateItemMoniker));
    O2_BEGIN;
    EQ(mk_oah(&h), S_OK); os_init(&a); os_init(&b);
    hr = U(CreateItemMoniker)(L"!", L"olet_item", &mk);
    EQ(hr, S_OK);
    EQ(IOleAdviseHolder_SendOnRename(h, mk), S_OK);     /* no sinks */
    EQ(IOleAdviseHolder_Advise(h, &a.i, &c), S_OK);
    EQ(IOleAdviseHolder_Advise(h, &b.i, &c), S_OK);
    EQ(IOleAdviseHolder_SendOnRename(h, mk), S_OK);
    EQ(a.onrename, 1); EQ(b.onrename, 1);
    EQ(a.last_mk, mk); EQ(b.last_mk, mk);
    EQ(a.onsave + a.onclose, 0);
    IOleAdviseHolder_Release(h);
    IMoniker_Release(mk);
    O2_END();
}
TEST(o2oah_sendonrename_null_moniker) {
    IOleAdviseHolder *h; O2Sink a; DWORD c;
    if (t_is_wine()) { t_skip("emulator dereferences NULL interface pointers"); return; }
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK); os_init(&a);
    EQ(IOleAdviseHolder_SendOnRename(h, NULL), O2_E_INVALIDARG);
    EQ(IOleAdviseHolder_Advise(h, &a.i, &c), S_OK);
    EQ(IOleAdviseHolder_SendOnRename(h, NULL), O2_E_INVALIDARG);
    EQ(a.onrename, 0);
    IOleAdviseHolder_Release(h);
}
TEST(o2oah_many_advises_grow_table) {
    IOleAdviseHolder *h; O2Sink s[17]; DWORD c; int i;
    REQUIRE(U(CreateOleAdviseHolder));
    EQ(mk_oah(&h), S_OK);
    for (i = 0; i < O2_N(s); i++) { os_init(&s[i]); EQ(IOleAdviseHolder_Advise(h, &s[i].i, &c), S_OK); EQ(c, (DWORD)(i + 1)); }
    EQ(IOleAdviseHolder_SendOnClose(h), S_OK);
    for (i = 0; i < O2_N(s); i++) { EQ(s[i].onclose, 1); }
    IOleAdviseHolder_Release(h);
    for (i = 0; i < O2_N(s); i++) EQ(s[i].ref, 1);
}
