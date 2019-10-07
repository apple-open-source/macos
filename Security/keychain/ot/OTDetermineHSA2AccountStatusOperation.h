
#import <Foundation/Foundation.h>

#if OCTAGON

#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTOperationDependencies.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTDetermineHSA2AccountStatusOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>
- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                         stateIfHSA2:(OctagonState*)stateIfHSA2
                      stateIfNotHSA2:(OctagonState*)stateIfNotHSA2
                    stateIfNoAccount:(OctagonState*)stateIfNoAccount
                          errorState:(OctagonState*)errorState;

@property OctagonState* nextState;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
