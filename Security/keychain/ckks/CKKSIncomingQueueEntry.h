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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#include "keychain/securityd/SecDbItem.h"
#include <utilities/SecDb.h>
#import "CKKSItem.h"
#import "CKKSMirrorEntry.h"
#import "CKKSSQLDatabaseObject.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSIncomingQueueEntry : CKKSSQLDatabaseObject

@property CKKSItem* item;
@property NSString* uuid;  // through-access to underlying item

@property NSString* action;
@property NSString* state;

- (instancetype)initWithCKKSItem:(CKKSItem*)ckme action:(NSString*)action state:(NSString*)state;

+ (instancetype _Nullable)fromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)tryFromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSIncomingQueueEntry*>* _Nullable)fetch:(ssize_t)n
                                      startingAtUUID:(NSString* _Nullable)uuid
                                               state:(NSString*)state
                                              action:(NSString* _Nullable)action
                                              zoneID:(CKRecordZoneID*)zoneID
                                               error:(NSError* __autoreleasing*)error;

+ (NSDictionary<NSString*, NSNumber*>*)countsByStateInZone:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSInteger)countByState:(CKKSItemState *)state zone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *)error;

@end

NS_ASSUME_NONNULL_END
#endif
