/* gap_04: SafeArrayAddRef / SafeArrayReleaseData / SafeArrayReleaseDescriptor (script-engine "pinning" API).
 *
 * Win10 oleaut32: AddRef zeroes *ppDataToRelease, increments the descriptor's pin count, and -- only for arrays whose
 * data block is heap-owned (fFeatures has none of FADF_AUTO|FADF_STATIC|FADF_EMBEDDED) -- also pins the data and
 * returns psa->pvData in *ppDataToRelease. ReleaseData(pData) / ReleaseDescriptor(psa) drop the pins; the memory is
 * only freed once its last pin goes (SafeArrayDestroy of a pinned array leaves the blocks alive). */
#include "t.h"
#include <oleauto.h>
#define EQ(expr, want) do { ULONG_PTR v_ = (ULONG_PTR)(expr); ULONG_PTR w_ = (ULONG_PTR)(want); t_check(v_ == w_, __FILE__, __LINE__, #expr " == " #want, "got 0x%llx want 0x%llx", (unsigned long long)v_, (unsigned long long)w_); } while (0)

UDECL(HRESULT, SafeArrayAddRef, (SAFEARRAY *, PVOID *));
UDECL(void, SafeArrayReleaseData, (PVOID));
UDECL(void, SafeArrayReleaseDescriptor, (SAFEARRAY *));

#define NEED() do { REQUIRE(UN(SafeArrayAddRef)); REQUIRE(UN(SafeArrayReleaseData)); REQUIRE(UN(SafeArrayReleaseDescriptor)); \
    REQUIRE(U(SafeArrayCreate)); REQUIRE(U(SafeArrayDestroy)); if (t_is_wine()) { t_skip("SafeArrayAddRef family is not implemented in wine"); return; } } while (0)
#define SENT ((PVOID)(ULONG_PTR)0xdeadbeef)

static SAFEARRAY *mk(VARTYPE vt, ULONG n) { SAFEARRAYBOUND b = { n, 0 }; return U(SafeArrayCreate)(vt, 1, &b); }

TEST(gap4_sa_addref_returns_data_pointer) {
    SAFEARRAY *sa; PVOID pd = SENT;
    NEED();
    sa = mk(VT_I4, 16); CHECK(sa != NULL); if (!sa) return;
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    CHECK(pd == sa->pvData);
    CHECK(pd != NULL);
    UN(SafeArrayReleaseData)(pd);
    UN(SafeArrayReleaseDescriptor)(sa);
    EQ(U(SafeArrayDestroy)(sa), S_OK);
}
TEST(gap4_sa_addref_all_heap_array_kinds) {
    static const VARTYPE vts[] = { VT_I1, VT_UI1, VT_I2, VT_UI2, VT_I4, VT_UI4, VT_INT, VT_UINT, VT_I8, VT_UI8, VT_R4, VT_R8, VT_CY, VT_DATE, VT_BOOL,
                                   VT_ERROR, VT_DECIMAL, VT_BSTR, VT_VARIANT, VT_UNKNOWN, VT_DISPATCH };
    unsigned k;
    NEED();
    for (k = 0; k < sizeof vts / sizeof *vts; k++) {
        SAFEARRAY *sa = mk(vts[k], 7); PVOID pd = SENT;
        CHECKF(sa != NULL, "vt %u", vts[k]); if (!sa) continue;
        EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
        CHECKF(pd == sa->pvData, "vt %u: %p vs %p", vts[k], pd, sa->pvData);
        UN(SafeArrayReleaseData)(pd);
        UN(SafeArrayReleaseDescriptor)(sa);
        EQ(U(SafeArrayDestroy)(sa), S_OK);
    }
}
TEST(gap4_sa_addref_multidimensional_and_vector) {
    SAFEARRAYBOUND b[3] = { {3, 0}, {4, 1}, {5, -2} }; SAFEARRAY *sa, *vec; PVOID pd = SENT;
    NEED();
    sa = U(SafeArrayCreate)(VT_R8, 3, b); CHECK(sa != NULL);
    if (sa) { EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK); CHECK(pd == sa->pvData); UN(SafeArrayReleaseData)(pd); UN(SafeArrayReleaseDescriptor)(sa); EQ(U(SafeArrayDestroy)(sa), S_OK); }
    vec = U(SafeArrayCreateVector)(VT_UI1, 5, 100); CHECK(vec != NULL); pd = SENT;
    if (vec) { EQ(UN(SafeArrayAddRef)(vec, &pd), S_OK); CHECKF(pd == NULL, "vector: pd %p", pd); /* Win10: a vector has no separately pinned data block, *ppDataToRelease is NULL */ UN(SafeArrayReleaseData)(pd); UN(SafeArrayReleaseDescriptor)(vec); EQ(U(SafeArrayDestroy)(vec), S_OK); }
}
TEST(gap4_sa_pinned_data_is_the_live_data) {
    SAFEARRAY *sa; PVOID pd = NULL; LONG i, idx; int bad = 0;
    NEED();
    sa = mk(VT_I4, 32); if (!sa) { CHECK(sa != NULL); return; }
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    for (i = 0; i < 32; i++) ((LONG *)pd)[i] = 1000 + i;                  /* write through the pinned pointer ... */
    for (i = 0; i < 32; i++) { LONG v = 0; idx = i; if (U(SafeArrayGetElement)(sa, &idx, &v) != S_OK || v != 1000 + i) bad++; }   /* ... read through the API */
    EQ(bad, 0);
    UN(SafeArrayReleaseData)(pd); UN(SafeArrayReleaseDescriptor)(sa);
    EQ(U(SafeArrayDestroy)(sa), S_OK);
}
TEST(gap4_sa_addref_nested_pins) {
    SAFEARRAY *sa; PVOID pd[4]; int i; HRESULT hr;
    NEED();
    sa = mk(VT_I2, 8); if (!sa) { CHECK(sa != NULL); return; }
    for (i = 0; i < 4; i++) { pd[i] = SENT; EQ(UN(SafeArrayAddRef)(sa, &pd[i]), S_OK); CHECK(pd[i] == sa->pvData); }
    for (i = 0; i < 4; i++) { UN(SafeArrayReleaseData)(pd[i]); UN(SafeArrayReleaseDescriptor)(sa); }
    hr = U(SafeArrayDestroy)(sa);
    EQ(hr, S_OK);
}
TEST(gap4_sa_addref_output_reset_for_unowned_data) {
    /* arrays whose data is not heap-owned: ppDataToRelease is reset to NULL and only the descriptor is pinned */
    static const USHORT fl[] = { FADF_AUTO, FADF_STATIC, FADF_EMBEDDED };
    unsigned k;
    NEED();
    for (k = 0; k < sizeof fl / sizeof *fl; k++) {
        SAFEARRAY *sa = mk(VT_I4, 4); PVOID pd = SENT; USHORT orig;
        CHECK(sa != NULL); if (!sa) continue;
        orig = sa->fFeatures;
        sa->fFeatures = orig | fl[k];
        EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
        CHECKF(pd == NULL, "feature 0x%x: out param %p", fl[k], pd);
        UN(SafeArrayReleaseDescriptor)(sa);
        sa->fFeatures = orig;
        EQ(U(SafeArrayDestroy)(sa), S_OK);
    }
}
TEST(gap4_sa_destroy_while_pinned_keeps_blocks_alive) {
    SAFEARRAY *sa; PVOID pd = NULL; HRESULT hr;
    NEED();
    sa = mk(VT_I4, 64); if (!sa) { CHECK(sa != NULL); return; }
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    CHECK(pd != NULL);
    hr = U(SafeArrayDestroy)(sa);              /* the owner lets go while a script engine still holds pins */
    EQ(hr, S_OK);
    /* Win10: the data block is detached from the still-pinned descriptor (SafeArrayDestroy clears pvData, the pins keep the blocks alive) */
    CHECKF(sa->pvData == NULL, "pvData %p after destroy-while-pinned", sa->pvData);
    UN(SafeArrayReleaseData)(pd);              /* last data pin: the data block is freed here */
    UN(SafeArrayReleaseDescriptor)(sa);        /* last descriptor pin: the descriptor is freed here */
    t_note("destroy-while-pinned and both releases completed");
}
TEST(gap4_sa_release_order_independent) {
    SAFEARRAY *sa; PVOID pd = NULL;
    NEED();
    sa = mk(VT_I4, 8); if (!sa) { CHECK(sa != NULL); return; }
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    EQ(U(SafeArrayDestroy)(sa), S_OK);
    UN(SafeArrayReleaseDescriptor)(sa);        /* descriptor first this time */
    UN(SafeArrayReleaseData)(pd);
    t_note("release descriptor-then-data completed");
}
TEST(gap4_sa_addref_does_not_lock) {
    SAFEARRAY *sa; PVOID pd = NULL;
    NEED();
    sa = mk(VT_I4, 4); if (!sa) { CHECK(sa != NULL); return; }
    EQ(sa->cLocks, 0);
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    EQ(sa->cLocks, 0);                          /* pinning is independent of SafeArrayLock's cLocks */
    UN(SafeArrayReleaseData)(pd); UN(SafeArrayReleaseDescriptor)(sa);
    EQ(sa->cLocks, 0);
    EQ(U(SafeArrayDestroy)(sa), S_OK);
}
TEST(gap4_sa_addref_bstr_array_contents_survive) {
    SAFEARRAY *sa; PVOID pd = NULL; LONG i; BSTR *p;
    NEED();
    REQUIRE(U(SysAllocString)); REQUIRE(U(SafeArrayPutElement));
    sa = mk(VT_BSTR, 3); if (!sa) { CHECK(sa != NULL); return; }
    for (i = 0; i < 3; i++) { BSTR b = U(SysAllocString)(L"pinned"); EQ(U(SafeArrayPutElement)(sa, &i, b), S_OK); U(SysFreeString)(b); }
    EQ(UN(SafeArrayAddRef)(sa, &pd), S_OK);
    p = (BSTR *)pd; CHECK(p != NULL);
    for (i = 0; i < 3 && p; i++) { CHECK(p[i] != NULL); if (p[i]) CHECK(!lstrcmpW(p[i], L"pinned")); }
    UN(SafeArrayReleaseData)(pd); UN(SafeArrayReleaseDescriptor)(sa);
    EQ(U(SafeArrayDestroy)(sa), S_OK);
}
