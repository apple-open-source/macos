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

#import <Foundation/Foundation.h>
#import <SecurityFoundation/SFKey.h>
#import "CKKSRecordHolder.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString* const CKKSManifestZoneKey;
extern NSString* const CKKSManifestSignerIDKey;
extern NSString* const CKKSManifestGenCountKey;

@class CKKSManifestMasterRecord;
@class CKRecord;
@class CKKSItem;
@class CKKSCurrentItemPointer;

@interface CKKSManifest : CKKSCKRecordHolder

@property (readonly, class) NSUInteger greatestKnownGenerationCount;

@property (nonatomic, readonly) NSData* digestValue;
@property (nonatomic, readonly) NSUInteger generationCount;
@property (nonatomic, readonly) NSString* signerID;

+ (void)performWithAccountInfo:(void (^)(void))action;

+ (bool)shouldSyncManifests;
+ (bool)shouldEnforceManifests;

+ (nullable instancetype)manifestForZone:(NSString*)zone peerID:(NSString*)peerID error:(NSError**)error;
+ (nullable instancetype)manifestForRecordName:(NSString*)recordName error:(NSError**)error;
+ (nullable instancetype)latestTrustedManifestForZone:(NSString*)zone error:(NSError**)error;

- (BOOL)updateWithRecord:(CKRecord*)record error:(NSError**)error;

- (BOOL)validateWithError:(NSError**)error;
- (BOOL)validateItem:(CKKSItem*)item withError:(NSError**)error;
- (BOOL)validateCurrentItem:(CKKSCurrentItemPointer*)currentItem withError:(NSError**)error;
- (BOOL)itemUUIDExistsInManifest:(NSString*)uuid;
- (BOOL)contentsAreEqualToManifest:(CKKSManifest*)otherManifest;

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID resync:(BOOL)resync error:(NSError**)error;

@end

@interface CKKSPendingManifest : CKKSManifest

@property (readonly, getter=isReadyToCommmit) BOOL readyToCommit;

- (nullable CKKSManifest*)commitToDatabaseWithError:(NSError**)error;

+ (BOOL)intransactionRecordChanged:(CKRecord*)record resync:(BOOL)resync error:(NSError**)error;

@end

@interface CKKSEgoManifest : CKKSManifest

+ (nullable CKKSEgoManifest*)tryCurrentEgoManifestForZone:(NSString*)zone;
+ (nullable instancetype)newManifestForZone:(NSString*)zone
                                  withItems:(NSArray<CKKSItem*>*)items
                            peerManifestIDs:(NSArray<NSString*>*)peerManifestIDs
                               currentItems:(NSDictionary*)currentItems
                                      error:(NSError**)error;

- (void)updateWithNewOrChangedRecords:(NSArray<CKRecord*>*)newOrChangedRecords
                     deletedRecordIDs:(NSArray<CKRecordID*>*)deletedRecordIDs;
- (void)setCurrentItemUUID:(NSString*)newCurrentItemUUID forIdentifier:(NSString*)currentPointerIdentifier;

- (NSArray<CKRecord*>*)allCKRecordsWithZoneID:(CKRecordZoneID*)zoneID;

@end

// ----------------------------------------------------
// Declarations for unit tests

@class CKKSManifestInjectionPointHelper;

@interface CKKSManifest (UnitTesting)

- (void)nilAllIvars;

@end

@interface CKKSEgoManifest (UnitTesting)

+ (nullable instancetype)newFakeManifestForZone:(NSString*)zone
                                withItemRecords:(NSArray<CKRecord*>*)itemRecords
                                   currentItems:(NSDictionary*)currentItems
                                       signerID:(NSString*)signerID
                                        keyPair:(SFECKeyPair*)keyPair
                                          error:(NSError**)error;

@end

@interface CKKSManifestInjectionPointHelper : NSObject

@property (class) BOOL ignoreChanges;  // turn to YES to have changes to the database get ignored by CKKSManifest to support negative testing

+ (void)registerEgoPeerID:(NSString*)egoPeerID keyPair:(SFECKeyPair*)keyPair;

@end

NS_ASSUME_NONNULL_END

#endif
