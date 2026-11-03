/* IRecordInfo over struct type information built with CreateTypeLib2 (SYS_WIN64, so the stored field offsets are the
 * offsets of this process' C struct), GetRecordInfoFromTypeInfo/GetRecordInfoFromGuids, VT_RECORD VARIANTs and
 * SAFEARRAYs of records.
 * Decomp-verified (Windows 10 oleaut32): GetRecordInfoFromTypeInfo needs a non-NULL ITypeInfo and out pointer, accepts
 * TKIND_RECORD with a non-NULL GUID (or an alias to a user-defined type) and otherwise returns E_INVALIDARG;
 * GetRecordInfoFromGuids rejects GUID_NULL for either guid and a NULL out pointer with E_INVALIDARG and first
 * looks the type up by its type GUID in a process-wide table of live record infos. */
#include "tlc.h"

typedef struct Person { LONG Id; BSTR Name; double Score; SAFEARRAY *Tags; } Person;

static TYPEDESC td_i4r = { .vt = VT_I4 }, td_bstrr = { .vt = VT_BSTR }, td_r8r = { .vt = VT_R8 };
static TYPEDESC td_sa_i4 = { .vt = VT_I4 };

static ITypeLib *build_person(WCHAR *path, const char *leaf)
{
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, leaf, path);
    if (!ctl) return NULL;
    OK_(ICreateTypeLib2_SetGuid(ctl, &TLC_GUID_LIB));
    OK_(ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"PersonLib"));
    ICreateTypeInfo *c = tl_cti(ctl, L"Person", TKIND_RECORD);
    OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_C));
    TYPEDESC sa; sa.vt = VT_SAFEARRAY; sa.lptdesc = &td_sa_i4;
    static const WCHAR *nm[4] = { L"Id", L"Name", L"Score", L"Tags" };
    TYPEDESC *td[4] = { &td_i4r, &td_bstrr, &td_r8r, &sa };
    static const ULONG off[4] = { offsetof(Person, Id), offsetof(Person, Name), offsetof(Person, Score), offsetof(Person, Tags) };
    for (UINT i = 0; i < 4; i++) {
        VARDESC vd; memset(&vd, 0, sizeof vd);
        vd.memid = MEMBERID_NIL; vd.elemdescVar.tdesc = *td[i]; vd.varkind = VAR_PERINSTANCE; vd.oInst = off[i];
        OK_(ICreateTypeInfo_AddVarDesc(c, i, &vd));
        OK_(ICreateTypeInfo_SetVarName(c, i, (LPOLESTR)nm[i]));
    }
    OK_(ICreateTypeInfo_LayOut(c));
    ICreateTypeInfo_Release(c);
    c = tl_cti(ctl, L"Other", TKIND_RECORD);
    OK_(ICreateTypeInfo_SetGuid(c, &TLC_GUID_D));
    {
        VARDESC vd; memset(&vd, 0, sizeof vd);
        vd.elemdescVar.tdesc = td_i4r; vd.varkind = VAR_PERINSTANCE; vd.oInst = 0;
        OK_(ICreateTypeInfo_AddVarDesc(c, 0, &vd));
        OK_(ICreateTypeInfo_SetVarName(c, 0, (LPOLESTR)L"x"));
        OK_(ICreateTypeInfo_LayOut(c));
    }
    ICreateTypeInfo_Release(c);
    return tl_reload(ctl, path);
}

/* Returns the IRecordInfo of "Person" (index 0), or NULL (with a skip note) when the type information does not describe
 * a 32 byte struct, in which case raw record memory tests would be unsafe. */
static int g_size_ok;
#define NEED_SIZE() do { if (!g_size_ok) { PERSON_END(); t_skip("record size not usable (Wine does not lay out records)"); return; } } while (0)
static IRecordInfo *person_ri(ITypeLib **tlp, ITypeInfo **tip, const char *leaf)
{
    WCHAR path[MAX_PATH];
    ITypeLib *tl = build_person(path, leaf);
    *tlp = tl;
    if (!tl) return NULL;
    ITypeInfo *ti = tl_ti(tl, 0);
    *tip = ti;
    if (!ti) return NULL;
    IRecordInfo *ri = NULL;
    OK_(pGetRecordInfoFromTypeInfo(ti, &ri));
    if (!ri) return NULL;
    ULONG sz = 0;
    OK_(IRecordInfo_GetSize(ri, &sz));
    g_size_ok = sz == sizeof(Person);
    if (!g_size_ok) {
        t_note("record size from type info = %lu, expected %u", sz, (unsigned)sizeof(Person));
        if (!t_is_wine()) { IRecordInfo_Release(ri); return NULL; }
    }
    return ri;
}

static void done(IRecordInfo *ri, ITypeInfo *ti, ITypeLib *tl)
{
    if (ri) IRecordInfo_Release(ri);
    if (ti) ITypeInfo_Release(ti);
    if (tl) ITypeLib_Release(tl);
}

#define PERSON_BEGIN(leaf) \
    ITypeLib *tl_ = NULL; ITypeInfo *ti_ = NULL; \
    IRecordInfo *ri = person_ri(&tl_, &ti_, leaf); \
    if (!ri) { done(NULL, ti_, tl_); t_skip("no usable record info"); return; }
#define PERSON_END() done(ri, ti_, tl_)

TEST(tlr_getrecordinfofromtypeinfo_args)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ITypeLib *tl = build_person(path, "rec_args.tlb");
    if (!tl) return;
    ITypeInfo *ti = tl_ti(tl, 0);
    IRecordInfo *ri = (IRecordInfo *)(ULONG_PTR)1;
    EQHR(pGetRecordInfoFromTypeInfo(NULL, &ri), E_INVALIDARG);
    EQHR(pGetRecordInfoFromTypeInfo(ti, NULL), E_INVALIDARG);
    EQHR(pGetRecordInfoFromTypeInfo(NULL, NULL), E_INVALIDARG);
    CHECK(ri == (IRecordInfo *)(ULONG_PTR)1);
    ri = NULL;
    OK_(pGetRecordInfoFromTypeInfo(ti, &ri));
    CHECK(ri != NULL);
    if (ri) IRecordInfo_Release(ri);
    if (ti) ITypeInfo_Release(ti);
    ITypeLib_Release(tl);
}

TEST(tlr_getrecordinfofromtypeinfo_wrong_kinds)
{
    TL_BEGIN();
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "rec_kinds.tlb", path);
    if (!ctl) return;
    static const TYPEKIND ks[] = { TKIND_ENUM, TKIND_INTERFACE, TKIND_DISPATCH, TKIND_COCLASS, TKIND_MODULE, TKIND_UNION, TKIND_RECORD };
    static const WCHAR *ns[] = { L"E", L"I", L"D", L"C", L"M", L"U", L"RNoGuid" };
    for (unsigned i = 0; i < 7; i++) {
        ICreateTypeInfo *c = tl_cti(ctl, ns[i], ks[i]);
        if (!c) continue;
        if (i < 6) {
            GUID g = TLC_GUID_E; g.Data4[7] = (BYTE)(0x40 + i);
            OK_(ICreateTypeInfo_SetGuid(c, &g)); /* has a guid, still not a record */
        }
        ICreateTypeInfo_Release(c);
    }
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    for (unsigned i = 0; i < 7; i++) {
        ITypeInfo *ti = tl_ti(tl, i);
        if (!ti) continue;
        IRecordInfo *ri = NULL;
        HRESULT hr = pGetRecordInfoFromTypeInfo(ti, &ri);
        CHECKF(hr == E_INVALIDARG, "type kind %d (%u): hr=0x%08lx", ks[i], i, (unsigned long)hr);
        if (hr == S_OK && ri) IRecordInfo_Release(ri);
        ITypeInfo_Release(ti);
    }
    ITypeLib_Release(tl);
}

TEST(tlr_getrecordinfofromguids_args)
{
    TL_BEGIN();
    IRecordInfo *ri = (IRecordInfo *)(ULONG_PTR)1;
    GUID g = TLC_GUID_C;
    EQHR(pGetRecordInfoFromGuids((GUID *)&TLC_GUID_LIB, 1, 0, 0, &g, NULL), E_INVALIDARG);
    EQHR(pGetRecordInfoFromGuids((GUID *)&GUID_NULL, 1, 0, 0, &g, &ri), E_INVALIDARG);
    EQHR(pGetRecordInfoFromGuids((GUID *)&TLC_GUID_LIB, 1, 0, 0, (GUID *)&GUID_NULL, &ri), E_INVALIDARG);
    EQHR(pGetRecordInfoFromGuids((GUID *)&GUID_NULL, 1, 0, 0, (GUID *)&GUID_NULL, &ri), E_INVALIDARG);
    CHECK(ri == (IRecordInfo *)(ULONG_PTR)1);
}

TEST(tlr_getrecordinfofromguids_unregistered_lib)
{
    TL_BEGIN();
    IRecordInfo *ri = (IRecordInfo *)(ULONG_PTR)1;
    GUID lib = TLC_GUID_LIB, rec = TLC_GUID_E;
    lib.Data1 ^= 0x5a5a5a5a; /* certainly not a registered library, and no live record info with this guid */
    HRESULT hr = pGetRecordInfoFromGuids(&lib, 1, 0, 0, &rec, &ri);
    t_note("GetRecordInfoFromGuids(unregistered) hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    SOFTHR(hr, TYPE_E_LIBNOTREGISTERED);
    CHECK(ri == NULL);
}

TEST(tlr_recordinfo_cache_by_guid)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_cache.tlb");
    /* decomp: the second request for a live type GUID returns the same object */
    IRecordInfo *ri2 = NULL;
    OK_(pGetRecordInfoFromTypeInfo(ti_, &ri2));
    CHECK(ri2 == ri);
    if (ri2) IRecordInfo_Release(ri2);
    /* GetRecordInfoFromGuids finds it via the table without touching the registry */
    IRecordInfo *ri3 = NULL;
    GUID rec = TLC_GUID_C;
    HRESULT hr = pGetRecordInfoFromGuids((GUID *)&TLC_GUID_LIB, 0, 0, 0, &rec, &ri3);
    EQHR(hr, S_OK);
    CHECK(ri3 == ri);
    if (ri3) IRecordInfo_Release(ri3);
    /* the lib guid is irrelevant while the cache entry is live */
    GUID otherlib = TLC_GUID_A;
    ri3 = NULL;
    hr = pGetRecordInfoFromGuids(&otherlib, 9, 9, 0x1234, &rec, &ri3);
    EQHR(hr, S_OK);
    CHECK(ri3 == ri);
    if (ri3) IRecordInfo_Release(ri3);
    PERSON_END();
}

TEST(tlr_recordinfo_refcounting_and_qi)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_ref.tlb");
    ULONG a = IRecordInfo_AddRef(ri);
    ULONG b = IRecordInfo_AddRef(ri);
    EQ(b, a + 1);
    EQ(IRecordInfo_Release(ri), a);
    EQ(IRecordInfo_Release(ri), a - 1);
    void *p = NULL;
    OK_(IRecordInfo_QueryInterface(ri, &IID_IUnknown, &p));
    if (p) IUnknown_Release((IUnknown *)p);
    p = NULL;
    OK_(IRecordInfo_QueryInterface(ri, &IID_IRecordInfo, &p));
    CHECK(p == (void *)ri);
    if (p) IUnknown_Release((IUnknown *)p);
    p = (void *)1;
    EQHR(IRecordInfo_QueryInterface(ri, &IID_ITypeInfo, &p), E_NOINTERFACE);
    CHECK(p == NULL);
    p = (void *)1;
    EQHR(IRecordInfo_QueryInterface(ri, &IID_IDispatch, &p), E_NOINTERFACE);
    PERSON_END();
}

TEST(tlr_recordinfo_identity_methods)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_ident.tlb");
    GUID g;
    OK_(IRecordInfo_GetGuid(ri, &g));
    CHECK(IsEqualGUID(&g, &TLC_GUID_C));
    BSTR nm = NULL;
    OK_(IRecordInfo_GetName(ri, &nm));
    CHECK(nm && !lstrcmpW(nm, L"Person"));
    pSysFreeString(nm);
    ULONG sz = 0;
    OK_(IRecordInfo_GetSize(ri, &sz));
    EQ(sz, sizeof(Person));
    ITypeInfo *rti = NULL;
    OK_(IRecordInfo_GetTypeInfo(ri, &rti));
    CHECK(rti != NULL);
    if (rti) {
        TYPEATTR *ta;
        OK_(ITypeInfo_GetTypeAttr(rti, &ta));
        if (ta) { EQ(ta->typekind, TKIND_RECORD); CHECK(IsEqualGUID(&ta->guid, &TLC_GUID_C)); ITypeInfo_ReleaseTypeAttr(rti, ta); }
        ITypeInfo_Release(rti);
    }
    PERSON_END();
}

TEST(tlr_recordinfo_names_protocol)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_names.tlb");
    ULONG n = 0;
    OK_(IRecordInfo_GetFieldNames(ri, &n, NULL));
    EQ(n, 4);
    BSTR names[6] = {0};
    n = 6;
    OK_(IRecordInfo_GetFieldNames(ri, &n, names));
    EQ(n, 4);
    static const WCHAR *want[4] = { L"Id", L"Name", L"Score", L"Tags" };
    for (UINT i = 0; i < 4 && i < n; i++) {
        CHECKF(names[i] && !lstrcmpW(names[i], want[i]), "field name %u", i);
        pSysFreeString(names[i]);
    }
    /* array smaller than the field count */
    BSTR two[2] = {0};
    n = 2;
    HRESULT hr = IRecordInfo_GetFieldNames(ri, &n, two);
    t_note("GetFieldNames with room for 2 of 4: hr=0x%08lx n=%lu", (unsigned long)hr, n);
    SOFTEQ(n, 2);
    for (int i = 0; i < 2; i++) pSysFreeString(two[i]);
    PERSON_END();
}

TEST(tlr_recordinfo_init_create_destroy)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_init.tlb");
    NEED_SIZE();
    Person p;
    memset(&p, 0xcd, sizeof p);
    OK_(IRecordInfo_RecordInit(ri, &p));
    CHECK(p.Id == 0 && p.Name == NULL && p.Score == 0.0 && p.Tags == NULL);
    Person *c = IRecordInfo_RecordCreate(ri);
    CHECK(c != NULL);
    if (c) {
        CHECK(c->Id == 0 && c->Name == NULL && c->Score == 0.0 && c->Tags == NULL);
        c->Id = 5;
        c->Name = pSysAllocString(L"created");
        OK_(IRecordInfo_RecordClear(ri, c));
        OK_(IRecordInfo_RecordDestroy(ri, c));
    }
    PERSON_END();
}

TEST(tlr_recordinfo_putget_fields)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_fields.tlb");
    Person p; memset(&p, 0, sizeof p);
    VARIANT v; pVariantInit(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 4711;
    OK_(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Id", &v));
    EQ(p.Id, 4711);
    V_VT(&v) = VT_R8; V_R8(&v) = 3.5;
    OK_(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Score", &v));
    CHECK(p.Score == 3.5);
    BSTR src = pSysAllocString(L"Ada Lovelace");
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = src;
    OK_(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Name", &v));
    CHECK(p.Name != NULL);
    CHECK(p.Name != src); /* the record owns its own copy */
    CHECK(p.Name && !lstrcmpW(p.Name, L"Ada Lovelace"));
    pSysFreeString(src);
    /* the field is matched case-insensitively? */
    V_VT(&v) = VT_I4; V_I4(&v) = 1;
    HRESULT hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"id", &v);
    t_note("PutField with lower-case field name hr=0x%08lx", (unsigned long)hr);
    /* reads */
    VARIANT r; pVariantInit(&r);
    OK_(IRecordInfo_GetField(ri, &p, L"Id", &r));
    EQ(V_VT(&r), VT_I4); EQ(V_I4(&r), hr == S_OK ? 1 : 4711);
    pVariantClear(&r);
    OK_(IRecordInfo_GetField(ri, &p, L"Score", &r));
    EQ(V_VT(&r), VT_R8); CHECK(V_R8(&r) == 3.5);
    pVariantClear(&r);
    OK_(IRecordInfo_GetField(ri, &p, L"Name", &r));
    EQ(V_VT(&r), VT_BSTR);
    CHECK(V_VT(&r) == VT_BSTR && V_BSTR(&r) && !lstrcmpW(V_BSTR(&r), L"Ada Lovelace"));
    CHECK(V_VT(&r) != VT_BSTR || V_BSTR(&r) != p.Name); /* GetField copies */
    pVariantClear(&r);
    /* replacing a BSTR frees the old and installs a copy */
    BSTR old = p.Name;
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = pSysAllocString(L"Grace");
    OK_(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Name", &v));
    CHECK(p.Name && !lstrcmpW(p.Name, L"Grace"));
    (void)old;
    pVariantClear(&v);
    /* coercion on put */
    V_VT(&v) = VT_I2; V_I2(&v) = 12;
    hr = IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Id", &v);
    t_note("PutField(VT_I2 into LONG) hr=0x%08lx Id=%ld", (unsigned long)hr, (long)p.Id);
    if (hr == S_OK) EQ(p.Id, 12);
    OK_(IRecordInfo_RecordClear(ri, &p));
    PERSON_END();
}

TEST(tlr_recordinfo_field_errors)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_ferr.tlb");
    Person p; memset(&p, 0, sizeof p);
    VARIANT v, r; pVariantInit(&v); pVariantInit(&r);
    V_VT(&v) = VT_I4;
    EQHR(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"NoSuchField", &v), TYPE_E_FIELDNOTFOUND);
    EQHR(IRecordInfo_GetField(ri, &p, L"NoSuchField", &r), TYPE_E_FIELDNOTFOUND);
    EQ(V_VT(&r), VT_EMPTY);
    SOFTHR(IRecordInfo_PutField(ri, INVOKE_FUNC, &p, L"Id", &v), E_INVALIDARG);
    SOFTHR(IRecordInfo_PutField(ri, 0, &p, L"Id", &v), E_INVALIDARG);
    if (!t_is_wine()) {
        SOFTHR(IRecordInfo_GetField(ri, NULL, L"Id", &r), E_INVALIDARG);
        SOFTHR(IRecordInfo_GetField(ri, &p, NULL, &r), E_INVALIDARG);
    }
    PERSON_END();
}

TEST(tlr_recordinfo_nocopy_access)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_nocopy.tlb");
    Person p; memset(&p, 0, sizeof p);
    p.Id = 77; p.Score = 1.25;
    p.Name = pSysAllocString(L"nocopy");
    VARIANT r; pVariantInit(&r);
    PVOID carr = (PVOID)(ULONG_PTR)1;
    HRESULT hr = IRecordInfo_GetFieldNoCopy(ri, &p, L"Id", &r, &carr);
    EQHR(hr, S_OK);
    EQ(V_VT(&r), VT_I4 | VT_BYREF);
    CHECK(V_I4REF(&r) == &p.Id);
    pVariantInit(&r);
    OK_(IRecordInfo_GetFieldNoCopy(ri, &p, L"Name", &r, &carr));
    EQ(V_VT(&r), VT_BSTR | VT_BYREF);
    CHECK(V_BSTRREF(&r) == &p.Name);
    pVariantInit(&r);
    OK_(IRecordInfo_GetFieldNoCopy(ri, &p, L"Score", &r, &carr));
    EQ(V_VT(&r), VT_R8 | VT_BYREF);
    CHECK(V_R8REF(&r) == &p.Score);
    /* PutFieldNoCopy stores the value as is (takes ownership of the BSTR) */
    BSTR repl = pSysAllocString(L"replacement");
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = repl;
    pSysFreeString(p.Name); p.Name = NULL;
    hr = IRecordInfo_PutFieldNoCopy(ri, INVOKE_PROPERTYPUT, &p, L"Name", &v);
    EQHR(hr, S_OK);
    CHECK(p.Name == repl);
    EQHR(IRecordInfo_GetFieldNoCopy(ri, &p, L"Bogus", &r, &carr), TYPE_E_FIELDNOTFOUND);
    OK_(IRecordInfo_RecordClear(ri, &p));
    PERSON_END();
}

TEST(tlr_recordinfo_safearray_field)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_safe.tlb");
    Person p; memset(&p, 0, sizeof p);
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *sa = pSafeArrayCreate(VT_I4, 1, &b);
    CHECK(sa != NULL);
    if (sa) {
        LONG *d = NULL;
        OK_(pSafeArrayAccessData(sa, (void **)&d));
        if (d) { d[0] = 10; d[1] = 20; d[2] = 30; }
        pSafeArrayUnaccessData(sa);
        VARIANT v; pVariantInit(&v);
        V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = sa;
        OK_(IRecordInfo_PutField(ri, INVOKE_PROPERTYPUT, &p, L"Tags", &v));
        CHECK(p.Tags != NULL);
        CHECK(p.Tags != sa); /* copied */
        pSafeArrayDestroy(sa);
        VARIANT r; pVariantInit(&r);
        OK_(IRecordInfo_GetField(ri, &p, L"Tags", &r));
        EQ(V_VT(&r), VT_ARRAY | VT_I4);
        if (V_VT(&r) == (VT_ARRAY | VT_I4) && V_ARRAY(&r)) {
            CHECK(V_ARRAY(&r) != p.Tags);
            LONG idx = 1, val = 0;
            OK_(pSafeArrayGetElement(V_ARRAY(&r), &idx, &val));
            EQ(val, 20);
        }
        pVariantClear(&r);
    }
    OK_(IRecordInfo_RecordClear(ri, &p));
    PERSON_END();
}

TEST(tlr_recordinfo_copy_is_deep)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_copy.tlb");
    NEED_SIZE();
    Person a, b; memset(&a, 0, sizeof a);
    a.Id = 3; a.Score = 9.5; a.Name = pSysAllocString(L"original");
    SAFEARRAYBOUND bd = { 2, 0 };
    a.Tags = pSafeArrayCreate(VT_I4, 1, &bd);
    OK_(IRecordInfo_RecordInit(ri, &b));
    OK_(IRecordInfo_RecordCopy(ri, &a, &b));
    EQ(b.Id, 3);
    CHECK(b.Score == 9.5);
    CHECK(b.Name && !lstrcmpW(b.Name, L"original"));
    CHECK(b.Name != a.Name);
    CHECK(b.Tags != NULL && b.Tags != a.Tags);
    /* modifying the copy leaves the source untouched */
    pSysFreeString(b.Name); b.Name = pSysAllocString(L"changed");
    CHECK(!lstrcmpW(a.Name, L"original"));
    OK_(IRecordInfo_RecordClear(ri, &b));
    OK_(IRecordInfo_RecordClear(ri, &a));
    /* RecordCreateCopy allocates the destination */
    Person s; memset(&s, 0, sizeof s);
    s.Id = 8; s.Name = pSysAllocString(L"src");
    PVOID dst = NULL;
    OK_(IRecordInfo_RecordCreateCopy(ri, &s, &dst));
    CHECK(dst != NULL && dst != (PVOID)&s);
    if (dst) {
        Person *d = dst;
        EQ(d->Id, 8);
        CHECK(d->Name && d->Name != s.Name && !lstrcmpW(d->Name, L"src"));
        OK_(IRecordInfo_RecordClear(ri, dst));
        OK_(IRecordInfo_RecordDestroy(ri, dst));
    }
    OK_(IRecordInfo_RecordClear(ri, &s));
    PERSON_END();
}

TEST(tlr_recordinfo_ismatchingtype)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_match.tlb");
    CHECK(IRecordInfo_IsMatchingType(ri, ri));
    IRecordInfo *other = NULL;
    ITypeInfo *to = tl_ti(tl_, 1);
    if (to) {
        OK_(pGetRecordInfoFromTypeInfo(to, &other));
        if (other) {
            CHECK(other != ri);
            CHECK(!IRecordInfo_IsMatchingType(ri, other));
            CHECK(!IRecordInfo_IsMatchingType(other, ri));
            GUID g;
            OK_(IRecordInfo_GetGuid(other, &g));
            CHECK(IsEqualGUID(&g, &TLC_GUID_D));
            ULONG sz = 0;
            OK_(IRecordInfo_GetSize(other, &sz));
            SOFTEQ(sz, 4);
            IRecordInfo_Release(other);
        }
        ITypeInfo_Release(to);
    }
    PERSON_END();
}

TEST(tlr_variant_record_copy_clear)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_var.tlb");
    NEED_SIZE();
    Person *rec = IRecordInfo_RecordCreate(ri);
    CHECK(rec != NULL);
    if (rec) {
        rec->Id = 55;
        rec->Name = pSysAllocString(L"in a variant");
        ULONG base;
        IRecordInfo_AddRef(ri); base = IRecordInfo_Release(ri);
        VARIANT src, dst; pVariantInit(&src); pVariantInit(&dst);
        V_VT(&src) = VT_RECORD;
        V_RECORD(&src) = rec;
        V_RECORDINFO(&src) = ri;
        IRecordInfo_AddRef(ri); /* the variant's own reference */
        HRESULT hr = pVariantCopy(&dst, &src);
        EQHR(hr, S_OK);
        EQ(V_VT(&dst), VT_RECORD);
        CHECK(V_RECORDINFO(&dst) == ri);
        CHECK(V_RECORD(&dst) != NULL);
        CHECK(V_RECORD(&dst) != V_RECORD(&src));
        if (V_RECORD(&dst) && V_RECORD(&dst) != rec) {
            Person *c = V_RECORD(&dst);
            EQ(c->Id, 55);
            CHECK(c->Name && c->Name != rec->Name && !lstrcmpW(c->Name, L"in a variant"));
        }
        ULONG with_copy;
        IRecordInfo_AddRef(ri); with_copy = IRecordInfo_Release(ri);
        t_note("record info refs: base=%lu, with source+copy=%lu", base, with_copy);
        SOFTEQ(with_copy, base + 2);
        hr = pVariantClear(&dst);
        EQHR(hr, S_OK);
        EQ(V_VT(&dst), VT_EMPTY);
        ULONG after;
        IRecordInfo_AddRef(ri); after = IRecordInfo_Release(ri);
        SOFTEQ(after, base + 1);
        /* the source variant owns `rec`; clear it (releases ri) but do not touch rec afterwards */
        hr = pVariantClear(&src);
        EQHR(hr, S_OK);
        IRecordInfo_AddRef(ri); after = IRecordInfo_Release(ri);
        SOFTEQ(after, base);
    }
    PERSON_END();
}

TEST(tlr_variant_record_byref_copy)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_varref.tlb");
    Person p; memset(&p, 0, sizeof p);
    p.Id = 1;
    VARIANT v, d; pVariantInit(&v); pVariantInit(&d);
    V_VT(&v) = VT_RECORD | VT_BYREF; /* by-ref records are not valid variants */
    V_BYREF(&v) = &p;
    HRESULT hr = pVariantCopy(&d, &v);
    t_note("VariantCopy(VT_RECORD|VT_BYREF) hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    pVariantClear(&d);
    PERSON_END();
}

TEST(tlr_variant_record_without_recordinfo)
{
    TL_BEGIN();
    Person p; memset(&p, 0, sizeof p);
    VARIANT v, d; pVariantInit(&v); pVariantInit(&d);
    V_VT(&v) = VT_RECORD;
    V_RECORD(&v) = NULL;
    V_RECORDINFO(&v) = NULL;
    HRESULT hr = pVariantCopy(&d, &v);
    t_note("VariantCopy(VT_RECORD, NULL, NULL) hr=0x%08lx", (unsigned long)hr);
    if (hr == S_OK) { EQ(V_VT(&d), VT_RECORD); CHECK(V_RECORD(&d) == NULL && V_RECORDINFO(&d) == NULL); }
    pVariantClear(&d);
    V_VT(&v) = VT_EMPTY;
    pVariantClear(&v);
}

TEST(tlr_safearray_of_records)
{
    TL_BEGIN();
    if (!pSafeArrayCreateEx || !pSafeArrayGetRecordInfo || !pSafeArrayGetVartype) { t_skip("safearray exports missing"); return; }
    PERSON_BEGIN("rec_sa.tlb");
    NEED_SIZE();
    SAFEARRAYBOUND b = { 3, 1 };
    SAFEARRAY *sa = pSafeArrayCreateEx(VT_RECORD, 1, &b, ri);
    CHECK(sa != NULL);
    if (sa) {
        EQ(sa->cbElements, sizeof(Person));
        EQ(sa->cDims, 1);
        EQ(sa->fFeatures & FADF_RECORD, FADF_RECORD);
        VARTYPE vt = 0;
        OK_(pSafeArrayGetVartype(sa, &vt));
        EQ(vt, VT_RECORD);
        IRecordInfo *got = NULL;
        OK_(pSafeArrayGetRecordInfo(sa, &got));
        CHECK(got == ri);
        if (got) IRecordInfo_Release(got);
        Person *d = NULL;
        OK_(pSafeArrayAccessData(sa, (void **)&d));
        if (d) {
            for (int i = 0; i < 3; i++) CHECK(d[i].Id == 0 && d[i].Name == NULL && d[i].Tags == NULL);
            for (int i = 0; i < 3; i++) { d[i].Id = 100 + i; }
            d[1].Name = pSysAllocString(L"element one");
            pSafeArrayUnaccessData(sa);
        }
        /* PutElement copies the record, GetElement copies it out */
        Person src; memset(&src, 0, sizeof src);
        src.Id = 999; src.Name = pSysAllocString(L"put element");
        LONG idx = 3;
        HRESULT hr = pSafeArrayPutElement(sa, &idx, &src);
        EQHR(hr, S_OK);
        Person out; memset(&out, 0, sizeof out);
        hr = pSafeArrayGetElement(sa, &idx, &out);
        EQHR(hr, S_OK);
        if (hr == S_OK) {
            EQ(out.Id, 999);
            CHECK(out.Name && out.Name != src.Name && !lstrcmpW(out.Name, L"put element"));
            IRecordInfo_RecordClear(ri, &out);
        }
        IRecordInfo_RecordClear(ri, &src);
        idx = 4;
        EQHR(pSafeArrayGetElement(sa, &idx, &out), DISP_E_BADINDEX);
        /* SafeArrayCopy deep-copies strings */
        SAFEARRAY *cp = NULL;
        hr = pSafeArrayCopy(sa, &cp);
        EQHR(hr, S_OK);
        if (cp) {
            Person *d2 = NULL, *d1 = NULL;
            pSafeArrayAccessData(cp, (void **)&d2);
            pSafeArrayAccessData(sa, (void **)&d1);
            if (d1 && d2) {
                EQ(d2[0].Id, 100);
                CHECK(d2[1].Name && d1[1].Name && d2[1].Name != d1[1].Name && !lstrcmpW(d2[1].Name, L"element one"));
                EQ(d2[2].Id, 102);
            }
            pSafeArrayUnaccessData(sa);
            pSafeArrayUnaccessData(cp);
            IRecordInfo *g2 = NULL;
            OK_(pSafeArrayGetRecordInfo(cp, &g2));
            CHECK(g2 == ri);
            if (g2) IRecordInfo_Release(g2);
            EQHR(pSafeArrayDestroy(cp), S_OK);
        }
        EQHR(pSafeArrayDestroy(sa), S_OK);
    }
    PERSON_END();
}

TEST(tlr_safearray_record_requires_recordinfo)
{
    TL_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *sa = pSafeArrayCreateEx(VT_RECORD, 1, &b, NULL);
    t_note("SafeArrayCreateEx(VT_RECORD, NULL recinfo) = %p", (void *)sa);
    CHECK(sa == NULL);
    if (sa) pSafeArrayDestroy(sa);
    sa = pSafeArrayCreate(VT_RECORD, 1, &b);
    CHECK(sa == NULL);
    if (sa) pSafeArrayDestroy(sa);
}

TEST(tlr_safearray_setrecordinfo_on_non_record)
{
    TL_BEGIN();
    PERSON_BEGIN("rec_sasri.tlb");
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *sa = pSafeArrayCreate(VT_I4, 1, &b);
    if (sa) {
        HRESULT hr = pSafeArraySetRecordInfo(sa, ri);
        t_note("SafeArraySetRecordInfo on a VT_I4 array hr=0x%08lx", (unsigned long)hr);
        IRecordInfo *got = (IRecordInfo *)(ULONG_PTR)1;
        hr = pSafeArrayGetRecordInfo(sa, &got);
        t_note("SafeArrayGetRecordInfo on a VT_I4 array hr=0x%08lx", (unsigned long)hr);
        if (hr == S_OK && got && got != (IRecordInfo *)(ULONG_PTR)1) IRecordInfo_Release(got);
        pSafeArrayDestroy(sa);
    }
    PERSON_END();
}

TEST(tlr_record_alias_userdefined)
{
    TL_BEGIN();
    /* decomp: an alias whose tdescAlias is VT_USERDEFINED (pointing to a record) is accepted by GetRecordInfoFromTypeInfo */
    WCHAR path[MAX_PATH];
    ICreateTypeLib2 *ctl = tl_create(SYS_WIN64, "rec_alias.tlb", path);
    if (!ctl) return;
    ICreateTypeInfo *rec = tl_cti(ctl, L"Rec", TKIND_RECORD);
    OK_(ICreateTypeInfo_SetGuid(rec, &TLC_GUID_C));
    VARDESC vd; memset(&vd, 0, sizeof vd);
    vd.elemdescVar.tdesc = td_i4r; vd.varkind = VAR_PERINSTANCE;
    OK_(ICreateTypeInfo_AddVarDesc(rec, 0, &vd));
    OK_(ICreateTypeInfo_SetVarName(rec, 0, (LPOLESTR)L"v"));
    OK_(ICreateTypeInfo_LayOut(rec));
    ITypeInfo *trec = tl_ti_of_cti(rec);
    ICreateTypeInfo *al = tl_cti(ctl, L"RecAlias", TKIND_ALIAS);
    HREFTYPE h;
    OK_(ICreateTypeInfo_AddRefTypeInfo(al, trec, &h));
    TYPEDESC ud; ud.vt = VT_USERDEFINED; ud.hreftype = h;
    OK_(ICreateTypeInfo_SetTypeDescAlias(al, &ud));
    OK_(ICreateTypeInfo_SetGuid(al, &TLC_GUID_E));
    ITypeInfo_Release(trec);
    ICreateTypeInfo_Release(al);
    ICreateTypeInfo_Release(rec);
    ITypeLib *tl = tl_reload(ctl, path);
    if (!tl) return;
    ITypeInfo *ta_ = tl_ti(tl, 1);
    if (ta_) {
        IRecordInfo *ri = NULL;
        HRESULT hr = pGetRecordInfoFromTypeInfo(ta_, &ri);
        t_note("GetRecordInfoFromTypeInfo(alias to record) hr=0x%08lx", (unsigned long)hr);
        SOFTHR(hr, S_OK);
        if (ri) {
            GUID g;
            if (IRecordInfo_GetGuid(ri, &g) == S_OK) t_note("alias record info guid ends with %02x", g.Data4[7]);
            BSTR nm = NULL;
            if (IRecordInfo_GetName(ri, &nm) == S_OK && nm) { char b[32]; tl_w2a(nm, b, sizeof b); t_note("alias record info name %s", b); }
            pSysFreeString(nm);
            IRecordInfo_Release(ri);
        }
        ITypeInfo_Release(ta_);
    }
    ITypeLib_Release(tl);
}
