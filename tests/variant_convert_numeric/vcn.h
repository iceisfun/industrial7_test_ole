/* Shared helpers for the variant_convert_numeric area. */
#ifndef VCN_H
#define VCN_H
#include "t.h"
#include <oleauto.h>
#include <math.h>

struct vdec { int scale; int sign; ULONG hi, mid, lo; };

static inline void vcn_mkdec(DECIMAL *d, const struct vdec *s)
{
    memset(d, 0, sizeof *d);
    d->scale = (BYTE)s->scale;
    d->sign = (BYTE)s->sign;
    d->Hi32 = s->hi;
    d->Mid32 = s->mid;
    d->Lo32 = s->lo;
}

/* soft rows only produce a note (unverified detail); hard rows are CHECKs */
#define VCN_C(soft, cond, ...) do { if (soft) { if (!(cond)) t_note(__VA_ARGS__); } else CHECKF(cond, __VA_ARGS__); } while (0)

#define VCN_HR(fn, i, soft, hr, want, d) \
    VCN_C(soft, (hr) == (want), "%s row %u in=%s: hr 0x%08lx want 0x%08lx", fn, i, d, (unsigned long)(hr), (unsigned long)(want))
#define VCN_INT(fn, i, soft, got, want, d) \
    VCN_C(soft, (long long)(got) == (long long)(want), "%s row %u in=%s: got %lld want %lld", fn, i, d, (long long)(got), (long long)(want))
#define VCN_UINT(fn, i, soft, got, want, d) \
    VCN_C(soft, (unsigned long long)(got) == (unsigned long long)(want), "%s row %u in=%s: got %llu want %llu", fn, i, d, (unsigned long long)(got), (unsigned long long)(want))
#define VCN_CY(fn, i, soft, got, want, d) \
    VCN_C(soft, (long long)(got) == (long long)(want), "%s row %u in=%s: cy got %lld want %lld", fn, i, d, (long long)(got), (long long)(want))
#define VCN_FLT(fn, i, soft, got, want, d) \
    VCN_C(soft, (float)(got) == (float)(want), "%s row %u in=%s: got %.9g want %.9g", fn, i, d, (double)(got), (double)(want))
#define VCN_DBL(fn, i, soft, got, want, d) \
    VCN_C(soft, (double)(got) == (double)(want), "%s row %u in=%s: got %.17g want %.17g", fn, i, d, (double)(got), (double)(want))

#define VCN_NEAR(fn, i, soft, got, want, tol, d) \
    VCN_C(soft, ((double)(want) == 0.0 ? (double)(got) == 0.0 : fabs((double)(got) - (double)(want)) <= (tol) * fabs((double)(want))), \
          "%s row %u in=%s: got %.17g want %.17g", fn, i, d, (double)(got), (double)(want))

static inline int vcn_deceq(const DECIMAL *a, const struct vdec *w)
{
    return a->scale == (BYTE)w->scale && a->sign == (BYTE)w->sign && a->Hi32 == w->hi &&
           a->Mid32 == w->mid && a->Lo32 == w->lo;
}
#define VCN_DEC(fn, i, soft, pgot, pwant, d) \
    VCN_C(soft, vcn_deceq(pgot, pwant), "%s row %u in=%s: dec got sc=%u sg=%02x %08lx:%08lx:%08lx want sc=%d sg=%02x %08lx:%08lx:%08lx", \
          fn, i, d, (pgot)->scale, (pgot)->sign, (unsigned long)(pgot)->Hi32, (unsigned long)(pgot)->Mid32, (unsigned long)(pgot)->Lo32, \
          (pwant)->scale, (pwant)->sign, (unsigned long)(pwant)->hi, (unsigned long)(pwant)->mid, (unsigned long)(pwant)->lo)

static inline int vcn_untouched(const void *p, size_t n)
{
    const unsigned char *b = p;
    for (size_t k = 0; k < n; k++) if (b[k] != 0xCC) return 0;
    return 1;
}
#define VCN_UNTOUCHED(fn, i, soft, p, n, d) \
    VCN_C(soft, vcn_untouched(p, n), "%s row %u in=%s: output written on failure", fn, i, d)

#define VCN_ROWS(a) ((unsigned)(sizeof(a) / sizeof((a)[0])))
#endif
