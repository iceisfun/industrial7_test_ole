/* gap_09: CStdAsyncStubBuffer2_Connect / CStdAsyncStubBuffer2_Disconnect (ole32.dll -> combase.dll) and
 * NdrCStdStubBuffer2_Release (rpcrt4.dll / combase.dll), driven with hand-built stub buffer objects.
 *
 * x64 layout the Win10 code relies on (offsets from the IRpcStubBuffer pointer that is passed in):
 *   -16  vtbl template pointer (NdrCStdStubBuffer2_Release only)      -8  base stub (IRpcStubBuffer *, may be NULL)
 *     0  lpVtbl     +8  LONG RefCount     +16  IUnknown *pvServerObject     +24  IPSFactoryBuffer *
 * Decompiled behaviour:
 *   CStdAsyncStubBuffer_Connect(p, punk):  punk->AddRef(); old = swap(p->pvServerObject, punk); if (old) old->Release(); return S_OK;
 *   CStdAsyncStubBuffer2_Connect(p, punk): hr = CStdAsyncStubBuffer_Connect(p, punk);
 *                                          if (hr >= 0 && base) hr = base->Connect(punk);  return hr;     (no rollback on failure)
 *   CStdAsyncStubBuffer2_Disconnect(p):    if (base) base->Disconnect(); old = swap(p->pvServerObject, NULL); if (old) old->Release();
 *   NdrCStdStubBuffer2_Release(p, psf):    n = --p->RefCount; if (n == 0) { base->Release() if base; free the vtbl template if it is
 *                                          heap managed; free(p - 16); psf->Release(); } return n;   (no Disconnect, server not released) */
#include "g9.h"

enum { EV_ADD = 1, EV_REL, EV_BCONN, EV_BDISC, EV_BREL, EV_PREL, EV_BADD, EV_PADD };
typedef struct { int ev, id; } evrec;
static evrec g_log[64];
static int g_nlog;
static void ev(int e, int id) { if (g_nlog < 64) { g_log[g_nlog].ev = e; g_log[g_nlog].id = id; g_nlog++; } }

/* fake server object */
typedef struct { const IUnknownVtbl *vtbl; LONG refs; int id; } Unk;
static HRESULT WINAPI u_qi(IUnknown *t, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI u_add(IUnknown *t) { Unk *u = (Unk *)t; ev(EV_ADD, u->id); return ++u->refs; }
static ULONG WINAPI u_rel(IUnknown *t) { Unk *u = (Unk *)t; ev(EV_REL, u->id); return --u->refs; }
static const IUnknownVtbl k_uvtbl = { u_qi, u_add, u_rel };
static void unk_init(Unk *u, int id) { u->vtbl = &k_uvtbl; u->refs = 1; u->id = id; }

/* fake base stub */
typedef struct { const IRpcStubBufferVtbl *vtbl; LONG refs; HRESULT conn_hr; IUnknown *conn_arg; int nconn, ndisc, ninvoke; } Base;
static HRESULT WINAPI b_qi(IRpcStubBuffer *t, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI b_add(IRpcStubBuffer *t) { ev(EV_BADD, 0); return ++((Base *)t)->refs; }
static ULONG WINAPI b_rel(IRpcStubBuffer *t) { ev(EV_BREL, 0); return --((Base *)t)->refs; }
static HRESULT WINAPI b_conn(IRpcStubBuffer *t, IUnknown *s) { Base *b = (Base *)t; b->nconn++; b->conn_arg = s; ev(EV_BCONN, s ? ((Unk *)s)->id : -1); return b->conn_hr; }
static void WINAPI b_disc(IRpcStubBuffer *t) { ((Base *)t)->ndisc++; ev(EV_BDISC, 0); }
static HRESULT WINAPI b_inv(IRpcStubBuffer *t, RPCOLEMESSAGE *m, IRpcChannelBuffer *c) { ((Base *)t)->ninvoke++; return S_OK; }
static IRpcStubBuffer *WINAPI b_iis(IRpcStubBuffer *t, REFIID r) { return NULL; }
static ULONG WINAPI b_cnt(IRpcStubBuffer *t) { return 0; }
static HRESULT WINAPI b_dsq(IRpcStubBuffer *t, void **p) { return E_NOTIMPL; }
static void WINAPI b_dsr(IRpcStubBuffer *t, void *p) { }
static const IRpcStubBufferVtbl k_bvtbl = { b_qi, b_add, b_rel, b_conn, b_disc, b_inv, b_iis, b_cnt, b_dsq, b_dsr };
static void base_init(Base *b, HRESULT hr) { memset(b, 0, sizeof *b); b->vtbl = &k_bvtbl; b->refs = 1; b->conn_hr = hr; }

/* fake IPSFactoryBuffer */
typedef struct { const IPSFactoryBufferVtbl *vtbl; LONG refs; } Psf;
static HRESULT WINAPI p_qi(IPSFactoryBuffer *t, REFIID r, void **p) { *p = NULL; return E_NOINTERFACE; }
static ULONG WINAPI p_add(IPSFactoryBuffer *t) { ev(EV_PADD, 0); return ++((Psf *)t)->refs; }
static ULONG WINAPI p_rel(IPSFactoryBuffer *t) { ev(EV_PREL, 0); return --((Psf *)t)->refs; }
static HRESULT WINAPI p_cp(IPSFactoryBuffer *t, IUnknown *o, REFIID r, IRpcProxyBuffer **pp, void **ppv) { return E_NOTIMPL; }
static HRESULT WINAPI p_cs(IPSFactoryBuffer *t, REFIID r, IUnknown *o, IRpcStubBuffer **pp) { return E_NOTIMPL; }
static const IPSFactoryBufferVtbl k_pvtbl = { p_qi, p_add, p_rel, p_cp, p_cs };

/* the stub object itself */
typedef struct {
    void *tmpl;                       /* -16 */
    IRpcStubBuffer *base;             /*  -8 */
    const IRpcStubBufferVtbl *vtbl;   /*   0 */
    LONG refs; LONG pad;              /*   8 */
    IUnknown *server;                 /*  16 */
    IPSFactoryBuffer *psf;            /*  24 */
    char tail[32];
} Stub;
static IRpcStubBuffer *stub_ptr(Stub *s) { return (IRpcStubBuffer *)&s->vtbl; }
static void stub_init(Stub *s, Base *b)
{
    memset(s, 0, sizeof *s);
    s->vtbl = &k_bvtbl;      /* any vtbl: the helpers under test never dispatch through it */
    s->refs = 1;
    s->base = b ? (IRpcStubBuffer *)b : NULL;
}

typedef HRESULT (WINAPI *conn_fn)(IRpcStubBuffer *, IUnknown *);
typedef void (WINAPI *disc_fn)(IRpcStubBuffer *);
typedef ULONG (WINAPI *rel_fn)(IRpcStubBuffer *, IPSFactoryBuffer *);

static const char *const k_mods[] = { "ole32.dll", "combase.dll" };

static void *fn_in(const char *mod, const char *name) { return t_proc_in(mod, name); }

static int ev_is(int i, int e, int id) { return i < g_nlog && g_log[i].ev == e && g_log[i].id == id; }

TEST(gap9_cstdasyncstub2_connect_with_base_stub)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        conn_fn f = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        Stub s; Base b; Unk u; HRESULT hr;
        if (!f) { t_note("%s: CStdAsyncStubBuffer2_Connect not exported", k_mods[m]); continue; }
        base_init(&b, S_OK); stub_init(&s, &b); unk_init(&u, 1);
        g_nlog = 0;
        hr = f(stub_ptr(&s), (IUnknown *)&u);
        CHECKF(hr == S_OK, "%s: hr %#lx", k_mods[m], hr);
        CHECK(s.server == (IUnknown *)&u);
        CHECK(u.refs == 2);
        CHECK(b.nconn == 1 && b.conn_arg == (IUnknown *)&u);
        CHECKF(g_nlog == 2 && ev_is(0, EV_ADD, 1) && ev_is(1, EV_BCONN, 1), "%s: AddRef of the server precedes the base Connect (nlog %d)", k_mods[m], g_nlog);
        CHECK(s.refs == 1 && s.base == (IRpcStubBuffer *)&b && s.vtbl == &k_bvtbl);
        CHECK(b.refs == 1);            /* the base stub is not AddRef'd */
    }
}

TEST(gap9_cstdasyncstub2_connect_without_base_stub)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        conn_fn f = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        Stub s; Unk u; HRESULT hr;
        if (!f) { t_note("%s: CStdAsyncStubBuffer2_Connect not exported", k_mods[m]); continue; }
        stub_init(&s, NULL); unk_init(&u, 1);
        g_nlog = 0;
        hr = f(stub_ptr(&s), (IUnknown *)&u);
        CHECKF(hr == S_OK, "%s: hr %#lx", k_mods[m], hr);
        CHECK(s.server == (IUnknown *)&u && u.refs == 2);
        CHECK(g_nlog == 1 && ev_is(0, EV_ADD, 1));
    }
}

TEST(gap9_cstdasyncstub2_connect_releases_previous_server_after_addref)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        conn_fn f = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        Stub s; Base b; Unk u1, u2;
        if (!f) continue;
        base_init(&b, S_OK); stub_init(&s, &b); unk_init(&u1, 1); unk_init(&u2, 2);
        CHECK(f(stub_ptr(&s), (IUnknown *)&u1) == S_OK);
        g_nlog = 0;
        CHECK(f(stub_ptr(&s), (IUnknown *)&u2) == S_OK);
        CHECK(s.server == (IUnknown *)&u2);
        CHECKF(g_nlog == 3 && ev_is(0, EV_ADD, 2) && ev_is(1, EV_REL, 1) && ev_is(2, EV_BCONN, 2),
               "%s: order AddRef(new), Release(old), base Connect (nlog %d)", k_mods[m], g_nlog);
        CHECK(u1.refs == 1 && u2.refs == 2);
        CHECK(b.nconn == 2);
    }
}

TEST(gap9_cstdasyncstub2_connect_same_server_twice_is_refcount_neutral)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        conn_fn f = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        Stub s; Unk u;
        if (!f) continue;
        stub_init(&s, NULL); unk_init(&u, 1);
        CHECK(f(stub_ptr(&s), (IUnknown *)&u) == S_OK);
        CHECK(u.refs == 2);
        g_nlog = 0;
        CHECK(f(stub_ptr(&s), (IUnknown *)&u) == S_OK);
        CHECKF(u.refs == 2, "%s: second Connect of the same object: AddRef then Release, net zero (refs %ld)", k_mods[m], u.refs);
        CHECK(g_nlog == 2 && ev_is(0, EV_ADD, 1) && ev_is(1, EV_REL, 1));
    }
}

TEST(gap9_cstdasyncstub2_connect_returns_base_connect_result)
{
    static const HRESULT hrs[] = { S_OK, S_FALSE, E_FAIL, E_NOINTERFACE, E_OUTOFMEMORY, E_INVALIDARG, 0x80040154, 2, 0x7fffffff, (HRESULT)0xffffffff };
    unsigned m, i;
    for (m = 0; m < 2; m++) {
        conn_fn f = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        if (!f) continue;
        for (i = 0; i < sizeof hrs / sizeof *hrs; i++) {
            Stub s; Base b; Unk u; HRESULT hr;
            base_init(&b, hrs[i]); stub_init(&s, &b); unk_init(&u, 1);
            hr = f(stub_ptr(&s), (IUnknown *)&u);
            CHECKF(hr == hrs[i], "%s: base returned %#lx, helper returned %#lx", k_mods[m], hrs[i], hr);
            /* no rollback: the server stays connected and referenced even when the base refuses */
            CHECKF(s.server == (IUnknown *)&u && u.refs == 2, "%s: server kept after base result %#lx", k_mods[m], hrs[i]);
        }
    }
}

TEST(gap9_cstdasyncstub2_disconnect_with_base_and_server)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        conn_fn fc = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        disc_fn fd = (disc_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Disconnect");
        Stub s; Base b; Unk u;
        if (!fc || !fd) { t_note("%s: async2 connect/disconnect missing", k_mods[m]); continue; }
        base_init(&b, S_OK); stub_init(&s, &b); unk_init(&u, 1);
        fc(stub_ptr(&s), (IUnknown *)&u);
        CHECK(u.refs == 2);
        g_nlog = 0;
        fd(stub_ptr(&s));
        CHECKF(g_nlog == 2 && ev_is(0, EV_BDISC, 0) && ev_is(1, EV_REL, 1), "%s: base Disconnect first, then Release of the server (nlog %d)", k_mods[m], g_nlog);
        CHECK(s.server == NULL && u.refs == 1);
        CHECK(b.ndisc == 1 && s.refs == 1 && s.base == (IRpcStubBuffer *)&b);
        /* a second Disconnect reaches the base again but has no server left to release */
        g_nlog = 0;
        fd(stub_ptr(&s));
        CHECKF(g_nlog == 1 && ev_is(0, EV_BDISC, 0), "%s: second Disconnect only notifies the base", k_mods[m]);
        CHECK(b.ndisc == 2 && u.refs == 1);
    }
}

TEST(gap9_cstdasyncstub2_disconnect_without_base_stub)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        disc_fn fd = (disc_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Disconnect");
        Stub s; Unk u;
        if (!fd) continue;
        stub_init(&s, NULL); unk_init(&u, 1);
        s.server = (IUnknown *)&u; u.refs = 2;
        g_nlog = 0;
        fd(stub_ptr(&s));
        CHECK(g_nlog == 1 && ev_is(0, EV_REL, 1));
        CHECK(s.server == NULL && u.refs == 1);
    }
}

TEST(gap9_cstdasyncstub2_disconnect_never_connected)
{
    unsigned m;
    for (m = 0; m < 2; m++) {
        disc_fn fd = (disc_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Disconnect");
        Stub s; Base b;
        if (!fd) continue;
        base_init(&b, S_OK); stub_init(&s, &b);
        g_nlog = 0;
        fd(stub_ptr(&s));
        CHECK(g_nlog == 1 && ev_is(0, EV_BDISC, 0));
        CHECK(s.server == NULL);
        stub_init(&s, NULL);
        g_nlog = 0;
        fd(stub_ptr(&s));
        CHECKF(g_nlog == 0, "%s: no base, no server: nothing is called", k_mods[m]);
    }
}

TEST(gap9_cstdasyncstub2_connect_disconnect_cycles_balance_refs)
{
    unsigned m; int i;
    for (m = 0; m < 2; m++) {
        conn_fn fc = (conn_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Connect");
        disc_fn fd = (disc_fn)fn_in(k_mods[m], "CStdAsyncStubBuffer2_Disconnect");
        Stub s; Base b; Unk u1, u2;
        if (!fc || !fd) continue;
        base_init(&b, S_OK); stub_init(&s, &b); unk_init(&u1, 1); unk_init(&u2, 2);
        for (i = 0; i < 50; i++) {
            fc(stub_ptr(&s), (IUnknown *)((i & 1) ? &u2 : &u1));
            if (i % 3 == 0) fd(stub_ptr(&s));
        }
        fd(stub_ptr(&s));
        CHECKF(u1.refs == 1 && u2.refs == 1, "%s: refs u1=%ld u2=%ld", k_mods[m], u1.refs, u2.refs);
        CHECK(s.server == NULL);
        CHECK(b.nconn == 50);
    }
}

TEST(gap9_cstdasyncstub2_connect_does_not_touch_other_stub_fields)
{
    conn_fn fc = (conn_fn)fn_in("combase.dll", "CStdAsyncStubBuffer2_Connect");
    disc_fn fd = (disc_fn)fn_in("combase.dll", "CStdAsyncStubBuffer2_Disconnect");
    Stub s, ref; Base b; Unk u; Psf p;
    if (!fc || !fd) { t_skip("combase async2 exports missing"); return; }
    p.vtbl = &k_pvtbl; p.refs = 1;
    base_init(&b, S_OK); stub_init(&s, &b); unk_init(&u, 1);
    s.psf = (IPSFactoryBuffer *)&p; s.refs = 7; memset(s.tail, 0x5A, sizeof s.tail);
    ref = s;
    fc(stub_ptr(&s), (IUnknown *)&u);
    CHECK(s.tmpl == ref.tmpl && s.base == ref.base && s.vtbl == ref.vtbl && s.refs == 7 && s.psf == ref.psf);
    CHECK(!memcmp(s.tail, ref.tail, sizeof s.tail));
    fd(stub_ptr(&s));
    CHECK(s.tmpl == ref.tmpl && s.base == ref.base && s.vtbl == ref.vtbl && s.refs == 7 && s.psf == ref.psf);
    CHECK(!memcmp(s.tail, ref.tail, sizeof s.tail));
    CHECK(p.refs == 1);
}

TEST(gap9_cstdasyncstub2_ole32_and_combase_exports_agree)
{
    /* ole32 forwards the async2 helpers to combase: same observable behaviour from either module */
    conn_fn fo = (conn_fn)fn_in("ole32.dll", "CStdAsyncStubBuffer2_Connect"), fc = (conn_fn)fn_in("combase.dll", "CStdAsyncStubBuffer2_Connect");
    int k;
    REQUIRE(fo); REQUIRE(fc);
    for (k = 0; k < 2; k++) {
        Stub s; Base b; Unk u;
        base_init(&b, E_FAIL); stub_init(&s, &b); unk_init(&u, 1);
        g_nlog = 0;
        CHECK((k ? fc : fo)(stub_ptr(&s), (IUnknown *)&u) == E_FAIL);
        CHECK(g_nlog == 2 && b.nconn == 1);
    }
    if (fo == fc) t_note("ole32 and combase resolve CStdAsyncStubBuffer2_Connect to the same address (forwarder)");
}

/* ---- NdrCStdStubBuffer2_Release ------------------------------------------------------------------------- */
TEST(gap9_ndrcstdstubbuffer2_release_decrements_and_returns_new_count)
{
    static const char *const mods[] = { "rpcrt4.dll", "combase.dll" };
    unsigned m;
    if (t_is_wine()) { t_skip("stub buffer layout differs under Wine"); return; }
    for (m = 0; m < 2; m++) {
        rel_fn f = (rel_fn)fn_in(mods[m], "NdrCStdStubBuffer2_Release");
        Stub s; Base b; Psf p; ULONG r; unsigned n;
        if (!f) { t_note("%s: NdrCStdStubBuffer2_Release missing", mods[m]); continue; }
        base_init(&b, S_OK); p.vtbl = &k_pvtbl; p.refs = 1;
        stub_init(&s, &b); s.refs = 5;
        g_nlog = 0;
        for (n = 4; n >= 1; n--) {
            r = f(stub_ptr(&s), (IPSFactoryBuffer *)&p);
            CHECKF(r == n && s.refs == (LONG)n, "%s: release -> %lu (field %ld) want %u", mods[m], r, s.refs, n);
        }
        CHECKF(g_nlog == 0, "%s: no callbacks while the count stays above zero (%d events)", mods[m], g_nlog);
        CHECK(b.refs == 1 && p.refs == 1 && b.ndisc == 0);
    }
}

TEST(gap9_ndrcstdstubbuffer2_release_does_not_disconnect_or_release_server)
{
    /* count 2 -> 1 -> (final release is exercised in a child process) */
    rel_fn f = (rel_fn)fn_in("combase.dll", "NdrCStdStubBuffer2_Release");
    Stub s; Base b; Psf p; Unk u;
    if (t_is_wine()) { t_skip("stub buffer layout differs under Wine"); return; }
    REQUIRE(f);
    base_init(&b, S_OK); p.vtbl = &k_pvtbl; p.refs = 1;
    stub_init(&s, &b); s.refs = 2; unk_init(&u, 1); s.server = (IUnknown *)&u; u.refs = 2;
    g_nlog = 0;
    CHECK(f(stub_ptr(&s), (IPSFactoryBuffer *)&p) == 1);
    CHECK(g_nlog == 0 && b.ndisc == 0 && u.refs == 2 && s.server == (IUnknown *)&u);
}

/* final release: frees the object block (allocated with CoTaskMemAlloc, which is what the Ndr allocator maps to) */
static Base cb_base; static Psf cb_psf; static Unk cb_unk;
static int final_release(const char *mod)
{
    rel_fn f = (rel_fn)fn_in(mod, "NdrCStdStubBuffer2_Release");
    unsigned char *blk; Stub *s; ULONG r;
    if (!f) return 90;
    blk = U(CoTaskMemAlloc)(sizeof(Stub));
    if (!blk) return 91;
    memset(blk, 0, sizeof(Stub));
    s = (Stub *)blk;
    base_init(&cb_base, S_OK); cb_psf.vtbl = &k_pvtbl; cb_psf.refs = 1; unk_init(&cb_unk, 1);
    s->base = (IRpcStubBuffer *)&cb_base; s->vtbl = &k_bvtbl; s->refs = 1; s->server = (IUnknown *)&cb_unk; cb_unk.refs = 2;
    g_nlog = 0;
    r = f(stub_ptr(s), (IPSFactoryBuffer *)&cb_psf);
    if (r != 0) return 1;
    if (cb_base.refs != 0) return 2;          /* base stub released exactly once */
    if (cb_psf.refs != 0) return 3;           /* factory released exactly once */
    if (cb_base.ndisc != 0) return 4;         /* Release does not call Disconnect */
    if (cb_unk.refs != 2) return 5;           /* nor does it release the server object */
    if (g_nlog != 2 || !ev_is(0, EV_BREL, 0) || !ev_is(1, EV_PREL, 0)) return 6;   /* base first, factory last */
    return 0;
}
CHILD(g9_ndr2_final_release_rpcrt4) { return final_release("rpcrt4.dll"); }
CHILD(g9_ndr2_final_release_combase) { return final_release("combase.dll"); }

TEST(gap9_ndrcstdstubbuffer2_final_release_frees_and_releases_owners)
{
    static const char *const kids[] = { "g9_ndr2_final_release_rpcrt4", "g9_ndr2_final_release_combase" };
    unsigned k;
    if (t_is_wine()) { t_skip("stub buffer layout differs under Wine"); return; }
    for (k = 0; k < 2; k++) {
        DWORD code = t_run_child(kids[k], "", 15000);
        if (code >= 0xC0000000u || code == 0xDEAD0000u || code == 0xDEAD0001u) {
            t_note("%s: child ended abnormally (%#lx): allocator or layout assumption for the free of the object block differs", kids[k], code);
            continue;
        }
        CHECKF(code == 0, "%s: step %lu failed (1 count, 2 base release, 3 factory release, 4 disconnect called, 5 server released, 6 order, 90 export missing)", kids[k], code);
    }
}
