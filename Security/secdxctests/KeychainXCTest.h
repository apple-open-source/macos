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

#import "KeychainXCTest.h"
#import "SecItemServer.h"
#import "SFKeychainServer.h"
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#if USE_KEYSTORE
#include "OSX/utilities/SecAKSWrappers.h"

typedef enum {
    LockStateUnlocked,
    LockStateLockedAndDisallowAKS,
    LockStateLockedAndAllowAKS // this state matches how backup works while locked
} LockState;

@interface KeychainXCTestFailureLogger : NSObject <XCTestObservation>
@end

@interface KeychainXCTest : XCTestCase

@property LockState lockState;
@property id mockSecDbKeychainItemV7;
@property id mockSecAKSObjCWrappers;
@property bool allowDecryption;
@property BOOL didAKSDecrypt;
@property BOOL simulateRolledAKSKey;
@property keyclass_t keyclassUsedForAKSDecryption;

@property NSString* keychainDirectoryPrefix;

@property SFAESKeySpecifier* keySpecifier;
@property NSData* fakeAKSKey;

@property id keychainPartialMock;

- (bool)setNewFakeAKSKey:(NSData*)newKeyData;

- (void)setEntitlements:(NSDictionary<NSString *, id> *)entitlements validated:(BOOL)validated;

- (NSData*)getDatabaseKeyDataWithError:(NSError**)error;

@end

@interface SFKeychainServerFakeConnection : SFKeychainServerConnection

- (void)setFakeAccessGroups:(NSArray*)fakeAccessGroups;

@end

#endif
