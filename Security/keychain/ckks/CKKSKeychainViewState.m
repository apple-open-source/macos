
#if OCTAGON

#import "keychain/ckks/CKKSKeychainViewState.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ot/ObjCImprovements.h"
#import "utilities/SecTrace.h"

@interface CKKSKeychainViewState ()
@property NSSet<CKKSZoneKeyState*>* allowableStates;
@property NSMutableDictionary<CKKSZoneKeyState*, CKKSCondition*>* mutableStateConditions;

@property BOOL readyNotificationArmed;

@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;
@end

@implementation CKKSKeychainViewState
@synthesize viewKeyHierarchyState = _viewKeyHierarchyState;

- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
               ckksManagedView:(BOOL)ckksManagedView
    notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
      notifyViewReadyScheduler:(CKKSNearFutureScheduler*)notifyViewReadyScheduler
{
    if((self = [super init])) {
        _zoneName = zoneID.zoneName;
        _zoneID = zoneID;

        _ckksManagedView = ckksManagedView;

        _queue = dispatch_queue_create("key-state", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];

        _notifyViewChangedScheduler = notifyViewChangedScheduler;
        _notifyViewReadyScheduler = notifyViewReadyScheduler;

        _allowableStates = CKKSKeyStateValidStates();

        _mutableStateConditions = [[NSMutableDictionary alloc] init];
        [_allowableStates enumerateObjectsUsingBlock:^(OctagonState * _Nonnull obj, BOOL * _Nonnull stop) {
            self.mutableStateConditions[obj] = [[CKKSCondition alloc] init];
        }];

        self.viewKeyHierarchyState = SecCKKSZoneKeyStateInitializing;

        _launch = [[CKKSLaunchSequence alloc] initWithRocketName:@"com.apple.security.ckks.launch"];
        [_launch addAttribute:@"view" value:zoneID.zoneName];

        // We'll never send a ready notification, unless armReadyNotification is called.
        _readyNotificationArmed = NO;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<CKKSKeychainViewState: %@(%@), %@>",
            self.zoneName,
            self.ckksManagedView ? @"ckks": @"ext",
            self.viewKeyHierarchyState];
}

- (id)copyWithZone:(NSZone*)zone
{
    CKKSKeychainViewState* s = [[CKKSKeychainViewState alloc] initWithZoneID:self.zoneID
                                                             ckksManagedView:self.ckksManagedView
                                                  notifyViewChangedScheduler:self.notifyViewReadyScheduler
                                                    notifyViewReadyScheduler:self.notifyViewReadyScheduler];
    s.viewKeyHierarchyState = self.viewKeyHierarchyState;
    return s;
}

- (NSUInteger)hash
{
    return self.zoneName.hash;
}

- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[CKKSKeychainViewState class]]) {
        return NO;
    }

    CKKSKeychainViewState* obj = (CKKSKeychainViewState*)object;
    return [self.zoneName isEqualToString:obj.zoneName] &&
         self.ckksManagedView == obj.ckksManagedView &&
         [self.viewKeyHierarchyState isEqualToString:obj.viewKeyHierarchyState];
}

- (CKKSZoneKeyState*)viewKeyHierarchyState
{
    return _viewKeyHierarchyState;
}

- (void)setViewKeyHierarchyState:(CKKSZoneKeyState*)state
{
#if DEBUG
    if(![self.allowableStates containsObject:state]) {
        ckksnotice_global("ckks", "Invalid key hierarchy state: %@", state);
    }
#endif

    dispatch_sync(self.queue, ^{
        if([state isEqualToString:_viewKeyHierarchyState]) {
            // No change, do nothing.
        } else {
            // Fixup the condition variables as part of setting this state
            if(_viewKeyHierarchyState) {
                self.mutableStateConditions[_viewKeyHierarchyState] = [[CKKSCondition alloc] init];
            }

            NSAssert([self.allowableStates containsObject:state], @"state machine tried to enter unknown state %@", state);
            _viewKeyHierarchyState = state;

            ckksnotice("ckks-view-state", self.zoneID, "Zone is entering %@", state);

            [self.launch addEvent:state];

            if(state) {
                [self.mutableStateConditions[state] fulfill];

                if([state isEqualToString:SecCKKSZoneKeyStateReady]) {
                    [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastKeystateReady zoneName:self.zoneID.zoneName];
                }
            }
        }
    });
}

- (NSDictionary<CKKSZoneKeyState*, CKKSCondition*>*)keyHierarchyConditions
{
    __block NSDictionary* conditions = nil;
    dispatch_sync(self.queue, ^{
        conditions = [self.mutableStateConditions copy];
    });

    return conditions;
}

- (void)launchComplete
{
    [self.launch launch];

    /*
     * Since we think we are ready, signal to CK that its time to check for PCS identities again.
     */

    dispatch_sync(self.queue, ^{
        if(self.readyNotificationArmed) {
            self.readyNotificationArmed = NO;
            [self.notifyViewReadyScheduler trigger];
        }
    });
}

- (void)armReadyNotification
{
    dispatch_sync(self.queue, ^{
        self.readyNotificationArmed = YES;
    });
}

@end

#endif
