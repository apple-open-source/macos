/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef CKKSDeviceStateEntry_h
#define CKKSDeviceStateEntry_h

#if OCTAGON

#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>

#import <CloudKit/CloudKit.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSSQLDatabaseObject.h"
#import "keychain/ckks/CKKSRecordHolder.h"

/*
 * This is the backing class for "device state" records: each device in an iCloud account copies
 * some state about itself into each keychain view it wants to participate in.
 *
 * This shares some overlap with the CKKSZoneStateEntry, but differs in that:
 *   - This will be uploaded to CloudKit
 *   - We will have receive such records from other devices
 */

@interface CKKSDeviceStateEntry : CKKSCKRecordHolder
@property NSString* device;

@property NSString* circlePeerID;
@property SOSCCStatus circleStatus;
@property CKKSZoneKeyState* keyState;

@property NSString* currentTLKUUID;
@property NSString* currentClassAUUID;
@property NSString* currentClassCUUID;

+ (instancetype)fromDatabase:(NSString*)device zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error;
+ (instancetype)tryFromDatabase:(NSString*)device zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error;
+ (instancetype)tryFromDatabaseFromCKRecordID:(CKRecordID*)recordID error:(NSError * __autoreleasing *)error;
+ (NSArray<CKKSDeviceStateEntry*>*)allInZone:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initForDevice:(NSString*)device
                 circlePeerID:(NSString*)circlePeerID
                 circleStatus:(SOSCCStatus)circleStatus
                     keyState:(CKKSZoneKeyState*)keyState
               currentTLKUUID:(NSString*)currentTLKUUID
            currentClassAUUID:(NSString*)currentClassAUUID
            currentClassCUUID:(NSString*)currentClassCUUID
                       zoneID:(CKRecordZoneID*)zoneID
              encodedCKRecord:(NSData*)encodedrecord;
@end

#endif // OCTAGON
#endif /* CKKSDeviceStateEntry_h */

