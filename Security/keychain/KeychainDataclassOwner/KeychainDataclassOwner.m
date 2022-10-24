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


- (BOOL)performAction:(ACDataclassAction*)action forAccount:(ACAccount*)account withChildren:(NSArray*)childAccounts forDataclass:(NSString*)dataclass withError:(NSError**)error
{
    // if the user asked us to delete their data, do that now
    if (action.type == ACDataclassActionDeleteSyncData) {
        CFErrorRef cfLocalError = NULL;
        if (SecDeleteItemsOnSignOut(&cfLocalError)) {
            secnotice("ItemDelete", "Deleted items on sign out");
        } else {
            NSError* localError = CFBridgingRelease(cfLocalError);
            secwarning("ItemDelete: Failed to delete items on sign out: %@", localError);
        }
    }

    return YES;
}

@end
