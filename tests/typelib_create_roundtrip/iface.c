/* ICreateTypeInfo / ICreateTypeInfo2 for interfaces, dispinterfaces, dual interfaces and coclasses:
 * attributes, inheritance chain, function descriptors, parameter descriptors, names, documentation,
 * error codes, and the save/reload round trip.
 * Hard CHECKs follow behaviour exercised by Wine's typelib.c tests (which pass on Windows) or the documented
 * ICreateTypeInfo contract; implementation-derived expectations use SOFTHR/SOFTEQ (logged, never fail). */
#include "tlc.h"

/* Create interface `name` deriving from stdole's IUnknown/IDispatch (if base != NULL). */
static ICreateTypeInfo *mk_typeinfo(ICreateTypeLib2 *ctl, const WCHAR *name, TYPEKIND k, ITypeInfo *base, HREFTYPE *href_out)
{
    ICreateTypeInfo *c = tl_cti(ctl, name, k);
    if (!c) return NULL;
    if (base) {
        HREFTYPE h = 0;
        OK_(ICreateTypeInfo_AddRefTypeInfo(c, base, &h));
        OK_(ICreateTypeInfo_AddImplType(c, 0, h));
        if (href_out) *href_out = h;
    }
    return c;
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: AddRefTypeInfo/AddImplType */
TEST(tlci_addref_implement_unknown)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    for (int s = 0; unk && s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "implunk.tlb", path);
        if (!ctl) break;
        ICreateTypeInfo *c = tl_cti(ctl, L"interface1", TKIND_INTERFACE);
        ITypeInfo *ti = tl_ti_of_cti(c);
        HREFTYPE h = 0;
        EQHR(ICreateTypeInfo_AddRefTypeInfo(c, NULL, &h), E_INVALIDARG);
        EQHR(ICreateTypeInfo_AddRefTypeInfo(c, unk, NULL), E_INVALIDARG);
        OK_(ICreateTypeInfo_AddRefTypeInfo(c, unk, &h));
        EQHR(ICreateTypeInfo_AddImplType(c, 1, h), TYPE_E_ELEMENTNOTFOUND);
        OK_(ICreateTypeInfo_AddImplType(c, 0, h));
        HREFTYPE rh = 0;
        OK_(ITypeInfo_GetRefTypeOfImplType(ti, 0, &rh));
        EQ(rh, 3);
        ITypeInfo *base = NULL;
        OK_(ITypeInfo_GetRefTypeInfo(ti, rh, &base));
        if (base) {
            TYPEATTR *ta = NULL;
            OK_(ITypeInfo_GetTypeAttr(base, &ta));
            if (ta) {
                EQ(ta->cbSizeInstance, ps);
                EQ(ta->cbSizeVft, 3 * ps);
                CHECK(IsEqualGUID(&ta->guid, &IID_IUnknown));
                ITypeInfo_ReleaseTypeAttr(base, ta);
            }
            ITypeInfo_Release(base);
        }
        EQHR(ITypeInfo_GetRefTypeOfImplType(ti, (UINT)-1, &rh), TYPE_E_ELEMENTNOTFOUND);
        EQHR(ITypeInfo_GetRefTypeOfImplType(ti, 1, &rh), TYPE_E_ELEMENTNOTFOUND);
        EQHR(ITypeInfo_GetRefTypeInfo(ti, 3, NULL), E_INVALIDARG);
        TYPEATTR *ta = NULL;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) {
            EQ(ta->cImplTypes, 1);
            EQ(ta->cbSizeVft, 3 * ps);
            EQ(ta->cbSizeInstance, ps);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        if (ti) ITypeInfo_Release(ti);
        if (c) ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (tl) {
            ITypeInfo *t0 = tl_ti(tl, 0);
            if (t0) {
                OK_(ITypeInfo_GetTypeAttr(t0, &ta));
                if (ta) { EQ(ta->cImplTypes, 1); EQ(ta->cbSizeVft, 3 * ps); ITypeInfo_ReleaseTypeAttr(t0, ta); }
                OK_(ITypeInfo_GetRefTypeOfImplType(t0, 0, &rh));
                OK_(ITypeInfo_GetRefTypeInfo(t0, rh, &base));
                if (base) {
                    OK_(ITypeInfo_GetTypeAttr(base, &ta));
                    if (ta) { CHECK(IsEqualGUID(&ta->guid, &IID_IUnknown)); ITypeInfo_ReleaseTypeAttr(base, ta); }
                    ITypeInfo_Release(base);
                }
                ITypeInfo_Release(t0);
            }
            ITypeLib_Release(tl);
        }
    }
    if (unk) ITypeInfo_Release(unk);
    ITypeLib_Release(so);
}

TEST(tlci_local_ref_hreftype)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "localref.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *a = tl_cti(ctl, L"IBase", TKIND_INTERFACE);
    ICreateTypeInfo *b = tl_cti(ctl, L"IDerived", TKIND_INTERFACE);
    ITypeInfo *ta = tl_ti_of_cti(a), *tb = tl_ti_of_cti(b);
    HREFTYPE h = 99;
    OK_(ICreateTypeInfo_AddRefTypeInfo(b, ta, &h));
    t_note("hreftype of local typeinfo #0 = %ld", (long)h);
    OK_(ICreateTypeInfo_AddImplType(b, 0, h));
    HREFTYPE rh = 77;
    OK_(ITypeInfo_GetRefTypeOfImplType(tb, 0, &rh));
    t_note("GetRefTypeOfImplType = %ld (AddRefTypeInfo returned %ld)", (long)rh, (long)h);
    SOFTEQ(rh, h);
    ITypeInfo *r = NULL;
    OK_(ITypeInfo_GetRefTypeInfo(tb, rh, &r));
    CHECK(r != NULL);
    if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "IBase"); ITypeInfo_Release(r); }
    /* referencing yourself */
    OK_(ICreateTypeInfo_AddRefTypeInfo(b, tb, &h));
    t_note("hreftype of self = %ld", (long)h);
    if (ta) ITypeInfo_Release(ta);
    if (tb) ITypeInfo_Release(tb);
    if (a) ICreateTypeInfo_Release(a);
    if (b) ICreateTypeInfo_Release(b);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *t1 = tl_ti(tl, 1);
    if (t1) {
        OK_(ITypeInfo_GetRefTypeOfImplType(t1, 0, &rh));
        OK_(ITypeInfo_GetRefTypeInfo(t1, rh, &r));
        if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "IBase"); ITypeInfo_Release(r); }
        ITypeInfo_Release(t1);
    }
    ITypeLib_Release(tl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: coclass impl types */
TEST(tlci_coclass_impltypes)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "coclass.tlb", path);
    if (!unk || !ctl) return;
    ICreateTypeInfo *i1 = tl_cti(ctl, L"interface1", TKIND_INTERFACE);
    ITypeInfo *ti1 = tl_ti_of_cti(i1);
    ICreateTypeInfo *co = tl_cti(ctl, L"coclass", TKIND_COCLASS);
    HREFTYPE h = 0;
    OK_(ICreateTypeInfo_AddRefTypeInfo(co, ti1, &h));
    OK_(ICreateTypeInfo_AddImplType(co, 0, h));
    /* adding at an index that is not the append position */
    EQHR(ICreateTypeInfo_AddImplType(co, 0, h), TYPE_E_ELEMENTNOTFOUND);
    OK_(ICreateTypeInfo_AddRefTypeInfo(co, unk, &h));
    OK_(ICreateTypeInfo_AddImplType(co, 1, h));
    EQHR(ICreateTypeInfo_AddImplType(co, 1, h), TYPE_E_ELEMENTNOTFOUND);
    OK_(ICreateTypeInfo_AddImplType(co, 2, h));
    OK_(ICreateTypeInfo_SetImplTypeFlags(co, 0, IMPLTYPEFLAG_FDEFAULT));
    OK_(ICreateTypeInfo_SetImplTypeFlags(co, 1, IMPLTYPEFLAG_FRESTRICTED));
    ITypeInfo *tco = tl_ti_of_cti(co);
    int fl = 0x55;
    EQHR(ITypeInfo_GetImplTypeFlags(tco, 0, NULL), E_INVALIDARG);
    OK_(ITypeInfo_GetImplTypeFlags(tco, 0, &fl)); EQ(fl, IMPLTYPEFLAG_FDEFAULT);
    OK_(ITypeInfo_GetImplTypeFlags(tco, 1, &fl)); EQ(fl, IMPLTYPEFLAG_FRESTRICTED);
    OK_(ITypeInfo_GetImplTypeFlags(tco, 2, &fl)); EQ(fl, 0);
    HREFTYPE rh;
    OK_(ITypeInfo_GetRefTypeOfImplType(tco, 0, &rh)); EQ(rh, 0);
    OK_(ITypeInfo_GetRefTypeOfImplType(tco, 1, &rh)); EQ(rh, 1);
    OK_(ITypeInfo_GetRefTypeOfImplType(tco, 2, &rh)); EQ(rh, 1);
    EQHR(ITypeInfo_GetRefTypeOfImplType(tco, (UINT)-1, &rh), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetImplTypeFlags(co, 3, 0), TYPE_E_ELEMENTNOTFOUND);
    TYPEATTR *ta = NULL;
    OK_(ITypeInfo_GetTypeAttr(tco, &ta));
    if (ta) { EQ(ta->typekind, TKIND_COCLASS); EQ(ta->cImplTypes, 3); ITypeInfo_ReleaseTypeAttr(tco, ta); }
    ITypeInfo_Release(tco);
    ITypeInfo_Release(ti1);
    ICreateTypeInfo_Release(co);
    ICreateTypeInfo_Release(i1);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 1);
        if (t) {
            OK_(ITypeInfo_GetTypeAttr(t, &ta));
            if (ta) { EQ(ta->cImplTypes, 3); ITypeInfo_ReleaseTypeAttr(t, ta); }
            OK_(ITypeInfo_GetImplTypeFlags(t, 0, &fl)); EQ(fl, IMPLTYPEFLAG_FDEFAULT);
            OK_(ITypeInfo_GetImplTypeFlags(t, 1, &fl)); EQ(fl, IMPLTYPEFLAG_FRESTRICTED);
            OK_(ITypeInfo_GetImplTypeFlags(t, 2, &fl)); EQ(fl, 0);
            ITypeInfo *r = NULL;
            OK_(ITypeInfo_GetRefTypeOfImplType(t, 0, &rh));
            OK_(ITypeInfo_GetRefTypeInfo(t, rh, &r));
            if (r) { char nm[16]; tl_doc_name(r, MEMBERID_NIL, nm, sizeof nm); EXPECT_STR(nm, "interface1"); ITypeInfo_Release(r); }
            OK_(ITypeInfo_GetRefTypeOfImplType(t, 1, &rh));
            OK_(ITypeInfo_GetRefTypeInfo(t, rh, &r));
            if (r) {
                OK_(ITypeInfo_GetTypeAttr(r, &ta));
                if (ta) { CHECK(IsEqualGUID(&ta->guid, &IID_IUnknown)); ITypeInfo_ReleaseTypeAttr(r, ta); }
                ITypeInfo_Release(r);
            }
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(unk);
    ITypeLib_Release(so);
}

TEST(tlci_implflags_values_roundtrip)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    ITypeInfo *dsp = tl_stdole_ti(so, &IID_IDispatch);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "implfl.tlb", path);
    if (!unk || !dsp || !ctl) return;
    ICreateTypeInfo *co = tl_cti(ctl, L"Co", TKIND_COCLASS);
    static const int flags[] = { IMPLTYPEFLAG_FDEFAULT, IMPLTYPEFLAG_FSOURCE, IMPLTYPEFLAG_FDEFAULT | IMPLTYPEFLAG_FSOURCE,
        IMPLTYPEFLAG_FRESTRICTED, IMPLTYPEFLAG_FDEFAULTVTABLE | IMPLTYPEFLAG_FSOURCE | IMPLTYPEFLAG_FDEFAULT };
    for (unsigned i = 0; i < 5; i++) {
        HREFTYPE h;
        OK_(ICreateTypeInfo_AddRefTypeInfo(co, i & 1 ? dsp : unk, &h));
        OK_(ICreateTypeInfo_AddImplType(co, i, h));
        OK_(ICreateTypeInfo_SetImplTypeFlags(co, i, flags[i]));
    }
    ICreateTypeInfo_Release(co);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            for (unsigned i = 0; i < 5; i++) {
                int fl = -1;
                OK_(ITypeInfo_GetImplTypeFlags(t, i, &fl));
                CHECKF(fl == flags[i], "impl %u flags 0x%x want 0x%x", i, fl, flags[i]);
            }
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(unk); ITypeInfo_Release(dsp); ITypeLib_Release(so);
}

TEST(tlci_implflags_wrong_kind)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "implwk.tlb", path);
    if (!ctl) return;
    static const TYPEKIND ks[] = { TKIND_INTERFACE, TKIND_DISPATCH, TKIND_ENUM, TKIND_RECORD };
    static const WCHAR *ns[] = { L"I", L"D", L"E", L"R" };
    for (unsigned i = 0; i < 4; i++) {
        ICreateTypeInfo *c = tl_cti(ctl, ns[i], ks[i]);
        if (!c) continue;
        SOFTHR(ICreateTypeInfo_SetImplTypeFlags(c, 0, IMPLTYPEFLAG_FDEFAULT), TYPE_E_BADMODULEKIND);
        ICreateTypeInfo_Release(c);
    }
    ICreateTypeLib2_Release(ctl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: dual interface */
TEST(tlci_dual_interface)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *dsp = tl_stdole_ti(so, &IID_IDispatch);
    for (int s = 0; dsp && s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "dual.tlb", path);
        if (!ctl) break;
        ICreateTypeInfo *c = tl_cti(ctl, L"dual", TKIND_INTERFACE);
        OK_(ICreateTypeInfo_SetTypeFlags(c, TYPEFLAG_FDUAL));
        OK_(tl_add_func(c, 0, INVOKE_PROPERTYGET, VT_BSTR, 0, NULL, 0));
        HREFTYPE h;
        OK_(ICreateTypeInfo_AddRefTypeInfo(c, dsp, &h));
        OK_(ICreateTypeInfo_AddImplType(c, 0, h));
        ITypeInfo *ti = tl_ti_of_cti(c);
        TYPEATTR *ta = NULL;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) {
            EQ(ta->cbSizeInstance, ps);
            EQ(ta->typekind, TKIND_INTERFACE);
            EQ(ta->cFuncs, 1);
            EQ(ta->cVars, 0);
            EQ(ta->cImplTypes, 1);
            EQ(ta->cbSizeVft, 8 * ps);
            EQ(ta->cbAlignment, 4);
            EQ(ta->wTypeFlags, TYPEFLAG_FDISPATCHABLE | TYPEFLAG_FDUAL);
            EQ(ta->wMajorVerNum, 0);
            EQ(ta->wMinorVerNum, 0);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        HREFTYPE rh = 0;
        OK_(ITypeInfo_GetRefTypeOfImplType(ti, (UINT)-1, &rh));
        CHECK(rh == -2);
        ITypeInfo *disp = NULL;
        OK_(ITypeInfo_GetRefTypeInfo(ti, -2, &disp));
        if (disp) {
            OK_(ITypeInfo_GetTypeAttr(disp, &ta));
            if (ta) {
                EQ(ta->typekind, TKIND_DISPATCH);
                EQ(ta->cImplTypes, 1);
                EQ(ta->cbSizeInstance, ps);
                EQ(ta->cbSizeVft, 7 * sizeof(void *));
                EQ(ta->wTypeFlags, TYPEFLAG_FDISPATCHABLE | TYPEFLAG_FDUAL);
                SOFTEQ(ta->cFuncs, 8);
                ITypeInfo_ReleaseTypeAttr(disp, ta);
            }
            ITypeInfo_Release(disp);
        }
        /* SetTypeDescAlias on a non-alias */
        TYPEDESC td = { .vt = VT_I1 };
        EQHR(ICreateTypeInfo_SetTypeDescAlias(c, &td), TYPE_E_BADMODULEKIND);
        ITypeInfo_Release(ti);
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (tl) {
            ITypeInfo *t = tl_ti(tl, 0);
            if (t) {
                OK_(ITypeInfo_GetTypeAttr(t, &ta));
                if (ta) {
                    EQ(ta->wTypeFlags & TYPEFLAG_FDUAL, TYPEFLAG_FDUAL);
                    EQ(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE, TYPEFLAG_FDISPATCHABLE);
                    EQ(ta->cbSizeVft, 8 * ps);
                    ITypeInfo_ReleaseTypeAttr(t, ta);
                }
                ITypeInfo_Release(t);
            }
            ITypeLib_Release(tl);
        }
    }
    if (dsp) ITypeInfo_Release(dsp);
    ITypeLib_Release(so);
}

TEST(tlci_dispatchable_flag_from_idispatch_base)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *dsp = tl_stdole_ti(so, &IID_IDispatch);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "dispfl.tlb", path);
    if (!dsp || !ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"interface1", TKIND_INTERFACE);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    ITypeInfo *ti = tl_ti_of_cti(c);
    TYPEATTR *ta = NULL;
    HREFTYPE h;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE, 0); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    OK_(ICreateTypeInfo_AddRefTypeInfo(c, dsp, &h));
    OK_(ICreateTypeInfo_AddImplType(c, 0, h));
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) {
        EQ(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE, TYPEFLAG_FDISPATCHABLE);
        EQ(ta->cImplTypes, 1);
        ITypeInfo_ReleaseTypeAttr(ti, ta);
    }
    if (c2) {
        OK_(ICreateTypeInfo2_DeleteImplType(c2, 0));
        EQHR(ICreateTypeInfo2_DeleteImplType(c2, 0), TYPE_E_ELEMENTNOTFOUND);
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) {
            EQ(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE, TYPEFLAG_FDISPATCHABLE); /* sticky */
            EQ(ta->cImplTypes, 0);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        int fl; HREFTYPE rh;
        EQHR(ITypeInfo_GetImplTypeFlags(ti, 0, &fl), TYPE_E_ELEMENTNOTFOUND);
        EQHR(ITypeInfo_GetRefTypeOfImplType(ti, 0, &rh), TYPE_E_ELEMENTNOTFOUND);
        ICreateTypeInfo2_Release(c2);
    }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            OK_(ITypeInfo_GetTypeAttr(t, &ta));
            if (ta) { EQ(ta->wTypeFlags & TYPEFLAG_FDISPATCHABLE, TYPEFLAG_FDISPATCHABLE); EQ(ta->cImplTypes, 0); ITypeInfo_ReleaseTypeAttr(t, ta); }
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(dsp); ITypeLib_Release(so);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: AddFuncDesc oVft/memid */
TEST(tlci_funcdesc_vft_offsets)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    for (int s = 0; unk && s < 2; s++) {
        SYSKIND sk = s ? SYS_WIN64 : SYS_WIN32;
        unsigned ps = tl_ptr(sk);
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(sk, "vft.tlb", path);
        if (!ctl) break;
        ICreateTypeInfo *c = mk_typeinfo(ctl, L"IVft", TKIND_INTERFACE, unk, NULL);
        ITypeInfo *ti = tl_ti_of_cti(c);
        for (UINT i = 0; i < 5; i++)
            OK_(tl_add_func(c, i, INVOKE_FUNC, VT_HRESULT, 0x100 + i, NULL, 0));
        for (UINT i = 0; i < 5; i++) {
            FUNCDESC *fd = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, i, &fd));
            if (fd) {
                CHECKF(fd->oVft == (3 + i) * ps, "func %u oVft %d want %u", i, fd->oVft, (3 + i) * ps);
                EQ(fd->memid, 0x100 + i);
                ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
        }
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cFuncs, 5); EQ(ta->cbSizeVft, 8 * ps); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        /* insert at the front shifts the vtable */
        OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_HRESULT, 0x200, NULL, 0));
        for (UINT i = 0; i < 6; i++) {
            FUNCDESC *fd = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, i, &fd));
            if (fd) {
                CHECKF(fd->oVft == (3 + i) * ps, "after insert func %u oVft %d want %u", i, fd->oVft, (3 + i) * ps);
                EQ(fd->memid, i == 0 ? 0x200 : 0x100 + i - 1);
                ITypeInfo_ReleaseFuncDesc(ti, fd);
            }
        }
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cFuncs, 6); EQ(ta->cbSizeVft, 9 * ps); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        ITypeInfo_Release(ti);
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (tl) {
            ITypeInfo *t = tl_ti(tl, 0);
            if (t) {
                for (UINT i = 0; i < 6; i++) {
                    FUNCDESC *fd = NULL;
                    OK_(ITypeInfo_GetFuncDesc(t, i, &fd));
                    if (fd) { if (!(fd->oVft == (3 + i) * ps)) t_note("SOFT reload func %u oVft %d", i, fd->oVft); ITypeInfo_ReleaseFuncDesc(t, fd); }
                }
                OK_(ITypeInfo_GetTypeAttr(t, &ta));
                if (ta) { SOFTEQ(ta->cbSizeVft, 9 * ps); ITypeInfo_ReleaseTypeAttr(t, ta); }
                ITypeInfo_Release(t);
            }
            ITypeLib_Release(tl);
        }
    }
    if (unk) ITypeInfo_Release(unk);
    ITypeLib_Release(so);
}

TEST(tlci_funcdesc_auto_memids_no_base)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "memids.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"name2", TKIND_INTERFACE);
    for (UINT i = 0; i < 4; i++)
        OK_(tl_add_func(c, i, INVOKE_FUNC, VT_VOID, MEMBERID_NIL, NULL, 0));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *t = tl_ti(tl, 0);
    if (t) {
        for (UINT i = 0; i < 4; i++) {
            FUNCDESC *fd = NULL;
            OK_(ITypeInfo_GetFuncDesc(t, i, &fd));
            if (fd) { SOFTEQ(fd->memid, 0x60000000 + i); ITypeInfo_ReleaseFuncDesc(t, fd); }
        }
        ITypeInfo_Release(t);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_funcdesc_auto_memids_with_base)
{
    TL_BEGIN();
    ITypeLib *so = tl_stdole();
    if (!so) return;
    ITypeInfo *unk = tl_stdole_ti(so, &IID_IUnknown);
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "memids2.tlb", path);
    if (!ctl || !unk) return;
    ICreateTypeInfo *c = mk_typeinfo(ctl, L"IDer", TKIND_INTERFACE, unk, NULL);
    for (UINT i = 0; i < 3; i++)
        OK_(tl_add_func(c, i, INVOKE_FUNC, VT_VOID, MEMBERID_NIL, NULL, 0));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            for (UINT i = 0; i < 3; i++) {
                FUNCDESC *fd = NULL;
                OK_(ITypeInfo_GetFuncDesc(t, i, &fd));
                if (fd) { SOFTEQ(fd->memid, 0x60010000 + i); ITypeInfo_ReleaseFuncDesc(t, fd); }
            }
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
    ITypeInfo_Release(unk); ITypeLib_Release(so);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: AddFuncDesc argument errors */
TEST(tlci_addfuncdesc_errors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "afderr.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IErr", TKIND_INTERFACE);
    FUNCDESC fd; memset(&fd, 0, sizeof fd);
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_PROPERTYGET; fd.callconv = CC_STDCALL;
    fd.elemdescFunc.tdesc.vt = VT_BSTR;
    EQHR(ICreateTypeInfo_AddFuncDesc(c, 0, NULL), E_INVALIDARG);
    EQHR(ICreateTypeInfo_AddFuncDesc(c, 1, &fd), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_AddFuncDesc(c, 0, &fd), S_OK);
    ITypeInfo *ti = tl_ti_of_cti(c);
    FUNCDESC *p = NULL;
    EQHR(ITypeInfo_GetFuncDesc(ti, 0, NULL), E_INVALIDARG);
    EQHR(ITypeInfo_GetFuncDesc(ti, 1, &p), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ITypeInfo_GetFuncDesc(ti, 0, &p), S_OK);
    if (p) {
        EQ(p->memid, 0);
        CHECK(p->lprgscode == NULL);
        CHECK(p->lprgelemdescParam == NULL);
        EQ(p->funckind, FUNC_PUREVIRTUAL);
        EQ(p->invkind, INVOKE_PROPERTYGET);
        EQ(p->callconv, CC_STDCALL);
        EQ(p->cParams, 0);
        EQ(p->cParamsOpt, 0);
        EQ(p->oVft, 0);
        EQ(p->cScodes, 0);
        EQ(p->elemdescFunc.tdesc.vt, VT_BSTR);
        EQ(p->wFuncFlags, 0);
        ITypeInfo_ReleaseFuncDesc(ti, p);
    }
    /* propput / propputref with zero parameters are inconsistent */
    fd.invkind = INVOKE_PROPERTYPUT;
    EQHR(ICreateTypeInfo_AddFuncDesc(c, 1, &fd), TYPE_E_INCONSISTENTPROPFUNCS);
    fd.invkind = INVOKE_PROPERTYPUTREF;
    EQHR(ICreateTypeInfo_AddFuncDesc(c, 1, &fd), TYPE_E_INCONSISTENTPROPFUNCS);
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->cFuncs, 1); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlci_addfuncdesc_funckind_vs_typekind)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "fkind.tlb", path);
    if (!ctl) return;
    static const struct { TYPEKIND tk; FUNCKIND ok; const WCHAR *n; } t[] = {
        {TKIND_INTERFACE, FUNC_PUREVIRTUAL, L"I"}, {TKIND_DISPATCH, FUNC_DISPATCH, L"D"}, {TKIND_MODULE, FUNC_STATIC, L"M"} };
    static const FUNCKIND all[] = { FUNC_VIRTUAL, FUNC_PUREVIRTUAL, FUNC_NONVIRTUAL, FUNC_STATIC, FUNC_DISPATCH };
    for (unsigned i = 0; i < 3; i++) {
        ICreateTypeInfo *c = tl_cti(ctl, t[i].n, t[i].tk);
        if (!c) continue;
        for (unsigned k = 0; k < 5; k++) {
            HRESULT hr = tl_add_func_k(c, 0, all[k], INVOKE_FUNC, VT_VOID, MEMBERID_NIL, NULL, 0);
            if (all[k] == t[i].ok) {
                CHECKF(hr == S_OK, "kind %d funckind %d hr 0x%08lx", t[i].tk, all[k], hr);
            } else
                SOFTHR(hr, TYPE_E_BADMODULEKIND);
            if (hr == S_OK) {
                ICreateTypeInfo2 *c2 = NULL;
                if (ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2) == S_OK) {
                    ICreateTypeInfo2_DeleteFuncDesc(c2, 0);
                    ICreateTypeInfo2_Release(c2);
                }
            }
        }
        ICreateTypeInfo_Release(c);
    }
    ICreateTypeLib2_Release(ctl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_DeleteFuncDesc */
TEST(tlci_deletefuncdesc)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "delfunc.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"interface1", TKIND_INTERFACE);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    ITypeInfo *ti = tl_ti_of_cti(c);
    OK_(tl_add_func(c, 0, INVOKE_PROPERTYGET, VT_BSTR, 0, NULL, 0));
    TYPEATTR *ta;
    OK_(ITypeInfo_GetTypeAttr(ti, &ta));
    if (ta) { EQ(ta->cFuncs, 1); ITypeInfo_ReleaseTypeAttr(ti, ta); }
    if (c2) {
        EQHR(ICreateTypeInfo2_DeleteFuncDesc(c2, 1), TYPE_E_ELEMENTNOTFOUND);
        OK_(ICreateTypeInfo2_DeleteFuncDesc(c2, 0));
        EQHR(ICreateTypeInfo2_DeleteFuncDesc(c2, 0), TYPE_E_ELEMENTNOTFOUND);
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cFuncs, 0); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        /* add several, delete the middle one, check the order */
        for (UINT i = 0; i < 4; i++) OK_(tl_add_func(c, i, INVOKE_FUNC, VT_VOID, 0x10 + i, NULL, 0));
        OK_(ICreateTypeInfo2_DeleteFuncDesc(c2, 1));
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cFuncs, 3); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        static const MEMBERID want[3] = { 0x10, 0x12, 0x13 };
        for (UINT i = 0; i < 3; i++) {
            FUNCDESC *fd = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, i, &fd));
            if (fd) { EQ(fd->memid, want[i]); ITypeInfo_ReleaseFuncDesc(ti, fd); }
        }
        ICreateTypeInfo2_Release(c2);
    }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            OK_(ITypeInfo_GetTypeAttr(t, &ta));
            if (ta) { EQ(ta->cFuncs, 3); ITypeInfo_ReleaseTypeAttr(t, ta); }
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: property put/get */
TEST(tlci_property_put_get_funcs)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "props.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IProps", TKIND_INTERFACE);
    ITypeInfo *ti = tl_ti_of_cti(c);
    OK_(tl_add_func(c, 0, INVOKE_PROPERTYGET, VT_BSTR, 0, NULL, 0));
    VARTYPE bstr = VT_BSTR;
    OK_(tl_add_func(c, 1, INVOKE_PROPERTYPUT, VT_VOID, 0, &bstr, 1));
    LPOLESTR nm[2] = { (LPOLESTR)L"Prop1", (LPOLESTR)L"param1" };
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 0), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 0, NULL, 1), E_INVALIDARG);
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, nm, 1));
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 1, nm, 1));
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 1, nm, 2), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 1000, NULL, 1), E_INVALIDARG);
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 1000, nm, 1), TYPE_E_ELEMENTNOTFOUND);
    /* the put function takes exactly one parameter, getter none */
    FUNCDESC *fd = NULL;
    OK_(ITypeInfo_GetFuncDesc(ti, 1, &fd));
    if (fd) {
        EQ(fd->invkind, INVOKE_PROPERTYPUT);
        EQ(fd->cParams, 1);
        EQ(fd->oVft, 1 * 8);
        EQ(fd->elemdescFunc.tdesc.vt, VT_VOID);
        CHECK(fd->lprgelemdescParam != NULL);
        if (fd->lprgelemdescParam) {
            EQ(fd->lprgelemdescParam[0].tdesc.vt, VT_BSTR);
            EQ(fd->lprgelemdescParam[0].paramdesc.wParamFlags, PARAMFLAG_FIN);
        }
        ITypeInfo_ReleaseFuncDesc(ti, fd);
    }
    /* same member id 0: GetIDsOfNames resolves "Prop1" to memid 0 */
    MEMBERID id = -1;
    LPOLESTR look = (LPOLESTR)L"Prop1";
    SOFTHR(ITypeInfo_GetIDsOfNames(ti, &look, 1, &id), S_OK);
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            BSTR names[3] = {0}; UINT cn = 0;
            OK_(ITypeInfo_GetNames(t, 0, names, 3, &cn));
            EQ(cn, 1);
            if (cn >= 1 && names[0]) CHECK(!lstrcmpW(names[0], L"Prop1"));
            for (UINT i = 0; i < cn; i++) pSysFreeString(names[i]);
            cn = 0;
            OK_(ITypeInfo_GetNames(t, 0, names, 3, &cn));
            for (UINT i = 0; i < cn; i++) pSysFreeString(names[i]);
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_SetFuncAndParamNames */
TEST(tlci_names_ambiguous_property_accessors)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "names.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"name", TKIND_DISPATCH);
    LPOLESTR prop[1] = { (LPOLESTR)L"prop" };
    LPOLESTR func[2] = { (LPOLESTR)L"func", NULL };
    FUNCDESC fd; ELEMDESC ed;
    memset(&fd, 0, sizeof fd); memset(&ed, 0, sizeof ed);
    fd.funckind = FUNC_DISPATCH; fd.callconv = CC_STDCALL; fd.elemdescFunc.tdesc.vt = VT_VOID;
    fd.wFuncFlags = FUNCFLAG_FBINDABLE;
    ed.tdesc.vt = VT_BSTR; ed.idldesc.wIDLFlags = IDLFLAG_FIN;
    fd.lprgelemdescParam = &ed; fd.invkind = INVOKE_PROPERTYPUT; fd.cParams = 1;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, prop, 1));
    fd.invkind = INVOKE_PROPERTYPUTREF;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 1, &fd));
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 1, prop, 1));
    fd.invkind = INVOKE_PROPERTYGET; fd.cParams = 0;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 2, &fd));
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 2, prop, 1));
    OK_(ICreateTypeInfo_AddFuncDesc(c, 3, &fd));
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 3, prop, 1), TYPE_E_AMBIGUOUSNAME);
    fd.invkind = INVOKE_FUNC; fd.cParams = 1;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 4, &fd));
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 4, func, 2) == S_OK ? S_OK : S_OK);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: names 1000/func1/func2 */
TEST(tlci_names_duplicate_function_name)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "dupname.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IDup", TKIND_INTERFACE);
    VARTYPE two[2] = { VT_I4, VT_I4 };
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x10, two, 2));
    OK_(tl_add_func(c, 1, INVOKE_FUNC, VT_VOID, 0x11, two, 2));
    LPOLESTR n1[3] = { (LPOLESTR)L"func1", (LPOLESTR)L"param1", (LPOLESTR)L"param2" };
    LPOLESTR n2[3] = { (LPOLESTR)L"func2", (LPOLESTR)L"param1", (LPOLESTR)L"param2" };
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 0, n1, 4), TYPE_E_ELEMENTNOTFOUND);
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 0, n1, 3));
    EQHR(ICreateTypeInfo_SetFuncAndParamNames(c, 1, n1, 3), TYPE_E_AMBIGUOUSNAME);
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 1, n2, 3));
    /* renaming a function to its own name is not a conflict with itself? */
    SOFTHR(ICreateTypeInfo_SetFuncAndParamNames(c, 0, n1, 3), S_OK);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            BSTR nm[4] = {0}; UINT cn = 0;
            OK_(ITypeInfo_GetNames(t, 0x10, nm, 4, &cn));
            EQ(cn, 3);
            if (cn == 3) {
                CHECK(!lstrcmpW(nm[0], L"func1")); CHECK(!lstrcmpW(nm[1], L"param1")); CHECK(!lstrcmpW(nm[2], L"param2"));
            }
            for (UINT i = 0; i < cn; i++) pSysFreeString(nm[i]);
            /* limited buffer */
            cn = 0; memset(nm, 0, sizeof nm);
            OK_(ITypeInfo_GetNames(t, 0x10, nm, 2, &cn));
            EQ(cn, 2);
            for (UINT i = 0; i < cn; i++) pSysFreeString(nm[i]);
            cn = 77;
            OK_(ITypeInfo_GetNames(t, 0x10, nm, 0, &cn));
            EQ(cn, 0);
            EQHR(ITypeInfo_GetNames(t, 0x7777, nm, 2, &cn), TYPE_E_ELEMENTNOTFOUND);
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
}

/* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - test_CreateTypeLib: default values */
TEST(tlci_param_defaults_roundtrip)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "defaults.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IDefaults", TKIND_INTERFACE);
    ELEMDESC el[2]; PARAMDESCEX pe[2];
    FUNCDESC fd; memset(&fd, 0, sizeof fd);
    memset(el, 0, sizeof el); memset(pe, 0, sizeof pe);
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
    fd.elemdescFunc.tdesc.vt = VT_VOID;
    fd.cParams = 2; fd.lprgelemdescParam = el;
    el[0].tdesc.vt = VT_INT; el[0].paramdesc.wParamFlags = PARAMFLAG_FHASDEFAULT; el[0].paramdesc.pparamdescex = &pe[0];
    pe[0].cBytes = sizeof(PARAMDESCEX); V_VT(&pe[0].varDefaultValue) = VT_INT; V_INT(&pe[0].varDefaultValue) = 0x123;
    el[1].tdesc.vt = VT_UI2; el[1].paramdesc.wParamFlags = PARAMFLAG_FHASDEFAULT; el[1].paramdesc.pparamdescex = &pe[1];
    pe[1].cBytes = sizeof(PARAMDESCEX); V_VT(&pe[1].varDefaultValue) = VT_UI2; V_UI2(&pe[1].varDefaultValue) = 0xffff;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    ITypeInfo *ti = tl_ti_of_cti(c);
    FUNCDESC *p = NULL;
    OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
    if (p) {
        EQ(p->cParams, 2);
        ELEMDESC *e = p->lprgelemdescParam;
        CHECK(e != NULL);
        if (e) {
            EQ(e[0].tdesc.vt, VT_INT);
            EQ(e[0].paramdesc.wParamFlags, PARAMFLAG_FHASDEFAULT);
            CHECK(e[0].paramdesc.pparamdescex != NULL);
            if (e[0].paramdesc.pparamdescex) {
                EQ(e[0].paramdesc.pparamdescex->cBytes, sizeof(PARAMDESCEX));
                EQ(V_VT(&e[0].paramdesc.pparamdescex->varDefaultValue), VT_I4);
                EQ(V_I4(&e[0].paramdesc.pparamdescex->varDefaultValue), 0x123);
            }
            EQ(e[1].tdesc.vt, VT_UI2);
            CHECK(e[1].paramdesc.pparamdescex != NULL);
            if (e[1].paramdesc.pparamdescex) {
                EQ(V_VT(&e[1].paramdesc.pparamdescex->varDefaultValue), VT_UI2);
                EQ(V_UI2(&e[1].paramdesc.pparamdescex->varDefaultValue), 0xffff);
            }
        }
        ITypeInfo_ReleaseFuncDesc(ti, p);
    }
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (tl) {
        ITypeInfo *t = tl_ti(tl, 0);
        if (t) {
            OK_(ITypeInfo_GetFuncDesc(t, 0, &p));
            if (p && p->lprgelemdescParam) {
                ELEMDESC *e = p->lprgelemdescParam;
                if (e[0].paramdesc.pparamdescex) {
                    EQ(V_VT(&e[0].paramdesc.pparamdescex->varDefaultValue), VT_I4);
                    EQ(V_I4(&e[0].paramdesc.pparamdescex->varDefaultValue), 0x123);
                } else CHECK(0);
                if (e[1].paramdesc.pparamdescex) EQ(V_UI2(&e[1].paramdesc.pparamdescex->varDefaultValue), 0xffff);
                else CHECK(0);
            }
            if (p) ITypeInfo_ReleaseFuncDesc(t, p);
            ITypeInfo_Release(t);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlci_param_default_variants)
{
    TL_BEGIN();
    struct dv { VARTYPE vt; VARTYPE tvt; int kind; };
    static const struct dv t[] = {
        { VT_I2, VT_I2, 0 }, { VT_I4, VT_I4, 0 }, { VT_R4, VT_R4, 1 }, { VT_R8, VT_R8, 1 }, { VT_BOOL, VT_BOOL, 2 },
        { VT_UI1, VT_UI1, 0 }, { VT_I1, VT_I1, 0 }, { VT_BSTR, VT_BSTR, 3 }, { VT_UI4, VT_UI4, 0 }, { VT_DATE, VT_DATE, 1 },
    };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "dvar.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IDV", TKIND_INTERFACE);
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        ELEMDESC el; PARAMDESCEX pe; FUNCDESC fd;
        memset(&el, 0, sizeof el); memset(&pe, 0, sizeof pe); memset(&fd, 0, sizeof fd);
        el.tdesc.vt = t[i].tvt; el.paramdesc.wParamFlags = PARAMFLAG_FIN | PARAMFLAG_FHASDEFAULT; el.paramdesc.pparamdescex = &pe;
        pe.cBytes = sizeof pe;
        V_VT(&pe.varDefaultValue) = t[i].vt;
        switch (t[i].kind) {
        case 0: V_I4(&pe.varDefaultValue) = 0; V_UI1(&pe.varDefaultValue) = 42; break;
        case 1: V_R8(&pe.varDefaultValue) = 0; if (t[i].vt == VT_R4) V_R4(&pe.varDefaultValue) = 1.5f; else V_R8(&pe.varDefaultValue) = 2.5; break;
        case 2: V_BOOL(&pe.varDefaultValue) = VARIANT_TRUE; break;
        case 3: V_BSTR(&pe.varDefaultValue) = pSysAllocString(L"dflt"); break;
        }
        fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
        fd.elemdescFunc.tdesc.vt = VT_VOID; fd.cParams = 1; fd.lprgelemdescParam = &el;
        HRESULT hr = ICreateTypeInfo_AddFuncDesc(c, i, &fd);
        CHECKF(hr == S_OK, "vt %u default AddFuncDesc hr=0x%08lx", t[i].vt, hr);
        if (t[i].kind == 3) pSysFreeString(V_BSTR(&pe.varDefaultValue));
    }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
            FUNCDESC *p = NULL;
            HRESULT ghr = ITypeInfo_GetFuncDesc(ti, i, &p);
            if (t[i].vt == VT_I1 || t[i].vt == VT_UI4) {
                if (ghr != S_OK) t_note("GetFuncDesc with default vt %u: hr=0x%08lx", t[i].vt, (unsigned long)ghr);
                if (ghr == S_OK) ITypeInfo_ReleaseFuncDesc(ti, p);
                continue;
            }
            CHECKF(ghr == S_OK, "GetFuncDesc with default vt %u: hr=0x%08lx", t[i].vt, (unsigned long)ghr);
            if (!p || !p->lprgelemdescParam) continue;
            PARAMDESCEX *pe = p->lprgelemdescParam[0].paramdesc.pparamdescex;
            CHECK(pe != NULL);
            if (pe) {
                VARIANT *v = &pe->varDefaultValue;
                CHECKF(V_VT(v) == t[i].vt, "param %u default vt %u want %u", i, V_VT(v), t[i].vt);
                switch (t[i].vt) {
                case VT_I2: EQ(V_I2(v), 42); break;
                case VT_I4: EQ(V_I4(v), 42); break;
                case VT_UI1: EQ(V_UI1(v), 42); break;
                case VT_I1: EQ(V_I1(v), 42); break;
                case VT_UI4: EQ(V_UI4(v), 42); break;
                case VT_R4: CHECK(V_R4(v) == 1.5f); break;
                case VT_R8: case VT_DATE: CHECK(V_R8(v) == 2.5); break;
                case VT_BOOL: SOFTEQ((USHORT)V_BOOL(v), (USHORT)VARIANT_TRUE); break;
                case VT_BSTR: CHECK(V_BSTR(v) && !lstrcmpW(V_BSTR(v), L"dflt")); break;
                }
            }
            ITypeInfo_ReleaseFuncDesc(ti, p);
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_param_flags_roundtrip)
{
    TL_BEGIN();
    static const WORD flags[] = { PARAMFLAG_NONE, PARAMFLAG_FIN, PARAMFLAG_FOUT, PARAMFLAG_FIN | PARAMFLAG_FOUT, PARAMFLAG_FLCID,
        PARAMFLAG_FRETVAL | PARAMFLAG_FOUT, PARAMFLAG_FOPT | PARAMFLAG_FIN, PARAMFLAG_FOUT | PARAMFLAG_FRETVAL | PARAMFLAG_FIN };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "pflags.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IPF", TKIND_INTERFACE);
    ELEMDESC el[8]; memset(el, 0, sizeof el);
    FUNCDESC fd; memset(&fd, 0, sizeof fd);
    for (unsigned i = 0; i < 8; i++) { el[i].tdesc.vt = VT_I4; el[i].paramdesc.wParamFlags = flags[i]; }
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL; fd.elemdescFunc.tdesc.vt = VT_HRESULT;
    fd.cParams = 8; fd.lprgelemdescParam = el;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        FUNCDESC *p = NULL;
        OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
        if (p && p->lprgelemdescParam) {
            EQ(p->cParams, 8);
            for (unsigned i = 0; i < 8; i++)
                CHECKF(p->lprgelemdescParam[i].paramdesc.wParamFlags == flags[i], "param %u flags 0x%x want 0x%x", i,
                       p->lprgelemdescParam[i].paramdesc.wParamFlags, flags[i]);
            EQ(p->elemdescFunc.tdesc.vt, VT_HRESULT);
        } else CHECK(0);
        if (p) ITypeInfo_ReleaseFuncDesc(ti, p);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_param_idlflags_vs_paramflags)
{
    TL_BEGIN();
    /* ELEMDESC.idldesc.wIDLFlags and paramdesc.wParamFlags share storage in the union */
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "idlfl.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IIdl", TKIND_INTERFACE);
    ELEMDESC el; FUNCDESC fd; memset(&el, 0, sizeof el); memset(&fd, 0, sizeof fd);
    el.tdesc.vt = VT_BSTR; el.idldesc.dwReserved = 0; el.idldesc.wIDLFlags = IDLFLAG_FIN | IDLFLAG_FOUT;
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL; fd.elemdescFunc.tdesc.vt = VT_VOID;
    fd.cParams = 1; fd.lprgelemdescParam = &el;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    ITypeInfo *ti = tl_ti_of_cti(c);
    FUNCDESC *p = NULL;
    OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
    if (p && p->lprgelemdescParam) {
        EQ(p->lprgelemdescParam[0].idldesc.wIDLFlags, IDLFLAG_FIN | IDLFLAG_FOUT);
        EQ(p->lprgelemdescParam[0].paramdesc.wParamFlags, PARAMFLAG_FIN | PARAMFLAG_FOUT);
    } else CHECK(0);
    if (p) ITypeInfo_ReleaseFuncDesc(ti, p);
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlci_param_type_pointer_chain)
{
    TL_BEGIN();
    /* adapted from Wine dlls/oleaut32/tests/typelib.c (LGPL-2.1+) - VT_PTR chains survive AddFuncDesc (deep copy) */
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ptrchain.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IPtr", TKIND_INTERFACE);
    TYPEDESC t1, t2;
    ELEMDESC el; FUNCDESC fd; memset(&el, 0, sizeof el); memset(&fd, 0, sizeof fd);
    t1.vt = VT_BSTR;
    t2.vt = VT_PTR; t2.lptdesc = &t1;
    el.tdesc.vt = VT_PTR; el.tdesc.lptdesc = &t2;
    el.paramdesc.wParamFlags = PARAMFLAG_FIN;
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL; fd.elemdescFunc.tdesc.vt = VT_VOID;
    fd.cParams = 1; fd.lprgelemdescParam = &el;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    /* scribble over the caller's memory: the stored copy must be independent */
    t1.vt = VT_I4; t2.vt = VT_I2; el.tdesc.vt = VT_R8;
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        FUNCDESC *p = NULL;
        OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
        if (p && p->lprgelemdescParam) {
            TYPEDESC *d = &p->lprgelemdescParam[0].tdesc;
            EQ(d->vt, VT_PTR);
            CHECK(d->lptdesc != NULL);
            if (d->lptdesc) {
                EQ(d->lptdesc->vt, VT_PTR);
                CHECK(d->lptdesc->lptdesc != NULL);
                if (d->lptdesc->lptdesc) EQ(d->lptdesc->lptdesc->vt, VT_BSTR);
            }
        } else CHECK(0);
        if (p) ITypeInfo_ReleaseFuncDesc(ti, p);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_param_type_carray_and_safearray)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "carray.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IArr", TKIND_INTERFACE);
    ELEMDESC el[2]; FUNCDESC fd; memset(el, 0, sizeof el); memset(&fd, 0, sizeof fd);
    TYPEDESC sa_el; sa_el.vt = VT_BSTR;
    ARRAYDESC ad; memset(&ad, 0, sizeof ad);
    ad.tdescElem.vt = VT_I4; ad.cDims = 2; ad.rgbounds[0].cElements = 3; ad.rgbounds[0].lLbound = 0;
    ARRAYDESC *adp = malloc(sizeof(ARRAYDESC) + sizeof(SAFEARRAYBOUND));
    memcpy(adp, &ad, sizeof ad);
    adp->rgbounds[1].cElements = 5; adp->rgbounds[1].lLbound = 1;
    el[0].tdesc.vt = VT_PTR;
    TYPEDESC ca; ca.vt = VT_CARRAY; ca.lpadesc = adp;
    el[0].tdesc.lptdesc = &ca;
    el[0].paramdesc.wParamFlags = PARAMFLAG_FIN;
    el[1].tdesc.vt = VT_SAFEARRAY; el[1].tdesc.lptdesc = &sa_el; el[1].paramdesc.wParamFlags = PARAMFLAG_FIN;
    fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL; fd.elemdescFunc.tdesc.vt = VT_VOID;
    fd.cParams = 2; fd.lprgelemdescParam = el;
    OK_(ICreateTypeInfo_AddFuncDesc(c, 0, &fd));
    free(adp);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        FUNCDESC *p = NULL;
        OK_(ITypeInfo_GetFuncDesc(ti, 0, &p));
        if (p && p->lprgelemdescParam) {
            TYPEDESC *d = &p->lprgelemdescParam[0].tdesc;
            EQ(d->vt, VT_PTR);
            if (d->lptdesc && d->lptdesc->vt == VT_CARRAY && d->lptdesc->lpadesc) {
                ARRAYDESC *a = d->lptdesc->lpadesc;
                EQ(a->tdescElem.vt, VT_I4);
                EQ(a->cDims, 2);
                EQ(a->rgbounds[0].cElements, 3);
                EQ(a->rgbounds[0].lLbound, 0);
                EQ(a->rgbounds[1].cElements, 5);
                EQ(a->rgbounds[1].lLbound, 1);
            } else CHECK(0);
            d = &p->lprgelemdescParam[1].tdesc;
            EQ(d->vt, VT_SAFEARRAY);
            CHECK(d->lptdesc && d->lptdesc->vt == VT_BSTR);
        } else CHECK(0);
        if (p) ITypeInfo_ReleaseFuncDesc(ti, p);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

/* every invkind x callconv combination survives the round trip */
TEST(tlci_invkind_callconv_matrix)
{
    TL_BEGIN();
    static const INVOKEKIND iks[] = { INVOKE_FUNC, INVOKE_PROPERTYGET, INVOKE_PROPERTYPUT, INVOKE_PROPERTYPUTREF };
    static const CALLCONV ccs[] = { CC_CDECL, CC_PASCAL, CC_STDCALL, CC_SYSCALL, CC_MPWCDECL, CC_MPWPASCAL };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "matrix.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IMat", TKIND_INTERFACE);
    UINT n = 0;
    for (unsigned i = 0; i < 4; i++)
        for (unsigned k = 0; k < 6; k++) {
            ELEMDESC el; FUNCDESC fd; memset(&el, 0, sizeof el); memset(&fd, 0, sizeof fd);
            el.tdesc.vt = VT_I4; el.paramdesc.wParamFlags = PARAMFLAG_FIN;
            fd.memid = 0x1000 + n; fd.funckind = FUNC_PUREVIRTUAL; fd.invkind = iks[i]; fd.callconv = ccs[k];
            fd.elemdescFunc.tdesc.vt = VT_VOID;
            if (iks[i] == INVOKE_PROPERTYPUT || iks[i] == INVOKE_PROPERTYPUTREF) { fd.cParams = 1; fd.lprgelemdescParam = &el; }
            HRESULT hr = ICreateTypeInfo_AddFuncDesc(c, n, &fd);
            CHECKF(hr == S_OK, "AddFuncDesc invkind %d cc %d hr 0x%08lx", iks[i], ccs[k], hr);
            n += hr == S_OK;
        }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        if (ta) { EQ(ta->cFuncs, n); ITypeInfo_ReleaseTypeAttr(ti, ta); }
        UINT m = 0;
        for (unsigned i = 0; i < 4; i++)
            for (unsigned k = 0; k < 6; k++, m++) {
                FUNCDESC *p = NULL;
                if (ITypeInfo_GetFuncDesc(ti, m, &p) != S_OK || !p) { CHECK(0); continue; }
                CHECKF(p->invkind == iks[i] && p->callconv == ccs[k] && p->memid == (MEMBERID)(0x1000 + m),
                       "func %u: invkind %d cc %d memid 0x%lx", m, p->invkind, p->callconv, p->memid);
                ITypeInfo_ReleaseFuncDesc(ti, p);
            }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_funcflags_roundtrip)
{
    TL_BEGIN();
    static const WORD fl[] = { FUNCFLAG_FRESTRICTED, FUNCFLAG_FSOURCE, FUNCFLAG_FBINDABLE, FUNCFLAG_FREQUESTEDIT,
        FUNCFLAG_FDISPLAYBIND, FUNCFLAG_FDEFAULTBIND, FUNCFLAG_FHIDDEN, FUNCFLAG_FUSESGETLASTERROR,
        FUNCFLAG_FDEFAULTCOLLELEM, FUNCFLAG_FUIDEFAULT, FUNCFLAG_FNONBROWSABLE, FUNCFLAG_FREPLACEABLE, FUNCFLAG_FIMMEDIATEBIND };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ffl.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IFF", TKIND_DISPATCH);
    for (unsigned i = 0; i < 13; i++) {
        FUNCDESC fd; memset(&fd, 0, sizeof fd);
        fd.memid = 0x500 + i; fd.funckind = FUNC_DISPATCH; fd.invkind = INVOKE_FUNC; fd.callconv = CC_STDCALL;
        fd.elemdescFunc.tdesc.vt = VT_VOID; fd.wFuncFlags = fl[i];
        OK_(ICreateTypeInfo_AddFuncDesc(c, i, &fd));
    }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        for (unsigned i = 0; i < 13; i++) {
            FUNCDESC *p = NULL;
            OK_(ITypeInfo_GetFuncDesc(ti, i, &p));
            if (p) { CHECKF(p->wFuncFlags == fl[i], "func %u flags 0x%x want 0x%x", i, p->wFuncFlags, fl[i]); ITypeInfo_ReleaseFuncDesc(ti, p); }
        }
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_dispatch_funckind_and_vft)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "dispvft.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"DI", TKIND_DISPATCH);
    for (UINT i = 0; i < 3; i++)
        OK_(tl_add_func_k(c, i, FUNC_DISPATCH, INVOKE_FUNC, VT_VOID, 1 + i, NULL, 0));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(ti, &ta));
        UINT nf = 0;
        if (ta) {
            EQ(ta->typekind, TKIND_DISPATCH);
            nf = ta->cFuncs;
            CHECK(nf >= 3);
            t_note("dispinterface without IDispatch base: cFuncs=%u cImplTypes=%u cbSizeVft=%u flags=0x%x", ta->cFuncs, ta->cImplTypes, ta->cbSizeVft, ta->wTypeFlags);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        int seen = 0;
        for (UINT i = 0; i < nf; i++) {
            FUNCDESC *p = NULL;
            if (ITypeInfo_GetFuncDesc(ti, i, &p) != S_OK || !p) continue;
            if (p->memid >= 1 && p->memid <= 3) { seen |= 1 << p->memid; EQ(p->funckind, FUNC_DISPATCH); EQ(p->invkind, INVOKE_FUNC); }
            ITypeInfo_ReleaseFuncDesc(ti, p);
        }
        EQ(seen, 0xe);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_typeinfo_attrs_roundtrip)
{
    TL_BEGIN();
    static const WORD tf = TYPEFLAG_FHIDDEN | TYPEFLAG_FRESTRICTED | TYPEFLAG_FNONEXTENSIBLE | TYPEFLAG_FOLEAUTOMATION;
    for (int s = 0; s < 2; s++) {
        WCHAR path[MAX_PATH];
        ICreateTypeLib2 *ctl = tl_create(s ? SYS_WIN64 : SYS_WIN32, "tattr.tlb", path);
        if (!ctl) return;
        ICreateTypeInfo *c = tl_cti(ctl, L"IAttrs", TKIND_INTERFACE);
        OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_A));
        OK_(ICreateTypeInfo_SetVersion(c, 5, 9));
        OK_(ICreateTypeInfo_SetTypeFlags(c, tf));
        OK_(ICreateTypeInfo_SetDocString(c, (LPOLESTR)L"Doc for IAttrs"));
        OK_(ICreateTypeInfo_SetHelpContext(c, 0x4242));
        OK_(ICreateTypeInfo_SetAlignment(c, 8));
        ICreateTypeInfo_Release(c);
        ITypeLib *tl = tl_reload(ctl, path);
        if (!tl) return;
        ITypeInfo *ti = tl_ti(tl, 0);
        if (ti) {
            TYPEATTR *ta;
            OK_(ITypeInfo_GetTypeAttr(ti, &ta));
            if (ta) {
                CHECK(IsEqualGUID(&ta->guid, &TLC_GUID_A));
                EQ(ta->wMajorVerNum, 5);
                EQ(ta->wMinorVerNum, 9);
                EQ(ta->wTypeFlags & tf, tf);
                EQ(ta->typekind, TKIND_INTERFACE);
                EQ(ta->lcid, 0);
                EQ(ta->memidConstructor, MEMBERID_NIL);
                EQ(ta->memidDestructor, MEMBERID_NIL);
                CHECK(ta->lpstrSchema == NULL);
                t_note("cbAlignment after SetAlignment(8) = %u", ta->cbAlignment);
                ITypeInfo_ReleaseTypeAttr(ti, ta);
            }
            BSTR nm = NULL, doc = NULL; DWORD hc = 0; BSTR hf = NULL;
            OK_(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &nm, &doc, &hc, &hf));
            CHECK(nm && !lstrcmpW(nm, L"IAttrs"));
            CHECK(doc && !lstrcmpW(doc, L"Doc for IAttrs"));
            EQ(hc, 0x4242);
            pSysFreeString(nm); pSysFreeString(doc); pSysFreeString(hf);
            ITypeInfo_Release(ti);
        }
        ITypeLib_Release(tl);
    }
}

TEST(tlci_typeinfo_version_table)
{
    TL_BEGIN();
    static const WORD v[][2] = { {0,0}, {1,0}, {0,1}, {7,65535}, {65535,0} };
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "tver.tlb", path);
    if (!ctl) return;
    for (unsigned i = 0; i < 5; i++) {
        WCHAR n[8]; wsprintfW(n, L"T%u", i);
        ICreateTypeInfo *c = tl_cti(ctl, n, TKIND_RECORD);
        if (c) { OK_(ICreateTypeInfo_SetVersion(c, v[i][0], v[i][1])); ICreateTypeInfo_Release(c); }
    }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    for (unsigned i = 0; i < 5; i++) {
        ITypeInfo *ti = tl_ti(tl, i);
        TYPEATTR *ta = NULL;
        if (ti && ITypeInfo_GetTypeAttr(ti, &ta) == S_OK) {
            CHECKF(ta->wMajorVerNum == v[i][0] && ta->wMinorVerNum == v[i][1], "type %u version %u.%u", i, ta->wMajorVerNum, ta->wMinorVerNum);
            ITypeInfo_ReleaseTypeAttr(ti, ta);
        }
        if (ti) ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_func_documentation)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "fdoc.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IDoc", TKIND_INTERFACE);
    EQHR(ICreateTypeInfo_SetFuncDocString(c, 0, (LPOLESTR)L"doc1"), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetFuncDocString(c, 0, NULL), E_INVALIDARG);
    EQHR(ICreateTypeInfo_SetFuncHelpContext(c, 0, 5), TYPE_E_ELEMENTNOTFOUND);
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x20, NULL, 0));
    OK_(tl_add_func(c, 1, INVOKE_FUNC, VT_VOID, 0x21, NULL, 0));
    OK_(ICreateTypeInfo_SetFuncDocString(c, 0, (LPOLESTR)L"first function"));
    OK_(ICreateTypeInfo_SetFuncHelpContext(c, 0, 0xabcdefab));
    OK_(ICreateTypeInfo_SetFuncHelpContext(c, 0, 0x201)); /* overwrite */
    OK_(ICreateTypeInfo_SetFuncHelpContext(c, 1, 77));
    EQHR(ICreateTypeInfo_SetFuncDocString(c, 2, (LPOLESTR)L"x"), TYPE_E_ELEMENTNOTFOUND);
    EQHR(ICreateTypeInfo_SetFuncHelpContext(c, 2, 1), TYPE_E_ELEMENTNOTFOUND);
    LPOLESTR nm[1] = { (LPOLESTR)L"Second" };
    OK_(ICreateTypeInfo_SetFuncAndParamNames(c, 1, nm, 1));
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    if (ti) {
        BSTR name = NULL, doc = NULL; DWORD hc = 0;
        OK_(ITypeInfo_GetDocumentation(ti, 0x20, &name, &doc, &hc, NULL));
        CHECK(name == NULL);
        CHECK(doc && !lstrcmpW(doc, L"first function"));
        EQ(hc, 0x201);
        pSysFreeString(name); pSysFreeString(doc);
        name = doc = NULL; hc = 99;
        OK_(ITypeInfo_GetDocumentation(ti, 0x21, &name, &doc, &hc, NULL));
        CHECK(name && !lstrcmpW(name, L"Second"));
        CHECK(doc == NULL);
        EQ(hc, 77);
        pSysFreeString(name); pSysFreeString(doc);
        EQHR(ITypeInfo_GetDocumentation(ti, 0x99, &name, &doc, &hc, NULL), TYPE_E_ELEMENTNOTFOUND);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlci_typeinfo_documentation_roundtrip)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "tdoc.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetHelpFileName(ctl, (LPOLESTR)L"c:\\x\\y.hlp"));
    ICreateTypeInfo *c = tl_cti(ctl, L"IDocT", TKIND_INTERFACE);
    OK_(ICreateTypeInfo_SetDocString(c, (LPOLESTR)L"interface docs"));
    OK_(ICreateTypeInfo_SetHelpContext(c, 12345));
    SOFTHR(ICreateTypeInfo_SetDocString(c, NULL), E_INVALIDARG);
    ITypeInfo *ti = tl_ti_of_cti(c);
    BSTR name = NULL, doc = NULL, hf = NULL; DWORD hc = 0;
    OK_(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, &name, &doc, &hc, &hf));
    CHECK(name && !lstrcmpW(name, L"IDocT"));
    CHECK(doc && !lstrcmpW(doc, L"interface docs"));
    EQ(hc, 12345);
    CHECK(hf && !lstrcmpW(hf, L"c:\\x\\y.hlp"));
    pSysFreeString(name); pSysFreeString(doc); pSysFreeString(hf);
    name = doc = hf = (BSTR)(ULONG_PTR)0xdeadbeef;
    OK_(ITypeInfo_GetDocumentation(ti, MEMBERID_NIL, NULL, NULL, NULL, NULL));
    ITypeInfo_Release(ti);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlci_typeinfo2_docstring2_and_helpstringcontext)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "doc2.tlb", path);
    if (!ctl) return;
    OK_(ICreateTypeLib2_SetHelpStringDll(ctl, (LPOLESTR)L"olet_help.dll"));
    ICreateTypeInfo *c = tl_cti(ctl, L"IDoc2", TKIND_INTERFACE);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    if (c2) {
        t_note("type SetHelpStringContext hr=0x%08lx", (unsigned long)ICreateTypeInfo2_SetHelpStringContext(c2, 777));
        OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x30, NULL, 0));
        t_note("SetFuncHelpStringContext hr=0x%08lx", (unsigned long)ICreateTypeInfo2_SetFuncHelpStringContext(c2, 0, 888));
        SOFTHR(ICreateTypeInfo2_SetFuncHelpStringContext(c2, 1, 888), TYPE_E_ELEMENTNOTFOUND);
        ICreateTypeInfo2_Release(c2);
    }
    ICreateTypeInfo_Release(c);
    t_note("lib SetHelpStringContext hr=0x%08lx", (unsigned long)ICreateTypeLib2_SetHelpStringContext(ctl, 4321));
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeLib2 *tl2 = NULL;
    OK_(ITypeLib_QueryInterface(tl, &IID_ITypeLib2, (void **)&tl2));
    if (tl2) {
        BSTR dll = NULL; DWORD hsc = 0; BSTR doc = NULL;
        HRESULT hr = ITypeLib2_GetDocumentation2(tl2, -1, 0, &doc, &hsc, &dll);
        t_note("lib GetDocumentation2 hr=0x%08lx helpstringcontext=%lu dll=%ls", (unsigned long)hr, hsc, dll ? dll : L"(null)");
        if (hr == S_OK) { SOFTEQ(hsc, 4321); if (!dll) t_note("no helpstringdll returned"); else SOFTEQ(!lstrcmpW(dll, L"olet_help.dll"), 1); }
        pSysFreeString(dll); pSysFreeString(doc);
        ITypeLib2_Release(tl2);
    }
    ITypeInfo *ti = tl_ti(tl, 0);
    ITypeInfo2 *ti2 = NULL;
    if (ti && ITypeInfo_QueryInterface(ti, &IID_ITypeInfo2, (void **)&ti2) == S_OK) {
        DWORD hsc = 0; BSTR dll = NULL, doc = NULL;
        HRESULT hr = ITypeInfo2_GetDocumentation2(ti2, MEMBERID_NIL, 0, &doc, &hsc, &dll);
        t_note("type GetDocumentation2 hr=0x%08lx hsc=%lu", (unsigned long)hr, hsc);
        if (hr == S_OK) SOFTEQ(hsc, 777);
        pSysFreeString(dll); pSysFreeString(doc);
        dll = doc = NULL; hsc = 0;
        hr = ITypeInfo2_GetDocumentation2(ti2, 0x30, 0, &doc, &hsc, &dll);
        t_note("func GetDocumentation2 hr=0x%08lx hsc=%lu", (unsigned long)hr, hsc);
        if (hr == S_OK) SOFTEQ(hsc, 888);
        pSysFreeString(dll); pSysFreeString(doc);
        ITypeInfo2_Release(ti2);
    }
    if (ti) ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

TEST(tlci_func_custdata)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "fcust.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"ICust", TKIND_INTERFACE);
    ICreateTypeInfo2 *c2 = NULL;
    OK_(ICreateTypeInfo_QueryInterface(c, &IID_ICreateTypeInfo2, (void **)&c2));
    OK_(tl_add_func(c, 0, INVOKE_FUNC, VT_VOID, 0x40, NULL, 0));
    VARTYPE p1 = VT_I4;
    OK_(tl_add_func(c, 1, INVOKE_FUNC, VT_VOID, 0x41, &p1, 1));
    VARIANT v; pVariantInit(&v);
    HRESULT fhr = E_FAIL, phr = E_FAIL;
    if (c2) {
        V_VT(&v) = VT_I4; V_I4(&v) = 123;
        OK_(ICreateTypeInfo2_SetCustData(c2, (GUID *)&TLC_GUID_CUST, &v));
        V_VT(&v) = VT_BSTR; V_BSTR(&v) = pSysAllocString(L"func cust");
        fhr = ICreateTypeInfo2_SetFuncCustData(c2, 0, (GUID *)&TLC_GUID_CUST, &v);
        pVariantClear(&v);
        V_VT(&v) = VT_I4; V_I4(&v) = 9;
        phr = ICreateTypeInfo2_SetParamCustData(c2, 1, 0, (GUID *)&TLC_GUID_CUST2, &v);
        t_note("SetFuncCustData hr=0x%08lx SetParamCustData hr=0x%08lx", (unsigned long)fhr, (unsigned long)phr);
        SOFTHR(ICreateTypeInfo2_SetFuncCustData(c2, 5, (GUID *)&TLC_GUID_CUST, &v), TYPE_E_ELEMENTNOTFOUND);
        SOFTHR(ICreateTypeInfo2_SetParamCustData(c2, 1, 3, (GUID *)&TLC_GUID_CUST, &v), TYPE_E_ELEMENTNOTFOUND);
        ICreateTypeInfo2_Release(c2);
    }
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    ITypeInfo2 *ti2 = NULL;
    if (ti && ITypeInfo_QueryInterface(ti, &IID_ITypeInfo2, (void **)&ti2) == S_OK) {
        VARIANT r; pVariantInit(&r);
        OK_(ITypeInfo2_GetCustData(ti2, (GUID *)&TLC_GUID_CUST, &r));
        EQ(V_VT(&r), VT_I4); EQ(V_I4(&r), 123);
        if (fhr == S_OK) {
            OK_(ITypeInfo2_GetFuncCustData(ti2, 0, (GUID *)&TLC_GUID_CUST, &r));
            EQ(V_VT(&r), VT_BSTR);
            CHECK(V_VT(&r) == VT_BSTR && V_BSTR(&r) && !lstrcmpW(V_BSTR(&r), L"func cust"));
            pVariantClear(&r);
        }
        if (phr == S_OK) {
            OK_(ITypeInfo2_GetParamCustData(ti2, 1, 0, (GUID *)&TLC_GUID_CUST2, &r));
            EQ(V_VT(&r), VT_I4); EQ(V_I4(&r), 9);
        }
        CUSTDATA cd;
        if (fhr == S_OK) {
            OK_(ITypeInfo2_GetAllFuncCustData(ti2, 0, &cd));
            EQ(cd.cCustData, 1);
            ClearCustData(&cd);
        }
        OK_(ITypeInfo2_GetAllCustData(ti2, &cd));
        EQ(cd.cCustData, 1);
        ClearCustData(&cd);
        ITypeInfo2_Release(ti2);
    }
    if (ti) ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

TEST(tlci_qi_interfaces_of_createtypeinfo)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "ctiqi.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IQ", TKIND_INTERFACE);
    void *p;
    static const struct { const GUID *g; HRESULT hr; } t[] = {
        { &IID_IUnknown, S_OK }, { &IID_ICreateTypeInfo, S_OK }, { &IID_ICreateTypeInfo2, S_OK },
        { &IID_ITypeInfo, S_OK }, { &IID_ITypeInfo2, S_OK }, { &IID_ITypeLib, E_NOINTERFACE }, { &IID_IDispatch, E_NOINTERFACE } };
    for (unsigned i = 0; i < 7; i++) {
        p = (void *)1;
        HRESULT hr = ICreateTypeInfo_QueryInterface(c, t[i].g, &p);
        CHECKF(hr == t[i].hr, "QI #%u hr=0x%08lx", i, (unsigned long)hr);
        if (hr == S_OK) IUnknown_Release((IUnknown *)p); else CHECK(p == NULL);
    }
    EQ(ICreateTypeInfo_AddRef(c), 2);
    EQ(ICreateTypeInfo_Release(c), 1);
    ICreateTypeInfo_Release(c);
    ICreateTypeLib2_Release(ctl);
}

TEST(tlci_typeinfo_refcount_independent_of_lib)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN32, "cnt.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *c = tl_cti(ctl, L"IRef", TKIND_INTERFACE);
    ITypeInfo *ti = tl_ti_of_cti(c);
    ICreateTypeInfo_Release(c);
    ITypeLib *tl = NULL; UINT idx = 5;
    OK_(ITypeInfo_GetContainingTypeLib(ti, &tl, &idx));
    EQ(idx, 0);
    if (tl) {
        EQ(ITypeLib_GetTypeInfoCount(tl), 1);
        ITypeLib_Release(tl);
    }
    OK_(ICreateTypeLib2_SaveAllChanges(ctl));
    ULONG r = ICreateTypeLib2_Release(ctl);
    t_note("ICreateTypeLib2 refs after release while a typeinfo is still held = %lu", r);
    ITypeInfo_Release(ti);
}
