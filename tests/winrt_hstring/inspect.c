/* WindowsInspectString / WindowsInspectString2: the debugger-support readers. The callback is handed a
 * target address and a size (0x18 for 64-bit targets, 0x14 for 32-bit ones) and must copy that much of the
 * HSTRING header; we are our own "target", so it is a plain memcpy. Derived from the combase decomp. */
#include "hs.h"

struct insp_ctx { int calls; UINT_PTR addr; UINT32 size; HRESULT ret; };

static HRESULT WINAPI insp_cb(void *context, UINT_PTR readAddress, UINT32 length, BYTE *buffer)
{
    struct insp_ctx *c = context;
    c->calls++; c->addr = readAddress; c->size = length;
    if (c->ret < 0) return c->ret;
    memcpy(buffer, (const void *)readAddress, length);
    return S_OK;
}

UDECL(HRESULT, WindowsInspectString2, (UINT_PTR, USHORT, PINSPECT_HSTRING_CALLBACK, void *, UINT32 *, UINT_PTR *));

#define REQUIRE_INSP() do { REQUIRE(U(WindowsInspectString)); if (t_is_wine()) { t_skip("wine does not implement WindowsInspectString"); return; } } while (0)
#define REQUIRE_INSP2() do { REQUIRE_INSP(); REQUIRE(UN(WindowsInspectString2)); } while (0)
#define MACH_X64 0x8664
#define MACH_X86 0x014c

static void insp_run(HSTRING h, int variant2, USHORT machine, struct insp_ctx *c, HRESULT *hr, UINT32 *len, UINT_PTR *addr)
{
    UINT_PTR a = 0x1111; UINT32 l = 0x2222;
    if (variant2) *hr = UN(WindowsInspectString2)((UINT_PTR)h, machine, insp_cb, c, &l, &a);
    else *hr = U(WindowsInspectString)((UINT_PTR)h, machine, insp_cb, c, &l, &a);
    *len = l; *addr = a;
}

TEST(winrt_hstring_inspect_null_handle_is_empty_without_callback)
{
    int v; REQUIRE_HS(); REQUIRE_INSP2();
    for (v = 0; v < 2; v++) {
        struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
        insp_run(NULL, v, MACH_X64, &c, &hr, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(l, 0);
        EXPECT_EQ(a, 0);
        EXPECT_EQ(c.calls, 0);
        /* the machine type is not looked at for a NULL handle */
        insp_run(NULL, v, 0x1234, &c, &hr, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(c.calls, 0);
    }
}

TEST(winrt_hstring_inspect_unknown_machine_is_invalidarg)
{
    static const USHORT bad[] = { 0, 1, 0x1234, 0x0200, 0x014d, 0x8665, 0xffff };
    HSTRING h = hs_makez(L"abc"); unsigned i; int v; REQUIRE_HS(); REQUIRE_INSP2();
    for (v = 0; v < 2; v++)
        for (i = 0; i < ARRAYSIZE(bad); i++) {
            struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
            insp_run(h, v, bad[i], &c, &hr, &l, &a);
            t_check(hr == HS_E_INVALIDARG, __FILE__, __LINE__, "bad machine", "machine 0x%x hr 0x%08lx", bad[i], (unsigned long)hr);
            EXPECT_EQ(c.calls, 0);
            EXPECT_EQ(l, 0);                 /* outputs are zeroed up front */
            EXPECT_EQ(a, 0);
        }
    WDEL(h);
}

TEST(winrt_hstring_inspect_x64_heap_string)
{
    static const WCHAR s[] = L"inspect me";
    HSTRING h = hs_makez(s); int v; REQUIRE_HS(); REQUIRE_INSP2();
    for (v = 0; v < 2; v++) {
        struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
        insp_run(h, v, MACH_X64, &c, &hr, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(c.calls, 1);
        EXPECT_EQ(c.addr, (UINT_PTR)h);       /* reads the header at the HSTRING itself */
        EXPECT_EQ(c.size, 0x18);
        EXPECT_EQ(l, 10);
        EXPECT_EQ(a, (UINT_PTR)WRAW(h, NULL));
    }
    WDEL(h);
}

TEST(winrt_hstring_inspect_x64_reference_string)
{
    static const WCHAR s[] = L"by reference";
    HSTRING_HEADER hdr; HSTRING r; int v; REQUIRE_HS(); REQUIRE_INSP2();
    EXPECT_EQ(WCSR(s, 12, &hdr, &r), S_OK);
    for (v = 0; v < 2; v++) {
        struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
        insp_run(r, v, MACH_X64, &c, &hr, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(c.calls, 1);
        EXPECT_EQ(l, 12);
        EXPECT_EQ(a, (UINT_PTR)s);
    }
}

TEST(winrt_hstring_inspect_x86_machine_reads_0x14_and_truncates_address)
{
    static const USHORT machines[] = { 0x014c, 0x01c0, 0x01c4 };   /* I386, ARM, ARMNT */
    HSTRING h = hs_makez(L"thirty-two bit view"); unsigned i; REQUIRE_HS(); REQUIRE_INSP();
    for (i = 0; i < ARRAYSIZE(machines); i++) {
        struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
        insp_run(h, 0, machines[i], &c, &hr, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(c.calls, 1);
        EXPECT_EQ(c.size, 0x14);
        EXPECT_EQ(l, 19);
        /* the address field is a 32-bit pointer at offset 0x10 of the 32-bit header; only the low half of our
         * 64-bit pointer is visible, and the result has its high half cleared */
        EXPECT_EQ(a, (UINT_PTR)(UINT32)(ULONG_PTR)WRAW(h, NULL));
    }
    WDEL(h);
}

TEST(winrt_hstring_inspect_arm64_machine_uses_64bit_layout)
{
    HSTRING h = hs_makez(L"arm64"); struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
    REQUIRE_HS(); REQUIRE_INSP();
    insp_run(h, 0, 0xaa64, &c, &hr, &l, &a);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(c.size, 0x18);
    EXPECT_EQ(l, 5);
    EXPECT_EQ(a, (UINT_PTR)WRAW(h, NULL));
    WDEL(h);
}

TEST(winrt_hstring_inspect_callback_failure_is_returned)
{
    static const HRESULT errs[] = { E_FAIL, E_ACCESSDENIED, (HRESULT)0x8007001e };
    HSTRING h = hs_makez(L"x"); unsigned i; int v; REQUIRE_HS(); REQUIRE_INSP2();
    for (v = 0; v < 2; v++)
        for (i = 0; i < ARRAYSIZE(errs); i++) {
            struct insp_ctx c = { 0, 0, 0, errs[i] }; HRESULT hr; UINT32 l; UINT_PTR a;
            insp_run(h, v, MACH_X64, &c, &hr, &l, &a);
            EXPECT_EQ(hr, errs[i]);
            EXPECT_EQ(c.calls, 1);
            EXPECT_EQ(l, 0);
            EXPECT_EQ(a, 0);
        }
    WDEL(h);
}

TEST(winrt_hstring_inspect_header_with_reserved_flag_bits_is_invalidarg)
{
    /* a callback that serves a forged header: flags must be 0/1 style (no bits in 0x7fffffe0) unless it
     * carries the string-buffer magic */
    struct forged { UINT32 flags; UINT32 len; UINT64 pad; UINT64 ptr; };
    static struct forged f;
    static const UINT32 flags[] = { 0x20, 0x40, 0x100, 0x7fffffe0, 0x00010000 };
    unsigned i; REQUIRE_HS(); REQUIRE_INSP();
    for (i = 0; i < ARRAYSIZE(flags); i++) {
        struct insp_ctx c = { 0, 0, 0, S_OK }; UINT32 l = 9; UINT_PTR a = 9; HRESULT hr;
        f.flags = flags[i]; f.len = 3; f.ptr = 0x1000;
        hr = U(WindowsInspectString)((UINT_PTR)&f, MACH_X64, insp_cb, &c, &l, &a);
        t_check(hr == HS_E_INVALIDARG, __FILE__, __LINE__, "forged flags rejected", "flags 0x%x hr 0x%08lx", flags[i], (unsigned long)hr);
        EXPECT_EQ(c.calls, 1);
        EXPECT_EQ(l, 0);
        EXPECT_EQ(a, 0);
    }
    /* low flag bits (reference / has-embedded-null) are fine */
    {
        struct insp_ctx c = { 0, 0, 0, S_OK }; UINT32 l = 9; UINT_PTR a = 9; HRESULT hr;
        f.flags = 3; f.len = 3; f.ptr = 0x1000;
        hr = U(WindowsInspectString)((UINT_PTR)&f, MACH_X64, insp_cb, &c, &l, &a);
        EXPECT_EQ(hr, S_OK);
        EXPECT_EQ(l, 3);
        EXPECT_EQ(a, 0x1000);
    }
}

TEST(winrt_hstring_inspect_preallocated_buffer_handle_is_accepted)
{
    /* the string-buffer magic in the flags word is the one case where high flag bits are allowed */
    HSTRING_BUFFER b = NULL; WCHAR *buf = NULL; struct insp_ctx c = { 0, 0, 0, S_OK }; HRESULT hr; UINT32 l; UINT_PTR a;
    REQUIRE_HS_BUF(); REQUIRE_INSP();
    if (t_is_wine()) { t_skip("wine buffer handles carry no magic"); return; }
    EXPECT_EQ(WPRE(6, &buf, &b), S_OK);
    if (!b) return;
    insp_run((HSTRING)b, 0, MACH_X64, &c, &hr, &l, &a);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(l, 6);
    EXPECT_EQ(a, (UINT_PTR)buf);
    EXPECT_EQ(WDELB(b), S_OK);
}
