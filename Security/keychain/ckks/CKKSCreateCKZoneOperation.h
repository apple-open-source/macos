
#import <Foundation/Foundation.h>

#if OCTAGON

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSCreateCKZoneOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>
@property CKKSOperationDependencies* deps;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
