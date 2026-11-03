/* gap10_ole32: internal / undocumented exports of ole32.dll and combase.dll whose Win10 behaviour is visible
 * in the decompilation. Every expectation below was derived from the Win10 22H2 (19041) decomp of the function. */
#include "gap10.h"

/* ------------------------------------------------------------------ IsValid* (ole32, one shared stub) */
/* IsValidIid, IsValidPtrIn, IsValidPtrOut and CoQueryReleaseObject share RVA 0x14b40 in ole32: "return 1",
 * the arguments are never examined (cdecl-style, so any argument values are safe). */
TEST(gap10_isvalid_stub_always_true) {
    NOT_WINE("internal export");
    static const char *const n[] = { "IsValidIid", "IsValidPtrIn", "IsValidPtrOut", "CoQueryReleaseObject" };
    for (int i = 0; i < 4; i++) {
        int (WINAPI *f)(void *, DWORD) = (void *)t_proc_in("ole32.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        EXPECT_EQ(f(NULL, 0), 1);
        EXPECT_EQ(f(NULL, 16), 1);
        EXPECT_EQ(f((void *)1, 0xffffffff), 1);
        EXPECT_EQ(f(&i, sizeof i), 1);
    }
}
TEST(gap10_isvalidinterface_null_check) {
    NOT_WINE("internal export");
    FNIN("ole32.dll", int, IsValidInterface, (void *));
    EXPECT_EQ(IsValidInterface_(NULL), 0);
    int x;
    EXPECT_EQ(IsValidInterface_(&x), 1);
    EXPECT_EQ(IsValidInterface_((void *)(ULONG_PTR)1), 1); /* only a NULL test, never dereferenced */
}

/* ------------------------------------------------------------------ PropSys* wrappers over oleaut32 */
TEST(gap10_propsysallocstring_is_sysallocstring) {
    FN(BSTR, PropSysAllocString, (const OLECHAR *));
    FN(void, PropSysFreeString, (BSTR));
    BSTR b = PropSysAllocString_(L"hello");
    CHECK(b != NULL);
    if (b) {
        EXPECT_EQ(((ULONG *)b)[-1], 10);                   /* byte-length prefix like SysAllocString */
        EXPECT_EQ(b[5], 0);
        CHECK(!memcmp(b, L"hello", 12));
        PropSysFreeString_(b);
    }
    BSTR e = PropSysAllocString_(L"");
    CHECK(e != NULL);
    if (e) { EXPECT_EQ(((ULONG *)e)[-1], 0); EXPECT_EQ(e[0], 0); PropSysFreeString_(e); }
    CHECK(PropSysAllocString_(NULL) == NULL);
    PropSysFreeString_(NULL); /* SysFreeString(NULL) is a no-op */
}

/* ------------------------------------------------------------------ DVTARGETDEVICE helpers */
#pragma pack(push, 1)
typedef struct { DWORD tdSize; WORD drv, dev, port, ext; BYTE data[0xf4]; } DVTD;
typedef struct { DWORD cb, cchDrv, cchDev, cchPort; } DVINFO;
#pragma pack(pop)
typedef DWORD (__cdecl *dvinfo_fn)(DVTD *, DVINFO *);
typedef DWORD (__cdecl *dvconv_fn)(DVTD *, DVINFO *, DVTD *);

static void dv_set(DVTD *d, DWORD sz, WORD a, WORD b, WORD c, WORD e) { memset(d, 0, 0x100); d->tdSize = sz; d->drv = a; d->dev = b; d->port = c; d->ext = e; }

TEST(gap10_dvtd32_info_errors) {
    NOT_WINE("internal export");
    dvinfo_fn f = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd32Info");
    REQUIRE(f);
    DVTD d; DVINFO inf;
    for (DWORD sz = 0; sz < 0xc; sz++) { dv_set(&d, sz, 0, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057); }
    dv_set(&d, 0xc, 0, 0, 0, 0);
    memset(&inf, 0xAA, sizeof inf);
    EXPECT_EQ(f(&d, &inf), 0);
    EXPECT_EQ(inf.cb, 0xc); EXPECT_EQ(inf.cchDrv, 0); EXPECT_EQ(inf.cchDev, 0); EXPECT_EQ(inf.cchPort, 0);
    /* offsets must lie in [0xc, tdSize] */
    dv_set(&d, 0x20, 0xb, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 1, 0, 0, 0);   EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0x21, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0xb, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0x21, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0xb, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0x21, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0, 0xb); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0, 0x21); EXPECT_EQ(f(&d, &inf), 0x80070057);
}
TEST(gap10_dvtd16_info_errors) {
    NOT_WINE("internal export");
    dvinfo_fn f = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd16Info");
    REQUIRE(f);
    DVTD d; DVINFO inf;
    for (DWORD sz = 0; sz < 0xc; sz++) { dv_set(&d, sz, 0, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057); }
    dv_set(&d, 0xc, 0, 0, 0, 0);
    memset(&inf, 0xAA, sizeof inf);
    EXPECT_EQ(f(&d, &inf), 0);
    EXPECT_EQ(inf.cchDrv, 0); EXPECT_EQ(inf.cchDev, 0); EXPECT_EQ(inf.cchPort, 0);
    dv_set(&d, 0x20, 0xb, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0x21, 0, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0x21, 0, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0x21, 0); EXPECT_EQ(f(&d, &inf), 0x80070057);
    dv_set(&d, 0x20, 0, 0, 0, 0x21); EXPECT_EQ(f(&d, &inf), 0x80070057);
}

/* Wide ("32") layout: header 0xc, "drv" at 0xc (8 bytes), "device" at 0x14 (14), "port" at 0x22 (10), size 0x2c. */
static void dv_build32(DVTD *d) {
    dv_set(d, 0x2c, 0xc, 0x14, 0x22, 0);
    memcpy((BYTE *)d + 0xc, L"drv", 8);
    memcpy((BYTE *)d + 0x14, L"device", 14);
    memcpy((BYTE *)d + 0x22, L"port", 10);
}
TEST(gap10_dvtd32_info_counts_characters) {
    NOT_WINE("internal export");
    dvinfo_fn f = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd32Info");
    REQUIRE(f);
    DVTD d; DVINFO inf;
    dv_build32(&d);
    EXPECT_EQ(f(&d, &inf), 0);
    EXPECT_EQ(inf.cchDrv, 4); EXPECT_EQ(inf.cchDev, 7); EXPECT_EQ(inf.cchPort, 5);   /* incl. terminator */
    EXPECT_EQ(inf.cb, 0x2c);                                                          /* 0xc + 2*(4+7+5) */
    /* a single field: device only */
    dv_set(&d, 0x14, 0, 0xc, 0, 0); memcpy((BYTE *)&d + 0xc, L"ab", 6);
    EXPECT_EQ(f(&d, &inf), 0);
    EXPECT_EQ(inf.cchDrv, 0); EXPECT_EQ(inf.cchDev, 3); EXPECT_EQ(inf.cchPort, 0); EXPECT_EQ(inf.cb, 0xc + 6);
}
TEST(gap10_dvtd16_info_counts_characters) {
    NOT_WINE("internal export");
    dvinfo_fn f = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd16Info");
    REQUIRE(f);
    DVTD d; DVINFO inf;
    /* ANSI ("16") layout: "drv\0" at 0xc, "device\0" at 0x10, "port\0" at 0x17, size 0x1c */
    dv_set(&d, 0x1c, 0xc, 0x10, 0x17, 0);
    memcpy((BYTE *)&d + 0xc, "drv", 4); memcpy((BYTE *)&d + 0x10, "device", 7); memcpy((BYTE *)&d + 0x17, "port", 5);
    EXPECT_EQ(f(&d, &inf), 0);
    EXPECT_EQ(inf.cchDrv, 4); EXPECT_EQ(inf.cchDev, 7); EXPECT_EQ(inf.cchPort, 5);
    EXPECT_EQ(inf.cb, 0x2c);   /* size of the wide form: 0xc + 2*(4+7+5) */
}
TEST(gap10_dvtd_convert_roundtrip) {
    NOT_WINE("internal export");
    dvinfo_fn i32 = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd32Info"), i16 = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd16Info");
    dvconv_fn c3216 = (dvconv_fn)t_proc_in("ole32.dll", "UtConvertDvtd32toDvtd16"), c1632 = (dvconv_fn)t_proc_in("ole32.dll", "UtConvertDvtd16toDvtd32");
    REQUIRE(i32); REQUIRE(i16); REQUIRE(c3216); REQUIRE(c1632);
    DVTD w, a, w2; DVINFO inf, inf2;
    dv_build32(&w);
    EXPECT_EQ(i32(&w, &inf), 0);
    memset(&a, 0xEE, sizeof a);
    EXPECT_EQ(c3216(&w, &inf, &a), 0);
    /* the narrow form packs the strings back to back with byte-sized characters */
    EXPECT_EQ(a.drv, 0xc); EXPECT_EQ(a.dev, 0x10); EXPECT_EQ(a.port, 0x17); EXPECT_EQ(a.ext, 0); EXPECT_EQ(a.tdSize, 0x1c);
    CHECK(!memcmp((BYTE *)&a + 0xc, "drv", 4));
    CHECK(!memcmp((BYTE *)&a + 0x10, "device", 7));
    CHECK(!memcmp((BYTE *)&a + 0x17, "port", 5));
    EXPECT_EQ(i16(&a, &inf2), 0);
    EXPECT_EQ(inf2.cchDrv, 4); EXPECT_EQ(inf2.cchDev, 7); EXPECT_EQ(inf2.cchPort, 5); EXPECT_EQ(inf2.cb, 0x2c);
    memset(&w2, 0xEE, sizeof w2);
    EXPECT_EQ(c1632(&a, &inf2, &w2), 0);
    EXPECT_EQ(w2.tdSize, 0x2c); EXPECT_EQ(w2.drv, 0xc); EXPECT_EQ(w2.dev, 0x14); EXPECT_EQ(w2.port, 0x22); EXPECT_EQ(w2.ext, 0);
    CHECK(!memcmp(&w2, &w, 0x2c));
}
TEST(gap10_dvtd_convert_only_device) {
    NOT_WINE("internal export");
    dvinfo_fn i32 = (dvinfo_fn)t_proc_in("ole32.dll", "UtGetDvtd32Info");
    dvconv_fn c3216 = (dvconv_fn)t_proc_in("ole32.dll", "UtConvertDvtd32toDvtd16");
    REQUIRE(i32); REQUIRE(c3216);
    DVTD w, a; DVINFO inf;
    dv_set(&w, 0x14, 0, 0xc, 0, 0); memcpy((BYTE *)&w + 0xc, L"ab", 6);
    EXPECT_EQ(i32(&w, &inf), 0);
    EXPECT_EQ(c3216(&w, &inf, &a), 0);
    EXPECT_EQ(a.drv, 0); EXPECT_EQ(a.dev, 0xc); EXPECT_EQ(a.port, 0); EXPECT_EQ(a.tdSize, 0xc + 3);
    CHECK(!memcmp((BYTE *)&a + 0xc, "ab", 3));
}

/* ------------------------------------------------------------------ storage property helpers */
static IStorage *mk_stg(const char *leaf) {
    HRESULT (WINAPI *create)(const OLECHAR *, DWORD, DWORD, IStorage **) = (void *)t_proc("StgCreateDocfile");
    if (!create) return NULL;
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; IStorage *s = NULL;
    t_tmpfile(p, sizeof p, leaf);
    MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH);
    if (create(w, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &s) != S_OK) return NULL;
    return s;
}
TEST(gap10_write_read_storage_properties) {
    NOT_WINE("internal export");
    FN(DWORD, WriteStorageProperties, (IStorage *, const GUID *, WORD, const WCHAR *));
    FN(DWORD, ReadStorageProperties, (IStorage *, GUID *));
    HRESULT (WINAPI *ReadClassStg_)(IStorage *, GUID *) = (void *)t_proc("ReadClassStg");
    HRESULT (WINAPI *ReadFmtUserTypeStg_)(IStorage *, CLIPFORMAT *, WCHAR **) = (void *)t_proc("ReadFmtUserTypeStg");
    void (WINAPI *CoTaskMemFree_)(void *) = (void *)t_proc("CoTaskMemFree");
    REQUIRE(ReadClassStg_); REQUIRE(ReadFmtUserTypeStg_); REQUIRE(CoTaskMemFree_);
    HRESULT hr = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    CHECK(SUCCEEDED(hr));
    IStorage *s = mk_stg("gap10_wsp.stg");
    if (!s) { t_skip("cannot create docfile"); U(CoUninitialize)(); return; }
    static const GUID clsid = { 0x6a5e1a40, 0x2b7c, 0x4d3e, { 0x91, 0xaa, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };
    EXPECT_EQ(WriteStorageProperties_(s, &clsid, 0xC123, L"Gap10 Type"), S_OK);
    GUID g = gNULL, g2 = gNULL;
    EXPECT_EQ(ReadStorageProperties_(s, &g), S_OK);       /* = ReadClassStg */
    EXPECT_EQ(ReadClassStg_(s, &g2), S_OK);
    CHECK(IsEqualGUID(&g, &clsid)); CHECK(IsEqualGUID(&g2, &clsid));
    CLIPFORMAT cf = 0; WCHAR *ut = NULL;
    EXPECT_EQ(ReadFmtUserTypeStg_(s, &cf, &ut), S_OK);
    EXPECT_EQ(cf, 0xC123);
    CHECK(ut && !lstrcmpW(ut, L"Gap10 Type"));
    if (ut) CoTaskMemFree_(ut);
    s->lpVtbl->Release(s);
    U(CoUninitialize)();
}
TEST(gap10_read_storage_properties_matches_readclassstg_on_fresh_storage) {
    NOT_WINE("internal export");
    FN(DWORD, ReadStorageProperties, (IStorage *, GUID *));
    HRESULT (WINAPI *ReadClassStg_)(IStorage *, GUID *) = (void *)t_proc("ReadClassStg");
    REQUIRE(ReadClassStg_);
    CHECK(SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED)));
    IStorage *s = mk_stg("gap10_fresh.stg");
    if (!s) { t_skip("cannot create docfile"); U(CoUninitialize)(); return; }
    GUID a, b; memset(&a, 0x11, sizeof a); memset(&b, 0x22, sizeof b);
    HRESULT h1 = ReadStorageProperties_(s, &a), h2 = ReadClassStg_(s, &b);
    EXPECT_EQ(h1, h2);
    CHECK(IsEqualGUID(&a, &b));
    t_note("fresh docfile ReadClassStg hr=0x%lx clsid.Data1=0x%lx", (unsigned long)h2, (unsigned long)b.Data1);
    s->lpVtbl->Release(s);
    U(CoUninitialize)();
}
TEST(gap10_openorcreatestream_create_then_open) {
    NOT_WINE("internal export");
    FN(DWORD, OpenOrCreateStream, (IStorage *, const WCHAR *, IStream **));
    CHECK(SUCCEEDED(U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED)));
    IStorage *s = mk_stg("gap10_ooc.stg");
    if (!s) { t_skip("cannot create docfile"); U(CoUninitialize)(); return; }
    IStream *st = NULL;
    EXPECT_EQ(OpenOrCreateStream_(s, L"Gap10Stream", &st), S_OK);   /* CreateStream succeeded */
    CHECK(st != NULL);
    if (st) {
        ULONG n = 0;
        EXPECT_EQ(st->lpVtbl->Write(st, "payload", 7, &n), S_OK); EXPECT_EQ(n, 7);
        st->lpVtbl->Release(st);
    }
    IStream *st2 = NULL;
    EXPECT_EQ(OpenOrCreateStream_(s, L"Gap10Stream", &st2), S_OK);  /* CreateStream says STG_E_FILEALREADYEXISTS -> OpenStream */
    CHECK(st2 != NULL);
    if (st2) {
        char buf[16] = { 0 }; ULONG n = 0;
        EXPECT_EQ(st2->lpVtbl->Read(st2, buf, sizeof buf, &n), S_OK);
        EXPECT_EQ(n, 7); CHECK(!memcmp(buf, "payload", 7));
        st2->lpVtbl->Release(st2);
    }
    s->lpVtbl->Release(s);
    U(CoUninitialize)();
}

/* ------------------------------------------------------------------ combase internals */
TEST(gap10_internal_stub_group_not_supported) {
    NOT_WINE("internal export");
    /* ten Internal* exports are one function in combase (RVA 0x18fe80): return CO_E_NOT_SUPPORTED-like 0x80004021 */
    static const char *const n[] = { "InternalCMLSendReceive", "InternalCanMakeOutCall", "InternalCompleteObjRef", "InternalFillLocalOXIDInfo",
        "InternalGetWindowPropInterface", "InternalMarshalObjRef", "InternalRegisterWindowPropInterface", "InternalReleaseMarshalObjRef",
        "InternalStubInvoke", "InternalUnmarshalObjRef" };
    for (int i = 0; i < 10; i++) {
        int (WINAPI *f)(void *, void *, void *, void *) = (void *)t_proc_in("combase.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        CHECKF((unsigned)f(NULL, NULL, NULL, NULL) == 0x80004021u, "%s", n[i]);
        CHECKF((unsigned)f((void *)1, (void *)2, (void *)3, (void *)4) == 0x80004021u, "%s", n[i]);
    }
}
TEST(gap10_internal_irot_stub_group_notimpl) {
    NOT_WINE("internal export");
    static const char *const n[] = { "InternalIrotEnumRunning", "InternalIrotGetObject", "InternalIrotGetObject2",
        "InternalIrotGetTimeOfLastChange", "InternalIrotIsRunning" };
    for (int i = 0; i < 5; i++) {
        int (WINAPI *f)(void *, void *, void *) = (void *)t_proc_in("combase.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        CHECKF((unsigned)f(NULL, NULL, NULL) == 0x80004001u, "%s", n[i]);   /* E_NOTIMPL */
    }
}
TEST(gap10_combase_noop_free_group_returns) {
    NOT_WINE("internal export");
    /* HBRUSH_UserFree & friends, InternalFreeObjRef, InternalSetOleThunkWowPtr: "ret" with no side effect */
    static const char *const n[] = { "HBRUSH_UserFree", "HBRUSH_UserFree64", "HICON_UserFree", "HICON_UserFree64", "HMONITOR_UserFree",
        "HMONITOR_UserFree64", "HRGN_UserFree64", "InternalFreeObjRef", "InternalSetOleThunkWowPtr" };
    for (int i = 0; i < 9; i++) {
        void (WINAPI *f)(void *, void *, void *) = (void *)t_proc_in("combase.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        f(NULL, NULL, NULL);
        f((void *)1, (void *)2, (void *)3);
    }
    CHECK(1);
}
TEST(gap10_coget_process_identifier) {
    NOT_WINE("internal export");
    FN(DWORD, CoGetProcessIdentifier, (GUID *));
    EXPECT_EQ(CoGetProcessIdentifier_(NULL), 0x80070057);
    GUID a, b; memset(&a, 0x5a, sizeof a); memset(&b, 0xa5, sizeof b);
    EXPECT_EQ(CoGetProcessIdentifier_(&a), S_OK);
    EXPECT_EQ(CoGetProcessIdentifier_(&b), S_OK);
    CHECK(IsEqualGUID(&a, &b));                 /* the per-boot rpcss process identifier is constant */
    t_note("process identifier is %s", IsEqualGUID(&a, &gNULL) ? "GUID_NULL (rpcss not contacted yet)" : "set");
}
TEST(gap10_coget_call_state) {
    NOT_WINE("internal export");
    FN(DWORD, CoGetCallState, (int, ULONG *));
    ULONG v = 0;
    EXPECT_EQ(CoGetCallState_(0, &v), S_OK); EXPECT_EQ(v, 0x1a4);   /* CALLTYPE_OUTGOING_CALL */
    EXPECT_EQ(CoGetCallState_(1, &v), S_OK); EXPECT_EQ(v, 0x1ac);   /* CALLTYPE_INCOMING_CALL */
    EXPECT_EQ(CoGetCallState_(2, &v), S_OK); EXPECT_EQ(v, 0x1b0);   /* CALLTYPE_ACTIVATION */
    v = 0x77;
    EXPECT_EQ(CoGetCallState_(3, &v), 0x80070057); EXPECT_EQ(v, 0x77);
    EXPECT_EQ(CoGetCallState_(-1, &v), 0x80070057);
    EXPECT_EQ(CoGetCallState_(0, NULL), 0x80070057);
}
TEST(gap10_coget_activation_state_null_out) {
    NOT_WINE("internal export");
    FN(DWORD, CoGetActivationState, (GUID, ULONG, ULONG *));
    GUID g = gNULL;
    EXPECT_EQ(CoGetActivationState_(g, 0, NULL), 0x80070057);
}
TEST(gap10_coretire_server_null) {
    NOT_WINE("internal export");
    FN(DWORD, CoRetireServer, (GUID *));
    EXPECT_EQ(CoRetireServer_(NULL), 0x80070057);
}
TEST(gap10_corevoke_zero_handles_are_noop) {
    NOT_WINE("internal export");
    FN(DWORD, CoRevokeConsoleHandles, (ULONG_PTR));
    FN(DWORD, CoRevokeRacActivationToken, (ULONG_PTR));
    EXPECT_EQ(CoRevokeConsoleHandles_(0), S_OK);
    EXPECT_EQ(CoRevokeRacActivationToken_(0), S_OK);
}
TEST(gap10_cogetclassversion_invalid_args) {
    NOT_WINE("internal export");
    FN(DWORD, CoGetClassVersion, (uCLSSPEC *, ULONG *, ULONG *));
    uCLSSPEC spec; memset(&spec, 0, sizeof spec);
    ULONG hi = 1, lo = 1;
    EXPECT_EQ(CoGetClassVersion_(NULL, &hi, &lo), 0x80070057);
    EXPECT_EQ(CoGetClassVersion_(&spec, NULL, &lo), 0x80070057);
    EXPECT_EQ(CoGetClassVersion_(&spec, &hi, NULL), 0x80070057);
    spec.tyspec = 1;                                    /* only tyspec 0 (CLSID) is accepted */
    EXPECT_EQ(CoGetClassVersion_(&spec, &hi, &lo), 0x80070057);
    spec.tyspec = 2;
    EXPECT_EQ(CoGetClassVersion_(&spec, &hi, &lo), 0x80070057);
}
TEST(gap10_cogetmodulearchitecture_and_type) {
    NOT_WINE("internal export");
    FNIN("combase.dll", DWORD, CoGetModuleArchitecture, (const WCHAR *, ULONG *));
    FNIN("combase.dll", DWORD, CoGetModuleType, (const WCHAR *, int *));
    ULONG m = 0x1234;
    EXPECT_EQ(CoGetModuleArchitecture_(NULL, &m), 0x80070057); EXPECT_EQ(m, 0);   /* output cleared first */
    m = 0x1234;
    EXPECT_EQ(CoGetModuleArchitecture_(L"\"C:\\x.dll\"", &m), 0x80070057);          /* quoted path rejected */
    EXPECT_EQ(m, 0);
    int ty = 99;
    EXPECT_EQ(CoGetModuleType_(L"kernel32.dll", NULL), 0x80070057);
    EXPECT_EQ(CoGetModuleType_(NULL, &ty), 0x80070057);
    EXPECT_EQ(ty, 0);                                   /* CO_MODULE_UNKNOWN is stored before the lookup */
    WCHAR path[MAX_PATH];
    UINT n = GetSystemDirectoryW(path, MAX_PATH);
    CHECK(n > 0 && n < MAX_PATH - 20);
    lstrcatW(path, L"\\kernel32.dll");
    m = 0; ty = 99;
    EXPECT_EQ(CoGetModuleArchitecture_(path, &m), S_OK);
    EXPECT_EQ(m, IMAGE_FILE_MACHINE_AMD64);
    EXPECT_EQ(CoGetModuleType_(path, &ty), S_OK);
    EXPECT_EQ(ty, 2);                                   /* CO_MODULE_64BIT */
    /* a bare name goes through SearchPathW */
    m = 0; EXPECT_EQ(CoGetModuleArchitecture_(L"kernel32.dll", &m), S_OK); EXPECT_EQ(m, IMAGE_FILE_MACHINE_AMD64);
    /* missing file -> HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) */
    m = 7;
    EXPECT_EQ(CoGetModuleArchitecture_(L"gap10_no_such_module_xyz.dll", &m), 0x80070002);
    EXPECT_EQ(m, 0);
    /* 32-bit module in SysWOW64 */
    WCHAR wow[MAX_PATH];
    UINT (WINAPI *wd)(WCHAR *, UINT) = (void *)t_proc("GetSystemWow64DirectoryW");
    if (wd && wd(wow, MAX_PATH) > 0 && lstrlenW(wow) < MAX_PATH - 20) {
        lstrcatW(wow, L"\\kernel32.dll");
        if (GetFileAttributesW(wow) != INVALID_FILE_ATTRIBUTES) {
            m = 0; ty = 99;
            EXPECT_EQ(CoGetModuleType_(wow, &ty), S_OK);
            EXPECT_EQ(ty, 1);                           /* CO_MODULE_32BIT */
            EXPECT_EQ(CoGetModuleArchitecture_(wow, &m), S_OK);
            EXPECT_EQ(m, IMAGE_FILE_MACHINE_I386);
        }
    }
}
TEST(gap10_cogetsystemwow64directory_matches_kernel32) {
    NOT_WINE("internal export");
    FN(UINT, CoGetSystemWow64DirectoryW, (WCHAR *, UINT));
    UINT (WINAPI *k)(WCHAR *, UINT) = (void *)t_proc_in("kernel32.dll", "GetSystemWow64DirectoryW");
    REQUIRE(k);
    WCHAR a[MAX_PATH], b[MAX_PATH];
    UINT na = CoGetSystemWow64DirectoryW_(a, MAX_PATH), nb = k(b, MAX_PATH);
    EXPECT_EQ(na, nb);
    if (na && nb) CHECK(!lstrcmpW(a, b));
    /* size query protocol is inherited from kernel32 */
    EXPECT_EQ(CoGetSystemWow64DirectoryW_(NULL, 0), k(NULL, 0));
}
TEST(gap10_covrf_thread_state) {
    NOT_WINE("internal export");
    FN(DWORD, CoVrfGetThreadState, (void **));
    FN(void, CoVrfReleaseThreadState, (void *));
    EXPECT_EQ(CoVrfGetThreadState_(NULL), 0x80070057);
    void *st = (void *)1;
    EXPECT_EQ(CoVrfGetThreadState_(&st), S_OK);
    t_note("CoVrfGetThreadState returned snapshot %p (NULL unless the COM verifier thread-state check is enabled)", st);
    CoVrfReleaseThreadState_(st);        /* NULL is accepted and ignored */
    CoVrfReleaseThreadState_(NULL);
}
TEST(gap10_internal_process_and_apartment_initialized) {
    NOT_WINE("internal export");
    FN(int, InternalIsProcessInitialized, (void));
    FN(int, InternalIsApartmentInitialized, (void));
    int p0 = InternalIsProcessInitialized_(), a0 = InternalIsApartmentInitialized_();
    t_note("before CoInitialize: process=%d apartment=%d", p0, a0);
    EXPECT_EQ(p0, 0);
    HRESULT hr = U(CoInitializeEx)(NULL, COINIT_APARTMENTTHREADED);
    CHECK(SUCCEEDED(hr));
    EXPECT_EQ(InternalIsProcessInitialized_(), 1);
    EXPECT_EQ(InternalIsApartmentInitialized_(), 1);
    U(CoUninitialize)();
    EXPECT_EQ(InternalIsProcessInitialized_(), 0);
    hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    CHECK(SUCCEEDED(hr));
    EXPECT_EQ(InternalIsProcessInitialized_(), 1);
    EXPECT_EQ(InternalIsApartmentInitialized_(), 1);
    U(CoUninitialize)();
}
TEST(gap10_internal_surrogate_and_appcontainer) {
    NOT_WINE("internal export");
    FN(int, InternalCoIsSurrogateProcess, (void));
    EXPECT_EQ(InternalCoIsSurrogateProcess_(), 0);
    FN(DWORD, InternalCallerIsAppContainer, (int *));
    int v = 55;
    DWORD hr = InternalCallerIsAppContainer_(&v);      /* no server call in progress on this thread */
    EXPECT_EQ(hr, 0x80010117); EXPECT_EQ(v, 55);
}
TEST(gap10_internal_stdidentity_proxymanager_is_identity) {
    NOT_WINE("internal export");
    FN(void *, InternalCStdIdentityGetIProxyManager, (void *));
    CHECK(InternalCStdIdentityGetIProxyManager_((void *)0x1000) == (void *)0x1000);
    CHECK(InternalCStdIdentityGetIProxyManager_(NULL) == NULL);
}
TEST(gap10_hook_object_entry_points_without_hook_dll) {
    NOT_WINE("internal export");
    FN(DWORD, GetHookInterface, (void **));
    FN(DWORD, HkOleRegisterObject, (const GUID *, const GUID *, void *, int));
    FN(DWORD, EnableHookObject, (int, int *));
    void *p = NULL;
    EXPECT_EQ(GetHookInterface_(&p), 0x80004002);                  /* E_NOINTERFACE: no OLE hook DLL */
    CHECK(p == NULL);
    EXPECT_EQ(HkOleRegisterObject_(&gNULL, &gNULL, NULL, 0), 0x80004002);
    EXPECT_EQ(EnableHookObject_(0, NULL), 0x8000ffff);             /* E_UNEXPECTED: pfn not set up */
}
TEST(gap10_dlldebugobjectrpchook_returns_true) {
    FN(int, DllDebugObjectRPCHook, (int, void *));
    EXPECT_EQ(DllDebugObjectRPCHook_(0, NULL), 1);                 /* stores the (disabled) hook, always TRUE */
}
TEST(gap10_dragdropsetfdt_null_window) {
    NOT_WINE("internal export");
    FNIN("ole32.dll", DWORD, DragDropSetFDT, (HWND));
    EXPECT_EQ(DragDropSetFDT_(NULL), S_OK);
}
