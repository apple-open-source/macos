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

#ifndef CloudKitKeychainSyncingTestsBase_h
#define CloudKitKeychainSyncingTestsBase_h

#import <CloudKit/CloudKit.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"
#import <OCMock/OCMock.h>
#pragma clang diagnostic pop

#import <XCTest/XCTest.h>

#include <Security/SecItemPriv.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSViewManager.h"

#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ot/OTFollowup.h"
#import <CoreCDP/CDPFollowUpContext.h>
#import <CoreCDP/CDPAccount.h>

NS_ASSUME_NONNULL_BEGIN

@interface CloudKitKeychainSyncingTestsBase : CloudKitKeychainSyncingMockXCTest
@property (nullable) CKRecordZoneID* keychainZoneID;
@property (nullable) CKKSKeychainView* keychainView;
@property (nullable) FakeCKZone* keychainZone;
@property (nullable, readonly) ZoneKeys* keychainZoneKeys;

@property NSCalendar* utcCalendar;

- (ZoneKeys*)keychainZoneKeys;

@end

NS_ASSUME_NONNULL_END

#endif /* CloudKitKeychainSyncingTestsBase_h */
