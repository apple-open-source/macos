
#import "keychain/ot/OTOperationDependencies.h"

@implementation OTOperationDependencies
- (instancetype)initForContainer:(NSString*)containerName
                       contextID:(NSString*)contextID
                     stateHolder:(OTCuttlefishAccountStateHolder*)stateHolder
                     flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                      sosAdapter:(id<OTSOSAdapter>)sosAdapter
                  octagonAdapter:(id<CKKSPeerProvider> _Nullable)octagonAdapter
                  authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                     viewManager:(CKKSViewManager*)viewManager
                lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                   cuttlefishXPC:(id<NSXPCProxyCreating>)cuttlefishXPC
              escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
{
    if((self = [super init])) {
        _containerName = containerName;
        _contextID = contextID;
        _stateHolder = stateHolder;
        _flagHandler = flagHandler;
        _sosAdapter = sosAdapter;
        _octagonAdapter = octagonAdapter;
        _authKitAdapter = authKitAdapter;
        _viewManager = viewManager;
        _lockStateTracker = lockStateTracker;
        _cuttlefishXPC = cuttlefishXPC;
        _escrowRequestClass = escrowRequestClass;
    }
    return self;
}

@end
