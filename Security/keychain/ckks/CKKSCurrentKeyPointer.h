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

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSResultOperation.h"

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCurrentKeyPointer : CKKSCKRecordHolder

@property CKKSKeyClass* keyclass;
@property NSString* currentKeyUUID;

- (instancetype)initForClass:(CKKSKeyClass*)keyclass
              currentKeyUUID:(NSString* _Nullable)currentKeyUUID
                      zoneID:(CKRecordZoneID*)zoneID
             encodedCKRecord:(NSData* _Nullable)encodedrecord;

+ (instancetype)fromDatabase:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (instancetype)forKeyClass:(CKKSKeyClass*)keyclass
                withKeyUUID:(NSString*)keyUUID
                     zoneID:(CKRecordZoneID*)zoneID
                      error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSCurrentKeyPointer*>*)all:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (bool)deleteAll:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

@end

@interface CKKSCurrentKeySet : NSObject

@property NSString* viewName;
@property (nullable) NSError* error;
@property (nullable) CKKSKey* tlk;
@property (nullable) CKKSKey* classA;
@property (nullable) CKKSKey* classC;
@property (nullable) CKKSCurrentKeyPointer* currentTLKPointer;
@property (nullable) CKKSCurrentKeyPointer* currentClassAPointer;
@property (nullable) CKKSCurrentKeyPointer* currentClassCPointer;

// Set to true if this is a 'proposed' key set, i.e., not yet uploaded to CloudKit
@property BOOL proposed;

// The tlkShares property holds all existing tlkShares for this key
@property NSArray<CKKSTLKShareRecord*>* tlkShares;

// This array (if present) holds any new TLKShares that should be uploaded
@property (nullable) NSArray<CKKSTLKShareRecord*>* pendingTLKShares;

- (instancetype)initForZoneName:(NSString*)zoneName;

+ (CKKSCurrentKeySet*)loadForZone:(CKRecordZoneID*)zoneID;

- (CKKSKeychainBackedKeySet* _Nullable)asKeychainBackedSet:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif
