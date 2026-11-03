"""Reference model of oleaut32 VarXFromY numeric conversions (Windows 10 x64).

Every rule here was read from the Win10 decompilation of oleaut32.dll
(VarI4FromR8, ConvI4FromR8, VarI4FromCy, VarDecFromR8, VarCyFromDec, ...).
The model returns ("ok", value) or ("hr", name).  Values:
  ints / Bool / Cy -> python int, R4/R8/Date -> python float,
  Dec -> tuple (scale, sign, hi, mid, lo)
"""
import struct, math
from fractions import Fraction

OVF = "DISP_E_OVERFLOW"
INV = "E_INVALIDARG"

INT_RANGE = {
    "I1": (-128, 127), "UI1": (0, 255), "I2": (-32768, 32767), "UI2": (0, 65535),
    "I4": (-2**31, 2**31 - 1), "UI4": (0, 2**32 - 1),
    "I8": (-2**63, 2**63 - 1), "UI8": (0, 2**64 - 1),
}
# float bounds: overflow if x < lo or x >= hi  (decomp constants)
FBOUND = {
    "I1": (-128.5, 127.5), "UI1": (-0.5, 255.5), "I2": (-32768.5, 32767.5),
    "UI2": (-0.5, 65535.5), "I4": (-2147483648.5, 2147483647.5),
    "UI4": (-0.5, 4294967295.5), "I8": (-2.0**63, 2.0**63), "UI8": (-0.5, 2.0**64),
}
INTS = list(INT_RANGE)
FLT_MAX = struct.unpack('<f', struct.pack('<I', 0x7f7fffff))[0]
DATE_LO, DATE_HI = -657435.0, 2958466.0
CY_BND = struct.unpack('>d', bytes.fromhex('430a36e2eb1c432d'))[0]


def f32(x):
    try:
        return struct.unpack('<f', struct.pack('<f', x))[0]
    except OverflowError:
        return math.copysign(math.inf, x)


def rte(fr):
    """round half to even of an exact Fraction / float / int"""
    return round(Fraction(fr))


def wrap(v, bits):
    return v & ((1 << bits) - 1)


def sgn(v, bits):
    v = wrap(v, bits)
    return v - (1 << bits) if v >> (bits - 1) else v


# ---------------------------------------------------------------- real -> int
def real_to_int(dst, f):
    lo, hi = FBOUND[dst]
    if math.isnan(f):
        raise ValueError("nan")
    if f < lo or f >= hi:
        return ("hr", OVF)
    return ("ok", rte(f))


def int_to_int(dst, v):
    lo, hi = INT_RANGE[dst]
    if v < lo or v > hi:
        return ("hr", OVF)
    return ("ok", v)


def cy_to_rounded(cy):
    return rte(Fraction(cy, 10000))


def i8_from_cy_literal(cy):
    """VarI8FromCy as decompiled: remainder computed from the LOW 32 bits only (Win32 bug: every
    negative value is rounded down by one more, e.g. -1.0 -> -2; also wrong for |cy| >= 2^32)."""
    q = int(abs(cy) // 10000) * (1 if cy >= 0 else -1)   # C truncating division
    lo = cy & 0xffffffff
    l2 = lo - q * 10000
    l3 = q * 10000 - lo
    if l2 >= 0:
        l3 = l2
    if 5000 < l3 + (q & 1):
        q += -1 if cy < 0 else 1
    return q


def cy_to_int(dst, cy):
    if dst == "I8":
        return ("ok", i8_from_cy_literal(cy))
    if dst in ("UI4", "UI8"):  # via VarUIxFromR8((double)cy / 10000.0)
        return real_to_int(dst, float(cy) / 10000.0)
    r = cy_to_rounded(cy)  # I1 UI1 I2 UI2 I4: round-half-even then range
    if r < -2**31 or r > 2**31 - 1:
        return ("hr", OVF)
    return int_to_int(dst, r)


def bool_to_int(dst, b):
    bits = {"I1": 8, "UI1": 8, "I2": 16, "UI2": 16, "I4": 32, "UI4": 32, "I8": 64, "UI8": 64}[dst]
    v = wrap(b, bits)
    if dst.startswith("I"):
        v = sgn(v, bits)
    return ("ok", v)


# ---------------------------------------------------------------- Dec helpers
def dec_valid(d):
    return d[0] <= 28 and (d[1] & 0x7f) == 0


def dec_mag(d):
    return (d[2] << 64) | (d[3] << 32) | d[4]


def pow10f(n):
    return float(10 ** n)


def r8_from_dec(d):
    """VarR8FromDec: ((double)Hi32 * 2^64 + dVar4) / 10^scale"""
    scale, sign, hi, mid, lo = d
    lo64 = (mid << 32) | lo
    dv4 = float(sgn(lo64, 64))
    if mid & 0x80000000:
        dv4 = dv4 + 18446744073709551616.0
    tot = (float(hi) * 18446744073709551616.0 + dv4) / pow10f(scale)
    if sign:
        tot = -tot
    return tot


def dec_to_int(dst, d):
    scale, sign, hi, mid, lo = d
    if not dec_valid(d):
        return ("hr", INV)
    if scale != 0:
        return real_to_int(dst, r8_from_dec(d))
    neg = sign != 0
    lo64 = (mid << 32) | lo
    if dst in ("I1", "I2", "I4"):
        lim = {"I1": 0x80, "I2": 0x8000, "I4": 0x80000000}[dst]
        if mid != 0 or hi != 0:
            return ("hr", OVF)
        if lo < lim:
            return ("ok", -lo if neg else lo)
        if lo == lim and neg:
            return ("ok", -lo)
        return ("hr", OVF)
    if dst == "I8":
        if hi != 0:
            return ("hr", OVF)
        if lo64 < 2**63:
            return ("ok", -lo64 if neg else lo64)
        if lo64 == 2**63 and neg:
            return ("ok", -lo64)
        return ("hr", OVF)
    if dst in ("UI1", "UI2", "UI4"):
        lim = {"UI1": 0x100, "UI2": 0x10000, "UI4": 1 << 32}[dst]
        if mid == 0 and hi == 0 and lo < lim and (not neg or lo == 0):
            return ("ok", lo)
        return ("hr", OVF)
    if dst == "UI8":
        # native quirk (decomp + Wine comment): sign only rejected when both Lo32 and Mid32 are non-zero
        if hi == 0 and ((not neg) or lo == 0 or mid == 0):
            return ("ok", lo64)
        return ("hr", OVF)
    raise KeyError(dst)


# ------------------------------------------------------------- Dec from real
def dec_from_r8(x):
    bits = struct.unpack('<Q', struct.pack('<d', x))[0]
    e = (bits >> 52) & 0x7ff
    zero = ("ok", (0, 0, 0, 0, 0))
    if e < 0x3a0:
        return zero
    if e > 0x45e:
        return ("hr", OVF)
    ax = abs(x)
    u = 0xe - (((e - 0x3fe) * 0x4d10) >> 16)
    if u < 0:
        if u != -1 or ax >= 1000000000000000.0:
            d = ax / pow10f(-u)
            sc = u
        else:
            d = ax
            sc = 0
    else:
        if u > 0x1c:
            u = 0x1c
        d = ax * pow10f(u)
        sc = u
    if d < 1e14 and sc < 0x1c:
        d = d * 10.0
        sc += 1
    loc = int(d)
    frac = d - float(loc)
    if frac > 0.5:
        loc += 1
    elif frac == 0.5 and (loc & 1):
        loc += 1
    if loc == 0:
        return zero
    sign = 0x80 if bits >> 63 else 0
    if sc < 0:
        mag = loc * 10 ** (-sc)
        if mag >= 1 << 96:
            return ("hr", OVF)
        sc = 0
    else:
        a = 8
        lim = min(sc, 14)
        while a:
            if a <= lim:
                p = 10 ** a
                if loc % p == 0:
                    loc //= p
                    sc -= a
                    lim -= a
            a >>= 1
        mag = loc
    return ("ok", (sc, sign, (mag >> 64) & 0xffffffff, (mag >> 32) & 0xffffffff, mag & 0xffffffff))


def dec_from_r4(f):
    """returns None for the tie branch whose helper was not decompiled"""
    bits = struct.unpack('<I', struct.pack('<f', f))[0]
    e = (bits >> 23) & 0xff
    zero = ("ok", (0, 0, 0, 0, 0))
    if e <= 0x1f:
        return zero
    if e > 0xde:
        return ("hr", OVF)
    t = ((e - 0x7e) * 0x4d10) >> 16
    i8 = 6 - t
    ax = abs(float(f))
    i9 = 0
    if i8 < 0:
        if i8 != -1 or ax >= 10000000.0:
            d = ax / pow10f(-i8)
            i9 = i8
        else:
            d = ax
    else:
        if i8 > 0x1c:
            i8 = 0x1c
        d = ax * pow10f(i8)
        i9 = i8
    if d < 1000000.0 and i9 < 0x1c:
        d = d * 10.0
        i9 += 1
    u = int(d) & 0xffffffff
    frac = d - float(u)
    if frac > 0.5:
        u += 1
    elif frac == 0.5 and (u & 1):
        return None
    if u == 0:
        return zero
    sign = 0x80 if bits >> 31 else 0
    if i9 < 0:
        mag = u * 10 ** (-i9)
        if mag >= 1 << 96:
            return ("hr", OVF)
        sc = 0
    else:
        lim = min(6, i9)
        a = 4
        while a:
            if a <= lim and u % (10 ** a) == 0:
                i9 -= a
                lim -= a
                u //= 10 ** a
            a >>= 1
        sc = i9
        mag = u
    return ("ok", (sc, sign, (mag >> 64) & 0xffffffff, (mag >> 32) & 0xffffffff, mag & 0xffffffff))


def dec_from_int(v):
    s = 0x80 if v < 0 else 0
    m = abs(v)
    return (0, s, (m >> 64) & 0xffffffff, (m >> 32) & 0xffffffff, m & 0xffffffff)


def dec_from_cy(cy):
    s = 0x80 if cy < 0 else 0
    m = abs(cy)
    return (4, s, 0, (m >> 32) & 0xffffffff, m & 0xffffffff)


# ----------------------------------------------------------------- Cy from X
def cy_from_real(x):
    if not (-CY_BND < x < CY_BND):
        return ("hr", OVF)
    return ("ok", rte(Fraction(x) * 10000))


def cy_from_dec(d):
    if not dec_valid(d):
        return ("hr", INV)
    neg = d[1] != 0
    r = rte(Fraction(dec_mag(d), 10 ** d[0]) * 10000)
    if r > 2**63 or (r == 2**63 and not neg):
        return ("hr", OVF)
    return ("ok", -r if neg else r)


# ------------------------------------------------------------------ dispatch
SRC_KIND = {
    "I1": "int", "UI1": "int", "I2": "int", "UI2": "int", "I4": "int", "UI4": "int",
    "I8": "int", "UI8": "int", "R4": "real", "R8": "real", "Date": "real",
    "Cy": "cy", "Bool": "bool", "Dec": "dec",
}


def convert(dst, src, v):
    """returns ("ok", value) / ("hr", name) / None (unknown, skip row)"""
    k = SRC_KIND[src]
    if dst in INT_RANGE:
        if k == "int":
            return int_to_int(dst, v)
        if k == "real":
            return real_to_int(dst, v)
        if k == "cy":
            return cy_to_int(dst, v)
        if k == "bool":
            return bool_to_int(dst, v)
        if k == "dec":
            return dec_to_int(dst, v)
    if dst == "R4":
        if k == "int":
            return ("ok", f32(float(v)))
        if k == "real":
            if not (-FLT_MAX <= v <= FLT_MAX):
                return ("hr", OVF)
            return ("ok", f32(v))
        if k == "cy":
            return ("ok", f32(float(v) / 10000.0))
        if k == "bool":
            return ("ok", float(v))
        if k == "dec":
            if not dec_valid(v):
                return ("hr", INV)
            return ("ok", f32(r8_from_dec(v)))
    if dst == "R8":
        if k in ("int", "real", "bool"):
            return ("ok", float(v))
        if k == "cy":
            return ("ok", float(v) / 10000.0)
        if k == "dec":
            if not dec_valid(v):
                return ("hr", INV)
            return ("ok", r8_from_dec(v))
    if dst == "Date":
        def chk(x):
            return ("ok", x) if (DATE_LO < x < DATE_HI) else ("hr", OVF)
        if k in ("int", "real"):
            return chk(float(v))
        if k == "cy":
            return chk(float(v) / 10000.0)
        if k == "bool":
            return ("ok", float(v))
        if k == "dec":
            if not dec_valid(v):
                return ("hr", INV)
            return ("ok", r8_from_dec(v))
    if dst == "Cy":
        if k == "int":
            if src == "I8":
                return ("ok", v * 10000) if -922337203685476 <= v <= 922337203685476 else ("hr", OVF)
            if src == "UI8":
                return cy_from_real(float(v))
            return ("ok", v * 10000)
        if k == "real":
            return cy_from_real(v)
        if k == "bool":
            return ("ok", v * 10000)
        if k == "dec":
            return cy_from_dec(v)
    if dst == "Dec":
        if k == "int":
            return ("ok", dec_from_int(v))
        if k == "real":
            return dec_from_r4(v) if src == "R4" else dec_from_r8(v)
        if k == "cy":
            return ("ok", dec_from_cy(v))
        if k == "bool":
            return ("ok", (0, 0x80, 0, 0, 1) if v != 0 else (0, 0, 0, 0, 0))
    if dst == "Bool":
        if k in ("int", "cy", "bool", "real"):
            return ("ok", -1 if v != 0 else 0)
        if k == "dec":
            if not dec_valid(v):
                return ("hr", INV)
            return ("ok", -1 if dec_mag(v) != 0 else 0)
    raise KeyError((dst, src))
