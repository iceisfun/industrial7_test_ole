/* Extra review-pass tests: integer->float/currency exactness, decomp-verified. */
#include "vcn.h"

TEST(vcn_extra_cy_from_i8_boundaries)
{
    REQUIRE(U(VarCyFromI8));
    CY c;
    c.int64 = 0x77; EXPECT_EQ(U(VarCyFromI8)(922337203685476LL, &c), S_OK); EXPECT_EQ(c.int64, 922337203685476LL * 10000);
    c.int64 = 0x77; EXPECT_EQ(U(VarCyFromI8)(-922337203685476LL, &c), S_OK); EXPECT_EQ(c.int64, -922337203685476LL * 10000);
    c.int64 = 0x77; EXPECT_EQ(U(VarCyFromI8)(922337203685477LL, &c), DISP_E_OVERFLOW); EXPECT_EQ(c.int64, 0x77);
    c.int64 = 0x77; EXPECT_EQ(U(VarCyFromI8)(-922337203685477LL, &c), DISP_E_OVERFLOW); EXPECT_EQ(c.int64, 0x77);
    EXPECT_EQ(U(VarCyFromI8)(0, &c), S_OK); EXPECT_EQ(c.int64, 0);
    EXPECT_EQ(U(VarCyFromI8)(1, &c), S_OK); EXPECT_EQ(c.int64, 10000);
}

TEST(vcn_extra_r4_r8_from_wide_ints)
{
    REQUIRE(U(VarR4FromI8)); REQUIRE(U(VarR4FromUI8)); REQUIRE(U(VarR8FromI8)); REQUIRE(U(VarR8FromUI8));
    REQUIRE(U(VarR4FromI4)); REQUIRE(U(VarR4FromUI4));
    float f; double d;
    /* 2^24+1 is not representable in float: nearest-even gives 2^24; 2^24+3 gives 2^24+4 */
    EXPECT_EQ(U(VarR4FromI4)(16777217, &f), S_OK);  CHECK(f == 16777216.0f);
    EXPECT_EQ(U(VarR4FromI4)(16777219, &f), S_OK);  CHECK(f == 16777220.0f);
    EXPECT_EQ(U(VarR4FromUI4)(0xffffffffUL, &f), S_OK); CHECK(f == 4294967296.0f);
    EXPECT_EQ(U(VarR4FromI8)(16777217LL, &f), S_OK); CHECK(f == 16777216.0f);
    EXPECT_EQ(U(VarR4FromI8)(-16777217LL, &f), S_OK); CHECK(f == -16777216.0f);
    EXPECT_EQ(U(VarR4FromUI8)(~0ULL, &f), S_OK); CHECK(f == 18446744073709551616.0f);
    /* 2^53+1 -> 2^53, 2^53+3 -> 2^53+4 */
    EXPECT_EQ(U(VarR8FromI8)(9007199254740993LL, &d), S_OK); CHECK(d == 9007199254740992.0);
    EXPECT_EQ(U(VarR8FromI8)(9007199254740995LL, &d), S_OK); CHECK(d == 9007199254740996.0);
    EXPECT_EQ(U(VarR8FromI8)(-9223372036854775807LL - 1, &d), S_OK); CHECK(d == -9223372036854775808.0);
    EXPECT_EQ(U(VarR8FromUI8)(~0ULL, &d), S_OK); CHECK(d == 18446744073709551616.0);
    EXPECT_EQ(U(VarR8FromUI8)(0x8000000000000001ULL, &d), S_OK); CHECK(d == 9223372036854775808.0);
}

TEST(vcn_extra_bool_from_all_widths)
{
    REQUIRE(U(VarBoolFromI1)); REQUIRE(U(VarBoolFromUI1)); REQUIRE(U(VarBoolFromI2)); REQUIRE(U(VarBoolFromUI2));
    REQUIRE(U(VarBoolFromI4)); REQUIRE(U(VarBoolFromUI4)); REQUIRE(U(VarBoolFromI8)); REQUIRE(U(VarBoolFromUI8));
    REQUIRE(U(VarBoolFromR4)); REQUIRE(U(VarBoolFromR8)); REQUIRE(U(VarBoolFromCy));
    VARIANT_BOOL b = 0x33;
    EXPECT_EQ(U(VarBoolFromI1)(-128, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromUI1)(0x80, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromI2)(0x7fff, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromUI2)(0, &b), S_OK); EXPECT_EQ(b, VARIANT_FALSE);
    EXPECT_EQ(U(VarBoolFromI4)(0x10000, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromUI4)(0x80000000UL, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromI8)(0x100000000LL, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromUI8)(0x8000000000000000ULL, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    EXPECT_EQ(U(VarBoolFromR4)(1e-30f, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);   /* no rounding: != 0 */
    EXPECT_EQ(U(VarBoolFromR8)(-0.0, &b), S_OK); EXPECT_EQ(b, VARIANT_FALSE);
    EXPECT_EQ(U(VarBoolFromR8)(0.4, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    CY c; c.int64 = 1;
    EXPECT_EQ(U(VarBoolFromCy)(c, &b), S_OK); EXPECT_EQ(b, VARIANT_TRUE);
    c.int64 = 0;
    EXPECT_EQ(U(VarBoolFromCy)(c, &b), S_OK); EXPECT_EQ(b, VARIANT_FALSE);
}
