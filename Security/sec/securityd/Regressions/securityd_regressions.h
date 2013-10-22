/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app in the Release and Debug schemes
 */
#include <test/testmore.h>

ONE_TEST(sd_10_policytree)
#ifdef NO_SERVER
ONE_TEST(sd_70_engine)
#else
OFF_ONE_TEST(sd_70_engine)
#endif
