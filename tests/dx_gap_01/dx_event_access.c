#include "dx1.h"
/* EventAccessControl / EventAccessQuery / EventAccessRemove with a throw-away provider GUID. They edit
 * HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security (value named after the GUID), so administrator rights are required.
 * The GUID is unique per process and the value is removed in every exit path; a leftover from an earlier crashed run
 * (guid prefix 0x01e7d0xx) is swept first. */
typedef ULONG (WINAPI *EAC_fn)(LPCGUID, ULONG, PSID, ULONG, BOOLEAN);
typedef ULONG (WINAPI *EAQ_fn)(LPCGUID, PSECURITY_DESCRIPTOR, PULONG);
typedef ULONG (WINAPI *EAR_fn)(LPCGUID);
#define EAC ((EAC_fn)t_proc_in("sechost.dll", "EventAccessControl"))
#define EAQ ((EAQ_fn)t_proc_in("sechost.dll", "EventAccessQuery"))
#define EAR ((EAR_fn)t_proc_in("sechost.dll", "EventAccessRemove"))

static const BYTE sid_world[12] = { 1, 1, 0,0,0,0,0,1, 0,0,0,0 };                      /* S-1-1-0 */
static const BYTE sid_authusers[12] = { 1, 1, 0,0,0,0,0,5, 11,0,0,0 };                 /* S-1-5-11 */

static GUID mkguid(BYTE salt)
{
    GUID g = { 0x01e7d000, 0x6a01, 0x4a01, { 0x80, 0x1e, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00 } };
    DWORD pid = GetCurrentProcessId();
    g.Data1 |= (pid & 0xff); g.Data4[6] = salt; g.Data4[7] = (BYTE)(pid >> 8); g.Data2 ^= (WORD)(pid >> 16);
    return g;
}

static int gate(void)
{
    if (t_is_wine()) { t_skip("sechost EventAccess* are abort-stubs under the emulator"); return 0; }
    if (!EAC || !EAQ || !EAR) { t_skip("missing EventAccess export"); return 0; }
    if (!dx_is_admin()) { t_skip("needs administrator"); return 0; }
    return 1;
}

static BYTE *query(const GUID *g, ULONG *sz, ULONG *err)
{
    ULONG n = 0, r; BYTE *b;
    r = EAQ(g, NULL, &n);
    if (r != ERROR_MORE_DATA) { *err = r; return NULL; }
    b = calloc(1, n + 16); *sz = n;
    r = EAQ(g, b, sz);
    if (r) { *err = r; free(b); return NULL; }
    *err = 0; return b;
}

/* collect ACEs: counts of allowed/denied ACEs for a SID */
static void ace_stats(BYTE *sd, const BYTE *sid, int *allowed, int *denied, DWORD *amask, int *total)
{
    BOOL present, def; PACL acl; DWORD i;
    *allowed = *denied = *total = 0; *amask = 0;
    if (!GetSecurityDescriptorDacl(sd, &present, &acl, &def) || !present || !acl) return;
    *total = acl->AceCount;
    for (i = 0; i < acl->AceCount; i++) {
        ACCESS_ALLOWED_ACE *a;
        if (!GetAce(acl, i, (void **)&a)) continue;
        if (EqualSid((PSID)&a->SidStart, (PSID)sid)) {
            if (a->Header.AceType == ACCESS_ALLOWED_ACE_TYPE) { (*allowed)++; *amask |= a->Mask; }
            else if (a->Header.AceType == ACCESS_DENIED_ACE_TYPE) (*denied)++;
        }
    }
}

TEST(dx_gap1_event_access_dacl_lifecycle)
{
    GUID g = mkguid(1); ULONG sz = 0, err = 0, r; BYTE *base, *cur; int al, de, tot, base_tot; DWORD mask;
    if (!gate()) return;
    EAR(&g);                                                        /* sweep */
    SET_ERR(0); r = EAR(&g); EXPECT_EQ(r, ERROR_FILE_NOT_FOUND); EXPECT_ERR(ERROR_FILE_NOT_FOUND);   /* nothing registered yet */
    base = query(&g, &sz, &err); CHECKF(base != NULL, "baseline query err %lu", err); if (!base) return;
    ace_stats(base, sid_world, &al, &de, &mask, &base_tot);
    SET_ERR(0); r = EAC(&g, 0 /* EventSecuritySetDACL */, (PSID)sid_world, 0x1 /* WMIGUID_QUERY */, TRUE);
    EXPECT_EQ(r, 0);
    if (r) { t_note("EventAccessControl(SetDACL) -> %lu", r); EAR(&g); free(base); return; }
    cur = query(&g, &sz, &err); CHECKF(cur != NULL, "query err %lu", err);
    if (cur) {
        CHECK(IsValidSecurityDescriptor(cur));
        ace_stats(cur, sid_world, &al, &de, &mask, &tot);
        EXPECT_EQ(al, 1); EXPECT_EQ(de, 0); CHECK(mask & 0x1);
        EXPECT_EQ(tot, 1);                                          /* SetDACL replaces the whole DACL */
        free(cur);
    }
    /* add another allowed ACE */
    SET_ERR(0); r = EAC(&g, 2 /* AddDACL */, (PSID)sid_authusers, 0x2, TRUE); EXPECT_EQ(r, 0);
    cur = query(&g, &sz, &err);
    if (cur) {
        int a2, d2, t2; DWORD m2;
        ace_stats(cur, sid_world, &al, &de, &mask, &tot); ace_stats(cur, sid_authusers, &a2, &d2, &m2, &t2);
        EXPECT_EQ(al, 1); EXPECT_EQ(a2, 1); CHECK(m2 & 0x2); EXPECT_EQ(tot, 2);
        free(cur);
    }
    /* a deny ACE */
    SET_ERR(0); r = EAC(&g, 2, (PSID)sid_authusers, 0x4, FALSE); EXPECT_EQ(r, 0);
    cur = query(&g, &sz, &err);
    if (cur) { int a2, d2, t2; DWORD m2; ace_stats(cur, sid_authusers, &a2, &d2, &m2, &t2); CHECK(d2 >= 1); EXPECT_EQ(tot, 2); free(cur); }
    /* SetDACL again drops everything but the new ACE */
    SET_ERR(0); r = EAC(&g, 0, (PSID)sid_authusers, 0x8, TRUE); EXPECT_EQ(r, 0);
    cur = query(&g, &sz, &err);
    if (cur) { int a2, d2, t2; DWORD m2; ace_stats(cur, sid_authusers, &a2, &d2, &m2, &t2); EXPECT_EQ(t2, 1); EXPECT_EQ(a2, 1); CHECK(m2 & 0x8); free(cur); }
    /* remove: the provider falls back to the default security descriptor */
    SET_ERR(0); r = EAR(&g); EXPECT_EQ(r, 0);
    cur = query(&g, &sz, &err);
    if (cur) { ace_stats(cur, sid_world, &al, &de, &mask, &tot); EXPECT_EQ(tot, base_tot); free(cur); }
    SET_ERR(0); r = EAR(&g); EXPECT_EQ(r, ERROR_FILE_NOT_FOUND); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    free(base);
}

TEST(dx_gap1_event_access_sacl_operations_note)
{
    GUID g = mkguid(2); ULONG r;
    if (!gate()) return;
    EAR(&g);
    SET_ERR(0); r = EAC(&g, 1 /* SetSACL */, (PSID)sid_world, 0x1, TRUE);
    t_note("EventAccessControl(SetSACL) -> %lu", r);
    SET_ERR(0); r = EAC(&g, 3 /* AddSACL */, (PSID)sid_world, 0x1, TRUE);
    t_note("EventAccessControl(AddSACL) -> %lu", r);
    EAR(&g);
}

TEST(dx_gap1_event_access_query_buffer_protocol_after_set)
{
    GUID g = mkguid(3); ULONG sz = 0, need, r; BYTE *b;
    if (!gate()) return;
    EAR(&g);
    r = EAC(&g, 0, (PSID)sid_world, 0x1, TRUE);
    if (r) { t_note("EventAccessControl -> %lu", r); EAR(&g); return; }
    SET_ERR(0); r = EAQ(&g, NULL, &sz); EXPECT_EQ(r, ERROR_MORE_DATA); need = sz; CHECK(need >= 20);
    b = calloc(1, need + 8);
    /* Windows 10: a zero-length buffer is ERROR_MORE_DATA with the real size; a buffer a few bytes short of the stored
     * value is still accepted when the descriptor itself fits (observed: need-1 .. need-4 return 0 after SetDACL) */
    sz = 0; r = EAQ(&g, b, &sz); EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_EQ(sz, need);
    sz = need - 1; r = EAQ(&g, b, &sz); t_note("buffer one byte short -> %lu", r); CHECK(r == 0 || r == ERROR_MORE_DATA); EXPECT_EQ(sz, need);
    sz = need; r = EAQ(&g, b, &sz); EXPECT_EQ(r, 0); EXPECT_EQ(sz, need); CHECK(IsValidSecurityDescriptor(b));
    sz = need + 5; r = EAQ(&g, b, &sz); EXPECT_EQ(r, 0); EXPECT_EQ(sz, need);       /* *size is replaced by the real length */
    free(b);
    EAR(&g);
}

TEST(dx_gap1_event_access_two_guids_independent)
{
    GUID g1 = mkguid(4), g2 = mkguid(5); ULONG s1, s2, e, r; BYTE *a, *b; int al, de, tot; DWORD m;
    if (!gate()) return;
    EAR(&g1); EAR(&g2);
    r = EAC(&g1, 0, (PSID)sid_world, 0x1, TRUE);
    if (r) { t_note("EventAccessControl -> %lu", r); EAR(&g1); return; }
    a = query(&g1, &s1, &e); b = query(&g2, &s2, &e);
    if (a && b) { ace_stats(a, sid_world, &al, &de, &m, &tot); EXPECT_EQ(tot, 1); CHECK(s1 != s2 || memcmp(a, b, s1) != 0); }
    free(a); free(b);
    SET_ERR(0); r = EAR(&g2); EXPECT_EQ(r, ERROR_FILE_NOT_FOUND);       /* g2 was never set */
    r = EAR(&g1); EXPECT_EQ(r, 0);          /* not EXPECT_EQ(EAR(..)): the macro evaluates its argument twice */
}
