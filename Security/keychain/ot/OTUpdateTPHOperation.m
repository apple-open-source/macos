
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

@property NSOperation* finishedOp;

@property (nullable) OctagonFlag* retryFlag;
@end

@implementation OTUpdateTPHOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                           retryFlag:(OctagonFlag* _Nullable)retryFlag
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;

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
            } else if ([self.error isCuttlefishError:CuttlefishErrorTransactionalFailure]) {
                secnotice("octagon", "Transaction failure in cuttlefishm, retrying: %@", self.error);
                fatal = false;
            } else {
                // more CloudKit errors should trigger a retry here
                secnotice("octagon", "Error is currently unknown, aborting: %@", self.error);
            }

            if(!fatal) {
                if(!pendingFlag) {
                    NSTimeInterval baseDelay = SecCKKSTestsEnabled() ? 2 : 30;
                    NSTimeInterval delay = CKRetryAfterSecondsForError(self.error) ?: baseDelay;
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


    [[self.deps.cuttlefishXPC remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        STRONGIFY(self);
        secerror("octagon: Can't talk with TrustedPeersHelper, update is lost: %@", error);
        self.error = error;
        [self runBeforeGroupFinished:self.finishedOp];

    }] updateWithContainer:self.deps.containerName
                   context:self.deps.contextID
                deviceName:nil
              serialNumber:nil
                 osVersion:nil
             policyVersion:nil
             policySecrets:nil
                     reply:^(TrustedPeersHelperPeerState* peerState, NSError* error) {
                         STRONGIFY(self);
                         if(error || !peerState) {
                             secerror("octagon: update errored: %@", error);
                             self.error = error;

                             // On an error, for now, go back to the intended state
                             // <rdar://problem/50190005> Octagon: handle lock state errors in update()
                             self.nextState = self.intendedState;
                             [self runBeforeGroupFinished:self.finishedOp];
                             return;
                         }

                         secnotice("octagon", "update complete: %@", peerState);

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

                         if(peerState.peerStatus & TPPeerStatusExcluded) {
                             secnotice("octagon", "Self peer (%@) is excluded; moving to untrusted", peerState.peerID);
                             self.nextState = OctagonStateBecomeUntrusted;

                         } else if(peerState.peerStatus & TPPeerStatusUnknown) {
                             secnotice("octagon", "Self peer (%@) is unknown; moving to untrusted", peerState.peerID);
                             self.nextState = OctagonStateBecomeUntrusted;

                         } else {
                             self.nextState = self.intendedState;
                         }

                         [self runBeforeGroupFinished:self.finishedOp];
         }];
}

@end

#endif // OCTAGON
