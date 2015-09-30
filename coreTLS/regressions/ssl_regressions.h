#include <test/testmore.h>

DISABLED_ONE_TEST(ssl_39_echo)

#if TARGET_OS_IPHONE

ONE_TEST(ssl_40_clientauth)
ONE_TEST(ssl_41_clientauth)

#else

DISABLED_ONE_TEST(ssl_40_clientauth)
DISABLED_ONE_TEST(ssl_41_clientauth)

#endif

ONE_TEST(ssl_42_ciphers)

OFF_ONE_TEST(ssl_43_ciphers)

ONE_TEST(ssl_44_crashes)
ONE_TEST(ssl_45_tls12)
ONE_TEST(ssl_46_SSLGetSupportedCiphers)
ONE_TEST(ssl_47_falsestart)

