/* CoCheckElevationEnabled, the clipboard-broker ordinals, OleGetPackageClipboardOwner,
 * Ole32DllGetClassObject against DllGetClassObject. */
#include "ext.h"

/* ================================================================================================
 * CoCheckElevationEnabled
 * ============================================================================================== */
TEST(oex_cocheckelevationenabled_reports_the_policy_flag)
{
    DWORD (WINAPI *f)(DWORD *) = (DWORD (WINAPI *)(DWORD *))t_proc_in("ole32.dll", "CoCheckElevationEnabled");
    DWORD v = 0xcccccccc, r;
    REQUIRE(f);
    r = f(&v);
    EQ(r, 0);              /* ERROR_SUCCESS */
    CHECK(v == 0 || v == 1);   /* the policy flag (EnableLUA): a machine setting, 1 on the Windows 10 guest */
    t_note("elevation flag %lu", (unsigned long)v);
    v = 0xcccccccc;
    EQ(f(&v), 0);
    CHECK(v == 0 || v == 1);
}

/* ================================================================================================
 * ordinals 801 / 803 (clipboard broker host switches) and OleGetPackageClipboardOwner
 * ============================================================================================== */
TEST(oex_clipboard_broker_ordinals_return_s_ok_repeatably)
{
    HRESULT (WINAPI *rundown)(void) = (void *)t_proc_ord("ole32.dll", 801);
    HRESULT (WINAPI *init)(void) = (void *)t_proc_ord("ole32.dll", 803);
    REQUIRE(rundown); REQUIRE(init);
    CHECK(t_proc_in("ole32.dll", "OleInitClipboardBroker") == NULL);   /* ordinal-only exports */
    EQ(init(), S_OK);
    EQ(init(), S_OK);
    EQ(rundown(), S_OK);
    EQ(rundown(), S_OK);
    EQ(init(), S_OK);
}

static void broker_thread(void *unused)
{
    HRESULT (WINAPI *init)(void) = (void *)t_proc_ord("ole32.dll", 803);
    if (!init) { t_skip("missing export: ordinal 803"); return; }
    EQ(init(), S_OK);
    EQ(x_apt_type(), 0xffff);   /* the thread is still without an apartment */
}
TEST(oex_oleinitclipboardbroker_does_not_start_ole)
{
    x_on_thread(broker_thread, NULL);
}

static void owner_on_thread(void *unused)
{
    HRESULT (WINAPI *f)(WCHAR **) = (void *)t_proc_in("ole32.dll", "OleGetPackageClipboardOwner");
    WCHAR *p = (WCHAR *)(ULONG_PTR)1;
    HRESULT h;
    if (!f) { t_skip("missing export: OleGetPackageClipboardOwner"); return; }
    h = f(&p);
    EQ(h, S_OK);              /* no packaged application owns the clipboard of a desktop process */
    EQ(p, NULL);
    EQ(x_apt_type(), 0xffff);
}
TEST(oex_getpackageclipboardowner_none_for_desktop_clipboard)
{
    x_on_thread(owner_on_thread, NULL);
}

/* ================================================================================================
 * Ole32DllGetClassObject is DllGetClassObject's scan; DllGetClassObjectWOW is its second name
 * ============================================================================================== */
typedef HRESULT (WINAPI *dgco_t)(REFCLSID, REFIID, LPVOID *);
TEST(oex_ole32dllgetclassobject_matches_dllgetclassobject)
{
    dgco_t f1 = (dgco_t)t_proc_in("ole32.dll", "DllGetClassObject");
    dgco_t f2 = (dgco_t)t_proc_in("ole32.dll", "DllGetClassObjectWOW");
    dgco_t f3 = (dgco_t)t_proc_in("ole32.dll", "Ole32DllGetClassObject");
    static const DWORD ids[] = { 0x303, 0x304, 0x305, 0x306, 0x309, 0x31A, 0x327 };   /* file, item, anti, pointer, composite, class, objref */
    unsigned i;
    REQUIRE(f1); REQUIRE(f2); REQUIRE(f3);
    CHECK(f1 == f2);           /* one body, two names */
    t_note("Ole32DllGetClassObject %s DllGetClassObject's address", f1 == f3 ? "shares" : "has its own copy of");
    for (i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        GUID g = {0, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
        void *p1 = (void *)1, *p3 = (void *)1; HRESULT a, c;
        g.Data1 = ids[i];
        a = f1(&g, &IID_IClassFactory, &p1);
        c = f3(&g, &IID_IClassFactory, &p3);
        CHECKF(a == S_OK && c == S_OK, "clsid %08lx: %08lx %08lx", (unsigned long)ids[i], (unsigned long)a, (unsigned long)c);
        CHECK(p1 != NULL && p1 != (void *)1 && p3 != NULL && p3 != (void *)1);
        if (a >= 0 && p1) IClassFactory_Release((IClassFactory *)p1);
        if (c >= 0 && p3) IClassFactory_Release((IClassFactory *)p3);
    }
}

TEST(oex_ole32dllgetclassobject_interfaces_and_arguments)
{
    dgco_t f3 = (dgco_t)t_proc_in("ole32.dll", "Ole32DllGetClassObject");
    GUID g = {0x303, 0, 0, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
    void *p;
    REQUIRE(f3);
    p = (void *)1; EQ(f3(&g, &IID_IUnknown, &p), S_OK); CHECK(p && p != (void *)1); if (p && p != (void *)1) IUnknown_Release((IUnknown *)p);
    p = (void *)1; EQ(f3(&g, &IID_IMarshal, &p), X_E_NOINTERFACE); EQ(p, NULL);
    p = (void *)1; EQ(f3(&g, &IID_IClassFactory2, &p), X_E_NOINTERFACE); EQ(p, NULL);
    EQ(f3(&g, &IID_IClassFactory, NULL), X_E_INVALIDARG);
}

/* ================================================================================================
 * every other CLSID gets a default-handler class factory from DllGetClassObject (OLE embedding classes are served by ole32)
 * ============================================================================================== */
static void dgco_default_thread(void *unused)
{
    dgco_t f = (dgco_t)t_proc_in("ole32.dll", "DllGetClassObject");
    GUID u = {0xdeadbeef, 1, 2, {3, 4, 5, 6, 7, 8, 9, 10}};
    void *p = (void *)1, *q; IUnknown *x = NULL; HRESULT h;
    if (!f) { t_skip("missing export: DllGetClassObject"); return; }
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    h = f(&u, &IID_IClassFactory, &p);
    EQ(h, S_OK);
    CHECK(p != NULL && p != (void *)1);
    if (h >= 0 && p && p != (void *)1) {
        IClassFactory *cf = (IClassFactory *)p;
        EQ(IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void **)&x), S_OK);
        CHECK(x != NULL);
        if (x) {
            q = NULL; EQ(IUnknown_QueryInterface(x, &IID_IOleObject, &q), S_OK); if (q) IUnknown_Release((IUnknown *)q);
            q = NULL; EQ(IUnknown_QueryInterface(x, &IID_IDataObject, &q), S_OK); if (q) IUnknown_Release((IUnknown *)q);
            q = NULL; EQ(IUnknown_QueryInterface(x, &IID_IRunnableObject, &q), S_OK); if (q) IUnknown_Release((IUnknown *)q);
            q = NULL; EQ(IUnknown_QueryInterface(x, &IID_IPersistStorage, &q), S_OK); if (q) IUnknown_Release((IUnknown *)q);
            IUnknown_Release(x);
        }
        x = NULL;
        EQ(IClassFactory_LockServer(cf, TRUE), S_OK);
        EQ(IClassFactory_LockServer(cf, FALSE), S_OK);
        EQ(IClassFactory_Release(cf), 0);
    }
    CoUninitialize();
}
TEST(oex_dllgetclassobject_unknown_clsid_is_served_by_a_default_handler_factory)
{
    x_on_thread(dgco_default_thread, NULL);
}

/* the object the factory makes also carries IPersist (its class id) and IMarshal on Windows 10 */
static void dgco_persist_thread(void *unused)
{
    dgco_t f = (dgco_t)t_proc_in("ole32.dll", "DllGetClassObject");
    GUID u = {0xdeadbeef, 1, 2, {3, 4, 5, 6, 7, 8, 9, 10}}, cid;
    void *p = NULL, *q = NULL; IUnknown *x = NULL; IPersist *ps = NULL;
    if (!f) { t_skip("missing export: DllGetClassObject"); return; }
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    EQ(f(&u, &IID_IClassFactory, &p), S_OK);
    if (p) {
        EQ(IClassFactory_CreateInstance((IClassFactory *)p, NULL, &IID_IUnknown, (void **)&x), S_OK);
        if (x) {
            EQ(IUnknown_QueryInterface(x, &IID_IPersist, (void **)&ps), S_OK);
            if (ps) { memset(&cid, 0, sizeof cid); EQ(IPersist_GetClassID(ps, &cid), S_OK); CHECK(IsEqualGUID(&cid, &u)); IPersist_Release(ps); }
            EQ(IUnknown_QueryInterface(x, &IID_IMarshal, &q), S_OK);
            if (q) IUnknown_Release((IUnknown *)q);
            IUnknown_Release(x);
        }
        IClassFactory_Release((IClassFactory *)p);
    }
    CoUninitialize();
}
TEST(oex_dllgetclassobject_default_handler_carries_its_class_id)
{
    x_on_thread(dgco_persist_thread, NULL);
}
