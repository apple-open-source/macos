
#if OCTAGON

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSOperationDependencies.h"

@implementation CKKSOperationDependencies

- (instancetype)initWithZoneID:(CKRecordZoneID*)zoneID
                  zoneModifier:(CKKSZoneModifier*)zoneModifier
              ckoperationGroup:(CKOperationGroup* _Nullable)operationGroup
                   flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                launchSequence:(CKKSLaunchSequence*)launchSequence
              lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
           reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                 peerProviders:(NSArray<id<CKKSPeerProvider>>*)peerProviders
              databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
    notifyViewChangedScheduler:(CKKSNearFutureScheduler*)notifyViewChangedScheduler
              savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
{
    if((self = [super init])) {
        _zoneID = zoneID;
        _zoneModifier = zoneModifier;
        _ckoperationGroup = operationGroup;
        _flagHandler = flagHandler;
        _launch = launchSequence;
        _lockStateTracker = lockStateTracker;
        _reachabilityTracker = reachabilityTracker;
        _peerProviders = peerProviders;
        _databaseProvider = databaseProvider;
        _notifyViewChangedScheduler = notifyViewChangedScheduler;
        _savedTLKNotifier = savedTLKNotifier;

        _keysetProviderOperations = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    NSArray<id<CKKSPeerProvider>>* peerProviders = self.peerProviders;
    NSMutableArray<CKKSPeerProviderState*>* trustStates = [NSMutableArray array];

#if DEBUG
    NSAssert(![self.databaseProvider insideSQLTransaction], @"Cannot fetch current trust states from inside a SQL transaction, on pain of deadlocK");
#endif

    for(id<CKKSPeerProvider> provider in peerProviders) {
        ckksnotice("ckks", self.zoneID, "Fetching account keys for provider %@", provider);
        [trustStates addObject:provider.currentState];
    }

    return trustStates;
}

- (void)provideKeySet:(CKKSCurrentKeySet*)keyset
{
    if(!keyset || !keyset.currentTLKPointer.currentKeyUUID) {
        ckksnotice("ckkskey", self.zoneID, "No valid keyset provided: %@", keyset);
        return;
    }
    ckksnotice("ckkskey", self.zoneID, "Providing keyset (%@) to listeners", keyset);

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in self.keysetProviderOperations) {
        [op provideKeySet:keyset];
    }
}

@end

#endif // OCTAGON
