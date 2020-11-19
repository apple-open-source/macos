#if OCTAGON

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSZoneModifier.h"

#import "keychain/ot/ObjCImprovements.h"

@implementation CKKSZoneModifyOperations

- (instancetype)init
{
    return nil;
}

- (instancetype)initWithZoneModificationOperation:(CKKSResultOperation*)zoneModificationOperationDependency
                        zoneSubscriptionOperation:(CKKSResultOperation*)zoneSubscriptionOperationDependency
{
    if((self = [super init])) {
        _zoneModificationOperation = zoneModificationOperationDependency;
        _zoneSubscriptionOperation = zoneSubscriptionOperationDependency;

        _zonesToCreate = [NSMutableArray array];
        _subscriptionsToSubscribe = [NSMutableArray array];
        _zoneIDsToDelete = [NSMutableArray array];
    }
    return self;
}
@end

// CKKSZoneModifier

@interface CKKSZoneModifier ()

// Returned to clients that call createZone/deleteZone before it launches
@property CKKSZoneModifyOperations* pendingOperations;

@property bool halted;

@property NSOperationQueue* operationQueue;
@property dispatch_queue_t queue;

// Used to linearize all CK operations to avoid tripping over our own feet
@property NSHashTable<CKDatabaseOperation*>* ckOperations;
@property CKKSZoneModifyOperations* inflightOperations;

// Set to true if the last CK operation finished with a network error
// Cleared if the last CK operation didn't
// Used as an heuristic to wait on the reachability tracker
@property bool networkFailure;
@end

@implementation CKKSZoneModifier
- (instancetype)init
{
    return nil;
}

- (instancetype)initWithContainer:(CKContainer*)container
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
             cloudkitDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if((self = [super init])) {
        _container = container;
        _reachabilityTracker = reachabilityTracker;
        _cloudKitClassDependencies = cloudKitClassDependencies;

        _database = [_container privateCloudDatabase];
        _operationQueue = [[NSOperationQueue alloc] init];
        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSZoneModifier.%@", container.containerIdentifier] UTF8String],
                                       DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        WEAKIFY(self);

        // This does double-duty: if there's pending zone creation/deletions, it launches them
        _cloudkitRetryAfter = [[CKKSNearFutureScheduler alloc] initWithName:@"zonemodifier-ckretryafter"
                                                               initialDelay:100*NSEC_PER_MSEC
                                                            continuingDelay:100*NSEC_PER_MSEC
                                                           keepProcessAlive:false
                                                  dependencyDescriptionCode:CKKSResultDescriptionPendingCloudKitRetryAfter
                                                                      block:^{
                                                                          STRONGIFY(self);
                                                                          [self launchOperations];
                                                                      }];

        _ckOperations = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (void)_onqueueCreatePendingObjects
{
    dispatch_assert_queue(self.queue);

    if(!self.pendingOperations) {
        CKKSResultOperation* zoneModificationOperationDependency = [CKKSResultOperation named:@"zone-modification" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            ckksnotice_global("ckkszonemodifier", "finished creating zones");
        }];

        CKKSResultOperation* zoneSubscriptionOperationDependency = [CKKSResultOperation named:@"zone-subscription" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            ckksnotice_global("ckkszonemodifier", "finished subscribing to zones");
        }];

        self.pendingOperations = [[CKKSZoneModifyOperations alloc] initWithZoneModificationOperation:zoneModificationOperationDependency
                                                                           zoneSubscriptionOperation:zoneSubscriptionOperationDependency];
    }
}

- (CKKSZoneModifyOperations*)createZone:(CKRecordZone*)zone
{
    __block CKKSZoneModifyOperations* ops = nil;

    dispatch_sync(self.queue, ^{
        [self _onqueueCreatePendingObjects];
        ops = self.pendingOperations;

        [ops.zonesToCreate addObject:zone];
        CKRecordZoneSubscription* subscription = [[CKRecordZoneSubscription alloc] initWithZoneID:zone.zoneID
                                                                                   subscriptionID:[@"zone:" stringByAppendingString: zone.zoneID.zoneName]];
        [ops.subscriptionsToSubscribe addObject:subscription];
    });

    [self.cloudkitRetryAfter trigger];

    return ops;
}

- (CKKSZoneModifyOperations*)deleteZone:(CKRecordZoneID*)zoneID
{
    __block CKKSZoneModifyOperations* ops = nil;

    dispatch_sync(self.queue, ^{
        [self _onqueueCreatePendingObjects];
        ops = self.pendingOperations;

        [ops.zoneIDsToDelete addObject:zoneID];
    });

    [self.cloudkitRetryAfter trigger];

    return ops;
}

- (void)halt
{
    dispatch_sync(self.queue, ^{
        self.halted = true;
    });
}

// Called by the NearFutureScheduler
- (void)launchOperations
{
    dispatch_sync(self.queue, ^{
        if(self.halted) {
            ckksnotice_global("ckkszonemodifier", "Halted; not launching operations");
            return;
        }

        CKKSZoneModifyOperations* ops = self.pendingOperations;
        if(!ops) {
            ckksinfo_global("ckkszonemodifier", "No pending zone modification operations; quitting");
            return;
        }

        if(self.inflightOperations && (![self.inflightOperations.zoneModificationOperation isFinished] ||
                                       ![self.inflightOperations.zoneSubscriptionOperation isFinished])) {
            ckksnotice_global("ckkszonemodifier", "Have in-flight zone modification operations, will retry later");

            WEAKIFY(self);
            CKKSResultOperation* retrigger = [CKKSResultOperation named:@"retry" withBlock:^{
                STRONGIFY(self);
                [self.cloudkitRetryAfter trigger];
            }];
            [retrigger addNullableDependency:self.inflightOperations.zoneModificationOperation];
            [retrigger addNullableDependency:self.inflightOperations.zoneSubscriptionOperation];
            [self.operationQueue addOperation:retrigger];
            return;
        }

        self.pendingOperations = nil;
        self.inflightOperations = ops;

        CKDatabaseOperation<CKKSModifyRecordZonesOperation>* modifyZonesOperation = [self createModifyZonesOperation:ops];
        CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation = [self createModifySubscriptionsOperation:ops];

        [self.database addOperation:modifyZonesOperation];
        if(zoneSubscriptionOperation) {
            [self.database addOperation:zoneSubscriptionOperation];
        }
    });
}

- (CKDatabaseOperation<CKKSModifyRecordZonesOperation>*)createModifyZonesOperation:(CKKSZoneModifyOperations*)ops
{
    ckksnotice_global("ckkszonemodifier", "Attempting to create zones %@, delete zones %@", ops.zonesToCreate, ops.zoneIDsToDelete);

    CKDatabaseOperation<CKKSModifyRecordZonesOperation>* zoneModifyOperation = [[self.cloudKitClassDependencies.modifyRecordZonesOperationClass alloc] initWithRecordZonesToSave:ops.zonesToCreate recordZoneIDsToDelete:ops.zoneIDsToDelete];
    [zoneModifyOperation linearDependencies:self.ckOperations];

    zoneModifyOperation.configuration.automaticallyRetryNetworkFailures = NO;
    zoneModifyOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    zoneModifyOperation.configuration.isCloudKitSupportOperation = YES;
    zoneModifyOperation.database = self.database;
    zoneModifyOperation.name = @"zone-creation-operation";
    zoneModifyOperation.group = [CKOperationGroup CKKSGroupWithName:@"zone-creation"];;

    // We will use the zoneCreationOperation operation in ops to signal completion
    WEAKIFY(self);

    zoneModifyOperation.modifyRecordZonesCompletionBlock = ^(NSArray<CKRecordZone *> *savedRecordZones,
                                                             NSArray<CKRecordZoneID *> *deletedRecordZoneIDs,
                                                             NSError *operationError) {
        STRONGIFY(self);

        if(operationError) {
            ckkserror_global("ckkszonemodifier", "Zone modification failed: %@", operationError);
            [self inspectErrorForRetryAfter:operationError];

            if ([self.reachabilityTracker isNetworkError:operationError]){
                self.networkFailure = true;
            }
        }
        ckksnotice_global("ckkszonemodifier", "created zones: %@", savedRecordZones);
        ckksnotice_global("ckkszonemodifier", "deleted zones: %@", deletedRecordZoneIDs);

        ops.savedRecordZones = savedRecordZones;
        ops.deletedRecordZoneIDs = deletedRecordZoneIDs;
        ops.zoneModificationOperation.error = operationError;

        [self.operationQueue addOperation:ops.zoneModificationOperation];
    };

    if(self.networkFailure) {
        ckksnotice_global("ckkszonemodifier", "Waiting for reachabilty before issuing zone creation");
        [zoneModifyOperation addNullableDependency:self.reachabilityTracker.reachabilityDependency];
    }

    return zoneModifyOperation;
}

- (CKDatabaseOperation<CKKSModifySubscriptionsOperation>* _Nullable)createModifySubscriptionsOperation:(CKKSZoneModifyOperations*)ops
{
    ckksnotice_global("ckkszonemodifier", "Attempting to subscribe to zones %@", ops.subscriptionsToSubscribe);

    if(ops.subscriptionsToSubscribe.count == 0) {
        [self.operationQueue addOperation: ops.zoneSubscriptionOperation];
        return nil;
    }

    CKDatabaseOperation<CKKSModifySubscriptionsOperation>* zoneSubscriptionOperation = [[self.cloudKitClassDependencies.modifySubscriptionsOperationClass alloc] initWithSubscriptionsToSave:ops.subscriptionsToSubscribe subscriptionIDsToDelete:nil];
    [zoneSubscriptionOperation linearDependencies:self.ckOperations];

    zoneSubscriptionOperation.configuration.automaticallyRetryNetworkFailures = NO;
    zoneSubscriptionOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    zoneSubscriptionOperation.configuration.isCloudKitSupportOperation = YES;
    zoneSubscriptionOperation.database = self.database;
    zoneSubscriptionOperation.name = @"zone-subscription-operation";

    WEAKIFY(self);
    zoneSubscriptionOperation.modifySubscriptionsCompletionBlock = ^(NSArray<CKSubscription *> * _Nullable savedSubscriptions,
                                                                     NSArray<NSString *> * _Nullable deletedSubscriptionIDs,
                                                                     NSError * _Nullable operationError) {
        STRONGIFY(self);

        if(operationError) {
            ckkserror_global("ckkszonemodifier", "Couldn't create cloudkit zone subscription; keychain syncing is severely degraded: %@", operationError);
            [self inspectErrorForRetryAfter:operationError];

            if ([self.reachabilityTracker isNetworkError:operationError]){
                self.networkFailure = true;
            }
        }
        ckksnotice_global("ckkszonemodifier", "Successfully subscribed to %@", savedSubscriptions);

        ops.savedSubscriptions = savedSubscriptions;
        ops.deletedSubscriptionIDs = deletedSubscriptionIDs;
        ops.zoneSubscriptionOperation.error = operationError;

        [self.operationQueue addOperation: ops.zoneSubscriptionOperation];
    };

    [zoneSubscriptionOperation addNullableDependency:ops.zoneModificationOperation];

    if(self.networkFailure) {
        ckksnotice_global("ckkszonemodifier", "Waiting for reachabilty before issuing zone subscription");
        [zoneSubscriptionOperation addNullableDependency:self.reachabilityTracker.reachabilityDependency];
    }

    return zoneSubscriptionOperation;
}

- (void)inspectErrorForRetryAfter:(NSError*)ckerror
{
    NSTimeInterval delay = CKRetryAfterSecondsForError(ckerror);
    if(delay) {
        uint64_t ns_delay = NSEC_PER_SEC * ((uint64_t) delay);
        ckksnotice_global("ckkszonemodifier", "CK operation failed with rate-limit, scheduling delay for %.1f seconds: %@", delay, ckerror);
        [self.cloudkitRetryAfter waitUntil:ns_delay];
    }
}

@end

#endif // OCTAGON
