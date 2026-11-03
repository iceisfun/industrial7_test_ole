/* combase RoInspectThreadErrorInfo / RoInspectCapturedStackBackTrace: debugger-facing helpers that read a target
 * process through a caller supplied callback. Driven here with a fake "target memory" so every read the DLL issues
 * (address, length, order) is observable. Offsets and checks come from the Win10 combase decompilation. */
#include "gap8.h"

typedef HRESULT (__cdecl *rd_fn)(void *ctx, ULONG64 addr, UINT32 len, BYTE *buf);
UDECL(HRESULT, RoInspectThreadErrorInfo, (ULONG64 teb, USHORT machine, rd_fn rd, void *ctx, ULONG64 *out));
UDECL(HRESULT, RoInspectCapturedStackBackTrace, (ULONG64 errinfo, USHORT machine, rd_fn rd, void *ctx, UINT32 *frames, ULONG64 *bt));

#define M_AMD64 0x8664
#define M_ARM64 0xaa64
#define M_I386  0x14c
#define M_ARMNT 0x1c4
#define M_ARM   0x1c0

typedef struct { ULONG64 addr; UINT32 len; } rec_t;
typedef struct {
    rec_t r[64]; int n;
    void *self;                       /* context identity check */
    /* thread-error-info script */
    ULONG64 teb_ptr_value;            /* value returned for the first read */
    ULONG64 info_value;               /* value returned for the second read */
    HRESULT fail1, fail2;             /* HRESULT to return from read #1 / #2 (0 = none) */
    HRESULT ret1, ret2;               /* success HRESULT to return for read #1 / #2 */
    int skip_write1;
    /* backtrace script */
    ULONG64 valid_base;               /* the single address base at which a valid structure lives (0 = none) */
    UINT32 sig; UINT16 ptrsize, frames; ULONG64 bt;
    UINT32 off_sig, off_ptr, off_frames, off_bt;
    int serve_everywhere;
} mem_t;

static int bad_ctx;
static HRESULT __cdecl rd_thread(void *ctx, ULONG64 addr, UINT32 len, BYTE *buf)
{
    mem_t *m = ctx; if (ctx != m->self) bad_ctx++;
    if (m->n < 64) { m->r[m->n].addr = addr; m->r[m->n].len = len; }
    m->n++;
    if (m->n == 1) {
        if (m->fail1) return m->fail1;
        if (!m->skip_write1 && len == 8) memcpy(buf, &m->teb_ptr_value, 8);
        return m->ret1;
    }
    if (m->fail2) return m->fail2;
    if (len == 8) memcpy(buf, &m->info_value, 8);
    return m->ret2;
}

static void init_thread_mem(mem_t *m)
{
    memset(m, 0, sizeof *m); m->self = m; m->teb_ptr_value = 0x0000000123450000ull; m->info_value = 0x00000007aabbccd8ull;
}

static void thread_ok(USHORT machine, ULONG64 off1, ULONG64 off2)
{
    mem_t m; ULONG64 out = 0xdeadbeefdeadbeefull; ULONG64 teb = 0x000000ab00001000ull; HRESULT hr;
    G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    init_thread_mem(&m); bad_ctx = 0;
    hr = UN(RoInspectThreadErrorInfo)(teb, machine, rd_thread, &m, &out);
    G8_EQ(hr, S_OK);
    G8_EQ(m.n, 2);
    G8_EQ(m.r[0].addr, teb + off1); G8_EQ(m.r[0].len, 8);
    G8_EQ(m.r[1].addr, m.teb_ptr_value + off2); G8_EQ(m.r[1].len, 8);
    G8_EQ(out, m.info_value);
    G8_EQ(bad_ctx, 0);
}

TEST(gap8_ro_inspect_thread_amd64_offsets)  { thread_ok(M_AMD64, 0x1758, 0x198); }
TEST(gap8_ro_inspect_thread_arm64_offsets)  { thread_ok(M_ARM64, 0x1758, 0x198); }
TEST(gap8_ro_inspect_thread_i386_offsets)   { thread_ok(M_I386, 0xf80, 0xf4); }
TEST(gap8_ro_inspect_thread_armnt_offsets)  { thread_ok(M_ARMNT, 0xf80, 0xf4); }
TEST(gap8_ro_inspect_thread_unknown_machine_uses_32bit_layout)
{
    /* any machine other than 0x8664/0xaa64 selects the 32-bit offsets; there is no machine validation */
    thread_ok(0, 0xf80, 0xf4);
    thread_ok(M_ARM, 0xf80, 0xf4);
    thread_ok(0x1234, 0xf80, 0xf4);
    thread_ok(0xffff, 0xf80, 0xf4);
}

TEST(gap8_ro_inspect_thread_null_pointer_is_not_initialized)
{
    mem_t m; ULONG64 out = 0x1111; HRESULT hr; G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    init_thread_mem(&m); m.teb_ptr_value = 0;
    hr = UN(RoInspectThreadErrorInfo)(0x5000, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, G8_E_NOTINIT);                 /* CO_E_NOTINITIALIZED: no per-thread error info yet */
    G8_EQ(m.n, 1);                           /* the second read is never issued */
    G8_EQ(out, 0x1111);                      /* output untouched */
    /* the callback that "succeeds" without writing anything leaves a zero pointer */
    init_thread_mem(&m); m.skip_write1 = 1;
    out = 0x2222;
    hr = UN(RoInspectThreadErrorInfo)(0x5000, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, G8_E_NOTINIT); G8_EQ(m.n, 1); G8_EQ(out, 0x2222);
}

TEST(gap8_ro_inspect_thread_first_read_failure_propagates)
{
    static const HRESULT codes[] = { G8_E_FAIL, G8_E_ACCESSDENIED, (HRESULT)0x8007001E, (HRESULT)0x80070057, (HRESULT)0xC0000005 };
    unsigned i; G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        mem_t m; ULONG64 out = 0x3333; HRESULT hr;
        init_thread_mem(&m); m.fail1 = codes[i];
        hr = UN(RoInspectThreadErrorInfo)(0x6000, M_AMD64, rd_thread, &m, &out);
        G8_EQ(hr, codes[i]); G8_EQ(m.n, 1); G8_EQ(out, 0x3333);
    }
}

TEST(gap8_ro_inspect_thread_second_read_failure_propagates)
{
    static const HRESULT codes[] = { G8_E_FAIL, G8_E_ACCESSDENIED, (HRESULT)0x8007001E };
    unsigned i; G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        mem_t m; ULONG64 out = 0x4444; HRESULT hr;
        init_thread_mem(&m); m.fail2 = codes[i];
        hr = UN(RoInspectThreadErrorInfo)(0x6000, M_AMD64, rd_thread, &m, &out);
        G8_EQ(hr, codes[i]); G8_EQ(m.n, 2); G8_EQ(out, 0x4444);
    }
}

TEST(gap8_ro_inspect_thread_positive_hresults_are_success)
{
    /* the DLL tests the callback results with "< 0": S_FALSE (1) from read #1 still continues, and the
     * HRESULT of the LAST read is the function's return value */
    mem_t m; ULONG64 out = 0; HRESULT hr; G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    init_thread_mem(&m); m.ret1 = S_FALSE; m.ret2 = S_FALSE;
    hr = UN(RoInspectThreadErrorInfo)(0x7000, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, S_FALSE); G8_EQ(m.n, 2); G8_EQ(out, m.info_value);
    init_thread_mem(&m); m.ret1 = S_FALSE; m.ret2 = S_OK;
    hr = UN(RoInspectThreadErrorInfo)(0x7000, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, S_OK);
    init_thread_mem(&m); m.ret1 = S_OK; m.ret2 = (HRESULT)2;
    hr = UN(RoInspectThreadErrorInfo)(0x7000, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, 2);
}

TEST(gap8_ro_inspect_thread_address_arithmetic_wraps)
{
    mem_t m; ULONG64 out = 0; HRESULT hr; ULONG64 teb = 0xfffffffffffff000ull; G8_REQ_NW(UN(RoInspectThreadErrorInfo));
    init_thread_mem(&m); m.teb_ptr_value = 0xfffffffffffffff0ull;
    hr = UN(RoInspectThreadErrorInfo)(teb, M_AMD64, rd_thread, &m, &out);
    G8_EQ(hr, S_OK);
    G8_EQ(m.r[0].addr, teb + 0x1758);          /* modulo 2^64, no overflow check */
    G8_EQ(m.r[1].addr, m.teb_ptr_value + 0x198);
}

/* ---- captured back trace ---------------------------------------------------- */

static int is64(USHORT mach) { return mach == M_AMD64 || mach == M_ARM64; }

static HRESULT __cdecl rd_bt(void *ctx, ULONG64 addr, UINT32 len, BYTE *buf)
{
    mem_t *m = ctx;
    if (m->n < 64) { m->r[m->n].addr = addr; m->r[m->n].len = len; }
    m->n++;
    if (!m->valid_base) return m->fail1 ? m->fail1 : G8_E_FAIL;
    if (len == 4 && addr == m->valid_base + m->off_sig) { memcpy(buf, &m->sig, 4); return S_OK; }
    if (len == 2 && addr == m->valid_base + m->off_ptr) { memcpy(buf, &m->ptrsize, 2); return S_OK; }
    if (len == 2 && addr == m->valid_base + m->off_frames) { memcpy(buf, &m->frames, 2); return S_OK; }
    if (len == 8 && addr == m->valid_base + m->off_bt) { memcpy(buf, &m->bt, 8); return S_OK; }
    return G8_E_FAIL;
}

static void init_bt(mem_t *m, USHORT mach)
{
    memset(m, 0, sizeof *m); m->self = m;
    m->sig = 0x10002; m->ptrsize = is64(mach) ? 8 : 4; m->frames = 23; m->bt = 0x00007ff612345670ull;
    if (is64(mach)) { m->off_sig = 0x80; m->off_ptr = 0x90; m->off_frames = 0x84; m->off_bt = 0x88; }
    else            { m->off_sig = 0x44; m->off_ptr = 0x50; m->off_frames = 0x48; m->off_bt = 0x4c; }
}

#define ERRADDR 0x0000020000100000ull

/* with a target that fails every read, the DLL probes nine candidate base addresses (errinfo - OFFSETS[i]) */
static int calibrate(USHORT mach, ULONG64 bases[9])
{
    mem_t m; UINT32 fr = 0x77; ULONG64 bt = 0x88; HRESULT hr; int i;
    init_bt(&m, mach);
    hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, mach, rd_bt, &m, &fr, &bt);
    G8_EQ(hr, G8_E_FAIL);
    G8_EQ(m.n, 9);
    G8_EQ(fr, 0x77); G8_EQ(bt, 0x88);
    if (m.n != 9) return 0;
    for (i = 0; i < 9; i++) {
        G8_EQ(m.r[i].len, 4);
        bases[i] = m.r[i].addr - m.off_sig;
        CHECKF(bases[i] <= ERRADDR, "probe %d base 0x%llx above error info address", i, bases[i]);
    }
    return 1;
}

static void bt_machine(USHORT mach)
{
    ULONG64 bases[9]; int k; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    if (!calibrate(mach, bases)) return;
    for (k = 0; k < 9; k++) {
        mem_t m; UINT32 fr = 0xffffffff; ULONG64 bt = 0; HRESULT hr;
        init_bt(&m, mach); m.valid_base = bases[k];
        hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, mach, rd_bt, &m, &fr, &bt);
        CHECKF(hr == S_OK, "machine 0x%x candidate %d hr=0x%lx", mach, k, hr);
        G8_EQ(fr, 23); G8_EQ(bt, m.bt);
        /* k failed probes (1 read each), then sig, ptrsize, frames, backtrace address */
        G8_EQ(m.n, k + 4);
        if (m.n == k + 4) {
            G8_EQ(m.r[k].addr, bases[k] + m.off_sig);     G8_EQ(m.r[k].len, 4);
            G8_EQ(m.r[k+1].addr, bases[k] + m.off_ptr);   G8_EQ(m.r[k+1].len, 2);
            G8_EQ(m.r[k+2].addr, bases[k] + m.off_frames); G8_EQ(m.r[k+2].len, 2);
            G8_EQ(m.r[k+3].addr, bases[k] + m.off_bt);    G8_EQ(m.r[k+3].len, 8);
        }
    }
}

TEST(gap8_ro_inspect_bt_amd64_probe_order_and_fields) { bt_machine(M_AMD64); }
TEST(gap8_ro_inspect_bt_arm64_probe_order_and_fields) { bt_machine(M_ARM64); }
TEST(gap8_ro_inspect_bt_i386_probe_order_and_fields)  { bt_machine(M_I386); }
TEST(gap8_ro_inspect_bt_armnt_probe_order_and_fields) { bt_machine(M_ARMNT); }
TEST(gap8_ro_inspect_bt_arm_probe_order_and_fields)   { bt_machine(M_ARM); }

TEST(gap8_ro_inspect_bt_candidate_tables_by_pointer_width)
{
    ULONG64 b64[9], b64b[9], b32[9], b32b[9]; int i; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    if (!calibrate(M_AMD64, b64) || !calibrate(M_ARM64, b64b) || !calibrate(M_I386, b32) || !calibrate(M_ARMNT, b32b)) return;
    for (i = 0; i < 9; i++) {
        G8_EQ(b64[i], b64b[i]);                       /* amd64 and arm64 share one table */
        G8_EQ(b32[i], b32b[i]);                       /* every other machine uses the other one */
    }
    t_note("64-bit candidate deltas:");
    for (i = 0; i < 9; i++) t_note("  [%d] errinfo - base = 0x%llx (32-bit table: 0x%llx)", i, ERRADDR - b64[i], ERRADDR - b32[i]);
    {   /* the unknown machine value takes the 32-bit table as well */
        mem_t m; UINT32 fr; ULONG64 bt; init_bt(&m, M_I386);
        UN(RoInspectCapturedStackBackTrace)(ERRADDR, 0x1234, rd_bt, &m, &fr, &bt);
        G8_EQ(m.n, 9);
        for (i = 0; i < 9 && i < m.n; i++) G8_EQ(m.r[i].addr - m.off_sig, b32[i]);
    }
}

TEST(gap8_ro_inspect_bt_wrong_signature_tries_every_candidate)
{
    static const UINT32 sigs[] = { 0, 1, 0x10001, 0x10003, 0x20002, 0xffffffff };
    ULONG64 bases[9]; unsigned s; int k; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    if (!calibrate(M_AMD64, bases)) return;
    for (s = 0; s < sizeof sigs / sizeof *sigs; s++) {
        mem_t m; UINT32 fr = 5; ULONG64 bt = 6; HRESULT hr;
        init_bt(&m, M_AMD64); m.valid_base = bases[8]; m.sig = sigs[s];
        hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, M_AMD64, rd_bt, &m, &fr, &bt);
        G8_EQ(hr, G8_E_INVALIDARG);                   /* the last candidate's verdict: the last candidate (8) held the bad signature; earlier ones fail to read */
        (void)k;
        G8_EQ(fr, 5); G8_EQ(bt, 6);
    }
}

TEST(gap8_ro_inspect_bt_wrong_pointer_size_rejected)
{
    static const struct { USHORT mach; UINT16 bad[3]; } t[] = {
        { M_AMD64, { 4, 0, 16 } }, { M_ARM64, { 4, 2, 0x108 } }, { M_I386, { 8, 0, 2 } }, { M_ARMNT, { 8, 1, 0 } }, { M_ARM, { 8, 0, 16 } },
    };
    ULONG64 bases[9]; unsigned i; int j; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    for (i = 0; i < sizeof t / sizeof *t; i++) {
        if (!calibrate(t[i].mach, bases)) continue;
        for (j = 0; j < 3; j++) {
            mem_t m; UINT32 fr = 5; ULONG64 bt = 6; HRESULT hr;
            init_bt(&m, t[i].mach); m.valid_base = bases[8]; m.ptrsize = t[i].bad[j];
            hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, t[i].mach, rd_bt, &m, &fr, &bt);
            CHECKF(hr == G8_E_INVALIDARG, "machine 0x%x ptrsize %u hr 0x%lx", t[i].mach, t[i].bad[j], hr);
            G8_EQ(fr, 5); G8_EQ(bt, 6);
        }
    }
}

TEST(gap8_ro_inspect_bt_unknown_machine_skips_pointer_size_validation)
{
    ULONG64 bases[9]; UINT16 sizes[] = { 0, 4, 8, 0x1234 }; unsigned i; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    if (!calibrate(M_I386, bases)) return;
    for (i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        mem_t m; UINT32 fr = 0; ULONG64 bt = 0; HRESULT hr;
        init_bt(&m, M_I386); m.valid_base = bases[0]; m.ptrsize = sizes[i];
        hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, 0x2222, rd_bt, &m, &fr, &bt);
        G8_EQ(hr, S_OK); G8_EQ(fr, 23); G8_EQ(bt, m.bt);
    }
}

TEST(gap8_ro_inspect_bt_frame_count_is_16_bit_zero_extended)
{
    static const UINT16 counts[] = { 0, 1, 0xffff, 0x8000, 62 };
    ULONG64 bases[9]; unsigned i; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    if (!calibrate(M_AMD64, bases)) return;
    for (i = 0; i < sizeof counts / sizeof *counts; i++) {
        mem_t m; UINT32 fr = 0xcccccccc; ULONG64 bt = 0; HRESULT hr;
        init_bt(&m, M_AMD64); m.valid_base = bases[0]; m.frames = counts[i];
        hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, M_AMD64, rd_bt, &m, &fr, &bt);
        G8_EQ(hr, S_OK); G8_EQ(fr, counts[i]);
    }
}

TEST(gap8_ro_inspect_bt_callback_error_is_returned_from_last_candidate)
{
    static const HRESULT codes[] = { G8_E_FAIL, G8_E_ACCESSDENIED, (HRESULT)0x8007001E };
    unsigned i; G8_REQ_NW(UN(RoInspectCapturedStackBackTrace));
    for (i = 0; i < sizeof codes / sizeof *codes; i++) {
        mem_t m; UINT32 fr = 1; ULONG64 bt = 2; HRESULT hr;
        init_bt(&m, M_AMD64); m.fail1 = codes[i];
        hr = UN(RoInspectCapturedStackBackTrace)(ERRADDR, M_AMD64, rd_bt, &m, &fr, &bt);
        G8_EQ(hr, codes[i]); G8_EQ(m.n, 9);
    }
}
