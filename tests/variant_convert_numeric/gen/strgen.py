"""String -> number tables (VarXFromStr).  Rules from decomp of VarI4FromStr / VarI8FromStr /
VarBoolFromStr and the NUMPRS_* grammar of VarParseNumFromStr (flags 0x1fff, USE_ALL)."""
import math, os
from decimal import Decimal, ROUND_HALF_EVEN
from fractions import Fraction
import model as M

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
NOUO = 0x80000000   # LOCALE_NOUSEROVERRIDE
EN, DE, FR, JA = 0x409, 0x407, 0x40c, 0x411
D = Decimal
MIS = "M"      # DISP_E_TYPEMISMATCH
HEX = "H"      # marker prefix for hex/oct rows: (HEX, value)

# (lcid, flags, string, spec, soft)   spec: Decimal | "M" | None for NULL string
ROWS = []


def r(lcid, s, spec, soft=False, flags=NOUO):
    ROWS.append((lcid, flags, s, spec, soft))


def en(s, spec, soft=False, flags=NOUO):
    r(EN, s, spec, soft, flags)


# ---- plain integers and decimals, en-US
for s, v in [("0", 0), ("1", 1), ("-1", -1), ("+1", 1), ("2", 2), ("7", 7), ("9", 9), ("10", 10), ("99", 99),
             ("100", 100), ("126", 126), ("127", 127), ("128", 128), ("129", 129), ("200", 200), ("254", 254),
             ("255", 255), ("256", 256), ("-126", -126), ("-127", -127), ("-128", -128), ("-129", -129),
             ("32766", 32766), ("32767", 32767), ("32768", 32768), ("-32767", -32767), ("-32768", -32768),
             ("-32769", -32769), ("65534", 65534), ("65535", 65535), ("65536", 65536), ("100000", 100000),
             ("2147483646", 2147483646), ("2147483647", 2147483647), ("2147483648", 2147483648),
             ("-2147483647", -2147483647), ("-2147483648", -2147483648), ("-2147483649", -2147483649),
             ("4294967294", 4294967294), ("4294967295", 4294967295), ("4294967296", 4294967296),
             ("9223372036854775806", 9223372036854775806), ("9223372036854775807", 9223372036854775807),
             ("9223372036854775808", 9223372036854775808), ("-9223372036854775808", -9223372036854775808),
             ("-9223372036854775809", -9223372036854775809), ("18446744073709551615", 18446744073709551615),
             ("18446744073709551616", 18446744073709551616),
             ("0000", 0), ("007", 7), ("0001", 1), ("-007", -7), ("00000000000000000255", 255)]:
    en(s, D(v))

# ---- fractions / banker's rounding (the Wine-verified ones plus half-even neighbours)
for s in ["0.1", "0.4", "0.5", "0.6", "1.4", "1.5", "1.6", "2.4", "2.5", "2.6", "3.5", "4.5", "5.5", "6.5", "7.5",
          "8.5", "9.5", "10.5", "11.5", "100.5", "101.5", "-0.1", "-0.4", "-0.5", "-0.6", "-1.4", "-1.5", "-1.6",
          "-2.4", "-2.5", "-2.6", "-3.5", "-4.5", "0.49", "0.51", "1.49", "1.51", "2.49", "2.51", "0.0", "0.00",
          "1.0", "1.00", "10.0", "10.50", "0.50", "2.50", "2.500", "0.5000000", "126.5", "127.4", "127.5",
          "127.6", "-128.4", "-128.5", "-128.6", "254.5", "255.4", "255.5", "255.6", "-0.50", "-0.51",
          "32766.5", "32767.4", "32767.5", "32767.6", "-32768.4", "-32768.5", "-32768.6", "65534.5",
          "65535.4", "65535.5", "65535.6", "2147483646.5", "2147483647.4", "2147483647.5", "2147483647.6",
          "-2147483648.4", "-2147483648.5", "-2147483648.6", "4294967294.5", "4294967295.4", "4294967295.5",
          "4294967295.6", "12.3456", "0.0001", "0.00005", "0.99999", "1234.5678", "99.999", "3.14159265358979",
          "123456.789", "-123456.789", "0.123456789", "1.23456789012"]:
    v = D(s)
    big = abs(v) > 100000
    en(s, v, soft=(big and v != v.to_integral_value()) or s in ("0.00005",))

# leading-dot / trailing-dot / signs
for s, v in [(".5", D("0.5")), ("-.5", D("-0.5")), ("+.5", D("0.5")), (".6", D("0.6")), ("5.", D(5)), ("-5.", D(-5)),
             ("+5.", D(5)), ("1.", D(1)), ("0.", D(0)), (".0", D(0)), ("+0", D(0)), ("-0", D(0)), ("-0.0", D(0)),
             ("+1.5", D("1.5")), (".25", D("0.25")), ("-.25", D("-0.25")), ("12.", D(12))]:
    en(s, v, soft=s in ("-0", "-0.0"))

# whitespace
for s, v, soft in [(" 1", 1, False), ("1 ", 1, False), (" 1 ", 1, False), ("  12  ", 12, False), ("   -5", -5, False),
                   ("5   ", 5, False), (" +7 ", 7, False), ("\t1", 1, True), ("1\t", 1, True), ("\n1", 1, True),
                   ("1\r\n", 1, True), ("\r1", 1, True), (" 1", 1, True), ("1 ", 1, True),
                   (" 1.5 ", D("1.5"), False), (" - 1", MIS, True), ("- 1", MIS, True), ("+ 1", MIS, True),
                   ("1 2", MIS, False), ("1 .5", MIS, True), ("1. 5", MIS, True), ("   ", MIS, False), (" ", MIS, False),
                   ("1  2  3", MIS, False)]:
    en(s, D(v) if v != MIS else MIS, soft)

# signs
for s, v, soft in [("--1", MIS, False), ("++1", MIS, False), ("+-1", MIS, True), ("-+1", MIS, True), ("1-", -1, True),
                   ("1+", 1, True), ("-1-", MIS, True), ("+1+", MIS, True), ("1--", MIS, True), ("-", MIS, False),
                   ("+", MIS, False), ("-.", MIS, False), (".", MIS, False), ("+.", MIS, False), ("1-1", MIS, False),
                   ("1+1", MIS, False), ("-1 -", MIS, True)]:
    en(s, D(v) if v != MIS else MIS, soft)

# parentheses
for s, v, soft in [("(1)", -1, False), ("(0)", 0, False), ("(5)", -5, False), ("(1.5)", D("-1.5"), False),
                   ("(2.5)", D("-2.5"), False), ("(0.5)", D("-0.5"), False), (" (1) ", -1, True), ("(1", MIS, False),
                   ("1)", MIS, False), ("()", MIS, False), ("(-1)", MIS, True), ("-(1)", MIS, True), ("((1))", MIS, False),
                   ("(1)(2)", MIS, False), ("( 1)", MIS, True), ("(1 )", MIS, True), ("(100)", -100, False), ("(128)", -128, False),
                   ("(129)", -129, False)]:
    en(s, D(v) if v != MIS else MIS, soft)

# thousands separators and currency
for s, v, soft in [("1,000", 1000, False), ("1,000,000", 1000000, False), ("12,345", 12345, False),
                   ("1,234.5", D("1234.5"), False), ("1,234.56", D("1234.56"), False), ("-1,000", -1000, False),
                   ("(1,000)", -1000, False), ("1,000.50", D("1000.5"), False), ("999,999", 999999, False),
                   ("1,5", 15, True), ("1,00", 100, True), ("1,0000", 10000, True), (",1", MIS, True),
                   ("1,", MIS, True), ("1,,000", MIS, True), (",", MIS, False), (",,", MIS, False),
                   ("1.000,5", MIS, True), ("2,147,483,647", 2147483647, False), ("2,147,483,648", 2147483648, False),
                   ("$1", 1, False), ("$0", 0, False), ("$100", 100, False), ("$1.50", D("1.5"), False),
                   ("$1,000", 1000, False), ("$1,234.56", D("1234.56"), False), ("-$1", -1, True), ("$-1", -1, True),
                   ("($1)", -1, True), ("$(1)", MIS, True), ("$", MIS, False), ("$$1", MIS, True), ("1$", MIS, True),
                   ("$ 1", MIS, True), (" $1 ", 1, True), ("$.5", D("0.5"), True), ("$1.", 1, True), ("+$1", 1, True),
                   ("$+1", 1, True), ("€1", MIS, True), ("£1", MIS, True), ("¥1", MIS, True)]:
    en(s, D(v) if v != MIS else MIS, soft)

# exponent notation
for s, v, soft in [("1e0", 1, False), ("1e1", 10, False), ("1e2", 100, False), ("1e3", 1000, False), ("1E3", 1000, False),
                   ("1e+3", 1000, False), ("1e-3", D("0.001"), False), ("1e-1", D("0.1"), False), ("5e-1", D("0.5"), False),
                   ("15e-1", D("1.5"), False), ("25e-1", D("2.5"), False), ("1.5e1", 15, False), ("1.5e2", 150, False),
                   ("1.5e0", D("1.5"), False), ("2.5e0", D("2.5"), False), ("-1e3", -1000, False), ("-1.5e1", -15, False),
                   ("1e-10", D("1e-10"), False), ("1e10", 10 ** 10, False), ("1e5", 100000, False), (".5e1", 5, False),
                   ("5.e1", 50, True), ("1.0e3", 1000, False), ("12e-1", D("1.2"), False), ("0e0", 0, False),
                   ("0e5", 0, False), ("1e", MIS, False), ("1e+", MIS, False), ("1e-", MIS, False), ("e3", MIS, False),
                   ("e", MIS, False), ("1e3e3", MIS, False), ("1e3.5", MIS, False), ("1e 3", MIS, False), ("1 e3", MIS, True),
                   ("1e+-3", MIS, True), ("(1e2)", -100, True), ("1,000e0", 1000, True), ("2e2", 200, False), ("255e0", 255, False),
                   ("2.55e2", 255, False), ("2.555e2", D("255.5"), False), ("1e127", 10 ** 127, False), ("1d3", 1000, True),
                   ("1D3", 1000, True), ("1e-400", 0, True)]:
    en(s, D(v) if v != MIS else MIS, soft)

# hex / octal (VB style).  Values > signed max of the destination are soft (see decomp: raw digit accumulation)
for s, v, soft in [("&H0", 0, False), ("&H1", 1, False), ("&H10", 16, False), ("&H7F", 127, False), ("&h7f", 127, False),
                   ("&H80", 128, False), ("&HFF", 255, False), ("&hff", 255, False), ("&H100", 256, False),
                   ("&H7FFF", 32767, False), ("&H8000", 32768, True), ("&HFFFF", 65535, True), ("&H10000", 65536, False),
                   ("&H7FFFFFFF", 2147483647, False), ("&H80000000", 2147483648, True), ("&HFFFFFFFF", 4294967295, True),
                   ("&H00000010", 16, False), ("&HABC", 0xabc, False), ("&Habc", 0xabc, False), ("&HaBc", 0xabc, False),
                   ("&O0", 0, False), ("&O7", 7, False), ("&O10", 8, False), ("&O17", 15, False), ("&o17", 15, False),
                   ("&O377", 255, False), ("&O400", 256, False), ("&O177777", 65535, True), ("&O7777777", 2097151, False),
                   ("&HG", MIS, False), ("&H", MIS, True), ("&O8", MIS, True), ("&O9", MIS, True), ("&B1", MIS, True),
                   ("0x10", MIS, False), ("0X10", MIS, False), ("0h10", MIS, True), ("10h", MIS, True), ("H10", MIS, False),
                   ("&10", MIS, True), ("&", MIS, False), ("&H 10", MIS, True), (" &H10", 16, True), ("&H10 ", 16, True),
                   ("&H1.5", MIS, True), ("&H1e3", 0x1e3, True), ("-&H10", -16, True), ("+&H10", 16, True), ("(&H10)", MIS, True),
                   ("&HFG", MIS, False), ("&HF F", MIS, False), ("&O1.5", MIS, True), ("&H0.5", MIS, True)]:
    en(s, D(v) if v != MIS else MIS, soft)

# garbage / non numbers
for s in ["", "a", "abc", "1a", "a1", "1a2", "one", "NaN", "nan", "Inf", "Infinity", "-Inf", "1.2.3", "1..2", "..1", "1.e.1",
          "0x", "x", "#", "1#", "#1", "1%", "%1", "1/2", "1:2", "1;2", "1*2", "True", "False", "TRUE", "#TRUE#", "#FALSE#",
          "1.5.", "-.-", "1_000", "١٢٣", "1 000x", "−" + "1", "1−", "½", "١", "1/2/3",
          "1.2e3.4", "++", "--", ".-", "-.e5", "e+5", "1 1", "9 9"]:
    en(s, MIS, soft=(s in ("١٢٣", "١", "−" + "1", "1−", "½")))

# flags = 0 (LCID explicit so same behaviour) a few rows
for s, v in [("1", 1), ("-1", -1), ("2.5", D("2.5")), ("1,000", 1000), ("$5", 5), ("(3)", -3), ("&H1F", 31), ("1e2", 100), (" 4 ", 4)]:
    en(s, D(v), flags=0)

# NULL string -> DISP_E_TYPEMISMATCH (Wine tests of every Var*FromStr, decomp: VarParseNumFromStr)
r(EN, None, MIS, flags=0)
r(EN, None, MIS, flags=NOUO)
r(DE, None, MIS, flags=0)

# ---- other locales
for s, v, soft in [("1,5", D("1.5"), False), ("0,5", D("0.5"), False), (",5", D("0.5"), False), ("2,5", D("2.5"), False),
                   ("1,6", D("1.6"), False), ("-1,5", D("-1.5"), False), ("-0,5", D("-0.5"), False), ("1.000", 1000, False),
                   ("1.000,5", D("1000.5"), False), ("1.234,56", D("1234.56"), False), ("1.234.567", 1234567, False),
                   ("12,", 12, True), ("1e3", 1000, False), ("1,5e2", 150, False), ("1,5e1", 15, False), ("(1,5)", D("-1.5"), False),
                   ("1,2,3", MIS, False), ("1.5", 15, True), ("1,000", D("1"), False), ("0,001", D("0.001"), False),
                   ("127,5", D("127.5"), False), ("255,5", D("255.5"), False), ("100,4", D("100.4"), False), ("100,6", D("100.6"), False),
                   ("1.", MIS, True), ("&H10", 16, False), ("&O17", 15, False), ("€1", 1, True), ("1 €", MIS, True),
                   ("1,00 €", MIS, True), ("-1.000,25", D("-1000.25"), False), (" 3,5 ", D("3.5"), False), ("4,5", D("4.5"), False)]:
    r(DE, s, D(v) if v != MIS else MIS, soft)
for s, v, soft in [("1,5", D("1.5"), False), ("0,5", D("0.5"), False), ("2,5", D("2.5"), False), ("-1,5", D("-1.5"), False),
                   ("1e3", 1000, False), ("(1,5)", D("-1.5"), False), ("12,345", D("12.345"), False), ("3,5", D("3.5"), False),
                   ("1 000", 1000, True), ("1 000", 1000, True), ("1 000", 1000, True), ("1 000,5", D("1000.5"), True),
                   ("1.5", MIS, True), ("1.000", MIS, True), ("1,000", D("1"), False), ("&HFF", 255, False), ("€1", 1, True)]:
    r(FR, s, D(v) if v != MIS else MIS, soft)
for s, v, soft in [("1.5", D("1.5"), False), ("0.5", D("0.5"), False), ("2.5", D("2.5"), False), ("-1.5", D("-1.5"), False),
                   ("1,000", 1000, False), ("1,234.5", D("1234.5"), False), ("(1.5)", D("-1.5"), False), ("1e3", 1000, False),
                   ("1,5", 15, True), ("¥1", 1, True), ("￥1", 1, True), ("\\1", 1, True),
                   ("１２３", 123, True), ("１", 1, True), ("&H10", 16, False), ("3.5", D("3.5"), False)]:
    r(JA, s, D(v) if v != MIS else MIS, soft)


# ---------------------------------------------------------------- dest models
def rte_dec(v):
    return round(Fraction(v))


def ndigits(s):
    t = "".join(ch for ch in s.split("e")[0].split("E")[0] if ch.isdigit()).lstrip("0")
    return len(t)


def is_hex(s):
    return "&" in s


def dest_result(dst, spec, s):
    k, v, so = _dest_result(dst, spec, s)
    if s is not None and dst in ('R4','R8','Cy','Dec','Bool') and (ndigits(s) > 15 or is_hex(s)):
        so = True
    return k, v, so


def _dest_result(dst, spec, s):
    """returns (kind, value, soft_extra) ; kind in ok/hr"""
    if spec == MIS:
        return ("hr", "INV" if (dst == "Bool" and s is None) else "TM", False)
    v = spec
    if dst in ("R4", "R8", "Cy", "Dec", "Bool") and s is not None and ndigits(s) > 15:
        sf = True
    else:
        sf = False
    if dst in M.INT_RANGE:
        r_ = rte_dec(v)
        lo, hi = M.INT_RANGE[dst]
        frac_tie = (Fraction(v) % 1) == Fraction(1, 2)
        soft = False
        if ndigits(s) > 10 and dst == "UI8":
            soft = True
        if is_hex(s) and r_ > (0x7fffffff if dst == "I4" else hi) and dst in ("I1", "I2", "I4", "I8"):
            soft = True
        if frac_tie and (abs(v) > 1000) and dst not in ("I4", "I8"):
            soft = True
        if frac_tie and dst not in ("I4", "I8") and (abs(r_ - lo) <= 1 or abs(r_ - hi) <= 1):
            soft = True   # tie exactly at a type boundary: only the I4/I8 string paths are visible in the decomp
        if is_hex(s) and dst in ("I1", "I2", "UI1", "UI2") and r_ > 127:
            soft = True   # VarNumFromParseNum hex path not visible in the decomp
        if r_ < lo or r_ > hi:
            return ("hr", M.OVF, soft)
        return ("ok", r_, soft)
    if dst == "R8":
        return ("ok", float(v), False)
    if dst == "R4":
        f_ = M.f32(float(v))
        if math.isinf(f_):
            return ("hr", M.OVF, True)
        return ("ok", f_, False)
    if dst == "Cy":
        c = Fraction(v) * 10000
        if abs(v) >= D("922337203685477.5807"):
            return ("hr", M.OVF, True)
        ri = round(c)
        tie = (c % 1) == Fraction(1, 2)
        return ("ok", ri, tie or len(str(v)) > 14)
    if dst == "Dec":
        if v == 0:
            return ("ok", (0, 0, 0, 0, 0), True)
        sign = 0x80 if v < 0 else 0
        a = abs(v).normalize()
        sc = 0
        tup = a.as_tuple()
        if tup.exponent >= 0:
            mag = int(a)
        else:
            sc = -tup.exponent
            mag = int(a.scaleb(sc))
        if mag >= 1 << 96 or sc > 28:
            return ("hr", M.OVF, True)
        return ("ok", (sc, sign, (mag >> 64) & 0xffffffff, (mag >> 32) & 0xffffffff, mag & 0xffffffff), len(tup.digits) > 17)
    if dst == "Bool":
        return ("ok", -1 if v != 0 else 0, False)
    raise KeyError(dst)


def wlit(s):
    if s is None:
        return "NULL"
    parts = []
    run = ""
    for c in s:
        o = ord(c)
        if 32 <= o < 127 and c not in '"\\':
            run += c
        else:
            if run:
                parts.append('L"%s"' % run)
                run = ""
            if o > 0xffff:
                import struct
                b = c.encode("utf-16-le")
                for k in range(0, len(b), 2):
                    parts.append('L"\\x%04x"' % int.from_bytes(b[k:k + 2], "little"))
            else:
                parts.append('L"\\x%04x"' % o)
    if run:
        parts.append('L"%s"' % run)
    return " ".join(parts) if parts else 'L""'


CT = {"I1": "CHAR", "UI1": "BYTE", "I2": "SHORT", "UI2": "USHORT", "I4": "LONG", "UI4": "ULONG", "I8": "LONG64",
      "UI8": "ULONG64", "R4": "float", "R8": "double", "Cy": "CY", "Dec": "DECIMAL", "Bool": "VARIANT_BOOL"}


def emit(dst, gm):
    fn = "Var%sFromStr" % dst
    o = [gm.HEADER]
    o.append("TEST(vcn_%s) {" % fn)
    o.append("    REQUIRE(U(%s));" % fn)
    o.append("    static const struct { const WCHAR *s; LCID lcid; ULONG fl; HRESULT hr; %s out; int soft; const char *d; } t[] = {" %
             ("struct vdec" if dst == "Dec" else {"Cy": "LONGLONG", "Bool": "VARIANT_BOOL"}.get(dst, CT[dst])))
    n = 0
    for lcid, fl, s, spec, soft in ROWS:
        kind, val, sx = dest_result(dst, spec, s)
        if dst == "Bool" and s is not None and (is_hex(s) or s in ("True", "False", "TRUE")):
            continue
        if dst == "Bool" and s is not None and s.upper() in ("#TRUE#", "#FALSE#"):
            continue
        if kind == "ok":
            hr = "S_OK"
            outs = gm.cval({"Cy": "Cy"}.get(dst, dst), val)
        else:
            hr = gm.HRC[val] if val != "INV" else "E_INVALIDARG"
            outs = "{0,0,0,0,0}" if dst == "Dec" else "0"
        if dst == "Bool" and kind == "hr" and val == "TM":
            pass
        desc = ("NULL" if s is None else s.encode("unicode_escape").decode("ascii").replace("\\", "\\\\").replace('"', "'")) + " @%x/%x" % (lcid, fl >> 28)
        o.append("        { %s, 0x%x, 0x%x, %s, %s, %d, \"%s\" }," % (wlit(s), lcid, fl, hr, outs, 1 if (soft or sx) else 0, desc))
        n += 1
    o.append("    };")
    o.append("    for (unsigned i = 0; i < VCN_ROWS(t); i++) {")
    outv = {"Cy": "CY", "Dec": "DECIMAL"}.get(dst, CT[dst])
    o.append("        %s out; HRESULT hr;" % outv)
    o.append("        memset(&out, 0xCC, sizeof out);")
    o.append("        hr = U(%s)(t[i].s, t[i].lcid, t[i].fl, &out);" % fn)
    # NULL string for Bool is E_INVALIDARG (decomp); handled by table below
    o.append("        VCN_HR(\"%s\", i, t[i].soft, hr, t[i].hr, t[i].d);" % fn)
    o.append("        if (t[i].hr == S_OK && hr == S_OK) {")
    if dst == "Dec":
        o.append("            VCN_DEC(\"%s\", i, t[i].soft, &out, &t[i].out, t[i].d);" % fn)
    elif dst == "Cy":
        o.append("            VCN_CY(\"%s\", i, t[i].soft, out.int64, t[i].out, t[i].d);" % fn)
    elif dst in ("R4", "R8"):
        o.append("            VCN_NEAR(\"%s\", i, t[i].soft, out, t[i].out, %s, t[i].d);" % (fn, "1e-6" if dst == "R4" else "1e-14"))
    elif dst.startswith("UI"):
        o.append("            VCN_UINT(\"%s\", i, t[i].soft, out, t[i].out, t[i].d);" % fn)
    else:
        o.append("            VCN_INT(\"%s\", i, t[i].soft, out, t[i].out, t[i].d);" % fn)
    o.append("        }")
    if dst != "Dec":
        o.append("        if (t[i].hr != S_OK && hr == t[i].hr) VCN_UNTOUCHED(\"%s\", i, t[i].soft, &out, sizeof out, t[i].d);" % fn)
    o.append("    }")
    o.append("}")
    return "\n".join(o) + "\n", n


def main():
    import gen as gm
    total = 0
    for dst in ["I1", "UI1", "I2", "UI2", "I4", "UI4", "I8", "UI8", "R4", "R8", "Cy", "Dec", "Bool"]:
        txt, n = emit(dst, gm)
        open(os.path.join(OUT, "str_%s.c" % dst.lower()), "w").write(txt)
        print("str", dst, n)
        total += n
    print("STR rows", total)
