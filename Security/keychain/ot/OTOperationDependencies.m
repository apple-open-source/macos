
#import "keychain/ot/OTOperationDependencies.h"

@implementation OTOperationDependencies
- (instancetype)initForContainer:(NSString*)containerName
                       contextID:(NSString*)contextID
                     stateHolder:(OTCuttlefishAccountStateHolder*)stateHolder
                     flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                      sosAdapter:(id<OTSOSAdapter>)sosAdapter
                  octagonAdapter:(id<CKKSPeerProvider> _Nullable)octagonAdapter
                  authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
               deviceInfoAdapter:(id<OTDeviceInformationAdapter>)deviceInfoAdapter
                 ckksAccountSync:(CKKSKeychainView* _Nullable)ckks
                lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
            cuttlefishXPCWrapper:(CuttlefishXPCWrapper *)cuttlefishXPCWrapper
              escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                   notifierClass:(Class<CKKSNotifier>)notifierClass
{
    if((self = [super init])) {
        _containerName = containerName;
        _contextID = contextID;
        _stateHolder = stateHolder;
        _flagHandler = flagHandler;
        _sosAdapter = sosAdapter;
        _octagonAdapter = octagonAdapter;
        _authKitAdapter = authKitAdapter;
        _deviceInformationAdapter = deviceInfoAdapter;
        _ckks = ckks;
        _lockStateTracker = lockStateTracker;
        _cuttlefishXPCWrapper = cuttlefishXPCWrapper;
        _escrowRequestClass = escrowRequestClass;
        _notifierClass = notifierClass;
    }
    return self;
}

@end
