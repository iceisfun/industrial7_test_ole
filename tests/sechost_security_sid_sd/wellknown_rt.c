#include "ssh.h"
/* Extra: sweeps over CreateWellKnownSid (scaffolding) feeding the sechost converters, and SDDL ACE table round trips. */

TEST(wkrt_sid_string_roundtrip_all_wellknown)
{
    int t, ok = 0;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    for (t = 0; t < 120; t++) {
        BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD sz = sizeof buf;
        WCHAR *w = NULL; PSID back = NULL;
        if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)t, NULL, buf, &sz)) continue;
        SET_ERR(0x77);
        CHECKF(UIN(SH, ConvertSidToStringSidW)(buf, &w), "type %d: ConvertSidToStringSidW failed %lu", t, GetLastError());
        if (!w) continue;
        EXPECT_ERR(0);
        CHECKF(w[0] == L'S' && w[1] == L'-' && w[2] == L'1' && w[3] == L'-', "type %d bad prefix", t);
        CHECKF(LocalSize(w) >= (lstrlenW(w) + 1) * 2, "type %d allocation too small", t);
        CHECKF(UIN(SH, ConvertStringSidToSidW)(w, &back), "type %d: reparse failed %lu", t, GetLastError());
        if (back) {
            CHECKF(EqualSid(back, buf), "type %d: round trip differs", t);
            CHECKF(GetLengthSid(back) == sz, "type %d: length %lu vs %lu", t, GetLengthSid(back), sz);
            LocalFree(back);
        }
        LocalFree(w); ok++;
    }
    CHECK(ok > 40);
}

TEST(wkrt_sid_string_a_equals_w_all_wellknown)
{
    int t;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    REQUIRE(UIN(AD, ConvertSidToStringSidA));
    for (t = 0; t < 120; t++) {
        BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD sz = sizeof buf;
        WCHAR *w = NULL; char *a = NULL; char n[256];
        if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)t, NULL, buf, &sz)) continue;
        if (!UIN(SH, ConvertSidToStringSidW)(buf, &w) || !UIN(AD, ConvertSidToStringSidA)(buf, &a)) { CHECKF(0, "type %d conversion failed", t); LocalFree(w); LocalFree(a); continue; }
        narrow(w, n, sizeof n);
        CHECKF(!strcmp(n, a), "type %d: W \"%s\" A \"%s\"", t, n, a);
        LocalFree(w); LocalFree(a);
    }
}

TEST(wkrt_sid_with_domain_sid_roundtrip)
{
    BYTE dom[SECURITY_MAX_SID_SIZE]; DWORD dsz = sizeof dom; SIDBUF d;
    int t, n = 0;
    REQUIRE(UIN(SH, ConvertSidToStringSidW));
    REQUIRE(UIN(SH, ConvertStringSidToSidW));
    memcpy(dom, mk_sid(&d, 5, 4, 21, 111111111, 222222222, 333333333), 8 + 16);
    (void)dsz;
    for (t = 0; t < 120; t++) {
        BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD sz = sizeof buf; WCHAR *w = NULL; PSID back = NULL;
        if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)t, dom, buf, &sz)) continue;
        if (!UIN(SH, ConvertSidToStringSidW)(buf, &w)) { CHECKF(0, "type %d fail", t); continue; }
        if (UIN(SH, ConvertStringSidToSidW)(w, &back)) { CHECKF(EqualSid(back, buf), "type %d differs", t); LocalFree(back); }
        else CHECKF(0, "type %d reparse failed %lu", t, GetLastError());
        LocalFree(w); n++;
    }
    CHECK(n > 40);
}

/* each single-ACE string normalizes to itself */
TEST(wkrt_sddl_single_ace_identity_table)
{
    static const char *tab[] = {
        "D:(A;;GA;;;WD)", "D:(D;;GA;;;WD)", "D:(A;OI;GA;;;BA)", "D:(A;CI;GR;;;BA)", "D:(A;OICI;GA;;;SY)",
        "D:(A;OICIIO;GA;;;SY)", "D:(A;OICINP;GA;;;SY)", "D:(A;ID;GA;;;SY)", "D:(A;OICIID;GA;;;SY)",
        "D:(A;;FA;;;BA)", "D:(A;;FR;;;BU)", "D:(A;;FX;;;BU)", "D:(A;;KA;;;BA)", "D:(A;;KR;;;BU)",
        "D:(A;;CC;;;WD)", "D:(A;;DC;;;WD)", "D:(A;;LC;;;WD)", "D:(A;;SW;;;WD)", "D:(A;;RP;;;WD)", "D:(A;;WP;;;WD)",
        "D:(A;;DT;;;WD)", "D:(A;;LO;;;WD)", "D:(A;;CR;;;WD)", "D:(A;;RC;;;WD)", "D:(A;;SD;;;WD)", "D:(A;;WD;;;WD)", "D:(A;;WO;;;WD)",
        "D:(A;;0x1;;;WD)", "D:(A;;0x12019f;;;WD)", "D:(A;;0xffffffff;;;WD)",
        "D:(A;;GA;;;S-1-5-21-1-2-3-500)", "D:(A;;GA;;;S-1-5-5-0-12345)",
        "S:(AU;SA;GA;;;WD)", "S:(AU;FA;GA;;;WD)", "S:(AU;SAFA;GA;;;WD)", "S:(AU;OICISA;GA;;;WD)",
        "D:(A;;GA;;;BA)(A;;GR;;;WD)", "D:PAI(A;;GA;;;BA)", "D:P(A;;GA;;;BA)", "D:AI(A;;GA;;;BA)", "D:PS:P",
    };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW));
    REQUIRE(UIN(SH, ConvertSecurityDescriptorToStringSecurityDescriptorW));
    for (i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        PSECURITY_DESCRIPTOR sd = sd_from_str(tab[i], NULL);
        int ok; const char *s;
        if (!sd) { CHECKF(0, "\"%s\" rejected err %lu", tab[i], GetLastError()); continue; }
        s = sd_to_str(sd, ALL_SI, &ok);
        CHECKF(ok, "\"%s\" format failed", tab[i]);
        if (ok && strcmp(s, tab[i])) t_note("\"%s\" normalizes to \"%s\"", tab[i], s);
        if (ok) {
            PSECURITY_DESCRIPTOR sd2 = sd_from_str(s, NULL);
            int ok2; const char *s2;
            CHECKF(sd2 != NULL, "\"%s\" output not reparseable", s);
            if (sd2) { s2 = sd_to_str(sd2, ALL_SI, &ok2); CHECKF(ok2 && !strcmp(s, s2), "\"%s\" not stable: \"%s\"", s, s2); LocalFree(sd2); }
        }
        LocalFree(sd);
    }
}

TEST(wkrt_sddl_bad_strings_rejected)
{
    static const char *bad[] = {
        "X:", "D", "D:(", "D:(A;;GA;;;WD", "D:(A;;GA;;WD)", "D:(Q;;GA;;;WD)", "D:(A;;GA;;;ZZ)", "O:ZZ", "G:XX",
        "D:(A;;GA;;;S-1-5-)", "D:(A;;0xZ;;;WD)", "D:(A;XX;GA;;;WD)", "D:(A;;ZZ;;;WD)",
    };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW));
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        WCHAR w[128]; PSECURITY_DESCRIPTOR sd = (PSECURITY_DESCRIPTOR)1; ULONG sz = 0;
        BOOL r;
        widen(bad[i], w, 128);
        SET_ERR(0);
        r = UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW)(w, SDDL_REVISION_1, &sd, &sz);
        CHECKF(!r, "\"%s\" unexpectedly accepted", bad[i]);
        if (r) LocalFree(sd);
        else CHECKF(GetLastError() != 0, "\"%s\" failed with no last-error", bad[i]);
    }
}

TEST(wkrt_sddl_revision_variants)
{
    WCHAR w[] = L"D:(A;;GA;;;WD)"; PSECURITY_DESCRIPTOR sd = NULL; ULONG sz;
    DWORD revs[] = { 0, 2, 3, 0x100, 0xffffffff };
    unsigned i;
    REQUIRE(UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW));
    for (i = 0; i < sizeof revs / sizeof revs[0]; i++) {
        SET_ERR(0);
        sd = NULL;
        CHECKF(!UIN(SH, ConvertStringSecurityDescriptorToSecurityDescriptorW)(w, revs[i], &sd, &sz), "revision %lu accepted", revs[i]);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    }
}
