/* CoInitialize / CoInitializeEx / CoUninitialize: flags, nesting counts, mode changes. */
#include "cia.h"

/* Expected apartment of a successful CoInitializeEx(flags): bit 1 (COINIT_APARTMENTTHREADED) selects STA. */
static const struct { DWORD flags; int sta; const char *name; } valid_flags[] = {
    { 0x0, 0, "MTA" },
    { 0x2, 1, "STA" },
    { 0x4, 0, "MTA|DISABLE_OLE1DDE" },
    { 0x6, 1, "STA|DISABLE_OLE1DDE" },
    { 0x8, 0, "MTA|SPEED_OVER_MEMORY" },
    { 0xA, 1, "STA|SPEED_OVER_MEMORY" },
    { 0xC, 0, "MTA|DISABLE_OLE1DDE|SPEED_OVER_MEMORY" },
    { 0xE, 1, "STA|DISABLE_OLE1DDE|SPEED_OVER_MEMORY" },
};

/* Flag values rejected by the (flags & 0xE) == flags test in combase!CoInitializeEx. */
static const DWORD invalid_flags[] = {
    0x1, 0x3, 0x5, 0x7, 0x9, 0xB, 0xD, 0xF,          /* bit 0 (COINIT_MULTITHREADED in the old numbering) */
    0x10, 0x12, 0x1E, 0x20, 0x40, 0x80, 0x100, 0x1000, 0x10000, 0x40000000, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF,
};

/* ----- single valid-flag table ------------------------------------------------------------ */
TEST(cia_init_valid_flags_table)
{
    size_t i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    for (i = 0; i < sizeof valid_flags / sizeof valid_flags[0]; i++) {
        HRESULT hr;
        int t = 0, q = 0;
        CIA_CHECK_NOTINIT();
        hr = cInitEx(NULL, valid_flags[i].flags);
        CHECKF(hr == S_OK, "%s: first init got 0x%08lx", valid_flags[i].name, (unsigned long)hr);
        EXPECT_HR(cia_apt(&t, &q), S_OK);
        if (valid_flags[i].sta) CHECKF(t == CIA_APT_MAINSTA, "%s: type %d", valid_flags[i].name, t);
        else CHECKF(t == CIA_APT_MTA, "%s: type %d", valid_flags[i].name, t);
        CHECKF(q == CIA_Q_NONE, "%s: qualifier %d", valid_flags[i].name, q);
        hr = cInitEx(NULL, valid_flags[i].flags);
        CHECKF(hr == S_FALSE, "%s: second init got 0x%08lx", valid_flags[i].name, (unsigned long)hr);
        cUninit();
        CHECK(cia_inited());
        cUninit();
        CIA_CHECK_NOTINIT();
    }
}

TEST(cia_init_invalid_flags_table)
{
    size_t i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoGetApartmentType));
    for (i = 0; i < sizeof invalid_flags / sizeof invalid_flags[0]; i++) {
        HRESULT hr = cInitEx(NULL, invalid_flags[i]);
        CHECKF(hr == CIA_E_INVALIDARG, "flags 0x%lx got 0x%08lx", (unsigned long)invalid_flags[i], (unsigned long)hr);
        CIA_CHECK_NOTINIT();   /* a rejected call must not have initialized anything */
    }
    cia_drain();
}

TEST(cia_init_invalid_flags_do_not_touch_existing_apartment)
{
    size_t i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    for (i = 0; i < sizeof invalid_flags / sizeof invalid_flags[0]; i++)
        EXPECT_HR(cInitEx(NULL, invalid_flags[i]), CIA_E_INVALIDARG);
    /* none of the failures incremented the count: a single CoUninitialize tears it down */
    cUninit();
    CIA_CHECK_NOTINIT();
    cia_drain();
}

/* ----- pvReserved ------------------------------------------------------------------------------- */
TEST(cia_init_reserved_pointer_must_be_null)
{
    int dummy = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoInitialize)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(cInitEx(&dummy, CIA_COINIT_STA), CIA_E_INVALIDARG);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInitEx(&dummy, CIA_COINIT_MTA), CIA_E_INVALIDARG);
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInit(&dummy), CIA_E_INVALIDARG);          /* CoInitialize forwards to CoInitializeEx(pv, STA) */
    CIA_CHECK_NOTINIT();
    cia_drain();
}

TEST(cia_init_reserved_pointer_rejected_when_already_initialized)
{
    int dummy = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(&dummy, CIA_COINIT_STA), CIA_E_INVALIDARG);   /* does not count as a nested init */
    cUninit();
    CIA_CHECK_NOTINIT();
    cia_drain();
}

/* ----- nesting -------------------------------------------------------------------------------------- */
TEST(cia_init_sta_nesting_s_ok_then_s_false)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    cUninit(); cUninit();
    CHECK(cia_inited());
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_mta_nesting_s_ok_then_s_false)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_FALSE);
    cUninit(); cUninit();
    CHECK(cia_inited());
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_nesting_depth_100)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 100; i++) EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), i == 0 ? S_OK : S_FALSE);
    for (i = 0; i < 99; i++) { cUninit(); if (!cia_inited()) { CHECKF(0, "uninitialized early at %d", i); break; } }
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_nesting_depth_100_mta)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 100; i++) EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), i == 0 ? S_OK : S_FALSE);
    for (i = 0; i < 99; i++) cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_nesting_big_loop)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 3000; i++) { HRESULT hr = cInitEx(NULL, CIA_COINIT_STA); if (hr != (i ? S_FALSE : S_OK)) { CHECKF(0, "i=%d hr=0x%08lx", i, (unsigned long)hr); break; } }
    for (i = 0; i < 2999; i++) cUninit();
    CHECK(cia_inited());
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_second_init_ignores_extra_flag_bits)
{
    /* DISABLE_OLE1DDE / SPEED_OVER_MEMORY do not change the model: nested calls with other flag bits succeed */
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_SPEED), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE | CIA_COINIT_SPEED), S_FALSE);
    cUninit(); cUninit(); cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_mta_second_init_ignores_extra_flag_bits)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA | CIA_COINIT_NOOLE1DDE), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA | CIA_COINIT_SPEED), S_FALSE);
    cUninit(); cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

/* ----- RPC_E_CHANGED_MODE ------------------------------------------------------------------------ */
TEST(cia_init_changed_mode_sta_then_mta)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);   /* unchanged */
    /* The failed call did not add a reference: one CoUninitialize is enough. */
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_changed_mode_mta_then_sta)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), CIA_CHANGED_MODE);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_changed_mode_all_flag_combinations)
{
    size_t i, j;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < sizeof valid_flags / sizeof valid_flags[0]; i++) {
        for (j = 0; j < sizeof valid_flags / sizeof valid_flags[0]; j++) {
            HRESULT hr1 = cInitEx(NULL, valid_flags[i].flags);
            HRESULT hr2 = cInitEx(NULL, valid_flags[j].flags);
            HRESULT want2 = (valid_flags[i].sta == valid_flags[j].sta) ? S_FALSE : CIA_CHANGED_MODE;
            CHECKF(hr1 == S_OK, "%s first 0x%08lx", valid_flags[i].name, (unsigned long)hr1);
            CHECKF(hr2 == want2, "%s then %s: got 0x%08lx want 0x%08lx", valid_flags[i].name, valid_flags[j].name, (unsigned long)hr2, (unsigned long)want2);
            if (SUCCEEDED(hr2) && hr2 == S_FALSE) cUninit();
            cUninit();
            if (cia_inited()) { CHECKF(0, "state leaked after %s/%s", valid_flags[i].name, valid_flags[j].name); cia_drain(); }
        }
    }
}

TEST(cia_init_changed_mode_repeated_failures_do_not_accumulate)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    for (i = 0; i < 50; i++) EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_mode_may_change_after_full_uninit)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    cUninit();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_mode_switch_repeated)
{
    int i;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    for (i = 0; i < 40; i++) {
        DWORD fl = (i & 1) ? CIA_COINIT_STA : CIA_COINIT_MTA;
        EXPECT_HR(cInitEx(NULL, fl), S_OK);
        CIA_CHECK_APT(S_OK, (i & 1) ? CIA_APT_MAINSTA : CIA_APT_MTA, CIA_Q_NONE);
        cUninit();
    }
    CIA_CHECK_NOTINIT();
}

/* ----- CoInitialize ------------------------------------------------------------------------------- */
TEST(cia_init_coinitialize_is_sta)
{
    REQUIRE(U(CoInitialize)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInit(NULL), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    EXPECT_HR(cInit(NULL), S_FALSE);
    cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_coinitialize_then_ex_sta_nests)
{
    REQUIRE(U(CoInitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInit(NULL), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_FALSE);
    cUninit(); cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_coinitialize_then_ex_mta_changed_mode)
{
    REQUIRE(U(CoInitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInit(NULL), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_ex_mta_then_coinitialize_changed_mode)
{
    REQUIRE(U(CoInitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cInit(NULL), CIA_CHANGED_MODE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_combase_and_ole32_agree)
{
    HRESULT (WINAPI *cb_init)(LPVOID, DWORD);
    void (WINAPI *cb_uninit)(void);
    REQUIRE(UIN("combase.dll", CoInitializeEx)); REQUIRE(UIN("combase.dll", CoUninitialize));
    REQUIRE(UIN("ole32.dll", CoInitializeEx)); REQUIRE(UIN("ole32.dll", CoUninitialize));
    cb_init = UIN("combase.dll", CoInitializeEx);
    cb_uninit = UIN("combase.dll", CoUninitialize);
    /* ole32 forwards (through the api-ms-win-core-com-l1-1-0 apiset) to combase: same function, same TLS state */
    CHECK((FARPROC)UIN("ole32.dll", CoInitializeEx) == (FARPROC)cb_init);
    CHECK((FARPROC)UIN("ole32.dll", CoUninitialize) == (FARPROC)cb_uninit);
    EXPECT_HR(UIN("ole32.dll", CoInitializeEx)(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cb_init(NULL, CIA_COINIT_STA), S_FALSE);
    cb_uninit();
    UIN("ole32.dll", CoUninitialize)();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_ole32_coinitialize_export_is_local_thunk)
{
    FARPROC o, c;
    REQUIRE(UIN("ole32.dll", CoInitialize));
    o = (FARPROC)UIN("ole32.dll", CoInitialize);
    c = t_proc_in("combase.dll", "CoInitialize");
    CHECK(c == NULL);   /* combase does not export CoInitialize; only ole32 does */
    CHECK(o != NULL);
}

/* ----- CoUninitialize misuse ---------------------------------------------------------------------- */
TEST(cia_init_uninit_without_init_is_harmless)
{
    int i;
    REQUIRE(U(CoUninitialize)); REQUIRE(U(CoInitializeEx));
    for (i = 0; i < 5; i++) cUninit();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);   /* the unbalanced calls must not have left a negative count */
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_extra_uninit_after_balanced)
{
    REQUIRE(U(CoUninitialize)); REQUIRE(U(CoInitializeEx));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    cUninit();
    cUninit(); cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_extra_uninit_mta)
{
    REQUIRE(U(CoUninitialize)); REQUIRE(U(CoInitializeEx));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    cUninit();
    cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_uninit_does_not_set_last_error_state_visible_hr)
{
    /* CoUninitialize returns void; just make sure calling it twice in a row from nothing leaves the thread usable
     * for both models afterwards. */
    REQUIRE(U(CoUninitialize)); REQUIRE(U(CoInitializeEx));
    cUninit(); cUninit();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    cUninit();
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    cUninit();
}

/* ----- per-thread behaviour ---------------------------------------------------------------------- */
struct two { HRESULT a, b, c; int t, q; HRESULT apthr; };

static void w_init_twice_sta(void *p)
{
    struct two *r = (struct two *)p;
    r->a = cInitEx(NULL, CIA_COINIT_STA);
    r->b = cInitEx(NULL, CIA_COINIT_STA);
    r->apthr = cia_apt(&r->t, &r->q);
    cUninit(); cUninit();
    r->c = cia_apt_hr();
}
TEST(cia_init_worker_thread_sta_nesting)
{
    struct two r; memset(&r, 0, sizeof r);
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(cia_run(w_init_twice_sta, &r));
    EXPECT_HR(r.a, S_OK); EXPECT_HR(r.b, S_FALSE); EXPECT_HR(r.apthr, S_OK);
    /* the first STA in the process is the main STA (no other STA exists while this test runs) */
    EXPECT_EQ(r.t, CIA_APT_MAINSTA); EXPECT_EQ(r.q, CIA_Q_NONE);
    EXPECT_HR(r.c, CIA_NOTINIT);
}

static void w_changed_mode(void *p)
{
    struct two *r = (struct two *)p;
    r->a = cInitEx(NULL, CIA_COINIT_MTA);
    r->b = cInitEx(NULL, CIA_COINIT_STA);
    cUninit();
    r->c = cia_apt_hr();
}
TEST(cia_init_worker_thread_changed_mode)
{
    struct two r; memset(&r, 0, sizeof r);
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    CHECK(cia_run(w_changed_mode, &r));
    EXPECT_HR(r.a, S_OK); EXPECT_HR(r.b, CIA_CHANGED_MODE); EXPECT_HR(r.c, CIA_NOTINIT);
}

static void w_uninit_only(void *p) { cUninit(); cUninit(); *(HRESULT *)p = cia_apt_hr(); }
TEST(cia_init_other_thread_uninit_does_not_affect_this_thread)
{
    HRESULT w = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CHECK(cia_run(w_uninit_only, &w));
    EXPECT_HR(w, CIA_NOTINIT);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

static void w_apt_only(void *p) { *(HRESULT *)p = cia_apt_hr(); }
TEST(cia_init_apartment_is_per_thread_for_sta)
{
    HRESULT w = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CHECK(cia_run(w_apt_only, &w));
    EXPECT_HR(w, CIA_NOTINIT);   /* an STA does not make other threads initialized */
    cUninit();
}

static void w_init_sta_ret(void *p) { struct two *r = (struct two *)p; r->a = cInitEx(NULL, CIA_COINIT_STA); r->apthr = cia_apt(&r->t, &r->q); cUninit(); }
TEST(cia_init_two_stas_main_and_secondary)
{
    struct two r; memset(&r, 0, sizeof r);
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CHECK(cia_run(w_init_sta_ret, &r));
    EXPECT_HR(r.a, S_OK); EXPECT_HR(r.apthr, S_OK);
    EXPECT_EQ(r.t, CIA_APT_STA);          /* second STA is a plain STA ... */
    EXPECT_EQ(r.q, CIA_Q_NONE);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);   /* ... and the first one remains the main STA */
    cUninit();
}

static void w_init_mta_ret(void *p) { struct two *r = (struct two *)p; r->a = cInitEx(NULL, CIA_COINIT_MTA); r->apthr = cia_apt(&r->t, &r->q); cUninit(); }
TEST(cia_init_mta_worker_coexists_with_sta_main)
{
    struct two r; memset(&r, 0, sizeof r);
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    CHECK(cia_run(w_init_mta_ret, &r));
    EXPECT_HR(r.a, S_OK); EXPECT_HR(r.apthr, S_OK);
    EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_NONE);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_init_sta_worker_coexists_with_mta_main)
{
    struct two r; memset(&r, 0, sizeof r);
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    CHECK(cia_run(w_init_sta_ret, &r));
    EXPECT_HR(r.a, S_OK); EXPECT_HR(r.apthr, S_OK);
    EXPECT_EQ(r.t, CIA_APT_MAINSTA);     /* no other STA: it is the main STA */
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

/* A thread that never initialized COM sees the process MTA as an implicit MTA while another thread holds it. */
static void w_hold_mta(void *p) { *(HRESULT *)p = cInitEx(NULL, CIA_COINIT_MTA); }
static void w_release_mta(void *p) { cUninit(); (void)p; }
TEST(cia_init_uninitialized_thread_sees_implicit_mta)
{
    cia_w w; HRESULT hr = 0; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    CIA_CHECK_NOTINIT();
    if (!cia_w_start(&w)) return;
    CHECK(cia_w_call(&w, w_hold_mta, &hr));
    EXPECT_HR(hr, S_OK);
    /* this thread never called CoInitializeEx */
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);
    CHECK(cia_w_call(&w, w_release_mta, NULL));
    CIA_CHECK_NOTINIT();
    cia_w_stop(&w);
    (void)r;
}

TEST(cia_init_mta_survives_until_last_thread_uninitializes)
{
    cia_w a; HRESULT hr = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, w_hold_mta, &hr)); EXPECT_HR(hr, S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);     /* per-thread count: first init on THIS thread is S_OK, not S_FALSE */
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_IMPLICIT_MTA);   /* worker still holds the MTA */
    CHECK(cia_w_call(&a, w_release_mta, NULL));
    CIA_CHECK_NOTINIT();
    cia_w_stop(&a);
}

TEST(cia_init_mta_two_workers_share_process_mta)
{
    cia_w a, b; HRESULT ha = 0, hb = 0; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    if (!cia_w_start(&b)) { cia_w_stop(&a); return; }
    CHECK(cia_w_call(&a, w_hold_mta, &ha)); CHECK(cia_w_call(&b, w_hold_mta, &hb));
    EXPECT_HR(ha, S_OK); EXPECT_HR(hb, S_OK);          /* each thread has its own count */
    CHECK(cia_w_call(&a, cia_cmd_apt, &r));
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_NONE);
    CHECK(cia_w_call(&a, w_release_mta, NULL));
    CHECK(cia_w_call(&a, cia_cmd_apt, &r));             /* a is no longer initialized but b keeps the MTA alive */
    EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MTA); EXPECT_EQ(r.q, CIA_Q_IMPLICIT_MTA);
    CHECK(cia_w_call(&b, w_release_mta, NULL));
    CHECK(cia_w_call(&a, cia_cmd_apt, &r));
    EXPECT_HR(r.hr, CIA_NOTINIT);
    cia_w_stop(&a); cia_w_stop(&b);
}

TEST(cia_init_sta_thread_without_mta_has_no_implicit_apartment)
{
    cia_w a; HRESULT hr = 0; struct cia_apt_res r;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, w_init_sta_ret, &(struct two){0}));
    CHECK(cia_w_call(&a, cia_cmd_apt, &r));
    EXPECT_HR(r.hr, CIA_NOTINIT);
    cia_w_stop(&a);
    (void)hr;
}

/* A long-lived STA worker: init/uninit on it while the owner thread stays STA too. */
TEST(cia_init_sta_worker_lifecycle_with_commands)
{
    cia_w a; HRESULT hr = 0; struct cia_apt_res r; int n = 0;
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize));
    if (!cia_w_start(&a)) return;
    CHECK(cia_w_call(&a, cia_cmd_init_sta, &hr)); EXPECT_HR(hr, S_OK);
    CHECK(cia_w_call(&a, cia_cmd_init_sta, &hr)); EXPECT_HR(hr, S_FALSE);
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, CIA_CHANGED_MODE);
    CHECK(cia_w_call(&a, cia_cmd_apt, &r)); EXPECT_HR(r.hr, S_OK); EXPECT_EQ(r.t, CIA_APT_MAINSTA);
    CHECK(cia_w_call(&a, cia_cmd_drain, &n)); EXPECT_EQ(n, 2);
    CHECK(cia_w_call(&a, cia_cmd_apt, &r)); EXPECT_HR(r.hr, CIA_NOTINIT);
    CHECK(cia_w_call(&a, cia_cmd_init_mta, &hr)); EXPECT_HR(hr, S_OK);      /* free to pick the other model now */
    CHECK(cia_w_call(&a, cia_cmd_drain, &n)); EXPECT_EQ(n, 1);
    cia_w_stop(&a);
}

/* ----- exports / convenience ------------------------------------------------------------------- */
TEST(cia_init_exports_present)
{
    static const char *const ole32_names[] = {
        "CoInitialize", "CoInitializeEx", "CoUninitialize", "CoGetApartmentType", "CoIncrementMTAUsage", "CoDecrementMTAUsage",
        "CoGetCurrentProcess", "CoGetCurrentLogicalThreadId", "CoGetContextToken", "CoGetObjectContext", "CoInitializeSecurity",
        "CoRegisterInitializeSpy", "CoRevokeInitializeSpy", "CoFreeUnusedLibraries", "CoFreeUnusedLibrariesEx", "CoFreeAllLibraries",
        "CoBuildVersion", "CoIsOle1Class", "OleInitialize", "OleUninitialize", "CoInitializeWOW", "CoGetApartmentID",
    };
    static const char *const combase_names[] = {
        "CoInitializeEx", "CoUninitialize", "CoGetApartmentType", "CoIncrementMTAUsage", "CoDecrementMTAUsage",
        "CoGetCurrentProcess", "CoGetCurrentLogicalThreadId", "CoGetContextToken", "CoGetObjectContext", "CoInitializeSecurity",
        "CoRegisterInitializeSpy", "CoRevokeInitializeSpy", "CoFreeUnusedLibraries", "CoFreeUnusedLibrariesEx", "CoGetApartmentID",
        "CoInitializeWOW",
    };
    size_t i;
    for (i = 0; i < sizeof ole32_names / sizeof *ole32_names; i++)
        CHECKF(t_proc_in("ole32.dll", ole32_names[i]) != NULL, "ole32 missing %s", ole32_names[i]);
    for (i = 0; i < sizeof combase_names / sizeof *combase_names; i++)
        CHECKF(t_proc_in("combase.dll", combase_names[i]) != NULL, "combase missing %s", combase_names[i]);
    /* CoInitialize, CoBuildVersion, CoIsOle1Class, OleInitialize/OleUninitialize, CoFreeAllLibraries live in ole32 only */
    CHECK(t_proc_in("combase.dll", "CoInitialize") == NULL);
    CHECK(t_proc_in("combase.dll", "OleInitialize") == NULL);
    CHECK(t_proc_in("combase.dll", "CoBuildVersion") == NULL);
}

TEST(cia_init_forwarded_exports_resolve_into_combase)
{
    static const char *const names[] = {
        "CoInitializeEx", "CoUninitialize", "CoGetApartmentType", "CoIncrementMTAUsage", "CoDecrementMTAUsage",
        "CoGetCurrentProcess", "CoGetCurrentLogicalThreadId", "CoGetContextToken", "CoGetObjectContext", "CoInitializeSecurity",
        "CoRegisterInitializeSpy", "CoRevokeInitializeSpy", "CoFreeUnusedLibraries", "CoFreeUnusedLibrariesEx", "CoGetApartmentID",
    };
    size_t i;
    for (i = 0; i < sizeof names / sizeof *names; i++) {
        FARPROC a = t_proc_in("ole32.dll", names[i]), b = t_proc_in("combase.dll", names[i]);
        CHECKF(a != NULL && a == b, "%s: ole32 %p combase %p", names[i], (void *)a, (void *)b);
    }
}
