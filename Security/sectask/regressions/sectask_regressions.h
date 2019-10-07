#include <regressions/test/testmore.h>

#if !TARGET_OS_SIMULATOR
ONE_TEST(sectask_10_sectask_self)
ONE_TEST(sectask_10_sectask)
#else
OFF_ONE_TEST(sectask_10_sectask_self)
OFF_ONE_TEST(sectask_10_sectask)
#endif
