

#if OCTAGON

#import <TargetConditionals.h>
#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTSOSUpdatePreapprovalsOperation.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CloudKitCategories.h"

@interface OTSOSUpdatePreapprovalsOperation ()
@property OTOperationDependencies* deps;

// Since we're making callback based async calls, use this operation trick to hold off the ending of this operation
@property NSOperation* finishedOp;
@end

@implementation OTSOSUpdatePreapprovalsOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                  sosNotPresentState:(OctagonState*)sosNotPresentState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _sosNotPresentState = sosNotPresentState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    WEAKIFY(self);

    if(!self.deps.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "SOS not enabled on this platform?");
        return;
    }

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            // Is this a very scary error?
            bool fatal = false;

            NSTimeInterval ckDelay = CKRetryAfterSecondsForError(self.error);
            NSTimeInterval cuttlefishDelay = [self.error cuttlefishRetryAfter];
            NSTimeInterval delay = MAX(ckDelay, cuttlefishDelay);
            if (delay == 0) {
                delay = 30;
            }

            if([self.error isCuttlefishError:CuttlefishErrorResultGraphNotFullyReachable]) {
                secnotice("octagon-sos", "SOS update preapproval error is 'result graph not reachable'; retrying is useless: %@", self.error);
                fatal = true;
            }

            if([self.error.domain isEqualToString:TrustedPeersHelperErrorDomain] && self.error.code == TrustedPeersHelperErrorNoPreparedIdentity) {
                secnotice("octagon-sos", "SOS update preapproval error is 'no prepared identity'; retrying immediately is useless: %@", self.error);
                fatal = true;
            }

            if(!fatal) {
                secnotice("octagon-sos", "SOS update preapproval error is not fatal: requesting retry in %0.2fs: %@", delay, self.error);
                [self.deps.flagHandler handlePendingFlag:[[OctagonPendingFlag alloc] initWithFlag:OctagonFlagAttemptSOSUpdatePreapprovals
                                                                                   delayInSeconds:delay]];
            }
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSError* sosPreapprovalError = nil;
    NSArray<NSData*>* publicSigningSPKIs = [OTSOSAdapterHelpers peerPublicSigningKeySPKIsForCircle:self.deps.sosAdapter error:&sosPreapprovalError];

    if(!publicSigningSPKIs || sosPreapprovalError) {
        secerror("octagon-sos: Can't fetch trusted peers; stopping preapproved key update: %@", sosPreapprovalError);
        self.error = sosPreapprovalError;
        self.nextState = self.sosNotPresentState;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    secnotice("octagon-sos", "Updating SOS preapproved keys to %@", publicSigningSPKIs);

    [self.deps.cuttlefishXPCWrapper setPreapprovedKeysWithContainer:self.deps.containerName
                                                            context:self.deps.contextID
                                                    preapprovedKeys:publicSigningSPKIs
                                                              reply:^(TrustedPeersHelperPeerState* _Nullable peerState, NSError* error) {
            STRONGIFY(self);
            if(error) {
                secerror("octagon-sos: unable to update preapproved keys: %@", error);
                self.error = error;
            } else {
                secnotice("octagon-sos", "Updated SOS preapproved keys");

                if (peerState.memberChanges) {
                    secnotice("octagon", "Member list changed");
                    [self.deps.octagonAdapter sendTrustedPeerSetChangedUpdate];
                }

                self.nextState = self.intendedState;
            }
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
