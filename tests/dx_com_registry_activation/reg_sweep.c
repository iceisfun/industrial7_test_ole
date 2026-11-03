/* The area's own housekeeping: stale registrations of dead olet processes are swept, own keys are tracked. */
#include "dxcommon.h"

TEST(dxsweep_removes_stale_keys_of_dead_pids) {
    DX_BEGIN();
    GUID g, i; DWORD fake = 0x7FFFFFF0; char ck[128], ik[128], pr[80], gs[40];
    dx_newguid(&g, DXK_NORMAL); memcpy(g.Data4 + 4, &fake, 4); dx_guidstr(&g, gs); snprintf(ck, sizeof ck, "CLSID\\%s", gs);
    dx_newguid(&i, DXK_IFACE); memcpy(i.Data4 + 4, &fake, 4); dx_guidstr(&i, gs); snprintf(ik, sizeof ik, "Interface\\%s", gs);
    snprintf(pr, sizeof pr, "olet%lu.Stale.1", fake);
    dx_set(ck, NULL, "stale"); dx_set(ik, NULL, "stale"); dx_set(pr, NULL, "stale");
    CHECK(dx_exists(ck) && dx_exists(ik) && dx_exists(pr));
    dx_sweep_stale();
    CHECK(!dx_exists(ck)); CHECK(!dx_exists(ik)); CHECK(!dx_exists(pr));
}
TEST(dxsweep_keeps_foreign_and_live_keys) {
    DX_BEGIN();
    GUID g; char ck[128], gs[40], foreign[128];
    dx_newguid(&g, DXK_NORMAL); dx_guidstr(&g, gs); snprintf(ck, sizeof ck, "CLSID\\%s", gs); dx_track(ck); dx_set(ck, NULL, "mine");
    snprintf(foreign, sizeof foreign, "CLSID\\{12345678-1234-1234-1234-%012lu}", GetCurrentProcessId()); dx_track(foreign); dx_set(foreign, NULL, "not olet");
    dx_sweep_stale();
    CHECK(dx_exists(ck)); CHECK(dx_exists(foreign));   /* only dead-process "olet" keys go */
}
TEST(dxsweep_registry_roundtrip_helpers) {
    DX_BEGIN();
    char v[64] = "x";
    char sub[100]; snprintf(sub, sizeof sub, "olet%lu.helpers\\a\\b", GetCurrentProcessId()); dx_track(sub);
    CHECK(dx_set(sub, "n", "value") == 0); CHECK(dx_get(sub, "n", v, sizeof v) == 0); EXPECT_STR(v, "value");
    CHECK(dx_setdw(sub, "d", 7) == 0); CHECK(dx_get(sub, "missing", v, sizeof v) != 0);
    dx_del(sub); CHECK(!dx_exists(sub));
}
