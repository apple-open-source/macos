
#import <Foundation/Foundation.h>
#import "keychain/ot/OctagonStateMachine.h"

NS_ASSUME_NONNULL_BEGIN

extern OctagonState* const EscrowRequestStateNothingToDo;
extern OctagonState* const EscrowRequestStateTriggerCloudServices;
extern OctagonState* const EscrowRequestStateAttemptEscrowUpload;
extern OctagonState* const EscrowRequestStateWaitForUnlock;

@class CKKSLockStateTracker;

@interface EscrowRequestController : NSObject <OctagonStateMachineEngine>
@property OctagonStateMachine* stateMachine;

// Use this for testing: if set to true, we will always attempt to trigger CloudServices if needed, even if we've done it recently
@property bool forceIgnoreCloudServicesRateLimiting;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithLockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

- (void)triggerEscrowUpdateRPC:(nonnull NSString *)reason
                         reply:(nonnull void (^)(NSError * _Nullable))reply;

- (void)storePrerecordsInEscrowRPC:(void (^)(uint64_t count, NSError* _Nullable error))reply;
@end

NS_ASSUME_NONNULL_END
