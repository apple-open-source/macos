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

- (void)deleteItemsInAccessGroup:(NSString*)agrp forItemClass:(CFStringRef)itemClass
{
    NSDictionary* query = @{
        (id)kSecAttrSynchronizable : @(YES),
        (id)kSecAttrAccessGroup : agrp,
        (id)kSecUseDataProtectionKeychain : @(YES),
        (id)kSecAttrTombstone : @(NO),
        (id)kSecUseTombstones : @(NO),
        (id)kSecClass : (__bridge NSString*)itemClass
    };

    OSStatus result = SecItemDelete((__bridge CFDictionaryRef)query);
    if (result == errSecSuccess || result == errSecItemNotFound) {
        secnotice("ItemDelete", "Deleted synchronizable items from table %@ for access group %@%@", itemClass, agrp, result == errSecItemNotFound ? @" (no items found)" : @"");
    } else {
        secwarning("ItemDelete: failed to delete synchronizable items from table %@ for access group %@: %d", itemClass, agrp, (int)result);
    }
}


- (BOOL)performAction:(ACDataclassAction*)action forAccount:(ACAccount*)account withChildren:(NSArray*)childAccounts forDataclass:(NSString*)dataclass withError:(NSError**)error
{
    // if the user asked us to delete their data, do that now
    // we should rejigger this implementation to send a new custom message to security with an entitlement specifically for the signout case
    // then we can do all the things we need to in securityd without having to entitlement the dataclass owners manager to read keychain items
    // <rdar://problem/42436575> redo KeychainDataclassOwner to remove Safari items from DataclassOwnerManager's entitlements
    if (action.type == ACDataclassActionDeleteSyncData) {

        NSString* const accessGroupCFNetwork = @"com.apple.cfnetwork";
        NSString* const accessGroupCreditCards = @"com.apple.safari.credit-cards";
        NSString* const accessGroupPasswordManager = @"com.apple.password-manager";

        [self deleteItemsInAccessGroup:accessGroupCFNetwork forItemClass:kSecClassInternetPassword];
        [self deleteItemsInAccessGroup:accessGroupCFNetwork forItemClass:kSecClassGenericPassword];
        [self deleteItemsInAccessGroup:accessGroupCFNetwork forItemClass:kSecClassCertificate];
        [self deleteItemsInAccessGroup:accessGroupCFNetwork forItemClass:kSecClassKey];

        [self deleteItemsInAccessGroup:accessGroupCreditCards forItemClass:kSecClassGenericPassword];

        [self deleteItemsInAccessGroup:accessGroupPasswordManager forItemClass:kSecClassInternetPassword];
        [self deleteItemsInAccessGroup:accessGroupPasswordManager forItemClass:kSecClassGenericPassword];
        [self deleteItemsInAccessGroup:accessGroupPasswordManager forItemClass:kSecClassCertificate];
        [self deleteItemsInAccessGroup:accessGroupPasswordManager forItemClass:kSecClassKey];
    }

    return YES;
}

@end
