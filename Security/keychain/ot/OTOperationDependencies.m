
#import "keychain/ot/OTOperationDependencies.h"

@implementation OTOperationDependencies
- (instancetype)initForContainer:(NSString*)containerName
                       contextID:(NSString*)contextID
                   activeAccount:(TPSpecificUser* _Nullable)activeAccount
                     stateHolder:(OTCuttlefishAccountStateHolder*)stateHolder
                     flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                      sosAdapter:(id<OTSOSAdapter>)sosAdapter
                  octagonAdapter:(id<CKKSPeerProvider> _Nullable)octagonAdapter
                 accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
                  authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                  personaAdapter:(id<OTPersonaAdapter>)personaAdapter
               deviceInfoAdapter:(id<OTDeviceInformationAdapter>)deviceInfoAdapter
                 ckksAccountSync:(CKKSKeychainView* _Nullable)ckks
                lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
            cuttlefishXPCWrapper:(CuttlefishXPCWrapper *)cuttlefishXPCWrapper
              escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                   notifierClass:(Class<CKKSNotifier>)notifierClass
                          flowID:(NSString* _Nullable)flowID
                 deviceSessionID:(NSString* _Nullable)deviceSessionID
          permittedToSendMetrics:(BOOL)permittedToSendMetrics
             reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
{
    if((self = [super init])) {
        _containerName = containerName;
        _contextID = contextID;
        _activeAccount = activeAccount;
        _stateHolder = stateHolder;
        _flagHandler = flagHandler;
        _sosAdapter = sosAdapter;
        _octagonAdapter = octagonAdapter;
        _accountsAdapter = accountsAdapter;
        _authKitAdapter = authKitAdapter;
        _personaAdapter = personaAdapter;
        _deviceInformationAdapter = deviceInfoAdapter;
        _ckks = ckks;
        _lockStateTracker = lockStateTracker;
        _cuttlefishXPCWrapper = cuttlefishXPCWrapper;
        _escrowRequestClass = escrowRequestClass;
        _notifierClass = notifierClass;
        _flowID = flowID;
        _deviceSessionID = deviceSessionID;
        _permittedToSendMetrics = permittedToSendMetrics;
        _reachabilityTracker = reachabilityTracker;
    }
    return self;
}

@end
