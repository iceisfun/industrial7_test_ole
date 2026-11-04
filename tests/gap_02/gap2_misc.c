/* gap_02: IsAccelerator (ole32), IsErrorPropagationEnabled (combase), and the sechost LSA wrappers LsaLookupSids,
 * LsaAddAccountRights, LsaEnumerateAccountRights, LsaEnumerateAccountsWithUserRight (non-destructive parts; the
 * account-rights mutation lives in tests/dx_gap_02).
 *
 * IsAccelerator(hAccel, cEntries, MSG *, WORD *lpwCmd)  decomp ole32!IsAccelerator:
 *   - cEntries == 0 -> FALSE
 *   - wSysKeyToKey: message m in 0x104..0x107 with lParam bit 29 (ALT context) clear is treated as m-4. Only WM_KEYDOWN(0x100) and
 *     WM_SYSKEYDOWN(0x104) (virtual-key form) and WM_CHAR(0x102)/WM_SYSCHAR(0x106) (character form) are considered; every other
 *     message is FALSE.
 *   - CopyAcceleratorTableW(hAccel, buf, cEntries) must return EXACTLY cEntries (so cEntries larger than the table or an invalid
 *     handle -> FALSE; smaller -> only the first cEntries entries are searched)
 *   - match: entry.key == (WORD)wParam && (entry.fVirt & FVIRTKEY) == (virtual form); for the virtual form FSHIFT/FCONTROL
 *     must equal GetKeyState(VK_SHIFT/VK_CONTROL)<0; FALT must equal GetKeyState(VK_MENU)<0 in BOTH forms.
 *   - *lpwCmd (when non-NULL) is always written: the command on a match, 0 otherwise. */
#include "gap2.h"

UDECL(BOOL, IsAccelerator, (HACCEL, int, LPMSG, WORD *));

static HACCEL mk_table(const ACCEL *a, int n)
{
    return UIN("user32.dll", CreateAcceleratorTableW)((LPACCEL)a, n);
}
static void del_table(HACCEL h) { if (h) UIN("user32.dll", DestroyAcceleratorTable)(h); }
static int modifiers_idle(void)
{
    SHORT (WINAPI *gks)(int) = UIN("user32.dll", GetKeyState);
    return gks && gks(VK_SHIFT) >= 0 && gks(VK_CONTROL) >= 0 && gks(VK_MENU) >= 0;
}
static MSG mkmsg(UINT m, WPARAM w, LPARAM l) { MSG g; memset(&g, 0, sizeof g); g.message = m; g.wParam = w; g.lParam = l; return g; }

static const ACCEL std_table[] = {
    { FVIRTKEY, 'A', 100 },
    { FVIRTKEY, VK_F5, 101 },
    { 0, 'x', 102 },                       /* character form */
    { FVIRTKEY | FCONTROL, 'C', 103 },
    { FVIRTKEY | FNOINVERT, VK_F6, 104 },
    { FVIRTKEY, 'A', 105 },                /* duplicate key: the first entry wins */
};
#define NSTD ((int)ARRAY_N(std_table))
#define ISACC UN(IsAccelerator)

#define SENT 0x7777
/* the emulator leaves *lpwCmd alone when nothing matches; ole32 writes 0 */
#define CMD0(c) do { if (!t_is_wine()) EXPECT_EQ((c), 0); } while (0)
static int run(HACCEL h, int n, MSG m, WORD *cmd) { return ISACC(h, n, &m, cmd) ? 1 : 0; }

TEST(gap2_isaccel_null_table)
{
    WORD cmd = SENT; MSG m = mkmsg(WM_KEYDOWN, 'A', 0);
    REQUIRE(ISACC);
    EXPECT_EQ(ISACC(NULL, 5, &m, &cmd), 0);
    CMD0(cmd);
    cmd = SENT;
    EXPECT_EQ(ISACC(NULL, 0, &m, &cmd), 0);
    CMD0(cmd);
    EXPECT_EQ(ISACC(NULL, 1, &m, NULL), 0);
}
TEST(gap2_isaccel_zero_entries)
{
    HACCEL h; WORD cmd = SENT; MSG m = mkmsg(WM_KEYDOWN, 'A', 0);
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    EXPECT_EQ(ISACC(h, 0, &m, &cmd), 0);
    CMD0(cmd);
    del_table(h);
}
TEST(gap2_isaccel_virtkey_match)
{
    HACCEL h; WORD cmd; REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    cmd = SENT; EXPECT_EQ(ISACC(h, NSTD, &(MSG){ .message = WM_KEYDOWN, .wParam = 'A' }, &cmd), 1); EXPECT_EQ(cmd, 100);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, VK_F5, 0), &cmd), 1); EXPECT_EQ(cmd, 101);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, VK_F6, 0), &cmd), 1); EXPECT_EQ(cmd, 104);   /* FNOINVERT is not compared */
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, VK_F7, 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 'B', 0), &cmd), 0); CMD0(cmd);
    del_table(h);
}
TEST(gap2_isaccel_return_value_is_exactly_one_or_zero)
{
    HACCEL h; MSG m = mkmsg(WM_KEYDOWN, 'A', 0); WORD cmd;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    EXPECT_EQ((int)ISACC(h, NSTD, &m, &cmd), 1);
    m.wParam = 'Z';
    EXPECT_EQ((int)ISACC(h, NSTD, &m, &cmd), 0);
    del_table(h);
}
TEST(gap2_isaccel_first_entry_wins)
{
    HACCEL h; WORD cmd = SENT;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 'A', 0), &cmd), 1);
    EXPECT_EQ(cmd, 100);
    del_table(h);
}
TEST(gap2_isaccel_character_form)
{
    HACCEL h; WORD cmd;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_CHAR, 'x', 0), &cmd), 1); EXPECT_EQ(cmd, 102);
    /* the virtual-key entry for 'A' does not answer a WM_CHAR, and the character entry 'x' (0x78 == VK_F9) does not answer WM_KEYDOWN */
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_CHAR, 'A', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 'x', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_CHAR, 'X', 0), &cmd), 0); CMD0(cmd);   /* comparison is case sensitive */
    del_table(h);
}
TEST(gap2_isaccel_message_matrix)
{
    static const struct { UINT msg; LPARAM lp; int match; const char *what; } t[] = {
        { WM_KEYDOWN,    0,           1, "WM_KEYDOWN" },
        { WM_KEYUP,      0,           0, "WM_KEYUP" },
        { WM_CHAR,       0,           0, "WM_CHAR vs virtual entry" },
        { WM_DEADCHAR,   0,           0, "WM_DEADCHAR" },
        { WM_SYSKEYDOWN, 0,           1, "WM_SYSKEYDOWN, ALT context clear -> WM_KEYDOWN" },
        { WM_SYSKEYDOWN, 0x20000000,  1, "WM_SYSKEYDOWN, ALT context set" },
        { WM_SYSKEYUP,   0,           0, "WM_SYSKEYUP" },
        { WM_SYSKEYUP,   0x20000000,  0, "WM_SYSKEYUP alt" },
        { WM_SYSCHAR,    0,           0, "WM_SYSCHAR vs virtual entry" },
        { WM_SYSDEADCHAR,0,           0, "WM_SYSDEADCHAR" },
        { 0x0108,        0,           0, "msg 0x108" },
        { WM_NULL,       0,           0, "WM_NULL" },
        { WM_COMMAND,    0,           0, "WM_COMMAND" },
        { 0xFFFFFFFFu,   0,           0, "msg -1" },
    };
    HACCEL h; unsigned i;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    for (i = 0; i < ARRAY_N(t); i++) {
        WORD cmd = SENT; MSG m = mkmsg(t[i].msg, 'A', t[i].lp);
        int r;
        if (t_is_wine() && t[i].msg == WM_SYSCHAR) continue;     /* emulator treats WM_SYSCHAR as the virtual-key form */
        r = ISACC(h, NSTD, &m, &cmd) ? 1 : 0;
        CHECKF(r == t[i].match, "%s: got %d", t[i].what, r);
        if (t[i].match || !t_is_wine()) CHECKF(cmd == (t[i].match ? 100 : 0), "%s: cmd %u", t[i].what, cmd);
    }
    del_table(h);
}
TEST(gap2_isaccel_char_messages_with_char_entry)
{
    static const struct { UINT msg; LPARAM lp; } t[] = {
        { WM_CHAR, 0 }, { WM_CHAR, 0x20000000 }, { WM_SYSCHAR, 0 }, { WM_SYSCHAR, 0x20000000 } };
    HACCEL h; unsigned i;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (t_is_wine()) { t_skip("emulator does not treat WM_SYSCHAR as the character form"); return; }
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    for (i = 0; i < ARRAY_N(t); i++) {      /* WM_SYSCHAR is the character form too; the lParam ALT bit has no effect on the result */
        WORD cmd = SENT;
        EXPECT_EQ(run(h, NSTD, mkmsg(t[i].msg, 'x', t[i].lp), &cmd), 1);
        EXPECT_EQ(cmd, 102);
    }
    del_table(h);
}
TEST(gap2_isaccel_modifier_flags_must_agree_with_key_state)
{
    static const ACCEL a[] = {
        { FVIRTKEY | FCONTROL, 'C', 200 }, { FVIRTKEY | FSHIFT, 'S', 201 }, { FVIRTKEY | FALT, 'M', 202 },
        { FVIRTKEY | FCONTROL | FSHIFT | FALT, 'Z', 203 },
        { FCONTROL, 'q', 204 },           /* character form: FCONTROL/FSHIFT are not examined */
        { FSHIFT, 'w', 205 },
        { FALT, 'e', 206 },               /* character form: FALT IS examined against GetKeyState(VK_MENU) */
    };
    HACCEL h; WORD cmd;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(a, (int)ARRAY_N(a));
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_KEYDOWN, 'C', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_KEYDOWN, 'S', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_KEYDOWN, 'M', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_SYSKEYDOWN, 'M', 0x20000000), &cmd), 0); CMD0(cmd);   /* the lParam ALT bit is not the ALT key state */
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_KEYDOWN, 'Z', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_CHAR, 'q', 0), &cmd), 1); EXPECT_EQ(cmd, 204);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_CHAR, 'w', 0), &cmd), 1); EXPECT_EQ(cmd, 205);
    cmd = SENT; EXPECT_EQ(run(h, 7, mkmsg(WM_CHAR, 'e', 0), &cmd), 0); CMD0(cmd);
    del_table(h);
}
TEST(gap2_isaccel_entry_count_semantics)
{
    HACCEL h; WORD cmd;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    /* more entries than the table has: CopyAcceleratorTableW returns fewer than requested -> FALSE even for entry 0 */
    cmd = SENT; EXPECT_EQ(run(h, NSTD + 1, mkmsg(WM_KEYDOWN, 'A', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 1000, mkmsg(WM_KEYDOWN, 'A', 0), &cmd), 0); CMD0(cmd);
    /* fewer entries: only the first n are searched */
    cmd = SENT; EXPECT_EQ(run(h, 1, mkmsg(WM_KEYDOWN, 'A', 0), &cmd), 1); EXPECT_EQ(cmd, 100);
    cmd = SENT; EXPECT_EQ(run(h, 1, mkmsg(WM_KEYDOWN, VK_F5, 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 2, mkmsg(WM_KEYDOWN, VK_F5, 0), &cmd), 1); EXPECT_EQ(cmd, 101);
    cmd = SENT; EXPECT_EQ(run(h, 2, mkmsg(WM_CHAR, 'x', 0), &cmd), 0); CMD0(cmd);
    cmd = SENT; EXPECT_EQ(run(h, 3, mkmsg(WM_CHAR, 'x', 0), &cmd), 1); EXPECT_EQ(cmd, 102);
    cmd = SENT; EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, VK_F6, 0), &cmd), 1); EXPECT_EQ(cmd, 104);
    del_table(h);
}
TEST(gap2_isaccel_wparam_truncated_to_word)
{
    HACCEL h; WORD cmd = SENT;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (t_is_wine()) { t_skip("emulator compares the full wParam"); return; }
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 0xABCD0041, 0), &cmd), 1);
    EXPECT_EQ(cmd, 100);
    cmd = SENT;
    EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, (WPARAM)0x1234567800000000ull | 0x74, 0), &cmd), 1);   /* VK_F5 = 0x74 */
    EXPECT_EQ(cmd, 101);
    del_table(h);
}
TEST(gap2_isaccel_null_cmd_pointer_is_allowed)
{
    HACCEL h;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 'A', 0), NULL), 1);
    EXPECT_EQ(run(h, NSTD, mkmsg(WM_KEYDOWN, 'B', 0), NULL), 0);
    del_table(h);
}
TEST(gap2_isaccel_cmd_is_written_on_every_failure_path)
{
    HACCEL h; WORD cmd; MSG m;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    m = mkmsg(WM_COMMAND, 'A', 0); cmd = SENT;
    EXPECT_EQ(ISACC(h, NSTD, &m, &cmd), 0); CMD0(cmd);            /* unsupported message */
    m = mkmsg(WM_KEYDOWN, 'A', 0); cmd = SENT;
    EXPECT_EQ(ISACC(h, 0, &m, &cmd), 0); CMD0(cmd);              /* zero entries */
    cmd = SENT;
    EXPECT_EQ(ISACC(h, NSTD + 3, &m, &cmd), 0); CMD0(cmd);       /* CopyAcceleratorTable short */
    del_table(h);
}
TEST(gap2_isaccel_invalid_and_destroyed_handles)
{
    HACCEL h; WORD cmd = SENT; MSG m = mkmsg(WM_KEYDOWN, 'A', 0);
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    EXPECT_EQ(ISACC((HACCEL)(ULONG_PTR)0x1234567, 4, &m, &cmd), 0);
    CMD0(cmd);
    h = mk_table(std_table, NSTD);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    del_table(h);
    cmd = SENT;
    EXPECT_EQ(ISACC(h, NSTD, &m, &cmd), 0);
    CMD0(cmd);
}
TEST(gap2_isaccel_single_entry_tables)
{
    unsigned k; HACCEL h;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    for (k = 0; k < 4; k++) {
        static const WORD keys[4] = { 'A', VK_RETURN, VK_ESCAPE, 0x41 + 25 };
        __attribute__((aligned(4))) ACCEL a; WORD cmd = SENT;   /* Win10 rejects a 2-byte-aligned ACCEL array with ERROR_NOACCESS (998) */
        a.fVirt = FVIRTKEY; a.key = keys[k]; a.cmd = (WORD)(300 + k);
        h = mk_table(&a, 1);
        if (!h) { t_skip("CreateAcceleratorTableW failed k=%u key=0x%x err=%lu", k, keys[k], GetLastError()); return; }
        EXPECT_EQ(run(h, 1, mkmsg(WM_KEYDOWN, keys[k], 0), &cmd), 1);
        EXPECT_EQ(cmd, 300 + k);
        cmd = SENT;
        EXPECT_EQ(run(h, 1, mkmsg(WM_KEYDOWN, keys[k] ^ 1, 0), &cmd), 0);
        CMD0(cmd);
        EXPECT_EQ(run(h, 2, mkmsg(WM_KEYDOWN, keys[k], 0), NULL), 0);
        del_table(h);
    }
}
TEST(gap2_isaccel_command_ids_full_word_range)
{
    static const ACCEL a[] = { { FVIRTKEY, 'A', 0 }, { FVIRTKEY, 'B', 1 }, { FVIRTKEY, 'C', 0x7fff }, { FVIRTKEY, 'D', 0x8000 }, { FVIRTKEY, 'E', 0xffff } };
    static const WORD want[] = { 0, 1, 0x7fff, 0x8000, 0xffff };
    HACCEL h; unsigned i;
    REQUIRE(ISACC); REQUIRE(UIN("user32.dll", CreateAcceleratorTableW));
    if (!modifiers_idle()) { t_skip("a modifier key is held"); return; }
    h = mk_table(a, 5);
    if (!h) { t_skip("CreateAcceleratorTableW failed"); return; }
    for (i = 0; i < 5; i++) {
        WORD cmd = SENT;
        EXPECT_EQ(run(h, 5, mkmsg(WM_KEYDOWN, 'A' + i, 0), &cmd), 1);
        EXPECT_EQ(cmd, want[i]);
    }
    del_table(h);
}

/* ---------------------------------------------------------------------------------------------------------- */
UDECL(BOOL, IsErrorPropagationEnabled, (void));
#define IEP UIN("combase.dll", IsErrorPropagationEnabled)
TEST(gap2_iep_returns_boolean)
{
    int r, i;
    typedef BOOL (WINAPI *fn_t)(void);
    fn_t f = (fn_t)t_proc_in("combase.dll", "IsErrorPropagationEnabled");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator aborts: unimplemented function"); return; }
    r = f();
    CHECKF(r == 0 || r == 1, "returned %d", r);       /* decomp: (QuirkIsEnabled(0x30000) == 0) */
    for (i = 0; i < 50; i++) EXPECT_EQ(f(), r);       /* process-wide quirk, stable */
    t_note("IsErrorPropagationEnabled -> %d", r);
}
static DWORD WINAPI iep_thread(void *p)
{
    typedef BOOL (WINAPI *fn_t)(void);
    fn_t f = (fn_t)t_proc_in("combase.dll", "IsErrorPropagationEnabled");
    *(int *)p = f ? f() : -1;
    return 0;
}
TEST(gap2_iep_same_on_every_thread_and_apartment)
{
    typedef BOOL (WINAPI *fn_t)(void);
    fn_t f = (fn_t)t_proc_in("combase.dll", "IsErrorPropagationEnabled");
    int a, b = -2; HANDLE th; HRESULT hr;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator aborts: unimplemented function"); return; }
    a = f();
    th = CreateThread(NULL, 0, iep_thread, &b, 0, NULL);
    CHECK(th != NULL);
    if (th) { EXPECT_EQ(WaitForSingleObject(th, 5000), WAIT_OBJECT_0); CloseHandle(th); }
    EXPECT_EQ(b, a);
    hr = U(CoInitializeEx)(NULL, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) { EXPECT_EQ(f(), a); U(CoUninitialize)(); }
}
TEST(gap2_iep_exported_from_combase_not_ole32)
{
    CHECK(t_proc_in("combase.dll", "IsErrorPropagationEnabled") != NULL);
    if (t_proc_in("ole32.dll", "IsErrorPropagationEnabled")) t_note("ole32.dll also exposes IsErrorPropagationEnabled");
}
