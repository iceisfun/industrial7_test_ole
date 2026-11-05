#include "ext.h"

void x_clsid_text(const GUID *g, char out[40])
{
    sprintf(out, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", (unsigned long)g->Data1, g->Data2, g->Data3,
            g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}

int x_reg_class_sub(const GUID *g, const char *sub, const char *dflt)
{
    char key[200], b[40]; HKEY k; LONG r;
    x_clsid_text(g, b);
    sprintf(key, "Software\\Classes\\CLSID\\%s%s%s", b, sub && *sub ? "\\" : "", sub ? sub : "");
    r = RegCreateKeyExA(HKEY_CURRENT_USER, key, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    if (r) return 0;
    if (dflt) RegSetValueExA(k, NULL, 0, REG_SZ, (const BYTE *)dflt, (DWORD)strlen(dflt) + 1);
    RegCloseKey(k);
    return 1;
}

void x_reg_class_delete(const GUID *g)
{
    char key[200], b[40];
    x_clsid_text(g, b);
    sprintf(key, "Software\\Classes\\CLSID\\%s", b);
    RegDeleteTreeA(HKEY_CURRENT_USER, key);
}
