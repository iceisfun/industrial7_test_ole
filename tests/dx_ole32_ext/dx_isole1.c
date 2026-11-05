/* CoIsOle1Class, the positive path: classes registered under HKCU\\Software\\Classes\\CLSID (this is a dx_ area: it changes user state;
 * the registrations it makes are deleted again). */
#include "ext.h"

/* ================================================================================================
 * CoIsOle1Class: true for classes of the OLE 1.0 id ranges (0003xxxx, 0004xxxx) that have an Ole1Class SUBKEY
 * ============================================================================================== */
static GUID ole1_guid(unsigned hi, unsigned n)
{
    GUID g = {0, 0x9a3c, 0x4f5a, {0x8d, 0x2e, 0x71, 0xb5, 0xc6, 0xe0, 0xf1, 0xa4}};
    g.Data1 = ((DWORD)hi << 16) | (0xc200 + n);
    return g;
}
typedef BOOL (WINAPI *isole1_t)(const GUID *);

TEST(dxoex_isole1class_registered_ranges)
{
    isole1_t f = (isole1_t)t_proc_in("ole32.dll", "CoIsOle1Class");
    unsigned his[] = { 3, 4, 2, 5, 0, 1, 0x30, 0x0300 }; unsigned i;
    GUID g[8];
    REQUIRE(f);
    for (i = 0; i < 8; i++) { g[i] = ole1_guid(his[i], i); if (!x_reg_class_sub(&g[i], "Ole1Class", "Word.Document.8")) { t_skip("cannot write HKCU\\Software\\Classes"); goto done; } }
    EQ(f(&g[0]), 1);   /* 0003xxxx */
    EQ(f(&g[1]), 1);   /* 0004xxxx */
    for (i = 2; i < 8; i++) { BOOL r = f(&g[i]); CHECKF(r == 0, "high word %x -> %d", his[i], (int)r); }
done:
    for (i = 0; i < 8; i++) x_reg_class_delete(&g[i]);
    EQ(f(&g[0]), 0);   /* unregistered again */
}

TEST(dxoex_isole1class_subkey_not_value)
{
    isole1_t f = (isole1_t)t_proc_in("ole32.dll", "CoIsOle1Class");
    GUID empty = ole1_guid(3, 20), nodef = ole1_guid(3, 21), asvalue = ole1_guid(3, 22), classonly = ole1_guid(3, 23);
    HKEY k; char key[200], b[40];
    REQUIRE(f);
    if (!x_reg_class_sub(&empty, "Ole1Class", "")) { t_skip("cannot write HKCU\\Software\\Classes"); return; }
    x_reg_class_sub(&nodef, "Ole1Class", NULL);                 /* the subkey without any value */
    x_reg_class_sub(&classonly, "", "olet test class");          /* the class key alone */
    x_reg_class_sub(&asvalue, "", NULL);
    x_clsid_text(&asvalue, b); sprintf(key, "Software\\Classes\\CLSID\\%s", b);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, key, 0, KEY_ALL_ACCESS, &k) == 0) { RegSetValueExA(k, "Ole1Class", 0, REG_SZ, (const BYTE *)"Foo", 4); RegCloseKey(k); }
    EQ(f(&empty), 1);      /* an empty default value still counts: the subkey is what is looked for */
    EQ(f(&nodef), 1);
    EQ(f(&asvalue), 0);    /* a VALUE named Ole1Class on the class key is not the subkey */
    EQ(f(&classonly), 0);
    x_reg_class_delete(&empty); x_reg_class_delete(&nodef); x_reg_class_delete(&asvalue); x_reg_class_delete(&classonly);
}

TEST(dxoex_isole1class_same_answer_from_both_hives_view)
{
    /* the class view is the merged HKCR: a class written under HKCU is visible, deleting it removes it */
    isole1_t f = (isole1_t)t_proc_in("ole32.dll", "CoIsOle1Class");
    GUID g = ole1_guid(4, 30);
    REQUIRE(f);
    EQ(f(&g), 0);
    if (!x_reg_class_sub(&g, "Ole1Class", "Foo")) { t_skip("cannot write HKCU\\Software\\Classes"); return; }
    EQ(f(&g), 1);
    EQ(f(&g), 1);
    x_reg_class_delete(&g);
    EQ(f(&g), 0);
    EQ(f(NULL), 0);
}

