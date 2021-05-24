/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app/SecurityTestOSX.app in the Release, Debug schemes, and World schemes
 3) Add any resource your test needs in to the SecurityTest.app, SecurityDevTest.app, and SecurityTestOSX.app targets.

 This file contains iOS/OSX shared tests that are built in libSharedRegression.a
 For iOS-only tests see Security_regressions.h
 */
#include <regressions/test/testmore.h>

ONE_TEST(si_25_cms_skid)
ONE_TEST(si_26_cms_apple_signed_samples)
ONE_TEST(si_27_cms_parse)
ONE_TEST(si_29_cms_chain_mode)
ONE_TEST(si_34_cms_timestamp)
ONE_TEST(si_35_cms_expiration_time)
ONE_TEST(si_44_seckey_gen)
ONE_TEST(si_44_seckey_rsa)
ONE_TEST(si_44_seckey_ec)
ONE_TEST(si_44_seckey_ies)
ONE_TEST(si_44_seckey_aks)
#if TARGET_OS_IOS && !TARGET_OS_SIMULATOR
ONE_TEST(si_44_seckey_fv)
#endif
ONE_TEST(si_44_seckey_proxy)
ONE_TEST(si_60_cms)
ONE_TEST(si_61_pkcs12)
ONE_TEST(si_62_csr)
ONE_TEST(si_64_ossl_cms)
ONE_TEST(si_65_cms_cert_policy)
ONE_TEST(si_66_smime)
ONE_TEST(si_68_secmatchissuer)
ONE_TEST(si_89_cms_hash_agility)
ONE_TEST(rk_01_recoverykey)

ONE_TEST(padding_00_mmcs)
