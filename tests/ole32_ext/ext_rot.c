/* GetObjectFromRotByPath, CleanROTForApartment, DestroyRunningObjectTable.
 * The running object table is process-wide state: each scenario that needs a pristine process (no table yet, a table already
 * destroyed) runs as a child process; the rest run on a fresh STA thread. */
#include "ext.h"

typedef HRESULT (WINAPI *gobp_t)(const WCHAR *, IUnknown **);
typedef void (WINAPI *voidfn_t)(void);

static int bad;
#define CK(c) do { if (!(c)) { bad++; printf("  CHILD-FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define CKH(e, want) do { HRESULT h_ = (e); if (h_ != (HRESULT)(want)) { bad++; printf("  CHILD-FAIL line %d: %s = %08lx want %08lx\n", __LINE__, #e, (unsigned long)h_, (unsigned long)(HRESULT)(want)); } } while (0)

/* ---- the registration is found by the file path, in any case --------------------------------------------- */
static void path_thread(void *unused)
{
    gobp_t gp = (gobp_t)t_proc_in("ole32.dll", "GetObjectFromRotByPath");
    IRunningObjectTable *rot = NULL; IMoniker *mk = NULL, *item = NULL;
    XObj obj; IUnknown *u = (IUnknown *)&obj, *o; DWORD ck = 0; HRESULT h; WCHAR lng[2200]; int i; LONG base;
    if (!gp) { t_skip("missing export: GetObjectFromRotByPath"); return; }
    x_obj_init(&obj);
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    EQ(GetRunningObjectTable(0, &rot), S_OK);
    EQ(CreateFileMoniker(L"C:\\olet_rot\\Doc One.txt", &mk), S_OK);
    EQ(IRunningObjectTable_Register(rot, 0, u, mk, &ck), S_OK);
    base = obj.refs;
    CHECK(base > 1);                                    /* the table holds the object */
    o = (IUnknown *)1; h = gp(L"C:\\olet_rot\\Doc One.txt", &o);
    EQ(h, S_OK); EQ(o, u); EQ(obj.refs, base + 1);      /* the caller owns one more */
    IUnknown_Release(o);
    o = (IUnknown *)1; h = gp(L"c:\\OLET_ROT\\doc one.TXT", &o);
    EQ(h, S_OK); EQ(o, u);                              /* the key is the upper-cased path */
    IUnknown_Release(o);
    o = (IUnknown *)1; EQ(gp(L"C:\\olet_rot\\Doc One.tx", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);
    o = (IUnknown *)1; EQ(gp(L"C:\\olet_rot\\Doc One.txtx", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);
    o = (IUnknown *)1; EQ(gp(L"C:\\olet_rot\\\\Doc One.txt", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);   /* no path normalization */
    o = (IUnknown *)1; EQ(gp(L"", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);
    o = (IUnknown *)1; EQ(gp(L"!it", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);                          /* only file monikers are keyed by path */
    /* limit of the key buffer: 1015 characters fit, 1016 do not */
    for (i = 0; i < 1015; i++) lng[i] = L'a'; lng[1015] = 0;
    o = (IUnknown *)1; EQ(gp(lng, &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);
    for (i = 0; i < 1016; i++) lng[i] = L'a'; lng[1016] = 0;
    o = (IUnknown *)1; EQ(gp(lng, &o), X_E_OUTOFMEMORY); EQ(o, NULL);
    for (i = 0; i < 2100; i++) lng[i] = L'a'; lng[2100] = 0;
    o = (IUnknown *)1; EQ(gp(lng, &o), X_E_OUTOFMEMORY); EQ(o, NULL);
    /* a registration under another kind of moniker is not found by a path */
    EQ(CreateFileMoniker(L"C:\\olet_rot\\Other.txt", &item), S_OK);
    EQ(IRunningObjectTable_Revoke(rot, ck), S_OK);
    o = (IUnknown *)1; EQ(gp(L"C:\\olet_rot\\Doc One.txt", &o), X_MK_E_UNAVAILABLE); EQ(o, NULL);
    EQ(obj.refs, 1);                                    /* revoked: nothing of the table is left */
    IMoniker_Release(item); IMoniker_Release(mk); IRunningObjectTable_Release(rot);
    CoUninitialize();
}
TEST(oex_rotpath_finds_file_moniker_registrations_in_any_case) { x_on_thread(path_thread, NULL); }

/* ---- no table yet: S_FALSE; and again once the last apartment of the process is gone ---------------------- */
CHILD(rotpath_nocom)
{
    gobp_t gp = (gobp_t)t_proc_in("ole32.dll", "GetObjectFromRotByPath");
    IUnknown *u = (IUnknown *)1; IRunningObjectTable *rot = NULL;
    if (!gp) return 0xC0DE0002;
    bad = 0;
    CKH(gp(L"C:\\olet_rot\\x.txt", &u), S_FALSE);        /* no COM in the process: there is no table to ask */
    CK(u == (IUnknown *)1);                               /* the output is not touched */
    CKH(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    CKH(GetRunningObjectTable(0, &rot), S_OK);
    u = (IUnknown *)1; CKH(gp(L"C:\\olet_rot\\x.txt", &u), X_MK_E_UNAVAILABLE);   /* the table is there now */
    CK(u == NULL);
    IRunningObjectTable_Release(rot);
    CoUninitialize();
    u = (IUnknown *)1; CKH(gp(L"C:\\olet_rot\\x.txt", &u), S_FALSE);              /* and gone with the last apartment */
    CK(u == (IUnknown *)1);
    return bad;
}
TEST(oex_rotpath_without_com_is_s_false)
{
    DWORD rc = t_run_child("rotpath_nocom", "", 60000);
    CHECKF(rc == 0, "child exit 0x%lx (number of failed checks)", (unsigned long)rc);
}

/* ---- CleanROTForApartment / DestroyRunningObjectTable ---------------------------------------------------- */
/* Both forget the process's local registrations: the cookies stop working (Revoke is E_INVALIDARG) and nothing is released, because
 * the registration itself lives on in the shared table (IsRunning, GetObject and the lookup by path still find it). */
static void clean_thread(void *which)
{
    int destroy = (int)(ULONG_PTR)which;
    voidfn_t fn = (voidfn_t)t_proc_in("ole32.dll", destroy ? "DestroyRunningObjectTable" : "CleanROTForApartment");
    gobp_t gp = (gobp_t)t_proc_in("ole32.dll", "GetObjectFromRotByPath");
    IRunningObjectTable *rot = NULL; IMoniker *mk1 = NULL, *mk2 = NULL; XObj o1, o2; DWORD c1 = 0, c2 = 0; IUnknown *got; LONG r1, r2;
    WCHAR n1[80], n2[80];
    if (!fn || !gp) { t_skip("missing export"); return; }
    x_obj_init(&o1); x_obj_init(&o2);
    _snwprintf(n1, 80, L"C:\\olet_clean%d\\one.txt", destroy); _snwprintf(n2, 80, L"C:\\olet_clean%d\\two.txt", destroy);
    EQ(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED), S_OK);
    EQ(GetRunningObjectTable(0, &rot), S_OK);
    EQ(CreateFileMoniker(n1, &mk1), S_OK); EQ(CreateFileMoniker(n2, &mk2), S_OK);
    EQ(IRunningObjectTable_Register(rot, 0, (IUnknown *)&o1, mk1, &c1), S_OK);
    EQ(IRunningObjectTable_Register(rot, 0, (IUnknown *)&o2, mk2, &c2), S_OK);
    r1 = o1.refs; r2 = o2.refs;
    CHECK(r1 > 1 && r2 > 1);                                         /* the table holds the objects */
    fn();
    EQ(o1.refs, r1); EQ(o2.refs, r2);                                /* nothing is released */
    if (destroy) {   /* Windows frees the table object itself: the old pointer must not be used again, not even to release it */
        IRunningObjectTable *t2 = NULL;
        EQ(GetRunningObjectTable(0, &t2), S_OK);
        rot = t2;
    }
    EQ(IRunningObjectTable_Revoke(rot, c1), X_E_INVALIDARG);          /* the local records are gone */
    EQ(IRunningObjectTable_Revoke(rot, c2), X_E_INVALIDARG);
    EQ(IRunningObjectTable_IsRunning(rot, mk1), S_OK);                /* the shared registration is not */
    got = (IUnknown *)1; EQ(gp(n1, &got), S_OK); EQ(got, (IUnknown *)&o1); if (got && got != (IUnknown *)1) IUnknown_Release(got);
    got = (IUnknown *)1; EQ(IRunningObjectTable_GetObject(rot, mk2, &got), S_OK); EQ(got, (IUnknown *)&o2); if (got && got != (IUnknown *)1) IUnknown_Release(got);
    IRunningObjectTable_Release(rot);
    IMoniker_Release(mk1); IMoniker_Release(mk2);
    CoUninitialize();
}
TEST(oex_clean_rot_for_apartment_forgets_local_registrations) { x_on_thread(clean_thread, (void *)0); }
TEST(oex_destroy_running_object_table_forgets_local_registrations) { x_on_thread(clean_thread, (void *)1); }

static void clean_noop_thread(void *unused)
{
    voidfn_t fn = (voidfn_t)t_proc_in("ole32.dll", "CleanROTForApartment");
    if (!fn) { t_skip("missing export"); return; }
    fn();   /* no COM and possibly no table: harmless */
    EQ(x_apt_type(), 0xffff);
}
TEST(oex_clean_rot_for_apartment_without_com_is_harmless) { x_on_thread(clean_noop_thread, NULL); }
