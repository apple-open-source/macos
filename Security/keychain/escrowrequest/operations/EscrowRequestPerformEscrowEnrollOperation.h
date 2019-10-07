
#import <Foundation/Foundation.h>
#import <CoreCDP/CDPStateController.h>

#import "keychain/escrowrequest/generated_source/SecEscrowPendingRecord.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ckks/CKKSGroupOperation.h"

NS_ASSUME_NONNULL_BEGIN

@interface EscrowRequestPerformEscrowEnrollOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>

@property uint64_t numberOfRecordsUploaded;

- (instancetype)initWithIntendedState:(OctagonState*)intendedState
                           errorState:(OctagonState*)errorState
                  enforceRateLimiting:(bool)enforceRateLimiting
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

+ (void)cdpUploadPrerecord:(SecEscrowPendingRecord*)recordToSend
                secretType:(CDPDeviceSecretType)secretType
                     reply:(void (^)(BOOL didUpdate, NSError* _Nullable error))reply;
@end

NS_ASSUME_NONNULL_END
