//
//  SOSCCAuthPlugin.m
//  Security
//
//  Created by Christian Schmidt on 7/8/15.
//  Copyright 2015 Apple, Inc. All rights reserved.
//

#import "SOSCCAuthPlugin.h"
#import <Foundation/Foundation.h>
#import <Accounts/Accounts.h>
#import <Accounts/Accounts_Private.h>
#import <AccountsDaemon/ACDAccountStore.h>
#import <AppleAccount/ACAccount+AppleAccount.h>
#import <AppleAccount/ACAccountStore+AppleAccount.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <SoftLinking/SoftLinking.h>
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

- (void) didReceiveAuthenticationResponseParameters: (NSDictionary *) parameters
									   accountStore: (ACDAccountStore *) store
											account: (ACAccount *) account
										 completion: (dispatch_block_t) completion
{
	secnotice("accounts", "account plugin fired for SOSCCAuthPlugin");
	completion();
}

@end
