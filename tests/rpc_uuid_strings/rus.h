/* Shared helpers for the rpc_uuid_strings area (rpcrt4 UUID / string-binding / binding-handle helpers).
 * Every rpcrt4 entry point is reached through LoadLibrary + GetProcAddress (U / UN); nothing is linked. */
#ifndef RUS_H
#define RUS_H
#include "t.h"
#include <rpc.h>
#include <rpcdcep.h>

#ifndef RPC_S_UUID_LOCAL_ONLY
#define RPC_S_UUID_LOCAL_ONLY 1824L
#endif

/* RPC_STATUS values, spelled out so that the expectations are visible at the call site */
#define RUS_OK                    0UL
#define RUS_OUTOFMEMORY           14UL    /* ERROR_OUTOFMEMORY */
#define RUS_INVALID_PARAMETER     87UL    /* ERROR_INVALID_PARAMETER */
#define RUS_INVALID_STRING_BINDING 1700UL /* 0x6a4 */
#define RUS_WRONG_KIND_OF_BINDING  1701UL
#define RUS_INVALID_BINDING        1702UL /* 0x6a6 */
#define RUS_PROTSEQ_NOT_SUPPORTED  1703UL /* 0x6a7 */
#define RUS_INVALID_RPC_PROTSEQ    1704UL /* 0x6a8 */
#define RUS_INVALID_STRING_UUID    1705UL /* 0x6a9 */
#define RUS_INVALID_ENDPOINT_FORMAT 1706UL /* 0x6aa */
#define RUS_INVALID_NET_ADDR       1707UL /* 0x6ab */
#define RUS_INVALID_TIMEOUT        1709UL /* 0x6ad */
#define RUS_NO_CALL_ACTIVE         1725UL /* 0x6bd */
#define RUS_UUID_LOCAL_ONLY        1824UL /* 0x720 */
#define RUS_ALREADY_LISTENING      1713UL /* 0x6b1 */

/* resolve a typed function pointer or skip the test */
#define NEED(name) __typeof__(&name) p_##name = U(name); REQUIRE(p_##name)

#define NSTR(a) ((int)(sizeof(a) / sizeof((a)[0])))

static inline int rus_uuid_eq(const UUID *a, const UUID *b) { return memcmp(a, b, sizeof(UUID)) == 0; }

static inline void rus_uuid_set(UUID *u, unsigned long d1, unsigned short d2, unsigned short d3,
                                int b0, int b1, int b2, int b3, int b4, int b5, int b6, int b7)
{
    u->Data1 = d1; u->Data2 = d2; u->Data3 = d3;
    u->Data4[0] = (unsigned char)b0; u->Data4[1] = (unsigned char)b1; u->Data4[2] = (unsigned char)b2;
    u->Data4[3] = (unsigned char)b3; u->Data4[4] = (unsigned char)b4; u->Data4[5] = (unsigned char)b5;
    u->Data4[6] = (unsigned char)b6; u->Data4[7] = (unsigned char)b7;
}

/* tiny deterministic generator (xorshift) for table-style round trips */
static inline unsigned rus_rand(unsigned *s)
{
    unsigned x = *s ? *s : 0x1234567u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

/* narrow <-> wide helpers for ASCII test strings */
static inline void rus_widen(WCHAR *dst, size_t cap, const char *src)
{
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) dst[i] = (unsigned char)src[i];
    dst[i] = 0;
}
static inline int rus_wide_eq_ascii(const WCHAR *w, const char *a)
{
    size_t i = 0;
    if (!w) return a == NULL;
    for (; a[i]; i++) if (w[i] != (unsigned char)a[i]) return 0;
    return w[i] == 0;
}
#endif
