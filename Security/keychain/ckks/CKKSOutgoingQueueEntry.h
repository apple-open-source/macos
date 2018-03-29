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

#include <securityd/SecDbItem.h>
#include <utilities/SecDb.h>
#import "CKKSItem.h"
#import "CKKSMirrorEntry.h"
#import "CKKSSQLDatabaseObject.h"

#ifndef CKKSOutgoingQueueEntry_h
#define CKKSOutgoingQueueEntry_h

#if OCTAGON
#import <CloudKit/CloudKit.h>

@class CKKSKeychainView;

@interface CKKSOutgoingQueueEntry : CKKSSQLDatabaseObject

@property CKKSItem* item;
@property NSString* uuid;  // property access to underlying CKKSItem

@property NSString* action;
@property NSString* state;
@property NSString* accessgroup;
@property NSDate* waitUntil;  // If non-null, the time at which this entry should be processed

- (instancetype)initWithCKKSItem:(CKKSItem*)item
                          action:(NSString*)action
                           state:(NSString*)state
                       waitUntil:(NSDate*)waitUntil
                     accessGroup:(NSString*)accessgroup;

+ (instancetype)withItem:(SecDbItemRef)item
                  action:(NSString*)action
                    ckks:(CKKSKeychainView*)ckks
                   error:(NSError* __autoreleasing*)error;
+ (instancetype)fromDatabase:(NSString*)uuid
                       state:(NSString*)state
                      zoneID:(CKRecordZoneID*)zoneID
                       error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)uuid
                          state:(NSString*)state
                         zoneID:(CKRecordZoneID*)zoneID
                          error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSOutgoingQueueEntry*>*)fetch:(ssize_t)n
                                     state:(NSString*)state
                                    zoneID:(CKRecordZoneID*)zoneID
                                     error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSOutgoingQueueEntry*>*)allInState:(NSString*)state
                                         zoneID:(CKRecordZoneID*)zoneID
                                          error:(NSError* __autoreleasing*)error;

+ (NSDictionary<NSString*, NSNumber*>*)countsByStateInZone:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSInteger)countByState:(CKKSItemState *)state zone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error;

@end

#endif
#endif /* CKKSOutgoingQueueEntry_h */
