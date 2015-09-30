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
#import <Accounts/ACLogging.h>
#import <AccountsDaemon/ACDAccountStore.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <Security/SOSCloudCircle.h>
#include "utilities/SecCFRelease.h"


@implementation SOSCCAuthPlugin

- (void) didReceiveAuthenticationResponseParameters: (NSDictionary *) parameters
									   accountStore: (ACDAccountStore *) store
											account: (ACAccount *) account
										 completion: (dispatch_block_t) completion
{
	BOOL	do_auth = NO;
	ACLogNotice(@"parameters %@", parameters);
	ACLogNotice(@"account %@", account);

	if ([account.accountType.identifier isEqualToString:ACAccountTypeIdentifierIdentityServices]) {
		ACAccount *icloud = [store aa_primaryAppleAccount];
		NSString  *dsid   = [parameters[@"com.apple.private.ids"][@"service-data"][@"profile-id"] substringFromIndex:2];	// remove "D:" prefix
		ACLogNotice(@"IDS account: iCloud %@ (personID %@)", icloud, icloud.aa_personID);
		do_auth = icloud && icloud.aa_personID && [icloud.aa_personID isEqualToString:dsid];
	} else if ([account.accountType.identifier isEqualToString:ACAccountTypeIdentifierAppleAccount]) {
		ACLogNotice(@"AppleID account: primary %@", @([account aa_isPrimaryAccount]));
		do_auth = [account aa_isPrimaryAccount];
	}

	ACLogNotice(@"do_auth %@", do_auth ? @"YES" : @"NO" );

	if (do_auth) {
		CFErrorRef	authError    = NULL;
		NSString	*rawPassword = [account _aa_rawPassword];

		if (rawPassword != NULL) {
			const char *password   = [rawPassword cStringUsingEncoding:NSUTF8StringEncoding];
			CFDataRef passwordData = CFDataCreate(kCFAllocatorDefault, (const uint8_t *) password, strlen(password));
			if (passwordData) {
				ACLogNotice(@"Performing SOS circle credential set for account %@: %@", account.identifier, account.username);
				NSString *dsid = [account aa_personID];
				if (!SOSCCSetUserCredentialsAndDSID((__bridge CFStringRef) account.username, passwordData, (__bridge CFStringRef) dsid, &authError)) {
					ACLogError(@"Unable to set SOS circle credentials for account %@: %@", account.identifier, authError);
					CFReleaseNull(authError);
				}

				CFRelease(passwordData);
			}
		} else {
			if (!SOSCCCanAuthenticate(&authError)) {
				ACLogError(@"Account %@ did not present a password and we could not authenticate the SOS circle: %@", account.identifier, authError);
				CFReleaseNull(authError);	// CFReleaseSafe?
			}
		}
	} else {
		ACLogNotice(@"NOT performing SOS circle credential set for account %@: %@", account.identifier, account.username);
	}

	completion();
}

@end
