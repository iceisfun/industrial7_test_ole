/* dx_gap_06: CreateTypeLib -> SaveAllChanges writes a .tlb file (scratch dir), LoadTypeLibEx(REGKIND_NONE) reads it back.
 * No registry is touched; the file is removed in the tail of each test. */
#include "t.h"
#include <objbase.h>
#include <oleauto.h>
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); \
    t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)
#define E_NAMECONFLICT ((HRESULT)0x8002802CL)

static const GUID LIBID_G6 = {0x6a6a6a01,0x0001,0x4c3d,{0x9a,0,0,0,0,0,0,0xd1}};
static const GUID TID_G6   = {0x6a6a6a01,0x0002,0x4c3d,{0x9a,0,0,0,0,0,0,0xd1}};

static void tlb_path(WCHAR *out, const char *tag) { wsprintfW(out, L"%hs\\g6x_%hs_%lu.tlb", t_tmpdir(), tag, GetCurrentProcessId()); }

static HRESULT build_lib(SYSKIND sk, const WCHAR *path, int with_enum) {
    ICreateTypeLib *c = NULL; HRESULT hr = U(CreateTypeLib)(sk, path, &c);
    if (FAILED(hr) || !c) return hr;
    c->lpVtbl->SetName(c, L"G6Lib");
    c->lpVtbl->SetGuid(c, &LIBID_G6);
    c->lpVtbl->SetVersion(c, 3, 7);
    c->lpVtbl->SetLcid(c, 0x0409);
    c->lpVtbl->SetDocString(c, L"gap6 generated library");
    c->lpVtbl->SetLibFlags(c, LIBFLAG_FRESTRICTED);
    if (with_enum) {
        ICreateTypeInfo *ti = NULL;
        hr = c->lpVtbl->CreateTypeInfo(c, L"G6Enum", TKIND_ENUM, &ti);
        if (SUCCEEDED(hr) && ti) {
            ti->lpVtbl->SetGuid(ti, &TID_G6);
            VARDESC vd; VARIANT v; memset(&vd, 0, sizeof vd); memset(&v, 0, sizeof v);
            V_VT(&v) = VT_I4; V_I4(&v) = 41;
            vd.memid = 0; vd.varkind = VAR_CONST; vd.lpvarValue = &v; vd.elemdescVar.tdesc.vt = VT_I4;
            WCHAR nm[] = L"G6_FORTY_ONE";
            ti->lpVtbl->SetVarName(ti, 0, nm);       /* ignored if the index is not yet valid */
            HRESULT hv = ti->lpVtbl->AddVarDesc(ti, 0, &vd);
            if (SUCCEEDED(hv)) ti->lpVtbl->SetVarName(ti, 0, nm);
            ti->lpVtbl->Release(ti);
        }
    }
    hr = c->lpVtbl->SaveAllChanges(c);
    c->lpVtbl->Release(c);
    return hr;
}

TEST(dx_gap6_ctl_save_and_reload_attributes) {
    REQUIRE(U(CreateTypeLib)); REQUIRE(U(LoadTypeLibEx));
    static const SYSKIND kinds[] = { SYS_WIN32, SYS_WIN64 };
    for (unsigned i = 0; i < 2; i++) {
        WCHAR p[MAX_PATH]; tlb_path(p, i ? "w64" : "w32");
        DeleteFileW(p);
        HRESULT hr = build_lib(kinds[i], p, 1);
        EQ(hr, S_OK);
        CHECK(GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
        ITypeLib *tl = NULL;
        hr = U(LoadTypeLibEx)(p, REGKIND_NONE, &tl);
        EQ(hr, S_OK);
        if (tl) {
            TLIBATTR *a = NULL;
            EQ(tl->lpVtbl->GetLibAttr(tl, &a), S_OK);
            if (a) {
                CHECK(!memcmp(&a->guid, &LIBID_G6, sizeof(GUID)));
                CHECK(a->wMajorVerNum == 3 && a->wMinorVerNum == 7);
                CHECK(a->lcid == 0x0409);
                CHECK(a->syskind == kinds[i]);
                CHECK((a->wLibFlags & LIBFLAG_FRESTRICTED) != 0);
                tl->lpVtbl->ReleaseTLibAttr(tl, a);
            }
            BSTR nm = NULL, doc = NULL;
            EQ(tl->lpVtbl->GetDocumentation(tl, -1, &nm, &doc, NULL, NULL), S_OK);
            CHECK(nm && !lstrcmpW(nm, L"G6Lib"));
            CHECK(doc && !lstrcmpW(doc, L"gap6 generated library"));
            U(SysFreeString)(nm); U(SysFreeString)(doc);
            EQ(tl->lpVtbl->GetTypeInfoCount(tl), 1);
            ITypeInfo *ti = NULL;
            if (tl->lpVtbl->GetTypeInfo(tl, 0, &ti) == S_OK && ti) {
                TYPEATTR *ta = NULL;
                if (ti->lpVtbl->GetTypeAttr(ti, &ta) == S_OK && ta) {
                    CHECK(ta->typekind == TKIND_ENUM);
                    CHECK(!memcmp(&ta->guid, &TID_G6, sizeof(GUID)));
                    t_note("enum cVars=%u", ta->cVars);
                    ti->lpVtbl->ReleaseTypeAttr(ti, ta);
                }
                ti->lpVtbl->Release(ti);
            }
            tl->lpVtbl->Release(tl);
        }
        DeleteFileW(p);
    }
}
TEST(dx_gap6_ctl_duplicate_typeinfo_name) {
    REQUIRE(U(CreateTypeLib));
    WCHAR p[MAX_PATH]; tlb_path(p, "dup");
    ICreateTypeLib *c = NULL;
    EQ(U(CreateTypeLib)(SYS_WIN32, p, &c), S_OK); if (!c) return;
    ICreateTypeInfo *a = NULL, *b = NULL;
    EQ(c->lpVtbl->CreateTypeInfo(c, L"Dup", TKIND_ENUM, &a), S_OK);
    HRESULT hr = c->lpVtbl->CreateTypeInfo(c, L"Dup", TKIND_ENUM, &b);
    CHECK(FAILED(hr));
    t_note("duplicate typeinfo name: 0x%lx (docs: TYPE_E_NAMECONFLICT 0x8002802C; wine: TYPE_E_DUPLICATEID 0x8002802D)", hr);
    CHECK(b == NULL);
    hr = c->lpVtbl->CreateTypeInfo(c, L"dup", TKIND_ENUM, &b);        /* names compare case-insensitively */
    t_note("case-variant duplicate name: 0x%lx", hr);
    if (b) b->lpVtbl->Release(b);
    if (a) a->lpVtbl->Release(a);
    c->lpVtbl->Release(c);
    DeleteFileW(p);
}
TEST(dx_gap6_ctl_save_without_changes_writes_file) {
    REQUIRE(U(CreateTypeLib));
    WCHAR p[MAX_PATH]; tlb_path(p, "empty"); DeleteFileW(p);
    HRESULT hr = build_lib(SYS_WIN32, p, 0);
    t_note("empty library SaveAllChanges: 0x%lx, file exists=%d", hr, GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
    DeleteFileW(p);
}
TEST(dx_gap6_ctl_second_save_overwrites) {
    REQUIRE(U(CreateTypeLib)); REQUIRE(U(LoadTypeLibEx));
    WCHAR p[MAX_PATH]; tlb_path(p, "twice"); DeleteFileW(p);
    EQ(build_lib(SYS_WIN32, p, 1), S_OK);
    WIN32_FILE_ATTRIBUTE_DATA d1, d2; memset(&d1, 0, sizeof d1); memset(&d2, 0, sizeof d2);
    GetFileAttributesExW(p, GetFileExInfoStandard, &d1);
    EQ(build_lib(SYS_WIN32, p, 1), S_OK);
    GetFileAttributesExW(p, GetFileExInfoStandard, &d2);
    CHECK(d1.nFileSizeLow == d2.nFileSizeLow && d1.nFileSizeLow > 0);
    ITypeLib *tl = NULL;
    EQ(U(LoadTypeLibEx)(p, REGKIND_NONE, &tl), S_OK);
    if (tl) tl->lpVtbl->Release(tl);
    DeleteFileW(p);
}
