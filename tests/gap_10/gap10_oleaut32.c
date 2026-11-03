/* gap10_oleaut32: the two internal oleaut32 exports not covered elsewhere. */
#include "gap10.h"

/* LPSAFEARRAY_Size(flags, start, ppsa): wire-size helper under LPSAFEARRAY_UserSize.
 * NULL pointer-to-pointer: size unchanged. NULL array: align(start, 4) + 4 (the referent id). */
TEST(gap10_lpsafearray_size_null_cases) {
    NOT_WINE("internal export");
    FNIN("oleaut32.dll", ULONG, LPSAFEARRAY_Size, (ULONG *, ULONG, SAFEARRAY **));
    ULONG flags[4] = { 0, 0, 0, 0 };
    SAFEARRAY *psa = NULL;
    for (ULONG start = 0; start < 12; start++) {
        EXPECT_EQ(LPSAFEARRAY_Size_(flags, start, NULL), start);
        EXPECT_EQ(LPSAFEARRAY_Size_(flags, start, &psa), ((start + 3) & ~3u) + 4);
    }
    EXPECT_EQ(LPSAFEARRAY_Size_(flags, 0x1000, &psa), 0x1004);
    EXPECT_EQ(LPSAFEARRAY_Size_(flags, 0x1001, &psa), 0x1008);
}
/* the 64-bit marshalling flag word changes nothing for a NULL array */
TEST(gap10_lpsafearray_size_null_cases_independent_of_context) {
    NOT_WINE("internal export");
    FNIN("oleaut32.dll", ULONG, LPSAFEARRAY_Size, (ULONG *, ULONG, SAFEARRAY **));
    SAFEARRAY *psa = NULL;
    static const ULONG ctx[] = { 0, 1, 2, 3, 4 };      /* MSHCTX_* values */
    for (int i = 0; i < 5; i++) {
        ULONG flags[4] = { ctx[i], 0, 0, 0 };
        EXPECT_EQ(LPSAFEARRAY_Size_(flags, 5, &psa), 12);
    }
}
