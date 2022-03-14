
#import "utilities/debugging.h"

#include "keychain/securityd/SecItemServer.h"

#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OTStates.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestController.h"
#import "keychain/keychainupgrader/KeychainItemUpgradeRequestServer.h"

#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKS.h"

#import "keychain/ot/ObjCImprovements.h"

OctagonState* const KeychainItemUpgradeRequestStateNothingToDo = (OctagonState*)@"nothing_to_do";
OctagonState* const KeychainItemUpgradeRequestStateWaitForUnlock = (OctagonState*)@"wait_for_unlock";
OctagonState* const KeychainItemUpgradeRequestStateUpgradePersistentRef = (OctagonState*)@"upgrade_persistent_ref";
OctagonState* const KeychainItemUpgradeRequestStateWaitForTrigger = (OctagonState*)@"wait_for_trigger";
OctagonFlag* const KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade = (OctagonFlag*)@"schedule_pref_upgrade";

@interface KeychainItemUpgradeRequestController ()
@property dispatch_queue_t queue;
@property CKKSLockStateTracker* lockStateTracker;
@property bool haveRecordedDate;
@end

@implementation KeychainItemUpgradeRequestController

- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker
{
    if((self = [super init])) {
        
        WEAKIFY(self);

        _queue = dispatch_queue_create("KeychainItemUpgradeControllerQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _lockStateTracker = lockStateTracker;

        _stateMachine = [[OctagonStateMachine alloc] initWithName:@"keychainitemupgrade"
                                                           states:[NSSet setWithArray:@[KeychainItemUpgradeRequestStateNothingToDo,
                                                                                        KeychainItemUpgradeRequestStateWaitForUnlock,
                                                                                        KeychainItemUpgradeRequestStateUpgradePersistentRef,
                                                                                        KeychainItemUpgradeRequestStateWaitForTrigger]]
                                                            flags: [NSSet setWithArray:@[KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade]]
                                                     initialState:KeychainItemUpgradeRequestStateUpgradePersistentRef
                                                            queue:_queue
                                                      stateEngine:self
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:nil];
        
        _persistentReferenceUpgrader = [[CKKSNearFutureScheduler alloc] initWithName:@"persistent-ref-upgrader"
                                                                        initialDelay:SecCKKSTestsEnabled() ? 100*NSEC_PER_MSEC : 5*NSEC_PER_SEC
                                                                    expontialBackoff:2
                                                                        maximumDelay:SecCKKSTestsEnabled() ? 5*NSEC_PER_SEC : 300*NSEC_PER_SEC
                                                                    keepProcessAlive:false
                                                           dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                               block:^{
                secnotice("upgr-phase3", "CKKSNFS triggered!");
                STRONGIFY(self);
                OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade
                                                                            conditions:OctagonPendingConditionsDeviceUnlocked];
                [self.stateMachine handlePendingFlag:pendingFlag];
        }];
    }

    return self;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol> * _Nullable)_onqueueNextStateMachineTransition:(nonnull OctagonState *)currentState
                                                                                                         flags:(nonnull OctagonFlags *)flags
                                                                                                  pendingFlags:(nonnull id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    dispatch_assert_queue(self.queue);

    if ([currentState isEqualToString:KeychainItemUpgradeRequestStateWaitForUnlock]) {
        secnotice("keychainitemupgrade", "waiting for unlock before continuing with state machine");
        OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"wait-for-unlock"
                                                                            entering:KeychainItemUpgradeRequestStateNothingToDo];
        [op addNullableDependency:self.lockStateTracker.unlockDependency];
        return op;
    } else if ([currentState isEqualToString:KeychainItemUpgradeRequestStateUpgradePersistentRef]) {
        secnotice("keychainitemupgrade", "upgrading persistent refs");
        CFErrorRef upgradeError = nil;
        bool inProgress = false;
        bool success = SecKeychainUpgradePersistentReferences(&inProgress, &upgradeError);
        
        if ([self.lockStateTracker isLockedError:(__bridge NSError *)(upgradeError)]) {
            OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade
                                                                            conditions:OctagonPendingConditionsDeviceUnlocked];
            [pendingFlagHandler _onqueueHandlePendingFlagLater:pendingFlag];

            OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"after-upgrade--attempt-wait-for-unlock"
                                                                                entering:KeychainItemUpgradeRequestStateWaitForUnlock];
            CFReleaseNull(upgradeError);
            return op;
        }
        else if (inProgress && upgradeError == NULL) {
            OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"upgrade-persistent-refs"
                                                                                entering:KeychainItemUpgradeRequestStateUpgradePersistentRef];
            return op;
        }
        else if (inProgress && (upgradeError || success != true)) {
            secnotice("keychainitemupgrade", "hit an error, triggering CKKSNFS: %@", upgradeError);
            [self.persistentReferenceUpgrader trigger];
            OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"wait-for-trigger"
                                                                                entering:KeychainItemUpgradeRequestStateWaitForTrigger];
            CFReleaseNull(upgradeError);
            return op;
        }
        else {
            secnotice("keychainitemupgrade", "finished upgrading items!");
            OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"nothing-to-do"
                                                                                entering:KeychainItemUpgradeRequestStateNothingToDo];
            CFReleaseNull(upgradeError);
            return op;
        }
    } else if ([currentState isEqualToString:KeychainItemUpgradeRequestStateWaitForTrigger]) {
        secnotice("keychainitemupgrade", "waiting for trigger to occur");
        if ([flags _onqueueContains:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade]) {
            [flags _onqueueRemoveFlag:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade];
            secnotice("keychainitemupgrade", "handling persistent ref flag, attempting to upgrade next batch");
            OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"upgrade-persistent-refs"
                                                                                entering:KeychainItemUpgradeRequestStateUpgradePersistentRef];
            return op;
        }
    } else if ([currentState isEqualToString:KeychainItemUpgradeRequestStateNothingToDo]) {
        //all finished! clear any future scheduling
        [self.persistentReferenceUpgrader cancel];
        if ([flags _onqueueContains:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade]) {
            [flags _onqueueRemoveFlag:KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade];
        }
        secnotice("keychainitemupgrade", "nothing to do");
    }
    
    return nil;
}

- (void)triggerKeychainItemUpdateRPC:(nonnull void (^)(NSError * _Nullable))reply
{
    [self.stateMachine startOperation];

    
    if (self.lockStateTracker) {
        [self.lockStateTracker recheck];
    }
    
    CKKSResultOperation<OctagonStateTransitionOperationProtocol>* requestUpgrade
    = [OctagonStateTransitionOperation named:@"upgrade-persistent-ref"
                                    entering:KeychainItemUpgradeRequestStateUpgradePersistentRef];

    NSSet* sourceStates = [NSSet setWithArray: @[KeychainItemUpgradeRequestStateNothingToDo]];

    OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"request-item-upgrade"
                                                                                                              sourceStates:sourceStates
                                                                                                               serialQueue:self.queue
                                                                                                                   timeout:10*NSEC_PER_SEC
                                                                                                              transitionOp:requestUpgrade];
    [self.stateMachine handleExternalRequest:request];
    
    reply(nil);
}

@end
