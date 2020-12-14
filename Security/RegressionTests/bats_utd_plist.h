#include <TargetConditionals.h>

/*
 * BATS Unit Test Discovery has a feature called "Disabled".  This is a
 * boolean:  if True, the test is not run.
 *
 * Every test in Security.plist has a "Disabled" string:
 *	<dict>
 *		<key>TestName</key>
 *		<string>KeychainSecd_iOS</string>
 *		<key>Disabled</key>
 *		<string>BATS_UTD_Disabled_KeychainSecd_iOS</string>
 *	</dict>
 *
 * In Security.plist, we use a string instead of a boolean.  We will convert to
 * booleans during the preprocssor stage.  This allows use to edit the plists
 * in xcode.  So, the final plist will look like:
 *	<dict>
 *		<key>TestName</key>
 *		<string>KeychainSecd_iOS</string>
 *		<key>Disabled</key>
 *		<true/>
 *	</dict>
 *
 * When you add a new test, you will need to add a define to each branch and
 * specify which platform you want the test to run on.
 *
 * If this include becomes to ugly, we either will need to have seperate UTD
 * plists per platform or have the test executable skip if on platforms it does
 * not like.
 */

#if TARGET_OS_BRIDGE
/* For BridgeOS, only two tests are currently working */
#define BATS_UTD_Disabled_AuthorizationTest _TRUE_
#define BATS_UTD_Disabled_EduModeTest _FALSE_
#define BATS_UTD_Disabled_KCPairingTest _TRUE_
#define BATS_UTD_Disabled_KeychainAnalyticsTests _TRUE_
#define BATS_UTD_Disabled_KeychainMockAKSTests _TRUE_
#define BATS_UTD_Disabled_KeychainSecd_iOS _TRUE_
#define BATS_UTD_Disabled_KeychainSecd_macOS _TRUE_
#define BATS_UTD_Disabled_SecurityUtiltitesTests _TRUE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_1 _TRUE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_2 _TRUE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_3 _TRUE_
#define BATS_UTD_Disabled_keystorectl_get_lock_state _FALSE_
#define BATS_UTD_Disabled_security_sysdiagnose _TRUE_
#define BATS_UTD_Disabled_KeychainSecdXCTests _TRUE_
#define BATS_UTD_Disabled_KeychainSecDbBackupTests _TRUE_
#define BATS_UTD_Disabled_SecCodeAPITest _TRUE_
#define BATS_UTD_Disabled_SecStaticCodeAPITest _TRUE_

#elif TARGET_OS_OSX
/* For MacOS, we disable the iOS only tests. */
#define BATS_UTD_Disabled_AuthorizationTest _FALSE_
#define BATS_UTD_Disabled_EduModeTest _FALSE_
#define BATS_UTD_Disabled_KCPairingTest _FALSE_
#define BATS_UTD_Disabled_KeychainAnalyticsTests _FALSE_
#define BATS_UTD_Disabled_KeychainMockAKSTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_iOS _TRUE_
#define BATS_UTD_Disabled_KeychainSecd_macOS _FALSE_
#define BATS_UTD_Disabled_SecurityUtiltitesTests _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_1 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_2 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_3 _FALSE_
#define BATS_UTD_Disabled_keystorectl_get_lock_state _FALSE_
#define BATS_UTD_Disabled_security_sysdiagnose _FALSE_
#define BATS_UTD_Disabled_KeychainSecdXCTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecDbBackupTests _FALSE_
#define BATS_UTD_Disabled_SecCodeAPITest _FALSE_
#define BATS_UTD_Disabled_SecStaticCodeAPITest _FALSE_

#elif TARGET_OS_WATCH
#define BATS_UTD_Disabled_AuthorizationTest _TRUE_
#define BATS_UTD_Disabled_EduModeTest _FALSE_
#define BATS_UTD_Disabled_KCPairingTest _FALSE_
#define BATS_UTD_Disabled_KeychainAnalyticsTests _FALSE_
#define BATS_UTD_Disabled_KeychainMockAKSTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_iOS _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_macOS _TRUE_
#define BATS_UTD_Disabled_SecurityUtiltitesTests _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_1 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_2 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_3 _FALSE_
#define BATS_UTD_Disabled_keystorectl_get_lock_state _FALSE_
#define BATS_UTD_Disabled_security_sysdiagnose _FALSE_
#define BATS_UTD_Disabled_KeychainSecdXCTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecDbBackupTests _TRUE_
#define BATS_UTD_Disabled_SecCodeAPITest _TRUE_
#define BATS_UTD_Disabled_SecStaticCodeAPITest _TRUE_

#elif TARGET_OS_TV
#define BATS_UTD_Disabled_AuthorizationTest _TRUE_
#define BATS_UTD_Disabled_EduModeTest _FALSE_
#define BATS_UTD_Disabled_KCPairingTest _FALSE_
#define BATS_UTD_Disabled_KeychainAnalyticsTests _FALSE_
#define BATS_UTD_Disabled_KeychainMockAKSTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_iOS _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_macOS _TRUE_
#define BATS_UTD_Disabled_SecurityUtiltitesTests _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_1 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_2 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_3 _FALSE_
#define BATS_UTD_Disabled_keystorectl_get_lock_state _FALSE_
#define BATS_UTD_Disabled_security_sysdiagnose _FALSE_
#define BATS_UTD_Disabled_KeychainSecdXCTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecDbBackupTests _TRUE_
#define BATS_UTD_Disabled_SecCodeAPITest _TRUE_
#define BATS_UTD_Disabled_SecStaticCodeAPITest _TRUE_

#else
/* By default, assume iOS platforms. We disable the MacOS only tests. */
#define BATS_UTD_Disabled_AuthorizationTest _TRUE_
#define BATS_UTD_Disabled_EduModeTest _FALSE_
#define BATS_UTD_Disabled_KCPairingTest _FALSE_
#define BATS_UTD_Disabled_KeychainAnalyticsTests _FALSE_
#define BATS_UTD_Disabled_KeychainMockAKSTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_iOS _FALSE_
#define BATS_UTD_Disabled_KeychainSecd_macOS _TRUE_
#define BATS_UTD_Disabled_SecurityUtiltitesTests _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_1 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_2 _FALSE_
#define BATS_UTD_Disabled_keychainnetworkextensionsharing_3 _FALSE_
#define BATS_UTD_Disabled_keystorectl_get_lock_state _FALSE_
#define BATS_UTD_Disabled_security_sysdiagnose _FALSE_
#define BATS_UTD_Disabled_KeychainSecdXCTests _FALSE_
#define BATS_UTD_Disabled_KeychainSecDbBackupTests _FALSE_
#define BATS_UTD_Disabled_SecCodeAPITest _TRUE_
#define BATS_UTD_Disabled_SecStaticCodeAPITest _TRUE_

#endif
