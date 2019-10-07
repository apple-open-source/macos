/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <OCMock/OCMock.h>
#import <XCTest/XCTest.h>

#include <Security/SecItemPriv.h>
#include "OSX/sec/Security/SecItemShim.h"

#import "keychain/ckks/tests/CloudKitKeychainSyncingTestsBase.h"

NS_ASSUME_NONNULL_BEGIN

// 1 master manifest, 72 manifest leaf nodes = 73
// 3 keys, 3 current keys, and 1 device state entry
#define SYSTEM_DB_RECORD_COUNT (7 + ([CKKSManifest shouldSyncManifests] ? 73 : 0))

@interface CloudKitKeychainSyncingTests : CloudKitKeychainSyncingTestsBase
@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
