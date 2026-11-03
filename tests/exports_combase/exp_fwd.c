/* ole32.dll / oleaut32.dll forwarders that land in combase.dll: a forwarded GetProcAddress must resolve to the
 * target module's real function (and never to a stub inside the forwarding DLL). */
#define EXP_WINE_TOLERANT
#include "exp_common.h"
#include <objbase.h>
#include <combaseapi.h>
#include <propidl.h>
#define FWDT(nm, tab, lo, hi, dll) TEST(nm) { exp_check_fwd_range(t_module(dll), tab, lo, (hi) < sizeof(tab)/sizeof(tab[0]) ? (hi) : sizeof(tab)/sizeof(tab[0]), dll); }

FWDT(exports_combase_ole32_fwd_000, ole_fwd_name,   0,  24, "ole32.dll")
FWDT(exports_combase_ole32_fwd_024, ole_fwd_name,  24,  48, "ole32.dll")
FWDT(exports_combase_ole32_fwd_048, ole_fwd_name,  48,  72, "ole32.dll")
FWDT(exports_combase_ole32_fwd_072, ole_fwd_name,  72,  96, "ole32.dll")
FWDT(exports_combase_ole32_fwd_096, ole_fwd_name,  96, 120, "ole32.dll")
FWDT(exports_combase_ole32_fwd_120, ole_fwd_name, 120, 144, "ole32.dll")
FWDT(exports_combase_ole32_fwd_144, ole_fwd_name, 144, 168, "ole32.dll")
FWDT(exports_combase_ole32_fwd_168, ole_fwd_name, 168, 192, "ole32.dll")
FWDT(exports_combase_ole32_fwd_192, ole_fwd_name, 192, 216, "ole32.dll")
FWDT(exports_combase_ole32_fwd_216, ole_fwd_name, 216, 240, "ole32.dll")
FWDT(exports_combase_ole32_fwd_240, ole_fwd_name, 240, 264, "ole32.dll")
FWDT(exports_combase_ole32_fwd_264, ole_fwd_name, 264, 300, "ole32.dll")
/* ole32 exports that forward to combase by ordinal (ordinal-only combase exports 110..112) */
FWDT(exports_combase_ole32_fwd_by_ordinal, ole_fwd_ord, 0, 100, "ole32.dll")
FWDT(exports_combase_oleaut32_fwd_hwnd_user, oa_fwd_name, 0, 100, "oleaut32.dll")

TEST(exports_combase_ole32_fwd_table_sizes) {
    CHECK(OLE_FWD_NAME_N > 250);
    EXPECT_EQ(OLE_FWD_ORD_N, 3);
    EXPECT_EQ(OA_FWD_NAME_N, 8);
}
TEST(exports_combase_ole32_fwd_ordinals_110_111_112) {
    /* CoSetMessageDispatcher -> combase #110, CoHandlePriorityEventsFromMessagePump -> #111, CoRegisterChannelHook -> #112 */
    HMODULE o = t_module("ole32.dll"), c = t_module("combase.dll");
    if (!o || !c) { t_skip("module missing"); return; }
    struct { const char *n; unsigned ord; } t[] = { { "CoSetMessageDispatcher", 110 }, { "CoHandlePriorityEventsFromMessagePump", 111 }, { "CoRegisterChannelHook", 112 } };
    for (int i = 0; i < 3; i++) {
        FARPROC a = GetProcAddress(o, t[i].n), b = GetProcAddress(c, (LPCSTR)(ULONG_PTR)t[i].ord);
        CHECKF(a != NULL, "%s", t[i].n);
        CHECK(b != NULL);
        CHECKF(a == b, "%s", t[i].n);
        CHECK(exp_find_ord(t[i].ord) && !exp_find_ord(t[i].ord)->name);
    }
}
TEST(exports_combase_ole32_fwd_not_in_ole32_image) {
    HMODULE o = t_module("ole32.dll");
    if (!o) { t_skip("no ole32"); return; }
    static const char *const n[] = { "CoInitializeEx", "CoUninitialize", "CoCreateInstance", "CoTaskMemAlloc", "CoTaskMemFree", "StringFromGUID2", "CLSIDFromString" };
    for (int i = 0; i < 7; i++) {
        FARPROC p = GetProcAddress(o, n[i]);
        CHECKF(p != NULL, "%s", n[i]);
        if (p && !t_is_wine()) CHECKF(!exp_in_image(o, p), "%s resolved inside ole32", n[i]);
        if (p && !t_is_wine()) CHECKF(p == GetProcAddress(t_module("combase.dll"), n[i]), "%s", n[i]);
    }
}
TEST(exports_combase_ole32_typed_forwarders) {
    /* typed via SDK headers; ole32 first in the module list, combase pinned explicitly */
    HMODULE o = t_module("ole32.dll"); if (!o) { t_skip("no ole32"); return; }
    CHECK((FARPROC)UIN("ole32.dll", CoInitializeEx) == (FARPROC)UIN("combase.dll", CoInitializeEx));
    CHECK((FARPROC)UIN("ole32.dll", CoUninitialize) == (FARPROC)UIN("combase.dll", CoUninitialize));
    CHECK((FARPROC)UIN("ole32.dll", CoTaskMemAlloc) == (FARPROC)UIN("combase.dll", CoTaskMemAlloc));
    CHECK((FARPROC)UIN("ole32.dll", CoGetClassObject) == (FARPROC)UIN("combase.dll", CoGetClassObject));
    CHECK((FARPROC)UIN("ole32.dll", StringFromGUID2) == (FARPROC)UIN("combase.dll", StringFromGUID2));
    CHECK((FARPROC)UIN("ole32.dll", PropVariantClear) == (FARPROC)UIN("combase.dll", PropVariantClear));
    CHECK((FARPROC)UIN("ole32.dll", CreateStreamOnHGlobal) == (FARPROC)UIN("combase.dll", CreateStreamOnHGlobal));
}
TEST(exports_combase_default_lookup_prefers_ole32_but_same_target) {
    /* U() searches ole32 first; for forwarded names that is the very same code as the combase export. */
    FARPROC a = (FARPROC)U(CoCreateGuid), b = (FARPROC)UIN("combase.dll", CoCreateGuid);
    CHECK(a != NULL && b != NULL);
    if (!t_is_wine()) CHECK(a == b);
}
TEST(exports_combase_ole32_has_exports_combase_lacks) {
    /* The inverse direction: ole32 keeps OLE-level code that never moved to combase. */
    HMODULE o = t_module("ole32.dll"), c = t_module("combase.dll");
    if (!o || !c) { t_skip("module missing"); return; }
    static const char *const n[] = { "OleInitialize", "OleUninitialize", "StgCreateDocfile", "CreateFileMoniker", "DoDragDrop" };
    for (int i = 0; i < 5; i++) {
        CHECKF(GetProcAddress(o, n[i]) != NULL, "ole32!%s", n[i]);
        if (!exp_find_name(n[i])) CHECKF(GetProcAddress(c, n[i]) == NULL, "combase!%s", n[i]);
    }
}
