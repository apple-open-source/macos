
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

    NSMutableSet<CKKSKeychainView*>* viewsToUpload = [NSMutableSet set];

    // One (or more) of our sub-CKKSes believes it needs to upload new TLKs.
    CKKSViewManager* viewManager = self.deps.viewManager;
    for(CKKSKeychainView* view in viewManager.currentViews) {
        if([view requiresTLKUpload]) {
            secnotice("octagon-ckks", "CKKS view %@ needs TLK uploads!", view);
            [viewsToUpload addObject: view];
        }
    }

    if(viewsToUpload.count == 0) {
         // Nothing to do; return to ready
        secnotice("octagon-ckks", "No CKKS views need uploads");
        self.nextState = self.intendedState;
        return;
    }

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon", "Finishing an update TLKs operation with %@", self.error ?: @"no error");
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithViews:viewsToUpload];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"upload-tlks-with-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets
                                                                     pendingTLKShares:fetchKeysOp.tlkShares
                                                                        viewsToUpload:viewsToUpload];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets
       pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
          viewsToUpload:(NSSet<CKKSKeychainView*>*)viewsToUpload
{
    WEAKIFY(self);

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
                for(CKKSKeychainView* view in viewsToUpload) {
                    secnotice("octagon-ckks", "Providing records to %@", view);
                    [view receiveTLKUploadRecords: keyHierarchyRecords];
                }

                self.nextState = self.intendedState;
            }
            [self runBeforeGroupFinished:self.finishedOp];
        }];
}

@end

#endif // OCTAGON
