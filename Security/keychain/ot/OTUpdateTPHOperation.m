
#if OCTAGON

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTUpdateTPHOperation.h"

@interface OTUpdateTPHOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* peerUnknownState;

@property NSOperation* finishedOp;

@property (nullable) OctagonFlag* retryFlag;
@end

@implementation OTUpdateTPHOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                    peerUnknownState:(OctagonState*)peerUnknownState
                          errorState:(OctagonState*)errorState
                           retryFlag:(OctagonFlag* _Nullable)retryFlag
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
        _peerUnknownState = peerUnknownState;

        _retryFlag = retryFlag;
    }
    return self;
}

- (void)groupStart
{
    WEAKIFY(self);
    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            if(self.retryFlag == nil) {
                secerror("octagon: Received an error updating TPH, but no retry flag present.");
                return;
            }

            // Is this a very scary error?
            bool fatal = true;

            OctagonPendingFlag* pendingFlag = nil;

            if([self.deps.lockStateTracker isLockedError:self.error]) {
                secnotice("octagon", "Updating trust state failed because locked, retry once unlocked: %@", self.error);
                self.nextState = OctagonStateWaitForUnlock;
                pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:self.retryFlag
                                                            conditions:OctagonPendingConditionsDeviceUnlocked];
                fatal = false;
            } else {
                // more CloudKit errors should trigger a retry here
                secnotice("octagon", "Error is currently unknown, aborting: %@", self.error);
            }

            if(!fatal) {
                if(!pendingFlag) {
                    NSTimeInterval delay = [self.error overallCuttlefishRetry];

                    pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:self.retryFlag
                                                            delayInSeconds:delay];
                }
                secnotice("octagon", "Updating trust state no fatal: requesting retry: %@",
                          pendingFlag);
                [self.deps.flagHandler handlePendingFlag:pendingFlag];
            }
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];


    [self.deps.cuttlefishXPCWrapper updateWithContainer:self.deps.containerName
                                                context:self.deps.contextID
                                             deviceName:self.deps.deviceInformationAdapter.deviceName
                                           serialNumber:self.deps.deviceInformationAdapter.serialNumber
                                              osVersion:self.deps.deviceInformationAdapter.osVersion
                                          policyVersion:nil
                                          policySecrets:nil
                              syncUserControllableViews:nil
                                                  reply:^(TrustedPeersHelperPeerState* peerState, TPSyncingPolicy* syncingPolicy, NSError* error) {
            STRONGIFY(self);
            if(error || !peerState) {
                secerror("octagon: update errored: %@", error);
                self.error = error;

                if ([error isCuttlefishError:CuttlefishErrorUpdateTrustPeerNotFound]) {
                    secnotice("octagon-ckks", "Cuttlefish reports we no longer exist.");
                    self.nextState = self.peerUnknownState;
                } else {
                    // On an error, for now, go back to the intended state
                    // <rdar://problem/50190005> Octagon: handle lock state errors in update()
                    self.nextState = self.intendedState;
                }
                [self runBeforeGroupFinished:self.finishedOp];
                return;
            }

            secnotice("octagon", "update complete: %@, %@", peerState, syncingPolicy);

            NSError* localError = nil;
            BOOL persisted = [self.deps.stateHolder persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                [metadata setTPSyncingPolicy:syncingPolicy];
                return metadata;
            } error:&localError];
            if(!persisted || localError) {
                secerror("octagon: Unable to save new syncing state: %@", localError);

            } else {
                // After an update(), we're sure that we have a fresh policy
                BOOL viewSetChanged = [self.deps.viewManager setCurrentSyncingPolicy:syncingPolicy policyIsFresh:YES];
                if(viewSetChanged) {
                    [self.deps.flagHandler handleFlag:OctagonFlagCKKSViewSetChanged];
                }
            }

            if(peerState.identityIsPreapproved) {
                secnotice("octagon-sos", "Self peer is now preapproved!");
                [self.deps.flagHandler handleFlag:OctagonFlagEgoPeerPreapproved];
            }
            if (peerState.memberChanges) {
                secnotice("octagon", "Member list changed");
                [self.deps.octagonAdapter sendTrustedPeerSetChangedUpdate];
            }

            if (peerState.unknownMachineIDsPresent) {
                secnotice("octagon-authkit", "Unknown machine IDs are present; requesting fetch");
                [self.deps.flagHandler handleFlag:OctagonFlagFetchAuthKitMachineIDList];
            }

            if (peerState.peerStatus & TPPeerStatusExcluded) {
                secnotice("octagon", "Self peer (%@) is excluded; moving to untrusted", peerState.peerID);
                self.nextState = OctagonStateBecomeUntrusted;
            } else if(peerState.peerStatus & TPPeerStatusUnknown) {
                if (peerState.identityIsPreapproved) {
                    secnotice("octagon", "Self peer (%@) is excluded but is preapproved, moving to sosuprade", peerState.peerID);
                    self.nextState = OctagonStateAttemptSOSUpgrade;
                } else {
                    secnotice("octagon", "Self peer (%@) is unknown; moving to '%@''", peerState.peerID, self.peerUnknownState);
                    self.nextState = self.peerUnknownState;
                }
            } else {
                self.nextState = self.intendedState;
            }

            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
