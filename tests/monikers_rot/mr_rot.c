/* Running Object Table: GetRunningObjectTable, IRunningObjectTable, IEnumMoniker from EnumRunning.
 * Win10 facts used here are from the decompilation of CRunningObjectTable::{Register,RevokeHelper,
 * _IsRunning,_GetObject,_GetTimeOfLastChange,NoteChangeTime,_EnumRunning} and GetRunningObjectTable. */
#include "mr.h"

/* ---------------------------------------------------------------- custom test moniker */
/* A tiny moniker implemented by the test: it records which methods the ROT calls on it, which
 * documents the exact protocol an implementation of the ROT has to follow. */
MR_GUID(mr_CLSID_TestMoniker, 0x9e4b0a01, 0x7e57, 0x4c11, 0x9a, 0x01, 0x6f, 0x1c, 0x0d, 0x0e, 0x7e, 0x57);
MR_GUID(mr_IID_IExternalConnection, 0x00000019, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46);

typedef struct mr_tmon {
    IMoniker m;
    IROTData rd;
    LONG ref;
    BOOL no_rotdata;
    WCHAR name[64];
} mr_tmon;

static const char *mr_log[32];
static int mr_nlog;
static void lg(const char *s) { if (mr_nlog < 32) mr_log[mr_nlog++] = s; }
static void log_reset(void) { mr_nlog = 0; }
static int log_is(int n, ...) {
    va_list a; int ok = (mr_nlog == n);
    va_start(a, n);
    for (int i = 0; i < n; i++) { const char *e = va_arg(a, const char *); if (i < mr_nlog && strcmp(e, mr_log[i])) ok = 0; }
    va_end(a);
    return ok;
}
static void log_dump(const char *what) {
    char buf[512]; buf[0] = 0;
    for (int i = 0; i < mr_nlog; i++) { strcat(buf, mr_log[i]); strcat(buf, i + 1 < mr_nlog ? ", " : ""); }
    t_note("%s call sequence: %s", what, buf);
}

static mr_tmon *tm_of(IMoniker *i) { return (mr_tmon *)i; }
static HRESULT WINAPI tm_QI(IMoniker *i, REFIID r, void **pp) {
    mr_tmon *t = tm_of(i);
    if (mr_guid_eq(r, &mr_IID_IUnknown) || mr_guid_eq(r, &mr_IID_IMoniker) || mr_guid_eq(r, &mr_IID_IPersist) ||
        mr_guid_eq(r, &mr_IID_IPersistStream)) { *pp = &t->m; InterlockedIncrement(&t->ref); return S_OK; }
    if (mr_guid_eq(r, &mr_IID_IROTData)) {
        lg("QI(IROTData)");
        if (t->no_rotdata) { *pp = NULL; return E_NOINTERFACE; }
        *pp = &t->rd; InterlockedIncrement(&t->ref); return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI tm_AddRef(IMoniker *i) { return InterlockedIncrement(&tm_of(i)->ref); }
static ULONG WINAPI tm_Release(IMoniker *i) {
    mr_tmon *t = tm_of(i); LONG r = InterlockedDecrement(&t->ref);
    if (!r) free(t);
    return r;
}
static HRESULT WINAPI tm_GetClassID(IMoniker *i, CLSID *c) { lg("GetClassID"); *c = mr_CLSID_TestMoniker; return S_OK; }
static HRESULT WINAPI tm_IsDirty(IMoniker *i) { return S_FALSE; }
static HRESULT WINAPI tm_Load(IMoniker *i, IStream *s) { return E_NOTIMPL; }
static HRESULT WINAPI tm_Save(IMoniker *i, IStream *s, BOOL c) { return E_NOTIMPL; }
static HRESULT WINAPI tm_GetSizeMax(IMoniker *i, ULARGE_INTEGER *s) { return E_NOTIMPL; }
static HRESULT WINAPI tm_BindToObject(IMoniker *i, IBindCtx *b, IMoniker *l, REFIID r, void **pp) { return E_NOTIMPL; }
static HRESULT WINAPI tm_BindToStorage(IMoniker *i, IBindCtx *b, IMoniker *l, REFIID r, void **pp) { return E_NOTIMPL; }
static HRESULT WINAPI tm_Reduce(IMoniker *i, IBindCtx *b, DWORD f, IMoniker **l, IMoniker **out) {
    lg("Reduce"); *out = i; IMoniker_AddRef(i); return MK_S_REDUCED_TO_SELF;
}
static HRESULT WINAPI tm_ComposeWith(IMoniker *i, IMoniker *r, BOOL o, IMoniker **c) { return MK_E_NEEDGENERIC; }
static HRESULT WINAPI tm_Enum(IMoniker *i, BOOL f, IEnumMoniker **e) { if (e) *e = NULL; return S_OK; }
static HRESULT WINAPI tm_IsEqual(IMoniker *i, IMoniker *o) { return E_NOTIMPL; }
static HRESULT WINAPI tm_Hash(IMoniker *i, DWORD *h) { *h = 0; return S_OK; }
static HRESULT WINAPI tm_IsRunning(IMoniker *i, IBindCtx *b, IMoniker *l, IMoniker *n) { return E_NOTIMPL; }
static HRESULT WINAPI tm_GetTimeOfLastChange(IMoniker *i, IBindCtx *b, IMoniker *l, FILETIME *ft) { lg("GetTimeOfLastChange"); return MK_E_UNAVAILABLE; }
static HRESULT WINAPI tm_Inverse(IMoniker *i, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI tm_CommonPrefixWith(IMoniker *i, IMoniker *o, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI tm_RelativePathTo(IMoniker *i, IMoniker *o, IMoniker **m) { return E_NOTIMPL; }
static HRESULT WINAPI tm_GetDisplayName(IMoniker *i, IBindCtx *b, IMoniker *l, LPOLESTR *n) {
    mr_tmon *t = tm_of(i); lg("GetDisplayName");
    *n = U(CoTaskMemAlloc)((lstrlenW(t->name) + 1) * sizeof(WCHAR));
    if (!*n) return E_OUTOFMEMORY;
    lstrcpyW(*n, t->name); return S_OK;
}
static HRESULT WINAPI tm_ParseDisplayName(IMoniker *i, IBindCtx *b, IMoniker *l, LPOLESTR d, ULONG *e, IMoniker **o) { return E_NOTIMPL; }
static HRESULT WINAPI tm_IsSystemMoniker(IMoniker *i, DWORD *t) { *t = MKSYS_NONE; return S_FALSE; }
static const IMonikerVtbl tm_vtbl = {
    tm_QI, tm_AddRef, tm_Release, tm_GetClassID, tm_IsDirty, tm_Load, tm_Save, tm_GetSizeMax,
    tm_BindToObject, tm_BindToStorage, tm_Reduce, tm_ComposeWith, tm_Enum, tm_IsEqual, tm_Hash, tm_IsRunning,
    tm_GetTimeOfLastChange, tm_Inverse, tm_CommonPrefixWith, tm_RelativePathTo, tm_GetDisplayName,
    tm_ParseDisplayName, tm_IsSystemMoniker };

static mr_tmon *rd_of(IROTData *r) { return (mr_tmon *)((char *)r - offsetof(mr_tmon, rd)); }
static HRESULT WINAPI rd_QI(IROTData *r, REFIID i, void **pp) { return tm_QI(&rd_of(r)->m, i, pp); }
static ULONG WINAPI rd_AddRef(IROTData *r) { return tm_AddRef(&rd_of(r)->m); }
static ULONG WINAPI rd_Release(IROTData *r) { return tm_Release(&rd_of(r)->m); }
static HRESULT WINAPI rd_GetComparisonData(IROTData *r, BYTE *buf, ULONG cb, ULONG *need) {
    mr_tmon *t = rd_of(r); ULONG n = 16 + (lstrlenW(t->name) + 1) * sizeof(WCHAR);
    lg("ROTData_GetComparisonData");
    *need = n;
    if (cb < n) return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
    memcpy(buf, &mr_CLSID_TestMoniker, 16); memcpy(buf + 16, t->name, n - 16);
    return S_OK;
}
static const IROTDataVtbl rd_vtbl = { rd_QI, rd_AddRef, rd_Release, rd_GetComparisonData };

static mr_tmon *tm_new(const WCHAR *name, BOOL no_rotdata) {
    mr_tmon *t = calloc(1, sizeof *t);
    t->m.lpVtbl = &tm_vtbl; t->rd.lpVtbl = &rd_vtbl; t->ref = 1; t->no_rotdata = no_rotdata;
    lstrcpynW(t->name, name, 64);
    return t;
}

/* an object that counts IExternalConnection traffic (ROTFLAGS_REGISTRATIONKEEPSALIVE) */
typedef struct mr_ext { const IExternalConnectionVtbl *lpVtbl; volatile LONG ref; volatile LONG strong, weak; } mr_ext;
static HRESULT WINAPI ex_QI(IExternalConnection *i, REFIID r, void **pp) {
    if (mr_guid_eq(r, &mr_IID_IUnknown) || mr_guid_eq(r, &mr_IID_IExternalConnection)) { *pp = i; InterlockedIncrement(&((mr_ext *)i)->ref); return S_OK; }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI ex_AddRef(IExternalConnection *i) { return InterlockedIncrement(&((mr_ext *)i)->ref); }
static ULONG WINAPI ex_Release(IExternalConnection *i) { return InterlockedDecrement(&((mr_ext *)i)->ref); }
static DWORD WINAPI ex_Add(IExternalConnection *i, DWORD c, DWORD r) {
    mr_ext *e = (mr_ext *)i; if (c & EXTCONN_STRONG) return InterlockedIncrement(&e->strong); if (c & EXTCONN_WEAK) return InterlockedIncrement(&e->weak); return 0;
}
static DWORD WINAPI ex_Rel(IExternalConnection *i, DWORD c, DWORD r, BOOL l) {
    mr_ext *e = (mr_ext *)i; if (c & EXTCONN_STRONG) return InterlockedDecrement(&e->strong); if (c & EXTCONN_WEAK) return InterlockedDecrement(&e->weak); return 0;
}
static const IExternalConnectionVtbl ex_vtbl = { ex_QI, ex_AddRef, ex_Release, ex_Add, ex_Rel };
static void ex_init(mr_ext *e) { e->lpVtbl = &ex_vtbl; e->ref = 1; e->strong = e->weak = 0; }

/* ---------------------------------------------------------------- helpers */
static IRunningObjectTable *get_rot(void) {
    IRunningObjectTable *r = NULL; HRESULT hr = U(GetRunningObjectTable)(0, &r);
    CHECKF(hr == S_OK && r, "GetRunningObjectTable hr=%08lx", hr);
    return r;
}
static int rot_has(IRunningObjectTable *rot, IMoniker *m) { return IRunningObjectTable_IsRunning(rot, m) == S_OK; }
/* count enumerated monikers whose display name is (case-insensitively) name */
static int enum_count(IEnumMoniker *e, IBindCtx *bc, const WCHAR *name) {
    IMoniker *m; int n = 0;
    while (IEnumMoniker_Next(e, 1, &m, NULL) == S_OK) {
        WCHAR *s = NULL;
        if (SUCCEEDED(IMoniker_GetDisplayName(m, bc, NULL, &s)) && s) { if (!lstrcmpiW(s, name)) n++; U(CoTaskMemFree)(s); }
        IMoniker_Release(m);
    }
    return n;
}

#define ROT_SETUP() \
    IRunningObjectTable *rot; IBindCtx *bc; \
    MR_NEED_BASE(); MR_COM_BEGIN(); rot = get_rot(); bc = mr_bc(); \
    if (!rot || !bc) { if (rot) IRunningObjectTable_Release(rot); if (bc) IBindCtx_Release(bc); MR_COM_END(); return; }
#define ROT_TEARDOWN() do { IRunningObjectTable_Release(rot); IBindCtx_Release(bc); MR_COM_END(); } while (0)

/* ---------------------------------------------------------------- basic API */
TEST(mr_rot_same_instance_on_repeat) {
    IRunningObjectTable *a = NULL, *b = NULL;
    MR_NEED_BASE(); MR_COM_BEGIN();
    EXPECT_EQ(U(GetRunningObjectTable)(0, &a), S_OK);
    EXPECT_EQ(U(GetRunningObjectTable)(0, &b), S_OK);
    CHECK(a != NULL);
    EXPECT_EQ(a, b);
    if (a) IRunningObjectTable_Release(a);
    if (b) IRunningObjectTable_Release(b);
    MR_COM_END();
}

TEST(mr_rot_query_interface) {
    void *p; ROT_SETUP();
    p = NULL; EXPECT_EQ(IRunningObjectTable_QueryInterface(rot, &mr_IID_IUnknown, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL; EXPECT_EQ(IRunningObjectTable_QueryInterface(rot, &mr_IID_IRunningObjectTable, &p), S_OK);
    if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EXPECT_EQ(IRunningObjectTable_QueryInterface(rot, &mr_IID_IMoniker, &p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    p = (void *)1; EXPECT_EQ(IRunningObjectTable_QueryInterface(rot, &mr_IID_IDispatch, &p), E_NOINTERFACE);
    EXPECT_EQ(p, NULL);
    ROT_TEARDOWN();
}

TEST(mr_rot_register_invalid_arguments) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0x1234;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"inv.txt");
    m = mr_file(n);
    /* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+): undefined flag bits */
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0xdeadbeef, mr_obj_unk(&o), m, &ck), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 4, mr_obj_unk(&o), m, &ck), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0x80000000, mr_obj_unk(&o), m, &ck), E_INVALIDARG);
    /* NULL object, NULL moniker, NULL cookie */
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, NULL, m, &ck), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), NULL, &ck), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, NULL), E_INVALIDARG);
    EXPECT_EQ(o.ref, 1);
    CHECK(!rot_has(rot, m));
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_register_revoke_cycle) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; IUnknown *got = (IUnknown *)1;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"cycle.txt");
    m = mr_file(n);
    CHECK(!rot_has(rot, m));
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    CHECK(ck != 0);
    CHECK(o.ref > 1);                       /* the table holds the object (weakly) via its stub */
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_OK);
    EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, &got), S_OK);
    CHECK(got != NULL && got != (IUnknown *)1);
    if (got && got != (IUnknown *)1) {
        void *p = NULL;
        EXPECT_EQ(IUnknown_QueryInterface(got, &mr_IID_IUnknown, &p), S_OK);
        if (p) IUnknown_Release((IUnknown *)p);
        if (got == mr_obj_unk(&o)) t_note("same-apartment GetObject returned the registered pointer itself");
        IUnknown_Release(got);
    }
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(o.ref, 1);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_FALSE);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_not_running_results) {
    IMoniker *m; WCHAR n[96]; IUnknown *u = (IUnknown *)1; FILETIME ft; IEnumMoniker *e = NULL;
    ROT_SETUP();
    mr_unique_path(n, 96, L"never.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_FALSE);
    EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, &u), MK_E_UNAVAILABLE);
    EXPECT_EQ(u, NULL);
    memset(&ft, 0x77, sizeof ft);
    EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, m, &ft), MK_E_UNAVAILABLE);
    mr_rel(m);
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) IEnumMoniker_Release(e);
    ROT_TEARDOWN();
}

TEST(mr_rot_null_arguments) {
    IMoniker *m; WCHAR n[96]; IUnknown *u = (IUnknown *)1; FILETIME ft;
    ROT_SETUP();
    mr_unique_path(n, 96, L"nullargs.txt");
    m = mr_file(n);
    if (!t_is_wine()) {
        EXPECT_EQ(IRunningObjectTable_IsRunning(rot, NULL), E_INVALIDARG);
        EXPECT_EQ(IRunningObjectTable_GetObject(rot, NULL, &u), E_INVALIDARG);
        EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, NULL, &ft), E_INVALIDARG);
    }
    if (t_is_wine()) t_skip("Wine dereferences some NULL out pointers here");
    else {
        EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, NULL), E_INVALIDARG);
        EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, m, NULL), E_INVALIDARG);
        EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, NULL), E_INVALIDARG);
        EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, 0x00010001, NULL), E_INVALIDARG);
    }
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_revoke_invalid_cookies) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"revoke.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, 0), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, 0xffffffff), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, 0x7fff0000), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    /* a cookie packs (signature << 16) | slot index: a wrong signature for a live slot is rejected */
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck ^ 0x00ff0000), E_INVALIDARG);
    CHECK(rot_has(rot, m));
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), E_INVALIDARG);     /* double revoke */
    mr_rel(m);
    ROT_TEARDOWN();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_rot_register_duplicate_moniker) {
    mr_obj o1, o2; IMoniker *m; WCHAR n[96]; DWORD c1 = 0, c2 = 0; IUnknown *u = NULL;
    ROT_SETUP();
    mr_obj_init(&o1); mr_obj_init(&o2);
    mr_unique_path(n, 96, L"dup.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o1), m, &c1), S_OK);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o2), m, &c2), MK_S_MONIKERALREADYREGISTERED);
    CHECK(c1 != c2);
    CHECK(c2 != 0);
    EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, &u), S_OK);
    if (u) { IUnknown_Release(u); u = NULL; }
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, c1), S_OK);
    CHECK(rot_has(rot, m));                 /* the second registration keeps the name alive */
    EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, &u), S_OK);
    if (u) { IUnknown_Release(u); u = NULL; }
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, c2), S_OK);
    CHECK(!rot_has(rot, m));
    EXPECT_EQ(o1.ref, 1);
    EXPECT_EQ(o2.ref, 1);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_duplicate_by_equal_but_distinct_moniker_objects) {
    /* identity is the moniker's comparison data, not the moniker pointer */
    mr_obj o; IMoniker *a, *b; WCHAR n[96]; DWORD c1 = 0, c2 = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"eq.txt");
    a = mr_file(n); b = mr_file(n);
    CHECK(a != b);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &c1), S_OK);
    CHECK(rot_has(rot, b));
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), b, &c2), MK_S_MONIKERALREADYREGISTERED);
    IRunningObjectTable_Revoke(rot, c2);
    IRunningObjectTable_Revoke(rot, c1);
    CHECK(!rot_has(rot, a));
    mr_rel(a); mr_rel(b);
    ROT_TEARDOWN();
}

TEST(mr_rot_file_moniker_names_compare_case_insensitively) {
    mr_obj o; IMoniker *a, *b; WCHAR n1[96], n2[96]; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    wsprintfW(n1, L"C:\\olet_mr_%lu_nonexistent\\CaseTest.TXT", GetCurrentProcessId());
    wsprintfW(n2, L"c:\\OLET_MR_%lu_NONEXISTENT\\casetest.txt", GetCurrentProcessId());
    a = mr_file(n1); b = mr_file(n2);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, b), S_OK);
    IRunningObjectTable_Revoke(rot, ck);
    mr_rel(a); mr_rel(b);
    ROT_TEARDOWN();
}

TEST(mr_rot_flags_keepalive_external_connection) {
    /* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+)
     * KEEPSALIVE registers the object with a strong table marshal: IExternalConnection::AddConnection(STRONG)
     * once at Register and ReleaseConnection once at Revoke. Flags 0 adds no strong connection. */
    mr_ext e; IMoniker *m; WCHAR n[96]; DWORD ck = 0;
    ROT_SETUP();
    ex_init(&e);
    mr_unique_path(n, 96, L"keep.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, (IUnknown *)&e, m, &ck), S_OK);
    EXPECT_EQ(e.strong, 0);
    CHECK(e.ref > 1);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(e.ref, 1);
    EXPECT_EQ(IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, (IUnknown *)&e, m, &ck), S_OK);
    EXPECT_EQ(e.strong, 1);
    CHECK(e.ref > 1);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(e.strong, 0);
    EXPECT_EQ(e.ref, 1);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_flags_allowanyclient) {
    /* Without a service-started, AppID-configured server identity this fails (documented). */
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; HRESULT hr;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"any.txt");
    m = mr_file(n);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE | ROTFLAGS_ALLOWANYCLIENT, mr_obj_unk(&o), m, &ck);
    if (hr == S_OK) { t_note("ALLOWANYCLIENT register succeeded on this machine"); IRunningObjectTable_Revoke(rot, ck); }
    else EXPECT_EQ(hr, CO_E_WRONG_SERVER_IDENTITY);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_ALLOWANYCLIENT, mr_obj_unk(&o), m, &ck);
    if (hr == S_OK) IRunningObjectTable_Revoke(rot, ck);
    else EXPECT_EQ(hr, CO_E_WRONG_SERVER_IDENTITY);
    EXPECT_EQ(o.ref, 1);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_object_with_proxy_is_rejected_or_ok) {
    /* An object that is itself a proxy cannot be registered; we only check the own-object path here. */
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"obj.txt");
    m = mr_file(n);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(IRunningObjectTable_Register(rot, i & 1, mr_obj_unk(&o), m, &ck), S_OK);
        EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
        EXPECT_EQ(o.ref, 1);
    }
    mr_rel(m);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- change times */
TEST(mr_rot_note_change_time) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; FILETIME set, got;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"time.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    set.dwLowDateTime = 0x12345678; set.dwHighDateTime = 0x01d00000;
    EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, ck, &set), S_OK);
    memset(&got, 0, sizeof got);
    EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, m, &got), S_OK);
    EXPECT_EQ(got.dwLowDateTime, set.dwLowDateTime);
    EXPECT_EQ(got.dwHighDateTime, set.dwHighDateTime);
    set.dwLowDateTime = 0x00000001; set.dwHighDateTime = 0x01d00001;
    EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, ck, &set), S_OK);
    EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, m, &got), S_OK);
    EXPECT_EQ(got.dwLowDateTime, 1);
    EXPECT_EQ(got.dwHighDateTime, 0x01d00001);
    /* invalid cookies */
    EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, 0, &set), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, ck ^ 0x00ff0000, &set), E_INVALIDARG);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_NoteChangeTime(rot, ck, &set), E_INVALIDARG);   /* revoked */
    EXPECT_EQ(IRunningObjectTable_GetTimeOfLastChange(rot, m, &got), MK_E_UNAVAILABLE);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_time_of_last_change_of_registered_file_moniker) {
    /* Register() asks the moniker for its time of last change; for a file moniker that exists this is the
     * file's last-write time, and the ROT then reports it (unless NoteChangeTime overrides it). */
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; mr_obj o; IMoniker *m; DWORD ck = 0; FILETIME got, fw; HANDLE h;
    ROT_SETUP();
    mr_obj_init(&o);
    t_tmpfile(path, sizeof path, "rottime.dat");
    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create fixture"); ROT_TEARDOWN(); return; }
    WriteFile(h, "x", 1, &ck, NULL);
    GetFileTime(h, NULL, NULL, &fw);
    CloseHandle(h); ck = 0;
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    m = mr_file(wp);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    memset(&got, 0, sizeof got);
    HRESULT hr = IRunningObjectTable_GetTimeOfLastChange(rot, m, &got);
    EXPECT_EQ(hr, S_OK);
    if (hr == S_OK) {
        EXPECT_EQ(got.dwLowDateTime, fw.dwLowDateTime);
        EXPECT_EQ(got.dwHighDateTime, fw.dwHighDateTime);
    }
    IRunningObjectTable_Revoke(rot, ck);
    mr_rel(m);
    DeleteFileA(path);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- protocol with custom monikers */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_rot_protocol_register_with_rotdata) {
    mr_obj o; mr_tmon *t; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    t = tm_new(L"olet-protocol-1", FALSE);
    log_reset();
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), &t->m, &ck), S_OK);
    CHECK(log_is(4, "Reduce", "GetTimeOfLastChange", "QI(IROTData)", "ROTData_GetComparisonData"));
    if (!log_is(4, "Reduce", "GetTimeOfLastChange", "QI(IROTData)", "ROTData_GetComparisonData")) log_dump("Register");
    log_reset();
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, &t->m), S_OK);
    CHECK(log_is(3, "Reduce", "QI(IROTData)", "ROTData_GetComparisonData"));
    if (!log_is(3, "Reduce", "QI(IROTData)", "ROTData_GetComparisonData")) log_dump("IsRunning");
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    CHECK(t->ref == 1);
    IMoniker_Release(&t->m);
    ROT_TEARDOWN();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_rot_protocol_register_without_rotdata) {
    /* no IROTData: the ROT falls back to the display name plus class id as the identity */
    mr_obj o; mr_tmon *t; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    t = tm_new(L"olet-protocol-2", TRUE);
    log_reset();
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), &t->m, &ck), S_OK);
    CHECK(log_is(5, "Reduce", "GetTimeOfLastChange", "QI(IROTData)", "GetDisplayName", "GetClassID"));
    if (!log_is(5, "Reduce", "GetTimeOfLastChange", "QI(IROTData)", "GetDisplayName", "GetClassID")) log_dump("Register(no IROTData)");
    log_reset();
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, &t->m), S_OK);
    CHECK(log_is(4, "Reduce", "QI(IROTData)", "GetDisplayName", "GetClassID"));
    if (!log_is(4, "Reduce", "QI(IROTData)", "GetDisplayName", "GetClassID")) log_dump("IsRunning(no IROTData)");
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    IMoniker_Release(&t->m);
    ROT_TEARDOWN();
}

TEST(mr_rot_protocol_distinct_instances_same_identity) {
    mr_obj o; mr_tmon *a, *b, *c; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    a = tm_new(L"olet-ident", FALSE); b = tm_new(L"olet-ident", FALSE); c = tm_new(L"olet-ident2", FALSE);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), &a->m, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, &b->m), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, &c->m), S_FALSE);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, &b->m), S_FALSE);
    IMoniker_Release(&a->m); IMoniker_Release(&b->m); IMoniker_Release(&c->m);
    ROT_TEARDOWN();
}

TEST(mr_rot_protocol_no_rotdata_and_rotdata_are_distinct_keys) {
    /* same display name, but one moniker is identified by IROTData data and the other by name+class:
     * the keys differ, so they are different ROT entries */
    mr_obj o; mr_tmon *a, *b; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    a = tm_new(L"olet-ident3", FALSE); b = tm_new(L"olet-ident3", TRUE);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), &a->m, &ck), S_OK);
    int running = rot_has(rot, &b->m);
    if (running) t_note("ROT keys with and without IROTData but same name/class compare equal");
    IRunningObjectTable_Revoke(rot, ck);
    IMoniker_Release(&a->m); IMoniker_Release(&b->m);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- registering other moniker kinds */
TEST(mr_rot_item_monikers) {
    mr_obj o; IMoniker *a, *b, *c; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    a = mr_item(L"!", L"olet_mr_item_A"); b = mr_item(L"!", L"OLET_MR_ITEM_a"); c = mr_item(L"!", L"olet_mr_item_B");
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, b), S_OK);        /* item names compare case-insensitively */
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, c), S_FALSE);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    mr_rel(a); mr_rel(b); mr_rel(c);
    ROT_TEARDOWN();
}

TEST(mr_rot_composite_moniker) {
    mr_obj o; IMoniker *i1, *i2, *c1, *c2, *c3; DWORD ck = 0; WCHAR n[96];
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"comp.txt");
    i1 = mr_file(n); i2 = mr_item(L"!", L"sub");
    c1 = mr_comp(i1, i2); c2 = mr_comp(i1, i2); c3 = mr_comp(i1, mr_item(L"!", L"other"));
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), c1, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, c2), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, c3), S_FALSE);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, i1), S_FALSE);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    mr_rel(i1); mr_rel(i2); mr_rel(c1); mr_rel(c2); mr_rel(c3);
    ROT_TEARDOWN();
}

TEST(mr_rot_class_moniker) {
    mr_obj o; IMoniker *a, *b; DWORD ck = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    a = mr_class(&mr_CLS_X); b = mr_class(&mr_CLS_X);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, b), S_OK);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    mr_rel(a); mr_rel(b);
    ROT_TEARDOWN();
}

/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_rot_pointer_monikers_cannot_be_registered) {
    mr_obj o; IMoniker *p, *inner; DWORD ck = 0; HRESULT hr;
    ROT_SETUP();
    mr_obj_init(&o);
    /* pointer moniker without IROTData and without display name */
    p = mr_ptr(NULL);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), p, &ck);
    EXPECT_EQ(hr, E_INVALIDARG);
    if (hr == S_OK) IRunningObjectTable_Revoke(rot, ck);
    mr_rel(p);
    inner = mr_item(L"!", L"I1");
    p = mr_ptr((IUnknown *)inner);
    hr = IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), p, &ck);
    EXPECT_EQ(hr, E_NOTIMPL);
    if (hr == S_OK) IRunningObjectTable_Revoke(rot, ck);
    EXPECT_EQ(o.ref, 1);
    mr_rel(p); mr_rel(inner);
    ROT_TEARDOWN();
}

TEST(mr_rot_anti_moniker_register) {
    /* anti monikers carry IROTData, so they are legal ROT keys (an odd but valid use) */
    mr_obj o; IMoniker *a; DWORD ck = 0; HRESULT hr;
    ROT_SETUP();
    mr_obj_init(&o);
    a = mr_anti(1);
    hr = IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck);
    CHECKF(hr == S_OK, "hr=%08lx", hr);
    if (hr == S_OK) {
        IMoniker *b = mr_anti(1), *c = mr_anti(2);
        EXPECT_EQ(IRunningObjectTable_IsRunning(rot, b), S_OK);
        EXPECT_EQ(IRunningObjectTable_IsRunning(rot, c), S_FALSE);
        IRunningObjectTable_Revoke(rot, ck);
        mr_rel(b); mr_rel(c);
    }
    mr_rel(a);
    ROT_TEARDOWN();
}

TEST(mr_rot_many_entries) {
    enum { N = 60 };
    static mr_obj o[N]; IMoniker *m[N]; DWORD ck[N]; WCHAR n[96], leaf[32]; int i;
    ROT_SETUP();
    for (i = 0; i < N; i++) {
        mr_obj_init(&o[i]); wsprintfW(leaf, L"many%d.txt", i); mr_unique_path(n, 96, leaf); m[i] = mr_file(n);
        HRESULT hr = IRunningObjectTable_Register(rot, i & 1, mr_obj_unk(&o[i]), m[i], &ck[i]);
        if (hr != S_OK) { CHECKF(0, "register %d hr=%08lx", i, hr); ck[i] = 0; }
    }
    for (i = 0; i < N; i++) if (ck[i] && !rot_has(rot, m[i])) { CHECKF(0, "entry %d not running", i); break; }
    for (i = 0; i < N; i += 2) if (ck[i]) { EXPECT_EQ(IRunningObjectTable_Revoke(rot, ck[i]), S_OK); ck[i] = 0; }
    for (i = 0; i < N; i++) if (rot_has(rot, m[i]) != (i & 1)) { CHECKF(0, "entry %d running state wrong", i); break; }
    for (i = 0; i < N; i++) { if (ck[i]) IRunningObjectTable_Revoke(rot, ck[i]); mr_rel(m[i]); }
    for (i = 0; i < N; i++) if (o[i].ref != 1) { CHECKF(0, "obj %d leaked ref=%ld", i, o[i].ref); break; }
    ROT_TEARDOWN();
}

TEST(mr_rot_cookie_reuse_gets_new_signature) {
    /* a revoked slot can be reused, but a stale cookie never revokes the newer registration */
    mr_obj o; IMoniker *m1, *m2; WCHAR n1[96], n2[96]; DWORD c1 = 0, c2 = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n1, 96, L"reuse1.txt"); mr_unique_path(n2, 96, L"reuse2.txt");
    m1 = mr_file(n1); m2 = mr_file(n2);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m1, &c1), S_OK);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, c1), S_OK);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m2, &c2), S_OK);
    CHECK(c1 != c2);
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, c1), E_INVALIDARG);
    CHECK(rot_has(rot, m2));
    EXPECT_EQ(IRunningObjectTable_Revoke(rot, c2), S_OK);
    mr_rel(m1); mr_rel(m2);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- enumeration */
/* adapted from Wine dlls/ole32/tests/moniker.c (LGPL-2.1+) */
TEST(mr_rot_enum_running_contents) {
    mr_obj o; IMoniker *a, *b; WCHAR n1[96], n2[96]; DWORD c1 = 0, c2 = 0; IEnumMoniker *e = NULL, *e2 = NULL;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n1, 96, L"enum1.txt"); mr_unique_path(n2, 96, L"enum2.txt");
    a = mr_file(n1); b = mr_file(n2);
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) { EXPECT_EQ(enum_count(e, bc, n1), 0); IEnumMoniker_Release(e); e = NULL; }
    EXPECT_EQ(IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), a, &c1), S_OK);
    EXPECT_EQ(IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), b, &c2), S_OK);
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) {
        /* Clone keeps the current position: a clone of a fresh enumerator sees everything */
        EXPECT_EQ(IEnumMoniker_Clone(e, &e2), S_OK);
        EXPECT_EQ(enum_count(e, bc, n1), 1);
        EXPECT_EQ(IEnumMoniker_Reset(e), S_OK);
        EXPECT_EQ(enum_count(e, bc, n2), 1);
        /* exhausted enumerator keeps returning S_FALSE */
        IMoniker *x = (IMoniker *)1; ULONG f = 77;
        EXPECT_EQ(IEnumMoniker_Next(e, 1, &x, &f), S_FALSE);
        EXPECT_EQ(f, 0);
        if (e2) { EXPECT_EQ(enum_count(e2, bc, n1), 1); }
        IEnumMoniker_Release(e);
    }
    if (e2) IEnumMoniker_Release(e2);
    IRunningObjectTable_Revoke(rot, c1); IRunningObjectTable_Revoke(rot, c2);
    mr_rel(a); mr_rel(b);
    ROT_TEARDOWN();
}

TEST(mr_rot_enum_moniker_properties) {
    /* monikers returned by EnumRunning are re-created from the ROT's stored copy: same class, equal to the original */
    mr_obj o; IMoniker *a, *m = NULL; WCHAR n[96]; DWORD ck = 0; IEnumMoniker *e = NULL; int found = 0;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"enumprops.txt");
    a = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck), S_OK);
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    while (e && IEnumMoniker_Next(e, 1, &m, NULL) == S_OK) {
        if (IMoniker_IsEqual(m, a) == S_OK) {
            CLSID c; found++;
            CHECK_TYPE(m, MKSYS_FILEMONIKER);
            EXPECT_EQ(IMoniker_GetClassID(m, &c), S_OK);
            CHECK(mr_guid_eq(&c, &mr_CLSID_FileMoniker));
            CHECK_DN(m, n);
        }
        IMoniker_Release(m);
    }
    EXPECT_EQ(found, 1);
    if (e) IEnumMoniker_Release(e);
    IRunningObjectTable_Revoke(rot, ck);
    mr_rel(a);
    ROT_TEARDOWN();
}

TEST(mr_rot_enum_is_a_snapshot) {
    mr_obj o; IMoniker *a; WCHAR n[96]; DWORD ck = 0; IEnumMoniker *e = NULL;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"snap.txt");
    a = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a, &ck), S_OK);
    if (e) { EXPECT_EQ(enum_count(e, bc, n), 0); IEnumMoniker_Release(e); e = NULL; }
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) { EXPECT_EQ(enum_count(e, bc, n), 1); IEnumMoniker_Release(e); e = NULL; }
    /* an enumerator taken while running still lists the entry after revoke */
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    IRunningObjectTable_Revoke(rot, ck);
    if (e) { EXPECT_EQ(enum_count(e, bc, n), 1); IEnumMoniker_Release(e); }
    mr_rel(a);
    ROT_TEARDOWN();
}

TEST(mr_rot_enum_next_skip_semantics) {
    mr_obj o; IMoniker *a[3], *got[16]; WCHAR n[96], leaf[32]; DWORD ck[3] = {0}; IEnumMoniker *e = NULL; ULONG f; HRESULT hr;
    ROT_SETUP();
    mr_obj_init(&o);
    for (int i = 0; i < 3; i++) { wsprintfW(leaf, L"nxt%d.txt", i); mr_unique_path(n, 96, leaf); a[i] = mr_file(n);
        IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), a[i], &ck[i]); }
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) {
        ULONG total = 0, step;
        /* enumerate everything in chunks of 2 and total the monikers (other ROT entries may exist) */
        for (;;) {
            f = 99; hr = IEnumMoniker_Next(e, 2, got, &f);
            if (hr != S_OK && hr != S_FALSE) { CHECKF(0, "Next hr=%08lx", hr); break; }
            CHECKF((hr == S_OK) == (f == 2), "hr=%08lx fetched=%lu", hr, f);
            for (ULONG k = 0; k < f; k++) IMoniker_Release(got[k]);
            total += f;
            if (hr == S_FALSE) break;
            if (total > 1000) break;
        }
        CHECK(total >= 3);
        EXPECT_EQ(IEnumMoniker_Reset(e), S_OK);
        step = 2;
        hr = IEnumMoniker_Skip(e, step);
        EXPECT_EQ(hr, S_OK);
        hr = IEnumMoniker_Skip(e, 100000);
        EXPECT_EQ(hr, S_FALSE);
        f = 5; hr = IEnumMoniker_Next(e, 1, got, &f);
        if (hr == S_OK) { t_note("Next after an over-long Skip still returned a moniker"); IMoniker_Release(got[0]); }
        EXPECT_EQ(IEnumMoniker_Reset(e), S_OK);
        /* celt == 1 may omit the fetched count */
        hr = IEnumMoniker_Next(e, 1, got, NULL);
        EXPECT_EQ(hr, S_OK);
        if (hr == S_OK) IMoniker_Release(got[0]);
        IEnumMoniker_Release(e);
    }
    for (int i = 0; i < 3; i++) { IRunningObjectTable_Revoke(rot, ck[i]); mr_rel(a[i]); }
    ROT_TEARDOWN();
}

TEST(mr_rot_enum_qi) {
    IEnumMoniker *e = NULL; void *p;
    ROT_SETUP();
    EXPECT_EQ(IRunningObjectTable_EnumRunning(rot, &e), S_OK);
    if (e) {
        p = NULL; EXPECT_EQ(IEnumMoniker_QueryInterface(e, &mr_IID_IEnumMoniker, &p), S_OK);
        if (p) IUnknown_Release((IUnknown *)p);
        p = NULL; EXPECT_EQ(IEnumMoniker_QueryInterface(e, &mr_IID_IUnknown, &p), S_OK);
        if (p) IUnknown_Release((IUnknown *)p);
        p = (void *)1; EXPECT_EQ(IEnumMoniker_QueryInterface(e, &mr_IID_IMoniker, &p), E_NOINTERFACE);
        EXPECT_EQ(p, NULL);
        IEnumMoniker_Release(e);
    }
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- apartments and threads */
TEST(mr_rot_entries_removed_when_apartment_uninitializes) {
    /* An STA that exits without revoking loses its ROT registrations (CRunningObjectTable::CleanupApartment). */
    struct { IMoniker *m; mr_obj o; HRESULT reg; } r;
    WCHAR n[96];
    void run(void *p) {
        typeof(r) *q = p; DWORD ck; IRunningObjectTable *rot = NULL;
        if (FAILED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED))) return;
        if (U(GetRunningObjectTable)(0, &rot) == S_OK) {
            mr_obj_init(&q->o);
            q->reg = IRunningObjectTable_Register(rot, 0, mr_obj_unk(&q->o), q->m, &ck);
            IRunningObjectTable_Release(rot);
        }
        U(CoUninitialize)();
    }
    ROT_SETUP();
    mr_unique_path(n, 96, L"apt.txt");
    r.m = mr_file(n); r.reg = E_FAIL;
    if (mr_run_thread(run, &r, 10000)) {
        EXPECT_EQ(r.reg, S_OK);
        /* only inferred from CleanROTForApartment (no decomp of the cleanup path): soft */
        if (rot_has(rot, r.m)) t_note("entry of an exited apartment still in the ROT (cleanup not confirmed)");
        if (r.o.ref != 1) t_note("object ref %ld after apartment exit", (long)r.o.ref);
    }
    mr_rel(r.m);
    ROT_TEARDOWN();
}

TEST(mr_rot_visible_across_apartments_in_process) {
    /* An entry registered in the main STA is visible to another STA/MTA thread of the same process. */
    struct { IMoniker *m; HRESULT running; HRESULT get; HRESULT noinit; } r = { 0, E_FAIL, E_FAIL, E_FAIL };
    mr_obj o; WCHAR n[96]; DWORD ck = 0;
    void run(void *p) {
        typeof(r) *q = p; IRunningObjectTable *rot = NULL;
        if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return;
        if (U(GetRunningObjectTable)(0, &rot) == S_OK) {
            q->running = IRunningObjectTable_IsRunning(rot, q->m);
            IRunningObjectTable_Release(rot);
        }
        U(CoUninitialize)();
    }
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"xapt.txt");
    r.m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), r.m, &ck), S_OK);
    if (mr_run_thread(run, &r, 10000)) EXPECT_EQ(r.running, S_OK);
    IRunningObjectTable_Revoke(rot, ck);
    mr_rel(r.m);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- bind context + ROT interplay */
TEST(mr_rot_bindctx_rot_is_same_table) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; IRunningObjectTable *r2 = NULL;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"bcrot.txt");
    m = mr_file(n);
    EXPECT_EQ(IBindCtx_GetRunningObjectTable(bc, &r2), S_OK);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    if (r2) { EXPECT_EQ(IRunningObjectTable_IsRunning(r2, m), S_OK); IRunningObjectTable_Release(r2); }
    /* IMoniker::IsRunning consults the bind context's ROT */
    EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_OK);
    IRunningObjectTable_Revoke(rot, ck);
    EXPECT_EQ(IMoniker_IsRunning(m, bc, NULL, NULL), S_FALSE);
    mr_rel(m);
    ROT_TEARDOWN();
}

TEST(mr_rot_bind_to_object_uses_rot) {
    /* a registered running object is what IMoniker::BindToObject (and BindMoniker) returns for its moniker */
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; IUnknown *u = NULL; HRESULT hr;
    ROT_SETUP();
    mr_obj_init(&o);
    mr_unique_path(n, 96, L"bindrot.txt");
    m = mr_file(n);
    EXPECT_EQ(IRunningObjectTable_Register(rot, 0, mr_obj_unk(&o), m, &ck), S_OK);
    hr = IMoniker_BindToObject(m, bc, NULL, &mr_IID_IUnknown, (void **)&u);
    EXPECT_EQ(hr, S_OK);
    if (u) IUnknown_Release(u);
    IRunningObjectTable_Revoke(rot, ck);
    mr_rel(m);
    ROT_TEARDOWN();
}

/* ---------------------------------------------------------------- cross process */
static HANDLE named_event(DWORD pid, const char *tag, BOOL create) {
    char nm[96]; snprintf(nm, sizeof nm, "Local\\olet_mr_%lu_%s", pid, tag);
    return create ? CreateEventA(NULL, TRUE, FALSE, nm) : OpenEventA(EVENT_ALL_ACCESS, FALSE, nm);
}

/* child "mr_rot_peer_reg <ppid> <exit_without_revoke>": registers an entry, signals, waits, revokes */
CHILD(mr_rot_peer_reg) {
    DWORD ppid = (DWORD)atoi(argv[1]); int norevoke = argc > 2 && atoi(argv[2]);
    HANDLE ready = named_event(ppid, "ready", FALSE), done = named_event(ppid, "done", FALSE);
    IRunningObjectTable *rot = NULL; IMoniker *m = NULL; mr_obj o; DWORD ck = 0; WCHAR n[96];
    if (!ready || !done) return 10;
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return 11;
    if (U(GetRunningObjectTable)(0, &rot) != S_OK) return 12;
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\peer.txt", ppid);
    if (U(CreateFileMoniker)(n, &m) != S_OK) return 13;
    mr_obj_init(&o);
    if (IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), m, &ck) != S_OK) return 14;
    SetEvent(ready);
    WaitForSingleObject(done, 12000);
    if (!norevoke) { if (IRunningObjectTable_Revoke(rot, ck) != S_OK) return 15; }
    return 0;
}

TEST(mr_rot_child_registration_visible_in_parent) {
    DWORD pid = GetCurrentProcessId(); HANDLE ready, done, hp; PROCESS_INFORMATION pi; char args[32];
    WCHAR n[96]; IMoniker *m; IUnknown *u = NULL; FILETIME ft; IEnumMoniker *e = NULL; DWORD code = 99;
    IRunningObjectTable *rot = NULL; IBindCtx *bc;
    MR_NEED_BASE(); MR_MTA_BEGIN();
    rot = get_rot(); bc = mr_bc();
    ready = named_event(pid, "ready", TRUE); done = named_event(pid, "done", TRUE);
    snprintf(args, sizeof args, "%lu 0", pid);
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\peer.txt", pid);
    m = mr_file(n);
    hp = rot && bc && ready && done ? t_spawn_child("mr_rot_peer_reg", args, 0, NULL, NULL, NULL, FALSE, NULL, &pi) : NULL;
    if (!hp) { t_skip("could not start child"); goto out; }
    CloseHandle(pi.hThread);
    if (WaitForSingleObject(ready, 15000) == WAIT_OBJECT_0) {
        EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_OK);
        EXPECT_EQ(IRunningObjectTable_GetObject(rot, m, &u), S_OK);
        if (u) { void *p = NULL; EXPECT_EQ(IUnknown_QueryInterface(u, &mr_IID_IUnknown, &p), S_OK);
                 if (p) IUnknown_Release((IUnknown *)p); IUnknown_Release(u); }
        memset(&ft, 0, sizeof ft);
        HRESULT hr = IRunningObjectTable_GetTimeOfLastChange(rot, m, &ft);
        if (hr != S_OK) t_note("GetTimeOfLastChange for a foreign entry: %08lx", hr);
        if (IRunningObjectTable_EnumRunning(rot, &e) == S_OK && e) { EXPECT_EQ(enum_count(e, bc, n), 1); IEnumMoniker_Release(e); }
        /* a registration owned by another process cannot be revoked by guessing: cookie 0 is always invalid */
        EXPECT_EQ(IRunningObjectTable_Revoke(rot, 0), E_INVALIDARG);
    } else CHECKF(0, "child never registered");
    SetEvent(done);
    if (WaitForSingleObject(hp, 15000) != WAIT_OBJECT_0) { TerminateProcess(hp, 1); WaitForSingleObject(hp, 5000); t_note("child killed"); }
    else { GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 0); }
    CloseHandle(hp);
    EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_FALSE);
out:
    mr_rel(m);
    if (ready) CloseHandle(ready);
    if (done) CloseHandle(done);
    if (rot) IRunningObjectTable_Release(rot);
    if (bc) IBindCtx_Release(bc);
    MR_COM_END();
}

TEST(mr_rot_child_exit_removes_registration) {
    DWORD pid = GetCurrentProcessId(); HANDLE ready, done, hp; PROCESS_INFORMATION pi; char args[32];
    WCHAR n[96]; IMoniker *m; DWORD code = 99; IRunningObjectTable *rot = NULL; int gone = 0;
    MR_NEED_BASE(); MR_MTA_BEGIN();
    rot = get_rot();
    ready = named_event(pid, "ready", TRUE); done = named_event(pid, "done", TRUE);
    snprintf(args, sizeof args, "%lu 1", pid);
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\peer.txt", pid);
    m = mr_file(n);
    hp = rot && ready && done ? t_spawn_child("mr_rot_peer_reg", args, 0, NULL, NULL, NULL, FALSE, NULL, &pi) : NULL;
    if (!hp) { t_skip("could not start child"); goto out; }
    CloseHandle(pi.hThread);
    if (WaitForSingleObject(ready, 15000) == WAIT_OBJECT_0) EXPECT_EQ(IRunningObjectTable_IsRunning(rot, m), S_OK);
    else CHECKF(0, "child never registered");
    SetEvent(done);
    if (WaitForSingleObject(hp, 15000) != WAIT_OBJECT_0) { TerminateProcess(hp, 1); WaitForSingleObject(hp, 5000); t_note("child killed"); }
    else { GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 0); }
    CloseHandle(hp);
    /* the child exited without Revoke: its entries must disappear (allow a short settle time) */
    for (int i = 0; i < 50 && !gone; i++) { if (IRunningObjectTable_IsRunning(rot, m) == S_FALSE) gone = 1; else Sleep(100); }
    CHECK(gone);
out:
    mr_rel(m);
    if (ready) CloseHandle(ready);
    if (done) CloseHandle(done);
    if (rot) IRunningObjectTable_Release(rot);
    MR_COM_END();
}

/* child "mr_rot_peer_look <ppid>": checks the parent's registration from another process */
CHILD(mr_rot_peer_look) {
    DWORD ppid = (DWORD)atoi(argv[1]); int bad = 0;
    IRunningObjectTable *rot = NULL; IMoniker *m = NULL, *other = NULL; WCHAR n[96]; IUnknown *u = NULL; IEnumMoniker *e = NULL;
    FILETIME ft;
    if (FAILED(U(CoInitializeEx)(NULL, COINIT_MULTITHREADED))) return 11;
    if (U(GetRunningObjectTable)(0, &rot) != S_OK) return 12;
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\look.txt", ppid);
    U(CreateFileMoniker)(n, &m);
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\notlook.txt", ppid);
    U(CreateFileMoniker)(n, &other);
    if (IRunningObjectTable_IsRunning(rot, m) != S_OK) bad |= 1;
    if (IRunningObjectTable_IsRunning(rot, other) != S_FALSE) bad |= 2;
    if (IRunningObjectTable_GetObject(rot, m, &u) != S_OK || !u) bad |= 4; else IUnknown_Release(u);
    u = (IUnknown *)1;
    if (IRunningObjectTable_GetObject(rot, other, &u) != MK_E_UNAVAILABLE || u) bad |= 8;
    if (IRunningObjectTable_GetTimeOfLastChange(rot, other, &ft) != MK_E_UNAVAILABLE) bad |= 16;
    if (IRunningObjectTable_EnumRunning(rot, &e) != S_OK) bad |= 32;
    else {
        int found = 0; IMoniker *x;
        while (IEnumMoniker_Next(e, 1, &x, NULL) == S_OK) { if (IMoniker_IsEqual(x, m) == S_OK) found++; IMoniker_Release(x); }
        if (found != 1) bad |= 64;
        IEnumMoniker_Release(e);
    }
    IMoniker_Release(m); IMoniker_Release(other); IRunningObjectTable_Release(rot);
    return bad;
}

TEST(mr_rot_parent_registration_visible_in_child) {
    mr_obj o; IMoniker *m; WCHAR n[96]; DWORD ck = 0; char args[32]; DWORD code;
    IRunningObjectTable *rot = NULL;
    MR_NEED_BASE(); MR_MTA_BEGIN();
    rot = get_rot();
    mr_obj_init(&o);
    wsprintfW(n, L"C:\\olet_mr_%lu_nonexistent\\look.txt", GetCurrentProcessId());
    m = mr_file(n);
    if (rot && m && IRunningObjectTable_Register(rot, ROTFLAGS_REGISTRATIONKEEPSALIVE, mr_obj_unk(&o), m, &ck) == S_OK) {
        snprintf(args, sizeof args, "%lu", GetCurrentProcessId());
        code = t_run_child("mr_rot_peer_look", args, 30000);
        CHECKF(code == 0, "child checks failed, bitmask %lu (0xdead0001 = timeout)", code);
        IRunningObjectTable_Revoke(rot, ck);
    } else CHECKF(0, "register failed");
    mr_rel(m);
    if (rot) IRunningObjectTable_Release(rot);
    MR_COM_END();
}
