/* I_RpcPauseExecution, I_RpcRequestMutex / I_RpcClearMutex / I_RpcDeleteMutex, NdrUnmarshallBasetypeInline. */
#include "ext.h"
#include <setjmp.h>


/* ---------------------------------------------------------------- I_RpcPauseExecution */
TEST(ext_pause_execution_sleeps_at_least_the_interval) {
    DWORD t0, d; static const DWORD ms[] = { 0, 20, 120, 350 }; unsigned k;
    REQUIRE(UN(I_RpcPauseExecution));
    for (k = 0; k < sizeof ms / sizeof *ms; k++) {
        t0 = GetTickCount(); UN(I_RpcPauseExecution)(ms[k]); d = GetTickCount() - t0;
        CHECKF(d + 32 >= ms[k], "I_RpcPauseExecution(%lu) returned after %lu ms", ms[k], d);   /* tick granularity is ~16 ms */
        CHECKF(d < ms[k] + 2000, "I_RpcPauseExecution(%lu) took %lu ms", ms[k], d);
    }
}
static DWORD WINAPI pause_thread(void *p) { UN(I_RpcPauseExecution)(150); InterlockedExchange((volatile LONG *)p, 1); return 0; }
TEST(ext_pause_execution_blocks_only_the_calling_thread) {
    volatile LONG done = 0; HANDLE th; DWORD t0 = GetTickCount();
    REQUIRE(UN(I_RpcPauseExecution));
    th = CreateThread(NULL, 0, pause_thread, (void *)&done, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    Sleep(20);
    CHECKF(!done, "the helper thread is still paused after 20 ms");
    CHECKF(GetTickCount() - t0 < 120, "the calling thread was not blocked by the helper's pause");
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CHECK(done == 1);
    CloseHandle(th);
}

/* ---------------------------------------------------------------- mutex */
TEST(ext_mutex_is_created_on_first_request_and_recursive) {
    void *m = NULL, *first;
    REQUIRE(UN(I_RpcRequestMutex)); REQUIRE(UN(I_RpcClearMutex)); REQUIRE(UN(I_RpcDeleteMutex));
    ext_warm();
    UN(I_RpcRequestMutex)(&m);
    CHECK(m != NULL);
    first = m;
    UN(I_RpcRequestMutex)(&m);                    /* recursive: the same thread enters again */
    UN(I_RpcRequestMutex)(&m);
    CHECKF(m == first, "the lock object is created once");
    UN(I_RpcClearMutex)(m); UN(I_RpcClearMutex)(m); UN(I_RpcClearMutex)(m);
    UN(I_RpcDeleteMutex)(m);
}
TEST(ext_mutex_delete_null_is_ignored) {
    REQUIRE(UN(I_RpcDeleteMutex));
    UN(I_RpcDeleteMutex)(NULL);
    CHECK(1);
}
TEST(ext_mutex_existing_cell_is_used_as_is) {
    /* a cell that already holds a lock object is not replaced */
    void *m = NULL, *keep;
    REQUIRE(UN(I_RpcRequestMutex));
    ext_warm();
    UN(I_RpcRequestMutex)(&m); keep = m; UN(I_RpcClearMutex)(m);
    UN(I_RpcRequestMutex)(&m);
    CHECK(m == keep);
    UN(I_RpcClearMutex)(m);
    UN(I_RpcDeleteMutex)(m);
}
typedef struct { void **cell; volatile LONG *state; HANDLE go; } mx_job;
static DWORD WINAPI mx_waiter(void *p) {
    mx_job *j = p;
    InterlockedExchange(j->state, 1);              /* about to request */
    UN(I_RpcRequestMutex)(j->cell);
    InterlockedExchange(j->state, 2);              /* entered */
    UN(I_RpcClearMutex)(*j->cell);
    return 0;
}
TEST(ext_mutex_excludes_another_thread_until_cleared) {
    void *m = NULL; volatile LONG state = 0; mx_job j; HANDLE th; int i;
    REQUIRE(UN(I_RpcRequestMutex));
    ext_warm();
    UN(I_RpcRequestMutex)(&m);                     /* held by this thread */
    j.cell = &m; j.state = &state; j.go = NULL;
    th = CreateThread(NULL, 0, mx_waiter, &j, 0, NULL);
    CHECK(th != NULL);
    if (!th) { UN(I_RpcClearMutex)(m); return; }
    for (i = 0; i < 100 && state < 1; i++) Sleep(5);
    Sleep(100);
    CHECKF(state == 1, "the other thread must still be waiting (state %ld)", (long)state);
    UN(I_RpcClearMutex)(m);
    CHECK(WaitForSingleObject(th, 5000) == WAIT_OBJECT_0);
    CHECK(state == 2);
    CloseHandle(th);
    UN(I_RpcDeleteMutex)(m);
}
#define RACERS 8
static DWORD WINAPI mx_racer(void *p) {
    void **cell = p; int i;
    for (i = 0; i < 50; i++) { UN(I_RpcRequestMutex)(cell); UN(I_RpcClearMutex)(*cell); }
    return 0;
}
TEST(ext_mutex_concurrent_first_request_makes_one_lock) {
    void *m = NULL; HANDLE th[RACERS]; int i, ok = 0;
    REQUIRE(UN(I_RpcRequestMutex));
    ext_warm();
    for (i = 0; i < RACERS; i++) th[i] = CreateThread(NULL, 0, mx_racer, &m, 0, NULL);
    for (i = 0; i < RACERS; i++) if (th[i]) { if (WaitForSingleObject(th[i], 10000) == WAIT_OBJECT_0) ok++; CloseHandle(th[i]); }
    CHECKF(ok == RACERS, "%d of %d threads finished", ok, RACERS);
    CHECK(m != NULL);
    UN(I_RpcDeleteMutex)(m);
}
static volatile LONG g_counter;
typedef struct { void **cell; } cnt_job;
static DWORD WINAPI mx_counter(void *p) {
    cnt_job *j = p; int i;
    for (i = 0; i < 2000; i++) { LONG v; UN(I_RpcRequestMutex)(j->cell); v = g_counter; g_counter = v + 1; UN(I_RpcClearMutex)(*j->cell); }
    return 0;
}
TEST(ext_mutex_protects_a_shared_counter) {
    void *m = NULL; HANDLE th[4]; cnt_job j; int i;
    REQUIRE(UN(I_RpcRequestMutex));
    ext_warm();
    UN(I_RpcRequestMutex)(&m); UN(I_RpcClearMutex)(m);        /* create up front */
    j.cell = &m; g_counter = 0;
    for (i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, mx_counter, &j, 0, NULL);
    for (i = 0; i < 4; i++) if (th[i]) { WaitForSingleObject(th[i], 20000); CloseHandle(th[i]); }
    CHECKF(g_counter == 8000, "counter %ld want 8000", (long)g_counter);
    UN(I_RpcDeleteMutex)(m);
}

/* ---------------------------------------------------------------- NdrUnmarshallBasetypeInline */
/* A MIDL_STUB_MESSAGE is 0x190 bytes; the routine only looks at Buffer (+0x08) and BufferEnd (+0x18). */
#define SM_BUFFER(sm)     (*(unsigned char **)((sm) + 0x08))
#define SM_BUFFER_END(sm) (*(unsigned char **)((sm) + 0x18))

static jmp_buf g_jb; static volatile int g_armed; static volatile DWORD g_code;
static LONG CALLBACK veh(PEXCEPTION_POINTERS ep) {
    if (!g_armed) return EXCEPTION_CONTINUE_SEARCH;
    g_code = ep->ExceptionRecord->ExceptionCode; g_armed = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}
typedef struct { unsigned char *sm; ULONGLONG *val; unsigned char fc; } ub_args;
static DWORD guarded_unmarshal(ub_args *a) {
    static int inst; DWORD c = 0;
    if (!inst) { AddVectoredExceptionHandler(1, veh); inst = 1; }
    g_code = 0;
    if (!setjmp(g_jb)) { g_armed = 1; UN(NdrUnmarshallBasetypeInline)(a->sm, a->val, a->fc); g_armed = 0; } else c = g_code;
    g_armed = 0;
    return c;
}
/* one step: buffer at offset `start` in an aligned 64 byte block holding `data`; returns the exception code (0 none) */
static DWORD ub_step(const unsigned char *data, size_t n, size_t start, unsigned char fc, ULONGLONG *out, size_t *adv) {
    static __attribute__((aligned(16))) unsigned char blk[64];
    unsigned char sm[0x190]; ub_args a; DWORD c;
    memset(blk, 0xcc, sizeof blk); memcpy(blk, data, n);
    memset(sm, 0, sizeof sm);
    SM_BUFFER(sm) = blk + start; SM_BUFFER_END(sm) = blk + sizeof blk;
    *out = 0x5a5a5a5a5a5a5a5aULL;
    a.sm = sm; a.val = out; a.fc = fc;
    c = guarded_unmarshal(&a);
    *adv = (size_t)(SM_BUFFER(sm) - blk);
    return c;
}
TEST(ext_unmarshal_basetype_byte_types) {
    static const unsigned char d[] = { 0x11, 0x81, 0xfe };
    static const unsigned char fcs[] = { 0x01, 0x02, 0x03, 0x04 };   /* FC_BYTE FC_CHAR FC_SMALL FC_USMALL */
    unsigned k; ULONGLONG v; size_t adv;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    for (k = 0; k < 4; k++) {
        EQ(ub_step(d, sizeof d, 1, fcs[k], &v, &adv), 0); EQ(v, 0x81); EQ(adv, 2);       /* no alignment, zero extended */
        EQ(ub_step(d, sizeof d, 2, fcs[k], &v, &adv), 0); EQ(v, 0xfe); EQ(adv, 3);
    }
}
TEST(ext_unmarshal_basetype_sixteen_bit_types_align_and_zero_extend) {
    static const unsigned char d[] = { 0, 0, 0x00, 0x80, 0x34, 0x12, 0xff, 0xff };
    static const unsigned char fcs[] = { 0x05, 0x06, 0x07, 0x0d };   /* FC_WCHAR FC_SHORT FC_USHORT FC_ENUM16 */
    unsigned k; ULONGLONG v; size_t adv;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    for (k = 0; k < 4; k++) {
        EQ(ub_step(d, sizeof d, 1, fcs[k], &v, &adv), 0); EQ(v, 0x8000); EQ(adv, 4);     /* offset 1 aligns up to 2 */
        EQ(ub_step(d, sizeof d, 4, fcs[k], &v, &adv), 0); EQ(v, 0x1234); EQ(adv, 6);
        EQ(ub_step(d, sizeof d, 6, fcs[k], &v, &adv), 0); EQ(v, 0xffff); EQ(adv, 8);     /* even FC_SHORT is zero extended */
    }
}
TEST(ext_unmarshal_basetype_thirty_two_bit_types_align_and_sign_extend) {
    static const unsigned char d[] = { 0, 0, 0, 0, 0xfe, 0xff, 0xff, 0xff, 1, 2, 3, 0x7f };
    static const unsigned char fcs[] = { 0x08, 0x09, 0x0a, 0x0e, 0x10, 0xb8 };   /* LONG ULONG FLOAT ENUM32 ERROR_STATUS_T INT3264 */
    unsigned k; ULONGLONG v; size_t adv;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    for (k = 0; k < 6; k++) {
        EQ(ub_step(d, sizeof d, 1, fcs[k], &v, &adv), 0); EQ(v, 0xfffffffffffffffeULL); EQ(adv, 8);    /* aligns 1 -> 4; sign extended */
        EQ(ub_step(d, sizeof d, 8, fcs[k], &v, &adv), 0); EQ(v, 0x7f030201); EQ(adv, 12);
    }
    EQ(ub_step(d, sizeof d, 4, 0xb9, &v, &adv), 0); EQ(v, 0xfffffffeULL); EQ(adv, 8);               /* FC_UINT3264 zero extends */
}
TEST(ext_unmarshal_basetype_eight_byte_types_align_to_eight) {
    static const unsigned char d[] = { 0,0,0,0,0,0,0,0, 0x88,0x77,0x66,0x55,0x44,0x33,0x22,0x11 };
    static const unsigned char fcs[] = { 0x0b, 0x0c };   /* FC_HYPER FC_DOUBLE */
    unsigned k; ULONGLONG v; size_t adv;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    for (k = 0; k < 2; k++) {
        EQ(ub_step(d, sizeof d, 1, fcs[k], &v, &adv), 0); EQ(v, 0x1122334455667788ULL); EQ(adv, 16);
        EQ(ub_step(d, sizeof d, 8, fcs[k], &v, &adv), 0); EQ(v, 0x1122334455667788ULL); EQ(adv, 16);
    }
}
TEST(ext_unmarshal_basetype_other_format_characters_do_nothing) {
    static const unsigned char d[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    static const unsigned char fcs[] = { 0x00, 0x0f, 0x11, 0x20, 0xb7, 0xba, 0xff };
    unsigned k; ULONGLONG v; size_t adv;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    for (k = 0; k < sizeof fcs; k++) {
        EQ(ub_step(d, sizeof d, 3, fcs[k], &v, &adv), 0);
        EQ(v, 0x5a5a5a5a5a5a5a5aULL);            /* nothing stored */
        EQ(adv, 3);                               /* Buffer unchanged */
    }
}
TEST(ext_unmarshal_basetype_buffer_end_check) {
    static __attribute__((aligned(16))) unsigned char blk[32];
    unsigned char sm[0x190]; ub_args a; ULONGLONG v; DWORD c;
    REQUIRE(UN(NdrUnmarshallBasetypeInline));
    memset(blk, 0x11, sizeof blk); memset(sm, 0, sizeof sm);
    a.sm = sm; a.val = &v; a.fc = 0x08;
    /* a value that ends exactly at BufferEnd is fine */
    SM_BUFFER(sm) = blk; SM_BUFFER_END(sm) = blk + 4;
    c = guarded_unmarshal(&a); EQ(c, 0); EQ(SM_BUFFER(sm), blk + 4);
    /* one byte short: the read happens, then RPC_X_BAD_STUB_DATA is raised */
    SM_BUFFER(sm) = blk; SM_BUFFER_END(sm) = blk + 3; v = 0;
    c = guarded_unmarshal(&a); EQ(c, RPC_X_BAD_STUB_DATA);
    EQ(SM_BUFFER(sm), blk + 4);               /* Buffer was advanced before the check */
    EQ(v, 0x11111111);
    /* alignment alone can push the buffer past the end */
    SM_BUFFER(sm) = blk + 1; SM_BUFFER_END(sm) = blk + 4; a.fc = 0x07;
    c = guarded_unmarshal(&a); EQ(c, 0);
    SM_BUFFER(sm) = blk + 1; SM_BUFFER_END(sm) = blk + 3; a.fc = 0x07;
    c = guarded_unmarshal(&a); EQ(c, RPC_X_BAD_STUB_DATA);
    /* an FC that reads nothing still checks the end */
    SM_BUFFER(sm) = blk + 8; SM_BUFFER_END(sm) = blk + 4; a.fc = 0x00;
    c = guarded_unmarshal(&a); EQ(c, RPC_X_BAD_STUB_DATA);
}
