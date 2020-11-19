
#if OCTAGON

#import "utilities/debugging.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OctagonCheckTrustStateOperation.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainView.h"

#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OctagonCheckTrustStateOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishedOp;
@end

@implementation OctagonCheckTrustStateOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    self.finishedOp = [[NSOperation alloc] init];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    // Make sure we're in agreement with TPH about _which_ peer ID we should have
    WEAKIFY(self);
    [self.deps.cuttlefishXPCWrapper fetchTrustStateWithContainer:self.deps.containerName
                                                         context:self.deps.contextID
                                                           reply:^(TrustedPeersHelperPeerState * _Nullable selfPeerState,
                                                                   NSArray<TrustedPeersHelperPeer *> * _Nullable trustedPeers,
                                                                   NSError * _Nullable error) {
            STRONGIFY(self);
            if(error || !selfPeerState || !trustedPeers) {
                secerror("octagon: TPH was unable to determine current peer state: %@", error);
                self.error = error;
                self.nextState = OctagonStateError;
                [self runBeforeGroupFinished:self.finishedOp];

            } else {
                [self afterTPHTrustState:selfPeerState trustedPeers:trustedPeers];
            }
        }];
}

- (void)afterTPHTrustState:(TrustedPeersHelperPeerState*)peerState
              trustedPeers:(NSArray<TrustedPeersHelperPeer *> *)trustedPeers
{
    NSError* localError = nil;

    if (peerState.memberChanges) {
        secnotice("octagon", "Member list changed");
        [self.deps.octagonAdapter sendTrustedPeerSetChangedUpdate];
    }

    bool changedCurrentAccountMetadata = false;
    OTAccountMetadataClassC* currentAccountMetadata = [self.deps.stateHolder loadOrCreateAccountMetadata:&localError];

    if(localError) {
        if([self.deps.lockStateTracker isLockedError:localError]) {
            secerror("octagon-consistency: Unable to fetch current account state due to lock state: %@", localError);
            self.nextState = OctagonStateWaitForClassCUnlock;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        secerror("octagon-consistency: Unable to fetch current account state. Can't ensure consistency: %@", localError);
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    // What state does TPH think it's in?
    // This code is slightly duplicated with rpcTrustStatus; we should probably fix that
    OTAccountMetadataClassC_TrustState trustState = OTAccountMetadataClassC_TrustState_UNKNOWN;
    if(peerState.peerStatus & TPPeerStatusExcluded) {
        trustState = OTAccountMetadataClassC_TrustState_UNTRUSTED;

    } else if(peerState.peerStatus & TPPeerStatusUnknown) {
        trustState = OTAccountMetadataClassC_TrustState_UNTRUSTED;

    } else if((peerState.peerStatus & TPPeerStatusSelfTrust) ||
              (peerState.peerStatus & TPPeerStatusFullyReciprocated) ||
              (peerState.peerStatus & TPPeerStatusPartiallyReciprocated)) {
        trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
    }

    secnotice("octagon-consistency", "TPH's trust state (%@) is considered %@",
              TPPeerStatusToString(peerState.peerStatus),
              OTAccountMetadataClassC_TrustStateAsString(trustState));

    if(trustState == currentAccountMetadata.trustState) {
        secnotice("octagon-consistency", "TPH peer status matches cache: (%@)", TPPeerStatusToString(peerState.peerStatus));
        [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventCheckTrustState];
    } else {
        secerror("octagon-consistency: Locally cached status (%@) does not match TPH's current peer status (%@)",
                 OTAccountMetadataClassC_TrustStateAsString(currentAccountMetadata.trustState),
                 OTAccountMetadataClassC_TrustStateAsString(trustState));

        if (currentAccountMetadata.trustState == OTAccountMetadataClassC_TrustState_TRUSTED && trustState == OTAccountMetadataClassC_TrustState_UNTRUSTED) {
            [[CKKSAnalytics logger] logHardFailureForEventNamed:OctagonEventCheckTrustState withAttributes:nil];
        }

        currentAccountMetadata.trustState = trustState;
        changedCurrentAccountMetadata = true;
    }

    if([peerState.peerID isEqualToString:currentAccountMetadata.peerID] ||
       (peerState.peerID == nil && currentAccountMetadata.peerID == nil)) {
        secnotice("octagon-consistency", "TPH peer ID matches cache: (%@)", peerState.peerID);
    } else {
        secerror("octagon-consistency: Locally cached peer ID (%@) does not match TPH's current peer ID (%@)",
                 currentAccountMetadata.peerID,
                 peerState.peerID);

        currentAccountMetadata.peerID = peerState.peerID;
        changedCurrentAccountMetadata = true;
    }

    if(changedCurrentAccountMetadata) {

        NSError* localError = nil;
        BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
            metadata.trustState = currentAccountMetadata.trustState;
            metadata.peerID = currentAccountMetadata.peerID;
            return metadata;
        } error:&localError];

        if(!persisted || localError) {
            if([self.deps.lockStateTracker isLockedError:localError]) {
                secerror("octagon-consistency: Unable to save new account state due to lock state: %@", localError);
                self.nextState = OctagonStateWaitForClassCUnlock;
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            secerror("octagon-consistency: Unable to save new account state. Erroring: %@", localError);
            self.error = localError;
            self.nextState = OctagonStateError;
            [self runBeforeGroupFinished:self.finishedOp];
            return;
        }

        secnotice("octagon-consistency", "Saved new account metadata");
    }

    // See if we possibly should do an update because peer data in Octagon
    // is different from local state, ie we upgraded
    if (peerState.osVersion) {
        NSString *osVersion = self.deps.deviceInformationAdapter.osVersion;
        if (osVersion && ![osVersion isEqualToString:peerState.osVersion]) {
            OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                            conditions:OctagonPendingConditionsDeviceUnlocked];
            [self.deps.flagHandler handlePendingFlag:pendingFlag];
        }
    }

    // And determine where to go from here!

    if(currentAccountMetadata.peerID && currentAccountMetadata.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
        secnotice("octagon", "Appear to be trusted for peer %@; ensuring correct state", currentAccountMetadata.peerID);
        self.nextState = OctagonStateEnsureConsistency;

    } else if(self.deps.sosAdapter.sosEnabled &&
              currentAccountMetadata.trustState != OTAccountMetadataClassC_TrustState_TRUSTED &&
              OctagonPerformSOSUpgrade()) {
        secnotice("octagon", "Have iCloud account but not trusted in Octagon yet: %@; attempting SOS upgrade",
                  [currentAccountMetadata trustStateAsString:currentAccountMetadata.trustState]);

        self.nextState = OctagonStateAttemptSOSUpgrade;

    } else if(currentAccountMetadata.trustState != OTAccountMetadataClassC_TrustState_TRUSTED) {
        secnotice("octagon", "Have iCloud account but not trusted in Octagon (%@)",
                  OTAccountMetadataClassC_TrustStateAsString(currentAccountMetadata.trustState));
#if TARGET_OS_WATCH
        self.nextState = OctagonStateStartCompanionPairing;
#else
        self.nextState = OctagonStateBecomeUntrusted;
#endif
    } else {
        secnotice("octagon", "Unknown trust state (%@). Assuming untrusted...",
                  OTAccountMetadataClassC_TrustStateAsString(currentAccountMetadata.trustState));
        self.nextState = OctagonStateBecomeUntrusted;
    }

    [self runBeforeGroupFinished:self.finishedOp];
}

@end

#endif // OCTAGON
