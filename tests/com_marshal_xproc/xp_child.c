/* Cross-process standard marshaling. The test exe re-launches itself (CHILD xp_server) as a COM server:
 * the child creates an XpObj, marshals it into a stream, writes the bytes to a file in the parent's
 * scratch directory and signals "ready"; the parent unmarshals the file (a proxy to the child process)
 * and calls methods. The child appends facts it observed to a report file (key=hex) that the parent reads. */
#include "xp.h"

static DWORD g_ppid;

static void rep(const char *k, unsigned long long v) { xp_report(g_ppid, k, v); }

static void server_hook(XpObj *o, int what, BOOL arg)
{
    GUID *iid = &o->lastCreateIid;
    if (what != XP_HOOK_CREATE) return;
    if (IsEqualGUID(iid, &XP_IID_IXpBogus2)) {
        /* mode "sec": observe the call context from inside the stub call */
        DWORD authn = 0xAAAA5555, authz = 0xAAAA5555, level = 0xAAAA5555, imp = 0xAAAA5555, caps = 0xAAAA5555, tid = 0xAAAA5555;
        OLECHAR *princ = NULL; void *priv = NULL; IServerSecurity *ss = NULL; HRESULT hr; HANDLE tok = NULL;
        hr = U(CoQueryClientBlanket)(&authn, &authz, &princ, &level, &imp, &priv, &caps);
        rep("qcb_hr", (unsigned)hr); rep("qcb_authn", authn); rep("qcb_authz", authz); rep("qcb_level", level);
        rep("qcb_imp", imp); rep("qcb_caps", caps); rep("qcb_princ_nonnull", princ != NULL);
        if (princ) U(CoTaskMemFree)(princ);
        hr = U(CoGetCallerTID)(&tid); rep("tid_hr", (unsigned)hr); rep("tid", tid);
        hr = U(CoGetCallContext)(&XP_IID_IServerSecurity, (void **)&ss);
        rep("ctx_hr", (unsigned)hr);
        if (ss) {
            BOOL i0 = IServerSecurity_IsImpersonating(ss);
            HRESULT hi = IServerSecurity_ImpersonateClient(ss);
            BOOL i1 = IServerSecurity_IsImpersonating(ss);
            BOOL tk1 = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); if (tok) CloseHandle(tok);
            HRESULT hr2 = IServerSecurity_RevertToSelf(ss);
            BOOL i2 = IServerSecurity_IsImpersonating(ss);
            BOOL tk2 = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok); DWORD e2 = GetLastError(); if (tok) CloseHandle(tok);
            rep("imp0", i0); rep("imp_hr", (unsigned)hi); rep("imp1", i1); rep("tok1", tk1);
            rep("rev_hr", (unsigned)hr2); rep("imp2", i2); rep("tok2", tk2); rep("tok2_err", e2);
            /* the same through the free functions */
            hi = U(CoImpersonateClient)(); i1 = IServerSecurity_IsImpersonating(ss);
            hr2 = U(CoRevertToSelf)(); i2 = IServerSecurity_IsImpersonating(ss);
            rep("fimp_hr", (unsigned)hi); rep("fimp1", i1); rep("frev_hr", (unsigned)hr2); rep("fimp2", i2);
            authn = authz = level = imp = caps = 0xAAAA5555; princ = NULL;
            hr = IServerSecurity_QueryBlanket(ss, &authn, &authz, &princ, &level, &imp, &priv, &caps);
            rep("sqb_hr", (unsigned)hr); rep("sqb_level", level); rep("sqb_authn", authn);
            if (princ) U(CoTaskMemFree)(princ);
            IServerSecurity_Release(ss);
        }
    } else if (IsEqualGUID(iid, &XP_IID_IUnknownBogus)) {
        /* mode "crashcall": the server process dies in the middle of the call */
        char mode[32] = "";
        if (o->user) strncpy(mode, (const char *)o->user, sizeof mode - 1);
        if (!strcmp(mode, "crashcall")) TerminateProcess(GetCurrentProcess(), 0x77);
        if (!strcmp(mode, "exc")) RaiseException(0xE0001234, 0, 0, NULL);
    }
}

CHILD(xp_server) {
    int sta; const char *mode; XpObj *o; IStream *s; HRESULT hr; HANDLE ready, quit, parent, hs[2]; char path[MAX_PATH * 2];
    DWORD mshl = MSHLFLAGS_NORMAL;
    if (argc < 4) return 2;
    g_ppid = (DWORD)atoi(argv[1]); sta = !strcmp(argv[2], "sta"); mode = argv[3];
    rep("stage", 1);
    hr = U(CoInitializeEx)(NULL, sta ? COINIT_APARTMENTTHREADED : COINIT_MULTITHREADED);
    rep("stage", 2);
    if (FAILED(hr)) { rep("init_hr", (unsigned)hr); return 3; }
    o = xp_obj_new();
    o->hook = server_hook; o->user = (void *)mode;
    if (!strcmp(mode, "strong")) mshl = MSHLFLAGS_TABLESTRONG;
    if (!strcmp(mode, "weak")) mshl = MSHLFLAGS_TABLEWEAK;
    s = xp_new_stream();
    hr = U(CoMarshalInterface)(s, &XP_IID_IClassFactory, XP_OBJ_UNK(o), MSHCTX_LOCAL, NULL, mshl);
    rep("stage", 3);
    rep("marshal_hr", (unsigned)hr);
    rep("ref_after_marshal", o->ref);
    xp_parent_file(path, sizeof path, g_ppid, "xp_marshal.bin");
    xp_write_stream_to_file(s, path);
    ready = xp_event_create(g_ppid, "ready"); quit = xp_event_create(g_ppid, "quit");
    parent = OpenProcess(SYNCHRONIZE, FALSE, g_ppid);
    SetEvent(ready);
    hs[0] = quit; hs[1] = parent;
    {
        DWORD end = GetTickCount() + 90000;
        for (;;) {
            DWORD r; MSG m; LONG left = (LONG)(end - GetTickCount());
            if (left <= 0) break;
            r = sta ? MsgWaitForMultipleObjects(parent ? 2 : 1, hs, FALSE, left, QS_ALLINPUT) : WaitForMultipleObjects(parent ? 2 : 1, hs, FALSE, left);
            if (r == WAIT_OBJECT_0 || r == WAIT_OBJECT_0 + 1 || r == WAIT_TIMEOUT || r == WAIT_FAILED) break;
            while (PeekMessageA(&m, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
        }
    }
    rep("ref_at_quit", o->ref);
    rep("create_calls", o->createCalls);
    if (mshl != MSHLFLAGS_NORMAL) {
        xp_rewind(s);
        hr = U(CoReleaseMarshalData)(s);
        rep("release_hr", (unsigned)hr);
        rep("ref_after_release", o->ref);
    }
    U(CoUninitialize)();
    rep("done", 1);
    return 0;
}

/* ------------------------------ parent side ------------------------------------------------ */
BOOL xp_server_start(XpServer *s, const char *model, const char *mode, DWORD timeout)
{
    char args[128], tmp[MAX_PATH * 2]; HANDLE h; DWORD pid = GetCurrentProcessId();
    memset(s, 0, sizeof *s);
    s->pid = pid;
    t_tmpdir();
    xp_parent_file(tmp, sizeof tmp, pid, "xp_report.txt"); DeleteFileA(tmp);
    xp_parent_file(s->file, sizeof s->file, pid, "xp_marshal.bin"); DeleteFileA(s->file);
    s->hReady = xp_event_create(pid, "ready"); s->hQuit = xp_event_create(pid, "quit");
    snprintf(args, sizeof args, "%lu %s %s", pid, model, mode);
    h = t_spawn_child("xp_server", args, 0, NULL, NULL, NULL, FALSE, NULL, &s->pi);
    if (!h) { t_note("xp_server_start: cannot spawn child (%lu)", GetLastError()); return FALSE; }
    for (DWORD waited = 0; waited < timeout; waited += 50) {
        if (xp_wait_pump(s->hReady, 50)) {
            if (FAILED(xp_stream_from_file(s->file, &s->stream))) return FALSE;
            return TRUE;
        }
        if (WaitForSingleObject(s->pi.hProcess, 0) == WAIT_OBJECT_0) {
            DWORD code = 0; GetExitCodeProcess(s->pi.hProcess, &code);
            t_note("xp_server_start: child exited early with code 0x%lx", code); return FALSE;
        }
    }
    { unsigned long long st = 0, mh = 0; xp_report_get(pid, "stage", &st); xp_report_get(pid, "marshal_hr", &mh);
      t_note("xp_server_start: child not ready within %lu ms (stage %llu marshal_hr %llx)", timeout, st, mh); }
    return FALSE;
}
HRESULT xp_server_unmarshal(XpServer *s, const GUID *iid, void **out)
{
    xp_rewind(s->stream);
    return U(CoUnmarshalInterface)(s->stream, iid, out);
}
void xp_server_stop(XpServer *s)
{
    if (s->hQuit) SetEvent(s->hQuit);
    if (s->pi.hProcess) {
        if (WaitForSingleObject(s->pi.hProcess, 20000) != WAIT_OBJECT_0) { TerminateProcess(s->pi.hProcess, 1); WaitForSingleObject(s->pi.hProcess, 5000); }
        GetExitCodeProcess(s->pi.hProcess, &s->pid);   /* reuse: exit code available to caller via s->pid */
        CloseHandle(s->pi.hProcess); CloseHandle(s->pi.hThread); s->pi.hProcess = NULL;
    }
    if (s->hReady) CloseHandle(s->hReady);
    if (s->hQuit) CloseHandle(s->hQuit);
    s->hReady = s->hQuit = NULL;
    if (s->stream) { IStream_Release(s->stream); s->stream = NULL; }
}

#define INIT(m) do { HRESULT h_ = U(CoInitializeEx)(NULL, (m)); if (FAILED(h_)) { t_skip("CoInitializeEx failed %08lx", h_); return; } } while (0)
#define UNINIT() U(CoUninitialize)()
static HRESULT probe(IClassFactory *p) { void *pv = NULL; return IClassFactory_CreateInstance(p, NULL, &XP_IID_IXpBogus2, &pv); }
static HRESULT cf_hr(IClassFactory *p, HRESULT want) { GUID g = xp_hr_iid(want); void *pv = (void *)1; HRESULT hr = IClassFactory_CreateInstance(p, NULL, &g, &pv); return hr; }
#define REPQ(k, var) xp_report_get(GetCurrentProcessId(), k, &var)

/* rig: start a server, unmarshal a proxy */
typedef struct { XpServer s; IClassFactory *p; HRESULT hr; } Rig;
static BOOL rig_start(Rig *r, const char *model, const char *mode)
{
    memset(r, 0, sizeof *r);
    if (!xp_server_start(&r->s, model, mode, 30000)) return FALSE;
    r->hr = xp_server_unmarshal(&r->s, &XP_IID_IClassFactory, (void **)&r->p);
    return SUCCEEDED(r->hr) && r->p;
}
static void rig_stop(Rig *r) { if (r->p) { IClassFactory_Release(r->p); r->p = NULL; } xp_server_stop(&r->s); }

static void basic_xproc(const char *model)
{
    Rig r; unsigned long long v; HRESULT hr; void *pv = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, model, "normal")) {
        hr = probe(r.p);                   XEQ(hr, S_OK);
        hr = cf_hr(r.p, S_FALSE);          XEQ(hr, S_FALSE);
        hr = cf_hr(r.p, XP_CUSTOM_HR);     XEQ(hr, XP_CUSTOM_HR);
        hr = cf_hr(r.p, E_OUTOFMEMORY);    XEQ(hr, E_OUTOFMEMORY);
        hr = cf_hr(r.p, 0x00045678);       XEQ(hr, 0x00045678);
        hr = IClassFactory_CreateInstance(r.p, NULL, &XP_IID_IUnknownBogus, &pv);
        XEQ(hr, E_NOINTERFACE); CHECK(pv == NULL);
        IClassFactory_Release(r.p); r.p = NULL;
        xp_server_stop(&r.s);
        if (REPQ("ref_at_quit", v)) XEQ(v, 1); else CHECK(0);      /* the stub dropped its reference when the proxy went away */
        if (REPQ("create_calls", v)) XEQ(v, 6); else CHECK(0);
        if (REPQ("done", v)) XEQ(v, 1); else CHECK(0);
        XEQ(r.s.pid, 0);                                           /* child exit code */
    } else CHECKF(0, "server start failed (%08lx)", r.hr);
    rig_stop(&r);
    UNINIT();
}
XT(xproc_basic_calls_mta_server) { basic_xproc("mta"); }
XT(xproc_basic_calls_sta_server) { basic_xproc("sta"); }

XT(xproc_proxy_qi_and_identity) {
    Rig r; IUnknown *id1 = NULL, *out = NULL, *id2 = NULL; IMultiQI *mq = NULL; MULTI_QI q[2]; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        hr = IClassFactory_QueryInterface(r.p, &XP_IID_IUnknown, (void **)&id1); XEQ(hr, S_OK);
        hr = IClassFactory_CreateInstance(r.p, NULL, &XP_IID_IUnknown, (void **)&out); XEQ(hr, S_OK);
        if (out) { hr = IUnknown_QueryInterface(out, &XP_IID_IUnknown, (void **)&id2); XEQ(hr, S_OK); }
        XEQ(id1, id2);                                             /* one proxy manager per remote object identity */
        XEQ(out, id1);
        if (id2) IUnknown_Release(id2);
        if (out) IUnknown_Release(out);
        hr = IClassFactory_QueryInterface(r.p, &XP_IID_IMultiQI, (void **)&mq); XEQ(hr, S_OK);
        if (mq) {
            memset(q, 0, sizeof q); q[0].pIID = &XP_IID_IClassFactory; q[1].pIID = &XP_IID_IXpBogus2;
            hr = IMultiQI_QueryMultipleInterfaces(mq, 2, q);
            XEQ(hr, S_FALSE); XEQ(q[0].hr, S_OK); XEQ(q[1].hr, E_NOINTERFACE);
            if (q[0].pItf) IUnknown_Release(q[0].pItf);
            IMultiQI_Release(mq);
        }
        if (id1) IUnknown_Release(id1);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}

XT(xproc_server_killed_call_fails) {
    Rig r; HRESULT hr; ULONG ref;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        XEQ(probe(r.p), S_OK);
        TerminateProcess(r.s.pi.hProcess, 0x55);
        WaitForSingleObject(r.s.pi.hProcess, 10000);
        hr = probe(r.p);
        CHECKF(FAILED(hr), "call to a dead server returned %08lx", hr);
        t_note("call to a terminated server process: %08lx (%s)", hr, xp_hrname(hr));
        { void *pv = NULL; hr = IClassFactory_QueryInterface(r.p, &XP_IID_IUnknownBogus, &pv);
          t_note("QI on the dead server's proxy: %08lx (%s)", hr, xp_hrname(hr)); }
        ref = IClassFactory_Release(r.p); r.p = NULL;
        XEQ(ref, 0);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}

XT(xproc_server_killed_before_unmarshal) {
    Rig r; HRESULT hr; IClassFactory *p = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    memset(&r, 0, sizeof r);
    if (xp_server_start(&r.s, "mta", "normal", 30000)) {
        TerminateProcess(r.s.pi.hProcess, 0x55);
        WaitForSingleObject(r.s.pi.hProcess, 10000);
        hr = xp_server_unmarshal(&r.s, &XP_IID_IClassFactory, (void **)&p);
        t_note("unmarshal of data whose server is dead: %08lx (%s)", hr, xp_hrname(hr));
        if (SUCCEEDED(hr) && p) {
            hr = probe(p);
            CHECKF(FAILED(hr), "call returned %08lx", hr);
            t_note("call on such a proxy: %08lx (%s)", hr, xp_hrname(hr));
            IClassFactory_Release(p);
        } else CHECK(FAILED(hr));
    } else CHECK(0);
    xp_server_stop(&r.s); UNINIT();
}
XT(xproc_server_dies_during_call) {
    Rig r; HRESULT hr; void *pv = NULL; DWORD code = 0;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "crashcall")) {
        XEQ(probe(r.p), S_OK);
        hr = IClassFactory_CreateInstance(r.p, NULL, &XP_IID_IUnknownBogus, &pv);   /* server TerminateProcess()es itself here */
        CHECKF(FAILED(hr), "pending call of a dying server returned %08lx", hr);
        t_note("call interrupted by server death: %08lx (%s)", hr, xp_hrname(hr));
        CHECK(pv == NULL);
        WaitForSingleObject(r.s.pi.hProcess, 10000);
        GetExitCodeProcess(r.s.pi.hProcess, &code);
        XEQ(code, 0x77);
        hr = probe(r.p);
        CHECKF(FAILED(hr), "subsequent call returned %08lx", hr);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_server_exception_in_call_is_contained_or_fatal) {
    Rig r; HRESULT hr; void *pv = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "exc")) {
        hr = IClassFactory_CreateInstance(r.p, NULL, &XP_IID_IUnknownBogus, &pv);
        CHECKF(FAILED(hr), "call whose server method raised an exception returned %08lx", hr);
        t_note("server method raised SEH exception 0xE0001234: client saw %08lx (%s); server alive afterwards: %d",
               hr, xp_hrname(hr), WaitForSingleObject(r.s.pi.hProcess, 500) == WAIT_TIMEOUT);
        CHECK(pv == NULL);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_release_marshal_data_remote) {
    Rig r; unsigned long long v; HRESULT hr; IClassFactory *p = NULL;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    memset(&r, 0, sizeof r);
    if (xp_server_start(&r.s, "mta", "normal", 30000)) {
        xp_rewind(r.s.stream);
        hr = U(CoReleaseMarshalData)(r.s.stream);
        XEQ(hr, S_OK);
        hr = xp_server_unmarshal(&r.s, &XP_IID_IClassFactory, (void **)&p);
        t_note("unmarshal of released data (server alive): %08lx (%s)", hr, xp_hrname(hr));
        if (SUCCEEDED(hr) && p) {
            hr = probe(p);
            XEQ(hr, XP_RPC_E_DISCONNECTED);
            IClassFactory_Release(p);
        }
        xp_server_stop(&r.s);
        if (REPQ("ref_at_quit", v)) XEQ(v, 1); else CHECK(0);
        if (REPQ("create_calls", v)) XEQ(v, 0); else CHECK(0);
    } else CHECK(0);
    xp_server_stop(&r.s); UNINIT();
}
XT(xproc_tablestrong_keeps_object_after_proxies_gone) {
    Rig r; unsigned long long v; IClassFactory *p2 = NULL; IUnknown *u1 = NULL, *u2 = NULL; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "strong")) {
        hr = xp_server_unmarshal(&r.s, &XP_IID_IClassFactory, (void **)&p2);       /* table entries may be unmarshaled again */
        XEQ(hr, S_OK);
        IClassFactory_QueryInterface(r.p, &XP_IID_IUnknown, (void **)&u1);
        if (p2) IClassFactory_QueryInterface(p2, &XP_IID_IUnknown, (void **)&u2);
        XEQ(u1, u2);                               /* same remote identity -> same proxy manager */
        if (u1) IUnknown_Release(u1);
        if (u2) IUnknown_Release(u2);
        XEQ(probe(r.p), S_OK);
        if (p2) IClassFactory_Release(p2);
        IClassFactory_Release(r.p); r.p = NULL;
        xp_server_stop(&r.s);
        if (REPQ("ref_at_quit", v)) CHECKF(v > 1, "TABLESTRONG must keep the object alive (ref=%llu)", v); else CHECK(0);
        if (REPQ("release_hr", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("ref_after_release", v)) XEQ(v, 1); else CHECK(0);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_tableweak_does_not_keep_object) {
    Rig r; unsigned long long v; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "weak")) {
        if (REPQ("ref_after_marshal", v)) t_note("object refcount right after a TABLEWEAK marshal: %llu", v);
        hr = probe(r.p); XEQ(hr, S_OK);
        IClassFactory_Release(r.p); r.p = NULL;
        xp_server_stop(&r.s);
        if (REPQ("ref_at_quit", v)) XEQ(v, 1); else CHECK(0);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}

XT(xproc_marshal_data_layout) {
    Rig r; unsigned char buf[512]; ULONG got = 0; HRESULT hr; unsigned long long v;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        STATSTG st; xp_rewind(r.s.stream);
        hr = IStream_Read(r.s.stream, buf, sizeof buf, &got);
        XEQ(hr, S_OK);
        IStream_Stat(r.s.stream, &st, STATFLAG_NONAME);
        CHECK(got >= 24 + 40 + 4);
        if (got >= 68) {
            DWORD sig, flags, sflags, crefs; GUID iid; unsigned long long oxid, oid; GUID ipid; WORD n, secoff;
            memcpy(&sig, buf, 4); memcpy(&flags, buf + 4, 4); memcpy(&iid, buf + 8, 16);
            memcpy(&sflags, buf + 24, 4); memcpy(&crefs, buf + 28, 4); memcpy(&oxid, buf + 32, 8); memcpy(&oid, buf + 40, 8); memcpy(&ipid, buf + 48, 16);
            memcpy(&n, buf + 64, 2); memcpy(&secoff, buf + 66, 2);
            XEQ(sig, 0x574F454D);                       /* 'MEOW' */
            XEQ(flags, 1);                              /* OBJREF_STANDARD */
            CHECK(IsEqualGUID(&iid, &XP_IID_IClassFactory));
            XEQ(sflags, 0);
            CHECK(crefs >= 1);
            CHECK(oxid != 0); CHECK(oid != 0);
            { GUID z_; memset(&z_, 0, sizeof z_); CHECK(!IsEqualGUID(&ipid, &z_)); }
            t_note("string bindings present: %d", n > 0);
            CHECK(secoff <= n);
            t_note("OBJREF: cPublicRefs=%lu wNumEntries=%u wSecurityOffset=%u total=%llu bytes", crefs, n, secoff, st.cbSize.QuadPart);
            if (n >= 1 && got >= 70) { WORD tower; memcpy(&tower, buf + 68, 2); t_note("first string binding tower id 0x%x", tower); }
        }
        if (REPQ("ref_after_marshal", v)) CHECK(v > 1); else CHECK(0);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}

static void sec_case(const char *model)
{
    Rig r; unsigned long long v, a, b; HRESULT hr;
    XP_NEED_BASE(); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, model, "sec")) {
        hr = probe(r.p);
        XEQ(hr, S_OK);
        if (REPQ("qcb_hr", v)) XEQ(v, 0); else CHECK(0);            /* a blanket is available inside a call */
        if (REPQ("ctx_hr", v)) XEQ(v, 0); else CHECK(0);            /* CoGetCallContext(IID_IServerSecurity) works inside a call */
        if (REPQ("qcb_level", v)) CHECKF(v >= 1 && v <= 6, "authn level %llu", v);
        if (REPQ("qcb_imp", v)) CHECKF(v >= 1 && v <= 5, "impersonation level %llu", v);
        if (REPQ("qcb_authn", a) && REPQ("qcb_authz", b)) t_note("server-side blanket: authn=%llu authz=%llu", a, b);
        if (REPQ("qcb_level", a) && REPQ("qcb_imp", b) && REPQ("qcb_caps", v)) t_note("server-side blanket: level=%llu imp=%llu caps=0x%llx", a, b, v);
        if (REPQ("sqb_hr", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("sqb_level", a) && REPQ("qcb_level", b)) XEQ(a, b);   /* IServerSecurity::QueryBlanket == CoQueryClientBlanket */
        if (REPQ("imp0", v)) XEQ(v, 0); else CHECK(0);              /* not impersonating on entry */
        if (REPQ("imp_hr", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("imp1", v)) XEQ(v, 1); else CHECK(0);
        if (REPQ("tok1", v)) XEQ(v, 1); else CHECK(0);              /* the thread really carries the client token */
        if (REPQ("rev_hr", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("imp2", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("tok2", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("tok2_err", v)) XEQ(v, ERROR_NO_TOKEN);
        if (REPQ("fimp_hr", v)) XEQ(v, 0); else CHECK(0);           /* CoImpersonateClient / CoRevertToSelf free functions */
        if (REPQ("fimp1", v)) XEQ(v, 1); else CHECK(0);
        if (REPQ("frev_hr", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("fimp2", v)) XEQ(v, 0); else CHECK(0);
        if (REPQ("tid_hr", v) && REPQ("tid", a)) t_note("CoGetCallerTID in a cross-process call: hr=%08llx tid=%llu", v, a);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_server_security_context_mta) { sec_case("mta"); }
XT(xproc_server_security_context_sta) { sec_case("sta"); }

/* ---- client side blanket APIs on a cross-process proxy ------------------------------------------ */
typedef struct { DWORD authn, authz, level, imp, caps; OLECHAR *princ; void *auth; HRESULT hr; } Blk;
static void blk_init(Blk *b) { b->authn = b->authz = b->level = b->imp = b->caps = 0xAAAA5555; b->princ = NULL; b->auth = NULL; }
static HRESULT blk_query(IUnknown *p, Blk *b) { blk_init(b); return b->hr = U(CoQueryProxyBlanket)(p, &b->authn, &b->authz, &b->princ, &b->level, &b->imp, &b->auth, &b->caps); }

XT(xproc_query_proxy_blanket) {
    Rig r; Blk b, b2; HRESULT hr; IClientSecurity *cs = NULL;
    XP_NEED_BASE(); NEED(CoQueryProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        hr = U(CoQueryProxyBlanket)((IUnknown *)r.p, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        XEQ(hr, S_OK);                                              /* all outputs are optional */
        hr = blk_query((IUnknown *)r.p, &b);
        XEQ(hr, S_OK);
        CHECK(b.authn != 0xAAAA5555); CHECK(b.authz != 0xAAAA5555); CHECK(b.level != 0xAAAA5555); CHECK(b.imp != 0xAAAA5555); CHECK(b.caps != 0xAAAA5555);
        CHECKF(b.level <= 6, "authn level %lu", b.level);
        CHECKF(b.imp >= 1 && b.imp <= 5, "impersonation level %lu", b.imp);
        t_note("default proxy blanket: authn=%lu authz=%lu level=%lu imp=%lu caps=0x%lx principal=%ls", b.authn, b.authz, b.level, b.imp, b.caps, b.princ ? b.princ : L"(null)");
        hr = IClassFactory_QueryInterface(r.p, &XP_IID_IClientSecurity, (void **)&cs);
        XEQ(hr, S_OK);
        if (cs) {
            IMarshal *m = NULL;
            blk_init(&b2);
            hr = IClientSecurity_QueryBlanket(cs, (IUnknown *)r.p, &b2.authn, &b2.authz, &b2.princ, &b2.level, &b2.imp, &b2.auth, &b2.caps);
            XEQ(hr, S_OK);
            XEQ(b2.authn, b.authn); XEQ(b2.authz, b.authz); XEQ(b2.level, b.level); XEQ(b2.imp, b.imp);
            if (b2.princ) U(CoTaskMemFree)(b2.princ);
            /* a pointer that is not a proxied interface of this proxy manager is refused */
            IClassFactory_QueryInterface(r.p, &XP_IID_IMarshal, (void **)&m);
            if (m) { hr = IClientSecurity_QueryBlanket(cs, (IUnknown *)m, NULL, NULL, NULL, NULL, NULL, NULL, NULL); XEQ(hr, E_NOINTERFACE); IMarshal_Release(m); }
            IClientSecurity_Release(cs);
        }
        if (b.princ) U(CoTaskMemFree)(b.princ);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_set_proxy_blanket_roundtrip) {
    Rig r; Blk b; HRESULT hr;
    XP_NEED_BASE(); NEED(CoSetProxyBlanket); NEED(CoQueryProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        XEQ(hr, S_OK);
        hr = blk_query((IUnknown *)r.p, &b);
        XEQ(hr, S_OK);
        XEQ(b.authn, RPC_C_AUTHN_WINNT); XEQ(b.authz, RPC_C_AUTHZ_NONE);
        XEQ(b.level, RPC_C_AUTHN_LEVEL_CALL); XEQ(b.imp, RPC_C_IMP_LEVEL_IMPERSONATE);
        t_note("blanket after set: caps=0x%lx principal=%ls", b.caps, b.princ ? b.princ : L"(null)");
        if (b.princ) U(CoTaskMemFree)(b.princ);
        XEQ(probe(r.p), S_OK);                                      /* the proxy keeps working with the new blanket */
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        XEQ(hr, S_OK);
        blk_query((IUnknown *)r.p, &b);
        XEQ(b.level, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
        if (b.princ) U(CoTaskMemFree)(b.princ);
        XEQ(probe(r.p), S_OK);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_set_proxy_blanket_invalid) {
    Rig r; HRESULT hr; Blk before, after;
    XP_NEED_BASE(); NEED(CoSetProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        blk_query((IUnknown *)r.p, &before);
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, 0xDEADBEEF, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        CHECKF(FAILED(hr), "unknown authentication service accepted: %08lx", hr);
        t_note("CoSetProxyBlanket with unknown authn service: %08lx (%s)", hr, xp_hrname(hr));
        blk_query((IUnknown *)r.p, &after);
        XEQ(after.authn, before.authn); XEQ(after.level, before.level);     /* a failed set changes nothing */
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 99, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        t_note("CoSetProxyBlanket with authn level 99: %08lx (%s)", hr, xp_hrname(hr));
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, 99, NULL, EOAC_NONE);
        t_note("CoSetProxyBlanket with impersonation level 99: %08lx (%s)", hr, xp_hrname(hr));
        if (before.princ) U(CoTaskMemFree)(before.princ);
        if (after.princ) U(CoTaskMemFree)(after.princ);
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_copy_proxy_is_independent) {
    Rig r; IUnknown *copy = NULL; HRESULT hr; Blk b1, bc; unsigned long long v;
    XP_NEED_BASE(); NEED(CoCopyProxy); NEED(CoSetProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
        XEQ(hr, S_OK);
        hr = U(CoCopyProxy)((IUnknown *)r.p, &copy);
        XEQ(hr, S_OK);
        CHECK(copy != NULL);
        if (copy) {
            XEQ(probe((IClassFactory *)copy), S_OK);
            blk_query(copy, &bc);
            XEQ(bc.level, RPC_C_AUTHN_LEVEL_CALL);                  /* the copy starts with the original's blanket */
            XEQ(bc.imp, RPC_C_IMP_LEVEL_IMPERSONATE);
            if (bc.princ) U(CoTaskMemFree)(bc.princ);
            hr = U(CoSetProxyBlanket)(copy, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
            XEQ(hr, S_OK);
            blk_query(copy, &bc); blk_query((IUnknown *)r.p, &b1);
            XEQ(bc.level, RPC_C_AUTHN_LEVEL_PKT_PRIVACY);
            XEQ(b1.level, RPC_C_AUTHN_LEVEL_CALL);                  /* ... and changing it does not touch the original */
            if (bc.princ) U(CoTaskMemFree)(bc.princ);
            if (b1.princ) U(CoTaskMemFree)(b1.princ);
            XEQ(probe((IClassFactory *)copy), S_OK);
            XEQ(probe(r.p), S_OK);
            IUnknown_Release(copy);
        }
        IClassFactory_Release(r.p); r.p = NULL;
        xp_server_stop(&r.s);
        if (REPQ("ref_at_quit", v)) XEQ(v, 1); else CHECK(0);       /* all proxies (original + copy) are gone */
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_cloaking_flags_accepted_or_refused) {
    Rig r; HRESULT hr; Blk b; static const DWORD caps[] = { EOAC_STATIC_CLOAKING, EOAC_DYNAMIC_CLOAKING };
    XP_NEED_BASE(); NEED(CoSetProxyBlanket); INIT(COINIT_APARTMENTTHREADED);
    if (rig_start(&r, "mta", "normal")) {
        for (int i = 0; i < 2; i++) {
            hr = U(CoSetProxyBlanket)((IUnknown *)r.p, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, caps[i]);
            t_note("CoSetProxyBlanket caps=0x%lx: %08lx (%s)", caps[i], hr, xp_hrname(hr));
            if (SUCCEEDED(hr)) {
                blk_query((IUnknown *)r.p, &b);
                t_note("  read back caps=0x%lx", b.caps);
                if (b.princ) U(CoTaskMemFree)(b.princ);
                XEQ(probe(r.p), S_OK);
            }
        }
    } else CHECK(0);
    rig_stop(&r); UNINIT();
}
XT(xproc_blanket_functions_on_non_proxy_objects) {
    XpObj *o; IUnknown *cp = (IUnknown *)0x1; HRESULT hr;
    XP_NEED_BASE(); NEED(CoQueryProxyBlanket); NEED(CoSetProxyBlanket); NEED(CoCopyProxy);
    o = xp_obj_new();
    hr = U(CoQueryProxyBlanket)(XP_OBJ_UNK(o), NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    XEQ(hr, E_NOINTERFACE);                                          /* the call is answered by QueryInterface of the object */
    XEQ(o->qiCalls, 1);
    hr = U(CoSetProxyBlanket)(XP_OBJ_UNK(o), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    XEQ(hr, E_NOINTERFACE);
    hr = U(CoCopyProxy)(XP_OBJ_UNK(o), &cp);
    XEQ(hr, E_NOINTERFACE);
    XEQ(o->qiCalls, 3);
    t_note("IID asked of a plain object by the blanket functions: {%08lx-%04x-%04x-...}", o->lastQi.Data1, o->lastQi.Data2, o->lastQi.Data3);
    XEQ(o->ref, 1);
    IClassFactory_Release(XP_OBJ_CF(o));
}
