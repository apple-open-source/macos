#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

#import "keychain/ot/OTAuthKitAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@class OTOperationDependencies;

@interface OTUpdateTrustedDeviceListOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                    listUpdatesState:(OctagonState*)stateIfListUpdates
                          errorState:(OctagonState*)errorState
                           retryFlag:(OctagonFlag* _Nullable)retryFlag;

@property BOOL logForUpgrade;
@end

NS_ASSUME_NONNULL_END

#endif
