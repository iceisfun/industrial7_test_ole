/* OBJREF wire layout of standard / handler marshaling, parsed from the stream bytes. */
#include "mb.h"
#include <stdlib.h>

static IStream *marshal_obj(const GUID *iid, IUnknown *u, DWORD ctx, DWORD fl, MBObjRef *o, BYTE **bytes, ULONG *len, HRESULT *hr_out) {
    IStream *s = mb_stream();
    HRESULT hr = U(CoMarshalInterface)(s, iid, u, ctx, NULL, fl);
    if (hr_out) *hr_out = hr;
    *bytes = NULL; *len = 0; memset(o, 0, sizeof *o);
    if (FAILED(hr)) return s;
    *bytes = mb_dump(s, len);
    if (!mb_parse_objref(*bytes, *len, o)) t_note("objref did not parse (len %lu)", *len);
    return s;
}
static void done(IStream *s, BYTE *b) { if (U(CoReleaseMarshalData) && mb_size(s)) { mb_rewind(s); U(CoReleaseMarshalData)(s); } free(b); IStream_Release(s); }

TEST(mbasic_objref_standard_inproc_header) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
    IStream *s = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
    EQ(hr, S_OK);
    EQ(o.signature, MB_OBJREF_SIG);            /* 'MEOW' */
    EQ(o.flags, 1);                            /* OBJREF_STANDARD */
    CHECK(IsEqualGUID(&o.iid, &IID_IClassFactory));
    EQ(o.sflags, 0);                           /* STDOBJREF.flags (SORF_NULL) */
    EQ(o.crefs, 5);                            /* NORMAL marshaling hands out 5 public references */
    CHECK(o.oid != 0);
    CHECK(!IsEqualGUID(&o.ipid, &MB_ZERO));
    EQ(o.nentries, 0);                         /* in-proc: no string bindings */
    EQ(o.secoff, 0);
    EQ(n, o.consumed);                         /* no trailing bytes */
    t_note("INPROC std OBJREF is %lu bytes, oxid=0x%llx", n, o.oxid);
    done(s, b); MB_DONE();
}
TEST(mbasic_objref_iid_follows_requested_iid) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    const GUID *iids[] = { &IID_IUnknown, &IID_IClassFactory };
    for (int i = 0; i < 2; i++) {
        MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
        IStream *s = marshal_obj(iids[i], MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
        EQ(hr, S_OK);
        CHECK(IsEqualGUID(&o.iid, iids[i]));
        done(s, b);
    }
    MB_DONE();
}
TEST(mbasic_objref_same_object_same_oid_and_ipid) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    MBObjRef a, b2; BYTE *ba, *bb; ULONG na, nb; HRESULT h1, h2;
    IStream *sa = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &a, &ba, &na, &h1);
    IStream *sb = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &b2, &bb, &nb, &h2);
    EQ(h1, S_OK); EQ(h2, S_OK);
    EQ(a.oid, b2.oid);
    CHECK(IsEqualGUID(&a.ipid, &b2.ipid));
    EQ(a.oxid, b2.oxid);
    done(sa, ba); done(sb, bb); MB_DONE();
}
TEST(mbasic_objref_different_objects_different_oid) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    MBObjRef a, b2; BYTE *ba, *bb; ULONG na, nb; HRESULT h1, h2;
    IStream *sa = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &a, &ba, &na, &h1);
    IStream *sb = marshal_obj(&IID_IClassFactory, MB_UNK(1), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &b2, &bb, &nb, &h2);
    EQ(h1, S_OK); EQ(h2, S_OK);
    CHECK(a.oid != b2.oid);
    CHECK(!IsEqualGUID(&a.ipid, &b2.ipid));
    EQ(a.oxid, b2.oxid);                       /* same apartment -> same OXID */
    done(sa, ba); done(sb, bb); MB_DONE();
}
TEST(mbasic_objref_same_object_two_ifaces_same_oid) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    MBObjRef a, b2; BYTE *ba, *bb; ULONG na, nb; HRESULT h1, h2;
    IStream *sa = marshal_obj(&IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &a, &ba, &na, &h1);
    IStream *sb = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &b2, &bb, &nb, &h2);
    EQ(h1, S_OK); EQ(h2, S_OK);
    EQ(a.oid, b2.oid);
    CHECK(!IsEqualGUID(&a.ipid, &b2.ipid));     /* IPID is per (object, interface) */
    done(sa, ba); done(sb, bb); MB_DONE();
}

typedef struct { MBObjRef o; HRESULT hr; HANDLE ready, quit; ULONG n; } TObj;
static DWORD CALLBACK other_apt(void *p) {
    TObj *t = p; BYTE *b;
    MB_INIT_MTA();
    IStream *s = marshal_obj(&IID_IClassFactory, MB_UNK(1), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &t->o, &b, &t->n, &t->hr);
    SetEvent(t->ready);
    WaitForSingleObject(t->quit, 10000);
    done(s, b);
    MB_DONE();
    return 0;
}
TEST(mbasic_objref_oxid_differs_per_apartment) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    TObj t; memset(&t, 0, sizeof t); t.ready = CreateEventA(NULL, TRUE, FALSE, NULL); t.quit = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = mb_thread(other_apt, &t);
    CHECK(WaitForSingleObject(t.ready, 10000) == WAIT_OBJECT_0);
    MBObjRef a; BYTE *ba; ULONG na; HRESULT h;
    IStream *sa = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &a, &ba, &na, &h);
    EQ(h, S_OK); EQ(t.hr, S_OK);
    CHECK(a.oxid != t.o.oxid);
    CHECK(a.oid != t.o.oid);
    done(sa, ba);
    SetEvent(t.quit);
    CHECK(mb_join(th, 10000, NULL));
    CloseHandle(t.ready); CloseHandle(t.quit);
    MB_DONE();
}
TEST(mbasic_objref_table_flags_crefs) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    static const struct { const char *n; DWORD f; } t[] = { {"NORMAL", MSHLFLAGS_NORMAL}, {"TABLESTRONG", MSHLFLAGS_TABLESTRONG}, {"TABLEWEAK", MSHLFLAGS_TABLEWEAK}, {"NOPING", MSHLFLAGS_NOPING} };
    for (int i = 0; i < 4; i++) {
        MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
        IStream *s = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, t[i].f, &o, &b, &n, &hr);
        EQ(hr, S_OK);
        EQ(o.signature, MB_OBJREF_SIG); EQ(o.flags, 1);
        CHECK(o.oid != 0);
        t_note("%s: STDOBJREF.flags=0x%lx cPublicRefs=%lu size=%lu", t[i].n, o.sflags, o.crefs, n);
        done(s, b);
    }
    MB_DONE();
}
TEST(mbasic_objref_local_contexts) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM };
    for (int i = 0; i < 2; i++) {
        MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
        IStream *s = marshal_obj(&IID_IClassFactory, MB_UNK(0), ctxs[i], MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
        EQ(hr, S_OK);
        EQ(o.signature, MB_OBJREF_SIG); EQ(o.flags, 1);
        CHECK(IsEqualGUID(&o.iid, &IID_IClassFactory));
        EQ(o.crefs, 5);
        CHECK(o.oid != 0); CHECK(o.oxid != 0);
        EQ(n, o.consumed);
        t_note("ctx %lu: nentries=%u secoff=%u size=%lu", ctxs[i], o.nentries, o.secoff, n);
        done(s, b);
    }
    MB_DONE();
}
TEST(mbasic_objref_differentmachine_string_bindings) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
    IStream *s = marshal_obj(&IID_IClassFactory, MB_UNK(0), MSHCTX_DIFFERENTMACHINE, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
    EQ(hr, S_OK);
    EQ(o.flags, 1);
    EQ(n, o.consumed);
    if (o.nentries == 0) { t_note("DIFFERENTMACHINE OBJREF carries no string bindings"); }
    else {
        const USHORT *w = (const USHORT *)o.dsa;
        CHECK(o.nentries >= 4);
        CHECK(o.secoff >= 2 && o.secoff <= o.nentries);
        /* string bindings and security bindings are each terminated by a double NUL word */
        CHECK(w[o.nentries - 1] == 0 && w[o.nentries - 2] == 0);
        if (o.secoff >= 2) CHECK(w[o.secoff - 1] == 0 && w[o.secoff - 2] == 0);
        CHECK(w[0] != 0);                       /* first binding has a tower id */
        t_note("DIFFERENTMACHINE: nentries=%u secoff=%u first tower id=0x%x", o.nentries, o.secoff, w[0]);
    }
    done(s, b); MB_DONE();
}
TEST(mbasic_objref_position_after_marshal_is_end) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    IStream *s = mb_stream();
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    ULONGLONG n1 = mb_size(s);
    EQ(mb_pos(s), n1);
    EQ(U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    EQ(mb_size(s), 2 * n1);                      /* appended at the current position */
    mb_rewind(s);
    EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(mb_pos(s), n1);                            /* release data consumes exactly one OBJREF */
    EQ(U(CoReleaseMarshalData)(s), S_OK);
    EQ(mb_pos(s), 2 * n1);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_objref_unmarshal_consumes_exactly_one_objref) {
    REQUIRE(U(CoMarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    IStream *s = mb_stream(); IClassFactory *cf = NULL;
    U(CoMarshalInterface)(s, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    ULONGLONG n1 = mb_size(s);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&cf), S_OK);
    EQ(mb_pos(s), n1);
    if (cf) IClassFactory_Release(cf);
    IStream_Release(s); MB_DONE();
}
TEST(mbasic_objref_getmarshalsizemax_covers_nonempty_bindings) {
    REQUIRE(U(CoGetMarshalSizeMax)); mb_obj_reset(1); MB_INIT_STA();
    ULONG a = 0, b = 0;
    EQ(U(CoGetMarshalSizeMax)(&a, &IID_IClassFactory, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    EQ(U(CoGetMarshalSizeMax)(&b, &IID_IClassFactory, MB_UNK(0), MSHCTX_DIFFERENTMACHINE, NULL, MSHLFLAGS_NORMAL), S_OK);
    CHECK(b >= a);
    t_note("GetMarshalSizeMax INPROC=%lu DIFFERENTMACHINE=%lu", a, b);
    MB_DONE();
}
TEST(mbasic_objref_handler_via_stdmarshalinfo) {
    REQUIRE(U(CoMarshalInterface)); MB_INIT_STA();
    g_smi_calls = 0; g_smi_hr = S_OK; g_smi_clsid = MB_CLSID_Handler;
    MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
    IStream *s = marshal_obj(&IID_IUnknown, mb_smi_unk(), MSHCTX_LOCAL, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
    t_note("IStdMarshalInfo object, MSHCTX_LOCAL: hr=0x%lx flags=%lu GetClassForHandler calls=%d ctx=%lu", (unsigned long)hr, o.flags, g_smi_calls, g_smi_last_ctx);
    if (SUCCEEDED(hr)) {
        if (o.flags == 2) CHECK(IsEqualGUID(&o.clsid, &MB_CLSID_Handler));
        if (g_smi_calls) EQ(g_smi_last_ctx, MSHCTX_LOCAL);
        CHECK(o.flags == 1 || o.flags == 2);
        done(s, b);
    } else IStream_Release(s);
    MB_DONE();
}
TEST(mbasic_objref_handler_clsid_null_means_standard) {
    REQUIRE(U(CoMarshalInterface)); MB_INIT_STA();
    g_smi_calls = 0; g_smi_hr = S_OK; g_smi_clsid = MB_ZERO;
    MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
    IStream *s = marshal_obj(&IID_IUnknown, mb_smi_unk(), MSHCTX_LOCAL, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
    if (SUCCEEDED(hr)) { EQ(o.flags, 1); done(s, b); } else { t_note("hr=0x%lx", (unsigned long)hr); IStream_Release(s); }
    MB_DONE();
}
TEST(mbasic_objref_handler_inproc_context_uses_standard) {
    REQUIRE(U(CoMarshalInterface)); MB_INIT_STA();
    g_smi_calls = 0; g_smi_hr = S_OK; g_smi_clsid = MB_CLSID_Handler;
    MBObjRef o; BYTE *b; ULONG n; HRESULT hr;
    IStream *s = marshal_obj(&IID_IUnknown, mb_smi_unk(), MSHCTX_INPROC, MSHLFLAGS_NORMAL, &o, &b, &n, &hr);
    EQ(hr, S_OK);
    t_note("INPROC with IStdMarshalInfo: flags=%lu calls=%d", o.flags, g_smi_calls);
    CHECK(o.flags == 1 || o.flags == 2);
    done(s, b); MB_DONE();
}
TEST(mbasic_objref_craft_unmarshal_with_different_iid_field) {
    REQUIRE(U(CoMarshalInterface)); REQUIRE(U(CoUnmarshalInterface)); mb_obj_reset(1); MB_INIT_STA();
    /* The IID in the OBJREF only names the interface that was marshaled; the caller's riid decides what is returned. */
    IStream *s = mb_stream(); IUnknown *u = NULL;
    EQ(U(CoMarshalInterface)(s, &IID_IUnknown, MB_UNK(0), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL), S_OK);
    mb_rewind(s);
    EQ(U(CoUnmarshalInterface)(s, &IID_IClassFactory, (void **)&u), S_OK);
    if (u) IUnknown_Release(u);
    IStream_Release(s); MB_DONE();
}
