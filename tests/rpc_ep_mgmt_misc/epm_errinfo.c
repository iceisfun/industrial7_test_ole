/* RPC extended error information: the per-thread record chain (RpcErrorAddRecord), its enumeration handle
 * (signature 0xFCFCFCFC while active, 0xFDFDFDFD once ended), save/load blobs. Statuses and field checks follow
 * the Windows 10 22H2 rpcrt4 decompilation. */
#include "epx.h"

#define SIG_ACTIVE 0xFCFCFCFCu
#define SIG_ENDED  0xFDFDFDFDu

static void mkrec(RPC_EXTENDED_ERROR_INFO *r, ULONG comp, ULONG status) {
    memset(r, 0, sizeof *r);
    r->Version = RPC_EEINFO_VERSION;
    r->GeneratingComponent = comp;
    r->Status = status;
    r->DetectionLocation = 0x42;
    r->NumberOfParameters = 0;
}
static void clear_info(void) { U(RpcErrorClearInformation)(); }

#define ERR_NEED() do { REQUIRE(U(RpcErrorStartEnumeration) && U(RpcErrorGetNextRecord) && U(RpcErrorEndEnumeration) && U(RpcErrorClearInformation) && U(RpcErrorAddRecord) && U(RpcErrorGetNumberOfRecords) && U(RpcErrorResetEnumeration)); WINE_STUB(); } while (0)

TEST(epm_errinfo_start_without_info_is_entry_not_found) {
    RPC_ERROR_ENUM_HANDLE e; ERR_NEED();
    clear_info();
    memset(&e, 0x11, sizeof e);
    EQ(U(RpcErrorStartEnumeration)(&e), EPX_ENTRY_NOT_FOUND);
    EQ(e.Signature, 0x11111111);          /* untouched on failure */
}

TEST(epm_errinfo_clear_is_idempotent) {
    RPC_ERROR_ENUM_HANDLE e; ERR_NEED();
    clear_info(); clear_info(); clear_info();
    EQ(U(RpcErrorStartEnumeration)(&e), EPX_ENTRY_NOT_FOUND);
}

TEST(epm_errinfo_invalid_enum_handles) {
    RPC_ERROR_ENUM_HANDLE e; RPC_EXTENDED_ERROR_INFO rec; int n = 0x55; PVOID blob = NULL; size_t sz = 0x55;
    ERR_NEED();
    REQUIRE(U(RpcErrorSaveErrorInfo));
    memset(&e, 0, sizeof e);
    memset(&rec, 0, sizeof rec);
    EQ(U(RpcErrorGetNextRecord)(&e, TRUE, &rec), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorEndEnumeration)(&e), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorResetEnumeration)(&e), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorGetNumberOfRecords)(&e, &n), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorSaveErrorInfo)(&e, &blob, &sz), RPC_S_INVALID_ARG);
    EQ(n, 0x55); CHECK(blob == NULL); EQ(sz, 0x55);
    e.Signature = SIG_ENDED;              /* an ended enumeration is as invalid as a zeroed one */
    EQ(U(RpcErrorGetNextRecord)(&e, TRUE, &rec), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorEndEnumeration)(&e), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorResetEnumeration)(&e), RPC_S_INVALID_ARG);
    EQ(U(RpcErrorGetNumberOfRecords)(&e, &n), RPC_S_INVALID_ARG);
}

TEST(epm_errinfo_add_record_validation) {
    RPC_EXTENDED_ERROR_INFO r; RPC_ERROR_ENUM_HANDLE e; unsigned i; ERR_NEED();
    clear_info();
    mkrec(&r, 0, 1); r.Version = 0;                      EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.Version = 2;                      EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.ComputerName = L"x";              EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.ProcessID = 4;                    EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.Flags = 1;                        EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.NumberOfParameters = 5;           EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0, 1); r.NumberOfParameters = -1;          EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    /* GeneratingComponent: 0, 11..19 and values >= 0x80000000 are accepted by the validator */
    for (i = 1; i <= 10; i++) { mkrec(&r, i, 1); EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG); }
    for (i = 20; i < 25; i++) { mkrec(&r, i, 1); EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG); }
    mkrec(&r, 0x7FFFFFFFu, 1);                           EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    mkrec(&r, 0x40000000u, 1);                           EQ(U(RpcErrorAddRecord)(&r), RPC_S_INVALID_ARG);
    /* none of the rejected records left anything behind */
    EQ(U(RpcErrorStartEnumeration)(&e), EPX_ENTRY_NOT_FOUND);
}

TEST(epm_errinfo_add_record_accepts_valid_components) {
    static const ULONG comp[] = { 0, 11, 12, 15, 19, 0x80000000u, 0x80000010u, 0xFFFFFFFFu };
    unsigned i; ERR_NEED();
    for (i = 0; i < sizeof comp / sizeof comp[0]; i++) {
        RPC_EXTENDED_ERROR_INFO r; int n = 0; RPC_ERROR_ENUM_HANDLE e;
        clear_info();
        mkrec(&r, comp[i], 0x1000 + i);
        EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
        EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
        EQ(U(RpcErrorGetNumberOfRecords)(&e, &n), RPC_S_OK);
        CHECKF(n >= 1, "component 0x%lx: %d records", (unsigned long)comp[i], n);
        U(RpcErrorEndEnumeration)(&e);
    }
    clear_info();
}

TEST(epm_errinfo_add_enumerate_roundtrip) {
    RPC_EXTENDED_ERROR_INFO r, out; RPC_ERROR_ENUM_HANDLE e; int n = -1; RPC_STATUS st;
    ERR_NEED();
    clear_info();
    mkrec(&r, 12, 0x1234);
    r.NumberOfParameters = 1;
    r.Parameters[0].ParameterType = eeptLongVal;
    r.Parameters[0].u.LVal = 777;
    EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    EQ(e.Signature, SIG_ACTIVE);
    EQ(U(RpcErrorGetNumberOfRecords)(&e, &n), RPC_S_OK);
    if (n != 1) t_note("record count after one RpcErrorAddRecord = %d", n);
    CHECK(n >= 1);
    memset(&out, 0, sizeof out);
    out.Version = RPC_EEINFO_VERSION;
    out.NumberOfParameters = MaxNumberOfEEInfoParams;
    st = U(RpcErrorGetNextRecord)(&e, TRUE, &out);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        EQ(out.Version, RPC_EEINFO_VERSION);
        EQ(out.Status, 0x1234);
        EQ(out.GeneratingComponent, 12);
        EQ(out.DetectionLocation, 0x42);
        EQ(out.NumberOfParameters, 1);
        EQ(out.Parameters[0].ParameterType, eeptLongVal);
        EQ(out.Parameters[0].u.LVal, 777);
        t_note("record: pid=%lu computer=%ls flags=0x%x", (unsigned long)out.ProcessID, out.ComputerName ? out.ComputerName : L"(null)", out.Flags);
        if (out.ProcessID != GetCurrentProcessId()) t_note("record ProcessID %lu differs from current pid", (unsigned long)out.ProcessID);
    }
    if (n == 1) EQ(U(RpcErrorGetNextRecord)(&e, TRUE, &out), EPX_ENTRY_NOT_FOUND);     /* chain exhausted */
    EQ(U(RpcErrorEndEnumeration)(&e), RPC_S_OK);
    EQ(e.Signature, SIG_ENDED);
    EQ(U(RpcErrorEndEnumeration)(&e), RPC_S_INVALID_ARG);                              /* ending twice */
    clear_info();
}

TEST(epm_errinfo_string_parameters_roundtrip) {
    RPC_EXTENDED_ERROR_INFO r, out; RPC_ERROR_ENUM_HANDLE e; RPC_STATUS st;
    ERR_NEED();
    clear_info();
    mkrec(&r, 11, 0x2222);
    r.NumberOfParameters = 3;
    r.Parameters[0].ParameterType = eeptAnsiString;    r.Parameters[0].u.AnsiString = "olet-ansi";
    r.Parameters[1].ParameterType = eeptUnicodeString; r.Parameters[1].u.UnicodeString = L"olet-wide";
    r.Parameters[2].ParameterType = eeptShortVal;      r.Parameters[2].u.SVal = -5;
    st = U(RpcErrorAddRecord)(&r);
    EQ(st, RPC_S_OK);
    if (st != RPC_S_OK) return;
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
    st = U(RpcErrorGetNextRecord)(&e, TRUE, &out);
    EQ(st, RPC_S_OK);
    if (st == RPC_S_OK) {
        EQ(out.NumberOfParameters, 3);
        EQ(out.Parameters[0].ParameterType, eeptAnsiString);
        EQ(out.Parameters[1].ParameterType, eeptUnicodeString);
        EQ(out.Parameters[2].ParameterType, eeptShortVal);
        if (out.Parameters[0].u.AnsiString) EXPECT_STR(out.Parameters[0].u.AnsiString, "olet-ansi"); else CHECK(0);
        if (out.Parameters[1].u.UnicodeString) EXPECT_WSTR(out.Parameters[1].u.UnicodeString, L"olet-wide"); else CHECK(0);
        CHECK(out.Parameters[2].u.SVal == -5);
    }
    U(RpcErrorEndEnumeration)(&e);
    clear_info();
}

TEST(epm_errinfo_reset_enumeration_rewinds) {
    RPC_EXTENDED_ERROR_INFO r, out; RPC_ERROR_ENUM_HANDLE e; int n = 0, got1 = 0, got2 = 0;
    ERR_NEED();
    clear_info();
    mkrec(&r, 12, 0x31); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    mkrec(&r, 12, 0x32); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    EQ(U(RpcErrorGetNumberOfRecords)(&e, &n), RPC_S_OK);
    t_note("records after two adds: %d", n);
    CHECK(n >= 2);
    for (;;) {
        memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
        if (U(RpcErrorGetNextRecord)(&e, TRUE, &out) != RPC_S_OK) break;
        got1++;
        if (got1 > 100) break;
    }
    EQ(got1, n);
    EQ(U(RpcErrorResetEnumeration)(&e), RPC_S_OK);
    for (;;) {
        memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
        if (U(RpcErrorGetNextRecord)(&e, TRUE, &out) != RPC_S_OK) break;
        got2++;
        if (got2 > 100) break;
    }
    EQ(got2, n);
    {   /* the enumeration owns a private copy: clearing the thread's chain does not change the count */
        int n2 = -1;
        clear_info();
        EQ(U(RpcErrorGetNumberOfRecords)(&e, &n2), RPC_S_OK);
        EQ(n2, n);
    }
    U(RpcErrorEndEnumeration)(&e);
    EQ(U(RpcErrorStartEnumeration)(&e), EPX_ENTRY_NOT_FOUND);
}

TEST(epm_errinfo_two_enumerations_are_independent) {
    RPC_EXTENDED_ERROR_INFO r, out; RPC_ERROR_ENUM_HANDLE a, b; int na = 0, nb = 0;
    ERR_NEED();
    clear_info();
    mkrec(&r, 12, 0x41); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&a), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&b), RPC_S_OK);
    memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
    EQ(U(RpcErrorGetNextRecord)(&a, TRUE, &out), RPC_S_OK);
    /* b has not moved */
    memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
    EQ(U(RpcErrorGetNextRecord)(&b, TRUE, &out), RPC_S_OK);
    EQ(out.Status, 0x41);
    U(RpcErrorGetNumberOfRecords)(&a, &na); U(RpcErrorGetNumberOfRecords)(&b, &nb);
    EQ(na, nb);
    EQ(U(RpcErrorEndEnumeration)(&a), RPC_S_OK);
    EQ(U(RpcErrorEndEnumeration)(&b), RPC_S_OK);
    clear_info();
}

static struct { int rc; } g_thr;
static void other_thread(void *p) {
    RPC_ERROR_ENUM_HANDLE e;
    g_thr.rc = (int)U(RpcErrorStartEnumeration)(&e);
}
TEST(epm_errinfo_chain_is_per_thread) {
    RPC_EXTENDED_ERROR_INFO r; ERR_NEED();
    clear_info();
    mkrec(&r, 12, 0x51); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    g_thr.rc = -1;
    CHECK(epx_run(other_thread, NULL, 10000));
    EQ(g_thr.rc, EPX_ENTRY_NOT_FOUND);
    clear_info();
}

TEST(epm_errinfo_save_load_roundtrip) {
    RPC_EXTENDED_ERROR_INFO r, out; RPC_ERROR_ENUM_HANDLE e, l; PVOID blob = NULL; size_t sz = 0; int n = 0, nl = 0; RPC_STATUS st;
    ERR_NEED();
    REQUIRE(U(RpcErrorSaveErrorInfo) && U(RpcErrorLoadErrorInfo));
    clear_info();
    mkrec(&r, 12, 0x61);
    r.NumberOfParameters = 1; r.Parameters[0].ParameterType = eeptLongVal; r.Parameters[0].u.LVal = 4242;
    EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    U(RpcErrorGetNumberOfRecords)(&e, &n);
    st = U(RpcErrorSaveErrorInfo)(&e, &blob, &sz);
    EQ(st, RPC_S_OK);
    CHECK(blob != NULL);
    CHECK(sz > 0 && sz < 65536);
    t_note("saved error blob: %lu bytes for %d record(s)", (unsigned long)sz, n);
    if (st == RPC_S_OK && blob) {
        st = U(RpcErrorLoadErrorInfo)(blob, sz, &l);
        EQ(st, RPC_S_OK);
        if (st == RPC_S_OK) {
            EQ(l.Signature, SIG_ACTIVE);
            EQ(U(RpcErrorGetNumberOfRecords)(&l, &nl), RPC_S_OK);
            EQ(nl, n);
            memset(&out, 0, sizeof out); out.Version = 1; out.NumberOfParameters = 4;
            EQ(U(RpcErrorGetNextRecord)(&l, TRUE, &out), RPC_S_OK);
            EQ(out.Status, 0x61);
            EQ(out.GeneratingComponent, 12);
            EQ(out.NumberOfParameters, 1);
            EQ(out.Parameters[0].u.LVal, 4242);
            EQ(U(RpcErrorEndEnumeration)(&l), RPC_S_OK);
        }
        HeapFree(GetProcessHeap(), 0, blob);          /* the blob is a process-heap allocation */
    }
    U(RpcErrorEndEnumeration)(&e);
    clear_info();
}

TEST(epm_errinfo_save_blob_is_stable_for_same_chain) {
    RPC_EXTENDED_ERROR_INFO r; RPC_ERROR_ENUM_HANDLE e; PVOID b1 = NULL, b2 = NULL; size_t s1 = 0, s2 = 0;
    ERR_NEED();
    REQUIRE(U(RpcErrorSaveErrorInfo));
    clear_info();
    mkrec(&r, 12, 0x71); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    EQ(U(RpcErrorSaveErrorInfo)(&e, &b1, &s1), RPC_S_OK);
    EQ(U(RpcErrorSaveErrorInfo)(&e, &b2, &s2), RPC_S_OK);
    EQ(s1, s2);
    CHECK(b1 != b2);                         /* each save allocates a fresh blob */
    if (b1 && b2 && s1 == s2 && memcmp(b1, b2, s1)) t_note("two saves of the same chain differ byte-wise");
    if (b1) HeapFree(GetProcessHeap(), 0, b1);
    if (b2) HeapFree(GetProcessHeap(), 0, b2);
    U(RpcErrorEndEnumeration)(&e);
    clear_info();
}

TEST(epm_errinfo_load_rejects_truncated_blob) {
    RPC_EXTENDED_ERROR_INFO r; RPC_ERROR_ENUM_HANDLE e, l; PVOID blob = NULL; size_t sz = 0; RPC_STATUS st;
    ERR_NEED();
    REQUIRE(U(RpcErrorSaveErrorInfo) && U(RpcErrorLoadErrorInfo));
    clear_info();
    mkrec(&r, 12, 0x81); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&e), RPC_S_OK);
    EQ(U(RpcErrorSaveErrorInfo)(&e, &blob, &sz), RPC_S_OK);
    if (blob && sz > 16) {
        memset(&l, 0x22, sizeof l);
        st = U(RpcErrorLoadErrorInfo)(blob, 8, &l);          /* header only: cannot be a complete record */
        t_note("RpcErrorLoadErrorInfo(truncated to 8 bytes) = %lu", (unsigned long)st);
        CHECK(st != RPC_S_OK);
        if (st != RPC_S_OK) EQ(l.Signature, 0x22222222);
        else U(RpcErrorEndEnumeration)(&l);
    }
    if (blob) HeapFree(GetProcessHeap(), 0, blob);
    U(RpcErrorEndEnumeration)(&e);
    clear_info();
}

TEST(epm_errinfo_records_survive_end_of_other_enumeration) {
    RPC_EXTENDED_ERROR_INFO r; RPC_ERROR_ENUM_HANDLE a, b; ERR_NEED();
    clear_info();
    mkrec(&r, 12, 0x91); EQ(U(RpcErrorAddRecord)(&r), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&a), RPC_S_OK);
    EQ(U(RpcErrorEndEnumeration)(&a), RPC_S_OK);
    EQ(U(RpcErrorStartEnumeration)(&b), RPC_S_OK);      /* ending an enumeration does not clear the thread's chain */
    EQ(U(RpcErrorEndEnumeration)(&b), RPC_S_OK);
    clear_info();
    EQ(U(RpcErrorStartEnumeration)(&a), EPX_ENTRY_NOT_FOUND);
}
