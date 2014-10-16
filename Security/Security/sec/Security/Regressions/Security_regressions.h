/* To add a test:
 1) add it here
 2) Add it as command line argument for SecurityTest.app in the Release and Debug schemes
 */
#include <test/testmore.h>

ONE_TEST(pbkdf2_00_hmac_sha1)
ONE_TEST(spbkdf_00_hmac_sha1)

ONE_TEST(si_00_find_nothing)
ONE_TEST(si_05_add)
ONE_TEST(si_10_find_internet)
ONE_TEST(si_11_update_data)
ONE_TEST(si_12_item_stress)
ONE_TEST(si_14_dateparse)
ONE_TEST(si_15_certificate)
ONE_TEST(si_16_ec_certificate)
ONE_TEST(si_20_sectrust_activation)
ONE_TEST(si_20_sectrust)
ONE_TEST(si_21_sectrust_asr)
ONE_TEST(si_22_sectrust_iap)
ONE_TEST(si_23_sectrust_ocsp)
ONE_TEST(si_24_sectrust_itms)
ONE_TEST(si_24_sectrust_nist)
ONE_TEST(si_24_sectrust_otatasking)
ONE_TEST(si_24_sectrust_mobileasset)
ONE_TEST(si_24_sectrust_diginotar)
ONE_TEST(si_24_sectrust_appleid)
ONE_TEST(si_24_sectrust_digicert_malaysia)
ONE_TEST(si_24_sectrust_shoebox)
ONE_TEST(si_25_sectrust_ipsec_eap)
ONE_TEST(si_26_applicationsigning)
ONE_TEST(si_27_sectrust_exceptions)
ONE_TEST(si_28_sectrustsettings)
ONE_TEST(si_29_sectrust_codesigning)
DISABLED_ONE_TEST(si_30_keychain_upgrade) //obsolete, needs updating
DISABLED_ONE_TEST(si_31_keychain_bad)
DISABLED_ONE_TEST(si_31_keychain_unreadable)
ONE_TEST(si_33_keychain_backup)
ONE_TEST(si_40_seckey)
ONE_TEST(si_40_seckey_custom)
ONE_TEST(si_41_sececkey)
ONE_TEST(si_42_identity)
ONE_TEST(si_43_persistent)
ONE_TEST(si_50_secrandom)
ONE_TEST(si_60_cms)
DISABLED_ONE_TEST(si_61_pkcs12)
ONE_TEST(si_62_csr)
ONE_TEST(si_63_scep)
ONE_TEST(si_64_ossl_cms)
ONE_TEST(si_65_cms_cert_policy)
ONE_TEST(si_66_smime)
ONE_TEST(si_67_sectrust_blacklist)
ONE_TEST(si_68_secmatchissuer)
ONE_TEST(si_69_keydesc)
ONE_TEST(si_70_sectrust_unified)
ONE_TEST(si_71_mobile_store_policy)
ONE_TEST(si_72_syncableitems)
ONE_TEST(si_73_secpasswordgenerate)
#if TARGET_OS_IPHONE
ONE_TEST(si_74_OTA_PKI_Signer)
ONE_TEST(si_75_AppleIDRecordSigning)
#if TARGET_IPHONE_SIMULATOR
OFF_ONE_TEST(si_76_shared_credentials)
#else
ONE_TEST(si_76_shared_credentials)
#endif
ONE_TEST(si_77_SecAccessControl)
ONE_TEST(si_79_smp_cert_policy)
#else
DISABLED_ONE_TEST(si_74_OTA_PKI_Signer)
DISABLED_ONE_TEST(si_75_AppleIDRecordSigning)
DISABLED_ONE_TEST(si_76_shared_credentials)
DISABLED_ONE_TEST(si_77_SecAccessControl)
DISABLED_ONE_TEST(si_79_smp_cert_policy)
#endif
ONE_TEST(si_78_query_attrs)
ONE_TEST(si_80_empty_data)
#if TARGET_IPHONE_SIMULATOR
OFF_ONE_TEST(si_81_item_acl_stress)
#else
ONE_TEST(si_81_item_acl_stress)
#endif
ONE_TEST(si_81_sectrust_server_auth)

ONE_TEST(vmdh_40)
ONE_TEST(vmdh_41_example)
ONE_TEST(vmdh_42_example2)

ONE_TEST(otr_00_identity)
ONE_TEST(otr_30_negotiation)
ONE_TEST(otr_otrdh)
ONE_TEST(otr_packetdata)

#if TARGET_OS_IPHONE
ONE_TEST(so_01_serverencryption)
#else
DISABLED_ONE_TEST(so_01_serverencryption)
#endif
