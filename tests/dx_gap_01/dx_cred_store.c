#include "dx1.h"
/* Credential store round trips through sechost's Cred* (generic and domain credentials, A and W, flags, limits).
 * Every target is named olet_gapdx_<pid>_*; leftovers from earlier runs (olet_gapdx_*) are swept at the start of each test
 * and everything created is deleted at the end. Needs a logon session that owns a credential set; otherwise skipped. */

static const char *pfx(void) { static char p[64]; if (!p[0]) snprintf(p, sizeof p, "olet_gapdx_%lu_", GetCurrentProcessId()); return p; }
static void tname(char *out, size_t cap, const char *leaf) { snprintf(out, cap, "%s%s", pfx(), leaf); }

static void sweep(void)
{
    DWORD n = 0, i; PCREDENTIALA *c = NULL; char *names[256]; DWORD types[256]; DWORD cnt = 0;
    if (!CR(CredEnumerateA) || !CR(CredDeleteA)) return;
    if (!CR(CredEnumerateA)("olet_gapdx_*", 0, &n, &c)) return;
    for (i = 0; i < n && cnt < 256; i++) { names[cnt] = _strdup(c[i]->TargetName); types[cnt++] = c[i]->Type; }
    CR(CredFree)(c);
    for (i = 0; i < cnt; i++) { CR(CredDeleteA)(names[i], types[i], 0); free(names[i]); }
}

static int usable(void)
{
    DWORD p[16]; unsigned i;
    if (!CR(CredWriteW) || !CR(CredReadW) || !CR(CredDeleteW) || !CR(CredEnumerateW) || !CR(CredFree) || !CR(CredGetSessionTypes)) { t_skip("missing Cred export"); return 0; }
    for (i = 0; i < 16; i++) p[i] = 0;
    if (!CR(CredGetSessionTypes)(7, p) || p[CRED_TYPE_GENERIC] < CRED_PERSIST_SESSION) { t_skip("generic credentials unsupported in this session"); return 0; }
    sweep();
    return 1;
}

static BYTE blob_bytes[DX_BLOB_MAX + 16];
static void fill_blob(void) { unsigned i; for (i = 0; i < sizeof blob_bytes; i++) blob_bytes[i] = (BYTE)(i * 31 + 7); }

/* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): test_generic */
TEST(dx_gap1_cred_generic_lifecycle_w)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; PCREDENTIALW *e = NULL; DWORD n = 0, i; BOOL ok, found = FALSE; FILETIME zero = { 0, 0 };
    if (!usable()) return;
    tname(t, sizeof t, "life"); dx_wide(t, tn, 96);
    fill_blob();
    memset(&c, 0, sizeof c);
    c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Comment = (WCHAR *)L"gap comment"; c.CredentialBlobSize = 37; c.CredentialBlob = blob_bytes;
    c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"gapuser"; c.TargetAlias = (WCHAR *)L"gap-alias";
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0);
    if (!ok && GetLastError() == ERROR_NO_SUCH_LOGON_SESSION) { t_skip("no credential set"); return; }
    CHECKF(ok, "write err %lu", GetLastError()); if (!ok) return;
    SET_ERR(0); ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECKF(ok, "read err %lu", GetLastError());
    if (ok) {
        EXPECT_EQ(r->Type, CRED_TYPE_GENERIC); EXPECT_WSTR(r->TargetName, tn); EXPECT_WSTR(r->Comment, L"gap comment");
        EXPECT_EQ(r->Flags, 0); EXPECT_EQ(r->CredentialBlobSize, 37); CHECK(r->CredentialBlob && !memcmp(r->CredentialBlob, blob_bytes, 37));
        EXPECT_EQ(r->Persist, CRED_PERSIST_SESSION); EXPECT_EQ(r->AttributeCount, 0);
        CHECK(r->UserName && !wcscmp(r->UserName, L"gapuser")); CHECK(r->TargetAlias && !wcscmp(r->TargetAlias, L"gap-alias"));
        CHECK(memcmp(&r->LastWritten, &zero, sizeof zero) != 0);
        /* a single allocation: all strings and the blob lie inside the block that starts at the structure */
        CHECK((BYTE *)r->TargetName > (BYTE *)r && (BYTE *)r->CredentialBlob > (BYTE *)r);
        CR(CredFree)(r);
    }
    /* target names are case-insensitive */
    { WCHAR up[96]; wcscpy(up, tn); CharUpperW(up);
      SET_ERR(0); r = NULL; ok = CR(CredReadW)(up, CRED_TYPE_GENERIC, 0, &r); CHECKF(ok, "upper-case read err %lu", GetLastError()); if (ok) CR(CredFree)(r); }
    /* the same name with another type is a different credential */
    SET_ERR(0); r = NULL; ok = CR(CredReadW)(tn, CRED_TYPE_DOMAIN_PASSWORD, 0, &r); CHECK(!ok); EXPECT_ERR(ERROR_NOT_FOUND);
    /* enumerate by prefix finds it */
    { char f[96]; WCHAR fw[96]; snprintf(f, sizeof f, "%s*", pfx()); dx_wide(f, fw, 96);
      SET_ERR(0); ok = CR(CredEnumerateW)(fw, 0, &n, &e); CHECKF(ok, "enumerate err %lu", GetLastError());
      if (ok) { for (i = 0; i < n; i++) if (e[i]->TargetName && !wcscmp(e[i]->TargetName, tn)) found = TRUE; CHECK(found); EXPECT_EQ(n, 1); CR(CredFree)(e); } }
    SET_ERR(0); ok = CR(CredDeleteW)(tn, CRED_TYPE_GENERIC, 0); CHECKF(ok, "delete err %lu", GetLastError());
    SET_ERR(0); ok = CR(CredDeleteW)(tn, CRED_TYPE_GENERIC, 0); CHECK(!ok); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); r = NULL; ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECK(!ok); EXPECT_ERR(ERROR_NOT_FOUND);
    sweep();
}

TEST(dx_gap1_cred_generic_lifecycle_a)
{
    char t[96]; CREDENTIALA c; PCREDENTIALA r = NULL; PCREDENTIALA *e = NULL; DWORD n = 0; BOOL ok;
    if (!usable()) return;
    if (!CR(CredWriteA) || !CR(CredReadA) || !CR(CredDeleteA) || !CR(CredEnumerateA)) { t_skip("missing A export"); return; }
    tname(t, sizeof t, "lifea"); fill_blob();
    memset(&c, 0, sizeof c);
    c.Type = CRED_TYPE_GENERIC; c.TargetName = t; c.Comment = (char *)"ansi comment"; c.CredentialBlobSize = 20; c.CredentialBlob = blob_bytes;
    c.Persist = CRED_PERSIST_SESSION; c.UserName = (char *)"ansiuser";
    SET_ERR(0); ok = CR(CredWriteA)(&c, 0); CHECKF(ok, "write err %lu", GetLastError()); if (!ok) return;
    SET_ERR(0); ok = CR(CredReadA)(t, CRED_TYPE_GENERIC, 0, &r); CHECKF(ok, "read err %lu", GetLastError());
    if (ok) {
        EXPECT_STR(r->TargetName, t); EXPECT_STR(r->Comment, "ansi comment"); EXPECT_STR(r->UserName, "ansiuser");
        EXPECT_EQ(r->CredentialBlobSize, 20); CHECK(!memcmp(r->CredentialBlob, blob_bytes, 20)); EXPECT_EQ(r->TargetAlias, 0);
        CR(CredFree)(r);
    }
    /* A writes, W reads: strings are converted, the blob is opaque */
    { WCHAR tw[96]; PCREDENTIALW rw = NULL; dx_wide(t, tw, 96);
      SET_ERR(0); ok = CR(CredReadW)(tw, CRED_TYPE_GENERIC, 0, &rw); CHECKF(ok, "W read err %lu", GetLastError());
      if (ok) { EXPECT_WSTR(rw->TargetName, tw); CHECK(!wcscmp(rw->Comment, L"ansi comment")); EXPECT_EQ(rw->CredentialBlobSize, 20); CR(CredFree)(rw); } }
    SET_ERR(0); ok = CR(CredEnumerateA)("olet_gapdx_*", 0, &n, &e); CHECK(ok); if (ok) { CHECK(n >= 1); CR(CredFree)(e); }
    SET_ERR(0); ok = CR(CredDeleteA)(t, CRED_TYPE_GENERIC, 0); CHECK(ok);
    sweep();
}

TEST(dx_gap1_cred_overwrite_replaces)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; BOOL ok;
    if (!usable()) return;
    tname(t, sizeof t, "over"); dx_wide(t, tn, 96); fill_blob();
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u1";
    c.Comment = (WCHAR *)L"first"; c.CredentialBlobSize = 10; c.CredentialBlob = blob_bytes;
    CHECK(CR(CredWriteW)(&c, 0));
    c.Comment = (WCHAR *)L"second"; c.CredentialBlobSize = 4; c.UserName = (WCHAR *)L"u2";
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "overwrite err %lu", GetLastError());
    ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECK(ok);
    if (ok) { CHECK(!wcscmp(r->Comment, L"second")); EXPECT_EQ(r->CredentialBlobSize, 4); CHECK(!wcscmp(r->UserName, L"u2")); CR(CredFree)(r); }
    sweep();
}

TEST(dx_gap1_cred_preserve_blob_flag)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; BOOL ok;
    if (!usable()) return;
    tname(t, sizeof t, "pres"); dx_wide(t, tn, 96); fill_blob();
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u1";
    /* no existing credential: ERROR_NOT_FOUND (documented) */
    c.CredentialBlobSize = 0; c.Comment = (WCHAR *)L"c1";
    SET_ERR(0); ok = CR(CredWriteW)(&c, CRED_PRESERVE_CREDENTIAL_BLOB); CHECK(!ok); EXPECT_ERR(ERROR_NOT_FOUND);
    c.CredentialBlobSize = 12; c.CredentialBlob = blob_bytes; CHECK(CR(CredWriteW)(&c, 0));
    c.CredentialBlobSize = 0; c.CredentialBlob = NULL; c.Comment = (WCHAR *)L"c2";
    SET_ERR(0); ok = CR(CredWriteW)(&c, CRED_PRESERVE_CREDENTIAL_BLOB); CHECKF(ok, "preserve err %lu", GetLastError());
    ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECK(ok);
    if (ok) { CHECK(!wcscmp(r->Comment, L"c2")); EXPECT_EQ(r->CredentialBlobSize, 12); CHECK(r->CredentialBlob && !memcmp(r->CredentialBlob, blob_bytes, 12)); CR(CredFree)(r); }
    /* the flag requires a zero blob size */
    c.CredentialBlobSize = 3; c.CredentialBlob = blob_bytes;
    SET_ERR(0); ok = CR(CredWriteW)(&c, CRED_PRESERVE_CREDENTIAL_BLOB); CHECK(!ok); t_note("preserve with non-zero blob size: error %lu", GetLastError());
    sweep();
}

TEST(dx_gap1_cred_invalid_flags_and_types_on_write)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; BOOL ok; DWORD e;
    if (!usable()) return;
    tname(t, sizeof t, "bad"); dx_wide(t, tn, 96);
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u";
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0x80); e = GetLastError();
    CHECK(!ok); CHECKF(e == ERROR_INVALID_FLAGS || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    c.Type = 0; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); e = GetLastError(); CHECK(!ok); CHECKF(e == ERROR_INVALID_PARAMETER, "type 0 err %lu", e);
    c.Type = (DWORD)-1; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); e = GetLastError(); CHECK(!ok); CHECKF(e == ERROR_INVALID_PARAMETER, "type -1 err %lu", e);
    c.Type = CRED_TYPE_GENERIC; c.Persist = 0; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); e = GetLastError();
    CHECK(!ok); t_note("Persist = CRED_PERSIST_NONE: error %lu", e);
    c.Persist = 4; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); e = GetLastError();
    CHECK(!ok); t_note("Persist = 4: error %lu", e);
    c.Persist = CRED_PERSIST_SESSION; c.TargetName = NULL; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECK(!ok); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    c.TargetName = (WCHAR *)L""; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); e = GetLastError(); CHECK(!ok); t_note("empty TargetName: error %lu", e);
    sweep();
}

TEST(dx_gap1_cred_domain_password_rules)
{
    /* adapted from Wine dlls/advapi32/tests/cred.c (LGPL-2.1+): test_CredWriteA / test_domain_password */
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; BOOL ok; DWORD p[16];
    if (!usable()) return;
    CR(CredGetSessionTypes)(7, p);
    if (p[CRED_TYPE_DOMAIN_PASSWORD] == CRED_PERSIST_NONE) { t_skip("domain passwords disabled"); return; }
    tname(t, sizeof t, "dom"); dx_wide(t, tn, 96); fill_blob();
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_DOMAIN_PASSWORD; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION;
    c.CredentialBlobSize = 8; c.CredentialBlob = blob_bytes;
    c.UserName = NULL; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECK(!ok); EXPECT_ERR(ERROR_BAD_USERNAME);
    c.UserName = (WCHAR *)L"olet\\gapdx"; SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "write err %lu", GetLastError());
    if (ok) {
        ok = CR(CredReadW)(tn, CRED_TYPE_DOMAIN_PASSWORD, 0, &r); CHECKF(ok, "read err %lu", GetLastError());
        if (ok) {
            /* the password of a domain credential is never handed back to ordinary callers */
            EXPECT_EQ(r->CredentialBlobSize, 0); EXPECT_EQ(r->CredentialBlob, 0);
            CHECK(!wcscmp(r->UserName, L"olet\\gapdx")); EXPECT_EQ(r->Type, CRED_TYPE_DOMAIN_PASSWORD);
            CR(CredFree)(r);
        }
        CHECK(CR(CredDeleteW)(tn, CRED_TYPE_DOMAIN_PASSWORD, 0));
    }
    sweep();
}

TEST(dx_gap1_cred_attributes_roundtrip)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; CREDENTIAL_ATTRIBUTEW at[2]; PCREDENTIALW r = NULL; BOOL ok;
    if (!usable()) return;
    tname(t, sizeof t, "attr"); dx_wide(t, tn, 96);
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u";
    memset(at, 0, sizeof at);
    at[0].Keyword = (WCHAR *)L"kw-one"; at[0].ValueSize = 5; at[0].Value = (LPBYTE)"12345";
    at[1].Keyword = (WCHAR *)L"kw-two"; at[1].ValueSize = 0;
    c.AttributeCount = 2; c.Attributes = at;
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "write err %lu", GetLastError()); if (!ok) { sweep(); return; }
    ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECK(ok);
    if (ok) {
        EXPECT_EQ(r->AttributeCount, 2);
        if (r->AttributeCount == 2) {
            CHECK(!wcscmp(r->Attributes[0].Keyword, L"kw-one")); EXPECT_EQ(r->Attributes[0].ValueSize, 5); CHECK(!memcmp(r->Attributes[0].Value, "12345", 5));
            CHECK(!wcscmp(r->Attributes[1].Keyword, L"kw-two")); EXPECT_EQ(r->Attributes[1].ValueSize, 0);
        }
        CR(CredFree)(r);
    }
    sweep();
}

TEST(dx_gap1_cred_blob_size_limit)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; BOOL ok;
    if (!usable()) return;
    tname(t, sizeof t, "blob"); dx_wide(t, tn, 96); fill_blob();
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u";
    c.CredentialBlob = blob_bytes;
    c.CredentialBlobSize = DX_BLOB_MAX;             /* CRED_MAX_CREDENTIAL_BLOB_SIZE = 5*512 (SDK) */
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "2560-byte blob err %lu", GetLastError());
    if (ok) { ok = CR(CredReadW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECK(ok); if (ok) { EXPECT_EQ(r->CredentialBlobSize, DX_BLOB_MAX); CHECK(!memcmp(r->CredentialBlob, blob_bytes, DX_BLOB_MAX)); CR(CredFree)(r); } }
    c.CredentialBlobSize = DX_BLOB_MAX + 1;
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECK(!ok); t_note("blob 2561 bytes: error %lu", GetLastError());
    c.CredentialBlobSize = 0; c.CredentialBlob = NULL;
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "empty blob err %lu", GetLastError());
    sweep();
}

TEST(dx_gap1_cred_string_length_limits)
{
    CREDENTIALW c; BOOL ok; WCHAR *big = calloc(40000, sizeof(WCHAR)); WCHAR tn[96]; char t[96]; unsigned i;
    if (!usable()) { free(big); return; }
    tname(t, sizeof t, "len"); dx_wide(t, tn, 96);
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u";
    for (i = 0; i < 256; i++) big[i] = L'c'; big[256] = 0;
    c.Comment = big;                                 /* CRED_MAX_STRING_LENGTH == 256 characters */
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "256-char comment err %lu", GetLastError());
    big[256] = L'c'; big[257] = 0;
    SET_ERR(0); ok = CR(CredWriteW)(&c, 0); t_note("257-char comment: ok=%d error %lu", ok, GetLastError());
    c.Comment = NULL;
    for (i = 0; i < 200; i++) big[i] = L'T'; big[200] = 0;   /* generic target names go up to 32767 characters */
    memcpy(big, L"olet_gapdx_", 22);
    { WCHAR *keep = c.TargetName; c.TargetName = big; swprintf(big + 11, 20, L"%lu_", GetCurrentProcessId());
      { size_t k = wcslen(big); while (k < 200) big[k++] = L'T'; big[200] = 0; }
      SET_ERR(0); ok = CR(CredWriteW)(&c, 0); CHECKF(ok, "200-char target err %lu", GetLastError());
      c.TargetName = keep; }
    free(big);
    sweep();
}

TEST(dx_gap1_cred_find_best_generic)
{
    WCHAR tn[96]; char t[96]; CREDENTIALW c; PCREDENTIALW r = NULL; BOOL ok;
    if (!usable() || !CR(CredFindBestCredentialW)) return;
    tname(t, sizeof t, "best"); dx_wide(t, tn, 96); fill_blob();
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"bestuser";
    c.CredentialBlobSize = 6; c.CredentialBlob = blob_bytes;
    CHECK(CR(CredWriteW)(&c, 0));
    SET_ERR(0); ok = CR(CredFindBestCredentialW)(tn, CRED_TYPE_GENERIC, 0, &r); CHECKF(ok, "find best err %lu", GetLastError());
    if (ok) { CHECK(!wcscmp(r->TargetName, tn)); CHECK(!wcscmp(r->UserName, L"bestuser")); CR(CredFree)(r); }
    if (CR(CredFindBestCredentialA)) {
        PCREDENTIALA ra = NULL; SET_ERR(0); ok = CR(CredFindBestCredentialA)(t, CRED_TYPE_GENERIC, 0, &ra); CHECKF(ok, "A err %lu", GetLastError());
        if (ok) { EXPECT_STR(ra->TargetName, t); CR(CredFree)(ra); }
    }
    sweep();
}

TEST(dx_gap1_cred_enumerate_filter_semantics)
{
    WCHAR tn[3][96]; char t[96]; CREDENTIALW c; PCREDENTIALW *e = NULL; DWORD n = 0, i; BOOL ok; char f[96]; WCHAR fw[96];
    static const char *const leaf[3] = { "enum_a", "enum_b", "other_c" };
    if (!usable()) return;
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_GENERIC; c.Persist = CRED_PERSIST_SESSION; c.UserName = (WCHAR *)L"u";
    for (i = 0; i < 3; i++) { tname(t, sizeof t, leaf[i]); dx_wide(t, tn[i], 96); c.TargetName = tn[i]; CHECK(CR(CredWriteW)(&c, 0)); }
    snprintf(f, sizeof f, "%senum_*", pfx()); dx_wide(f, fw, 96);
    SET_ERR(0); ok = CR(CredEnumerateW)(fw, 0, &n, &e); CHECK(ok); if (ok) { EXPECT_EQ(n, 2); CR(CredFree)(e); }
    snprintf(f, sizeof f, "%s*", pfx()); dx_wide(f, fw, 96);
    SET_ERR(0); ok = CR(CredEnumerateW)(fw, 0, &n, &e); CHECK(ok); if (ok) { EXPECT_EQ(n, 3); CR(CredFree)(e); }
    snprintf(f, sizeof f, "%sENUM_*", pfx()); dx_wide(f, fw, 96);
    SET_ERR(0); ok = CR(CredEnumerateW)(fw, 0, &n, &e); CHECK(ok); if (ok) { t_note("upper-case filter matched %lu", n); CR(CredFree)(e); }
    /* a filter without a trailing asterisk is a plain name match */
    snprintf(f, sizeof f, "%sother_c", pfx()); dx_wide(f, fw, 96);
    SET_ERR(0); ok = CR(CredEnumerateW)(fw, 0, &n, &e); CHECK(ok); if (ok) { t_note("exact filter matched %lu", n); CR(CredFree)(e); }
    /* ALL_CREDENTIALS includes ours too */
    SET_ERR(0); ok = CR(CredEnumerateW)(NULL, CRED_ENUMERATE_ALL_CREDENTIALS, &n, &e); CHECK(ok);
    if (ok) { DWORD hit = 0; for (i = 0; i < n; i++) if (e[i]->TargetName && !wcsncmp(e[i]->TargetName, tn[0], 11)) hit++; CHECKF(hit >= 3, "hit %lu", hit); CR(CredFree)(e); }
    sweep();
}

TEST(dx_gap1_cred_domain_credentials_roundtrip)
{
    CREDENTIAL_TARGET_INFORMATIONW ti; CREDENTIALW c; WCHAR tn[96]; char t[96]; DWORD n = 0, p[16]; PCREDENTIALW *e = NULL; BOOL ok;
    if (!usable() || !CR(CredWriteDomainCredentialsW) || !CR(CredReadDomainCredentialsW)) return;
    CR(CredGetSessionTypes)(7, p);
    if (p[CRED_TYPE_DOMAIN_PASSWORD] == CRED_PERSIST_NONE) { t_skip("domain passwords disabled"); return; }
    tname(t, sizeof t, "domcred"); dx_wide(t, tn, 96); fill_blob();
    memset(&ti, 0, sizeof ti); ti.TargetName = tn; ti.DnsServerName = tn;
    memset(&c, 0, sizeof c); c.Type = CRED_TYPE_DOMAIN_PASSWORD; c.TargetName = tn; c.Persist = CRED_PERSIST_SESSION;
    c.UserName = (WCHAR *)L"olet\\gapdxdom"; c.CredentialBlobSize = 8; c.CredentialBlob = blob_bytes;
    SET_ERR(0); ok = CR(CredWriteDomainCredentialsW)(&ti, &c, 0); CHECKF(ok, "write domain err %lu", GetLastError()); if (!ok) { sweep(); return; }
    SET_ERR(0); ok = CR(CredReadDomainCredentialsW)(&ti, 0, &n, &e); CHECKF(ok, "read domain err %lu", GetLastError());
    if (ok) {
        DWORD i, hit = 0;
        CHECK(n >= 1);
        for (i = 0; i < n; i++) if (e[i]->TargetName && !wcscmp(e[i]->TargetName, tn)) { hit++; CHECK(!wcscmp(e[i]->UserName, L"olet\\gapdxdom")); }
        EXPECT_EQ(hit, 1);
        CR(CredFree)(e);
    }
    if (CR(CredGetTargetInfoW)) {
        PCREDENTIAL_TARGET_INFORMATIONW r = NULL; SET_ERR(0); ok = CR(CredGetTargetInfoW)(tn, 0, &r);
        t_note("CredGetTargetInfoW after CredWriteDomainCredentials: ok=%d err %lu", ok, GetLastError());
        if (ok) { CHECK(r->TargetName || r->DnsServerName); CR(CredFree)(r); }
    }
    CR(CredDeleteW)(tn, CRED_TYPE_DOMAIN_PASSWORD, 0);
    sweep();
}
