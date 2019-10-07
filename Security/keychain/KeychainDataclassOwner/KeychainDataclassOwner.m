/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import "KeychainDataclassOwner.h"
#import "NSError+UsefulConstructors.h"
#import "SecCFRelease.h"
#import "SOSCloudCircle.h"
#import "debugging.h"
#import <Accounts/Accounts.h>
#import <Accounts/ACDataclassAction.h>
#import <Accounts/ACConstants.h>
#import <Security/Security.h>
#import <Security/SecItemPriv.h>

@implementation KeychainDataclassOwner

static NSString* const KeychainDataclass = @"KeychainDataclass";

+ (NSArray*)dataclasses
{
    return @[kAccountDataclassKeychainSync];
}

- (NSArray*)actionsForDeletingAccount:(ACAccount*)account forDataclass:(NSString*)dataclass
{
    if (![dataclass isEqual:kAccountDataclassKeychainSync]) {
        return nil;
    }

    ACDataclassAction* cancelAction = [ACDataclassAction actionWithType:ACDataclassActionCancel];
    ACDataclassAction* deleteAction = [ACDataclassAction actionWithType:ACDataclassActionDeleteSyncData];
    ACDataclassAction* keepAction = [ACDataclassAction actionWithType:ACDataclassActionMergeSyncDataIntoLocalData];

    return @[cancelAction, deleteAction, keepAction];
}

- (NSArray*)actionsForDisablingDataclassOnAccount:(ACAccount*)account forDataclass:(NSString*)dataclass
{
    return [self actionsForDeletingAccount:account forDataclass:dataclass];
}


- (BOOL)performAction:(ACDataclassAction*)action forAccount:(ACAccount*)account withChildren:(NSArray*)childAccounts forDataclass:(NSString*)dataclass withError:(NSError**)error
{
    // if the user asked us to delete their data, do that now
    // we should rejigger this implementation to send a new custom message to security with an entitlement specifically for the signout case
    // then we can do all the things we need to in securityd without having to entitlement the dataclass owners manager to read keychain items
    // <rdar://problem/42436575> redo KeychainDataclassOwner to remove Safari items from DataclassOwnerManager's entitlements
    if (action.type == ACDataclassActionDeleteSyncData) {
        NSDictionary* baseQuery = @{ (id)kSecAttrSynchronizable : @(YES),
                                     (id)kSecAttrAccessGroup : @"com.apple.cfnetwork",
                                     (id)kSecUseDataProtectionKeychain : @(YES),
                                     (id)kSecAttrTombstone : @(NO),
                                     (id)kSecUseTombstones : @(NO) };
        NSMutableDictionary* inetQuery = baseQuery.mutableCopy;
        inetQuery[(id)kSecClass] = (id)kSecClassInternetPassword;
        OSStatus inetResult = SecItemDelete((__bridge CFDictionaryRef)inetQuery);

        NSMutableDictionary* genpQuery = baseQuery.mutableCopy;
        genpQuery[(id)kSecClass] = (id)kSecClassGenericPassword;
        OSStatus genpResult = SecItemDelete((__bridge CFDictionaryRef)genpQuery);

        NSMutableDictionary* certQuery = baseQuery.mutableCopy;
        certQuery[(id)kSecClass] = (id)kSecClassCertificate;
        OSStatus certResult = SecItemDelete((__bridge CFDictionaryRef)certQuery);

        NSMutableDictionary* keyQuery = baseQuery.mutableCopy;
        keyQuery[(id)kSecClass] = (id)kSecClassKey;
        OSStatus keyResult = SecItemDelete((__bridge CFDictionaryRef)keyQuery);

        NSMutableDictionary* creditCardsQuery = baseQuery.mutableCopy;
        creditCardsQuery[(id)kSecClass] = (id)kSecClassGenericPassword;
        creditCardsQuery[(id)kSecAttrAccessGroup] = @"com.apple.safari.credit-cards";
        OSStatus creditCardsResult = SecItemDelete((__bridge CFDictionaryRef)creditCardsQuery);

        if (inetResult != errSecSuccess) {
            secwarning("failed to delete synchronizable passwords from table inet: %d", (int)inetResult);
        }
        if (genpResult != errSecSuccess) {
            secwarning("failed to delete synchronizable passwords from table genp: %d", (int)genpResult);
        }
        if (certResult != errSecSuccess) {
            secwarning("failed to delete synchronizable passwords from table cert: %d", (int)certResult);
        }
        if (keyResult != errSecSuccess) {
            secwarning("failed to delete synchronizable passwords from table keys: %d", (int)keyResult);
        }
        if (creditCardsResult != errSecSuccess) {
            secwarning("failed to delete credit cards from table genp: %d", (int)creditCardsResult);
        }
    }

    return YES;
}

@end
