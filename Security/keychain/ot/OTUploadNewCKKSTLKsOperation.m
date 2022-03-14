
#if OCTAGON

#import "utilities/debugging.h"

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/OTUploadNewCKKSTLKsOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/ErrorUtils.h"

@interface OTUploadNewCKKSTLKsOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* ckksConflictState;
@property OctagonState* peerMissingState;

@property NSOperation* finishedOp;
@end

@implementation OTUploadNewCKKSTLKsOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                   ckksConflictState:(OctagonState*)ckksConflictState
                    peerMissingState:(OctagonState*)peerMissingState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _ckksConflictState = ckksConflictState;
        _peerMissingState = peerMissingState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "Beginning an operation to upload any pending CKKS tlks");

    WEAKIFY(self);

    // One (or more) of our CKKS views believes it needs to upload new TLKs.
    NSSet<CKKSKeychainViewState*>* viewsToUpload = [self.deps.ckks viewsRequiringTLKUpload];

    if(viewsToUpload.count == 0) {
         // Nothing to do; return to ready
        secnotice("octagon-ckks", "No CKKS views need uploads");
        self.nextState = self.intendedState;
        return;
    }

    secnotice("octagon-ckks", "CKKS needs TLK uploads for %@", viewsToUpload);

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon", "Finishing an update TLKs operation with %@", self.error ?: @"no error");
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps
                                                                                      viewsToFetch:viewsToUpload];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"upload-tlks-with-keys"
                                                            withBlock:^{
        STRONGIFY(self);
        [self proceedWithKeys:fetchKeysOp.viewKeySets
             pendingTLKShares:fetchKeysOp.pendingTLKShares];
    }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets
       pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
{
    WEAKIFY(self);

    if(viewKeySets.count == 0 && pendingTLKShares.count == 0) {
        // Nothing to do
        secnotice("octagon-ckks", "No CKKS views gave us TLKs to upload");
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
   }

    secnotice("octagon-ckks", "Beginning tlk upload with keys: %@", viewKeySets);
    [self.deps.cuttlefishXPCWrapper updateTLKsWithContainer:self.deps.containerName
                                                    context:self.deps.contextID
                                                   ckksKeys:viewKeySets
                                                  tlkShares:pendingTLKShares
                                                      reply:^(NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable error) {
            STRONGIFY(self);

            if(error) {
                if ([error isCuttlefishError:CuttlefishErrorKeyHierarchyAlreadyExists]) {
                    secnotice("octagon-ckks", "A CKKS key hierarchy is out of date; moving to '%@'", self.ckksConflictState);
                    self.nextState = self.ckksConflictState;
                } else if ([error isCuttlefishError:CuttlefishErrorUpdateTrustPeerNotFound]) {
                    secnotice("octagon-ckks", "Cuttlefish reports we no longer exist.");
                    self.nextState = self.peerMissingState;
                    self.error = error;

                } else {
                    secerror("octagon: Error calling tlk upload: %@", error);
                    self.error = error;
                }
            } else {
                // Tell CKKS about our shiny new records!
                [self.deps.ckks receiveTLKUploadRecords:keyHierarchyRecords];

                self.nextState = self.intendedState;
            }
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
