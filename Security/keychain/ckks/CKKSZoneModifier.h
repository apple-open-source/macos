#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSZoneModifyOperations : NSObject
@property CKKSResultOperation* zoneModificationOperation;
@property CKKSResultOperation* zoneSubscriptionOperation;

@property (readonly) NSMutableArray<CKRecordZone*>* zonesToCreate;
@property (readonly) NSMutableArray<CKRecordZoneSubscription*>* subscriptionsToSubscribe;
@property (readonly) NSMutableArray<CKRecordZoneID*>* zoneIDsToDelete;

- (instancetype)init NS_UNAVAILABLE;

// After the operations run, these properties will be filled in:
// Note that your zone may or may not succeed even if the whole operation fails
@property (nullable) NSArray<CKRecordZone*>* savedRecordZones;
@property (nullable) NSArray<CKRecordZoneID*>* deletedRecordZoneIDs;
// The error from the zone modifcation, if any, will be on the zoneModificationOperation

@property (nullable) NSArray<CKSubscription*>* savedSubscriptions;
@property (nullable) NSArray<NSString*>* deletedSubscriptionIDs;
// The error from the zone subscription, if any, will be on the zoneSubscriptionOperation
@end


@interface CKKSZoneModifier : NSObject
@property (readonly) CKKSReachabilityTracker* reachabilityTracker;
@property (readonly) CKKSCloudKitClassDependencies* cloudKitClassDependencies;
@property (readonly) CKContainer* container;
@property (readonly) CKDatabase* database;

// Block on this to schedule operations for directly after a CloudKit retryAfter delay expires
@property (readonly) CKKSNearFutureScheduler* cloudkitRetryAfter;

// Send every CK error here: it will update the cloudkitRetryAfter scheduler
- (void)inspectErrorForRetryAfter:(NSError*)ckerror;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithContainer:(CKContainer*)container
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
             cloudkitDependencies:(CKKSCloudKitClassDependencies*)_cloudKitClassDependencies;

- (CKKSZoneModifyOperations*)createZone:(CKRecordZone*)zone;

// Note that on a zone delete, we do not run the subscription operation
// It may or may not run, depending on if any other zone creations have been requested
- (CKKSZoneModifyOperations*)deleteZone:(CKRecordZoneID*)zoneID;

// For tests only
- (void)halt;

@end

NS_ASSUME_NONNULL_END

#endif
