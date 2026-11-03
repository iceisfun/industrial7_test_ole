#include "dx1.h"
/* GetClassFile's registry strategies: file extension (.ext -> ProgID -> CLSID, documented) and FileType patterns.
 * ole32 caches the extension and pattern tables per process on first use, so every scenario runs GetClassFile in a fresh
 * child process (CHILD gap1dx_gcf) after the parent has written its keys under HKCU\Software\Classes (merged into HKCR).
 * Keys are named olet.gapdx.<pid>* / .olet<pid>*; all removed afterwards, leftovers (olet.gapdx.* / .olet*) swept first. */

static const GUID clsid_a = { 0x7e57c1a5, 0x00a1, 0x4a1a, { 0x91, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x01 } };
static const GUID clsid_b = { 0x7e57c1a5, 0x00b2, 0x4a1a, { 0x91, 0x01, 0x6f, 0x6c, 0x65, 0x74, 0x00, 0x02 } };

static void guid_str(const GUID *g, WCHAR *w) { swprintf(w, 64, L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}", g->Data1, g->Data2, g->Data3,
    g->Data4[0], g->Data4[1], g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]); }

static LONG put(const WCHAR *path, const WCHAR *name, const WCHAR *val)
{
    HKEY k; LONG r = RegCreateKeyExW(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_WRITE, NULL, &k, NULL);
    if (r) return r;
    r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE *)val, (DWORD)((wcslen(val) + 1) * 2));
    RegCloseKey(k); return r;
}
static void del(const WCHAR *path) { RegDeleteTreeW(HKEY_CURRENT_USER, path); }

static void hexenc(const char *s, char *out) { while (*s) { sprintf(out, "%02x", (unsigned char)*s++); out += 2; } *out = 0; }
static void hexdec(const char *h, char *out) { while (h[0] && h[1]) { unsigned v; sscanf(h, "%2x", &v); *out++ = (char)v; h += 2; } *out = 0; }

/* child: argv[1] = hex(path), argv[2] = "E<hex hresult>" | "C<index 0|1>"  -> exit 0 when it matches */
CHILD(gap1dx_gcf)
{
    typedef HRESULT (WINAPI *gcf_t)(LPCOLESTR, CLSID *);
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; GUID g; HRESULT hr; gcf_t f;
    if (argc < 3) return 90;
    hexdec(argv[1], path); dx_wide(path, wp, MAX_PATH);
    if (!t_module("ole32.dll")) return 91;
    f = (gcf_t)t_proc_in("ole32.dll", "GetClassFile"); if (!f) return 92;
    memset(&g, 0xaa, sizeof g);
    hr = f(wp, &g);
    printf("CHILD hr=0x%08lx\n", (unsigned long)hr);
    if (argv[2][0] == 'E') { unsigned long want = strtoul(argv[2] + 1, NULL, 16); return (unsigned long)hr == want ? 0 : (hr == S_OK ? 3 : 1); }
    if (hr != S_OK) return 1;
    return IsEqualGUID(&g, argv[2][1] == '0' ? &clsid_a : &clsid_b) ? 0 : 2;
}

static int run(const WCHAR *wpath, const char *expect)
{
    char path[MAX_PATH], hex[2 * MAX_PATH + 1], args[2 * MAX_PATH + 32];
    WideCharToMultiByte(CP_ACP, 0, wpath, -1, path, MAX_PATH, NULL, NULL);
    hexenc(path, hex); snprintf(args, sizeof args, "%s %s", hex, expect);
    return (int)t_run_child("gap1dx_gcf", args, 20000);
}

static int make_raw(const WCHAR *p, const char *data)
{
    DWORD w; HANDLE h = CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, data, (DWORD)strlen(data), &w, NULL); CloseHandle(h); return 1;
}
static int make_docfile(const WCHAR *p, const GUID *cls)
{
    IStorage *s = NULL;
    if (!U(StgCreateDocfile) || FAILED(U(StgCreateDocfile)(p, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &s))) return 0;
    if (cls) U(WriteClassStg)(s, cls);
    s->lpVtbl->Commit(s, STGC_DEFAULT); s->lpVtbl->Release(s); return 1;
}

static void fpath(WCHAR *w, const char *leaf) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf); dx_wide(p, w, MAX_PATH); }

/* registers .olet<pid><tag> -> ProgID olet.gapdx.<pid><tag> [-> CLSID]; returns extension (with dot) */
static int reg_ext(const char *tag, const GUID *clsid, int with_progid_key, WCHAR *ext)
{
    WCHAR progid[64], key[128], cs[64];
    swprintf(ext, 32, L".olet%lu%hs", GetCurrentProcessId(), tag);
    swprintf(progid, 64, L"olet.gapdx.%lu%hs", GetCurrentProcessId(), tag);
    swprintf(key, 128, L"Software\\Classes\\%ls", ext); CHECK(put(key, NULL, progid) == 0);
    { WCHAR b[128] = L""; LONG n = sizeof b; LONG rr = RegQueryValueW(HKEY_CLASSES_ROOT, ext, b, &n); if (rr != 0 || wcscmp(b, progid)) { t_note("HKCR does not merge HKCU\\Software\\Classes here (%ld)", rr); return 0; } }
    if (with_progid_key) {
        swprintf(key, 128, L"Software\\Classes\\%ls", progid); put(key, NULL, L"olet gap test");
        if (clsid) { guid_str(clsid, cs); swprintf(key, 128, L"Software\\Classes\\%ls\\CLSID", progid); put(key, NULL, cs); }
    }
    return 1;
}
static void unreg_ext(const char *tag)
{
    WCHAR k[128];
    swprintf(k, 128, L"Software\\Classes\\.olet%lu%hs", GetCurrentProcessId(), tag); del(k);
    swprintf(k, 128, L"Software\\Classes\\olet.gapdx.%lu%hs", GetCurrentProcessId(), tag); del(k);
}

TEST(dx_gap1_getclassfile_extension_to_clsid)
{
    WCHAR ext[32], p[MAX_PATH], leaf[64]; char l8[64];
    if (!U(GetClassFile)) { t_skip("missing GetClassFile"); return; }
    if (!reg_ext("a", &clsid_a, 1, ext)) { unreg_ext("a"); t_skip("HKCR does not merge HKCU classes"); return; }
    swprintf(leaf, 64, L"x%ls", ext); WideCharToMultiByte(CP_ACP, 0, leaf, -1, l8, 64, NULL, NULL); fpath(p, l8);
    CHECK(make_raw(p, "ordinary content"));
    EXPECT_EQ(run(p, "C0"), 0);
    unreg_ext("a");
}

TEST(dx_gap1_getclassfile_extension_case_insensitive)
{
    WCHAR ext[32], p[MAX_PATH]; char leaf[64], up[64]; size_t i;
    if (!U(GetClassFile)) { t_skip("missing GetClassFile"); return; }
    if (!reg_ext("b", &clsid_b, 1, ext)) { unreg_ext("b"); t_skip("HKCR does not merge HKCU classes"); return; }
    snprintf(leaf, sizeof leaf, "y.OLET%lub", GetCurrentProcessId());
    for (i = 0; leaf[i]; i++) up[i] = leaf[i]; up[i] = 0;
    fpath(p, up); CHECK(make_raw(p, "data"));
    EXPECT_EQ(run(p, "C1"), 0);
    unreg_ext("b");
}

TEST(dx_gap1_getclassfile_extension_without_clsid_is_invalid)
{
    WCHAR ext[32], p[MAX_PATH]; char leaf[64];
    if (!U(GetClassFile)) { t_skip("missing GetClassFile"); return; }
    /* ProgID exists but has no CLSID subkey */
    if (!reg_ext("c", NULL, 1, ext)) { unreg_ext("c"); t_skip("HKCR does not merge HKCU classes"); return; }
    snprintf(leaf, sizeof leaf, "z.olet%luc", GetCurrentProcessId()); fpath(p, leaf); CHECK(make_raw(p, "data"));
    EXPECT_EQ(run(p, "E800401E6"), 0);
    unreg_ext("c");
    /* extension names a ProgID that does not exist at all */
    if (!reg_ext("d", NULL, 0, ext)) { unreg_ext("d"); t_skip("HKCR does not merge HKCU classes"); return; }
    snprintf(leaf, sizeof leaf, "z.olet%lud", GetCurrentProcessId()); fpath(p, leaf); CHECK(make_raw(p, "data"));
    EXPECT_EQ(run(p, "E800401E6"), 0);
    unreg_ext("d");
}

TEST(dx_gap1_getclassfile_docfile_class_beats_extension)
{
    WCHAR ext[32], p[MAX_PATH]; char leaf[64];
    if (!U(GetClassFile) || !U(StgCreateDocfile)) { t_skip("missing export"); return; }
    if (!reg_ext("e", &clsid_b, 1, ext)) { unreg_ext("e"); t_skip("HKCR does not merge HKCU classes"); return; }
    snprintf(leaf, sizeof leaf, "w.olet%lue", GetCurrentProcessId()); fpath(p, leaf);
    CHECK(make_docfile(p, &clsid_a));
    EXPECT_EQ(run(p, "C0"), 0);                 /* the stored class, not the extension's */
    unreg_ext("e");
}

TEST(dx_gap1_getclassfile_docfile_without_class_uses_extension)
{
    WCHAR ext[32], p[MAX_PATH]; char leaf[64];
    if (!U(GetClassFile) || !U(StgCreateDocfile)) { t_skip("missing export"); return; }
    if (!reg_ext("f", &clsid_b, 1, ext)) { unreg_ext("f"); t_skip("HKCR does not merge HKCU classes"); return; }
    snprintf(leaf, sizeof leaf, "v.olet%luf", GetCurrentProcessId()); fpath(p, leaf);
    CHECK(make_docfile(p, NULL));
    EXPECT_EQ(run(p, "C1"), 0);
    unreg_ext("f");
}

TEST(dx_gap1_getclassfile_file_pattern_note)
{
    /* FileType\{CLSID}\<n> = offset,cb,mask,value. Whether <n> is a key (default value) or a value is not settled by the
     * documentation, so both layouts are tried and the outcome recorded. */
    WCHAR cs[64], key[160], p[MAX_PATH]; char leaf[64]; int rc;
    if (!U(GetClassFile)) { t_skip("missing GetClassFile"); return; }
    guid_str(&clsid_a, cs);
    snprintf(leaf, sizeof leaf, "pat.olet%lug", GetCurrentProcessId()); fpath(p, leaf);
    CHECK(make_raw(p, "\xAB\xCD\x12\x34 body bytes \xFE\xFE\xFE\xFE"));
    swprintf(key, 160, L"Software\\Classes\\FileType\\%ls\\0", cs);
    put(key, NULL, L"0,4,FFFFFFFF,ABCD1234");
    swprintf(key, 160, L"Software\\Classes\\FileType\\%ls\\1", cs);
    put(key, NULL, L"-4,4,,FEFEFEFE");
    rc = run(p, "C0");
    t_note("FileType subkey layout (n = key with default value): child exit %d", rc);
    swprintf(key, 160, L"Software\\Classes\\FileType\\%ls", cs); del(key);
    swprintf(key, 160, L"Software\\Classes\\FileType\\%ls", cs);
    put(key, L"0", L"0,4,FFFFFFFF,ABCD1234");
    put(key, L"1", L"-4,4,,FEFEFEFE");
    rc = run(p, "C0");
    t_note("FileType value layout (n = value name): child exit %d", rc);
    swprintf(key, 160, L"Software\\Classes\\FileType\\%ls", cs); del(key);
    /* with the keys gone the file is an unknown type again */
    EXPECT_EQ(run(p, "E800401E6"), 0);
}
