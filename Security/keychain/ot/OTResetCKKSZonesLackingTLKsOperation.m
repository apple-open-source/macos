#if OCTAGON

#import "utilities/debugging.h"

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/OTResetCKKSZonesLackingTLKsOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"

#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainView.h"

#import "keychain/ot/ObjCImprovements.h"

@interface OTResetCKKSZonesLackingTLKsOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishedOp;
@end

@implementation OTResetCKKSZonesLackingTLKsOperation
@synthesize nextState = _nextState;
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
    secnotice("octagon", "Checking if any CKKS zones need resetting");

    WEAKIFY(self);

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon", "Finishing resetting CKKS missing TLKs operation with %@", self.error ?: @"no error");
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithDependencies:self.deps];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"continue-ckks-resets"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets
                                                                    incompleteKeySets:fetchKeysOp.incompleteKeySets
                                                                     pendingTLKShares:fetchKeysOp.tlkShares];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets
      incompleteKeySets:(NSArray<CKKSCurrentKeySet*>*)incompleteKeySets
       pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
{
    // Now that CKKS has returned, what are we even doing
    NSMutableSet<CKKSKeychainView*>* viewsToReset = [NSMutableSet set];

    for(CKKSCurrentKeySet* incompleteKeySet in incompleteKeySets) {
        if(incompleteKeySet.error == nil) {
            CKKSViewManager* viewManager = self.deps.viewManager;
            CKKSKeychainView* viewMatchingSet = [viewManager findView:incompleteKeySet.viewName];

            if(!viewMatchingSet) {
                secnotice("octagon-ckks", "No view matching viewset %@?", incompleteKeySet);
                continue;
            }

            if(incompleteKeySet.currentTLKPointer != nil &&
               incompleteKeySet.tlk == nil) {

                BOOL otherDevicesAlive = [viewMatchingSet otherDevicesReportHavingTLKs:incompleteKeySet];
                if(otherDevicesAlive) {
                    secnotice("octagon-ckks", "Recently active devices claim to have TLK from key set %@; not scheduling for reset", incompleteKeySet);
                } else {
                    secnotice("octagon-ckks", "Key set %@ has no TLK; scheduling for reset", incompleteKeySet);
                    [viewsToReset addObject:viewMatchingSet];
                }
            }
        } else {
            secnotice("octagon-ckks", "Error loading key set %@; not attempting reset", incompleteKeySet);
        }
    }

    if(viewsToReset.count == 0) {
        // Nothing to do; return to ready
        secnotice("octagon-ckks", "No CKKS views need resetting");
        self.nextState = self.intendedState;
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    [self resetViews:viewsToReset];
}

- (void)resetViews:(NSSet<CKKSKeychainView*>*)viewsToReset {
    CKOperationGroup* opGroup = [CKOperationGroup CKKSGroupWithName:@"octagon-reset-missing-tlks"];
    for (CKKSKeychainView* view in viewsToReset) {
        secnotice("octagon-ckks", "Resetting CKKS %@", view);
        CKKSResultOperation* op = [view resetCloudKitZone:opGroup];

        // Use an intermediary operation, just to ensure we have a timeout
        CKKSResultOperation* waitOp = [CKKSResultOperation named:[NSString stringWithFormat:@"wait-for-%@", view.zoneName]
                                                       withBlock:^{}];
        [waitOp timeout:120*NSEC_PER_SEC];
        [waitOp addDependency:op];
        [self.operationQueue addOperation:waitOp];

        [self.finishedOp addDependency:waitOp];
    }

    self.nextState = self.intendedState;
    [self.operationQueue addOperation:self.finishedOp];
}

@end

#endif // OCTAGON
