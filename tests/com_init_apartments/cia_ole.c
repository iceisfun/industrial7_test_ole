/* OleInitialize / OleUninitialize layered on CoInitializeEx, and CoInitializeSecurity (once per process, child processes). */
#include "cia.h"

#define OLE_TEST(name) \
    static void body_##name(void *unused); \
    TEST(name) { REQUIRE(U(OleInitialize)); REQUIRE(U(OleUninitialize)); REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); \
                 REQUIRE(U(CoGetApartmentType)); cia_run(body_##name, NULL); } \
    static void body_##name(void *unused)

/* Each OLE_TEST runs on a fresh thread, so the thread's OLE init count starts at zero. */

OLE_TEST(cia_ole_basic_nesting)
{
    EXPECT_HR(cOleInit(NULL), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    EXPECT_HR(cOleInit(NULL), S_FALSE);
    EXPECT_HR(cOleInit(NULL), S_FALSE);
    cOleUninit(); cOleUninit();
    CHECK(cia_inited());
    cOleUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_deep_nesting)
{
    int i;
    for (i = 0; i < 100; i++) EXPECT_HR(cOleInit(NULL), i == 0 ? S_OK : S_FALSE);
    for (i = 0; i < 99; i++) cOleUninit();
    CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
    cOleUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_reserved_pointer)
{
    int dummy;
    EXPECT_HR(cOleInit(&dummy), CIA_E_INVALIDARG);
    CIA_CHECK_NOTINIT();
    cOleUninit();                                        /* unbalanced: harmless */
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cOleInit(NULL), S_OK);                     /* the failed attempt left no residue */
    cOleUninit();
    CIA_CHECK_NOTINIT();
    cia_drain();
}

OLE_TEST(cia_ole_after_coinitialize_returns_s_ok_first)
{
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cOleInit(NULL), S_OK);                     /* first OleInitialize is S_OK even if COM is already up */
    EXPECT_HR(cOleInit(NULL), S_FALSE);
    cUninit();                                           /* COM count: 3 -> 2 */
    cOleUninit();                                        /* OLE count 2 -> 1, COM 2 -> 1 */
    CHECK(cia_inited());
    cOleUninit();                                        /* OLE 1 -> 0, COM 1 -> 0 */
    CIA_CHECK_NOTINIT();
    cOleUninit();                                        /* extra: no effect */
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_com_and_ole_counts_are_separate)
{
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cOleInit(NULL), S_OK);
    cOleUninit();                                        /* drops the COM reference taken by OleInitialize only */
    CHECK(cia_inited());
    cUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_uninit_without_init)
{
    int i;
    for (i = 0; i < 4; i++) cOleUninit();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cOleInit(NULL), S_OK);                     /* no negative count was recorded */
    cOleUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_extra_uninit_does_not_underflow)
{
    EXPECT_HR(cOleInit(NULL), S_OK);
    cOleUninit();
    cOleUninit(); cOleUninit();
    EXPECT_HR(cOleInit(NULL), S_OK);
    EXPECT_HR(cOleInit(NULL), S_FALSE);
    cOleUninit(); cOleUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_changed_mode_on_mta_thread)
{
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cOleInit(NULL), CIA_CHANGED_MODE);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cOleUninit();                                        /* nothing to undo: OLE was never initialized, MTA is untouched */
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_changed_mode_repeated)
{
    int i;
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    for (i = 0; i < 10; i++) { EXPECT_HR(cOleInit(NULL), CIA_CHANGED_MODE); cOleUninit(); }
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

struct ole2r { HRESULT a, b; };
static void w_ole_twice(void *p)
{
    struct ole2r *r = (struct ole2r *)p;
    r->a = cOleInit(NULL); r->b = cOleInit(NULL);
    cOleUninit(); cOleUninit();
}
OLE_TEST(cia_ole_is_per_thread)
{
    struct ole2r r = { E_FAIL, E_FAIL };
    EXPECT_HR(cOleInit(NULL), S_OK);
    CHECK(cia_run(w_ole_twice, &r));
    EXPECT_HR(r.a, S_OK);                                /* the worker has its own OLE count */
    EXPECT_HR(r.b, S_FALSE);
    EXPECT_HR(cOleInit(NULL), S_FALSE);                  /* and did not disturb this thread's */
    cOleUninit(); cOleUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_mixed_nesting_with_ex_flags)
{
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA | CIA_COINIT_NOOLE1DDE), S_OK);
    EXPECT_HR(cOleInit(NULL), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_FALSE);
    cUninit(); cOleUninit(); cUninit();
    CIA_CHECK_NOTINIT();
}

OLE_TEST(cia_ole_state_survives_full_com_uninit)
{
    /* Windows quirk: CoUninitialize does not reset the OLE count, so after a full COM uninit
     * the next OleInitialize still reports S_FALSE ("OLE already initialized"). */
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cOleInit(NULL), S_OK);
    cUninit(); cUninit();
    CIA_CHECK_NOTINIT();
    EXPECT_HR(cOleInit(NULL), S_FALSE);
    cOleUninit();
    cia_drain();
}

OLE_TEST(cia_ole_coinitialize_ex_mta_then_ole_does_not_leak_com_ref)
{
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    EXPECT_HR(cOleInit(NULL), CIA_CHANGED_MODE);
    EXPECT_HR(cOleInit(NULL), CIA_CHANGED_MODE);
    cUninit();                                           /* a single CoUninitialize ends the MTA: failures took no references */
    CIA_CHECK_NOTINIT();
    cOleUninit(); cOleUninit();
}

OLE_TEST(cia_ole_ole32_only_exports)
{
    CHECK(t_proc_in("combase.dll", "OleInitialize") == NULL);
    CHECK(t_proc_in("combase.dll", "OleUninitialize") == NULL);
    CHECK(t_proc_in("ole32.dll", "OleInitialize") != NULL);
    CHECK(t_proc_in("ole32.dll", "OleUninitialize") != NULL);
    CHECK(t_proc_in("ole32.dll", "OleInitializeWOW") != NULL);
}

/* ===== CoInitializeSecurity: process-wide, once only; each scenario runs in its own child process ===== */

CHILD(cia_sec)
{
    const char *sc = argc > 1 ? argv[1] : "";
    int bad = 0;
    HRESULT hr;
    int dummy = 0;
    if (!U(CoInitializeSecurity) || !U(CoInitializeEx) || !U(CoUninitialize)) return 0xC0DE0002;

#define SEC_OK() U(CoInitializeSecurity)(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL)

    if (!strcmp(sc, "uninit")) {
        /* before any COM initialization on the thread */
        CCHK_HR(SEC_OK(), CIA_NOTINIT);
        CCHK_HR(SEC_OK(), CIA_NOTINIT);                 /* ... and it stays "not initialized", never "too late" */
        CCHK_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
        CCHK_HR(SEC_OK(), S_OK);                         /* the earlier failures did not consume the one-shot */
        cUninit();
    } else if (!strcmp(sc, "once_sta") || !strcmp(sc, "once_mta")) {
        CCHK_HR(cInitEx(NULL, !strcmp(sc, "once_sta") ? CIA_COINIT_STA : CIA_COINIT_MTA), S_OK);
        CCHK_HR(SEC_OK(), S_OK);
        CCHK_HR(SEC_OK(), CIA_TOO_LATE);
        CCHK_HR(SEC_OK(), CIA_TOO_LATE);
        /* different arguments do not matter either */
        CCHK_HR(U(CoInitializeSecurity)(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IDENTIFY, NULL, 0, NULL), CIA_TOO_LATE);
        CCHK_HR(U(CoInitializeSecurity)(NULL, 0, NULL, NULL, RPC_C_AUTHN_LEVEL_NONE, RPC_C_IMP_LEVEL_ANONYMOUS, NULL, 0, NULL), CIA_TOO_LATE);
        cUninit();
    } else if (!strcmp(sc, "invalid_args_do_not_consume")) {
        static const struct { LONG cAuth; DWORD authn, imp, caps; int r1, r3; } bad_args[] = {
            /* dwImpLevel must be 1..4 */
            { -1, RPC_C_AUTHN_LEVEL_DEFAULT, 0, 0, 0, 0 },
            { -1, RPC_C_AUTHN_LEVEL_DEFAULT, 5, 0, 0, 0 },
            { -1, RPC_C_AUTHN_LEVEL_DEFAULT, 0xFFFFFFFF, 0, 0, 0 },
            /* dwAuthnLevel must be <= 6 */
            { -1, 7, RPC_C_IMP_LEVEL_IMPERSONATE, 0, 0, 0 },
            { -1, 0xFFFFFFFF, RPC_C_IMP_LEVEL_IMPERSONATE, 0, 0, 0 },
            /* undefined capability bits */
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0x10000, 0, 0 },
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0x80, 0, 0 },
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0x80000000, 0, 0 },
            /* static + dynamic cloaking are mutually exclusive */
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0x60, 0, 0 },
            /* EOAC_ACCESS_CONTROL and EOAC_APPID together */
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0xC, 0, 0 },
            /* EOAC_SECURE_REFS requires an authentication level above NONE */
            { -1, RPC_C_AUTHN_LEVEL_NONE, RPC_C_IMP_LEVEL_IMPERSONATE, 0x2, 0, 0 },
            /* reserved pointers must be NULL */
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0, 1, 0 },
            { -1, 0, RPC_C_IMP_LEVEL_IMPERSONATE, 0, 0, 1 },
        };
        size_t i;
        CCHK_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
        for (i = 0; i < sizeof bad_args / sizeof bad_args[0]; i++) {
            hr = U(CoInitializeSecurity)(NULL, bad_args[i].cAuth, NULL, bad_args[i].r1 ? &dummy : NULL, bad_args[i].authn,
                                         bad_args[i].imp, NULL, bad_args[i].caps, bad_args[i].r3 ? &dummy : NULL);
            if (hr != CIA_E_INVALIDARG && !bad) bad = 1000 + (int)i;
        }
        /* an SD pointer that is not a valid security descriptor */
        { SECURITY_DESCRIPTOR sd; memset(&sd, 0xAA, sizeof sd);
          hr = U(CoInitializeSecurity)(&sd, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, 0, NULL);
          CCHK_HR(hr, CIA_E_INVALIDARG); }
        CCHK_HR(SEC_OK(), S_OK);                         /* none of the failures used up the call */
        CCHK_HR(SEC_OK(), CIA_TOO_LATE);
        cUninit();
    } else if (!strcmp(sc, "thread_irrelevant")) {
        /* the one-shot is process-wide, not per thread / per apartment */
        CCHK_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
        CCHK_HR(SEC_OK(), S_OK);
        cUninit();
        CCHK_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
        hr = SEC_OK();
        /* after a full uninit/reinit the process-wide state may or may not be reset */
        if (hr != CIA_TOO_LATE && hr != S_OK && !bad) bad = __LINE__;
        cUninit();
    } else {
        return 0xC0DE0003;
    }
    return bad;
}

static void run_sec(const char *scenario)
{
    DWORD rc;
    REQUIRE(U(CoInitializeSecurity));
    rc = t_run_child("cia_sec", scenario, 30000);
    CHECKF(rc == 0, "scenario %s: child exit 0x%lx (line number of first failed check, or DEAD000x)", scenario, (unsigned long)rc);
}
TEST(cia_sec_requires_initialized_com)           { run_sec("uninit"); }
TEST(cia_sec_once_per_process_sta)               { run_sec("once_sta"); }
TEST(cia_sec_once_per_process_mta)               { run_sec("once_mta"); }
TEST(cia_sec_invalid_arguments_do_not_consume_one_shot) { run_sec("invalid_args_do_not_consume"); }
TEST(cia_sec_one_shot_across_apartments)         { run_sec("thread_irrelevant"); }
