#include <os/log.h>
#include <CoreFoundation/CoreFoundation.h>

#import "KeychainSyncAccountUpdater.h"

@implementation KeychainSyncAccountUpdater

- (BOOL)includePluginInUpdateSession:(nonnull UpdaterSessionParameters *)parameters
{
	return YES;
}

- (void)updateAccountWithPrivilege
{
	CFPreferencesSetValue(CFSTR("SecItemSynchronizable"), kCFBooleanTrue, CFSTR("com.apple.security"), kCFPreferencesAnyUser,  kCFPreferencesCurrentHost);
	Boolean okay = CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
	os_log(OS_LOG_DEFAULT, "EnableKeychainSync %d", okay);
}

@end
