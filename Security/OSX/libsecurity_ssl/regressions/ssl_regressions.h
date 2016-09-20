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
ONE_TEST(ssl_48_split)
// This one require a version of coreTLS that support SNI server side. (> coreTLS-17 ?)
OFF_ONE_TEST(ssl_49_sni)
OFF_ONE_TEST(ssl_50_server)

ONE_TEST(ssl_51_state)
ONE_TEST(ssl_52_noconn)
ONE_TEST(ssl_53_clientauth)
ONE_TEST(ssl_54_dhe)
ONE_TEST(ssl_55_sessioncache)
ONE_TEST(ssl_56_renegotiate)

