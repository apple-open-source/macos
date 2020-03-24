
#if OCTAGON

#import <Foundation/Foundation.h>
#import <TrustedPeers/TrustedPeers.h>
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTSOSAdapter.h"

#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/OTDeviceInformation.h"

NS_ASSUME_NONNULL_BEGIN

@class OTOperationDependencies;

@interface OTSOSUpgradeOperation : CKKSGroupOperation <OctagonStateTransitionOperationProtocol>

@property (readonly, nullable) TPPolicyVersion* policyOverride;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                   ckksConflictState:(OctagonState*)ckksConflictState
                          errorState:(OctagonState*)errorState
                          deviceInfo:(OTDeviceInformation*)deviceInfo
                      policyOverride:(TPPolicyVersion* _Nullable)policyOverride;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

