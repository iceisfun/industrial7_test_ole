/* CoGetApartmentType, CoIncrementMTAUsage/CoDecrementMTAUsage (implicit MTA), CoGetApartmentID. */
#include "cia.h"

/* ----- CoGetApartmentType: argument validation ------------------------------------------------ */
TEST(cia_apt_null_arguments)
{
    APTTYPE t = (APTTYPE)0xdeadbeef; APTTYPEQUALIFIER q = (APTTYPEQUALIFIER)0xdeadbeef;
    REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(U(CoGetApartmentType)(NULL, NULL), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoGetApartmentType)(&t, NULL), CIA_E_INVALIDARG);
    EXPECT_EQ((DWORD)t, 0xdeadbeef);            /* nothing written when an argument is NULL */
    EXPECT_HR(U(CoGetApartmentType)(NULL, &q), CIA_E_INVALIDARG);
    EXPECT_EQ((DWORD)q, 0xdeadbeef);
}

TEST(cia_apt_null_arguments_while_initialized)
{
    APTTYPE t = (APTTYPE)0xdeadbeef; APTTYPEQUALIFIER q = (APTTYPEQUALIFIER)0xdeadbeef;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoGetApartmentType)(NULL, NULL), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoGetApartmentType)(&t, NULL), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoGetApartmentType)(NULL, &q), CIA_E_INVALIDARG);
    EXPECT_EQ((DWORD)t, 0xdeadbeef); EXPECT_EQ((DWORD)q, 0xdeadbeef);
    cUninit();
}

TEST(cia_apt_uninitialized_outputs)
{
    int t = 0, q = 0;
    REQUIRE(U(CoGetApartmentType));
    /* on failure the outputs are still written: APTTYPE_CURRENT (-1) and APTTYPEQUALIFIER_NONE */
    EXPECT_HR(cia_apt(&t, &q), CIA_NOTINIT);
    EXPECT_EQ(t, CIA_APT_CURRENT);
    EXPECT_EQ(q, CIA_Q_NONE);
    CHECK(CIA_APT_CURRENT == (int)APTTYPE_CURRENT);
    CHECK(CIA_APT_STA == (int)APTTYPE_STA && CIA_APT_MTA == (int)APTTYPE_MTA && CIA_APT_NA == (int)APTTYPE_NA && CIA_APT_MAINSTA == (int)APTTYPE_MAINSTA);
}

TEST(cia_apt_sta_is_mainsta_when_first)
{
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_apt_mta_type)
{
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_apt_type_stable_across_repeated_queries)
{
    int i;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_OK);
    for (i = 0; i < 200; i++) CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
}

TEST(cia_apt_flags_do_not_alter_reported_type)
{
    static const DWORD sta[] = { 2, 6, 10, 14 }, mta[] = { 0, 4, 8, 12 };
    size_t i;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 4; i++) {
        EXPECT_HR(cInitEx(NULL, sta[i]), S_OK); CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE); cUninit();
        EXPECT_HR(cInitEx(NULL, mta[i]), S_OK); CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE); cUninit();
    }
}

TEST(cia_apt_nested_inits_keep_type)
{
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

static void w_apt(void *p) { struct cia_apt_res *r = (struct cia_apt_res *)p; r->hr = cia_apt(&r->t, &r->q); }
static void w_sta_apt(void *p) { struct cia_apt_res *r = (struct cia_apt_res *)p; cInitEx(NULL, CIA_COINIT_STA); r->hr = cia_apt(&r->t, &r->q); cUninit(); }
static void w_mta_apt(void *p) { struct cia_apt_res *r = (struct cia_apt_res *)p; cInitEx(NULL, CIA_COINIT_MTA); r->hr = cia_apt(&r->t, &r->q); cUninit(); }

TEST(cia_apt_main_sta_is_reassigned_after_uninit)
{
    struct cia_apt_res r;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    /* with no STA alive, a worker's STA becomes the main STA ... */
    memset(&r, 0, sizeof r); CHECK(cia_run(w_sta_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MAINSTA);
    /* ... and after it is gone, the next STA (this thread) is the main STA again */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    memset(&r, 0, sizeof r); CHECK(cia_run(w_sta_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_STA);
    cUninit();
}

TEST(cia_apt_mta_workers_report_mta_none)
{
    struct cia_apt_res r; int i;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 5; i++) {
        memset(&r, 0, sizeof r); CHECK(cia_run(w_mta_apt, &r));
        EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_NONE);
    }
}

TEST(cia_apt_fresh_thread_is_not_initialized)
{
    struct cia_apt_res r;
    REQUIRE(U(CoGetApartmentType));
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));
    EXPECT_HR(r.hr, CIA_NOTINIT); EXPECT_EQ(r.t, CIA_APT_CURRENT); EXPECT_EQ(r.q, CIA_Q_NONE);
}

/* ----- CoIncrementMTAUsage / CoDecrementMTAUsage ------------------------------------------------ */
TEST(cia_mta_usage_basic)
{
    CO_MTA_USAGE_COOKIE c1 = NULL, c2 = NULL;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    CIA_CHECK_NOTINIT();
    EXPECT_HR(U(CoIncrementMTAUsage)(&c1), S_OK);
    CHECK(c1 != NULL);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c2), S_OK);
    CHECK(c2 != NULL && c2 != c1);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c1), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);      /* one reference left */
    EXPECT_HR(U(CoDecrementMTAUsage)(c2), S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_decrement_any_order)
{
    CO_MTA_USAGE_COOKIE c[4]; int i;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    for (i = 0; i < 4; i++) { c[i] = NULL; EXPECT_HR(U(CoIncrementMTAUsage)(&c[i]), S_OK); CHECK(c[i] != NULL); }
    for (i = 0; i < 4; i++) { int j; for (j = i + 1; j < 4; j++) CHECK(c[i] != c[j]); }
    EXPECT_HR(U(CoDecrementMTAUsage)(c[2]), S_OK);
    EXPECT_HR(U(CoDecrementMTAUsage)(c[0]), S_OK);
    EXPECT_HR(U(CoDecrementMTAUsage)(c[3]), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c[1]), S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_many_increments)
{
    CO_MTA_USAGE_COOKIE c[64]; int i;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    for (i = 0; i < 64; i++) { c[i] = NULL; EXPECT_HR(U(CoIncrementMTAUsage)(&c[i]), S_OK); }
    for (i = 0; i < 63; i++) EXPECT_HR(U(CoDecrementMTAUsage)(c[i]), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c[63]), S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_repeated_cycles)
{
    int i;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    for (i = 0; i < 25; i++) {
        CO_MTA_USAGE_COOKIE c = NULL;
        EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
        CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
        EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
        CIA_CHECK_NOTINIT();
    }
}

TEST(cia_mta_usage_from_sta_thread)
{
    CO_MTA_USAGE_COOKIE c = NULL; struct cia_apt_res r;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    CHECK(c != NULL);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);      /* the calling STA thread is unaffected */
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));     /* but a thread that never initialized COM now sees the MTA */
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));
    EXPECT_HR(r.hr, CIA_NOTINIT);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_visible_to_other_threads)
{
    CO_MTA_USAGE_COOKIE c = NULL; struct cia_apt_res r;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));
    EXPECT_HR(r.hr, CIA_NOTINIT);
}

TEST(cia_mta_usage_then_init_mta_same_thread)
{
    CO_MTA_USAGE_COOKIE c = NULL;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);        /* the implicit MTA is not a per-thread init */
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);          /* explicitly initialized now */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_FALSE);
    cUninit(); cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_then_init_sta_same_thread)
{
    CO_MTA_USAGE_COOKIE c = NULL;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);        /* no RPC_E_CHANGED_MODE: implicit MTA is not this thread's model */
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_uninit_of_last_explicit_mta_keeps_usage_reference)
{
    CO_MTA_USAGE_COOKIE c = NULL;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);   /* the explicit init is gone, the usage count holds the MTA */
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    CIA_CHECK_NOTINIT();
}

static void w_inc_and_exit(void *p) { *(HRESULT *)p = U(CoIncrementMTAUsage)(&((CO_MTA_USAGE_COOKIE *)p)[1]); }
TEST(cia_mta_usage_survives_creating_thread_exit)
{
    struct { HRESULT hr; CO_MTA_USAGE_COOKIE c; } s; struct cia_apt_res r;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    memset(&s, 0, sizeof s);
    /* cia_run passes one pointer: use the struct with hr first, cookie second */
    CHECK(cia_run(w_inc_and_exit, &s));
    EXPECT_HR(s.hr, S_OK); CHECK(s.c != NULL);
    /* the incrementing thread is gone; the usage reference keeps the MTA alive */
    memset(&r, 0, sizeof r); CHECK(cia_run(w_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_IMPLICIT_MTA);
    /* and it can be released from a different thread */
    EXPECT_HR(U(CoDecrementMTAUsage)(s.c), S_OK);
    CIA_CHECK_NOTINIT();
}

static void w_dec(void *p) { struct { CO_MTA_USAGE_COOKIE c; HRESULT hr; } *s = p; s->hr = U(CoDecrementMTAUsage)(s->c); }
TEST(cia_mta_usage_decrement_on_other_thread)
{
    CO_MTA_USAGE_COOKIE c = NULL; struct { CO_MTA_USAGE_COOKIE c; HRESULT hr; } s;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    s.c = c; s.hr = E_FAIL;
    CHECK(cia_run(w_dec, &s));
    EXPECT_HR(s.hr, S_OK);
    CIA_CHECK_NOTINIT();
}

TEST(cia_mta_usage_cookie_is_reset_to_null_first)
{
    CO_MTA_USAGE_COOKIE c = (CO_MTA_USAGE_COOKIE)(ULONG_PTR)0x1234;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    CHECK(c != (CO_MTA_USAGE_COOKIE)(ULONG_PTR)0x1234 && c != NULL);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
}

TEST(cia_mta_usage_combase_and_ole32_same)
{
    CO_MTA_USAGE_COOKIE c = NULL;
    HRESULT (WINAPI *inc)(CO_MTA_USAGE_COOKIE *); HRESULT (WINAPI *dec)(CO_MTA_USAGE_COOKIE);
    REQUIRE(UIN("combase.dll", CoIncrementMTAUsage)); REQUIRE(UIN("ole32.dll", CoIncrementMTAUsage));
    REQUIRE(UIN("combase.dll", CoDecrementMTAUsage)); REQUIRE(UIN("ole32.dll", CoDecrementMTAUsage));
    inc = UIN("combase.dll", CoIncrementMTAUsage); dec = UIN("ole32.dll", CoDecrementMTAUsage);
    EXPECT_HR(inc(&c), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    EXPECT_HR(dec(c), S_OK);
    CIA_CHECK_NOTINIT();
}

/* implicit-MTA thread can use other apartment-aware APIs */
TEST(cia_mta_usage_makes_context_token_available)
{
    CO_MTA_USAGE_COOKIE c = NULL; ULONG_PTR tok = 0;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetContextToken));
    EXPECT_HR(U(CoGetContextToken)(&tok), CIA_NOTINIT);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    tok = 0;
    EXPECT_HR(U(CoGetContextToken)(&tok), S_OK);
    CHECK(tok != 0);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    tok = 0x55;
    EXPECT_HR(U(CoGetContextToken)(&tok), CIA_NOTINIT);
}

TEST(cia_mta_usage_makes_logical_thread_info_available)
{
    CO_MTA_USAGE_COOKIE c = NULL; IComThreadingInfo *ti = NULL; APTTYPE at = (APTTYPE)99;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage)); REQUIRE(U(CoGetObjectContext));
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&ti), CIA_NOTINIT);
    CHECK(ti == NULL);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    EXPECT_HR(U(CoGetObjectContext)(&CIA_IID_IComThreadingInfo, (void **)&ti), S_OK);
    if (ti) {
        EXPECT_HR(IComThreadingInfo_GetCurrentApartmentType(ti, &at), S_OK);
        EXPECT_EQ(at, CIA_APT_MTA);
        IComThreadingInfo_Release(ti);
    }
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
}

/* ----- CoGetApartmentID ---------------------------------------------------------------------------- */
TEST(cia_aptid_argument_validation)
{
    DWORD id = 0x12345678;
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; } REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    /* validation order from the decomp: NULL out pointer or APTTYPE_STA -> E_INVALIDARG; anything but CURRENT -> E_NOTIMPL */
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, NULL), CIA_E_INVALIDARG);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_STA, NULL), CIA_E_INVALIDARG);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_STA, &id), CIA_E_INVALIDARG);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_MTA, NULL), CIA_E_INVALIDARG);
    EXPECT_EQ(id, 0x12345678);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_MTA, &id), CIA_E_NOTIMPL);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_NA, &id), CIA_E_NOTIMPL);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_MAINSTA, &id), CIA_E_NOTIMPL);
    EXPECT_HR(UN(CoGetApartmentID)(4, &id), CIA_E_NOTIMPL);
    EXPECT_HR(UN(CoGetApartmentID)(-2, &id), CIA_E_NOTIMPL);
    EXPECT_HR(UN(CoGetApartmentID)(0x7fffffff, &id), CIA_E_NOTIMPL);
    EXPECT_EQ(id, 0x12345678);
    /* the same answers while initialized */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_STA, &id), CIA_E_INVALIDARG);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_MTA, &id), CIA_E_NOTIMPL);
    cUninit();
}

TEST(cia_aptid_uninitialized_thread)
{
    DWORD id = 0x12345678;
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; }
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &id), CIA_NOTINIT);
    EXPECT_EQ(id, 0x12345678);
}

TEST(cia_aptid_current_sta_stable)
{
    DWORD a = 0, b = 0;
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; } REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &a), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &b), S_OK);
    EXPECT_EQ(a, b);
    cUninit();
}

TEST(cia_aptid_current_mta_stable)
{
    DWORD a = 0, b = 0;
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; } REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &a), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &b), S_OK);
    EXPECT_EQ(a, b);
    cUninit();
}

struct aid { HRESULT hr; DWORD id; };
static void w_aid_sta(void *p) { struct aid *a = (struct aid *)p; cInitEx(NULL, CIA_COINIT_STA); a->hr = UN(CoGetApartmentID)(CIA_APT_CURRENT, &a->id); cUninit(); }
static void w_aid_mta(void *p) { struct aid *a = (struct aid *)p; cInitEx(NULL, CIA_COINIT_MTA); a->hr = UN(CoGetApartmentID)(CIA_APT_CURRENT, &a->id); cUninit(); }
TEST(cia_aptid_differs_between_stas_and_is_shared_in_mta)
{
    struct aid m = {0, 0}, s1 = {0, 0}, s2 = {0, 0}, t1 = {0, 0}, t2 = {0, 0};
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; } REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(UN(CoGetApartmentID)(CIA_APT_CURRENT, &m.id), S_OK);
    CHECK(cia_run(w_aid_sta, &s1));
    CHECK(cia_run(w_aid_sta, &s2));
    EXPECT_HR(s1.hr, S_OK); EXPECT_HR(s2.hr, S_OK);
    t_note("apartment ids: main STA %lu, worker STA %lu / %lu", m.id, s1.id, s2.id);
    CHECK(s1.id != m.id);          /* concurrently alive STAs have distinct ids */
    cUninit();
    CHECK(cia_run(w_aid_mta, &t1)); CHECK(cia_run(w_aid_mta, &t2));
    EXPECT_HR(t1.hr, S_OK); EXPECT_HR(t2.hr, S_OK);
    t_note("MTA ids from two short-lived MTA threads: %lu / %lu", t1.id, t2.id);
}

TEST(cia_aptid_implicit_mta_thread)
{
    CO_MTA_USAGE_COOKIE c = NULL; DWORD id = 0; HRESULT hr;
    REQUIRE(UN(CoGetApartmentID)); if (t_is_wine()) { t_skip("CoGetApartmentID is an unimplemented stub under Wine"); return; } REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage));
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    hr = UN(CoGetApartmentID)(CIA_APT_CURRENT, &id);
    CHECKF(hr == S_OK, "implicit MTA thread: hr 0x%08lx", (unsigned long)hr);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
}

/* ----- apartment types we cannot enter from a pure C test ----------------------------------- */
TEST(cia_apt_na_not_reachable_without_context_callback)
{
    /* The neutral apartment is entered via IContextCallback on an NTA context; a plain thread never reports it. */
    int t, q;
    REQUIRE(U(CoGetApartmentType)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cia_apt(&t, &q), S_OK);
    CHECK(t != CIA_APT_NA);
    cUninit();
}
