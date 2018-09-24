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

#ifndef CloudKitDependencies_h
#define CloudKitDependencies_h

#import <ApplePushService/ApplePushService.h>
#import <CloudKit/CloudKit.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/* CKModifyRecordZonesOperation */
@protocol CKKSModifyRecordZonesOperation <NSObject>
+ (instancetype)alloc;
- (instancetype)initWithRecordZonesToSave:(nullable NSArray<CKRecordZone*>*)recordZonesToSave
                    recordZoneIDsToDelete:(nullable NSArray<CKRecordZoneID*>*)recordZoneIDsToDelete;

@property (nonatomic, strong, nullable) CKDatabase* database;
@property (nonatomic, copy, nullable) NSArray<CKRecordZone*>* recordZonesToSave;
@property (nonatomic, copy, nullable) NSArray<CKRecordZoneID*>* recordZoneIDsToDelete;
@property NSOperationQueuePriority queuePriority;
@property NSQualityOfService qualityOfService;
@property (nonatomic, strong, nullable) CKOperationGroup* group;
@property (nonatomic, copy, null_resettable) CKOperationConfiguration *configuration;

@property (nonatomic, copy, nullable) void (^modifyRecordZonesCompletionBlock)
    (NSArray<CKRecordZone*>* _Nullable savedRecordZones, NSArray<CKRecordZoneID*>* _Nullable deletedRecordZoneIDs, NSError* _Nullable operationError);

@end

@interface CKModifyRecordZonesOperation (SecCKKSModifyRecordZonesOperation) <CKKSModifyRecordZonesOperation>
;
@end

/* CKModifySubscriptionsOperation */
@protocol CKKSModifySubscriptionsOperation <NSObject>
+ (instancetype)alloc;
- (instancetype)initWithSubscriptionsToSave:(nullable NSArray<CKSubscription*>*)subscriptionsToSave
                    subscriptionIDsToDelete:(nullable NSArray<NSString*>*)subscriptionIDsToDelete;

@property (nonatomic, strong, nullable) CKDatabase* database;
@property (nonatomic, copy, nullable) NSArray<CKSubscription*>* subscriptionsToSave;
@property (nonatomic, copy, nullable) NSArray<NSString*>* subscriptionIDsToDelete;
@property NSOperationQueuePriority queuePriority;
@property NSQualityOfService qualityOfService;
@property (nonatomic, strong, nullable) CKOperationGroup* group;
@property (nonatomic, copy, null_resettable) CKOperationConfiguration *configuration;

@property (nonatomic, copy, nullable) void (^modifySubscriptionsCompletionBlock)
    (NSArray<CKSubscription*>* _Nullable savedSubscriptions, NSArray<NSString*>* _Nullable deletedSubscriptionIDs, NSError* _Nullable operationError);
@end

@interface CKModifySubscriptionsOperation (SecCKKSModifySubscriptionsOperation) <CKKSModifySubscriptionsOperation>
;
@end

/* CKFetchRecordZoneChangesOperation */
@protocol CKKSFetchRecordZoneChangesOperation <NSObject>
+ (instancetype)alloc;
- (instancetype)initWithRecordZoneIDs:(NSArray<CKRecordZoneID*>*)recordZoneIDs
                configurationsByRecordZoneID:(nullable NSDictionary<CKRecordZoneID*, CKFetchRecordZoneChangesConfiguration*>*)configurationsByRecordZoneID;

@property (nonatomic, strong, nullable) CKDatabase *database;
@property (nonatomic, copy, nullable) NSArray<CKRecordZoneID*>* recordZoneIDs;
@property (nonatomic, copy, nullable) NSDictionary<CKRecordZoneID*, CKFetchRecordZoneChangesConfiguration*>* configurationsByRecordZoneID;

@property (nonatomic, assign) BOOL fetchAllChanges;
@property (nonatomic, copy, nullable) void (^recordChangedBlock)(CKRecord* record);
@property (nonatomic, copy, nullable) void (^recordWithIDWasDeletedBlock)(CKRecordID* recordID, NSString* recordType);
@property (nonatomic, copy, nullable) void (^recordZoneChangeTokensUpdatedBlock)
    (CKRecordZoneID* recordZoneID, CKServerChangeToken* _Nullable serverChangeToken, NSData* _Nullable clientChangeTokenData);
@property (nonatomic, copy, nullable) void (^recordZoneFetchCompletionBlock)(CKRecordZoneID* recordZoneID,
                                                                             CKServerChangeToken* _Nullable serverChangeToken,
                                                                             NSData* _Nullable clientChangeTokenData,
                                                                             BOOL moreComing,
                                                                             NSError* _Nullable recordZoneError);
@property (nonatomic, copy, nullable) void (^fetchRecordZoneChangesCompletionBlock)(NSError* _Nullable operationError);

@property (nonatomic, strong, nullable) CKOperationGroup* group;
@property (nonatomic, copy, null_resettable) CKOperationConfiguration *configuration;

@property (nonatomic, copy) NSString *operationID;
@property (nonatomic, readonly, strong, nullable) CKOperationConfiguration *resolvedConfiguration;
@end

@interface CKFetchRecordZoneChangesOperation () <CKKSFetchRecordZoneChangesOperation>
@end

/* CKFetchRecordsOperation */
@protocol CKKSFetchRecordsOperation <NSObject>
+ (instancetype)alloc;
- (instancetype)init;
- (instancetype)initWithRecordIDs:(NSArray<CKRecordID*>*)recordIDs;

@property (nonatomic, strong, nullable) CKDatabase *database;
@property (nonatomic, copy, nullable) NSArray<CKRecordID*>* recordIDs;
@property (nonatomic, copy, nullable) NSArray<NSString*>* desiredKeys;
@property (nonatomic, copy, nullable) CKOperationConfiguration* configuration;
@property (nonatomic, copy, nullable) void (^perRecordProgressBlock)(CKRecordID* recordID, double progress);
@property (nonatomic, copy, nullable) void (^perRecordCompletionBlock)
    (CKRecord* _Nullable record, CKRecordID* _Nullable recordID, NSError* _Nullable error);
@property (nonatomic, copy, nullable) void (^fetchRecordsCompletionBlock)
    (NSDictionary<CKRecordID*, CKRecord*>* _Nullable recordsByRecordID, NSError* _Nullable operationError);
@end

@interface CKFetchRecordsOperation () <CKKSFetchRecordsOperation>
@end

/* CKQueryOperation */

@protocol CKKSQueryOperation <NSObject>
+ (instancetype)alloc;
- (instancetype)initWithQuery:(CKQuery*)query;
//Not implemented: - (instancetype)initWithCursor:(CKQueryCursor *)cursor;

@property (nonatomic, copy, nullable) CKQuery* query;
@property (nonatomic, copy, nullable) CKQueryCursor* cursor;

@property (nonatomic, copy, nullable) CKRecordZoneID* zoneID;
@property (nonatomic, assign) NSUInteger resultsLimit;
@property (nonatomic, copy, nullable) NSArray<NSString*>* desiredKeys;

@property (nonatomic, copy, nullable) void (^recordFetchedBlock)(CKRecord* record);
@property (nonatomic, copy, nullable) void (^queryCompletionBlock)(CKQueryCursor* _Nullable cursor, NSError* _Nullable operationError);
@end

@interface CKQueryOperation () <CKKSQueryOperation>
@end

/* APSConnection */
@protocol CKKSAPSConnection <NSObject>
+ (instancetype)alloc;
- (id)initWithEnvironmentName:(NSString*)environmentName
            namedDelegatePort:(NSString*)namedDelegatePort
                        queue:(dispatch_queue_t)queue;

- (void)setEnabledTopics:(NSArray*)enabledTopics;

@property (nonatomic, readwrite, assign) id<APSConnectionDelegate> delegate;
@end

@interface APSConnection (SecCKKSAPSConnection) <CKKSAPSConnection>
@end

/* NSNotificationCenter */
@protocol CKKSNSNotificationCenter <NSObject>
+ (instancetype)defaultCenter;
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject;
- (void)removeObserver:(id)observer;
@end
@interface NSNotificationCenter () <CKKSNSNotificationCenter>
@end

/* Since CKDatabase doesn't share any types with NSOperationQueue, tell the type system about addOperation */
@protocol CKKSOperationQueue <NSObject>
- (void)addOperation:(NSOperation*)operation;
@end

@interface CKDatabase () <CKKSOperationQueue>
@end

@interface NSOperationQueue () <CKKSOperationQueue>
@end

NS_ASSUME_NONNULL_END

#endif /* CloudKitDependencies_h */
