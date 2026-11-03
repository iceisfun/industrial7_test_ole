/* combase RoGetParameterizedTypeInstanceIID / RoParameterizedTypeExtraGetTypeSignature / RoFreeParameterizedTypeExtra.
 * A mock IRoMetaDataLocator feeds the DLL's SimpleMetaDataBuilder, and the expected IIDs are derived independently
 * from the WinRT type signature (UUID v5 over SHA-1, namespace 11f47ad5-7b73-42c0-abae-878b1e16adee). */
#include "gap8.h"

typedef struct IRSB IRSB;
struct IRSBVtbl {                                        /* IRoSimpleMetaDataBuilder (no IUnknown part) */
    HRESULT (__cdecl *SetWinRtInterface)(IRSB *, GUID);
    HRESULT (__cdecl *SetDelegate)(IRSB *, GUID);
    HRESULT (__cdecl *SetInterfaceGroupSimpleDefault)(IRSB *, PCWSTR, PCWSTR, const GUID *);
    HRESULT (__cdecl *SetInterfaceGroupParameterizedDefault)(IRSB *, PCWSTR, UINT32, PCWSTR *);
    HRESULT (__cdecl *SetRuntimeClassSimpleDefault)(IRSB *, PCWSTR, PCWSTR, const GUID *);
    HRESULT (__cdecl *SetRuntimeClassParameterizedDefault)(IRSB *, PCWSTR, UINT32, const PCWSTR *);
    HRESULT (__cdecl *SetStruct)(IRSB *, PCWSTR, UINT32, PCWSTR *);
    HRESULT (__cdecl *SetEnum)(IRSB *, PCWSTR, PCWSTR);
    HRESULT (__cdecl *SetParameterizedInterface)(IRSB *, GUID, UINT32);
    HRESULT (__cdecl *SetParameterizedDelegate)(IRSB *, GUID, UINT32);
};
struct IRSB { const struct IRSBVtbl *v; };

typedef struct IRML IRML;
struct IRMLVtbl { HRESULT (__cdecl *Locate)(IRML *, PCWSTR, IRSB *); };
struct IRML { const struct IRMLVtbl *v; };

typedef struct { const struct IRMLVtbl *v; int calls; WCHAR names[8][96]; HRESULT fail; int nogenerics; } LOC;

typedef HRESULT (WINAPI *get_iid_fn)(UINT32, PCWSTR *, IRML *, GUID *, void **);
typedef const char *(WINAPI *get_sig_fn)(void *);
typedef void (WINAPI *free_fn)(void *);
UDECL(HRESULT, RoGetParameterizedTypeInstanceIID, (UINT32, PCWSTR *, IRML *, GUID *, void **));
UDECL(const char *, RoParameterizedTypeExtraGetTypeSignature, (void *));
UDECL(void, RoFreeParameterizedTypeExtra, (void *));

static const GUID PIID_IVector     = { 0x913337e9, 0x11a1, 0x4345, { 0xa3,0xa2,0x4e,0x7f,0x95,0x6e,0x22,0x2d } };
static const GUID PIID_IVectorView = { 0xbbe1fa4c, 0xb0e3, 0x4583, { 0xba,0xef,0x1f,0x1b,0x2e,0x48,0x3e,0x56 } };
static const GUID PIID_IIterable   = { 0xfaa585ea, 0x6214, 0x4217, { 0xaf,0xda,0x7f,0x46,0xde,0x58,0x69,0xb3 } };
static const GUID PIID_IKeyValuePair = { 0x02b51929, 0xc1c4, 0x4a7e, { 0x89,0x40,0x03,0x12,0xb5,0xc1,0x85,0x00 } };
static const GUID IID_IUriRuntimeClass_ = { 0x9e365e57, 0x48b2, 0x4160, { 0x95,0x6f,0xc7,0x38,0x51,0x20,0xbb,0xfc } };

static HRESULT __cdecl loc_locate(IRML *self, PCWSTR name, IRSB *b)
{
    LOC *l = (LOC *)self;
    if (l->calls < 8) lstrcpynW(l->names[l->calls], name, 96);
    l->calls++;
    if (l->fail) return l->fail;
    if (!lstrcmpW(name, L"Windows.Foundation.Collections.IVector`1")) return b->v->SetParameterizedInterface(b, PIID_IVector, 1);
    if (!lstrcmpW(name, L"Windows.Foundation.Collections.IVectorView`1")) return b->v->SetParameterizedInterface(b, PIID_IVectorView, 1);
    if (!lstrcmpW(name, L"Windows.Foundation.Collections.IIterable`1")) return b->v->SetParameterizedInterface(b, PIID_IIterable, 1);
    if (!lstrcmpW(name, L"Windows.Foundation.Collections.IKeyValuePair`2")) return b->v->SetParameterizedInterface(b, PIID_IKeyValuePair, 2);
    if (!lstrcmpW(name, L"Windows.Foundation.Uri"))
        return b->v->SetRuntimeClassSimpleDefault(b, name, L"Windows.Foundation.IUriRuntimeClass", &IID_IUriRuntimeClass_);
    return (HRESULT)0x80070490;     /* ERROR_NOT_FOUND-ish: unknown type */
}
static const struct IRMLVtbl loc_vtbl = { loc_locate };
static void loc_init(LOC *l) { memset(l, 0, sizeof *l); l->v = &loc_vtbl; }

static void expect_sig_iid(const char *sig, const GUID *got, const char *what)
{
    GUID want; char a[48], b[48];
    g8_signature_iid(sig, &want);
    g8_guid_str(&want, a); g8_guid_str(got, b);
    CHECKF(!memcmp(&want, got, sizeof want), "%s: signature \"%s\" -> expected %s got %s", what, sig, a, b);
}

#define NEEDP() do { G8_REQ_NW(UN(RoGetParameterizedTypeInstanceIID)); } while (0)

/* run one instantiation and verify IID + reported signature */
static void inst(UINT32 n, PCWSTR *names, const char *sig, int expect_locator_calls)
{
    LOC l; GUID iid; void *extra = (void *)(ULONG_PTR)0x1; HRESULT hr; const char *s;
    memset(&iid, 0xaa, sizeof iid); loc_init(&l);
    hr = UN(RoGetParameterizedTypeInstanceIID)(n, names, (IRML *)&l, &iid, &extra);
    G8_EQ(hr, S_OK);
    if (hr != S_OK) return;
    expect_sig_iid(sig, &iid, "iid");
    CHECK(extra != NULL && extra != (void *)(ULONG_PTR)1);
    if (expect_locator_calls >= 0) G8_EQ(l.calls, expect_locator_calls);
    if (UN(RoParameterizedTypeExtraGetTypeSignature) && extra) {
        s = UN(RoParameterizedTypeExtraGetTypeSignature)(extra);
        CHECK(s != NULL);
        if (s) { CHECKF(!strcmp(s, sig), "type signature \"%s\" want \"%s\"", s, sig); }
    }
    if (UN(RoFreeParameterizedTypeExtra) && extra) UN(RoFreeParameterizedTypeExtra)(extra);
}

/* adapted from the documented example in the Windows SDK (roparameterizediid.h); known public IIDs */
TEST(gap8_ro_param_ivector_string_known_iid)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"String" };
    LOC l; GUID iid, want = { 0x98b9acc1, 0x4b56, 0x532e, { 0xac,0x73,0x03,0xd5,0x29,0x1c,0xca,0x90 } };
    NEEDP(); loc_init(&l);
    G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &iid, NULL), S_OK);
    CHECK(!memcmp(&iid, &want, sizeof iid));                /* IVector<String> */
    G8_EQ(l.calls, 1);                                      /* "String" is a builtin: only the generic is looked up */
    CHECK(!lstrcmpW(l.names[0], n[0]));
}

TEST(gap8_ro_param_known_string_instances)
{
    static const struct { const WCHAR *gen; const char *sig; GUID iid; } t[] = {
        { L"Windows.Foundation.Collections.IVectorView`1", "pinterface({bbe1fa4c-b0e3-4583-baef-1f1b2e483e56};string)",
          { 0x2f13c006, 0xa03a, 0x5f69, { 0xb0,0x90,0x75,0xa4,0x3e,0x33,0x42,0x3e } } },
        { L"Windows.Foundation.Collections.IIterable`1", "pinterface({faa585ea-6214-4217-afda-7f46de5869b3};string)",
          { 0xe2fcc7c1, 0x3bfc, 0x5a0b, { 0xb2,0xb0,0x72,0xe7,0x69,0xd1,0xcb,0x7e } } },
    };
    unsigned i; NEEDP();
    for (i = 0; i < 2; i++) {
        PCWSTR n[2]; LOC l; GUID iid; n[0] = t[i].gen; n[1] = L"String"; loc_init(&l);
        G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &iid, NULL), S_OK);
        CHECK(!memcmp(&iid, &t[i].iid, sizeof iid));
        inst(2, n, t[i].sig, 1);
    }
}

TEST(gap8_ro_param_signature_matches_extra)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"String" };
    NEEDP(); G8_REQ_NW(UN(RoParameterizedTypeExtraGetTypeSignature));
    inst(2, n, "pinterface({913337e9-11a1-4345-a3a2-4e7f956e222d};string)", 1);
}

TEST(gap8_ro_param_builtin_type_name_table)
{
    /* WinRT builtin element names -> signature atoms (WinRT type system spec), as the DLL's builtin table is used */
    static const struct { const WCHAR *name; const char *atom; } t[] = {
        { L"Int32", "i4" }, { L"UInt32", "u4" }, { L"Int64", "i8" }, { L"UInt64", "u8" }, { L"Int16", "i2" }, { L"UInt16", "u2" },
        { L"Single", "f4" }, { L"Double", "f8" }, { L"Boolean", "b1" }, { L"Char16", "c2" }, { L"String", "string" },
        { L"Guid", "g16" }, { L"UInt8", "u1" },
    };
    unsigned i; NEEDP();
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        PCWSTR n[2]; LOC l; GUID iid; char sig[160]; HRESULT hr;
        n[0] = L"Windows.Foundation.Collections.IVector`1"; n[1] = t[i].name; loc_init(&l);
        hr = UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &iid, NULL);
        if (hr != S_OK) { t_note("builtin %ls -> hr 0x%lx (locator calls %d)", t[i].name, hr, l.calls); continue; }
        sprintf(sig, "pinterface({913337e9-11a1-4345-a3a2-4e7f956e222d};%s)", t[i].atom);
        expect_sig_iid(sig, &iid, "builtin");
        if (l.calls != 1) t_note("%ls was not a builtin (locator saw %d names)", t[i].name, l.calls);
    }
}

TEST(gap8_ro_param_two_arguments_keyvaluepair)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IKeyValuePair`2", L"String", L"Int32" };
    NEEDP();
    inst(3, n, "pinterface({02b51929-c1c4-4a7e-8940-0312b5c18500};string;i4)", 1);
}

TEST(gap8_ro_param_nested_generic)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"Windows.Foundation.Collections.IVectorView`1", L"String" };
    NEEDP();
    inst(3, n, "pinterface({913337e9-11a1-4345-a3a2-4e7f956e222d};pinterface({bbe1fa4c-b0e3-4583-baef-1f1b2e483e56};string))", 2);
}

TEST(gap8_ro_param_runtime_class_argument)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"Windows.Foundation.Uri" };
    NEEDP();
    inst(2, n, "pinterface({913337e9-11a1-4345-a3a2-4e7f956e222d};rc(Windows.Foundation.Uri;{9e365e57-48b2-4160-956f-c7385120bbfc}))", 2);
}

TEST(gap8_ro_param_distinct_arguments_distinct_iids)
{
    PCWSTR a[] = { L"Windows.Foundation.Collections.IVector`1", L"String" }, b[] = { L"Windows.Foundation.Collections.IVector`1", L"Int32" },
           c[] = { L"Windows.Foundation.Collections.IVectorView`1", L"String" };
    GUID ga, gb, gc, ga2; LOC l; NEEDP();
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, a, (IRML *)&l, &ga, NULL), S_OK);
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, b, (IRML *)&l, &gb, NULL), S_OK);
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, c, (IRML *)&l, &gc, NULL), S_OK);
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, a, (IRML *)&l, &ga2, NULL), S_OK);
    CHECK(memcmp(&ga, &gb, sizeof ga)); CHECK(memcmp(&ga, &gc, sizeof ga)); CHECK(memcmp(&gb, &gc, sizeof ga));
    CHECK(!memcmp(&ga, &ga2, sizeof ga));                   /* deterministic */
    G8_EQ(ga.Data3 >> 12, 5);                               /* version 5 UUID */
    G8_EQ(ga.Data4[0] >> 6, 2);                             /* RFC 4122 variant */
}

TEST(gap8_ro_param_zero_arguments_is_unexpected)
{
    GUID iid; void *extra = (void *)(ULONG_PTR)1; HRESULT hr; LOC l; NEEDP();
    memset(&iid, 0xaa, sizeof iid); loc_init(&l);
    hr = UN(RoGetParameterizedTypeInstanceIID)(0, NULL, (IRML *)&l, &iid, &extra);
    G8_EQ(hr, G8_E_UNEXPECTED);
    { GUID z; memset(&z, 0, sizeof z); CHECK(!memcmp(&iid, &z, sizeof z)); }   /* IID is zeroed before anything else */
    G8_EQ(l.calls, 0);
    CHECK(extra != (void *)(ULONG_PTR)1);                    /* the extra handle is produced even though the call failed */
    if (extra != (void *)(ULONG_PTR)1 && extra && UN(RoFreeParameterizedTypeExtra)) UN(RoFreeParameterizedTypeExtra)(extra);
    /* same without an extra handle */
    memset(&iid, 0xaa, sizeof iid);
    G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(0, NULL, (IRML *)&l, &iid, NULL), G8_E_UNEXPECTED);
    { GUID z; memset(&z, 0, sizeof z); CHECK(!memcmp(&iid, &z, sizeof z)); }
}

TEST(gap8_ro_param_locator_failure_propagates)
{
    static const HRESULT codes[] = { G8_E_FAIL, (HRESULT)0x80070490, G8_E_ACCESSDENIED, (HRESULT)0x80004001 };
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"Windows.Foundation.Uri" };
    unsigned i; NEEDP();
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        LOC l; GUID iid; HRESULT hr; GUID z; memset(&z, 0, sizeof z); memset(&iid, 0xaa, sizeof iid);
        loc_init(&l); l.fail = codes[i];
        hr = UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &iid, NULL);
        G8_EQ(hr, codes[i]);
        G8_EQ(l.calls, 1);                                  /* stops at the first failing lookup */
        CHECK(!memcmp(&iid, &z, sizeof z));
    }
}

TEST(gap8_ro_param_unknown_type_name_asks_locator)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"Olet.Gap8.NoSuchType" };
    LOC l; GUID iid; HRESULT hr; NEEDP(); loc_init(&l);
    hr = UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &iid, NULL);
    G8_EQ(hr, (HRESULT)0x80070490);                         /* the locator's own failure comes back unchanged */
    G8_EQ(l.calls, 2);
    CHECK(!lstrcmpW(l.names[0], n[0])); CHECK(!lstrcmpW(l.names[1], n[1]));
}

TEST(gap8_ro_param_extra_handle_is_independent_per_call)
{
    PCWSTR a[] = { L"Windows.Foundation.Collections.IVector`1", L"String" }, b[] = { L"Windows.Foundation.Collections.IIterable`1", L"Int32" };
    LOC l; GUID g; void *e1 = NULL, *e2 = NULL; const char *s1, *s2;
    NEEDP(); G8_REQ_NW(UN(RoParameterizedTypeExtraGetTypeSignature)); REQUIRE(UN(RoFreeParameterizedTypeExtra));
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, a, (IRML *)&l, &g, &e1), S_OK);
    loc_init(&l); G8_EQ(UN(RoGetParameterizedTypeInstanceIID)(2, b, (IRML *)&l, &g, &e2), S_OK);
    CHECK(e1 && e2 && e1 != e2);
    if (e1 && e2) {
        s1 = UN(RoParameterizedTypeExtraGetTypeSignature)(e1); s2 = UN(RoParameterizedTypeExtraGetTypeSignature)(e2);
        CHECK(s1 && s2 && strcmp(s1, s2));
        CHECK(s1 && strstr(s1, "913337e9")); CHECK(s2 && strstr(s2, "faa585ea"));
        G8_EQ(UN(RoParameterizedTypeExtraGetTypeSignature)(e1), s1);   /* stable pointer */
    }
    UN(RoFreeParameterizedTypeExtra)(e1); UN(RoFreeParameterizedTypeExtra)(e2);
}

TEST(gap8_ro_param_free_null_is_noop)
{
    G8_REQ_NW(UN(RoFreeParameterizedTypeExtra));
    UN(RoFreeParameterizedTypeExtra)(NULL);
    UN(RoFreeParameterizedTypeExtra)(NULL);
    CHECK(1);
}

TEST(gap8_ro_param_many_instantiations_with_free)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IVector`1", L"String" };
    int i; NEEDP(); REQUIRE(UN(RoFreeParameterizedTypeExtra));
    for (i = 0; i < 2000; i++) {
        LOC l; GUID g; void *e = NULL; loc_init(&l);
        if (UN(RoGetParameterizedTypeInstanceIID)(2, n, (IRML *)&l, &g, &e) != S_OK) { CHECK(0); break; }
        UN(RoFreeParameterizedTypeExtra)(e);
    }
}

TEST(gap8_ro_param_ordering_of_locator_calls_follows_arguments)
{
    PCWSTR n[] = { L"Windows.Foundation.Collections.IKeyValuePair`2", L"Windows.Foundation.Uri", L"Windows.Foundation.Collections.IVectorView`1", L"Windows.Foundation.Uri" };
    LOC l; GUID iid; HRESULT hr; NEEDP(); loc_init(&l);
    hr = UN(RoGetParameterizedTypeInstanceIID)(4, n, (IRML *)&l, &iid, NULL);
    G8_EQ(hr, S_OK);
    G8_EQ(l.calls, 4);
    if (l.calls == 4) { CHECK(!lstrcmpW(l.names[0], n[0])); CHECK(!lstrcmpW(l.names[1], n[1])); CHECK(!lstrcmpW(l.names[2], n[2])); CHECK(!lstrcmpW(l.names[3], n[3])); }
    expect_sig_iid("pinterface({02b51929-c1c4-4a7e-8940-0312b5c18500};rc(Windows.Foundation.Uri;{9e365e57-48b2-4160-956f-c7385120bbfc});"
                   "pinterface({bbe1fa4c-b0e3-4583-baef-1f1b2e483e56};rc(Windows.Foundation.Uri;{9e365e57-48b2-4160-956f-c7385120bbfc})))", &iid, "ordering");
}
