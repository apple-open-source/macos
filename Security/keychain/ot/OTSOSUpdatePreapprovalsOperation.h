
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTSOSAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@class OTOperationDependencies;

@interface OTSOSUpdatePreapprovalsOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>

@property OctagonState* sosNotPresentState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                  sosNotPresentState:(OctagonState*)sosNotPresentState
                          errorState:(OctagonState*)errorState;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

