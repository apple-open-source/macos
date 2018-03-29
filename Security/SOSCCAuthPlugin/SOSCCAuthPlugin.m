//
//  SOSCCAuthPlugin.m
//  Security
//
//  Created by Christian Schmidt on 7/8/15.
//  Copyright 2015 Apple, Inc. All rights reserved.
//

#import <SOSCCAuthPlugin.h>
#import <Foundation/Foundation.h>
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#import <AccountsDaemon/ACDAccountStore.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <Security/SecureObjectSync/SOSCloudCircle.h>
#import "utilities/SecCFRelease.h"
#import "utilities/debugging.h"


#if !TARGET_OS_SIMULATOR
SOFT_LINK_FRAMEWORK(PrivateFrameworks, AuthKit);
SOFT_LINK_CLASS(AuthKit, AKAccountManager);
#endif

@implementation SOSCCAuthPlugin

- (void) didReceiveAuthenticationResponseParameters: (NSDictionary *) parameters
									   accountStore: (ACDAccountStore *) store
											account: (ACAccount *) account
										 completion: (dispatch_block_t) completion
{
	BOOL	do_auth = NO;
    NSString* accountIdentifier = account.identifier; // strong reference
	secinfo("accounts", "parameters %@", parameters);
	secinfo("accounts", "account %@", account);

	if ([account.accountType.identifier isEqualToString:ACAccountTypeIdentifierIdentityServices]) {
		ACAccount *icloud = [store aa_primaryAppleAccount];
		NSString  *dsid   = [parameters[@"com.apple.private.ids"][@"service-data"][@"profile-id"] substringFromIndex:2];	// remove "D:" prefix
		secinfo("accounts", "IDS account: iCloud %@ (personID %@)", icloud, icloud.aa_personID);
		do_auth = icloud && icloud.aa_personID && [icloud.aa_personID isEqualToString:dsid];
	} else if ([account.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
		secinfo("accounts", "AppleID account: primary %@", @([account aa_isPrimaryAccount]));
		do_auth = [account aa_isPrimaryAccount];
	}

#if !TARGET_OS_SIMULATOR
    // If this is an HSA2 account let cdpd SetCreds
    AKAccountManager *manager = [getAKAccountManagerClass() sharedInstance];
    if(manager != nil) {
        AKAppleIDSecurityLevel securityLevel = [manager securityLevelForAccount: account];
        if(securityLevel == AKAppleIDSecurityLevelHSA2) {
            secnotice("accounts", "Not performing SOSCCSetUserCredentialsAndDSID in accountsd plugin since we're HSA2" );
            do_auth = NO;
        }
    } else {
        secnotice("accounts", "Couldn't softlink AKAccountManager - proceeding with do_auth = %@", do_auth ? @"YES" : @"NO");
    }
#endif

	secnotice("accounts", "do_auth %@", do_auth ? @"YES" : @"NO" );

	if (do_auth) {
		CFErrorRef	authError    = NULL;
		NSString	*rawPassword = [account _aa_rawPassword];

		if (rawPassword != NULL) {
			const char *password   = [rawPassword cStringUsingEncoding:NSUTF8StringEncoding];
			CFDataRef passwordData = CFDataCreate(kCFAllocatorDefault, (const uint8_t *) password, strlen(password));
			if (passwordData) {
				secinfo("accounts", "Performing SOS circle credential set for account %@: %@", accountIdentifier, account.username);
				NSString *dsid = [account aa_personID];
				if (!SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef) account.username, passwordData, (__bridge CFStringRef) dsid, &authError)) {
					secerror("Unable to set SOS circle credentials for account %@: %@", accountIdentifier, authError);
					CFReleaseNull(authError);
				}

				CFRelease(passwordData);
			}
		} else {
			if (!SOSCCCanAuthenticate(&authError)) {
				secerror("Account %@ did not present a password and we could not authenticate the SOS circle: %@", accountIdentifier, authError);
				CFReleaseNull(authError);	// CFReleaseSafe?
			}
		}
	} else {
		secinfo("accounts", "NOT performing SOS circle credential set for account %@: %@", accountIdentifier, account.username);
	}

	completion();
}

@end
