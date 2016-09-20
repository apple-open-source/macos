/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app/SecurityTestOSX.app in the Release, Debug schemes, and World schemes
 3) Add any resource your test needs in to the SecurityTest.app and SecurityTestOSX.app targets.

 This file contains iOS/OSX shared tests that are built in libSharedRegression.a
 For iOS only tests see Security_regressions.h
 */
#include <test/testmore.h>

ONE_TEST(si_15_certificate)
ONE_TEST(si_16_ec_certificate)
ONE_TEST(si_20_sectrust)
ONE_TEST(si_20_sectrust_policies)
ONE_TEST(si_21_sectrust_asr)
ONE_TEST(si_22_sectrust_iap)
#if !TARGET_OS_WATCH
ONE_TEST(si_23_sectrust_ocsp)
#else
DISABLED_ONE_TEST(si_23_sectrust_ocsp)
#endif
ONE_TEST(si_24_sectrust_itms)
ONE_TEST(si_24_sectrust_nist)
ONE_TEST(si_24_sectrust_diginotar)
ONE_TEST(si_24_sectrust_digicert_malaysia)
ONE_TEST(si_24_sectrust_passbook)
ONE_TEST(si_26_sectrust_copyproperties)
ONE_TEST(si_27_sectrust_exceptions)
ONE_TEST(si_28_sectrustsettings)
ONE_TEST(si_44_seckey_gen)
ONE_TEST(si_44_seckey_rsa)
ONE_TEST(si_44_seckey_ec)
ONE_TEST(si_44_seckey_ies)
#if !TARGET_OS_WATCH
ONE_TEST(si_67_sectrust_blacklist)
#else
DISABLED_ONE_TEST(si_67_sectrust_blacklist)
#endif
ONE_TEST(si_70_sectrust_unified)
ONE_TEST(si_71_mobile_store_policy)
ONE_TEST(si_74_OTA_PKI_Signer)
ONE_TEST(si_82_seccertificate_ct)
ONE_TEST(si_82_sectrust_ct)
ONE_TEST(si_83_seccertificate_sighashalg)
ONE_TEST(si_85_sectrust_ssl_policy)
ONE_TEST(si_87_sectrust_name_constraints)
ONE_TEST(si_97_sectrust_path_scoring)
