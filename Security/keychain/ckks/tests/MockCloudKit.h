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
#import <CloudKit/CloudKit_Private.h>
#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSCKRecordHolder;
@class FakeCKZone;

typedef NSMutableDictionary<CKRecordZoneID*, FakeCKZone*> FakeCKDatabase;

@interface FakeCKOperation : NSBlockOperation
@property (nonatomic, assign, readonly) BOOL isFinishingOnCallbackQueue;
@end


@interface FakeCKModifyRecordZonesOperation : FakeCKOperation <CKKSModifyRecordZonesOperation> {
    CKOperationConfiguration* _configuration;
}
@property (nullable) NSError* creationError;
@property (nonatomic, nullable) NSMutableArray<CKRecordZone*>* recordZonesSaved;
@property (nonatomic, nullable) NSMutableArray<CKRecordZoneID*>* recordZoneIDsDeleted;
@property (nonatomic, copy, null_resettable) CKOperationConfiguration *configuration;

+ (FakeCKDatabase*)ckdb;

+ (NSError* _Nullable)shouldFailModifyRecordZonesOperation;

+ (void)ensureZoneDeletionAllowed:(FakeCKZone*)zone;
@end

@interface FakeCKModifySubscriptionsOperation : FakeCKOperation <CKKSModifySubscriptionsOperation> {
    CKOperationConfiguration* _configuration;
}
@property (nullable) NSError* subscriptionError;
@property (nonatomic, nullable) NSMutableArray<CKSubscription*>* subscriptionsSaved;
@property (nonatomic, nullable) NSMutableArray<NSString*>* subscriptionIDsDeleted;
@property (nonatomic, copy, null_resettable) CKOperationConfiguration *configuration;
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeCKFetchRecordZoneChangesOperation : FakeCKOperation <CKKSFetchRecordZoneChangesOperation> {
    CKOperationConfiguration* _configuration;
}

+ (FakeCKDatabase*)ckdb;
@property (nonatomic, copy) NSString *operationID;
@property (nonatomic, readonly, strong, nullable) CKOperationConfiguration *resolvedConfiguration;
@property (nullable) void (^blockAfterFetch)(void);
@end

@interface FakeCKFetchRecordsOperation : FakeCKOperation <CKKSFetchRecordsOperation>
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeCKQueryOperation : FakeCKOperation <CKKSQueryOperation>
+ (FakeCKDatabase*)ckdb;
@end

@interface FakeAPSConnection : NSObject <OctagonAPSConnection>
@end

@interface FakeNSNotificationCenter : NSObject <CKKSNSNotificationCenter>
+ (instancetype)defaultCenter;
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject;
@end

@interface FakeNSDistributedNotificationCenter : NSObject <CKKSNSDistributedNotificationCenter>
@end

@interface FakeCKZone : NSObject
// Used while mocking: database is the contents of the current current CloudKit database, pastDatabase is the state of the world in the past at different change tokens
@property CKRecordZoneID* zoneID;
@property CKServerChangeToken* currentChangeToken;
@property NSMutableDictionary<CKRecordID*, CKRecord*>* currentDatabase;
@property NSMutableDictionary<CKServerChangeToken*, NSMutableDictionary<CKRecordID*, CKRecord*>*>* pastDatabases;
@property bool flag;  // used however you'd like in a test
@property (nullable) CKServerChangeToken* limitFetchTo; // Only return partial results up until here (if asking for current change token)
@property (nullable) NSError* limitFetchError; // If limitFetchTo fires, finish the operation with this error (likely a network timeout)
@property int fetchRecordZoneChangesOperationCount;
@property NSMutableArray<NSDate*>* fetchRecordZoneChangesTimestamps;

// Usually nil. If set, trying to 'create' this zone should fail.
@property (nullable) NSError* creationError;

// Usually false. If set, trying to 'create' this should should 1) pretend to succeed but 2) delete this zone from existence
@property bool failCreationSilently;

// Usually nil. If set, trying to subscribe to this zone should fail.
@property (nullable) NSError* subscriptionError;

// Serial queue. Use this for transactionality.
@property dispatch_queue_t queue;

// Set this to run some code after a write operation has started, but before any results are delivered
@property (nullable) void (^blockBeforeWriteOperation)(void);

- (instancetype)initZone:(CKRecordZoneID*)zoneID;

// Always Succeed
- (void)addToZone:(CKKSCKRecordHolder*)item zoneID:(CKRecordZoneID*)zoneID;
- (void)addToZone:(CKRecord*)record;

// If you want a transaction of adding, use these
- (CKRecord*)_onqueueAddToZone:(CKKSCKRecordHolder*)item zoneID:(CKRecordZoneID*)zoneID;
- (CKRecord*)_onqueueAddToZone:(CKRecord*)record;

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

// Helpfully creates an internal plugin error that CK would return
+ (NSError*)internalPluginError:(NSString*)serverDomain code:(NSInteger)code description:(NSString*)desc;
@end

@interface FakeCKKSNotifier : NSObject <CKKSNotifier>
@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
