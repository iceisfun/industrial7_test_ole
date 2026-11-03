/* SafeArrayGetElement / PutElement semantics for BSTR, VARIANT, IUnknown, IDispatch and VT_RECORD elements. */
#include "sa.h"

static const WCHAR W_HELLO[] = L"hello";

/* ---- BSTR ---------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayGetPutElement_BSTR */
TEST(sa_bstr_put_copies_string)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR src = bstr_w(W_HELLO);
    LONG i = 1;
    EQHR(pPutElement(psa, &i, src), S_OK);
    BSTR raw = ((BSTR *)psa->pvData)[1];
    CHECK(raw != NULL);
    CHECK(raw != src);                         /* a copy, not an alias */
    if (raw) { EQ(pSysStringLen(raw), 5); CHECK(!lstrcmpW(raw, W_HELLO)); }
    src[0] = L'J';                             /* source changes must not leak into the array */
    if (raw) CHECK(raw[0] == L'h');
    BSTR out = NULL;
    EQHR(pGetElement(psa, &i, &out), S_OK);
    CHECK(out != NULL && out != raw && out != src);
    if (out) { CHECK(!lstrcmpW(out, W_HELLO)); EQ(pSysStringLen(out), 5); pSysFreeString(out); }
    pSysFreeString(src);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_bstr_put_preserves_embedded_nuls)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    const WCHAR data[] = { L'a', 0, L'b', 0, L'c' };
    BSTR src = pSysAllocStringLen(data, 5);
    LONG i = 0; BSTR out = NULL;
    EQHR(pPutElement(psa, &i, src), S_OK);
    EQHR(pGetElement(psa, &i, &out), S_OK);
    CHECK(out != NULL);
    if (out) { EQ(pSysStringLen(out), 5); CHECK(!memcmp(out, data, sizeof data)); EQ(out[5], 0); pSysFreeString(out); }
    pSysFreeString(src);
    pDestroy(psa);
}

TEST(sa_bstr_put_odd_byte_length)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR src = pSysAllocStringByteLen("abc", 3);   /* byte length 3, not a whole number of WCHARs */
    LONG i = 0; BSTR out = NULL;
    EQHR(pPutElement(psa, &i, src), S_OK);
    BSTR raw = ((BSTR *)psa->pvData)[0];
    CHECK(raw != NULL);
    if (raw) { EQ(pSysStringByteLen(raw), 3); CHECK(!memcmp(raw, "abc", 3)); }
    EQHR(pGetElement(psa, &i, &out), S_OK);
    if (out) { EQ(pSysStringByteLen(out), 3); CHECK(!memcmp(out, "abc", 3)); pSysFreeString(out); }
    pSysFreeString(src);
    pDestroy(psa);
}

TEST(sa_bstr_put_null_and_empty)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 3);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG i = 0;
    EQHR(pPutElement(psa, &i, NULL), S_OK);              /* NULL BSTR is stored as NULL */
    EQ(((BSTR *)psa->pvData)[0], NULL);
    BSTR empty = bstr_w(L"");
    i = 1;
    EQHR(pPutElement(psa, &i, empty), S_OK);             /* empty BSTR stays a distinct non-NULL object */
    BSTR raw = ((BSTR *)psa->pvData)[1];
    CHECK(raw != NULL && raw != empty);
    if (raw) { EQ(pSysStringLen(raw), 0); EQ(raw[0], 0); }
    pSysFreeString(empty);
    /* replace a non-NULL element with NULL */
    EQHR(pPutElement(psa, &i, NULL), S_OK);
    EQ(((BSTR *)psa->pvData)[1], NULL);
    pDestroy(psa);
}

TEST(sa_bstr_get_from_null_slot_gives_empty_string)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    LONG i = 0; BSTR out = (BSTR)0x1;
    EQHR(pGetElement(psa, &i, &out), S_OK);
    CHECK(out != NULL && out != (BSTR)0x1);              /* a NULL slot reads back as an allocated empty BSTR */
    if (out && out != (BSTR)0x1) { EQ(pSysStringLen(out), 0); EQ(out[0], 0); pSysFreeString(out); }
    pDestroy(psa);
}

TEST(sa_bstr_get_returns_fresh_copy_each_time)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR s = bstr_w(L"abc"); LONG i = 0;
    EQHR(pPutElement(psa, &i, s), S_OK);
    BSTR a = NULL, b = NULL;
    EQHR(pGetElement(psa, &i, &a), S_OK);
    EQHR(pGetElement(psa, &i, &b), S_OK);
    CHECK(a && b && a != b);
    if (a) { a[0] = L'X'; }
    BSTR raw = ((BSTR *)psa->pvData)[0];
    if (raw) CHECK(raw[0] == L'a');
    pSysFreeString(a); pSysFreeString(b); pSysFreeString(s);
    pDestroy(psa);
}

TEST(sa_bstr_put_replaces_previous)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_BSTR, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    BSTR s1 = bstr_w(L"first"), s2 = bstr_w(L"second string"); LONG i = 0;
    EQHR(pPutElement(psa, &i, s1), S_OK);
    EQHR(pPutElement(psa, &i, s2), S_OK);
    BSTR raw = ((BSTR *)psa->pvData)[0];
    CHECK(raw != NULL);
    if (raw) { EQ(pSysStringLen(raw), 13); CHECK(!lstrcmpW(raw, L"second string")); }
    /* putting the array's own stored pointer back is safe and yields the same text */
    EQHR(pPutElement(psa, &i, ((BSTR *)psa->pvData)[0]), S_OK);
    raw = ((BSTR *)psa->pvData)[0];
    if (raw) CHECK(!lstrcmpW(raw, L"second string"));
    pSysFreeString(s1); pSysFreeString(s2);
    pDestroy(psa);
}

TEST(sa_bstr_multidim_roundtrip)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {2, 1}, {3, 1} };
    SAFEARRAY *psa = pCreate(VT_BSTR, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    for (LONG r = 1; r <= 2; r++) for (LONG c = 1; c <= 3; c++) {
        WCHAR buf[16]; wsprintfW(buf, L"r%ldc%ld", r, c);
        BSTR s = bstr_w(buf); LONG idx[2] = { r, c };
        EQHR(pPutElement(psa, idx, s), S_OK);
        pSysFreeString(s);
    }
    for (LONG r = 1; r <= 2; r++) for (LONG c = 1; c <= 3; c++) {
        WCHAR buf[16]; wsprintfW(buf, L"r%ldc%ld", r, c);
        LONG idx[2] = { r, c }; BSTR o = NULL;
        EQHR(pGetElement(psa, idx, &o), S_OK);
        CHECK(o && !lstrcmpW(o, buf));
        pSysFreeString(o);
    }
    EQHR(pDestroy(psa), S_OK);
}

/* ---- VARIANT ------------------------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/safearray.c (LGPL-2.1+), test_SafeArrayGetPutElement_VARIANT */
TEST(sa_variant_put_get_scalars)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 4);
    CHECK(psa != NULL);
    if (!psa) return;
    VARIANT v, o; LONG i;
    pVariantInit(&v);
    V_VT(&v) = VT_I4; V_I4(&v) = 0x12345; i = 0;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    V_VT(&v) = VT_R8; V_R8(&v) = 2.25; i = 1;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    V_VT(&v) = VT_BOOL; V_BOOL(&v) = VARIANT_TRUE; i = 2;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    V_VT(&v) = VT_NULL; i = 3;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    VARIANT *raw = psa->pvData;
    EQ(V_VT(&raw[0]), VT_I4); EQ(V_I4(&raw[0]), 0x12345);
    EQ(V_VT(&raw[1]), VT_R8); CHECK(V_R8(&raw[1]) == 2.25);
    EQ(V_VT(&raw[2]), VT_BOOL);
    EQ(V_VT(&raw[3]), VT_NULL);
    pVariantInit(&o); i = 1;
    EQHR(pGetElement(psa, &i, &o), S_OK);
    EQ(V_VT(&o), VT_R8); CHECK(V_R8(&o) == 2.25);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_variant_empty_slot_reads_as_empty)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    VARIANT o; memset(&o, 0xab, sizeof o); V_VT(&o) = VT_EMPTY; LONG i = 1;
    EQHR(pGetElement(psa, &i, &o), S_OK);
    EQ(V_VT(&o), VT_EMPTY);
    EQ(((VARIANT *)psa->pvData)[1].vt, VT_EMPTY);
    pDestroy(psa);
}

TEST(sa_variant_bstr_deep_copied)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    VARIANT v, o; pVariantInit(&v); pVariantInit(&o);
    V_VT(&v) = VT_BSTR; V_BSTR(&v) = bstr_w(W_HELLO);
    LONG i = 0;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    VARIANT *raw = psa->pvData;
    EQ(V_VT(&raw[0]), VT_BSTR);
    CHECK(V_BSTR(&raw[0]) != V_BSTR(&v));
    CHECK(V_BSTR(&raw[0]) && !lstrcmpW(V_BSTR(&raw[0]), W_HELLO));
    CHECK(V_BSTR(&v) && !lstrcmpW(V_BSTR(&v), W_HELLO));    /* source untouched */
    EQHR(pGetElement(psa, &i, &o), S_OK);
    EQ(V_VT(&o), VT_BSTR);
    CHECK(V_BSTR(&o) != V_BSTR(&raw[0]) && V_BSTR(&o) != V_BSTR(&v));
    CHECK(V_BSTR(&o) && !lstrcmpW(V_BSTR(&o), W_HELLO));
    pVariantClear(&o); pVariantClear(&v);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_variant_put_clears_previous_element)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj o; tobj_init(&o, 1);
    VARIANT v; pVariantInit(&v);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = OBJ(o);
    LONG i = 0;
    EQHR(pPutElement(psa, &i, &v), S_OK);
    EQ(o.ref, 2);                                    /* VariantCopy AddRef'd */
    EQ(V_UNKNOWN(&((VARIANT *)psa->pvData)[0]), &o);
    V_VT(&v) = VT_I4; V_I4(&v) = 7;
    EQHR(pPutElement(psa, &i, &v), S_OK);            /* previous interface pointer is released */
    EQ(o.ref, 1);
    EQ(V_VT(&((VARIANT *)psa->pvData)[0]), VT_I4);
    pDestroy(psa);
}

TEST(sa_variant_get_addrefs_object)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_VARIANT, 0, 1);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj o; tobj_init(&o, 1);
    VARIANT v, g; pVariantInit(&v); pVariantInit(&g);
    V_VT(&v) = VT_DISPATCH; V_DISPATCH(&v) = (IDispatch *)&o;
    LONG i = 0;
    EQHR(pPutElement(psa, &i, &v), S_OK); EQ(o.ref, 2);
    EQHR(pGetElement(psa, &i, &g), S_OK); EQ(o.ref, 3);
    EQ(V_VT(&g), VT_DISPATCH);
    pVariantClear(&g); EQ(o.ref, 2);
    EQHR(pDestroy(psa), S_OK); EQ(o.ref, 1);
}

TEST(sa_variant_nested_safearray)
{
    SA_BEGIN();
    /* a VT_ARRAY variant stored in a VARIANT array is deep-copied (SafeArrayCopy) */
    SAFEARRAY *inner = sa1(VT_I4, 0, 2), *outer = sa1(VT_VARIANT, 0, 1);
    CHECK(inner && outer);
    if (!inner || !outer) return;
    LONG i = 1, x = 77; EQHR(pPutElement(inner, &i, &x), S_OK);
    VARIANT v; pVariantInit(&v); V_VT(&v) = VT_ARRAY | VT_I4; V_ARRAY(&v) = inner;
    i = 0; EQHR(pPutElement(outer, &i, &v), S_OK);
    VARIANT *raw = outer->pvData;
    EQ(V_VT(&raw[0]), VT_ARRAY | VT_I4);
    CHECK(V_ARRAY(&raw[0]) != NULL && V_ARRAY(&raw[0]) != inner);
    if (V_ARRAY(&raw[0])) {
        LONG j = 1, y = 0; EQHR(pGetElement(V_ARRAY(&raw[0]), &j, &y), S_OK); EQ(y, 77);
    }
    EQHR(pDestroy(outer), S_OK);
    EQHR(pDestroy(inner), S_OK);
}

/* ---- IUnknown / IDispatch ------------------------------------------------- */
static void unknown_semantics(VARTYPE vt)
{
    SAFEARRAY *psa = sa1(vt, 0, 3);
    if (!psa) { CHECKF(0, "create vt=%u", vt); return; }
    TObj a, b; tobj_init(&a, 1); tobj_init(&b, 1);
    LONG i = 0;
    EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    EQ(a.ref, 2);                                   /* array holds a reference */
    EQ(((void **)psa->pvData)[0], &a);              /* the identical pointer is stored */
    EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);       /* same pointer again: net no change */
    EQ(a.ref, 2);
    EQHR(pPutElement(psa, &i, OBJ(b)), S_OK);       /* replacing releases the old one */
    EQ(a.ref, 1); EQ(b.ref, 2);
    IUnknown *got = NULL;
    EQHR(pGetElement(psa, &i, &got), S_OK);
    EQ(got, &b); EQ(b.ref, 3);                      /* Get AddRefs for the caller */
    b.lpVtbl->Release((IUnknown *)&b); EQ(b.ref, 2);
    EQHR(pPutElement(psa, &i, NULL), S_OK);         /* NULL clears the slot and releases */
    EQ(b.ref, 1); EQ(((void **)psa->pvData)[0], NULL);
    i = 1; got = (IUnknown *)0x1;
    EQHR(pGetElement(psa, &i, &got), S_OK);         /* empty slot -> NULL, no AddRef */
    EQ(got, NULL);
    EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    i = 2; EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    EQ(a.ref, 3);
    EQHR(pDestroy(psa), S_OK);                      /* Destroy releases every element */
    EQ(a.ref, 1);
}
TEST(sa_unknown_element_refcounts) { SA_BEGIN(); unknown_semantics(VT_UNKNOWN); }
TEST(sa_dispatch_element_refcounts) { SA_BEGIN(); unknown_semantics(VT_DISPATCH); }

TEST(sa_unknown_destroydata_releases)
{
    SA_BEGIN();
    SAFEARRAY *psa = sa1(VT_UNKNOWN, 0, 2);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj a; tobj_init(&a, 1);
    LONG i = 0; EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    i = 1; EQHR(pPutElement(psa, &i, OBJ(a)), S_OK);
    EQ(a.ref, 3);
    EQHR(pDestroyData(psa), S_OK);
    EQ(a.ref, 1);
    EQ(psa->pvData, NULL);
    EQHR(pDestroyData(psa), S_OK);                  /* second call: nothing left */
    EQ(a.ref, 1);
    EQHR(pDestroy(psa), S_OK);
}

TEST(sa_unknown_multidim_refcounts)
{
    SA_BEGIN();
    SAFEARRAYBOUND b[2] = { {2, 0}, {2, 0} };
    SAFEARRAY *psa = pCreate(VT_UNKNOWN, 2, b);
    CHECK(psa != NULL);
    if (!psa) return;
    TObj o; tobj_init(&o, 1);
    for (LONG x = 0; x < 2; x++) for (LONG y = 0; y < 2; y++) { LONG idx[2] = { x, y }; EQHR(pPutElement(psa, idx, OBJ(o)), S_OK); }
    EQ(o.ref, 5);
    EQHR(pDestroy(psa), S_OK);
    EQ(o.ref, 1);
}

/* ---- VT_RECORD -------------------------------------------------------------- */
TEST(sa_record_create_ex_properties)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 12);
    SAFEARRAYBOUND b = { 3, 2 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->cbElements, 12);
    EQ(psa->fFeatures, FADF_RECORD);
    EQ(r.ref, 2);                                   /* the array holds a reference to the record info */
    VARTYPE vt = 0; EQHR(pGetVartype(psa, &vt), S_OK); EQ(vt, VT_RECORD);
    IRecordInfo *ri = NULL;
    EQHR(pGetRecordInfo(psa, &ri), S_OK);
    EQ(ri, &r); EQ(r.ref, 3);                       /* Get AddRefs */
    ri->lpVtbl->Release(ri);
    EQ(SA_HIDDEN_REC(psa), &r);
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}

TEST(sa_record_create_ex_size_limits)
{
    SA_BEGIN();
    SAFEARRAYBOUND b = { 2, 0 };
    TRec r; trec_setup(&r, 0xffff);
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa != NULL);
    if (psa) { EQ(psa->cbElements, 0xffff); EQHR(pDestroy(psa), S_OK); }
    trec_setup(&r, 0x10000);                         /* cbElements is a 16 bit quantity */
    psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa == NULL);
    if (psa) pDestroy(psa);
    EQ(r.ref, 1);
}

TEST(sa_record_zero_sized)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 0);
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->cbElements, 0);
    LONG i = 0; char buf[4];
    EQHR(pGetElement(psa, &i, buf), E_INVALIDARG);   /* cbElements == 0 is unusable */
    EQHR(pPutElement(psa, &i, buf), E_INVALIDARG);
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}

TEST(sa_record_vector_ex)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 8);
    SAFEARRAY *psa = pCreateVectorEx(VT_RECORD, 1, 4, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(psa->cbElements, 8);
    EQ(psa->fFeatures, FADF_RECORD | FADF_CREATEVECTOR);
    EQ(r.ref, 2);
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}

TEST(sa_record_getput_use_recordcopy)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 8);
    SAFEARRAYBOUND b = { 3, 0 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    BYTE in[8] = { 1,2,3,4,5,6,7,8 }, out[8]; memset(out, 0, 8);
    LONG i = 2;
    r.nCopy = 0;
    EQHR(pPutElement(psa, &i, in), S_OK);
    EQ(r.nCopy, 1);
    EQ(r.copySrc, in);                               /* Put: RecordCopy(pvExisting = caller's record, pvNew = slot) */
    EQ(r.copyDst, (BYTE *)psa->pvData + 16);
    CHECK(!memcmp((BYTE *)psa->pvData + 16, in, 8));
    EQ(r.ref, 2);                                    /* temporary Get/Release pair is balanced */
    EQHR(pGetElement(psa, &i, out), S_OK);
    EQ(r.nCopy, 2);
    EQ(r.copySrc, (BYTE *)psa->pvData + 16);         /* Get: RecordCopy(pvExisting = slot, pvNew = caller's buffer) */
    EQ(r.copyDst, out);
    CHECK(!memcmp(out, in, 8));
    EQ(r.ref, 2);
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}

TEST(sa_record_copy_failure_propagates)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 4);
    SAFEARRAYBOUND b = { 2, 0 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    BYTE rec[4] = { 9, 9, 9, 9 }; LONG i = 0;
    r.copyHr = E_FAIL;
    EQHR(pPutElement(psa, &i, rec), E_FAIL);
    EQHR(pGetElement(psa, &i, rec), E_FAIL);
    EQ(psa->cLocks, 0);
    r.copyHr = S_OK;
    EQ(r.ref, 2);
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}

TEST(sa_record_destroy_clears_each_element)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 4);
    SAFEARRAYBOUND b[2] = { {3, 0}, {2, 0} };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 2, b, REC(r));
    CHECK(psa != NULL);
    if (!psa) return;
    r.nClear = 0;
    EQHR(pDestroy(psa), S_OK);
    EQ(r.nClear, 6);                                 /* RecordClear once per element */
    EQ(r.ref, 1);
}

TEST(sa_record_setget_recordinfo)
{
    SA_BEGIN();
    TRec r1, r2; trec_setup(&r1, 4); trec_setup(&r2, 4);
    SAFEARRAYBOUND b = { 1, 0 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r1));
    CHECK(psa != NULL);
    if (!psa) return;
    EQ(r1.ref, 2);
    EQHR(pSetRecordInfo(psa, REC(r2)), S_OK);
    EQ(r1.ref, 1); EQ(r2.ref, 2);                    /* old released, new AddRef'd */
    IRecordInfo *ri = NULL;
    EQHR(pGetRecordInfo(psa, &ri), S_OK); EQ(ri, &r2); EQ(r2.ref, 3);
    ri->lpVtbl->Release(ri);
    EQHR(pSetRecordInfo(psa, NULL), S_OK);
    EQ(r2.ref, 1);
    ri = (IRecordInfo *)1;
    EQHR(pGetRecordInfo(psa, &ri), S_OK); EQ(ri, NULL);
    EQHR(pSetRecordInfo(psa, REC(r1)), S_OK);        /* restore so Destroy has a record info to clear with */
    EQHR(pDestroy(psa), S_OK);
    EQ(r1.ref, 1);
}

TEST(sa_record_recordinfo_args)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 4);
    IRecordInfo *ri = NULL;
    EQHR(pSetRecordInfo(NULL, REC(r)), E_INVALIDARG);
    EQHR(pGetRecordInfo(NULL, &ri), E_INVALIDARG);
    SAFEARRAY *plain = sa1(VT_I4, 0, 1);
    if (plain) {
        EQHR(pSetRecordInfo(plain, REC(r)), E_INVALIDARG);   /* no FADF_RECORD */
        EQHR(pGetRecordInfo(plain, &ri), E_INVALIDARG);
        EQ(r.ref, 1);
        pDestroy(plain);
    }
    SAFEARRAYBOUND b = { 1, 0 };
    SAFEARRAY *psa = pCreateEx(VT_RECORD, 1, &b, REC(r));
    if (psa) { EQHR(pGetRecordInfo(psa, NULL), E_INVALIDARG); pDestroy(psa); }
}

TEST(sa_record_alloc_descriptor_then_setrecordinfo)
{
    SA_BEGIN();
    TRec r; trec_setup(&r, 4);
    SAFEARRAY *psa = NULL;
    EQHR(pAllocDescriptorEx(VT_RECORD, 1, &psa), S_OK);
    if (!psa) return;
    EQHR(pSetRecordInfo(psa, REC(r)), S_OK);
    EQ(r.ref, 2);
    psa->cbElements = 4; psa->rgsabound[0].cElements = 2;
    EQHR(pAllocData(psa), S_OK);
    LONG i = 1; BYTE rec[4] = { 1, 2, 3, 4 }, out[4] = {0};
    EQHR(pPutElement(psa, &i, rec), S_OK);
    EQHR(pGetElement(psa, &i, out), S_OK);
    CHECK(!memcmp(rec, out, 4));
    EQHR(pDestroy(psa), S_OK);
    EQ(r.ref, 1);
}
