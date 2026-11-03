/* Additional init/uninit edge cases. */
#include "cia.h"

TEST(cia_extra_sta_after_uninit_reports_notinit_repeatedly)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    for (int i = 0; i < 3; i++) {
        EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
        CIA_CHECK_APT(S_OK, CIA_APT_MAINSTA, CIA_Q_NONE);
        cUninit();
        CIA_CHECK_NOTINIT();
    }
}

TEST(cia_extra_mta_then_uninit_reports_notinit)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), S_OK);
    CIA_CHECK_APT(S_OK, CIA_APT_MTA, CIA_Q_NONE);
    cUninit();
    CIA_CHECK_NOTINIT();
}

TEST(cia_extra_failed_changed_mode_needs_no_uninit)
{
    REQUIRE(U(CoInitializeEx)); REQUIRE(U(CoUninitialize)); REQUIRE(U(CoGetApartmentType));
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_STA), S_OK);
    EXPECT_HR(cInitEx(NULL, CIA_COINIT_MTA), CIA_CHANGED_MODE);
    cUninit();                      /* single uninit balances the single success */
    CIA_CHECK_NOTINIT();
}
