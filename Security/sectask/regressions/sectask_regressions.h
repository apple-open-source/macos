#include <test/testmore.h>

#if !TARGET_IPHONE_SIMULATOR
ONE_TEST(sectask_10_sectask_self)
ONE_TEST(sectask_11_sectask_audittoken)
#else
OFF_ONE_TEST(sectask_10_sectask_self)
OFF_ONE_TEST(sectask_11_sectask_audittoken)
#endif
