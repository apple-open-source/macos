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
#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSCKRecordHolder;
@class FakeCKZone;

typedef NSMutableDictionary<CKRecordZoneID*, FakeCKZone*> FakeCKDatabase;


@interface FakeCKModifyRecordZonesOperation : NSBlockOperation <CKKSModifyRecordZonesOperation>
@property (nullable) NSError* creationError;
@property (nonatomic, nullable) NSMutableArray<CKRecordZone*>* recordZonesSaved;
@property (nonatomic, nullable) NSMutableArray<CKRecordZoneID*>* recordZoneIDsDeleted;
+ (FakeCKDatabase*)ckdb;
+(void)ensureZoneDeletionAllowed:(FakeCKZone*)zone;
@end

@interface FakeCKModifySubscriptionsOperation : NSBlockOperation <CKKSModifySubscriptionsOperation>
@property (nullable) NSError* subscriptionError;
@property (nonatomic, nullable) NSMutableArray<CKSubscription*>* subscriptionsSaved;
@property (nonatomic, nullable) NSMutableArray<NSString*>* subscriptionIDsDeleted;
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeCKFetchRecordZoneChangesOperation : NSOperation <CKKSFetchRecordZoneChangesOperation>
+ (FakeCKDatabase*)ckdb;
@property (nullable) void (^blockAfterFetch)();
@end

@interface FakeCKFetchRecordsOperation : NSBlockOperation <CKKSFetchRecordsOperation>
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeCKQueryOperation : NSBlockOperation <CKKSQueryOperation>
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeAPSConnection : NSObject <CKKSAPSConnection>
@end

@interface FakeNSNotificationCenter : NSObject <CKKSNSNotificationCenter>
+ (instancetype)defaultCenter;
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject;
@end

@interface FakeCKZone : NSObject
// Used while mocking: database is the contents of the current current CloudKit database, pastDatabase is the state of the world in the past at different change tokens
@property CKRecordZoneID* zoneID;
@property CKServerChangeToken* currentChangeToken;
@property NSMutableDictionary<CKRecordID*, CKRecord*>* currentDatabase;
@property NSMutableDictionary<CKServerChangeToken*, NSMutableDictionary<CKRecordID*, CKRecord*>*>* pastDatabases;
@property bool flag;  // used however you'd like in a test

// Usually nil. If set, trying to 'create' this zone should fail.
@property (nullable) NSError* creationError;

// Usually false. If set, trying to 'create' this should should 1) pretend to succeed but 2) delete this zone from existence
@property bool failCreationSilently;

// Usually nil. If set, trying to subscribe to this zone should fail.
@property (nullable) NSError* subscriptionError;

- (instancetype)initZone:(CKRecordZoneID*)zoneID;

- (void)rollChangeToken;

// Always Succeed
- (void)addToZone:(CKKSCKRecordHolder*)item zoneID:(CKRecordZoneID*)zoneID;
- (void)addToZone:(CKRecord*)record;

// Removes this record from all versions of the CK database, without changing the change tag
- (void)deleteFromHistory:(CKRecordID*)recordID;

- (void)addCKRecordToZone:(CKRecord*)record;
- (NSError* _Nullable)deleteCKRecordIDFromZone:(CKRecordID*)recordID;

// Sets up the next fetchChanges to fail with this error
- (void)failNextFetchWith:(NSError*)fetchChangesError;

// Get the next fetchChanges error. Returns NULL if the fetchChanges should succeed.
- (NSError* _Nullable)popFetchChangesError;

// Checks if this record add/modification should fail
- (NSError* _Nullable)errorFromSavingRecord:(CKRecord*)record;
@end

@interface FakeCKKSNotifier : NSObject <CKKSNotifier>
@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
