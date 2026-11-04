/* variant_format_parse: VarParseNumFromStr and VarNumFromParseNum (oleaut32.dll).
 * Decomp-verified: VarParseNumFromStr returns E_INVALIDARG (0x80070057) for NULL pnumprs or rgbDig, DISP_E_TYPEMISMATCH
 * (0x80020005) for a NULL string; the NUMPARSE input is cDig (digit capacity) and dwInFlags (NUMPRS_*); outputs are cDig (digits
 * stored), dwOutFlags, cchUsed, nBaseShift (0 decimal, 3 octal, 4 hex) and nPwr10.  Digit bytes are values 0..9 (0..15 for hex),
 * zero suffixes are dropped from cDig and moved into nPwr10.  Bytes past cDig in rgbDig are an implementation detail: not checked.
 * LCID 0x409 and LOCALE_NOUSEROVERRIDE are used for every row. */
#include "vfp.h"

#define F0 0
typedef struct {
    const char *s;      /* input */
    int cap;            /* NUMPARSE.cDig on input; 0 means 128 (sizeof rgb) */
    ULONG in;           /* dwInFlags */
    int fail;           /* expect DISP_E_TYPEMISMATCH */
    int cdig;           /* output cDig */
    ULONG out;          /* output dwOutFlags */
    int cch;            /* cchUsed */
    int base;           /* nBaseShift */
    int pwr;            /* nPwr10 */
    const char *digs;   /* expected leading digit values as hex chars */
} PRow;

#define R(s, cap, in, cdig, out, cch, base, pwr, digs) { s, cap, in, 0, cdig, out, cch, base, pwr, digs }
#define X(s, cap, in) { s, cap, in, 1, 0, 0, 0, 0, 0, "" }

static HRESULT parse_one(const WCHAR *w, int cap, ULONG in, LCID lcid, NUMPARSE *np, BYTE *rgb)
{
    memset(rgb, 0xff, 128);
    memset(np, 0xff, sizeof *np);
    np->cDig = cap ? cap : 128;
    np->dwInFlags = in;
    return U(VarParseNumFromStr)(w, lcid, LOCALE_NOUSEROVERRIDE, np, rgb);
}

static void run_prows(const PRow *rows, size_t n, LCID lcid)
{
    size_t i;
    for (i = 0; i < n; i++) {
        NUMPARSE np;
        BYTE rgb[128];
        HRESULT hr = parse_one(vfp_w(rows[i].s), rows[i].cap, rows[i].in, lcid, &np, rgb);
        if (rows[i].fail) {
            CHECKF(hr == VFP_E_TYPEMISMATCH, "\"%s\" in=0x%lx: want TYPEMISMATCH got 0x%lx", rows[i].s, rows[i].in, (unsigned long)hr);
            continue;
        }
        CHECKF(hr == S_OK, "\"%s\" in=0x%lx: hr=0x%lx", rows[i].s, rows[i].in, (unsigned long)hr);
        if (hr != S_OK) continue;
        CHECKF(np.cDig == rows[i].cdig, "\"%s\" in=0x%lx: cDig %d want %d", rows[i].s, rows[i].in, np.cDig, rows[i].cdig);
        CHECKF(np.dwInFlags == rows[i].in, "\"%s\" in=0x%lx: dwInFlags changed to 0x%lx", rows[i].s, rows[i].in, np.dwInFlags);
        CHECKF(np.dwOutFlags == rows[i].out, "\"%s\" in=0x%lx: dwOutFlags 0x%lx want 0x%lx", rows[i].s, rows[i].in, np.dwOutFlags, rows[i].out);
        CHECKF(np.cchUsed == rows[i].cch, "\"%s\" in=0x%lx: cchUsed %d want %d", rows[i].s, rows[i].in, np.cchUsed, rows[i].cch);
        CHECKF(np.nBaseShift == rows[i].base, "\"%s\" in=0x%lx: nBaseShift %d want %d", rows[i].s, rows[i].in, np.nBaseShift, rows[i].base);
        CHECKF(np.nPwr10 == rows[i].pwr, "\"%s\" in=0x%lx: nPwr10 %d want %d", rows[i].s, rows[i].in, np.nPwr10, rows[i].pwr);
        {
            int k;
            for (k = 0; rows[i].digs[k] && k < np.cDig; k++) {
                char c = rows[i].digs[k];
                int want = c <= '9' ? c - '0' : c - 'a' + 10;
                CHECKF(rgb[k] == want, "\"%s\" in=0x%lx: digit[%d]=%d want %d", rows[i].s, rows[i].in, k, rgb[k], want);
            }
        }
    }
}

#define NP_LW NUMPRS_LEADING_WHITE
#define NP_TW NUMPRS_TRAILING_WHITE
#define NP_LP NUMPRS_LEADING_PLUS
#define NP_TP NUMPRS_TRAILING_PLUS
#define NP_LM NUMPRS_LEADING_MINUS
#define NP_TM NUMPRS_TRAILING_MINUS
#define NP_HO NUMPRS_HEX_OCT
#define NP_PA NUMPRS_PARENS
#define NP_DE NUMPRS_DECIMAL
#define NP_TH NUMPRS_THOUSANDS
#define NP_CU NUMPRS_CURRENCY
#define NP_EX NUMPRS_EXPONENT
#define NP_UA NUMPRS_USE_ALL
#define NP_NEG NUMPRS_NEG
#define NP_INX NUMPRS_INEXACT

/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
static const PRow basic_rows[] = {
    X("", 0, 0), R("7", 0, 0, 1, 0, 1, 0, 0, "7"), R("10", 0, 0, 1, 0, 2, 0, 1, "1"), R("11", 1, 0, 1, NP_INX, 2, 0, 1, "1"),
    R("01", 0, 0, 1, 0, 2, 0, 0, "1"), R("01", 1, 0, 1, 0, 2, 0, 0, "1"), X("a", 0, 0), R("10", 1, 0, 1, 0, 2, 0, 1, "1"),
    R("0", 0, 0, 1, 0, 1, 0, 0, "0"), R("00", 0, 0, 1, 0, 2, 0, 0, "0"), R("000", 0, 0, 1, 0, 3, 0, 0, "0"), R("1234", 0, 0, 4, 0, 4, 0, 0, "1234"),
    R("1200", 0, 0, 2, 0, 4, 0, 2, "12"), R("9", 0, 0, 1, 0, 1, 0, 0, "9"), R("0123", 0, 0, 3, 0, 4, 0, 0, "123"), R("12abc", 0, 0, 2, 0, 2, 0, 0, "12"),
    R("1 2", 0, 0, 1, 0, 1, 0, 0, "1"), R("123456789", 0, 0, 9, 0, 9, 0, 0, "123456789"), R("12345", 3, 0, 3, NP_INX, 5, 0, 2, "123"),
    R("123456", 4, 0, 4, NP_INX, 6, 0, 2, "1234"), R("1000000", 0, 0, 1, 0, 7, 0, 6, "1"), R("10000001", 7, 0, 1, NP_INX, 8, 0, 7, "1"),
    R("99999999999999999999", 0, 0, 20, 0, 20, 0, 0, "99999999999999999999"), R("12345678901234567890123", 0, 0, 23, 0, 23, 0, 0, "12345678901234567890123"),
    R("5", 1, 0, 1, 0, 1, 0, 0, "5"), R("55", 1, 0, 1, NP_INX, 2, 0, 1, "5"), X("-5", 0, 0),
};
static const PRow white_rows[] = {
    X(" 0", 0, 0), R(" 0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R("\t0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R("\n0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"),
    R("\r0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R("\v0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R("\f0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"),
    R("\xa0" "0", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R("  0", 0, NP_LW, 1, NP_LW, 3, 0, 0, "0"), R("0 ", 0, 0, 1, 0, 1, 0, 0, "0"),
    R("0 ", 0, NP_TW, 1, NP_TW, 2, 0, 0, "0"), R("0  ", 0, NP_TW, 1, NP_TW, 3, 0, 0, "0"), R("0\t", 0, NP_TW, 1, NP_TW, 2, 0, 0, "0"),
    R(" 0 ", 0, NP_LW, 1, NP_LW, 2, 0, 0, "0"), R(" 0 ", 0, NP_LW | NP_TW, 1, NP_LW | NP_TW, 3, 0, 0, "0"), R("0 ", 0, NP_LW, 1, NP_LW & 0, 1, 0, 0, "0"),
    R(" 12 ", 0, NP_LW | NP_TW, 2, NP_LW | NP_TW, 4, 0, 0, "12"), R("   7   ", 0, NP_LW | NP_TW, 1, NP_LW | NP_TW, 7, 0, 0, "7"),
    R("7x", 0, NP_TW, 1, 0, 1, 0, 0, "7"),
};
static const PRow sign_rows[] = {
    X("+0", 0, 0), R("+0", 0, NP_LP, 1, NP_LP, 2, 0, 0, "0"), R("0+", 0, 0, 1, 0, 1, 0, 0, "0"), R("0+", 0, NP_TP, 1, NP_TP, 2, 0, 0, "0"),
    R("+0+", 0, NP_LP, 1, NP_LP, 2, 0, 0, "0"), R("+0+", 0, NP_LP | NP_TP, 1, NP_LP, 2, 0, 0, "0"),
    X("-0", 0, 0), R("-0", 0, NP_LM, 1, NP_NEG | NP_LM, 2, 0, 0, "0"), R("0-", 0, 0, 1, 0, 1, 0, 0, "0"), R("0-", 0, NP_TM, 1, NP_NEG | NP_TM, 2, 0, 0, "0"),
    R("-0-", 0, NP_LM, 1, NP_NEG | NP_LM, 2, 0, 0, "0"), R("-0-", 0, NP_LM | NP_TM, 1, NP_NEG | NP_LM, 2, 0, 0, "0"),
    R("-12", 0, NP_LM, 2, NP_NEG | NP_LM, 3, 0, 0, "12"), R("12-", 0, NP_TM, 2, NP_NEG | NP_TM, 3, 0, 0, "12"), R("+12", 0, NP_LP, 2, NP_LP, 3, 0, 0, "12"),
    R("12+", 0, NP_TP, 2, NP_TP, 3, 0, 0, "12"), R("-12", 0, NP_LP | NP_LM, 2, NP_NEG | NP_LM, 3, 0, 0, "12"), R("+12", 0, NP_LP | NP_LM, 2, NP_LP, 3, 0, 0, "12"),
    R("12-", 0, NP_TP | NP_TM, 2, NP_NEG | NP_TM, 3, 0, 0, "12"), R("12+", 0, NP_TP | NP_TM, 2, NP_TP, 3, 0, 0, "12"),
    R("+ 0 ", 0, NP_LP | NP_LW, 1, NP_LP | NP_LW, 3, 0, 0, "0"), R(" +0", 0, NP_LP | NP_LW, 1, NP_LP | NP_LW, 3, 0, 0, "0"),
    R(" + 0", 0, NP_LP | NP_LW, 1, NP_LP | NP_LW, 4, 0, 0, "0"), X("+ +0", 0, NP_LP | NP_LW), X("--1", 0, NP_LM), 
    R(" -5", 0, NP_LM | NP_LW, 1, NP_NEG | NP_LM | NP_LW, 3, 0, 0, "5"), R("5- ", 0, NP_TM | NP_TW, 1, NP_NEG | NP_TM | NP_TW, 3, 0, 0, "5"),
    X("-", 0, NP_LM), X("+", 0, NP_LP), R("-0.51", 0, NUMPRS_STD, 2, NP_NEG | NP_DE | NP_LM, 5, 0, -2, "51"),
};
static const PRow hexoct_rows[] = {
    R("0", 0, NP_HO, 1, 0, 1, 0, 0, "0"), R("0h", 0, NP_HO, 1, 0, 1, 0, 0, "0"), R("0x", 0, NP_HO, 1, 0, 1, 0, 0, "0"), R("0x0", 0, NP_HO, 1, 0, 1, 0, 0, "0"),
    X("FE", 0, NP_HO), R("0100", 0, NP_HO, 1, 0, 4, 0, 2, "1"),
    R("&HF800", 0, NP_HO, 4, NP_HO, 6, 4, 0, "f800"), R("&h0abcdef", 0, NP_HO, 6, NP_HO, 9, 4, 0, "abcdef"), R("&O300", 0, NP_HO, 3, NP_HO, 5, 3, 0, "300"),
    R("&o0777", 0, NP_HO, 3, NP_HO, 6, 3, 0, "777"), R("&o128", 0, NP_HO, 2, NP_HO, 4, 3, 0, "12"), R("&ha.2", 0, NP_HO | NP_DE, 1, NP_HO, 3, 4, 0, "a"),
    R("01.2", 0, NP_HO | NP_DE, 2, NP_DE, 4, 0, -1, "12"), R("&HFF", 0, NP_HO, 2, NP_HO, 4, 4, 0, "ff"), R("&hff", 0, NP_HO, 2, NP_HO, 4, 4, 0, "ff"),
    R("&H10", 0, NP_HO, 2, NP_HO, 4, 4, 0, "10"), R("&H0", 0, NP_HO, 1, NP_HO, 3, 4, 0, "0"), R("&O7", 0, NP_HO, 1, NP_HO, 3, 3, 0, "7"),
    X("&H12", 0, 0),
    R("&o1e1", 0, NP_HO | NP_EX, 1, NP_HO, 3, 3, 0, "1"), R("&h1e2", 0, NP_HO | NP_EX, 3, NP_HO, 5, 4, 0, "1e2"), R("&h1,000", 0, NP_HO | NP_TH, 1, NP_HO, 3, 4, 0, "1"),
    R("&HFFFFFFFF", 0, NP_HO, 8, NP_HO, 10, 4, 0, "ffffffff"), R("&H7FFFFFFF", 0, NP_HO, 8, NP_HO, 10, 4, 0, "7fffffff"), R("&O17777777777", 0, NP_HO, 11, NP_HO, 13, 3, 0, "17777777777"),
};
static const PRow parens_rows[] = {
    X("()", 0, NP_PA), R("0()", 0, NP_PA, 1, 0, 1, 0, 0, "0"), R("(0)", 0, NP_PA, 1, NP_NEG | NP_PA, 3, 0, 0, "0"), R("(12)", 0, NP_PA, 2, NP_NEG | NP_PA, 4, 0, 0, "12"),
    X("(12)", 0, 0), R("12)", 0, NP_PA, 2, 0, 2, 0, 0, "12"), R(" (12)", 0, NP_PA | NP_LW, 2, NP_NEG | NP_PA | NP_LW, 5, 0, 0, "12"),
    R("(1.5)", 0, NP_PA | NP_DE, 2, NP_NEG | NP_PA | NP_DE, 5, 0, -1, "15"),
};
static const PRow thousands_rows[] = {
    R("0", 0, NP_TH, 1, 0, 1, 0, 0, "0"), R("1,000", 0, 0, 1, 0, 1, 0, 0, "1"), R("1,000", 0, NP_TH, 1, NP_TH, 5, 0, 3, "1"),
    R("1,001.0", 0, NP_TH, 4, NP_TH, 5, 0, 0, "1001"), R("1,,000", 0, NP_TH, 1, NP_TH, 6, 0, 3, "1"), R("1,00,0,,", 0, NP_TH, 1, NP_TH, 8, 0, 3, "1"),
    X(",1,000", 0, NP_TH), R("1,", 0, NP_TH, 1, NP_TH, 2, 0, 0, "1"), R("1,234,567", 0, NP_TH, 7, NP_TH, 9, 0, 0, "1234567"), R("12,345", 0, NP_TH, 5, NP_TH, 6, 0, 0, "12345"),
    R("123,456,789", 0, NP_TH, 9, NP_TH, 11, 0, 0, "123456789"), R("1,2,3", 0, NP_TH, 3, NP_TH, 5, 0, 0, "123"), R("1,234.5", 0, NP_TH | NP_DE, 5, NP_TH | NP_DE, 7, 0, -1, "12345"),
    R("1,234.5", 0, NP_TH, 4, NP_TH, 5, 0, 0, "1234"), R("1.234", 0, NP_TH, 1, 0, 1, 0, 0, "1"),
};
static const PRow currency_rows[] = {
    X("$11", 0, 0), R("11", 0, NP_CU, 2, 0, 2, 0, 0, "11"), X("$", 0, NP_CU), R("$11", 0, NP_CU, 2, NP_CU, 3, 0, 0, "11"), X("$&ha", 0, NP_HO | NP_CU),
    R("&ha$", 0, NP_HO | NP_CU, 1, NP_HO, 3, 4, 0, "a"), X("$$11", 0, NP_CU), R("$11$$", 0, NP_CU | NP_UA, 2, NP_CU, 5, 0, 0, "11"),
    R("$-11", 0, NP_CU | NP_LM, 2, NP_CU | NP_LM | NP_NEG, 4, 0, 0, "11"), R("-$11", 0, NP_CU | NP_LM, 2, NP_CU | NP_LM | NP_NEG, 4, 0, 0, "11"),
    X("$ 11", 0, NP_CU | NP_UA), R("$ 11", 0, NP_CU | NP_LW, 2, NP_CU | NP_LW, 4, 0, 0, "11"), X("11 $", 0, NP_CU | NP_UA), X("11 $", 0, NP_CU | NP_TH | NP_UA),
    R("11 $", 0, NP_CU | NP_TW | NP_UA, 2, NP_CU | NP_TW, 4, 0, 0, "11"), R("$11.1", 0, NP_CU, 2, NP_CU, 3, 0, 0, "11"),
    R("$11.1", 0, NP_CU | NP_DE, 3, NP_CU | NP_DE, 5, 0, -1, "111"), R("1$99", 0, NP_CU | NP_DE, 1, NP_CU, 2, 0, 0, "1"), R("$1,234", 0, NP_CU | NP_TH, 4, NP_CU | NP_TH, 6, 0, 0, "1234"),
    R("$1,234.5", 0, NP_CU | NP_TH | NP_DE, 5, NP_CU | NP_TH | NP_DE, 8, 0, -1, "12345"), R("$.02", 0, NP_CU | NP_DE, 1, NP_CU | NP_DE, 4, 0, -2, "2"),
    R("$0", 0, NP_CU, 1, NP_CU, 2, 0, 0, "0"), R("$100", 0, NP_CU, 1, NP_CU, 4, 0, 2, "1"), R("$1", 0, NP_CU, 1, NP_CU, 2, 0, 0, "1"),
};
static const PRow decimal_rows[] = {
    R("1.1", 0, NP_DE, 2, NP_DE, 3, 0, -1, "11"), R("1.", 0, NP_DE, 1, NP_DE, 2, 0, 0, "1"), R(".2", 0, NP_DE, 1, NP_DE, 2, 0, -1, "2"), R(".0", 0, NP_DE, 1, NP_DE, 2, 0, -1, "0"),
    R(".000", 0, NP_DE, 1, NP_DE, 4, 0, -3, "0"), R(".001", 0, NP_DE, 1, NP_DE, 4, 0, -3, "1"), R(".101", 0, NP_DE, 3, NP_DE, 4, 0, -3, "101"), R(".30", 0, NP_DE, 1, NP_DE, 3, 0, -1, "3"),
    X(".", 0, NP_DE), R("1.1.", 0, NP_DE, 2, NP_DE, 3, 0, -1, "11"), R("1..1", 0, NP_DE, 1, NP_DE, 2, 0, 0, "1"), R("10.1", 0, NUMPRS_STD, 3, NP_DE, 4, 0, -1, "101"),
    R("0.01", 0, NUMPRS_STD, 1, NP_DE, 4, 0, -2, "1"), R("0.10", 0, NUMPRS_STD, 1, NP_DE, 4, 0, -1, "1"), R("10.5", 0, NP_DE, 3, NP_DE, 4, 0, -1, "105"),
    R("100.5", 0, NP_DE, 4, NP_DE, 5, 0, -1, "1005"), R("0.5", 0, NP_DE, 1, NP_DE, 3, 0, -1, "5"), R("00.5", 0, NP_DE, 1, NP_DE, 4, 0, -1, "5"), R("1.50", 0, NP_DE, 2, NP_DE, 4, 0, -1, "15"),
    R("1.5", 0, 0, 1, 0, 1, 0, 0, "1"), R("3.14159", 0, NP_DE, 6, NP_DE, 7, 0, -5, "314159"), R("12.", 0, NP_DE, 2, NP_DE, 3, 0, 0, "12"), R("120.", 0, NP_DE, 2, NP_DE, 4, 0, 1, "12"),
    R("0.0", 0, NP_DE, 1, NP_DE, 3, 0, -1, "0"),
};
static const PRow exponent_rows[] = {
    R("1e1", 0, 0, 1, 0, 1, 0, 0, "1"), R("1e1", 0, NP_EX, 1, NP_EX, 3, 0, 1, "1"), R("1 e1", 0, NP_EX | NP_TW, 1, NP_TW, 2, 0, 0, "1"), R("1e-1", 0, NP_EX, 1, NP_EX, 4, 0, -1, "1"),
    R("1e+01", 0, NP_EX, 1, NP_EX, 5, 0, 1, "1"), R("1e0", 0, NP_EX, 1, NP_EX, 3, 0, 0, "1"), R("1e+0", 0, NP_EX, 1, NP_EX, 4, 0, 0, "1"), R("1e-0", 0, NP_EX, 1, NP_EX, 4, 0, 0, "1"),
    R("1e1.", 0, NP_EX, 1, NP_EX, 3, 0, 1, "1"), R("1.5e20", 0, NP_EX | NP_DE, 2, NP_EX | NP_DE, 6, 0, 19, "15"), R("1.5e-20", 0, NP_EX | NP_DE, 2, NP_EX | NP_DE, 7, 0, -21, "15"),
    R("1E5", 0, NP_EX, 1, NP_EX, 3, 0, 5, "1"), R("1E+5", 0, NP_EX, 1, NP_EX, 4, 0, 5, "1"), R("2e10", 0, NP_EX, 1, NP_EX, 4, 0, 10, "2"), R("12e2", 0, NP_EX, 2, NP_EX, 4, 0, 2, "12"),
    R("1d5", 0, NP_EX, 1, NP_EX, 3, 0, 5, "1"), 
    R("0e5", 0, NP_EX, 1, NP_EX, 3, 0, 5, "0"), R("1.5E3", 0, NP_EX | NP_DE, 2, NP_EX | NP_DE, 5, 0, 2, "15"), R("100e-2", 0, NP_EX, 1, NP_EX, 6, 0, 0, "1"),
};
static const PRow useall_rows[] = {
    R("0", 0, NP_UA, 1, 0, 1, 0, 0, "0"), X("0 ", 0, NP_UA), R("0 ", 0, NP_UA | NP_TW, 1, NP_TW, 2, 0, 0, "0"), X("0x", 0, NP_UA), X("12abc", 0, NP_UA), R("12", 0, NP_UA, 2, 0, 2, 0, 0, "12"),
    X("1.5", 0, NP_UA), R("1.5", 0, NP_UA | NP_DE, 2, NP_DE, 3, 0, -1, "15"), X("1,000", 0, NP_UA), R("1,000", 0, NP_UA | NP_TH, 1, NP_TH, 5, 0, 3, "1"), X("1e5", 0, NP_UA),
    R("1e5", 0, NP_UA | NP_EX, 1, NP_EX, 3, 0, 5, "1"), X("-1", 0, NP_UA), R("-1", 0, NP_UA | NP_LM, 1, NP_NEG | NP_LM, 2, 0, 0, "1"), X("1-", 0, NP_UA), R("1-", 0, NP_UA | NP_TM, 1, NP_NEG | NP_TM, 2, 0, 0, "1"),
    X("(1)", 0, NP_UA), R("(1)", 0, NP_UA | NP_PA, 1, NP_NEG | NP_PA, 3, 0, 0, "1"),
};
static const PRow combo_rows[] = {
    R("+ 0 ", 0, NP_LP | NP_LW, 1, NP_LP | NP_LW, 3, 0, 0, "0"), R("10", 1, 0, 1, 0, 2, 0, 1, "1"), R("-0.51", 0, NUMPRS_STD, 2, NP_NEG | NP_DE | NP_LM, 5, 0, -2, "51"),
    R("10.1", 0, NUMPRS_STD, 3, NP_DE, 4, 0, -1, "101"), R("$1,234.56", 0, NUMPRS_STD, 6, NUMPRS_CURRENCY | NP_TH | NP_DE, 9, 0, -2, "123456"),
    R(" -1,234.5e2 ", 0, NUMPRS_STD, 5, NP_LW | NP_LM | NP_NEG | NP_TH | NP_DE | NP_EX | NP_TW, 12, 0, 1, "12345"),
    R("(1,000.50)", 0, NUMPRS_STD, 5, NP_PA | NP_NEG | NP_TH | NP_DE, 10, 0, -1, "10005"), R("1.0e3", 0, NUMPRS_STD, 1, NP_DE | NP_EX, 5, 0, 3, "1"),
    R("-5", 0, NUMPRS_STD, 1, NP_NEG | NP_LM, 2, 0, 0, "5"), R("5-", 0, NUMPRS_STD, 1, NP_NEG | NP_TM, 2, 0, 0, "5"), R("+5", 0, NUMPRS_STD, 1, NP_LP, 2, 0, 0, "5"),
};

#define PTEST(name, rows) TEST(name) { REQUIRE(U(VarParseNumFromStr)); run_prows(rows, sizeof rows / sizeof *rows, VFP_EN_US); }
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_basic_digits, basic_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_whitespace, white_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_signs, sign_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_hex_oct, hexoct_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_parens, parens_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_thousands, thousands_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_currency, currency_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_decimal, decimal_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_exponent, exponent_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_use_all, useall_rows)
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
PTEST(vfp_parsenum_flag_combinations, combo_rows)

TEST(vfp_parsenum_null_arguments)
{
    NUMPARSE np;
    BYTE rgb[16];
    REQUIRE(U(VarParseNumFromStr));
    memset(&np, 0, sizeof np); np.cDig = 16;
    EXPECT_EQ(U(VarParseNumFromStr)(L"1", VFP_EN_US, 0, NULL, rgb), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarParseNumFromStr)(L"1", VFP_EN_US, 0, &np, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarParseNumFromStr)(NULL, VFP_EN_US, 0, NULL, NULL), VFP_E_INVALIDARG);
    EXPECT_EQ(U(VarParseNumFromStr)(NULL, VFP_EN_US, 0, &np, rgb), VFP_E_TYPEMISMATCH);
}
TEST(vfp_parsenum_input_fields_preserved)
{
    NUMPARSE np;
    BYTE rgb[128];
    HRESULT hr;
    REQUIRE(U(VarParseNumFromStr));
    hr = parse_one(L"12", 0, NUMPRS_LEADING_WHITE | NUMPRS_DECIMAL, VFP_EN_US, &np, rgb);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(np.dwInFlags, NUMPRS_LEADING_WHITE | NUMPRS_DECIMAL);
    /* unrecognised input flag bits are carried through, not interpreted */
    hr = parse_one(L"12", 0, 0x80000000, VFP_EN_US, &np, rgb);
    t_note("unknown dwInFlags bit 0x80000000: hr=0x%lx dwInFlags=0x%lx", (unsigned long)hr, np.dwInFlags);
}
TEST(vfp_parsenum_failure_leaves_output_defined)
{
    /* on DISP_E_TYPEMISMATCH the call must not report a success value */
    NUMPARSE np;
    BYTE rgb[128];
    HRESULT hr;
    REQUIRE(U(VarParseNumFromStr));
    hr = parse_one(L"abc", 0, 0, VFP_EN_US, &np, rgb);
    EXPECT_EQ(hr, VFP_E_TYPEMISMATCH);
    hr = parse_one(L"", 0, NUMPRS_STD, VFP_EN_US, &np, rgb);
    EXPECT_EQ(hr, VFP_E_TYPEMISMATCH);
    hr = parse_one(L"   ", 0, NUMPRS_STD, VFP_EN_US, &np, rgb);
    EXPECT_EQ(hr, VFP_E_TYPEMISMATCH);
}
TEST(vfp_parsenum_digit_capacity_boundaries)
{
    /* cDig input limits the number of stored digits; extra significant digits are dropped and flagged INEXACT, the power of ten compensates */
    static const struct { const char *s; int cap; int cdig; ULONG out; int pwr; } rows[] = {
        { "123456789", 9, 9, 0, 0 }, { "123456789", 8, 8, NUMPRS_INEXACT, 1 }, { "123456789", 1, 1, NUMPRS_INEXACT, 8 }, { "123456789", 5, 5, NUMPRS_INEXACT, 4 },
        { "100", 1, 1, 0, 2 }, { "1000000", 1, 1, 0, 6 }, { "1000001", 1, 1, NUMPRS_INEXACT, 6 }, { "1000001", 7, 7, 0, 0 }, { "10", 1, 1, 0, 1 },
    };
    size_t i;
    REQUIRE(U(VarParseNumFromStr));
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        NUMPARSE np; BYTE rgb[128];
        HRESULT hr = parse_one(vfp_w(rows[i].s), rows[i].cap, 0, VFP_EN_US, &np, rgb);
        CHECKF(hr == S_OK && np.cDig == rows[i].cdig && np.dwOutFlags == rows[i].out && np.nPwr10 == rows[i].pwr && np.cchUsed == (int)strlen(rows[i].s),
               "%s cap=%d: hr=0x%lx cDig=%d out=0x%lx pwr=%d cch=%d", rows[i].s, rows[i].cap, (unsigned long)hr, np.cDig, np.dwOutFlags, np.nPwr10, np.cchUsed);
    }
}
TEST(vfp_parsenum_incomplete_exponent_observation)
{
    static const char *const in[] = { "1e", "1e+", "1ex", "1e-", "(12", "-+1", "+-1" };
    size_t i;
    REQUIRE(U(VarParseNumFromStr));
    for (i = 0; i < sizeof in / sizeof *in; i++) {
        NUMPARSE np; BYTE rgb[128];
        HRESULT hr = parse_one(vfp_w(in[i]), 0, NUMPRS_STD, VFP_EN_US, &np, rgb);
        t_note("\"%s\" STD: hr=0x%lx cDig=%d out=0x%lx cch=%d pwr=%d", in[i], (unsigned long)hr, np.cDig, np.dwOutFlags, np.cchUsed, np.nPwr10);
    }
}
TEST(vfp_parsenum_tiny_capacity_overflow)
{
    /* cDig = 0 cannot hold any digit; observation only */
    NUMPARSE np; BYTE rgb[8];
    HRESULT hr;
    REQUIRE(U(VarParseNumFromStr));
    memset(&np, 0, sizeof np); np.cDig = 0; np.dwInFlags = 0;
    hr = U(VarParseNumFromStr)(L"12", VFP_EN_US, LOCALE_NOUSEROVERRIDE, &np, rgb);
    t_note("cDig=0 -> hr=0x%lx cDig=%d out=0x%lx", (unsigned long)hr, np.cDig, np.dwOutFlags);
}

/* ---- locales: decimal comma and thousands dot (de-DE / es-ES are stable across Win10 builds) --------- */
TEST(vfp_parsenum_german_separators)
{
    static const PRow rows[] = {
        R("1,5", 0, NP_DE, 2, NP_DE, 3, 0, -1, "15"), R("1.5", 0, NP_DE, 1, 0, 1, 0, 0, "1"), R("1.234", 0, NP_TH, 4, NP_TH, 5, 0, 0, "1234"),
        R("1.234,5", 0, NP_TH | NP_DE, 5, NP_TH | NP_DE, 7, 0, -1, "12345"), R("1.234.567", 0, NP_TH, 7, NP_TH, 9, 0, 0, "1234567"), R("12,50", 0, NP_DE, 3, NP_DE, 5, 0, -1, "125"),
        R("0,05", 0, NP_DE, 1, NP_DE, 4, 0, -2, "5"), R("1,5e2", 0, NP_DE | NP_EX, 2, NP_DE | NP_EX, 5, 0, 1, "15"),
    };
    REQUIRE(U(VarParseNumFromStr));
    run_prows(rows, sizeof rows / sizeof *rows, VFP_DE_DE);
}
TEST(vfp_parsenum_english_separators_in_german_locale_rejected)
{
    NUMPARSE np; BYTE rgb[128];
    HRESULT hr;
    REQUIRE(U(VarParseNumFromStr));
    hr = parse_one(L"1.5", 0, NUMPRS_DECIMAL | NUMPRS_USE_ALL, VFP_DE_DE, &np, rgb);
    CHECKF(hr == VFP_E_TYPEMISMATCH, "hr=0x%lx", (unsigned long)hr);
    hr = parse_one(L"1,000", 0, NUMPRS_THOUSANDS | NUMPRS_USE_ALL, VFP_EN_US, &np, rgb);
    EXPECT_EQ(hr, S_OK);
}
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_parsenum_non_ascii_digits_rejected)
{
    NUMPARSE np; BYTE rgb[128];
    REQUIRE(U(VarParseNumFromStr));
    EXPECT_EQ(parse_one(L"\x660", 0, NUMPRS_STD, VFP_EN_US, &np, rgb), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(parse_one(L"\x661", 0, NUMPRS_STD, VFP_EN_US, &np, rgb), VFP_E_TYPEMISMATCH);
    EXPECT_EQ(parse_one(L"\xff11", 0, NUMPRS_STD, VFP_EN_US, &np, rgb), VFP_E_TYPEMISMATCH);   /* fullwidth 1: only mapped for CJK locales */
}
/* adapted from Wine dlls/oleaut32/tests/vartest.c (LGPL-2.1+) */
TEST(vfp_parsenum_pound_sign_not_currency)
{
    NUMPARSE np; BYTE rgb[128];
    REQUIRE(U(VarParseNumFromStr));
    EXPECT_EQ(parse_one(L"\x31\xa3", 0, NUMPRS_CURRENCY | NUMPRS_USE_ALL, VFP_EN_US, &np, rgb), VFP_E_TYPEMISMATCH);
}
TEST(vfp_parsenum_ideographic_space_not_white)
{
    /* U+3000 is not consumed by LEADING_WHITE in a non-CJK locale; observation only */
    NUMPARSE np; BYTE rgb[128];
    HRESULT hr;
    REQUIRE(U(VarParseNumFromStr));
    hr = parse_one(L"\x3000" L"1", 0, NUMPRS_LEADING_WHITE, VFP_EN_US, &np, rgb);
    t_note("U+3000 as leading white (en-US): hr=0x%lx", (unsigned long)hr);
}
