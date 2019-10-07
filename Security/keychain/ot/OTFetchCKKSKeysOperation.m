
#if OCTAGON

#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTFetchCKKSKeysOperation ()
@property NSSet<CKKSKeychainView*>* views;
@property CKKSViewManager* manager;
@end

@implementation OTFetchCKKSKeysOperation

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
{
    if((self = [super init])) {
        _manager = dependencies.viewManager;
        _views = nil;
        _viewKeySets = @[];
        _tlkShares = @[];
        _pendingTLKShares = @[];
        _incompleteKeySets = @[];
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
        [keyOps addObject:[[view findKeySet] timeout:45*NSEC_PER_SEC]];
    }

    WEAKIFY(self);
    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"proceed-with-ckks-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);

                                                                NSMutableArray<CKKSKeychainBackedKeySet*>* viewKeySets = [NSMutableArray array];
                                                                NSMutableArray<CKKSCurrentKeySet*>* ckksBrokenKeySets = [NSMutableArray array];
                                                                NSMutableArray<CKKSTLKShare*>* tlkShares = [NSMutableArray array];
                                                                NSMutableArray<CKKSTLKShare*>* pendingTLKShares = [NSMutableArray array];

                                                                for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
                                                                    if(op.error) {
                                                                        secnotice("octagon-ckks", "No keys for zone %@: %@", op.zoneName, op.error);
                                                                        continue;
                                                                    }

                                                                    NSError* localerror = nil;
                                                                    CKKSKeychainBackedKeySet* keyset = [op.keyset asKeychainBackedSet:&localerror];

                                                                    if(keyset) {
                                                                        secnotice("octagon-ckks", "Have proposed keys: %@", op.keyset);
                                                                        [viewKeySets addObject:keyset];
                                                                    } else {
                                                                        secnotice("octagon-ckks", "Unable to convert proposed keys: %@ %@", op.keyset, localerror);
                                                                        if(op.keyset) {
                                                                            [ckksBrokenKeySets addObject:op.keyset];
                                                                        }
                                                                    }

                                                                    for(CKKSTLKShareRecord* tlkShareRecord in op.keyset.tlkShares) {
                                                                        [tlkShares addObject:tlkShareRecord.share];
                                                                    }
                                                                    secnotice("octagon-ckks", "Have %u tlk shares", (uint32_t)op.keyset.tlkShares.count);

                                                                    for(CKKSTLKShareRecord* tlkShareRecord in op.keyset.pendingTLKShares) {
                                                                        [pendingTLKShares addObject:tlkShareRecord.share];
                                                                    }
                                                                    secnotice("octagon-ckks", "Have %u pending tlk shares", (uint32_t)op.keyset.pendingTLKShares.count);
                                                                }

                                                                self.viewKeySets = viewKeySets;
                                                                self.incompleteKeySets = ckksBrokenKeySets;
                                                                self.tlkShares = tlkShares;
                                                                self.pendingTLKShares = pendingTLKShares;

                                                                secnotice("octagon-ckks", "Fetched %d key sets, %d broken key set,s %d tlk shares, and %d pendingTLKShares",
                                                                          (int)self.viewKeySets.count,
                                                                          (int)self.incompleteKeySets.count,
                                                                          (int)self.tlkShares.count,
                                                                          (int)self.pendingTLKShares.count);
                                                            }];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyOps) {
        [proceedWithKeys addDependency: op];
    }

    [self runBeforeGroupFinished:proceedWithKeys];
}
@end

#endif // OCTAGON
