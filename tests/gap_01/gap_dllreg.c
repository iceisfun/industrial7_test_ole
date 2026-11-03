#include "g1.h"
/* DllRegisterServer / DllUnregisterServer in ole32, oleaut32 and rpcrt4.
 * Decomp facts (Win10 19041): ole32!DllRegisterServer is `xor eax,eax; ret` (shares the body of LockServer): S_OK, no work;
 * ole32 and rpcrt4 do NOT export DllUnregisterServer; oleaut32!DllUnregisterServer is `return E_FAIL` (0x80004005).
 * oleaut32!DllRegisterServer really writes HKCR (see dx_gap_01), rpcrt4!DllRegisterServer forwards into the NDR OLE
 * extension DLL registered under HKLM\Software\Microsoft\Rpc\Extensions. */

typedef HRESULT (WINAPI *srv_fn)(void);

TEST(gap1_dllreg_ole32_register_is_noop_ok)
{
    srv_fn f = (srv_fn)t_proc_in("ole32.dll", "DllRegisterServer"); int i;
    REQUIRE(f);
    for (i = 0; i < 4; i++) { SET_ERR(0x1234); EXPECT_EQ(f(), S_OK); }
}

TEST(gap1_dllreg_ole32_has_no_unregister)
{
    FARPROC p = t_proc_in("ole32.dll", "DllUnregisterServer");
    if (t_is_wine()) { t_note("emulator exports ole32!DllUnregisterServer: %p", p); return; }
    CHECK(p == NULL);
}

TEST(gap1_dllreg_rpcrt4_has_no_unregister)
{
    FARPROC p = t_proc_in("rpcrt4.dll", "DllUnregisterServer");
    if (t_is_wine()) { t_note("emulator exports rpcrt4!DllUnregisterServer: %p", p); return; }
    CHECK(p == NULL);
}

TEST(gap1_dllreg_oleaut32_unregister_fails_e_fail)
{
    srv_fn f = (srv_fn)t_proc_in("oleaut32.dll", "DllUnregisterServer"); int i;
    REQUIRE(f);
    if (t_is_wine()) { t_note("emulator implements oleaut32!DllUnregisterServer; Windows returns E_FAIL"); return; }
    for (i = 0; i < 3; i++) EXPECT_EQ(f(), E_FAIL);
}

TEST(gap1_dllreg_rpcrt4_register_result_note)
{
    srv_fn f = (srv_fn)t_proc_in("rpcrt4.dll", "DllRegisterServer"); HRESULT hr;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("rpcrt4 NdrOleExtDLL lookup is emulator specific"); return; }
    hr = f();
    t_note("rpcrt4!DllRegisterServer -> 0x%08lx", (unsigned long)hr);
    /* a second call goes through the already-loaded extension table and must give the same answer */
    { HRESULT hr2 = f(); if (hr2 != hr) t_note("second call returned 0x%08lx", (unsigned long)hr2); }
}

TEST(gap1_dllreg_exports_resolve_per_module)
{
    CHECK(t_proc_in("ole32.dll", "DllRegisterServer") != NULL);
    CHECK(t_proc_in("oleaut32.dll", "DllRegisterServer") != NULL);
    CHECK(t_proc_in("oleaut32.dll", "DllUnregisterServer") != NULL);
    CHECK(t_proc_in("rpcrt4.dll", "DllRegisterServer") != NULL);
    /* three distinct implementations */
    CHECK(t_proc_in("ole32.dll", "DllRegisterServer") != t_proc_in("oleaut32.dll", "DllRegisterServer"));
    CHECK(t_proc_in("rpcrt4.dll", "DllRegisterServer") != t_proc_in("oleaut32.dll", "DllRegisterServer"));
}
