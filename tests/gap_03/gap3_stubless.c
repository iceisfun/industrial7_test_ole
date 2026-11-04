/* gap_03: combase/ole32 ObjectStublessClient3..32 and NdrProxyForwardingFunction3..32 (resolved by name with GetProcAddress).
 * Win10 combase: ObjectStublessClientN is `mov eax, N ; jmp <common stubless dispatcher>` placed 0x10 bytes apart;
 * ole32 re-exports each as a forwarder or a `jmp [iat]` thunk landing on the same combase code;
 * NdrProxyForwardingFunctionN(P, ...) tail-calls  (*(void***)P[+0x20])[N](P[+0x20], ...)  with every argument preserved. */
#include "g3once.h"

static FARPROC sym(const char *dll, const char *fmt, int n) {
    char nm[64]; wsprintfA(nm, fmt, n);
    HMODULE m = t_module(dll);
    return m ? GetProcAddress(m, nm) : NULL;
}
#define NOWINE() do { if (t_is_wine()) { t_skip("wine: different thunk/proxy layout"); return; } } while (0)

TEST(gap3_stubless_thunks_load_slot_number_into_eax) {
    NOWINE();
    HMODULE c = t_module("combase.dll"); if (!c) { t_skip("no combase"); return; }
    ULONG_PTR common = 0, prev = 0;
    for (int n = 3; n <= 32; n++) {
        const unsigned char *p = (const unsigned char *)sym("combase.dll", "ObjectStublessClient%d", n);
        if (!p) { CHECKF(0, "combase lacks ObjectStublessClient%d", n); continue; }
        CHECKF(p[0] == 0xB8 && *(const DWORD *)(p + 1) == (DWORD)n, "ObjectStublessClient%d: expected `mov eax, %d`, bytes %02x %02x %02x %02x %02x", n, n, p[0], p[1], p[2], p[3], p[4]);
        CHECKF(p[5] == 0xE9, "ObjectStublessClient%d: expected jmp rel32, got %02x", n, p[5]);
        if (p[5] == 0xE9) {
            ULONG_PTR tgt = (ULONG_PTR)(p + 10) + *(const INT *)(p + 6);
            if (!common) common = tgt;
            CHECKF(tgt == common, "ObjectStublessClient%d jumps to %p, others to %p", n, (void *)tgt, (void *)common);
        }
        if (prev) CHECKF((ULONG_PTR)p - prev == 0x10, "ObjectStublessClient%d is 0x%llx after the previous thunk", n, (unsigned long long)((ULONG_PTR)p - prev));
        prev = (ULONG_PTR)p;
        CHECKF((ULONG_PTR)p >= (ULONG_PTR)c, "thunk outside combase");
    }
}
TEST(gap3_stubless_ole32_reexports_land_on_combase_thunks) {
    NOWINE();
    if (!t_module("ole32.dll") || !t_module("combase.dll")) { t_skip("modules"); return; }
    int same = 0, via_jmp = 0, bad = 0;
    for (int n = 3; n <= 32; n++) {
        const unsigned char *o = (const unsigned char *)sym("ole32.dll", "ObjectStublessClient%d", n);
        const unsigned char *c = (const unsigned char *)sym("combase.dll", "ObjectStublessClient%d", n);
        if (!o || !c) { CHECKF(0, "ObjectStublessClient%d missing (ole32 %p combase %p)", n, o, c); continue; }
        if (o == c) { same++; continue; }
        if (o[0] == 0xFF && o[1] == 0x25) {                      /* jmp qword ptr [rip+disp32] through the import table */
            const void *const *slot = (const void *const *)(o + 6 + *(const INT *)(o + 2));
            if (*slot == (const void *)c) { via_jmp++; continue; }
        }
        bad++;
        CHECKF(0, "ole32!ObjectStublessClient%d (%p) does not lead to combase (%p)", n, o, c);
    }
    t_note("ole32 ObjectStublessClient3..32: %d forwarded to the same address, %d jmp-through-IAT thunks, %d unexplained", same, via_jmp, bad);
    CHECK(same + via_jmp == 30);
}
TEST(gap3_stubless_nonexistent_slots_are_not_exported) {
    HMODULE c = t_module("combase.dll"), o = t_module("ole32.dll");
    static const int ns[] = { -1, 0, 1, 2, 33, 34, 64, 100 };
    for (unsigned i = 0; i < sizeof ns / sizeof ns[0]; i++) {
        char nm[48]; wsprintfA(nm, "ObjectStublessClient%d", ns[i]);
        CHECKF(!(c && GetProcAddress(c, nm)), "combase exports %s", nm);
        CHECKF(!(o && GetProcAddress(o, nm)), "ole32 exports %s", nm);
        wsprintfA(nm, "NdrProxyForwardingFunction%d", ns[i]);
        CHECKF(!(c && GetProcAddress(c, nm)), "combase exports %s", nm);
        CHECKF(!(o && GetProcAddress(o, nm)), "ole32 exports %s", nm);
    }
}

/* ---- NdrProxyForwardingFunctionN with eight arguments (four on the stack), 64-bit results and the replaced `this` ---- */
typedef struct { int n; void *this_; ULONG_PTR a[7]; } Rec;
static Rec g_rec;
typedef ULONG_PTR (WINAPI *m8_fn)(void *, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);
#define REC(N) static ULONG_PTR WINAPI rec_##N(void *t, ULONG_PTR a1, ULONG_PTR a2, ULONG_PTR a3, ULONG_PTR a4, ULONG_PTR a5, ULONG_PTR a6, ULONG_PTR a7) { \
    g_rec.n = N; g_rec.this_ = t; g_rec.a[0] = a1; g_rec.a[1] = a2; g_rec.a[2] = a3; g_rec.a[3] = a4; g_rec.a[4] = a5; g_rec.a[5] = a6; g_rec.a[6] = a7; \
    return ((ULONG_PTR)0x0123456700000000ull) | (ULONG_PTR)(0x100 + N); }
#define LIST(X) X(3) X(4) X(5) X(6) X(7) X(8) X(9) X(10) X(11) X(12) X(13) X(14) X(15) X(16) X(17) X(18) X(19) X(20) X(21) X(22) X(23) X(24) X(25) X(26) X(27) X(28) X(29) X(30) X(31) X(32)
LIST(REC)
#define ENT(N) (void *)rec_##N,
static void *g_vt[33] = { 0, 0, 0, LIST(ENT) };
#undef ENT

TEST(gap3_proxyfwd_eight_args_and_64bit_result_all_slots) {
    NOWINE();
    void *base_obj[1] = { g_vt };
    void *frame[8] = { 0 };
    frame[4] = base_obj;                          /* P + 0x20: the base interface pointer */
    int bad = 0;
    for (int n = 3; n <= 32; n++) {
        m8_fn f = (m8_fn)sym("combase.dll", "NdrProxyForwardingFunction%d", n);
        if (!f) { bad++; CHECKF(0, "missing NdrProxyForwardingFunction%d", n); continue; }
        memset(&g_rec, 0, sizeof g_rec);
        ULONG_PTR r = f(frame, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7);
        int ok = g_rec.n == n && g_rec.this_ == (void *)base_obj && r == (((ULONG_PTR)0x0123456700000000ull) | (ULONG_PTR)(0x100 + n));
        for (int k = 0; k < 7; k++) ok &= g_rec.a[k] == (ULONG_PTR)(0xA1 + k);
        if (!ok) { bad++; CHECKF(0, "slot %d: reached %d this=%p (want %p) ret=%llx a=%llx %llx %llx %llx %llx %llx %llx", n, g_rec.n, g_rec.this_, (void *)base_obj, (unsigned long long)r,
                                (unsigned long long)g_rec.a[0], (unsigned long long)g_rec.a[1], (unsigned long long)g_rec.a[2], (unsigned long long)g_rec.a[3], (unsigned long long)g_rec.a[4], (unsigned long long)g_rec.a[5], (unsigned long long)g_rec.a[6]); }
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap3_proxyfwd_follows_the_base_pointer_not_the_proxy_vtable) {
    NOWINE();
    /* two different base objects behind two different proxies: each call must reach its own base's slot */
    static void *vt_a[33], *vt_b[33];
    memcpy(vt_a, g_vt, sizeof vt_a); memcpy(vt_b, g_vt, sizeof vt_b);
    vt_b[9] = (void *)rec_20;                       /* slot 9 of base B is really method 20's recorder */
    void *ba[1] = { vt_a }, *bb[1] = { vt_b };
    void *fa[8] = { 0 }, *fb[8] = { 0 };
    fa[4] = ba; fb[4] = bb;
    m8_fn f9 = (m8_fn)sym("combase.dll", "NdrProxyForwardingFunction%d", 9);
    if (!f9) { CHECK(0); return; }
    memset(&g_rec, 0, sizeof g_rec); f9(fa, 1, 2, 3, 4, 5, 6, 7);
    EXPECT_EQ(g_rec.n, 9); CHECK(g_rec.this_ == (void *)ba);
    memset(&g_rec, 0, sizeof g_rec); f9(fb, 1, 2, 3, 4, 5, 6, 7);
    EXPECT_EQ(g_rec.n, 20); CHECK(g_rec.this_ == (void *)bb);
}
TEST(gap3_proxyfwd_ole32_aliases_reach_same_code) {
    NOWINE();
    if (!t_module("ole32.dll") || !t_module("combase.dll")) { t_skip("modules"); return; }
    int diff = 0;
    for (int n = 3; n <= 32; n++) {
        FARPROC o = sym("ole32.dll", "NdrProxyForwardingFunction%d", n), c = sym("combase.dll", "NdrProxyForwardingFunction%d", n);
        if (!o || !c) { CHECKF(0, "NdrProxyForwardingFunction%d missing (ole32 %p combase %p)", n, o, c); continue; }
        if (o != c) diff++;
    }
    CHECKF(diff == 0, "%d ole32 NdrProxyForwardingFunctionN exports are not forwarders to combase", diff);
}
