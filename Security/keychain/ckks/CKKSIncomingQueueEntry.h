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

#import "CKKSSQLDatabaseObject.h"
#import "CKKSItem.h"
#import "CKKSMirrorEntry.h"
#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>

#ifndef CKKSIncomingQueueEntry_h
#define CKKSIncomingQueueEntry_h
#if OCTAGON

#import <CloudKit/CloudKit.h>

@interface CKKSIncomingQueueEntry : CKKSSQLDatabaseObject

@property CKKSItem* item;
@property NSString* uuid; // through-access to underlying item

@property NSString* action;
@property NSString* state;

- (instancetype) initWithCKKSItem:(CKKSItem*) ckme
                           action:(NSString*) action
                            state:(NSString*) state;

+ (instancetype) fromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error;
+ (instancetype) tryFromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error;

+ (NSArray<CKKSIncomingQueueEntry*>*)fetch:(ssize_t)n
                            startingAtUUID:(NSString*)uuid
                                     state:(NSString*)state
                                    zoneID:(CKRecordZoneID*)zoneID
                                     error: (NSError * __autoreleasing *) error;

+ (NSDictionary<NSString*,NSNumber*>*)countsByState:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error;

@end

#endif
#endif /* CKKSIncomingQueueEntry_h */
