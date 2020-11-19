
#import <Foundation/Foundation.h>

#if OCTAGON

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSOperationDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSLocalResetOperation : CKKSResultOperation <OctagonStateTransitionOperationProtocol>
@property CKKSOperationDependencies* deps;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState;

// Used to run a local reset without scheduling its surrounding operation.
// Please be on a SQL transaction when you run this.
- (void)onqueuePerformLocalReset;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
