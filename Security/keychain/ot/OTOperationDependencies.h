
#import <Foundation/Foundation.h>

#import "keychain/ot/CuttlefishXPCWrapper.h"
#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/escrowrequest/Framework/SecEscrowRequest.h"

NS_ASSUME_NONNULL_BEGIN

// Used for dependency injection into most OctagonStateTransition operations
@interface OTOperationDependencies : NSObject

@property NSString* containerName;
@property NSString* contextID;

@property OTCuttlefishAccountStateHolder* stateHolder;

@property id<OctagonStateFlagHandler> flagHandler;
@property id<OTSOSAdapter> sosAdapter;
@property (nullable) id<CKKSPeerProvider> octagonAdapter;
@property id<OTAuthKitAdapter> authKitAdapter;
@property id<OTDeviceInformationAdapter> deviceInformationAdapter;
@property (readonly) CuttlefishXPCWrapper* cuttlefishXPCWrapper;
@property (readonly, weak) CKKSViewManager* viewManager;
@property CKKSLockStateTracker* lockStateTracker;
@property Class<SecEscrowRequestable> escrowRequestClass;

- (instancetype)initForContainer:(NSString*)containerName
                       contextID:(NSString*)contextID
                     stateHolder:(OTCuttlefishAccountStateHolder*)stateHolder
                     flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                      sosAdapter:(id<OTSOSAdapter>)sosAdapter
                  octagonAdapter:(id<CKKSPeerProvider> _Nullable)octagonAdapter
                  authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
               deviceInfoAdapter:(id<OTDeviceInformationAdapter>)deviceInfoAdapter
                     viewManager:(CKKSViewManager*)viewManager
                lockStateTracker:(CKKSLockStateTracker *)lockStateTracker
            cuttlefishXPCWrapper:(CuttlefishXPCWrapper *)cuttlefishXPCWrapper
              escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass;
@end

NS_ASSUME_NONNULL_END
