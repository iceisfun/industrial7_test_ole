#include "gap8.h"
DWORD g8_run_thread(g8_thread_fn fn, void *arg, DWORD ms)
{
    HANDLE h = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!h) return 0xDEAD0000;
    DWORD w = WaitForSingleObject(h, ms), code = 0xDEAD0001;
    if (w == WAIT_OBJECT_0) GetExitCodeThread(h, &code); else TerminateThread(h, 0xDEAD0001);
    CloseHandle(h);
    return code;
}

typedef struct { unsigned h[5]; unsigned long long len; unsigned char buf[64]; size_t nb; } sha1_ctx;
#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
static void sha1_block(sha1_ctx *c, const unsigned char *p)
{
    unsigned w[80], a, b, d, e, f, k, t, cc; int i;
    for (i = 0; i < 16; i++) w[i] = (unsigned)p[4*i] << 24 | p[4*i+1] << 16 | p[4*i+2] << 8 | p[4*i+3];
    for (i = 16; i < 80; i++) w[i] = ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4];
    for (i = 0; i < 80; i++) {
        if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
        t = ROL(a, 5) + f + e + k + w[i]; e = d; d = cc; cc = ROL(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}
void g8_sha1(const unsigned char *data, size_t n, unsigned char out[20])
{
    sha1_ctx c; size_t i; unsigned char pad[128]; size_t padn; unsigned long long bits = (unsigned long long)n * 8;
    c.h[0] = 0x67452301; c.h[1] = 0xEFCDAB89; c.h[2] = 0x98BADCFE; c.h[3] = 0x10325476; c.h[4] = 0xC3D2E1F0;
    for (i = 0; i + 64 <= n; i += 64) sha1_block(&c, data + i);
    padn = n - i; memcpy(pad, data + i, padn); pad[padn++] = 0x80;
    while ((padn % 64) != 56) pad[padn++] = 0;
    for (i = 0; i < 8; i++) pad[padn++] = (unsigned char)(bits >> (56 - 8 * i));
    for (i = 0; i < padn; i += 64) sha1_block(&c, pad + i);
    for (i = 0; i < 5; i++) { out[4*i] = c.h[i] >> 24; out[4*i+1] = c.h[i] >> 16; out[4*i+2] = c.h[i] >> 8; out[4*i+3] = c.h[i]; }
}
void g8_signature_iid(const char *sig, GUID *out)
{
    /* namespace 11f47ad5-7b73-42c0-abae-878b1e16adee, big-endian field order */
    static const unsigned char ns[16] = { 0x11,0xf4,0x7a,0xd5, 0x7b,0x73, 0x42,0xc0, 0xab,0xae, 0x87,0x8b,0x1e,0x16,0xad,0xee };
    unsigned char in[16 + 512], h[20]; size_t n = strlen(sig);
    memcpy(in, ns, 16); memcpy(in + 16, sig, n);
    g8_sha1(in, 16 + n, h);
    h[6] = (h[6] & 0x0f) | 0x50; h[8] = (h[8] & 0x3f) | 0x80;
    out->Data1 = (unsigned)h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];
    out->Data2 = (unsigned short)(h[4] << 8 | h[5]);
    out->Data3 = (unsigned short)(h[6] << 8 | h[7]);
    memcpy(out->Data4, h + 8, 8);
}
void g8_guid_str(const GUID *g, char *o)
{
    sprintf(o, "{%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", g->Data1, g->Data2, g->Data3, g->Data4[0], g->Data4[1],
            g->Data4[2], g->Data4[3], g->Data4[4], g->Data4[5], g->Data4[6], g->Data4[7]);
}
