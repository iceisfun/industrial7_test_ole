/* IFillLockBytes / asynchronous docfile entry points.
 * On Windows 10 these exports exist, but coml2 gates them on a per-process flag
 * (PEB+0x2c8 & 0x200 in the decompilation) and returns STG_E_UNIMPLEMENTEDFUNCTION (0x800300FE)
 * before looking at any argument when the flag is clear. Every test therefore accepts
 * "unimplemented" as a legitimate outcome of the first call and only drives the object when it was created. */
#include "slm.h"

static ULARGE_INTEGER UL(ULONGLONG v) { ULARGE_INTEGER u; u.QuadPart = v; return u; }

static HRESULT build_img(CLB *c, ULONG n) {
    IStorage *stg = NULL; IStream *s = NULL;
    HRESULT hr = U(StgCreateDocfileOnILockBytes)(&c->iface, SM_CREATE_RW, 0, &stg);
    if (FAILED(hr)) return hr;
    hr = IStorage_CreateStream(stg, L"s", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &s);
    if (SUCCEEDED(hr)) {
        BYTE *b = malloc(n + 1); for (ULONG i = 0; i < n; i++) b[i] = (BYTE)(i * 5 + 1);
        ULONG w; hr = IStream_Write(s, b, n, &w); free(b); IStream_Release(s);
    }
    if (SUCCEEDED(hr)) hr = IStorage_Commit(stg, STGC_DEFAULT);
    IStorage_Release(stg);
    return hr;
}
static int gate_ok(HRESULT hr) { return hr == S_OK; }
#define GATED_OR_ERR(hr) ((hr) == H_UNIMPL || (hr) == H_INVALIDPOINTER)

TEST(slm_fill_exports_resolve) {
    CHECK(U(StgGetIFillLockBytesOnILockBytes) != NULL);
    CHECK(U(StgGetIFillLockBytesOnFile) != NULL);
    CHECK(U(StgOpenAsyncDocfileOnIFillLockBytes) != NULL);
    CHECK(U(StgIsStorageILockBytes) != NULL);
}
TEST(slm_fill_on_lockbytes_gate_or_object) {
    REQUIRE(U(StgGetIFillLockBytesOnILockBytes));
    if (t_is_wine()) { t_skip("wine aborts on its unimplemented async-docfile stubs"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IFillLockBytes *f = (IFillLockBytes *)(ULONG_PTR)1;
    HRESULT hr = U(StgGetIFillLockBytesOnILockBytes)(&c.iface, &f);
    CHECKF(hr == S_OK || hr == H_UNIMPL, "unexpected hr 0x%08lx", hr);
    if (gate_ok(hr)) {
        CHECK(f != NULL && f != (IFillLockBytes *)(ULONG_PTR)1);
        void *p = NULL;
        EXPECT_EQ(IFillLockBytes_QueryInterface(f, &G_IUnknown, &p), S_OK); if (p) IFillLockBytes_Release(f);
        p = NULL;
        EXPECT_EQ(IFillLockBytes_QueryInterface(f, &G_IFillLockBytes, &p), S_OK); if (p) IFillLockBytes_Release(f);
        p = NULL;
        EXPECT_EQ(IFillLockBytes_QueryInterface(f, &G_ILockBytes, &p), S_OK); if (p) ILockBytes_Release((ILockBytes *)p);
        p = (void *)1;
        EXPECT_EQ(IFillLockBytes_QueryInterface(f, &G_IStorage, &p), E_NOINTERFACE); CHECK(p == NULL);
        IFillLockBytes_Release(f);
    } else {
        t_note("StgGetIFillLockBytesOnILockBytes gated off in this process (0x%08lx)", hr);
        CHECK(f == (IFillLockBytes *)(ULONG_PTR)1 || f == NULL);
    }
    clb_free(&c);
    slm_couninit();
}
TEST(slm_fill_on_lockbytes_bad_args) {
    REQUIRE(U(StgGetIFillLockBytesOnILockBytes));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB c; clb_init(&c);
    IFillLockBytes *f = NULL;
    HRESULT hr = U(StgGetIFillLockBytesOnILockBytes)(NULL, &f);
    CHECKF(GATED_OR_ERR(hr), "NULL lockbytes: 0x%08lx", hr);
    hr = U(StgGetIFillLockBytesOnILockBytes)(&c.iface, NULL);
    CHECKF(GATED_OR_ERR(hr), "NULL out: 0x%08lx", hr);
    clb_free(&c);
    slm_couninit();
}
TEST(slm_fill_append_then_open_async) {
    REQUIRE(U(StgGetIFillLockBytesOnILockBytes)); REQUIRE(U(StgOpenAsyncDocfileOnIFillLockBytes));
    if (t_is_wine()) { t_skip("wine aborts on its unimplemented async-docfile stubs"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB img; clb_init(&img); img.log_ops = FALSE;
    EXPECT_EQ(build_img(&img, 9000), S_OK);
    CLB dst; clb_init(&dst); dst.log_ops = FALSE;
    IFillLockBytes *f = NULL;
    HRESULT hr = U(StgGetIFillLockBytesOnILockBytes)(&dst.iface, &f);
    if (hr == H_UNIMPL) { t_note("async docfile support is gated off"); goto out; }
    EXPECT_EQ(hr, S_OK);
    if (hr != S_OK || !f) goto out;
    EXPECT_EQ(IFillLockBytes_SetFillSize(f, UL(img.size)), S_OK);
    for (ULONG off = 0; off < img.size; off += 700) {
        ULONG n = img.size - off < 700 ? img.size - off : 700, w = 0;
        HRESULT h2 = IFillLockBytes_FillAppend(f, img.buf + off, n, &w);
        if (h2 != S_OK || w != n) { CHECKF(0, "FillAppend off=%lu hr=0x%08lx w=%lu", off, h2, w); break; }
    }
    CHECK(dst.size >= img.size);
    if (dst.size >= img.size) CHECK(!memcmp(dst.buf, img.buf, img.size));
    IStorage *stg = NULL;
    hr = U(StgOpenAsyncDocfileOnIFillLockBytes)(f, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stg);
    EXPECT_EQ(hr, S_OK);
    if (stg) {
        IStream *s = NULL; BYTE b[9000]; ULONG r = 0;
        hr = IStorage_OpenStream(stg, L"s", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &s);
        EXPECT_EQ(hr, S_OK);
        if (s) {
            hr = IStream_Read(s, b, 9000, &r);
            EXPECT_EQ(hr, S_OK); EXPECT_EQ(r, 9000);
            int bad = 0; for (ULONG i = 0; i < r; i++) bad += b[i] != (BYTE)(i * 5 + 1);
            EXPECT_EQ(bad, 0);
            IStream_Release(s);
        }
        IUnknown *cpc = NULL;
        HRESULT hq = IStorage_QueryInterface(stg, &G_IConnectionPointContainer, (void **)&cpc);
        t_note("async storage QI(IConnectionPointContainer)=0x%08lx", hq);
        if (cpc) IUnknown_Release(cpc);
        IStorage_Release(stg);
    }
    EXPECT_EQ(IFillLockBytes_Terminate(f, FALSE), S_OK);
    IFillLockBytes_Release(f);
out:
    clb_free(&img); clb_free(&dst);
    slm_couninit();
}
TEST(slm_fill_open_async_bad_args) {
    REQUIRE(U(StgOpenAsyncDocfileOnIFillLockBytes));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IStorage *stg = (IStorage *)(ULONG_PTR)1;
    HRESULT hr = U(StgOpenAsyncDocfileOnIFillLockBytes)(NULL, SM_OPEN_RO, 0, &stg);
    CHECKF(GATED_OR_ERR(hr), "NULL pflb: 0x%08lx", hr);
    hr = U(StgOpenAsyncDocfileOnIFillLockBytes)(NULL, SM_OPEN_RO, 0, NULL);
    CHECKF(GATED_OR_ERR(hr), "NULL both: 0x%08lx", hr);
    slm_couninit();
}
TEST(slm_fill_open_async_empty_data) {
    REQUIRE(U(StgGetIFillLockBytesOnILockBytes)); REQUIRE(U(StgOpenAsyncDocfileOnIFillLockBytes));
    if (t_is_wine()) { t_skip("wine aborts on its unimplemented async-docfile stubs"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    CLB dst; clb_init(&dst); dst.log_ops = FALSE;
    IFillLockBytes *f = NULL;
    HRESULT hr = U(StgGetIFillLockBytesOnILockBytes)(&dst.iface, &f);
    if (hr == S_OK && f) {
        IStorage *stg = (IStorage *)(ULONG_PTR)1;
        hr = U(StgOpenAsyncDocfileOnIFillLockBytes)(f, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stg);
        t_note("async open with no data filled: hr=0x%08lx", hr);   /* typically STG_E_PENDING or similar */
        if (SUCCEEDED(hr) && stg > (IStorage *)1) IStorage_Release(stg);
        else CHECK(stg == NULL || stg == (IStorage *)(ULONG_PTR)1);
        IFillLockBytes_Terminate(f, TRUE);
        IFillLockBytes_Release(f);
    } else CHECK(hr == H_UNIMPL);
    clb_free(&dst);
    slm_couninit();
}
TEST(slm_fill_on_file_gate_and_names) {
    REQUIRE(U(StgGetIFillLockBytesOnFile));
    if (t_is_wine()) { t_skip("wine aborts on its unimplemented async-docfile stubs"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    wchar_t path[MAX_PATH]; char leaf[MAX_PATH]; char full[MAX_PATH];
    snprintf(leaf, sizeof leaf, "fill_%lu.stg", GetCurrentProcessId());
    t_tmpfile(full, sizeof full, leaf);
    MultiByteToWideChar(CP_ACP, 0, full, -1, path, MAX_PATH);
    IFillLockBytes *f = (IFillLockBytes *)(ULONG_PTR)1;
    HRESULT hr = U(StgGetIFillLockBytesOnFile)(path, &f);
    CHECKF(hr == S_OK || hr == H_UNIMPL, "unexpected hr 0x%08lx", hr);
    if (hr == S_OK && f) {
        CLB img; clb_init(&img); img.log_ops = FALSE;
        if (build_img(&img, 2000) == S_OK) {
            ULONG w = 0;
            EXPECT_EQ(IFillLockBytes_FillAppend(f, img.buf, img.size, &w), S_OK);
            EXPECT_EQ(w, img.size);
        }
        IFillLockBytes_Terminate(f, FALSE);
        IFillLockBytes_Release(f);
        DWORD attr = GetFileAttributesA(full);
        t_note("file after fill: attrs=0x%lx", attr);
        if (attr != INVALID_FILE_ATTRIBUTES) {
            HRESULT hs = U(StgIsStorageFile)(path);
            t_note("StgIsStorageFile on filled file: 0x%08lx", hs);
        }
        clb_free(&img);
    } else if (hr == H_UNIMPL) {
        t_note("StgGetIFillLockBytesOnFile gated off");
    }
    DeleteFileA(full);
    slm_couninit();
}
TEST(slm_fill_on_file_bad_names) {
    REQUIRE(U(StgGetIFillLockBytesOnFile));
    if (t_is_wine()) { t_skip("wine dereferences NULL"); return; }
    if (!slm_coinit()) { t_skip("CoInitializeEx failed"); return; }
    IFillLockBytes *f = NULL;
    HRESULT hr = U(StgGetIFillLockBytesOnFile)(NULL, &f);
    CHECK(FAILED(hr));
    hr = U(StgGetIFillLockBytesOnFile)(L"", &f);
    CHECK(FAILED(hr));
    {
        char full[MAX_PATH]; wchar_t wp[MAX_PATH];
        t_tmpfile(full, sizeof full, "fill_nullout.stg");
        MultiByteToWideChar(CP_ACP, 0, full, -1, wp, MAX_PATH);
        hr = U(StgGetIFillLockBytesOnFile)(wp, NULL);
        CHECK(FAILED(hr));
        DeleteFileA(full);
    }
    slm_couninit();
}
