/* VarFormat / VarTokenizeFormatString: an E (or e) that ends the format after a number picture is not text. Windows 10 22H2. en-US only. */
#include "t.h"
#include <oleauto.h>
#include <wchar.h>

static void fe_row(double v, const WCHAR *fmt, const WCHAR *want)
{
    VARIANT var; BSTR b = NULL; HRESULT h;
    memset(&var, 0, sizeof var); V_VT(&var) = VT_R8; V_R8(&var) = v;
    h = U(VarFormat)(&var, (LPOLESTR)fmt, 1, 1, 0, &b);
    t_check(h == S_OK && b && !lstrcmpW(b, want), __FILE__, __LINE__, "VarFormat", "fmt \"%ls\" value %g hr 0x%08lx got \"%ls\" want \"%ls\"", fmt, v, (unsigned long)h, b ? b : L"<null>", want);
    if (b) U(SysFreeString)(b);
}

TEST(fmt_e_trailing_is_dropped)
{
    REQUIRE(U(VarFormat));
    if (GetUserDefaultLCID() != 0x409) { t_skip("user locale is not en-US"); return; }
    fe_row(1.0, L"0E", L"1");
    fe_row(1234.5678, L"0E", L"1235");
    fe_row(0.0, L"0E", L"0");
    fe_row(-1.0, L"0E", L"-1");
    fe_row(10000000000.0, L"0E", L"10000000000");
    fe_row(1234.5678, L"0e", L"1235");
    fe_row(1.0, L"#E", L"1");
    fe_row(1.0, L"0 E", L"1 ");
    fe_row(1234.5678, L"0.0E", L"1234.6");
    fe_row(1234.5678, L"0.00e", L"1234.57");
    fe_row(1234.5678, L"#,##0E", L"1,235");
    fe_row(1234.5678, L"00E", L"1235");
    fe_row(1.0, L"00E", L"01");
    fe_row(1.0, L"0,E", L"0");
    fe_row(1.0, L"0.E", L"1.");
    fe_row(1.0, L".E", L"1.");
    fe_row(1.0, L"0 e", L"1 ");
}

TEST(fmt_e_elsewhere_is_text_or_exponent)
{
    REQUIRE(U(VarFormat));
    if (GetUserDefaultLCID() != 0x409) { t_skip("user locale is not en-US"); return; }
    fe_row(1.0, L"0E ", L"1E ");
    fe_row(1234.5678, L"0E ", L"1235E ");
    fe_row(1.0, L"0EE", L"1E");
    fe_row(1.0, L"0E;0E", L"1E");
    fe_row(1.0, L"0E;-0E", L"1E");
    fe_row(1.0, L"0E%", L"100E%");
    fe_row(1.0, L"0E.", L"1E.");
    fe_row(1.0, L"0E.0", L"1E.0");
    fe_row(1.0, L"0E,", L"0E");
    fe_row(1.0, L"0.0E\"x\"", L"1.0Ex");
    fe_row(1.0, L"0\\E", L"1E");
    fe_row(1.0, L"0 E+0", L"1 E+0");
    fe_row(1.0, L"E", L"E");
    fe_row(1.0, L"e", L"e");
    fe_row(1.0, L"E0", L"E1");
    fe_row(1.0, L"E+", L"E+");
    fe_row(1.0, L"E-", L"E-");
    fe_row(1.0, L"\"E\"0", L"E1");
    fe_row(1.0, L"%0E", L"%1");
    fe_row(1.0, L"0E+E", L"1E+0E");
    fe_row(1.0, L"0.0E+0E", L"1.0E+0E");
    /* the signed forms are the exponent */
    fe_row(1.0, L"0E+", L"1E+0");
    fe_row(1234.5678, L"0E+", L"1E+3");
    fe_row(1.0, L"0E-", L"1E0");
    fe_row(1.0, L"0e+", L"1e+0");
    fe_row(1.0, L"0E+00", L"1E+00");
    fe_row(1.0, L"000E-000", L"100E-002");
}

static void fe_tok(const WCHAR *fmt, const char *want_hex)
{
    BYTE tok[300]; INT cb = 300; HRESULT h; char o[700]; int n = 0, i;
    memset(tok, 0xcc, sizeof tok);
    h = U(VarTokenizeFormatString)((LPOLESTR)fmt, tok, sizeof tok, 1, 1, 0x409, &cb);
    for (i = 0; i < cb && i < 200 && h == S_OK; i++) n += sprintf(o + n, "%02x", tok[i]);
    o[n] = 0;
    t_check(h == S_OK && !strcmp(o, want_hex), __FILE__, __LINE__, "VarTokenizeFormatString", "fmt \"%ls\" hr 0x%08lx got %s want %s", fmt, (unsigned long)h, o, want_hex);
}

TEST(fmt_e_tokens)
{
    REQUIRE(U(VarTokenizeFormatString));
    fe_tok(L"0E", "0e02060000000000000100340102");
    fe_tok(L"0e", "0e02060000000000000100340102");
    fe_tok(L"0 E", "1102060000000000000100340100010102");
    fe_tok(L"#E", "0e02060000000000000100350102");
    fe_tok(L"0.0E", "1102060000000000000101340136340102");
    fe_tok(L"0E ", "1102060000000000000100340100010202");
    fe_tok(L"0EE", "1102060000000000000100340100010102");
    fe_tok(L"E", "0f0006000000000000000000000102");
    fe_tok(L"0E+", "10020600000002000001003401370002");
    fe_tok(L"0E-", "10020600000002000001003401380002");
}
