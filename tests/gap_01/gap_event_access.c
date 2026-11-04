#include "g1.h"
/* EventAccessControl / EventAccessQuery / EventAccessRemove (sechost.dll). ETW provider security descriptors live in
 * HKLM\SYSTEM\CurrentControlSet\Control\WMI\Security. Only the non-mutating surface is exercised here; the lifecycle
 * (which writes that key) is in dx_gap_01.
 * Decomp facts: EventAccessControl: NULL guid / NULL sid / operation > 3 -> 87 (return value AND last error).
 *               EventAccessQuery: NULL guid / NULL size -> 87; NULL buffer with a readable key -> ERROR_MORE_DATA and the
 *               needed size in *size; a buffer that is not a valid relative SD -> ERROR_INVALID_SECURITY_DESCR (1338).
 *               EventAccessRemove: NULL guid -> 87. */

typedef ULONG (WINAPI *EAC_fn)(LPCGUID, ULONG, PSID, ULONG, BOOLEAN);
typedef ULONG (WINAPI *EAQ_fn)(LPCGUID, PSECURITY_DESCRIPTOR, PULONG);
typedef ULONG (WINAPI *EAR_fn)(LPCGUID);
#define EAC (t_is_wine() ? NULL : (EAC_fn)t_proc_in("sechost.dll", "EventAccessControl"))
#define EAQ (t_is_wine() ? NULL : (EAQ_fn)t_proc_in("sechost.dll", "EventAccessQuery"))
#define EAR (t_is_wine() ? NULL : (EAR_fn)t_proc_in("sechost.dll", "EventAccessRemove"))

static const BYTE sid_world[12] = { 1, 1, 0,0,0,0,0,1, 0,0,0,0 };      /* S-1-1-0 */

static GUID unique_guid(DWORD salt)
{
    GUID g = { 0x01e7a901, 0x6a01, 0x4a01, { 0x80, 0x1e, 0x74, 0x65, 0x73, 0x74, 0x00, 0x00 } };
    DWORD pid = GetCurrentProcessId();
    g.Data1 ^= pid; g.Data4[6] = (BYTE)(salt); g.Data4[7] = (BYTE)(pid >> 8);
    return g;
}

TEST(gap1_event_access_exports_present)
{
    CHECK(t_proc_in("sechost.dll", "EventAccessControl") != NULL); CHECK(t_proc_in("sechost.dll", "EventAccessQuery") != NULL);
    CHECK(t_proc_in("sechost.dll", "EventAccessRemove") != NULL);
    if (!t_is_wine()) { CHECK(t_proc_in("advapi32.dll", "EventAccessControl") != NULL); }
}

TEST(gap1_event_access_control_invalid_args)
{
    GUID g = unique_guid(1); ULONG r; ULONG op;
    REQUIRE(EAC);
    SET_ERR(0); r = EAC(NULL, 0, (PSID)sid_world, 1, TRUE);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = EAC(&g, 0, NULL, 1, TRUE);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = EAC(NULL, 0, NULL, 1, TRUE);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* operations 0..3 are defined (set/add DACL/SACL); anything above is rejected before the registry is touched */
    { static const ULONG ops[] = { 4, 5, 7, 0x80, 0xffff, 0x80000000, 0xffffffff };
      unsigned i;
      for (i = 0; i < sizeof ops / sizeof *ops; i++) {
          op = ops[i]; SET_ERR(0); r = EAC(&g, op, (PSID)sid_world, 1, TRUE);
          CHECKF(r == ERROR_INVALID_PARAMETER, "op %lx -> %lu", op, r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
      } }
}

TEST(gap1_event_access_control_invalid_args_independent_of_rights)
{
    GUID g = unique_guid(2); static const ULONG rights[] = { 0, 1, 0x1f, 0xffffffff }; unsigned i; ULONG r;
    REQUIRE(EAC);
    for (i = 0; i < 4; i++) {
        SET_ERR(0); r = EAC(NULL, 0, (PSID)sid_world, rights[i], FALSE); EXPECT_EQ(r, 87); EXPECT_ERR(87);
        SET_ERR(0); r = EAC(&g, 9, (PSID)sid_world, rights[i], TRUE);  EXPECT_EQ(r, 87); EXPECT_ERR(87);
    }
}

TEST(gap1_event_access_query_invalid_args)
{
    GUID g = unique_guid(3); ULONG sz = 0, r; BYTE buf[16];
    REQUIRE(EAQ);
    SET_ERR(0); r = EAQ(NULL, buf, &sz); EXPECT_EQ(r, 87); EXPECT_ERR(87);
    SET_ERR(0); r = EAQ(&g, buf, NULL);  EXPECT_EQ(r, 87); EXPECT_ERR(87);
    SET_ERR(0); r = EAQ(&g, NULL, NULL); EXPECT_EQ(r, 87); EXPECT_ERR(87);
    SET_ERR(0); r = EAQ(NULL, NULL, &sz); EXPECT_EQ(r, 87); EXPECT_ERR(87);
}

TEST(gap1_event_access_remove_invalid_args)
{
    ULONG r;
    REQUIRE(EAR);
    SET_ERR(0); r = EAR(NULL); EXPECT_EQ(r, 87); EXPECT_ERR(87);
}

/* Returns 1 when the registry is readable, otherwise notes why not. */
static ULONG last_query_rc;
static int can_query(void)
{
    GUID g = unique_guid(4); ULONG sz = 0, r;
    SET_ERR(0); r = EAQ(&g, NULL, &sz); last_query_rc = r;
    if (r == ERROR_MORE_DATA) return 1;
    t_note("EventAccessQuery(unregistered guid, NULL buffer) returned %lu (size %lu): registry not readable here", r, sz);
    return 0;
}

TEST(gap1_event_access_query_size_protocol)
{
    GUID g = unique_guid(5); ULONG sz = 0, need, r; BYTE *b; BYTE tiny[2] = { 0xaa, 0xaa };
    REQUIRE(EAQ);
    if (!can_query()) {
        /* a restricted (non-admin) token cannot read the trace security registry: environment, not a defect */
        if (last_query_rc == ERROR_ACCESS_DENIED) { t_skip("EventAccessQuery needs read access to the trace security key (restricted token)"); return; }
        if (!t_is_wine()) CHECK(0 && "EventAccessQuery not usable on Windows");
        return;
    }
    /* an unregistered provider falls back to the default trace security descriptor */
    sz = 0; SET_ERR(0); r = EAQ(&g, NULL, &sz);
    EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_ERR(ERROR_MORE_DATA); CHECK(sz >= 20);   /* a relative SD header is 20 bytes */
    need = sz;
    sz = 12345; SET_ERR(0); r = EAQ(&g, NULL, &sz);       /* NULL buffer reports the size whatever *size said */
    EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_EQ(sz, need);
    sz = 1; SET_ERR(0); r = EAQ(&g, tiny, &sz);
    EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_EQ(sz, need); CHECK(tiny[0] == 0xaa && tiny[1] == 0xaa);
    sz = 0; SET_ERR(0); r = EAQ(&g, tiny, &sz);
    EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_EQ(sz, need);
    sz = need - 1; b = malloc(need + 8); memset(b, 0xaa, need + 8); SET_ERR(0); r = EAQ(&g, b, &sz);
    EXPECT_EQ(r, ERROR_MORE_DATA); EXPECT_EQ(sz, need);
    sz = need; SET_ERR(0); r = EAQ(&g, b, &sz);
    EXPECT_EQ(r, 0); EXPECT_EQ(sz, need); CHECK(b[need] == 0xaa);
    free(b);
}

TEST(gap1_event_access_query_returns_valid_relative_sd)
{
    GUID g = unique_guid(6); ULONG sz = 0, r; BYTE *b; SECURITY_DESCRIPTOR_CONTROL ctl; DWORD rev; BOOL present, def; PACL dacl;
    REQUIRE(EAQ);
    if (!can_query()) return;
    SET_ERR(0); r = EAQ(&g, NULL, &sz); EXPECT_EQ(r, ERROR_MORE_DATA);
    b = calloc(1, sz + 8);
    r = EAQ(&g, b, &sz); EXPECT_EQ(r, 0);
    CHECK(IsValidSecurityDescriptor(b));
    CHECK(GetSecurityDescriptorControl(b, &ctl, &rev));
    CHECK(ctl & SE_SELF_RELATIVE);
    EXPECT_EQ(rev, 1);
    CHECK(GetSecurityDescriptorDacl(b, &present, &dacl, &def));
    CHECK(present);
    if (present && dacl) t_note("default trace SD: DACL has %u ACEs", dacl->AceCount);
    /* two unregistered providers get the same default descriptor */
    { GUID g2 = unique_guid(7); ULONG s2 = sz; BYTE *b2 = calloc(1, sz + 8);
      r = EAQ(&g2, b2, &s2); EXPECT_EQ(r, 0); EXPECT_EQ(s2, sz); CHECK(!memcmp(b, b2, sz)); free(b2); }
    free(b);
}

TEST(gap1_event_access_query_a_zero_guid)
{
    GUID z; ULONG sz = 0, r;
    REQUIRE(EAQ);
    memset(&z, 0, sizeof z);
    if (!can_query()) return;
    SET_ERR(0); r = EAQ(&z, NULL, &sz);
    t_note("EventAccessQuery(GUID_NULL, NULL buffer) -> %lu size %lu", r, sz);
    CHECKF(r == ERROR_MORE_DATA || r == ERROR_FILE_NOT_FOUND, "r=%lu", r);
}
