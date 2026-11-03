/* CoRegisterInitializeSpy / CoRevokeInitializeSpy and the IInitializeSpy callbacks.
 * Spy registrations are per thread, so every test runs on a fresh worker thread (cookie numbering starts at 0). */
#include "cia.h"

#define SPY_TEST(name) \
    static void body_##name(void *unused); \
    TEST(name) { REQUIRE(U(CoRegisterInitializeSpy)); REQUIRE(U(CoRevokeInitializeSpy)); REQUIRE(U(CoInitializeEx)); \
                 REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType)); cia_log_reset(); cia_run(body_##name, NULL); } \
    static void body_##name(void *unused)

#define LOG_N(n) EXPECT_EQ(cia_nlog, (n))
#define LOG_IS(i, spy, evt, A, B, C) do { \
    if ((i) < cia_nlog) { \
        cia_ev *e_ = &cia_log[i]; \
        t_check(e_->id == (spy) && e_->ev == (evt), __FILE__, __LINE__, "log[" #i "] spy/event", "got spy %d ev %d want spy %d ev %d", e_->id, e_->ev, (spy), (evt)); \
        t_check(e_->a == (DWORD)(A), __FILE__, __LINE__, "log[" #i "].a", "got 0x%lx want 0x%lx", (unsigned long)e_->a, (unsigned long)(DWORD)(A)); \
        t_check(e_->b == (DWORD)(B), __FILE__, __LINE__, "log[" #i "].b", "got 0x%lx want 0x%lx", (unsigned long)e_->b, (unsigned long)(DWORD)(B)); \
        t_check(e_->c == (DWORD)(C), __FILE__, __LINE__, "log[" #i "].c", "got 0x%lx want 0x%lx", (unsigned long)e_->c, (unsigned long)(DWORD)(C)); \
    } else t_check(0, __FILE__, __LINE__, "log entry exists: " #i, NULL); } while (0)

/* log arg layout:  PREINIT(coinit, refs, -)  POSTINIT(hr, coinit, refs)  PREUNINIT(refs, -, -)  POSTUNINIT(refs, -, -) */

SPY_TEST(cia_spy_register_null_arguments)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(NULL, NULL), CIA_E_INVALIDARG);
    ck.QuadPart = 0x1122334455667788ull;
    EXPECT_HR(U(CoRegisterInitializeSpy)(NULL, &ck), CIA_E_INVALIDARG);
    EXPECT_EQ(ck.QuadPart, 0x1122334455667788ull);        /* cookie untouched when the spy is NULL */
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), NULL), CIA_E_INVALIDARG);
    EXPECT_EQ(s.nqi, 0);                                  /* the spy is not even queried */
    EXPECT_EQ(s.ref, 1);
}

SPY_TEST(cia_spy_register_cookie_layout)
{
    static cia_spy s[3]; ULARGE_INTEGER ck[3]; int i;
    for (i = 0; i < 3; i++) { cia_spy_init(&s[i], i + 1); ck[i].QuadPart = 0x5555555555555555ull; }
    for (i = 0; i < 3; i++) {
        EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s[i]), &ck[i]), S_OK);
        EXPECT_EQ(ck[i].HighPart, GetCurrentThreadId());  /* high dword = registering thread id */
        EXPECT_EQ(ck[i].LowPart, i);                      /* low dword = per-thread sequence number, from 0 */
    }
    for (i = 0; i < 3; i++) EXPECT_HR(U(CoRevokeInitializeSpy)(ck[i]), S_OK);
}

SPY_TEST(cia_spy_register_queries_and_holds_reference)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_EQ(s.nqi, 1);
    CHECK(IsEqualGUID(&s.last_qi, &CIA_IID_IInitializeSpy));    /* it asks for IID_IInitializeSpy */
    EXPECT_EQ(s.ref, 2);                                        /* and keeps the AddRef'd pointer returned by QI */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    EXPECT_EQ(s.ref, 1);                                        /* revoke releases it */
}

SPY_TEST(cia_spy_register_qi_failure)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1); s.fail_qi = 1;
    ck.QuadPart = 0;
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), E_NOINTERFACE);
    EXPECT_EQ(ck.QuadPart, ~0ull);                              /* cookie is set to all ones before the QI */
    EXPECT_EQ(s.ref, 1);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    LOG_N(0);                                                   /* nothing was registered */
    cUninit();
    LOG_N(0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), CIA_E_INVALIDARG);
}

SPY_TEST(cia_spy_register_works_without_com_initialized)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    CIA_CHECK_NOTINIT();            /* registering does not initialize COM */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_revoke_invalid_cookies)
{
    static cia_spy s; ULARGE_INTEGER ck, bad;
    cia_spy_init(&s, 1);
    bad.QuadPart = 0;
    EXPECT_HR(U(CoRevokeInitializeSpy)(bad), CIA_E_INVALIDARG);         /* tid 0 never matches */
    bad.QuadPart = ~0ull;
    EXPECT_HR(U(CoRevokeInitializeSpy)(bad), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    bad = ck; bad.LowPart += 1;
    EXPECT_HR(U(CoRevokeInitializeSpy)(bad), CIA_E_INVALIDARG);         /* right thread, unknown sequence number */
    bad = ck; bad.HighPart ^= 1;
    EXPECT_HR(U(CoRevokeInitializeSpy)(bad), CIA_E_INVALIDARG);         /* wrong thread id */
    EXPECT_EQ(s.ref, 2);                                                /* failed revokes released nothing */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), CIA_E_INVALIDARG);          /* already revoked */
    EXPECT_EQ(s.ref, 1);
}

SPY_TEST(cia_spy_revoke_twice_releases_once)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), CIA_E_INVALIDARG);
    EXPECT_EQ(s.ref, 1);
}

static ULARGE_INTEGER g_foreign_cookie;
static HRESULT g_foreign_hr;
static void w_foreign_revoke(void *p) { g_foreign_hr = U(CoRevokeInitializeSpy)(g_foreign_cookie); }
SPY_TEST(cia_spy_revoke_from_other_thread_fails)
{
    static cia_spy s;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &g_foreign_cookie), S_OK);
    g_foreign_hr = S_OK;
    CHECK(cia_run(w_foreign_revoke, NULL));
    EXPECT_HR(g_foreign_hr, CIA_E_INVALIDARG);        /* a spy can only be revoked by the thread that registered it */
    EXPECT_EQ(s.ref, 2);
    EXPECT_HR(U(CoRevokeInitializeSpy)(g_foreign_cookie), S_OK);
}

SPY_TEST(cia_spy_cookie_reuse_after_revoke)
{
    static cia_spy s[4]; ULARGE_INTEGER ck[3], again, fourth; int i;
    for (i = 0; i < 4; i++) cia_spy_init(&s[i], i + 1);
    for (i = 0; i < 3; i++) EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s[i]), &ck[i]), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck[1]), S_OK);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s[1]), &again), S_OK);
    EXPECT_EQ(again.LowPart, 1);                                 /* the freed registration slot is recycled */
    EXPECT_EQ(again.HighPart, GetCurrentThreadId());
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s[3]), &fourth), S_OK);   /* nothing free: next fresh number */
    EXPECT_EQ(fourth.LowPart, 3);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck[0]), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(again), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck[2]), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(fourth), S_OK);
}

/* ----- callbacks --------------------------------------------------------------------------- */
SPY_TEST(cia_spy_callbacks_sta_nesting)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_OK);
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREINIT, 0x6, 0, 0);          /* (dwCoInit, refs before) */
    LOG_IS(1, 1, CIA_EV_POSTINIT, S_OK, 0x6, 1);      /* (hr, dwCoInit, refs after) */
    cia_log_reset();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_FALSE);
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREINIT, 0x6, 1, 0);
    LOG_IS(1, 1, CIA_EV_POSTINIT, S_FALSE, 0x6, 2);
    cia_log_reset();
    cUninit();
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREUNINIT, 2, 0, 0);          /* refs before */
    LOG_IS(1, 1, CIA_EV_POSTUNINIT, 1, 0, 0);         /* refs after */
    cia_log_reset();
    cUninit();
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREUNINIT, 1, 0, 0);
    LOG_IS(1, 1, CIA_EV_POSTUNINIT, 0, 0, 0);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_callbacks_mta)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 7);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    LOG_N(2);
    LOG_IS(0, 7, CIA_EV_PREINIT, 0, 0, 0);            /* COINIT_MULTITHREADED is 0 */
    LOG_IS(1, 7, CIA_EV_POSTINIT, S_OK, 0, 1);
    cia_log_reset();
    cUninit();
    LOG_N(2);
    LOG_IS(0, 7, CIA_EV_PREUNINIT, 1, 0, 0);
    LOG_IS(1, 7, CIA_EV_POSTUNINIT, 0, 0, 0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_callbacks_flag_values_masked)
{
    static const DWORD fl[] = { 0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE };
    static cia_spy s; ULARGE_INTEGER ck; size_t i;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    for (i = 0; i < 8; i++) {
        cia_log_reset();
        EXPECT_HR(cInitEx(NULL, fl[i]), S_OK);
        LOG_N(2);
        LOG_IS(0, 1, CIA_EV_PREINIT, fl[i], 0, 0);
        LOG_IS(1, 1, CIA_EV_POSTINIT, S_OK, fl[i], 1);
        cUninit();
    }
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_callbacks_changed_mode)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    LOG_N(2);                                          /* both callbacks still fire on a failing call */
    LOG_IS(0, 1, CIA_EV_PREINIT, 0, 1, 0);
    LOG_IS(1, 1, CIA_EV_POSTINIT, CIA_CHANGED_MODE, 0, 1);   /* hr passed to the spy, count unchanged */
    cia_log_reset();
    cUninit();
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREUNINIT, 1, 0, 0);
    LOG_IS(1, 1, CIA_EV_POSTUNINIT, 0, 0, 0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_invalid_flags_never_reach_spies)
{
    static cia_spy s; ULARGE_INTEGER ck; static const DWORD bad[] = { 1, 3, 0x10, 0xFFFFFFFF }; size_t i;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    for (i = 0; i < 4; i++) EXPECT_HR(cInitEx(NULL, bad[i]), CIA_E_INVALIDARG);
    LOG_N(0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    cia_drain();
}

SPY_TEST(cia_spy_pre_initialize_result_is_ignored)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1); s.pre_ret = E_FAIL;
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);     /* a failing PreInitialize does not veto the call */
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();                                /* nor PreUninitialize */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_post_initialize_result_replaces_return_value)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1); s.override_post = 1; s.post_ret = E_FAIL;
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), E_FAIL);   /* the spy chooses what CoInitializeEx returns */
    t_note("after PostInitialize returned E_FAIL the thread %s initialized", cia_inited() ? "is still" : "is not");
    s.post_ret = S_FALSE;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    s.post_ret = S_OK;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    s.override_post = 0;
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    cia_drain();
}

SPY_TEST(cia_spy_post_initialize_can_mask_changed_mode)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    s.override_post = 1; s.post_ret = S_OK;
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);     /* the failure was rewritten by the spy ... */
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);   /* ... but the thread is still just an STA */
    s.override_post = 0;
    cUninit();
    CIA_CHECK_NOTINIT();                                /* and only one reference exists */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_two_spies_order_newest_first)
{
    static cia_spy a, b; ULARGE_INTEGER ca, cb;
    cia_spy_init(&a, 1); cia_spy_init(&b, 2);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&a), &ca), S_OK);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&b), &cb), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    LOG_N(4);                                           /* all Pre calls, then all Post calls; most recent registration first */
    LOG_IS(0, 2, CIA_EV_PREINIT, 2, 0, 0);
    LOG_IS(1, 1, CIA_EV_PREINIT, 2, 0, 0);
    LOG_IS(2, 2, CIA_EV_POSTINIT, S_OK, 2, 1);
    LOG_IS(3, 1, CIA_EV_POSTINIT, S_OK, 2, 1);
    cia_log_reset();
    cUninit();
    LOG_N(4);
    LOG_IS(0, 2, CIA_EV_PREUNINIT, 1, 0, 0);
    LOG_IS(1, 1, CIA_EV_PREUNINIT, 1, 0, 0);
    LOG_IS(2, 2, CIA_EV_POSTUNINIT, 0, 0, 0);
    LOG_IS(3, 1, CIA_EV_POSTUNINIT, 0, 0, 0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ca), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(cb), S_OK);
}

SPY_TEST(cia_spy_same_instance_registered_three_times)
{
    static cia_spy s; ULARGE_INTEGER c[3]; int i;
    cia_spy_init(&s, 1);
    for (i = 0; i < 3; i++) EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &c[i]), S_OK);
    EXPECT_EQ(s.ref, 4);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_EQ(cia_log_count(1, CIA_EV_PREINIT), 3);     /* once per registration */
    EXPECT_EQ(cia_log_count(1, CIA_EV_POSTINIT), 3);
    cia_log_reset();
    EXPECT_HR(U(CoRevokeInitializeSpy)(c[1]), S_OK);
    cUninit();
    EXPECT_EQ(cia_log_count(1, CIA_EV_PREUNINIT), 2);
    EXPECT_EQ(cia_log_count(1, CIA_EV_POSTUNINIT), 2);
    EXPECT_HR(U(CoRevokeInitializeSpy)(c[0]), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(c[2]), S_OK);
    EXPECT_EQ(s.ref, 1);
}

SPY_TEST(cia_spy_revoke_in_pre_uninitialize)
{
    static cia_spy a, b; ULARGE_INTEGER ca, cb;
    cia_spy_init(&a, 1); cia_spy_init(&b, 2);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&a), &ca), S_OK);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&b), &cb), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    cia_log_reset();
    b.cookie = cb; b.revoke_in_preuninit = 1;
    cUninit();
    EXPECT_HR(b.revoke_hr, S_OK);                       /* revoking from inside a callback is allowed ... */
    EXPECT_EQ(cia_log_count(2, CIA_EV_PREUNINIT), 1);
    EXPECT_EQ(cia_log_count(2, CIA_EV_POSTUNINIT), 0);  /* ... and the spy gets no PostUninitialize */
    EXPECT_EQ(cia_log_count(1, CIA_EV_POSTUNINIT), 1);
    EXPECT_EQ(b.ref, 1);
    EXPECT_HR(U(CoRevokeInitializeSpy)(cb), CIA_E_INVALIDARG);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ca), S_OK);
}

SPY_TEST(cia_spy_revoked_spy_gets_no_more_calls)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    cUninit();
    LOG_N(0);
}

SPY_TEST(cia_spy_survives_full_uninit_and_fires_again)
{
    static cia_spy s; ULARGE_INTEGER ck; int i;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    for (i = 0; i < 3; i++) {
        cia_log_reset();
        EXPECT_HR(cInitEx(NULL, (i & 1) ? CIA_COINIT_MTA : CIA_COINIT_STA), S_OK);
        cUninit();
        LOG_N(4);
        EXPECT_EQ(cia_log_count(1, CIA_EV_PREINIT), 1);
        EXPECT_EQ(cia_log_count(1, CIA_EV_POSTINIT), 1);
        EXPECT_EQ(cia_log_count(1, CIA_EV_PREUNINIT), 1);
        EXPECT_EQ(cia_log_count(1, CIA_EV_POSTUNINIT), 1);
    }
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_unbalanced_uninit_still_notifies)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    cUninit();                                           /* COM was never initialized on this thread */
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREUNINIT, 0, 0, 0);
    LOG_IS(1, 1, CIA_EV_POSTUNINIT, 0, 0, 0);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_refcounts_balanced_across_events)
{
    static cia_spy s; ULARGE_INTEGER ck; LONG base;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    base = s.ref;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    cUninit(); cUninit();
    EXPECT_EQ(s.ref, base);                              /* every notification AddRefs and Releases the spy around the call */
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    EXPECT_EQ(s.ref, 1);
}

static cia_spy g_other_thread_spy;
static void w_init_other_thread(void *p) { cInitEx(NULL, CIA_COINIT_STA); cUninit(); }
SPY_TEST(cia_spy_is_per_thread)
{
    ULARGE_INTEGER ck;
    cia_spy_init(&g_other_thread_spy, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&g_other_thread_spy), &ck), S_OK);
    CHECK(cia_run(w_init_other_thread, NULL));           /* a different thread's init/uninit is invisible to this spy */
    LOG_N(0);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    LOG_N(2);
    cUninit();
    LOG_N(4);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    { int i; for (i = 0; i < cia_nlog; i++) EXPECT_EQ(cia_log[i].tid, GetCurrentThreadId()); }
}

SPY_TEST(cia_spy_coinitialize_reports_sta_flag)
{
    static cia_spy s; ULARGE_INTEGER ck;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInit(NULL), S_OK);
    LOG_N(2);
    LOG_IS(0, 1, CIA_EV_PREINIT, 2, 0, 0);               /* CoInitialize == CoInitializeEx(STA) */
    LOG_IS(1, 1, CIA_EV_POSTINIT, S_OK, 2, 1);
    cUninit();
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_reserved_pointer_failure_not_reported)
{
    static cia_spy s; ULARGE_INTEGER ck; int dummy = 0;
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cInitEx(&dummy, CIA_COINIT_STA), CIA_E_INVALIDARG);
    LOG_N(0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
    cia_drain();
}

SPY_TEST(cia_spy_ole_initialize_notifications)
{
    static cia_spy s; ULARGE_INTEGER ck;
    REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize));
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(cOleInit(NULL), S_OK);
    EXPECT_EQ(cia_log_count(1, CIA_EV_PREINIT), 1);      /* OleInitialize goes through CoInitializeEx(NULL, STA) */
    EXPECT_EQ(cia_log_count(1, CIA_EV_POSTINIT), 1);
    EXPECT_EQ(cia_log[0].a, 2);
    cia_log_reset();
    cOleUninit();
    EXPECT_EQ(cia_log_count(1, CIA_EV_PREUNINIT), 1);    /* OleUninitialize calls CoUninitialize once */
    EXPECT_EQ(cia_log_count(1, CIA_EV_POSTUNINIT), 1);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

/* many registrations on one thread */
SPY_TEST(cia_spy_many_registrations)
{
    enum { N = 64 };
    static cia_spy s[N]; static ULARGE_INTEGER c[N]; int i;
    for (i = 0; i < N; i++) { cia_spy_init(&s[i], i); EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s[i]), &c[i]), S_OK); EXPECT_EQ(c[i].LowPart, i); }
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_EQ(cia_nlog, 2 * N);
    for (i = 0; i < N; i++) { EXPECT_EQ(cia_log[i].ev, CIA_EV_PREINIT); EXPECT_EQ(cia_log[i].id, N - 1 - i); }   /* newest first */
    for (i = N; i < 2 * N; i++) EXPECT_EQ(cia_log[i].ev, CIA_EV_POSTINIT);
    cUninit();
    for (i = 0; i < N; i++) EXPECT_HR(U(CoRevokeInitializeSpy)(c[i]), S_OK);
    for (i = 0; i < N; i++) EXPECT_EQ(s[i].ref, 1);
}

/* spies and implicit MTA do not interfere */
SPY_TEST(cia_spy_not_notified_by_mta_usage)
{
    static cia_spy s; ULARGE_INTEGER ck; CO_MTA_USAGE_COOKIE c = NULL;
    REQUIRE(U(CoIncrementMTAUsage)); REQUIRE(U(CoDecrementMTAUsage));
    cia_spy_init(&s, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&s), &ck), S_OK);
    EXPECT_HR(U(CoIncrementMTAUsage)(&c), S_OK);
    EXPECT_HR(U(CoDecrementMTAUsage)(c), S_OK);
    LOG_N(0);
    EXPECT_HR(U(CoRevokeInitializeSpy)(ck), S_OK);
}

SPY_TEST(cia_spy_thread_exit_without_revoke_is_safe)
{
    static cia_spy leaked; ULARGE_INTEGER ck;
    cia_spy_init(&leaked, 1);
    EXPECT_HR(U(CoRegisterInitializeSpy)(CIA_SPYP(&leaked), &ck), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    t_note("spy refcount while registered and initialized: %ld", (long)leaked.ref);
    cUninit();
    /* thread returns without revoking; the runtime cleans up its registration at thread exit (must not crash) */
}
