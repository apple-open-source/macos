
#import <Foundation/Foundation.h>

#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ckks/CKKSResultOperation.h"

NS_ASSUME_NONNULL_BEGIN

@interface EscrowRequestInformCloudServicesOperation : CKKSResultOperation <OctagonStateTransitionOperationProtocol>

- (instancetype)initWithIntendedState:(OctagonState*)intendedState
                           errorState:(OctagonState*)errorState
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker;

+ (NSData* _Nullable)triggerCloudServicesPasscodeRequest:(NSString*)uuid error:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
