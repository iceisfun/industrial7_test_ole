/* rpcrt4 UUID primitives: UuidCreate*, UuidIsNil, UuidEqual, UuidCompare, UuidHash.
 * Expectations come from the Windows 10 19041 rpcrt4 decompilation (see comments per test). */
#include "rus.h"

/* the table Wine uses for its equality grid, kept here as plain data */
static const UUID g_tbl[10] = {
    { 0x00000000, 0x0000, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x00000001, 0x0000, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x00000000, 0x0001, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x00000000, 0x0000, 0x0001, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x00000000, 0x0000, 0x0000, {0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00} },
    { 0x00000000, 0x0000, 0x0000, {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01} },
    { 0x12312312, 0x1231, 0x1231, {0x12,0x34,0x56,0x78,0x9a,0xbc,0xde,0xff} },
    { 0x11111111, 0x1111, 0x1111, {0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11} },
    { 0xffffffff, 0xffff, 0xffff, {0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff} },
    { 0x8a885d04, 0x1ceb, 0x11c9, {0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60} },
};

static unsigned long long rus_v1_stamp(const UUID *u)
{
    return ((unsigned long long)(u->Data3 & 0x0fff) << 48) | ((unsigned long long)u->Data2 << 32) | u->Data1;
}

/* ---------------------------------------------------------------- UuidCreate */

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+) */
TEST(rus_uuidcreate_version4_variant)
{
    NEED(UuidCreate);
    UUID u;
    memset(&u, 0xcc, sizeof(u));
    RPC_STATUS st = p_UuidCreate(&u);
    EXPECT_EQ(st, RPC_S_OK);
    /* decomp: Data3 = (Data3 & 0xfff) | 0x4000 ; Data4[0] = (Data4[0] & 0x3f) | 0x80 */
    EXPECT_EQ(u.Data3 >> 12, 4);
    EXPECT_EQ(u.Data4[0] >> 6, 2);
}

TEST(rus_uuidcreate_masks_over_1000)
{
    NEED(UuidCreate);
    /* every randomly generated bit must have been both 0 and 1 at least once in 1000 draws, the fixed bits never */
    unsigned char andm[16], orm[16];
    memset(andm, 0xff, 16); memset(orm, 0, 16);
    for (int i = 0; i < 1000; i++) {
        UUID u; unsigned char *b = (unsigned char *)&u;
        EXPECT_EQ(p_UuidCreate(&u), RPC_S_OK);
        for (int k = 0; k < 16; k++) { andm[k] &= b[k]; orm[k] |= b[k]; }
    }
    UUID a, o; memcpy(&a, andm, 16); memcpy(&o, orm, 16);
    UUID wa = { 0, 0, 0x4000, {0x80,0,0,0,0,0,0,0} };
    UUID wo = { 0xffffffff, 0xffff, 0x4fff, {0xbf,0xff,0xff,0xff,0xff,0xff,0xff,0xff} };
    CHECK(rus_uuid_eq(&a, &wa));
    CHECK(rus_uuid_eq(&o, &wo));
}

TEST(rus_uuidcreate_unique_1000)
{
    NEED(UuidCreate);
    static UUID v[1000];
    for (int i = 0; i < 1000; i++) EXPECT_EQ(p_UuidCreate(&v[i]), RPC_S_OK);
    int dup = 0;
    for (int i = 0; i < 1000; i++)
        for (int j = i + 1; j < 1000; j++)
            if (rus_uuid_eq(&v[i], &v[j])) dup++;
    EXPECT_EQ(dup, 0);
}

TEST(rus_uuidcreate_not_nil)
{
    NEED(UuidCreate); NEED(UuidIsNil);
    for (int i = 0; i < 50; i++) {
        UUID u; RPC_STATUS s = 77;
        EXPECT_EQ(p_UuidCreate(&u), RPC_S_OK);
        EXPECT_EQ(p_UuidIsNil(&u, &s), 0);
        EXPECT_EQ(s, RPC_S_OK);
    }
}

TEST(rus_uuidcreate_overwrites_dirty_buffer)
{
    NEED(UuidCreate);
    UUID a, b;
    memset(&a, 0, sizeof a); memset(&b, 0xff, sizeof b);
    EXPECT_EQ(p_UuidCreate(&a), RPC_S_OK);
    EXPECT_EQ(p_UuidCreate(&b), RPC_S_OK);
    CHECK(!rus_uuid_eq(&a, &b));
    /* all 16 bytes are written: a buffer of 0xff cannot survive both the and-mask 0x3f and a random fill */
    EXPECT_EQ(b.Data4[0] >> 6, 2);
}

TEST(rus_uuidcreate_per_thread_unique)
{
    NEED(UuidCreate);
    /* four consecutive calls from this thread are all different */
    UUID u[4];
    for (int i = 0; i < 4; i++) EXPECT_EQ(p_UuidCreate(&u[i]), RPC_S_OK);
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 4; j++)
            CHECK(!rus_uuid_eq(&u[i], &u[j]));
}

/* ------------------------------------------------------ UuidCreateSequential */

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+) */
TEST(rus_uuidseq_version1)
{
    NEED(UuidCreateSequential);
    UUID u;
    RPC_STATUS st = p_UuidCreateSequential(&u);
    CHECKF(st == RPC_S_OK || st == RPC_S_UUID_LOCAL_ONLY, "status %lu", (unsigned long)st);
    /* decomp: Data3 = ((ts >> 48) & 0xfff) | 0x1000 */
    EXPECT_EQ(u.Data3 >> 12, 1);
    if (st == RPC_S_UUID_LOCAL_ONLY) t_note("UuidCreateSequential returned RPC_S_UUID_LOCAL_ONLY (no hardware address)");
    t_note("sequential uuid Data4[0] variant bits: 0x%02x", u.Data4[0] >> 6);
}

TEST(rus_uuidseq_node_and_status_stable)
{
    NEED(UuidCreateSequential);
    UUID first, u;
    RPC_STATUS s0 = p_UuidCreateSequential(&first);
    CHECK(s0 == RPC_S_OK || s0 == RPC_S_UUID_LOCAL_ONLY);
    /* Data4 (clock sequence + node) comes from a process wide cache: identical on every call */
    for (int i = 0; i < 200; i++) {
        RPC_STATUS s = p_UuidCreateSequential(&u);
        EXPECT_EQ(s, s0);
        CHECK(memcmp(u.Data4, first.Data4, 8) == 0);
        EXPECT_EQ(u.Data3 >> 12, 1);
    }
    if (s0 == RPC_S_OK) t_note("node multicast bit (Data4[2]&1) = %d", first.Data4[2] & 1);
    else t_note("local-only node, Data4[2]&1 = %d", first.Data4[2] & 1);
}

TEST(rus_uuidseq_timestamp_strictly_increasing)
{
    NEED(UuidCreateSequential);
    UUID u;
    p_UuidCreateSequential(&u);
    unsigned long long prev = rus_v1_stamp(&u);
    int bad = 0;
    for (int i = 0; i < 5000; i++) {
        p_UuidCreateSequential(&u);
        unsigned long long cur = rus_v1_stamp(&u);
        if (cur <= prev) bad++;
        prev = cur;
    }
    EXPECT_EQ(bad, 0);
}

TEST(rus_uuidseq_unique_5000)
{
    NEED(UuidCreateSequential);
    static UUID v[5000];
    for (int i = 0; i < 5000; i++) p_UuidCreateSequential(&v[i]);
    int dup = 0;
    /* strictly increasing timestamp already implies uniqueness; verify pairwise on neighbours and a stride */
    for (int i = 1; i < 5000; i++) if (rus_uuid_eq(&v[i], &v[i - 1])) dup++;
    for (int i = 0; i + 97 < 5000; i++) if (rus_uuid_eq(&v[i], &v[i + 97])) dup++;
    EXPECT_EQ(dup, 0);
}

TEST(rus_uuidseq_not_nil_and_differs_from_v4)
{
    NEED(UuidCreateSequential); NEED(UuidCreate); NEED(UuidIsNil);
    UUID s, r; RPC_STATUS st;
    p_UuidCreateSequential(&s); p_UuidCreate(&r);
    EXPECT_EQ(p_UuidIsNil(&s, &st), 0);
    EXPECT_EQ(s.Data3 >> 12, 1);
    EXPECT_EQ(r.Data3 >> 12, 4);
}

static HANDLE g_seq_ready;
static UUID g_seq_out[4][600];
static DWORD WINAPI seq_thread(LPVOID p)
{
    int idx = (int)(INT_PTR)p;
    __typeof__(&UuidCreateSequential) f = U(UuidCreateSequential);
    for (int i = 0; i < 600; i++) f(&g_seq_out[idx][i]);
    return 0;
}

TEST(rus_uuidseq_threads_unique)
{
    NEED(UuidCreateSequential);
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, seq_thread, (LPVOID)(INT_PTR)i, 0, NULL);
    DWORD w = WaitForMultipleObjects(4, h, TRUE, 10000);
    EXPECT_EQ(w, WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
    if (w != WAIT_OBJECT_0) return;
    int dup = 0;
    for (int a = 0; a < 4; a++)
        for (int b = a; b < 4; b++)
            for (int i = 0; i < 600; i++)
                for (int j = (a == b ? i + 1 : 0); j < 600; j++)
                    if (rus_uuid_eq(&g_seq_out[a][i], &g_seq_out[b][j])) dup++;
    EXPECT_EQ(dup, 0);
    /* within one thread the timestamp rises */
    for (int a = 0; a < 4; a++) {
        int bad = 0;
        for (int i = 1; i < 600; i++) if (rus_v1_stamp(&g_seq_out[a][i]) <= rus_v1_stamp(&g_seq_out[a][i - 1])) bad++;
        EXPECT_EQ(bad, 0);
    }
}

/* ------------------------------------------------------------ UuidCreateNil */

TEST(rus_uuidnil_create)
{
    NEED(UuidCreateNil); NEED(UuidIsNil);
    UUID u; RPC_STATUS s = 5;
    memset(&u, 0xa5, sizeof u);
    EXPECT_EQ(p_UuidCreateNil(&u), RPC_S_OK);
    unsigned char zero[16] = {0};
    CHECK(memcmp(&u, zero, 16) == 0);
    EXPECT_EQ(p_UuidIsNil(&u, &s), 1);
    EXPECT_EQ(s, RPC_S_OK);
}

TEST(rus_uuidnil_isnil_each_byte)
{
    NEED(UuidIsNil);
    /* a single non-zero byte at any of the 16 positions makes the uuid non-nil */
    for (int pos = 0; pos < 16; pos++) {
        UUID u; RPC_STATUS s = 99;
        memset(&u, 0, sizeof u);
        ((unsigned char *)&u)[pos] = 0x80;
        EXPECT_EQ(p_UuidIsNil(&u, &s), 0);
        EXPECT_EQ(s, RPC_S_OK);
        ((unsigned char *)&u)[pos] = 0x01;
        EXPECT_EQ(p_UuidIsNil(&u, &s), 0);
    }
}

TEST(rus_uuidnil_isnil_null_pointer)
{
    NEED(UuidIsNil);
    RPC_STATUS s = 99;
    EXPECT_EQ(p_UuidIsNil(NULL, &s), 1); /* NULL is treated as the nil uuid */
    EXPECT_EQ(s, RPC_S_OK);
}

TEST(rus_uuidnil_isnil_table)
{
    NEED(UuidIsNil);
    for (int i = 0; i < NSTR(g_tbl); i++) {
        UUID u = g_tbl[i]; RPC_STATUS s = 123;
        int nil = (i == 0);
        EXPECT_EQ(p_UuidIsNil(&u, &s), nil);
        EXPECT_EQ(s, RPC_S_OK);
    }
}

/* ---------------------------------------------------------------- UuidEqual */

/* adapted from Wine dlls/rpcrt4/tests/rpc.c (LGPL-2.1+) */
TEST(rus_uuidequal_grid_with_null)
{
    NEED(UuidEqual);
    /* index 10 means NULL. NULL equals NULL and equals the nil uuid (table index 0), nothing else. */
    for (int i = 0; i <= 10; i++)
        for (int j = 0; j <= 10; j++) {
            UUID a, b; RPC_STATUS s = 55;
            UUID *pa = NULL, *pb = NULL;
            if (i < 10) { a = g_tbl[i]; pa = &a; }
            if (j < 10) { b = g_tbl[j]; pb = &b; }
            int want = (i == j) || ((i == 0 || i == 10) && (j == 0 || j == 10));
            int got = p_UuidEqual(pa, pb, &s);
            CHECKF(got == want, "i=%d j=%d got=%d want=%d", i, j, got, want);
            EXPECT_EQ(s, RPC_S_OK);
        }
}

TEST(rus_uuidequal_each_byte_differs)
{
    NEED(UuidEqual);
    UUID base = g_tbl[6];
    for (int pos = 0; pos < 16; pos++) {
        UUID o = base; RPC_STATUS s = 9;
        ((unsigned char *)&o)[pos] ^= 0x01;
        EXPECT_EQ(p_UuidEqual(&base, &o, &s), 0);
        EXPECT_EQ(p_UuidEqual(&o, &base, &s), 0);
        EXPECT_EQ(s, RPC_S_OK);
    }
    RPC_STATUS s = 9; UUID c = base;
    EXPECT_EQ(p_UuidEqual(&base, &c, &s), 1);
    EXPECT_EQ(p_UuidEqual(&base, &base, &s), 1); /* same address */
}

TEST(rus_uuidequal_null_vs_nonnil_both_orders)
{
    NEED(UuidEqual);
    UUID x = g_tbl[6]; RPC_STATUS s = 1;
    EXPECT_EQ(p_UuidEqual(NULL, &x, &s), 0);
    EXPECT_EQ(p_UuidEqual(&x, NULL, &s), 0);
    EXPECT_EQ(p_UuidEqual(NULL, NULL, &s), 1);
    EXPECT_EQ(s, RPC_S_OK);
}

/* -------------------------------------------------------------- UuidCompare */

struct cmp_row { int a, b; int want; };

TEST(rus_uuidcompare_field_order)
{
    NEED(UuidCompare);
    /* field by field: Data1, Data2, Data3 as unsigned integers, then Data4 bytewise unsigned */
    struct { UUID a, b; int want; const char *what; } rows[] = {
        { {2,0,0,{0}},                 {1,0,0,{0}},                 1, "d1 2>1" },
        { {1,0,0,{0}},                 {2,0,0,{0}},                -1, "d1 1<2" },
        { {0x80000000,0,0,{0}},        {1,0,0,{0}},                 1, "d1 unsigned 0x80000000 > 1" },
        { {0xffffffff,0,0,{0}},        {0,1,1,{1,1,1,1,1,1,1,1}},   1, "d1 unsigned 0xffffffff > small" },
        { {1,0,0,{0}},                 {0x80000000,0,0,{0}},       -1, "d1 1 < 0x80000000" },
        { {5,2,0,{0}},                 {5,1,0,{0}},                 1, "d2 2>1" },
        { {5,0x8000,0,{0}},            {5,1,0,{0}},                 1, "d2 unsigned 0x8000 > 1" },
        { {5,1,0,{0}},                 {5,0xffff,0,{0}},           -1, "d2 1 < 0xffff" },
        { {5,5,3,{0}},                 {5,5,2,{0}},                 1, "d3 3>2" },
        { {5,5,0x8000,{0}},            {5,5,0x7fff,{0}},            1, "d3 unsigned" },
        { {5,5,0xffff,{0}},            {5,5,0,{1}},                 1, "d3 beats data4" },
        { {5,5,5,{0x80,0,0,0,0,0,0,0}},{5,5,5,{0x7f,0,0,0,0,0,0,0}},1, "d4[0] unsigned 0x80 > 0x7f" },
        { {5,5,5,{0,0,0,0,0,0,0,0x80}},{5,5,5,{0,0,0,0,0,0,0,0x7f}},1, "d4[7] unsigned" },
        { {5,5,5,{0,1,0,0,0,0,0,0}},   {5,5,5,{0,0,0xff,0,0,0,0,0}},1, "d4 first differing byte decides" },
        { {5,5,5,{1,2,3,4,5,6,7,8}},   {5,5,5,{1,2,3,4,5,6,7,9}}, -1, "d4 last byte" },
        { {5,5,5,{1,2,3,4,5,6,7,8}},   {5,5,5,{1,2,3,4,5,6,7,8}},  0, "equal" },
        { {0xdeadbeef,0xcafe,0xf00d,{0xff,0xfe,0xfd,0xfc,0xfb,0xfa,0xf9,0xf8}},
          {0xdeadbeef,0xcafe,0xf00d,{0xff,0xfe,0xfd,0xfc,0xfb,0xfa,0xf9,0xf8}}, 0, "equal high" },
    };
    for (int i = 0; i < NSTR(rows); i++) {
        RPC_STATUS s = 31;
        int got = p_UuidCompare(&rows[i].a, &rows[i].b, &s);
        CHECKF(got == rows[i].want, "%s: got %d want %d", rows[i].what, got, rows[i].want);
        EXPECT_EQ(s, RPC_S_OK);
        int rev = p_UuidCompare(&rows[i].b, &rows[i].a, &s);
        CHECKF(rev == -rows[i].want, "%s (reversed): got %d want %d", rows[i].what, rev, -rows[i].want);
    }
}

TEST(rus_uuidcompare_return_values_are_unit)
{
    NEED(UuidCompare);
    /* the function returns exactly -1, 0 or 1, never the raw difference */
    UUID a = {0x7fffffff, 0, 0, {0}}, b = {0x00000001, 0, 0, {0}}; RPC_STATUS s;
    EXPECT_EQ(p_UuidCompare(&a, &b, &s), 1);
    EXPECT_EQ(p_UuidCompare(&b, &a, &s), -1);
    UUID c = {1, 0, 0, {0,0,0,0,0,0,0,0xff}}, d = {1, 0, 0, {0,0,0,0,0,0,0,1}};
    EXPECT_EQ(p_UuidCompare(&c, &d, &s), 1);
    EXPECT_EQ(p_UuidCompare(&d, &c, &s), -1);
}

TEST(rus_uuidcompare_null_is_nil)
{
    NEED(UuidCompare);
    UUID nil = {0}, x = g_tbl[6]; RPC_STATUS s = 9;
    EXPECT_EQ(p_UuidCompare(NULL, NULL, &s), 0);
    EXPECT_EQ(p_UuidCompare(NULL, &nil, &s), 0);
    EXPECT_EQ(p_UuidCompare(&nil, NULL, &s), 0);
    EXPECT_EQ(p_UuidCompare(&nil, &nil, &s), 0);
    EXPECT_EQ(p_UuidCompare(NULL, &x, &s), -1);
    EXPECT_EQ(p_UuidCompare(&x, NULL, &s), 1);
    EXPECT_EQ(p_UuidCompare(&nil, &x, &s), -1);
    EXPECT_EQ(p_UuidCompare(&x, &nil, &s), 1);
    EXPECT_EQ(s, RPC_S_OK);
}

TEST(rus_uuidcompare_total_order_over_table)
{
    NEED(UuidCompare);
    /* antisymmetry, reflexivity, transitivity over the table (including the nil member) */
    int n = NSTR(g_tbl);
    for (int i = 0; i < n; i++) {
        RPC_STATUS s;
        UUID a = g_tbl[i];
        EXPECT_EQ(p_UuidCompare(&a, &a, &s), 0);
        for (int j = 0; j < n; j++) {
            UUID b = g_tbl[j];
            int ab = p_UuidCompare(&a, &b, &s), ba = p_UuidCompare(&b, &a, &s);
            CHECKF(ab == -ba, "antisymmetry %d,%d: %d vs %d", i, j, ab, ba);
            CHECKF((ab == 0) == (i == j), "equality %d,%d", i, j);
            for (int k = 0; k < n; k++) {
                UUID c = g_tbl[k];
                int bc = p_UuidCompare(&b, &c, &s), ac = p_UuidCompare(&a, &c, &s);
                if (ab < 0 && bc < 0) CHECKF(ac < 0, "transitivity %d<%d<%d", i, j, k);
            }
        }
    }
}

TEST(rus_uuidcompare_agrees_with_memcmp_of_big_endian_form)
{
    NEED(UuidCompare);
    /* ordering is lexicographic over (Data1,Data2,Data3 numerically, Data4 bytes): build a big-endian image and compare */
    unsigned seed = 0xC0FFEE;
    for (int i = 0; i < 400; i++) {
        UUID a, b; RPC_STATUS s;
        unsigned char *pa = (unsigned char *)&a, *pb = (unsigned char *)&b;
        for (int k = 0; k < 16; k++) { pa[k] = (unsigned char)rus_rand(&seed); pb[k] = pa[k]; }
        /* force the first difference at a random byte of the canonical order */
        int fld = rus_rand(&seed) % 11; /* 0:d1 1:d2 2:d3 3..10: d4 bytes */
        if (fld == 0) b.Data1 ^= 0x00010000u << (rus_rand(&seed) % 15);
        else if (fld == 1) b.Data2 ^= (unsigned short)(1u << (rus_rand(&seed) % 16));
        else if (fld == 2) b.Data3 ^= (unsigned short)(1u << (rus_rand(&seed) % 16));
        else b.Data4[fld - 3] ^= (unsigned char)(1u << (rus_rand(&seed) % 8));
        unsigned char ia[16], ib[16];
        ia[0]=a.Data1>>24; ia[1]=a.Data1>>16; ia[2]=a.Data1>>8; ia[3]=a.Data1; ia[4]=a.Data2>>8; ia[5]=a.Data2; ia[6]=a.Data3>>8; ia[7]=a.Data3;
        memcpy(ia + 8, a.Data4, 8);
        ib[0]=b.Data1>>24; ib[1]=b.Data1>>16; ib[2]=b.Data1>>8; ib[3]=b.Data1; ib[4]=b.Data2>>8; ib[5]=b.Data2; ib[6]=b.Data3>>8; ib[7]=b.Data3;
        memcpy(ib + 8, b.Data4, 8);
        int want = memcmp(ia, ib, 16); want = want < 0 ? -1 : want > 0 ? 1 : 0;
        /* nil operands are the one special case; random data is practically never nil */
        int got = p_UuidCompare(&a, &b, &s);
        CHECKF(got == want, "iter %d fld %d got %d want %d", i, fld, got, want);
    }
}

/* ----------------------------------------------------------------- UuidHash */

static unsigned short ref_hash(const UUID *u)
{
    const unsigned short *w = (const unsigned short *)u;
    unsigned short h = 0;
    for (int i = 0; i < 8; i++) h ^= w[i];
    return h;
}

TEST(rus_uuidhash_known_values)
{
    NEED(UuidHash);
    struct { UUID u; unsigned short want; } rows[] = {
        { {0x12345678,0x9abc,0xdef0,{1,2,3,4,5,6,7,8}},                           0x0800 },
        { {0x8a885d04,0x1ceb,0x11c9,{0x9f,0xe8,0x08,0x00,0x2b,0x10,0x48,0x60}},   0x425a },
        { {0,0,0,{0,0,0,0,0,0,0,1}},                                              0x0100 },
        { {0xffffffff,0xffff,0xffff,{0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff}},   0x0000 },
        { {0,0,0,{0,0,0,0,0,0,0,0x80}},                                           0x8000 },
        { {1,0,0,{0,0,0,0,0,0,0,0}},                                              0x0001 },
        { {0x00010000,0,0,{0,0,0,0,0,0,0,0}},                                     0x0001 },
        { {0,0,0,{0xc0,0,0,0,0,0,0,0x46}},                                        0x46c0 },
        { {0,0,0,{0,0,0,0,0,0,0,0}},                                              0x0000 },
    };
    for (int i = 0; i < NSTR(rows); i++) {
        RPC_STATUS s = 91;
        unsigned short h = p_UuidHash(&rows[i].u, &s);
        CHECKF(h == rows[i].want, "row %d got 0x%04x want 0x%04x", i, h, rows[i].want);
        EXPECT_EQ(s, RPC_S_OK);
    }
}

TEST(rus_uuidhash_null_is_zero)
{
    NEED(UuidHash);
    RPC_STATUS s = 91;
    EXPECT_EQ(p_UuidHash(NULL, &s), 0);
    EXPECT_EQ(s, RPC_S_OK);
}

TEST(rus_uuidhash_xor_of_eight_words)
{
    NEED(UuidHash);
    unsigned seed = 7;
    for (int i = 0; i < 300; i++) {
        UUID u; unsigned char *b = (unsigned char *)&u; RPC_STATUS s = 5;
        for (int k = 0; k < 16; k++) b[k] = (unsigned char)rus_rand(&seed);
        unsigned short got = p_UuidHash(&u, &s);
        CHECKF(got == ref_hash(&u), "iter %d got 0x%04x want 0x%04x", i, got, ref_hash(&u));
        EXPECT_EQ(s, RPC_S_OK);
    }
}

TEST(rus_uuidhash_each_byte_position)
{
    NEED(UuidHash);
    /* a single byte at position p contributes to the low byte when p is even, the high byte when odd */
    for (int pos = 0; pos < 16; pos++) {
        UUID u; RPC_STATUS s;
        memset(&u, 0, sizeof u);
        ((unsigned char *)&u)[pos] = 0xa5;
        unsigned short want = (pos & 1) ? 0xa500 : 0x00a5;
        unsigned short got = p_UuidHash(&u, &s);
        CHECKF(got == want, "pos %d got 0x%04x want 0x%04x", pos, got, want);
    }
}

TEST(rus_uuidhash_equal_uuids_hash_equal)
{
    NEED(UuidHash); NEED(UuidCreate);
    for (int i = 0; i < 50; i++) {
        UUID a, b; RPC_STATUS s;
        p_UuidCreate(&a); b = a;
        EXPECT_EQ(p_UuidHash(&a, &s), p_UuidHash(&b, &s));
    }
}
