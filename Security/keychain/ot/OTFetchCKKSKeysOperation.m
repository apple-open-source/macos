
#if OCTAGON

#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTFetchCKKSKeysOperation ()
@property NSSet<CKKSKeychainView*>* views;
@property CKKSViewManager* manager;

@property BOOL fetchBeforeGettingKeyset;
@end

@implementation OTFetchCKKSKeysOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       refetchNeeded:(BOOL)refetchNeeded
{
    if((self = [super init])) {
        _manager = dependencies.viewManager;
        _views = nil;
        _viewKeySets = @[];
        _tlkShares = @[];
        _pendingTLKShares = @[];
        _incompleteKeySets = @[];

        _desiredTimeout = SecCKKSTestsEnabled() ? 5*NSEC_PER_SEC : 15*NSEC_PER_SEC;

        _fetchBeforeGettingKeyset = refetchNeeded;

        _viewsTimedOutWithoutKeysets = [NSSet set];
    }
    return self;
}

- (instancetype)initWithViews:(NSSet<CKKSKeychainView*>*)views
{
    if((self = [super init])) {
        _views = views;
        _manager = nil;
        _viewKeySets = @[];
        _tlkShares = @[];
        _pendingTLKShares = @[];
        _incompleteKeySets = @[];

        _desiredTimeout = SecCKKSTestsEnabled() ? 5*NSEC_PER_SEC : 15*NSEC_PER_SEC;

        _fetchBeforeGettingKeyset = NO;

        _viewsTimedOutWithoutKeysets = [NSSet set];
    }
    return self;
}

- (void)groupStart
{
    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keyOps = [NSMutableArray array];

    if (self.views == nil) {
        NSMutableSet<CKKSKeychainView*>* mutViews = [NSMutableSet<CKKSKeychainView*> set];
        for (id key in self.manager.views) {
            CKKSKeychainView* view = self.manager.views[key];
            [mutViews addObject: view];
        }
        self.views = mutViews;
    }

    for (CKKSKeychainView* view in self.views) {
        secnotice("octagon-ckks", "Waiting for %@", view);
        [keyOps addObject:[[view findKeySet:self.fetchBeforeGettingKeyset] timeout:self.desiredTimeout]];
    }

    WEAKIFY(self);
    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"proceed-with-ckks-keys"
                                                            withBlock:^{
        STRONGIFY(self);

        NSMutableArray<CKKSKeychainBackedKeySet*>* viewKeySets = [NSMutableArray array];
        NSMutableArray<CKKSCurrentKeySet*>* ckksBrokenKeySets = [NSMutableArray array];
        NSMutableArray<CKKSTLKShare*>* tlkShares = [NSMutableArray array];
        NSMutableArray<CKKSTLKShare*>* pendingTLKShares = [NSMutableArray array];

        NSMutableSet<NSString*>* viewsMIA = [NSMutableSet set];

        for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
            if(op.error) {
                secnotice("octagon-ckks", "No keys for zone %@: %@", op.zoneName, op.error);

                if([op.error.domain isEqualToString:CKKSResultErrorDomain] && op.error.code == CKKSResultTimedOut) {
                    [viewsMIA addObject:op.zoneName];
                }
                continue;
            }

            NSError* localerror = nil;
            CKKSCurrentKeySet* keyset = op.keyset;
            CKKSKeychainBackedKeySet* keychainBackedKeyset = [keyset asKeychainBackedSet:&localerror];

            if(keychainBackedKeyset) {
                secnotice("octagon-ckks", "Have proposed keys: %@", keyset);
                [viewKeySets addObject:keychainBackedKeyset];
            } else {
                if(keyset) {
                    secnotice("octagon-ckks", "Unable to convert proposed keys: %@ %@", keyset, localerror);
                    [ckksBrokenKeySets addObject:op.keyset];
                }
            }

            for(CKKSTLKShareRecord* tlkShareRecord in op.keyset.tlkShares) {
                [tlkShares addObject:tlkShareRecord.share];
            }

            for(CKKSTLKShareRecord* tlkShareRecord in op.keyset.pendingTLKShares) {
                [pendingTLKShares addObject:tlkShareRecord.share];
            }
            secnotice("octagon-ckks", "Have %u tlk shares, %u pending tlk shares",
                      (uint32_t)op.keyset.tlkShares.count,
                      (uint32_t)op.keyset.pendingTLKShares.count);
        }

        self.viewKeySets = viewKeySets;
        self.incompleteKeySets = ckksBrokenKeySets;
        self.tlkShares = tlkShares;
        self.pendingTLKShares = pendingTLKShares;
        self.viewsTimedOutWithoutKeysets = viewsMIA;

        secnotice("octagon-ckks", "Fetched %d key sets, %d broken key sets, %d tlk shares, %d pendingTLKShares, and %d views timing out",
                  (int)self.viewKeySets.count,
                  (int)self.incompleteKeySets.count,
                  (int)self.tlkShares.count,
                  (int)self.pendingTLKShares.count,
                  (int)self.viewsTimedOutWithoutKeysets.count);
    }];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
        [proceedWithKeys addDependency: op];
    }

    [self runBeforeGroupFinished:proceedWithKeys];
}
@end

#endif // OCTAGON
