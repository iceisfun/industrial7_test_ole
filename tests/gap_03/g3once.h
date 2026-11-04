/* t.h's EXPECT_EQ / EXPECT_NE evaluate their first argument twice (once for the comparison, once for the message).
 * Many gap_03 checks call functions with side effects (remove, close, reset), so evaluate each operand exactly once. */
#ifndef G3ONCE_H
#define G3ONCE_H
#include "t.h"
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR g3a_ = (ULONG_PTR)(a), g3b_ = (ULONG_PTR)(b); \
    t_check(g3a_ == g3b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)g3a_, (unsigned long long)g3b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR g3a_ = (ULONG_PTR)(a), g3b_ = (ULONG_PTR)(b); \
    t_check(g3a_ != g3b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)g3a_); } while (0)
#endif
