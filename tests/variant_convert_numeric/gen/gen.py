#!/usr/bin/env python3
"""Generates conv_*.c / str_*.c table tests for tests/variant_convert_numeric.

Usage: python3 gen.py   (run from this directory; writes ../conv_*.c, ../str_*.c)
The expectations come from model.py, which mirrors the Windows 10 oleaut32 decompilation.
"""
import math, struct, os, sys
from decimal import Decimal, ROUND_HALF_EVEN, getcontext
from fractions import Fraction
import model as M

getcontext().prec = 80
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")

CTYPE = {"I1": "CHAR", "UI1": "BYTE", "I2": "SHORT", "UI2": "USHORT", "I4": "LONG", "UI4": "ULONG",
         "I8": "LONG64", "UI8": "ULONG64", "R4": "float", "R8": "double", "Date": "DATE",
         "Cy": "LONGLONG", "Bool": "VARIANT_BOOL", "Dec": "struct vdec"}
OUTVAR = dict(CTYPE, Cy="CY", Dec="DECIMAL")
DSTS = ["I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8", "R4", "R8", "Cy", "Date", "Bool", "Dec"]
SRCS = ["I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8", "R4", "R8", "Cy", "Date", "Bool", "Dec"]
names = set(open(os.path.join(OUT, "gen", "names.txt")).read().split())
HRC = {"ok": "S_OK", M.OVF: "DISP_E_OVERFLOW", M.INV: "E_INVALIDARG", "INV": "E_INVALIDARG", "TM": "DISP_E_TYPEMISMATCH"}


# ------------------------------------------------------------------ emitters
def cint(v, ct):
    if ct in ("ULONG64",):
        return "%dULL" % v
    if ct == "LONG64":
        return "(-9223372036854775807LL-1)" if v == -2**63 else "%dLL" % v
    if ct == "LONGLONG":
        return "(-9223372036854775807LL-1)" if v == -2**63 else "%dLL" % v
    if ct == "ULONG":
        return "%dUL" % v
    if ct == "LONG":
        return "(-2147483647-1)" if v == -2**31 else "%d" % v
    return "%d" % v


def cflt(x, single):
    if math.isnan(x):
        return "NAN"
    if math.isinf(x):
        return "INFINITY" if x > 0 else "(-INFINITY)"
    h = float(x).hex()
    return h + ("f" if single else "")


def cval(kind, v):
    if kind == "R4":
        return cflt(v, True)
    if kind in ("R8", "Date"):
        return cflt(v, False)
    if kind == "Dec":
        return "{%d,0x%x,%dUL,%dUL,%dUL}" % v
    return cint(v, CTYPE[kind])


def desc(kind, v):
    if kind == "Dec":
        return "dec(sc=%d,sg=0x%x,%x:%x:%x)" % v
    if kind in ("R4", "R8", "Date"):
        return "%r" % v if not (math.isnan(v) or math.isinf(v)) else str(v)
    return str(v)


def outcmp(kind):
    return {"R4": "VCN_FLT", "R8": "VCN_DBL", "Date": "VCN_DBL", "Cy": "VCN_CY", "Dec": "VCN_DEC"}.get(kind) or (
        "VCN_UINT" if kind.startswith("UI") else "VCN_INT")


# --------------------------------------------------------------- domains
def nxt(x, up=True):
    return math.nextafter(x, math.inf if up else -math.inf)


def nxt32(x, up=True):
    b = struct.unpack('<I', struct.pack('<f', x))[0]
    if x == 0:
        return struct.unpack('<f', struct.pack('<I', 1 if up else 0x80000001))[0]
    if (x > 0) == up:
        b += 1
    else:
        b -= 1
    return struct.unpack('<f', struct.pack('<I', b & 0xffffffff))[0]


INT_BOUNDS = [127, 128, 255, 256, 32767, 32768, 65535, 65536, 2**31 - 1, 2**31, 2**32 - 1, 2**32,
              2**53, 2**63 - 1, 2**63, 2**64 - 1, 10, 100, 200, 254, 253]


def int_domain(src):
    lo, hi = M.INT_RANGE[src]
    s = {0, 1, 2, 3, -1, -2, 7, 99, 100, lo, hi, lo + 1, hi - 1}
    for b in INT_BOUNDS:
        for d in (-1, 0, 1):
            for sg in (1, -1):
                v = sg * b + d
                if lo <= v <= hi:
                    s.add(v)
    s = sorted(v for v in s if lo <= v <= hi)
    if src in ("I8", "UI8", "UI4", "I4"):
        # keep only what is exactly representable in float32 for R4 dest rows (handled in filter)
        pass
    return s


def real_domain(single):
    f = M.f32 if single else (lambda x: x)
    nx = nxt32 if single else nxt
    s = set()
    base = [0.0, -0.0, 0.1, -0.1, 0.25, -0.25, 0.4, -0.4, 0.5, -0.5, 0.6, -0.6, 0.75, 1.0, -1.0, 1.25,
            1.5, -1.5, 1.75, 2.0, 2.5, -2.5, 3.5, -3.5, 4.5, 5.5, 6.5, 7.5, 10.0, 99.5, 100.5, 126.5, 127.0,
            -127.5, -126.5, 128.0, 129.5, 254.0, 255.0, 256.0, 1000.5, 1e5, 1e10, -1e10, 1e15, 1e19, 1e20, 1e30,
            1e38, 3.5e38, 1e300, 1e-5, 1e-10, -1e-10, 1e-30, 5e-324, 123456.789, -123456.789,
            0.49999999, 0.50000001, 1.49999999, 1.50000001, 2.49999999, 2.50000001,
            math.inf, -math.inf, 2.0**24, 2.0**24 + 2, 2.0**31, -2.0**31, 2.0**32, 2.0**53, 2.0**63, -2.0**63,
            2.0**64, -2.0**64]
    for x in base:
        s.add(f(x))
    for x in (2.0**63, -2.0**63, 2.0**62, -2.0**62):   # I8 float bound is 2^62 on Win10; keep the 2^63 neighbourhood too
        s.update([f(x), nx(f(x), True), nx(f(x), False)])
    for dst, (lo, hi) in M.FBOUND.items():
        for b in (lo, hi):
            fb = f(b)
            s.add(fb)
            s.add(nx(fb, True))
            s.add(nx(fb, False))
            if abs(fb) < 1e7:
                s.add(f(fb + 1))
                s.add(f(fb - 1))
    # date and currency bounds
    for b in (M.DATE_LO, M.DATE_HI, M.DATE_LO + 1, M.DATE_HI - 1):
        fb = f(b)
        s.update([fb, nx(fb, True), nx(fb, False)])
    s.update([f(M.CY_BND), nx(f(M.CY_BND), True), nx(f(M.CY_BND), False), f(-M.CY_BND), nx(f(-M.CY_BND), True),
              nx(f(-M.CY_BND), False), f(922337203685477.5), f(-922337203685477.5), f(922337203685477.0)])
    s.update([f(M.FLT_MAX), f(-M.FLT_MAX), f(3.4028236e38), f(-3.4028236e38), f(1e-40), f(1e-45), f(-1e-40)])
    # exact currency ties (k/32 * 10000 is a half)
    s.update([f(1 / 32), f(3 / 32), f(5 / 32), f(-1 / 32), f(-3 / 32), f(7 / 32), f(1.03125), f(0.0625), f(0.125), f(0.00390625)])
    out = []
    for x in s:
        if single:
            try:
                x = M.f32(x)
            except OverflowError:
                continue
            if math.isinf(x) and x not in (math.inf, -math.inf):
                continue
        out.append(x)
    return sorted(set(out), key=lambda v: (math.copysign(1, v) if v == 0 else 0, v))


def cy_domain():
    s = {0, 1, -1, 4999, 5000, 5001, -4999, -5000, -5001, 9999, 10000, -10000, 10001, 15000, -15000, 25000, -25000,
         35000, 45000, 12345, -12345, 123456789, 2**63 - 1, -2**63, -2**63 + 1, 922337203685477 * 10000,
         9223372036854770000, 9223372036854775807, 1500, 2500, 5, 50, 500, 2**31, 2**32, -2**32}
    for X2 in (255, 254, 127, 126, 128, 32767, 32766, 32768, 65535, 65534, 65536, 2147483647, 2147483646, 2147483648,
               4294967295, 4294967294, 4294967296, 100, 0):
        for h in (0, 5000, 4999, 5001, 1):
            for sg in (1, -1):
                v = sg * (X2 * 10000 + h)
                if -2**63 <= v < 2**63:
                    s.add(v)
    s.add(-5000 - 128 * 10000)
    s.add(-(128 * 10000 + 5000) - 1)
    s.add(-(32768 * 10000 + 5000))
    s.add(-(2147483648 * 10000 + 5000))
    s.add(-(2147483648 * 10000 + 5001))
    return sorted(s)


def bool_domain():
    return [0, -1, 1, 2, -2, 255, 256, 0x7fff, -32768, 0x100, -256, 0x4000]


def dec_domain():
    mags = [0, 1, 5, 9, 10, 15, 25, 99, 100, 127, 128, 129, 255, 256, 32767, 32768, 65535, 65536,
            2**31 - 1, 2**31, 2**32 - 1, 2**32, 2**32 + 1, 2**63 - 1, 2**63, 2**63 + 1, 2**64 - 1, 2**64, 2**95, 2**96 - 1]
    out = []

    def mk(sc, sg, m):
        return (sc, sg, (m >> 64) & 0xffffffff, (m >> 32) & 0xffffffff, m & 0xffffffff)
    for sg in (0, 0x80):
        for m in mags:
            out.append(mk(0, sg, m))
    # fractional scales with .5 / near-.5 neighbours
    halves = [5, 15, 25, 35, 45, 1275, 1285, 1265, 2555, 2545, 327675, 327685, 327665, 655355, 655365, 655345,
              21474836475, 21474836485, 21474836465, 42949672955, 42949672965, 42949672945, 4, 6, 14, 16, 1, 0, 9, 99, 1999, 1234, 5000]
    for sc in (1, 2, 4, 5, 10):
        for sg in (0, 0x80):
            for h in halves:
                v = h * 10 ** (sc - 1)
                out.append(mk(sc, sg, v))
    for sg in (0, 0x80):
        out.append(mk(28, sg, 1))
        out.append(mk(28, sg, 5 * 10 ** 27))
        out.append(mk(28, sg, 10 ** 28))
        out.append(mk(28, sg, 2**96 - 1))
        out.append(mk(12, sg, 123456789012345678))
        out.append(mk(3, sg, 2**64 + 5))
        out.append(mk(1, sg, 2**96 - 1))
        out.append(mk(22, sg, 12345678901234567890))
    # invalid forms
    out += [(29, 0, 0, 0, 0), (29, 0, 0, 0, 1), (255, 0, 0, 0, 1), (0, 1, 0, 0, 1), (0, 0x40, 0, 0, 1),
            (0, 0x7f, 0, 0, 0), (0, 0x81, 0, 0, 1), (0, 0xff, 0, 0, 1), (30, 0x80, 0, 0, 5), (4, 0x20, 0, 0, 5)]
    # negative zero / zero with scale
    out += [(0, 0x80, 0, 0, 0), (5, 0x80, 0, 0, 0), (28, 0, 0, 0, 0)]
    seen = []
    for d in out:
        if d not in seen:
            seen.append(d)
    return seen


DOM = {}
for s_ in ("I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8"):
    DOM[s_] = int_domain(s_)
DOM["R4"] = real_domain(True)
DOM["R8"] = real_domain(False)
DOM["Date"] = DOM["R8"]
DOM["Cy"] = cy_domain()
DOM["Bool"] = bool_domain()
DOM["Dec"] = dec_domain()

# values whose result depends on things we could not verify -> soft rows
def is_soft(dst, src, v):
    # decimals that need pow10 beyond 10^22 go through fnDblPower10: exactness not verified
    if src == "Dec" and v[0] > 22 and dst in M.INT_RANGE + ["R4", "R8", "Date"] + [] if False else False:
        return True
    return False


def soft_row(dst, src, v, res):
    # Dec from R4 beyond 1e7 takes the multi-word multiply path (not ported 1:1)
    if dst == "Dec" and src == "R4" and abs(v) >= 1e7 and not math.isinf(v):
        return True
    # VarCyFromDec with scale >= 14 uses a staged sticky-bit division
    if dst == "Cy" and src == "Dec" and v[0] >= 14:
        return True
    # VarI8FromCy takes the remainder from the low 32 bits: positive values >= 2^32 are decomp-literal only
    if dst == "I8" and src == "Cy" and v >= 2**32:
        return True
    # Dec with scale > 0 and a magnitude beyond 2^53 goes through inexact double arithmetic
    if src == "Dec" and v[0] > 0 and M.dec_valid(v) and M.dec_mag(v) >= 2**53 and dst not in ("Cy", "Dec", "Bool"):
        return True
    # Dec scale > 22 reach double arithmetic with fnDblPower10 (not checked bit-exact)
    if src == "Dec" and v[0] > 22 and dst in ("R4", "R8", "Date", "I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8"):
        return True
    # float32 conversions of big int64 can differ by double rounding
    if dst == "R4" and src in ("I8", "UI8", "UI4", "I4") and abs(v) > 2**24:
        return True
    if dst == "R4" and src == "Cy":
        return abs(v) > 2**50
    # near FLT_MAX boundary: exact constant _dsR4Max not visible in the decompilation
    if dst == "R4" and src in ("R8", "Date") and isinstance(v, float) and not math.isinf(v):
        if M.FLT_MAX < abs(v) < 3.4028236e38:
            return True
    # Dec from big values: pow10 table exactness
    if dst == "Dec" and src in ("R8", "Date", "R4") and isinstance(v, float) and abs(v) >= 1e15 and abs(v) != math.inf:
        return True
    return False


def gen_table(dst, src):
    fn = "Var%sFrom%s" % (dst, src)
    if fn not in names:
        return None
    rows = []
    for v in DOM[src]:
        try:
            r = M.convert(dst, src, v)
        except ValueError:
            continue
        if r is None:
            continue
        rows.append((v, r, soft_row(dst, src, v, r)))
    return fn, rows


NO_UNTOUCHED = lambda dst, src: dst in ("Dec",) or (dst == "Date" and src == "Dec")


def emit_test(fn, dst, src, rows):
    outv = OUTVAR[dst]
    o = []
    o.append("TEST(vcn_%s) {" % fn)
    o.append("    REQUIRE(U(%s));" % fn)
    o.append("    static const struct { %s in; HRESULT hr; %s out; int soft; const char *d; } t[] = {" % (CTYPE[src], CTYPE[dst]))
    for v, r, soft in rows:
        if r[0] == "ok":
            hr, outs = "S_OK", cval(dst, r[1])
        else:
            hr = HRC[r[1]]
            outs = "{0,0,0,0,0}" if dst == "Dec" else ("0" if dst not in ("R4", "R8", "Date") else "0")
        o.append("        { %s, %s, %s, %d, \"%s\" }," % (cval(src, v), hr, outs, 1 if soft else 0, desc(src, v).replace('"', "'")))
    o.append("    };")
    o.append("    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {")
    o.append("        %s out; HRESULT hr;" % outv)
    o.append("        memset(&out, 0xCC, sizeof out);")
    if src == "Cy":
        o.append("        CY c; c.int64 = t[i].in; hr = U(%s)(c, &out);" % fn)
    elif src == "Dec":
        o.append("        DECIMAL dd; vcn_mkdec(&dd, &t[i].in); hr = U(%s)(&dd, &out);" % fn)
    else:
        o.append("        hr = U(%s)(t[i].in, &out);" % fn)
    o.append("        VCN_HR(\"%s\", i, t[i].soft, hr, t[i].hr, t[i].d);" % fn)
    o.append("        if (t[i].hr == S_OK && hr == S_OK) {")
    acc = {"Cy": "out.int64", "Dec": "out"}.get(dst, "out")
    if dst == "Dec":
        o.append("            VCN_DEC(\"%s\", i, t[i].soft, &out, &t[i].out, t[i].d);" % fn)
    else:
        o.append("            %s(\"%s\", i, t[i].soft, %s, t[i].out, t[i].d);" % (outcmp(dst), fn, acc))
    o.append("        }")
    if not NO_UNTOUCHED(dst, src):
        o.append("        if (t[i].hr != S_OK && hr == t[i].hr) VCN_UNTOUCHED(\"%s\", i, t[i].soft, &out, sizeof out, t[i].d);" % fn)
    o.append("    }")
    o.append("}")
    return "\n".join(o) + "\n"


HEADER = """/* GENERATED by gen/gen.py from gen/model.py -- do not edit by hand.
 * Windows 10 oleaut32 numeric conversion tables; expectations derived from the decompilation of the
 * VarXFromY functions (round-half-even via ConvI4FromR8 & friends, exact overflow thresholds).
 * Rows marked soft are reported with t_note only (unverified detail). */
#include "vcn.h"
"""


def write_dst_file(dst):
    parts = [HEADER]
    cnt = 0
    nrows = 0
    for src in SRCS:
        r = gen_table(dst, src)
        if not r:
            continue
        fn, rows = r
        parts.append(emit_test(fn, dst, src, rows))
        cnt += 1
        nrows += len(rows)
    path = os.path.join(OUT, "conv_%s.c" % dst.lower())
    open(path, "w").write("\n".join(parts))
    return cnt, nrows


if __name__ == "__main__":
    tot = 0
    totr = 0
    for d in DSTS:
        c, n = write_dst_file(d)
        print("dst", d, "tests", c, "rows", n)
        tot += c
        totr += n
    print("TOTAL tests", tot, "rows", totr)
    import strgen
    strgen.main()
