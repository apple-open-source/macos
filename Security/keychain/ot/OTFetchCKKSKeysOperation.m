
#if OCTAGON

#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTFetchCKKSKeysOperation ()
@property CKKSKeychainView* ckks;
@property NSSet<CKKSKeychainViewState*>* viewFilter;

@property BOOL fetchBeforeGettingKeyset;
@end

@implementation OTFetchCKKSKeysOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       refetchNeeded:(BOOL)refetchNeeded
{
    if((self = [super init])) {
        _ckks = dependencies.ckks;
        _viewFilter = nil;
        _viewKeySets = @[];
        _pendingTLKShares = @[];
        _incompleteKeySets = @[];

        _desiredTimeout = SecCKKSTestsEnabled() ? 5*NSEC_PER_SEC : 15*NSEC_PER_SEC;

        _fetchBeforeGettingKeyset = refetchNeeded;

        _zonesTimedOutWithoutKeysets = [NSSet set];
    }
    return self;
}

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                        viewsToFetch:(NSSet<CKKSKeychainViewState*>*)views
{
    if((self = [self initWithDependencies:dependencies
                            refetchNeeded:NO])) {
        _viewFilter = views;
    }
    return self;
}

- (void)groupStart
{
    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keyOps = [NSMutableArray array];

    if(self.ckks) {
        secnotice("octagon-ckks", "Waiting for %@", self.ckks);
        [keyOps addObject:[[self.ckks findKeySets:self.fetchBeforeGettingKeyset] timeout:self.desiredTimeout]];
    }

    WEAKIFY(self);
    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"proceed-with-ckks-keys"
                                                            withBlock:^{
        STRONGIFY(self);

        NSMutableArray<CKKSKeychainBackedKeySet*>* viewKeySets = [NSMutableArray array];
        NSMutableArray<CKKSCurrentKeySet*>* ckksBrokenKeySets = [NSMutableArray array];
        NSMutableArray<CKKSTLKShare*>* pendingTLKShares = [NSMutableArray array];

        NSMutableSet<CKRecordZoneID*>* viewsMIA = [NSMutableSet set];

        for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
            if(op.error) {
                secnotice("octagon-ckks", "No keys for %@: %@", op.intendedZoneIDs, op.error);

                if([op.error.domain isEqualToString:CKKSResultErrorDomain] && op.error.code == CKKSResultTimedOut) {
                    [viewsMIA unionSet:op.intendedZoneIDs];
                }
                continue;
            }

            for(CKRecordZoneID* zoneID in op.keysets.allKeys) {
                CKKSCurrentKeySet* keyset = op.keysets[zoneID];

                if(self.viewFilter) {
                    // We want to keep this keyset only if it's in the viewfilter.
                    bool found = false;
                    for(CKKSKeychainViewState* viewState in self.viewFilter) {
                        if([keyset.zoneID.zoneName isEqualToString:viewState.zoneID.zoneName]) {
                            found = true;
                            break;
                        }
                    }
                    if(!found) {
                        // This view wasn't in the view filter.
                        secnotice("octagon-ckks", "Skipping keys: %@", keyset);
                        continue;
                    }
                }

                NSError* localerror = nil;
                CKKSKeychainBackedKeySet* keychainBackedKeyset = [keyset asKeychainBackedSet:&localerror];

                if(keychainBackedKeyset) {
                    secnotice("octagon-ckks", "Have proposed keys: %@", keyset);
                    [viewKeySets addObject:keychainBackedKeyset];
                } else {
                    if(keyset) {
                        secnotice("octagon-ckks", "Unable to convert proposed keys: %@ %@", keyset, localerror);
                        [ckksBrokenKeySets addObject:keyset];
                    }
                }

                for(CKKSTLKShareRecord* tlkShareRecord in keyset.pendingTLKShares) {
                    [pendingTLKShares addObject:tlkShareRecord.share];
                }
                secnotice("octagon-ckks", "Have %u pending tlk shares",
                          (uint32_t)keyset.pendingTLKShares.count);
            }
        }

        self.viewKeySets = viewKeySets;
        self.incompleteKeySets = ckksBrokenKeySets;
        self.pendingTLKShares = pendingTLKShares;
        self.zonesTimedOutWithoutKeysets = viewsMIA;

        secnotice("octagon-ckks", "Fetched %d key sets, %d broken key sets, %d pendingTLKShares, and %d views timing out",
                  (int)self.viewKeySets.count,
                  (int)self.incompleteKeySets.count,
                  (int)self.pendingTLKShares.count,
                  (int)self.zonesTimedOutWithoutKeysets.count);
    }];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
        [proceedWithKeys addDependency: op];
    }

    [self runBeforeGroupFinished:proceedWithKeys];
}
@end

#endif // OCTAGON
