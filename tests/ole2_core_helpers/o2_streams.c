/* Storage/stream helpers adjacent to the OLE2 core: Set/GetConvertStg, Write/ReadFmtUserTypeStg,
 * OleSaveToStream, OleLoadFromStream. Expectations are taken from the Win10 ole32 decompilation. */
#include "olh.h"

#define O2_STGM (STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE)

/* ---- convert bit ---------------------------------------------------------------------------- */
TEST(o2str_convert_bit_roundtrip) {
    IStorage *stg;
    REQUIRE(U(SetConvertStg)); REQUIRE(U(GetConvertStg)); REQUIRE(U(StgCreateDocfile));
    stg = o2_mkstg(); REQUIRE(stg);
    EQ(U(SetConvertStg)(stg, TRUE), S_OK);
    EQ(U(GetConvertStg)(stg), S_OK);
    EQ(U(SetConvertStg)(stg, FALSE), S_OK);
    EQ(U(GetConvertStg)(stg), S_FALSE);
    EQ(U(SetConvertStg)(stg, 7), S_OK);       /* any non-zero value sets the bit */
    EQ(U(GetConvertStg)(stg), S_OK);
    EQ(U(SetConvertStg)(stg, 0), S_OK);
    EQ(U(GetConvertStg)(stg), S_FALSE);
    IStorage_Release(stg);
}
TEST(o2str_convert_bit_persists_through_reopen) {
    IStorage *stg, *st2; WCHAR path[MAX_PATH]; char np[MAX_PATH];
    REQUIRE(U(SetConvertStg)); REQUIRE(U(GetConvertStg)); REQUIRE(U(StgCreateDocfile)); REQUIRE(U(StgOpenStorage));
    t_tmpfile(np, sizeof np, "cvt.stg");
    MultiByteToWideChar(CP_ACP, 0, np, -1, path, MAX_PATH);
    DeleteFileW(path);
    EQ(U(StgCreateDocfile)(path, STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE, 0, &stg), S_OK);
    EQ(U(SetConvertStg)(stg, TRUE), S_OK);
    EQ(IStorage_Commit(stg, STGC_DEFAULT), S_OK);
    IStorage_Release(stg);
    EQ(U(StgOpenStorage)(path, NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, 0, &st2), S_OK);
    EQ(U(GetConvertStg)(st2), S_OK);
    IStorage_Release(st2);
    DeleteFileW(path);
}

/* ---- Write/ReadFmtUserTypeStg --------------------------------------------------------------- */
TEST(o2str_fmt_usertype_roundtrip_table) {
    static const struct { WORD cf; const WCHAR *name; } c[] = {
        { CF_TEXT, L"My Type" }, { CF_UNICODETEXT, L"Another User Type Name" }, { CF_BITMAP, L"B" }
    };
    int i;
    REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg)); REQUIRE(U(CoTaskMemFree)); REQUIRE(U(StgCreateDocfile));
    for (i = 0; i < O2_N(c); i++) {
        IStorage *stg = o2_mkstg(); WCHAR *out = NULL; CLIPFORMAT cf = 0xFFFF;
        REQUIRE(stg);
        EQ(U(WriteFmtUserTypeStg)(stg, c[i].cf, (LPOLESTR)c[i].name), S_OK);
        EQ(U(ReadFmtUserTypeStg)(stg, &cf, &out), S_OK);
        EQ(cf, c[i].cf);
        CHECK(out != NULL);
        if (out) { CHECK(lstrcmpW(out, c[i].name) == 0); U(CoTaskMemFree)(out); }
        IStorage_Release(stg);
    }
}
TEST(o2str_fmt_usertype_optional_outputs) {
    IStorage *stg; WCHAR *out = NULL; CLIPFORMAT cf = 0xFFFF;
    REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg)); REQUIRE(U(CoTaskMemFree)); REQUIRE(U(StgCreateDocfile));
    stg = o2_mkstg(); REQUIRE(stg);
    EQ(U(WriteFmtUserTypeStg)(stg, CF_TEXT, L"T"), S_OK);
    EQ(U(ReadFmtUserTypeStg)(stg, NULL, &out), S_OK);
    CHECK(out && lstrcmpW(out, L"T") == 0); if (out) U(CoTaskMemFree)(out);
    EQ(U(ReadFmtUserTypeStg)(stg, &cf, NULL), S_OK);
    EQ(cf, CF_TEXT);
    EQ(U(ReadFmtUserTypeStg)(stg, NULL, NULL), S_OK);
    IStorage_Release(stg);
}
TEST(o2str_fmt_usertype_null_name_keeps_previous) {
    IStorage *stg; WCHAR *out = NULL; CLIPFORMAT cf = 0;
    REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg)); REQUIRE(U(CoTaskMemFree)); REQUIRE(U(StgCreateDocfile));
    stg = o2_mkstg(); REQUIRE(stg);
    EQ(U(WriteFmtUserTypeStg)(stg, CF_TEXT, L"Keep Me"), S_OK);
    EQ(U(WriteFmtUserTypeStg)(stg, CF_UNICODETEXT, NULL), S_OK);   /* NULL user type leaves the stored name */
    EQ(U(ReadFmtUserTypeStg)(stg, &cf, &out), S_OK);
    EQ(cf, CF_UNICODETEXT);
    CHECK(out && lstrcmpW(out, L"Keep Me") == 0); if (out) U(CoTaskMemFree)(out);
    IStorage_Release(stg);
}
TEST(o2str_fmt_usertype_zero_format) {
    IStorage *stg; WCHAR *out = NULL; CLIPFORMAT cf = 0xFFFF;
    REQUIRE(U(WriteFmtUserTypeStg)); REQUIRE(U(ReadFmtUserTypeStg)); REQUIRE(U(CoTaskMemFree)); REQUIRE(U(StgCreateDocfile));
    stg = o2_mkstg(); REQUIRE(stg);
    EQ(U(WriteFmtUserTypeStg)(stg, 0, L"No Format"), S_OK);
    EQ(U(ReadFmtUserTypeStg)(stg, &cf, &out), S_OK);
    EQ(cf, 0);
    CHECK(out && lstrcmpW(out, L"No Format") == 0); if (out) U(CoTaskMemFree)(out);
    IStorage_Release(stg);
}
TEST(o2str_fmt_usertype_read_empty_storage) {
    IStorage *stg; WCHAR *out = (WCHAR *)1; CLIPFORMAT cf = 0x1234; HRESULT hr;
    REQUIRE(U(ReadFmtUserTypeStg)); REQUIRE(U(CoTaskMemFree)); REQUIRE(U(StgCreateDocfile));
    stg = o2_mkstg(); REQUIRE(stg);
    hr = U(ReadFmtUserTypeStg)(stg, &cf, &out);
    CHECK(FAILED(hr));               /* no \1CompObj stream */
    EQ(cf, 0);                       /* outputs are cleared on failure */
    EQ(out, NULL);
    IStorage_Release(stg);
}
TEST(o2str_fmt_usertype_write_null_storage) {
    REQUIRE(U(WriteFmtUserTypeStg));
    if (t_is_wine()) { t_skip("emulator dereferences NULL storage"); return; }
    EQ(U(WriteFmtUserTypeStg)(NULL, CF_TEXT, L"x"), O2_E_INVALIDARG);
}

/* ---- OleSaveToStream / OleLoadFromStream ---------------------------------------------------- */
typedef struct { IPersistStream i; LONG ref; CLSID clsid; HRESULT clsid_hr; int saves, clear, loads; } O2PS;
static HRESULT STDMETHODCALLTYPE ps_QI(IPersistStream *s, REFIID r, void **pp) {
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IPersist) || IsEqualIID(r, &IID_IPersistStream)) {
        *pp = s; InterlockedIncrement(&((O2PS *)s)->ref); return S_OK;
    }
    *pp = NULL; return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE ps_AddRef(IPersistStream *s) { return InterlockedIncrement(&((O2PS *)s)->ref); }
static ULONG STDMETHODCALLTYPE ps_Release(IPersistStream *s) { return InterlockedDecrement(&((O2PS *)s)->ref); }
static HRESULT STDMETHODCALLTYPE ps_GetClassID(IPersistStream *s, CLSID *c) {
    O2PS *p = (O2PS *)s; if (FAILED(p->clsid_hr)) return p->clsid_hr; *c = p->clsid; return S_OK;
}
static HRESULT STDMETHODCALLTYPE ps_IsDirty(IPersistStream *s) { return S_FALSE; }
static HRESULT STDMETHODCALLTYPE ps_Load(IPersistStream *s, IStream *st) { ((O2PS *)s)->loads++; return S_OK; }
static HRESULT STDMETHODCALLTYPE ps_Save(IPersistStream *s, IStream *st, BOOL clr) {
    O2PS *p = (O2PS *)s; ULONG w = 0; p->saves++; p->clear = clr;
    return IStream_Write(st, "PAYLOAD!", 8, &w);
}
static HRESULT STDMETHODCALLTYPE ps_Size(IPersistStream *s, ULARGE_INTEGER *sz) { sz->QuadPart = 24; return S_OK; }
static IPersistStreamVtbl ps_vtbl = { ps_QI, ps_AddRef, ps_Release, ps_GetClassID, ps_IsDirty, ps_Load, ps_Save, ps_Size };
static void ps_init(O2PS *p, HRESULT clsid_hr) { memset(p, 0, sizeof *p); p->i.lpVtbl = &ps_vtbl; p->ref = 1; p->clsid = O2_CLSID_A; p->clsid_hr = clsid_hr; }

static HGLOBAL stream_bytes(IStream *s, SIZE_T *n) {
    HGLOBAL h = NULL; STATSTG st;
    if (FAILED(U(GetHGlobalFromStream)(s, &h))) return NULL;
    memset(&st, 0, sizeof st);
    IStream_Stat(s, &st, STATFLAG_NONAME);
    *n = (SIZE_T)st.cbSize.QuadPart;
    return h;
}
TEST(o2str_save_to_stream_layout) {
    O2PS p; IStream *s = NULL; SIZE_T n = 0; HGLOBAL h; unsigned char *b;
    REQUIRE(U(OleSaveToStream)); REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(GetHGlobalFromStream));
    ps_init(&p, S_OK);
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    EQ(U(OleSaveToStream)(&p.i, s), S_OK);
    EQ(p.saves, 1);
    EQ(p.clear, TRUE);                          /* fClearDirty is always TRUE */
    h = stream_bytes(s, &n); CHECK(h != NULL);
    EQ(n, 16 + 8);                              /* CLSID, then the object's own data */
    b = h ? GlobalLock(h) : NULL;
    if (b) { CHECK(memcmp(b, &O2_CLSID_A, 16) == 0); CHECK(memcmp(b + 16, "PAYLOAD!", 8) == 0); GlobalUnlock(h); }
    IStream_Release(s);
}
TEST(o2str_save_to_stream_getclassid_failure) {
    O2PS p; IStream *s = NULL; SIZE_T n = 99;
    REQUIRE(U(OleSaveToStream)); REQUIRE(U(CreateStreamOnHGlobal)); REQUIRE(U(GetHGlobalFromStream));
    ps_init(&p, (HRESULT)0x80041234);
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    EQ(U(OleSaveToStream)(&p.i, s), (HRESULT)0x80041234);   /* propagated unchanged */
    EQ(p.saves, 0);
    stream_bytes(s, &n); EQ(n, 0);                          /* nothing written */
    IStream_Release(s);
}
TEST(o2str_load_from_stream_null_out) {
    IStream *s = NULL;
    REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    EQ(U(OleLoadFromStream)(s, &IID_IUnknown, NULL), O2_E_INVALIDARG);
    IStream_Release(s);
}
TEST(o2str_load_from_stream_empty_stream) {
    IStream *s = NULL; void *p = (void *)1; HRESULT hr;
    REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    O2_BEGIN;
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    hr = U(OleLoadFromStream)(s, &IID_IUnknown, &p);
    CHECK(FAILED(hr));              /* ReadClassStm cannot read a CLSID */
    EQ(p, NULL);                    /* output is cleared before anything else */
    IStream_Release(s);
    O2_END();
}
TEST(o2str_load_from_stream_unregistered_clsid) {
    IStream *s = NULL; void *p = (void *)1; ULONG w = 0; LARGE_INTEGER z; HRESULT hr;
    REQUIRE(U(OleLoadFromStream)); REQUIRE(U(CreateStreamOnHGlobal));
    O2_BEGIN;
    EQ(U(CreateStreamOnHGlobal)(NULL, TRUE, &s), S_OK);
    EQ(IStream_Write(s, &O2_CLSID_B, sizeof O2_CLSID_B, &w), S_OK);
    z.QuadPart = 0; IStream_Seek(s, z, STREAM_SEEK_SET, NULL);
    hr = U(OleLoadFromStream)(s, &IID_IUnknown, &p);
    EQ(hr, O2_REGDB_E_CLASSNOTREG);
    EQ(p, NULL);
    IStream_Release(s);
    O2_END();
}
