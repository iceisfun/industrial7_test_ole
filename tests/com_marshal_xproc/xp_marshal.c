/* Marshaling API surface: argument validation, stream formats (OBJREF), table marshaling,
 * custom marshalers, the free-threaded marshaler, external connections and the global interface table.
 * (decomp) = taken from the Windows 10 combase.dll decompilation. */
#include "xp.h"

#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()
#define RPC_E_INVALID_OBJREF_ ((HRESULT)0x8001011D)

static IStream *mk_stream_with(const void *data, ULONG len)
{
    IStream *s = xp_new_stream(); ULONG w;
    if (s && len) IStream_Write(s, data, len, &w);
    if (s) xp_rewind(s);
    return s;
}

/* ---- CoMarshalInterface argument validation -------------------------------------------------------- */
XT(mar_marshal_interface_invalid_args) {
    IStream *s; XpObj *o; HRESULT hr; STATSTG st;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(NULL, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, E_INVALIDARG);                                               /* (decomp) */
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), 6, NULL, MSHLFLAGS_NORMAL);         /* dwDestContext >= 6 */
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), 0x1234, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, 0x40);         /* reserved flag bits */
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, 0x80);
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, 0x80000);
    XEQ(hr, E_INVALIDARG);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, 0x40000000);
    XEQ(hr, E_INVALIDARG);
    IStream_Stat(s, &st, STATFLAG_NONAME);
    XEQ(st.cbSize.QuadPart, 0);                                          /* nothing was written by the failed calls */
    XEQ(o->ref, 1);                                                      /* ... and no stub keeps the object */
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_marshal_interface_dest_context_pointer_only_for_differentmachine) {
    IStream *s; XpObj *o; HRESULT hr; static const int dummy = 0;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, (void *)&dummy, MSHLFLAGS_NORMAL);
    CHECKF(FAILED(hr), "pvDestContext with MSHCTX_INPROC returned %08lx", hr);           /* (decomp) only MSHCTX_DIFFERENTMACHINE may carry one */
    t_note("CoMarshalInterface(MSHCTX_INPROC, pvDestContext != NULL): %08lx (%s)", hr, xp_hrname(hr));
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_LOCAL, (void *)&dummy, MSHLFLAGS_NORMAL);
    CHECKF(FAILED(hr), "pvDestContext with MSHCTX_LOCAL returned %08lx", hr);
    XEQ(o->ref, 1);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_marshal_all_contexts_and_valid_flags) {
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC, MSHCTX_CROSSCTX };
    static const DWORD flags[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK, MSHLFLAGS_NOPING, MSHLFLAGS_NORMAL | MSHLFLAGS_NOPING };
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    for (unsigned c = 0; c < 5; c++) for (unsigned f = 0; f < 5; f++) {
        IStream *s = xp_new_stream(); XpObj *o = xp_obj_new(); HRESULT hr; unsigned char hdr[8]; ULONG n = 0;
        hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), ctxs[c], NULL, flags[f]);
        CHECKF(hr == S_OK, "ctx %lu flags %lu: %08lx", ctxs[c], flags[f], hr);
        if (hr == S_OK) {
            xp_rewind(s);
            IStream_Read(s, hdr, 8, &n);
            XEQ(n, 8);
            CHECKF(!memcmp(hdr, "MEOW", 4), "ctx %lu flags %lu: stream does not start with MEOW", ctxs[c], flags[f]);
            xp_rewind(s);
            hr = U(CoReleaseMarshalData)(s);
            XEQ(hr, S_OK);
            XEQ(o->ref, 1);
        }
        IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    }
    UNINIT();
}

/* ---- CoUnmarshalInterface / CoReleaseMarshalData -------------------------------------------------------- */
XT(mar_unmarshal_invalid_args) {
    IStream *s; void *pv = (void *)1; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream();
    hr = U(CoUnmarshalInterface)(NULL, &XP_IID_IClassFactory, &pv);
    XEQ(hr, E_INVALIDARG);                                               /* (decomp) */
    CHECK(pv == (void *)1);                                              /* argument errors precede the *ppv = NULL store */
    hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, NULL);
    XEQ(hr, E_INVALIDARG);
    IStream_Release(s);
    UNINIT();
}
XT(mar_unmarshal_failure_clears_out_pointer) {
    IStream *s; void *pv = (void *)1; HRESULT hr; char junk[64];
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    memset(junk, 0x5A, sizeof junk);
    s = mk_stream_with(junk, sizeof junk);
    hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, &pv);
    CHECK(FAILED(hr));
    CHECK(pv == NULL);                                                   /* (decomp) cleared before the stream is read */
    IStream_Release(s);
    UNINIT();
}
static DWORD uninit_unmarshal_proc(void *p)
{
    IStream *s = p; void *pv = (void *)1; HRESULT hr; XpObj *o = xp_obj_new();
    IStream *s2 = xp_new_stream();
    hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, &pv);
    if (hr != XP_CO_E_NOTINITIALIZED || pv != NULL) return 1;
    hr = U(CoMarshalInterface)(s2, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    if (hr != XP_CO_E_NOTINITIALIZED) return 2;
    hr = U(CoMarshalInterThreadInterfaceInStream)(&XP_IID_IClassFactory, XP_OBJ_UNK(o), (IStream **)&pv);
    if (hr != XP_CO_E_NOTINITIALIZED) return 3;
    IStream_Release(s2); IClassFactory_Release(XP_OBJ_CF(o));
    return 0;
}
XT(mar_calls_in_uninitialized_thread_are_refused) {
    XpObj *o; IStream *s; HRESULT hr;
    XP_NEED_BASE(); NEED(CoMarshalInterThreadInterfaceInStream); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new(); s = xp_new_stream();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    xp_rewind(s);
    XEQ(xp_thread_run(-1, uninit_unmarshal_proc, s, 15000), 0);          /* (decomp) InitChannelIfNecessary -> CO_E_NOTINITIALIZED */
    xp_rewind(s); U(CoReleaseMarshalData)(s);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

typedef struct { const char *what; DWORD sig, flags; ULONG extra; HRESULT want; } BadRef;
XT(mar_unmarshal_malformed_objref) {
    static const BadRef cases[] = {
        { "bad signature",              0x12345678, 1, 64, (HRESULT)0x8001011D },
        { "signature byte swapped",     0x4D454F57, 1, 64, (HRESULT)0x8001011D },
        { "flags zero",                 0x574F454D, 0, 64, (HRESULT)0x8001011D },
        { "flags 0x10",                 0x574F454D, 0x10, 64, (HRESULT)0x8001011D },
        { "flags standard|handler",     0x574F454D, 3, 64, (HRESULT)0x8001011D },
        { "flags standard|custom",      0x574F454D, 5, 64, (HRESULT)0x8001011D },
        { "flags standard|extended",    0x574F454D, 9, 64, (HRESULT)0x8001011D },
        { "flags handler|custom",       0x574F454D, 6, 64, (HRESULT)0x8001011D },
        { "flags all",                  0x574F454D, 15, 64, (HRESULT)0x8001011D },
        { "extended on a plain stream", 0x574F454D, 8, 64, (HRESULT)0x8001011D },
        { "standard but truncated",     0x574F454D, 1, 8, (HRESULT)0x8003001E },     /* STG_E_READFAULT: header ok, body short */
        { "handler but truncated",      0x574F454D, 2, 8, (HRESULT)0x8003001E },
        { "custom but truncated",       0x574F454D, 4, 8, (HRESULT)0x8003001E },
    };
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    for (unsigned i = 0; i < sizeof cases / sizeof *cases; i++) {
        unsigned char buf[24 + 128]; IStream *s; void *pv = (void *)1; HRESULT hr; GUID iid = XP_IID_IClassFactory;
        memset(buf, 0, sizeof buf);
        memcpy(buf, &cases[i].sig, 4); memcpy(buf + 4, &cases[i].flags, 4); memcpy(buf + 8, &iid, 16);
        s = mk_stream_with(buf, 24 + cases[i].extra);
        hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, &pv);
        CHECKF(hr == cases[i].want, "%s: got %08lx want %08lx", cases[i].what, hr, cases[i].want);
        CHECK(pv == NULL);
        IStream_Release(s);
    }
    UNINIT();
}
XT(mar_unmarshal_short_stream) {
    static const ULONG lens[] = { 0, 1, 4, 8, 16, 23 };
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        unsigned char buf[32]; IStream *s; void *pv = (void *)1; HRESULT hr;
        memset(buf, 0, sizeof buf);
        if (lens[i] >= 4) { DWORD sig = 0x574F454D; memcpy(buf, &sig, 4); }
        s = mk_stream_with(buf, lens[i]);
        hr = U(CoUnmarshalInterface)(s, &XP_IID_IClassFactory, &pv);
        CHECKF(hr == (HRESULT)0x8003001E, "stream of %lu bytes: %08lx", lens[i], hr);     /* STG_E_READFAULT (decomp: short read of the 0x18 byte header) */
        CHECK(pv == NULL);
        xp_rewind(s);
        hr = U(CoReleaseMarshalData)(s);
        CHECKF(hr == (HRESULT)0x8003001E, "CoReleaseMarshalData on %lu bytes: %08lx", lens[i], hr);
        IStream_Release(s);
    }
    UNINIT();
}
XT(mar_release_marshal_data_at_end_of_stream) {
    IStream *s; XpObj *o; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);   /* leaves the seek pointer at the end */
    XEQ(hr, S_OK);
    hr = U(CoReleaseMarshalData)(s);
    XEQ(hr, STG_E_READFAULT);                                             /* nothing to read there */
    CHECK(o->ref > 1);                                                    /* ... and so the stub reference is still held */
    xp_rewind(s);
    hr = U(CoReleaseMarshalData)(s);
    XEQ(hr, S_OK);
    XEQ(o->ref, 1);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_unmarshal_wrong_iid_same_apartment) {
    IStream *s; XpObj *o; HRESULT hr; void *pv = (void *)1;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(s, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    xp_rewind(s);
    hr = U(CoUnmarshalInterface)(s, &XP_IID_IUnknownBogus, &pv);
    XEQ(hr, E_NOINTERFACE);
    CHECK(pv == NULL);
    XEQ(o->ref, 1);                                                      /* a failed unmarshal still consumes the marshal reference */
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

/* ---- stream layout ---------------------------------------------------------------------------------------- */
typedef struct { DWORD sig, flags; GUID iid; DWORD sflags, crefs; unsigned long long oxid, oid; GUID ipid; } StdHdr;
static BOOL read_std(IStream *s, StdHdr *h, WORD *nents, WORD *secoff)
{
    unsigned char b[68]; ULONG n = 0;
    xp_rewind(s);
    IStream_Read(s, b, sizeof b, &n);
    if (n < sizeof b) return FALSE;
    memcpy(&h->sig, b, 4); memcpy(&h->flags, b + 4, 4); memcpy(&h->iid, b + 8, 16);
    memcpy(&h->sflags, b + 24, 4); memcpy(&h->crefs, b + 28, 4); memcpy(&h->oxid, b + 32, 8); memcpy(&h->oid, b + 40, 8); memcpy(&h->ipid, b + 48, 16);
    memcpy(nents, b + 64, 2); memcpy(secoff, b + 66, 2);
    return TRUE;
}
XT(mar_inproc_standard_objref_layout) {
    IStream *s; XpObj *o; StdHdr h; WORD n, so; HRESULT hr; STATSTG st;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    if (read_std(s, &h, &n, &so)) {
        XEQ(h.sig, 0x574F454D); XEQ(h.flags, 1);
        CHECK(IsEqualGUID(&h.iid, &XP_IID_IClassFactory));
        XEQ(h.sflags, 0);                                                /* NORMAL: no SORF_* flags */
        XEQ(h.crefs, 5);                                                 /* public references handed out per marshal */
        CHECK(h.oxid != 0); CHECK(h.oid != 0);
        t_note("in-process OBJREF: wNumEntries=%u wSecurityOffset=%u", n, so);
        IStream_Stat(s, &st, STATFLAG_NONAME);
        t_note("in-process OBJREF total size %llu", st.cbSize.QuadPart);
    } else CHECK(0);
    xp_rewind(s); U(CoReleaseMarshalData)(s);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_objref_identity_is_stable_per_object) {
    IStream *s1, *s2; XpObj *o, *o2; StdHdr a, b, c; WORD n, so; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s1 = xp_new_stream(); s2 = xp_new_stream(); o = xp_obj_new(); o2 = xp_obj_new();
    hr = U(CoMarshalInterface)(s1, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL); XEQ(hr, S_OK);
    hr = U(CoMarshalInterface)(s2, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL); XEQ(hr, S_OK);
    if (read_std(s1, &a, &n, &so) && read_std(s2, &b, &n, &so)) {
        XEQ(a.oid, b.oid);                                               /* one OID per object identity ... */
        XEQ(a.oxid, b.oxid);                                             /* ... in one OXID (apartment) */
        CHECK(!IsEqualGUID(&a.ipid, &b.ipid));                           /* ... and one IPID per marshaled interface */
    } else CHECK(0);
    xp_rewind(s1); U(CoReleaseMarshalData)(s1); xp_rewind(s2); U(CoReleaseMarshalData)(s2);
    {
        IStream *s3 = xp_new_stream();
        hr = U(CoMarshalInterface)(s3, &XP_IID_IClassFactory, XP_OBJ_UNK(o2), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL); XEQ(hr, S_OK);
        if (read_std(s3, &c, &n, &so) && read_std(s1, &a, &n, &so)) {
            CHECK(c.oid != a.oid); XEQ(c.oxid, a.oxid);                  /* a different object, same apartment */
        }
        xp_rewind(s3); U(CoReleaseMarshalData)(s3); IStream_Release(s3);
    }
    IStream_Release(s1); IStream_Release(s2);
    IClassFactory_Release(XP_OBJ_CF(o)); IClassFactory_Release(XP_OBJ_CF(o2));
    UNINIT();
}
XT(mar_noping_flag_recorded_in_stdobjref) {
    IStream *s; XpObj *o; StdHdr h; WORD n, so; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_LOCAL, NULL, MSHLFLAGS_NOPING);
    XEQ(hr, S_OK);
    if (read_std(s, &h, &n, &so)) {
        CHECKF((h.sflags & 0x1000) != 0, "SORF_NOPING not set (std flags %08lx)", h.sflags);
        t_note("NOPING std flags=%08lx cPublicRefs=%lu", h.sflags, h.crefs);
    } else CHECK(0);
    xp_rewind(s); U(CoReleaseMarshalData)(s);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_get_marshal_size_max_bounds_actual_size) {
    static const DWORD flags[] = { MSHLFLAGS_NORMAL, MSHLFLAGS_TABLESTRONG, MSHLFLAGS_TABLEWEAK };
    static const DWORD ctxs[] = { MSHCTX_INPROC, MSHCTX_LOCAL, MSHCTX_DIFFERENTMACHINE };
    XP_NEED_BASE(); NEED(CoGetMarshalSizeMax); INIT(COINIT_APARTMENTTHREADED);
    for (int c = 0; c < 3; c++) for (int f = 0; f < 3; f++) {
        DWORD max = 0; IStream *s = xp_new_stream(); XpObj *o = xp_obj_new(); HRESULT hr; STATSTG st;
        hr = U(CoGetMarshalSizeMax)(&max, &XP_IID_IClassFactory, XP_OBJ_UNK(o), ctxs[c], NULL, flags[f]);
        XEQ(hr, S_OK);
        hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), ctxs[c], NULL, flags[f]);
        XEQ(hr, S_OK);
        IStream_Stat(s, &st, STATFLAG_NONAME);
        CHECKF(st.cbSize.QuadPart <= max, "ctx %lu flags %lu: wrote %llu bytes, size max %lu", ctxs[c], flags[f], st.cbSize.QuadPart, max);
        CHECKF(max >= 68, "size max %lu is smaller than a standard OBJREF header", max);
        xp_rewind(s); U(CoReleaseMarshalData)(s);
        IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    }
    UNINIT();
}

/* ---- CoGetStandardMarshal --------------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
XT(mar_get_standard_marshal_inproc_roundtrip) {
    IMarshal *m = NULL; IStream *s; XpObj *o; HRESULT hr; CLSID clsid; DWORD size = 0, size2 = 0; StdHdr h; WORD n, so; IUnknown *unk = NULL; ULONG rc;
    XP_NEED_BASE(); NEED(CoGetStandardMarshal); NEED(CoGetMarshalSizeMax); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new();
    hr = U(CoGetStandardMarshal)(&XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &m);
    XEQ(hr, S_OK);
    if (m) {
        hr = IMarshal_GetUnmarshalClass(m, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &clsid);
        XEQ(hr, S_OK); CHECK(IsEqualGUID(&clsid, &XP_CLSID_StdMarshal));
        hr = IMarshal_GetMarshalSizeMax(m, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &size);
        XEQ(hr, S_OK);
        hr = U(CoGetMarshalSizeMax)(&size2, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK); XEQ(size, size2);
        hr = IMarshal_MarshalInterface(m, s, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        if (read_std(s, &h, &n, &so)) {
            XEQ(h.sig, 0x574F454D); XEQ(h.flags, 1); CHECK(IsEqualGUID(&h.iid, &XP_IID_IUnknown));
            XEQ(h.sflags, 0); XEQ(h.crefs, 5); t_note("inproc string bindings=%u secoff=%u", (unsigned)n, (unsigned)so);
        } else CHECK(0);
        xp_rewind(s);
        hr = IMarshal_UnmarshalInterface(m, s, &XP_IID_IUnknown, (void **)&unk);
        XEQ(hr, S_OK);
        XEQ(unk, XP_OBJ_UNK(o));
        if (unk) IUnknown_Release(unk);
        /* Win10: the stub manager keeps the object referenced after the in-proc unmarshal (public references of the
           marshal data are not all returned); releasing a second marshal data below tears it down */
        XEQ(o->ref, 2);
        xp_rewind(s);
        hr = IMarshal_MarshalInterface(m, s, &XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK); CHECK(o->ref > 1);
        xp_rewind(s);
        hr = IMarshal_ReleaseMarshalData(m, s);
        XEQ(hr, S_OK); XEQ(o->ref, 1);
        rc = IMarshal_Release(m);
        (void)rc;
    }
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_get_standard_marshal_unmarshal_class_per_context) {
    static const DWORD ctxs[] = { MSHCTX_LOCAL, MSHCTX_NOSHAREDMEM, MSHCTX_DIFFERENTMACHINE, MSHCTX_INPROC, MSHCTX_CROSSCTX };
    XP_NEED_BASE(); NEED(CoGetStandardMarshal); INIT(COINIT_APARTMENTTHREADED);
    for (int i = 0; i < 5; i++) {
        IMarshal *m = NULL; XpObj *o = xp_obj_new(); CLSID clsid; HRESULT hr;
        hr = U(CoGetStandardMarshal)(&XP_IID_IUnknown, XP_OBJ_UNK(o), ctxs[i], NULL, MSHLFLAGS_NORMAL, &m);
        CHECKF(hr == S_OK, "ctx %lu: %08lx", ctxs[i], hr);
        if (m) {
            hr = IMarshal_GetUnmarshalClass(m, &XP_IID_IUnknown, XP_OBJ_UNK(o), ctxs[i], NULL, MSHLFLAGS_NORMAL, &clsid);
            XEQ(hr, S_OK);
            if (ctxs[i] == MSHCTX_CROSSCTX)                      /* Win10: cross-context marshaling is unmarshaled by its own class {00000350-...} */
                CHECKF(clsid.Data1 == 0x350 && clsid.Data2 == 0 && clsid.Data3 == 0, "ctx %lu unmarshal class {%08lx-...}", ctxs[i], clsid.Data1);
            else
                CHECKF(IsEqualGUID(&clsid, &XP_CLSID_StdMarshal), "ctx %lu unmarshal class {%08lx-...}", ctxs[i], clsid.Data1);
            IMarshal_Release(m);
        }
        IClassFactory_Release(XP_OBJ_CF(o));
    }
    UNINIT();
}
XT(mar_get_standard_marshal_args) {
    IMarshal *m = (IMarshal *)1; HRESULT hr; XpObj *o;
    XP_NEED_BASE(); NEED(CoGetStandardMarshal); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new();
    if (!t_is_wine()) { hr = U(CoGetStandardMarshal)(&XP_IID_IUnknown, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, NULL);
    XEQ(hr, E_INVALIDARG); }                                             /* (decomp) */
    hr = U(CoGetStandardMarshal)(&XP_IID_IUnknown, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &m);
    XEQ(hr, S_OK);                                                       /* (decomp) NULL object: the shared unmarshaler */
    CHECK(m != NULL && m != (IMarshal *)1);
    if (m && m != (IMarshal *)1) {
        CLSID c;
        hr = IMarshal_GetUnmarshalClass(m, &XP_IID_IUnknown, NULL, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &c);
        t_note("static unmarshaler GetUnmarshalClass: %08lx", hr);
        IMarshal_Release(m);
    }
    m = (IMarshal *)1;
    hr = U(CoGetStandardMarshal)(&XP_IID_IUnknown, XP_OBJ_UNK(o), 99, NULL, MSHLFLAGS_NORMAL, &m);
    t_note("CoGetStandardMarshal with dwDestContext 99: %08lx (%s)", hr, xp_hrname(hr));
    if (SUCCEEDED(hr) && m) IMarshal_Release(m);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

/* ---- custom marshaler ---------------------------------------------------------------------------------------------- */
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
typedef struct {
    IMarshal iface; LONG ref; const CLSID *unmarshalClass;
    LONG nGetClass, nSize, nMarshal, nUnmarshal, nRelease, nDisconnect;
    GUID gotRiid; DWORD gotCtx, gotFlags, gotCtx2, gotFlags2; void *gotPvCtx; ULONGLONG streamSizeAtMarshal; LONG order[4]; LONG seq;
} CM;
static CM *CMo(IMarshal *m) { return (CM *)m; }
static HRESULT WINAPI cm_qi(IMarshal *m, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &XP_IID_IUnknown) || IsEqualGUID(riid, &XP_IID_IMarshal)) { *ppv = m; InterlockedIncrement(&CMo(m)->ref); return S_OK; }
    *ppv = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI cm_addref(IMarshal *m) { return InterlockedIncrement(&CMo(m)->ref); }
static ULONG WINAPI cm_release(IMarshal *m) { return InterlockedDecrement(&CMo(m)->ref); }
static HRESULT WINAPI cm_getclass(IMarshal *m, REFIID riid, void *pv, DWORD ctx, void *pctx, DWORD fl, CLSID *clsid)
{
    CM *c = CMo(m); LONG n = InterlockedIncrement(&c->nGetClass);
    if (n == 1) { c->order[0] = InterlockedIncrement(&c->seq); c->gotRiid = *riid; c->gotCtx = ctx; c->gotFlags = fl; c->gotPvCtx = pctx; }
    else { c->order[3] = InterlockedIncrement(&c->seq); c->gotCtx2 = ctx; c->gotFlags2 = fl; }
    *clsid = *c->unmarshalClass; return S_OK;
}
static HRESULT WINAPI cm_size(IMarshal *m, REFIID riid, void *pv, DWORD ctx, void *pctx, DWORD fl, DWORD *size)
{ CM *c = CMo(m); c->order[1] = InterlockedIncrement(&c->seq); InterlockedIncrement(&c->nSize); *size = 0; return S_OK; }
static HRESULT WINAPI cm_marshal(IMarshal *m, IStream *s, REFIID riid, void *pv, DWORD ctx, void *pctx, DWORD fl)
{
    CM *c = CMo(m); IMarshal *std = NULL; STATSTG st; HRESULT hr;
    c->order[2] = InterlockedIncrement(&c->seq); InterlockedIncrement(&c->nMarshal);
    IStream_Stat(s, &st, STATFLAG_NONAME); c->streamSizeAtMarshal = st.cbSize.QuadPart;
    if (c->unmarshalClass != &XP_CLSID_StdMarshal) return S_OK;
    hr = U(CoGetStandardMarshal)(riid, (IUnknown *)m, ctx, NULL, fl, &std);
    if (SUCCEEDED(hr)) { hr = IMarshal_MarshalInterface(std, s, riid, pv, ctx, pctx, fl); IMarshal_Release(std); }
    return hr;
}
static HRESULT WINAPI cm_unmarshal(IMarshal *m, IStream *s, REFIID riid, void **ppv) { InterlockedIncrement(&CMo(m)->nUnmarshal); return E_NOTIMPL; }
static HRESULT WINAPI cm_relmd(IMarshal *m, IStream *s) { InterlockedIncrement(&CMo(m)->nRelease); return E_NOTIMPL; }
static HRESULT WINAPI cm_disc(IMarshal *m, DWORD r) { InterlockedIncrement(&CMo(m)->nDisconnect); return E_NOTIMPL; }
static IMarshalVtbl cm_vtbl = { cm_qi, cm_addref, cm_release, cm_getclass, cm_size, cm_marshal, cm_unmarshal, cm_relmd, cm_disc };
static void cm_init(CM *c, const CLSID *cls) { memset(c, 0, sizeof *c); c->iface.lpVtbl = &cm_vtbl; c->ref = 1; c->unmarshalClass = cls; }

XT(mar_custom_marshaler_delegating_to_std) {
    CM c; IStream *s; HRESULT hr; IUnknown *unk = NULL; DWORD size = 0;
    XP_NEED_BASE(); NEED(CoGetStandardMarshal); NEED(CoGetMarshalSizeMax); INIT(COINIT_APARTMENTTHREADED);
    cm_init(&c, &XP_CLSID_StdMarshal);
    s = xp_new_stream();
    hr = U(CoMarshalInterface)(s, &XP_IID_IUnknown, (IUnknown *)&c.iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    /* Win10 asks for the unmarshal class twice: first from CoMarshalInterface (with the caller's context), again from the
       standard marshaler when the delegating IMarshal::MarshalInterface runs (zero context and flags) */
    XEQ(c.nGetClass, 2); XEQ(c.nMarshal, 1);
    XEQ(c.nSize, 0);                                                     /* the standard-marshal class path never asks for the size */
    CHECK(c.order[0] < c.order[2] && c.order[2] < c.order[3]);           /* class, then the data, then the class again */
    XEQ(c.streamSizeAtMarshal, 0);                                       /* IMarshal::MarshalInterface receives an empty stream */
    CHECK(IsEqualGUID(&c.gotRiid, &XP_IID_IUnknown));
    XEQ(c.gotCtx, MSHCTX_INPROC); XEQ(c.gotFlags, MSHLFLAGS_NORMAL);
    XEQ(c.gotCtx2, 0); XEQ(c.gotFlags2, 0);
    xp_rewind(s);
    hr = U(CoUnmarshalInterface)(s, &XP_IID_IUnknown, (void **)&unk);
    XEQ(hr, S_OK);
    XEQ(unk, (IUnknown *)&c.iface);                                      /* same apartment: the object comes back as is */
    if (unk) IUnknown_Release(unk);
    XEQ(c.nUnmarshal, 0);                                                /* the custom object is not asked to unmarshal */
    IStream_Release(s);
    /* ReleaseMarshalData of such data goes to the standard marshaler too */
    s = xp_new_stream();
    hr = U(CoMarshalInterface)(s, &XP_IID_IUnknown, (IUnknown *)&c.iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL); XEQ(hr, S_OK);
    xp_rewind(s);
    hr = U(CoReleaseMarshalData)(s); XEQ(hr, S_OK);
    XEQ(c.nRelease, 0);
    IStream_Release(s);
    c.nSize = 0;
    hr = U(CoGetMarshalSizeMax)(&size, &XP_IID_IUnknown, (IUnknown *)&c.iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    XEQ(c.nSize, 1);
    XEQ(size, 88);                                                       /* sizeof(OBJREF): 24 byte header + 64 byte union (handler variant) */
    UNINIT();
}
XT(mar_custom_marshaler_objref_custom_layout) {
    CM c; IStream *s; HRESULT hr; unsigned char b[48]; ULONG n = 0; static const GUID dfmarshal = { 0x0000030B, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
    DWORD sig, flags, cbext, sz; GUID iid, clsid;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    cm_init(&c, &dfmarshal);
    s = xp_new_stream();
    hr = U(CoMarshalInterface)(s, &XP_IID_IUnknown, (IUnknown *)&c.iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    XEQ(c.nGetClass, 1); XEQ(c.nSize, 1); XEQ(c.nMarshal, 1);           /* any other unmarshal class: size is queried as well */
    CHECK(c.order[0] < c.order[1] && c.order[1] < c.order[2]);
    xp_rewind(s);
    IStream_Read(s, b, sizeof b, &n);
    XEQ(n, 48);                                                          /* header (24) + CLSID + cbExtension + size, custom data empty */
    memcpy(&sig, b, 4); memcpy(&flags, b + 4, 4); memcpy(&iid, b + 8, 16); memcpy(&clsid, b + 24, 16); memcpy(&cbext, b + 40, 4); memcpy(&sz, b + 44, 4);
    XEQ(sig, 0x574F454D); XEQ(flags, 4);                                 /* OBJREF_CUSTOM */
    CHECK(IsEqualGUID(&iid, &XP_IID_IUnknown)); CHECK(IsEqualGUID(&clsid, &dfmarshal));
    XEQ(cbext, 0); XEQ(sz, 0);
    IStream_Release(s);
    UNINIT();
}
XT(mar_custom_marshaler_failure_propagates) {
    CM c; IStream *s; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    cm_init(&c, &XP_CLSID_StdMarshal);
    c.unmarshalClass = &XP_CLSID_StdMarshal;
    s = xp_new_stream();
    /* an interface the (std-delegating) marshaler cannot marshal: made-up IID without proxy/stub registration */
    hr = U(CoMarshalInterface)(s, &XP_IID_IUnknownBogus, (IUnknown *)&c.iface, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    t_note("marshal of an unregistered IID through a delegating custom marshaler: %08lx (%s)", hr, xp_hrname(hr));
    XEQ(c.nGetClass, 2);                                                 /* as above: CoMarshalInterface, then the standard marshaler */
    IStream_Release(s);
    UNINIT();
}

/* ---- free-threaded marshaler ------------------------------------------------------------------------------------ */
/* adapted from Wine dlls/ole32/tests/marshal.c (LGPL-2.1+) */
XT(mar_ftm_inproc_stream_format) {
    IUnknown *ft = NULL; IMarshal *fm = NULL; IStream *s; XpObj *o; HRESULT hr; DWORD size = 0; CLSID clsid; STATSTG st; unsigned char b[28]; ULONG n = 0;
    static const GUID inproc_ftm = { 0x0000033A, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
    unsigned long long ptr = 0; DWORD fl = 0xFFFF;
    XP_NEED_BASE(); NEED(CoCreateFreeThreadedMarshaler); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new();
    hr = U(CoCreateFreeThreadedMarshaler)(NULL, &ft);
    XEQ(hr, S_OK);
    if (ft) hr = IUnknown_QueryInterface(ft, &XP_IID_IMarshal, (void **)&fm);
    XEQ(hr, S_OK);
    if (fm) {
        s = xp_new_stream();
        hr = IMarshal_GetMarshalSizeMax(fm, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &size);
        XEQ(hr, S_OK); XEQ(size, 4 + 8 + 16);
        hr = IMarshal_GetUnmarshalClass(fm, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL, &clsid);
        XEQ(hr, S_OK); CHECK(IsEqualGUID(&clsid, &inproc_ftm));
        hr = IMarshal_MarshalInterface(fm, s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        IStream_Stat(s, &st, STATFLAG_NONAME);
        XEQ(st.cbSize.QuadPart, 28);
        xp_rewind(s); IStream_Read(s, b, sizeof b, &n); XEQ(n, 28);
        memcpy(&fl, b, 4); memcpy(&ptr, b + 4, 8);
        XEQ(fl, MSHLFLAGS_NORMAL);                                       /* the raw interface pointer travels in the stream */
        XEQ(ptr, (ULONG_PTR)XP_OBJ_CF(o));
        CHECK(o->ref > 1);                                               /* NORMAL marshaling AddRefs the object */
        {
            IUnknown *pu = NULL;
            xp_rewind(s);
            hr = IMarshal_UnmarshalInterface(fm, s, &XP_IID_IUnknown, (void **)&pu);
            XEQ(hr, S_OK);
            XEQ(pu, XP_OBJ_UNK(o));
            if (pu) IUnknown_Release(pu);
            XEQ(o->ref, 1);                                              /* ... and unmarshaling takes that reference over */
        }
        /* table-weak: no reference taken */
        xp_rewind(s);
        hr = IMarshal_MarshalInterface(fm, s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLEWEAK);
        XEQ(hr, S_OK); XEQ(o->ref, 1);
        /* table-strong: one reference until the data is released */
        xp_rewind(s);
        hr = IMarshal_MarshalInterface(fm, s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLESTRONG);
        XEQ(hr, S_OK); CHECK(o->ref > 1);
        xp_rewind(s);
        hr = IMarshal_ReleaseMarshalData(fm, s);
        XEQ(hr, S_OK); XEQ(o->ref, 1);
        hr = IMarshal_DisconnectObject(fm, 0);
        XEQ(hr, S_OK);                                                   /* a no-op for the FTM */
        /* local context: the standard marshaler produces the data */
        hr = IMarshal_GetUnmarshalClass(fm, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL, &clsid);
        XEQ(hr, S_OK);
        CHECKF(IsEqualGUID(&clsid, &XP_CLSID_StdMarshal) || clsid.Data1 != inproc_ftm.Data1, "local context unmarshal class {%08lx-...}", clsid.Data1);
        t_note("FTM unmarshal class for MSHCTX_LOCAL: {%08lx-%04x-%04x-...}", clsid.Data1, clsid.Data2, clsid.Data3);
        xp_rewind(s);
        hr = IMarshal_MarshalInterface(fm, s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
        XEQ(hr, S_OK);
        xp_rewind(s); IStream_Read(s, b, 4, &n);
        CHECK(!memcmp(b, "MEOW", 4));
        xp_rewind(s); U(CoReleaseMarshalData)(s);
        XEQ(o->ref, 1);
        IStream_Release(s);
        IMarshal_Release(fm);
    }
    if (ft) IUnknown_Release(ft);
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_ftm_cross_apartment_returns_raw_pointer) {
    static const DWORD hosts[] = { COINIT_APARTMENTTHREADED, COINIT_MULTITHREADED };
    XP_NEED_BASE(); NEED(CoCreateFreeThreadedMarshaler); INIT(COINIT_APARTMENTTHREADED);
    for (int i = 0; i < 2; i++) {
        XpObj *o = xp_obj_new(); XpHost h; IClassFactory *p = NULL; HRESULT hr;
        if (!xp_obj_enable_ftm(o)) { CHECK(0); IClassFactory_Release(XP_OBJ_CF(o)); continue; }
        if (xp_host_start(&h, hosts[i], XP_OBJ_UNK(o), &XP_IID_IClassFactory, MSHLFLAGS_NORMAL)) {
            hr = xp_host_unmarshal(&h, &XP_IID_IClassFactory, (void **)&p);
            XEQ(hr, S_OK);
            XEQ(p, XP_OBJ_CF(o));                                        /* no proxy: the FTM hands the real pointer to every apartment */
            if (p) {
                void *pv = NULL; DWORD me = GetCurrentThreadId();
                hr = IClassFactory_CreateInstance(p, NULL, &XP_IID_IUnknown, &pv);
                XEQ(hr, S_OK); XEQ(o->lastTid, me);                      /* the call ran on the caller's own thread */
                if (pv) IUnknown_Release((IUnknown *)pv);
                IClassFactory_Release(p);
            }
        } else CHECK(0);
        xp_host_stop(&h);
        IClassFactory_Release(XP_OBJ_CF(o));
    }
    UNINIT();
}
XT(mar_ftm_aggregation_args) {
    IUnknown *ft = (IUnknown *)1; XpObj *o; HRESULT hr; IUnknown *inner = NULL;
    XP_NEED_BASE(); NEED(CoCreateFreeThreadedMarshaler); INIT(COINIT_APARTMENTTHREADED);
    if (!t_is_wine()) { hr = U(CoCreateFreeThreadedMarshaler)(NULL, NULL); CHECKF(FAILED(hr), "NULL output pointer accepted: %08lx", hr); }
    o = xp_obj_new();
    hr = U(CoCreateFreeThreadedMarshaler)(XP_OBJ_UNK(o), &ft);
    XEQ(hr, S_OK);
    if (ft) {
        hr = IUnknown_QueryInterface(ft, &XP_IID_IMarshal, (void **)&inner);
        XEQ(hr, S_OK);
        CHECK(o->ref > 1);                                               /* the FTM's IMarshal delegates AddRef to the outer object */
        if (inner) IUnknown_Release(inner);
        XEQ(o->ref, 1);
        hr = IUnknown_QueryInterface(ft, &XP_IID_IUnknownBogus, (void **)&inner);
        XEQ(hr, E_NOINTERFACE);
        IUnknown_Release(ft);
    }
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}

/* ---- table marshaling across apartments ------------------------------------------------------------------------- */
struct RelW { IStream *s; HRESULT hr; };
static void relw_work(void *p) { struct RelW *a = p; xp_rewind(a->s); a->hr = U(CoReleaseMarshalData)(a->s); }
XT(mar_tablestrong_two_unmarshals_and_release_in_host) {
    XpFx f; IClassFactory *p2 = NULL; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_TABLESTRONG)) {
        hr = xp_host_unmarshal(&f.h, &XP_IID_IClassFactory, (void **)&p2);
        XEQ(hr, S_OK);                                                   /* table entries can be unmarshaled any number of times */
        CHECK(p2 != NULL);
        if (p2) IClassFactory_Release(p2);
        IClassFactory_Release(f.p); f.p = NULL;
        CHECK(f.o->ref > 1);                                             /* the table still holds the object */
        XEQ(f.gone, 0);
        {   /* release the table entry in the hosting apartment, where it was registered */
            struct RelW a; a.s = f.h.stream; a.hr = E_FAIL;
            CHECK(xp_host_run(&f.h, relw_work, &a));
            XEQ(a.hr, S_OK);
        }
        t_note("object references after the table entry is released: %ld", (long)f.o->ref);
        XEQ(f.o->ref, 1);
    } else CHECK(0);
    xp_fx_teardown(&f);
    UNINIT();
}
XT(mar_tableweak_does_not_keep_object_alive) {
    XpFx f; IClassFactory *p2 = NULL; HRESULT hr; void *pv = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_TABLEWEAK)) {
        hr = xp_host_unmarshal(&f.h, &XP_IID_IClassFactory, (void **)&p2);
        XEQ(hr, S_OK);
        if (p2) {
            hr = IClassFactory_CreateInstance(p2, NULL, &XP_IID_IUnknown, &pv);
            XEQ(hr, S_OK);
            if (pv) IUnknown_Release((IUnknown *)pv);
            IClassFactory_Release(p2);
        }
        IClassFactory_Release(f.p); f.p = NULL;
        XEQ(f.o->ref, 1);                                                /* all proxies gone: nothing keeps the object */
    } else CHECK(0);
    xp_fx_teardown(&f);
    UNINIT();
}
XT(mar_normal_marshal_unmarshal_twice_second_is_disconnected) {
    XpFx f; IClassFactory *p2 = NULL; HRESULT hr; void *pv = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (xp_fx_setup(&f, COINIT_APARTMENTTHREADED, MSHLFLAGS_NORMAL)) {
        hr = xp_host_unmarshal(&f.h, &XP_IID_IClassFactory, (void **)&p2);
        t_note("second unmarshal of NORMAL marshal data from another apartment: %08lx (%s)", hr, xp_hrname(hr));
        if (SUCCEEDED(hr) && p2) {
            hr = IClassFactory_CreateInstance(p2, NULL, &XP_IID_IUnknown, &pv);
            t_note("  call through that second proxy: %08lx (%s)", hr, xp_hrname(hr));
            if (pv) IUnknown_Release((IUnknown *)pv);
            IClassFactory_Release(p2);
        }
        /* the first proxy keeps working either way */
        hr = IClassFactory_CreateInstance(f.p, NULL, &XP_IID_IXpBogus2, &pv);
        XEQ(hr, S_OK);
    } else CHECK(0);
    xp_fx_teardown(&f);
    UNINIT();
}

/* ---- IExternalConnection and CoLockObjectExternal ------------------------------------------------------------------- */
XT(mar_external_connection_marshal_and_release) {
    IStream *s; XpObj *o; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new_extconn();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    XEQ(hr, S_OK);
    CHECK(o->extConnAdd >= 1);                                           /* the stub manager announces the external strong connection */
    XEQ(o->extConnAdd - o->extConnRel, 1);                               /* Win10: exactly one external strong connection outstanding (the stub manager adds one and returns one while setting up) */
    xp_rewind(s);
    hr = U(CoReleaseMarshalData)(s);
    XEQ(hr, S_OK);
    CHECK(o->extConnRel >= 1);
    XEQ(o->extConnAdd, o->extConnRel);                                   /* balanced */
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_external_connection_table_marshal) {
    IStream *s; XpObj *o; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    s = xp_new_stream(); o = xp_obj_new_extconn();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLEWEAK);
    XEQ(hr, S_OK);
    XEQ(o->extConnAdd, 0);                                               /* weak table entries are not external connections */
    xp_rewind(s); U(CoReleaseMarshalData)(s);
    xp_rewind(s);
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_INPROC, NULL, MSHLFLAGS_TABLESTRONG);
    XEQ(hr, S_OK);
    CHECK(o->extConnAdd >= 1);                                           /* strong table entries are */
    xp_rewind(s); U(CoReleaseMarshalData)(s);
    IStream_Release(s); IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_lock_object_external_counts) {
    XpObj *o; HRESULT hr;
    XP_NEED_BASE(); NEED(CoLockObjectExternal); INIT(COINIT_APARTMENTTHREADED);
    o = xp_obj_new_extconn();
    hr = U(CoLockObjectExternal)(XP_OBJ_UNK(o), TRUE, TRUE);
    XEQ(hr, S_OK);
    CHECK(o->ref > 1);                                                   /* a strong external lock holds a reference */
    CHECK(o->extConnAdd >= 1);
    hr = U(CoLockObjectExternal)(XP_OBJ_UNK(o), FALSE, TRUE);
    XEQ(hr, S_OK);
    XEQ(o->ref, 1);
    CHECK(o->extConnRel >= 1);
    /* a second lock/unlock pair, with disconnect instead of unlock */
    hr = U(CoLockObjectExternal)(XP_OBJ_UNK(o), TRUE, FALSE); XEQ(hr, S_OK);
    CHECK(o->ref > 1);
    hr = U(CoDisconnectObject)(XP_OBJ_UNK(o), 0); XEQ(hr, S_OK);
    XEQ(o->ref, 1);                                                      /* disconnecting drops every external lock */
    IClassFactory_Release(XP_OBJ_CF(o));
    UNINIT();
}
XT(mar_lock_object_external_invalid) {
    HRESULT hr;
    XP_NEED_BASE(); NEED(CoLockObjectExternal); INIT(COINIT_APARTMENTTHREADED);
    if (!t_is_wine()) {   /* the emulator dereferences NULL */
        hr = U(CoLockObjectExternal)(NULL, TRUE, TRUE);
        t_note("CoLockObjectExternal(NULL, TRUE): %08lx (%s)", hr, xp_hrname(hr));
        CHECK(FAILED(hr));
        hr = U(CoLockObjectExternal)(NULL, FALSE, TRUE);
        CHECK(FAILED(hr));
    }
    {
        XpObj *o = xp_obj_new();
        hr = U(CoLockObjectExternal)(XP_OBJ_UNK(o), FALSE, TRUE);        /* unlock without a lock */
        t_note("CoLockObjectExternal(FALSE) on an object that was never locked: %08lx (%s)", hr, xp_hrname(hr));
        XEQ(o->ref, 1);
        IClassFactory_Release(XP_OBJ_CF(o));
    }
    UNINIT();
}

/* ---- global interface table ------------------------------------------------------------------------------------------- */
typedef struct { IGlobalInterfaceTable *git; DWORD cookie; IUnknown *got; HRESULT hr, hrOther; IClassFactory *cf; HRESULT hrCall; DWORD tid; } GitCtx;
static DWORD git_get_proc(void *p)
{
    GitCtx *c = p; void *pv = NULL;
    c->tid = GetCurrentThreadId();
    c->hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(c->git, c->cookie, &XP_IID_IClassFactory, &pv);
    c->cf = pv;
    if (c->cf) {
        void *o = NULL;
        c->hrCall = IClassFactory_CreateInstance(c->cf, NULL, &XP_IID_IXpBogus2, &o);
        IClassFactory_Release(c->cf);
    }
    pv = NULL;
    c->hrOther = IGlobalInterfaceTable_GetInterfaceFromGlobal(c->git, c->cookie, &XP_IID_IUnknownBogus, &pv);
    if (pv) IUnknown_Release((IUnknown *)pv);
    return 0;
}
struct gitrun { int m; GitCtx *c; HANDLE done; };
static DWORD WINAPI xp_git_thread(void *p)
{
    struct gitrun *r = p; HRESULT hr = U(CoInitializeEx)(NULL, r->m);
    if (SUCCEEDED(hr)) { git_get_proc(r->c); U(CoUninitialize)(); }
    SetEvent(r->done);
    return 0;
}
static void git_case(int getterModel)
{
    IGlobalInterfaceTable *git = NULL; XpObj *o; HRESULT hr; DWORD cookie = 0; GitCtx c; void *pv;
    XP_NEED_BASE(); NEED(CoCreateInstance); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoCreateInstance)(&XP_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &XP_IID_IGlobalInterfaceTable, (void **)&git);
    XEQ(hr, S_OK);
    if (git) {
        o = xp_obj_new();
        hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, XP_OBJ_UNK(o), &XP_IID_IClassFactory, &cookie);
        XEQ(hr, S_OK); CHECK(cookie != 0);
        CHECK(o->ref > 1);                                               /* the table holds a strong marshal reference */
        memset(&c, 0, sizeof c); c.git = git; c.cookie = cookie; c.hrCall = (HRESULT)0xDEAD0001;
        {
            HANDLE th; DWORD tid; struct gitrun *r = HeapAlloc(GetProcessHeap(), 0, sizeof *r);
            r->m = getterModel; r->c = &c; r->done = CreateEventA(NULL, TRUE, FALSE, NULL);
            th = CreateThread(NULL, 0, xp_git_thread, r, 0, &tid);      /* getter runs in its own COM thread while this STA pumps */
            xp_wait_pump(r->done, 15000);
            WaitForSingleObject(th, 5000); CloseHandle(th); CloseHandle(r->done); HeapFree(GetProcessHeap(), 0, r);
        }
        XEQ(c.hr, S_OK);
        XEQ(c.hrCall, S_OK);
        XEQ(c.hrOther, E_NOINTERFACE);
        CHECK(c.tid != GetCurrentThreadId());
        hr = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie);
        XEQ(hr, S_OK);
        XEQ(o->ref, 1);
        pv = (void *)1;
        hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, cookie, &XP_IID_IClassFactory, &pv);
        CHECKF(FAILED(hr), "get after revoke returned %08lx", hr);
        t_note("GetInterfaceFromGlobal with a revoked cookie: %08lx (%s)", hr, xp_hrname(hr));
        hr = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie);
        CHECKF(FAILED(hr), "double revoke returned %08lx", hr);
        t_note("RevokeInterfaceFromGlobal twice: %08lx (%s)", hr, xp_hrname(hr));
        IClassFactory_Release(XP_OBJ_CF(o));
        IGlobalInterfaceTable_Release(git);
    }
    UNINIT();
}
XT(mar_git_get_from_other_sta) { git_case(COINIT_APARTMENTTHREADED); }
XT(mar_git_get_from_mta) { git_case(COINIT_MULTITHREADED); }
XT(mar_git_register_invalid_args) {
    IGlobalInterfaceTable *git = NULL; HRESULT hr; DWORD cookie = 0x1234; XpObj *o;
    XP_NEED_BASE(); NEED(CoCreateInstance); INIT(COINIT_APARTMENTTHREADED);
    hr = U(CoCreateInstance)(&XP_CLSID_StdGlobalInterfaceTable, NULL, CLSCTX_INPROC_SERVER, &XP_IID_IGlobalInterfaceTable, (void **)&git);
    XEQ(hr, S_OK);
    if (git) {
        void *pv = (void *)1;
        o = xp_obj_new();
        hr = IGlobalInterfaceTable_RegisterInterfaceInGlobal(git, XP_OBJ_UNK(o), &XP_IID_IUnknownBogus, &cookie);
        XEQ(hr, S_OK);                                                   /* Win10: no proxy/stub lookup happens when registering; the table holds the pointer */
        t_note("RegisterInterfaceInGlobal(unregistered IID): %08lx (%s) cookie=%lx", hr, xp_hrname(hr), cookie);
        XEQ(o->ref, 2);                                                  /* the table entry holds a reference ... */
        if (SUCCEEDED(hr)) { hr = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, cookie); XEQ(hr, S_OK); }
        XEQ(o->ref, 1);                                                  /* ... that revoking returns */
        hr = IGlobalInterfaceTable_GetInterfaceFromGlobal(git, 0xDEADBEEF, &XP_IID_IClassFactory, &pv);
        CHECKF(FAILED(hr), "get with a bogus cookie returned %08lx", hr);
        t_note("GetInterfaceFromGlobal(bogus cookie): %08lx (%s)", hr, xp_hrname(hr));
        hr = IGlobalInterfaceTable_RevokeInterfaceFromGlobal(git, 0xDEADBEEF);
        CHECKF(FAILED(hr), "revoke with a bogus cookie returned %08lx", hr);
        IClassFactory_Release(XP_OBJ_CF(o));
        IGlobalInterfaceTable_Release(git);
    }
    UNINIT();
}
