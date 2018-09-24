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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
SOFT_LINK_CLASS(AuthKit, AKAccountManager);
#pragma clang diagnostic pop
#endif

@implementation SOSCCAuthPlugin

static bool accountIsHSA2(ACAccount *account) {
    bool hsa2 = false;

#if !TARGET_OS_SIMULATOR
    AKAccountManager *manager = [getAKAccountManagerClass() sharedInstance];
    if(manager != nil) {
#if TARGET_OS_OSX
        ACAccount *aka = [manager authKitAccountWithAltDSID:account.icaAltDSID];
#else
        ACAccount *aka = [manager authKitAccountWithAltDSID:account.aa_altDSID];
#endif
        if (aka) {
            AKAppleIDSecurityLevel securityLevel = [manager securityLevelForAccount: aka];
            if(securityLevel == AKAppleIDSecurityLevelHSA2) {
                hsa2 = true;
            }
        }
    }
#endif
    secnotice("accounts", "Account %s HSA2", (hsa2) ? "is": "isn't" );
    return hsa2;
}

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

    if(do_auth && !accountIsHSA2(account)) {
		NSString	*rawPassword = [account _aa_rawPassword];

		if (rawPassword != NULL) {
            dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
                CFErrorRef asyncError = NULL;
                NSString *dsid = [account aa_personID];
                const char *password   = [rawPassword cStringUsingEncoding:NSUTF8StringEncoding];
                CFDataRef passwordData = CFDataCreate(kCFAllocatorDefault, (const uint8_t *) password, strlen(password));

                if (passwordData) {
                    secinfo("accounts", "Performing async SOS circle credential set for account %@: %@", accountIdentifier, account.username);

                    if (!SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef) account.username, passwordData, (__bridge CFStringRef) dsid, &asyncError)) {
                        secerror("Unable to set SOS circle credentials for account %@: %@", accountIdentifier, asyncError);
                        secinfo("accounts", "Returning from failed async call to SOSCCSetUserCredentialsAndDSID");
                        CFReleaseNull(asyncError);
                    } else {
                        secinfo("accounts", "Returning from successful async call to SOSCCSetUserCredentialsAndDSID");
                    }
                    CFRelease(passwordData);
                } else {
                    secinfo("accounts", "Failed to create string for call to SOSCCSetUserCredentialsAndDSID");
                }
            });
		} else {
            CFErrorRef    authError    = NULL;
			if (!SOSCCCanAuthenticate(&authError)) {
				secerror("Account %@ did not present a password and we could not authenticate the SOS circle: %@", accountIdentifier, authError);
				CFReleaseNull(authError);
			}
		}
	} else {
		secinfo("accounts", "NOT performing SOS circle credential set for account %@: %@", accountIdentifier, account.username);
	}

	completion();
}

@end
