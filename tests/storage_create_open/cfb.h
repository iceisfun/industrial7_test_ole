/* Minimal [MS-CFB] reader/writer used to check on-disk output and to hand-craft input files. */
#ifndef CFB_H
#define CFB_H
#include "sco.h"

#define CFB_FREESECT   0xFFFFFFFFu
#define CFB_ENDOFCHAIN 0xFFFFFFFEu
#define CFB_FATSECT    0xFFFFFFFDu
#define CFB_DIFSECT    0xFFFFFFFCu
#define CFB_NOSTREAM   0xFFFFFFFFu

typedef struct {
    BYTE *b; LONG n; unsigned ssz;
    DWORD *fat; unsigned nfat;          /* entries */
    BYTE *dir; unsigned dirbytes;
    DWORD *mfat; unsigned nmfat;
    BYTE *mini; unsigned minilen;
    int ok;
} cfb_t;

static const BYTE *cfb_sec(cfb_t *c, DWORD sid) {
    LONGLONG off = ((LONGLONG)sid + 1) * c->ssz;
    if (sid >= 0xFFFFFFF0u || off + c->ssz > c->n) return NULL;
    return c->b + off;
}
/* follow a FAT chain; returns malloc'ed bytes (whole sectors), *len = bytes, -1 loop/invalid */
static BYTE *cfb_chain(cfb_t *c, DWORD start, unsigned *len) {
    unsigned cap = 0, cnt = 0; BYTE *out = NULL; DWORD s = start;
    *len = 0;
    while (s != CFB_ENDOFCHAIN) {
        const BYTE *p = cfb_sec(c, s);
        if (!p || cnt > 1000000 || s >= c->nfat) { free(out); *len = (unsigned)-1; return NULL; }
        if ((cnt + 1) * c->ssz > cap) { cap = (cnt + 16) * c->ssz; out = (BYTE *)realloc(out, cap); }
        memcpy(out + cnt * c->ssz, p, c->ssz); cnt++;
        s = c->fat[s];
    }
    *len = cnt * c->ssz;
    return out;
}
static unsigned cfb_chain_len(cfb_t *c, DWORD start) {
    unsigned cnt = 0; DWORD s = start;
    while (s != CFB_ENDOFCHAIN && cnt < 1000000) { if (s >= c->nfat) return (unsigned)-1; cnt++; s = c->fat[s]; }
    return cnt;
}
static int cfb_parse(cfb_t *c, BYTE *buf, LONG n) {
    memset(c, 0, sizeof *c);
    c->b = buf; c->n = n;
    if (n < 512 || memcmp(buf, sco_sig, 8)) return 0;
    c->ssz = 1u << sco_u16(buf + 0x1E);
    if (c->ssz != 512 && c->ssz != 4096) return 0;
    unsigned nfs = sco_u32(buf + 0x2C);
    DWORD difat_start = sco_u32(buf + 0x44); unsigned ndif = sco_u32(buf + 0x48);
    DWORD *fs = (DWORD *)malloc((nfs + 1) * sizeof(DWORD)); unsigned k = 0;
    for (unsigned i = 0; i < 109 && k < nfs; i++) fs[k++] = sco_u32(buf + 0x4C + 4 * i);
    DWORD d = difat_start;
    for (unsigned i = 0; i < ndif && k < nfs; i++) {
        const BYTE *p = cfb_sec(c, d); if (!p) { free(fs); return 0; }
        unsigned per = c->ssz / 4 - 1;
        for (unsigned j = 0; j < per && k < nfs; j++) fs[k++] = sco_u32(p + 4 * j);
        d = sco_u32(p + 4 * per);
    }
    if (k != nfs) { free(fs); return 0; }
    c->nfat = nfs * (c->ssz / 4);
    c->fat = (DWORD *)malloc(c->nfat * 4 + 4);
    for (unsigned i = 0; i < nfs; i++) {
        const BYTE *p = cfb_sec(c, fs[i]); if (!p) { free(fs); return 0; }
        for (unsigned j = 0; j < c->ssz / 4; j++) c->fat[i * (c->ssz / 4) + j] = sco_u32(p + 4 * j);
    }
    free(fs);
    c->dir = cfb_chain(c, sco_u32(buf + 0x30), &c->dirbytes);
    if (!c->dir) return 0;
    DWORD mf = sco_u32(buf + 0x3C);
    if (mf != CFB_ENDOFCHAIN) { BYTE *m = cfb_chain(c, mf, &c->nmfat); if (!m) return 0; c->nmfat /= 4; c->mfat = (DWORD *)m; }
    DWORD rs = sco_u32(c->dir + 0x74); unsigned rsz = sco_u32(c->dir + 0x78);
    if (rs != CFB_ENDOFCHAIN && rsz) { c->mini = cfb_chain(c, rs, &c->minilen); if (!c->mini) return 0; }
    c->ok = 1;
    return 1;
}
static void cfb_free(cfb_t *c) { free(c->fat); free(c->dir); free(c->mfat); free(c->mini); memset(c, 0, sizeof *c); }
static unsigned cfb_nentries(cfb_t *c) { return c->dirbytes / 128; }
static const BYTE *cfb_ent(cfb_t *c, unsigned i) { return c->dir + 128 * i; }
static void cfb_ent_name(cfb_t *c, unsigned i, WCHAR *out) {
    const BYTE *e = cfb_ent(c, i); unsigned l = sco_u16(e + 0x40);
    if (l > 64) l = 64;
    memcpy(out, e, l); if (l < 2) out[0] = 0; else if (out[l / 2 - 1] != 0) out[l / 2] = 0;
}
static int cfb_find(cfb_t *c, const WCHAR *name) {
    WCHAR n[33];
    for (unsigned i = 0; i < cfb_nentries(c); i++) {
        if (cfb_ent(c, i)[0x42] == 0) continue;
        cfb_ent_name(c, i, n);
        if (!lstrcmpW(n, name) && cfb_ent(c, i)[0x42] != 0) return (int)i;
    }
    return -1;
}
/* returns malloc'ed stream data of the entry's size, or NULL */
static BYTE *cfb_stream(cfb_t *c, unsigned i, ULONGLONG *sz) {
    const BYTE *e = cfb_ent(c, i);
    DWORD start = sco_u32(e + 0x74); ULONGLONG size = sco_u32(e + 0x78) | ((ULONGLONG)sco_u32(e + 0x7C) << 32);
    *sz = size;
    BYTE *out = (BYTE *)malloc((size_t)size + 1);
    if (size < 4096 && i != 0) {
        unsigned left = (unsigned)size, o = 0; DWORD s = start;
        while (left && s != CFB_ENDOFCHAIN) {
            if (s >= c->nmfat || (s + 1) * 64u > c->minilen) { free(out); return NULL; }
            unsigned take = left < 64 ? left : 64;
            memcpy(out + o, c->mini + s * 64u, take); o += take; left -= take; s = c->mfat[s];
        }
        if (left) { free(out); return NULL; }
    } else {
        unsigned len; BYTE *d = cfb_chain(c, start, &len);
        if (!d || len < size) { free(d); free(out); return NULL; }
        memcpy(out, d, (size_t)size); free(d);
    }
    return out;
}

/* ---- hand-crafted file builder ---------------------------------------------------------------- */
typedef struct { const WCHAR *name; BYTE type; DWORD left, right, child; DWORD start; ULONGLONG size; const GUID *clsid; DWORD bits; } cfb_ent_t;
static void cfb_put32(BYTE *p, DWORD v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); p[2] = (BYTE)(v >> 16); p[3] = (BYTE)(v >> 24); }
static void cfb_put16(BYTE *p, unsigned v) { p[0] = (BYTE)v; p[1] = (BYTE)(v >> 8); }
static void cfb_fill_ent(BYTE *e, const cfb_ent_t *s) {
    memset(e, 0, 128);
    if (s->name) { unsigned l = (unsigned)wcslen(s->name); memcpy(e, s->name, l * 2); cfb_put16(e + 0x40, (l + 1) * 2); }
    e[0x42] = s->type; e[0x43] = 1;
    cfb_put32(e + 0x44, s->left); cfb_put32(e + 0x48, s->right); cfb_put32(e + 0x4C, s->child);
    if (s->clsid) memcpy(e + 0x50, s->clsid, 16);
    cfb_put32(e + 0x60, s->bits);
    cfb_put32(e + 0x74, s->start); cfb_put32(e + 0x78, (DWORD)s->size); cfb_put32(e + 0x7C, (DWORD)(s->size >> 32));
}
/* Build a v3 file: header(512) + sector0 FAT + sector1 directory + extra sectors given by caller.
 * fat_extra[]: FAT entries for sectors 2..2+nextra-1 (in order). Returns malloc'ed buffer. */
static BYTE *cfb_build(LONG *outlen, const cfb_ent_t *ents, unsigned nents, DWORD minifat_start, unsigned nminifat,
                       const DWORD *fat_extra, unsigned nextra, const BYTE *extra_data) {
    LONG total = 512 * (2 + 1 + nextra);       /* header, FAT, dir, extras */
    BYTE *b = (BYTE *)calloc(1, total);
    memcpy(b, sco_sig, 8);
    cfb_put16(b + 0x18, 0x3E); cfb_put16(b + 0x1A, 3); cfb_put16(b + 0x1C, 0xFFFE);
    cfb_put16(b + 0x1E, 9); cfb_put16(b + 0x20, 6);
    cfb_put32(b + 0x2C, 1); cfb_put32(b + 0x30, 1);       /* 1 FAT sector, dir at sector 1 */
    cfb_put32(b + 0x38, 4096);
    cfb_put32(b + 0x3C, minifat_start); cfb_put32(b + 0x40, nminifat);
    cfb_put32(b + 0x44, CFB_ENDOFCHAIN); cfb_put32(b + 0x48, 0);
    for (int i = 0; i < 109; i++) cfb_put32(b + 0x4C + 4 * i, i == 0 ? 0 : CFB_FREESECT);
    BYTE *fat = b + 512 * 1;                              /* sector 0 */
    for (int i = 0; i < 128; i++) cfb_put32(fat + 4 * i, CFB_FREESECT);
    cfb_put32(fat + 0, CFB_FATSECT); cfb_put32(fat + 4, CFB_ENDOFCHAIN);
    for (unsigned i = 0; i < nextra; i++) cfb_put32(fat + 4 * (2 + i), fat_extra[i]);
    BYTE *dir = b + 512 * 2;                              /* sector 1 */
    for (int i = 0; i < 4; i++) { cfb_ent_t u = { NULL, 0, CFB_NOSTREAM, CFB_NOSTREAM, CFB_NOSTREAM, 0, 0, NULL, 0 }; cfb_fill_ent(dir + 128 * i, &u); dir[128 * i + 0x43] = 0; }
    for (unsigned i = 0; i < nents && i < 4; i++) cfb_fill_ent(dir + 128 * i, &ents[i]);
    if (nextra) memcpy(b + 512 * 3, extra_data, 512 * nextra);
    *outlen = total;
    return b;
}
#endif
