
#import <Foundation/Foundation.h>
#import "keychain/ot/OctagonStateMachine.h"

NS_ASSUME_NONNULL_BEGIN

extern OctagonState* const KeychainItemUpgradeRequestStateNothingToDo;
extern OctagonState* const KeychainItemUpgradeRequestStateWaitForUnlock;
extern OctagonState* const KeychainItemUpgradeRequestStateUpgradePersistentRef;
extern OctagonFlag* const KeychainItemUpgradeRequestFlagSchedulePersistentReferenceUpgrade;
extern OctagonState* const KeychainItemUpgradeRequestStateWaitForTrigger;

@class CKKSLockStateTracker;
@class CKKSNearFutureScheduler;

@interface KeychainItemUpgradeRequestController : NSObject <OctagonStateMachineEngine>
@property OctagonStateMachine* stateMachine;
@property CKKSNearFutureScheduler* persistentReferenceUpgrader;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

- (void)triggerKeychainItemUpdateRPC:(nonnull void (^)(NSError * _Nullable))reply;

@end

NS_ASSUME_NONNULL_END
