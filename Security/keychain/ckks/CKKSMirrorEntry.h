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

#include "keychain/securityd/SecDbItem.h"
#include <utilities/SecDb.h>
#import "CKKSItem.h"
#import "CKKSSQLDatabaseObject.h"

#ifndef CKKSMirrorEntry_h
#define CKKSMirrorEntry_h

#import <CloudKit/CloudKit.h>

NS_ASSUME_NONNULL_BEGIN

@class CKKSWrappedAESSIVKey;

@interface CKKSMirrorEntry : CKKSSQLDatabaseObject

@property CKKSItem* item;
@property NSString* uuid;

@property uint64_t wasCurrent;

- (instancetype)initWithCKKSItem:(CKKSItem*)item;
- (instancetype)initWithCKRecord:(CKRecord*)record;
- (void)setFromCKRecord:(CKRecord*)record;
- (bool)matchesCKRecord:(CKRecord*)record;

+ (instancetype)fromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (NSDictionary<NSString*, NSNumber*>*)countsByParentKey:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSNumber* _Nullable)counts:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error;

@end

NS_ASSUME_NONNULL_END
#endif
#endif /* CKKSOutgoingQueueEntry_h */
