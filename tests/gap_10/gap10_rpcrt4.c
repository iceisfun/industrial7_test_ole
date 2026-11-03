/* gap10_rpcrt4: internal I_Rpc*, Ndr* helpers and data-table exports of rpcrt4.dll whose Win10 behaviour is
 * visible in the decompilation. */
#include "gap10.h"
#include <rpcdcep.h>
#include <ndrtypes.h>

static volatile LONG g_free_cnt;
static void *g_free_last;
static void __RPC_USER cnt_free(void *p) { g_free_cnt++; g_free_last = p; }
static void *__RPC_USER cnt_alloc(SIZE_T n) { return HeapAlloc(GetProcessHeap(), 0, n ? n : 1); }

/* ---------------------------------------------------------------- exported lookup tables */
/* SimpleTypeAlignment/BufferSize/MemorySize and NdrTypeFlags are exported DATA: GetProcAddress returns the
 * table itself (indexed by the FC_* format character). */
TEST(gap10_simpletype_alignment_table) {
    NOT_WINE("export is a stub or has a different signature");
    const unsigned char *t = (const unsigned char *)t_proc_in("rpcrt4.dll", "SimpleTypeAlignment");
    REQUIRE(t);
    /* stored as alignment-1: FC_BYTE..FC_USMALL 0, WCHAR/SHORT/USHORT 1, LONG/ULONG/FLOAT 3, HYPER/DOUBLE 7, ENUM16 1, ENUM32/IGNORE/ERROR_STATUS_T 3 */
    static const unsigned char want[17] = { 0, 0, 0, 0, 0, 1, 1, 1, 3, 3, 3, 7, 7, 1, 3, 3, 3 };
    for (int i = 0; i < 17; i++) CHECKF(t[i] == want[i], "SimpleTypeAlignment[%d] = %u want %u", i, t[i], want[i]);
    for (int i = 17; i < 64; i++) CHECKF(t[i] == 0, "SimpleTypeAlignment[%d] = %u want 0", i, t[i]);
}
TEST(gap10_simpletype_buffersize_table) {
    NOT_WINE("export is a stub or has a different signature");
    const unsigned char *t = (const unsigned char *)t_proc_in("rpcrt4.dll", "SimpleTypeBufferSize");
    REQUIRE(t);
    static const unsigned char want[17] = { 0, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 2, 4, 4, 4 };
    for (int i = 0; i < 17; i++) CHECKF(t[i] == want[i], "SimpleTypeBufferSize[%d] = %u want %u", i, t[i], want[i]);
    for (int i = 17; i < 64; i++) CHECKF(t[i] == 0, "SimpleTypeBufferSize[%d] = %u want 0", i, t[i]);
}
TEST(gap10_simpletype_memorysize_table) {
    NOT_WINE("export is a stub or has a different signature");
    const unsigned char *t = (const unsigned char *)t_proc_in("rpcrt4.dll", "SimpleTypeMemorySize");
    REQUIRE(t);
    /* differs from the wire size for FC_ENUM16 (4 in memory, 2 on the wire) and FC_IGNORE (pointer sized in memory) */
    static const unsigned char want[17] = { 0, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 4, 4, 8, 4 };
    for (int i = 0; i < 17; i++) CHECKF(t[i] == want[i], "SimpleTypeMemorySize[%d] = %u want %u", i, t[i], want[i]);
    for (int i = 17; i < 64; i++) CHECKF(t[i] == 0, "SimpleTypeMemorySize[%d] = %u want 0", i, t[i]);
}
TEST(gap10_ndrtypeflags_table) {
    NOT_WINE("export is a stub or has a different signature");
    const DWORD *t = (const DWORD *)t_proc_in("rpcrt4.dll", "NdrTypeFlags");
    REQUIRE(t);
    static const DWORD want[0x34] = {
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,                       /* 0x00-0x10 base types */
        0x202, 0x202, 0x202, 0x202,                                              /* 0x11-0x14 pointers */
        0x84, 0x84, 0x84, 0x84, 0x84, 0x84,                                      /* 0x15-0x1a structs */
        8, 8, 8, 8, 8, 8, 8,                                                     /* 0x1b-0x21 */
        0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,                          /* 0x22-0x29 arrays */
        0xa0, 0xa0, 0, 0xc0, 0xc0, 2, 0x100, 0x100, 0x100, 0 };                  /* 0x2a-0x33 */
    for (int i = 0; i < 0x34; i++) CHECKF(t[i] == want[i], "NdrTypeFlags[0x%x] = 0x%lx want 0x%lx", i, (unsigned long)t[i], (unsigned long)want[i]);
    for (int i = 0x34; i < 0x40; i++) CHECKF(t[i] == 0, "NdrTypeFlags[0x%x] = 0x%lx want 0", i, (unsigned long)t[i]);
}
TEST(gap10_ndrgetsimpletype_functions_index_the_tables) {
    NOT_WINE("export is a stub or has a different signature");
    typedef unsigned char (__cdecl *u8f)(unsigned char);
    typedef DWORD (__cdecl *u32f)(unsigned char);
    u8f al = (u8f)t_proc_in("rpcrt4.dll", "NdrGetSimpleTypeBufferAlignment"), bs = (u8f)t_proc_in("rpcrt4.dll", "NdrGetSimpleTypeBufferSize"),
        ms = (u8f)t_proc_in("rpcrt4.dll", "NdrGetSimpleTypeMemorySize");
    u32f fl = (u32f)t_proc_in("rpcrt4.dll", "NdrGetTypeFlags");
    const unsigned char *ta = (void *)t_proc_in("rpcrt4.dll", "SimpleTypeAlignment"), *tb = (void *)t_proc_in("rpcrt4.dll", "SimpleTypeBufferSize"),
        *tm = (void *)t_proc_in("rpcrt4.dll", "SimpleTypeMemorySize");
    const DWORD *tf = (void *)t_proc_in("rpcrt4.dll", "NdrTypeFlags");
    REQUIRE(al); REQUIRE(bs); REQUIRE(ms); REQUIRE(fl); REQUIRE(ta); REQUIRE(tb); REQUIRE(tm); REQUIRE(tf);
    for (int i = 0; i < 0x40; i++) {
        CHECKF((unsigned char)al((unsigned char)i) == ta[i], "alignment(%d)", i);
        CHECKF((unsigned char)bs((unsigned char)i) == tb[i], "bufsize(%d)", i);
        CHECKF((unsigned char)ms((unsigned char)i) == tm[i], "memsize(%d)", i);
        CHECKF(fl((unsigned char)i) == tf[i], "typeflags(%d)", i);
    }
    /* spot values by format character */
    EXPECT_EQ((unsigned char)bs(FC_LONG), 4);  EXPECT_EQ((unsigned char)bs(FC_HYPER), 8); EXPECT_EQ((unsigned char)bs(FC_ENUM16), 2);
    EXPECT_EQ((unsigned char)ms(FC_ENUM16), 4); EXPECT_EQ((unsigned char)al(FC_DOUBLE), 7); EXPECT_EQ((unsigned char)al(FC_WCHAR), 1);
    EXPECT_EQ((unsigned char)bs(FC_IGNORE), 4); EXPECT_EQ((unsigned char)ms(FC_IGNORE), 8);
}

/* ---------------------------------------------------------------- I_UuidCreate */
TEST(gap10_i_uuidcreate_is_sequential_uuid) {
    FN(RPC_STATUS, I_UuidCreate, (UUID *));
    UUID a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EXPECT_EQ(I_UuidCreate_(&a), RPC_S_OK);          /* RPC_S_UUID_LOCAL_ONLY from UuidCreateSequential is mapped to success */
    EXPECT_EQ(I_UuidCreate_(&b), RPC_S_OK);
    EXPECT_EQ(a.Data3 >> 12, 1);                      /* version 1 (time based) */
    EXPECT_EQ(b.Data3 >> 12, 1);
    PRESENT((a.Data4[0] & 0xC0) == 0x80 && (b.Data4[0] & 0xC0) == 0x80, "variant bits 0x%x 0x%x", a.Data4[0], b.Data4[0]);   /* RFC 4122 variant */
    CHECK(memcmp(&a, &b, sizeof a) != 0);
    CHECK(!memcmp(a.Data4 + 2, b.Data4 + 2, 6));      /* same node id for the sequence */
}

/* ---------------------------------------------------------------- stub group: 0x6e4 */
TEST(gap10_rpcrt4_cannot_support_stub_group) {
    static const char *const n[] = { "I_RpcBindingInqDynamicEndpointA", "I_RpcServerSubscribeForDisconnectNotification", "I_RpcTransDatagramAllocate",
        "I_RpcTransDatagramAllocate2", "I_RpcTransDatagramFree" };
    for (int i = 0; i < 5; i++) {
        long (WINAPI *f)(void *, void *, void *, void *) = (void *)t_proc_in("rpcrt4.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        CHECKF(f(NULL, NULL, NULL, NULL) == 0x6e4, "%s", n[i]);           /* 1764 RPC_S_CANNOT_SUPPORT */
        CHECKF(f((void *)1, (void *)2, (void *)3, (void *)4) == 0x6e4, "%s", n[i]);
    }
}
TEST(gap10_ndrns_getbuffer_sendreceive_raise) {
    NOT_WINE("NdrNs* stubs");
    static const char *const n[] = { "NdrNsGetBuffer", "NdrNsSendReceive" };
    for (int i = 0; i < 2; i++) {
        void (WINAPI *f)(void *, void *, void *) = (void *)t_proc_in("rpcrt4.dll", n[i]);
        if (!f) { t_skip("missing export %s", n[i]); return; }
        GTRY f(NULL, NULL, NULL); GEND;
        CHECKF(g_code == 0x6e4, "%s raised 0x%lx", n[i], (unsigned long)g_code);
    }
}

/* ---------------------------------------------------------------- I_RpcIfInqTransferSyntaxes */
TEST(gap10_i_rpcifinqtransfersyntaxes) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, I_RpcIfInqTransferSyntaxes, (RPC_IF_HANDLE, RPC_SYNTAX_IDENTIFIER *, UINT, UINT *));
    RPC_CLIENT_INTERFACE ci; memset(&ci, 0, sizeof ci);
    ci.Length = sizeof ci;
    static const GUID ndr = { 0x8a885d04, 0x1ceb, 0x11c9, { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } };
    ci.TransferSyntax.SyntaxGUID = ndr; ci.TransferSyntax.SyntaxVersion.MajorVersion = 2; ci.TransferSyntax.SyntaxVersion.MinorVersion = 0;
    RPC_SYNTAX_IDENTIFIER out[2]; memset(out, 0xCC, sizeof out);
    UINT cnt = 99;
    EXPECT_EQ(I_RpcIfInqTransferSyntaxes_(&ci, out, sizeof out, &cnt), RPC_S_OK);
    EXPECT_EQ(cnt, 1);
    CHECK(IsEqualGUID(&out[0].SyntaxGUID, &ndr));
    EXPECT_EQ(out[0].SyntaxVersion.MajorVersion, 2); EXPECT_EQ(out[0].SyntaxVersion.MinorVersion, 0);
    /* size 0 -> ERROR_INSUFFICIENT_BUFFER but the count is still reported */
    cnt = 99;
    EXPECT_EQ(I_RpcIfInqTransferSyntaxes_(&ci, out, 0, &cnt), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cnt, 1);
    cnt = 99;
    EXPECT_EQ(I_RpcIfInqTransferSyntaxes_(&ci, NULL, 0, &cnt), ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cnt, 1);
}

/* ---------------------------------------------------------------- towers */
#define L16(p, o) (*(WORD *)((BYTE *)(p) + (o)))
TEST(gap10_towerconstruct_ncalrpc_layout) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, TowerConstruct, (const RPC_SYNTAX_IDENTIFIER *, const RPC_SYNTAX_IDENTIFIER *, const WCHAR *, const char *, const char *, BYTE **));
    FN(void, I_RpcFree, (void *));
    static const RPC_SYNTAX_IDENTIFIER ifid = { { 0x12345678, 0x1234, 0x5678, { 1, 2, 3, 4, 5, 6, 7, 8 } }, { 3, 7 } };
    static const RPC_SYNTAX_IDENTIFIER xfer = { { 0x8a885d04, 0x1ceb, 0x11c9, { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } }, { 2, 0 } };
    BYTE *tw = NULL;
    EXPECT_EQ(TowerConstruct_(&ifid, &xfer, L"ncalrpc", "olet_gap10_ep", NULL, &tw), RPC_S_OK);
    REQUIRE(tw);
    DWORD len = *(DWORD *)tw;
    EXPECT_EQ(len, 0x3b + 19);                               /* floors 1-3 (0x37 bytes) + 4 + floor 4 (5 + strlen+1 = 19) */
    EXPECT_EQ(L16(tw, 4), 4);                                /* four floors */
    /* floor 1: interface id */
    EXPECT_EQ(L16(tw, 6), 0x13); EXPECT_EQ(tw[8], 0x0d);
    CHECK(!memcmp(tw + 9, &ifid.SyntaxGUID, 16));
    EXPECT_EQ(L16(tw, 0x19), 3); EXPECT_EQ(L16(tw, 0x1b), 2); EXPECT_EQ(L16(tw, 0x1d), 7);
    /* floor 2: transfer syntax */
    EXPECT_EQ(L16(tw, 0x1f), 0x13); EXPECT_EQ(tw[0x21], 0x0d);
    CHECK(!memcmp(tw + 0x22, &xfer.SyntaxGUID, 16));
    EXPECT_EQ(L16(tw, 0x32), 2); EXPECT_EQ(L16(tw, 0x34), 2); EXPECT_EQ(L16(tw, 0x36), 0);
    /* floor 3: RPC connection-oriented/local protocol id 0x0c (ncalrpc) with rhs {minor=0} */
    EXPECT_EQ(L16(tw, 0x38), 1); EXPECT_EQ(tw[0x3a], 0x0c); EXPECT_EQ(L16(tw, 0x3b), 2); EXPECT_EQ(L16(tw, 0x3d), 0);
    /* floor 4: endpoint */
    EXPECT_EQ(L16(tw, 0x3f), 1); EXPECT_EQ(tw[0x41], 0x10); EXPECT_EQ(L16(tw, 0x42), 14);
    CHECK(!memcmp(tw + 0x44, "olet_gap10_ep", 14));
    I_RpcFree_(tw);
}
TEST(gap10_towerconstruct_ncalrpc_empty_endpoint) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, TowerConstruct, (const RPC_SYNTAX_IDENTIFIER *, const RPC_SYNTAX_IDENTIFIER *, const WCHAR *, const char *, const char *, BYTE **));
    FN(void, I_RpcFree, (void *));
    static const RPC_SYNTAX_IDENTIFIER ifid = { { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } }, { 1, 0 } };
    BYTE *tw = NULL;
    EXPECT_EQ(TowerConstruct_(&ifid, &ifid, L"ncalrpc", NULL, NULL, &tw), RPC_S_OK);
    REQUIRE(tw);
    EXPECT_EQ(*(DWORD *)tw, 0x3b + 7);                       /* a missing endpoint is encoded as a 2 byte empty rhs */
    EXPECT_EQ(L16(tw, 0x3f), 1); EXPECT_EQ(tw[0x41], 0x10); EXPECT_EQ(L16(tw, 0x42), 2); EXPECT_EQ(L16(tw, 0x44), 0);
    I_RpcFree_(tw);
}
TEST(gap10_towerconstruct_bad_protseq) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, TowerConstruct, (const RPC_SYNTAX_IDENTIFIER *, const RPC_SYNTAX_IDENTIFIER *, const WCHAR *, const char *, const char *, BYTE **));
    static const RPC_SYNTAX_IDENTIFIER ifid = { { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } }, { 1, 0 } };
    BYTE *tw = (BYTE *)0x1;
    EXPECT_EQ(TowerConstruct_(&ifid, &ifid, L"foo", "ep", NULL, &tw), RPC_S_INVALID_RPC_PROTSEQ);   /* 1704 */
    EXPECT_EQ(TowerConstruct_(&ifid, &ifid, L"", "ep", NULL, &tw), RPC_S_INVALID_RPC_PROTSEQ);
    EXPECT_EQ(TowerConstruct_(&ifid, &ifid, L"ncacn", "ep", NULL, &tw), RPC_S_INVALID_RPC_PROTSEQ);   /* needs the "ncacn_" prefix */
    CHECK(tw == (BYTE *)0x1);                                                                           /* output untouched on failure */
}
TEST(gap10_tower_roundtrip) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, TowerConstruct, (const RPC_SYNTAX_IDENTIFIER *, const RPC_SYNTAX_IDENTIFIER *, const WCHAR *, const char *, const char *, BYTE **));
    FN(RPC_STATUS, TowerExplode, (const BYTE *, RPC_SYNTAX_IDENTIFIER *, RPC_SYNTAX_IDENTIFIER *, char **, char **, char **));
    FN(void, I_RpcFree, (void *));
    static const RPC_SYNTAX_IDENTIFIER ifid = { { 0xdeadbeef, 0x1111, 0x2222, { 9, 8, 7, 6, 5, 4, 3, 2 } }, { 5, 9 } };
    static const RPC_SYNTAX_IDENTIFIER xfer = { { 0x8a885d04, 0x1ceb, 0x11c9, { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } }, { 2, 0 } };
    struct { const WCHAR *ps; const char *ps_a; const char *ep; const char *na; } c[] = {
        { L"ncalrpc", "ncalrpc", "olet_gap10_rt", NULL },
        { L"ncacn_ip_tcp", "ncacn_ip_tcp", "135", "127.0.0.1" },
    };
    for (int i = 0; i < 2; i++) {
        BYTE *tw = NULL;
        RPC_STATUS rc = TowerConstruct_(&ifid, &xfer, c[i].ps, c[i].ep, c[i].na, &tw);
        CHECKF(rc == RPC_S_OK, "%s construct rc=%ld", c[i].ps_a, (long)rc);
        if (rc || !tw) continue;
        RPC_SYNTAX_IDENTIFIER i2, x2; char *ps = NULL, *ep = NULL, *na = NULL;
        memset(&i2, 0, sizeof i2); memset(&x2, 0, sizeof x2);
        rc = TowerExplode_(tw, &i2, &x2, &ps, &ep, &na);
        CHECKF(rc == RPC_S_OK, "%s explode rc=0x%lx", c[i].ps_a, (unsigned long)rc);
        if (rc == RPC_S_OK) {
            CHECK(!memcmp(&i2, &ifid, sizeof i2)); CHECK(!memcmp(&x2, &xfer, sizeof x2));
            CHECKF(ps && !strcmp(ps, c[i].ps_a), "%s protseq '%s'", c[i].ps_a, ps ? ps : "(null)");
            CHECKF(ep && !strcmp(ep, c[i].ep), "%s endpoint '%s'", c[i].ps_a, ep ? ep : "(null)");
            if (c[i].na) CHECKF(na && !strcmp(na, c[i].na), "%s netaddr '%s'", c[i].ps_a, na ? na : "(null)");
            else CHECK(na == NULL);
            if (ps) I_RpcFree_(ps); if (ep) I_RpcFree_(ep); if (na) I_RpcFree_(na);
        }
        I_RpcFree_(tw);
    }
}
TEST(gap10_towerexplode_invalid_tower) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, TowerConstruct, (const RPC_SYNTAX_IDENTIFIER *, const RPC_SYNTAX_IDENTIFIER *, const WCHAR *, const char *, const char *, BYTE **));
    FN(RPC_STATUS, TowerExplode, (const BYTE *, RPC_SYNTAX_IDENTIFIER *, RPC_SYNTAX_IDENTIFIER *, char **, char **, char **));
    FN(void, I_RpcFree, (void *));
    char *ps = NULL, *ep = NULL, *na = NULL;
    EXPECT_EQ(TowerExplode_(NULL, NULL, NULL, &ps, &ep, &na), 0x16c9a0cd);    /* EPT_S_NOT_REGISTERED-style invalid tower code */
    static const RPC_SYNTAX_IDENTIFIER ifid = { { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } }, { 1, 0 } };
    BYTE *tw = NULL;
    EXPECT_EQ(TowerConstruct_(&ifid, &ifid, L"ncalrpc", "ep", NULL, &tw), RPC_S_OK);
    REQUIRE(tw);
    DWORD len = *(DWORD *)tw;
    BYTE *cp = HeapAlloc(GetProcessHeap(), 0, len + 4 + 16);
    /* every truncation of the tower is rejected, never read past the declared length */
    int bad = 0;
    for (DWORD cut = 0; cut < len; cut++) {
        memset(cp, 0, len + 4 + 16);
        memcpy(cp, tw, len + 4);
        *(DWORD *)cp = cut;
        ps = ep = na = NULL;
        RPC_STATUS rc = TowerExplode_(cp, NULL, NULL, &ps, &ep, &na);
        if (rc == RPC_S_OK) bad++;
        if (rc == RPC_S_OK) { if (ps) I_RpcFree_(ps); if (ep) I_RpcFree_(ep); if (na) I_RpcFree_(na); }
    }
    CHECKF(bad == 0, "%d truncated towers were accepted", bad);
    /* floor count out of 3..6 */
    memcpy(cp, tw, len + 4);
    for (WORD fl = 0; fl < 8; fl++) {
        L16(cp, 4) = fl; ps = ep = na = NULL;
        RPC_STATUS rc = TowerExplode_(cp, NULL, NULL, &ps, &ep, &na);
        if (fl < 3 || fl > 6) CHECKF(rc == 0x16c9a0cd, "floor count %u rc 0x%lx", fl, (unsigned long)rc);
        if (rc == RPC_S_OK) { if (ps) I_RpcFree_(ps); if (ep) I_RpcFree_(ep); if (na) I_RpcFree_(na); }
    }
    HeapFree(GetProcessHeap(), 0, cp);
    I_RpcFree_(tw);
}

/* ---------------------------------------------------------------- misc process-state-free helpers */
TEST(gap10_i_rpcmgmt_dedicated_threadpool_default_off) {
    FN(unsigned char, I_RpcMgmtQueryDedicatedThreadPool, (void));
    EXPECT_EQ(I_RpcMgmtQueryDedicatedThreadPool_(), 0);
}
TEST(gap10_i_rpcverifiercorruptionexpected_default) {
    FN(int, I_RpcVerifierCorruptionExpected, (void));
    EXPECT_EQ(I_RpcVerifierCorruptionExpected_(), 0);
}
TEST(gap10_i_rpcenablewmitrace) {
    NOT_WINE("export is a stub or has a different signature");
    FN(RPC_STATUS, I_RpcEnableWmiTrace, (void *, void **));
    void *a = NULL, *b = NULL;
    EXPECT_EQ(I_RpcEnableWmiTrace_(NULL, &a), RPC_S_OK);
    EXPECT_EQ(I_RpcEnableWmiTrace_((void *)1, &b), RPC_S_OK);
    CHECK(a != NULL); CHECK(a == b);                          /* pointer to the process-wide trace state */
    HMODULE m = t_module("rpcrt4.dll");
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(a, &mbi, sizeof mbi) && mbi.AllocationBase == (void *)m);
}
TEST(gap10_i_rpcinitimports_fills_function_table) {
    FN(void, I_RpcInitImports, (void **));
    FN(void, I_RpcInitNdrImports, (void **));
    void *tbl[4] = { (void *)0x11, (void *)0x22, (void *)0x33, (void *)0x44 };
    I_RpcInitImports_(tbl);
    HMODULE m = t_module("rpcrt4.dll");
    for (int i = 0; i < 3; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        CHECKF(tbl[i] != NULL && VirtualQuery(tbl[i], &mbi, sizeof mbi) && mbi.AllocationBase == (void *)m, "helper[%d]=%p not inside rpcrt4", i, tbl[i]);
    }
    EXPECT_EQ(tbl[3], 0x44);                                   /* exactly three entries are written */
    void *nt = NULL;
    I_RpcInitNdrImports_(&nt);
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(nt != NULL && VirtualQuery(nt, &mbi, sizeof mbi) && mbi.AllocationBase == (void *)m);
}
TEST(gap10_i_rpcndrcgetwirerepresentation_copies_20_bytes) {
    FN(RPC_STATUS, I_RpcNDRCGetWireRepresentation, (const BYTE *, BYTE *));
    BYTE in[0x30], out[0x20];
    for (int i = 0; i < 0x30; i++) in[i] = (BYTE)(i + 1);
    memset(out, 0xEE, sizeof out);
    EXPECT_EQ(I_RpcNDRCGetWireRepresentation_(in, out), 0);
    CHECK(!memcmp(out, in + 0xc, 20));
    EXPECT_EQ(out[20], 0xEE);
}
TEST(gap10_i_rpcgetdefaultsd) {
    FN(void, I_RpcGetDefaultSD, (void **));
    void *sd = NULL;
    I_RpcGetDefaultSD_(&sd);
    CHECK(sd != NULL);
    if (sd) {
        CHECK(IsValidSecurityDescriptor(sd));
        CHECK(GetSecurityDescriptorLength(sd) > 20);
    }
}
TEST(gap10_i_rpcserverisclientdisconnected_no_call) {
    NOT_WINE("I_RpcServerIsClientDisconnected");
    FN(RPC_STATUS, I_RpcServerIsClientDisconnected, (void *));
    EXPECT_EQ(I_RpcServerIsClientDisconnected_(NULL), 1725);         /* RPC_S_NO_CALL_ACTIVE: no RPC call on this thread */
}
TEST(gap10_i_rpcsessionstrictcontexthandle_outside_call) {
    NOT_WINE("I_RpcSessionStrictContextHandle");
    FN(void, I_RpcSessionStrictContextHandle, (void));
    I_RpcSessionStrictContextHandle_();       /* no active call: silently does nothing */
    CHECK(1);
}
TEST(gap10_i_rpcgetsystemhandle_null_out) {
    NOT_WINE("I_RpcGetSystemHandle");
    FN(DWORD, I_RpcGetSystemHandle, (void *, BYTE, DWORD, DWORD, void *));
    EXPECT_EQ(I_RpcGetSystemHandle_(NULL, 0, 0, 0, NULL), 0x80010010);
}
TEST(gap10_i_rpcbindingsetprivateoption_null_binding) {
    NOT_WINE("I_RpcBindingSetPrivateOption");
    FN(RPC_STATUS, I_RpcBindingSetPrivateOption, (void *, ULONG, ULONG_PTR));
    EXPECT_EQ(I_RpcBindingSetPrivateOption_(NULL, 1, 1), ERROR_INVALID_PARAMETER);
}

/* ---------------------------------------------------------------- Ndr helpers with a hand-made stub message */
static void sm_init(MIDL_STUB_MESSAGE *sm, MIDL_STUB_DESC *d, RPC_MESSAGE *rm, BYTE *buf, size_t cap) {
    memset(sm, 0, sizeof *sm); memset(d, 0, sizeof *d); memset(rm, 0, sizeof *rm);
    d->pfnAllocate = cnt_alloc; d->pfnFree = cnt_free; d->Version = 0x20000; d->MIDLVersion = 0x50100a4;
    sm->pfnAllocate = cnt_alloc; sm->pfnFree = cnt_free; sm->StubDesc = d; sm->RpcMsg = rm;
    sm->Buffer = sm->BufferStart = buf; sm->BufferEnd = buf + cap; sm->BufferLength = (ULONG)cap;
    rm->Buffer = buf; rm->BufferLength = (ULONG)cap; rm->DataRepresentation = 0x10;
}
TEST(gap10_ndrbytecountpointerfree_frees_only_outside_buffer) {
    NOT_WINE("NdrByteCountPointerFree");
    FNIN("rpcrt4.dll", void, NdrByteCountPointerFree, (PMIDL_STUB_MESSAGE, unsigned char *, PFORMAT_STRING));
    MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC d; RPC_MESSAGE rm; BYTE buf[64];
    sm_init(&sm, &d, &rm, buf, sizeof buf);
    static const unsigned char fmt[] = { 0x1d, 0, 0, 0 };
    g_free_cnt = 0;
    NdrByteCountPointerFree_(&sm, NULL, fmt);
    EXPECT_EQ(g_free_cnt, 0);
    NdrByteCountPointerFree_(&sm, buf + 8, fmt);               /* inside the RPC buffer: not owned by the stub */
    EXPECT_EQ(g_free_cnt, 0);
    NdrByteCountPointerFree_(&sm, buf, fmt);                   /* BufferStart itself is inside */
    EXPECT_EQ(g_free_cnt, 0);
    NdrByteCountPointerFree_(&sm, buf + 64, fmt);              /* BufferEnd is inclusive */
    EXPECT_EQ(g_free_cnt, 0);
    BYTE *heap = HeapAlloc(GetProcessHeap(), 0, 16);
    NdrByteCountPointerFree_(&sm, heap, fmt);
    EXPECT_EQ(g_free_cnt, 1);
    CHECK(g_free_last == heap);
    HeapFree(GetProcessHeap(), 0, heap);
}
TEST(gap10_ndrsimplestructfree_without_pointers_is_noop) {
    NOT_WINE("NdrSimpleStructFree");
    FNIN("rpcrt4.dll", void, NdrSimpleStructFree, (PMIDL_STUB_MESSAGE, unsigned char *, PFORMAT_STRING));
    MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC d; RPC_MESSAGE rm; BYTE buf[16];
    sm_init(&sm, &d, &rm, buf, sizeof buf);
    static const unsigned char fmt[] = { FC_STRUCT, 3, 8, 0, FC_LONG, FC_LONG, FC_END };   /* FC_STRUCT, not FC_PSTRUCT */
    int mem[2] = { 1, 2 };
    g_free_cnt = 0;
    NdrSimpleStructFree_(&sm, (unsigned char *)mem, fmt);
    EXPECT_EQ(g_free_cnt, 0);
    EXPECT_EQ(mem[0], 1); EXPECT_EQ(mem[1], 2);
}
TEST(gap10_ndrpartialignoreclientmarshall_writes_presence_dword) {
    NOT_WINE("NdrPartialIgnoreClientMarshall");
    FNIN("rpcrt4.dll", void, NdrPartialIgnoreClientMarshall, (PMIDL_STUB_MESSAGE, void *, PFORMAT_STRING));
    MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC d; RPC_MESSAGE rm; BYTE raw[40];
    BYTE *buf = (BYTE *)(((ULONG_PTR)raw + 7) & ~7ull);
    static const unsigned char fmt[] = { 0 };
    int dummy;
    for (int off = 0; off < 4; off++) {
        for (int nn = 0; nn < 2; nn++) {
            memset(raw, 0xCD, sizeof raw);
            sm_init(&sm, &d, &rm, buf, 24);
            sm.Buffer = buf + off;
            NdrPartialIgnoreClientMarshall_(&sm, nn ? &dummy : NULL, fmt);
            BYTE *aligned = (BYTE *)(((ULONG_PTR)(buf + off) + 3) & ~3ull);
            CHECKF(sm.Buffer == aligned + 4, "off %d: Buffer advanced by %d", off, (int)(sm.Buffer - (buf + off)));
            EXPECT_EQ(*(DWORD *)aligned, nn ? 1 : 0);                    /* referent-present flag, not the pointer */
        }
    }
}
TEST(gap10_ndrusermarshalsimpletypeconvert_range_and_rep_checks) {
    NOT_WINE("NdrUserMarshalSimpleTypeConvert");
    FNIN("rpcrt4.dll", unsigned char *, NdrUserMarshalSimpleTypeConvert, (ULONG *, unsigned char *, unsigned char));
    MIDL_STUB_MESSAGE sm; MIDL_STUB_DESC d; RPC_MESSAGE rm; BYTE buf[32] __attribute__((aligned(8))); memset(buf, 0, sizeof buf);
    sm_init(&sm, &d, &rm, buf, sizeof buf);
    /* USER_MARSHAL_CB-style flags block: DWORD flags (low 16 bits context, high 16 bits data representation) then the stub message at +8 */
    struct { ULONG flags; ULONG pad; MIDL_STUB_MESSAGE *sm; } fl;
    fl.flags = (0x10u << 16); fl.pad = 0; fl.sm = &sm;
    BYTE *save = sm.Buffer;
    unsigned char *r = NdrUserMarshalSimpleTypeConvert_(&fl.flags, buf + 4, FC_LONG);
    CHECK(r == buf + 8);                                       /* consumed one 4-byte value */
    CHECK(sm.Buffer == save);                                  /* stub message position restored */
    r = NdrUserMarshalSimpleTypeConvert_(&fl.flags, buf + 8, FC_SHORT);
    CHECK(r == buf + 10);
    /* buffer pointer outside [BufferStart, BufferEnd] -> RPC_S_INVALID_ARG-style exception 0x57 */
    GTRY NdrUserMarshalSimpleTypeConvert_(&fl.flags, buf + 33, FC_LONG); GEND;
    EXPECT_EQ(g_code, 0x57);
    GTRY NdrUserMarshalSimpleTypeConvert_(&fl.flags, buf - 1, FC_LONG); GEND;
    EXPECT_EQ(g_code, 0x57);
    /* data representation in the flags must match the message's */
    fl.flags = (0x00u << 16);
    GTRY NdrUserMarshalSimpleTypeConvert_(&fl.flags, buf + 4, FC_LONG); GEND;
    EXPECT_EQ(g_code, 0x57);
}
TEST(gap10_i_rpcssdontserializecontext_runs) {
    NOT_WINE("I_RpcSsDontSerializeContext");
    FN(void, I_RpcSsDontSerializeContext, (void));
    I_RpcSsDontSerializeContext_();
    CHECK(1);
}
